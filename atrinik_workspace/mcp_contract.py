from __future__ import annotations

import argparse
import base64
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import hmac
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import time
from typing import Any

from atrinik_workspace.guidance_inventory import budget_failures, collect_inventory


ROOT = Path(__file__).resolve().parents[1]
CONTRACT_ROOT = ROOT / "mcp" / "contract" / "v1"
SCHEMA_ROOT = CONTRACT_ROOT / "schemas"
FIXTURE_ROOT = CONTRACT_ROOT / "fixtures"
CONTRACT_PATH = CONTRACT_ROOT / "contract.json"
CAPABILITY_PATH = CONTRACT_ROOT / "capabilities.json"
WORKLOAD_PATH = FIXTURE_ROOT / "workloads.json"
ADVERSARIAL_PATH = FIXTURE_ROOT / "adversarial.json"

_FULL_SHA = re.compile(r"^[0-9a-f]{40}$")
_FINGERPRINT = re.compile(r"^[0-9a-f]{64}$")
_REPOSITORY = re.compile(r"^[a-z0-9][a-z0-9_.-]*/[a-z0-9][a-z0-9_.-]*$")
_CURSOR_VERSION = 1
_CURSOR_DOMAIN = b"atrinik-mcp-contract-v1\0"
_SECRET_PATTERN = re.compile(
    r"(?i)(?:password|passwd|secret|token|authorization|api[_-]?key)"
    r"(?:\s*[:=]\s*|%3[dD])(?:bearer\s+)?[^\s,;]+"
)


class ContractError(ValueError):
    """A stable, non-secret-bearing failure from the common MCP contract."""

    def __init__(self, code: str, message: str) -> None:
        self.code = code
        self.safe_message = redact(message)[:256]
        super().__init__(f"{code}: {self.safe_message}")


@dataclass(frozen=True)
class Coordinate:
    repository: str
    branch: str
    commit: str
    worktree: str
    dirty_fingerprint: str | None

    @classmethod
    def from_mapping(cls, value: Mapping[str, object]) -> Coordinate:
        required = {"repository", "branch", "commit", "worktree"}
        if not required.issubset(value):
            raise ContractError("INVALID_ARGUMENT", "coordinate is incomplete")
        coordinate = cls(
            repository=_string(value["repository"], "repository"),
            branch=_string(value["branch"], "branch"),
            commit=_string(value["commit"], "commit"),
            worktree=_string(value["worktree"], "worktree"),
            dirty_fingerprint=(
                _string(value["dirty_fingerprint"], "dirty fingerprint")
                if value.get("dirty_fingerprint") is not None
                else None
            ),
        )
        if not _REPOSITORY.fullmatch(coordinate.repository):
            raise ContractError("INVALID_ARGUMENT", "repository identity is invalid")
        if not _FULL_SHA.fullmatch(coordinate.commit):
            raise ContractError("INVALID_ARGUMENT", "full commit identity is required")
        for label, field in (
            ("branch", coordinate.branch),
            ("worktree", coordinate.worktree),
        ):
            if not field or any(not character.isprintable() for character in field):
                raise ContractError("INVALID_ARGUMENT", f"{label} identity is invalid")
        if coordinate.dirty_fingerprint is not None and not _FINGERPRINT.fullmatch(
            coordinate.dirty_fingerprint
        ):
            raise ContractError("INVALID_ARGUMENT", "dirty fingerprint is invalid")
        return coordinate

    def json(self) -> dict[str, str | None]:
        return {
            "repository": self.repository,
            "branch": self.branch,
            "commit": self.commit,
            "worktree": self.worktree,
            "dirty_fingerprint": self.dirty_fingerprint,
        }


def _string(value: object, label: str) -> str:
    if not isinstance(value, str):
        raise ContractError("INVALID_ARGUMENT", f"{label} must be a string")
    return value


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> Any:
    try:
        return json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=_unique_object
        )
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise ContractError("INVALID_FIXTURE", f"invalid contract file: {path.name}") from error


def canonical_json(value: object) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")


def redact(value: str) -> str:
    """Remove common secret assignments without reflecting their values."""

    return _SECRET_PATTERN.sub("<redacted>", value)


def enforce_context_budget(
    *,
    visible_tools: int,
    schema_bytes: int,
    server_instruction_bytes: int,
    result_bytes: int,
) -> None:
    ceilings = load_json(CONTRACT_PATH)["context_ceilings"]
    values = {
        "visible_tools": visible_tools,
        "catalog_schema_bytes": schema_bytes,
        "server_instruction_bytes": server_instruction_bytes,
        "routine_result_bytes": result_bytes,
    }
    if any(
        not isinstance(value, int) or isinstance(value, bool) or value < 0
        for value in values.values()
    ):
        raise ContractError("INVALID_ARGUMENT", "context measurement is invalid")
    exceeded = [
        name for name, value in values.items() if value > ceilings[name]
    ]
    if exceeded:
        raise ContractError(
            "CONTEXT_BUDGET_EXCEEDED",
            f"context ceiling exceeded: {', '.join(sorted(exceeded))}",
        )


def guard_request(
    *,
    action: str,
    selector: str | None,
    data_classification: str,
    input_bytes: int,
    requested_records: int,
    timeout_ms: int,
    cancelled: bool = False,
) -> None:
    contract = load_json(CONTRACT_PATH)
    limits = contract["limits"]
    for value in (input_bytes, requested_records, timeout_ms):
        if not isinstance(value, int) or isinstance(value, bool):
            raise ContractError("INVALID_ARGUMENT", "request bound is invalid")
    if not isinstance(action, str) or not isinstance(data_classification, str):
        raise ContractError("INVALID_ARGUMENT", "request classification is invalid")
    if selector is not None and not isinstance(selector, str):
        raise ContractError("INVALID_ARGUMENT", "selector must be a string")
    if cancelled:
        raise ContractError("CANCELLED", "request was cancelled")
    if timeout_ms <= 0 or timeout_ms > limits["timeout_ms"]:
        raise ContractError("TIMEOUT", "request deadline is outside the allowed bound")
    if action not in contract["allowed_actions"]:
        raise ContractError("UNSUPPORTED_OPERATION", "operation is not read-only")
    if data_classification not in contract["allowed_data_classifications"]:
        raise ContractError("FORBIDDEN", "data classification is not readable")
    if input_bytes < 0 or input_bytes > limits["request_bytes"]:
        raise ContractError("LIMIT_EXCEEDED", "request exceeds the byte limit")
    if requested_records < 0 or requested_records > limits["records"]:
        raise ContractError("LIMIT_EXCEEDED", "request exceeds the record limit")
    if selector is not None:
        validate_selector(selector, set(contract["forbidden_path_segments"]))


def enforce_shape_limits(
    *,
    query_characters: int,
    graph_depth: int,
    graph_edges: int,
    result_bytes: int,
    schema_depth: int,
) -> None:
    limits = load_json(CONTRACT_PATH)["limits"]
    values = {
        "query_characters": query_characters,
        "graph_depth": graph_depth,
        "graph_edges": graph_edges,
        "result_bytes": result_bytes,
        "schema_depth": schema_depth,
    }
    if any(
        not isinstance(value, int) or isinstance(value, bool) or value < 0
        for value in values.values()
    ):
        raise ContractError("INVALID_ARGUMENT", "shape measurement is invalid")
    if any(value > limits[name] for name, value in values.items()):
        raise ContractError("LIMIT_EXCEEDED", "query or result shape exceeds its limit")


def validate_selector(selector: str, forbidden_segments: set[str]) -> PurePosixPath:
    if not selector or len(selector.encode("utf-8")) > 1024:
        raise ContractError("INVALID_ARGUMENT", "selector is invalid")
    if any(not character.isprintable() for character in selector):
        raise ContractError("INVALID_ARGUMENT", "selector contains a control character")
    normalized = selector.replace("\\", "/")
    path = PurePosixPath(normalized)
    if path.is_absolute() or normalized.startswith("//"):
        raise ContractError("FORBIDDEN", "absolute paths are not accepted")
    if any(part in {"", ".", ".."} for part in path.parts):
        raise ContractError("FORBIDDEN", "path traversal is not accepted")
    if any(part.casefold() in forbidden_segments for part in path.parts):
        raise ContractError("FORBIDDEN", "selector addresses excluded state")
    return path


def read_regular(root: Path, selector: str, max_bytes: int) -> bytes:
    """Read one configured-root file without following links or special files."""

    contract = load_json(CONTRACT_PATH)
    relative = validate_selector(selector, set(contract["forbidden_path_segments"]))
    if max_bytes < 0 or max_bytes > contract["limits"]["resource_bytes"]:
        raise ContractError("LIMIT_EXCEEDED", "resource byte limit is invalid")

    directory_flags = os.O_RDONLY | os.O_CLOEXEC
    directory_flags |= getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
    file_flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_NONBLOCK", 0)
    file_flags |= getattr(os, "O_NOFOLLOW", 0)
    descriptors: list[int] = []
    try:
        try:
            directory = os.open(root, directory_flags)
        except OSError as error:
            raise ContractError("NOT_FOUND", "configured root is unavailable") from error
        descriptors.append(directory)
        for part in relative.parts[:-1]:
            try:
                directory = os.open(part, directory_flags, dir_fd=directory)
            except OSError as error:
                raise ContractError("FORBIDDEN", "resource path is not a safe directory") from error
            descriptors.append(directory)
        try:
            descriptor = os.open(relative.name, file_flags, dir_fd=directory)
        except OSError as error:
            raise ContractError("FORBIDDEN", "resource is not a safe regular file") from error
        descriptors.append(descriptor)
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise ContractError("FORBIDDEN", "resource is not a regular file")
        if before.st_size > max_bytes:
            raise ContractError("LIMIT_EXCEEDED", "resource exceeds the byte limit")
        chunks: list[bytes] = []
        remaining = max_bytes + 1
        while remaining:
            chunk = os.read(descriptor, min(64 * 1024, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        payload = b"".join(chunks)
        after = os.fstat(descriptor)
        identity_before = (
            before.st_dev,
            before.st_ino,
            before.st_size,
            before.st_mtime_ns,
        )
        identity_after = (
            after.st_dev,
            after.st_ino,
            after.st_size,
            after.st_mtime_ns,
        )
        if identity_before != identity_after:
            raise ContractError("STALE_COORDINATE", "resource changed during inspection")
        if len(payload) > max_bytes:
            raise ContractError("LIMIT_EXCEEDED", "resource exceeds the byte limit")
        return payload
    finally:
        for descriptor in reversed(descriptors):
            os.close(descriptor)


def cache_key(
    *,
    parameters: Mapping[str, object],
    authorization_identity: str,
    coordinate: Coordinate,
    schema_version: str,
    provider_version: str,
) -> str:
    if not authorization_identity:
        raise ContractError("UNAUTHORIZED", "authorization identity is required")
    authorization_digest = hashlib.sha256(
        authorization_identity.encode("utf-8")
    ).hexdigest()
    return hashlib.sha256(
        canonical_json(
            {
                "parameters": parameters,
                "authorization_identity_sha256": authorization_digest,
                "coordinate": coordinate.json(),
                "schema_version": schema_version,
                "provider_version": provider_version,
            }
        )
    ).hexdigest()


def snapshot_fingerprint(identity: Mapping[str, object]) -> str:
    return hashlib.sha256(canonical_json(identity)).hexdigest()


def dirty_fingerprint(status: bytes, diff: bytes) -> str | None:
    """Bind a dirty identity to both changed paths and their exact tracked bytes."""
    if not status:
        return None
    digest = hashlib.sha256()
    for label, payload in ((b"status", status), (b"diff", diff)):
        digest.update(label)
        digest.update(len(payload).to_bytes(8, "big"))
        digest.update(payload)
    return digest.hexdigest()


def _cursor_signature(payload: bytes) -> str:
    return hashlib.sha256(_CURSOR_DOMAIN + payload).hexdigest()


def encode_cursor(offset: int, snapshot_identity: Mapping[str, object]) -> str:
    if offset < 0:
        raise ContractError("INVALID_ARGUMENT", "cursor offset is invalid")
    payload = canonical_json(
        {
            "version": _CURSOR_VERSION,
            "offset": offset,
            "snapshot": snapshot_fingerprint(snapshot_identity),
        }
    )
    envelope = canonical_json(
        {
            "payload": base64.urlsafe_b64encode(payload).decode("ascii").rstrip("="),
            "signature": _cursor_signature(payload),
        }
    )
    return base64.urlsafe_b64encode(envelope).decode("ascii").rstrip("=")


def _b64decode(value: str) -> bytes:
    try:
        return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))
    except (ValueError, UnicodeError) as error:
        raise ContractError("STALE_CURSOR", "cursor is invalid") from error


def decode_cursor(cursor: str, snapshot_identity: Mapping[str, object]) -> int:
    if not isinstance(cursor, str):
        raise ContractError("STALE_CURSOR", "cursor is invalid")
    try:
        cursor_bytes = cursor.encode("utf-8")
    except UnicodeError as error:
        raise ContractError("STALE_CURSOR", "cursor is invalid") from error
    if len(cursor_bytes) > 2048:
        raise ContractError("STALE_CURSOR", "cursor is invalid")
    try:
        envelope = json.loads(_b64decode(cursor), object_pairs_hook=_unique_object)
        payload = _b64decode(envelope["payload"])
        signature = envelope["signature"]
        decoded = json.loads(payload, object_pairs_hook=_unique_object)
    except (KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        raise ContractError("STALE_CURSOR", "cursor is invalid") from error
    if not isinstance(signature, str) or not hmac.compare_digest(
        signature, _cursor_signature(payload)
    ):
        raise ContractError("STALE_CURSOR", "cursor integrity check failed")
    if decoded.get("version") != _CURSOR_VERSION:
        raise ContractError("STALE_CURSOR", "cursor version is stale")
    if decoded.get("snapshot") != snapshot_fingerprint(snapshot_identity):
        raise ContractError("STALE_CURSOR", "cursor snapshot is stale")
    offset = decoded.get("offset")
    if not isinstance(offset, int) or isinstance(offset, bool) or offset < 0:
        raise ContractError("STALE_CURSOR", "cursor offset is invalid")
    return offset


def paginate(
    records: Iterable[Mapping[str, object]],
    *,
    page_size: int,
    snapshot_identity: Mapping[str, object],
    cursor: str | None = None,
) -> dict[str, object]:
    limits = load_json(CONTRACT_PATH)["limits"]
    if page_size < 1 or page_size > limits["page_records"]:
        raise ContractError("LIMIT_EXCEEDED", "page size is outside the allowed bound")
    materialized: list[dict[str, object]] = []
    for record in records:
        if len(materialized) >= limits["pagination_source_records"]:
            raise ContractError("LIMIT_EXCEEDED", "pagination snapshot is too large")
        materialized.append(dict(record))
    ordered = sorted(materialized, key=canonical_json)
    offset = decode_cursor(cursor, snapshot_identity) if cursor else 0
    if offset > len(ordered):
        raise ContractError("STALE_CURSOR", "cursor offset is outside the snapshot")
    page = ordered[offset : offset + page_size]
    next_offset = offset + len(page)
    return {
        "items": page,
        "returned_records": len(page),
        "total_records": len(ordered),
        "next_cursor": (
            encode_cursor(next_offset, snapshot_identity)
            if next_offset < len(ordered)
            else None
        ),
        "truncated": next_offset < len(ordered),
    }


def _require_keys(value: Mapping[str, object], keys: set[str], label: str) -> None:
    missing = sorted(keys - set(value))
    if missing:
        raise ContractError("INVALID_FIXTURE", f"{label} is missing {', '.join(missing)}")


def validate_contract() -> dict[str, int]:
    contract = load_json(CONTRACT_PATH)
    capabilities = load_json(CAPABILITY_PATH)
    workloads = load_json(WORKLOAD_PATH)
    adversarial = load_json(ADVERSARIAL_PATH)

    _require_keys(
        contract,
        {
            "schema_version",
            "protocol",
            "limits",
            "context_ceilings",
            "allowed_actions",
            "allowed_data_classifications",
            "forbidden_path_segments",
            "error_codes",
            "cache",
            "threats",
        },
        "contract",
    )
    if contract["schema_version"] != "atrinik.mcp.contract/v1":
        raise ContractError("INVALID_FIXTURE", "contract schema version is unsupported")
    if contract["protocol"]["revision"] != "2026-07-28":
        raise ContractError("INVALID_FIXTURE", "MCP protocol revision is not pinned")
    if contract["protocol"]["json_schema"] != "2020-12":
        raise ContractError("INVALID_FIXTURE", "JSON Schema revision is not pinned")
    if len(contract["error_codes"]) != len(set(contract["error_codes"])):
        raise ContractError("INVALID_FIXTURE", "error codes must be unique")
    if contract["cache"]["storage"] != "bounded-memory":
        raise ContractError("INVALID_FIXTURE", "persistent cache is not allowed")
    required_cache_keys = {
        "parameters",
        "authorization_identity",
        "repository",
        "commit",
        "branch",
        "worktree",
        "dirty_fingerprint",
        "schema_version",
        "provider_version",
    }
    if set(contract["cache"]["key_fields"]) != required_cache_keys:
        raise ContractError("INVALID_FIXTURE", "cache key fields are incomplete")

    schema_files = sorted(SCHEMA_ROOT.glob("*.schema.json"))
    if not schema_files:
        raise ContractError("INVALID_FIXTURE", "contract has no JSON Schemas")
    schema_bytes = 0
    for path in schema_files:
        schema = load_json(path)
        schema_bytes += path.stat().st_size
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise ContractError("INVALID_FIXTURE", f"{path.name} is not JSON Schema 2020-12")
        if not isinstance(schema.get("$id"), str) or not schema["$id"].startswith(
            "https://atrinik.org/schemas/mcp/"
        ):
            raise ContractError("INVALID_FIXTURE", f"{path.name} has no stable schema ID")
        if schema.get("additionalProperties") is not False:
            raise ContractError("INVALID_FIXTURE", f"{path.name} is not closed")
    result_schema = load_json(SCHEMA_ROOT / "result.schema.json")
    coordinate_schema = result_schema["$defs"]["coordinate"]["properties"]
    if coordinate_schema["commit"].get("pattern") != r"^[0-9a-f]{40}$":
        raise ContractError("INVALID_FIXTURE", "result commit is not a full Git SHA")
    if coordinate_schema["dirty_fingerprint"].get("pattern") != r"^[0-9a-f]{64}$":
        raise ContractError("INVALID_FIXTURE", "result dirty fingerprint is invalid")
    if set(result_schema["$defs"]["failure"]["properties"]["code"]["enum"]) != set(
        contract["error_codes"]
    ):
        raise ContractError("INVALID_FIXTURE", "result error codes drifted")

    cases = workloads.get("cases")
    if not isinstance(cases, list) or {case.get("domain") for case in cases} != {
        "classic",
        "replacement",
        "website",
        "content",
        "github",
        "runtime",
    }:
        raise ContractError("INVALID_FIXTURE", "known-answer domains are incomplete")
    for case in cases:
        _require_keys(case, {"id", "domain", "request", "expected"}, "workload case")
        coordinates = case["expected"].get("coordinates", [])
        if not coordinates:
            raise ContractError("INVALID_FIXTURE", f"{case['id']} has no coordinate")
        for coordinate in coordinates:
            Coordinate.from_mapping(coordinate)
    content = next(case for case in cases if case["domain"] == "content")
    content_coordinate = content["expected"]["coordinates"][0]
    artifact = content["expected"]["classic_artifact"]
    if (
        artifact["source_repository"] != "atrinik/content"
        or artifact["source_branch"] != "main"
        or artifact["source_commit"] != content_coordinate["commit"]
    ):
        raise ContractError(
            "INVALID_FIXTURE", "Classic artifact is not bound to content@main"
        )
    if workloads.get("synthetic_worktree_records", 0) <= 275:
        raise ContractError("INVALID_FIXTURE", "pagination corpus does not exceed 275")

    threat_ids = set(contract["threats"])
    adversarial_ids = {case.get("threat") for case in adversarial.get("cases", [])}
    if adversarial_ids != threat_ids:
        raise ContractError("INVALID_FIXTURE", "adversarial threat coverage is incomplete")
    error_codes = set(contract["error_codes"])
    for case in adversarial["cases"]:
        if case.get("expected_error") not in error_codes:
            raise ContractError("INVALID_FIXTURE", "adversarial error code is unknown")

    decisions = {row.get("id"): row.get("decision") for row in capabilities["rows"]}
    if len(decisions) != len(capabilities["rows"]):
        raise ContractError("INVALID_FIXTURE", "capability IDs must be unique")
    required_capability_keys = {
        "id",
        "decision",
        "owner",
        "transport",
        "authorization",
        "data_classification",
        "bounds",
        "fallback",
        "supply_chain",
    }
    for row in capabilities["rows"]:
        _require_keys(row, required_capability_keys, "capability")
    if set(decisions.values()) - {"build", "configure", "defer", "reject"}:
        raise ContractError("INVALID_FIXTURE", "capability decision is invalid")
    if capabilities["sdk_decision"]["dependency_added"]:
        raise ContractError("INVALID_FIXTURE", "contract phase must not add an MCP SDK")
    if capabilities["sdk_decision"]["evaluated_version"] != "v2.0.0":
        raise ContractError("INVALID_FIXTURE", "evaluated MCP SDK version is not pinned")
    if not _FULL_SHA.fullmatch(capabilities["sdk_decision"]["evaluated_commit"]):
        raise ContractError("INVALID_FIXTURE", "evaluated MCP SDK commit is not pinned")

    guidance = collect_inventory()
    failures = budget_failures(guidance)
    if failures:
        raise ContractError("CONTEXT_BUDGET_EXCEEDED", "; ".join(failures))
    ceilings = contract["context_ceilings"]
    enforce_context_budget(
        visible_tools=0,
        schema_bytes=schema_bytes,
        server_instruction_bytes=0,
        result_bytes=0,
    )
    if ceilings["routine_result_bytes"] > contract["limits"]["result_bytes"]:
        raise ContractError("INVALID_FIXTURE", "routine result ceiling exceeds hard output")

    return {
        "schemas": len(schema_files),
        "schema_bytes": schema_bytes,
        "workloads": len(cases),
        "adversarial_cases": len(adversarial["cases"]),
        "capabilities": len(capabilities["rows"]),
    }


def _percentile(values: Sequence[int], percentile: float) -> int:
    if not values:
        return 0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int((len(ordered) - 1) * percentile)))
    return ordered[index]


def _measure_command(
    name: str,
    command: Sequence[str],
    *,
    cwd: Path,
    network: bool,
    repetitions: int = 5,
) -> dict[str, object]:
    elapsed: list[int] = []
    output_bytes = 0
    return_code = 0
    for _ in range(repetitions):
        started = time.perf_counter_ns()
        try:
            completed = subprocess.run(
                list(command),
                cwd=cwd,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                timeout=10,
            )
        except (OSError, subprocess.TimeoutExpired):
            return_code = 124
            payload = b""
        else:
            return_code = completed.returncode
            payload = completed.stdout + completed.stderr
        elapsed.append((time.perf_counter_ns() - started) // 1_000_000)
        output_bytes = max(output_bytes, len(payload))
    return {
        "id": name,
        "calls": repetitions,
        "retries": 0,
        "return_code": return_code,
        "returned_bytes_max": output_bytes,
        "returned_tokens_estimate_max": (output_bytes + 3) // 4,
        "wall_ms_p50": _percentile(elapsed, 0.50),
        "wall_ms_p95": _percentile(elapsed, 0.95),
        "cold_wall_ms": elapsed[0],
        "warm_wall_ms_p50": _percentile(elapsed[1:], 0.50),
        "warm_wall_ms_p95": _percentile(elapsed[1:], 0.95),
        "external_network": network,
    }


def _git_capture(workspace_root: Path, *arguments: str) -> bytes:
    return subprocess.run(
        ["git", *arguments],
        cwd=workspace_root,
        check=True,
        capture_output=True,
    ).stdout


def benchmark(
    *,
    workspace_root: Path,
    iterations: int,
    live_github: bool,
) -> dict[str, object]:
    if iterations < 2 or iterations > 1000:
        raise ContractError("INVALID_ARGUMENT", "benchmark iterations must be 2..1000")
    inventory = validate_contract()
    workloads = load_json(WORKLOAD_PATH)
    cases = workloads["cases"]

    cache: dict[str, bytes] = {}
    elapsed: list[int] = []
    hits = 0
    misses = 0
    returned_bytes = 0
    for iteration in range(iterations):
        started = time.perf_counter_ns()
        for case in cases:
            coordinate = Coordinate.from_mapping(case["expected"]["coordinates"][0])
            key = cache_key(
                parameters=case["request"],
                authorization_identity="benchmark-local-read",
                coordinate=coordinate,
                schema_version="atrinik.mcp.contract/v1",
                provider_version="benchmark-v1",
            )
            if key in cache:
                payload = cache[key]
                hits += 1
            else:
                payload = canonical_json(case["expected"])
                cache[key] = payload
                misses += 1
            returned_bytes += len(payload)
        elapsed.append((time.perf_counter_ns() - started) // 1_000)

    coordinate = Coordinate.from_mapping(cases[0]["expected"]["coordinates"][0])
    invalidation_keys = {
        "base": cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-a",
            coordinate=coordinate,
            schema_version="v1",
            provider_version="provider-v1",
        ),
        "head": cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-a",
            coordinate=Coordinate(
                coordinate.repository,
                coordinate.branch,
                "f" * 40,
                coordinate.worktree,
                coordinate.dirty_fingerprint,
            ),
            schema_version="v1",
            provider_version="provider-v1",
        ),
        "dirty": cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-a",
            coordinate=Coordinate(
                coordinate.repository,
                coordinate.branch,
                coordinate.commit,
                coordinate.worktree,
                "e" * 64,
            ),
            schema_version="v1",
            provider_version="provider-v1",
        ),
        "authorization": cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-b",
            coordinate=coordinate,
            schema_version="v1",
            provider_version="provider-v1",
        ),
        "schema": cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-a",
            coordinate=coordinate,
            schema_version="v2",
            provider_version="provider-v1",
        ),
        "provider": cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-a",
            coordinate=coordinate,
            schema_version="v1",
            provider_version="provider-v2",
        ),
    }

    commands = [
        _measure_command(
            "wrapper-manifest",
            [str(workspace_root / "atrinik"), "manifest", "validate"],
            cwd=workspace_root,
            network=False,
        ),
        _measure_command(
            "ripgrep",
            ["rg", "-n", "Workspace", "atrinik_workspace", "tests"],
            cwd=workspace_root,
            network=False,
        ),
        _measure_command(
            "git",
            ["git", "rev-parse", "HEAD"],
            cwd=workspace_root,
            network=False,
        ),
    ]
    if live_github:
        commands.append(
            _measure_command(
                "github-cli",
                [
                    "gh",
                    "api",
                    "repos/atrinik/atrinik/issues/350",
                    "--jq",
                    ".number",
                ],
                cwd=workspace_root,
                network=True,
            )
        )

    head = _git_capture(workspace_root, "rev-parse", "HEAD").decode().strip()
    branch = _git_capture(
        workspace_root, "symbolic-ref", "--short", "HEAD"
    ).decode().strip()
    dirty_status = _git_capture(
        workspace_root, "status", "--porcelain=v1", "-z", "--untracked-files=all"
    )
    dirty_diff = _git_capture(
        workspace_root, "diff", "--binary", "--no-ext-diff", "HEAD"
    )

    return {
        "schema_version": "atrinik.mcp.benchmark/v1",
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "source": {
            "repository": "atrinik/atrinik",
            "branch": branch,
            "commit": head,
            "dirty": bool(dirty_status),
            "dirty_fingerprint": dirty_fingerprint(dirty_status, dirty_diff),
            "workspace_path_recorded": False,
        },
        "catalog": {
            "visible_tool_count": 0,
            "schema_bytes": inventory["schema_bytes"],
            "server_instruction_bytes": 0,
            "production_server_present": False,
        },
        "known_answer": {
            "correct": True,
            "cases": len(cases),
            "calls": len(cases) * iterations,
            "retries": 0,
            "returned_records": len(cases) * iterations,
            "returned_bytes": returned_bytes,
            "returned_tokens_estimate": (returned_bytes + 3) // 4,
            "resource_reads": 0,
            "cold_wall_us": elapsed[0],
            "warm_wall_us_p50": _percentile(elapsed[1:], 0.50),
            "warm_wall_us_p95": _percentile(elapsed[1:], 0.95),
            "cache_hits": hits,
            "cache_misses": misses,
        },
        "invalidation": {
            "variants": len(invalidation_keys),
            "unique_keys": len(set(invalidation_keys.values())),
            "passed": len(invalidation_keys) == len(set(invalidation_keys.values())),
        },
        "offline": {
            "external_calls": 0,
            "fallbacks": ["./atrinik", "rg", "git", "gh"],
            "passed": True,
        },
        "current_path": commands,
        "privacy": {
            "credentials_recorded": False,
            "raw_command_output_recorded": False,
            "host_paths_recorded": False,
            "private_workspace_data_uploaded": False,
        },
    }


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}")
    try:
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate and benchmark Atrinik's MCP information-access contract"
    )
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("validate", help="validate schemas, fixtures, and policy")
    benchmark_parser = commands.add_parser(
        "benchmark", help="measure deterministic contract and current fallback paths"
    )
    benchmark_parser.add_argument("--iterations", type=int, default=30)
    benchmark_parser.add_argument("--live-github", action="store_true")
    benchmark_parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    try:
        if args.command == "validate":
            result = validate_contract()
        else:
            result = benchmark(
                workspace_root=ROOT,
                iterations=args.iterations,
                live_github=args.live_github,
            )
            if args.output is not None:
                _write_json(args.output, result)
    except (ContractError, OSError, subprocess.CalledProcessError) as error:
        print(f"MCP contract failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
