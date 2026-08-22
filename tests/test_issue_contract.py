from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import runpy
import unittest
from unittest import mock

from atrinik_workspace.issue_contract import (
    MAX_ISSUE_BYTES,
    IssueContractError,
    find_violations,
    main,
    validate_issue_contract,
)


class IssueContractTests(unittest.TestCase):
    def test_rejects_active_retired_line_requirements(self) -> None:
        cases = (
            "Backport the fix to `1.x`.",
            "Backport the fix to\n`1.x`.",
            "Publish the release on the `1.x` branch.",
            "Select the `content-1x` checkout for this issue.",
            "Add the `released on @1.x` label.",
            "Maintain the `1.x` maintenance line.",
            "Required: backport the change to `1.x`.",
        )
        for body in cases:
            with self.subTest(body=body):
                with self.assertRaisesRegex(IssueContractError, "active 1.x"):
                    validate_issue_contract(body)

    def test_allows_supported_and_historical_references(self) -> None:
        body = """\
        Author the change in `content@main` and publish the Classic-target
        artifact generated from it. The former `1.x` branch is retired; its
        tags, releases, provenance, parity records, and migration snapshots
        remain immutable historical evidence. Do not backport to the retired
        line.
        """
        validate_issue_contract(body)

    def test_allows_linked_historical_issue_account(self) -> None:
        body = (
            "[content#239](https://github.com/atrinik/content/issues/239) "
            "required a backport to `1.x` and carried the old release label."
        )
        validate_issue_contract(body)

    def test_scans_markdown_blocks_and_skips_unrelated_text(self) -> None:
        body = """\
        content@main is the source of truth.
        - The `1.x` tag is recorded.
        - Backport the change to `1.x`.
        """
        violations = find_violations(body)
        self.assertEqual(len(violations), 1)
        self.assertEqual(violations[0].line, 3)

    def test_cli_rejects_ambiguous_input(self) -> None:
        for argv in ([], ["body.md", "--stdin"]):
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    main(argv)

    def test_module_entry_point_returns_success(self) -> None:
        stdout = io.StringIO()
        module_path = (
            Path(__file__).resolve().parents[1]
            / "atrinik_workspace"
            / "issue_contract.py"
        )
        with redirect_stdout(stdout), mock.patch(
            "sys.argv", [str(module_path), "--stdin"]
        ), mock.patch(
            "sys.stdin", io.StringIO("content@main is valid.\n")
        ), self.assertRaises(SystemExit) as exit_info:
            runpy.run_path(str(module_path), run_name="__main__")
        self.assertEqual(exit_info.exception.code, 0)
        self.assertIn("issue-contract: pass", stdout.getvalue())

    def test_rejects_oversized_input(self) -> None:
        with self.assertRaisesRegex(IssueContractError, "byte validation bound"):
            validate_issue_contract("x" * (MAX_ISSUE_BYTES + 1))

    def test_cli_accepts_stdin_and_reports_failures(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout), mock.patch(
            "sys.stdin", io.StringIO("Historical `1.x` tags remain immutable.\n")
        ):
            self.assertEqual(main(["--stdin"]), 0)
        self.assertIn("issue-contract: pass", stdout.getvalue())

        stderr = io.StringIO()
        with redirect_stderr(stderr), mock.patch(
            "sys.stdin", io.StringIO("Use branch `1.x` for this work.\n")
        ):
            self.assertEqual(main(["--stdin"]), 1)
        self.assertIn("issue-contract:", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
