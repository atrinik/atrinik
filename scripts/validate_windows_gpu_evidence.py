#!/usr/bin/env python3
"""Validate the bounded evidence record for a native Windows GPU preflight."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import stat
import sys
from typing import Any, Mapping, Sequence


SCHEMA_VERSION = 1
KIND = "native-windows-classic-gpu-preflight"
MAX_EVIDENCE_BYTES = 1 * 1024 * 1024
MAX_ARTIFACT_BYTES = 8 * 1024 * 1024
COMMAND_NAMES = (
    "cross-build",
    "package-handoff",
    "native-package-smoke",
    "d3d12-benchmark",
    "linux-coordinator-diagnostics",
    "cleanup",
)
COMMAND_STATUSES = frozenset({"passed", "failed", "not-run"})
FAILURE_CLASSES = frozenset(
    {
        "passed",
        "cross-build",
        "package-handoff",
        "windows-client-startup-runtime",
        "gpu-backend-device",
        "benchmark",
        "linux-only-coordinator",
        "cleanup",
    }
)
BACKENDS = frozenset({"direct3d12", "vulkan", "metal", "unavailable"})
HARDWARE_TIERS = frozenset({"reference", "minimum", "not-qualified", "unknown"})
ARCHITECTURES = frozenset({"x86_64", "arm64"})
SHA256 = re.compile(r"^[0-9a-f]{64}$")
COMMIT = re.compile(r"^[0-9a-f]{40}$")
PROFILE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
ARTIFACT_PATH = re.compile(r"^evidence/[A-Za-z0-9][A-Za-z0-9._/-]{0,159}$")
UTC_TIMESTAMP = re.compile(
    r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}"
    r"(?:\.\d{1,9})?Z$"
)
CONTROL = re.compile(r"[\x00-\x1f\x7f]")
SECRET_VALUE = re.compile(
    r"(?:-----BEGIN [^-]*PRIVATE KEY-----|"
    r"\b(?:gh[pousr]_|github_pat_|xox[baprs]-|AKIA[0-9A-Z]{16})\S*|"
    r"(?i:\b(?:password|passphrase|secret|token|credential|"
    r"api[_ -]?key|authorization|cookie)\b\s*[:=]\s*\S+)|"
    r"https?://[^/\s:@]+:[^@\s]+@)",
)
PRIVATE_PATH = re.compile(
    r"(?:[A-Za-z]:[\\/]|\\\\[^\\/\s]+[\\/]|"
    r"/(?:home|Users|mnt/[A-Za-z]/Users)(?:/|$))",
    re.IGNORECASE,
)
IP_ADDRESS = re.compile(
    r"(?<![\w.])(?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.){3}"
    r"(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(?![\w.])"
)


class EvidenceError(ValueError):
    """A stable, secret-free evidence validation failure."""


def _exact_keys(value: Mapping[str, Any], expected: set[str], label: str) -> None:
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        details = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if extra:
            details.append("unexpected " + ", ".join(extra))
        raise EvidenceError(f"{label} has invalid fields ({'; '.join(details)})")


def _mapping(value: Any, label: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceError(f"{label} must be an object")
    return value


def _safe_text(value: Any, label: str, *, maximum: int = 160) -> str:
    if not isinstance(value, str) or not value or len(value) > maximum:
        raise EvidenceError(f"{label} must be a non-empty bounded string")
    if CONTROL.search(value):
        raise EvidenceError(f"{label} contains control characters")
    if SECRET_VALUE.search(value) or PRIVATE_PATH.search(value) or IP_ADDRESS.search(value):
        raise EvidenceError(f"{label} contains sensitive or machine-specific data")
    return value


def _integer(value: Any, label: str, *, maximum: int = 4_294_967_295) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not -1 <= value <= maximum:
        raise EvidenceError(f"{label} must be a bounded integer")
    return value


def _sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or SHA256.fullmatch(value) is None:
        raise EvidenceError(f"{label} must be a lowercase SHA-256 digest")
    return value


def _artifact_path(value: Any, label: str, root: Path | None) -> str:
    if not isinstance(value, str) or ARTIFACT_PATH.fullmatch(value) is None:
        raise EvidenceError(f"{label} must be a relative evidence path")
    parts = value.split("/")
    if any(part in {"", ".", ".."} for part in parts) or "\\" in value:
        raise EvidenceError(f"{label} contains unsafe path syntax")
    if root is not None:
        _check_artifact(root, parts, label)
    return value


def _check_artifact(root: Path, parts: list[str], label: str) -> None:
    try:
        root_status = root.lstat()
    except OSError as error:
        raise EvidenceError(f"evidence root is unreadable for {label}") from error
    if not stat.S_ISDIR(root_status.st_mode) or stat.S_ISLNK(root_status.st_mode):
        raise EvidenceError(f"evidence root is not a safe directory for {label}")
    current = root
    for index, part in enumerate(parts):
        current /= part
        try:
            status = current.lstat()
        except OSError as error:
            raise EvidenceError(f"{label} does not resolve to a regular file") from error
        if stat.S_ISLNK(status.st_mode):
            raise EvidenceError(f"{label} does not resolve through a safe path")
        if index < len(parts) - 1 and not stat.S_ISDIR(status.st_mode):
            raise EvidenceError(f"{label} has a non-directory path component")
        if index == len(parts) - 1 and not stat.S_ISREG(status.st_mode):
            raise EvidenceError(f"{label} does not resolve to a safe regular file")
    if status.st_size > MAX_ARTIFACT_BYTES:
        raise EvidenceError(f"{label} exceeds the bounded artifact size")


def _timestamp(value: Any) -> str:
    if not isinstance(value, str) or UTC_TIMESTAMP.fullmatch(value) is None:
        raise EvidenceError("recorded_at_utc must be an RFC 3339 UTC timestamp")
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError as error:
        raise EvidenceError("recorded_at_utc is not a valid timestamp") from error
    return value


def _validate_source(value: Any, status: str) -> Mapping[str, Any]:
    source = _mapping(value, "source")
    _exact_keys(source, {"repository", "commit", "dirty", "profile", "package_sha256"}, "source")
    if source["repository"] != "atrinik/classic":
        raise EvidenceError("source.repository must be atrinik/classic")
    if not isinstance(source["commit"], str) or COMMIT.fullmatch(source["commit"]) is None:
        raise EvidenceError("source.commit must be a lowercase 40-character commit")
    if not isinstance(source["dirty"], bool):
        raise EvidenceError("source.dirty must be boolean")
    if not isinstance(source["profile"], str) or PROFILE.fullmatch(source["profile"]) is None:
        raise EvidenceError("source.profile must be a bounded safe profile name")
    package_sha = source["package_sha256"]
    if package_sha is not None:
        _sha(package_sha, "source.package_sha256")
    if status == "passed":
        if source["dirty"]:
            raise EvidenceError("a passed preflight must use a clean source")
        if package_sha is None:
            raise EvidenceError("a passed preflight must include the package digest")
    return source


def _validate_host(value: Any) -> Mapping[str, Any]:
    host = _mapping(value, "host")
    _exact_keys(host, {"os", "os_build", "architecture"}, "host")
    os_name = _safe_text(host["os"], "host.os")
    if not os_name.startswith("Windows"):
        raise EvidenceError("host.os must identify Windows")
    _safe_text(host["os_build"], "host.os_build")
    if host["architecture"] not in ARCHITECTURES:
        raise EvidenceError("host.architecture is unsupported")
    return host


def _validate_gpu(value: Any, status: str) -> Mapping[str, Any]:
    gpu = _mapping(value, "gpu")
    _exact_keys(
        gpu,
        {
            "backend",
            "adapter",
            "device",
            "driver_name",
            "driver_version",
            "qualified_hardware",
            "hardware_tier",
        },
        "gpu",
    )
    if gpu["backend"] not in BACKENDS:
        raise EvidenceError("gpu.backend is unsupported")
    for field in ("adapter", "device", "driver_name", "driver_version"):
        _safe_text(gpu[field], f"gpu.{field}")
    if not isinstance(gpu["qualified_hardware"], bool):
        raise EvidenceError("gpu.qualified_hardware must be boolean")
    if gpu["hardware_tier"] not in HARDWARE_TIERS:
        raise EvidenceError("gpu.hardware_tier is unsupported")
    if status == "passed":
        if gpu["backend"] != "direct3d12":
            raise EvidenceError("a passed native Windows preflight must use direct3d12")
        if not gpu["qualified_hardware"] or gpu["hardware_tier"] not in {"reference", "minimum"}:
            raise EvidenceError("a passed preflight must attest qualified reference or minimum hardware")
        if any(gpu[field] == "<unavailable>" for field in ("adapter", "device", "driver_name", "driver_version")):
            raise EvidenceError("a passed preflight must include complete GPU identity")
    return gpu


def _validate_commands(value: Any, root: Path | None) -> dict[str, Mapping[str, Any]]:
    if not isinstance(value, list) or len(value) != len(COMMAND_NAMES):
        raise EvidenceError("commands must contain each required command exactly once")
    result: dict[str, Mapping[str, Any]] = {}
    for expected_name, raw_command in zip(COMMAND_NAMES, value, strict=True):
        command = _mapping(raw_command, f"commands.{expected_name}")
        _exact_keys(
            command,
            {"name", "status", "exit_code", "stdout_path", "stderr_path"},
            f"commands.{expected_name}",
        )
        if command["name"] != expected_name or expected_name in result:
            raise EvidenceError("commands must be in the prescribed order without duplicates")
        command_status = command["status"]
        if command_status not in COMMAND_STATUSES:
            raise EvidenceError(f"commands.{expected_name}.status is unsupported")
        exit_code = command["exit_code"]
        if command_status == "not-run":
            if exit_code is not None:
                raise EvidenceError(f"commands.{expected_name} not-run requires null exit_code")
        else:
            _integer(exit_code, f"commands.{expected_name}.exit_code")
            if command_status == "passed" and exit_code != 0:
                raise EvidenceError(f"commands.{expected_name} passed requires exit code 0")
            if command_status == "failed" and exit_code == 0:
                raise EvidenceError(f"commands.{expected_name} failed requires a nonzero exit code")
        _artifact_path(command["stdout_path"], f"commands.{expected_name}.stdout_path", root)
        _artifact_path(command["stderr_path"], f"commands.{expected_name}.stderr_path", root)
        result[expected_name] = command
    return result


def _validate_benchmark(value: Any, root: Path | None) -> Mapping[str, Any]:
    benchmark = _mapping(value, "benchmark")
    _exact_keys(benchmark, {"status", "performance_json_path", "records"}, "benchmark")
    if benchmark["status"] not in COMMAND_STATUSES:
        raise EvidenceError("benchmark.status is unsupported")
    records = _integer(benchmark["records"], "benchmark.records", maximum=100_000)
    path = benchmark["performance_json_path"]
    if benchmark["status"] == "not-run":
        if path is not None or records != 0:
            raise EvidenceError("a not-run benchmark requires null path and zero records")
    else:
        if path is None:
            raise EvidenceError("a run benchmark requires performance_json_path")
        if not str(path).lower().endswith((".json", ".jsonl")):
            raise EvidenceError("performance_json_path must identify JSON or JSONL")
        _artifact_path(path, "benchmark.performance_json_path", root)
    if benchmark["status"] == "passed" and records < 3:
        raise EvidenceError("a passed benchmark must contain at least three fresh records")
    return benchmark


def _validate_logs(value: Any, root: Path | None) -> Mapping[str, Any]:
    logs = _mapping(value, "logs")
    _exact_keys(logs, {"client", "server", "coordinator"}, "logs")
    for field in ("client", "server", "coordinator"):
        _artifact_path(logs[field], f"logs.{field}", root)
    return logs


def _validate_cleanup(value: Any) -> Mapping[str, Any]:
    cleanup = _mapping(value, "cleanup")
    _exact_keys(cleanup, {"status", "actions", "exit_codes"}, "cleanup")
    if cleanup["status"] not in {"passed", "failed"}:
        raise EvidenceError("cleanup.status must be passed or failed")
    actions = cleanup["actions"]
    exit_codes = cleanup["exit_codes"]
    if not isinstance(actions, list) or not 1 <= len(actions) <= 16:
        raise EvidenceError("cleanup.actions must contain one to sixteen actions")
    if not isinstance(exit_codes, list) or len(exit_codes) != len(actions):
        raise EvidenceError("cleanup.exit_codes must match cleanup.actions")
    for index, action in enumerate(actions):
        _safe_text(action, f"cleanup.actions[{index}]")
        _integer(exit_codes[index], f"cleanup.exit_codes[{index}]")
    if cleanup["status"] == "passed" and any(code != 0 for code in exit_codes):
        raise EvidenceError("passed cleanup requires all zero exit codes")
    if cleanup["status"] == "failed" and all(code == 0 for code in exit_codes):
        raise EvidenceError("failed cleanup requires a nonzero exit code")
    return cleanup


def _validate_failure(
    value: Any,
    *,
    status: str,
    classification: str,
    next_action: str,
    commands: Mapping[str, Mapping[str, Any]],
    cleanup: Mapping[str, Any],
    benchmark: Mapping[str, Any],
) -> None:
    if status == "passed":
        if classification != "passed" or next_action != "none" or value is not None:
            raise EvidenceError("a passed preflight requires passed classification, none next action, and no failure")
        return
    if classification == "passed" or next_action == "none":
        raise EvidenceError("a failed preflight requires a failure classification and actionable next action")
    failure = _mapping(value, "failure")
    _exact_keys(failure, {"command", "message"}, "failure")
    command_name = failure["command"]
    if command_name not in COMMAND_NAMES or commands[command_name]["status"] != "failed":
        raise EvidenceError("failure.command must identify a failed required command")
    _safe_text(failure["message"], "failure.message")
    expected = {
        "cross-build": {"cross-build"},
        "package-handoff": {"package-handoff"},
        "windows-client-startup-runtime": {"native-package-smoke"},
        "gpu-backend-device": {"native-package-smoke", "d3d12-benchmark"},
        "benchmark": {"d3d12-benchmark"},
        "linux-only-coordinator": {"linux-coordinator-diagnostics"},
        "cleanup": {"cleanup"},
    }
    if command_name not in expected.get(classification, set()):
        raise EvidenceError("failure.command does not match failure classification")
    if classification == "benchmark" and benchmark["status"] != "failed":
        raise EvidenceError("benchmark failures require benchmark.status failed")
    if cleanup["status"] == "failed" and classification != "cleanup":
        raise EvidenceError("any cleanup failure must be classified as cleanup")
    if classification == "cleanup" and cleanup["status"] != "failed":
        raise EvidenceError("cleanup classification requires cleanup.status failed")


def validate_evidence(document: Any, *, root: Path | None = None) -> Mapping[str, Any]:
    """Validate one evidence object and return it for callers that need it."""

    evidence = _mapping(document, "evidence")
    _exact_keys(
        evidence,
        {
            "schema_version",
            "kind",
            "recorded_at_utc",
            "status",
            "classification",
            "next_action",
            "source",
            "host",
            "gpu",
            "commands",
            "benchmark",
            "logs",
            "cleanup",
            "failure",
        },
        "evidence",
    )
    if evidence["schema_version"] != SCHEMA_VERSION:
        raise EvidenceError("schema_version is unsupported")
    if evidence["kind"] != KIND:
        raise EvidenceError("kind is unsupported")
    _timestamp(evidence["recorded_at_utc"])
    status = evidence["status"]
    if status not in {"passed", "failed"}:
        raise EvidenceError("status must be passed or failed")
    classification = evidence["classification"]
    if classification not in FAILURE_CLASSES:
        raise EvidenceError("classification is unsupported")
    next_action = _safe_text(evidence["next_action"], "next_action")
    _validate_source(evidence["source"], status)
    _validate_host(evidence["host"])
    _validate_gpu(evidence["gpu"], status)
    commands = _validate_commands(evidence["commands"], root)
    benchmark = _validate_benchmark(evidence["benchmark"], root)
    _validate_logs(evidence["logs"], root)
    cleanup = _validate_cleanup(evidence["cleanup"])
    _validate_failure(
        evidence["failure"],
        status=status,
        classification=classification,
        next_action=next_action,
        commands=commands,
        cleanup=cleanup,
        benchmark=benchmark,
    )
    if status == "passed":
        if any(
            commands[name]["status"] != "passed"
            for name in COMMAND_NAMES
            if name != "linux-coordinator-diagnostics"
        ):
            raise EvidenceError("a passed preflight requires all package and cleanup commands to pass")
        if commands["linux-coordinator-diagnostics"]["status"] not in {"passed", "not-run"}:
            raise EvidenceError("a passed preflight cannot contain a failed Linux coordinator diagnostic")
        if benchmark["status"] != "passed":
            raise EvidenceError("a passed preflight requires a passed benchmark")
        if cleanup["status"] != "passed":
            raise EvidenceError("a passed preflight requires successful cleanup")
    return evidence


def _load(path: Path) -> Any:
    try:
        status = path.lstat()
    except OSError as error:
        raise EvidenceError("evidence input is not readable") from error
    if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
        raise EvidenceError("evidence input must be a regular file")
    if status.st_size > MAX_EVIDENCE_BYTES:
        raise EvidenceError("evidence input exceeds the bounded size")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise EvidenceError("evidence input is not valid UTF-8 JSON") from error


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument(
        "--root",
        type=Path,
        help="optional artifact root; when supplied every evidence path must exist below it",
    )
    arguments = parser.parse_args(argv)
    try:
        evidence = validate_evidence(_load(arguments.evidence), root=arguments.root)
    except (EvidenceError, OSError) as error:
        print(f"windows GPU evidence invalid: {error}", file=sys.stderr)
        return 1
    print(
        json.dumps(
            {
                "classification": evidence["classification"],
                "evidence_status": evidence["status"],
                "kind": KIND,
                "schema_version": SCHEMA_VERSION,
                "status": "valid",
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
