#!/usr/bin/env python3
"""Compare bind-mounted and named-volume Docker I/O for the devcontainer."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import sys
import subprocess
import tempfile
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from atrinik_workspace.docker_storage import volume_namespace
from atrinik_workspace.model import WorkspaceError
DEFAULT_WINDOWS_IMAGE = (
    "ghcr.io/atrinik/windows-build:1.2.1@sha256:"
    "d1f082eb28891600a9cf018a1d4310b9f3e1f985f82139fa48fbd4ac77b623bb"
)
NAMESPACE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_.-]{0,62}$")
RUN_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{0,30}$")
PURPOSE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{0,62}$")
MAX_VOLUME_NAME = 255
WORKLOAD_SCRIPT = r"""set -euo pipefail
mkdir -p /output/cache /output/result
created=0
reused=0
for source in /input/input-*.bin; do
    name=$(basename "$source")
    target="/output/cache/$name"
    if [ -f "$target" ]; then
        reused=$((reused + 1))
    else
        cp "$source" "$target"
        created=$((created + 1))
    fi
done
sha256sum /output/cache/input-*.bin | sort > /output/result/checksums.txt
tar -cf /output/result/artifact.tar -C /output/cache .
bytes=$(wc -c < /output/result/artifact.tar)
digest=$(sha256sum /output/result/artifact.tar | cut -d ' ' -f1)
printf 'created=%s reused=%s bytes=%s digest=%s\n' \
    "$created" "$reused" "$bytes" "$digest"
"""
EXPORT_SCRIPT = r"""set -euo pipefail
tar -cf /export/volume.tar -C /input .
"""
INTERRUPT_SCRIPT = r"""set -euo pipefail
printf 'interrupted\n' > /output/interrupted.marker
sleep 30
"""


class BenchmarkError(RuntimeError):
    """The benchmark cannot safely continue."""


def _validate_image(image: str) -> str:
    if re.fullmatch(r"[^\s@]+@sha256:[0-9a-f]{64}", image) is None:
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


def _volume_name(namespace: str, run_id: str) -> str:
    purpose = f"benchmark-{run_id}-volume"
    if PURPOSE_PATTERN.fullmatch(purpose) is None:
        raise BenchmarkError("benchmark run id produces an invalid volume purpose")
    name = f"atrinik-{namespace}-{purpose}"
    if len(name) > MAX_VOLUME_NAME:
        raise BenchmarkError("benchmark volume name exceeds the Docker limit")
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
    result = _docker(arguments, check=False, timeout=30)
    if result.returncode != 0:
        return "unavailable"
    return (result.stdout or "").strip() or "unknown"


def _create_volume(name: str) -> None:
    existing = _docker(["volume", "inspect", name], check=False, timeout=30)
    if existing.returncode == 0:
        raise BenchmarkError(f"refusing to reuse pre-existing benchmark volume: {name}")
    _docker(
        [
            "volume",
            "create",
            "--label",
            "atrinik.benchmark=devcontainer-storage",
            name,
        ],
        timeout=60,
    )


def _write_inputs(root: Path, file_count: int, file_bytes: int) -> int:
    root.mkdir(mode=0o700)
    total = 0
    for index in range(file_count):
        seed = hashlib.sha256(
            f"atrinik-devcontainer-storage:{index}".encode("ascii")
        ).digest()
        payload = (seed * ((file_bytes + len(seed) - 1) // len(seed)))[:file_bytes]
        (root / f"input-{index:04d}.bin").write_bytes(payload)
        total += len(payload)
    return total


def _reset_bind_output(path: Path) -> None:
    if path.is_symlink():
        raise BenchmarkError(f"benchmark bind output is a symlink: {path}")
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(mode=0o700)


def _bind_mount(source: Path, target: str, *, readonly: bool = False) -> str:
    spec = f"type=bind,source={source},target={target}"
    return f"{spec},readonly" if readonly else spec


def _volume_mount(
    name: str,
    target: str,
    *,
    readonly: bool = False,
) -> str:
    spec = f"type=volume,source={name},target={target},volume-nocopy"
    return f"{spec},readonly" if readonly else spec


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
                values[key] = int(value)
            except ValueError as error:
                raise BenchmarkError("Docker workload returned invalid counters") from error
        elif key == "digest":
            if re.fullmatch(r"[0-9a-f]{64}", value) is None:
                raise BenchmarkError("Docker workload returned an invalid digest")
            values[key] = value
    if set(values) != {"created", "reused", "bytes", "digest"}:
        raise BenchmarkError("Docker workload returned incomplete evidence")
    return values


def _timed_workload(
    image: str,
    inputs: Path,
    output_mount: str,
) -> dict[str, Any]:
    started = time.perf_counter()
    result = _docker(
        [
            "run",
            "--rm",
            "--user",
            "0:0",
            "--mount",
            _bind_mount(inputs, "/input", readonly=True),
            "--mount",
            output_mount,
            image,
            "bash",
            "-euc",
            WORKLOAD_SCRIPT,
        ]
    )
    elapsed_ms = (time.perf_counter() - started) * 1000
    evidence = _parse_workload_output(result.stdout or "")
    evidence["duration_ms"] = round(elapsed_ms, 3)
    return evidence


def _host_artifact(output: Path) -> dict[str, Any]:
    artifact = output / "result" / "artifact.tar"
    if artifact.is_symlink() or not artifact.is_file():
        raise BenchmarkError("bind workload did not produce a regular host artifact")
    payload = artifact.read_bytes()
    return {
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
    }


def _comparison(
    bind: dict[str, Any],
    named: dict[str, Any],
) -> dict[str, Any]:
    bind_warm = bind["warm"]
    named_warm = named["warm"]
    bind_median = float(
        statistics.median(entry["duration_ms"] for entry in bind_warm)
    )
    named_median = float(
        statistics.median(entry["duration_ms"] for entry in named_warm)
    )
    return {
        "bind_warm_median_ms": round(bind_median, 3),
        "named_volume_warm_median_ms": round(named_median, 3),
        "named_volume_over_bind_ratio": (
            round(named_median / bind_median, 3)
            if bind_median
            else None
        ),
        "bind_warm_reused_files": sorted(
            {entry["reused"] for entry in bind_warm}
        ),
        "named_volume_warm_reused_files": sorted(
            {entry["reused"] for entry in named_warm}
        ),
        "named_volume_warm_after_interrupt_reused_files": named[
            "warm_after_interrupt"
        ]["reused"],
    }


def _interrupt_container(
    image: str,
    volume: str,
    run_id: str,
    active_containers: list[str],
) -> dict[str, Any]:
    name = f"atrinik-benchmark-{run_id}-interrupt"
    active_containers.append(name)
    _docker(
        [
            "run",
            "-d",
            "--name",
            name,
            "--user",
            "0:0",
            "--mount",
            _volume_mount(volume, "/output"),
            image,
            "bash",
            "-euc",
            INTERRUPT_SCRIPT,
        ],
        timeout=60,
    )
    time.sleep(0.25)
    state = _docker(
        ["inspect", "--format", "{{.State.Running}}", name],
        timeout=30,
    )
    running = (state.stdout or "").strip() == "true"
    if not running:
        raise BenchmarkError("interruption container exited before it was stopped")
    removed = _docker(["rm", "-f", name], check=False, timeout=60)
    if removed.returncode != 0:
        raise BenchmarkError("could not stop the exact interruption container")
    active_containers.remove(name)
    return {
        "container": name,
        "was_running": running,
        "removed": True,
    }


def _export_volume(
    image: str,
    volume: str,
    export_root: Path,
) -> dict[str, Any]:
    export_root.mkdir(mode=0o700)
    _docker(
        [
            "run",
            "--rm",
            "--user",
            "0:0",
            "--mount",
            _volume_mount(volume, "/input", readonly=True),
            "--mount",
            _bind_mount(export_root, "/export"),
            image,
            "bash",
            "-euc",
            EXPORT_SCRIPT,
        ]
    )
    archive = export_root / "volume.tar"
    if archive.is_symlink() or not archive.is_file():
        raise BenchmarkError("named-volume export did not produce a regular archive")
    payload = archive.read_bytes()
    return {
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
    }


def _cleanup_containers(
    names: list[str],
    observations: list[dict[str, Any]],
) -> None:
    for name in reversed(names):
        try:
            result = _docker(["rm", "-f", name], check=False, timeout=60)
        except BenchmarkError as error:
            observations.append(
                {"container": name, "removed": False, "error": str(error)}
            )
            continue
        observation: dict[str, Any] = {
            "container": name,
            "removed": result.returncode == 0,
        }
        if result.returncode != 0:
            observation["error"] = (result.stderr or "").strip()[-500:]
        observations.append(observation)


def _cleanup_volumes(
    names: list[str],
    keep: bool,
    observations: list[dict[str, Any]],
) -> None:
    for name in reversed(names):
        if keep:
            observations.append({"volume": name, "removed": False, "retained": True})
            continue
        try:
            result = _docker(["volume", "rm", name], check=False, timeout=60)
        except BenchmarkError as error:
            observations.append(
                {"volume": name, "removed": False, "error": str(error)}
            )
            continue
        observations.append(
            {
                "volume": name,
                "removed": result.returncode == 0,
                "stderr": (result.stderr or "").strip()[-500:],
            }
        )


def _output_path(value: str) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    candidate = candidate.resolve(strict=False)
    build_root = (ROOT / "build").resolve(strict=False)
    if build_root not in candidate.parents:
        raise BenchmarkError("benchmark output must remain below the ignored build directory")
    candidate.parent.mkdir(parents=True, exist_ok=True)
    return candidate


def run_benchmark(
    *,
    image: str,
    namespace: str,
    run_id: str,
    iterations: int,
    file_count: int,
    file_bytes: int,
    keep_volumes: bool,
) -> tuple[dict[str, Any], bool]:
    volumes: list[str] = []
    containers: list[str] = []
    cleanup: list[dict[str, Any]] = []
    report: dict[str, Any] = {
        "schema": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "image": image,
        "namespace": namespace,
        "run_id": run_id,
        "environment": {
            "platform": platform.platform(aliased=True),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "docker_client": _docker_fact(["version", "--format", "{{.Client.Version}}"]),
            "docker_server": _docker_fact(["version", "--format", "{{.Server.Version}}"]),
            "docker_driver": _docker_fact(["info", "--format", "{{.Driver}}"]),
        },
        "workload": {
            "files": file_count,
            "bytes_per_file": file_bytes,
        },
        "storage_contract": {
            "bind": "temporary host directory mounted at /output",
            "named_volume": "exact Docker volume mounted at /output",
            "source": "temporary host directory mounted read-only at /input",
            "export": "named volume archived through a temporary host bind",
        },
    }
    failure: str | None = None
    try:
        with tempfile.TemporaryDirectory(
            prefix="atrinik-devcontainer-storage-"
        ) as temporary:
            temporary_root = Path(temporary)
            inputs = temporary_root / "inputs"
            total_input_bytes = _write_inputs(inputs, file_count, file_bytes)
            report["workload"]["total_input_bytes"] = total_input_bytes

            bind_output = temporary_root / "bind-output"
            _reset_bind_output(bind_output)
            bind_result: dict[str, Any] = {
                "cold": _timed_workload(
                    image,
                    inputs,
                    _bind_mount(bind_output, "/output"),
                ),
                "warm": [],
                "host_export": _host_artifact(bind_output),
            }
            for _ in range(iterations):
                bind_result["warm"].append(
                    _timed_workload(
                        image,
                        inputs,
                        _bind_mount(bind_output, "/output"),
                    )
                )
            bind_result["host_export"] = _host_artifact(bind_output)
            report["bind"] = bind_result

            named_volume = _volume_name(namespace, run_id)
            _create_volume(named_volume)
            volumes.append(named_volume)
            named_mount = _volume_mount(named_volume, "/output")
            named_result: dict[str, Any] = {
                "volume": named_volume,
                "cold": _timed_workload(image, inputs, named_mount),
                "warm": [],
            }
            for _ in range(iterations):
                named_result["warm"].append(
                    _timed_workload(image, inputs, named_mount)
                )
            named_result["interrupted_container"] = _interrupt_container(
                image,
                named_volume,
                run_id,
                containers,
            )
            named_result["warm_after_interrupt"] = _timed_workload(
                image,
                inputs,
                named_mount,
            )
            named_result["host_export"] = _export_volume(
                image,
                named_volume,
                temporary_root / "volume-export",
            )
            report["named_volume"] = named_result
            report["comparison"] = _comparison(bind_result, named_result)
    except (BenchmarkError, OSError) as error:
        failure = str(error)
        report["error"] = failure
    finally:
        _cleanup_containers(containers, cleanup)
        _cleanup_volumes(volumes, keep_volumes, cleanup)
        report["cleanup"] = cleanup

    success = failure is None and all(
        observation.get("removed", True) or observation.get("retained", False)
        for observation in cleanup
        if "volume" in observation
    )
    report["success"] = success
    return report, success


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Benchmark bind and named-volume Docker storage for Atrinik."
    )
    parser.add_argument("--image", default=DEFAULT_WINDOWS_IMAGE)
    parser.add_argument("--namespace")
    parser.add_argument("--run-id")
    parser.add_argument(
        "--output",
        default="build/storage/devcontainer-storage-benchmark.json",
    )
    parser.add_argument("--iterations", type=int, default=3)
    parser.add_argument("--files", type=int, default=32)
    parser.add_argument("--file-bytes", type=int, default=262144)
    parser.add_argument(
        "--keep-volumes",
        action="store_true",
        help="retain the exact benchmark volume for manual cache inspection",
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
        if not 1 <= args.files <= 4096:
            raise BenchmarkError("files must be between 1 and 4096")
        if not 1 <= args.file_bytes <= 8 * 1024 * 1024:
            raise BenchmarkError("file-bytes must be between 1 and 8 MiB")
        if args.files * args.file_bytes > 128 * 1024 * 1024:
            raise BenchmarkError("workload is capped at 128 MiB")
        output = _output_path(args.output)
        report, success = run_benchmark(
            image=image,
            namespace=namespace,
            run_id=run_id,
            iterations=args.iterations,
            file_count=args.files,
            file_bytes=args.file_bytes,
            keep_volumes=args.keep_volumes,
        )
        output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    except (BenchmarkError, WorkspaceError) as error:
        _parser().error(str(error))
        return 2

    print(f"storage benchmark report: {output}")
    if args.keep_volumes:
        print(f"benchmark volume retained: {report.get('named_volume', {}).get('volume', 'none')}")
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
