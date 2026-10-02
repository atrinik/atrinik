"""Opt-in, bounded observations of explicitly approved runtime publications.

This module never constructs Workspace or exercises runtime control authority.
"""
from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import time
from typing import Callable

from atrinik_workspace.mcp_contract import ContractError, canonical_json, guard_request, paginate

SCHEMA_VERSION = "atrinik.mcp.runtime/v1"
PROVIDER_VERSION = "atrinik-observe/1"
MAX_BYTES = 256 * 1024
_NAME = re.compile(r"[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}\Z")
_HASH = re.compile(r"[0-9a-f]{64}\Z")
_STATES = frozenset({"starting", "running", "stopped", "exited", "failed"})


@dataclass(frozen=True)
class RuntimeApproval:
    """Host-owned approval, never accepted as a tool argument.

    The digest pins the complete registered spec, including state, scenario,
    source, service and owner identities, without publishing private fields.
    """

    name: str
    profile: str
    generation: str
    spec_sha256: str

    def __post_init__(self) -> None:
        if any(not isinstance(v, str) or not _NAME.fullmatch(v)
               for v in (self.name, self.profile, self.generation)):
            raise ContractError("INVALID_ARGUMENT", "runtime approval identity is invalid")
        if not isinstance(self.spec_sha256, str) or not _HASH.fullmatch(self.spec_sha256):
            raise ContractError("INVALID_ARGUMENT", "runtime approval digest is invalid")


def _object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate field")
        result[key] = value
    return result


def _read(path: Path, check: Callable[[], None]) -> bytes:
    """Fixed internal paths only; every ancestor and file opened no-follow."""
    if not path.is_absolute() or ".." in path.parts:
        raise ContractError("FORBIDDEN", "runtime root is invalid")
    if not all(getattr(os, name, 0) for name in ("O_NOFOLLOW", "O_DIRECTORY", "O_NONBLOCK")):
        raise ContractError("UNSUPPORTED_OPERATION", "safe runtime reads are unavailable")
    fds = []
    try:
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW
        directory = os.open(path.anchor, flags | os.O_DIRECTORY)
        fds.append(directory)
        for part in path.parts[1:-1]:
            check()
            directory = os.open(part, flags | os.O_DIRECTORY, dir_fd=directory)
            fds.append(directory)
        fd = os.open(path.name, flags | os.O_NONBLOCK, dir_fd=directory)
        fds.append(fd)
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode):
            raise ContractError("FORBIDDEN", "runtime publication is not regular")
        if before.st_size > MAX_BYTES:
            raise ContractError("LIMIT_EXCEEDED", "runtime publication exceeds byte limit")
        chunks = []
        remaining = MAX_BYTES + 1
        while remaining:
            check()
            chunk = os.read(fd, min(remaining, 65536))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        after = os.fstat(fd)
        visible = os.stat(path.name, dir_fd=directory, follow_symlinks=False)
        identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        if identity(before) != identity(after) or identity(after) != identity(visible):
            raise ContractError("STALE_COORDINATE", "runtime publication changed")
        payload = b"".join(chunks)
        if len(payload) > MAX_BYTES:
            raise ContractError("LIMIT_EXCEEDED", "runtime publication exceeds byte limit")
        check()
        return payload
    except OSError:
        raise ContractError("FORBIDDEN", "runtime publication is unavailable or unsafe") from None
    finally:
        for fd in reversed(fds):
            os.close(fd)


def _decode(payload: bytes) -> dict:
    try:
        result = json.loads(payload, object_pairs_hook=_object,
                            parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
        if not isinstance(result, dict):
            raise ValueError()
        pending = [(result, 1)]
        while pending:
            value, depth = pending.pop()
            if depth > 32:
                raise ContractError("LIMIT_EXCEEDED", "runtime publication exceeds nesting limit")
            if isinstance(value, dict):
                pending.extend((item, depth + 1) for item in value.values())
            elif isinstance(value, list):
                pending.extend((item, depth + 1) for item in value)
        return result
    except ContractError:
        raise
    except (ValueError, UnicodeError, RecursionError):
        raise ContractError("INCOMPLETE", "runtime publication is malformed") from None


class RuntimeService:
    def __init__(self, context, *, approvals=(), enabled: bool = False,
                 runtime_root: Path | None = None):
        self.context = context
        self.enabled = enabled is True
        self.runtime_root = Path(runtime_root) if runtime_root is not None else Path(context.root) / "workspace"
        approvals = tuple(approvals)
        self.approvals = {approval.name: approval for approval in approvals}
        if len(self.approvals) != len(approvals) or len(approvals) > 1000:
            raise ContractError("INVALID_ARGUMENT", "runtime approval catalog is invalid")

    def status(self, topology: str, *, timeout_ms: int = 5000,
               cancelled: Callable[[], bool] | None = None) -> dict:
        start = time.monotonic()
        if not self.enabled:
            raise ContractError("UNAUTHORIZED", "runtime observations are disabled")
        if not isinstance(topology, str) or not _NAME.fullmatch(topology):
            raise ContractError("INVALID_ARGUMENT", "runtime selector is invalid")
        guard_request(action="inspect", selector=topology,
                      data_classification="operational-metadata", input_bytes=len(topology),
                      requested_records=1, timeout_ms=timeout_ms)
        def check():
            if cancelled is not None and cancelled():
                raise ContractError("CANCELLED", "runtime observation cancelled")
            if (time.monotonic() - start) * 1000 >= timeout_ms:
                raise ContractError("TIMEOUT", "runtime observation timed out")
        check()
        approval = self.approvals.get(topology)
        if approval is None:
            raise ContractError("UNAUTHORIZED", "runtime identity is not approved")
        snapshot = self.context.resolve(profile=approval.profile)
        check()
        snapshot.assert_current()
        root = self.runtime_root / "topologies" / topology
        paths = [root / name for name in (".atrinik-workspace-managed.json", "spec.json", "status.json")]
        payloads = [_read(path, check) for path in paths]
        marker, spec, status = map(_decode, payloads)
        if marker != {"schema_version": 1, "purpose": "topology:" + topology}:
            raise ContractError("FORBIDDEN", "runtime ownership is unverified")
        if hashlib.sha256(payloads[1]).hexdigest() != approval.spec_sha256:
            raise ContractError("STALE_COORDINATE", "runtime approval is stale")
        for record in (spec, status):
            control = record.get("control")
            if (record.get("schema_version") != 3 or record.get("name") != topology
                    or record.get("profile") != approval.profile or not isinstance(control, dict)
                    or control.get("generation") != approval.generation):
                raise ContractError("STALE_COORDINATE", "runtime coordinate differs")
        for key in ("resolved", "state", "state_policy", "runtime", "providers", "stack", "control"):
            if key not in spec or status.get(key) != spec[key]:
                raise ContractError("STALE_COORDINATE", "runtime identity differs")
        services = status.get("services")
        approved_services = spec.get("services")
        if (not isinstance(services, dict) or not isinstance(approved_services, dict)
                or not set(services) <= set(approved_services)
                or not set(approved_services) <= {"server", "client", "metaserver"}
                or type(status.get("ready")) is not bool):
            raise ContractError("INCOMPLETE", "runtime service publication is malformed")
        counts = {state: 0 for state in sorted(_STATES)}
        for service in services.values():
            if (not isinstance(service, dict) or not isinstance(service.get("status"), str)
                    or service.get("status") not in _STATES
                    or service.get("generation") != approval.generation):
                raise ContractError("INCOMPLETE", "runtime service publication is malformed")
            counts[service["status"]] += 1
        for path, previous in zip(paths, payloads):
            if _read(path, check) != previous:
                raise ContractError("STALE_COORDINATE", "runtime publication changed")
        snapshot.assert_current()
        check()
        return {
            "schema_version": SCHEMA_VERSION, "provider_version": PROVIDER_VERSION,
            "coordinate": snapshot.coordinate.json(),
            "topology": approval.name, "profile": approval.profile,
            "generation": approval.generation, "spec_sha256": approval.spec_sha256,
            "freshness": {"observed_at": datetime.now(timezone.utc).isoformat(),
                          "ttl_ms": 0, "status_sha256": hashlib.sha256(payloads[2]).hexdigest()},
            "observation": "published-status", "recorded_ready": status["ready"],
            "service_counts": counts, "expected_services": len(approved_services),
            "state_identity": hashlib.sha256(canonical_json(spec["state"])).hexdigest(),
            "build_identity": hashlib.sha256(canonical_json(spec["runtime"])).hexdigest(),
            "services": sorted(approved_services),
            "redactions": ["paths", "commands", "environment", "process-identifiers", "logs", "private-state"],
            "bounds": {"file_bytes": MAX_BYTES, "timeout_ms": timeout_ms},
            "incomplete": len(services) != len(approved_services), "truncated": False,
        }


    def list(self, *, page_size: int = 25, cursor: str | None = None,
             timeout_ms: int = 5000, cancelled: Callable[[], bool] | None = None) -> dict:
        """List only explicitly approved registered publications, never scan disk."""
        if not self.enabled:
            raise ContractError("UNAUTHORIZED", "runtime observations are disabled")
        guard_request(action="list", selector=None, data_classification="operational-metadata",
                      input_bytes=len(canonical_json({"cursor": cursor, "page_size": page_size})),
                      requested_records=page_size, timeout_ms=timeout_ms)
        if type(page_size) is not int or not 1 <= page_size <= 50:
            raise ContractError("LIMIT_EXCEEDED", "runtime page size is invalid")
        if cursor is not None and (not isinstance(cursor, str) or len(cursor) > 1024):
            raise ContractError("INVALID_ARGUMENT", "runtime cursor is invalid")
        start = time.monotonic()
        def check():
            if cancelled is not None and cancelled():
                raise ContractError("CANCELLED", "runtime listing cancelled")
            if (time.monotonic() - start) * 1000 >= timeout_ms:
                raise ContractError("TIMEOUT", "runtime listing timed out")
        check()
        records = []
        fingerprints = []
        for name in sorted(self.approvals):
            remaining = timeout_ms - int((time.monotonic() - start) * 1000)
            if remaining <= 0:
                raise ContractError("TIMEOUT", "runtime listing timed out")
            try:
                record = self.status(name, timeout_ms=remaining, cancelled=cancelled)
                stable = {key: value for key, value in record.items()
                          if key not in {"freshness", "bounds"}}
                stable["status_sha256"] = record["freshness"]["status_sha256"]
                fingerprints.append(stable)
                records.append({key: record[key] for key in (
                    "topology", "profile", "generation", "coordinate", "state_identity",
                    "build_identity", "services", "recorded_ready", "incomplete")})
            except ContractError as error:
                if error.code in {"CANCELLED", "TIMEOUT"}:
                    raise
                record = {"topology": name, "incomplete": True, "error": error.code}
                records.append(record)
                fingerprints.append(record)
        # A later observation must not hide a publication replaced earlier in
        # this listing. Recheck every successful observation at the handoff.
        for observed in fingerprints:
            check()
            if "error" in observed:
                continue
            directory = self.runtime_root / "topologies" / observed["topology"]
            for filename, digest in (("spec.json", observed["spec_sha256"]),
                                     ("status.json", observed["status_sha256"])):
                if hashlib.sha256(_read(directory / filename, check)).hexdigest() != digest:
                    raise ContractError("STALE_COORDINATE", "runtime listing changed")
        identity = {"records": fingerprints,
                    "approvals": [vars(self.approvals[name]) for name in sorted(self.approvals)],
                    "authorization_identity": self.context.authorization_identity,
                    "schema_version": SCHEMA_VERSION, "provider_version": PROVIDER_VERSION,
                    "page_size": page_size}
        result = paginate(records, page_size=page_size, cursor=cursor, snapshot_identity=identity)
        result.update(schema_version=SCHEMA_VERSION, provider_version=PROVIDER_VERSION,
                      freshness={"observed_at": datetime.now(timezone.utc).isoformat(), "ttl_ms": 0},
                      incomplete=any(record["incomplete"] for record in result["items"]),
                      redactions=["paths", "commands", "environment", "process-identifiers", "logs", "private-state"],
                      bounds={"scanned_records": 1000, "page_records": 50, "timeout_ms": timeout_ms})
        if len(canonical_json(result)) > 32768:
            raise ContractError("LIMIT_EXCEEDED", "runtime result exceeds byte limit")
        check()
        return result


def runtime_tools(service: RuntimeService):
    """Explicit transport composition; never called by default context startup."""
    from atrinik_workspace.mcp_server import Tool
    from atrinik_workspace.mcp_context import check_request

    def active():
        check_request()
        return False

    return (
        Tool("runtime_status", "Inspect an approved runtime publication; no live process probe.",
             {"type": "object", "properties": {"topology": {"type": "string", "maxLength": 64}},
              "required": ["topology"], "additionalProperties": False},
             lambda args: service.status(args["topology"], cancelled=active)),
        Tool("runtime_list", "List explicitly approved runtime publications with bounded failures.",
             {"type": "object", "properties": {
                 "page_size": {"type": "integer", "minimum": 1, "maximum": 50},
                 "cursor": {"type": "string", "maxLength": 1024}}, "additionalProperties": False},
             lambda args: service.list(**args, cancelled=active)),
    )
