from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import unittest
from unittest import mock

from atrinik_workspace.issue_contract import (
    MAX_ISSUE_BYTES,
    IssueContractError,
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
