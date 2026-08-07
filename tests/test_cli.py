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
                "default_branch": "main",
                "path": "/workspace/repos/client",
                "initialized": True,
                "branch": "main",
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
                "--port",
                "1731",
                "--dry-run",
                "--",
                "--version",
            ]
        )

        self.assertEqual(options.target, "server")
        self.assertEqual(options.profile, "mixed-review")
        self.assertEqual(options.state, "shared")
        self.assertEqual(options.port, 1731)
        self.assertTrue(options.dry_run)
        self.assertEqual(options.arguments, ["--", "--version"])

    def test_run_client_dispatches_matching_state_and_port(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(
                [
                    "run",
                    "client",
                    "--profile",
                    "review",
                    "--state",
                    "shared",
                    "--port",
                    "1731",
                    "--dry-run",
                ]
            )

        self.assertEqual(result, 0)
        workspace_type.return_value.run_client.assert_called_once_with(
            "review", "shared", 1731, [], True
        )

    def test_up_defaults_runtime_name_to_profile(self) -> None:
        status = {
            "supervisor": {"running": True},
            "endpoint": {
                "host": "127.0.0.1",
                "port": 17300,
                "fingerprint": "a" * 64,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.topology_up.return_value = status
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "up",
                        "--profile",
                        "review",
                        "--state",
                        "shared",
                        "--service",
                        "server",
                        "--port",
                        "17300",
                    ]
                )

        self.assertEqual(result, 0)
        workspace_type.return_value.topology_up.assert_called_once_with(
            "review", "review", "shared", ["server"], 17300
        )
        output.assert_called_once_with("topology review: started at 127.0.0.1:17300")

    def test_topology_show_supports_json(self) -> None:
        summary = {
            "profile": "review",
            "services": ["server"],
            "dependencies": ["server"],
            "state": "/state",
            "build_root": "/build",
            "components": {},
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.topology_summary.return_value = summary
            with mock.patch("builtins.print") as output:
                result = main(
                    ["topology", "show", "review", "--service", "server", "--json"]
                )

        self.assertEqual(result, 0)
        workspace_type.return_value.topology_summary.assert_called_once_with(
            "review", "default", ["server"]
        )
        self.assertEqual(json.loads(output.call_args.args[0]), summary)

    def test_ps_without_name_lists_all_topologies_as_json(self) -> None:
        statuses = [{"name": "baseline"}, {"name": "candidate"}]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.topology_statuses.return_value = statuses
            with mock.patch("builtins.print") as output:
                result = main(["ps", "--json"])

        self.assertEqual(result, 0)
        workspace_type.return_value.topology_statuses.assert_called_once_with()
        self.assertEqual(json.loads(output.call_args.args[0]), statuses)

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
