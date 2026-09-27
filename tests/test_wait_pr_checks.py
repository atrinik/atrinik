from __future__ import annotations

import importlib.util
import io
import json
import math
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock


SCRIPT = Path(__file__).parent.parent / "scripts" / "wait_pr_checks.py"
SPEC = importlib.util.spec_from_file_location("wait_pr_checks", SCRIPT)
assert SPEC and SPEC.loader
watcher = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = watcher
SPEC.loader.exec_module(watcher)


class Clock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


def check(name, status="COMPLETED", conclusion="SUCCESS"):
    return {"__typename": "CheckRun", "name": name, "status": status, "conclusion": conclusion}


def snapshot(*checks, head="head", base="base", base_ref="main"):
    return {
        "headRefOid": head,
        "baseRefOid": base,
        "baseRefName": base_ref,
        "statusCheckRollup": list(checks),
    }


class Runner:
    def __init__(self, responses):
        self.responses = iter(responses)
        self.calls = []

    def __call__(self, command, **kwargs):
        self.calls.append((command, kwargs))
        response = next(self.responses)
        if isinstance(response, Exception):
            raise response
        if isinstance(response, tuple):
            return subprocess.CompletedProcess(command, response[0], response[1], response[2])
        return subprocess.CompletedProcess(command, 0, json.dumps(response), "")


class WaitPrChecksTests(unittest.TestCase):
    def run_wait(self, responses, *, expected=("build",), timeout=3, expected_base=None):
        clock = Clock()
        output = []
        runner = Runner(responses)
        result = watcher.wait_for_checks(
            "7", "owner/repo", "head", expected,
            expected_base=expected_base,
            timeout=timeout,
            interval=1,
            command_timeout=2,
            runner=runner,
            monotonic=clock.monotonic,
            sleep=clock.sleep,
            emit=output.append,
        )
        return result, output, runner

    def test_success_requires_fresh_second_pinned_snapshot(self):
        value = snapshot(check("build"))
        result, output, runner = self.run_wait([value, value])
        self.assertEqual(result, 0)
        self.assertEqual(len(runner.calls), 2)
        self.assertEqual(output, [
            "checks: build=success",
            "result: success (all mandatory checks passed on the pinned head and base)",
        ])
        self.assertLessEqual(runner.calls[0][1]["timeout"], 2)

    def test_unchanged_pending_summary_is_suppressed(self):
        pending = snapshot(check("build", "IN_PROGRESS", None))
        result, output, _ = self.run_wait([pending, pending, pending])
        self.assertEqual(result, 1)
        self.assertEqual(output.count("checks: build=pending"), 1)
        self.assertEqual(output[-1], "result: failure (timeout; build=pending)")

    def test_empty_and_missing_rollups_never_pass(self):
        for value in (snapshot(), snapshot(check("other"))):
            with self.subTest(value=value):
                result, output, _ = self.run_wait([value], timeout=1)
                self.assertEqual(result, 1)
                self.assertIn("checks: build=missing", output)
                self.assertEqual(output[-1], "result: failure (timeout; build=missing)")

    def test_failure_cancelled_neutral_skipped_and_unknown_are_terminal(self):
        for conclusion in ("FAILURE", "CANCELLED", "NEUTRAL", "SKIPPED", "MYSTERY"):
            with self.subTest(conclusion=conclusion):
                result, output, _ = self.run_wait([snapshot(check("build", conclusion=conclusion))])
                self.assertEqual(result, 1)
                expected = conclusion.lower() if conclusion != "MYSTERY" else "unknown"
                self.assertIn(f"build={expected}", output[-1])

    def test_head_and_base_drift_fail_closed(self):
        pending = snapshot(check("build", "QUEUED", None))
        cases = (
            ([snapshot(check("build"), head="other")], "head drift"),
            ([pending, snapshot(check("build"), base="changed")], "base drift"),
        )
        for responses, message in cases:
            with self.subTest(message=message):
                result, output, _ = self.run_wait(responses)
                self.assertEqual(result, 1)
                self.assertIn(message, output[-1])

    def test_expected_base_accepts_sha_or_ref_and_rejects_mismatch(self):
        value = snapshot(check("build"))
        for expected_base in ("base", "main"):
            with self.subTest(expected_base=expected_base):
                result, _, _ = self.run_wait([value, value], expected_base=expected_base)
                self.assertEqual(result, 0)
        result, output, _ = self.run_wait([value], expected_base="release")
        self.assertEqual(result, 1)
        self.assertIn("does not match", output[-1])

    def test_api_errors_and_command_timeouts_are_sanitized_and_bounded(self):
        failures = [
            (1, "secret stdout", "token=secret"),
            subprocess.TimeoutExpired(["gh"], 2, output="secret", stderr="secret"),
            (0, "not-json", "private diagnostics"),
        ]
        result, output, runner = self.run_wait(failures)
        self.assertEqual(result, 1)
        self.assertEqual(output.count("snapshot: unavailable"), 1)
        self.assertFalse(any("secret" in line or "private" in line for line in output))
        self.assertTrue(all(0 < call[1]["timeout"] <= 2 for call in runner.calls))

    def test_status_context_is_supported_and_all_duplicate_names_must_succeed(self):
        context = {"__typename": "StatusContext", "context": "build", "state": "SUCCESS"}
        success = snapshot(context)
        result, _, _ = self.run_wait([success, success])
        self.assertEqual(result, 0)

        mixed = snapshot(context, check("build", "IN_PROGRESS", None))
        result, output, _ = self.run_wait([mixed], timeout=1)
        self.assertEqual(result, 1)
        self.assertIn("checks: build=pending", output)

    def test_invalid_timing_never_invokes_gh_or_sleep(self):
        for option in ("timeout", "interval", "command_timeout"):
            for value in (0, -1, math.inf, -math.inf, math.nan):
                with self.subTest(option=option, value=value):
                    calls = []
                    arguments = {"timeout": 1, "interval": 1, "command_timeout": 1}
                    arguments[option] = value
                    with self.assertRaisesRegex(ValueError, "finite and positive"):
                        watcher.wait_for_checks(
                            "7", "owner/repo", "head", ("build",),
                            runner=lambda *args, **kwargs: calls.append((args, kwargs)),
                            sleep=lambda seconds: calls.append(("sleep", seconds)),
                            **arguments,
                        )
                    self.assertEqual(calls, [])

    def test_cli_rejects_non_finite_timing_before_waiting(self):
        base = ["7", "--repo", "owner/repo", "--expected-head", "head", "--expect", "build"]
        for option in ("--timeout", "--interval", "--command-timeout"):
            for value in ("nan", "inf", "-inf"):
                with self.subTest(option=option, value=value):
                    with mock.patch.object(watcher, "wait_for_checks") as wait:
                        with mock.patch("sys.stderr", new=io.StringIO()):
                            with self.assertRaises(SystemExit) as raised:
                                watcher.main([*base, f"{option}={value}"])
                    self.assertEqual(raised.exception.code, 2)
                    wait.assert_not_called()


if __name__ == "__main__":
    unittest.main()
