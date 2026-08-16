from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from typing import Iterable
import unittest


SCHEMA_VERSION = 1
MAX_TESTS = 100_000
MAX_JSON_BYTES = 1024 * 1024
MAX_LOCAL_JOBS = 64
LOCAL_MODULE = "atrinik_workspace.ci_sharding"


def _canonical_digest(value: object) -> str:
    encoded = json.dumps(
        value, ensure_ascii=True, separators=(",", ":"), sort_keys=True
    ).encode("ascii")
    return hashlib.sha256(encoded).hexdigest()


def _test_cases(suite: unittest.TestSuite) -> Iterable[unittest.TestCase]:
    for candidate in suite:
        if isinstance(candidate, unittest.TestSuite):
            yield from _test_cases(candidate)
        elif isinstance(candidate, unittest.TestCase):
            yield candidate
        else:
            raise ValueError(f"unsupported discovered test node: {type(candidate).__name__}")


def discover_test_ids() -> list[str]:
    suite = unittest.defaultTestLoader.discover("tests", top_level_dir=".")
    test_ids = sorted(test.id() for test in _test_cases(suite))
    if not test_ids:
        raise ValueError("test discovery returned no tests")
    if len(test_ids) > MAX_TESTS:
        raise ValueError(f"test discovery exceeds the {MAX_TESTS} test limit")
    if len(test_ids) != len(set(test_ids)):
        raise ValueError("test discovery returned duplicate test IDs")
    return test_ids


def load_weights(path: Path) -> dict[str, object]:
    raw = path.read_bytes()
    if len(raw) > MAX_JSON_BYTES:
        raise ValueError("timing weights exceed 1 MiB")
    document = json.loads(raw)
    if not isinstance(document, dict) or set(document) != {
        "baseline",
        "default_seconds",
        "prefix_seconds",
        "schema_version",
        "tests",
    }:
        raise ValueError("timing weights have an unsupported schema")
    if document["schema_version"] != SCHEMA_VERSION:
        raise ValueError("timing weights have an unsupported schema version")
    default = document["default_seconds"]
    if isinstance(default, bool) or not isinstance(default, (int, float)) or default <= 0:
        raise ValueError("default timing weight must be positive")
    prefixes = document["prefix_seconds"]
    if not isinstance(prefixes, list):
        raise ValueError("prefix timing weights must be an array")
    seen_prefixes: set[str] = set()
    for entry in prefixes:
        if not isinstance(entry, dict) or set(entry) != {"prefix", "seconds"}:
            raise ValueError("prefix timing weight has an unsupported schema")
        prefix = entry["prefix"]
        seconds = entry["seconds"]
        if not isinstance(prefix, str) or not prefix or prefix in seen_prefixes:
            raise ValueError("prefix timing weights must have unique nonempty prefixes")
        if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or seconds <= 0:
            raise ValueError("prefix timing weights must be positive")
        seen_prefixes.add(prefix)
    tests = document["tests"]
    if not isinstance(tests, dict) or len(tests) > MAX_TESTS:
        raise ValueError("per-test timing weights must be a bounded object")
    for test_id, seconds in tests.items():
        if not isinstance(test_id, str) or not test_id:
            raise ValueError("per-test timing IDs must be nonempty strings")
        if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or seconds <= 0:
            raise ValueError("per-test timing weights must be positive")
    if not isinstance(document["baseline"], dict):
        raise ValueError("timing baseline metadata must be an object")
    return document


def test_weight(test_id: str, weights: dict[str, object]) -> float:
    tests = weights["tests"]
    assert isinstance(tests, dict)
    exact = tests.get(test_id)
    if exact is not None:
        return float(exact)
    prefixes = weights["prefix_seconds"]
    assert isinstance(prefixes, list)
    matches = [
        entry
        for entry in prefixes
        if isinstance(entry, dict) and test_id.startswith(str(entry["prefix"]))
    ]
    if matches:
        longest = max(matches, key=lambda entry: len(str(entry["prefix"])))
        return float(longest["seconds"])
    return float(weights["default_seconds"])


def assign_tests(
    test_ids: list[str], weights: dict[str, object], shard_count: int
) -> list[list[str]]:
    if shard_count < 1 or shard_count > 64:
        raise ValueError("shard count must be between 1 and 64")
    shards: list[list[str]] = [[] for _ in range(shard_count)]
    totals = [0.0] * shard_count
    ordered = sorted(test_ids, key=lambda test_id: (-test_weight(test_id, weights), test_id))
    for test_id in ordered:
        shard = min(range(shard_count), key=lambda index: (totals[index], index))
        shards[shard].append(test_id)
        totals[shard] += test_weight(test_id, weights)
    for shard in shards:
        shard.sort()
    return shards


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


class TimingResult(unittest.TextTestResult):
    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, **kwargs)
        self.started_at = 0.0
        self.durations: dict[str, float] = {}

    def startTest(self, test: unittest.TestCase) -> None:
        self.started_at = time.monotonic()
        super().startTest(test)

    def stopTest(self, test: unittest.TestCase) -> None:
        self.durations[test.id()] = time.monotonic() - self.started_at
        super().stopTest(test)


def run_shard(arguments: argparse.Namespace) -> int:
    weights = load_weights(arguments.weights)
    test_ids = discover_test_ids()
    shards = assign_tests(test_ids, weights, arguments.shard_count)
    if arguments.shard_index < 0 or arguments.shard_index >= arguments.shard_count:
        raise ValueError("shard index is outside the configured shard count")
    selected = shards[arguments.shard_index]
    weight_digest = _canonical_digest(weights)
    assignments = {str(index): tests for index, tests in enumerate(shards)}
    manifest = {
        "assignments_sha256": _canonical_digest(assignments),
        "discovered_count": len(test_ids),
        "discovered_sha256": _canonical_digest(test_ids),
        "schema_version": SCHEMA_VERSION,
        "selected_ids": selected,
        "shard_count": arguments.shard_count,
        "shard_index": arguments.shard_index,
        "timing_weights_sha256": weight_digest,
    }
    _write_json(arguments.manifest, manifest)

    suite = unittest.defaultTestLoader.loadTestsFromNames(selected)
    started = time.monotonic()
    runner = unittest.TextTestRunner(verbosity=2, resultclass=TimingResult)
    result = runner.run(suite)
    elapsed = time.monotonic() - started
    assert isinstance(result, TimingResult)
    timing_report = {
        "elapsed_seconds": elapsed,
        "schema_version": SCHEMA_VERSION,
        "shard_count": arguments.shard_count,
        "shard_index": arguments.shard_index,
        "tests": dict(sorted(result.durations.items())),
        "timing_weights_sha256": weight_digest,
    }
    _write_json(arguments.timings, timing_report)
    print(f"\nSlowest {arguments.durations} tests in shard {arguments.shard_index}")
    for test_id, seconds in sorted(
        result.durations.items(), key=lambda item: (-item[1], item[0])
    )[: arguments.durations]:
        print(f"{seconds:8.3f}s  {test_id}")
    return 0 if result.wasSuccessful() else 1


def verify_shards(arguments: argparse.Namespace) -> int:
    weights = load_weights(arguments.weights)
    expected_ids = discover_test_ids()
    expected_digest = _canonical_digest(expected_ids)
    weight_digest = _canonical_digest(weights)
    expected_shards = assign_tests(expected_ids, weights, arguments.shard_count)
    expected_assignment_digest = _canonical_digest(
        {str(index): tests for index, tests in enumerate(expected_shards)}
    )
    paths = sorted(arguments.manifests.glob("shard-*-manifest.json"))
    if len(paths) != arguments.shard_count:
        raise ValueError(
            f"expected {arguments.shard_count} shard manifests, found {len(paths)}"
        )
    seen: dict[str, int] = {}
    indexes: set[int] = set()
    assignment_digest: str | None = None
    for path in paths:
        raw = path.read_bytes()
        if len(raw) > MAX_JSON_BYTES:
            raise ValueError(f"shard manifest exceeds 1 MiB: {path}")
        document = json.loads(raw)
        if not isinstance(document, dict) or set(document) != {
            "assignments_sha256",
            "discovered_count",
            "discovered_sha256",
            "schema_version",
            "selected_ids",
            "shard_count",
            "shard_index",
            "timing_weights_sha256",
        }:
            raise ValueError(f"unsupported shard manifest schema: {path}")
        if (
            document["schema_version"] != SCHEMA_VERSION
            or document["shard_count"] != arguments.shard_count
            or document["discovered_count"] != len(expected_ids)
            or document["discovered_sha256"] != expected_digest
            or document["timing_weights_sha256"] != weight_digest
        ):
            raise ValueError(f"shard manifest coordinates do not match: {path}")
        index = document["shard_index"]
        if (
            isinstance(index, bool)
            or not isinstance(index, int)
            or index < 0
            or index >= arguments.shard_count
            or index in indexes
        ):
            raise ValueError(f"duplicate or invalid shard index: {path}")
        indexes.add(index)
        current_assignment = document["assignments_sha256"]
        if current_assignment != expected_assignment_digest:
            raise ValueError(f"shard manifest assignment does not match: {path}")
        if assignment_digest is None:
            assignment_digest = current_assignment
        elif current_assignment != assignment_digest:
            raise ValueError("shard manifests disagree on the complete assignment")
        selected_ids = document["selected_ids"]
        if (
            not isinstance(selected_ids, list)
            or len(selected_ids) > MAX_TESTS
            or selected_ids != sorted(selected_ids)
        ):
            raise ValueError(f"shard test IDs are not a sorted array: {path}")
        if selected_ids != expected_shards[index]:
            raise ValueError(
                f"shard test IDs do not match deterministic assignment: {path}"
            )
        for test_id in selected_ids:
            if not isinstance(test_id, str):
                raise ValueError(f"shard test ID is not a string: {path}")
            seen[test_id] = seen.get(test_id, 0) + 1
    if indexes != set(range(arguments.shard_count)):
        raise ValueError("shard manifest indexes are incomplete")
    duplicates = sorted(test_id for test_id, count in seen.items() if count != 1)
    missing = sorted(set(expected_ids) - seen.keys())
    unexpected = sorted(seen.keys() - set(expected_ids))
    if duplicates or missing or unexpected:
        raise ValueError(
            "shard discovery mismatch: "
            f"duplicates={duplicates[:10]}, missing={missing[:10]}, "
            f"unexpected={unexpected[:10]}"
        )
    output = {
        "assignment_sha256": assignment_digest,
        "discovered_count": len(expected_ids),
        "discovered_sha256": expected_digest,
        "schema_version": SCHEMA_VERSION,
        "shard_count": arguments.shard_count,
        "timing_weights_sha256": weight_digest,
    }
    _write_json(arguments.output, output)
    print(
        f"verified {len(expected_ids)} discovered tests exactly once across "
        f"{arguments.shard_count} shards"
    )
    return 0


def default_local_jobs() -> int:
    """Choose a conservative process count for a developer workstation."""

    return max(1, min(os.cpu_count() or 1, 3))


def select_test_ids(
    discovered_ids: list[str], requested_ids: list[str] | None
) -> list[str]:
    """Return a deterministic full-suite or explicitly targeted selection."""

    if not requested_ids:
        return discovered_ids
    if len(requested_ids) != len(set(requested_ids)):
        raise ValueError("targeted test IDs must be unique")
    discovered = set(discovered_ids)
    missing = sorted(set(requested_ids) - discovered)
    if missing:
        raise ValueError(f"targeted tests were not discovered: {missing[:10]}")
    return sorted(requested_ids)


def _local_timestamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace(
        "+00:00", "Z"
    )


def _run_selected_tests(
    selected_ids: list[str], *, durations: int
) -> tuple[dict[str, object], int]:
    suite = unittest.defaultTestLoader.loadTestsFromNames(selected_ids)
    started = time.monotonic()
    runner = unittest.TextTestRunner(verbosity=2, resultclass=TimingResult)
    result = runner.run(suite)
    elapsed = time.monotonic() - started
    assert isinstance(result, TimingResult)
    payload: dict[str, object] = {
        "elapsed_seconds": elapsed,
        "failed": not result.wasSuccessful(),
        "schema_version": SCHEMA_VERSION,
        "selected_ids": selected_ids,
        "tests": dict(sorted(result.durations.items())),
        "tests_run": result.testsRun,
    }
    print(f"\nSlowest {durations} tests")
    for test_id, seconds in sorted(
        result.durations.items(), key=lambda item: (-item[1], item[0])
    )[:durations]:
        print(f"{seconds:8.3f}s  {test_id}")
    return payload, 0 if result.wasSuccessful() else 1


def run_local_worker(arguments: argparse.Namespace) -> int:
    assignment = json.loads(arguments.assignment.read_text(encoding="utf-8"))
    if not isinstance(assignment, dict) or set(assignment) != {
        "schema_version",
        "selected_ids",
        "shard_count",
        "shard_index",
    }:
        raise ValueError("local shard assignment has an unsupported schema")
    selected_ids = assignment["selected_ids"]
    if (
        assignment["schema_version"] != SCHEMA_VERSION
        or not isinstance(selected_ids, list)
        or any(not isinstance(test_id, str) for test_id in selected_ids)
        or selected_ids != sorted(selected_ids)
        or selected_ids != sorted(set(selected_ids))
        or not isinstance(assignment["shard_count"], int)
        or isinstance(assignment["shard_count"], bool)
        or assignment["shard_count"] < 1
        or not isinstance(assignment["shard_index"], int)
        or isinstance(assignment["shard_index"], bool)
        or assignment["shard_index"] < 0
        or assignment["shard_index"] >= assignment["shard_count"]
    ):
        raise ValueError("local shard assignment is invalid")
    result, exit_code = _run_selected_tests(selected_ids, durations=arguments.durations)
    _write_json(arguments.result, result)
    return exit_code


def _terminate_local_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGTERM)
            return
        except ProcessLookupError:
            return
    process.terminate()


def _kill_local_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGKILL)
            return
        except ProcessLookupError:
            return
    process.kill()


def _validate_local_results(
    run_root: Path,
    assignments: list[list[str]],
    expected_ids: list[str],
    exit_codes: list[int],
) -> list[str]:
    errors: list[str] = []
    seen: dict[str, int] = {}
    for index, selected_ids in enumerate(assignments):
        result_path = run_root / f"shard-{index}-result.json"
        if not result_path.is_file():
            errors.append(f"shard {index} did not retain a result")
            continue
        try:
            result = json.loads(result_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            errors.append(f"shard {index} result is unreadable: {error}")
            continue
        if not isinstance(result, dict):
            errors.append(f"shard {index} result is not an object")
            continue
        if result.get("schema_version") != SCHEMA_VERSION:
            errors.append(f"shard {index} result has an unsupported schema")
        if result.get("selected_ids") != selected_ids:
            errors.append(f"shard {index} result assignment does not match")
        tests = result.get("tests")
        if not isinstance(tests, dict) or sorted(tests) != selected_ids:
            errors.append(f"shard {index} did not report every selected test")
        elif any(
            not isinstance(seconds, (int, float))
            or isinstance(seconds, bool)
            or not math.isfinite(seconds)
            or seconds < 0
            for seconds in tests.values()
        ):
            errors.append(f"shard {index} reported invalid test durations")
        elapsed = result.get("elapsed_seconds")
        if (
            not isinstance(elapsed, (int, float))
            or isinstance(elapsed, bool)
            or not math.isfinite(elapsed)
            or elapsed < 0
        ):
            errors.append(f"shard {index} reported invalid elapsed time")
        if result.get("tests_run") != len(selected_ids):
            errors.append(f"shard {index} test count does not match its assignment")
        if result.get("failed") is not False or exit_codes[index] != 0:
            errors.append(f"shard {index} failed")
        returned_ids = result.get("selected_ids")
        if isinstance(returned_ids, list) and all(
            isinstance(test_id, str) for test_id in returned_ids
        ):
            for test_id in returned_ids:
                seen[test_id] = seen.get(test_id, 0) + 1
    expected = set(expected_ids)
    duplicates = sorted(test_id for test_id, count in seen.items() if count != 1)
    missing = sorted(expected - seen.keys())
    unexpected = sorted(seen.keys() - expected)
    if duplicates or missing or unexpected:
        errors.append(
            "local test results do not cover the requested suite exactly once: "
            f"duplicates={duplicates[:10]}, missing={missing[:10]}, "
            f"unexpected={unexpected[:10]}"
        )
    return errors


def _coverage_files(run_root: Path, shard_count: int) -> list[str]:
    coverage_root = run_root / "coverage"
    paths = sorted(path for path in coverage_root.iterdir() if path.is_file())
    allowed = {
        f".coverage.shard-{index}."
        for index in range(shard_count)
    }
    unexpected = [
        path.name
        for path in paths
        if not any(path.name.startswith(prefix) for prefix in allowed)
    ]
    if unexpected:
        raise ValueError(f"coverage directory contains unexpected data: {unexpected}")
    missing = [
        str(index)
        for index in range(shard_count)
        if not any(path.name.startswith(f".coverage.shard-{index}.") for path in paths)
    ]
    if missing:
        raise ValueError(f"coverage data is missing for shard(s): {', '.join(missing)}")
    return [path.name for path in paths]


def _print_local_evidence(run_root: Path, shard_count: int) -> None:
    for index in range(shard_count):
        log_path = run_root / f"shard-{index}.log"
        if log_path.is_file():
            print(f"\n=== shard {index} output ===")
            print(log_path.read_text(encoding="utf-8"), end="")
        result_path = run_root / f"shard-{index}-result.json"
        if not result_path.is_file():
            continue
        try:
            result = json.loads(result_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            print(f"shard {index}: result is unreadable: {error}")
            continue
        if not isinstance(result, dict):
            print(f"shard {index}: result is not an object")
            continue
        tests = result.get("tests", {})
        elapsed = result.get("elapsed_seconds", 0.0)
        if isinstance(tests, dict) and isinstance(elapsed, (int, float)):
            print(
                f"shard {index}: {len(tests)} tests, {elapsed:.3f}s"
            )


def _local_timing_summary(run_root: Path, shard_count: int) -> dict[str, object]:
    aggregate_test_seconds = 0.0
    longest_shard_seconds = 0.0
    shard_timings: list[dict[str, object]] = []
    for index in range(shard_count):
        result_path = run_root / f"shard-{index}-result.json"
        if not result_path.is_file():
            continue
        try:
            result = json.loads(result_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(result, dict):
            continue
        tests = result.get("tests", {})
        elapsed = result.get("elapsed_seconds", 0.0)
        if isinstance(tests, dict):
            aggregate_test_seconds += sum(
                seconds
                for seconds in tests.values()
                if (
                    isinstance(seconds, (int, float))
                    and not isinstance(seconds, bool)
                    and math.isfinite(seconds)
                    and seconds >= 0
                )
            )
        if (
            isinstance(elapsed, (int, float))
            and not isinstance(elapsed, bool)
            and math.isfinite(elapsed)
            and elapsed >= 0
        ):
            longest_shard_seconds = max(longest_shard_seconds, elapsed)
        shard_timings.append(
            {
                "elapsed_seconds": elapsed,
                "index": index,
                "test_count": len(tests) if isinstance(tests, dict) else 0,
            }
        )
    return {
        "aggregate_test_seconds": aggregate_test_seconds,
        "longest_shard_seconds": longest_shard_seconds,
        "shards": shard_timings,
    }


def run_local(arguments: argparse.Namespace) -> int:
    weights = load_weights(arguments.weights)
    discovered_ids = discover_test_ids()
    selected_ids = select_test_ids(discovered_ids, arguments.tests)
    if arguments.jobs is None:
        jobs = default_local_jobs()
    else:
        jobs = arguments.jobs
    if jobs < 1 or jobs > MAX_LOCAL_JOBS:
        raise ValueError(f"local jobs must be between 1 and {MAX_LOCAL_JOBS}")
    shard_count = min(jobs, len(selected_ids))
    assignments = assign_tests(selected_ids, weights, shard_count)

    output_root = arguments.output
    output_root.mkdir(parents=True, exist_ok=True)
    run_root = Path(tempfile.mkdtemp(prefix="run-", dir=output_root))
    coverage_root = run_root / "coverage"
    coverage_root.mkdir()
    state: dict[str, object] = {
        "completed_at": None,
        "coverage": {"enabled": arguments.coverage, "combined": False},
        "discovered_count": len(discovered_ids),
        "discovered_sha256": _canonical_digest(discovered_ids),
        "jobs": shard_count,
        "schema_version": SCHEMA_VERSION,
        "selected_count": len(selected_ids),
        "selected_sha256": _canonical_digest(selected_ids),
        "started_at": _local_timestamp(),
        "status": "running",
        "weights_sha256": _canonical_digest(weights),
    }
    _write_json(run_root / "run.json", state)
    for index, selected in enumerate(assignments):
        _write_json(
            run_root / f"shard-{index}-manifest.json",
            {
                "assignments_sha256": _canonical_digest(
                    {str(shard): shard_ids for shard, shard_ids in enumerate(assignments)}
                ),
                "discovered_count": len(discovered_ids),
                "discovered_sha256": _canonical_digest(discovered_ids),
                "schema_version": SCHEMA_VERSION,
                "selected_ids": selected,
                "shard_count": shard_count,
                "shard_index": index,
                "timing_weights_sha256": _canonical_digest(weights),
            },
        )

    processes: list[subprocess.Popen[bytes]] = []
    log_handles: list[object] = []
    exit_codes: list[int] = []
    previous_signal_handlers: dict[int, object] = {}

    def interrupt(_signum: int, _frame: object) -> None:
        raise KeyboardInterrupt

    for signal_name in ("SIGINT", "SIGTERM"):
        signal_number = getattr(signal, signal_name, None)
        if signal_number is not None:
            previous_signal_handlers[signal_number] = signal.getsignal(signal_number)
            signal.signal(signal_number, interrupt)
    try:
        for index, selected in enumerate(assignments):
            assignment_path = run_root / f"shard-{index}-assignment.json"
            _write_json(
                assignment_path,
                {
                    "schema_version": SCHEMA_VERSION,
                    "selected_ids": selected,
                    "shard_count": shard_count,
                    "shard_index": index,
                },
            )
            log_handle = (run_root / f"shard-{index}.log").open("wb")
            log_handles.append(log_handle)
            command = [sys.executable, "-m", LOCAL_MODULE]
            if arguments.coverage:
                command = [
                    sys.executable,
                    "-m",
                    "coverage",
                    "run",
                    "--parallel-mode",
                    "-m",
                    LOCAL_MODULE,
                ]
            command.extend(
                [
                    "worker",
                    "--assignment",
                    str(assignment_path),
                    "--result",
                    str(run_root / f"shard-{index}-result.json"),
                    "--durations",
                    str(arguments.durations),
                ]
            )
            environment = os.environ.copy()
            environment.pop("COVERAGE_FILE", None)
            if arguments.coverage:
                environment["COVERAGE_FILE"] = str(
                    coverage_root / f".coverage.shard-{index}"
                )
            process = subprocess.Popen(
                command,
                cwd=Path.cwd(),
                env=environment,
                stdout=log_handle,
                stderr=subprocess.STDOUT,
                start_new_session=os.name == "posix",
            )
            processes.append(process)
        exit_codes = [process.wait() for process in processes]
    except KeyboardInterrupt:
        for process in processes:
            _terminate_local_process(process)
        for process in processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                _kill_local_process(process)
                process.wait()
        exit_codes = [130 if process.returncode is None else process.returncode for process in processes]
        state["status"] = "interrupted"
        state["completed_at"] = _local_timestamp()
        state["exit_code"] = 130
        _write_json(run_root / "run.json", state)
        print(f"local tests interrupted; evidence retained in {run_root}", file=sys.stderr)
        return 130
    except OSError as error:
        for process in processes:
            _terminate_local_process(process)
        for process in processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                _kill_local_process(process)
                process.wait()
        state["status"] = "failed"
        state["completed_at"] = _local_timestamp()
        state["errors"] = [f"unable to start local test shard: {error}"]
        state["exit_code"] = 1
        _write_json(run_root / "run.json", state)
        print(f"local tests: unable to start local test shard: {error}", file=sys.stderr)
        return 1
    finally:
        for signal_number, handler in previous_signal_handlers.items():
            signal.signal(signal_number, handler)
        for handle in log_handles:
            handle.close()

    errors = _validate_local_results(run_root, assignments, selected_ids, exit_codes)
    state["timing"] = _local_timing_summary(run_root, shard_count)
    if arguments.coverage and not errors:
        try:
            coverage_files = _coverage_files(run_root, shard_count)
            combined = run_root / ".coverage"
            subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "coverage",
                    "combine",
                    "--data-file",
                    str(combined),
                    str(coverage_root),
                ],
                cwd=Path.cwd(),
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            report = (run_root / "coverage-report.txt").open("w", encoding="utf-8")
            try:
                subprocess.run(
                    [
                        sys.executable,
                        "-m",
                        "coverage",
                        "report",
                        "--data-file",
                        str(combined),
                    ],
                    cwd=Path.cwd(),
                    check=True,
                    stdout=report,
                    stderr=subprocess.STDOUT,
                    text=True,
                )
            finally:
                report.close()
            state["coverage"] = {
                "combined": True,
                "data_files": coverage_files,
                "enabled": True,
                "report": str(run_root / "coverage-report.txt"),
            }
        except (OSError, subprocess.CalledProcessError, ValueError) as error:
            errors.append(f"coverage aggregation failed: {error}")
        except KeyboardInterrupt:
            state["status"] = "interrupted"
            state["completed_at"] = _local_timestamp()
            state["exit_code"] = 130
            _write_json(run_root / "run.json", state)
            print(
                f"local tests interrupted; evidence retained in {run_root}",
                file=sys.stderr,
            )
            return 130
    _print_local_evidence(run_root, shard_count)
    state["completed_at"] = _local_timestamp()
    state["errors"] = errors
    state["exit_code"] = 0 if not errors else 1
    state["status"] = "passed" if not errors else "failed"
    _write_json(run_root / "run.json", state)
    if errors:
        for error in errors:
            print(f"local tests: {error}", file=sys.stderr)
        print(f"local test evidence retained in {run_root}", file=sys.stderr)
        return 1
    print(f"local tests passed; evidence retained in {run_root}")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description="Run deterministic measured test shards")
    subparsers = result.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("run")
    run.add_argument("--shard-count", type=int, required=True)
    run.add_argument("--shard-index", type=int, required=True)
    run.add_argument("--weights", type=Path, required=True)
    run.add_argument("--manifest", type=Path, required=True)
    run.add_argument("--timings", type=Path, required=True)
    run.add_argument("--durations", type=int, default=50)
    run.set_defaults(function=run_shard)
    verify = subparsers.add_parser("verify")
    verify.add_argument("--shard-count", type=int, required=True)
    verify.add_argument("--weights", type=Path, required=True)
    verify.add_argument("--manifests", type=Path, required=True)
    verify.add_argument("--output", type=Path, required=True)
    verify.set_defaults(function=verify_shards)
    local = subparsers.add_parser(
        "local", description="Run the suite in process-isolated local shards"
    )
    local.add_argument("--jobs", type=int)
    local.add_argument("--weights", type=Path, default=Path("ci/test-timing-weights.json"))
    local.add_argument("--test", dest="tests", action="append")
    local.add_argument("--coverage", action="store_true")
    local.add_argument("--output", type=Path, default=Path("build/local-tests"))
    local.add_argument("--durations", type=int, default=20)
    local.set_defaults(function=run_local)
    worker = subparsers.add_parser("worker", help=argparse.SUPPRESS)
    worker.add_argument("--assignment", type=Path, required=True)
    worker.add_argument("--result", type=Path, required=True)
    worker.add_argument("--durations", type=int, default=20)
    worker.set_defaults(function=run_local_worker)
    return result


def main() -> int:
    arguments = parser().parse_args()
    if getattr(arguments, "durations", 1) < 1:
        raise ValueError("duration count must be positive")
    return int(arguments.function(arguments))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ci-sharding: {error}", file=sys.stderr)
        raise SystemExit(2) from error
