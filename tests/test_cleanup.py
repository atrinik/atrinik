from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import fcntl
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
    _exclusive_sound_producer_lease,
    _listed_usage,
    _parse_time,
    _path_relation,
    _tree_usage,
    _sound_producer_lock_snapshot,
    _worktree_records,
    _workspace_owned,
)
from atrinik_workspace.locking import active_lock_fds, inherit_lock_fds
from atrinik_workspace.model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
    WorkspaceError,
    atomic_json,
    managed_directory,
    managed_remove as real_managed_remove,
)
from atrinik_workspace.process_tree import initialize_lease
from atrinik_workspace.workspace import (
    WORKER_DEPENDENCY_SCHEMA_VERSION,
    Workspace,
    replace_directory,
)


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

    def make_sound_cache(self) -> tuple[Path, Path]:
        sound = self.wrapper / "sound"
        sound.mkdir()
        command("git", "init", "-b", "main", cwd=sound)
        command("git", "config", "user.name", "Tests", cwd=sound)
        command("git", "config", "user.email", "tests@example.invalid", cwd=sound)
        (sound / ".gitignore").write_text("/build/\n", encoding="utf-8")
        command("git", "add", ".gitignore", cwd=sound)
        command("git", "commit", "-m", "feat: seed", cwd=sound)
        command(
            "git", "remote", "add", "origin",
            "https://github.com/atrinik/sound.git", cwd=sound,
        )
        self.write_sound_producer_lease(sound)
        cache = sound / "build" / "atrinik-workspace" / ("a" * 20)
        cache.mkdir(parents=True)
        atomic_json(
            cache / ".atrinik-playtest-tree.json",
            {
                "format": "atrinik-sound-playtest-tree",
                "playtest_only": True,
                "publishable": False,
                "schema_version": 1,
            },
        )
        (cache / "playtest-manifest.json").write_text("{}\n", encoding="utf-8")
        return sound, cache

    @staticmethod
    def write_sound_producer_lease(worktree: Path) -> Path:
        git_directory = Path(
            command(
                "git", "rev-parse", "--path-format=absolute", "--git-dir",
                cwd=worktree,
            )
        )
        lease = git_directory / "atrinik-playtest-builds.lock"
        lease.write_text("atrinik-sound-playtest-builds-v1\n", encoding="utf-8")
        return lease

    def test_sound_cache_is_explicit_preview_first_and_lock_protected(self) -> None:
        _sound, cache = self.make_sound_cache()

        preview = self.workspace.cleanup(["sound-cache"], 0, [], False)
        item = next(row for row in preview["items"] if row["path"] == str(cache))
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["stale_sound_cache"])
        self.assertTrue(cache.is_dir())

        lock_path = cache.parent / f".{cache.name}.build.lock"
        descriptor = os.open(lock_path, os.O_CREAT | os.O_RDWR, 0o600)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            locked = self.workspace.cleanup(["sound-cache"], 0, [], False)
            locked_item = next(
                row for row in locked["items"] if row["path"] == str(cache)
            )
            self.assertEqual(locked_item["disposition"], "skipped")
            self.assertIn("active_build_lock", locked_item["reasons"])
        finally:
            os.close(descriptor)

        applied = self.workspace.cleanup(["sound-cache"], 0, [], True)
        removed = next(row for row in applied["items"] if row["path"] == str(cache))
        self.assertEqual(removed["disposition"], "removed", applied)
        self.assertFalse(cache.exists())

    def test_sound_cache_apply_preserves_per_output_verifier_reader(self) -> None:
        _sound, cache = self.make_sound_cache()
        lock_path = cache.parent / f".{cache.name}.build.lock"
        descriptor = os.open(lock_path, os.O_CREAT | os.O_RDWR, 0o600)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_SH | fcntl.LOCK_NB)
            applied = self.workspace.cleanup(["sound-cache"], 0, [], True)
        finally:
            os.close(descriptor)

        item = next(row for row in applied["items"] if row["path"] == str(cache))
        self.assertEqual(item["disposition"], "skipped")
        self.assertIn("active_build_lock", item["reasons"])
        self.assertTrue(cache.is_dir())

    def test_sound_cache_apply_preserves_git_admin_verifier_reader(self) -> None:
        sound, cache = self.make_sound_cache()
        producer_lock = self.write_sound_producer_lease(sound)
        descriptor = os.open(producer_lock, os.O_RDWR | os.O_NOFOLLOW)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_SH | fcntl.LOCK_NB)
            applied = self.workspace.cleanup(["sound-cache"], 0, [], True)
        finally:
            os.close(descriptor)

        item = next(row for row in applied["items"] if row["path"] == str(cache))
        self.assertNotEqual(item["disposition"], "removed")
        self.assertTrue(cache.is_dir())

    def test_sound_cache_rejects_stale_worktree_record_replaced_by_directory(self) -> None:
        sound, _primary_cache = self.make_sound_cache()
        stale = self.workspace.paths.worktrees / "sound" / "stale"
        stale.parent.mkdir(parents=True)
        command(
            "git", "worktree", "add", "-b", "feat/stale", str(stale),
            cwd=sound,
        )
        shutil.rmtree(stale)
        cache = stale / "build" / "atrinik-workspace" / ("b" * 20)
        cache.mkdir(parents=True)
        atomic_json(
            cache / ".atrinik-playtest-tree.json",
            {
                "format": "atrinik-sound-playtest-tree",
                "playtest_only": True,
                "publishable": False,
                "schema_version": 1,
            },
        )

        preview = self.workspace.cleanup(["sound-cache"], 0, [], False)

        self.assertNotIn(str(cache), {row["path"] for row in preview["items"]})
        self.assertTrue(cache.is_dir())

    def test_sound_cache_symlink_loop_fails_closed_without_mutation(self) -> None:
        _sound, cache = self.make_sound_cache()
        loop = cache.parent / "loop"
        loop.symlink_to(loop)

        preview = self.workspace.cleanup(["sound-cache"], 0, [], False)
        self.assertIn("sound_cache_inventory_error", preview["inventory_errors"])
        self.assertTrue(cache.is_dir())
        self.assertTrue(loop.is_symlink())

        applied = self.workspace.cleanup(["sound-cache"], 0, [], True)
        self.assertIn("sound_cache_inventory_error", applied["inventory_errors"])
        self.assertEqual(applied["summary"]["removed_count"], 0)
        self.assertTrue(cache.is_dir())
        self.assertTrue(loop.is_symlink())

    def test_sound_cache_rejects_copied_same_repository_worktree_pointer(self) -> None:
        sound, _primary_cache = self.make_sound_cache()
        parent = self.workspace.paths.worktrees / "sound"
        stale = parent / "stale-pointer"
        donor = parent / "donor"
        parent.mkdir(parents=True)
        command(
            "git", "worktree", "add", "-b", "feat/stale-pointer", str(stale),
            cwd=sound,
        )
        command(
            "git", "worktree", "add", "-b", "feat/donor", str(donor),
            cwd=sound,
        )
        self.write_sound_producer_lease(donor)
        shutil.rmtree(stale)
        stale.mkdir()
        shutil.copyfile(donor / ".git", stale / ".git")
        cache = stale / "build" / "atrinik-workspace" / ("e" * 20)
        cache.mkdir(parents=True)
        atomic_json(
            cache / ".atrinik-playtest-tree.json",
            {
                "format": "atrinik-sound-playtest-tree",
                "playtest_only": True,
                "publishable": False,
                "schema_version": 1,
            },
        )

        preview = self.workspace.cleanup(["sound-cache"], 0, [], False)

        self.assertNotIn(str(cache), {row["path"] for row in preview["items"]})
        self.assertTrue(cache.is_dir())

        with mock.patch.object(Cleanup, "_github_pulls", return_value=[]):
            worktrees = self.workspace.cleanup(
                ["worktrees"], 0, ["sound"], False
            )
        stale_item = next(
            row for row in worktrees["items"] if row["path"] == str(stale)
        )
        self.assertEqual(stale_item["disposition"], "protected")
        self.assertIn(
            "unexpected_git_worktree_identity", stale_item["reasons"]
        )
        with mock.patch.object(Cleanup, "_github_pulls", return_value=[]):
            applied = self.workspace.cleanup(["all"], 0, ["sound"], True)
        stale_item = next(
            row for row in applied["items"] if row["path"] == str(stale)
        )
        self.assertNotEqual(stale_item["disposition"], "removed")
        self.assertTrue(stale.is_dir())
        self.assertTrue(cache.is_dir())

    def test_sound_cache_accepts_registered_linked_worktree(self) -> None:
        sound, _primary_cache = self.make_sound_cache()
        linked = self.workspace.paths.worktrees / "sound" / "linked"
        linked.parent.mkdir(parents=True)
        command(
            "git", "worktree", "add", "-b", "feat/linked", str(linked),
            cwd=sound,
        )
        self.write_sound_producer_lease(linked)
        cache = linked / "build" / "atrinik-workspace" / ("c" * 20)
        cache.mkdir(parents=True)
        atomic_json(
            cache / ".atrinik-playtest-tree.json",
            {
                "format": "atrinik-sound-playtest-tree",
                "playtest_only": True,
                "publishable": False,
                "schema_version": 1,
            },
        )

        preview = self.workspace.cleanup(["sound-cache"], 0, [], False)
        item = next(row for row in preview["items"] if row["path"] == str(cache))

        self.assertEqual(item["disposition"], "eligible")

    def test_sound_cache_apply_order_precedes_its_worktree(self) -> None:
        cleanup = Cleanup(self.workspace)
        self.assertLess(
            cleanup._apply_order({"kind": "sound-cache", "path": "/cache"}),
            cleanup._apply_order({"kind": "worktree", "path": "/worktree"}),
        )

    def test_active_sound_producer_lock_protects_its_worktree(self) -> None:
        sound, _primary_cache = self.make_sound_cache()
        linked = self.workspace.paths.worktrees / "sound" / "active"
        linked.parent.mkdir(parents=True)
        command(
            "git", "worktree", "add", "-b", "feat/active", str(linked),
            cwd=sound,
        )
        self.write_sound_producer_lease(linked)
        head = command("git", "rev-parse", "HEAD", cwd=linked)
        cache = linked / "build" / "atrinik-workspace" / ("d" * 20)
        cache.mkdir(parents=True)
        atomic_json(
            cache / ".atrinik-playtest-tree.json",
            {
                "format": "atrinik-sound-playtest-tree",
                "playtest_only": True,
                "publishable": False,
                "schema_version": 1,
            },
        )
        lock = cache.parent / f".{cache.name}.build.lock"
        descriptor = os.open(lock, os.O_CREAT | os.O_RDWR, 0o600)
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        pull = self.merged_pull(head)
        try:
            with mock.patch.object(Cleanup, "_github_pulls", return_value=[pull]):
                preview = self.workspace.cleanup(
                    ["worktrees"], 0, ["sound"], False
                )
                worktree = next(
                    row for row in preview["items"] if row["path"] == str(linked)
                )
                self.assertEqual(worktree["disposition"], "protected")
                self.assertIn("active_sound_build", worktree["reasons"])

                applied = self.workspace.cleanup(["all"], 0, ["sound"], True)
        finally:
            os.close(descriptor)

        worktree = next(
            row for row in applied["items"] if row["path"] == str(linked)
        )
        self.assertNotEqual(worktree["disposition"], "removed")
        self.assertTrue(linked.is_dir())
        self.assertTrue(cache.is_dir())

    def test_nonremovable_sound_caches_protect_owning_worktrees(self) -> None:
        sound, _primary_cache = self.make_sound_cache()
        parent = self.workspace.paths.worktrees / "sound"
        parent.mkdir(parents=True)
        caches: list[tuple[Path, Path]] = []
        for label, key in (("young", "f" * 20), ("invalid", "1" * 20)):
            linked = parent / label
            command(
                "git", "worktree", "add", "-b", f"feat/{label}", str(linked),
                cwd=sound,
            )
            self.write_sound_producer_lease(linked)
            cache = linked / "build" / "atrinik-workspace" / key
            cache.mkdir(parents=True)
            atomic_json(
                cache / ".atrinik-playtest-tree.json",
                {
                    "format": "atrinik-sound-playtest-tree",
                    "playtest_only": True,
                    "publishable": label != "invalid",
                    "schema_version": 1,
                },
            )
            caches.append((linked, cache))
        pulls = [
            self.merged_pull(command("git", "rev-parse", "HEAD", cwd=linked))
            for linked, _cache in caches
        ]

        with mock.patch.object(Cleanup, "_github_pulls", return_value=pulls):
            applied = self.workspace.cleanup(["all"], 7, ["sound"], True)

        for linked, cache in caches:
            worktree = next(
                row for row in applied["items"] if row["path"] == str(linked)
            )
            self.assertNotEqual(worktree["disposition"], "removed")
            self.assertIn("sound_cache_present", worktree["reasons"])
            self.assertTrue(linked.is_dir())
            self.assertTrue(cache.is_dir())

    def test_sound_worktree_removal_holds_exclusive_producer_lease(self) -> None:
        sound, _primary_cache = self.make_sound_cache()
        linked = self.workspace.paths.worktrees / "sound" / "race"
        linked.parent.mkdir(parents=True)
        command(
            "git", "worktree", "add", "-b", "feat/race", str(linked),
            cwd=sound,
        )
        head = command("git", "rev-parse", "HEAD", cwd=linked)
        git_directory = Path(
            command("git", "rev-parse", "--path-format=absolute", "--git-dir", cwd=linked)
        )
        producer_lock = git_directory / "atrinik-playtest-builds.lock"
        with mock.patch.object(
            Cleanup, "_github_pulls", return_value=self.merged_pull(head)
        ):
            uncoordinated = self.workspace.cleanup(
                ["worktrees"], 0, ["sound"], False
            )
        item = next(
            row for row in uncoordinated["items"] if row["path"] == str(linked)
        )
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("sound_cleanup_lease_unavailable", item["reasons"])
        producer_lock.write_text(
            "atrinik-sound-playtest-builds-v1\n", encoding="utf-8"
        )
        observed = False

        def checked_command(path: Path, *arguments: str) -> str:
            nonlocal observed
            if arguments[:2] == ("worktree", "remove"):
                descriptor = os.open(
                    producer_lock, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600
                )
                try:
                    with self.assertRaises(BlockingIOError):
                        fcntl.flock(descriptor, fcntl.LOCK_SH | fcntl.LOCK_NB)
                    observed = True
                finally:
                    os.close(descriptor)
            return _command(path, *arguments)

        with mock.patch.object(
            Cleanup, "_github_pulls", return_value=self.merged_pull(head)
        ), mock.patch("atrinik_workspace.cleanup._command", side_effect=checked_command):
            applied = self.workspace.cleanup(["worktrees"], 0, ["sound"], True)

        item = next(row for row in applied["items"] if row["path"] == str(linked))
        self.assertTrue(observed)
        self.assertEqual(item["disposition"], "removed", applied)
        self.assertFalse(linked.exists())

    def test_sound_producer_lease_inode_replacement_is_rejected(self) -> None:
        sound, _cache = self.make_sound_cache()
        lease, identity = _sound_producer_lock_snapshot(sound)
        lease.unlink()
        lease.write_text("atrinik-sound-playtest-builds-v1\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "changed identity"):
            with _exclusive_sound_producer_lease(sound, identity):
                self.fail("replacement producer lease was accepted")

    def add_local_submodule_to_wrapper(self) -> Path:
        source = self.root / "local-submodule"
        source.mkdir()
        command("git", "init", "-b", "main", cwd=source)
        command("git", "config", "user.name", "Tests", cwd=source)
        command("git", "config", "user.email", "tests@example.invalid", cwd=source)
        (source / "README").write_text("local dependency\n", encoding="utf-8")
        command("git", "add", "README", cwd=source)
        command("git", "commit", "-m", "test: seed dependency", cwd=source)
        command(
            "git",
            "-c",
            "protocol.file.allow=always",
            "submodule",
            "add",
            str(source),
            "vendor/dependency",
            cwd=self.wrapper,
        )
        command("git", "commit", "-am", "test: add local submodule", cwd=self.wrapper)
        return source

    @staticmethod
    def initialize_local_submodule(worktree: Path) -> None:
        command(
            "git",
            "-c",
            "protocol.file.allow=always",
            "submodule",
            "update",
            "--init",
            "--recursive",
            cwd=worktree,
        )

    def make_historical_wrapper_graph(
        self, label: str = "legacy-master-review"
    ) -> tuple[Path, str, str, str, str]:
        base = command("git", "rev-parse", "main", cwd=self.wrapper)
        path = self.wrapper / "build" / "worktrees" / label
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
        (path / "review").write_text("historical review\n", encoding="utf-8")
        command("git", "add", "review", cwd=path)
        command("git", "commit", "-m", "test: historical review", cwd=path)
        head = command("git", "rev-parse", "HEAD", cwd=path)
        tree = command("git", "rev-parse", f"{head}^{{tree}}", cwd=self.wrapper)
        merge = command(
            "git",
            "commit-tree",
            tree,
            "-p",
            base,
            "-m",
            "test: squash historical review",
            cwd=self.wrapper,
        )
        boundary = command(
            "git",
            "commit-tree",
            tree,
            "-p",
            merge,
            "-m",
            "test: freeze historical master",
            cwd=self.wrapper,
        )
        return path, base, head, merge, boundary

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
        self,
        head: str,
        *,
        state: str = "closed",
        base: str = "main",
        base_sha: str = "b" * 40,
        merge_commit_sha: str = "c" * 40,
    ) -> list[dict[str, object]]:
        return [
            {
                "number": 42,
                "state": state,
                "html_url": "https://github.com/atrinik/atrinik/pull/42",
                "merged_at": self.old.isoformat() if state == "closed" else None,
                "merge_commit_sha": (
                    merge_commit_sha if state == "closed" else None
                ),
                "head": {"sha": head},
                "base": {"ref": base, "sha": base_sha},
            }
        ]

    def plan(self, scopes: list[str], older: int = 7) -> dict[str, object]:
        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(head)

        with mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            return self.workspace.cleanup(scopes, older, [], False)

    def test_github_workers_inherit_active_layout_descriptor(self) -> None:
        observed: list[tuple[int, ...]] = []
        cleanup = Cleanup(self.workspace)
        item = {
            "kind": "worktree",
            "disposition": "skipped",
            "reasons": ["github_pending"],
            "repository": "atrinik/client",
            "head": "a" * 40,
            "base_branch": "main",
        }

        def inspect(_repository: str, _head: str) -> list[dict[str, object]]:
            observed.append(active_lock_fds())
            raise WorkspaceError("offline")

        with (
            tempfile.TemporaryFile(mode="w+") as lease,
            inherit_lock_fds(lease),
            mock.patch.object(Cleanup, "_github_pulls", side_effect=inspect),
        ):
            cleanup._resolve_github([item], 7)
            self.assertEqual(observed, [(lease.fileno(),)])

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
        ) as process:
            self.assertEqual(Cleanup._github_pulls(repository, head), [row])
        query = process.call_args.args[0][-1]
        self.assertIn("merge_commit_sha", query)
        self.assertIn("sha:.base.sha", query)

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

    def test_historical_master_pull_requires_the_legacy_wrapper_namespace(
        self,
    ) -> None:
        historical, base, _, merge, boundary = self.make_historical_wrapper_graph()
        modern = self.make_wrapper_worktree("modern-master-review")

        def pulls(_repository: str, head: str) -> list[dict[str, object]]:
            return self.merged_pull(
                head,
                base="master",
                base_sha=base,
                merge_commit_sha=merge,
            )

        with mock.patch.dict(
            "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
            {("atrinik/atrinik", "main", "master"): boundary},
            clear=True,
        ), mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        by_path = {row["path"]: row for row in report["items"]}
        historical_item = by_path[str(historical)]
        self.assertEqual(historical_item["disposition"], "eligible")
        self.assertEqual(
            historical_item["reasons"], ["merged_pr_head_historical_base"]
        )
        self.assertEqual(historical_item["base_branch"], "main")
        self.assertEqual(historical_item["merged_pr"]["base"], "master")
        self.assertEqual(by_path[str(modern)]["disposition"], "protected")
        self.assertEqual(by_path[str(modern)]["reasons"], ["wrong_base_branch"])

    def test_historical_pull_graph_fields_are_strict(self) -> None:
        worktree, base, head, merge, boundary = self.make_historical_wrapper_graph()
        valid = self.merged_pull(
            head,
            base="master",
            base_sha=base,
            merge_commit_sha=merge,
        )[0]
        cases: list[tuple[str, dict[str, object]]] = []
        for label, container, field, value in (
            ("missing-base-sha", "base", "sha", None),
            ("malformed-base-sha", "base", "sha", "not-a-sha"),
            ("missing-merge-commit", "row", "merge_commit_sha", None),
            ("malformed-merge-commit", "row", "merge_commit_sha", "not-a-sha"),
        ):
            row = json.loads(json.dumps(valid))
            target = row if container == "row" else row[container]
            if value is None:
                del target[field]
            else:
                target[field] = value
            cases.append((label, row))

        for label, row in cases:
            with self.subTest(label=label), mock.patch.dict(
                "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
                {("atrinik/atrinik", "main", "master"): boundary},
                clear=True,
            ), mock.patch.object(Cleanup, "_github_pulls", return_value=[row]):
                report = self.workspace.cleanup(["worktrees"], 0, [], False)

            item = next(
                row for row in report["items"] if row["path"] == str(worktree)
            )
            self.assertEqual(item["disposition"], "protected")
            self.assertEqual(item["reasons"], ["invalid_pull_request_evidence"])

    def test_historical_pull_requires_first_parent_and_frozen_boundary(self) -> None:
        worktree, base, head, merge, boundary = self.make_historical_wrapper_graph()
        tree = command("git", "rev-parse", f"{merge}^{{tree}}", cwd=self.wrapper)
        outside = command(
            "git",
            "commit-tree",
            tree,
            "-p",
            boundary,
            "-m",
            "test: outside frozen master",
            cwd=self.wrapper,
        )
        cases = (
            ("wrong-first-parent", head, merge),
            ("outside-boundary", boundary, outside),
        )

        for label, base_sha, merge_commit in cases:
            pulls = self.merged_pull(
                head,
                base="master",
                base_sha=base_sha,
                merge_commit_sha=merge_commit,
            )
            with self.subTest(label=label), mock.patch.dict(
                "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
                {("atrinik/atrinik", "main", "master"): boundary},
                clear=True,
            ), mock.patch.object(Cleanup, "_github_pulls", return_value=pulls):
                report = self.workspace.cleanup(["worktrees"], 0, [], False)

            item = next(
                row for row in report["items"] if row["path"] == str(worktree)
            )
            self.assertEqual(item["disposition"], "protected")
            self.assertEqual(item["reasons"], ["historical_base_unverified"])

    def test_historical_pull_graph_proof_ignores_git_replace_refs(self) -> None:
        worktree, base, head, merge, _ = self.make_historical_wrapper_graph()
        tree = command("git", "rev-parse", f"{merge}^{{tree}}", cwd=self.wrapper)
        invalid_merge = command(
            "git",
            "commit-tree",
            tree,
            "-p",
            head,
            "-m",
            "test: invalid historical merge parent",
            cwd=self.wrapper,
        )
        boundary = command(
            "git",
            "commit-tree",
            tree,
            "-p",
            invalid_merge,
            "-m",
            "test: boundary containing invalid merge",
            cwd=self.wrapper,
        )
        command("git", "replace", invalid_merge, merge, cwd=self.wrapper)

        self.assertEqual(
            command(
                "git",
                "rev-list",
                "--parents",
                "-n",
                "1",
                invalid_merge,
                cwd=self.wrapper,
            ).split(),
            [invalid_merge, base],
        )
        pulls = self.merged_pull(
            head,
            base="master",
            base_sha=base,
            merge_commit_sha=invalid_merge,
        )
        with mock.patch.dict(
            "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
            {("atrinik/atrinik", "main", "master"): boundary},
            clear=True,
        ), mock.patch.object(Cleanup, "_github_pulls", return_value=pulls):
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(item["disposition"], "protected")
        self.assertEqual(item["reasons"], ["historical_base_unverified"])

    def test_historical_pull_graph_proof_rejects_info_grafts(self) -> None:
        worktree, base, head, merge, boundary = self.make_historical_wrapper_graph()
        common_git = Path(
            command(
                "git",
                "rev-parse",
                "--path-format=absolute",
                "--git-common-dir",
                cwd=self.wrapper,
            )
        )
        grafts = common_git / "info" / "grafts"
        grafts.parent.mkdir(parents=True, exist_ok=True)
        grafts.write_text(f"{boundary} {merge}\n", encoding="utf-8")
        pulls = self.merged_pull(
            head,
            base="master",
            base_sha=base,
            merge_commit_sha=merge,
        )

        with mock.patch.dict(
            "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
            {("atrinik/atrinik", "main", "master"): boundary},
            clear=True,
        ), mock.patch.object(Cleanup, "_github_pulls", return_value=pulls):
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(item["disposition"], "protected")
        self.assertEqual(item["reasons"], ["historical_base_unverified"])

    def test_historical_namespace_is_proven_from_repository_primary(self) -> None:
        historical, base, head, merge, boundary = (
            self.make_historical_wrapper_graph()
        )
        invocation = self.make_wrapper_worktree("linked-invocation")
        linked_workspace = Workspace(invocation)

        def pulls(_repository: str, queried_head: str) -> list[dict[str, object]]:
            if queried_head == head:
                return self.merged_pull(
                    queried_head,
                    base="master",
                    base_sha=base,
                    merge_commit_sha=merge,
                )
            return self.merged_pull(queried_head, state="open")

        with mock.patch.dict(
            "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
            {("atrinik/atrinik", "main", "master"): boundary},
            clear=True,
        ), mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = linked_workspace.cleanup(["worktrees"], 0, [], False)

        item = next(
            row for row in report["items"] if row["path"] == str(historical)
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["merged_pr_head_historical_base"])

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

    def test_submodule_changes_are_dirty_even_when_configuration_ignores_them(
        self,
    ) -> None:
        self.add_local_submodule_to_wrapper()
        dirty = self.make_wrapper_worktree("dirty-submodule")
        untracked = self.make_wrapper_worktree("untracked-submodule")
        self.initialize_local_submodule(dirty)
        self.initialize_local_submodule(untracked)
        command(
            "git",
            "config",
            "submodule.vendor/dependency.ignore",
            "all",
            cwd=self.wrapper,
        )
        (dirty / "vendor" / "dependency" / "README").write_text(
            "dirty dependency\n", encoding="utf-8"
        )
        (untracked / "vendor" / "dependency" / "untracked").write_text(
            "local dependency output\n", encoding="utf-8"
        )
        self.assertEqual(
            command(
                "git",
                "status",
                "--porcelain=v1",
                "--untracked-files=all",
                cwd=dirty,
            ),
            "",
        )
        self.assertEqual(
            command(
                "git",
                "status",
                "--porcelain=v1",
                "--untracked-files=all",
                cwd=untracked,
            ),
            "",
        )

        with mock.patch.object(Cleanup, "_github_pulls") as pulls:
            report = self.workspace.cleanup(["worktrees"], 0, [], False)

        by_path = {row["path"]: row for row in report["items"]}
        for worktree in (dirty, untracked):
            with self.subTest(worktree=worktree.name):
                self.assertEqual(by_path[str(worktree)]["disposition"], "protected")
                self.assertIn(
                    "populated_submodules", by_path[str(worktree)]["reasons"]
                )
                self.assertIn("dirty_worktree", by_path[str(worktree)]["reasons"])
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
        atomic_json(
            migrations / "content.json",
            {
                "canonical": {"path": str(self.wrapper / "content")},
                "legacy": {"path": str(self.wrapper / "content-1x")},
                "worktree_moves": [
                    {"source": str(worktree), "destination": str(worktree)}
                ],
                "profiles": [{"path": str(self.workspace.paths.profiles / "old.json")}],
                "resources": {
                    "historical_paths": [{"path": str(worktree)}]
                },
            },
        )
        report = self.workspace.cleanup(["worktrees"], 0, ["client"], False)
        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertIn("migration_reference", item["reasons"])
        self.assertGreaterEqual(len(item["references"]["migration"]), 6)

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
                    "canonical": {"path": str(self.root / "content")},
                    "legacy": {"path": str(self.root / "content-1x")},
                    "worktree_moves": [
                        {
                            "source": str(self.root / "content-old-worktree"),
                            "destination": str(self.root / "content-worktree"),
                        }
                    ],
                    "profiles": [{"path": str(self.root / "profile.json")}],
                    "resources": {
                        "scenarios": [{"path": str(self.root / "scenario.json")}]
                    },
                }
            )
        )
        self.assertEqual(len(paths), 13)
        self.assertIn(("classic.path", self.root / "classic"), paths)

        with self.assertRaisesRegex(WorkspaceError, "is not a list"):
            list(Cleanup._migration_paths({"sources": {}}))
        with self.assertRaisesRegex(WorkspaceError, "is invalid"):
            list(Cleanup._migration_paths({"sources": ["invalid"]}))
        with self.assertRaisesRegex(WorkspaceError, "classic path"):
            list(Cleanup._migration_paths({"classic": 7}))

    def test_migration_path_inventory_rejects_every_malformed_extension(self) -> None:
        cases = (
            ({"canonical": []}, "migration canonical is invalid"),
            ({"legacy": {"path": "relative"}}, "migration legacy.path is invalid"),
            ({"resources": []}, "migration resources are invalid"),
            ({"resources": {7: []}}, "migration resource category is invalid"),
            ({"resources": {"states": [7]}}, r"resources.states\[0\] is invalid"),
            (
                {"resources": {"states": [{"path": "relative"}]}},
                r"resources.states\[0\].path is invalid",
            ),
        )
        for value, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(WorkspaceError, message):
                    list(Cleanup._migration_paths(value))

        self.assertEqual(
            list(Cleanup._migration_paths({"resources": {"states": [{}]}})),
            [],
        )

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

    def test_populated_submodule_worktree_is_protected_in_preview_and_apply(
        self,
    ) -> None:
        self.add_local_submodule_to_wrapper()
        worktree = self.make_wrapper_worktree("populated-submodule")
        self.initialize_local_submodule(worktree)
        branch = command("git", "branch", "--show-current", cwd=worktree)
        submodule_git = worktree / "vendor" / "dependency" / ".git"

        for apply in (False, True):
            with self.subTest(apply=apply), mock.patch.object(
                Cleanup, "_github_pulls"
            ) as pulls, mock.patch(
                "atrinik_workspace.cleanup._command", wraps=_command
            ) as git_command:
                report = self.workspace.cleanup(["worktrees"], 0, [], apply)

            item = next(
                row for row in report["items"] if row["path"] == str(worktree)
            )
            remove_calls = [
                call.args[1:]
                for call in git_command.call_args_list
                if call.args[1:3] == ("worktree", "remove")
            ]
            self.assertEqual(item["disposition"], "protected")
            self.assertEqual(item["reasons"], ["populated_submodules"])
            self.assertEqual(remove_calls, [])
            pulls.assert_not_called()
            if apply:
                self.assertFalse(report["mutated"])
                self.assertFalse(report["mutation_attempted"])
            self.assertTrue(submodule_git.exists())
            self.assertTrue(
                command(
                    "git", "branch", "--list", branch, cwd=self.wrapper
                ).endswith(branch)
            )

        self.assertTrue(worktree.is_dir())

    def test_apply_revalidates_and_removes_a_proven_historical_worktree(self) -> None:
        worktree, base, head, merge, boundary = self.make_historical_wrapper_graph()
        branch = command("git", "branch", "--show-current", cwd=worktree)
        pulls = self.merged_pull(
            head,
            base="master",
            base_sha=base,
            merge_commit_sha=merge,
        )

        with mock.patch.dict(
            "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
            {("atrinik/atrinik", "main", "master"): boundary},
            clear=True,
        ), mock.patch.object(
            Cleanup, "_github_pulls", return_value=pulls
        ) as pull_query:
            report = self.workspace.cleanup(["worktrees"], 0, [], True)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(pull_query.call_count, 2)
        self.assertEqual(item["disposition"], "removed")
        self.assertFalse(worktree.exists())
        self.assertEqual(
            command("git", "branch", "--list", branch, cwd=self.wrapper).strip(),
            branch,
        )

    def test_apply_aborts_when_historical_graph_revalidation_changes(self) -> None:
        worktree, base, head, merge, boundary = self.make_historical_wrapper_graph()
        tree = command("git", "rev-parse", f"{merge}^{{tree}}", cwd=self.wrapper)
        outside = command(
            "git",
            "commit-tree",
            tree,
            "-p",
            boundary,
            "-m",
            "test: raced outside frozen master",
            cwd=self.wrapper,
        )
        calls = 0

        def pulls(_repository: str, queried_head: str) -> list[dict[str, object]]:
            nonlocal calls
            calls += 1
            return self.merged_pull(
                queried_head,
                base="master",
                base_sha=base if calls == 1 else boundary,
                merge_commit_sha=merge if calls == 1 else outside,
            )

        with mock.patch.dict(
            "atrinik_workspace.cleanup.HISTORICAL_PULL_BASE_BOUNDARIES",
            {("atrinik/atrinik", "main", "master"): boundary},
            clear=True,
        ), mock.patch.object(Cleanup, "_github_pulls", side_effect=pulls):
            report = self.workspace.cleanup(["worktrees"], 0, [], True)

        item = next(row for row in report["items"] if row["path"] == str(worktree))
        self.assertEqual(calls, 2)
        self.assertTrue(report["aborted"])
        self.assertFalse(report["mutated"])
        self.assertFalse(report["mutation_attempted"])
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["revalidation_failed"])
        self.assertEqual(
            item["revalidation"],
            {
                "disposition": "protected",
                "reasons": ["historical_base_unverified"],
            },
        )
        self.assertTrue(worktree.is_dir())

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

    def test_prunable_metadata_preserves_populated_submodule_admin_objects(
        self,
    ) -> None:
        self.add_local_submodule_to_wrapper()
        populated = self.make_wrapper_worktree("prunable-populated-submodule")
        self.initialize_local_submodule(populated)
        submodule = populated / "vendor" / "dependency"
        command("git", "config", "user.name", "Tests", cwd=submodule)
        command("git", "config", "user.email", "tests@example.invalid", cwd=submodule)
        (submodule / "private").write_text(
            "private linked-worktree object\n", encoding="utf-8"
        )
        command("git", "add", "private", cwd=submodule)
        command(
            "git",
            "commit",
            "-m",
            "test: private linked-worktree object",
            cwd=submodule,
        )
        private_commit = command("git", "rev-parse", "HEAD", cwd=submodule)
        worktree_git = Path(
            command(
                "git",
                "rev-parse",
                "--path-format=absolute",
                "--git-dir",
                cwd=populated,
            )
        )
        modules = worktree_git / "modules"
        private_object = Path(
            command(
                "git",
                "rev-parse",
                "--path-format=absolute",
                "--git-path",
                f"objects/{private_commit[:2]}/{private_commit[2:]}",
                cwd=submodule,
            )
        )
        self.assertTrue(private_object.is_relative_to(modules))
        private_object_evidence = private_object.read_bytes()

        ordinary = self.make_wrapper_worktree("prunable-ordinary")
        (ordinary / "review").write_text("ordinary prunable\n", encoding="utf-8")
        command("git", "add", "review", cwd=ordinary)
        command("git", "commit", "-m", "test: ordinary prunable", cwd=ordinary)
        ordinary_head = command("git", "rev-parse", "HEAD", cwd=ordinary)
        shutil.rmtree(populated)
        shutil.rmtree(ordinary)
        self.assertTrue(modules.is_dir())
        self.assertEqual(private_object.read_bytes(), private_object_evidence)

        for apply in (False, True):
            with self.subTest(apply=apply), mock.patch.object(
                Cleanup,
                "_github_pulls",
                return_value=self.merged_pull(ordinary_head),
            ) as pulls, mock.patch(
                "atrinik_workspace.cleanup._command", wraps=_command
            ) as git_command:
                report = self.workspace.cleanup(["worktrees"], 0, [], apply)

            by_path = {row["path"]: row for row in report["items"]}
            populated_item = by_path[str(populated)]
            ordinary_item = by_path[str(ordinary)]
            prune_calls = [
                call.args[1:]
                for call in git_command.call_args_list
                if call.args[1:3] == ("worktree", "prune")
            ]
            self.assertEqual(populated_item["disposition"], "protected")
            self.assertIn("populated_submodules", populated_item["reasons"])
            self.assertEqual(ordinary_item["disposition"], "protected")
            self.assertEqual(
                ordinary_item["reasons"], ["shared_prune_scope_protected"]
            )
            self.assertEqual(prune_calls, [])
            pulls.assert_called_once_with("atrinik/atrinik", ordinary_head)
            if apply:
                self.assertFalse(report["mutated"])
                self.assertFalse(report["mutation_attempted"])
            self.assertTrue(modules.is_dir())
            self.assertEqual(private_object.read_bytes(), private_object_evidence)

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
        lease_fd = os.open(
            topology / "process-tree.lease", os.O_RDWR | os.O_CREAT, 0o600
        )
        fcntl.flock(lease_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        generation = "a" * 64
        lease = initialize_lease(lease_fd, generation)
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
                "control": {
                    "socket": str((topology / "control.sock").resolve()),
                    "generation": generation,
                    "lease": lease,
                },
                "supervisor": {
                    "pid": 123,
                    "start_time": "1",
                    "generation": generation,
                },
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

        try:
            with mock.patch(
                "atrinik_workspace.cleanup.process_matches", return_value=False
            ), mock.patch.object(Cleanup, "_github_pulls") as pulls:
                report = self.workspace.cleanup(
                    ["worktrees"], 0, ["client"], False
                )
        finally:
            os.close(lease_fd)

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

    def make_worker_dependency_cache(
        self,
        key: str = "a" * 64,
        schema_version: int = WORKER_DEPENDENCY_SCHEMA_VERSION,
    ) -> Path:
        root = self.workspace.paths.builds / "worker-dependencies"
        managed_directory(root, self.workspace.paths.builds, "worker-dependency-cache")
        entry = root / key
        managed_directory(
            entry,
            self.workspace.paths.builds,
            f"worker-dependencies:{key}",
        )
        metadata = {
            "schema_version": schema_version,
            "purpose": "worker-dependencies",
            "key": key,
            "inputs": {"lock": "exact"},
            "node_modules_lock_sha256": "b" * 64,
            "last_used_at": self.old.isoformat(),
        }
        if schema_version != 1:
            metadata["node_modules_sha256"] = "c" * 64
        if schema_version == WORKER_DEPENDENCY_SCHEMA_VERSION:
            metadata["node_modules_view_sha256"] = "d" * 64
        atomic_json(entry / ".atrinik-worker-dependencies.json", metadata)
        (entry / "node_modules").mkdir()
        return entry

    def test_worker_dependency_cache_is_bounded_preview_first_and_locked(self) -> None:
        entry = self.make_worker_dependency_cache()
        preview = self.workspace.cleanup(["builds"], 7, [], False)
        item = next(
            row
            for row in preview["items"]
            if row["kind"] == "worker-dependencies"
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["stale_worker_dependencies"])
        self.assertEqual(item["age_basis"], "last-used-at")
        self.assertTrue(entry.exists())

        lock = (
            self.workspace.paths.builds
            / "locks"
            / f"worker-dependencies-{'a' * 64}.lock"
        )
        lock.parent.mkdir(parents=True, exist_ok=True)
        with lock.open("w", encoding="utf-8") as stream:
            import fcntl

            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            protected = self.workspace.cleanup(["builds"], 7, [], False)
        item = next(
            row
            for row in protected["items"]
            if row["kind"] == "worker-dependencies"
        )
        self.assertIn("build_lock_busy", item["reasons"])

        applied = self.workspace.cleanup(["builds"], 7, [], True)
        item = next(
            row
            for row in applied["items"]
            if row["kind"] == "worker-dependencies"
        )
        self.assertEqual(item["disposition"], "removed")
        self.assertFalse(entry.exists())
        self.assertTrue((self.workspace.paths.builds / "worker-dependencies").exists())

    def test_worker_dependency_cache_removal_is_mount_bounded(self) -> None:
        entry = self.make_worker_dependency_cache()
        payload = entry / "node_modules" / "package.js"
        payload.write_text("preserve\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.cleanup.remove_owned_tree",
            side_effect=WorkspaceError("owned removal encountered a mount boundary"),
        ):
            report = self.workspace.cleanup(["builds"], 7, [], True)

        item = next(
            row
            for row in report["items"]
            if row["kind"] == "worker-dependencies"
        )
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["removal_failed"])
        self.assertIn("mount boundary", item["error"])
        self.assertEqual(payload.read_text(encoding="utf-8"), "preserve\n")

    def test_prior_worker_dependency_schema_is_reclaimable(self) -> None:
        for schema_version, key in ((1, "1" * 64), (2, "2" * 64), (3, "3" * 64)):
            with self.subTest(schema_version=schema_version):
                entry = self.make_worker_dependency_cache(key, schema_version)
                preview = self.workspace.cleanup(["builds"], 7, [], False)
                item = next(
                    row
                    for row in preview["items"]
                    if row["path"] == str(entry)
                )
                self.assertEqual(item["disposition"], "eligible")
                self.assertEqual(item["reasons"], ["stale_worker_dependencies"])
                self.assertTrue(entry.exists())

                applied = self.workspace.cleanup(["builds"], 7, [], True)
                item = next(
                    row
                    for row in applied["items"]
                    if row["path"] == str(entry)
                )
                self.assertEqual(item["disposition"], "removed")
                self.assertFalse(entry.exists())

    def test_invalid_worker_dependency_cache_is_protected(self) -> None:
        entry = self.make_worker_dependency_cache()
        (entry / ".atrinik-worker-dependencies.json").write_text(
            "{}\n", encoding="utf-8"
        )
        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(
            row
            for row in report["items"]
            if row["kind"] == "worker-dependencies"
        )
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("invalid_worker_dependency_cache", item["reasons"])
        self.assertTrue(entry.exists())

    def test_worker_dependency_cleanup_rejects_concurrent_refresh(self) -> None:
        entry = self.make_worker_dependency_cache()
        original_remove = Cleanup._remove

        def refresh_before_remove(
            cleanup: Cleanup, item: dict[str, object], older_than_days: int = 0
        ) -> None:
            if item["kind"] == "worker-dependencies":
                metadata_path = entry / ".atrinik-worker-dependencies.json"
                metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
                metadata["last_used_at"] = cleanup.now.isoformat()
                atomic_json(metadata_path, metadata)
            original_remove(cleanup, item, older_than_days)

        with mock.patch.object(Cleanup, "_remove", new=refresh_before_remove):
            report = self.workspace.cleanup(["builds"], 7, [], True)
        item = next(
            row
            for row in report["items"]
            if row["kind"] == "worker-dependencies"
        )
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["removal_failed"])
        self.assertIn("refreshed", item["error"])
        self.assertTrue(entry.exists())

    def test_worker_dependency_transactions_are_previewed_and_bounded(self) -> None:
        key = "d" * 64
        root = self.workspace.paths.builds / "worker-dependencies"
        managed_directory(root, self.workspace.paths.builds, "worker-dependency-cache")
        transactions = root / ".transactions"
        managed_directory(
            transactions,
            self.workspace.paths.builds,
            "worker-dependency-transactions",
        )
        transaction = transactions / f"{key}-backup-_n056r1s"
        transaction.mkdir()
        (transaction / "partial-package").write_text("partial\n", encoding="utf-8")
        old_timestamp = self.old.timestamp()
        os.utime(transaction / "partial-package", (old_timestamp, old_timestamp))
        os.utime(transaction, (old_timestamp, old_timestamp))

        with mock.patch.object(
            Cleanup,
            "_worker_dependency_transaction_created_at",
            return_value=self.old,
        ):
            preview = self.workspace.cleanup(["builds"], 7, [], False)
        item = next(
            row
            for row in preview["items"]
            if row["kind"] == "worker-dependency-transaction"
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["stale_worker_dependency_transaction"])
        self.assertTrue(transaction.exists())

        with mock.patch.object(
            Cleanup,
            "_worker_dependency_transaction_created_at",
            return_value=self.old,
        ):
            applied = self.workspace.cleanup(["builds"], 7, [], True)
        item = next(
            row
            for row in applied["items"]
            if row["kind"] == "worker-dependency-transaction"
        )
        self.assertEqual(item["disposition"], "removed")
        self.assertFalse(transaction.exists())
        self.assertTrue(transactions.exists())

    def test_new_worker_dependency_backup_gets_a_fresh_grace_period(self) -> None:
        key = "f" * 64
        entry = self.make_worker_dependency_cache(key)
        transactions = entry.parent / ".transactions"
        managed_directory(
            transactions,
            self.workspace.paths.builds,
            "worker-dependency-transactions",
        )
        old_timestamp = self.old.timestamp()
        for path in sorted(entry.rglob("*"), reverse=True):
            os.utime(path, (old_timestamp, old_timestamp), follow_symlinks=False)
        os.utime(entry, (old_timestamp, old_timestamp), follow_symlinks=False)
        staging = transactions / f"{key}-staging-install"
        staging.mkdir()

        with mock.patch(
            "atrinik_workspace.workspace.remove_owned_tree",
            side_effect=WorkspaceError("simulated interruption"),
        ):
            replace_directory(
                entry,
                staging,
                f"{key}-backup-",
                backup_parent=transactions,
            )

        preview = self.workspace.cleanup(["builds"], 7, [], False)
        backup = next(
            row
            for row in preview["items"]
            if row["kind"] == "worker-dependency-transaction"
        )
        self.assertEqual(backup["disposition"], "protected")
        self.assertIn("younger_than_grace_period", backup["reasons"])

    def test_new_staging_with_old_source_mtimes_gets_a_fresh_grace_period(
        self,
    ) -> None:
        key = "9" * 64
        root = self.workspace.paths.builds / "worker-dependencies"
        managed_directory(root, self.workspace.paths.builds, "worker-dependency-cache")
        transactions = root / ".transactions"
        managed_directory(
            transactions,
            self.workspace.paths.builds,
            "worker-dependency-transactions",
        )
        source = self.root / "old-worker-source"
        source.mkdir()
        (source / "worker.ts").write_text("old\n", encoding="utf-8")
        old_timestamp = self.old.timestamp()
        os.utime(source / "worker.ts", (old_timestamp, old_timestamp))
        os.utime(source, (old_timestamp, old_timestamp))
        staging = transactions / f"{key}-staging-install"
        managed_directory(
            staging,
            self.workspace.paths.builds,
            f"worker-dependency-transaction:{key}",
        )
        (staging / MANAGED_MARKER).unlink()
        shutil.copy2(source / "worker.ts", staging / "worker.ts")
        shutil.copystat(source, staging, follow_symlinks=False)

        preview = self.workspace.cleanup(["builds"], 7, [], False)
        item = next(row for row in preview["items"] if row["path"] == str(staging))
        self.assertEqual(item["disposition"], "protected")
        self.assertEqual(item["age_basis"], "tree-mtime-or-root-ctime")
        self.assertIn("younger_than_grace_period", item["reasons"])

    def test_worker_dependency_transaction_removal_rejects_aba(self) -> None:
        key = "8" * 64
        root = self.workspace.paths.builds / "worker-dependencies"
        managed_directory(root, self.workspace.paths.builds, "worker-dependency-cache")
        transactions = root / ".transactions"
        managed_directory(
            transactions,
            self.workspace.paths.builds,
            "worker-dependency-transactions",
        )
        transaction = transactions / f"{key}-staging-install"
        transaction.mkdir()
        old_timestamp = self.old.timestamp()
        os.utime(transaction, (old_timestamp, old_timestamp))
        original_remove = Cleanup._remove
        original_transaction = transactions / f".{transaction.name}-original"

        def replace_before_remove(
            cleanup: Cleanup, item: dict[str, object], older_than_days: int = 0
        ) -> None:
            if item["kind"] == "worker-dependency-transaction":
                transaction.replace(original_transaction)
                transaction.mkdir()
                os.utime(transaction, (old_timestamp, old_timestamp))
            try:
                original_remove(cleanup, item, older_than_days)
            finally:
                if original_transaction.exists():
                    shutil.rmtree(original_transaction)

        with (
            mock.patch.object(
                Cleanup,
                "_worker_dependency_transaction_created_at",
                return_value=self.old,
            ),
            mock.patch.object(Cleanup, "_remove", new=replace_before_remove),
        ):
            applied = self.workspace.cleanup(["builds"], 7, [], True)
        item = next(
            row
            for row in applied["items"]
            if row["path"] == str(transaction)
        )
        self.assertEqual(item["disposition"], "error")
        self.assertEqual(item["reasons"], ["removal_failed"])
        self.assertIn("changed before removal", item["error"])
        self.assertTrue(transaction.exists())

    def test_worker_dependency_transaction_uncertainty_protects_artifacts(self) -> None:
        key = "e" * 64
        root = self.workspace.paths.builds / "worker-dependencies"
        managed_directory(root, self.workspace.paths.builds, "worker-dependency-cache")
        transactions = root / ".transactions"
        managed_directory(
            transactions,
            self.workspace.paths.builds,
            "worker-dependency-transactions",
        )
        old_timestamp = self.old.timestamp()

        invalid_name = transactions / "unrecognized"
        invalid_name.mkdir()
        os.utime(invalid_name, (old_timestamp, old_timestamp))

        wrong_marker = transactions / f"{key}-staging-bad"
        wrong_marker.mkdir()
        atomic_json(
            wrong_marker / MANAGED_MARKER,
            {"schema_version": SCHEMA_VERSION, "purpose": "unrelated"},
        )
        os.utime(wrong_marker, (old_timestamp, old_timestamp))

        future = transactions / f"{key}-backup-feed"
        future.mkdir()
        now = datetime.now(timezone.utc)
        future_timestamp = (now + timedelta(days=1)).timestamp()
        os.utime(future, (future_timestamp, future_timestamp))

        young = transactions / f"{key}-staging-cafe"
        young.mkdir()

        busy = transactions / f"{key}-staging-dead"
        busy.mkdir()
        os.utime(busy, (old_timestamp, old_timestamp))
        lock = (
            self.workspace.paths.builds
            / "locks"
            / f"worker-dependencies-{key}.lock"
        )
        lock.parent.mkdir(parents=True, exist_ok=True)
        with lock.open("w", encoding="utf-8") as stream:
            import fcntl

            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            report = self.workspace.cleanup(["builds"], 7, [], False)

        items = {
            Path(row["path"]).name: row
            for row in report["items"]
            if row["kind"] == "worker-dependency-transaction"
        }
        self.assertIn(
            "invalid_worker_dependency_transaction",
            items[invalid_name.name]["reasons"],
        )
        self.assertIn(
            "invalid_worker_dependency_transaction",
            items[wrong_marker.name]["reasons"],
        )
        self.assertIn("future_tree_mtime", items[future.name]["reasons"])
        self.assertIn("younger_than_grace_period", items[young.name]["reasons"])
        self.assertIn("build_lock_busy", items[busy.name]["reasons"])

    def test_invalid_worker_dependency_transaction_root_is_protected(self) -> None:
        root = self.workspace.paths.builds / "worker-dependencies"
        managed_directory(root, self.workspace.paths.builds, "worker-dependency-cache")
        transactions = root / ".transactions"
        transactions.mkdir()
        atomic_json(
            transactions / MANAGED_MARKER,
            {"schema_version": SCHEMA_VERSION, "purpose": "unrelated"},
        )
        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(
            row
            for row in report["items"]
            if row["path"] == str(transactions)
        )
        self.assertEqual(item["disposition"], "protected")
        self.assertEqual(item["reasons"], ["invalid_worker_dependency_transactions"])

    def test_worker_dependency_cache_age_and_key_fail_closed(self) -> None:
        entry = self.make_worker_dependency_cache()
        metadata_path = entry / ".atrinik-worker-dependencies.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        now = datetime.now(timezone.utc)
        metadata["last_used_at"] = (now + timedelta(days=1)).isoformat()
        atomic_json(metadata_path, metadata)
        report = self.workspace.cleanup(["builds"], 7, [], False)
        item = next(
            row for row in report["items"] if row["kind"] == "worker-dependencies"
        )
        self.assertIn("future_last_used", item["reasons"])

        metadata["last_used_at"] = now.isoformat()
        atomic_json(metadata_path, metadata)
        report = self.workspace.cleanup(["builds"], 7, [], False)
        item = next(
            row for row in report["items"] if row["kind"] == "worker-dependencies"
        )
        self.assertIn("younger_than_grace_period", item["reasons"])

        invalid = self.make_worker_dependency_cache("not-a-key")
        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(row for row in report["items"] if row["path"] == str(invalid))
        self.assertIn("invalid_worker_dependency_cache", item["reasons"])
    def test_compiler_cache_is_bounded_marker_owned_and_explicitly_reclaimed(
        self,
    ) -> None:
        cache = self.workspace.paths.builds / "compiler-cache"
        managed_directory(cache, self.workspace.paths.builds, "compiler-cache")
        atomic_json(
            cache / ".atrinik-cache.json",
            {
                "schema_version": 1,
                "purpose": "compiler-cache",
                "last_used_at": "2020-01-01T00:00:00+00:00",
                "max_size": "5G",
            },
        )
        (cache / "entry").write_text("cached\n", encoding="utf-8")

        self.assertFalse(
            any(row["kind"] == "compiler-cache" for row in self.plan([])["items"])
        )
        report = self.plan(["compiler-cache"])
        item = next(
            row for row in report["items"] if row["kind"] == "compiler-cache"
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["stale_compiler_cache"])

        (cache / ".atrinik-cache.json").unlink()
        report = self.plan(["compiler-cache"])
        item = next(
            row for row in report["items"] if row["kind"] == "compiler-cache"
        )
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("invalid_cache_metadata", item["reasons"])
        self.assertIn("cache_age_unavailable", item["reasons"])
        atomic_json(
            cache / ".atrinik-cache.json",
            {
                "schema_version": 1,
                "purpose": "compiler-cache",
                "last_used_at": "2020-01-01T00:00:00+00:00",
                "max_size": "5G",
            },
        )

        removed = self.workspace.cleanup(["compiler-cache"], 0, [], True)
        self.assertEqual(removed["summary"]["removed_count"], 1)
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
            cleanup._normalize_scopes(["all"]),
            [
                "worktrees", "builds", "npm-cache", "compiler-cache",
                "sound-cache",
            ],
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
        self.assertEqual(cleanup._normalize_names(["content"]), {"content"})
        with self.assertRaisesRegex(WorkspaceError, "unknown components"):
            cleanup._normalize_names(["content-1x"])

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
        profile["components"].pop("content")
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
