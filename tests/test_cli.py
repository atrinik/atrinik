from __future__ import annotations

import json
from pathlib import Path
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError


class ParserTests(unittest.TestCase):
    def test_supply_chain_commands_dispatch_validated_inventory(self) -> None:
        inventory = mock.Mock()
        inventory.dependencies = [object(), object()]
        inventory.audit.return_value = ["client: audited"]
        inventory.report.return_value = "report\n"
        roots = {"atrinik": Path("/workspace/atrinik")}
        with (
            mock.patch("atrinik_workspace.cli.Workspace"),
            mock.patch(
                "atrinik_workspace.cli.Inventory.load", return_value=inventory
            ) as load,
            mock.patch(
                "atrinik_workspace.cli.repository_roots", return_value=roots
            ) as resolve_roots,
            mock.patch(
                "atrinik_workspace.cli.version_report", return_value="versions\n"
            ) as versions,
            mock.patch("atrinik_workspace.cli.write_generated") as write,
            mock.patch("builtins.print") as output,
        ):
            self.assertEqual(main(["supply-chain", "validate"]), 0)
            self.assertEqual(
                main(
                    [
                        "supply-chain",
                        "audit",
                        "--profile",
                        "review",
                        "--repository",
                        "client=/tmp/client",
                    ]
                ),
                0,
            )
            self.assertEqual(
                main(
                    [
                        "supply-chain",
                        "report",
                        "--format",
                        "spdx",
                        "--output",
                        "build/report.json",
                    ]
                ),
                0,
            )
            self.assertEqual(
                main(
                    [
                        "supply-chain",
                        "versions",
                        "--output",
                        "build/versions.json",
                    ]
                ),
                0,
            )

        self.assertEqual(load.call_count, 4)
        self.assertEqual(inventory.validate_schema.call_count, 4)
        resolve_roots.assert_called_once()
        self.assertEqual(resolve_roots.call_args.args[2], "review")
        self.assertEqual(resolve_roots.call_args.args[3], ["client=/tmp/client"])
        inventory.audit.assert_called_once_with(roots)
        inventory.report.assert_called_once_with("spdx")
        versions.assert_called_once_with(inventory)
        self.assertEqual(write.call_count, 2)
        self.assertTrue(
            any("valid (2 dependencies)" in str(call.args[0]) for call in output.call_args_list)
        )
        self.assertTrue(
            any("client: audited" in str(call.args[0]) for call in output.call_args_list)
        )

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

    def test_scenario_create_prints_complete_manual_handoff(self) -> None:
        summary = {
            "name": "issue-42",
            "profile": "issue-42",
            "preset": "basic-player",
            "state": "scenario-issue-42",
            "account": "scenario12345678",
            "character": "Scenario 12345678",
            "path": "/workspace/scenarios/issue-42",
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scenario_create.return_value = summary
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "scenario",
                        "create",
                        "issue-42",
                        "--profile",
                        "issue-42",
                    ]
                )

        self.assertEqual(result, 0)
        workspace_type.return_value.scenario_create.assert_called_once_with(
            "issue-42", "issue-42", "basic-player"
        )
        rendered = "\n".join(str(call.args[0]) for call in output.call_args_list)
        self.assertIn("./atrinik profile show issue-42", rendered)
        self.assertIn("./atrinik build server --profile issue-42 --test", rendered)
        self.assertIn("./atrinik scenario credentials issue-42", rendered)
        self.assertIn(
            "./atrinik up --name issue-42 --profile issue-42 "
            "--state scenario-issue-42",
            rendered,
        )
        self.assertIn("./atrinik ps issue-42 --json", rendered)
        self.assertIn("./atrinik logs issue-42 client --follow", rendered)
        self.assertIn("./atrinik down issue-42", rendered)

    def test_scenario_credentials_are_explicitly_requested(self) -> None:
        credentials = {
            "account": "scenario12345678",
            "character": "Scenario 12345678",
            "password": "secret-value",
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scenario_credentials.return_value = credentials
            with mock.patch("builtins.print") as output:
                result = main(["scenario", "credentials", "issue-42"])

        self.assertEqual(result, 0)
        rendered = "\n".join(str(call.args[0]) for call in output.call_args_list)
        self.assertEqual(
            rendered,
            "account\tscenario12345678\n"
            "character\tScenario 12345678\n"
            "password\tsecret-value",
        )

    def test_scenario_create_json_is_machine_readable(self) -> None:
        summary = {
            "name": "issue-42",
            "profile": "issue-42",
            "preset": "basic-player",
            "state": "scenario-issue-42",
            "account": "scenario12345678",
            "character": "Scenario 12345678",
            "path": "/workspace/scenarios/issue-42",
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scenario_create.return_value = summary
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "scenario",
                        "create",
                        "issue-42",
                        "--profile",
                        "issue-42",
                        "--json",
                    ]
                )

        self.assertEqual(result, 0)
        self.assertEqual(json.loads(output.call_args.args[0]), summary)


if __name__ == "__main__":
    unittest.main()
