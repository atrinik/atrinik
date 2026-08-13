from __future__ import annotations

import json
from pathlib import Path
import unittest
from unittest import mock

from atrinik_workspace.cli import _human_bytes, main, parser
from atrinik_workspace.model import WorkspaceError


class ParserTests(unittest.TestCase):
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
