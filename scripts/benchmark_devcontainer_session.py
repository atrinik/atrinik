#!/usr/bin/env python3
"""Measure cold, warm, parallel, and recovery behavior for devcontainer sessions."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys
from threading import Lock
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from atrinik_workspace.docker_storage import volume_namespace
from atrinik_workspace.model import WorkspaceError


DEFAULT_LINUX_IMAGE = (
    "ghcr.io/atrinik/linux-build:1.3.0@sha256:"
    "260658d2709e993b41148a9d8f724c2d2f7f1fd93543a139b00d139b10e7f31a"
)
NAMESPACE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_.-]{0,62}$")
RUN_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{0,30}$")
ROLE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{0,30}$")
PINNED_IMAGE_PATTERN = re.compile(r"^[^\s@]+@sha256:[0-9a-f]{64}$")
MAX_DOCKER_NAME = 255
SESSION_IDLE_TIMEOUT_SECONDS = 30 * 60
SESSION_LIFETIME_SECONDS = 12 * 60 * 60

SESSION_PROCESS_SCRIPT = r"""set -euo pipefail
trap 'exit 0' TERM INT
sleep 43200
"""

SESSION_WORKLOAD_SCRIPT = r"""set -euo pipefail
if [ "$#" -ne 2 ]; then
    exit 2
fi
root="$1"
count="$2"
mkdir -p "$root/cache"
created=0
reused=0
for index in $(seq 0 "$((count - 1))"); do
    target="$root/cache/input-$index.bin"
    if [ -f "$target" ]; then
        reused=$((reused + 1))
    else
        printf 'atrinik-devcontainer-session:%s\n' "$index" > "$target"
        created=$((created + 1))
    fi
done
sha256sum "$root"/cache/input-*.bin | sort > "$root/checksums.txt"
bytes=$(wc -c < "$root/checksums.txt")
digest=$(sha256sum "$root/checksums.txt" | cut -d ' ' -f1)
printf 'created=%s reused=%s bytes=%s digest=%s\n' \
    "$created" "$reused" "$bytes" "$digest"
"""


class BenchmarkError(RuntimeError):
    """The benchmark cannot safely continue."""


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _future_utc(seconds: int) -> str:
    return (
        datetime.now(timezone.utc) + timedelta(seconds=seconds)
    ).isoformat().replace("+00:00", "Z")


def _validate_image(image: str) -> str:
    if PINNED_IMAGE_PATTERN.fullmatch(image) is None:
        raise BenchmarkError("image must include a lowercase sha256 digest")
    return image


def _validate_namespace(namespace: str) -> str:
    if NAMESPACE_PATTERN.fullmatch(namespace) is None:
        raise BenchmarkError("namespace is not Docker-safe")
    return namespace


def _validate_run_id(run_id: str) -> str:
    if RUN_ID_PATTERN.fullmatch(run_id) is None:
        raise BenchmarkError("run id must contain only lowercase Docker-safe characters")
    return run_id


def _validate_role(role: str) -> str:
    if ROLE_PATTERN.fullmatch(role) is None:
        raise BenchmarkError("session role is not Docker-safe")
    return role


def _volume_name(namespace: str, run_id: str, role: str) -> str:
    _validate_namespace(namespace)
    _validate_run_id(run_id)
    _validate_role(role)
    name = f"atrinik-{namespace}-session-{run_id}-{role}-volume"
    if len(name) > MAX_DOCKER_NAME:
        raise BenchmarkError("session volume name exceeds the Docker limit")
    return name


def _container_name(namespace: str, run_id: str, role: str) -> str:
    _validate_namespace(namespace)
    _validate_run_id(run_id)
    _validate_role(role)
    name = f"atrinik-{namespace}-session-{run_id}-{role}"
    if len(name) > MAX_DOCKER_NAME:
        raise BenchmarkError("session container name exceeds the Docker limit")
    return name


def _docker(
    arguments: list[str],
    *,
    check: bool = True,
    timeout: int = 600,
) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(
            ["docker", *arguments],
            check=check,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
    except FileNotFoundError as error:
        raise BenchmarkError("Docker CLI is unavailable") from error
    except subprocess.TimeoutExpired as error:
        raise BenchmarkError(
            f"Docker command timed out: {' '.join(arguments[:3])}"
        ) from error
    if check and result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        if len(detail) > 2000:
            detail = detail[-2000:]
        raise BenchmarkError(
            f"Docker command failed ({' '.join(arguments[:3])}): {detail}"
        )
    return result


def _docker_fact(arguments: list[str]) -> str:
    try:
        result = _docker(arguments, check=False, timeout=30)
    except BenchmarkError:
        return "unavailable"
    if result.returncode != 0:
        return "unavailable"
    return (result.stdout or "").strip()[:256] or "unknown"


def _is_missing(result: subprocess.CompletedProcess[str]) -> bool:
    detail = (result.stderr or result.stdout or "").lower()
    return any(
        marker in detail for marker in ("no such", "not found", "does not exist")
    )


def _ensure_container_absent(name: str) -> None:
    result = _docker(["container", "inspect", name], check=False, timeout=30)
    if result.returncode == 0:
        raise BenchmarkError(f"refusing to reuse pre-existing session container: {name}")
    if not _is_missing(result):
        raise BenchmarkError(f"could not prove session container is absent: {name}")


def _create_volume(name: str, run_id: str) -> None:
    result = _docker(["volume", "inspect", name], check=False, timeout=30)
    if result.returncode == 0:
        raise BenchmarkError(f"refusing to reuse pre-existing session volume: {name}")
    if not _is_missing(result):
        raise BenchmarkError(f"could not prove session volume is absent: {name}")
    _docker(
        [
            "volume",
            "create",
            "--label",
            "atrinik.benchmark=devcontainer-session",
            "--label",
            f"atrinik.session={run_id}",
            name,
        ],
        timeout=60,
    )


def _volume_mount(name: str, target: str = "/session") -> str:
    return f"type=volume,source={name},target={target},volume-nocopy"


def _parse_workload_output(stdout: str) -> dict[str, Any]:
    line = next(
        (candidate for candidate in reversed(stdout.splitlines()) if candidate),
        "",
    )
    values: dict[str, Any] = {}
    for item in line.split():
        key, separator, value = item.partition("=")
        if not separator:
            continue
        if key in {"created", "reused", "bytes"}:
            try:
                parsed = int(value)
            except ValueError as error:
                raise BenchmarkError("session workload returned invalid counters") from error
            if parsed < 0:
                raise BenchmarkError("session workload returned a negative counter")
            values[key] = parsed
        elif key == "digest":
            if re.fullmatch(r"[0-9a-f]{64}", value) is None:
                raise BenchmarkError("session workload returned an invalid digest")
            values[key] = value
    if set(values) != {"created", "reused", "bytes", "digest"}:
        raise BenchmarkError("session workload returned incomplete evidence")
    return values


def _start_session(
    image: str,
    name: str,
    volume: str,
    run_id: str,
) -> dict[str, Any]:
    _ensure_container_absent(name)
    created = False
    try:
        _docker(
            [
                "run",
                "--detach",
                "--name",
                name,
                "--user",
                "0:0",
                "--label",
                "atrinik.benchmark=devcontainer-session",
                "--label",
                f"atrinik.session={run_id}",
                "--mount",
                _volume_mount(volume),
                image,
                "bash",
                "-euc",
                SESSION_PROCESS_SCRIPT,
            ],
            timeout=60,
        )
        created = True
        inspection = _docker(
            [
                "container",
                "inspect",
                "--format",
                "{{.Id}}\t{{.Config.Image}}\t{{.State.Running}}",
                name,
            ],
            timeout=30,
        )
        fields = (inspection.stdout or "").strip().split("\t")
        if len(fields) != 3 or fields[2] != "true":
            raise BenchmarkError(f"session container is not running: {name}")
        if fields[1] != image:
            raise BenchmarkError(f"session container image drifted: {name}")
        if re.fullmatch(r"[0-9a-f]{12,64}", fields[0]) is None:
            raise BenchmarkError(f"session container identity is invalid: {name}")
    except (BenchmarkError, OSError):
        if created:
            try:
                _docker(
                    ["container", "rm", "--force", name],
                    check=False,
                    timeout=60,
                )
            except BenchmarkError:
                pass
        raise
    return {
        "name": name,
        "container_id": fields[0],
        "image": image,
        "reported_image": fields[1],
        "started_at": _utc_now(),
    }


def _timed_cold_workload(
    image: str,
    name: str,
    run_id: str,
    file_count: int,
) -> dict[str, Any]:
    _ensure_container_absent(name)
    started = time.perf_counter()
    result = _docker(
        [
            "run",
            "--rm",
            "--name",
            name,
            "--user",
            "0:0",
            "--label",
            "atrinik.benchmark=devcontainer-session",
            "--label",
            f"atrinik.session={run_id}",
            image,
            "bash",
            "-euc",
            SESSION_WORKLOAD_SCRIPT,
            "--",
            "/tmp/atrinik-session-work",
            str(file_count),
        ]
    )
    evidence = _parse_workload_output(result.stdout or "")
    evidence["duration_ms"] = round((time.perf_counter() - started) * 1000, 3)
    return evidence


def _timed_session_workload(
    container: str,
    file_count: int,
) -> dict[str, Any]:
    started = time.perf_counter()
    result = _docker(
        [
            "exec",
            container,
            "bash",
            "-euc",
            SESSION_WORKLOAD_SCRIPT,
            "--",
            "/session",
            str(file_count),
        ]
    )
    evidence = _parse_workload_output(result.stdout or "")
    evidence["duration_ms"] = round((time.perf_counter() - started) * 1000, 3)
    return evidence


def _stop_exact_container(name: str) -> dict[str, Any]:
    state = _docker(
        ["container", "inspect", "--format", "{{.State.Running}}", name],
        check=False,
        timeout=30,
    )
    if state.returncode != 0 or not (state.stdout or "").strip():
        raise BenchmarkError(f"could not prove session container state: {name}")
    removed = _docker(["container", "rm", "--force", name], check=False, timeout=60)
    if removed.returncode != 0:
        raise BenchmarkError(f"could not stop the exact session container: {name}")
    return {
        "container": name,
        "was_running": (state.stdout or "").strip() == "true",
        "removed": True,
        "observed_at": _utc_now(),
    }


def _parallel_session(
    image: str,
    name: str,
    volume: str,
    run_id: str,
    file_count: int,
    owned_containers: list[str],
    owned_containers_lock: Lock,
) -> dict[str, Any]:
    started = time.perf_counter()
    session = _start_session(image, name, volume, run_id)
    with owned_containers_lock:
        owned_containers.append(name)
    workload = _timed_session_workload(name, file_count)
    return {
        "container": name,
        "volume": volume,
        "session": session,
        "workload": workload,
        "duration_ms": round((time.perf_counter() - started) * 1000, 3),
    }


def _cleanup_containers(
    names: list[str],
    observations: list[dict[str, Any]],
) -> None:
    for name in reversed(list(dict.fromkeys(names))):
        try:
            result = _docker(
                ["container", "rm", "--force", name],
                check=False,
                timeout=60,
            )
            removed = result.returncode == 0
            if not removed:
                absent = _docker(
                    ["container", "inspect", name],
                    check=False,
                    timeout=30,
                )
                removed = absent.returncode != 0 and _is_missing(absent)
            observation: dict[str, Any] = {
                "container": name,
                "removed": removed,
            }
            if not removed:
                observation["error"] = (
                    (result.stderr or result.stdout or "").strip()[-500:]
                )
            observations.append(observation)
        except BenchmarkError as error:
            observations.append(
                {"container": name, "removed": False, "error": str(error)}
            )


def _cleanup_volumes(
    names: list[str],
    keep: bool,
    observations: list[dict[str, Any]],
) -> None:
    for name in reversed(list(dict.fromkeys(names))):
        if keep:
            observations.append(
                {"volume": name, "removed": False, "retained": True}
            )
            continue
        try:
            result = _docker(
                ["volume", "rm", name],
                check=False,
                timeout=60,
            )
            removed = result.returncode == 0
            if not removed:
                absent = _docker(
                    ["volume", "inspect", name],
                    check=False,
                    timeout=30,
                )
                removed = absent.returncode != 0 and _is_missing(absent)
            observation = {"volume": name, "removed": removed}
            if not removed:
                observation["error"] = (
                    (result.stderr or result.stdout or "").strip()[-500:]
                )
            observations.append(observation)
        except BenchmarkError as error:
            observations.append(
                {"volume": name, "removed": False, "error": str(error)}
            )


def _docker_desktop_indicator(values: list[str]) -> bool | None:
    usable = [
        value.lower()
        for value in values
        if value not in {"unknown", "unavailable"}
    ]
    if not usable:
        return None
    return any("docker desktop" in value for value in usable)


def _comparison(
    cold: list[dict[str, Any]],
    warm: list[dict[str, Any]],
    parallel_wall_ms: float,
    parallel: list[dict[str, Any]],
) -> dict[str, Any]:
    cold_median = float(statistics.median(item["duration_ms"] for item in cold))
    warm_median = float(statistics.median(item["duration_ms"] for item in warm))
    parallel_serial = sum(item["duration_ms"] for item in parallel)
    return {
        "cold_median_ms": round(cold_median, 3),
        "warm_median_ms": round(warm_median, 3),
        "cold_to_warm_ratio": (
            round(cold_median / warm_median, 3) if warm_median else None
        ),
        "parallel_wall_ms": round(parallel_wall_ms, 3),
        "parallel_serial_work_ms": round(parallel_serial, 3),
        "parallel_speedup_vs_serial": (
            round(parallel_serial / parallel_wall_ms, 3)
            if parallel_wall_ms
            else None
        ),
    }


def run_benchmark(
    *,
    image: str,
    namespace: str,
    run_id: str,
    iterations: int,
    parallel_sessions: int,
    file_count: int,
    keep_volumes: bool,
) -> tuple[dict[str, Any], bool]:
    volumes: list[str] = []
    containers: list[str] = []
    cleanup: list[dict[str, Any]] = []
    environment = {
        "platform": platform.platform(aliased=True),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "docker_client": _docker_fact(
            ["version", "--format", "{{.Client.Version}}"]
        ),
        "docker_server": _docker_fact(
            ["version", "--format", "{{.Server.Version}}"]
        ),
        "docker_driver": _docker_fact(["info", "--format", "{{.Driver}}"]),
        "docker_os": _docker_fact(["info", "--format", "{{.OperatingSystem}}"]),
        "docker_kernel": _docker_fact(["info", "--format", "{{.KernelVersion}}"]),
        "docker_architecture": _docker_fact(
            ["info", "--format", "{{.Architecture}}"]
        ),
    }
    environment["docker_desktop"] = _docker_desktop_indicator(
        [
            environment["docker_server"],
            environment["docker_driver"],
            environment["docker_os"],
        ]
    )
    report: dict[str, Any] = {
        "schema": 1,
        "benchmark": "devcontainer-session",
        "generated_at": _utc_now(),
        "image": image,
        "namespace": namespace,
        "run_id": run_id,
        "environment": environment,
        "workload": {
            "files": file_count,
            "iterations": iterations,
            "parallel_sessions": parallel_sessions,
        },
        "session_contract": {
            "record_is_authority": False,
            "authority": "live coordinator probe, delivery ledger, worktree, CAS, and leases",
            "source_mounts": [],
            "credentials": "none",
            "mutable_server_state": "none",
            "idle_timeout_seconds": SESSION_IDLE_TIMEOUT_SECONDS,
            "maximum_lifetime_seconds": SESSION_LIFETIME_SECONDS,
        },
        "session_record": {
            "agent_identity": "synthetic benchmark",
            "delivery_scope": f"benchmark:{run_id}",
            "checkout_worktree": None,
            "profile": None,
            "container_identity": None,
            "source_mounts": [],
            "named_volumes": [],
            "started_at": None,
            "last_activity_at": None,
            "idle_deadline": None,
            "active_services": [],
            "cleanup_owner": "benchmark",
        },
    }
    failure: str | None = None
    try:
        cold = []
        for index in range(iterations):
            cold_container = _container_name(
                namespace,
                run_id,
                f"cold-{index:02d}",
            )
            cold.append(
                _timed_cold_workload(
                    image,
                    cold_container,
                    run_id,
                    file_count,
                )
            )
            containers.append(cold_container)

        warm_volume = _volume_name(namespace, run_id, "warm")
        _create_volume(warm_volume, run_id)
        volumes.append(warm_volume)
        warm_container = _container_name(namespace, run_id, "warm")
        startup_started = time.perf_counter()
        warm_session = _start_session(
            image,
            warm_container,
            warm_volume,
            run_id,
        )
        containers.append(warm_container)
        warm_session["startup_duration_ms"] = round(
            (time.perf_counter() - startup_started) * 1000,
            3,
        )
        report["session_record"].update(
            {
                "container_identity": warm_session,
                "named_volumes": [
                    {"name": warm_volume, "target": "/session"}
                ],
                "started_at": warm_session["started_at"],
                "last_activity_at": _utc_now(),
                "idle_deadline": _future_utc(SESSION_IDLE_TIMEOUT_SECONDS),
            }
        )
        warm_runs = [
            _timed_session_workload(warm_container, file_count)
            for _ in range(iterations)
        ]
        if warm_runs[-1]["reused"] != file_count:
            raise BenchmarkError("warm session did not reuse its named volume")
        report["cold"] = cold
        report["warm"] = {
            "volume": warm_volume,
            "container": warm_container,
            "session": warm_session,
            "runs": warm_runs,
        }

        before_stop = _timed_session_workload(warm_container, file_count)
        stopped = _stop_exact_container(warm_container)
        recovery_container = _container_name(namespace, run_id, "recovery")
        recovery_started = time.perf_counter()
        recovery_session = _start_session(
            image,
            recovery_container,
            warm_volume,
            run_id,
        )
        containers.append(recovery_container)
        recovery_session["startup_duration_ms"] = round(
            (time.perf_counter() - recovery_started) * 1000,
            3,
        )
        recovered = _timed_session_workload(recovery_container, file_count)
        if recovered["reused"] != file_count:
            raise BenchmarkError(
                "recovery session did not reuse the preserved named volume"
            )
        recovered_stopped = _stop_exact_container(recovery_container)
        report["recovery"] = {
            "volume": warm_volume,
            "before_stop": before_stop,
            "stopped": stopped,
            "restarted": recovery_session,
            "after_restart": recovered,
            "shutdown": recovered_stopped,
        }

        parallel_volumes = [
            _volume_name(namespace, run_id, f"parallel-{index:02d}")
            for index in range(parallel_sessions)
        ]
        parallel_containers = [
            _container_name(namespace, run_id, f"parallel-{index:02d}")
            for index in range(parallel_sessions)
        ]
        parallel_container_lock = Lock()
        for volume in parallel_volumes:
            _create_volume(volume, run_id)
            volumes.append(volume)
        parallel_started = time.perf_counter()
        with ThreadPoolExecutor(max_workers=parallel_sessions) as executor:
            futures = [
                executor.submit(
                    _parallel_session,
                    image,
                    container,
                    volume,
                    run_id,
                    file_count,
                    containers,
                    parallel_container_lock,
                )
                for container, volume in zip(
                    parallel_containers,
                    parallel_volumes,
                )
            ]
            parallel_results = [future.result() for future in futures]
        parallel_wall_ms = (time.perf_counter() - parallel_started) * 1000
        digests = {
            result["workload"]["digest"] for result in parallel_results
        }
        if (
            len(digests) != 1
            or any(
                result["workload"]["created"] != file_count
                or result["workload"]["reused"] != 0
                for result in parallel_results
            )
        ):
            raise BenchmarkError(
                "parallel sessions did not use independent empty volumes"
            )
        report["parallel"] = {
            "session_count": parallel_sessions,
            "wall_duration_ms": round(parallel_wall_ms, 3),
            "sessions": parallel_results,
        }
        report["comparison"] = _comparison(
            cold,
            warm_runs,
            parallel_wall_ms,
            parallel_results,
        )
    except (BenchmarkError, OSError) as error:
        failure = str(error)
        report["error"] = failure
    finally:
        _cleanup_containers(containers, cleanup)
        _cleanup_volumes(volumes, keep_volumes, cleanup)
        report["cleanup"] = cleanup

    success = failure is None and all(
        observation.get("removed", True)
        or observation.get("retained", False)
        for observation in cleanup
    )
    report["success"] = success
    return report, success


def _output_path(value: str) -> Path:
    candidate = Path(value)
    if candidate.is_absolute():
        raw_candidate = candidate
    else:
        raw_candidate = ROOT / candidate
    if raw_candidate.is_symlink():
        raise BenchmarkError("benchmark output cannot be a symlink")
    candidate = raw_candidate.resolve(strict=False)
    build_root = (ROOT / "build").resolve(strict=False)
    if build_root not in candidate.parents:
        raise BenchmarkError("benchmark output must remain below the ignored build directory")
    candidate.parent.mkdir(parents=True, exist_ok=True)
    return candidate


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Benchmark reusable and isolated Docker sessions for Atrinik."
    )
    parser.add_argument("--image", default=DEFAULT_LINUX_IMAGE)
    parser.add_argument("--namespace")
    parser.add_argument("--run-id")
    parser.add_argument(
        "--output",
        default="build/storage/devcontainer-session-benchmark.json",
    )
    parser.add_argument("--iterations", type=int, default=3)
    parser.add_argument("--parallel-sessions", type=int, default=2)
    parser.add_argument("--files", type=int, default=32)
    parser.add_argument(
        "--keep-volumes",
        action="store_true",
        help="retain exact benchmark volumes for manual inspection",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        image = _validate_image(args.image)
        namespace = _validate_namespace(
            args.namespace
            if args.namespace is not None
            else volume_namespace(ROOT)
        )
        run_id = _validate_run_id(
            args.run_id
            or datetime.now(timezone.utc).strftime("run%Y%m%d%H%M%S")
            + f"-{os.getpid()}"
        )
        if not 1 <= args.iterations <= 20:
            raise BenchmarkError("iterations must be between 1 and 20")
        if not 1 <= args.parallel_sessions <= 8:
            raise BenchmarkError("parallel-sessions must be between 1 and 8")
        if not 1 <= args.files <= 4096:
            raise BenchmarkError("files must be between 1 and 4096")
        output = _output_path(args.output)
        report, success = run_benchmark(
            image=image,
            namespace=namespace,
            run_id=run_id,
            iterations=args.iterations,
            parallel_sessions=args.parallel_sessions,
            file_count=args.files,
            keep_volumes=args.keep_volumes,
        )
        output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    except (BenchmarkError, WorkspaceError) as error:
        _parser().error(str(error))
        return 2

    print(f"session benchmark report: {output}")
    if args.keep_volumes:
        print(
            "benchmark volumes retained: "
            + ", ".join(
                observation["volume"]
                for observation in report.get("cleanup", [])
                if observation.get("retained")
            )
        )
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
