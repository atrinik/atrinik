from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cleanup import Cleanup, _base_item
from atrinik_workspace.model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
    WorkspaceError,
    atomic_json,
    managed_directory,
    managed_remove as real_managed_remove,
)
from atrinik_workspace.workspace import Workspace


def command(*arguments: str, cwd: Path) -> str:
    return subprocess.run(
        list(arguments), cwd=cwd, check=True, capture_output=True, text=True
    ).stdout.strip()


class CleanupTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        (self.wrapper / "components.json").write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "components": [
                        {
                            "name": name,
                            "repository": f"atrinik/{name}",
                            "branch": "main",
                            "build": build,
                        }
                        for name, build in (
                            ("client", "client"),
                            ("server", "server"),
                            ("protocol", "protocol"),
                            ("libatrinik", "library"),
                            ("content", "content"),
                            ("sound", "assets"),
                            ("resources", "assets"),
                        )
                    ],
                }
            ),
            encoding="utf-8",
        )
        (self.wrapper / ".gitignore").write_text("ignored/\n", encoding="utf-8")
        command("git", "init", "-b", "main", cwd=self.wrapper)
        command("git", "config", "user.name", "Tests", cwd=self.wrapper)
        command("git", "config", "user.email", "tests@example.invalid", cwd=self.wrapper)
        command("git", "add", ".", cwd=self.wrapper)
        command("git", "commit", "-m", "feat: seed", cwd=self.wrapper)
        command(
            "git",
            "remote",
            "add",
            "origin",
            "https://github.com/atrinik/atrinik.git",
            cwd=self.wrapper,
        )
        self.workspace_root = self.root / "workspace"
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.workspace_root)}
        )
        self.environment.start()
        self.workspace = Workspace(self.wrapper)
        self.workspace.paths.ensure()
        self.old = datetime.now(timezone.utc) - timedelta(days=30)

    def tearDown(self) -> None:
        self.environment.stop()
        self.temporary.cleanup()

    def make_wrapper_worktree(self, label: str = "review") -> Path:
        path = self.workspace.paths.worktrees / "atrinik" / label
        path.parent.mkdir(parents=True, exist_ok=True)
        command(
            "git",
            "worktree",
            "add",
            "-b",
            f"feat/{label}",
            str(path),
            "main",
            cwd=self.wrapper,
        )
        (path / "ignored").mkdir()
        (path / "ignored" / "output.o").write_bytes(b"x" * 4096)
        return path

    def make_component_worktree(self, label: str = "component-review") -> Path:
        primary = self.wrapper / "client"
        primary.mkdir()
        command("git", "init", "-b", "main", cwd=primary)
        command("git", "config", "user.name", "Tests", cwd=primary)
        command("git", "config", "user.email", "tests@example.invalid", cwd=primary)
        (primary / "README").write_text("client\n", encoding="utf-8")
        command("git", "add", ".", cwd=primary)
        command("git", "commit", "-m", "feat: seed", cwd=primary)
        command(
            "git",
            "remote",
            "add",
            "origin",
            "https://github.com/atrinik/client.git",
            cwd=primary,
        )
        path = self.workspace.paths.worktrees / "client" / label
        path.parent.mkdir(parents=True, exist_ok=True)
        command(
            "git",
            "worktree",
            "add",
            "-b",
            f"feat/{label}",
            str(path),
            "main",
            cwd=primary,
        )
        return path

    def merged_pull(
        self, head: str, *, state: str = "closed", base: str = "main"
    ) -> list[dict[str, object]]:
        return [
            {
                "number": 42,
                "state": state,
                "html_url": "https://github.com/atrinik/atrinik/pull/42",
                "merged_at": self.old.isoformat() if state == "closed" else None,
                "head": {"sha": head},
                "base": {"ref": base},
            }
        ]

    def plan(self, scopes: list[str], older: int = 7) -> dict[str, object]:
        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(head)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            return self.workspace.cleanup(scopes, older, [], False)

    def test_dry_run_finds_exact_merged_head_and_preserves_ignored_output(self) -> None:
        worktree = self.make_wrapper_worktree()
        report = self.plan(["worktrees"])
        item = next(row for row in report["items"] if row["path"] == str(worktree))

        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["merged_pr_head"])
        self.assertGreater(item["ignored_bytes"], 0)
        self.assertGreater(item["allocated_bytes"], 0)
        primary = next(
            row for row in report["items"] if row["path"] == str(self.wrapper)
        )
        self.assertEqual(primary["allocated_bytes"], 0)
        self.assertTrue(worktree.is_dir())
        self.assertEqual(report["mode"], "dry-run")

    def test_non_exact_or_unavailable_pull_evidence_fails_closed(self) -> None:
        worktree = self.make_wrapper_worktree()
        head = command("git", "rev-parse", "HEAD", cwd=worktree)
        closed_unmerged = self.merged_pull(head)
        closed_unmerged[0]["merged_at"] = None
        cases = (
            (self.merged_pull(head, state="open"), "open_pull_request"),
            (self.merged_pull(head, base="release"), "wrong_base_branch"),
            (self.merged_pull("f" * 40), "pr_head_mismatch"),
            (closed_unmerged, "closed_unmerged_pr"),
            (self.merged_pull(head) * 2, "ambiguous_pull_requests"),
        )
        for pulls, reason in cases:
            with self.subTest(reason=reason), mock.patch.object(
                Cleanup, "_github_pulls", return_value=pulls
            ):
                report = self.workspace.cleanup(["worktrees"], 7, [], False)
                item = next(row for row in report["items"] if row["path"] == str(worktree))
                self.assertEqual(item["disposition"], "protected")
                self.assertEqual(item["reasons"], [reason])

        with mock.patch.object(
            Cleanup, "_github_pulls", side_effect=WorkspaceError("offline")
        ):
            report = self.workspace.cleanup(["worktrees"], 7, [], False)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(item["disposition"], "protected")
        self.assertEqual(item["reasons"], ["github_unavailable"])
        self.assertEqual(item["github_error"], "offline")

    def test_detached_locked_and_in_progress_worktrees_are_protected(self) -> None:
        detached = self.workspace.paths.worktrees / "atrinik" / "detached"
        detached.parent.mkdir(parents=True, exist_ok=True)
        command(
            "git",
            "worktree",
            "add",
            "--detach",
            str(detached),
            "main",
            cwd=self.wrapper,
        )
        locked = self.make_wrapper_worktree("locked")
        command("git", "worktree", "lock", str(locked), cwd=self.wrapper)
        in_progress = self.make_wrapper_worktree("in-progress")
        merge_head = Path(
            command("git", "rev-parse", "--git-path", "MERGE_HEAD", cwd=in_progress)
        )
        merge_head.write_text(
            command("git", "rev-parse", "HEAD", cwd=in_progress) + "\n",
            encoding="utf-8",
        )

        with mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        by_path = {row["path"]: row for row in report["items"]}
        self.assertIn("detached_head", by_path[str(detached)]["reasons"])
        self.assertIn("locked_worktree", by_path[str(locked)]["reasons"])
        self.assertIn(
            "git_operation_in_progress", by_path[str(in_progress)]["reasons"]
        )
        pulls.assert_not_called()

    def test_symlinked_allowlist_namespace_cannot_authorize_external_worktree(
        self,
    ) -> None:
        namespace = self.workspace.paths.worktrees / "atrinik"
        external = self.root / "external-worktrees"
        external.mkdir()
        namespace.symlink_to(external, target_is_directory=True)
        worktree = external / "review"
        command(
            "git",
            "worktree",
            "add",
            "-b",
            "feat/external-review",
            str(worktree),
            "main",
            cwd=self.wrapper,
        )

        with mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("external_path", item["reasons"])
        pulls.assert_not_called()

    def test_dirty_and_profile_selected_worktrees_do_not_query_github(self) -> None:
        dirty = self.make_wrapper_worktree("dirty")
        (dirty / "untracked").write_text("local\n", encoding="utf-8")
        with mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, [], False)
        item = next(row for row in report["items"] if row["path"] == str(dirty))
        self.assertIn("dirty_worktree", item["reasons"])
        pulls.assert_not_called()

    def test_all_profile_selector_kinds_and_retained_scenarios_protect_worktrees(self) -> None:
        worktree = self.make_component_worktree()
        self.workspace.create_profile("selected")
        self.workspace.set_profile("selected", "client", "worktree", worktree.name)
        with mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, ["client"], False)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("profile_reference", item["reasons"])
        self.assertEqual(item["references"]["profiles"], ["selected"])
        pulls.assert_not_called()

        profile_path = self.workspace.paths.profiles / "selected.json"
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        profile["components"]["client"] = {"kind": "path", "value": str(worktree)}
        profile_path.write_text(json.dumps(profile), encoding="utf-8")
        report = self.workspace.cleanup(["worktrees"], 0, ["client"], False)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("profile_reference", item["reasons"])

        profile_path.unlink()
        scenario = self.workspace.paths.scenarios / "retained"
        scenario.mkdir()
        atomic_json(
            scenario / MANAGED_MARKER,
            {"schema_version": SCHEMA_VERSION, "purpose": "test-scenario"},
        )
        atomic_json(
            scenario / "scenario.json",
            {
                "name": "retained",
                "resolved": {
                    "client": {
                        "checkout_path": str(worktree),
                        "checkout": "client",
                        "repository": "atrinik/client",
                        "branch": "main",
                        "source": ".",
                        "head": command("git", "rev-parse", "HEAD", cwd=worktree),
                    }
                },
            },
        )
        report = self.workspace.cleanup(["worktrees"], 0, ["client"], False)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("scenario_reference", item["reasons"])

    def test_malformed_scenario_fails_closed_without_querying_github(self) -> None:
        worktree = self.make_wrapper_worktree()
        scenario = self.workspace.paths.scenarios / "malformed"
        scenario.mkdir()
        atomic_json(
            scenario / MANAGED_MARKER,
            {"schema_version": SCHEMA_VERSION, "purpose": "test-scenario"},
        )
        atomic_json(scenario / "scenario.json", [])

        with mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("scenario_inventory_error", item["reasons"])
        self.assertIn("scenario_inventory_error", report["inventory_errors"])
        pulls.assert_not_called()

    def test_migration_original_and_destination_paths_are_protected(self) -> None:
        worktree = self.make_component_worktree()
        migrations = self.workspace.paths.workspace / "migrations"
        migrations.mkdir()
        atomic_json(
            migrations / "repositories.json",
            {
                "sources": [{"source": str(worktree)}],
                "worktree_migrations": [{"path": str(worktree), "destination": str(worktree)}],
                "composite_worktrees": [{"destination": str(worktree)}],
            },
        )
        report = self.workspace.cleanup(["worktrees"], 0, ["client"], False)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("migration_reference", item["reasons"])
        self.assertGreaterEqual(len(item["references"]["migration"]), 3)

    def test_apply_revalidates_then_removes_without_deleting_branch(self) -> None:
        worktree = self.make_wrapper_worktree()
        branch = command("git", "branch", "--show-current", cwd=worktree)

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(head)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees"], 7, [], True)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(item["disposition"], "removed")
        self.assertFalse(worktree.exists())
        self.assertEqual(
            command("git", "branch", "--list", branch, cwd=self.wrapper).strip(),
            f"{branch}",
        )

    def test_apply_aborts_before_mutation_when_revalidation_changes(self) -> None:
        worktree = self.make_wrapper_worktree()
        calls = 0

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            nonlocal calls
            calls += 1
            return self.merged_pull(head, state="closed" if calls == 1 else "open")

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees"], 7, [], True)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertTrue(report["aborted"])
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["revalidation_failed"])
        self.assertTrue(worktree.exists())

    def test_prunable_metadata_requires_merged_pr_and_is_pruned(self) -> None:
        worktree = self.make_wrapper_worktree()
        shutil.rmtree(worktree)

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(head)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees"], 7, [], False)
            item = next(
                row for row in report["items"] if row["kind"] == "prunable-metadata"
            )
            self.assertEqual(item["disposition"], "eligible")
            report = self.workspace.cleanup(["worktrees"], 7, [], True)
        item = next(row for row in report["items"] if row["kind"] == "prunable-metadata")
        self.assertEqual(item["disposition"], "removed")
        self.assertNotIn(str(worktree), command("git", "worktree", "list", cwd=self.wrapper))

    def make_build(self, profile: str = "review", key: str = "a" * 12) -> Path:
        path = self.workspace.paths.builds / "profiles" / f"{profile}-{key}"
        path.parent.mkdir(parents=True, exist_ok=True)
        managed_directory(path, self.workspace.paths.builds, f"profile:{profile}:{key}")
        (path / "artifact").write_bytes(b"x" * 8192)
        timestamp = self.old.timestamp()
        for candidate in (path / MANAGED_MARKER, path / "artifact", path):
            os.utime(candidate, (timestamp, timestamp), follow_symlinks=False)
        return path

    def test_legacy_build_uses_conservative_tree_age_and_apply_removes_it(self) -> None:
        build = self.make_build()
        report = self.plan(["builds"])
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["age_basis"], "legacy-tree-mtime")
        self.assertTrue(build.exists())

        report = self.workspace.cleanup(["builds"], 7, [], True)
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertEqual(item["disposition"], "removed")
        self.assertFalse(build.exists())

    def test_removable_source_worktree_bypasses_build_age_with_metadata(
        self,
    ) -> None:
        worktree = self.make_component_worktree()
        build = self.make_build()
        atomic_json(
            build / ".atrinik-build.json",
            {
                "schema_version": 1,
                "profile": "review",
                "key": "a" * 12,
                "purpose": f"profile:review:{'a' * 12}",
                "coordinates": {
                    "client": {
                        "component": "client",
                        "checkout": "client",
                        "repository": "atrinik/client",
                        "branch": "main",
                        "source": ".",
                        "checkout_path": str(worktree),
                        "source_path": str(worktree),
                        "head": command("git", "rev-parse", "HEAD", cwd=worktree),
                    }
                },
                "last_used_at": datetime.now(timezone.utc).isoformat(),
            },
        )

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(head)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees", "builds"], 7, [], False)
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertEqual(item["disposition"], "eligible")
        self.assertTrue(item["source_worktree_removal"])
        self.assertEqual(item["reasons"], ["source_worktree_removal"])

    def test_apply_stops_after_first_post_mutation_failure_and_reports_actual_state(self) -> None:
        first = self.make_build("first", "a" * 12)
        second = self.make_build("second", "b" * 12)
        calls = 0

        def remove(path: Path, builds: Path, purpose: str) -> None:
            nonlocal calls
            calls += 1
            if calls == 2:
                raise WorkspaceError("injected removal failure")
            real_managed_remove(path, builds, purpose)

        with mock.patch("atrinik_workspace.cleanup.managed_remove", side_effect=remove):
            report = self.workspace.cleanup(["builds"], 7, [], True)
        by_path = {row["path"]: row for row in report["items"]}
        self.assertEqual(by_path[str(first)]["disposition"], "removed")
        self.assertEqual(by_path[str(second)]["disposition"], "error")
        self.assertFalse(first.exists())
        self.assertTrue(second.exists())
        self.assertTrue(report["aborted"])

    def test_apply_reports_replan_error_after_a_completed_mutation(self) -> None:
        first = self.make_build("first", "a" * 12)
        second = self.make_build("second", "b" * 12)
        original = Cleanup._plan
        calls = 0

        def plan(cleanup: Cleanup, *arguments: object) -> dict[str, object]:
            nonlocal calls
            calls += 1
            if calls == 3:
                raise WorkspaceError("raced inventory")
            return original(cleanup, *arguments)

        with mock.patch.object(Cleanup, "_plan", new=plan):
            report = self.workspace.cleanup(["builds"], 7, [], True)

        by_path = {row["path"]: row for row in report["items"]}
        self.assertEqual(by_path[str(first)]["disposition"], "removed")
        self.assertEqual(by_path[str(second)]["disposition"], "error")
        self.assertEqual(by_path[str(second)]["reasons"], ["revalidation_error"])
        self.assertTrue(report["mutated"])
        self.assertTrue(report["aborted"])
        self.assertFalse(first.exists())
        self.assertTrue(second.exists())

    def test_invalid_marker_and_busy_lock_protect_builds(self) -> None:
        build = self.make_build()
        lock = self.workspace.paths.builds / "locks" / f"review-{'a' * 12}.lock"
        lock.parent.mkdir(parents=True, exist_ok=True)
        with lock.open("w", encoding="utf-8") as stream:
            import fcntl

            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            report = self.plan(["builds"])
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertIn("build_lock_busy", item["reasons"])

        unmanaged = self.workspace.paths.builds / "profiles" / "unmarked"
        unmanaged.mkdir()
        report = self.plan(["builds"])
        item = next(row for row in report["items"] if row["path"] == str(unmanaged))
        self.assertEqual(item["kind"], "unmanaged-build")
        self.assertEqual(item["disposition"], "protected")

    def test_symlinked_workspace_build_container_is_report_only(self) -> None:
        shutil.rmtree(self.workspace.paths.builds)
        external = self.root / "external-builds"
        target = external / "profiles" / f"review-{'a' * 12}"
        managed_directory(target, external, f"profile:review:{'a' * 12}")
        self.workspace.paths.builds.symlink_to(external, target_is_directory=True)

        report = self.workspace.cleanup(["builds"], 0, [], False)

        item = next(
            row
            for row in report["items"]
            if row["path"] == str(self.workspace.paths.builds)
        )
        self.assertEqual(item["kind"], "unmanaged-build")
        self.assertEqual(item["reasons"], ["invalid_build_container"])
        self.assertTrue(target.is_dir())

    def test_build_only_scope_still_protects_a_nested_registered_worktree(self) -> None:
        build = self.make_build()
        nested = build / "nested-worktree"
        command(
            "git",
            "worktree",
            "add",
            "-b",
            "feat/nested-build-worktree",
            str(nested),
            "main",
            cwd=self.wrapper,
        )
        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertIn("contains_registered_worktree", item["reasons"])
        self.assertTrue(nested.is_dir())

    def test_live_current_topology_protects_exact_worktree_coordinates(self) -> None:
        worktree = self.make_component_worktree()
        topology = self.workspace.paths.topologies / "live-current"
        topology.mkdir()
        atomic_json(
            topology / MANAGED_MARKER,
            {"schema_version": SCHEMA_VERSION, "purpose": "topology:live-current"},
        )
        atomic_json(
            topology / "status.json",
            {
                "schema_version": SCHEMA_VERSION,
                "name": "live-current",
                "stack": "default",
                "providers": {"client": "client"},
                "dependencies": ["client"],
                "supervisor": {"pid": 123, "start_time": "1"},
                "services": {},
                "build_root": str(self.workspace.paths.builds / "profiles" / "live"),
                "resolved": {
                    "client": {
                        "path": str(worktree),
                        "checkout_path": str(worktree),
                        "checkout": "client",
                        "repository": "atrinik/client",
                        "branch": "main",
                        "source": ".",
                        "head": command("git", "rev-parse", "HEAD", cwd=worktree),
                        "dirty": False,
                    }
                },
            },
        )

        with mock.patch(
            "atrinik_workspace.cleanup.process_matches", return_value=True
        ), mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, ["client"], False)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("topology_reference", item["reasons"])
        self.assertEqual(item["references"]["topologies"], ["live-current"])
        self.assertNotIn("topology_inventory_error", report["inventory_errors"])
        pulls.assert_not_called()

    def test_live_topology_and_retention_record_protect_exact_build(self) -> None:
        build = self.make_build()
        topology = self.workspace.paths.topologies / "live"
        topology.mkdir()
        (topology / MANAGED_MARKER).write_text(
            json.dumps(
                {"schema_version": SCHEMA_VERSION, "purpose": "topology:live"}
            ),
            encoding="utf-8",
        )
        (topology / "status.json").write_text(
            json.dumps(
                {
                    "schema_version": SCHEMA_VERSION,
                    "name": "live",
                    "stack": "default",
                    "providers": {},
                    "dependencies": [],
                    "supervisor": {"pid": 123, "start_time": "1"},
                    "services": {},
                    "build_root": str(build),
                    "resolved": {},
                }
            ),
            encoding="utf-8",
        )
        with mock.patch("atrinik_workspace.cleanup.process_matches", return_value=True):
            report = self.plan(["builds"])
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertIn("live_topology", item["reasons"])

        shutil.rmtree(topology)
        (self.workspace.paths.builds / "retention.json").write_text(
            json.dumps({"schema_version": 1, "build_roots": [str(build)]}),
            encoding="utf-8",
        )
        report = self.plan(["builds"])
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertIn("retention_reference", item["reasons"])

    def test_npm_cache_requires_explicit_scope_and_legacy_path_is_adopted_on_apply(self) -> None:
        cache = self.workspace.paths.builds / "npm-cache"
        cache.mkdir()
        (cache / "entry").write_text("cached\n", encoding="utf-8")
        timestamp = self.old.timestamp()
        os.utime(cache / "entry", (timestamp, timestamp))
        os.utime(cache, (timestamp, timestamp))

        self.assertFalse(any(row["kind"] == "npm-cache" for row in self.plan([])["items"]))
        report = self.plan(["npm-cache"])
        item = next(row for row in report["items"] if row["kind"] == "npm-cache")
        self.assertTrue(item["legacy_known_cache"])
        self.assertEqual(item["disposition"], "eligible")
        self.assertTrue(cache.exists())

        report = self.workspace.cleanup(["npm-cache"], 7, [], True)
        item = next(row for row in report["items"] if row["kind"] == "npm-cache")
        self.assertEqual(item["disposition"], "removed")
        self.assertFalse(cache.exists())

    def test_dry_run_does_not_create_an_absent_workspace(self) -> None:
        shutil.rmtree(self.workspace.paths.workspace)
        report = self.workspace.cleanup(["builds"], 7, [], False)
        self.assertEqual(report["mode"], "dry-run")
        self.assertFalse(self.workspace.paths.workspace.exists())

    def test_physical_aliases_deduplicate_and_content_branches_stay_distinct(
        self,
    ) -> None:
        repository = Path(__file__).resolve().parents[1]
        actual_workspace = Workspace(repository)
        cleanup = Cleanup(actual_workspace)
        self.assertEqual(
            cleanup._normalize_names(
                ["classic", "classic-client", "classic-server", "classic-protocol"]
            ),
            {"classic"},
        )
        self.assertEqual(
            cleanup._normalize_names(["content", "content-1x"]),
            {"content", "content-1x"},
        )

        head = "a" * 40
        pulls = self.merged_pull(head, base="main")
        reason, _, _ = Cleanup._pull_evidence(pulls, head, "main")
        self.assertIsNone(reason)
        reason, _, _ = Cleanup._pull_evidence(pulls, head, "1.x")
        self.assertEqual(reason, "wrong_base_branch")

        profile = actual_workspace._load_profile("classic", require_file=False)
        migrated = actual_workspace.paths.worktrees / "content" / "classic-maps"
        profile["name"] = "migrated"
        profile["components"]["content-1x"] = {
            "kind": "migrated-worktree",
            "value": str(migrated),
        }
        atomic_json(actual_workspace.paths.profiles / "migrated.json", profile)
        references: dict[str, object] = {"profiles": {}}
        errors: set[str] = set()
        cleanup._profile_references(references, errors)
        self.assertEqual(errors, set())
        self.assertEqual(references["profiles"], {migrated: ["migrated"]})

    def test_allocated_size_credit_deduplicates_shared_inodes(self) -> None:
        first = _base_item("worktree", "atrinik", "atrinik/atrinik", self.root / "a")
        second = _base_item("profile-build", "atrinik", "atrinik/atrinik", self.root / "b")
        first["_inodes"] = {(1, 1): 4096, (1, 2): 4096}
        second["_inodes"] = {(1, 2): 4096, (1, 3): 8192}

        Cleanup._credit_sizes([first, second])

        self.assertEqual(first["allocated_bytes"], 8192)
        self.assertEqual(second["allocated_bytes"], 8192)
        self.assertEqual(
            first["allocated_bytes"] + second["allocated_bytes"], 16384
        )


if __name__ == "__main__":
    unittest.main()
