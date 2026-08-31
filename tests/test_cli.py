from __future__ import annotations

import json
from pathlib import Path
import unittest
from unittest import mock

from atrinik_workspace.cli import _human_bytes, _parse_services, main, parser
from atrinik_workspace.model import WorkspaceError


class ParserTests(unittest.TestCase):
    def test_scope_human_output_reports_exact_coordinates(self) -> None:
        record = {
            "name": "review",
            "generation": "generation-1",
            "status": "complete",
            "profile": {"name": "scope-review", "path": "/profiles/scope-review.json"},
            "topology": {"name": "scope-review", "path": "/topologies/scope-review"},
            "worktrees": [
                {
                    "checkout": "client",
                    "branch": "scope/review/client",
                    "path": "/worktrees/client/scope-review",
                }
            ],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scope_create.return_value = record
            with mock.patch("builtins.print") as output:
                result = main(["scope", "create", "client", "--name", "review"])

        self.assertEqual(result, 0)
        self.assertEqual(
            [call.args[0] for call in output.call_args_list],
            [
                "scope\treview\tgeneration-1",
                "profile\tscope-review\t/profiles/scope-review.json",
                "topology\tscope-review\t/topologies/scope-review",
                "worktree\tclient\tscope/review/client\t/worktrees/client/scope-review",
            ],
        )

        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scope_list.return_value = [record]
            with mock.patch("builtins.print") as output:
                result = main(["scope", "list"])
        self.assertEqual(result, 0)
        output.assert_called_once_with("review\tcomplete\tscope-review\tscope-review")

    def test_scope_release_human_output_and_preview_guard(self) -> None:
        release = {
            "scope": "review",
            "mode": "dry-run",
            "plan_sha256": "a" * 64,
            "items": [
                {
                    "disposition": "candidate",
                    "kind": "profile",
                    "path": "/profiles/scope-review.json",
                    "reasons": [],
                },
                {
                    "disposition": "protected",
                    "kind": "state",
                    "path": None,
                    "reasons": ["persistent state"],
                },
            ],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scope_release.return_value = release
            with mock.patch("builtins.print") as output:
                result = main(["scope", "release", "review", "--dry-run"])

        self.assertEqual(result, 0)
        self.assertEqual(
            [call.args[0] for call in output.call_args_list],
            [
                "scope\treview\tdry-run",
                f"plan\t{'a' * 64}",
                "candidate\tprofile\t/profiles/scope-review.json\t",
                "protected\tstate\t-\tpersistent state",
            ],
        )
        with mock.patch("atrinik_workspace.cli.Workspace"):
            with mock.patch("sys.stderr"):
                result = main(
                    ["scope", "release", "review", "--dry-run", "--plan", "a" * 64]
                )
        self.assertEqual(result, 1)

    def test_scope_create_dispatches_exact_coordinates_as_json(self) -> None:
        record = {"schema_version": 1, "name": "review", "status": "complete"}
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scope_create.return_value = record
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "scope", "create", "classic-server", "content",
                        "--name", "review", "--from", "classic",
                        "--branch", "classic=feat/review", "--state", "shared",
                        "--json",
                    ]
                )
        self.assertEqual(result, 0)
        workspace_type.return_value.scope_create.assert_called_once_with(
            ["classic-server", "content"],
            name="review",
            base_profile="classic",
            labels=[],
            branches=["classic=feat/review"],
            start_points=[],
            topology=None,
            state_mode="named",
            state_name="shared",
        )
        self.assertEqual(json.loads(output.call_args.args[0]), record)

    def test_scope_release_requires_preview_mode_and_forwards_plan(self) -> None:
        options = parser().parse_args(
            ["scope", "release", "review", "--apply", "--plan", "a" * 64]
        )
        self.assertTrue(options.apply)
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scope_release.return_value = {
                "scope": "review", "mode": "apply", "plan_sha256": "a" * 64,
                "items": [],
            }
            result = main(
                ["scope", "release", "review", "--apply", "--plan", "a" * 64, "--json"]
            )
        self.assertEqual(result, 0)
        workspace_type.return_value.scope_release.assert_called_once_with(
            "review", apply=True, plan_sha256="a" * 64
        )

    def test_cleanup_accepts_the_explicit_topologies_scope(self) -> None:
        options = parser().parse_args(["cleanup", "--scope", "topologies"])
        self.assertEqual(options.scope, ["topologies"])

    def test_cleanup_accepts_the_explicit_journal_scope_and_name(self) -> None:
        options = parser().parse_args(
            ["cleanup", "receipt.json", "--scope", "cleanup-journals"]
        )
        self.assertEqual(options.components, ["receipt.json"])
        self.assertEqual(options.scope, ["cleanup-journals"])

    def test_human_bytes_uses_compact_iec_units_and_promotes_rounding(self) -> None:
        self.assertEqual(
            [_human_bytes(value) for value in (0, 1023, 1024, 1536)],
            ["0B", "1023B", "1KiB", "1.5KiB"],
        )
        self.assertEqual(_human_bytes(1024**2 - 1), "1MiB")
        self.assertEqual(_human_bytes(1024**3), "1GiB")
        self.assertEqual(_human_bytes(1024**4), "1TiB")
        self.assertEqual(_human_bytes(1024**5), "1PiB")
        self.assertEqual(_human_bytes(1024**6), "1EiB")
        with self.assertRaisesRegex(ValueError, "cannot be negative"):
            _human_bytes(-1)

    def test_cleanup_defaults_to_preview_and_reports_json(self) -> None:
        report = {
            "schema_version": 1,
            "mode": "dry-run",
            "scopes": ["worktrees", "builds"],
            "older_than_days": 7,
            "filters": [],
            "inventory_errors": [],
            "items": [],
            "summary": {
                "item_count": 0,
                "candidate_count": 0,
                "candidate_bytes": 0,
                "protected_count": 0,
                "protected_bytes": 0,
                "skipped_count": 0,
                "skipped_bytes": 0,
                "removed_count": 0,
                "removed_bytes": 0,
                "error_count": 0,
                "error_bytes": 0,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.cleanup.return_value = report
            with mock.patch("builtins.print") as output:
                result = main(["cleanup", "--dry-run", "--json"])

        self.assertEqual(result, 0)
        workspace_type.return_value.cleanup.assert_called_once_with([], 7, [], False)
        self.assertEqual(json.loads(output.call_args.args[0]), report)

    def test_cleanup_json_preserves_exact_numeric_byte_fields(self) -> None:
        allocated = 1024**4 + 123
        ignored = 1024**3 + 7
        report = {
            "items": [
                {
                    "allocated_bytes": allocated,
                    "ignored_bytes": ignored,
                }
            ],
            "summary": {
                "candidate_bytes": allocated,
                "protected_bytes": ignored,
                "removed_bytes": allocated + ignored,
                "error_count": 0,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.cleanup.return_value = report
            with mock.patch("builtins.print") as output:
                result = main(["cleanup", "--json"])

        rendered = json.loads(output.call_args.args[0])
        self.assertEqual(result, 0)
        self.assertEqual(rendered, report)
        self.assertEqual(rendered["items"][0]["allocated_bytes"], allocated)
        self.assertEqual(rendered["items"][0]["ignored_bytes"], ignored)
        self.assertEqual(
            rendered["summary"]["removed_bytes"], allocated + ignored
        )

    def test_cleanup_combines_scopes_filters_and_reports_apply_failure(self) -> None:
        report = {
            "schema_version": 1,
            "mode": "apply",
            "scopes": ["worktrees", "builds"],
            "older_than_days": 0,
            "filters": ["classic"],
            "inventory_errors": [],
            "items": [],
            "summary": {
                "item_count": 0,
                "candidate_count": 0,
                "candidate_bytes": 0,
                "protected_count": 0,
                "protected_bytes": 0,
                "skipped_count": 0,
                "skipped_bytes": 0,
                "removed_count": 0,
                "removed_bytes": 0,
                "error_count": 1,
                "error_bytes": 0,
            },
            "aborted": True,
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.cleanup.return_value = report
            with mock.patch("builtins.print"):
                result = main(
                    [
                        "cleanup",
                        "classic-client",
                        "--scope",
                        "worktrees",
                        "--scope",
                        "builds",
                        "--older-than",
                        "0",
                        "--apply",
                        "--json",
                    ]
                )

        self.assertEqual(result, 1)
        workspace_type.return_value.cleanup.assert_called_once_with(
            ["worktrees", "builds"], 0, ["classic-client"], True
        )
        workspace_type.return_value.cleanup_acknowledge.assert_not_called()

    def test_cleanup_apply_flushes_output_before_acknowledging_receipt(self) -> None:
        report = {
            "items": [],
            "summary": {"error_count": 0},
        }
        events = mock.Mock()
        output = mock.Mock()
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace = workspace_type.return_value
            workspace.cleanup.return_value = report
            events.attach_mock(output.flush, "flush")
            events.attach_mock(workspace.cleanup_acknowledge, "acknowledge")
            with mock.patch("sys.stdout", output):
                result = main(["cleanup", "--apply", "--json"])

        self.assertEqual(result, 0)
        workspace.cleanup_acknowledge.assert_called_once_with(report)
        self.assertLess(
            events.mock_calls.index(mock.call.flush()),
            events.mock_calls.index(mock.call.acknowledge(report)),
        )

    def test_cleanup_apply_reports_skipped_result_without_acknowledging(self) -> None:
        report = {
            "items": [
                {
                    "disposition": "skipped",
                    "kind": "worktree",
                    "path": "/workspace/busy",
                    "reasons": ["resource_busy"],
                }
            ],
            "summary": {
                "error_count": 0,
                "skipped_count": 1,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace = workspace_type.return_value
            workspace.cleanup.return_value = report
            with mock.patch("builtins.print") as output:
                result = main(["cleanup", "--apply", "--json"])

        self.assertEqual(result, 1)
        workspace.cleanup_acknowledge.assert_not_called()
        self.assertIn(
            mock.call(
                "cleanup apply is incomplete; retry the identical request",
                file=mock.ANY,
            ),
            output.mock_calls,
        )

    def test_cleanup_apply_acknowledges_terminal_inventory_skip(self) -> None:
        report = {
            "items": [
                {
                    "disposition": "skipped",
                    "kind": "sound-cache",
                    "path": "/workspace/retained",
                    "reasons": ["retained_by_age"],
                }
            ],
            "summary": {
                "error_count": 0,
                "skipped_count": 1,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace = workspace_type.return_value
            workspace.cleanup.return_value = report
            with mock.patch("builtins.print"):
                result = main(["cleanup", "--apply", "--json"])

        self.assertEqual(result, 0)
        workspace.cleanup_acknowledge.assert_called_once_with(report)

    def test_cleanup_text_report_includes_item_age_reasons_and_totals(self) -> None:
        report = {
            "items": [
                {
                    "disposition": "eligible",
                    "kind": "worktree",
                    "allocated_bytes": 4096,
                    "ignored_bytes": 1024,
                    "age_seconds": 2 * 86400,
                    "path": "/workspace/review",
                    "reasons": ["merged_pr_head"],
                }
            ],
            "summary": {
                "candidate_count": 1,
                "candidate_bytes": 4096,
                "protected_count": 0,
                "protected_bytes": 0,
                "removed_count": 0,
                "removed_bytes": 0,
                "error_count": 0,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.cleanup.return_value = report
            with mock.patch("builtins.print") as output:
                result = main(["cleanup", "--scope", "all"])

        self.assertEqual(result, 0)
        lines = [call.args[0] for call in output.call_args_list]
        self.assertIn(
            "eligible\tworktree\tallocated=4KiB,ignored=1KiB\t2d\t"
            "/workspace/review\tmerged_pr_head",
            lines,
        )
        self.assertIn(
            "summary\tcandidates=1 candidate_bytes=4KiB protected=0 "
            "protected_bytes=0B removed=0 removed_bytes=0B errors=0",
            lines,
        )

    def test_cleanup_text_report_includes_topology_observation_and_paths(self) -> None:
        report = {
            "items": [
                {
                    "disposition": "eligible",
                    "kind": "topology",
                    "allocated_bytes": 512,
                    "age_seconds": 8 * 86400,
                    "path": "/workspace/topologies/old",
                    "reasons": ["inactive_topology"],
                    "name": "old",
                    "liveness": "exited",
                    "control_observation": "legacy",
                    "generation": None,
                    "process_tree_lease": "released",
                    "runtime_bundle_lease": "historical",
                    "port_reservation_lease": "released",
                    "repository_layout_lease": "released",
                    "age_basis": "stopped-at",
                    "age_observed_at": "2026-08-01T00:00:00+00:00",
                    "deletion_paths": [
                        "/workspace/topologies/old",
                        "/workspace/topologies/old/status.json",
                    ],
                }
            ],
            "summary": {
                "candidate_count": 1,
                "candidate_bytes": 512,
                "protected_count": 0,
                "protected_bytes": 0,
                "removed_count": 0,
                "removed_bytes": 0,
                "error_count": 0,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.cleanup.return_value = report
            with mock.patch("builtins.print") as output:
                result = main(["cleanup", "--scope", "topologies"])

        lines = [call.args[0] for call in output.call_args_list]
        self.assertEqual(result, 0)
        self.assertIn(
            "topology-observation\told\tliveness=exited\tcontrol=legacy\t"
            "generation=-\tprocess-tree=released\truntime-bundle=historical\t"
            "port-reservation=released\trepository-layout=released\t"
            "age-basis=stopped-at\tage-observed-at=2026-08-01T00:00:00+00:00",
            lines,
        )
        self.assertIn(
            "delete\told\t/workspace/topologies/old/status.json", lines
        )

    def test_cleanup_text_report_uses_concise_iec_byte_units(self) -> None:
        report = {
            "items": [
                {
                    "disposition": "eligible",
                    "kind": "worktree",
                    "allocated_bytes": 0,
                    "ignored_bytes": 511,
                    "age_seconds": None,
                    "path": "/workspace/zero",
                    "reasons": ["zero-sized"],
                },
                {
                    "disposition": "protected",
                    "kind": "worktree",
                    "allocated_bytes": 1536,
                    "ignored_bytes": 1024**2,
                    "age_seconds": 0,
                    "path": "/workspace/medium",
                    "reasons": ["protected"],
                },
                {
                    "disposition": "removed",
                    "kind": "worktree",
                    "allocated_bytes": 1024**3,
                    "ignored_bytes": 1024**4,
                    "age_seconds": 86400,
                    "path": "/workspace/large",
                    "reasons": ["removed"],
                },
                {
                    "disposition": "skipped",
                    "kind": "profile-build",
                    "allocated_bytes": 1,
                    "age_seconds": None,
                    "path": "/workspace/no-ignored-size",
                    "reasons": ["retained"],
                },
            ],
            "summary": {
                "candidate_count": 1,
                "candidate_bytes": 0,
                "protected_count": 1,
                "protected_bytes": 1536,
                "removed_count": 1,
                "removed_bytes": 1024**3,
                "error_count": 0,
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.cleanup.return_value = report
            with mock.patch("builtins.print") as output:
                result = main(["cleanup"])

        lines = [call.args[0] for call in output.call_args_list]
        self.assertEqual(result, 0)
        self.assertIn(
            "eligible\tworktree\tallocated=0B,ignored=511B\t-\t"
            "/workspace/zero\tzero-sized",
            lines,
        )
        self.assertIn(
            "protected\tworktree\tallocated=1.5KiB,ignored=1MiB\t0d\t"
            "/workspace/medium\tprotected",
            lines,
        )
        self.assertIn(
            "removed\tworktree\tallocated=1GiB,ignored=1TiB\t1d\t"
            "/workspace/large\tremoved",
            lines,
        )
        self.assertIn(
            "skipped\tprofile-build\tallocated=1B\t-\t"
            "/workspace/no-ignored-size\tretained",
            lines,
        )
        self.assertIn(
            "summary\tcandidates=1 candidate_bytes=0B protected=1 "
            "protected_bytes=1.5KiB removed=1 removed_bytes=1GiB errors=0",
            lines,
        )

    def test_init_with_classic_dispatches_only_documented_additive_option(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(["init", "--with", "classic", "--jobs", "2"])

        self.assertEqual(result, 0)
        workspace_type.return_value.initialize.assert_called_once_with(
            [], 2, include_classic=True
        )

    def test_sync_with_classic_never_uses_an_initialization_alias(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(["sync", "--with", "classic"])

        self.assertEqual(result, 0)
        workspace_type.return_value.sync.assert_called_once_with(
            [], "none", include_classic=True
        )

    def test_build_dispatches_cache_controls(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.build.return_value = Path("/build")
            result = main(
                [
                    "build",
                    "client",
                    "--profile",
                    "review",
                    "--test",
                    "--force-reconfigure",
                    "--no-ccache",
                ]
            )

        self.assertEqual(result, 0)
        workspace_type.return_value.build.assert_called_once_with(
            "client",
            "review",
            True,
            force_reconfigure=True,
            use_ccache=False,
        )

    def test_dev_services_parse_in_stable_topology_order(self) -> None:
        self.assertEqual(_parse_services("client, server"), ["server", "client"])
        self.assertEqual(_parse_services("both"), ["server", "client"])
        with self.assertRaisesRegex(WorkspaceError, "duplicates"):
            _parse_services("server,server")
        with self.assertRaisesRegex(WorkspaceError, "only server and client"):
            _parse_services("worker")

    def test_dev_build_dispatches_selective_incremental_controls(self) -> None:
        summary = {
            "schema_version": 1,
            "profile": "classic",
            "services": ["server"],
            "tests": True,
            "build_root": "/workspace/build/classic",
            "cache": {
                "build_root": "reused",
                "inputs": {"content": "reused"},
                "cmake": {},
                "source_views": "reused",
            },
            "runtime": {"staging": "deferred until dev up"},
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.dev_build.return_value = summary
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "dev",
                        "build",
                        "--profile",
                        "classic",
                        "--services",
                        "server",
                        "--test",
                        "--force-reconfigure",
                        "--no-ccache",
                        "--json",
                    ]
                )

        self.assertEqual(result, 0)
        workspace_type.return_value.dev_build.assert_called_once_with(
            "classic",
            ["server"],
            True,
            force_reconfigure=True,
            use_ccache=False,
        )
        self.assertEqual(json.loads(output.call_args.args[0]), summary)

    def test_dev_up_and_restart_dispatch_exact_service_coordinates(self) -> None:
        status = {
            "name": "classic-local",
            "endpoint": {"host": "127.0.0.1", "port": 17300},
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.dev_up.return_value = status
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "dev",
                        "up",
                        "--name",
                        "classic-local",
                        "--services",
                        "client,server",
                        "--temporary-state",
                        "--port",
                        "17300",
                    ]
                )
        self.assertEqual(result, 0)
        workspace_type.return_value.dev_up.assert_called_once_with(
            "classic-local",
            "classic",
            None,
            ["server", "client"],
            17300,
            state_mode="temporary",
        )
        output.assert_called_once_with(
            "topology classic-local: started at 127.0.0.1:17300"
        )

        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.dev_restart.return_value = status
            with mock.patch("builtins.print") as output:
                result = main(
                    ["dev", "restart", "classic-local", "--service", "server"]
                )
        self.assertEqual(result, 0)
        workspace_type.return_value.dev_restart.assert_called_once_with(
            "classic-local", "server"
        )
        output.assert_called_once_with(
            "topology classic-local: restarted server at 127.0.0.1:17300"
        )

    def test_windows_package_dispatches_profile_state_port_and_output(self) -> None:
        summary = {
            "schema_version": 1,
            "profile": "review",
            "state": "scenario-review",
            "path": "/tmp/review.zip",
            "sha256": "a" * 64,
            "bytes": 123,
            "contains_private_server_state": True,
            "build": {"mode": "container", "image": "example@sha256:" + "b" * 64},
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.package_windows_profile.return_value = summary
            with mock.patch("builtins.print") as output:
                result = main(
                    [
                        "package",
                        "windows",
                        "--profile",
                        "review",
                        "--state",
                        "scenario-review",
                        "--port",
                        "1731",
                        "--output",
                        "/tmp/review.zip",
                        "--json",
                    ]
                )

        self.assertEqual(result, 0)
        workspace_type.return_value.package_windows_profile.assert_called_once_with(
            "review", "scenario-review", Path("/tmp/review.zip"), port=1731
        )
        self.assertEqual(json.loads(output.call_args.args[0]), summary)

    def test_classic_cohort_option_rejects_abbreviated_spelling(self) -> None:
        for command in ("init", "sync"):
            with self.subTest(command=command):
                with self.assertRaises(SystemExit):
                    parser().parse_args([command, "--wi", "classic"])

    def test_repository_migration_json_dispatches_selected_mode(self) -> None:
        plan = {
            "migration": "repositories",
            "status": "ready",
            "moves": [],
            "refusals": [],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.migrate_repositories.return_value = plan
            with mock.patch("builtins.print") as output:
                result = main(
                    ["migrate", "repositories", "--dry-run", "--json"]
                )

        self.assertEqual(result, 0)
        workspace_type.return_value.migrate_repositories.assert_called_once_with(
            "dry-run"
        )
        self.assertEqual(json.loads(output.call_args.args[0]), plan)

    def test_repository_migration_refusal_returns_failure(self) -> None:
        plan = {
            "migration": "repositories",
            "status": "refused",
            "moves": [],
            "refusals": [
                {
                    "code": "dirty_primary",
                    "message": "primary is dirty",
                    "recovery": "preserve the changes",
                }
            ],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.migrate_repositories.return_value = plan
            with mock.patch("builtins.print"):
                result = main(["migrate", "repositories", "--apply"])

        self.assertEqual(result, 1)

    def test_content_migration_json_dispatches_each_selected_mode(self) -> None:
        for flag, mode in (
            ("--dry-run", "dry-run"),
            ("--apply", "apply"),
            ("--audit", "audit"),
            ("--restore", "restore"),
        ):
            with self.subTest(flag=flag):
                plan = {
                    "migration": "content",
                    "status": "complete" if mode == "audit" else "ready",
                    "refusals": [],
                }
                with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
                    workspace_type.return_value.migrate_content.return_value = plan
                    with mock.patch("builtins.print") as output:
                        result = main(["migrate", "content", flag, "--json"])

                self.assertEqual(result, 0)
                workspace_type.return_value.migrate_content.assert_called_once_with(mode)
                self.assertEqual(json.loads(output.call_args.args[0]), plan)

    def test_content_migration_refusal_returns_failure(self) -> None:
        plan = {
            "migration": "content",
            "status": "refused",
            "refusals": [
                {
                    "code": "legacy_content_unproven",
                    "message": "legacy content is dirty",
                    "recovery": "preserve the checkout",
                }
            ],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.migrate_content.return_value = plan
            with mock.patch("builtins.print"):
                result = main(["migrate", "content", "--apply"])

        self.assertEqual(result, 1)

    def test_content_migration_text_reports_profiles_and_worktree_moves(self) -> None:
        plan = {
            "migration": "content",
            "status": "ready",
            "profiles": [
                {
                    "status": "rewrite",
                    "name": "classic-review",
                    "path": "/workspace/profiles/classic-review.json",
                }
            ],
            "worktree_moves": [
                {
                    "profile": "classic-review",
                    "source": "/workspace/worktrees/content-1x/maps",
                    "destination": "/workspace/worktrees/content/maps",
                }
            ],
            "refusals": [],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.migrate_content.return_value = plan
            with mock.patch("builtins.print") as output:
                result = main(["migrate", "content", "--dry-run"])

        self.assertEqual(result, 0)
        self.assertIn(
            mock.call(
                "profile\trewrite\tclassic-review\t"
                "/workspace/profiles/classic-review.json"
            ),
            output.call_args_list,
        )
        self.assertIn(
            mock.call(
                "worktree\tmove\tclassic-review\t"
                "/workspace/worktrees/content-1x/maps\t"
                "/workspace/worktrees/content/maps"
            ),
            output.call_args_list,
        )

    def test_repository_migration_text_reports_action_statuses(self) -> None:
        plan = {
            "migration": "repositories",
            "status": "ready",
            "classic": {
                "status": "verified",
                "path": "/workspace/classic",
            },
            "sources": [
                {
                    "status": "planned",
                    "component": "classic-client",
                    "source": "/workspace/legacy-client",
                    "archive": "/state/archive/legacy-client",
                }
            ],
            "worktree_migrations": [
                {
                    "status": "planned",
                    "component": "classic-client",
                    "path": "/workspace/worktrees/legacy-client/review",
                    "destination": "/state/worktrees/classic/review",
                }
            ],
            "composite_worktrees": [],
            "profile_rewrites": [
                {
                    "status": "planned",
                    "name": "review",
                    "path": "/state/profiles/review.json",
                }
            ],
            "topologies": [],
            "inert_paths": [],
            "refusals": [],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.migrate_repositories.return_value = plan
            with mock.patch("builtins.print") as output:
                result = main(["migrate", "repositories", "--dry-run"])

        self.assertEqual(result, 0)
        self.assertIn(
            mock.call(
                "source\tplanned\tclassic-client\t/workspace/legacy-client\t"
                "/state/archive/legacy-client"
            ),
            output.call_args_list,
        )
        self.assertIn(
            mock.call(
                "worktree\tplanned\tclassic-client\t"
                "/workspace/worktrees/legacy-client/review\t"
                "/state/worktrees/classic/review"
            ),
            output.call_args_list,
        )
        self.assertIn(
            mock.call("classic\tverified\t/workspace/classic"),
            output.call_args_list,
        )

    def test_supply_chain_commands_dispatch_validated_inventory(self) -> None:
        inventory = mock.Mock()
        inventory.dependencies = [object(), object()]
        inventory.audit.return_value = ["client: audited"]
        inventory.report.return_value = "report\n"
        roots = {"atrinik": Path("/workspace/atrinik")}
        commits = {"atrinik": "a" * 40}
        with (
            mock.patch("atrinik_workspace.cli.Workspace"),
            mock.patch(
                "atrinik_workspace.cli.Inventory.load", return_value=inventory
            ) as load,
            mock.patch(
                "atrinik_workspace.cli.repository_roots", return_value=roots
            ) as resolve_roots,
            mock.patch(
                "atrinik_workspace.cli.report_component_commits",
                return_value=("classic", commits),
            ) as resolve_commits,
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
                        "--profile",
                        "classic-review",
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
            self.assertEqual(main(["init"]), 0)

        self.assertEqual(load.call_count, 4)
        self.assertEqual(inventory.validate_schema.call_count, 4)
        resolve_roots.assert_called_once()
        self.assertEqual(resolve_roots.call_args.args[2], "review")
        self.assertEqual(resolve_roots.call_args.args[3], ["client=/tmp/client"])
        inventory.audit.assert_called_once_with(roots)
        resolve_commits.assert_called_once()
        self.assertEqual(resolve_commits.call_args.args[2], "classic-review")
        inventory.report.assert_called_once_with("spdx", commits, "classic")
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

    def test_worktree_list_wrapper_self_emits_complete_json_rows(self) -> None:
        rows = [
            (
                "atrinik",
                {
                    "worktree": "/workspace/atrinik",
                    "HEAD": "a" * 40,
                    "branch": "refs/heads/main",
                },
            ),
            (
                "atrinik",
                {
                    "worktree": "/workspace/atrinik/linked worktree",
                    "HEAD": "b" * 40,
                    "detached": "",
                    "locked": "inspection",
                },
            ),
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.list_wrapper_worktrees.return_value = rows
            with mock.patch("builtins.print") as output:
                result = main(["worktree", "list", "--wrapper-self", "--json"])

        self.assertEqual(result, 0)
        workspace_type.return_value.list_wrapper_worktrees.assert_called_once_with()
        workspace_type.return_value.list_worktrees.assert_not_called()
        self.assertEqual(
            json.loads(output.call_args.args[0]),
            [{"component": component, **record} for component, record in rows],
        )

    def test_worktree_list_manifest_mode_remains_unchanged(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.list_worktrees.return_value = []
            result = main(["worktree", "list", "client", "--json"])

        self.assertEqual(result, 0)
        workspace_type.return_value.list_worktrees.assert_called_once_with(["client"])
        workspace_type.return_value.list_wrapper_worktrees.assert_not_called()

    def test_worktree_list_wrapper_self_rejects_component_selectors(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            with mock.patch("sys.stderr"):
                result = main(
                    ["worktree", "list", "client", "--wrapper-self", "--json"]
                )

        self.assertEqual(result, 1)
        workspace_type.return_value.list_wrapper_worktrees.assert_not_called()
        workspace_type.return_value.list_worktrees.assert_not_called()

    def test_worktree_list_wrapper_self_bounds_retained_json(self) -> None:
        rows = [
            (
                "atrinik",
                {
                    "worktree": "/workspace/" + "x" * (512 * 1024),
                    "HEAD": "a" * 40,
                    "branch": "refs/heads/main",
                },
            )
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.list_wrapper_worktrees.return_value = rows
            with mock.patch("sys.stderr"):
                result = main(["worktree", "list", "--wrapper-self", "--json"])

        self.assertEqual(result, 1)

    def test_profile_create_can_clone_another_profile(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(["profile", "create", "copy", "--from", "review"])

        self.assertEqual(result, 0)
        workspace_type.return_value.create_profile.assert_called_once_with(
            "copy", "review"
        )

    def test_profile_sound_mode_dispatches_explicit_opt_in(self) -> None:
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(
                ["profile", "sound-mode", "audio-review", "local-playtest"]
            )

        self.assertEqual(result, 0)
        workspace_type.return_value.set_profile_sound_mode.assert_called_once_with(
            "audio-review", "local-playtest"
        )

    def test_profile_released_sound_dispatches_complete_coordinates(self) -> None:
        sha256 = "a" * 64
        commit = "b" * 40
        tree = "c" * 40
        arguments = [
            "profile", "sound-mode", "audio-release", "released",
            "--release-repository", "atrinik/sound",
            "--release-tag", "v1.4.0",
            "--release-product-version", "1.4.0",
            "--release-source-commit", commit,
            "--release-source-tree", tree,
            "--release-asset-url",
            "https://github.com/atrinik/sound/releases/download/v1.4.0/"
            "atrinik-sound-classic-runtime-1.4.0.tar.gz",
            "--release-archive-sha256", sha256,
            "--release-manifest-sha256", sha256,
            "--release-source-manifest-sha256", sha256,
            "--release-schema-sha256", sha256,
            "--release-toolchain-sha256", sha256,
            "--release-tree-sha256", sha256,
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            result = main(arguments)

        self.assertEqual(result, 0)
        coordinates = workspace_type.return_value.set_profile_sound_mode.call_args.args[2]
        self.assertEqual(coordinates["product"], "atrinik-sound-classic-runtime")
        self.assertEqual(coordinates["manifest_schema_version"], 1)
        self.assertEqual(coordinates["archive_sha256"], sha256)

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

    def test_topology_text_names_stack_and_logical_providers(self) -> None:
        summary = {
            "profile": "review",
            "stack": "classic",
            "services": ["server"],
            "dependencies": ["protocol", "server"],
            "providers": {
                "protocol": "classic-protocol",
                "server": "classic-server",
            },
            "state": "/workspace/state/server/review",
            "build_root": "/workspace/build/review",
            "components": {},
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.topology_summary.return_value = summary
            with mock.patch("builtins.print") as output:
                result = main(["topology", "show", "review"])

        self.assertEqual(result, 0)
        lines = [call.args[0] for call in output.call_args_list]
        self.assertIn("stack\tclassic", lines)
        self.assertIn("provider\tprotocol\tclassic-protocol", lines)
        self.assertIn("provider\tserver\tclassic-server", lines)

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
        workspace_type.return_value.command_maintenance.assert_not_called()

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
            "review",
            "review",
            "shared",
            ["server"],
            17300,
            state_mode=None,
        )
        output.assert_called_once_with("topology review: started at 127.0.0.1:17300")

    def test_topology_state_policy_options_are_mutually_exclusive(self) -> None:
        temporary = parser().parse_args(
            ["up", "--profile", "review", "--temporary-state"]
        )
        explicit_default = parser().parse_args(
            ["topology", "show", "review", "--default-state"]
        )
        self.assertEqual(temporary.state, "default")
        self.assertEqual(temporary.state_mode, "temporary")
        self.assertEqual(explicit_default.state, "default")
        self.assertEqual(explicit_default.state_mode, "default")
        with self.assertRaises(SystemExit):
            parser().parse_args(
                ["up", "--temporary-state", "--state", "shared"]
            )

    def test_client_only_temporary_state_selector_is_not_silently_ignored(
        self,
    ) -> None:
        error = WorkspaceError("temporary state requires the server service")
        cases = (
            (
                [
                    "topology",
                    "show",
                    "review",
                    "--temporary-state",
                    "--service",
                    "client",
                ],
                "topology_summary",
                mock.call(
                    "review", None, ["client"], state_mode="temporary"
                ),
            ),
            (
                [
                    "up",
                    "--name",
                    "client-only",
                    "--profile",
                    "review",
                    "--temporary-state",
                    "--service",
                    "client",
                ],
                "topology_up",
                mock.call(
                    "client-only",
                    "review",
                    None,
                    ["client"],
                    None,
                    state_mode="temporary",
                ),
            ),
        )
        for arguments, method_name, expected_call in cases:
            with self.subTest(command=arguments[0]):
                with mock.patch(
                    "atrinik_workspace.cli.Workspace"
                ) as workspace_type:
                    method = getattr(workspace_type.return_value, method_name)
                    method.side_effect = error
                    with mock.patch("builtins.print"):
                        self.assertEqual(main(arguments), 1)
                self.assertEqual(method.call_args, expected_call)

    def test_human_topology_state_policy_output_includes_stable_owner(self) -> None:
        generation = "a" * 64
        summary = {
            "profile": "review",
            "stack": "classic",
            "sound": {"mode": "source"},
            "services": ["server"],
            "dependencies": ["server"],
            "providers": {"server": "classic-server"},
            "state": None,
            "state_policy": {
                "mode": "temporary",
                "owner": {"kind": "topology-generation"},
                "lifecycle": "disposable",
                "path": None,
            },
            "build_root": "/workspace/build/review",
            "components": {},
        }
        status = {
            "endpoint": None,
            "state_policy": {
                "mode": "temporary",
                "owner": {
                    "topology": "review",
                    "kind": "topology-generation",
                    "generation": generation,
                },
                "lifecycle": "disposable",
                "path": "/workspace/topologies/review/temporary-states/abc",
            },
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace = workspace_type.return_value
            workspace.topology_summary.return_value = summary
            workspace.topology_up.return_value = status
            with mock.patch("builtins.print") as output:
                self.assertEqual(
                    main(
                        [
                            "topology",
                            "show",
                            "review",
                            "--temporary-state",
                            "--service",
                            "server",
                        ]
                    ),
                    0,
                )
                self.assertIn(
                    mock.call(
                        'state-policy\ttemporary\t'
                        '{"kind": "topology-generation"}\t'
                        "disposable\tallocated-on-start"
                    ),
                    output.call_args_list,
                )
                output.reset_mock()
                self.assertEqual(
                    main(
                        [
                            "up",
                            "--name",
                            "review",
                            "--profile",
                            "review",
                            "--temporary-state",
                            "--service",
                            "server",
                        ]
                    ),
                    0,
                )
                self.assertEqual(
                    output.call_args_list[-1],
                    mock.call(
                        'state-policy\ttemporary\t'
                        f'{{"generation": "{generation}", '
                        '"kind": "topology-generation", '
                        '"topology": "review"}\t'
                        "disposable\t/workspace/topologies/review/"
                        "temporary-states/abc"
                    ),
                )

    def test_temporary_state_start_retain_and_promotion_dispatch(self) -> None:
        status = {
            "supervisor": {"running": True},
            "endpoint": None,
            "state_policy": {
                "mode": "temporary",
                "owner": {
                    "kind": "topology-generation",
                    "topology": "review",
                    "generation": "a" * 64,
                },
                "lifecycle": "disposable",
                "path": "/workspace/topologies/review/temporary-states/abc",
            },
        }
        promoted = {
            "topology": "review",
            "name": "saved-review",
            "path": status["state_policy"]["path"],
        }
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace = workspace_type.return_value
            workspace.topology_up.return_value = status
            workspace.state_promote.return_value = promoted
            with mock.patch("builtins.print"):
                self.assertEqual(
                    main(
                        [
                            "up",
                            "--name",
                            "review",
                            "--profile",
                            "review",
                            "--temporary-state",
                            "--service",
                            "server",
                        ]
                    ),
                    0,
                )
                self.assertEqual(main(["down", "review", "--retain-state"]), 0)
                self.assertEqual(
                    main(["state", "promote", "review", "saved-review"]), 0
                )
        workspace.topology_up.assert_called_once_with(
            "review",
            "review",
            None,
            ["server"],
            None,
            state_mode="temporary",
        )
        workspace.topology_down.assert_called_once_with(
            "review", retain_state=True
        )
        workspace.state_promote.assert_called_once_with(
            "review", "saved-review"
        )

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
            "review", "default", ["server"], state_mode=None
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

    def test_ps_reports_current_and_historical_retained_leases(self) -> None:
        common = {
            "profile": "review",
            "endpoint": None,
            "services": {},
            "supervisor": {"running": True, "pid": 1234},
        }
        statuses = [
            {
                **common,
                "name": "current",
                "observation": {
                    "process_tree_lease": "retained",
                    "runtime_bundle_lease": "retained",
                    "runtime_generation": "a" * 64,
                    "safe_action": "run ./atrinik down current",
                },
            },
            {
                **common,
                "name": "historical",
                "observation": {
                    "process_tree_lease": "retained",
                    "repository_layout_lease_owner": "supervisor 1234",
                    "safe_action": "run ./atrinik down historical",
                },
            },
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.topology_statuses.return_value = statuses
            with mock.patch("builtins.print") as output:
                result = main(["ps"])

        self.assertEqual(result, 0)
        rendered = "\n".join(
            str(call.args[0]) if call.args else "" for call in output.call_args_list
        )
        self.assertIn(f"runtime-generation\tretained\t{'a' * 64}", rendered)
        self.assertIn(
            "repository-layout-lease\tretained\tsupervisor 1234", rendered
        )

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

    def test_scenario_create_prints_automatic_login_handoff(self) -> None:
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
        self.assertNotIn("./atrinik scenario credentials issue-42", rendered)
        self.assertIn(
            "the supervised client logs in with the scenario automatically",
            rendered,
        )
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

    def test_scenario_list_human_output_identifies_inert_records(self) -> None:
        summaries = [
            {
                "name": "current",
                "profile": "default",
                "preset": "basic-player",
                "state": "scenario-current",
            },
            {
                "name": "historical",
                "path": "/workspace/scenarios/historical",
                "inert": True,
                "inert_reason": "profile_unresolvable",
            },
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scenario_list.return_value = summaries
            with mock.patch("builtins.print") as output:
                result = main(["scenario", "list"])

        self.assertEqual(result, 0)
        self.assertEqual(
            [call.args[0] for call in output.call_args_list],
            [
                "current\tdefault\tbasic-player\tscenario-current",
                "historical\tinert\tprofile_unresolvable\t"
                "/workspace/scenarios/historical",
            ],
        )

    def test_scenario_list_json_preserves_valid_and_inert_records(self) -> None:
        summaries = [
            {
                "name": "current",
                "profile": "default",
                "preset": "basic-player",
                "state": "scenario-current",
            },
            {
                "name": "historical",
                "path": "/workspace/scenarios/historical",
                "inert": True,
                "inert_reason": "profile_unresolvable",
            },
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scenario_list.return_value = summaries
            with mock.patch("builtins.print") as output:
                result = main(["scenario", "list", "--json"])

        self.assertEqual(result, 0)
        self.assertEqual(json.loads(output.call_args.args[0]), summaries)

    def test_scenario_list_human_output_escapes_inert_control_characters(self) -> None:
        summaries = [
            {
                "name": "unsafe\nname",
                "path": "/workspace/scenarios/unsafe\tname",
                "inert": True,
                "inert_reason": "invalid_record",
            }
        ]
        with mock.patch("atrinik_workspace.cli.Workspace") as workspace_type:
            workspace_type.return_value.scenario_list.return_value = summaries
            with mock.patch("builtins.print") as output:
                result = main(["scenario", "list"])

        self.assertEqual(result, 0)
        output.assert_called_once_with(
            "unsafe\\nname\tinert\tinvalid_record\t"
            "/workspace/scenarios/unsafe\\tname"
        )


if __name__ == "__main__":
    unittest.main()
