from __future__ import annotations

import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError


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

    def test_relative_external_profile_path_is_not_silently_absolutized(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.set_profile.side_effect = WorkspaceError(
                "profile checkout path must be absolute"
            )
            with mock.patch("sys.stderr"):
                result = main(
                    ["profile", "set", "review", "content", "--path", "relative"]
                )

        self.assertEqual(result, 1)
        workspace_type.return_value.set_profile.assert_called_once_with(
            "review", "content", "path", "relative"
        )


if __name__ == "__main__":
    unittest.main()
