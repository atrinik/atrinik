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


if __name__ == "__main__":
    unittest.main()
