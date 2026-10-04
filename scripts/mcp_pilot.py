#!/usr/bin/env python3
"""Run the optional, read-only Atrinik MCP pilot from a reviewed config."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PILOT_ROOT = ROOT / "mcp" / "pilot"
WORKLOADS = ROOT / "mcp" / "contract" / "v1" / "fixtures" / "workloads.json"
SCENARIOS = (
    "enabled",
    "fully-disabled-baseline",
    "server-down",
    "stale-coordinate",
    "auth-revoked",
    "offline",
    "disabled",
)
FAILURE_CODES = {
    "server-down": "OFFLINE",
    "stale-coordinate": "STALE_COORDINATE",
    "auth-revoked": "UNAUTHORIZED",
    "offline": "OFFLINE",
    "disabled": "UNSUPPORTED_OPERATION",
    "fully-disabled-baseline": "UNSUPPORTED_OPERATION",
}
PROFILE_IDS = {"context", "search", "content", "runtime-status", "external-evaluation"}
TOP_LEVEL_KEYS = {
    "$schema", "schema_version", "evidence_kind", "pilot_enabled", "trust_root",
    "expected_head", "profiles",
}
TOP_LEVEL_REQUIRED = TOP_LEVEL_KEYS - {"$schema"}
PROFILE_KEYS = {
    "profile_id", "server_id", "owner", "revision", "enabled", "runtime_opt_in",
    "evaluation_only", "adapter",
}
ADAPTER_KEYS = {"argv", "pin_path", "sha256"}
METRIC_FIELDS = {
    "calls", "retries", "tool_schema_bytes", "context_bytes", "cache_hits",
    "cache_misses",
}
FULL_SHA = re.compile(r"^[0-9a-f]{40}$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
SAFE_TOKEN = re.compile(r"^[^\x00-\x1f\x7f]*$")
FORBIDDEN_WORDS = re.compile(
    r"(?i)(password|passwd|secret|token|authorization|api[_-]?key|"
    r"\brm\b|\bdelete\b|\bwrite\b|\bapply\b|\bdeploy\b|\bshell\b)"
)


class PilotError(ValueError):
    pass


def _load(path: Path) -> Any:
    def unique(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate key: {key}")
            result[key] = value
        return result

    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)
    except (OSError, UnicodeError, ValueError) as error:
        raise PilotError(f"invalid JSON: {path.name}") from error


def _canonical(value: object) -> bytes:
    return json.dumps(
        value, ensure_ascii=True, allow_nan=False, separators=(",", ":"), sort_keys=True
    ).encode("utf-8")


def _sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _safe_relative(value: object, label: str) -> Path:
    if not isinstance(value, str) or not value or not SAFE_TOKEN.fullmatch(value):
        raise PilotError(f"{label} must be a printable relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise PilotError(f"{label} must be a portable relative path")
    return path


def _validate_argv(argv: object) -> list[str]:
    if not isinstance(argv, list) or not argv:
        raise PilotError("adapter argv must be a non-empty array")
    if not all(isinstance(item, str) and item and SAFE_TOKEN.fullmatch(item) for item in argv):
        raise PilotError("adapter argv contains an invalid token")
    joined = " ".join(argv)
    if FORBIDDEN_WORDS.search(joined):
        raise PilotError("adapter argv requests a secret, broad shell, or mutation surface")
    if any(any(character in item for character in ";|&`$><\n\r") for item in argv):
        raise PilotError("adapter argv contains shell syntax")
    allowed = {"{workload_id}", "{scenario}"}
    for item in argv:
        for match in re.findall(r"\{[^{}]+\}", item):
            if match not in allowed:
                raise PilotError("adapter argv contains an unsupported placeholder")
    return argv


def validate_config(path: Path) -> dict[str, Any]:
    config = _load(path)
    if not isinstance(config, dict) or config.get("schema_version") != "atrinik.mcp.pilot-config/v1":
        raise PilotError("unsupported pilot config schema")
    if set(config) - TOP_LEVEL_KEYS or not TOP_LEVEL_REQUIRED.issubset(config):
        raise PilotError("pilot config fields do not match the schema")
    kind = config.get("evidence_kind")
    if kind not in {"real-pilot", "synthetic-self-test"}:
        raise PilotError("invalid evidence kind")
    if not isinstance(config.get("pilot_enabled"), bool):
        raise PilotError("pilot_enabled must be boolean")
    trust_root = _safe_relative(config.get("trust_root"), "trust_root")
    expected_head = config.get("expected_head")
    if expected_head is not None and (
        not isinstance(expected_head, str) or not FULL_SHA.fullmatch(expected_head)
    ):
        raise PilotError("expected_head must be a full revision")
    if kind == "real-pilot" and config["pilot_enabled"] and expected_head is None:
        raise PilotError("an enabled real pilot requires expected_head")
    profiles = config.get("profiles")
    if not isinstance(profiles, list) or not profiles:
        raise PilotError("profiles must be a non-empty array")
    seen: set[str] = set()
    for profile in profiles:
        if not isinstance(profile, dict):
            raise PilotError("profile must be an object")
        required_profile = PROFILE_KEYS - {"adapter"}
        if set(profile) - PROFILE_KEYS or not required_profile.issubset(profile):
            raise PilotError("profile fields do not match the schema")
        profile_id = profile.get("profile_id")
        if profile_id not in PROFILE_IDS or profile_id in seen:
            raise PilotError("profile_id is unknown or duplicated")
        seen.add(profile_id)
        for field in ("enabled", "runtime_opt_in", "evaluation_only"):
            if not isinstance(profile.get(field), bool):
                raise PilotError(f"{profile_id}.{field} must be boolean")
        if profile_id == "runtime-status" and profile["enabled"] and not profile["runtime_opt_in"]:
            raise PilotError("runtime status requires separate opt-in")
        if profile_id == "external-evaluation" and not profile["evaluation_only"]:
            raise PilotError("external connectors are evaluation-only")
        for field in ("server_id", "owner", "revision"):
            value = profile.get(field)
            if (
                not isinstance(value, str)
                or not value
                or len(value) > 160
                or not SAFE_TOKEN.fullmatch(value)
            ):
                raise PilotError(f"{profile_id}.{field} is required")
        command = profile.get("adapter")
        if not profile["enabled"]:
            if command is not None:
                raise PilotError("disabled profiles must not carry executable adapters")
            continue
        if not isinstance(command, dict):
            raise PilotError("enabled profiles require an adapter")
        if set(command) != ADAPTER_KEYS:
            raise PilotError("adapter fields do not match the pinned command contract")
        argv = _validate_argv(command.get("argv"))
        pin = _safe_relative(command.get("pin_path"), "adapter.pin_path")
        digest = command.get("sha256")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise PilotError("adapter sha256 is invalid")
        pin_path = (ROOT / trust_root / pin).resolve()
        root = (ROOT / trust_root).resolve()
        if root not in pin_path.parents and pin_path != root:
            raise PilotError("adapter pin escapes the trust root")
        try:
            payload = pin_path.read_bytes()
        except OSError as error:
            raise PilotError("adapter pin is unavailable") from error
        if _sha256(payload) != digest:
            raise PilotError("adapter pin digest does not match")
        pin_token = pin.as_posix()
        direct = argv[0] == pin_token
        interpreted = len(argv) > 1 and argv[0] in {"python3", "python3.11"} and argv[1] == pin_token
        if not (direct or interpreted):
            raise PilotError("adapter pin is not the executed program")
        if not any("{workload_id}" in token for token in argv) or not any(
            "{scenario}" in token for token in argv
        ):
            raise PilotError("adapter argv must bind workload and scenario")
    if not config["pilot_enabled"] and any(profile["enabled"] for profile in profiles):
        raise PilotError("disabled pilot cannot enable an adapter")
    return config


def _metrics(value: object) -> tuple[dict[str, int | bool], bool]:
    empty: dict[str, int | bool] = {field: 0 for field in METRIC_FIELDS}
    empty["external_network"] = False
    if not isinstance(value, dict) or set(value) != METRIC_FIELDS | {"external_network"}:
        return empty, False
    result: dict[str, int | bool] = {}
    for field in METRIC_FIELDS:
        item = value[field]
        if not isinstance(item, int) or isinstance(item, bool) or item < 0:
            return empty, False
        result[field] = item
    if not isinstance(value["external_network"], bool):
        return empty, False
    result["external_network"] = value["external_network"]
    return result, True


def _git_head(root: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=root, check=True, capture_output=True, timeout=5
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise PilotError("cannot resolve exact pilot HEAD") from error
    head = result.stdout.decode("ascii", "strict").strip()
    if not FULL_SHA.fullmatch(head):
        raise PilotError("git returned an invalid HEAD")
    return head


def _clean_head(root: Path, expected: str) -> str:
    head = _git_head(root)
    if head != expected:
        raise PilotError("real pilot HEAD does not match the pinned revision")
    try:
        status = subprocess.run(
            ["git", "--no-optional-locks", "-c", "core.fsmonitor=false", "status", "--porcelain=v1",
             "--untracked-files=all", "--ignore-submodules=none"],
            cwd=root, check=True, capture_output=True, timeout=5,
        )
        index = subprocess.run(
            ["git", "--no-optional-locks", "ls-files", "-v", "-z", "--", ":/"],
            cwd=root, check=True, capture_output=True, timeout=5,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise PilotError("cannot verify a clean real pilot root") from error
    if status.stdout:
        raise PilotError("real pilot requires a clean root without tracked or untracked changes")
    if any(record and not record.startswith(b"H ") for record in index.stdout.split(b"\0")):
        raise PilotError("real pilot source has hidden index flags or an unsupported index entry")
    if _git_head(root) != head:
        raise PilotError("real pilot HEAD changed during source verification")
    return head


def _interpreter(program: str, root: Path) -> str:
    # Resolve PATH as it will be interpreted from the adapter's working directory.
    # Preserve symlinks, since resolving a virtualenv Python changes its identity.
    search = os.pathsep.join(
        str(Path(entry) if Path(entry).is_absolute() else root / entry)
        for entry in os.environ.get("PATH", "").split(os.pathsep)
    )
    executable = shutil.which(program, path=search)
    if executable is None:
        raise PilotError("configured adapter interpreter is unavailable")
    return os.path.abspath(executable)


def _run_adapter(argv: list[str], *, root: Path, timeout_ms: int) -> tuple[dict[str, Any], dict[str, Any]]:
    started = time.perf_counter_ns()
    limit = 64 * 1024
    with tempfile.TemporaryFile() as stdout_file, tempfile.TemporaryFile() as stderr_file:
        def bound_output() -> None:
            resource.setrlimit(resource.RLIMIT_FSIZE, (limit, limit))

        process = subprocess.Popen(
            argv,
            cwd=root,
            stdin=subprocess.DEVNULL,
            stdout=stdout_file,
            stderr=stderr_file,
            env={"PATH": os.environ.get("PATH", "")},
            start_new_session=True,
            preexec_fn=bound_output,
        )
        try:
            return_code = process.wait(timeout=timeout_ms / 1000)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            return_code = 124
        stdout_file.seek(0)
        stderr_file.seek(0)
        stdout = stdout_file.read(limit + 1)
        stderr = stderr_file.read(limit + 1)
    wall_ns = time.perf_counter_ns() - started
    parsed = True
    try:
        value = json.loads(stdout)
    except (UnicodeError, ValueError):
        parsed = False
        value = {}
    parse_valid = parsed and isinstance(value, dict) and len(stdout) <= limit and len(stderr) <= limit
    if not parse_valid:
        value = {}
    measurement = {
        "return_code": return_code,
        "stdout_bytes": len(stdout),
        "stderr_bytes": len(stderr),
        "stdout_sha256": _sha256(stdout),
        "stderr_sha256": _sha256(stderr),
        "wall_ns": wall_ns,
        "bounded_parse_valid": parse_valid,
    }
    return value, measurement


def run_pilot(config_path: Path, *, enable_real_pilot: bool) -> dict[str, Any]:
    config = validate_config(config_path)
    if config["evidence_kind"] == "real-pilot" and config["pilot_enabled"] and not enable_real_pilot:
        raise PilotError("real pilot requires the explicit CLI gate")
    root = (ROOT / config["trust_root"]).resolve()
    real = config["evidence_kind"] == "real-pilot" and config["pilot_enabled"]
    head = _clean_head(root, config["expected_head"]) if real else None
    exact = real
    workloads = _load(WORKLOADS)["cases"]
    observations: list[dict[str, Any]] = []
    enabled_profiles = [profile for profile in config["profiles"] if profile["enabled"]]
    timeout_ms = 5000
    for profile in enabled_profiles:
        template = profile["adapter"]["argv"]
        pin_token = Path(profile["adapter"]["pin_path"]).as_posix()
        interpreter = _interpreter(template[0], root) if template[0] != pin_token else None
        for case in workloads:
            for scenario in SCENARIOS:
                pin = (root / pin_token).resolve()
                escaped = root not in pin.parents and pin != root
                if escaped or _sha256(pin.read_bytes()) != profile["adapter"]["sha256"]:
                    raise PilotError("adapter pin changed before execution")
                argv = [token.replace("{workload_id}", case["id"]).replace("{scenario}", scenario) for token in template]
                if argv[0] == pin_token:
                    argv[0] = str(pin)
                else:
                    argv[0] = interpreter
                    argv[1] = str(pin)
                if real:
                    _clean_head(root, head)
                adapter, measured = _run_adapter(argv, root=root, timeout_ms=timeout_ms)
                if real:
                    _clean_head(root, head)
                expected_error = FAILURE_CODES.get(scenario)
                reported_metrics, metrics_valid = _metrics(adapter.get("metrics"))
                kind_matches = adapter.get("synthetic") is (
                    config["evidence_kind"] == "synthetic-self-test"
                )
                mutation_count = adapter.get("mutation_count")
                safe_mutation_count = (
                    mutation_count if isinstance(mutation_count, int) and not isinstance(mutation_count, bool) and mutation_count >= 0 else None
                )
                error_code = adapter.get("error_code")
                safe_error_code = (
                    error_code
                    if isinstance(error_code, str) and error_code in set(FAILURE_CODES.values())
                    else None
                )
                fallback_used = adapter.get("fallback_used")
                safe_fallback_used = fallback_used if isinstance(fallback_used, bool) else False
                answer = adapter.get("answer")
                exact_answer = scenario == "enabled" and answer == case["expected"]
                observed_coordinates = answer["coordinates"] if exact_answer else []
                correct = (
                    measured["return_code"] == 0
                    and measured["bounded_parse_valid"]
                    and adapter.get("schema_version") == "atrinik.mcp.pilot-adapter/v1"
                    and adapter.get("workload_id") == case["id"]
                    and adapter.get("scenario") == scenario
                    and safe_mutation_count == 0
                    and kind_matches
                    and metrics_valid
                    and (
                        exact_answer
                        if scenario == "enabled"
                        else safe_error_code == expected_error
                        and safe_fallback_used is True
                    )
                )
                observations.append(
                    {
                        "profile_id": profile["profile_id"],
                        "server_id": profile["server_id"],
                        "owner": profile["owner"],
                        "revision": profile["revision"],
                        "workload_id": case["id"],
                        "scenario": scenario,
                        "coordinates": observed_coordinates,
                        "content_artifact": answer.get("classic_artifact") if exact_answer else None,
                        "correct": correct,
                        "mutation_count": safe_mutation_count,
                        "error_code": safe_error_code if scenario != "enabled" else None,
                        "fallback_used": safe_fallback_used if scenario != "enabled" else False,
                        "reported_metrics": reported_metrics,
                        "measurement": measured,
                    }
                )
    passed = bool(observations) and all(item["correct"] for item in observations)
    # This preparation harness cannot prove review-worktree execution, affected
    # repository checks, independent reproduction, or adoption thresholds by itself.
    real_complete = False
    return {
        "schema_version": "atrinik.mcp.pilot-result/v1",
        "evidence_kind": config["evidence_kind"],
        "synthetic": config["evidence_kind"] == "synthetic-self-test",
        "source": {"commit": head, "expected_commit": config.get("expected_head"), "exact_head": exact},
        "configuration": {
            "pilot_enabled": config["pilot_enabled"],
            "enabled_profiles": [profile["profile_id"] for profile in enabled_profiles],
            "runtime_enabled": any(p["profile_id"] == "runtime-status" for p in enabled_profiles),
            "external_enabled": any(p["profile_id"] == "external-evaluation" for p in enabled_profiles),
        },
        "coverage": {"workloads": len(workloads), "scenarios": list(SCENARIOS)},
        "observations": observations,
        "correct": passed if enabled_profiles else True,
        "real_pilot_complete": real_complete,
        "adoption_claimed": False,
        "privacy": {
            "raw_output_recorded": False,
            "credentials_recorded": False,
            "host_paths_recorded": False,
        },
    }


def _write(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}")
    try:
        temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    validate = commands.add_parser("validate")
    validate.add_argument("--config", type=Path, required=True)
    run = commands.add_parser("run")
    run.add_argument("--config", type=Path, required=True)
    run.add_argument("--output", type=Path)
    run.add_argument("--enable-real-pilot", action="store_true")
    self_test = commands.add_parser("self-test")
    self_test.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "validate":
            config = validate_config(args.config)
            result: object = {"valid": True, "profiles": len(config["profiles"])}
        else:
            config_path = args.config if args.command == "run" else PILOT_ROOT / "config.synthetic.example.json"
            result = run_pilot(config_path, enable_real_pilot=getattr(args, "enable_real_pilot", False))
            if args.output is not None:
                _write(args.output, result)
    except (PilotError, OSError) as error:
        print(f"MCP pilot failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
