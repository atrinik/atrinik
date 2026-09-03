from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import unittest

from scripts.validate_windows_gpu_evidence import (
    COMMAND_NAMES,
    EvidenceError,
    main,
    validate_evidence,
)


class WindowsGpuEvidenceTests(unittest.TestCase):
    @staticmethod
    def _evidence() -> dict:
        commands = []
        for name in COMMAND_NAMES:
            not_run = name == "linux-coordinator-diagnostics"
            commands.append(
                {
                    "name": name,
                    "status": "not-run" if not_run else "passed",
                    "exit_code": None if not_run else 0,
                    "stdout_path": f"evidence/commands/{name}.stdout",
                    "stderr_path": f"evidence/commands/{name}.stderr",
                }
            )
        return {
            "schema_version": 1,
            "kind": "native-windows-classic-gpu-preflight",
            "recorded_at_utc": "2026-09-03T06:00:00Z",
            "status": "passed",
            "classification": "passed",
            "next_action": "none",
            "source": {
                "repository": "atrinik/classic",
                "commit": "a" * 40,
                "dirty": False,
                "profile": "classic-windows",
                "package_sha256": "b" * 64,
            },
            "host": {
                "os": "Windows 11",
                "os_build": "build 26100",
                "architecture": "x86_64",
            },
            "gpu": {
                "backend": "direct3d12",
                "adapter": "Qualified adapter",
                "device": "Qualified adapter",
                "driver_name": "Qualified driver",
                "driver_version": "1.0",
                "qualified_hardware": True,
                "hardware_tier": "reference",
            },
            "commands": commands,
            "benchmark": {
                "status": "passed",
                "performance_json_path": "evidence/performance.jsonl",
                "records": 15,
            },
            "logs": {
                "client": "evidence/logs/client.log",
                "server": "evidence/logs/server.log",
                "coordinator": "evidence/logs/coordinator.log",
            },
            "cleanup": {
                "status": "passed",
                "actions": [
                    "stopped packaged client",
                    "stopped packaged server",
                    "removed temporary extraction directory",
                ],
                "exit_codes": [0, 0, 0],
            },
            "failure": None,
        }

    @staticmethod
    def _materialize(root: Path, evidence: dict) -> None:
        paths = {
            command[field]
            for command in evidence["commands"]
            for field in ("stdout_path", "stderr_path")
        }
        paths.add(evidence["benchmark"]["performance_json_path"])
        paths.update(evidence["logs"].values())
        for value in paths:
            target = root.joinpath(*value.split("/"))
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("captured\n", encoding="utf-8")

    def test_positive_record_validates_and_cli_reports_summary(self) -> None:
        evidence = self._evidence()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._materialize(root, evidence)
            input_path = root / "evidence.json"
            input_path.write_text(json.dumps(evidence), encoding="utf-8")
            self.assertEqual(validate_evidence(evidence, root=root), evidence)
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                result = main([str(input_path), "--root", str(root)])
            self.assertEqual(result, 0)
            self.assertEqual(
                json.loads(stdout.getvalue()),
                {
                    "classification": "passed",
                    "evidence_status": "passed",
                    "kind": "native-windows-classic-gpu-preflight",
                    "schema_version": 1,
                    "status": "valid",
                },
            )

    def test_pass_requires_direct3d12_and_three_fresh_records(self) -> None:
        evidence = self._evidence()
        evidence["gpu"]["backend"] = "vulkan"
        with self.assertRaisesRegex(EvidenceError, "must use direct3d12"):
            validate_evidence(evidence)
        evidence = self._evidence()
        evidence["benchmark"]["records"] = 2
        with self.assertRaisesRegex(EvidenceError, "at least three"):
            validate_evidence(evidence)

    def test_failure_classification_preserves_actionable_gpu_result(self) -> None:
        evidence = self._evidence()
        evidence["status"] = "failed"
        evidence["classification"] = "gpu-backend-device"
        evidence["next_action"] = "Install a qualified Direct3D12 driver and rerun"
        evidence["gpu"].update(
            {
                "backend": "unavailable",
                "adapter": "<unavailable>",
                "device": "<unavailable>",
                "driver_name": "<unavailable>",
                "driver_version": "<unavailable>",
                "qualified_hardware": False,
                "hardware_tier": "unknown",
            }
        )
        benchmark = evidence["benchmark"]
        benchmark.update({"status": "failed", "records": 0})
        evidence["commands"][3].update({"status": "failed", "exit_code": 1})
        evidence["failure"] = {
            "command": "d3d12-benchmark",
            "message": "Direct3D12 device creation failed",
        }
        self.assertEqual(validate_evidence(evidence), evidence)

    def test_paths_and_secret_like_values_fail_closed(self) -> None:
        evidence = self._evidence()
        evidence["commands"][0]["stdout_path"] = "evidence/../private.log"
        with self.assertRaisesRegex(EvidenceError, "relative evidence path"):
            validate_evidence(evidence)
        evidence = self._evidence()
        evidence["commands"][0]["stdout_path"] = "evidence/" + ("a" * 152)
        with self.assertRaisesRegex(EvidenceError, "relative evidence path"):
            validate_evidence(evidence)
        evidence = self._evidence()
        evidence["gpu"]["driver_version"] = "token: abc123"
        with self.assertRaisesRegex(EvidenceError, "sensitive"):
            validate_evidence(evidence)

    def test_cli_rejects_invalid_json_without_echoing_input(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "evidence.json"
            path.write_text("{not-json}", encoding="utf-8")
            stderr = io.StringIO()
            from contextlib import redirect_stderr

            with redirect_stderr(stderr):
                result = main([str(path)])
            self.assertEqual(result, 1)
            self.assertIn("not valid UTF-8 JSON", stderr.getvalue())
            self.assertNotIn("not-json", stderr.getvalue())

    def test_cli_rejects_duplicate_keys_and_invalid_types(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            duplicate_path = Path(temporary) / "duplicate.json"
            duplicate_path.write_text(
                '{"schema_version": 1, "schema_version": 2}', encoding="utf-8"
            )
            self.assertEqual(main([str(duplicate_path)]), 1)
            evidence = self._evidence()
            evidence["status"] = []
            with self.assertRaisesRegex(EvidenceError, "status must be"):
                validate_evidence(evidence)
            evidence = self._evidence()
            evidence["gpu"]["hardware_tier"] = []
            with self.assertRaisesRegex(EvidenceError, "hardware_tier"):
                validate_evidence(evidence)
            evidence = self._evidence()
            evidence["gpu"]["backend"] = []
            with self.assertRaisesRegex(EvidenceError, "backend"):
                validate_evidence(evidence)


if __name__ == "__main__":
    unittest.main()
