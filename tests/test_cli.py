from __future__ import annotations

import json
from pathlib import Path
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError


class ParserTests(unittest.TestCase):
    def test_status_supports_machine_readable_output(self) -> None:
        rows = [
            {
                "component": "client",
                "repository": "atrinik/client",
                "default_branch": "master",
                "path": "/workspace/repos/client",
                "initialized": True,
                "branch": "master",
                "head": "0123456789ab",
                "dirty": False,
                "remote": "origin",
                "ahead": 0,
                "behind": 0,
            }
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.repository_status.return_value = rows
            with mock.patch("builtins.print") as output:
                result = main(["status", "client", "--json"])

        self.assertEqual(result, 0)
        workspace_type.return_value.repository_status.assert_called_once_with(["client"])
        self.assertEqual(json.loads(output.call_args.args[0]), rows)

    def test_profile_create_can_clone_another_profile(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(["profile", "create", "copy", "--from", "review"])

        self.assertEqual(result, 0)
        workspace_type.return_value.create_profile.assert_called_once_with(
            "copy", "review"
        )

    def test_path_prints_resolved_component_checkout(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.component_path.return_value = Path(
                "/workspace/worktrees/server/change"
            )
            with mock.patch("builtins.print") as output:
                result = main(["path", "server", "--profile", "review"])

        self.assertEqual(result, 0)
        workspace_type.return_value.component_path.assert_called_once_with(
            "server", "review"
        )
        output.assert_called_once_with(Path("/workspace/worktrees/server/change"))

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
