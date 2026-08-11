from __future__ import annotations

import copy
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import stat
import subprocess
import tempfile
import threading
import time
import unittest
from unittest import mock

from atrinik_workspace.migration import rename_no_replace as real_rename_no_replace
from atrinik_workspace.launch_identity import client_launch_label
from atrinik_workspace.model import (
    MANAGED_MARKER,
    WorkspaceError,
    atomic_json,
    load_json,
    managed_directory,
    managed_reset,
    profile_key,
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
                    "branch": "main",
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
        command("git", "init", "-b", "main", cwd=seed)
        command("git", "config", "user.name", "Tests", cwd=seed)
        command("git", "config", "user.email", "tests@example.invalid", cwd=seed)
        (seed / "README").write_text(f"{name}\n", encoding="utf-8")
        if name == "resources":
            (seed / "runtime-paths.txt").write_text("paintings\n", encoding="utf-8")
            (seed / "paintings").mkdir()
            (seed / "paintings" / "scene.jpg").write_text(
                "resource\n", encoding="utf-8"
            )
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
        command("git", "push", "-u", "origin", "main", cwd=seed)
        command("git", "symbolic-ref", "HEAD", "refs/heads/main", cwd=origin)
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

    def scenario_resolved_fixture(self) -> dict[str, dict[str, object]]:
        resolved: dict[str, dict[str, object]] = {}
        for component in ("server", "content", "resources", "libatrinik", "protocol"):
            path = self.workspace.paths.repositories / component
            provider = self.workspace.manifest.by_name[component]
            resolved[component] = {
                "path": str(path),
                "checkout_path": str(path),
                "checkout": component,
                "repository": provider.repository,
                "branch": provider.branch,
                "source": ".",
                "head": command("git", "rev-parse", "HEAD", cwd=path),
                "dirty": False,
            }
        return resolved

    @staticmethod
    def make_region_map_cache(root: Path) -> Path:
        output = root / "runtime" / "client-maps"
        output.mkdir(parents=True)
        (output / "incuna_-1.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        (output / "incuna_-1.def").write_text(
            "pixel_size 4\n", encoding="utf-8"
        )
        atomic_json(
            output / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "region-map-cache"},
        )
        return output

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

    def test_initialize_is_idempotent_and_preserves_existing_heads(self) -> None:
        before = {
            name: command(
                "git",
                "rev-parse",
                "HEAD",
                cwd=self.workspace.paths.repositories / name,
            )
            for name, _ in COMPONENTS
        }

        self.workspace.initialize(None, jobs=2)
        self.workspace.initialize(None, jobs=4)

        self.assertEqual(
            before,
            {
                name: command(
                    "git",
                    "rev-parse",
                    "HEAD",
                    cwd=self.workspace.paths.repositories / name,
                )
                for name, _ in COMPONENTS
            },
        )

    def test_initialize_serializes_concurrent_invocations(self) -> None:
        other = Workspace(self.wrapper)

        with ThreadPoolExecutor(max_workers=2) as executor:
            futures = [
                executor.submit(workspace.initialize, None, 2)
                for workspace in (self.workspace, other)
            ]
            for future in futures:
                future.result(timeout=10)

        self.assertTrue((self.workspace.paths.repositories / "client" / ".git").exists())

    def test_failed_clone_does_not_strand_destination(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)

        def fail_clone(arguments: list[str], **kwargs: object) -> str:
            if arguments[:2] == ["git", "clone"]:
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

    def test_clone_destination_race_never_replaces_raced_in_path(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)

        def race(temporary: Path, target: Path) -> None:
            target.mkdir()
            (target / "sentinel").write_text("preserve\n", encoding="utf-8")
            real_rename_no_replace(temporary, target)

        with mock.patch.object(
            self.workspace,
            "_component_clone_url",
            return_value=str(self.origins["client"]),
        ), mock.patch(
            "atrinik_workspace.workspace.rename_no_replace", side_effect=race
        ):
            with self.assertRaisesRegex(WorkspaceError, "destination appeared"):
                self.workspace._ensure_repository(
                    self.workspace._component("client")
                )

        self.assertEqual(
            (destination / "sentinel").read_text(encoding="utf-8"),
            "preserve\n",
        )
        self.assertEqual(
            list(self.workspace.paths.repositories.glob(".atrinik-clone-client-*")), []
        )

    def test_clone_transport_follows_wrapper_remote(self) -> None:
        command("git", "init", cwd=self.wrapper)
        command(
            "git",
            "remote",
            "add",
            "origin",
            "git@github.com:atrinik/atrinik.git",
            cwd=self.wrapper,
        )

        url = self.workspace._component_clone_url(self.workspace._component("client"))

        self.assertEqual(url, "git@github.com:atrinik/client.git")

    def test_clone_transport_defaults_to_public_https(self) -> None:
        url = self.workspace._component_clone_url(self.workspace._component("client"))

        self.assertEqual(url, "https://github.com/atrinik/client.git")

    def test_initialize_preserves_broken_symlink_at_component_path(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)
        destination.symlink_to(self.root / "missing-checkout", target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._ensure_repository(self.workspace._component("client"))

        self.assertTrue(destination.is_symlink())

    def test_sync_rejects_checkout_symlink_to_external_git_root(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        external = self.root / "external-client"
        destination.rename(external)
        destination.symlink_to(external, target_is_directory=True)
        before = command("git", "rev-parse", "HEAD", cwd=external)

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace.sync(["client"], "none")

        self.assertTrue(destination.is_symlink())
        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=external), before)

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

    def test_worktree_sync_excludes_protected_paths_before_dirty_check(self) -> None:
        repository = self.workspace.paths.repositories / "content"
        protected = self.workspace.paths.worktrees / "content" / "classic-maps"
        ordinary = self.workspace.paths.worktrees / "content" / "main-maps"
        records = [
            {"worktree": str(repository), "branch": "refs/heads/main"},
            {"worktree": str(protected), "branch": "refs/heads/classic-maps"},
            {"worktree": str(ordinary), "branch": "refs/heads/main-maps"},
        ]

        with (
            mock.patch(
                "atrinik_workspace.workspace._worktree_records",
                return_value=records,
            ),
            mock.patch(
                "atrinik_workspace.workspace._is_clean",
                side_effect=lambda path: path.resolve() != protected.resolve(),
            ) as clean,
        ):
            candidates, skipped = self.workspace._component_worktrees(
                repository, {protected.resolve()}
            )

        self.assertEqual(candidates, [ordinary.resolve()])
        self.assertEqual(skipped, [protected.resolve()])
        clean.assert_called_once_with(ordinary.resolve())

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

    def test_profile_rejects_symlinked_checkout_path(self) -> None:
        checkout = self.workspace.paths.repositories / "content"
        link = self.root / "content-link"
        link.symlink_to(checkout, target_is_directory=True)
        self.workspace.create_profile("review")

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace.set_profile("review", "content", "path", str(link))

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

    def test_integrated_classic_build_requires_one_complete_monorepo(self) -> None:
        checkout = self.root / "classic"
        checkout.mkdir()
        (checkout / "CMakeLists.txt").write_text("project(classic)\n", encoding="utf-8")
        selected = {}
        for role in ("client", "server", "protocol", "libatrinik"):
            selected[role] = checkout / role
            selected[role].mkdir()

        self.assertTrue(
            self.workspace._uses_integrated_classic_build(
                ["client", "server"], selected
            )
        )
        self.assertFalse(
            self.workspace._uses_integrated_classic_build(
                ["protocol", "libatrinik", "client"], selected
            )
        )
        selected["server"] = self.root / "other" / "server"
        self.assertFalse(
            self.workspace._uses_integrated_classic_build(
                ["protocol", "libatrinik", "client", "server"], selected
            )
        )

    def test_integrated_classic_build_creates_one_nested_source_graph(self) -> None:
        checkout = self.root / "classic"
        for role in ("client", "server", "protocol", "libatrinik"):
            (checkout / role).mkdir(parents=True)
            (checkout / role / "README").write_text(role + "\n", encoding="utf-8")
        (checkout / "CMakeLists.txt").write_text(
            "project(classic)\n", encoding="utf-8"
        )
        (checkout / "server" / "install_data").mkdir()
        sound = self.root / "sound"
        sound.mkdir()
        selected = {
            role: checkout / role
            for role in ("client", "server", "protocol", "libatrinik")
        }
        selected["sound"] = sound
        root = self.workspace.paths.builds / "profiles" / "classic-test"
        root.mkdir(parents=True)
        (root / "runtime" / "content").mkdir(parents=True)
        (root / "runtime" / "resources").mkdir()

        with mock.patch.object(self.workspace, "_cmake") as cmake:
            self.workspace._build_integrated_classic(root, selected, tests=True)

        cmake.assert_called_once_with(
            root / "sources" / "integrated",
            root / "build" / "integrated",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            True,
        )
        self.assertEqual(
            (root / "sources" / "integrated" / "client" / "sound").resolve(),
            sound,
        )
        self.assertEqual(
            (
                root
                / "sources"
                / "integrated"
                / "server"
                / "runtime"
                / "content"
            ).resolve(),
            root / "runtime" / "content",
        )
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "server"),
            root / "build" / "integrated" / "server",
        )

    def test_paired_classic_build_falls_back_without_shared_role_builds(self) -> None:
        selected = {
            role: self.workspace.paths.repositories / role
            for role in ("client", "server", "protocol", "libatrinik")
        }
        with (
            mock.patch.object(
                self.workspace, "_profile_build_key", return_value="fallback"
            ),
            mock.patch.object(self.workspace, "_refresh_build_metadata"),
            mock.patch.object(self.workspace, "_collect_content"),
            mock.patch.object(self.workspace, "_stage_resources"),
            mock.patch.object(self.workspace, "_build_protocol") as build_protocol,
            mock.patch.object(self.workspace, "_build_library") as build_library,
            mock.patch.object(self.workspace, "_build_client") as build_client,
            mock.patch.object(self.workspace, "_build_server") as build_server,
            mock.patch.object(self.workspace, "_generate_region_maps"),
        ):
            self.workspace._build_resolved(
                "topology", "default", False, ["client", "server"], selected
            )

        build_protocol.assert_not_called()
        build_library.assert_not_called()
        build_client.assert_called_once()
        build_server.assert_called_once()

    def test_classic_binary_directory_tracks_last_successful_graph(self) -> None:
        root = self.workspace.paths.builds / "profiles" / "classic-test"
        (root / "build").mkdir(parents=True)

        self.assertEqual(
            self.workspace._classic_binary_directory(root, "client"),
            root / "build" / "client",
        )
        self.workspace._record_classic_graph(
            root, {"client", "server"}, "integrated"
        )
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "client"),
            root / "build" / "integrated" / "client",
        )
        self.workspace._record_classic_graph(root, {"client"}, "standalone")
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "client"),
            root / "build" / "client",
        )
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "server"),
            root / "build" / "integrated" / "server",
        )

    def test_profile_schema_namespace_leaves_old_partial_build_inert(self) -> None:
        selected = {
            "resources": self.workspace.paths.repositories / "resources"
        }
        old_key = profile_key(selected)
        old_root = (
            self.workspace.paths.builds / "profiles" / f"default-{old_key}"
        )
        managed_directory(
            old_root,
            self.workspace.paths.builds,
            f"profile:default:{old_key}",
        )
        sentinel = old_root / "historical-output.bin"
        sentinel.write_bytes(b"historical build output\x00\n")

        new_root = self.workspace._build_resolved(
            "resources", "default", False, ["resources"], selected
        )

        self.assertNotEqual(new_root, old_root)
        self.assertEqual(sentinel.read_bytes(), b"historical build output\x00\n")
        self.assertTrue(new_root.is_dir())

    def test_profile_build_key_names_repository_and_branch_coordinates(self) -> None:
        selected = {"server": self.workspace.paths.repositories / "server"}
        with mock.patch(
            "atrinik_workspace.workspace.profile_key", return_value="key"
        ) as make_key:
            self.assertEqual(
                self.workspace._profile_build_key("default", selected), "key"
            )

        namespace = make_key.call_args.kwargs["namespace"]
        self.assertIn(
            "server=server@atrinik/server@main@server:.", namespace
        )

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

    def make_worker_source(self) -> Path:
        source = self.root / "worker-source"
        source.mkdir()
        (source / "package.json").write_text(
            json.dumps(
                {
                    "name": "worker-test",
                    "version": "1.0.0",
                    "scripts": {"check": "node check.js"},
                    "dependencies": {"alpha": "1.0.0"},
                    "devDependencies": {"@scope/beta": "2.0.0"},
                }
            ),
            encoding="utf-8",
        )
        (source / "package-lock.json").write_text(
            json.dumps(
                {
                    "name": "worker-test",
                    "version": "1.0.0",
                    "lockfileVersion": 3,
                    "packages": {},
                }
            ),
            encoding="utf-8",
        )
        (source / "worker.ts").write_text("export const value = 1;\n", encoding="utf-8")
        return source

    @staticmethod
    def fake_worker_run(
        installs: list[Path], versions: dict[str, str], install_lock: threading.Lock
    ):
        def invoke(
            arguments: list[str],
            *,
            cwd: Path | None = None,
            capture: bool = False,
            env: dict[str, str] | None = None,
            **_kwargs: object,
        ) -> str:
            if arguments == ["node", "--version"]:
                return versions["node"]
            if arguments == ["npm", "--version"]:
                return versions["npm"]
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps(
                    {
                        "cache": (env or {}).get("npm_config_cache"),
                        "ignore-scripts": False,
                    }
                )
            if arguments == ["npm", "ci"]:
                assert cwd is not None
                with install_lock:
                    installs.append(cwd)
                modules = cwd / "node_modules"
                (modules / "alpha").mkdir(parents=True)
                (modules / "@scope" / "beta").mkdir(parents=True)
                (modules / ".package-lock.json").write_text(
                    json.dumps(
                        {
                            "lockfileVersion": 3,
                            "packages": {
                                "node_modules/alpha": {},
                                "node_modules/@scope/beta": {},
                            },
                        }
                    )
                    + "\n",
                    encoding="utf-8",
                )
                return ""
            raise AssertionError(f"unexpected command: {arguments}")

        return invoke

    def test_worker_dependencies_reuse_exact_inputs_and_rebuild_corruption(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(installs, versions, threading.Lock()),
        ):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            second = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(first[3])
            self.assertTrue(second[3])
            self.assertEqual(first[1], second[1])
            self.assertEqual(len(installs), 1)

            (source / "worker.ts").write_text(
                "export const value = 2;\n", encoding="utf-8"
            )
            application_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertTrue(application_changed[3])
            self.assertEqual(application_changed[1], first[1])
            self.assertEqual(len(installs), 1)

            (application_changed[0] / "alpha").rename(
                application_changed[0] / "alpha-corrupt"
            )
            rebuilt = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(rebuilt[3])
            self.assertEqual(rebuilt[1], first[1])
            self.assertEqual(len(installs), 2)

            versions["npm"] = "11.1.0"
            invalidated = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(invalidated[1], first[1])
            self.assertEqual(len(installs), 3)

            (source / ".npmrc").write_text("strict-peer-deps=true\n", encoding="utf-8")
            config_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(config_changed[1], invalidated[1])
            self.assertEqual(len(installs), 4)

            (source / "package-lock.json").write_text(
                '{"lockfileVersion":3,"changed":true}\n', encoding="utf-8"
            )
            lock_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(lock_changed[1], config_changed[1])
            self.assertEqual(len(installs), 5)

    def test_worker_root_lifecycle_scripts_key_complete_source(self) -> None:
        source = self.make_worker_source()
        package = json.loads((source / "package.json").read_text(encoding="utf-8"))
        package["scripts"]["postinstall"] = "node worker.ts"
        (source / "package.json").write_text(json.dumps(package), encoding="utf-8")
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(installs, versions, threading.Lock()),
        ):
            environment = {"PATH": "/bin", "BUILD_MODE": "one"}
            environment["npm_config_cache"] = "/cache"
            first = self.workspace._worker_dependency_inputs(source, environment)
            (source / "worker.ts").write_text(
                "export const value = 2;\n", encoding="utf-8"
            )
            second = self.workspace._worker_dependency_inputs(source, environment)
            changed_environment = dict(environment, BUILD_MODE="two")
            third = self.workspace._worker_dependency_inputs(
                source, changed_environment
            )
        self.assertEqual(first["root_lifecycle_scripts"], ["postinstall"])
        self.assertNotEqual(
            first["root_lifecycle_source_sha256"],
            second["root_lifecycle_source_sha256"],
        )
        self.assertNotEqual(
            second["environment_sha256"], third["environment_sha256"]
        )

    def test_worker_dependency_concurrency_installs_once(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        install_lock = threading.Lock()
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(installs, versions, install_lock),
        ):
            with ThreadPoolExecutor(max_workers=2) as executor:
                results = list(
                    executor.map(
                        lambda _value: self.workspace._worker_dependencies(
                            source, {"PATH": "/bin"}
                        ),
                        range(2),
                    )
                )
        self.assertEqual(len(installs), 1)
        self.assertEqual(results[0][1], results[1][1])
        self.assertEqual(sorted(result[3] for result in results), [False, True])

    def test_worker_view_reuses_application_and_isolates_dependencies(self) -> None:
        source = self.make_worker_source()
        dependencies = self.root / "cached-node-modules"
        (dependencies / "alpha").mkdir(parents=True)
        (dependencies / "@scope" / "beta").mkdir(parents=True)
        hidden = dependencies / ".package-lock.json"
        hidden.write_text(
            json.dumps(
                {
                    "lockfileVersion": 3,
                    "packages": {
                        "node_modules/alpha": {},
                        "node_modules/@scope/beta": {},
                    },
                }
            )
            + "\n",
            encoding="utf-8",
        )
        hidden_digest = hashlib.sha256(hidden.read_bytes()).hexdigest()
        metadata = {"node_modules_lock_sha256": hidden_digest}
        root = self.workspace.paths.builds / "profiles" / "worker-test"
        managed_directory(root, self.workspace.paths.builds, "worker-test")

        first = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        second = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(first[1])
        self.assertTrue(second[1])
        (second[0] / "node_modules" / "alpha" / "local").write_text(
            "profile only\n", encoding="utf-8"
        )
        self.assertFalse((dependencies / "alpha" / "local").exists())

        (source / "worker.ts").write_text(
            "export const value = 2;\n", encoding="utf-8"
        )
        changed = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(changed[1])
        self.assertEqual(
            (changed[0] / "worker.ts").read_text(encoding="utf-8"),
            "export const value = 2;\n",
        )
        self.assertFalse((changed[0] / "node_modules" / "alpha" / "local").exists())

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
        self.assertTrue((output / "paintings" / "scene.jpg").is_file())
        self.assertFalse((output / "paintings" / "scene.jpg").is_symlink())
        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            "resource\n",
        )
        self.assertFalse((output / "README").exists())

    def test_resource_view_ignores_untracked_files(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / "paintings" / "private.txt").write_text(
            "do not serve\n", encoding="utf-8"
        )
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        output = self.workspace._stage_resources(root, {"resources": source})

        self.assertFalse((output / "paintings" / "private.txt").exists())

    def test_resource_view_rejects_unsafe_manifest_path(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / "runtime-paths.txt").write_text("../outside\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        with self.assertRaisesRegex(WorkspaceError, "invalid resource runtime path"):
            self.workspace._stage_resources(root, {"resources": source})

    def test_resource_view_failure_preserves_previous_output(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        (source / "runtime-paths.txt").write_text("../outside\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "invalid resource runtime path"):
            self.workspace._stage_resources(root, {"resources": source})

        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            "resource\n",
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

    def test_region_maps_are_atomic_cached_and_keyed_by_clean_inputs(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: add runtime inputs", cwd=source)

        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        binary = root / "build" / "server"
        binary.mkdir(parents=True)
        executable = binary / "atrinik-server"
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "from pathlib import Path\n"
            "import sys\n"
            "binary = Path(__file__).resolve()\n"
            "counter = binary.with_name('worldmaker-count')\n"
            "count = int(counter.read_text()) + 1 if counter.exists() else 1\n"
            "counter.write_text(str(count))\n"
            "assets = Path(next(arg.split('=', 1)[1] for arg in sys.argv "
            "if arg.startswith('--assetspath=')))\n"
            "output = assets / 'client-maps'\n"
            "output.mkdir(parents=True)\n"
            "(output / 'incuna_-1.png').write_bytes(b'\\x89PNG\\r\\n\\x1a\\n')\n"
            "(output / 'incuna_-1.def').write_text('pixel_size 4\\n')\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            root / "runtime" / "content" / "lib",
            root / "runtime" / "content" / "maps",
            root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        selected = {
            role: self.workspace.paths.repositories / role
            for role in ("server", "content", "resources")
        }

        output = self.workspace._generate_region_maps(root, "default", selected)
        self.workspace._generate_region_maps(root, "default", selected)

        self.assertEqual((binary / "worldmaker-count").read_text(), "1")
        self.assertTrue((output / "incuna_-1.png").is_file())
        previous = (output / "incuna_-1.def").read_text(encoding="utf-8")
        atomic_json(output / ".atrinik-region-maps.json", {"stale": True})

        def mutate_after_generation(
            arguments: list[str], **kwargs: object
        ) -> str:
            result = workspace_run(arguments, **kwargs)
            if Path(arguments[0]).resolve() == executable:
                (source / "README").write_text(
                    "dirty input\n", encoding="utf-8"
                )
            return result

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=mutate_after_generation
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed during generation"):
                self.workspace._generate_region_maps(root, "default", selected)
        self.assertEqual(
            (output / "incuna_-1.def").read_text(encoding="utf-8"), previous
        )

        executable.write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "command failed"):
            self.workspace._generate_region_maps(root, "default", selected)
        self.assertEqual(
            (output / "incuna_-1.def").read_text(encoding="utf-8"), previous
        )

    def test_region_map_inputs_ignore_unrelated_common_build_roles(self) -> None:
        profile = self.workspace._load_profile("default", require_file=False)
        required = self.workspace._dependency_roles(profile, {"server"})
        selected = {
            role: self.workspace.paths.repositories / role
            for role in required | {"client", "sound"}
        }

        inputs, cacheable = self.workspace._region_map_inputs("default", selected)

        self.assertTrue(cacheable)
        self.assertEqual(set(inputs["coordinates"]), required)

    def test_region_map_validation_rejects_malformed_outputs(self) -> None:
        output = self.root / "client-maps"

        def reset() -> None:
            if output.exists():
                shutil.rmtree(output)
            output.mkdir()

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_region_maps(output)

        target = self.root / "map-target"
        target.mkdir()
        output.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_region_maps(output)
        output.unlink()

        reset()
        with self.assertRaisesRegex(WorkspaceError, "lack required"):
            self.workspace._validate_region_maps(output)

        (output / "incuna_-1.def").write_text("pixel_size 4\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "pairs are incomplete"):
            self.workspace._validate_region_maps(output)

        reset()
        (output / "incuna_-1.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        with self.assertRaisesRegex(WorkspaceError, "missing definition"):
            self.workspace._validate_region_maps(output)

        (output / "incuna_-1.png").write_bytes(b"not a png")
        with self.assertRaisesRegex(WorkspaceError, "not a PNG"):
            self.workspace._validate_region_maps(output)

        (output / "incuna_-1.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        (output / "incuna_-1.def").write_bytes(b"\xff")
        with self.assertRaisesRegex(WorkspaceError, "not UTF-8"):
            self.workspace._validate_region_maps(output)

        reset()
        (output / "unexpected").mkdir()
        with self.assertRaisesRegex(WorkspaceError, "output is invalid"):
            self.workspace._validate_region_maps(output)

        reset()
        (output / "incuna_-1.png").write_bytes(b"")
        (output / "incuna_-1.def").write_text("pixel_size 4\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "is empty"):
            self.workspace._validate_region_maps(output)

        reset()
        entry = mock.Mock()
        entry.name = "vanished.png"
        entry.lstat.side_effect = OSError("vanished")
        with mock.patch("pathlib.Path.iterdir", return_value=iter([entry])):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect"):
                self.workspace._validate_region_maps(output)

    def test_region_map_cache_rejects_incomplete_or_invalid_metadata(self) -> None:
        output = self.make_region_map_cache(self.root)
        marker = output / MANAGED_MARKER
        metadata = output / ".atrinik-region-maps.json"
        inputs = {"schema_version": 1, "cacheable": True, "coordinates": {}}

        marker.unlink()
        self.assertFalse(
            self.workspace._region_map_cache_matches(output, inputs, True)
        )

        linked_output = self.root / "linked-maps"
        linked_output.symlink_to(output, target_is_directory=True)
        self.assertFalse(
            self.workspace._region_map_cache_matches(linked_output, inputs, True)
        )

        atomic_json(
            marker,
            {"schema_version": 1, "purpose": "region-map-cache"},
        )
        atomic_json(metadata, inputs)
        marker.write_text("{", encoding="utf-8")
        self.assertFalse(
            self.workspace._region_map_cache_matches(output, inputs, True)
        )

    def test_exclusive_lock_rejects_concurrent_nonblocking_user(self) -> None:
        lock = self.workspace.paths.builds / "locks" / "test.lock"
        with exclusive_lock(lock, "test resource"):
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(lock, "test resource", nonblocking=True):
                    self.fail("concurrent lock unexpectedly succeeded")

    def test_exclusive_lock_refuses_symlink(self) -> None:
        target = self.root / "valuable"
        target.write_text("preserve\n", encoding="utf-8")
        lock = self.workspace.paths.builds / "locks" / "test.lock"
        lock.parent.mkdir(parents=True)
        lock.symlink_to(target)

        with self.assertRaisesRegex(WorkspaceError, "cannot open test resource lock"):
            with exclusive_lock(lock, "test resource"):
                self.fail("symlinked lock unexpectedly opened")

        self.assertEqual(target.read_text(encoding="utf-8"), "preserve\n")

    def test_layout_sensitive_operations_wait_for_repository_lock(self) -> None:
        operations = (
            (
                "_create_worktree",
                lambda: self.workspace.create_worktree(
                    "client", "review", "feat/review", None, False
                ),
            ),
            (
                "_remove_worktree",
                lambda: self.workspace.remove_worktree("client", "review"),
            ),
            (
                "_create_profile",
                lambda: self.workspace.create_profile("review"),
            ),
            (
                "_set_profile",
                lambda: self.workspace.set_profile(
                    "review", "client", "primary"
                ),
            ),
            ("_build", lambda: self.workspace.build("client", "default", False)),
            (
                "_scenario_create",
                lambda: self.workspace.scenario_create("review", "default"),
            ),
            (
                "_scenario_reset",
                lambda: self.workspace.scenario_reset("review"),
            ),
            (
                "_topology_up",
                lambda: self.workspace.topology_up(
                    "review", "default", "review", ["server"], None
                ),
            ),
            (
                "_run_server",
                lambda: self.workspace.run_server(
                    "default", "review", 13327, [], True
                ),
            ),
        )
        lock = self.workspace.paths.workspace / "repository-layout.lock"
        for private_name, invoke in operations:
            with self.subTest(operation=private_name):
                with mock.patch.object(self.workspace, private_name) as operation:
                    with ThreadPoolExecutor(max_workers=1) as executor:
                        with exclusive_lock(lock, "repository layout"):
                            future = executor.submit(invoke)
                            time.sleep(0.05)
                            self.assertFalse(future.done())
                            operation.assert_not_called()
                        future.result(timeout=2)
                    operation.assert_called_once()

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
        self.make_region_map_cache(root)
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
        self.assertTrue((first / "assets" / "data").is_dir())
        self.assertFalse((first / "assets" / "data").is_symlink())
        self.assertNotEqual(first / "assets", second / "assets")
        staged_maps = first / "assets" / "client-maps"
        self.assertTrue((staged_maps / "incuna_-1.png").is_file())
        self.assertFalse(staged_maps.is_symlink())

        generated = first / "assets" / "data" / "listing.txt"
        generated.write_text("generated\n", encoding="utf-8")
        repeated = self.workspace._prepare_server_runtime(
            root, {"server": source}, state_one, "one"
        )
        self.assertEqual(repeated, first)
        self.assertFalse(generated.exists())

    def test_asset_staging_directory_rejects_invalid_nodes(self) -> None:
        missing = self.root / "missing-assets"
        self.workspace._prepare_asset_staging_directory(missing)
        self.assertTrue(missing.is_dir())

        invalid_file = self.root / "asset-file"
        invalid_file.write_text("invalid\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "asset staging path is invalid"):
            self.workspace._prepare_asset_staging_directory(invalid_file)

        target = self.root / "asset-target"
        target.mkdir()
        invalid_link = self.root / "asset-link"
        invalid_link.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "asset staging path is invalid"):
            self.workspace._prepare_asset_staging_directory(invalid_link)

    def test_topology_summary_uses_complete_profile_build_roles(self) -> None:
        summary = self.workspace.topology_summary(
            "default", "default", ["client"]
        )

        self.assertEqual(summary["services"], ["client"])
        self.assertIsNone(summary["state"])
        self.assertEqual(
            set(summary["dependencies"]),
            {
                "client",
                "server",
                "sound",
                "content",
                "resources",
                "libatrinik",
                "protocol",
            },
        )
        self.assertEqual(
            set(summary["components"]),
            {
                "client",
                "server",
                "sound",
                "content",
                "resources",
                "libatrinik",
                "protocol",
            },
        )
        self.assertIn("default-", summary["build_root"])

    def test_supervised_topology_lifecycle_and_logs(self) -> None:
        build_root = self.workspace.paths.builds / "fake-topology"
        executable = build_root / "build" / "client" / "atrinik"
        executable.parent.mkdir(parents=True)
        (build_root / "sources" / "client").mkdir(parents=True)
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import os, sys, time\n"
            "print('client ready', flush=True)\n"
            "print('arguments=' + repr(sys.argv[1:]), flush=True)\n"
            "print('config=' + os.environ['ATRINIK_CONFIG_DIR'], flush=True)\n"
            "print('launch=' + os.environ['ATRINIK_LAUNCH_LABEL'], flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)

        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ) as build_resolved,
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            status = self.workspace.topology_up(
                "review", "default", "default", ["client"]
            )
        self.assertEqual(build_resolved.call_args.args[3], ["client"])
        try:
            self.assertTrue(status["supervisor"]["running"])
            self.assertTrue(status["services"]["client"]["running"])
            self.assertEqual(
                Path(status["services"]["client"]["cwd"]),
                self.workspace.paths.topologies / "review" / "client-runtime",
            )
            with (
                mock.patch.object(
                    self.workspace, "_build_resolved", return_value=build_root
                ),
                mock.patch.object(self.workspace, "_require_client_display"),
            ):
                second = self.workspace.topology_up(
                    "review-two", "default", "default", ["client"]
                )
            self.assertNotEqual(
                status["services"]["client"]["cwd"],
                second["services"]["client"]["cwd"],
            )
            with self.assertRaisesRegex(WorkspaceError, "already running"):
                with (
                    mock.patch.object(
                        self.workspace, "_build_resolved", return_value=build_root
                    ),
                    mock.patch.object(self.workspace, "_require_client_display"),
                ):
                    self.workspace.topology_up(
                        "review", "default", "default", ["client"]
                    )

            deadline = time.monotonic() + 5
            log = self.workspace.paths.topologies / "review" / "client.log"
            while time.monotonic() < deadline and (
                not log.is_file() or "client ready" not in log.read_text()
            ):
                time.sleep(0.05)
            self.assertIn("client ready", log.read_text())
            self.assertIn(
                str(self.workspace.paths.topologies / "review" / "client-config"),
                log.read_text(),
            )
            self.assertIn("launch=topology review - profile default", log.read_text())
            persisted_spec = (
                self.workspace.paths.topologies / "review" / "spec.json"
            ).read_text()
            self.assertNotIn("ATRINIK_LAUNCH_LABEL", persisted_spec)
            self.assertNotIn("topology review - profile default", persisted_spec)

            second_log = self.workspace.paths.topologies / "review-two" / "client.log"
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and (
                not second_log.is_file() or "launch=" not in second_log.read_text()
            ):
                time.sleep(0.05)
            self.assertIn(
                "launch=topology review-two - profile default",
                second_log.read_text(),
            )

            with mock.patch("builtins.print") as output:
                self.workspace.topology_logs("review", "client", 10, False)
            self.assertIn("client ready", "".join(call.args[0] for call in output.call_args_list))
        finally:
            try:
                if self.workspace.topology_status("review-two")["supervisor"][
                    "running"
                ]:
                    self.workspace.topology_down("review-two", timeout=5)
            except WorkspaceError:
                pass
            if self.workspace.topology_status("review")["supervisor"]["running"]:
                self.workspace.topology_down("review", timeout=5)

        stopped = self.workspace.topology_status("review")
        self.assertFalse(stopped["supervisor"]["running"])
        self.assertFalse(stopped["services"]["client"]["running"])

    def test_supervised_pair_pins_client_and_holds_state_lock_until_down(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        build_root = self.workspace.paths.builds / "fake-server-topology"
        binary = build_root / "build" / "server"
        binary.mkdir(parents=True)
        executable = binary / "atrinik-server"
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import sys, time\n"
            f"print('QUIC certificate SHA-256: {'a' * 64}', flush=True)\n"
            "print('Server ready. Waiting for connections...', flush=True)\n"
            "print(repr(sys.argv[1:]), flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            build_root / "runtime" / "content" / "lib",
            build_root / "runtime" / "content" / "maps",
            build_root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        atomic_json(
            build_root / "runtime" / "content" / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        atomic_json(
            build_root / "runtime" / "resources" / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "resource-view"},
        )
        self.make_region_map_cache(build_root)
        client = build_root / "build" / "client" / "atrinik"
        client.parent.mkdir(parents=True)
        (build_root / "sources" / "client").mkdir(parents=True)
        client.write_text(
            "#!/usr/bin/env python3\n"
            "import os, sys, time\n"
            "print(repr(sys.argv[1:]), flush=True)\n"
            "print('config=' + os.environ['ATRINIK_CONFIG_DIR'], flush=True)\n"
            "print('launch=' + os.environ['ATRINIK_LAUNCH_LABEL'], flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        client.chmod(0o755)

        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ) as build_resolved,
            mock.patch.object(
                self.workspace, "_select_topology_port", return_value=17300
            ),
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            status = self.workspace.topology_up(
                "server-review", "default", "default", None, 17300
            )
        self.assertEqual(
            build_resolved.call_args.args[3],
            ["client", "server"],
        )
        self.assertTrue(status["ready"])
        self.assertEqual(status["endpoint"]["port"], 17300)
        self.assertEqual(status["endpoint"]["fingerprint"], "a" * 64)
        server_runtime = Path(status["services"]["server"]["cwd"])
        self.assertEqual(
            (server_runtime / "maps").resolve(),
            self.workspace.paths.topologies
            / "server-review"
            / "runtime"
            / "content"
            / "maps",
        )
        topology_maps = (
            self.workspace.paths.topologies
            / "server-review"
            / "runtime"
            / "client-maps"
        )
        self.assertTrue((topology_maps / "incuna_-1.png").is_file())
        staged_maps = server_runtime / "assets" / "client-maps"
        self.assertTrue((staged_maps / "incuna_-1.png").is_file())
        self.assertFalse(staged_maps.is_symlink())
        self.assertTrue(
            (build_root / "runtime" / "client-maps" / "incuna_-1.png").is_file()
        )
        self.assertEqual(
            Path(status["services"]["client"]["cwd"]),
            self.workspace.paths.topologies
            / "server-review"
            / "client-runtime",
        )
        client_log = self.workspace.paths.topologies / "server-review" / "client.log"
        server_log = self.workspace.paths.topologies / "server-review" / "server.log"
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and (
            not client_log.is_file() or "--server=" not in client_log.read_text()
        ):
            time.sleep(0.05)
        self.assertIn("'--port_quic=17300'", server_log.read_text())
        self.assertIn("'--port_mapping=off'", server_log.read_text())
        self.assertIn("'--stun_server=off'", server_log.read_text())
        self.assertIn(
            f"'--assetspath={server_runtime / 'assets'}'", server_log.read_text()
        )
        self.assertIn(
            f"'--server=127.0.0.1 17300 {'a' * 64}'", client_log.read_text()
        )
        self.assertIn("'--connect=127.0.0.1'", client_log.read_text())
        self.assertIn("'--stun_server=off'", client_log.read_text())
        self.assertIn("'--nometa'", client_log.read_text())
        self.assertIn(
            "launch=topology server-review - profile default",
            client_log.read_text(),
        )
        self.assertIn(
            str(
                self.workspace.paths.topologies
                / "server-review"
                / "client-config"
            ),
            client_log.read_text(),
        )
        state = self.workspace._state_location("default")
        try:
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    Path(f"{state}.lock"), "server state", nonblocking=True
                ):
                    self.fail("supervised state lock unexpectedly became available")

            supervisor = status["supervisor"]
            pidfd = os.pidfd_open(supervisor["pid"])
            try:
                signal.pidfd_send_signal(pidfd, signal.SIGKILL)
            finally:
                os.close(pidfd)
            deadline = time.monotonic() + 5
            while (
                time.monotonic() < deadline
                and self.workspace.topology_status("server-review")["supervisor"][
                    "running"
                ]
            ):
                time.sleep(0.05)
            orphaned = self.workspace.topology_status("server-review")
            self.assertFalse(orphaned["supervisor"]["running"])
            self.assertFalse(orphaned["ready"])
            self.assertTrue(orphaned["services"]["server"]["running"])
            with self.assertRaisesRegex(WorkspaceError, "already running"):
                self.workspace.topology_up(
                    "server-review", "default", "default", None, 17300
                )
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    Path(f"{state}.lock"), "server state", nonblocking=True
                ):
                    self.fail("orphaned server released its state lock")
            recovered = self.workspace.topology_down("server-review", timeout=5)
            self.assertFalse(
                any(service["running"] for service in recovered["services"].values())
            )
        finally:
            remaining = self.workspace.topology_status("server-review")
            if remaining["supervisor"]["running"] or any(
                service["running"] for service in remaining["services"].values()
            ):
                self.workspace.topology_down("server-review", timeout=5)

        with exclusive_lock(Path(f"{state}.lock"), "server state", nonblocking=True):
            pass

    def test_topology_port_selection_rejects_unavailable_port(self) -> None:
        candidate = mock.MagicMock()
        candidate.__enter__.return_value = candidate
        candidate.bind.side_effect = OSError("address in use")
        with mock.patch(
            "atrinik_workspace.workspace.socket.socket", return_value=candidate
        ):
            with self.assertRaisesRegex(WorkspaceError, "is unavailable"):
                self.workspace._select_topology_port(17300)

        candidate.reset_mock()
        candidate.bind.side_effect = None
        candidate.getsockname.return_value = ("0.0.0.0", 49152)
        with mock.patch(
            "atrinik_workspace.workspace.socket.socket", return_value=candidate
        ):
            self.assertEqual(self.workspace._select_topology_port(None), 49152)

        with self.assertRaisesRegex(WorkspaceError, "between 0 and 65535"):
            self.workspace._select_topology_port(True)

    def test_topology_status_rejects_boolean_process_id(self) -> None:
        root = self.workspace._topology_directory("invalid", create=True)
        atomic_json(
            root / "status.json",
            {
                "schema_version": 1,
                "name": "invalid",
                "profile": "default",
                "dependencies": [],
                "state": None,
                "build_root": "/tmp/build",
                "resolved": {},
                "endpoint": None,
                "ready": False,
                "started_at": "2026-08-06T00:00:00+00:00",
                "stopped_at": None,
                "supervisor": {"pid": True, "start_time": "1"},
                "services": {},
                "error": "test fixture",
            },
        )

        with self.assertRaisesRegex(WorkspaceError, "supervisor status is invalid"):
            self.workspace.topology_status("invalid")

    def test_topology_status_makes_pre_coordinate_records_inert(self) -> None:
        root = self.workspace._topology_directory("historical-coordinate", create=True)
        provider = self.workspace.manifest.provider("default", "server")
        checkout = self.workspace.paths.repositories / "server"
        base = {
            "schema_version": 1,
            "name": "historical-coordinate",
            "profile": "default",
            "stack": "default",
            "providers": {"server": "server"},
            "dependencies": ["server"],
            "state": "/tmp/state",
            "build_root": "/tmp/build",
            "resolved": {
                "server": {
                    "path": str(checkout),
                    "checkout_path": str(checkout),
                    "checkout": "server",
                    "source": ".",
                    "head": "a" * 40,
                    "dirty": False,
                }
            },
            "endpoint": None,
            "ready": False,
            "started_at": "2026-08-08T00:00:00+00:00",
            "stopped_at": None,
            "supervisor": {"pid": 999, "start_time": "1"},
            "services": {},
            "error": "historical fixture",
        }
        atomic_json(root / "status.json", base)

        with mock.patch(
            "atrinik_workspace.workspace.process_matches", return_value=False
        ):
            historical = self.workspace.topology_status("historical-coordinate")

        self.assertTrue(historical["inert_historical_record"])

        current = copy.deepcopy(base)
        current["resolved"]["server"]["repository"] = "atrinik/wrong"
        current["resolved"]["server"]["branch"] = provider.branch
        atomic_json(root / "status.json", current)
        with self.assertRaisesRegex(WorkspaceError, "component identity is invalid"):
            self.workspace.topology_status("historical-coordinate")

    def test_client_only_topology_rejects_server_port(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "requires the server"):
            self.workspace.topology_up(
                "client-only", "default", "default", ["client"], 17300
            )

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

    def test_scenario_lifecycle_owns_isolated_state_and_credentials(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ) as provision:
            with mock.patch("builtins.print") as output:
                created = self.workspace.scenario_create(
                    "issue-42", "default", "basic-player"
                )
            output.assert_not_called()

            self.assertEqual(created["state"], "scenario-issue-42")
            self.assertEqual(created["account"], "scenario1dd9ee81")
            self.assertEqual(created["character"], "Scenario 1dd9ee81")
            scenario_root = self.workspace.paths.scenarios / "issue-42"
            state = scenario_root / "state"
            self.assertEqual(
                Path(self.workspace.list_states()["scenario-issue-42"]), state
            )
            self.assertEqual(
                stat.S_IMODE((scenario_root / "password").stat().st_mode), 0o600
            )
            credentials = self.workspace.scenario_credentials("issue-42")
            self.assertEqual(credentials["account"], created["account"])
            self.assertEqual(credentials["character"], created["character"])
            self.assertTrue(credentials["password"])

            (state / "accounts").mkdir()
            reset = self.workspace.scenario_reset("issue-42")
            self.assertFalse((state / "accounts").exists())
            self.assertGreater(reset["provisioned_at"], created["provisioned_at"])

        self.assertEqual(provision.call_count, 2)

    def test_scenario_lifecycle_prepares_fresh_asset_staging(self) -> None:
        selected = {
            component: self.workspace.paths.repositories / component
            for component in ("server", "content", "resources", "libatrinik", "protocol")
        }
        source = selected["server"]
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")

        build_root = self.workspace.paths.builds / "profiles" / "scenario-assets"
        managed_directory(build_root, self.workspace.paths.builds, "test-profile")
        binary = build_root / "build" / "server"
        binary.mkdir(parents=True)
        for name in ("atrinik-server", "libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            build_root / "runtime" / "content" / "lib",
            build_root / "runtime" / "content" / "maps",
            build_root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        self.make_region_map_cache(build_root)

        staged_assets: list[Path] = []

        def provision(
            arguments: list[str], *, cwd: Path | None = None, **kwargs: object
        ) -> object:
            if "--provision_scenario" not in arguments:
                return workspace_run(arguments, cwd=cwd, **kwargs)
            assert cwd is not None
            assetspath = Path(
                next(
                    argument.split("=", 1)[1]
                    for argument in arguments
                    if argument.startswith("--assetspath=")
                )
            )
            self.assertEqual(assetspath, cwd / "assets")
            self.assertFalse(assetspath.is_symlink())
            self.assertTrue((assetspath / "data").is_dir())
            self.assertFalse((assetspath / "data").is_symlink())
            self.assertTrue((assetspath / "client-maps" / "incuna_-1.png").is_file())
            self.assertFalse((assetspath / "client-maps").is_symlink())
            self.assertFalse((assetspath / "data" / "previous-run").exists())
            self.assertFalse((cwd / "data" / "http").exists())
            (assetspath / "data" / "previous-run").write_text(
                "generated\n", encoding="utf-8"
            )
            staged_assets.append(assetspath)
            return None

        with (
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch("atrinik_workspace.workspace.run", side_effect=provision),
        ):
            created = self.workspace.scenario_create(
                "fresh-assets", "default", "basic-player"
            )
            self.assertEqual(
                self.workspace.scenario_show("fresh-assets")["state"],
                "scenario-fresh-assets",
            )
            reset = self.workspace.scenario_reset("fresh-assets")

        self.assertEqual(created["state"], "scenario-fresh-assets")
        self.assertEqual(reset["state"], "scenario-fresh-assets")
        self.assertEqual(len(staged_assets), 2)
        self.assertNotEqual(staged_assets[0], staged_assets[1])
        state = self.workspace.paths.scenarios / "fresh-assets" / "state"
        self.assertFalse((state / "assets").exists())
        self.assertFalse((state / "http").exists())

    def test_scenario_create_rolls_back_failed_provisioning(self) -> None:
        with mock.patch.object(
            self.workspace,
            "_scenario_provision_state",
            side_effect=WorkspaceError("provision failed"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "provision failed"):
                self.workspace.scenario_create("failed", "default")

        self.assertFalse((self.workspace.paths.scenarios / "failed").exists())
        self.assertNotIn("scenario-failed", self.workspace.list_states())

    def test_historical_default_scenario_is_inert_without_stack_identity(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("historical-default", "default")

        metadata_path = (
            self.workspace.paths.scenarios / "historical-default" / "scenario.json"
        )
        metadata = load_json(metadata_path)
        metadata["schema_version"] = 1
        del metadata["stack"]
        del metadata["providers"]
        atomic_json(metadata_path, metadata)

        with self.assertRaisesRegex(
            WorkspaceError,
            "historical scenario lacks immutable stack/provider identity and is inert",
        ):
            self.workspace.scenario_show("historical-default")

    def test_historical_scenario_is_inert_without_repository_identity(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("historical-coordinate", "default")

        metadata_path = (
            self.workspace.paths.scenarios
            / "historical-coordinate"
            / "scenario.json"
        )
        metadata = load_json(metadata_path)
        metadata["schema_version"] = 3
        for record in metadata["resolved"].values():
            del record["repository"]
            del record["branch"]
        atomic_json(metadata_path, metadata)

        with self.assertRaisesRegex(
            WorkspaceError,
            "historical scenario lacks immutable repository/branch identity and is inert",
        ):
            self.workspace.scenario_show("historical-coordinate")

    def test_scenario_audit_records_only_server_dependency_closure(self) -> None:
        required = {"server", "content", "resources", "libatrinik", "protocol"}
        selected = {
            component: self.workspace.paths.repositories / component
            for component in (*sorted(required), "client")
        }
        metadata = {
            "profile": "default",
            "state": "scenario-audit",
            "account": "scenarioaudit",
            "character": "Scenario Audit",
            "archetype": "human_male",
        }
        runtime = self.root / "scenario-runtime"
        runtime.mkdir()
        with (
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=self.root / "build"
            ),
            mock.patch.object(
                self.workspace, "_prepare_server_runtime", return_value=runtime
            ),
            mock.patch("atrinik_workspace.workspace.run"),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="a" * 40
            ),
            mock.patch(
                "atrinik_workspace.workspace._is_clean", return_value=True
            ),
        ):
            resolved = self.workspace._scenario_provision_state(
                metadata, self.root / "state", self.root / "password"
            )

        self.assertEqual(set(resolved), required)
        self.assertNotIn("client", resolved)

    def test_scenario_rejects_insecure_password_permissions(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("permissions", "default")

        password = self.workspace.paths.scenarios / "permissions" / "password"
        password.chmod(0o644)
        with self.assertRaisesRegex(WorkspaceError, "mode 0600"):
            self.workspace.scenario_show("permissions")

    def test_scenario_reset_refuses_locked_state(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("locked", "default")

        state = self.workspace.paths.scenarios / "locked" / "state"
        with exclusive_lock(Path(f"{state}.lock"), "test state"):
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                self.workspace.scenario_reset("locked")

    def test_foreground_client_pins_matching_server_state(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        certificate = (
            "-----BEGIN PRIVATE KEY-----\nignored\n-----END PRIVATE KEY-----\n"
            "-----BEGIN CERTIFICATE-----\nAQID\n-----END CERTIFICATE-----\n"
        )
        (state / "quic-identity.pem").write_text(certificate, encoding="ascii")

        build_root = self.root / "client-build"
        executable = build_root / "build" / "client" / "atrinik"
        executable.parent.mkdir(parents=True)
        executable.write_text("client\n", encoding="utf-8")
        (build_root / "sources" / "client").mkdir(parents=True)
        expected = "039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81"
        with (
            mock.patch.object(self.workspace, "build", return_value=build_root),
            mock.patch("builtins.print") as output,
            mock.patch("atrinik_workspace.workspace.run") as execute,
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            result = self.workspace.run_client(
                "default", "default", 1731, ["--fullscreen"], False
            )

        self.assertEqual(result, executable)
        rendered = "\n".join(str(call.args[0]) for call in output.call_args_list)
        self.assertIn(f"--server=127.0.0.1 1731 {expected}", rendered)
        self.assertIn("--stun_server=off", rendered)
        self.assertIn("--nometa", rendered)
        self.assertIn("--fullscreen", rendered)
        self.assertIn("launch label: profile default (direct run)", rendered)
        environment = execute.call_args.kwargs["env"]
        self.assertEqual(
            environment["ATRINIK_LAUNCH_LABEL"],
            "profile default (direct run)",
        )

    def test_client_launch_label_is_bounded(self) -> None:
        long_profile = "p" * 96
        with self.assertRaisesRegex(WorkspaceError, "launch label exceeds 96 bytes"):
            client_launch_label(long_profile)

    def test_foreground_client_requires_initialized_server_identity(self) -> None:
        server = self.workspace.paths.repositories / "server"
        self.workspace.state_path("default", server)
        with self.assertRaisesRegex(WorkspaceError, "start the matching server"):
            self.workspace.run_client("default", "default", 1730, [], True)

    def test_foreground_client_rejects_symlinked_server_identity(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        target = self.root / "identity.pem"
        target.write_text(
            "-----BEGIN CERTIFICATE-----\nAQID\n-----END CERTIFICATE-----\n",
            encoding="ascii",
        )
        (state / "quic-identity.pem").symlink_to(target)
        with self.assertRaisesRegex(WorkspaceError, "cannot open server QUIC identity"):
            self.workspace.run_client("default", "default", 1730, [], True)

    def test_foreground_server_keeps_local_defaults_with_extra_arguments(self) -> None:
        server = self.workspace.paths.repositories / "server"
        build_root = self.root / "server-build"
        runtime = self.root / "server-runtime"
        runtime.mkdir()
        executable = runtime / "atrinik-server"
        executable.write_text("server\n", encoding="utf-8")
        selected = {"server": server}
        with (
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(
                self.workspace, "_prepare_server_runtime", return_value=runtime
            ),
            mock.patch("builtins.print") as output,
        ):
            result = self.workspace.run_server(
                "default",
                "default",
                1731,
                ["--no_console", "--assetspath=/tmp/untrusted"],
                True,
            )

        self.assertEqual(result, executable)
        rendered = "\n".join(str(call.args[0]) for call in output.call_args_list)
        self.assertIn("--port_quic=1731", rendered)
        self.assertIn("--port_mapping=off", rendered)
        self.assertIn("--stun_server=off", rendered)
        self.assertIn("--no_console", rendered)
        self.assertLess(
            rendered.index("--assetspath=/tmp/untrusted"),
            rendered.index(f"--assetspath={runtime / 'assets'}"),
        )

    def test_foreground_launch_rejects_invalid_port(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "between 1 and 65535"):
            self.workspace.run_client("default", "default", True, [], True)

    def test_redacts_join_passwords(self) -> None:
        displayed = display_arguments(
            ["server", "--join_password=secret", "--join-password", "also-secret"]
        )
        self.assertNotIn("secret", displayed)
        self.assertIn("<redacted>", displayed)

    def test_operational_subprocess_output_uses_stderr(self) -> None:
        completed = mock.MagicMock(stdout="")
        with mock.patch(
            "atrinik_workspace.workspace.subprocess.run", return_value=completed
        ) as invoke:
            workspace_run(["tool"])

        self.assertIs(invoke.call_args.kwargs["stdout"], os.sys.stderr)


if __name__ == "__main__":
    unittest.main()
