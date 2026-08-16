from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
from types import SimpleNamespace
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import ci_sharding


class SampleShardTest(unittest.TestCase):
    def test_pass(self) -> None:
        pass


class CiShardingTests(unittest.TestCase):
    def weights(self) -> dict[str, object]:
        return {
            "baseline": {},
            "default_seconds": 1.0,
            "prefix_seconds": [
                {"prefix": "tests.slow.", "seconds": 4.0},
            ],
            "schema_version": 1,
            "tests": {"tests.fast.outlier": 8.0},
        }

    def test_assignment_is_deterministic_and_uses_measured_weights(self) -> None:
        test_ids = [
            "tests.slow.one",
            "tests.slow.two",
            "tests.fast.one",
            "tests.fast.two",
            "tests.fast.outlier",
        ]
        first = ci_sharding.assign_tests(test_ids, self.weights(), 2)
        second = ci_sharding.assign_tests(list(reversed(test_ids)), self.weights(), 2)

        self.assertEqual(first, second)
        self.assertNotEqual(
            next(index for index, shard in enumerate(first) if "tests.fast.outlier" in shard),
            next(index for index, shard in enumerate(first) if "tests.slow.one" in shard),
        )
        self.assertEqual(sorted(test for shard in first for test in shard), sorted(test_ids))

    def test_weight_validation_rejects_duplicate_or_nonpositive_entries(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            path = Path(temporary_name) / "weights.json"
            invalid = self.weights()
            invalid["prefix_seconds"] = [
                {"prefix": "tests.", "seconds": 1.0},
                {"prefix": "tests.", "seconds": 2.0},
            ]
            path.write_text(json.dumps(invalid), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unique nonempty prefixes"):
                ci_sharding.load_weights(path)

    def test_weight_validation_rejects_every_schema_boundary(self) -> None:
        cases: list[tuple[str, object, str]] = [
            ("schema", [], "unsupported schema"),
            ("schema_version", 2, "unsupported schema version"),
            ("default_seconds", 0, "default timing weight must be positive"),
            ("prefix_seconds", {}, "prefix timing weights must be an array"),
            (
                "prefix_seconds",
                [{"prefix": "tests."}],
                "prefix timing weight has an unsupported schema",
            ),
            (
                "prefix_seconds",
                [{"prefix": "tests.", "seconds": 0}],
                "prefix timing weights must be positive",
            ),
            ("tests", [], "per-test timing weights must be a bounded object"),
            ("tests", {"": 1.0}, "per-test timing IDs must be nonempty strings"),
            ("tests", {"tests.bad": 0}, "per-test timing weights must be positive"),
            ("baseline", [], "timing baseline metadata must be an object"),
        ]
        with tempfile.TemporaryDirectory() as temporary_name:
            path = Path(temporary_name) / "weights.json"
            for field, value, message in cases:
                with self.subTest(field=field, value=value):
                    document: object = self.weights()
                    if field == "schema":
                        document = value
                    else:
                        assert isinstance(document, dict)
                        document[field] = value
                    path.write_text(json.dumps(document), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, message):
                        ci_sharding.load_weights(path)

            path.write_bytes(b" " * (ci_sharding.MAX_JSON_BYTES + 1))
            with self.assertRaisesRegex(ValueError, "exceed 1 MiB"):
                ci_sharding.load_weights(path)

    def test_shard_coordinate_validation_rejects_invalid_ranges(self) -> None:
        for shard_count in (0, 65):
            with self.subTest(shard_count=shard_count):
                with self.assertRaisesRegex(ValueError, "between 1 and 64"):
                    ci_sharding.assign_tests([], self.weights(), shard_count)

        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            weights_path = root / "weights.json"
            weights_path.write_text(json.dumps(self.weights()), encoding="utf-8")
            run_arguments = SimpleNamespace(
                durations=1,
                manifest=root / "manifest.json",
                shard_count=1,
                shard_index=1,
                timings=root / "timings.json",
                weights=weights_path,
            )
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=["test"]):
                with self.assertRaisesRegex(ValueError, "outside the configured"):
                    ci_sharding.run_shard(run_arguments)

            verify_arguments = SimpleNamespace(
                manifests=root,
                output=root / "verified.json",
                shard_count=1,
                weights=weights_path,
            )
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=["test"]):
                with self.assertRaisesRegex(ValueError, "expected 1 shard manifests"):
                    ci_sharding.verify_shards(verify_arguments)

    def test_verify_requires_every_discovered_test_exactly_once(self) -> None:
        test_ids = ["tests.example.one", "tests.example.two", "tests.example.three"]
        weights = self.weights()
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            weights_path = root / "weights.json"
            weights_path.write_text(json.dumps(weights), encoding="utf-8")
            assignments = ci_sharding.assign_tests(test_ids, weights, 2)
            assignment_digest = ci_sharding._canonical_digest(
                {str(index): tests for index, tests in enumerate(assignments)}
            )
            for index, selected in enumerate(assignments):
                ci_sharding._write_json(
                    root / f"shard-{index}-manifest.json",
                    {
                        "assignments_sha256": assignment_digest,
                        "discovered_count": len(test_ids),
                        "discovered_sha256": ci_sharding._canonical_digest(test_ids),
                        "schema_version": 1,
                        "selected_ids": selected,
                        "shard_count": 2,
                        "shard_index": index,
                        "timing_weights_sha256": ci_sharding._canonical_digest(weights),
                    },
                )
            arguments = SimpleNamespace(
                manifests=root,
                output=root / "verified.json",
                shard_count=2,
                weights=weights_path,
            )
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=test_ids):
                self.assertEqual(ci_sharding.verify_shards(arguments), 0)
            self.assertTrue(arguments.output.is_file())

            duplicate = json.loads((root / "shard-1-manifest.json").read_text())
            duplicate["selected_ids"].append(assignments[0][0])
            duplicate["selected_ids"].sort()
            ci_sharding._write_json(root / "shard-1-manifest.json", duplicate)
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=test_ids):
                with self.assertRaisesRegex(ValueError, "deterministic assignment"):
                    ci_sharding.verify_shards(arguments)

            duplicate["selected_ids"] = assignments[1]
            duplicate["shard_index"] = 2
            ci_sharding._write_json(root / "shard-1-manifest.json", duplicate)
            with mock.patch.object(ci_sharding, "discover_test_ids", return_value=test_ids):
                with self.assertRaisesRegex(ValueError, "invalid shard index"):
                    ci_sharding.verify_shards(arguments)

    def test_run_shard_retains_assignment_timings_and_slowest_output(self) -> None:
        test_id = f"{__name__}.SampleShardTest.test_pass"
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            weights_path = root / "weights.json"
            weights_path.write_text(json.dumps(self.weights()), encoding="utf-8")
            arguments = SimpleNamespace(
                durations=1,
                manifest=root / "shard-0-manifest.json",
                shard_count=1,
                shard_index=0,
                timings=root / "shard-0-timings.json",
                weights=weights_path,
            )
            output = io.StringIO()
            with (
                mock.patch.object(
                    ci_sharding, "discover_test_ids", return_value=[test_id]
                ),
                redirect_stdout(output),
            ):
                self.assertEqual(ci_sharding.run_shard(arguments), 0)

            manifest = json.loads(arguments.manifest.read_text())
            timings = json.loads(arguments.timings.read_text())
            self.assertEqual(manifest["selected_ids"], [test_id])
            self.assertEqual(set(timings["tests"]), {test_id})
            self.assertIn("Slowest 1 tests in shard 0", output.getvalue())

    def test_main_rejects_nonpositive_duration_count(self) -> None:
        with mock.patch.object(
            ci_sharding.sys,
            "argv",
            [
                "ci_sharding",
                "run",
                "--shard-count",
                "1",
                "--shard-index",
                "0",
                "--weights",
                "weights.json",
                "--manifest",
                "manifest.json",
                "--timings",
                "timings.json",
                "--durations",
                "0",
            ],
        ):
            with self.assertRaisesRegex(ValueError, "duration count must be positive"):
                ci_sharding.main()

    def test_local_selection_is_sorted_and_rejects_unknown_or_duplicate_ids(self) -> None:
        discovered = ["tests.beta", "tests.alpha", "tests.gamma"]
        self.assertEqual(
            ci_sharding.select_test_ids(discovered, ["tests.gamma", "tests.alpha"]),
            ["tests.alpha", "tests.gamma"],
        )
        with self.assertRaisesRegex(ValueError, "unique"):
            ci_sharding.select_test_ids(discovered, ["tests.alpha", "tests.alpha"])
        with self.assertRaisesRegex(ValueError, "not discovered"):
            ci_sharding.select_test_ids(discovered, ["tests.missing"])

    def test_local_result_validation_requires_exact_once_and_successful_workers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            selected = [["tests.alpha"], ["tests.beta"]]
            for index, test_ids in enumerate(selected):
                ci_sharding._write_json(
                    root / f"shard-{index}-result.json",
                    {
                        "elapsed_seconds": 0.1,
                        "failed": False,
                        "schema_version": 1,
                        "selected_ids": test_ids,
                        "tests": {test_ids[0]: 0.1},
                        "tests_run": 1,
                    },
                )
            self.assertEqual(
                ci_sharding._validate_local_results(
                    root, selected, ["tests.alpha", "tests.beta"], [0, 0]
                ),
                [],
            )
            (root / "shard-1-result.json").unlink()
            errors = ci_sharding._validate_local_results(
                root, selected, ["tests.alpha", "tests.beta"], [0, 0]
            )
            self.assertIn("shard 1 did not retain a result", errors)

    def test_local_parent_success_and_coverage_retain_complete_evidence(self) -> None:
        class FakeProcess:
            next_pid = 4000

            def __init__(self, exit_code: int) -> None:
                self.pid = FakeProcess.next_pid
                FakeProcess.next_pid += 1
                self.returncode: int | None = None
                self.exit_code = exit_code

            def wait(self, timeout: float | None = None) -> int:
                del timeout
                self.returncode = self.exit_code
                return self.exit_code

            def poll(self) -> int | None:
                return self.returncode

        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            weights_path = root / "weights.json"
            weights_path.write_text(json.dumps(self.weights()), encoding="utf-8")
            calls: list[list[str]] = []

            def launch(command: list[str], **keywords: object) -> FakeProcess:
                calls.append(command)
                assignment = Path(command[command.index("--assignment") + 1])
                result_path = Path(command[command.index("--result") + 1])
                selected = json.loads(assignment.read_text(encoding="utf-8"))[
                    "selected_ids"
                ]
                ci_sharding._write_json(
                    result_path,
                    {
                        "elapsed_seconds": 0.25,
                        "failed": False,
                        "schema_version": 1,
                        "selected_ids": selected,
                        "tests": {test_id: 0.1 for test_id in selected},
                        "tests_run": len(selected),
                    },
                )
                environment = keywords["env"]
                assert isinstance(environment, dict)
                coverage_file = environment.get("COVERAGE_FILE")
                if coverage_file is not None:
                    coverage_path = Path(f"{coverage_file}.fake")
                    coverage_path.parent.mkdir(parents=True, exist_ok=True)
                    coverage_path.write_bytes(b"coverage")
                return FakeProcess(0)

            arguments = SimpleNamespace(
                coverage=True,
                durations=1,
                jobs=2,
                output=root / "runs",
                tests=None,
                weights=weights_path,
            )
            with (
                mock.patch.object(
                    ci_sharding,
                    "discover_test_ids",
                    return_value=["tests.alpha", "tests.beta"],
                ),
                mock.patch.object(ci_sharding.subprocess, "Popen", side_effect=launch),
                mock.patch.object(
                    ci_sharding.subprocess,
                    "run",
                    return_value=subprocess.CompletedProcess([], 0, stdout=""),
                ),
            ):
                self.assertEqual(ci_sharding.run_local(arguments), 0)

            self.assertEqual(len(calls), 2)
            run_root = next(arguments.output.glob("run-*"))
            state = json.loads((run_root / "run.json").read_text(encoding="utf-8"))
            self.assertEqual(state["status"], "passed")
            self.assertTrue(state["coverage"]["combined"])
            self.assertEqual(state["timing"]["aggregate_test_seconds"], 0.2)
            self.assertTrue((run_root / "coverage-report.txt").exists())

    def test_local_parent_failure_interrupt_and_launch_error_retain_state(self) -> None:
        class FakeProcess:
            def __init__(self, *, interrupt: bool = False) -> None:
                self.pid = 5000
                self.returncode: int | None = None
                self.interrupt = interrupt

            def wait(self, timeout: float | None = None) -> int:
                del timeout
                if self.interrupt and self.returncode is None:
                    self.returncode = 130
                    raise KeyboardInterrupt
                self.returncode = 1 if not self.interrupt else 130
                return self.returncode

            def poll(self) -> int | None:
                return self.returncode

        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            weights_path = root / "weights.json"
            weights_path.write_text(json.dumps(self.weights()), encoding="utf-8")
            common = dict(
                durations=1,
                jobs=1,
                output=root / "runs",
                tests=None,
                weights=weights_path,
            )

            def launch_failure(*_args: object, **_keywords: object) -> FakeProcess:
                raise OSError("simulated launch failure")

            def worker_failure(command: list[str], **_keywords: object) -> FakeProcess:
                assignment = Path(command[command.index("--assignment") + 1])
                result_path = Path(command[command.index("--result") + 1])
                selected = json.loads(assignment.read_text(encoding="utf-8"))[
                    "selected_ids"
                ]
                ci_sharding._write_json(
                    result_path,
                    {
                        "elapsed_seconds": 0.1,
                        "failed": True,
                        "schema_version": 1,
                        "selected_ids": selected,
                        "tests": {test_id: 0.1 for test_id in selected},
                        "tests_run": len(selected),
                    },
                )
                return FakeProcess()

            def interrupted_worker(*_args: object, **_keywords: object) -> FakeProcess:
                return FakeProcess(interrupt=True)

            with mock.patch.object(
                ci_sharding, "discover_test_ids", return_value=["tests.alpha"]
            ):
                for launcher, expected_status, expected_code in (
                    (worker_failure, "failed", 1),
                    (interrupted_worker, "interrupted", 130),
                ):
                    arguments = SimpleNamespace(coverage=False, **common)
                    previous_runs = set(arguments.output.glob("run-*"))
                    with (
                        mock.patch.object(
                            ci_sharding.subprocess, "Popen", side_effect=launcher
                        ),
                        mock.patch.object(
                            ci_sharding, "_terminate_local_process"
                        ),
                    ):
                        self.assertEqual(ci_sharding.run_local(arguments), expected_code)
                    run_root = next(iter(set(arguments.output.glob("run-*")) - previous_runs))
                    state = json.loads(
                        (run_root / "run.json").read_text(encoding="utf-8")
                    )
                    self.assertEqual(state["status"], expected_status)
                    self.assertEqual(state["exit_code"], expected_code)

                arguments = SimpleNamespace(coverage=False, **common)
                previous_runs = set(arguments.output.glob("run-*"))
                with mock.patch.object(
                    ci_sharding.subprocess, "Popen", side_effect=launch_failure
                ):
                    self.assertEqual(ci_sharding.run_local(arguments), 1)
                run_root = next(iter(set(arguments.output.glob("run-*")) - previous_runs))
                state = json.loads(
                    (run_root / "run.json").read_text(encoding="utf-8")
                )
                self.assertEqual(state["status"], "failed")
                self.assertIn("simulated launch failure", state["errors"][0])

    def test_local_worker_validates_assignment_and_records_result(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            assignment = root / "assignment.json"
            result = root / "result.json"
            ci_sharding._write_json(
                assignment,
                {
                    "schema_version": 1,
                    "selected_ids": [
                        "tests.test_ci_sharding.SampleShardTest.test_pass"
                    ],
                    "shard_count": 1,
                    "shard_index": 0,
                },
            )
            arguments = SimpleNamespace(
                assignment=assignment, durations=1, result=result
            )
            self.assertEqual(ci_sharding.run_local_worker(arguments), 0)
            self.assertEqual(
                json.loads(result.read_text(encoding="utf-8"))["tests_run"], 1
            )

    def test_default_local_jobs_is_bounded_by_three(self) -> None:
        with mock.patch.object(ci_sharding.os, "cpu_count", return_value=32):
            self.assertEqual(ci_sharding.default_local_jobs(), 3)
        with mock.patch.object(ci_sharding.os, "cpu_count", return_value=None):
            self.assertEqual(ci_sharding.default_local_jobs(), 1)


if __name__ == "__main__":
    unittest.main()
