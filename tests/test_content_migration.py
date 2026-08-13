from __future__ import annotations

import json
import os
import fcntl
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import content_migration as migration_module
from atrinik_workspace.content_migration import ContentMigration
from atrinik_workspace.workspace import Workspace


ROOT = Path(__file__).resolve().parents[1]


class ContentMigrationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        shutil.copy2(ROOT / "components.json", self.wrapper / "components.json")
        self.environment = mock.patch.dict(
            os.environ,
            {"ATRINIK_WORKSPACE_DIR": str(self.root / "workspace")},
        )
        self.environment.start()
        self.workspace = Workspace(self.wrapper)
        self.workspace.paths.ensure()
        self.main = self._repository(self.wrapper / "content", "main", "main\n")
        self.one_x = self._repository(
            self.wrapper / "content-1x", "1.x", "frozen\n"
        )
        self.main_anchor = self._git(self.main, "rev-parse", "HEAD")
        self.one_x_anchor = self._git(self.one_x, "rev-parse", "HEAD")
        self.anchors = mock.patch.multiple(
            migration_module,
            CERTIFIED_MAIN_COMMIT=self.main_anchor,
            CERTIFIED_1X_COMMIT=self.one_x_anchor,
        )
        self.anchors.start()

    def tearDown(self) -> None:
        self.anchors.stop()
        self.environment.stop()
        self.temporary.cleanup()

    @staticmethod
    def _git(path: Path, *arguments: str) -> str:
        return subprocess.run(
            ["git", "-C", str(path), *arguments],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()

    def _repository(self, path: Path, branch: str, payload: str) -> Path:
        path.mkdir()
        subprocess.run(["git", "init", "-q", "-b", branch], cwd=path, check=True)
        subprocess.run(
            ["git", "config", "user.name", "Migration Fixture"],
            cwd=path,
            check=True,
        )
        subprocess.run(
            ["git", "config", "user.email", "fixture@example.invalid"],
            cwd=path,
            check=True,
        )
        subprocess.run(
            ["git", "remote", "add", "origin", "https://github.com/atrinik/content.git"],
            cwd=path,
            check=True,
        )
        (path / "payload.txt").write_text(payload, encoding="utf-8")
        subprocess.run(["git", "add", "payload.txt"], cwd=path, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "fixture"], cwd=path, check=True)
        return path

    def _legacy_profile(
        self, name: str = "classic-review", selector: dict[str, str] | None = None
    ) -> tuple[Path, bytes]:
        current = self.workspace._load_profile("classic", require_file=False)
        components = dict(current["components"])
        components["content-1x"] = selector or {"kind": "primary", "value": ""}
        components.pop("content")
        value = {**current, "name": name, "components": components}
        path = self.workspace.paths.profiles / f"{name}.json"
        original = json.dumps(value, indent=2, sort_keys=True).encode() + b"\n"
        path.write_bytes(original)
        return path, original

    def migration(self) -> ContentMigration:
        return ContentMigration(
            self.workspace.paths.repository,
            self.workspace.paths,
            self.workspace.manifest,
        )

    def test_primary_profile_apply_audit_and_restore_preserve_legacy_checkout(self) -> None:
        profile, original = self._legacy_profile()

        planned = self.migration().execute("dry-run")
        self.assertEqual(planned["status"], "ready")
        self.assertEqual(planned["profiles"][0]["selector"], {"kind": "primary", "value": ""})
        self.assertEqual(planned["parity_proof"]["main_commit"], self.main_anchor)
        self.assertEqual(planned["parity_proof"]["final_1x_commit"], self.one_x_anchor)

        applied = self.migration().execute("apply")
        self.assertEqual(applied["status"], "complete")
        current = json.loads(profile.read_text(encoding="utf-8"))
        self.assertIn("content", current["components"])
        self.assertNotIn("content-1x", current["components"])
        self.assertTrue(self.one_x.is_dir())
        self.assertEqual(self.migration().execute("audit")["status"], "complete")

        restored = self.migration().execute("restore")
        self.assertEqual(restored["status"], "restored")
        self.assertEqual(profile.read_bytes(), original)
        self.assertEqual(self.migration().execute("audit")["status"], "restored")
        reapplied = self.migration().execute("apply")
        self.assertEqual(reapplied["status"], "refused")
        self.assertEqual(
            reapplied["refusals"][0]["code"], "migration_already_restored"
        )

    def test_dirty_legacy_primary_refuses_without_rewriting_profile(self) -> None:
        profile, original = self._legacy_profile()
        (self.one_x / "untracked.txt").write_text("keep\n", encoding="utf-8")

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertIn("profile_unproven", {row["code"] for row in result["refusals"]})
        self.assertEqual(profile.read_bytes(), original)
        self.assertTrue((self.one_x / "untracked.txt").is_file())

    def test_clean_legacy_primary_ahead_of_frozen_commit_refuses(self) -> None:
        profile, original = self._legacy_profile()
        (self.one_x / "local-commit.txt").write_text("preserve\n", encoding="utf-8")
        subprocess.run(["git", "add", "local-commit.txt"], cwd=self.one_x, check=True)
        subprocess.run(
            ["git", "commit", "-q", "-m", "local preserved change"],
            cwd=self.one_x,
            check=True,
        )

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertEqual(profile.read_bytes(), original)
        self.assertTrue((self.one_x / "local-commit.txt").is_file())

    def test_duplicate_legacy_profile_keys_refuse_without_normalization(self) -> None:
        profile, original = self._legacy_profile()
        duplicated = b'{"name":"ambiguous",' + original[1:]
        profile.write_bytes(duplicated)

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertIn("profile_unproven", {row["code"] for row in result["refusals"]})
        self.assertEqual(profile.read_bytes(), duplicated)

    def test_incoherent_legacy_checkout_selectors_refuse_without_rewrite(self) -> None:
        profile, _ = self._legacy_profile()
        value = json.loads(profile.read_text(encoding="utf-8"))
        value["components"]["classic-client"] = {
            "kind": "worktree",
            "value": "client-only",
        }
        original = json.dumps(value, indent=2, sort_keys=True).encode() + b"\n"
        profile.write_bytes(original)

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertEqual(profile.read_bytes(), original)

    def test_malformed_current_profile_prevents_not_needed_audit(self) -> None:
        current = self.workspace._load_profile("classic", require_file=False)
        current["name"] = "current-invalid"
        current["components"]["content"] = {"kind": "primary", "value": "bad"}
        path = self.workspace.paths.profiles / "current-invalid.json"
        path.write_text(json.dumps(current), encoding="utf-8")

        result = self.migration().execute("audit")

        self.assertEqual(result["status"], "incomplete")
        self.assertIn("profile_unproven", {row["code"] for row in result["refusals"]})

    def test_proven_managed_main_worktree_moves_to_shared_namespace(self) -> None:
        source = self.workspace.paths.worktrees / "content-1x" / "maps"
        source.parent.mkdir(parents=True)
        subprocess.run(
            ["git", "worktree", "add", "-q", "-b", "review/maps", str(source)],
            cwd=self.main,
            check=True,
        )
        profile, _ = self._legacy_profile(
            selector={"kind": "worktree", "value": "maps"}
        )
        destination = self.workspace.paths.worktrees / "content" / "maps"

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "complete")
        self.assertFalse(source.exists())
        self.assertTrue(destination.is_dir())
        selector = json.loads(profile.read_text(encoding="utf-8"))["components"]["content"]
        self.assertEqual(selector, {"kind": "worktree", "value": "maps"})
        self.assertEqual(self.migration().execute("audit")["status"], "complete")

        restored = self.migration().execute("restore")

        self.assertEqual(restored["status"], "restored")
        self.assertTrue(source.is_dir())
        self.assertFalse(destination.exists())
        self.assertEqual(self.migration().execute("audit")["status"], "restored")

    def test_detached_external_worktree_refuses_without_repointing(self) -> None:
        selected = self.root / "detached-content"
        subprocess.run(
            ["git", "worktree", "add", "-q", "--detach", str(selected)],
            cwd=self.main,
            check=True,
        )
        profile, original = self._legacy_profile(
            selector={"kind": "path", "value": str(selected)}
        )

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertEqual(profile.read_bytes(), original)
        self.assertTrue(selected.is_dir())

    def test_proven_external_and_migration_only_main_worktrees_are_preserved(self) -> None:
        for kind in ("path", "migrated-worktree"):
            with self.subTest(kind=kind):
                selected = self.root / f"external-{kind}"
                subprocess.run(
                    ["git", "worktree", "add", "-q", "-b", f"review/{kind}", str(selected)],
                    cwd=self.main,
                    check=True,
                )
                profile, _ = self._legacy_profile(
                    name=f"classic-{kind}",
                    selector={"kind": kind, "value": str(selected)},
                )

                result = self.migration().execute("apply")

                self.assertEqual(result["status"], "complete")
                selector = json.loads(profile.read_text(encoding="utf-8"))["components"]["content"]
                self.assertEqual(selector, {"kind": "path", "value": str(selected.resolve())})
                self.assertTrue(selected.is_dir())
                self.assertEqual(self.migration().execute("audit")["status"], "complete")
                self.assertEqual(self.migration().execute("restore")["status"], "restored")
                self.migration().record_path.unlink()

    def test_locked_in_progress_and_colliding_managed_worktrees_refuse(self) -> None:
        for condition in ("locked", "in-progress", "collision"):
            with self.subTest(condition=condition):
                source = self.workspace.paths.worktrees / "content-1x" / condition
                source.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(
                    ["git", "worktree", "add", "-q", "-b", f"review/{condition}", str(source)],
                    cwd=self.main,
                    check=True,
                )
                if condition == "locked":
                    subprocess.run(
                        ["git", "worktree", "lock", str(source)],
                        cwd=self.main,
                        check=True,
                    )
                elif condition == "in-progress":
                    operation = Path(self._git(source, "rev-parse", "--git-path", "MERGE_HEAD"))
                    operation.write_text(self.main_anchor + "\n", encoding="utf-8")
                else:
                    destination = self.workspace.paths.worktrees / "content" / condition
                    destination.mkdir(parents=True)
                profile, original = self._legacy_profile(
                    name=f"classic-{condition}",
                    selector={"kind": "worktree", "value": condition},
                )

                result = self.migration().execute("apply")

                self.assertEqual(result["status"], "refused")
                self.assertEqual(profile.read_bytes(), original)
                self.assertTrue(source.is_dir())
                if condition == "locked":
                    subprocess.run(
                        ["git", "worktree", "unlock", str(source)],
                        cwd=self.main,
                        check=True,
                    )

    def test_symlinked_managed_namespace_refuses_without_moving_worktree(self) -> None:
        source = self.workspace.paths.worktrees / "content-1x" / "linked"
        source.parent.mkdir(parents=True)
        subprocess.run(
            ["git", "worktree", "add", "-q", "-b", "review/linked", str(source)],
            cwd=self.main,
            check=True,
        )
        external = self.root / "external-worktrees"
        external.mkdir()
        (self.workspace.paths.worktrees / "content").symlink_to(
            external, target_is_directory=True
        )
        profile, original = self._legacy_profile(
            selector={"kind": "worktree", "value": "linked"}
        )

        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertEqual(profile.read_bytes(), original)
        self.assertTrue(source.is_dir())

    def test_managed_worktree_paths_reject_every_unsafe_namespace_shape(self) -> None:
        migration = self.migration()
        canonical = migration._inspect()["canonical"]
        root = self.workspace.paths.worktrees
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "worktree label is invalid"
        ):
            migration._managed_worktree_paths("../escape")

        shutil.rmtree(root)
        external = self.root / "external-worktrees"
        external.mkdir()
        root.symlink_to(external, target_is_directory=True)
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "root is not a normal directory"
        ):
            migration._managed_worktree_paths("maps")
        root.unlink()
        root.write_text("unsafe\n", encoding="utf-8")
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "root is not a normal directory"
        ):
            migration._managed_worktree_paths("maps")
        root.unlink()
        root.mkdir()

        source_parent = root / "content-1x"
        source_parent.write_text("unsafe\n", encoding="utf-8")
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "worktree namespace is unsafe"
        ):
            migration._managed_worktree_paths("maps")

        selected_file = self.root / "selected-file"
        selected_file.write_text("unsafe\n", encoding="utf-8")
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "not a normal directory"
        ):
            migration._prove_main_worktree(selected_file, canonical)
        selected_link = self.root / "selected-link"
        selected_link.symlink_to(self.main, target_is_directory=True)
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "not a normal directory"
        ):
            migration._prove_main_worktree(selected_link, canonical)

        nested = self.main / "nested"
        nested.mkdir()
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "not a Git worktree root"
        ):
            migration._prove_main_worktree(nested, canonical)
        source_parent.unlink()
        destination_parent = root / "content"
        destination_parent.symlink_to(external, target_is_directory=True)
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "worktree namespace is unsafe"
        ):
            migration._managed_worktree_paths("maps")
        self.assertEqual(list(external.iterdir()), [])

    def test_historical_build_scenario_and_stopped_topology_are_inventoried(self) -> None:
        self._legacy_profile()
        build = self.workspace.paths.builds / "profiles" / "legacy"
        build.mkdir(parents=True)
        (build / ".atrinik-build.json").write_text(
            '{"coordinate":{"checkout":"content-1x","branch":"1.x"}}\n',
            encoding="utf-8",
        )
        scenario = self.workspace.paths.scenarios / "legacy"
        scenario.mkdir(parents=True)
        (scenario / "scenario.json").write_text(
            '{"providers":{"content":"content-1x"}}\n', encoding="utf-8"
        )
        topology = self.workspace.paths.topologies / "stopped"
        topology.mkdir(parents=True)
        (topology / "status.json").write_text(
            json.dumps(
                {
                    "stack": "classic",
                    "profile": "classic-review",
                    "supervisor": {"pid": 100, "start_time": "1"},
                    "services": {"server": {"pid": 101, "start_time": "2"}},
                }
            ),
            encoding="utf-8",
        )

        with mock.patch.object(migration_module, "process_matches", return_value=False):
            result = self.migration().execute("dry-run")

        self.assertEqual(result["status"], "ready")
        self.assertEqual(len(result["resources"]["builds"]), 1)
        self.assertEqual(len(result["resources"]["scenarios"]), 1)
        self.assertEqual(result["resources"]["topologies"][0]["status"], "historical-inert")

    def test_restore_refuses_while_repository_layout_lock_is_held(self) -> None:
        self._legacy_profile()
        self.assertEqual(self.migration().execute("apply")["status"], "complete")
        lock_path = self.workspace.paths.workspace / "repository-layout.lock"
        descriptor = os.open(lock_path, os.O_RDWR)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            result = self.migration().execute("restore")
        finally:
            os.close(descriptor)

        self.assertEqual(result["status"], "refused")
        self.assertEqual(result["refusals"][0]["code"], "repository_layout_busy")

    def test_tampered_journal_cannot_redirect_profile_restore(self) -> None:
        self._legacy_profile()
        self.assertEqual(self.migration().execute("apply")["status"], "complete")
        external = self.root / "external.json"
        external.write_text("preserve\n", encoding="utf-8")
        record = json.loads(self.migration().record_path.read_text(encoding="utf-8"))
        rewrite = next(row for row in record["profiles"] if row["status"] == "rewrite")
        rewrite["path"] = str(external)
        self.migration().record_path.write_text(
            json.dumps(record), encoding="utf-8"
        )

        audit = self.migration().execute("audit")
        restore = self.migration().execute("restore")

        self.assertEqual(audit["status"], "incomplete")
        self.assertEqual(restore["status"], "incomplete")
        self.assertEqual(external.read_text(encoding="utf-8"), "preserve\n")

    def test_duplicate_migration_record_keys_refuse_audit(self) -> None:
        self._legacy_profile()
        self.assertEqual(self.migration().execute("apply")["status"], "complete")
        record = self.migration().record_path
        original = record.read_bytes()
        record.write_bytes(b'{"migration":"ambiguous",' + original[1:])

        result = self.migration().execute("audit")

        self.assertEqual(result["status"], "incomplete")
        self.assertIn(
            "invalid_migration_record", {row["code"] for row in result["refusals"]}
        )

    def test_pending_and_unsafe_record_paths_fail_closed(self) -> None:
        profile, original = self._legacy_profile()
        self.migration().pending_path.parent.mkdir(parents=True, exist_ok=True)
        self.migration().pending_path.write_text("{}\n", encoding="utf-8")

        audit = self.migration().execute("audit")

        self.assertEqual(audit["status"], "incomplete")
        self.assertEqual(audit["refusals"][0]["code"], "pending_migration")
        self.migration().pending_path.unlink()
        external = self.root / "external-record.json"
        external.write_text("preserve\n", encoding="utf-8")
        self.migration().record_path.symlink_to(external)

        applied = self.migration().execute("apply")

        self.assertEqual(applied["status"], "refused")
        self.assertEqual(applied["refusals"][0]["code"], "invalid_migration_record")
        self.assertEqual(profile.read_bytes(), original)
        self.assertEqual(external.read_text(encoding="utf-8"), "preserve\n")

    def test_failed_restore_rolls_profiles_forward_to_complete_state(self) -> None:
        source = self.workspace.paths.worktrees / "content-1x" / "rollback"
        source.parent.mkdir(parents=True)
        subprocess.run(
            ["git", "worktree", "add", "-q", "-b", "review/rollback", str(source)],
            cwd=self.main,
            check=True,
        )
        profile, _ = self._legacy_profile(
            selector={"kind": "worktree", "value": "rollback"}
        )
        self.assertEqual(self.migration().execute("apply")["status"], "complete")
        replacement = profile.read_bytes()
        real_git = migration_module._git
        failed = False

        def fail_restore_move(path: Path, *arguments: str, check: bool = True) -> str:
            nonlocal failed
            if arguments[:2] == ("worktree", "move") and not failed:
                failed = True
                raise migration_module.WorkspaceError("simulated move failure")
            return real_git(path, *arguments, check=check)

        with mock.patch.object(migration_module, "_git", side_effect=fail_restore_move):
            with self.assertRaisesRegex(
                migration_module.WorkspaceError, "restore stopped"
            ):
                self.migration().execute("restore")

        self.assertEqual(profile.read_bytes(), replacement)
        self.assertEqual(self.migration().execute("audit")["status"], "complete")

    def test_affected_live_topology_blocks_apply(self) -> None:
        self._legacy_profile()
        topology = self.workspace.paths.topologies / "live-classic"
        topology.mkdir()
        (topology / "status.json").write_text(
            json.dumps(
                {
                    "stack": "classic",
                    "profile": "classic-review",
                    "supervisor": {"pid": 100, "start_time": "1"},
                    "services": {
                        "server": {"pid": 101, "start_time": "2"}
                    },
                }
            ),
            encoding="utf-8",
        )

        with mock.patch.object(migration_module, "process_matches", return_value=True):
            result = self.migration().execute("apply")

        self.assertEqual(result["status"], "refused")
        self.assertIn("live_topology", {row["code"] for row in result["refusals"]})

    def test_current_live_classic_topology_is_not_mislabeled_as_affected(self) -> None:
        topology = self.workspace.paths.topologies / "current-classic"
        topology.mkdir(parents=True)
        (topology / "status.json").write_text(
            json.dumps(
                {
                    "stack": "classic",
                    "profile": "classic",
                    "supervisor": {"pid": 100, "start_time": "1"},
                    "services": {"server": {"pid": 101, "start_time": "2"}},
                }
            ),
            encoding="utf-8",
        )

        with mock.patch.object(migration_module, "process_matches", return_value=True):
            result = self.migration().execute("dry-run")

        self.assertEqual(result["status"], "not-needed")
        self.assertFalse(result["resources"]["topologies"][0]["affected"])

    def test_missing_canonical_checkout_fails_closed(self) -> None:
        shutil.rmtree(self.main)
        result = self.migration().execute("dry-run")
        self.assertEqual(result["status"], "refused")
        self.assertIn(
            "canonical_content_missing", {row["code"] for row in result["refusals"]}
        )

    def test_fresh_workspace_audit_reports_no_migration_needed(self) -> None:
        result = self.migration().execute("audit")

        self.assertEqual(result["status"], "not-needed")
        self.assertEqual(result["refusals"], [])

    def test_helper_failures_are_bounded_and_actionable(self) -> None:
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "unsupported content migration mode"
        ):
            self.migration().execute("erase")
        for url in (
            "git@github.com:atrinik/content.git",
            "ssh://git@github.com/atrinik/content.git",
            "https://github.com/atrinik/content.git",
        ):
            with self.subTest(url=url):
                self.assertTrue(migration_module._remote_matches(url))
        self.assertFalse(migration_module._remote_matches("file:///tmp/content"))

        missing = mock.Mock(side_effect=FileNotFoundError())
        with mock.patch.object(migration_module.subprocess, "run", missing):
            with self.assertRaisesRegex(
                migration_module.WorkspaceError, "required command not found"
            ):
                migration_module._git(self.main, "status")
            with self.assertRaisesRegex(
                migration_module.WorkspaceError, "required command not found"
            ):
                migration_module._git_succeeds(self.main, "status")

        failed = subprocess.CalledProcessError(
            1, ["git"], output="", stderr="fixture failure"
        )
        with mock.patch.object(migration_module.subprocess, "run", side_effect=failed):
            with self.assertRaisesRegex(
                migration_module.WorkspaceError, "fixture failure"
            ):
                migration_module._git(self.main, "status")

        target = self.root / "atomic" / "profile.json"
        with mock.patch.object(migration_module.os, "replace", side_effect=OSError("race")):
            with self.assertRaisesRegex(OSError, "race"):
                migration_module._atomic_bytes(target, b"preserve\n")
        self.assertFalse(target.exists())
        self.assertEqual(list(target.parent.iterdir()), [])

    def test_layout_lock_rejects_unsafe_path_and_apply_contention(self) -> None:
        unsafe = self.root / "unsafe-lock"
        unsafe.mkdir()
        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "cannot open repository layout lock"
        ):
            migration_module._open_layout_lock(unsafe)

        self._legacy_profile()
        lock_path = self.workspace.paths.workspace / "repository-layout.lock"
        descriptor = os.open(lock_path, os.O_RDWR | os.O_CREAT, 0o600)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            result = self.migration().execute("apply")
        finally:
            os.close(descriptor)
        self.assertEqual(result["status"], "refused")
        self.assertEqual(result["refusals"][-1]["code"], "repository_layout_busy")

    def test_apply_is_idempotent_only_before_explicit_restore(self) -> None:
        self._legacy_profile()
        self.assertEqual(self.migration().execute("apply")["status"], "complete")
        self.assertEqual(self.migration().execute("apply")["status"], "already-applied")

    def test_apply_reports_not_needed_without_legacy_profiles(self) -> None:
        result = self.migration().execute("apply")

        self.assertEqual(result["status"], "not-needed")
        self.assertFalse(self.migration().record_path.exists())

    def test_canonical_checkout_identity_failures_refuse_migration(self) -> None:
        cases = ("symlink", "remote", "branch", "ancestry", "dirty")
        for condition in cases:
            with self.subTest(condition=condition):
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    wrapper = root / "wrapper"
                    wrapper.mkdir()
                    shutil.copy2(ROOT / "components.json", wrapper / "components.json")
                    with mock.patch.dict(
                        os.environ, {"ATRINIK_WORKSPACE_DIR": str(root / "workspace")}
                    ):
                        workspace = Workspace(wrapper)
                        workspace.paths.ensure()
                        main = self._repository(wrapper / "content", "main", "main\n")
                        one_x = self._repository(
                            wrapper / "content-1x", "1.x", "frozen\n"
                        )
                        main_anchor = self._git(main, "rev-parse", "HEAD")
                        one_x_anchor = self._git(one_x, "rev-parse", "HEAD")
                        current = workspace._load_profile("classic", require_file=False)
                        components = dict(current["components"])
                        components["content-1x"] = {"kind": "primary", "value": ""}
                        components.pop("content")
                        current.update(name="legacy", components=components)
                        (workspace.paths.profiles / "legacy.json").write_text(
                            json.dumps(current), encoding="utf-8"
                        )
                        if condition == "symlink":
                            actual = wrapper / "actual-content"
                            main.rename(actual)
                            main.symlink_to(actual, target_is_directory=True)
                        elif condition == "remote":
                            subprocess.run(
                                [
                                    "git",
                                    "remote",
                                    "set-url",
                                    "origin",
                                    "https://example.invalid/content.git",
                                ],
                                cwd=main,
                                check=True,
                            )
                        elif condition == "branch":
                            subprocess.run(
                                ["git", "branch", "-m", "other"],
                                cwd=main,
                                check=True,
                            )
                        elif condition == "dirty":
                            (main / "dirty.txt").write_text(
                                "preserve\n", encoding="utf-8"
                            )
                        anchor = "0" * 40 if condition == "ancestry" else main_anchor
                        with mock.patch.multiple(
                            migration_module,
                            CERTIFIED_MAIN_COMMIT=anchor,
                            CERTIFIED_1X_COMMIT=one_x_anchor,
                        ):
                            result = ContentMigration(
                                workspace.paths.repository,
                                workspace.paths,
                                workspace.manifest,
                            ).execute("dry-run")
                        self.assertEqual(result["status"], "refused")
                        self.assertIn(
                            "canonical_content_unproven",
                            {row["code"] for row in result["refusals"]},
                        )

    def test_profile_schema_and_selector_failures_preserve_exact_bytes(self) -> None:
        current = self.workspace._load_profile("classic", require_file=False)
        legacy_released = {
            key: value
            for key, value in {
                **current,
                "schema_version": 4,
                "sound_mode": "released",
            }.items()
            if key != "sound_release"
        }
        cases: dict[str, object] = {
            "not-object": [],
            "wrong-name": {**current, "name": "elsewhere"},
            "schema": {**current, "schema_version": 99},
            "sound": {**current, "sound_mode": "surround"},
            "legacy-released": legacy_released,
            "components": {**current, "components": []},
            "component-set": {
                **current,
                "components": {"content": {"kind": "primary", "value": ""}},
            },
        }
        for name, value in cases.items():
            with self.subTest(name=name):
                path = self.workspace.paths.profiles / f"{name}.json"
                raw = json.dumps(value).encode()
                path.write_bytes(raw)
                result = self.migration().execute("dry-run")
                row = next(item for item in result["profiles"] if item["name"] == name)
                self.assertEqual(row["status"], "blocked")
                self.assertEqual(path.read_bytes(), raw)
                path.unlink()

        inert = {**current, "name": "replacement", "stack": "default"}
        path = self.workspace.paths.profiles / "replacement.json"
        path.write_text(json.dumps(inert), encoding="utf-8")
        row = next(
            item
            for item in self.migration().execute("dry-run")["profiles"]
            if item["name"] == "replacement"
        )
        self.assertEqual(row["status"], "inert")

    def test_profile_inventory_fails_closed_on_unsafe_storage_shapes(self) -> None:
        profiles = self.workspace.paths.profiles
        shutil.rmtree(profiles)
        self.assertEqual(self.migration().execute("dry-run")["profiles"], [])

        actual = self.root / "external-profiles"
        actual.mkdir()
        profiles.symlink_to(actual, target_is_directory=True)
        result = self.migration().execute("dry-run")
        self.assertIn(
            "invalid_profiles_directory",
            {row["code"] for row in result["refusals"]},
        )
        profiles.unlink()
        profiles.mkdir()

        linked_profile = profiles / "linked.json"
        linked_profile.symlink_to(self.root / "missing-profile.json")
        result = self.migration().execute("dry-run")
        linked = next(row for row in result["profiles"] if row["name"] == "linked")
        self.assertEqual(linked["status"], "blocked")
        linked_profile.unlink()

        profile, original = self._legacy_profile(name="oversized")
        with mock.patch.object(migration_module, "PROFILE_MAX_BYTES", 1):
            result = self.migration().execute("dry-run")
        oversized = next(
            row for row in result["profiles"] if row["name"] == "oversized"
        )
        self.assertEqual(oversized["status"], "blocked")
        self.assertEqual(profile.read_bytes(), original)

    def test_current_profile_selector_validation_rejects_each_unsafe_kind(self) -> None:
        components = self.workspace._load_profile("classic", require_file=False)[
            "components"
        ]
        cases = (
            ("unknown", {**components, "unknown": {"kind": "primary", "value": ""}}),
            ("shape", {**components, "content": []}),
            ("primary", {**components, "content": {"kind": "primary", "value": "bad"}}),
            ("worktree", {**components, "content": {"kind": "worktree", "value": "../bad"}}),
            ("path", {**components, "content": {"kind": "path", "value": "relative"}}),
            ("kind", {**components, "content": {"kind": "migrated-worktree", "value": "/tmp/x"}}),
        )
        migration = self.migration()
        for name, value in cases:
            with self.subTest(name=name):
                with self.assertRaises(migration_module.WorkspaceError):
                    migration._validate_current_profile_components(value)

    def test_legacy_selector_validation_covers_supported_boundaries(self) -> None:
        migration = self.migration()
        inspection = migration._inspect()
        canonical = inspection["canonical"]
        legacy = inspection["legacy"]
        cases: tuple[object, ...] = (
            [],
            {"kind": 1, "value": ""},
            {"kind": "primary", "value": "unexpected"},
            {"kind": "worktree", "value": "../bad"},
            {"kind": "path", "value": "relative"},
            {"kind": "migrated-worktree", "value": "relative"},
            {"kind": "unknown", "value": "value"},
        )
        for selector in cases:
            with self.subTest(selector=selector):
                with self.assertRaises(migration_module.WorkspaceError):
                    migration._migrate_selector("profile", selector, canonical, legacy)
        replacement, move = migration._migrate_selector(
            "profile",
            {"kind": "path", "value": str(self.main)},
            canonical,
            legacy,
        )
        self.assertEqual(replacement, {"kind": "primary", "value": ""})
        self.assertIsNone(move)

    def test_resource_inventory_rejects_unsafe_and_malformed_records(self) -> None:
        builds = self.workspace.paths.builds
        shutil.rmtree(builds)
        external = self.root / "external-builds"
        external.mkdir()
        builds.symlink_to(external, target_is_directory=True)
        topologies = self.workspace.paths.topologies
        shutil.rmtree(topologies)
        topologies.write_text("unsafe\n", encoding="utf-8")
        locks = self.workspace.paths.workspace / "repository-layout.lock"
        locks.symlink_to(self.root / "missing-lock")

        resources, refusals = self.migration()._resource_inventory(set())

        codes = {row["code"] for row in refusals}
        self.assertIn("invalid_builds_directory", codes)
        self.assertIn("invalid_topologies_directory", codes)
        self.assertIn("invalid_lock_path", codes)
        self.assertEqual(resources["locks"][0]["status"], "unsafe")

    def test_resource_inventory_rejects_bad_entries_and_process_records(self) -> None:
        build = self.workspace.paths.builds / "bad"
        build.mkdir(parents=True)
        (build / ".atrinik-build.json").symlink_to(self.root / "missing-build")
        lock_directory = self.workspace.paths.builds / "locks"
        lock_directory.mkdir()
        (lock_directory / "bad").mkdir()
        topology_link = self.workspace.paths.topologies / "linked"
        topology_link.symlink_to(self.root, target_is_directory=True)
        topology = self.workspace.paths.topologies / "malformed"
        topology.mkdir()
        (topology / "status.json").write_text(
            json.dumps({"supervisor": None, "services": {"server": None}}),
            encoding="utf-8",
        )

        resources, refusals = self.migration()._resource_inventory(set())

        codes = [row["code"] for row in refusals]
        self.assertIn("unobservable_builds_record", codes)
        self.assertIn("unobservable_topology", codes)
        self.assertIn("invalid_lock_path", codes)
        self.assertTrue(any(row["status"] == "unsafe" for row in resources["locks"]))

    def test_resource_inventory_bounds_records_and_observes_lock_failures(self) -> None:
        build = self.workspace.paths.builds / "large"
        build.mkdir(parents=True)
        record = build / ".atrinik-build.json"
        record.write_bytes(b"1234")
        lock_directory = self.workspace.paths.builds / "locks"
        lock_directory.mkdir()
        lock = lock_directory / "busy.lock"
        lock.write_text("lock\n", encoding="utf-8")
        with (
            mock.patch.object(migration_module, "RESOURCE_RECORD_MAX_BYTES", 1),
            mock.patch.object(
                migration_module,
                "holders_exist",
                side_effect=migration_module.WorkspaceError("unobservable"),
            ),
        ):
            resources, refusals = self.migration()._resource_inventory(set())
        self.assertIn(
            "unobservable_builds_record", {row["code"] for row in refusals}
        )
        observed = next(row for row in resources["locks"] if row["path"] == str(lock))
        self.assertTrue(observed["active"])

    def test_apply_rolls_back_when_profile_changes_after_preflight(self) -> None:
        profile, original = self._legacy_profile()
        migration = self.migration()
        plan = migration.execute("dry-run")
        profile.write_bytes(original + b" ")

        with self.assertRaisesRegex(
            migration_module.WorkspaceError, "profile changed during migration"
        ):
            migration._apply(plan)

        self.assertFalse(migration.pending_path.exists())
        self.assertFalse(migration.record_path.exists())
        self.assertEqual(profile.read_bytes(), original + b" ")

    def test_apply_rolls_back_profiles_when_record_publication_fails(self) -> None:
        profile, original = self._legacy_profile()
        migration = self.migration()
        plan = migration.execute("dry-run")
        real_atomic_json = migration_module.atomic_json
        calls = 0

        def fail_record(path: Path, value: object) -> None:
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("record publication failed")
            real_atomic_json(path, value)

        with mock.patch.object(migration_module, "atomic_json", side_effect=fail_record):
            with self.assertRaisesRegex(
                migration_module.WorkspaceError, "record publication failed"
            ):
                migration._apply(plan)

        self.assertEqual(profile.read_bytes(), original)
        self.assertFalse(migration.pending_path.exists())
        self.assertFalse(migration.record_path.exists())

    def test_apply_reports_retained_pending_journal_after_commit(self) -> None:
        self._legacy_profile()
        migration = self.migration()
        plan = migration.execute("dry-run")
        real_unlink = Path.unlink

        def fail_pending(path: Path, *args: object, **kwargs: object) -> None:
            if path == migration.pending_path:
                raise OSError("retain journal")
            real_unlink(path, *args, **kwargs)

        with mock.patch.object(Path, "unlink", fail_pending):
            result = migration._apply(plan)

        self.assertEqual(result["status"], "complete")
        self.assertEqual(result["pending_journal_retained"], str(migration.pending_path))
        self.assertTrue(migration.record_path.is_file())
        self.assertTrue(migration.pending_path.is_file())

    def test_journal_shape_and_digest_tampering_fail_closed(self) -> None:
        self._legacy_profile()
        migration = self.migration()
        self.assertEqual(migration.execute("apply")["status"], "complete")
        original = json.loads(migration.record_path.read_text(encoding="utf-8"))
        rewrite_index = next(
            index
            for index, row in enumerate(original["profiles"])
            if row["status"] == "rewrite"
        )
        cases = {
            "shape": lambda value: value.update(migration="other"),
            "profile-row": lambda value: value["profiles"].append([]),
            "profile-name": lambda value: value["profiles"][rewrite_index].update(
                name="../bad"
            ),
            "profile-path": lambda value: value["profiles"][rewrite_index].update(
                path="/tmp/redirect"
            ),
            "profile-bytes": lambda value: value["profiles"][rewrite_index].update(
                original_base64="!"
            ),
            "profile-digest": lambda value: value["profiles"][rewrite_index].update(
                original_sha256="0" * 64
            ),
            "move-row": lambda value: value["worktree_moves"].append([]),
        }
        for name, mutate in cases.items():
            with self.subTest(name=name):
                value = json.loads(json.dumps(original))
                mutate(value)
                migration.record_path.write_text(json.dumps(value), encoding="utf-8")
                result = migration.execute("audit")
                self.assertEqual(result["status"], "incomplete")
                self.assertEqual(
                    result["refusals"][0]["code"], "invalid_migration_record"
                )

    def test_audit_detects_profile_and_worktree_drift(self) -> None:
        source = self.workspace.paths.worktrees / "content-1x" / "audit-drift"
        source.parent.mkdir(parents=True)
        subprocess.run(
            ["git", "worktree", "add", "-q", "-b", "review/audit-drift", str(source)],
            cwd=self.main,
            check=True,
        )
        profile, _ = self._legacy_profile(
            selector={"kind": "worktree", "value": "audit-drift"}
        )
        migration = self.migration()
        self.assertEqual(migration.execute("apply")["status"], "complete")
        profile.write_text("changed\n", encoding="utf-8")
        source.mkdir(parents=True)

        audit = migration.execute("audit")

        codes = {row["code"] for row in audit["refusals"]}
        self.assertIn("profile_audit_failed", codes)
        self.assertIn("worktree_audit_failed", codes)

    def test_restore_refuses_new_unsafe_resources_after_a_clean_audit(self) -> None:
        self._legacy_profile()
        migration = self.migration()
        self.assertEqual(migration.execute("apply")["status"], "complete")
        lock_directory = self.workspace.paths.builds / "locks"
        lock_directory.mkdir(parents=True)
        (lock_directory / "unsafe").mkdir()

        restored = migration.execute("restore")

        self.assertEqual(restored["status"], "refused")
        self.assertIn("invalid_lock_path", {row["code"] for row in restored["refusals"]})


if __name__ == "__main__":
    unittest.main()
