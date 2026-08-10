from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError, atomic_json
from atrinik_workspace.workspace import Workspace


ROOT = Path(__file__).resolve().parents[1]


class CohortWorkspaceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper with spaces"
        self.wrapper.mkdir()
        shutil.copy2(ROOT / "components.json", self.wrapper / "components.json")
        self.environment = mock.patch.dict(
            os.environ,
            {"ATRINIK_WORKSPACE_DIR": str(self.root / "workspace with spaces")},
        )
        self.environment.start()
        self.workspace = Workspace(self.wrapper)

    def tearDown(self) -> None:
        self.environment.stop()
        self.temporary.cleanup()

    def make_classic_build_profile(self, *, include_worker: bool = True) -> None:
        profile = self.workspace._load_profile("classic", require_file=False)
        stack = self.workspace.manifest.stack("classic")
        build_targets = {
            role
            for role in (
                "content",
                "protocol",
                "libatrinik",
                "client",
                "server",
                "metaserver-worker",
            )
            if role in stack.providers and stack.providers[role].build != "none"
        }
        roles = self.workspace._dependency_roles(profile, build_targets)
        for role in roles:
            component = stack.providers[role]
            if not include_worker and role == "metaserver-worker":
                continue
            root = self.workspace._selector_root(profile, component)
            source = root / component.source if component.source != "." else root
            source.mkdir(parents=True, exist_ok=True)

    def test_plain_init_selects_only_default_cohort(self) -> None:
        with mock.patch.object(
            self.workspace, "_ensure_repository", return_value=self.wrapper
        ) as ensure:
            self.workspace.initialize(None, jobs=3)

        selected = {call.args[0].name for call in ensure.call_args_list}
        self.assertEqual(
            selected,
            {component.name for component in self.workspace.manifest.cohort("default")},
        )
        self.assertNotIn("classic", selected)
        self.assertNotIn("content-1x", selected)
        self.assertNotIn("playtester", selected)
        self.assertNotIn("tools", selected)

    def test_with_classic_is_additive_and_complete(self) -> None:
        with mock.patch.object(
            self.workspace, "_ensure_repository", return_value=self.wrapper
        ) as ensure:
            self.workspace.initialize(None, jobs=2, include_classic=True)

        selected = {call.args[0].name for call in ensure.call_args_list}
        expected = {
            component.name
            for cohort in ("default", "classic")
            for component in self.workspace.manifest.cohort(cohort)
        }
        self.assertEqual(selected, expected)
        self.assertIn("content-1x", selected)
        self.assertIn("playtester", selected)
        self.assertIn("tools", selected)

    def test_with_classic_leaves_existing_default_checkouts_byte_identical(self) -> None:
        seed = self.root / "seed"
        seed.mkdir()
        subprocess.run(["git", "init", "-q", "-b", "main"], cwd=seed, check=True)
        subprocess.run(
            ["git", "config", "user.name", "Cohort Fixture"], cwd=seed, check=True
        )
        subprocess.run(
            ["git", "config", "user.email", "fixture@example.invalid"],
            cwd=seed,
            check=True,
        )
        (seed / "payload.bin").write_bytes(b"default checkout\x00fixture\n")
        subprocess.run(["git", "add", "payload.bin"], cwd=seed, check=True)
        subprocess.run(
            ["git", "commit", "-q", "-m", "fixture"], cwd=seed, check=True
        )
        subprocess.run(["git", "branch", "1.x"], cwd=seed, check=True)
        subprocess.run(
            ["git", "branch", "history/client/pr-48"], cwd=seed, check=True
        )

        defaults = self.workspace.manifest.cohort("default")
        for component in defaults:
            subprocess.run(
                [
                    "git",
                    "clone",
                    "-q",
                    "--branch",
                    component.branch,
                    "--single-branch",
                    str(seed),
                    str(self.wrapper / component.path),
                ],
                check=True,
            )

        def snapshot(path: Path) -> dict[str, tuple[str, int, bytes | str]]:
            result: dict[str, tuple[str, int, bytes | str]] = {}
            for entry in sorted(path.rglob("*")):
                relative = str(entry.relative_to(path))
                mode = entry.lstat().st_mode
                if entry.is_symlink():
                    result[relative] = ("link", mode, os.readlink(entry))
                elif entry.is_file():
                    result[relative] = ("file", mode, entry.read_bytes())
                elif entry.is_dir():
                    result[relative] = ("directory", mode, b"")
            return result

        before = {
            component.name: snapshot(self.wrapper / component.path)
            for component in defaults
        }
        heads = {
            component.name: subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=self.wrapper / component.path,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            for component in defaults
        }
        with (
            mock.patch.object(
                self.workspace, "_component_clone_url", return_value=str(seed)
            ),
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ),
        ):
            self.workspace.initialize(None, jobs=4, include_classic=True)

        classic_history = subprocess.run(
            [
                "git",
                "rev-parse",
                "--verify",
                "refs/remotes/origin/history/client/pr-48",
            ],
            cwd=self.wrapper / "classic",
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(classic_history.returncode, 0)

        for component in defaults:
            checkout = self.wrapper / component.path
            self.assertEqual(snapshot(checkout), before[component.name])
            head = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=checkout,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            self.assertEqual(head, heads[component.name])

    def test_builtin_topology_summaries_use_exact_stack_coordinates(self) -> None:
        def resolve_profile(
            profile_name: str,
            component_names: set[str] | None = None,
            *,
            trace: bool = True,
        ) -> dict[str, Path]:
            stack = self.workspace.manifest.stack(profile_name)
            selected = component_names or {
                component.name for component in stack.components
            }
            return {
                component.name: self.wrapper / component.checkout
                for component in stack.components
                if component.name in selected
            }

        with (
            mock.patch.object(
                self.workspace, "resolve_profile", side_effect=resolve_profile
            ),
            mock.patch.object(
                self.workspace,
                "_state_location",
                return_value=self.root / "state",
            ),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="a" * 40
            ),
            mock.patch("atrinik_workspace.workspace._is_clean", return_value=True),
        ):
            default = self.workspace.topology_summary(
                "default", "default", ["server", "client"]
            )
            classic = self.workspace.topology_summary(
                "classic", "default", ["server", "client"]
            )

        self.assertEqual(
            {
                role: default["providers"][role]
                for role in ("server", "client", "protocol", "content")
            },
            {
                "server": "server",
                "client": "client",
                "protocol": "protocol",
                "content": "content",
            },
        )
        self.assertEqual(
            {
                role: classic["providers"][role]
                for role in (
                    "server",
                    "client",
                    "protocol",
                    "libatrinik",
                    "content",
                )
            },
            {
                "server": "classic-server",
                "client": "classic-client",
                "protocol": "classic-protocol",
                "libatrinik": "classic-libatrinik",
                "content": "content-1x",
            },
        )
        self.assertEqual(
            default["components"]["content"]["repository"], "atrinik/content"
        )
        self.assertEqual(default["components"]["content"]["branch"], "main")
        self.assertEqual(
            classic["components"]["content-1x"]["repository"],
            "atrinik/content",
        )
        self.assertEqual(classic["components"]["content-1x"]["branch"], "1.x")

    def test_concurrent_first_initialization_claims_workspace_once(self) -> None:
        workspaces = [Workspace(self.wrapper) for _ in range(8)]

        with mock.patch.object(
            Workspace, "_ensure_repository", return_value=self.wrapper
        ):
            with ThreadPoolExecutor(max_workers=len(workspaces)) as executor:
                futures = [
                    executor.submit(workspace.initialize, None, 2)
                    for workspace in workspaces
                ]
                for future in futures:
                    future.result(timeout=10)

        self.assertTrue(self.workspace.paths.marker.is_file())
        self.assertTrue(self.workspace.paths.profiles.is_dir())

    def test_initialization_preflights_all_destinations_before_cloning(self) -> None:
        (self.wrapper / "client").mkdir()
        with (
            mock.patch.object(
                self.workspace,
                "_validate_primary_checkout",
                side_effect=WorkspaceError("classic checkout needs migration"),
            ),
            mock.patch.object(self.workspace, "_ensure_repository") as ensure,
        ):
            with self.assertRaisesRegex(WorkspaceError, "needs migration"):
                self.workspace.initialize(None, jobs=4)

        ensure.assert_not_called()

    def test_component_history_distinguishes_classic_from_fresh_identity(self) -> None:
        path = self.wrapper / "client"
        with mock.patch(
            "atrinik_workspace.workspace.classic_lineage", return_value=True
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "run ./atrinik migrate repositories --dry-run"
            ):
                self.workspace._validate_checkout_lineage(
                    self.workspace.manifest.by_checkout["client"], path
                )

        with mock.patch(
            "atrinik_workspace.workspace.classic_lineage", return_value=False
        ) as lineage:
            self.workspace._validate_checkout_lineage(
                self.workspace.manifest.by_checkout["classic"], path
            )
        lineage.assert_not_called()

    def test_explicit_component_init_remains_exact(self) -> None:
        with mock.patch.object(
            self.workspace, "_ensure_repository", return_value=self.wrapper
        ) as ensure:
            self.workspace.initialize(
                ["classic-server", "classic-client", "classic-protocol"], jobs=1
            )

        ensure.assert_called_once()
        self.assertEqual(ensure.call_args.args[0].name, "classic")

    def test_classic_profile_selectors_are_checkout_wide(self) -> None:
        self.workspace.create_profile("classic-review", "classic")
        with mock.patch.object(
            self.workspace, "_validate_selected_checkout", return_value="origin"
        ):
            self.workspace.set_profile(
                "classic-review", "classic-server", "worktree", "review"
            )

        profile = self.workspace._load_profile("classic-review", require_file=True)
        expected = {"kind": "worktree", "value": "review"}
        for component in self.workspace.manifest.stack("classic").components:
            if component.checkout_name == "classic":
                self.assertEqual(profile["components"][component.name], expected)

        profile["components"]["classic-server"] = {
            "kind": "primary",
            "value": "",
        }
        atomic_json(
            self.workspace.paths.profiles / "classic-review.json", profile
        )
        with self.assertRaisesRegex(
            WorkspaceError, "components in one checkout must match"
        ):
            self.workspace._load_profile("classic-review", require_file=True)

    def test_classic_profile_resolves_logical_source_directories(self) -> None:
        checkout = self.wrapper / "classic"
        for source in ("client", "server", "protocol", "libatrinik", "editor"):
            (checkout / source).mkdir(parents=True, exist_ok=True)
        names = {
            component.name
            for component in self.workspace.manifest.stack("classic").components
            if component.checkout_name == "classic"
        }
        with mock.patch.object(
            self.workspace, "_validate_selected_checkout", return_value="origin"
        ):
            resolved = self.workspace.resolve_profile("classic", names)

        self.assertEqual(
            resolved,
            {
                name: checkout / self.workspace.manifest.by_name[name].source
                for name in names
            },
        )

    def test_complete_classic_commands_share_common_build_root(self) -> None:
        self.make_classic_build_profile()
        expected_roles = {
            "client",
            "server",
            "protocol",
            "libatrinik",
            "content",
            "sound",
            "resources",
            "metaserver-worker",
        }

        roots: list[Path] = []

        def build_resolved(
            target: str,
            profile_name: str,
            tests: bool,
            targets: list[str],
            selected: dict[str, Path],
        ) -> Path:
            self.assertEqual(set(selected), expected_roles)
            root = self.workspace.paths.builds / "profiles" / (
                f"{profile_name}-{self.workspace._profile_build_key(profile_name, selected)}"
            )
            roots.append(root)
            return root

        with (
            mock.patch.object(
                self.workspace, "_validate_selected_checkout", return_value="origin"
            ) as validate,
            mock.patch.object(
                self.workspace, "_build_resolved", side_effect=build_resolved
            ),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="a" * 40
            ) as git,
            mock.patch(
                "atrinik_workspace.workspace._is_clean", return_value=True
            ) as clean,
        ):
            roots.extend(
                [
                    self.workspace.build("client", "classic", tests=False),
                    self.workspace.build("server", "classic", tests=False),
                    self.workspace.build("all", "classic", tests=False),
                    Path(
                        self.workspace.topology_summary(
                            "classic", "default", ["server", "client"]
                        )["build_root"]
                    ),
                ]
            )

        self.assertEqual(len(set(roots)), 1)
        # Each of four resolutions validates the five selected physical
        # checkouts once; four Classic logical providers share one checkout.
        self.assertEqual(validate.call_count, 20)
        expected_checkouts = {
            "classic",
            "content-1x",
            "sound",
            "resources",
            "metaserver-worker",
        }
        validated = [
            call.args[0].checkout_name for call in validate.call_args_list
        ]
        for offset in range(0, len(validated), len(expected_checkouts)):
            self.assertEqual(
                set(validated[offset : offset + len(expected_checkouts)]),
                expected_checkouts,
            )
        self.assertEqual(git.call_count, 5)
        self.assertEqual(clean.call_count, 5)

    def test_partial_classic_profile_keeps_requested_dependency_closure(self) -> None:
        self.make_classic_build_profile(include_worker=False)

        with mock.patch.object(
            self.workspace, "_validate_selected_checkout", return_value="origin"
        ):
            selected = self.workspace._resolve_build_profile("classic", {"client"})

        self.assertEqual(
            set(selected), {"client", "sound", "libatrinik", "protocol"}
        )

    def test_malformed_present_common_checkout_fails_closed(self) -> None:
        self.make_classic_build_profile(include_worker=False)
        worker = self.wrapper / "metaserver-worker"
        worker.write_text("not a checkout\n", encoding="utf-8")

        with mock.patch.object(
            self.workspace, "_validate_selected_checkout", return_value="origin"
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "component checkout is not a directory"
            ):
                self.workspace._resolve_build_profile("classic", {"client"})

    def test_complete_classic_worktree_profile_uses_common_build_root(self) -> None:
        self.make_classic_build_profile()
        self.workspace.create_profile("classic-review", "classic")
        worktree = self.workspace.paths.worktrees / "classic" / "review"
        for source in ("client", "server", "protocol", "libatrinik", "editor"):
            (worktree / source).mkdir(parents=True, exist_ok=True)
        with mock.patch.object(
            self.workspace, "_validate_selected_checkout", return_value="origin"
        ):
            self.workspace.set_profile(
                "classic-review", "classic", "worktree", "review"
            )
            client = self.workspace._resolve_build_profile(
                "classic-review", {"client"}
            )
            server = self.workspace._resolve_build_profile(
                "classic-review", {"server"}
            )

        self.assertEqual(set(client), set(server))
        self.assertEqual(
            self.workspace._profile_build_key("classic-review", client),
            self.workspace._profile_build_key("classic-review", server),
        )
        for role in ("client", "server", "protocol", "libatrinik"):
            self.assertEqual(client[role], worktree / role)

    def test_classic_component_source_rejects_symlinked_module(self) -> None:
        checkout = self.wrapper / "classic"
        (checkout / "client").mkdir(parents=True)
        (checkout / "server").symlink_to("client", target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "not a normal directory"):
            self.workspace._component_source(
                self.workspace.manifest.by_name["classic-server"], checkout
            )

    def test_content_1x_clone_is_branch_qualified(self) -> None:
        component = self.workspace.manifest.by_name["content-1x"]
        with (
            mock.patch("atrinik_workspace.workspace.run") as run,
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ),
        ):
            destination = self.workspace._ensure_repository(component)

        self.assertEqual(destination, self.wrapper / "content-1x")
        clone = run.call_args.args[0]
        self.assertEqual(clone[:5], ["git", "clone", "--branch", "1.x", "--single-branch"])

    def test_default_sync_skips_uninitialized_components_without_cloning(self) -> None:
        with mock.patch.object(self.workspace, "_ensure_repository") as ensure:
            self.workspace.sync(None, "none")

        ensure.assert_not_called()
        self.assertFalse((self.wrapper / "client").exists())

    def test_explicit_sync_with_classic_skips_missing_implicit_members(self) -> None:
        (self.wrapper / "client").mkdir()
        with (
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ),
            mock.patch.object(
                self.workspace, "_canonical_remote", return_value="origin"
            ),
            mock.patch("atrinik_workspace.workspace._is_clean", return_value=True),
            mock.patch("atrinik_workspace.workspace.git") as git,
        ):
            self.workspace.sync(["client"], "none", include_classic=True)

        self.assertEqual(git.call_count, 2)

    def test_duplicate_repository_profile_paths_require_correct_primary(self) -> None:
        selected = self.wrapper / "external-content-review"
        selected.mkdir()
        content_1x = self.workspace.manifest.by_name["content-1x"]
        primary = self.wrapper / "content-1x"
        primary.mkdir()
        with (
            mock.patch.object(
                self.workspace, "_validate_checkout", return_value="origin"
            ),
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="main"
            ),
            mock.patch.object(
                self.workspace,
                "_git_common_directory",
                side_effect=[self.wrapper / ".git-main", self.wrapper / ".git-1x"],
            ),
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "cannot be proven to belong to content-1x@1.x"
            ):
                self.workspace._validate_selected_checkout(
                    content_1x, selected, "path"
                )

        with (
            mock.patch.object(
                self.workspace, "_validate_checkout", return_value="origin"
            ),
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="feature/maps"
            ),
            mock.patch.object(
                self.workspace,
                "_git_common_directory",
                return_value=self.wrapper / ".git-1x",
            ),
        ):
            self.assertEqual(
                self.workspace._validate_selected_checkout(
                    content_1x, selected, "path"
                ),
                "origin",
            )

    def test_migrated_content_worktree_requires_old_managed_lineage(self) -> None:
        selected = self.workspace.paths.worktrees / "content" / "maps-review"
        selected.mkdir(parents=True)
        content_1x = self.workspace.manifest.by_name["content-1x"]
        content = self.workspace.manifest.by_name["content"]
        primary = self.wrapper / "content"
        primary.mkdir()
        common = self.wrapper / ".git-content"
        with (
            mock.patch.object(
                self.workspace, "_validate_checkout", return_value="origin"
            ),
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ) as validate_primary,
            mock.patch.object(
                self.workspace, "_git_common_directory", return_value=common
            ),
        ):
            self.assertEqual(
                self.workspace._validate_selected_checkout(
                    content_1x, selected, "migrated-worktree"
                ),
                "origin",
            )
        validate_primary.assert_called_once_with(
            self.workspace.manifest.by_checkout["content"], primary, trace=True
        )

        outside = self.wrapper / "external-content-review"
        outside.mkdir()
        with mock.patch.object(
            self.workspace, "_validate_checkout", return_value="origin"
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "must remain directly below"
            ):
                self.workspace._validate_selected_checkout(
                    content_1x, outside, "migrated-worktree"
                )

    def test_migration_only_selector_is_restricted_when_loading_profiles(self) -> None:
        self.workspace.paths.ensure()
        profile = self.workspace._load_profile("classic", require_file=False)
        profile["name"] = "migrated-content"
        profile["components"]["content-1x"] = {
            "kind": "migrated-worktree",
            "value": str(
                (self.workspace.paths.worktrees / "content" / "maps").resolve()
            ),
        }
        path = self.workspace.paths.profiles / "migrated-content.json"
        atomic_json(path, profile)
        loaded = self.workspace._load_profile(
            "migrated-content", require_file=True
        )
        self.assertEqual(
            loaded["components"]["content-1x"]["kind"],
            "migrated-worktree",
        )

        loaded["components"]["classic-server"] = {
            "kind": "migrated-worktree",
            "value": str(
                (self.workspace.paths.worktrees / "content" / "maps").resolve()
            ),
        }
        atomic_json(path, loaded)
        with self.assertRaisesRegex(
            WorkspaceError, "invalid migrated content worktree selector"
        ):
            self.workspace._load_profile("migrated-content", require_file=True)

    def test_content_sync_protects_profile_owned_migrated_worktree(self) -> None:
        self.workspace.paths.ensure()
        selected = self.workspace.paths.worktrees / "content" / "classic-maps"
        selected.mkdir(parents=True)
        profile = self.workspace._load_profile("classic", require_file=False)
        profile["name"] = "classic-maps"
        profile["components"]["content-1x"] = {
            "kind": "migrated-worktree",
            "value": str(selected.resolve()),
        }
        atomic_json(self.workspace.paths.profiles / "classic-maps.json", profile)

        content = self.workspace.manifest.by_name["content"]
        primary = self.wrapper / content.checkout
        primary.mkdir()
        with (
            mock.patch.object(
                self.workspace, "_validate_selected_checkout", return_value="origin"
            ),
            mock.patch.object(
                self.workspace, "_validate_primary_checkout", return_value="origin"
            ),
            mock.patch.object(
                self.workspace, "_canonical_remote", return_value="origin"
            ),
            mock.patch("atrinik_workspace.workspace._is_clean", return_value=True),
            mock.patch.object(
                self.workspace,
                "_component_worktrees",
                return_value=([], [selected.resolve()]),
            ) as worktrees,
            mock.patch("atrinik_workspace.workspace.git"),
        ):
            self.workspace.sync(["content"], "merge")

        worktrees.assert_called_once_with(primary, {selected.resolve()})

    def test_explicit_sync_of_missing_component_fails_closed(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "run ./atrinik init classic"):
            self.workspace.sync(["classic-server"], "none")

    def test_status_reports_membership_and_optional_state_offline(self) -> None:
        with mock.patch("atrinik_workspace.workspace.git") as git:
            rows = self.workspace.repository_status()

        git.assert_not_called()
        by_name = {row["component"]: row for row in rows}
        self.assertFalse(by_name["client"]["optional"])
        self.assertEqual(by_name["client"]["cohorts"], ["default"])
        self.assertTrue(by_name["classic"]["optional"])
        self.assertEqual(by_name["classic"]["cohorts"], ["classic"])
        self.assertEqual(
            by_name["classic"]["modules"],
            [
                "classic-client",
                "classic-server",
                "classic-protocol",
                "classic-libatrinik",
                "classic-editor",
            ],
        )
        self.assertEqual(by_name["content-1x"]["default_branch"], "1.x")

    def test_builtin_profiles_retain_coherent_stack_identity(self) -> None:
        default = self.workspace._load_profile("default", require_file=False)
        classic = self.workspace._load_profile("classic", require_file=False)

        self.assertEqual(default["stack"], "default")
        self.assertEqual(classic["stack"], "classic")
        self.assertIn("server", default["components"])
        self.assertNotIn("classic-server", default["components"])
        self.assertIn("classic-server", classic["components"])
        self.assertNotIn("server", classic["components"])
        self.assertEqual(
            self.workspace.manifest.provider("classic", "content").name,
            "content-1x",
        )

    def test_profile_override_cannot_cross_stacks(self) -> None:
        self.workspace.create_profile("classic-review", "classic")

        with self.assertRaisesRegex(WorkspaceError, "not part of classic stack"):
            self.workspace.set_profile(
                "classic-review", "renderer", "primary"
            )

    def test_fresh_seed_runtime_fails_before_resolving_checkouts(self) -> None:
        with mock.patch.object(self.workspace, "resolve_profile") as resolve:
            with self.assertRaisesRegex(
                WorkspaceError, "no wrapper build/runtime contract yet"
            ):
                self.workspace.build("server", "default", tests=False)

        resolve.assert_not_called()

    def test_missing_classic_components_are_reported_not_initialized(self) -> None:
        summary = self.workspace.profile_summary("classic")

        self.assertEqual(summary["stack"], "classic")
        self.assertTrue(summary["components"])
        self.assertTrue(
            all(not row["initialized"] for row in summary["components"])
        )


if __name__ == "__main__":
    unittest.main()
