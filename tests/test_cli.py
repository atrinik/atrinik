from __future__ import annotations

import unittest

from atrinik_workspace.cli import parser


class ParserTests(unittest.TestCase):
    def test_run_options_follow_component_subcommand(self) -> None:
        options = parser().parse_args(
            [
                "run",
                "server",
                "--profile",
                "mixed-review",
                "--state",
                "shared",
                "--dry-run",
                "--",
                "--version",
            ]
        )

        self.assertEqual(options.target, "server")
        self.assertEqual(options.profile, "mixed-review")
        self.assertEqual(options.state, "shared")
        self.assertTrue(options.dry_run)
        self.assertEqual(options.arguments, ["--", "--version"])


if __name__ == "__main__":
    unittest.main()
