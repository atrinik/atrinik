from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
from typing import Iterable
import unittest


SCHEMA_VERSION = 1
MAX_TESTS = 100_000
MAX_JSON_BYTES = 1024 * 1024


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
