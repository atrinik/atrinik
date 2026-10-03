"""Outcome tests for the deterministic, opt-in MCP pilot harness."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "mcp_pilot.py"
PILOT = ROOT / "mcp" / "pilot"


def load_script():
    spec = importlib.util.spec_from_file_location("mcp_pilot_under_test", SCRIPT)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


pilot = load_script()


class McpPilotTests(unittest.TestCase):
    def config(self, name: str) -> dict:
        return json.loads((PILOT / name).read_text(encoding="utf-8"))

    def write_config(self, directory: Path, value: dict) -> Path:
        path = directory / "pilot.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_examples_keep_every_profile_disabled_by_default(self) -> None:
        disabled = self.config("config.disabled.example.json")
        self.assertFalse(disabled["pilot_enabled"])
        self.assertEqual(
            {profile["profile_id"] for profile in disabled["profiles"]},
            {"context", "search", "content", "runtime-status", "external-evaluation"},
        )
        self.assertTrue(all(not profile["enabled"] for profile in disabled["profiles"]))
        runtime = next(p for p in disabled["profiles"] if p["profile_id"] == "runtime-status")
        external = next(p for p in disabled["profiles"] if p["profile_id"] == "external-evaluation")
        self.assertFalse(runtime["runtime_opt_in"])
        self.assertTrue(external["evaluation_only"])

    def test_validation_rejects_unsafe_adapter_and_unpinned_executable(self) -> None:
        base = self.config("config.synthetic.example.json")
        invalids = {
            "absolute pin": lambda c: c["profiles"][0]["adapter"].update(pin_path="/bin/sh"),
            "shell syntax": lambda c: c["profiles"][0]["adapter"].update(argv=["sh", "-c", "x; y {workload_id} {scenario}"]),
            "secret argument": lambda c: c["profiles"][0]["adapter"].update(argv=["tool", "--token", "{workload_id}", "{scenario}"]),
            "wrong digest": lambda c: c["profiles"][0]["adapter"].update(sha256="0" * 64),
            "unexecuted pin": lambda c: c["profiles"][0]["adapter"].update(
                argv=["python3", "-c", "pass", "{workload_id}", "{scenario}"]
            ),
            "unknown field": lambda c: c.update(credential="private"),
        }
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for label, mutate in invalids.items():
                with self.subTest(label=label):
                    candidate = copy.deepcopy(base)
                    mutate(candidate)
                    with self.assertRaises(pilot.PilotError):
                        pilot.validate_config(self.write_config(directory, candidate))

    def test_master_switch_prevents_enabled_adapter(self) -> None:
        config = self.config("config.synthetic.example.json")
        config["pilot_enabled"] = False
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(pilot.PilotError, "disabled pilot"):
                pilot.validate_config(self.write_config(Path(temporary), config))

    def test_runtime_and_external_enablement_have_separate_guards(self) -> None:
        base = self.config("config.synthetic.example.json")
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            runtime = copy.deepcopy(base)
            profile = next(p for p in runtime["profiles"] if p["profile_id"] == "runtime-status")
            profile["enabled"] = True
            profile["adapter"] = copy.deepcopy(runtime["profiles"][0]["adapter"])
            with self.assertRaisesRegex(pilot.PilotError, "separate opt-in"):
                pilot.validate_config(self.write_config(directory, runtime))

            external = copy.deepcopy(base)
            profile = next(p for p in external["profiles"] if p["profile_id"] == "external-evaluation")
            profile["evaluation_only"] = False
            with self.assertRaisesRegex(pilot.PilotError, "evaluation-only"):
                pilot.validate_config(self.write_config(directory, external))

    def test_real_pilot_needs_cli_gate_and_matching_full_head(self) -> None:
        config = self.config("config.synthetic.example.json")
        config["evidence_kind"] = "real-pilot"
        config["expected_head"] = "a" * 40
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_config(Path(temporary), config)
            with self.assertRaisesRegex(pilot.PilotError, "explicit CLI gate"):
                pilot.run_pilot(path, enable_real_pilot=False)
            with mock.patch.object(pilot, "_git_head", return_value="b" * 40):
                with self.assertRaisesRegex(pilot.PilotError, "does not match"):
                    pilot.run_pilot(path, enable_real_pilot=True)

    def repository_fixture(self, directory: Path):
        root = directory / "source"
        root.mkdir()
        adapter = root / "adapter.py"
        adapter.write_text("print('{}')\n")
        (root / "tracked.py").write_text("value = 1\n")
        (root / ".gitignore").write_text("ignored/\n")
        for args in (["init", "--quiet"], ["add", "."],
                     ["-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                      "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "fixture"]):
            subprocess.run(["git", "-C", str(root), *args], check=True, capture_output=True)
        config = self.config("config.synthetic.example.json")
        config.update(evidence_kind="real-pilot", expected_head=pilot._git_head(root))
        config["profiles"][0]["adapter"].update(
            pin_path="adapter.py", sha256=hashlib.sha256(adapter.read_bytes()).hexdigest(),
            argv=["python3", "adapter.py", "{workload_id}", "{scenario}"],
        )
        workloads = directory / "workloads.json"
        workloads.write_text(json.dumps({"cases": [{"id": "fixture", "expected": {}}]}))
        return root, config, workloads

    def test_real_pilot_rejects_tracked_and_untracked_changes_before_execution(self):
        for dirty in ("tracked", "untracked", "pinned-adapter"):
            with self.subTest(dirty=dirty), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                root, config, workloads = self.repository_fixture(directory)
                target = root / {"tracked": "tracked.py", "untracked": "new.py", "pinned-adapter": "adapter.py"}[dirty]
                target.write_text("print('modified')\n")
                if dirty == "pinned-adapter":
                    config["profiles"][0]["adapter"]["sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
                path = self.write_config(directory, config)
                with mock.patch.object(pilot, "ROOT", root), mock.patch.object(pilot, "WORKLOADS", workloads), \
                        mock.patch.object(pilot, "_run_adapter") as run:
                    with self.assertRaisesRegex(pilot.PilotError, "clean root"):
                        pilot.run_pilot(path, enable_real_pilot=True)
                    run.assert_not_called()

    def test_real_pilot_allows_ignored_artifacts_but_rechecks_after_execution(self):
        for changed in (False, True):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                root, config, workloads = self.repository_fixture(directory)
                (root / "ignored").mkdir()
                (root / "ignored" / "artifact").write_text("fixture output")
                path = self.write_config(directory, config)
                def adapter(*args, **kwargs):
                    if changed:
                        (root / "tracked.py").write_text("value = 2\n")
                    return {}, {"return_code": 0, "bounded_parse_valid": True}
                with mock.patch.object(pilot, "ROOT", root), mock.patch.object(pilot, "WORKLOADS", workloads), \
                        mock.patch.object(pilot, "SCENARIOS", ("enabled",)), \
                        mock.patch.object(pilot, "_run_adapter", side_effect=adapter) as run:
                    if changed:
                        with self.assertRaisesRegex(pilot.PilotError, "clean root"):
                            pilot.run_pilot(path, enable_real_pilot=True)
                    else:
                        result = pilot.run_pilot(path, enable_real_pilot=True)
                        self.assertTrue(result["source"]["exact_head"])
                        self.assertEqual(result["source"]["commit"], config["expected_head"])
                    self.assertEqual(run.call_count, 1)

    def test_synthetic_run_requires_no_git_and_preserves_selected_interpreter(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            root, config, workloads = self.repository_fixture(directory)
            config.update(evidence_kind="synthetic-self-test", expected_head=None)
            config["profiles"][0]["adapter"]["argv"][0] = "python3.11"
            executables = directory / "executables"
            executables.mkdir()
            selected = executables / "python3.11"
            # An executable selector fixture proves that the harness does not
            # silently substitute its own Python interpreter for the chosen one.
            selected.write_text("#!" + sys.executable + "\nprint('{\"selected_interpreter\": true}')\n")
            selected.chmod(0o755)
            path = self.write_config(directory, config)
            observed = []
            run_adapter = pilot._run_adapter
            def run(argv, **kwargs):
                self.assertEqual(argv[0], str(selected))
                value, measured = run_adapter(argv, **kwargs)
                observed.append(value)
                return value, measured
            with mock.patch.object(pilot, "ROOT", root), mock.patch.object(pilot, "WORKLOADS", workloads), \
                    mock.patch.object(pilot, "SCENARIOS", ("enabled",)), \
                    mock.patch.object(pilot, "_git_head", side_effect=AssertionError("synthetic must not inspect Git")), \
                    mock.patch.object(pilot, "_run_adapter", side_effect=run), \
                    mock.patch.dict(os.environ, {"PATH": str(executables)}):
                result = pilot.run_pilot(path, enable_real_pilot=False)
                self.assertEqual(observed, [{"selected_interpreter": True}])
                self.assertIsNone(result["source"]["commit"])
                self.assertFalse(result["source"]["exact_head"])
                selected.unlink()
                with self.assertRaisesRegex(pilot.PilotError, "interpreter is unavailable"):
                    pilot.run_pilot(path, enable_real_pilot=False)

    def test_synthetic_self_test_records_all_known_answers_and_failure_modes(self) -> None:
        command = [sys.executable, str(SCRIPT), "self-test"]
        first = subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True, timeout=30)
        second = subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True, timeout=30)
        result = json.loads(first.stdout)
        repeat = json.loads(second.stdout)
        workloads = json.loads((ROOT / "mcp/contract/v1/fixtures/workloads.json").read_text())["cases"]
        workload_ids = {case["id"] for case in workloads}

        self.assertTrue(result["synthetic"])
        self.assertTrue(result["correct"])
        self.assertFalse(result["real_pilot_complete"])
        self.assertFalse(result["adoption_claimed"])
        self.assertEqual(result["coverage"]["workloads"], 6)
        self.assertEqual({item["workload_id"] for item in result["observations"]}, workload_ids)
        self.assertEqual({item["scenario"] for item in result["observations"]}, set(pilot.SCENARIOS))
        self.assertEqual(len(result["observations"]), 6 * len(pilot.SCENARIOS))
        self.assertTrue(all(item["mutation_count"] == 0 for item in result["observations"]))
        self.assertTrue(all(item["measurement"]["stdout_bytes"] > 0 for item in result["observations"]))
        self.assertTrue(all(len(item["measurement"]["stdout_sha256"]) == 64 for item in result["observations"]))
        self.assertTrue(all(item["measurement"]["wall_ns"] >= 0 for item in result["observations"]))
        self.assertEqual(result["privacy"], {"raw_output_recorded": False, "credentials_recorded": False, "host_paths_recorded": False})

        def stable(value: dict) -> list[tuple[str, str, bool, int, int, str]]:
            return [
                (item["workload_id"], item["scenario"], item["correct"], item["measurement"]["return_code"], item["measurement"]["stdout_bytes"], item["measurement"]["stdout_sha256"])
                for item in value["observations"]
            ]

        self.assertEqual(stable(result), stable(repeat))

    def test_adapter_measurement_preserves_exact_bytes_and_return_code(self) -> None:
        payload = b'{"workload_id":"case","scenario":"enabled","mutation_count":0}\n'
        program = "import sys; sys.stdout.write(" + repr(payload.decode()) + "); sys.stderr.write('diagnostic'); raise SystemExit(7)"
        response, measured = pilot._run_adapter(
            [sys.executable, "-c", program], root=ROOT, timeout_ms=5000
        )
        self.assertEqual(response["workload_id"], "case")
        self.assertEqual(measured["return_code"], 7)
        self.assertEqual(measured["stdout_bytes"], len(payload))
        self.assertEqual(measured["stderr_bytes"], len(b"diagnostic"))
        self.assertEqual(measured["stdout_sha256"], hashlib.sha256(payload).hexdigest())
        self.assertGreaterEqual(measured["wall_ns"], 0)
        self.assertTrue(measured["bounded_parse_valid"])

    def test_enabled_answer_requires_exact_fixture_content_and_coordinates(self) -> None:
        config = PILOT / "config.synthetic.example.json"
        malformed_answer = {"coordinates": [{"commit": "0" * 40}]}
        measurement = {
            "return_code": 0,
            "stdout_bytes": 2,
            "stderr_bytes": 0,
            "stdout_sha256": hashlib.sha256(b"{}").hexdigest(),
            "stderr_sha256": hashlib.sha256(b"").hexdigest(),
            "wall_ns": 1,
            "bounded_parse_valid": True,
        }

        def adapter(argv, *, root, timeout_ms):
            workload = argv[argv.index("--workload") + 1]
            scenario = argv[argv.index("--scenario") + 1]
            answer = malformed_answer if scenario == "enabled" else {}
            return {
                "schema_version": "atrinik.mcp.pilot-adapter/v1",
                "synthetic": True,
                "workload_id": workload,
                "scenario": scenario,
                "mutation_count": 0,
                "answer": answer,
                "error_code": pilot.FAILURE_CODES.get(scenario),
                "fallback_used": scenario != "enabled",
                "metrics": {
                    "calls": 0,
                    "retries": 0,
                    "tool_schema_bytes": 0,
                    "context_bytes": 0,
                    "cache_hits": 0,
                    "cache_misses": 0,
                    "external_network": False,
                },
            }, measurement

        with mock.patch.object(pilot, "_git_head", return_value="a" * 40), mock.patch.object(
            pilot, "_run_adapter", side_effect=adapter
        ):
            result = pilot.run_pilot(config, enable_real_pilot=False)
        enabled = [item for item in result["observations"] if item["scenario"] == "enabled"]
        self.assertFalse(result["correct"])
        self.assertTrue(enabled)
        self.assertTrue(all(not item["correct"] for item in enabled))
        self.assertTrue(all(item["coordinates"] == [] for item in enabled))

    def test_adapter_output_is_bounded_and_invalid_values_are_not_reported(self) -> None:
        program = "print('x' * 70000)"
        response, measured = pilot._run_adapter(
            [sys.executable, "-c", program], root=ROOT, timeout_ms=5000
        )
        self.assertEqual(response, {})
        self.assertLessEqual(measured["stdout_bytes"], 64 * 1024 + 1)
        self.assertFalse(measured["bounded_parse_valid"])
