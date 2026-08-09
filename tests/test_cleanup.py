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

from atrinik_workspace.cleanup import (
    Cleanup,
    _base_item,
    _command,
    _listed_usage,
    _parse_time,
    _path_relation,
    _tree_usage,
    _worktree_records,
    _workspace_owned,
)
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

    def make_component_worktree(
        self,
        label: str = "component-review",
        *,
        component: str = "client",
    ) -> Path:
        primary = self.wrapper / component
        primary.mkdir()
        command("git", "init", "-b", "main", cwd=primary)
        command("git", "config", "user.name", "Tests", cwd=primary)
        command("git", "config", "user.email", "tests@example.invalid", cwd=primary)
        (primary / "README").write_text(f"{component}\n", encoding="utf-8")
        command("git", "add", ".", cwd=primary)
        command("git", "commit", "-m", "feat: seed", cwd=primary)
        command(
            "git",
            "remote",
            "add",
            "origin",
            f"https://github.com/atrinik/{component}.git",
            cwd=primary,
        )
        path = self.workspace.paths.worktrees / component / label
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

    def test_low_level_inventory_helpers_report_invalid_inputs(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "zero or greater"):
            self.workspace.cleanup(["builds"], -1, [], False)
        with mock.patch(
            "atrinik_workspace.cleanup.subprocess.run",
            side_effect=FileNotFoundError,
        ):
            with self.assertRaisesRegex(WorkspaceError, "required command"):
                _command(self.wrapper, "status")
        failure = subprocess.CalledProcessError(
            2, ["git"], stderr="broken repository\n"
        )
        with mock.patch(
            "atrinik_workspace.cleanup.subprocess.run", side_effect=failure
        ):
            with self.assertRaisesRegex(WorkspaceError, "broken repository"):
                _command(self.wrapper, "status")

        for value, message in (
            (None, "must be a timestamp"),
            ("not-a-time", "not a valid timestamp"),
            ("2026-01-01T00:00:00", "must include a UTC offset"),
        ):
            with self.subTest(value=value), self.assertRaisesRegex(
                WorkspaceError, message
            ):
                _parse_time(value, "fixture")

        self.assertTrue(_workspace_owned(self.workspace.paths))
        self.workspace.paths.marker.write_text("not-json\n", encoding="utf-8")
        self.assertFalse(_workspace_owned(self.workspace.paths))
        self.workspace.paths.marker.unlink()
        self.assertFalse(_workspace_owned(self.workspace.paths))
        with self.assertRaisesRegex(WorkspaceError, "ownership marker"):
            self.workspace.cleanup(["builds"], 0, [], True)

        usage_root = self.root / "usage"
        excluded = usage_root / "excluded"
        excluded.mkdir(parents=True)
        (excluded / "large").write_bytes(b"x" * 8192)
        (usage_root / "kept").write_bytes(b"x")
        sizes, observed, error = _tree_usage(usage_root, [excluded])
        self.assertIsNone(error)
        self.assertIsNotNone(observed)
        self.assertGreater(sum(sizes.values()), 0)

        with mock.patch.object(Path, "resolve", side_effect=RuntimeError("loop")):
            self.assertTrue(_path_relation(usage_root, excluded))
            sizes, observed, error = _tree_usage(usage_root, [excluded])
        self.assertEqual(sizes, {})
        self.assertIsNone(observed)
        self.assertIn("loop", error or "")

    def test_github_query_wraps_process_and_response_failures(self) -> None:
        repository = "atrinik/atrinik"
        head = "a" * 40
        cases = (
            (FileNotFoundError(), "required command"),
            (
                subprocess.CalledProcessError(
                    1, ["gh"], stderr="authentication required\n"
                ),
                "authentication required",
            ),
            (subprocess.TimeoutExpired(["gh"], 30), "timed out"),
        )
        for error, message in cases:
            with self.subTest(message=message), mock.patch(
                "atrinik_workspace.cleanup.subprocess.run", side_effect=error
            ), self.assertRaisesRegex(WorkspaceError, message):
                Cleanup._github_pulls(repository, head)

        for stdout, message in (
            ("not-json\n", "not JSON"),
            ("[]\n", "invalid shape"),
        ):
            completed = subprocess.CompletedProcess(["gh"], 0, stdout, "")
            with self.subTest(stdout=stdout), mock.patch(
                "atrinik_workspace.cleanup.subprocess.run", return_value=completed
            ), self.assertRaisesRegex(WorkspaceError, message):
                Cleanup._github_pulls(repository, head)

        row = self.merged_pull(head)[0]
        completed = subprocess.CompletedProcess(
            ["gh"], 0, json.dumps(row) + "\n", ""
        )
        with mock.patch(
            "atrinik_workspace.cleanup.subprocess.run", return_value=completed
        ):
            self.assertEqual(Cleanup._github_pulls(repository, head), [row])

        self.assertEqual(
            Cleanup._pull_evidence([], head, "main")[0], "no_associated_pr"
        )
        self.assertEqual(
            Cleanup._pull_evidence([{"number": 1}], head, "main")[0],
            "invalid_pull_request_evidence",
        )

    def test_tree_usage_is_no_follow_deduplicated_and_excludes_subtrees(self) -> None:
        root = self.root / "tree-usage"
        excluded = root / "excluded"
        excluded.mkdir(parents=True)
        (excluded / "payload").write_bytes(b"excluded")
        artifact = root / "artifact"
        artifact.write_bytes(b"included")
        hardlink = root / "hardlink"
        os.link(artifact, hardlink)
        outside = self.root / "outside"
        outside.write_bytes(b"outside")
        symlink = root / "symlink"
        symlink.symlink_to(outside)

        sizes, observed, error = _tree_usage(root, [excluded])

        artifact_key = (artifact.lstat().st_dev, artifact.lstat().st_ino)
        excluded_key = (excluded.lstat().st_dev, excluded.lstat().st_ino)
        outside_key = (outside.lstat().st_dev, outside.lstat().st_ino)
        symlink_key = (symlink.lstat().st_dev, symlink.lstat().st_ino)
        self.assertIsNone(error)
        self.assertIsNotNone(observed)
        self.assertIn(artifact_key, sizes)
        self.assertIn(symlink_key, sizes)
        self.assertNotIn(excluded_key, sizes)
        self.assertNotIn(outside_key, sizes)
        self.assertEqual(
            _listed_usage(root, ["artifact", "hardlink", "symlink"]),
            {
                artifact_key: artifact.lstat().st_blocks * 512,
                symlink_key: symlink.lstat().st_blocks * 512,
            },
        )

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

    def test_plan_reuses_each_repository_worktree_inventory(self) -> None:
        self.make_component_worktree()

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(head)

        with mock.patch(
            "atrinik_workspace.cleanup._worktree_records",
            wraps=_worktree_records,
        ) as records, mock.patch.object(
            Cleanup, "_github_pulls", side_effect=pulls
        ):
            self.workspace.cleanup(["worktrees"], 7, [], False)

        self.assertEqual(records.call_count, 2)

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

        report = self.workspace.cleanup(["worktrees"], 0, [], True)
        self.assertTrue(report["aborted"])
        self.assertGreater(report["summary"]["error_count"], 0)
        self.assertTrue(worktree.is_dir())

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

    def test_migration_path_inventory_covers_every_current_record_shape(self) -> None:
        paths = list(
            Cleanup._migration_paths(
                {
                    "sources": [
                        {
                            "source": str(self.root / "source"),
                            "archive": str(self.root / "archive"),
                        }
                    ],
                    "worktree_migrations": [
                        {
                            "path": str(self.root / "old"),
                            "destination": str(self.root / "new"),
                        }
                    ],
                    "composite_worktrees": [
                        {"destination": str(self.root / "composite")}
                    ],
                    "worktrees": [{"destination": str(self.root / "worktree")}],
                    "classic": {"path": str(self.root / "classic")},
                }
            )
        )
        self.assertEqual(len(paths), 7)
        self.assertIn(("classic.path", self.root / "classic"), paths)

        with self.assertRaisesRegex(WorkspaceError, "is not a list"):
            list(Cleanup._migration_paths({"sources": {}}))
        with self.assertRaisesRegex(WorkspaceError, "is invalid"):
            list(Cleanup._migration_paths({"sources": ["invalid"]}))
        with self.assertRaisesRegex(WorkspaceError, "classic path"):
            list(Cleanup._migration_paths({"classic": 7}))

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

        with mock.patch.object(
            Cleanup, "_github_pulls", side_effect=pulls
        ), mock.patch(
            "atrinik_workspace.cleanup._worktree_records",
            wraps=_worktree_records,
        ) as records:
            report = self.workspace.cleanup(["worktrees"], 7, [], True)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(records.call_count, 3)
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

    def test_prunable_revalidation_protects_the_repository_wide_scope(self) -> None:
        first = self.make_wrapper_worktree("prunable-first")
        second = self.make_wrapper_worktree("prunable-second")
        (second / "unique").write_text("second\n", encoding="utf-8")
        command("git", "add", "unique", cwd=second)
        command("git", "commit", "-m", "test: unique head", cwd=second)
        first_head = command("git", "rev-parse", "HEAD", cwd=first)
        second_head = command("git", "rev-parse", "HEAD", cwd=second)
        shutil.rmtree(first)
        shutil.rmtree(second)
        calls: dict[str, int] = {}

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            calls[head] = calls.get(head, 0) + 1
            if head == second_head and calls[head] > 1:
                return self.merged_pull(head, state="open")
            return self.merged_pull(head)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees"], 7, [], True)

        listing = command("git", "worktree", "list", cwd=self.wrapper)
        self.assertEqual(calls[first_head], 2)
        self.assertEqual(calls[second_head], 2)
        self.assertTrue(report["aborted"])
        self.assertFalse(report["mutated"])
        self.assertIn(str(first), listing)
        self.assertIn(str(second), listing)

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

    def test_apply_revalidates_a_builds_removable_source_worktree(self) -> None:
        worktree = self.make_component_worktree()
        unselected = self.make_component_worktree(
            "server-review", component="server"
        )
        head = command("git", "rev-parse", "HEAD", cwd=worktree)
        unselected_head = command("git", "rev-parse", "HEAD", cwd=unselected)
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
                        "head": head,
                    },
                    "server": {
                        "component": "server",
                        "checkout": "server",
                        "repository": "atrinik/server",
                        "branch": "main",
                        "source": ".",
                        "checkout_path": str(unselected),
                        "source_path": str(unselected),
                        "head": unselected_head,
                    }
                },
                "last_used_at": datetime.now(timezone.utc).isoformat(),
            },
        )
        calls: dict[str, int] = {}

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            calls[head] = calls.get(head, 0) + 1
            state = "open" if calls[head] > 1 else "closed"
            return self.merged_pull(head, state=state)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(
                ["worktrees", "builds"], 7, ["client"], True
            )

        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertEqual(calls, {head: 2})
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["revalidation_failed"])
        self.assertFalse(report["mutated"])
        self.assertTrue(report["aborted"])
        self.assertTrue(build.exists())
        self.assertTrue(worktree.exists())
        self.assertTrue(unselected.exists())

    def test_build_use_refreshes_exact_coordinate_metadata(self) -> None:
        worktree = self.make_component_worktree()
        self.workspace.create_profile("build-review")
        self.workspace.set_profile(
            "build-review", "client", "worktree", worktree.name
        )
        selected = {"client": worktree}
        key = "c" * 12
        root = self.workspace.paths.builds / "profiles" / f"build-review-{key}"
        managed_directory(
            root, self.workspace.paths.builds, f"profile:build-review:{key}"
        )

        self.workspace._refresh_build_metadata(
            root, "build-review", key, selected
        )

        metadata = json.loads((root / ".atrinik-build.json").read_text(encoding="utf-8"))
        self.assertEqual(metadata["profile"], "build-review")
        self.assertEqual(metadata["key"], key)
        self.assertEqual(
            metadata["coordinates"]["client"],
            {
                "component": "client",
                "checkout": "client",
                "repository": "atrinik/client",
                "branch": "main",
                "source": ".",
                "checkout_path": str(worktree.resolve()),
                "source_path": str(worktree.resolve()),
                "head": command("git", "rev-parse", "HEAD", cwd=worktree),
            },
        )
        self.assertIsNotNone(_parse_time(metadata["last_used_at"], "last use"))

    def test_invalid_or_future_build_metadata_protects_profile_root(self) -> None:
        worktree = self.make_component_worktree()
        build = self.make_build()
        metadata = {
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
            "last_used_at": (
                datetime.now(timezone.utc) + timedelta(days=1)
            ).isoformat(),
        }
        atomic_json(build / ".atrinik-build.json", metadata)

        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertEqual(item["age_basis"], "last-used-at")
        self.assertIn("future_last_used", item["reasons"])

        metadata["coordinates"]["client"]["repository"] = "atrinik/server"
        atomic_json(build / ".atrinik-build.json", metadata)
        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertIn("invalid_build_metadata", item["reasons"])
        self.assertIn("manifest identity", item["error"])
        self.assertIn("build_age_unavailable", item["reasons"])

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

    def test_apply_reports_revalidation_error_after_a_completed_mutation(self) -> None:
        first = self.make_build("first", "a" * 12)
        second = self.make_build("second", "b" * 12)
        original = Cleanup._revalidate_target
        calls = 0

        def revalidate(
            cleanup: Cleanup, *arguments: object
        ) -> dict[str, object] | None:
            nonlocal calls
            calls += 1
            if calls == 2:
                raise WorkspaceError("raced inventory")
            return original(cleanup, *arguments)

        with mock.patch.object(Cleanup, "_revalidate_target", new=revalidate):
            report = self.workspace.cleanup(["builds"], 7, [], True)

        by_path = {row["path"]: row for row in report["items"]}
        self.assertEqual(by_path[str(first)]["disposition"], "removed")
        self.assertEqual(by_path[str(second)]["disposition"], "error")
        self.assertEqual(by_path[str(second)]["reasons"], ["revalidation_error"])
        self.assertTrue(report["mutated"])
        self.assertTrue(report["aborted"])
        self.assertFalse(first.exists())
        self.assertTrue(second.exists())

    def test_apply_plans_once_and_revalidates_only_candidates(self) -> None:
        first = self.make_build("first", "a" * 12)
        second = self.make_build("second", "b" * 12)
        protected = self.make_build("young", "c" * 12)
        timestamp = datetime.now(timezone.utc).timestamp()
        for candidate in (
            protected / MANAGED_MARKER,
            protected / "artifact",
            protected,
        ):
            os.utime(candidate, (timestamp, timestamp), follow_symlinks=False)

        original_plan = Cleanup._plan
        original_revalidate = Cleanup._revalidate_target
        plan_calls = 0
        revalidated: list[str] = []

        def plan(cleanup: Cleanup, *arguments: object) -> dict[str, object]:
            nonlocal plan_calls
            plan_calls += 1
            return original_plan(cleanup, *arguments)

        def revalidate(
            cleanup: Cleanup, target: dict[str, object], *arguments: object
        ) -> dict[str, object] | None:
            revalidated.append(str(target["path"]))
            return original_revalidate(cleanup, target, *arguments)

        with mock.patch.object(Cleanup, "_plan", new=plan), mock.patch.object(
            Cleanup, "_revalidate_target", new=revalidate
        ):
            report = self.workspace.cleanup(["builds"], 7, [], True)

        self.assertEqual(plan_calls, 1)
        self.assertEqual(revalidated, [str(first), str(second)])
        self.assertFalse(first.exists())
        self.assertFalse(second.exists())
        self.assertTrue(protected.exists())
        self.assertFalse(report["aborted"] if "aborted" in report else False)

    def test_apply_preserves_globally_deduplicated_candidate_bytes(self) -> None:
        first = self.make_build("first", "a" * 12)
        second = self.make_build("second", "b" * 12)
        (second / "artifact").unlink()
        os.link(first / "artifact", second / "artifact")
        timestamp = self.old.timestamp()
        os.utime(second, (timestamp, timestamp), follow_symlinks=False)

        preview = self.workspace.cleanup(["builds"], 7, [], False)
        report = self.workspace.cleanup(["builds"], 7, [], True)

        self.assertEqual(
            report["summary"]["removed_bytes"],
            preview["summary"]["candidate_bytes"],
        )
        self.assertFalse(first.exists())
        self.assertFalse(second.exists())

    def test_apply_target_revalidation_repeats_traversal_safety(self) -> None:
        build = self.make_build()
        checkout = self.wrapper / "client"
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
                        "checkout_path": str(checkout),
                        "source_path": str(checkout),
                        "head": "a" * 40,
                    }
                },
                "last_used_at": self.old.isoformat(),
            },
        )
        original = _tree_usage
        build_walks = 0

        def usage(
            root: Path, excluded: object = ()
        ) -> tuple[dict[tuple[int, int], int], datetime | None, str | None]:
            nonlocal build_walks
            if root == build:
                build_walks += 1
                if build_walks == 2:
                    return {}, None, "raced traversal"
            return original(root, excluded)

        with mock.patch("atrinik_workspace.cleanup._tree_usage", side_effect=usage):
            report = self.workspace.cleanup(["builds"], 7, [], True)

        item = next(row for row in report["items"] if row["path"] == str(build))
        self.assertEqual(build_walks, 2)
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["revalidation_failed"])
        self.assertIn("filesystem_traversal_error", item["revalidation"]["reasons"])
        self.assertTrue(build.exists())

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

    def test_profile_marker_and_lock_validation_rejects_unsafe_shapes(self) -> None:
        build = self.make_build()
        marker = build / MANAGED_MARKER
        self.assertEqual(
            Cleanup._profile_marker(build),
            (f"profile:review:{'a' * 12}", "review", "a" * 12),
        )

        marker.unlink()
        with self.assertRaisesRegex(WorkspaceError, "missing or invalid"):
            Cleanup._profile_marker(build)
        atomic_json(marker, {"schema_version": SCHEMA_VERSION})
        with self.assertRaisesRegex(WorkspaceError, "shape is invalid"):
            Cleanup._profile_marker(build)
        atomic_json(
            marker,
            {"schema_version": SCHEMA_VERSION, "purpose": "not-a-profile"},
        )
        with self.assertRaisesRegex(WorkspaceError, "purpose is invalid"):
            Cleanup._profile_marker(build)
        atomic_json(
            marker,
            {
                "schema_version": SCHEMA_VERSION,
                "purpose": f"profile:other:{'a' * 12}",
            },
        )
        with self.assertRaisesRegex(WorkspaceError, "does not match its path"):
            Cleanup._profile_marker(build)

        directory_lock = self.workspace.paths.builds / "directory.lock"
        directory_lock.mkdir()
        busy, error = Cleanup._lock_busy(directory_lock)
        self.assertFalse(busy)
        self.assertTrue(error)
        fifo_lock = self.workspace.paths.builds / "fifo.lock"
        os.mkfifo(fifo_lock)
        busy, error = Cleanup._lock_busy(fifo_lock)
        self.assertFalse(busy)
        self.assertIn("not a regular file", error or "")
        symlink_lock = self.workspace.paths.builds / "symlink.lock"
        symlink_lock.symlink_to(directory_lock, target_is_directory=True)
        busy, error = Cleanup._lock_busy(symlink_lock)
        self.assertFalse(busy)
        self.assertTrue(error)

    def test_symlinked_workspace_build_container_is_report_only(self) -> None:
        shutil.rmtree(self.workspace.paths.builds)
        external = self.root / "external-builds"
        target = external / "profiles" / f"review-{'a' * 12}"
        managed_directory(target, external, f"profile:review:{'a' * 12}")
        (external / "npm-cache").mkdir()
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

        report = self.workspace.cleanup(["npm-cache"], 0, [], False)
        cache = next(row for row in report["items"] if row["kind"] == "npm-cache")
        self.assertEqual(cache["reasons"], ["invalid_cache_path"])
        self.assertFalse(cache["legacy_known_cache"])

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

    def test_managed_npm_cache_honors_metadata_marker_and_active_builds(self) -> None:
        cache = self.workspace.paths.builds / "npm-cache"
        managed_directory(cache, self.workspace.paths.builds, "npm-cache")
        metadata_path = cache / ".atrinik-cache.json"
        atomic_json(
            metadata_path,
            {
                "schema_version": 1,
                "purpose": "npm-cache",
                "last_used_at": datetime.now(timezone.utc).isoformat(),
            },
        )

        report = self.workspace.cleanup(["npm-cache"], 7, [], False)
        item = next(row for row in report["items"] if row["kind"] == "npm-cache")
        self.assertFalse(item["legacy_known_cache"])
        self.assertEqual(item["age_basis"], "last-used-at")
        self.assertIn("younger_than_grace_period", item["reasons"])

        atomic_json(
            metadata_path,
            {
                "schema_version": 1,
                "purpose": "npm-cache",
                "last_used_at": (
                    datetime.now(timezone.utc) + timedelta(days=1)
                ).isoformat(),
            },
        )
        report = self.workspace.cleanup(["npm-cache"], 0, [], False)
        item = next(row for row in report["items"] if row["kind"] == "npm-cache")
        self.assertIn("future_last_used", item["reasons"])

        atomic_json(metadata_path, {"schema_version": 1, "purpose": "wrong"})
        atomic_json(
            cache / MANAGED_MARKER,
            {"schema_version": SCHEMA_VERSION, "purpose": "wrong"},
        )
        lock = self.workspace.paths.builds / "locks" / "active.lock"
        lock.parent.mkdir()
        with lock.open("w", encoding="utf-8") as stream:
            import fcntl

            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            report = self.workspace.cleanup(["npm-cache"], 0, [], False)
        item = next(row for row in report["items"] if row["kind"] == "npm-cache")
        self.assertIn("invalid_managed_marker", item["reasons"])
        self.assertIn("invalid_cache_metadata", item["reasons"])
        self.assertIn("cache_age_unavailable", item["reasons"])
        self.assertIn("active_build", item["reasons"])

        cleanup = Cleanup(self.workspace)
        with self.assertRaisesRegex(WorkspaceError, "marker appeared"):
            cleanup._remove(
                {
                    "kind": "npm-cache",
                    "path": str(cache),
                    "legacy_known_cache": True,
                }
            )
        with self.assertRaisesRegex(WorkspaceError, "unsupported cleanup target"):
            cleanup._remove({"kind": "unknown", "path": str(cache)})

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
            cleanup._normalize_scopes(["all"]), ["worktrees", "builds", "npm-cache"]
        )
        self.assertEqual(cleanup._normalize_names(["atrinik"]), {"atrinik"})
        with self.assertRaisesRegex(WorkspaceError, "unknown components"):
            cleanup._normalize_names(["not-a-component"])
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
