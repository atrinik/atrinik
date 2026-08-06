from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import (
    MANAGED_MARKER,
    WorkspaceError,
    load_json,
    managed_directory,
    managed_reset,
)
from atrinik_workspace.workspace import (
    Workspace,
    _remote_matches as real_remote_matches,
    display_arguments,
    exclusive_lock,
    run as workspace_run,
)


COMPONENTS = (
    ("client", "client"),
    ("server", "server"),
    ("protocol", "protocol"),
    ("libatrinik", "library"),
    ("content", "content"),
    ("sound", "assets"),
    ("resources", "assets"),
)


def command(*arguments: str, cwd: Path) -> str:
    result = subprocess.run(
        list(arguments), cwd=cwd, check=True, capture_output=True, text=True
    )
    return result.stdout.strip()


class WorkspaceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        manifest = {
            "schema_version": 1,
            "components": [
                {
                    "name": name,
                    "repository": f"atrinik/{name}",
                    "branch": "master",
                    "build": build,
                }
                for name, build in COMPONENTS
            ],
        }
        (self.wrapper / "components.json").write_text(
            json.dumps(manifest), encoding="utf-8"
        )
        self.workspace_directory = self.root / "workspace"
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.workspace_directory)}
        )
        self.environment.start()
        self.workspace = Workspace(self.wrapper)
        self.workspace.paths.ensure()
        self.seeds: dict[str, Path] = {}
        self.origins: dict[str, Path] = {}
        for name, _ in COMPONENTS:
            self.make_component(name)
        self.remote_matcher = mock.patch(
            "atrinik_workspace.workspace._remote_matches",
            side_effect=lambda url, repository: real_remote_matches(url, repository)
            or url == str(self.origins[repository.split("/", 1)[1]]),
        )
        self.remote_matcher.start()

    def tearDown(self) -> None:
        self.remote_matcher.stop()
        self.environment.stop()
        self.temporary.cleanup()

    def make_component(self, name: str) -> None:
        origin = self.root / "origins" / f"{name}.git"
        origin.parent.mkdir(exist_ok=True)
        command("git", "init", "--bare", str(origin), cwd=self.root)
        seed = self.root / "seeds" / name
        seed.mkdir(parents=True)
        command("git", "init", "-b", "master", cwd=seed)
        command("git", "config", "user.name", "Tests", cwd=seed)
        command("git", "config", "user.email", "tests@example.invalid", cwd=seed)
        (seed / "README").write_text(f"{name}\n", encoding="utf-8")
        if name == "server":
            (seed / "install_data" / "keys").mkdir(parents=True)
            (seed / "install_data" / "unique-items").mkdir()
            (seed / "install_data" / "keys" / "test.pub").write_text(
                "key\n", encoding="utf-8"
            )
            (seed / "install_data" / "unique-items" / ".keep").write_text(
                "\n", encoding="utf-8"
            )
            (seed / "install_data" / "bans").write_text("", encoding="utf-8")
            (seed / "install_data" / "motd").write_text("Welcome\n", encoding="utf-8")
        command("git", "add", ".", cwd=seed)
        command("git", "commit", "-m", "feat: seed", cwd=seed)
        command("git", "remote", "add", "origin", str(origin), cwd=seed)
        command("git", "push", "-u", "origin", "master", cwd=seed)
        checkout = self.workspace.paths.repositories / name
        checkout.parent.mkdir(parents=True, exist_ok=True)
        command("git", "clone", str(origin), str(checkout), cwd=self.root)
        command(
            "git",
            "remote",
            "add",
            "upstream",
            f"https://github.com/atrinik/{name}.git",
            cwd=checkout,
        )
        command("git", "config", "user.name", "Tests", cwd=checkout)
        command("git", "config", "user.email", "tests@example.invalid", cwd=checkout)
        self.seeds[name] = seed
        self.origins[name] = origin

    def advance_origin(self, name: str, filename: str) -> str:
        seed = self.seeds[name]
        command("git", "pull", "--ff-only", cwd=seed)
        (seed / filename).write_text("change\n", encoding="utf-8")
        command("git", "add", filename, cwd=seed)
        command("git", "commit", "-m", "fix: advance", cwd=seed)
        command("git", "push", cwd=seed)
        return command("git", "rev-parse", "HEAD", cwd=seed)

    def test_initialize_accepts_existing_real_repositories(self) -> None:
        self.workspace.initialize(None, jobs=3)
        self.assertTrue((self.workspace.paths.repositories / "server" / ".git").exists())

    def test_failed_clone_does_not_strand_destination(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)

        def fail_clone(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] == "gh":
                temporary = Path(arguments[-1])
                (temporary / "partial").write_text("incomplete\n", encoding="utf-8")
                raise WorkspaceError("clone failed")
            return workspace_run(arguments, **kwargs)

        with mock.patch("atrinik_workspace.workspace.run", side_effect=fail_clone):
            with self.assertRaisesRegex(WorkspaceError, "clone failed"):
                self.workspace._ensure_repository(self.workspace._component("client"))

        self.assertFalse(destination.exists())
        self.assertEqual(
            list(self.workspace.paths.repositories.glob(".atrinik-clone-client-*")), []
        )

    def test_initialize_preserves_broken_symlink_at_component_path(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)
        destination.symlink_to(self.root / "missing-checkout", target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._ensure_repository(self.workspace._component("client"))

        self.assertTrue(destination.is_symlink())

    def test_sync_fast_forwards_primary_checkout(self) -> None:
        expected = self.advance_origin("client", "new-file")
        self.workspace.sync(["client"], "none")
        actual = command(
            "git", "rev-parse", "HEAD", cwd=self.workspace.paths.repositories / "client"
        )
        self.assertEqual(actual, expected)

    def test_sync_refuses_dirty_primary_checkout(self) -> None:
        checkout = self.workspace.paths.repositories / "client"
        (checkout / "dirty").write_text("keep\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "dirty primary"):
            self.workspace.sync(["client"], "none")
        self.assertTrue((checkout / "dirty").is_file())

    def test_sync_preflights_every_checkout_before_updating(self) -> None:
        client = self.workspace.paths.repositories / "client"
        before = command("git", "rev-parse", "HEAD", cwd=client)
        self.advance_origin("client", "new-file")
        server = self.workspace.paths.repositories / "server"
        (server / "dirty").write_text("keep\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "dirty primary"):
            self.workspace.sync(["client", "server"], "none")

        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=client), before)

    def test_sync_uses_canonical_upstream_when_origin_is_absent(self) -> None:
        checkout = self.workspace.paths.repositories / "client"
        command("git", "remote", "remove", "origin", cwd=checkout)
        command(
            "git", "remote", "set-url", "upstream", str(self.origins["client"]), cwd=checkout
        )
        command(
            "git",
            "remote",
            "set-url",
            "--add",
            "upstream",
            "https://github.com/atrinik/client.git",
            cwd=checkout,
        )
        expected = self.advance_origin("client", "upstream-file")

        self.workspace.sync(["client"], "none")

        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=checkout), expected)

    def test_repository_status_reports_dirty_and_cached_divergence(self) -> None:
        client = self.workspace.paths.repositories / "client"
        self.advance_origin("client", "remote-change")
        command("git", "fetch", "origin", cwd=client)
        (client / "untracked").write_text("keep\n", encoding="utf-8")

        rows = {
            row["component"]: row
            for row in self.workspace.repository_status(["client", "server"])
        }

        self.assertTrue(rows["client"]["initialized"])
        self.assertTrue(rows["client"]["dirty"])
        self.assertEqual(rows["client"]["remote"], "origin")
        self.assertEqual(rows["client"]["ahead"], 0)
        self.assertEqual(rows["client"]["behind"], 1)
        self.assertFalse(rows["server"]["dirty"])

    def test_repository_status_reports_uninitialized_component(self) -> None:
        shutil.rmtree(self.workspace.paths.repositories / "client")

        row = self.workspace.repository_status(["client"])[0]

        self.assertFalse(row["initialized"])
        self.assertIsNone(row["head"])
        self.assertIsNone(row["dirty"])

    def test_repository_status_rejects_non_directory_component_path(self) -> None:
        client = self.workspace.paths.repositories / "client"
        shutil.rmtree(client)
        client.write_text("not a checkout\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace.repository_status(["client"])

    def test_worktree_profile_and_safe_removal(self) -> None:
        path = self.workspace.create_worktree(
            "content", "map-review", "feat/map-review", None, False
        )
        self.workspace.create_profile("review")
        self.workspace.set_profile("review", "content", "worktree", "map-review")
        resolved = self.workspace.resolve_profile("review")
        self.assertEqual(resolved["content"], path.resolve())

        (path / "untracked").write_text("keep\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "dirty worktree"):
            self.workspace.remove_worktree("content", "map-review")
        self.assertTrue((path / "untracked").is_file())

    def test_profile_can_clone_an_existing_selection(self) -> None:
        path = self.workspace.create_worktree(
            "content", "map-review", "feat/map-review", None, False
        )
        self.workspace.create_profile("review")
        self.workspace.set_profile("review", "content", "worktree", "map-review")

        self.workspace.create_profile("review-copy", "review")

        self.assertEqual(
            self.workspace.component_path("content", "review-copy"), path.resolve()
        )
        self.workspace.set_profile("review-copy", "content", "primary")
        self.assertEqual(
            self.workspace.component_path("content", "review"), path.resolve()
        )

    def test_component_path_only_requires_selected_component(self) -> None:
        for name, _ in COMPONENTS:
            if name != "content":
                shutil.rmtree(self.workspace.paths.repositories / name)

        self.assertEqual(
            self.workspace.component_path("content", "default"),
            (self.workspace.paths.repositories / "content").resolve(),
        )

    def test_profile_rejects_checkout_for_another_component(self) -> None:
        self.workspace.create_profile("review")
        with self.assertRaisesRegex(WorkspaceError, "no origin/upstream"):
            self.workspace.set_profile(
                "review",
                "content",
                "path",
                str(self.workspace.paths.repositories / "client"),
            )

    def test_profile_rejects_nested_checkout_path(self) -> None:
        checkout = self.workspace.paths.repositories / "content"
        nested = checkout / "nested"
        nested.mkdir()
        self.workspace.create_profile("review")
        with self.assertRaisesRegex(WorkspaceError, "worktree root"):
            self.workspace.set_profile("review", "content", "path", str(nested))

    def test_component_build_resolves_only_its_dependencies(self) -> None:
        for name, _ in COMPONENTS:
            if name != "content":
                shutil.rmtree(self.workspace.paths.repositories / name)
        expected = self.workspace.paths.builds / "result"
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=expected
        ) as build_resolved:
            actual = self.workspace.build("content", "default", tests=False)

        self.assertEqual(actual, expected)
        selected = build_resolved.call_args.args[4]
        self.assertEqual(set(selected), {"content"})

    def test_start_point_cannot_be_an_option(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "must not begin"):
            self.workspace.create_worktree(
                "content", "bad-start", "feat/bad-start", "--help", False
            )
        self.assertFalse(
            (self.workspace.paths.worktrees / "content" / "bad-start").exists()
        )

    def test_source_view_reserves_ownership_marker_and_copies_worker(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / MANAGED_MARKER).write_text("component data\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        linked = self.workspace._profile_source_view(root, "linked", source, set())
        self.assertEqual(load_json(linked / MANAGED_MARKER)["purpose"], "source-view:linked")

        copied = self.workspace._profile_source_view(
            root, "copied", source, set(), copy_all=True
        )
        (copied / "README").write_text("changed in view\n", encoding="utf-8")
        self.assertEqual((source / "README").read_text(encoding="utf-8"), "content\n")

    def test_resource_view_reserves_generated_metadata_names(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / MANAGED_MARKER).write_text("component marker\n", encoding="utf-8")
        (source / ".atrinik-dependency.json").write_text(
            "component metadata\n", encoding="utf-8"
        )
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        output = self.workspace._stage_resources(root, {"resources": source})

        self.assertEqual(load_json(output / MANAGED_MARKER)["purpose"], "resource-view")
        self.assertEqual(
            load_json(output / ".atrinik-dependency.json")["workspace_source"],
            str(source),
        )
        self.assertEqual(
            (source / ".atrinik-dependency.json").read_text(encoding="utf-8"),
            "component metadata\n",
        )

    def test_content_collection_failure_preserves_previous_output(self) -> None:
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = root / "runtime" / "content"
        managed_reset(output, self.workspace.paths.builds, "collected-content")
        (output / "sentinel").write_text("last good\n", encoding="utf-8")

        def fail_collector(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] == os.sys.executable:
                raise WorkspaceError("collector failed")
            return workspace_run(arguments, **kwargs)

        with mock.patch("atrinik_workspace.workspace.run", side_effect=fail_collector):
            with self.assertRaisesRegex(WorkspaceError, "collector failed"):
                self.workspace._collect_content(
                    root,
                    {"content": self.workspace.paths.repositories / "content"},
                )

        self.assertEqual((output / "sentinel").read_text(encoding="utf-8"), "last good\n")

    def test_exclusive_lock_rejects_concurrent_nonblocking_user(self) -> None:
        lock = self.workspace.paths.builds / "locks" / "test.lock"
        with exclusive_lock(lock, "test resource"):
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(lock, "test resource", nonblocking=True):
                    self.fail("concurrent lock unexpectedly succeeded")

    def test_server_runtime_paths_are_isolated_by_state(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        binary = root / "build" / "server"
        binary.mkdir(parents=True)
        for name in ("atrinik-server", "libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            root / "runtime" / "content" / "lib",
            root / "runtime" / "content" / "maps",
            root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        state_one = self.root / "state-one"
        state_two = self.root / "state-two"
        state_one.mkdir()
        state_two.mkdir()

        first = self.workspace._prepare_server_runtime(
            root, {"server": source}, state_one, "one"
        )
        second = self.workspace._prepare_server_runtime(
            root, {"server": source}, state_two, "two"
        )

        self.assertNotEqual(first, second)
        self.assertTrue((first / "data").is_symlink())
        self.assertEqual((second / "data").resolve(), state_two)

    def test_state_initializes_once_and_reuses_it(self) -> None:
        server = self.workspace.paths.repositories / "server"
        first = self.workspace.state_path("default", server)
        (first / "accounts").mkdir()
        second = self.workspace.state_path("default", server)
        self.assertEqual(first, second)
        self.assertTrue((second / "accounts").is_dir())

    def test_state_add_refuses_malformed_existing_directory(self) -> None:
        malformed = self.root / "valuable"
        malformed.mkdir()
        (malformed / "unrelated").write_text("keep\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "lacks required file"):
            self.workspace.state_add("bad", malformed)
        self.assertEqual((malformed / "unrelated").read_text(), "keep\n")

    def test_redacts_join_passwords(self) -> None:
        displayed = display_arguments(
            ["server", "--join_password=secret", "--join-password", "also-secret"]
        )
        self.assertNotIn("secret", displayed)
        self.assertIn("<redacted>", displayed)


if __name__ == "__main__":
    unittest.main()
