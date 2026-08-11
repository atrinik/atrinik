from __future__ import annotations

import copy
from concurrent.futures import ThreadPoolExecutor
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

from atrinik_workspace import workspace as workspace_module
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
    WORKER_SOURCE_EXCLUSIONS,
    WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
    CONFIGURE_METADATA,
    RUNTIME_INPUT_METADATA,
    SOURCE_VIEW_METADATA,
    Workspace,
    _copy_regular_file as real_copy_regular_file,
    _copy_worker_source as real_copy_worker_source,
    _tree_digest,
    _remote_matches as real_remote_matches,
    display_arguments,
    exclusive_lock,
    remove_owned_tree,
    replace_directory as worker_replace_directory,
    replace_runtime_directory as workspace_replace_directory,
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

    @staticmethod
    def make_content_candidate(output: Path, commit: str, payload: str) -> None:
        (output / "lib").mkdir(parents=True)
        (output / "maps").mkdir()
        compatibility = output / "compatibility.json"
        compatibility.write_text(payload, encoding="utf-8")
        atomic_json(
            output / "manifest.json",
            {
                "schema_version": 2,
                "source": {
                    "repository": "atrinik/content",
                    "branch": "main",
                    "commit": commit,
                },
                "files": [
                    {
                        "path": "compatibility.json",
                        "sha256": hashlib.sha256(
                            compatibility.read_bytes()
                        ).hexdigest(),
                        "size": compatibility.stat().st_size,
                    }
                ],
            },
        )

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
        (source / "src" / "build").mkdir(parents=True)
        (source / "src" / "build" / "nested.ts").write_text(
            "export const nested = true;\n", encoding="utf-8"
        )
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
            if arguments == [
                "node",
                "-p",
                "JSON.stringify({platform:process.platform,arch:process.arch,"
                "versions:process.versions})",
            ]:
                return json.dumps(
                    {
                        "platform": versions.get("node_platform", "linux"),
                        "arch": versions.get("node_architecture", "x64"),
                        "versions": {"modules": "127", "napi": "10"},
                    }
                )
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps(
                    {
                        "cache": (env or {}).get("npm_config_cache"),
                        "ignore-scripts": False,
                    }
                )
            if arguments == ["npm", "ci"]:
                assert cwd is not None
                if (cwd / MANAGED_MARKER).exists() or (
                    cwd / MANAGED_MARKER
                ).is_symlink():
                    raise AssertionError("workspace metadata was exposed to npm")
                if not (cwd / "worker.ts").is_file():
                    raise AssertionError("npm lifecycle source was not staged")
                if (cwd / "worker.ts").stat().st_atime_ns != 0:
                    raise AssertionError(
                        "lifecycle source access time was not normalized"
                    )
                npmrc = cwd / ".npmrc"
                if npmrc.exists():
                    if (
                        npmrc.is_symlink()
                        or stat.S_IMODE(npmrc.stat().st_mode) != 0o600
                    ):
                        raise AssertionError("project npm configuration is not isolated")
                if not (cwd / "src" / "build" / "nested.ts").is_file():
                    raise AssertionError("nested generated-name source was omitted")
                with install_lock:
                    installs.append(cwd)
                modules = cwd / "node_modules"
                (modules / "alpha").mkdir(parents=True)
                (modules / "alpha" / "bin.js").write_text(
                    "console.log('alpha');\n", encoding="utf-8"
                )
                (modules / "@scope" / "beta").mkdir(parents=True)
                (modules / ".bin").mkdir()
                (modules / ".bin" / "alpha").symlink_to("../alpha/bin.js")
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

    def test_worker_dependency_publication_revalidates_after_rename(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}

        def mutate_before_verification(
            output: Path,
            staging: Path,
            backup_prefix: str,
            backup_parent: Path | None = None,
            verify_after_install: object = None,
        ) -> None:
            assert callable(verify_after_install)

            def corrupt_then_verify() -> None:
                (output / "node_modules" / "alpha" / "bin.js").write_text(
                    "post-rename corruption\n", encoding="utf-8"
                )
                verify_after_install()

            worker_replace_directory(
                output,
                staging,
                backup_prefix,
                backup_parent,
                corrupt_then_verify,
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run(
                    installs, versions, threading.Lock()
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace.replace_directory",
                side_effect=mutate_before_verification,
            ),
            self.assertRaisesRegex(WorkspaceError, "published Worker dependencies"),
        ):
            self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(len(installs), 1)
        entries = [
            path
            for path in (self.workspace.paths.builds / "worker-dependencies").iterdir()
            if path.name not in {".transactions", MANAGED_MARKER}
        ]
        self.assertEqual(entries, [])

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
            self.assertFalse((first[0].parent / "worker.ts").exists())

            (source / "worker.ts").write_text(
                "export const value = 2;\n", encoding="utf-8"
            )
            application_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(application_changed[3])
            self.assertNotEqual(application_changed[1], first[1])
            self.assertEqual(len(installs), 2)

            (application_changed[0] / "alpha").rename(
                application_changed[0] / "alpha-corrupt"
            )
            rebuilt = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(rebuilt[3])
            self.assertEqual(rebuilt[1], application_changed[1])
            self.assertEqual(len(installs), 3)

            versions["npm"] = "11.1.0"
            invalidated = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(invalidated[1], first[1])
            self.assertEqual(len(installs), 4)

            (source / ".npmrc").write_text("strict-peer-deps=true\n", encoding="utf-8")
            config_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(config_changed[1], invalidated[1])
            self.assertEqual(len(installs), 5)
            self.assertFalse((config_changed[0].parent / ".npmrc").exists())

            (source / "package-lock.json").write_text(
                '{"lockfileVersion":3,"changed":true}\n', encoding="utf-8"
            )
            lock_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(lock_changed[1], config_changed[1])
            self.assertEqual(len(installs), 6)

    def test_worker_dependency_keys_node_runtime_architecture(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {
            "node": "v22.0.0",
            "npm": "11.0.0",
            "node_architecture": "x64",
        }
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(
                installs, versions, threading.Lock()
            ),
        ):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            versions["node_architecture"] = "arm64"
            changed = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertNotEqual(first[1], changed[1])
        self.assertEqual(len(installs), 2)

    def test_worker_dependency_cache_preserves_unowned_entries(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            entry = first[0].parent
            for marker in (None, {"schema_version": 1, "purpose": "unrelated"}):
                with self.subTest(marker=marker):
                    shutil.rmtree(entry)
                    entry.mkdir()
                    valuable = entry / "valuable.txt"
                    valuable.write_text("preserve\n", encoding="utf-8")
                    if marker is not None:
                        atomic_json(entry / MANAGED_MARKER, marker)
                    with self.assertRaisesRegex(
                        WorkspaceError, "unmanaged|marker does not match"
                    ):
                        self.workspace._worker_dependencies(source, {"PATH": "/bin"})
                    self.assertEqual(valuable.read_text(encoding="utf-8"), "preserve\n")

    def test_worker_dependency_cache_authenticates_complete_tree(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            modules = first[0]
            (modules / "alpha" / "bin.js").write_text(
                "corrupt\n", encoding="utf-8"
            )
            rebuilt_content = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_content[3])
            self.assertEqual(len(installs), 2)

            entry_metadata_path = (
                rebuilt_content[0].parent / ".atrinik-worker-dependencies.json"
            )
            entry_metadata = load_json(entry_metadata_path)
            entry_metadata["node_modules_view_sha256"] = "0" * 64
            atomic_json(entry_metadata_path, entry_metadata)
            rebuilt_view_digest = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_view_digest[3])
            self.assertEqual(len(installs), 3)

            (modules / "unexpected.txt").write_text("unexpected\n", encoding="utf-8")
            rebuilt_addition = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_addition[3])
            self.assertEqual(len(installs), 4)

            (modules / "escape").symlink_to("../../outside")
            rebuilt_link = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_link[3])
            self.assertEqual(len(installs), 5)

    def test_worker_dependency_cache_authenticates_copied_metadata(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            installed = first[0] / "alpha" / "bin.js"
            status = installed.stat()
            os.utime(
                installed,
                ns=(status.st_atime_ns, status.st_mtime_ns + 1),
            )
            rebuilt = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(rebuilt[3])
            self.assertEqual(len(installs), 2)

            installed = rebuilt[0] / "alpha" / "bin.js"
            if hasattr(os, "setxattr"):
                try:
                    os.setxattr(installed, "user.atrinik-test", b"changed")
                except OSError:
                    return
                rebuilt_xattr = self.workspace._worker_dependencies(
                    source, {"PATH": "/bin"}
                )
                self.assertFalse(rebuilt_xattr[3])
                self.assertEqual(len(installs), 3)

    def test_worker_dependency_failed_rebuild_preserves_owned_cache(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        damaged = first[0] / "alpha" / "bin.js"
        damaged.write_text("valuable-corrupt-state\n", encoding="utf-8")

        def failing_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "ci"]:
                raise WorkspaceError("simulated install failure")
            return runner(arguments, **kwargs)

        with mock.patch("atrinik_workspace.workspace.run", side_effect=failing_run):
            with self.assertRaisesRegex(WorkspaceError, "simulated install failure"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(
            damaged.read_text(encoding="utf-8"), "valuable-corrupt-state\n"
        )

    def test_worker_dependency_recovers_interrupted_atomic_backup(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            entry = first[0].parent
            transaction = (
                entry.parent
                / ".transactions"
                / f"{first[1]}-backup-_interrupted"
            )
            entry.rename(transaction)
            recovered = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertTrue(recovered[3])
        self.assertEqual(len(installs), 1)
        self.assertTrue(entry.is_dir())
        self.assertFalse(transaction.exists())

    def test_worker_dependency_recovers_unmarked_install_staging(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            entry = first[0].parent
            staging = (
                entry.parent
                / ".transactions"
                / f"{first[1]}-staging-install"
            )
            shutil.rmtree(entry)
            staging.mkdir()
            (staging / "partial-install").write_text(
                "interrupted\n", encoding="utf-8"
            )
            recovered = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
        self.assertFalse(recovered[3])
        self.assertEqual(len(installs), 2)
        self.assertTrue(entry.is_dir())
        self.assertFalse(staging.exists())

    def test_worker_tree_digest_is_canonical_and_lifecycle_rejects_links(self) -> None:
        first = self.root / "tree-first"
        second = self.root / "tree-second"
        first.mkdir()
        second.mkdir()
        (first / "a").write_bytes(b"Xf\0b\0Y")
        (second / "a").write_bytes(b"X")
        (second / "b").write_bytes(b"Y")
        self.assertNotEqual(_tree_digest(first, set()), _tree_digest(second, set()))
        before = _tree_digest(first, set())
        (first / "a").chmod(0o755)
        self.assertNotEqual(before, _tree_digest(first, set()))
        before = _tree_digest(first, set(), copied_metadata=True)
        status = (first / "a").stat()
        os.utime(first / "a", ns=(status.st_atime_ns, status.st_mtime_ns + 1))
        self.assertNotEqual(
            before, _tree_digest(first, set(), copied_metadata=True)
        )
        source = self.make_worker_source()
        (source / "linked.ts").symlink_to("worker.ts")
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run([], versions, threading.Lock()),
        ):
            with self.assertRaisesRegex(WorkspaceError, "symbolic link"):
                self.workspace._worker_dependency_inputs(
                    source, {"PATH": "/bin", "npm_config_cache": "/cache"}
                )

    def test_worker_dependency_rejects_external_npm_configuration(self) -> None:
        source = self.make_worker_source()
        userconfig = self.root / "user.npmrc"
        userconfig.write_text(
            "//registry.example/:_authToken=first\n", encoding="utf-8"
        )
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run([], versions, threading.Lock())

        def configured_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps({"userconfig": str(userconfig)})
            return runner(arguments, **kwargs)

        environment = {"PATH": "/bin", "npm_config_cache": "/cache"}
        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=configured_run
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "external file-backed npm configuration"
            ):
                self.workspace._worker_dependency_inputs(source, environment)

    def test_worker_dependency_rejects_custom_npm_script_shell(self) -> None:
        source = self.make_worker_source()
        script_shell = self.root / "npm-script-shell"
        script_shell.write_text(
            "#!/bin/sh\nexec /bin/sh \"$@\"\n", encoding="utf-8"
        )
        script_shell.chmod(0o755)
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []
        runner = self.fake_worker_run(installs, versions, threading.Lock())

        def configured_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps({"script-shell": str(script_shell)})
            return runner(arguments, **kwargs)

        environment = {"PATH": "/bin", "npm_config_cache": "/cache"}
        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=configured_run
        ):
            for contents in (
                "#!/bin/sh\nexec /bin/sh \"$@\"\n",
                "#!/bin/sh\nexit 99\n",
            ):
                script_shell.write_text(contents, encoding="utf-8")
                with self.assertRaisesRegex(
                    WorkspaceError, "custom npm script-shell"
                ):
                    self.workspace._worker_dependencies(source, environment)
        self.assertEqual(installs, [])

    def test_worker_dependency_rejects_external_node_preload_options(self) -> None:
        source = self.make_worker_source()
        hook = self.root / "node-hook.cjs"
        environment = {
            "PATH": "/bin",
            "NODE_OPTIONS": f"--require={hook}",
        }
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=AssertionError("Node must not run with external preload code"),
        ):
            for contents in ("module.exports = 1;\n", "module.exports = 2;\n"):
                hook.write_text(contents, encoding="utf-8")
                with self.assertRaisesRegex(
                    WorkspaceError, "custom Node execution options"
                ):
                    self.workspace._worker_dependencies(source, environment)

        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []
        runner = self.fake_worker_run(installs, versions, threading.Lock())

        def configured_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps({"node-options": f"--require={hook}"})
            return runner(arguments, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=configured_run
        ):
            with self.assertRaisesRegex(WorkspaceError, "custom npm node-options"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(installs, [])

    def test_worker_dependency_authenticates_staged_project_npmrc(self) -> None:
        source = self.make_worker_source()
        (source / ".npmrc").write_text("strict-peer-deps=true\n", encoding="utf-8")
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []

        def corrupt_copy(*args: object, **kwargs: object) -> None:
            real_copy_regular_file(*args, **kwargs)
            destination = args[1]
            assert isinstance(destination, Path)
            self.assertEqual(stat.S_IMODE(destination.stat().st_mode), 0o600)
            destination.write_text("strict-peer-deps=false\n", encoding="utf-8")

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run(
                    installs, versions, threading.Lock()
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace._copy_regular_file",
                side_effect=corrupt_copy,
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "does not match its cache key"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(installs, [])

    def test_worker_dependency_authenticates_staged_source_snapshot(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []

        def corrupt_copy(*args: object, **kwargs: object) -> None:
            real_copy_worker_source(*args, **kwargs)
            destination = args[1]
            assert isinstance(destination, Path)
            (destination / "worker.ts").write_text(
                "mixed snapshot\n", encoding="utf-8"
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run(
                    installs, versions, threading.Lock()
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace._copy_worker_source",
                side_effect=corrupt_copy,
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "does not match its cache key"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(installs, [])

    def test_worker_dependency_consumer_holds_key_lock(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}

        def consume(_modules: Path, key: str, _metadata: dict[str, object]) -> str:
            lock = (
                self.workspace.paths.builds
                / "locks"
                / f"worker-dependencies-{key}.lock"
            )
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(lock, "competing cleanup", nonblocking=True):
                    self.fail("dependency lease was not held")
            return "consumed"

        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run([], versions, threading.Lock()),
        ):
            result = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}, consume
            )
        self.assertEqual(result[5], "consumed")

    def test_worker_dependency_timing_excludes_view_consumption(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        consumed: list[Path] = []
        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run([], versions, threading.Lock()),
            ),
            mock.patch(
                "atrinik_workspace.workspace.time.monotonic",
                side_effect=(10.0, 12.5),
            ),
        ):
            result = self.workspace._worker_dependencies(
                source,
                {"PATH": "/bin"},
                lambda modules, _key, _metadata: consumed.append(modules),
            )
        self.assertEqual(result[4], 2.5)
        self.assertEqual(consumed, [result[0]])

    def test_worker_dependency_rejects_embedded_install_path(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run([], versions, threading.Lock())

        def path_embedding_run(arguments: list[str], **kwargs: object) -> str:
            result = runner(arguments, **kwargs)
            if arguments == ["npm", "ci"]:
                cwd = kwargs["cwd"]
                assert isinstance(cwd, Path)
                (cwd / "node_modules" / "alpha" / "embedded").write_text(
                    str(cwd), encoding="utf-8"
                )
            return result

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=path_embedding_run
        ):
            with self.assertRaisesRegex(WorkspaceError, "embeds its install path"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})

    def test_worker_dependency_hides_reserved_metadata_from_lifecycle(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run([], versions, threading.Lock())

        def marker_creating_run(arguments: list[str], **kwargs: object) -> str:
            result = runner(arguments, **kwargs)
            if arguments == ["npm", "ci"]:
                cwd = kwargs["cwd"]
                assert isinstance(cwd, Path)
                atomic_json(cwd / MANAGED_MARKER, {"lifecycle": "unexpected"})
            return result

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=marker_creating_run
        ):
            with self.assertRaisesRegex(WorkspaceError, "reserved workspace metadata"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})

    def test_worker_package_and_tool_metadata_fail_closed(self) -> None:
        source = self.make_worker_source()
        package_path = source / "package.json"
        cases = (
            ([], "root is not an object"),
            ({"dependencies": []}, "dependencies is invalid"),
            ({"dependencies": {"../escape": "1"}}, "package name is unsafe"),
        )
        for package, message in cases:
            with self.subTest(message=message):
                package_path.write_text(json.dumps(package), encoding="utf-8")
                with self.assertRaisesRegex(WorkspaceError, message):
                    self.workspace._worker_required_packages(source)

        package_path.write_text(
            json.dumps({"scripts": {"check": "node check.js"}}), encoding="utf-8"
        )
        environment = {"PATH": "/bin", "npm_config_cache": "/cache"}

        def invalid_version(arguments: list[str], **_kwargs: object) -> str:
            if arguments == ["node", "--version"]:
                return "v22\ninvalid"
            return "11.0.0"

        with mock.patch("atrinik_workspace.workspace.run", side_effect=invalid_version):
            with self.assertRaisesRegex(WorkspaceError, "invalid version"):
                self.workspace._worker_dependency_inputs(source, environment)

        def valid_node_runtime() -> str:
            return json.dumps(
                {
                    "platform": "linux",
                    "arch": "x64",
                    "versions": {"modules": "127", "napi": "10"},
                }
            )

        def invalid_runtime(arguments: list[str], **_kwargs: object) -> str:
            if arguments in (["node", "--version"], ["npm", "--version"]):
                return "v22.0.0"
            return "not-json"

        with mock.patch("atrinik_workspace.workspace.run", side_effect=invalid_runtime):
            with self.assertRaisesRegex(WorkspaceError, "runtime identity"):
                self.workspace._worker_dependency_inputs(source, environment)

        def invalid_config(arguments: list[str], **_kwargs: object) -> str:
            if arguments in (["node", "--version"], ["npm", "--version"]):
                return "v22.0.0"
            if arguments[:2] == ["node", "-p"]:
                return valid_node_runtime()
            return "not-json"

        with mock.patch("atrinik_workspace.workspace.run", side_effect=invalid_config):
            with self.assertRaisesRegex(WorkspaceError, "not valid JSON"):
                self.workspace._worker_dependency_inputs(source, environment)

        package_path.write_text(json.dumps({"scripts": []}), encoding="utf-8")
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=lambda arguments, **_kwargs: (
                "{}"
                if arguments == ["npm", "config", "list", "--json"]
                else valid_node_runtime()
                if arguments[:2] == ["node", "-p"]
                else "v22.0.0"
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "scripts are invalid"):
                self.workspace._worker_dependency_inputs(source, environment)

    def test_worker_installed_tree_validation_rejects_unsafe_shapes(self) -> None:
        modules = self.root / "validation-node-modules"
        with self.assertRaisesRegex(WorkspaceError, "not a regular directory"):
            self.workspace._validate_worker_node_modules(
                modules, "0" * 64, "0" * 64, ()
            )
        modules.mkdir()
        hidden = modules / ".package-lock.json"

        def write_hidden(packages: object) -> str:
            hidden.write_text(json.dumps({"packages": packages}), encoding="utf-8")
            return hashlib.sha256(hidden.read_bytes()).hexdigest()

        digest = write_hidden({})
        with self.assertRaisesRegex(WorkspaceError, "installed lockfile does not match"):
            self.workspace._validate_worker_node_modules(
                modules, "0" * 64, _tree_digest(modules, set()), ()
            )
        digest = write_hidden([])
        with self.assertRaisesRegex(WorkspaceError, "packages are invalid"):
            self.workspace._validate_worker_node_modules(
                modules, digest, _tree_digest(modules, set()), ()
            )
        for packages, message in (
            ({"invalid": {}}, "package path is invalid"),
            ({"node_modules/../escape": {}}, "package path is unsafe"),
            ({"node_modules/missing": {}}, "package is missing or unsafe"),
        ):
            with self.subTest(message=message):
                digest = write_hidden(packages)
                with self.assertRaisesRegex(WorkspaceError, message):
                    self.workspace._validate_worker_node_modules(
                        modules, digest, _tree_digest(modules, set()), ()
                    )
        digest = write_hidden({})
        with self.assertRaisesRegex(WorkspaceError, "dependency is missing"):
            self.workspace._validate_worker_node_modules(
                modules, digest, _tree_digest(modules, set()), ("required",)
            )
        with self.assertRaisesRegex(WorkspaceError, "does not match cache metadata"):
            self.workspace._validate_worker_node_modules(
                modules, digest, "0" * 64, ()
            )

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
            first["lifecycle_source_sha256"],
            second["lifecycle_source_sha256"],
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
        metadata = {
            "node_modules_lock_sha256": hidden_digest,
            "node_modules_sha256": _tree_digest(
                dependencies,
                set(),
                bounded_symlinks=True,
                copied_metadata=True,
            ),
            "node_modules_view_sha256": _tree_digest(
                dependencies,
                WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
                bounded_symlinks=True,
                copied_metadata=True,
                ignore_root_mtime=True,
            ),
            "inputs": {
                "lifecycle_source_sha256": _tree_digest(
                    source,
                    WORKER_SOURCE_EXCLUSIONS,
                    reject_symlinks=True,
                    copied_metadata=True,
                )
            },
        }
        root = self.workspace.paths.builds / "profiles" / "worker-test"
        managed_directory(root, self.workspace.paths.builds, "worker-test")

        def corrupt_copy(*args: object, **kwargs: object) -> None:
            real_copy_worker_source(*args, **kwargs)
            destination = args[1]
            assert isinstance(destination, Path)
            (destination / "worker.ts").write_text(
                "mixed view snapshot\n", encoding="utf-8"
            )

        with mock.patch(
            "atrinik_workspace.workspace._copy_worker_source",
            side_effect=corrupt_copy,
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "does not match its fingerprint"
            ):
                self.workspace._worker_view(
                    root, source, dependencies, "a" * 64, metadata
                )

        first = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        (first[0] / "node_modules" / ".vite").mkdir()
        (first[0] / "node_modules" / ".vite" / "cache").write_text(
            "profile generated\n", encoding="utf-8"
        )
        second = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(first[1])
        self.assertTrue(second[1])
        self.assertTrue((second[0] / "src" / "build" / "nested.ts").is_file())
        self.workspace._reconcile_worker_view_after_checks(
            source, second[0], "a" * 64, metadata
        )
        unexpected_dependency_output = second[0] / "node_modules" / "alpha" / "changed"
        unexpected_dependency_output.write_text("changed\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "does not match cache metadata"):
            self.workspace._reconcile_worker_view_after_checks(
                source, second[0], "a" * 64, metadata
            )
        unexpected_dependency_output.unlink()

        copied_source = second[0] / "worker.ts"
        copied_status = copied_source.stat()
        os.utime(
            copied_source,
            ns=(copied_status.st_atime_ns, copied_status.st_mtime_ns + 1),
        )
        reconciled_metadata = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(reconciled_metadata[1])
        if hasattr(os, "setxattr"):
            try:
                os.setxattr(
                    reconciled_metadata[0] / "worker.ts",
                    "user.atrinik-view-test",
                    b"changed",
                )
            except OSError:
                pass
            else:
                reconciled_metadata = self.workspace._worker_view(
                    root, source, dependencies, "a" * 64, metadata
                )
                self.assertFalse(reconciled_metadata[1])

        external_metadata = self.root / "matching-worker-view.json"
        view_metadata = reconciled_metadata[0] / ".atrinik-worker-view.json"
        shutil.copy2(view_metadata, external_metadata)
        view_metadata.unlink()
        view_metadata.symlink_to(external_metadata)
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_metadata[0], "a" * 64, metadata
            )
        reconciled_control = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(reconciled_control[1])
        self.assertFalse(
            (reconciled_control[0] / ".atrinik-worker-view.json").is_symlink()
        )

        copied_source = reconciled_control[0] / "worker.ts"
        copied_status = copied_source.stat()
        os.utime(
            copied_source,
            ns=(copied_status.st_atime_ns, copied_status.st_mtime_ns + 1),
        )
        with mock.patch(
            "atrinik_workspace.workspace._copy_worker_source",
            side_effect=AssertionError("post-check reconciliation copied source bytes"),
        ):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_control[0], "a" * 64, metadata
            )
        reconciled_after_check = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertTrue(reconciled_after_check[1])

        external_marker = self.root / "matching-worker-marker.json"
        marker = reconciled_after_check[0] / MANAGED_MARKER
        shutil.copy2(marker, external_marker)
        view_metadata = reconciled_after_check[0] / ".atrinik-worker-view.json"

        def fail_after_corrupting_controls(*args: object, **kwargs: object) -> None:
            marker.unlink()
            view_metadata.unlink()
            view_metadata.symlink_to(external_metadata)
            raise subprocess.CalledProcessError(1, ["npm", "run", "check"])

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=fail_after_corrupting_controls,
            ),
            self.assertRaises(subprocess.CalledProcessError),
        ):
            self.workspace._run_worker_checks(
                reconciled_after_check[0], {}, "a" * 64, metadata
            )
        self.assertEqual(
            load_json(marker),
            {
                "schema_version": 1,
                "purpose": "source-view:metaserver-worker",
            },
        )
        self.assertFalse(view_metadata.is_symlink())

        def fail_after_replacing_controls_with_directories(
            *args: object, **kwargs: object
        ) -> None:
            marker.unlink()
            marker.mkdir()
            (marker / "nested").write_text("corrupt\n", encoding="utf-8")
            view_metadata.unlink()
            view_metadata.mkdir()
            (view_metadata / "nested").write_text("corrupt\n", encoding="utf-8")
            raise subprocess.CalledProcessError(1, ["npm", "run", "check"])

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=fail_after_replacing_controls_with_directories,
            ),
            self.assertRaises(subprocess.CalledProcessError),
        ):
            self.workspace._run_worker_checks(
                reconciled_after_check[0], {}, "a" * 64, metadata
            )
        self.assertTrue(marker.is_file())
        self.assertTrue(view_metadata.is_file())

        atomic_json(marker, {"schema_version": 1, "purpose": "wrong"})
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_after_check[0], "a" * 64, metadata
            )
        shutil.copy2(external_marker, marker)

        view_metadata.unlink()
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_after_check[0], "a" * 64, metadata
            )
        shutil.copy2(external_metadata, view_metadata)

        marker.unlink()
        marker.symlink_to(external_marker)
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_after_check[0], "a" * 64, metadata
            )
        with self.assertRaises(WorkspaceError):
            self.workspace._worker_view(
                root, source, dependencies, "a" * 64, metadata
            )
        self.assertTrue(marker.is_symlink())
        marker.unlink()
        shutil.copy2(external_marker, marker)

        (second[0] / "node_modules" / "alpha" / "local").write_text(
            "profile only\n", encoding="utf-8"
        )
        self.assertFalse((dependencies / "alpha" / "local").exists())

        (source / "worker.ts").write_text(
            "export const value = 2;\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(WorkspaceError, "lifecycle inputs"):
            self.workspace._worker_view(
                root, source, dependencies, "a" * 64, metadata
            )
        metadata["inputs"]["lifecycle_source_sha256"] = _tree_digest(
            source,
            WORKER_SOURCE_EXCLUSIONS,
            reject_symlinks=True,
            copied_metadata=True,
        )

        def corrupt_view_before_verification(
            output: Path,
            staging: Path,
            backup_prefix: str,
            backup_parent: Path | None = None,
            verify_after_install: object = None,
        ) -> None:
            assert callable(verify_after_install)

            def corrupt_then_verify() -> None:
                (output / "worker.ts").write_text(
                    "post-rename corruption\n", encoding="utf-8"
                )
                verify_after_install()

            worker_replace_directory(
                output,
                staging,
                backup_prefix,
                backup_parent,
                corrupt_then_verify,
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.replace_directory",
                side_effect=corrupt_view_before_verification,
            ),
            self.assertRaisesRegex(WorkspaceError, "published Worker view"),
        ):
            self.workspace._worker_view(
                root, source, dependencies, "b" * 64, metadata
            )
        self.assertEqual(
            (first[0] / "worker.ts").read_text(encoding="utf-8"),
            "export const value = 1;\n",
        )
        changed = self.workspace._worker_view(
            root, source, dependencies, "b" * 64, metadata
        )
        self.assertFalse(changed[1])
        self.assertEqual(
            (changed[0] / "worker.ts").read_text(encoding="utf-8"),
            "export const value = 2;\n",
        )
        self.assertFalse((changed[0] / "node_modules" / "alpha" / "local").exists())
    def test_source_view_reconciles_links_copies_and_stale_entries_in_place(self) -> None:
        source = self.workspace.paths.repositories / "server"
        copied_source = source / "install_data"
        copied_file = copied_source / "keys" / "test.pub"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        view = self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )
        readme = view / "README"
        copied = view / "install_data" / "keys" / "test.pub"
        (view / "stale").write_text("stale\n", encoding="utf-8")
        readme.unlink()
        readme.symlink_to(source / "install_data", target_is_directory=True)
        copied_file.write_text("changed\n", encoding="utf-8")
        (copied_source / "unique-items" / "new").write_text("new\n", encoding="utf-8")

        reconciled = self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )

        self.assertEqual(reconciled, view)
        self.assertFalse((view / "stale").exists())
        self.assertTrue(readme.is_symlink())
        self.assertEqual(os.readlink(readme), str(source / "README"))
        self.assertEqual(readme.resolve(), source / "README")
        self.assertEqual(copied.read_text(encoding="utf-8"), "changed\n")
        self.assertTrue((view / "install_data" / "unique-items" / "new").is_file())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        metadata = load_json(view / SOURCE_VIEW_METADATA)
        self.assertEqual(metadata["purpose"], "source-view:server")

        copied_file.chmod(0o700)
        self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )
        self.assertEqual(copied.stat().st_mode & 0o777, 0o700)
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])

        (source / "README").unlink()
        self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )
        self.assertFalse(readme.exists())

    def test_source_view_retains_unchanged_entries_and_rejects_escaping_symlink(
        self,
    ) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        view = self.workspace._profile_source_view(root, "content", source, set())
        inode = (view / "README").lstat().st_ino

        self.workspace._profile_source_view(root, "content", source, set())
        self.assertEqual((view / "README").lstat().st_ino, inode)
        self.assertTrue(self.workspace._source_view_unchanged[str(view.resolve())])

        internal_target = source / "internal-target"
        internal_target.write_text("one\n", encoding="utf-8")
        source_link = source / "internal-link"
        source_link.symlink_to(internal_target.name)
        self.workspace._profile_source_view(root, "content", source, set())
        self.workspace._profile_source_view(root, "content", source, set())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        first_metadata = load_json(view / SOURCE_VIEW_METADATA)
        second_target = source / "second-target"
        second_target.write_text("two\n", encoding="utf-8")
        source_link.unlink()
        source_link.symlink_to(second_target.name)
        self.workspace._profile_source_view(root, "content", source, set())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        self.assertNotEqual(first_metadata, load_json(view / SOURCE_VIEW_METADATA))

        outside_directory = self.root / "outside-directory"
        outside_directory.mkdir()
        sentinel = outside_directory / "sentinel"
        sentinel.write_text("preserved\n", encoding="utf-8")
        (view / "README").unlink()
        (view / "README").symlink_to(outside_directory, target_is_directory=True)
        self.workspace._profile_source_view(root, "content", source, set())
        self.assertTrue(sentinel.is_file())
        self.assertEqual((view / "README").resolve(), source / "README")
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])

        outside = self.root / "outside"
        outside.write_text("outside\n", encoding="utf-8")
        (source / "escape").symlink_to(outside)
        with self.assertRaisesRegex(WorkspaceError, "escapes its source root"):
            self.workspace._profile_source_view(root, "content", source, set())

        (source / "escape").unlink()
        nested = source / "nested-linked-directory"
        nested.mkdir()
        (nested / "escape").symlink_to(outside)
        with self.assertRaisesRegex(WorkspaceError, "escapes its source root"):
            self.workspace._profile_source_view(root, "content", source, set())

    def test_copied_source_view_recursively_excludes_generated_entries(self) -> None:
        source = self.workspace.paths.repositories / "content"
        nested = source / "nested"
        (nested / "kept").mkdir(parents=True)
        (nested / "kept" / "input").write_text("kept\n", encoding="utf-8")
        for excluded in (".git", "build", "dist", "node_modules", ".wrangler"):
            (nested / excluded).mkdir()
            (nested / excluded / "stale").write_text("excluded\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        view = self.workspace._profile_source_view(
            root,
            "worker",
            source,
            {"build", "dist", "node_modules", ".wrangler"},
            copy_all=True,
        )

        self.assertTrue((view / "nested" / "kept" / "input").is_file())
        for excluded in (".git", "build", "dist", "node_modules", ".wrangler"):
            self.assertFalse((view / "nested" / excluded).exists())
        (view / "nested" / "node_modules").mkdir()
        self.workspace._profile_source_view(
            root,
            "worker",
            source,
            {"build", "dist", "node_modules", ".wrangler"},
            copy_all=True,
        )
        self.assertFalse((view / "nested" / "node_modules").exists())

    def test_copied_source_view_allows_only_repository_internal_symlinks(self) -> None:
        source = self.workspace.paths.repositories / "content"
        left = source / "left"
        right = source / "right"
        left.mkdir()
        right.mkdir()
        (right / "value").write_text("safe\n", encoding="utf-8")
        (left / "value-link").symlink_to(Path("../right/value"))
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        view = self.workspace._profile_source_view(
            root, "worker", source, set(), copy_all=True
        )

        self.assertTrue((view / "left" / "value-link").is_symlink())
        self.assertEqual(
            (view / "left" / "value-link").resolve(), view / "right" / "value"
        )
        top_level_link = source / "top-level-link"
        top_level_link.symlink_to(Path("../content/README"))
        conflicting = root / "sources" / source.name
        conflicting.mkdir()
        (conflicting / "README").write_text("wrong\n", encoding="utf-8")
        self.workspace._profile_source_view(
            root, "worker", source, set(), copy_all=True
        )
        self.assertEqual((view / "top-level-link").resolve(), view / "README")
        self.assertNotEqual(
            (view / "top-level-link").resolve(), conflicting / "README"
        )

        (source / "build").mkdir()
        (source / "build" / "generated").write_text("excluded\n", encoding="utf-8")
        (source / "excluded-link").symlink_to(Path("build/generated"))
        with self.assertRaisesRegex(WorkspaceError, "targets an excluded entry"):
            self.workspace._profile_source_view(
                root, "worker", source, {"build"}, copy_all=True
            )
        (source / "excluded-link").unlink()
        outside = self.root / "outside-copy-root"
        outside.write_text("unsafe\n", encoding="utf-8")
        (left / "escape-link").symlink_to(outside)
        with self.assertRaisesRegex(WorkspaceError, "escapes its source root"):
            self.workspace._profile_source_view(
                root, "worker", source, set(), copy_all=True
            )

    def test_profile_views_and_cmake_reject_intermediate_symlink_aliases(self) -> None:
        builds = self.workspace.paths.builds
        profile_a = builds / "profiles" / "a"
        profile_z = builds / "profiles" / "z"
        managed_directory(profile_a, builds, "profile:a")
        managed_directory(profile_z, builds, "profile:z")
        source = self.workspace.paths.repositories / "content"
        target_view = self.workspace._profile_source_view(
            profile_z, "content", source, set()
        )
        (profile_a / "sources").symlink_to(
            profile_z / "sources", target_is_directory=True
        )

        with self.assertRaisesRegex(WorkspaceError, "symlinked managed build path"):
            self.workspace._profile_source_view(profile_a, "content", source, set())
        self.assertEqual((target_view / "README").resolve(), source / "README")

        target_binary = profile_z / "build" / "sample"
        managed_directory(target_binary, builds, "cmake-binary")
        (profile_a / "build").symlink_to(
            profile_z / "build", target_is_directory=True
        )
        with self.assertRaisesRegex(WorkspaceError, "symlinked managed build path"):
            self.workspace._prepare_cmake_binary(profile_a / "build" / "sample")
        self.assertTrue(target_binary.is_dir())

    def test_preserved_source_view_directory_removes_unexpected_children(self) -> None:
        source = self.workspace.paths.repositories / "server"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        view = self.workspace._profile_source_view(
            root, "server", source, {"runtime"}, preserved_entries={"runtime"}
        )
        runtime = view / "runtime"
        runtime.mkdir()
        (runtime / "content").symlink_to(source, target_is_directory=True)
        (runtime / "stale").write_text("stale\n", encoding="utf-8")

        self.workspace._source_view_directory(view, "runtime", {"content"})

        self.assertTrue((runtime / "content").is_symlink())
        self.assertFalse((runtime / "stale").exists())

    def test_cmake_skips_only_unchanged_fingerprint_and_honors_force(self) -> None:
        source_root = self.workspace.paths.repositories / "content"
        (source_root / "CMakeLists.txt").write_text(
            "project(test C)\n", encoding="utf-8"
        )
        command("git", "add", "CMakeLists.txt", cwd=source_root)
        command("git", "commit", "-m", "test: add CMake project", cwd=source_root)
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        source = self.workspace._profile_source_view(root, "content", source_root, set())
        binary = root / "build" / "content"

        def configured(command: list[str], **kwargs: object) -> str | None:
            if command[0] == "git":
                return workspace_run(command, **kwargs)
            if command[:2] == ["ninja", "-C"]:
                return "build.ninja:\n  input: RERUN_CMAKE\n"
            if command[:2] != ["cmake", "-S"]:
                return
            (binary / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={source.resolve()}\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                "CMAKE_BUILD_TYPE:STRING=Debug\n"
                "BUILD_TESTING:UNINITIALIZED=OFF\n"
                "CMAKE_C_COMPILER_LAUNCHER:UNINITIALIZED=\n"
                "CMAKE_CXX_COMPILER_LAUNCHER:UNINITIALIZED=\n",
                encoding="utf-8",
            )
            (binary / "build.ninja").write_text("# generated\n", encoding="utf-8")

        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value=None),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch("atrinik_workspace.workspace.run", side_effect=configured) as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            first_count = run.call_count
            self.workspace._profile_source_view(root, "content", source_root, set())
            self.workspace._cmake(source, binary, [], tests=False)
            second_commands = [
                call.args[0]
                for call in run.call_args_list[first_count:]
                if call.args[0][0] != "git"
            ]
            self.assertEqual(
                second_commands,
                [
                    ["ninja", "-C", str(binary), "-t", "query", "build.ninja"],
                    ["cmake", "--build", str(binary), "--parallel"],
                ],
            )

            (binary / "build.ninja").unlink()
            repair_start = run.call_count
            self.workspace._cmake(source, binary, [], tests=False)
            repair_commands = [
                call.args[0] for call in run.call_args_list[repair_start:]
            ]
            self.assertTrue(
                any(command[:2] == ["cmake", "-S"] for command in repair_commands)
            )

            self.workspace._force_reconfigure = True
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertTrue(any(command[:2] == ["cmake", "-S"] for command in [
                call.args[0] for call in run.call_args_list[first_count + 1 :]
            ]))

        self.assertTrue((binary / CONFIGURE_METADATA).is_file())

    def test_cmake_fingerprint_invalidates_for_tests_environment_and_toolchain(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / "CMakeLists.txt").write_text("project(test C)\n", encoding="utf-8")
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        identities = {
            "cmake": {"command": "cmake", "path": "/cmake", "version": "cmake 1"},
            "ninja": {"command": "ninja", "path": "/ninja", "version": "ninja 1"},
            "cc": {"command": "cc", "path": "/cc", "version": "cc 1"},
            "c++": {"command": "c++", "path": "/c++", "version": "c++ 1"},
        }
        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value=None),
            mock.patch.object(
                self.workspace, "_tool_identity", side_effect=lambda tool: identities[tool]
            ),
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            self.workspace._cmake(source, binary, [], tests=True)
            with mock.patch.dict(os.environ, {"CFLAGS": "-DCHANGED"}):
                self.workspace._cmake(source, binary, [], tests=True)
            with mock.patch.dict(
                os.environ,
                {
                    "ATRINIK_PACKAGE_VERSION": "review",
                    "PKG_CONFIG_PATH": "/opt/review/pkgconfig",
                },
            ):
                self.workspace._cmake(source, binary, [], tests=True)
            identities["cc"] = {"command": "cc", "path": "/cc", "version": "cc 2"}
            self.workspace._cmake(source, binary, [], tests=True)

        configure_calls = [
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        ]
        self.assertEqual(len(configure_calls), 5)

    def test_tool_identity_handles_wrappers_and_empty_version_output(self) -> None:
        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value="/tool"),
            mock.patch("atrinik_workspace.workspace.run", return_value="") as run,
        ):
            identity = self.workspace._tool_identity("wrapper --compiler cc")

        run.assert_called_once_with(
            ["/tool", "--compiler", "cc", "--version"],
            capture=True,
            trace=False,
        )
        self.assertEqual(identity["version"], "unavailable: empty --version output")

    def test_direct_source_does_not_trust_unowned_source_view_metadata(self) -> None:
        source = self.workspace.paths.repositories / "content"
        cmakelists = source / "CMakeLists.txt"
        cmakelists.write_text("project(test C)\n", encoding="utf-8")
        atomic_json(
            source / SOURCE_VIEW_METADATA,
            {
                "schema_version": 1,
                "purpose": "source-view:forged",
                "source": "/forged",
                "entries": {},
            },
        )

        identity = self.workspace._cmake_source_identity(source)

        self.assertEqual(identity["path"], str(source.resolve()))
        self.assertEqual(
            identity["cmakelists"], hashlib.sha256(cmakelists.read_bytes()).hexdigest()
        )
        self.assertFalse(identity["configure_skip_safe"])

    def test_direct_source_skip_requires_clean_git_identity(self) -> None:
        source = self.workspace.paths.repositories / "protocol"
        cmakelists = source / "CMakeLists.txt"
        cmakelists.write_text("project(test C)\n", encoding="utf-8")
        command("git", "add", "CMakeLists.txt", cwd=source)
        command("git", "commit", "-m", "test: add CMake project", cwd=source)

        clean = self.workspace._cmake_source_identity(source)
        self.assertTrue(clean["configure_skip_safe"])
        self.assertEqual(clean["git"]["root"], str(source.resolve()))
        self.assertEqual(len(clean["git"]["head"]), 40)

        (source / "new-input.c").write_text("int added;\n", encoding="utf-8")
        dirty = self.workspace._cmake_source_identity(source)
        self.assertFalse(dirty["configure_skip_safe"])
        self.assertEqual(dirty["git"]["head"], clean["git"]["head"])

        non_git = self.root / "non-git-cmake-source"
        non_git.mkdir()
        (non_git / "CMakeLists.txt").write_text(
            "project(non_git NONE)\n", encoding="utf-8"
        )
        fallback = self.workspace._cmake_source_identity(non_git)
        self.assertIsNone(fallback["git"])
        self.assertFalse(fallback["configure_skip_safe"])

    def test_dirty_direct_cmake_source_never_skips_explicit_configure(self) -> None:
        source = self.workspace.paths.repositories / "protocol"
        (source / "CMakeLists.txt").write_text("project(test C)\n", encoding="utf-8")
        binary = (
            self.workspace.paths.builds / "profiles" / "test" / "build" / "protocol"
        )
        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value=None),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch.object(self.workspace, "_cmake_state_valid", return_value=True),
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            self.workspace._cmake(source, binary, [], tests=False)

        configure_calls = [
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        ]
        self.assertEqual(len(configure_calls), 2)

    def test_cmake_enables_bounded_marker_owned_ccache_with_normalized_paths(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / "CMakeLists.txt").write_text("project(test CXX)\n", encoding="utf-8")
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        with (
            mock.patch(
                "atrinik_workspace.workspace.shutil.which",
                side_effect=lambda tool: "/usr/bin/ccache" if tool == "ccache" else f"/usr/bin/{tool}",
            ),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch.object(
                self.workspace, "_compiler_supports_prefix_maps", return_value=True
            ),
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)

        configure = next(
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        )
        arguments = configure.args[0]
        environment = configure.kwargs["env"]
        self.assertIn("-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/ccache", arguments)
        self.assertIn("-fdebug-prefix-map=", environment["CFLAGS"])
        self.assertIn("-ffile-prefix-map=", environment["CXXFLAGS"])
        self.assertEqual(environment["CCACHE_MAXSIZE"], "5G")
        self.assertEqual(environment["CCACHE_BASEDIR"], str(binary.parent.parent.resolve()))
        self.assertEqual(environment["CCACHE_NOHASHDIR"], "true")
        self.assertNotIn("CCACHE_HASHDIR", environment)
        cache = self.workspace.paths.builds / "compiler-cache"
        self.assertEqual(load_json(cache / MANAGED_MARKER)["purpose"], "compiler-cache")
        self.assertEqual(load_json(cache / ".atrinik-cache.json")["max_size"], "5G")

    def test_debug_prefix_flags_require_supported_non_toolchain_compilers(self) -> None:
        source = self.workspace.paths.repositories / "content"
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        environment = {"CFLAGS": "/existing", "CXXFLAGS": "/existing-cxx"}
        with mock.patch.object(
            self.workspace, "_compiler_supports_prefix_maps", return_value=False
        ):
            support = self.workspace._add_debug_prefix_environment(
                source, binary, environment, []
            )
        self.assertEqual(support, {"c": False, "cxx": False})
        self.assertEqual(environment["CFLAGS"], "/existing")
        self.assertEqual(environment["CXXFLAGS"], "/existing-cxx")

        partial_environment = {"CFLAGS": "/c", "CXXFLAGS": "/cxx"}
        with mock.patch.object(
            self.workspace,
            "_compiler_supports_prefix_maps",
            side_effect=[True, False],
        ):
            support = self.workspace._add_debug_prefix_environment(
                source, binary, partial_environment, []
            )
        self.assertEqual(support, {"c": True, "cxx": False})
        self.assertIn("-fdebug-prefix-map=", partial_environment["CFLAGS"])
        self.assertEqual(partial_environment["CXXFLAGS"], "/cxx")

        with mock.patch.object(
            self.workspace, "_compiler_supports_prefix_maps"
        ) as supported:
            support = self.workspace._add_debug_prefix_environment(
                source,
                binary,
                environment,
                ["-DCMAKE_TOOLCHAIN_FILE=/tmp/windows-toolchain.cmake"],
            )
        self.assertEqual(support, {"c": False, "cxx": False})
        supported.assert_not_called()

    def test_cmake_keeps_hash_directory_for_unproven_toolchain_compilers(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / "CMakeLists.txt").write_text("project(test C)\n", encoding="utf-8")
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        with (
            mock.patch.dict(os.environ, {"CCACHE_NOHASHDIR": "true"}),
            mock.patch(
                "atrinik_workspace.workspace.shutil.which",
                side_effect=lambda tool: f"/usr/bin/{tool}",
            ),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch.object(
                self.workspace, "_compiler_supports_prefix_maps"
            ) as supported,
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(
                source,
                binary,
                ["-DCMAKE_TOOLCHAIN_FILE=/missing/toolchain.cmake"],
                tests=False,
            )

        supported.assert_not_called()
        configure = next(
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        )
        environment = configure.kwargs["env"]
        self.assertEqual(environment["CCACHE_HASHDIR"], "true")
        self.assertNotIn("CCACHE_NOHASHDIR", environment)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "cmake", "ninja")),
        "real CMake toolchain is unavailable",
    )
    def test_real_cmake_reinitializes_changed_toolchain_and_preserves_init_flags(
        self,
    ) -> None:
        source = self.root / "cmake-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(wrapper_toolchain C)\n"
            "add_executable(sample main.c)\n",
            encoding="utf-8",
        )
        (source / "main.c").write_text(
            "#ifndef TOOLCHAIN_VALUE\n#error missing toolchain flag\n#endif\n"
            "int main(void) { return 0; }\n",
            encoding="utf-8",
        )
        compiler = self.root / "compiler-wrapper"
        compiler.write_text("#!/bin/sh\nexec /usr/bin/cc \"$@\"\n", encoding="utf-8")
        compiler.chmod(0o755)
        fragment = self.root / "toolchain-flags.cmake"
        fragment.write_text(
            'set(CMAKE_C_FLAGS_INIT "-DTOOLCHAIN_VALUE=1")\n', encoding="utf-8"
        )
        toolchain_target = self.root / "toolchain-real.cmake"
        toolchain_target.write_text(
            f'include("{fragment}")\nset(CMAKE_C_COMPILER "{compiler}")\n',
            encoding="utf-8",
        )
        toolchain = self.root / "toolchain.cmake"
        toolchain.symlink_to(toolchain_target)
        binary = self.workspace.paths.builds / "profiles" / "real" / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        cache = (binary / "CMakeCache.txt").read_text(encoding="utf-8")
        self.assertIn("-DTOOLCHAIN_VALUE=1", cache)
        preserved = binary / "preserved-on-unchanged-toolchain"
        preserved.write_text("current\n", encoding="utf-8")
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        self.assertTrue(preserved.is_file())
        sentinel = binary / "removed-on-toolchain-change"
        sentinel.write_text("stale\n", encoding="utf-8")

        fragment.write_text(
            'set(CMAKE_C_FLAGS_INIT "-DTOOLCHAIN_VALUE=2")\n', encoding="utf-8"
        )
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        self.assertFalse(sentinel.exists())
        self.assertIn(
            "-DTOOLCHAIN_VALUE=2",
            (binary / "CMakeCache.txt").read_text(encoding="utf-8"),
        )
        compiler_sentinel = binary / "removed-on-compiler-change"
        compiler_sentinel.write_text("stale\n", encoding="utf-8")
        compiler.write_text(
            "#!/bin/sh\n# updated wrapper\nexec /usr/bin/cc \"$@\"\n",
            encoding="utf-8",
        )
        compiler.chmod(0o755)

        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        self.assertFalse(compiler_sentinel.exists())
        link_sentinel = binary / "removed-on-toolchain-link-change"
        link_sentinel.write_text("stale\n", encoding="utf-8")
        second_target = self.root / "toolchain-second.cmake"
        second_target.write_text(
            f"include([[{fragment}]])\nset(CMAKE_C_COMPILER \"{compiler}\")\n"
            "# second toolchain target\n",
            encoding="utf-8",
        )
        toolchain.unlink()
        toolchain.symlink_to(second_target)

        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        self.assertFalse(link_sentinel.exists())
        (binary / "build.ninja").write_text("corrupt graph\n", encoding="utf-8")
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        self.assertIn(
            "RERUN_CMAKE",
            subprocess.run(
                ["ninja", "-C", str(binary), "-t", "query", "build.ninja"],
                check=True,
                capture_output=True,
                text=True,
            ).stdout,
        )

        compiler_path = self.root / "compiler-path.txt"
        compiler_path.write_text(str(compiler), encoding="utf-8")
        dynamic_target = self.root / "toolchain-dynamic.cmake"
        dynamic_target.write_text(
            f'include("{fragment}")\n'
            f'file(READ "{compiler_path}" SELECTED_COMPILER)\n'
            'set(CMAKE_C_COMPILER "${SELECTED_COMPILER}")\n',
            encoding="utf-8",
        )
        toolchain.unlink()
        toolchain.symlink_to(dynamic_target)
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        incomplete_sentinel = binary / "removed-for-unproven-toolchain-inputs"
        incomplete_sentinel.write_text("stale\n", encoding="utf-8")
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        self.assertFalse(incomplete_sentinel.exists())

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "cmake", "ninja")),
        "real CMake toolchain is unavailable",
    )
    def test_real_cmake_repairs_cache_and_rebuilds_for_implicit_environment(
        self,
    ) -> None:
        source = self.root / "environment-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(environment_rebuild C)\n"
            "find_program(SELECTED_TOOL selected-tool REQUIRED)\n"
            'file(WRITE "${CMAKE_BINARY_DIR}/selected-tool.txt" "${SELECTED_TOOL}")\n'
            "add_executable(environment main.c)\n",
            encoding="utf-8",
        )
        (source / "main.c").write_text(
            '#include <stdio.h>\n#include "selected-value.h"\n'
            "int main(void) { return puts(SELECTED_VALUE); }\n",
            encoding="utf-8",
        )
        include_one = self.root / "include-one"
        include_two = self.root / "include-two"
        include_one.mkdir()
        include_two.mkdir()
        tool_one = self.root / "tool-one"
        tool_two = self.root / "tool-two"
        tool_one.mkdir()
        tool_two.mkdir()
        for directory in (tool_one, tool_two):
            tool = directory / "selected-tool"
            tool.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            tool.chmod(0o755)
        (include_one / "selected-value.h").write_text(
            '#define SELECTED_VALUE "atrinik-environment-one"\n', encoding="utf-8"
        )
        (include_two / "selected-value.h").write_text(
            '#define SELECTED_VALUE "atrinik-environment-two"\n', encoding="utf-8"
        )
        binary = self.workspace.paths.builds / "profiles" / "environment" / "build" / "sample"
        self.workspace._use_ccache = False

        base_path = os.environ.get("PATH", "")
        with mock.patch.dict(
            os.environ,
            {"CPATH": str(include_one), "PATH": f"{tool_one}{os.pathsep}{base_path}"},
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertEqual(
                (binary / "selected-tool.txt").read_text(encoding="utf-8"),
                str(tool_one / "selected-tool"),
            )
            preserved = binary / "preserved-on-unchanged-environment"
            preserved.write_text("current\n", encoding="utf-8")
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertTrue(preserved.is_file())
            cache = binary / "CMakeCache.txt"
            cache.write_text(
                cache.read_text(encoding="utf-8").replace(
                    "CMAKE_BUILD_TYPE:STRING=Debug",
                    "CMAKE_BUILD_TYPE:STRING=Release",
                ),
                encoding="utf-8",
            )
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertFalse(preserved.exists())
            self.assertIn(
                "CMAKE_BUILD_TYPE:STRING=Debug", cache.read_text(encoding="utf-8")
            )

        environment_sentinel = binary / "removed-on-implicit-environment-change"
        environment_sentinel.write_text("stale\n", encoding="utf-8")
        with mock.patch.dict(
            os.environ,
            {"CPATH": str(include_two), "PATH": f"{tool_two}{os.pathsep}{base_path}"},
        ):
            self.workspace._cmake(source, binary, [], tests=False)
        self.assertFalse(environment_sentinel.exists())
        self.assertIn(
            b"atrinik-environment-two", (binary / "environment").read_bytes()
        )
        discovery_sentinel = binary / "removed-on-discovery-environment-change"
        discovery_sentinel.write_text("stale\n", encoding="utf-8")
        with mock.patch.dict(
            os.environ,
            {"CPATH": str(include_two), "PATH": f"{tool_one}{os.pathsep}{base_path}"},
        ):
            self.workspace._cmake(source, binary, [], tests=False)
        self.assertFalse(discovery_sentinel.exists())
        self.assertEqual(
            (binary / "selected-tool.txt").read_text(encoding="utf-8"),
            str(tool_one / "selected-tool"),
        )

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cmake", "ninja")),
        "real CMake/Ninja toolchain is unavailable",
    )
    def test_real_cmake_reconfigures_after_source_symlink_retarget(self) -> None:
        source = self.root / "symlinked-cmake-source"
        source.mkdir()
        nested = source / "src"
        nested.mkdir()
        (nested / "a.c").write_text("int a;\n", encoding="utf-8")
        first = source / "first.cmake"
        second = source / "second.cmake"
        first.write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(first NONE)\n"
            'file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "first:${SOURCES}")\n',
            encoding="utf-8",
        )
        second.write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(second NONE)\n"
            'file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "second:${SOURCES}")\n',
            encoding="utf-8",
        )
        older = first.stat().st_mtime - 60
        os.utime(second, (older, older))
        cmakelists = source / "CMakeLists.txt"
        cmakelists.symlink_to(first.name)
        root = self.workspace.paths.builds / "profiles" / "symlink-configure"
        managed_directory(root, self.workspace.paths.builds, "profile:symlink")
        view = self.workspace._profile_source_view(
            root, "sample", source, set()
        )
        binary = root / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(view, binary, [], tests=False)
        self.assertIn(
            "first:", (binary / "selected.txt").read_text(encoding="utf-8")
        )
        cmakelists.unlink()
        cmakelists.symlink_to(second.name)
        self.workspace._profile_source_view(root, "sample", source, set())
        self.workspace._cmake(view, binary, [], tests=False)
        selected = (binary / "selected.txt").read_text(encoding="utf-8")
        self.assertIn("second:", selected)
        self.assertNotIn("b.c", selected)

        added = nested / "b.c"
        added.write_text("int b;\n", encoding="utf-8")
        os.utime(added, (older, older))
        self.workspace._profile_source_view(root, "sample", source, set())
        self.workspace._cmake(view, binary, [], tests=False)
        self.assertIn(
            "b.c", (binary / "selected.txt").read_text(encoding="utf-8")
        )

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("git", "cmake", "ninja")),
        "real Git/CMake toolchain is unavailable",
    )
    def test_real_dirty_direct_source_reconfigures_plain_glob(self) -> None:
        source = self.root / "direct-cmake-source"
        (source / "src").mkdir(parents=True)
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(direct_dirty NONE)\n"
            'file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${SOURCES}")\n',
            encoding="utf-8",
        )
        (source / "src" / "a.c").write_text("int a;\n", encoding="utf-8")
        command("git", "init", "-b", "main", cwd=source)
        command("git", "config", "user.name", "Tests", cwd=source)
        command("git", "config", "user.email", "tests@example.invalid", cwd=source)
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: seed direct source", cwd=source)
        root = self.workspace.paths.builds / "profiles" / "direct-dirty"
        managed_directory(root, self.workspace.paths.builds, "profile:direct-dirty")
        binary = root / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(source, binary, [], tests=False)
        self.assertNotIn("b.c", (binary / "selected.txt").read_text(encoding="utf-8"))
        (source / "src" / "b.c").write_text("int b;\n", encoding="utf-8")
        self.workspace._cmake(source, binary, [], tests=False)
        self.assertIn("b.c", (binary / "selected.txt").read_text(encoding="utf-8"))

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("git", "cmake", "ninja")),
        "real Git/CMake toolchain is unavailable",
    )
    def test_real_clean_source_commit_reconfigures_despite_older_mtime(self) -> None:
        source = self.root / "clean-commit-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(clean_commit NONE)\n"
            "configure_file(value.in generated.txt COPYONLY)\n",
            encoding="utf-8",
        )
        configured_input = source / "value.in"
        configured_input.write_text("one\n", encoding="utf-8")
        command("git", "init", "-b", "main", cwd=source)
        command("git", "config", "user.name", "Tests", cwd=source)
        command("git", "config", "user.email", "tests@example.invalid", cwd=source)
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: seed clean source", cwd=source)
        root = self.workspace.paths.builds / "profiles" / "clean-commit"
        managed_directory(root, self.workspace.paths.builds, "profile:clean-commit")
        view = self.workspace._profile_source_view(root, "renamed", source, set())
        binary = root / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(view, binary, [], tests=False)
        self.assertEqual((binary / "generated.txt").read_text(), "one\n")
        sentinel = binary / "preserve-me"
        sentinel.write_text("keep\n", encoding="utf-8")

        configured_input.write_text("two\n", encoding="utf-8")
        command("git", "add", "value.in", cwd=source)
        command("git", "commit", "-m", "test: change configured input", cwd=source)
        older = (binary / "build.ninja").stat().st_mtime - 60
        os.utime(configured_input, (older, older))
        self.workspace._profile_source_view(root, "renamed", source, set())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        self.workspace._cmake(view, binary, [], tests=False)

        self.assertEqual((binary / "generated.txt").read_text(), "two\n")
        self.assertTrue(sentinel.is_file())

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "ccache", "cmake", "ninja")),
        "real ccache/CMake toolchain is unavailable",
    )
    def test_real_cmake_reuses_ccache_across_equivalent_profile_views(self) -> None:
        with mock.patch.dict(
            os.environ,
            {"ATRINIK_WORKSPACE_DIR": str(self.root / "workspace with spaces")},
        ):
            workspace = Workspace(self.wrapper)
            workspace.paths.ensure()
        checkout = self.root / "shared cmake source"
        checkout.mkdir()
        (checkout / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(wrapper_ccache C)\n"
            "add_executable(sample main.c)\n",
            encoding="utf-8",
        )
        (checkout / "main.c").write_text(
            "const char *source_file = __FILE__;\n"
            "int main(void) { return source_file[0] == 0; }\n",
            encoding="utf-8",
        )
        roots = [
            workspace.paths.builds / "profiles" / name
            for name in ("cache-a", "cache-b")
        ]
        views: list[Path] = []
        for root in roots:
            managed_directory(root, workspace.paths.builds, f"profile:{root.name}")
            views.append(
                workspace._profile_source_view(
                    root, "sample", checkout, set()
                )
            )

        workspace._cmake(
            views[0], roots[0] / "build" / "sample", [], tests=False
        )
        cache = workspace.paths.builds / "compiler-cache"
        statistics_environment = os.environ.copy()
        statistics_environment["CCACHE_DIR"] = str(cache)
        subprocess.run(
            ["ccache", "--zero-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        )
        workspace._cmake(
            views[1], roots[1] / "build" / "sample", [], tests=False
        )

        statistics = subprocess.run(
            ["ccache", "--print-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        ).stdout
        values = {
            name: int(value)
            for name, value in (
                line.split("\t", 1) for line in statistics.splitlines() if "\t" in line
            )
            if value.isdigit()
        }
        self.assertGreater(
            values.get("direct_cache_hit", 0)
            + values.get("preprocessed_cache_hit", 0),
            0,
        )
        object_file = next((roots[1] / "build" / "sample").rglob("main.c.o"))
        object_data = object_file.read_bytes()
        self.assertIn(b"/atrinik/source/main.c", object_data)
        self.assertNotIn(str(roots[1]).encode(), object_data)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "ccache", "cmake", "ninja")),
        "real ccache/CMake toolchain is unavailable",
    )
    def test_real_cmake_toolchain_keeps_profile_paths_out_of_shared_hits(self) -> None:
        source = self.root / "opaque-toolchain-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(wrapper_opaque_ccache C)\n"
            "add_executable(opaque_sample opaque_unique_main.c)\n",
            encoding="utf-8",
        )
        (source / "opaque_unique_main.c").write_text(
            "const char *opaque_source_file = __FILE__;\n"
            "int main(void) { return opaque_source_file[0] == 0; }\n",
            encoding="utf-8",
        )
        toolchain = self.root / "opaque-toolchain.cmake"
        toolchain.write_text(
            f'set(CMAKE_C_COMPILER "{shutil.which("cc")}")\n', encoding="utf-8"
        )
        roots = [
            self.workspace.paths.builds / "profiles" / name
            for name in ("opaque-cache-a", "opaque-cache-b")
        ]
        views: list[Path] = []
        for root in roots:
            managed_directory(root, self.workspace.paths.builds, f"profile:{root.name}")
            views.append(
                self.workspace._profile_source_view(root, "opaque", source, set())
            )

        self.workspace._cmake(
            views[0],
            roots[0] / "build" / "opaque",
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        cache = self.workspace.paths.builds / "compiler-cache"
        statistics_environment = os.environ.copy()
        statistics_environment["CCACHE_DIR"] = str(cache)
        subprocess.run(
            ["ccache", "--zero-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        )
        self.workspace._cmake(
            views[1],
            roots[1] / "build" / "opaque",
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        statistics = subprocess.run(
            ["ccache", "--print-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        ).stdout
        values = {
            name: int(value)
            for name, value in (
                line.split("\t", 1) for line in statistics.splitlines() if "\t" in line
            )
            if value.isdigit()
        }
        self.assertEqual(
            values.get("direct_cache_hit", 0)
            + values.get("preprocessed_cache_hit", 0),
            0,
        )
        self.assertGreater(values.get("cache_miss", 0), 0)
        object_file = next(
            (roots[1] / "build" / "opaque").rglob("opaque_unique_main.c.o")
        )
        object_data = object_file.read_bytes()
        self.assertIn(str(roots[1]).encode(), object_data)
        self.assertNotIn(str(roots[0]).encode(), object_data)

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
        self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())
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

    def test_resource_view_rejects_tracked_generated_metadata_names(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / MANAGED_MARKER).write_text("payload\n", encoding="utf-8")
        (source / "runtime-paths.txt").write_text(
            f"{MANAGED_MARKER}\n", encoding="utf-8"
        )
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: select reserved resource", cwd=source)
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        with self.assertRaisesRegex(WorkspaceError, "reserved generated paths"):
            self.workspace._stage_resources(root, {"resources": source})

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

    def test_resource_view_reuses_only_exact_clean_valid_inputs(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        copied = 0
        real_copy = shutil.copy2

        def counting_copy(source_path: Path, destination: Path, **kwargs: object) -> None:
            nonlocal copied
            copied += 1
            real_copy(source_path, destination, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.shutil.copy2", side_effect=counting_copy
        ):
            output = self.workspace._stage_resources(root, {"resources": source})
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 1)

            (source / "paintings" / "scene.jpg").write_text(
                "new commit\n", encoding="utf-8"
            )
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: change resource", cwd=source)
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 2)

            dirty = source / "local-only"
            dirty.write_text("dirty\n", encoding="utf-8")
            self.workspace._stage_resources(root, {"resources": source})
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 4)
            self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())
            dirty.unlink()

            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 5)
            (output / RUNTIME_INPUT_METADATA).write_text("{", encoding="utf-8")
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 6)

            (output / MANAGED_MARKER).write_text("{", encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, "cannot read"):
                self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 6)
            atomic_json(
                output / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "resource-view"},
            )

            (output / "unexpected").write_text("corrupt\n", encoding="utf-8")
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 7)
            self.assertFalse((output / "unexpected").exists())

            (output / "paintings" / "scene.jpg").write_text(
                "bad cache!\n", encoding="utf-8"
            )
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 8)
            self.assertEqual(
                (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
                "new commit\n",
            )

    def test_resource_view_race_preserves_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        previous = (output / "paintings" / "scene.jpg").read_text(encoding="utf-8")

        (source / "paintings" / "scene.jpg").write_text(
            "next commit\n", encoding="utf-8"
        )
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: advance resource", cwd=source)
        real_copy = shutil.copy2
        mutated = False

        def mutate_after_copy(source_path: Path, destination: Path, **kwargs: object) -> None:
            nonlocal mutated
            real_copy(source_path, destination, **kwargs)
            if not mutated:
                mutated = True
                (source / "README").write_text("changed during staging\n", encoding="utf-8")

        try:
            with mock.patch(
                "atrinik_workspace.workspace.shutil.copy2",
                side_effect=mutate_after_copy,
            ):
                with self.assertRaisesRegex(WorkspaceError, "changed during staging"):
                    self.workspace._stage_resources(root, {"resources": source})
        finally:
            command("git", "checkout", "--", "README", cwd=source)

        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            previous,
        )

    def test_resource_install_race_rolls_back_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        previous = (output / "paintings" / "scene.jpg").read_text(encoding="utf-8")
        (source / "paintings" / "scene.jpg").write_text(
            "next commit\n", encoding="utf-8"
        )
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: advance resource", cwd=source)
        advanced = False

        def replace_then_advance(
            destination: Path,
            staging: Path,
            backup_prefix: str,
            verify_after_install: object = None,
        ) -> None:
            nonlocal advanced

            def advance_and_verify() -> None:
                nonlocal advanced
                if not advanced:
                    advanced = True
                    (source / "paintings" / "scene.jpg").write_text(
                        "commit after install\n", encoding="utf-8"
                    )
                    command("git", "add", ".", cwd=source)
                    command(
                        "git",
                        "commit",
                        "-m",
                        "test: race after install",
                        cwd=source,
                    )
                assert callable(verify_after_install)
                verify_after_install()

            workspace_replace_directory(
                destination,
                staging,
                backup_prefix,
                advance_and_verify,
            )

        with mock.patch(
            "atrinik_workspace.workspace.replace_runtime_directory",
            side_effect=replace_then_advance,
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed during staging"):
                self.workspace._stage_resources(root, {"resources": source})

        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            previous,
        )

    def test_resource_view_resamples_coordinates_before_staging(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        real_files = self.workspace._resource_runtime_files
        sampled = False

        def advance_after_first_sample(path: Path) -> tuple[list[str], list[str]]:
            nonlocal sampled
            result = real_files(path)
            if not sampled:
                sampled = True
                (source / "catalog").mkdir()
                (source / "catalog" / "resources.json").write_text(
                    "new resource\n", encoding="utf-8"
                )
                (source / "runtime-paths.txt").write_text(
                    "catalog\n", encoding="utf-8"
                )
                command("git", "add", ".", cwd=source)
                command(
                    "git", "commit", "-m", "test: change resource allowlist", cwd=source
                )
            return result

        with mock.patch.object(
            self.workspace,
            "_resource_runtime_files",
            side_effect=advance_after_first_sample,
        ):
            output = self.workspace._stage_resources(root, {"resources": source})

        self.assertTrue((output / "catalog" / "resources.json").is_file())
        self.assertFalse((output / "paintings").exists())
        self.assertEqual(
            load_json(output / RUNTIME_INPUT_METADATA)["coordinate"]["head"],
            command("git", "rev-parse", "HEAD", cwd=source),
        )

    def test_resource_cache_hit_rechecks_source_after_validation(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        real_validate = self.workspace._validate_resource_view
        mutated = False

        def mutate_after_validation(
            path: Path,
            selected_source: Path,
            tracked: list[str],
            *,
            require_metadata: bool = True,
        ) -> None:
            nonlocal mutated
            real_validate(
                path,
                selected_source,
                tracked,
                require_metadata=require_metadata,
            )
            if not mutated:
                mutated = True
                (source / "local-race").write_text("dirty\n", encoding="utf-8")

        with mock.patch.object(
            self.workspace,
            "_validate_resource_view",
            side_effect=mutate_after_validation,
        ):
            self.workspace._stage_resources(root, {"resources": source})

        self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())

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

    def test_content_collection_reuses_only_exact_clean_valid_inputs(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        collections = 0

        def collect(arguments: list[str], **kwargs: object) -> str:
            nonlocal collections
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            collections += 1
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                f"collection {collections}\n",
            )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 1)

            (source / "README").write_text("new commit\n", encoding="utf-8")
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: change content", cwd=source)
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 2)

            dirty = source / "local-only"
            dirty.write_text("dirty\n", encoding="utf-8")
            self.workspace._collect_content(root, {"content": source})
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 4)
            self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())
            dirty.unlink()

            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 5)
            (output / RUNTIME_INPUT_METADATA).write_text("{", encoding="utf-8")
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 6)
            compatibility = output / "compatibility.json"
            corrupted = compatibility.read_bytes()
            compatibility.write_bytes(b"X" + corrupted[1:])
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 7)
            (output / MANAGED_MARKER).write_text("{", encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, "cannot read"):
                self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 7)
            atomic_json(
                output / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "collected-content"},
            )
            (output / "manifest.json").unlink()
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 8)

    def test_content_collection_race_preserves_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        mutate = False

        def collect(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                "generated\n",
            )
            if mutate:
                (source / "README").write_text(
                    "changed during collection\n", encoding="utf-8"
                )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            previous = (output / "manifest.json").read_text(encoding="utf-8")
            (source / "README").write_text("next commit\n", encoding="utf-8")
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: advance content", cwd=source)
            mutate = True
            try:
                with self.assertRaisesRegex(
                    WorkspaceError, "changed during collection"
                ):
                    self.workspace._collect_content(root, {"content": source})
            finally:
                command("git", "checkout", "--", "README", cwd=source)

        self.assertEqual(
            (output / "manifest.json").read_text(encoding="utf-8"), previous
        )

    def test_content_cache_hit_rechecks_source_after_validation(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        def collect(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                "content\n",
            )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            real_validate = self.workspace._validate_collected_content
            mutated = False

            def mutate_after_validation(
                path: Path,
                coordinate: dict[str, str],
                *,
                require_metadata: bool = True,
            ) -> None:
                nonlocal mutated
                real_validate(
                    path, coordinate, require_metadata=require_metadata
                )
                if not mutated:
                    mutated = True
                    (source / "local-race").write_text("dirty\n", encoding="utf-8")

            with mock.patch.object(
                self.workspace,
                "_validate_collected_content",
                side_effect=mutate_after_validation,
            ):
                self.workspace._collect_content(root, {"content": source})

        self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())

    def test_content_install_race_rolls_back_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        def collect(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                "content\n",
            )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            previous = (output / "manifest.json").read_text(encoding="utf-8")
            (source / "README").write_text("next commit\n", encoding="utf-8")
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: advance content", cwd=source)
            advanced = False

            def advance_after_metadata(path: Path, value: object) -> None:
                nonlocal advanced
                atomic_json(path, value)
                if path.name == RUNTIME_INPUT_METADATA and not advanced:
                    advanced = True
                    (source / "README").write_text(
                        "commit after metadata\n", encoding="utf-8"
                    )
                    command("git", "add", ".", cwd=source)
                    command(
                        "git",
                        "commit",
                        "-m",
                        "test: race after metadata",
                        cwd=source,
                    )

            with mock.patch(
                "atrinik_workspace.workspace.atomic_json",
                side_effect=advance_after_metadata,
            ):
                with self.assertRaisesRegex(
                    WorkspaceError, "changed during collection"
                ):
                    self.workspace._collect_content(root, {"content": source})

        self.assertEqual(
            (output / "manifest.json").read_text(encoding="utf-8"), previous
        )

    def test_topology_runtime_copies_are_independent_from_shared_cache(self) -> None:
        sources: dict[str, Path] = {}
        for name, purpose in (
            ("content", "collected-content"),
            ("resources", "resource-view"),
            ("client-maps", "region-map-cache"),
        ):
            source = self.root / f"shared-{name}-cache"
            source.mkdir()
            (source / "payload").write_text("shared\n", encoding="utf-8")
            atomic_json(
                source / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
            sources[name] = source
        first_root = self.root / "first-topology"
        second_root = self.root / "second-topology"
        first_root.mkdir()
        second_root.mkdir()
        specifications = tuple(
            (name, sources[name], purpose)
            for name, purpose in (
                ("content", "collected-content"),
                ("resources", "resource-view"),
                ("client-maps", "region-map-cache"),
            )
        )
        first_inputs = self.workspace._copy_topology_runtime_inputs(
            first_root, specifications
        )
        second_inputs = self.workspace._copy_topology_runtime_inputs(
            second_root, specifications
        )

        for name in sources:
            first = first_inputs[name]
            second = second_inputs[name]
            (first / "payload").write_text("first changed\n", encoding="utf-8")
            shutil.rmtree(first)

            self.assertEqual(
                (sources[name] / "payload").read_text(encoding="utf-8"),
                "shared\n",
            )
            self.assertEqual(
                (second / "payload").read_text(encoding="utf-8"), "shared\n"
            )

    def test_topology_runtime_set_copy_failure_preserves_all_snapshots(self) -> None:
        topology = self.root / "topology"
        runtime = topology / "runtime"
        runtime.mkdir(parents=True)
        sources: list[tuple[str, Path, str]] = []
        for name, purpose in (
            ("content", "collected-content"),
            ("resources", "resource-view"),
            ("client-maps", "region-map-cache"),
        ):
            source = self.root / f"shared-{name}"
            source.mkdir()
            (source / "payload").write_text("new\n", encoding="utf-8")
            atomic_json(
                source / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
            destination = runtime / name
            destination.mkdir()
            (destination / "payload").write_text("previous\n", encoding="utf-8")
            atomic_json(
                destination / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
            sources.append((name, source, purpose))
        status = topology / "status.json"
        status.write_text("previous status\n", encoding="utf-8")
        real_copy = self.workspace._copy_topology_runtime_tree
        copied = 0

        def fail_second_copy(
            source_path: Path, destination: Path
        ) -> int:
            nonlocal copied
            copied += 1
            if copied == 2:
                raise OSError("disk full")
            return real_copy(source_path, destination)

        with mock.patch.object(
            self.workspace,
            "_copy_topology_runtime_tree",
            side_effect=fail_second_copy,
        ):
            with self.assertRaisesRegex(OSError, "disk full"):
                self.workspace._copy_topology_runtime_inputs(
                    topology, tuple(sources)
                )

        for name, _source, _purpose in sources:
            self.assertEqual(
                (runtime / name / "payload").read_text(encoding="utf-8"),
                "previous\n",
            )
        self.assertEqual(status.read_text(encoding="utf-8"), "previous status\n")
        self.assertEqual(
            sorted(entry.name for entry in topology.iterdir()),
            ["runtime", "status.json"],
        )

    def test_topology_runtime_set_rejects_internal_links(self) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        source.mkdir()
        external = self.root / "external"
        external.write_text("private\n", encoding="utf-8")
        (source / "payload").symlink_to(external)
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )

        with self.assertRaisesRegex(WorkspaceError, "contains a"):
            self.workspace._copy_topology_runtime_inputs(
                topology,
                (("content", source, "collected-content"),),
            )

        self.assertEqual(list(topology.iterdir()), [])

    def test_topology_runtime_set_rejects_file_changed_to_link_during_copy(
        self,
    ) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        source.mkdir()
        payload = source / "payload"
        payload.write_text("shared\n", encoding="utf-8")
        external = self.root / "external"
        external.write_text("private\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        real_stat = os.stat
        changed = False

        def stat_then_change(
            path: object, *args: object, **kwargs: object
        ) -> os.stat_result:
            nonlocal changed
            result = real_stat(path, *args, **kwargs)
            if path == "payload" and kwargs.get("dir_fd") is not None and not changed:
                changed = True
                payload.unlink()
                payload.symlink_to(external)
            return result

        with mock.patch(
            "atrinik_workspace.workspace.os.stat", side_effect=stat_then_change
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed or contains a link"):
                self.workspace._copy_topology_runtime_inputs(
                    topology,
                    (("content", source, "collected-content"),),
                )

        self.assertEqual(list(topology.iterdir()), [])

    def test_topology_runtime_set_rejects_destination_directory_link_race(
        self,
    ) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        nested = source / "nested"
        nested.mkdir(parents=True)
        (nested / "payload").write_text("shared\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        external = self.root / "external"
        external.mkdir()
        (external / "sentinel").write_text("private\n", encoding="utf-8")
        real_open = os.open
        changed = False

        def open_after_change(
            path: object, flags: int, *args: object, **kwargs: object
        ) -> int:
            nonlocal changed
            directory_fd = kwargs.get("dir_fd")
            if (
                path == "nested"
                and isinstance(directory_fd, int)
                and flags & os.O_DIRECTORY
                and not changed
            ):
                parent = Path(f"/proc/self/fd/{directory_fd}").resolve()
                if ".runtime-" in str(parent):
                    changed = True
                    (parent / "nested").rmdir()
                    (parent / "nested").symlink_to(external, target_is_directory=True)
            return real_open(path, flags, *args, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.os.open", side_effect=open_after_change
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "staging destination changed"
            ):
                self.workspace._copy_topology_runtime_inputs(
                    topology,
                    (("content", source, "collected-content"),),
                )

        self.assertTrue(changed)
        self.assertEqual(
            (external / "sentinel").read_text(encoding="utf-8"), "private\n"
        )
        self.assertEqual(sorted(path.name for path in external.iterdir()), ["sentinel"])
        self.assertEqual(list(topology.iterdir()), [])

    def test_topology_runtime_set_copies_read_only_directories(self) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        nested = source / "nested"
        nested.mkdir(parents=True)
        (nested / "payload").write_text("shared\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        nested.chmod(0o555)
        source.chmod(0o555)

        copied = self.workspace._copy_topology_runtime_inputs(
            topology,
            (("content", source, "collected-content"),),
        )["content"]
        copied = self.workspace._copy_topology_runtime_inputs(
            topology,
            (("content", source, "collected-content"),),
        )["content"]

        self.assertEqual(
            (copied / "nested" / "payload").read_text(encoding="utf-8"),
            "shared\n",
        )
        self.assertEqual(stat.S_IMODE(copied.stat().st_mode), 0o555)
        self.assertEqual(stat.S_IMODE((copied / "nested").stat().st_mode), 0o555)

    def test_replace_directory_cleanup_failure_keeps_committed_output(self) -> None:
        output = self.root / "output"
        output.mkdir()
        (output / "first").write_text("delete first\n", encoding="utf-8")
        (output / "payload").write_text("previous\n", encoding="utf-8")
        staging = self.root / "staging"
        staging.mkdir()
        (staging / "payload").write_text("new\n", encoding="utf-8")
        real_unlink = os.unlink

        def fail_backup_cleanup(
            path: object, *args: object, **kwargs: object
        ) -> None:
            directory_fd = kwargs.get("dir_fd")
            parent = (
                Path(f"/proc/self/fd/{directory_fd}").resolve()
                if isinstance(directory_fd, int)
                else None
            )
            if path == "payload" and parent is not None and parent.name == "previous":
                raise PermissionError("read only")
            real_unlink(path, *args, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.os.unlink",
            side_effect=fail_backup_cleanup,
        ):
            workspace_replace_directory(output, staging, ".previous-")

            next_staging = self.root / "next-staging"
            next_staging.mkdir()
            (next_staging / "payload").write_text("next\n", encoding="utf-8")
            with self.assertRaisesRegex(
                WorkspaceError, "cannot recover replaced-directory transaction"
            ):
                workspace_replace_directory(output, next_staging, ".previous-")

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "new\n"
        )
        self.assertEqual(
            len(
                [
                    path
                    for path in self.root.iterdir()
                    if path.name.startswith(".previous-")
                ]
            ),
            2,
        )
        self.assertTrue((self.root / ".previous-pending.json").is_file())
        workspace_replace_directory(output, next_staging, ".previous-")
        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "next\n"
        )
        self.assertEqual(
            [
                path
                for path in self.root.iterdir()
                if path.name.startswith(".previous-")
            ],
            [],
        )

    def test_replace_directory_restores_interrupted_pending_output(self) -> None:
        output = self.root / "output"
        pending = self.root / ".previous-pending"
        previous = pending / "previous"
        previous.mkdir(parents=True)
        (previous / "payload").write_text("previous\n", encoding="utf-8")
        atomic_json(
            self.root / ".previous-pending.json",
            {
                "schema_version": 1,
                "purpose": "replaced-directory-backup",
                "output": "output",
                "phase": "prepared",
            },
        )
        missing_staging = self.root / "missing-staging"

        with self.assertRaises(FileNotFoundError):
            workspace_replace_directory(output, missing_staging, ".previous-")

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "previous\n"
        )
        self.assertFalse(pending.exists())
        self.assertFalse((self.root / ".previous-pending.json").exists())

    def test_replace_directory_restores_unverified_installed_output(self) -> None:
        output = self.root / "output"
        output.mkdir()
        (output / "payload").write_text("unverified\n", encoding="utf-8")
        pending = self.root / ".previous-pending"
        previous = pending / "previous"
        previous.mkdir(parents=True)
        (previous / "payload").write_text("previous\n", encoding="utf-8")
        atomic_json(
            self.root / ".previous-pending.json",
            {
                "schema_version": 1,
                "purpose": "replaced-directory-backup",
                "output": "output",
                "phase": "prepared",
            },
        )

        with self.assertRaises(FileNotFoundError):
            workspace_replace_directory(
                output, self.root / "missing-staging", ".previous-"
            )

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "previous\n"
        )
        self.assertFalse(pending.exists())
        self.assertFalse((self.root / ".previous-pending.json").exists())

    def test_owned_tree_removal_refuses_nested_mount_before_deletion(self) -> None:
        owned = self.root / "owned"
        first = owned / "a-first"
        first.mkdir(parents=True)
        first_payload = first / "payload"
        first_payload.write_text("preserve first\n", encoding="utf-8")
        nested = owned / "nested"
        nested.mkdir()
        nested.chmod(0o755)
        payload = nested / "payload"
        payload.write_text("preserve\n", encoding="utf-8")
        first.chmod(0o555)
        owned.chmod(0o555)
        owned_mode = stat.S_IMODE(owned.stat().st_mode)
        first_mode = stat.S_IMODE(first.stat().st_mode)
        original_mode = stat.S_IMODE(nested.stat().st_mode)

        with mock.patch(
            "atrinik_workspace.workspace._descriptor_mount_id",
            side_effect=[1] * 9 + [2],
        ):
            with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                remove_owned_tree(owned)

        self.assertEqual(payload.read_text(encoding="utf-8"), "preserve\n")
        self.assertEqual(
            first_payload.read_text(encoding="utf-8"), "preserve first\n"
        )
        self.assertEqual(stat.S_IMODE(owned.stat().st_mode), owned_mode)
        self.assertEqual(stat.S_IMODE(first.stat().st_mode), first_mode)
        self.assertEqual(stat.S_IMODE(nested.stat().st_mode), original_mode)

    def test_owned_tree_removal_uses_portable_mount_fallback(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        (owned / "payload").write_text("remove\n", encoding="utf-8")

        with (
            mock.patch("atrinik_workspace.workspace.sys.platform", "darwin"),
            mock.patch(
                "atrinik_workspace.workspace._darwin_descriptor_mount_id",
                return_value=(1, 2),
            ),
        ):
            remove_owned_tree(owned)

        self.assertFalse(owned.exists())

    def test_owned_tree_removal_checks_file_mounts_before_deletion(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        first = owned / "a-first"
        mounted = owned / "z-mounted"
        first.write_text("preserve first\n", encoding="utf-8")
        mounted.write_text("preserve mounted\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.workspace._descriptor_mount_id",
            side_effect=[1] * 6 + [2],
        ):
            with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                remove_owned_tree(owned)

        self.assertEqual(first.read_text(encoding="utf-8"), "preserve first\n")
        self.assertEqual(
            mounted.read_text(encoding="utf-8"), "preserve mounted\n"
        )

    def test_owned_tree_removal_does_not_require_procfs(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        (owned / "payload").write_text("remove\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.workspace.Path.read_text",
            side_effect=FileNotFoundError("no procfs"),
        ):
            remove_owned_tree(owned)

        self.assertFalse(owned.exists())

    @unittest.skipUnless(sys.platform == "linux", "requires Linux O_PATH")
    def test_owned_tree_removal_handles_unreadable_directories(self) -> None:
        real_open = os.open

        def deny_initial_read(
            path: object, flags: int, *args: object, **kwargs: object
        ) -> int:
            if path in {"owned", "nested", "unsupported"} and not (
                flags & os.O_PATH
            ):
                raise PermissionError(errno.EACCES, "permission denied")
            return real_open(path, flags, *args, **kwargs)

        readable = self.root / "readable"
        readable.mkdir()
        (readable / "payload").write_text("remove\n", encoding="utf-8")
        with mock.patch(
            "atrinik_workspace.workspace._linux_fchmod_path_descriptor",
            side_effect=WorkspaceError("fchmodat2 unavailable"),
        ) as fallback:
            remove_owned_tree(readable)
        fallback.assert_not_called()
        self.assertFalse(readable.exists())

        owned = self.root / "owned"
        nested = owned / "nested"
        nested.mkdir(parents=True)
        (nested / "payload").write_text("remove\n", encoding="utf-8")
        nested.chmod(0)
        owned.chmod(0)

        with mock.patch(
            "atrinik_workspace.workspace.os.open", side_effect=deny_initial_read
        ):
            remove_owned_tree(owned)

        self.assertFalse(owned.exists())

        unsupported = self.root / "unsupported"
        unsupported.mkdir()
        payload = unsupported / "payload"
        payload.write_text("preserve\n", encoding="utf-8")
        unsupported.chmod(0)
        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.os.open",
                    side_effect=deny_initial_read,
                ),
                mock.patch(
                    "atrinik_workspace.workspace._linux_fchmod_path_descriptor",
                    side_effect=WorkspaceError("fchmodat2 unavailable"),
                ),
            ):
                with self.assertRaisesRegex(WorkspaceError, "unavailable"):
                    remove_owned_tree(unsupported)
            self.assertEqual(stat.S_IMODE(unsupported.stat().st_mode), 0)
            unsupported.chmod(0o700)
            self.assertEqual(payload.read_text(encoding="utf-8"), "preserve\n")
        finally:
            unsupported.chmod(0o700)

    def test_owned_tree_removal_rejects_special_nodes_without_opening(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        fifo = owned / "fifo"
        os.mkfifo(fifo)

        with (
            mock.patch("atrinik_workspace.workspace.sys.platform", "darwin"),
            mock.patch(
                "atrinik_workspace.workspace._darwin_descriptor_mount_id",
                return_value=(1, 2),
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "entry is unsupported"):
                remove_owned_tree(owned)

        self.assertTrue(fifo.exists())

    def test_mount_identity_probes_fail_closed(self) -> None:
        class FakeFunction:
            def __init__(self, result: int, values: tuple[int, int] | None = None):
                self.result = result
                self.values = values

            def __call__(self, *arguments: object) -> int:
                if self.values is not None:
                    buffer = arguments[-1]._obj  # type: ignore[attr-defined]
                    ctypes.c_int32.from_buffer(buffer, 48).value = self.values[0]
                    ctypes.c_int32.from_buffer(buffer, 52).value = self.values[1]
                return self.result

        class FakeLibrary:
            def __init__(self, function: FakeFunction):
                self.fstatfs = function
                self.statx = function

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(0, (7, 9))),
        ):
            self.assertEqual(workspace_module._darwin_descriptor_mount_id(1), (7, 9))

        ctypes.set_errno(errno.EIO)
        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(-1)),
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect filesystem"):
                workspace_module._darwin_descriptor_mount_id(1)

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL", return_value=object()
        ):
            with self.assertRaisesRegex(WorkspaceError, "statx mount identity"):
                workspace_module._linux_descriptor_mount_id(1)

        ctypes.set_errno(errno.EIO)
        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(-1)),
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect filesystem"):
                workspace_module._linux_descriptor_mount_id(1)

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(0)),
        ):
            with self.assertRaisesRegex(WorkspaceError, "did not return"):
                workspace_module._linux_descriptor_mount_id(1)

        class SuccessfulStatx(FakeFunction):
            def __call__(self, *arguments: object) -> int:
                buffer = arguments[-1]._obj  # type: ignore[attr-defined]
                ctypes.c_uint32.from_buffer(buffer, 0).value = 0x1000
                ctypes.c_uint64.from_buffer(buffer, 144).value = 8675309
                return 0

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(SuccessfulStatx(0)),
        ):
            self.assertEqual(
                workspace_module._linux_descriptor_mount_id(1), 8675309
            )

        with mock.patch("atrinik_workspace.workspace.sys.platform", "freebsd"):
            with self.assertRaisesRegex(WorkspaceError, "unavailable on freebsd"):
                workspace_module._descriptor_mount_id(1)

    def test_owned_tree_removal_detects_descriptor_races(self) -> None:
        invalid = self.root / "invalid-removal-root"
        invalid.write_text("file\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "root is invalid"):
            remove_owned_tree(invalid)

        mounted = self.root / "mounted-removal-root"
        mounted.mkdir()
        mounted_payload = mounted / "payload"
        mounted_payload.write_text("preserve\n", encoding="utf-8")
        mounted_identity = mounted_payload.lstat()
        with mock.patch(
            "atrinik_workspace.workspace._descriptor_mount_id",
            side_effect=[1, 2],
        ):
            with self.assertRaisesRegex(WorkspaceError, "root changed or is mounted"):
                remove_owned_tree(mounted)
        self.assertEqual(
            (mounted_payload.read_text(encoding="utf-8"), mounted_payload.lstat().st_ino),
            ("preserve\n", mounted_identity.st_ino),
        )

        probe_root = self.root / "probe-open-race"
        probe_root.mkdir()
        probe_file = probe_root / "payload"
        probe_file.write_text("data\n", encoding="utf-8")
        probe_identity = probe_file.lstat()
        probe_descriptor = os.open(probe_root, os.O_RDONLY | os.O_DIRECTORY)
        try:
            with mock.patch(
                "atrinik_workspace.workspace.os.open",
                side_effect=OSError("changed"),
            ):
                with self.assertRaisesRegex(WorkspaceError, "entry changed"):
                    workspace_module._probe_owned_tree_entry_mount(
                        probe_descriptor,
                        "payload",
                        probe_file.stat(),
                        1,
                        probe_file,
                    )
        finally:
            os.close(probe_descriptor)
        self.assertEqual(
            (probe_file.read_text(encoding="utf-8"), probe_file.lstat().st_ino),
            ("data\n", probe_identity.st_ino),
        )

        boundary = self.root / "prepare-boundary"
        boundary.mkdir()
        boundary_payload = boundary / "payload"
        boundary_payload.write_text("preserve\n", encoding="utf-8")
        boundary.chmod(0o555)
        boundary_mode = stat.S_IMODE(boundary.stat().st_mode)
        boundary_identity = boundary_payload.lstat()
        boundary_descriptor = os.open(boundary, os.O_RDONLY | os.O_DIRECTORY)
        try:
            boundary_stat = os.fstat(boundary_descriptor)
            with self.assertRaisesRegex(WorkspaceError, "crossed a filesystem"):
                workspace_module._prepare_owned_tree_removal(
                    boundary_descriptor,
                    boundary_stat.st_dev + 1,
                    1,
                    boundary,
                )
        finally:
            os.close(boundary_descriptor)
        self.assertEqual(
            (
                boundary_payload.read_text(encoding="utf-8"),
                boundary_payload.lstat().st_ino,
            ),
            ("preserve\n", boundary_identity.st_ino),
        )
        self.assertEqual(stat.S_IMODE(boundary.stat().st_mode), boundary_mode)

        def changed_device(result: os.stat_result) -> os.stat_result:
            fields = list(result)
            fields[2] += 1
            return os.stat_result(fields)

        for operation in (
            workspace_module._prepare_owned_tree_removal,
            workspace_module._remove_owned_tree_contents,
        ):
            root = self.root / f"device-race-{operation.__name__}"
            root.mkdir()
            child = root / "payload"
            child.write_text("data\n", encoding="utf-8")
            root.chmod(0o555)
            root_mode = stat.S_IMODE(root.stat().st_mode)
            descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            try:
                root_stat = os.fstat(descriptor)
                with (
                    mock.patch(
                        "atrinik_workspace.workspace._descriptor_mount_id",
                        return_value=1,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace._probe_owned_tree_entry_mount"
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace.os.stat",
                        return_value=changed_device(child.stat()),
                    ),
                ):
                    with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                        operation(descriptor, root_stat.st_dev, 1, root)
            finally:
                os.close(descriptor)
            self.assertEqual(child.read_text(encoding="utf-8"), "data\n")
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), root_mode)

        real_open = os.open
        for operation in (
            workspace_module._prepare_owned_tree_removal,
            workspace_module._remove_owned_tree_contents,
        ):
            root = self.root / f"open-race-{operation.__name__}"
            nested = root / "nested"
            nested.mkdir(parents=True)
            nested_payload = nested / "payload"
            nested_payload.write_text("preserve\n", encoding="utf-8")
            nested.chmod(0o555)
            root.chmod(0o555)
            root_mode = stat.S_IMODE(root.stat().st_mode)
            nested_mode = stat.S_IMODE(nested.stat().st_mode)
            nested_identity = nested_payload.lstat()
            descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            root_stat = os.fstat(descriptor)

            def fail_nested_open(
                path: object, flags: int, *args: object, **kwargs: object
            ) -> int:
                if path == "nested" and kwargs.get("dir_fd") == descriptor:
                    raise OSError("changed")
                return real_open(path, flags, *args, **kwargs)

            try:
                with (
                    mock.patch(
                        "atrinik_workspace.workspace._descriptor_mount_id",
                        return_value=1,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace._probe_owned_tree_entry_mount"
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace.os.open",
                        side_effect=fail_nested_open,
                    ),
                ):
                    with self.assertRaisesRegex(WorkspaceError, "directory changed"):
                        operation(descriptor, root_stat.st_dev, 1, root)
            finally:
                os.close(descriptor)
            self.assertEqual(
                (
                    nested_payload.read_text(encoding="utf-8"),
                    nested_payload.lstat().st_ino,
                ),
                ("preserve\n", nested_identity.st_ino),
            )
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), root_mode)
            self.assertEqual(stat.S_IMODE(nested.stat().st_mode), nested_mode)

        for operation in (
            workspace_module._prepare_owned_tree_removal,
            workspace_module._remove_owned_tree_contents,
        ):
            root = self.root / f"identity-race-{operation.__name__}"
            nested = root / "nested"
            nested.mkdir(parents=True)
            nested_payload = nested / "payload"
            nested_payload.write_text("preserve\n", encoding="utf-8")
            nested.chmod(0o555)
            root.chmod(0o555)
            root_mode = stat.S_IMODE(root.stat().st_mode)
            nested_mode = stat.S_IMODE(nested.stat().st_mode)
            nested_identity = nested_payload.lstat()
            descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            root_stat = os.fstat(descriptor)
            mount_ids = (
                [1, 2]
                if operation is workspace_module._prepare_owned_tree_removal
                else [2]
            )
            try:
                with (
                    mock.patch(
                        "atrinik_workspace.workspace._descriptor_mount_id",
                        side_effect=mount_ids,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace._probe_owned_tree_entry_mount"
                    ),
                ):
                    with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                        operation(descriptor, root_stat.st_dev, 1, root)
            finally:
                os.close(descriptor)
            self.assertEqual(
                (
                    nested_payload.read_text(encoding="utf-8"),
                    nested_payload.lstat().st_ino,
                ),
                ("preserve\n", nested_identity.st_ino),
            )
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), root_mode)
            self.assertEqual(stat.S_IMODE(nested.stat().st_mode), nested_mode)

    def test_replaced_directory_recovery_rejects_invalid_states(self) -> None:
        def snapshot(parent: Path) -> dict[str, tuple[object, ...]]:
            result: dict[str, tuple[object, ...]] = {}
            for directory, dirnames, filenames in os.walk(
                parent, followlinks=False
            ):
                directory_path = Path(directory)
                for name in sorted([*dirnames, *filenames]):
                    entry = directory_path / name
                    relative = entry.relative_to(parent).as_posix()
                    metadata = entry.lstat()
                    if entry.is_symlink():
                        value: object = os.readlink(entry)
                    elif entry.is_file():
                        value = entry.read_bytes()
                    else:
                        value = None
                    result[relative] = (
                        stat.S_IFMT(metadata.st_mode),
                        stat.S_IMODE(metadata.st_mode),
                        metadata.st_ino,
                        value,
                    )
            return result

        def prepare(
            name: str,
            phase: str,
            *,
            output: str = "absent",
            backup: str = "absent",
        ) -> tuple[Path, Path, Path]:
            parent = self.root / name
            parent.mkdir()
            target = parent / "output"
            pending = parent / ".previous-pending"
            if output == "directory":
                target.mkdir()
            elif output == "symlink":
                external = parent / "external"
                external.mkdir()
                target.symlink_to(external, target_is_directory=True)
            if backup != "absent":
                pending.mkdir()
                if backup == "previous":
                    (pending / "previous").mkdir()
                elif backup == "previous-symlink":
                    external = parent / "backup-external"
                    external.mkdir()
                    (pending / "previous").symlink_to(
                        external, target_is_directory=True
                    )
                elif backup == "other":
                    (pending / "other").write_text("unexpected\n", encoding="utf-8")
            atomic_json(
                parent / ".previous-pending.json",
                {
                    "schema_version": 1,
                    "purpose": "replaced-directory-backup",
                    "output": "output",
                    "phase": phase,
                },
            )
            return target, pending, parent / ".previous-pending.json"

        unmanaged = self.root / "unmanaged"
        unmanaged.mkdir()
        (unmanaged / ".previous-pending").mkdir()
        unmanaged_before = snapshot(unmanaged)
        with self.assertRaisesRegex(WorkspaceError, "is not managed"):
            workspace_module.recover_replaced_directory(
                unmanaged / "output", ".previous-"
            )
        self.assertEqual(snapshot(unmanaged), unmanaged_before)

        invalid_link = self.root / "invalid-link"
        invalid_link.mkdir()
        link_target = invalid_link / "journal-target"
        link_target.write_text("{}", encoding="utf-8")
        (invalid_link / ".previous-pending.json").symlink_to(link_target)
        invalid_link_before = snapshot(invalid_link)
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            workspace_module.recover_replaced_directory(
                invalid_link / "output", ".previous-"
            )
        self.assertEqual(snapshot(invalid_link), invalid_link_before)

        invalid_json = self.root / "invalid-json"
        invalid_json.mkdir()
        atomic_json(invalid_json / ".previous-pending.json", [])
        invalid_json_before = snapshot(invalid_json)
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            workspace_module.recover_replaced_directory(
                invalid_json / "output", ".previous-"
            )
        self.assertEqual(snapshot(invalid_json), invalid_json_before)

        invalid_phase = self.root / "invalid-phase"
        invalid_phase.mkdir()
        atomic_json(
            invalid_phase / ".previous-pending.json",
            {
                "schema_version": 1,
                "purpose": "replaced-directory-backup",
                "output": "output",
                "phase": "unknown",
            },
        )
        invalid_phase_before = snapshot(invalid_phase)
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            workspace_module.recover_replaced_directory(
                invalid_phase / "output", ".previous-"
            )
        self.assertEqual(snapshot(invalid_phase), invalid_phase_before)

        cases = (
            (
                "initializing-nonempty",
                "initializing",
                "absent",
                "other",
                "backup is invalid",
            ),
            ("prepared-no-backup", "prepared", "absent", "absent", "backup is invalid"),
            (
                "previous-link",
                "prepared",
                "absent",
                "previous-symlink",
                "payload is invalid",
            ),
            (
                "prepared-output-link",
                "prepared",
                "symlink",
                "previous",
                "replacement is invalid",
            ),
            (
                "committed-no-output",
                "committed",
                "absent",
                "previous",
                "replacement is invalid",
            ),
            (
                "committed-empty",
                "committed",
                "absent",
                "empty",
                "replacement is invalid",
            ),
            ("prepared-empty", "prepared", "absent", "empty", "replacement is invalid"),
            ("unexpected-payload", "prepared", "directory", "other", "not empty"),
        )
        for name, phase, output, backup, message in cases:
            with self.subTest(name=name):
                target, _pending, _journal = prepare(
                    name, phase, output=output, backup=backup
                )
                before = snapshot(target.parent)
                with self.assertRaisesRegex(WorkspaceError, message):
                    workspace_module.recover_replaced_directory(
                        target, ".previous-"
                    )
                self.assertEqual(snapshot(target.parent), before)

        target, pending, journal = prepare(
            "committed-clean", "committed", output="directory"
        )
        workspace_module.recover_replaced_directory(target, ".previous-")
        self.assertFalse(pending.exists())
        self.assertFalse(journal.exists())

    def test_content_and_resource_validators_reject_malformed_trees(self) -> None:
        coordinate = {
            "repository": "atrinik/content",
            "branch": "main",
            "head": "a" * 40,
        }

        def content(name: str) -> Path:
            path = self.root / name
            path.mkdir()
            self.make_content_candidate(path, coordinate["head"], "content\n")
            atomic_json(
                path / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "collected-content"},
            )
            return path

        root_file = self.root / "content-file"
        root_file.write_text("bad\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_collected_content(
                root_file, coordinate, require_metadata=False
            )

        missing_directory = content("content-missing-directory")
        shutil.rmtree(missing_directory / "lib")
        with self.assertRaisesRegex(WorkspaceError, "required directory"):
            self.workspace._validate_collected_content(
                missing_directory, coordinate, require_metadata=False
            )

        invalid_manifest = content("content-invalid-manifest")
        atomic_json(invalid_manifest / "manifest.json", [])
        with self.assertRaisesRegex(WorkspaceError, "manifest is invalid"):
            self.workspace._validate_collected_content(
                invalid_manifest, coordinate, require_metadata=False
            )

        invalid_entry = content("content-invalid-entry")
        manifest = load_json(invalid_entry / "manifest.json")
        manifest["files"][0]["path"] = "../escape"
        atomic_json(invalid_entry / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "file entry is invalid"):
            self.workspace._validate_collected_content(
                invalid_entry, coordinate, require_metadata=False
            )

        linked = content("content-link")
        (linked / "linked").symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "contains a link"):
            self.workspace._validate_collected_content(
                linked, coordinate, require_metadata=False
            )

        extra = content("content-extra")
        (extra / "extra").write_text("extra\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "does not match"):
            self.workspace._validate_collected_content(
                extra, coordinate, require_metadata=False
            )

        wrong_size = content("content-size")
        manifest = load_json(wrong_size / "manifest.json")
        manifest["files"][0]["size"] += 1
        atomic_json(wrong_size / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "size does not match"):
            self.workspace._validate_collected_content(
                wrong_size, coordinate, require_metadata=False
            )

        unreadable = content("content-unreadable")
        real_lstat = Path.lstat
        walking = False

        def fail_content_lstat(candidate: Path) -> os.stat_result:
            if walking and candidate == unreadable / "compatibility.json":
                raise OSError("changed")
            return real_lstat(candidate)

        real_walk = os.walk

        def activate_content_walk(
            *arguments: object, **kwargs: object
        ) -> object:
            nonlocal walking
            walking = True
            yield from real_walk(*arguments, **kwargs)

        with (
            mock.patch(
                "atrinik_workspace.workspace.Path.lstat",
                autospec=True,
                side_effect=fail_content_lstat,
            ),
            mock.patch(
                "atrinik_workspace.workspace.os.walk",
                side_effect=activate_content_walk,
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect collected"):
                self.workspace._validate_collected_content(
                    unreadable, coordinate, require_metadata=False
                )

        special_content = content("content-special")
        fifo = special_content / "fifo"
        os.mkfifo(fifo)
        manifest = load_json(special_content / "manifest.json")
        manifest["files"].append(
            {"path": "fifo", "sha256": "0" * 64, "size": 0}
        )
        atomic_json(special_content / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "non-regular file"):
            self.workspace._validate_collected_content(
                special_content, coordinate, require_metadata=False
            )

        source = self.workspace.paths.repositories / "resources"

        def resource(name: str) -> Path:
            path = self.root / name
            (path / "paintings").mkdir(parents=True)
            shutil.copy2(
                source / "paintings" / "scene.jpg",
                path / "paintings" / "scene.jpg",
            )
            atomic_json(
                path / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "resource-view"},
            )
            return path

        resource_file = self.root / "resource-file"
        resource_file.write_text("bad\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_resource_view(
                resource_file,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        resource_link = resource("resource-link")
        (resource_link / "linked").symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "contains a link"):
            self.workspace._validate_resource_view(
                resource_link,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        resource_extra = resource("resource-extra")
        (resource_extra / "extra").write_text("extra\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "tracked file set"):
            self.workspace._validate_resource_view(
                resource_extra,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        resource_directory = resource("resource-directory")
        (resource_directory / "extra").mkdir()
        with self.assertRaisesRegex(WorkspaceError, "tracked directories"):
            self.workspace._validate_resource_view(
                resource_directory,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        unreadable_resource = resource("resource-unreadable")

        def fail_resource_lstat(candidate: Path) -> os.stat_result:
            if candidate == unreadable_resource / "paintings" / "scene.jpg":
                raise OSError("changed")
            return real_lstat(candidate)

        with mock.patch(
            "atrinik_workspace.workspace.Path.lstat",
            autospec=True,
            side_effect=fail_resource_lstat,
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect staged"):
                self.workspace._validate_resource_view(
                    unreadable_resource,
                    source,
                    ["paintings/scene.jpg"],
                    require_metadata=False,
                )

        special_resource = self.root / "resource-special"
        (special_resource / "paintings").mkdir(parents=True)
        os.mkfifo(special_resource / "paintings" / "fifo")
        atomic_json(
            special_resource / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "resource-view"},
        )
        with self.assertRaisesRegex(WorkspaceError, "non-regular file"):
            self.workspace._validate_resource_view(
                special_resource,
                source,
                ["paintings/fifo"],
                require_metadata=False,
            )

    def test_replace_directory_interrupted_journal_publish_is_retryable(
        self,
    ) -> None:
        output = self.root / "output"
        output.mkdir()
        (output / "payload").write_text("previous\n", encoding="utf-8")
        staging = self.root / "staging"
        staging.mkdir()
        (staging / "payload").write_text("new\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.workspace.atomic_json",
            side_effect=KeyboardInterrupt("interrupted"),
        ):
            with self.assertRaises(KeyboardInterrupt):
                workspace_replace_directory(output, staging, ".previous-")

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "previous\n"
        )
        self.assertFalse((self.root / ".previous-pending").exists())
        workspace_replace_directory(output, staging, ".previous-")
        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "new\n"
        )

    def test_topology_runtime_install_rejects_post_copy_replacement(self) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        source.mkdir()
        (source / "payload").write_text("shared\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        external = self.root / "external"
        external.write_text("private\n", encoding="utf-8")
        real_copy = self.workspace._copy_topology_runtime_tree

        def copy_then_replace(source_path: Path, destination: Path) -> int:
            descriptor = real_copy(source_path, destination)
            destination.replace(destination.with_name("copied-original"))
            destination.mkdir()
            (destination / "payload").symlink_to(external)
            atomic_json(
                destination / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "collected-content"},
            )
            return descriptor

        with mock.patch.object(
            self.workspace,
            "_copy_topology_runtime_tree",
            side_effect=copy_then_replace,
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "installed topology runtime input changed"
            ):
                self.workspace._copy_topology_runtime_inputs(
                    topology,
                    (("content", source, "collected-content"),),
                )

        self.assertEqual(list(topology.iterdir()), [])
        self.assertEqual(external.read_text(encoding="utf-8"), "private\n")

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
            "import os\n"
            "import sys\n"
            "binary = Path(__file__).resolve()\n"
            "counter = binary.with_name('worldmaker-count')\n"
            "count = int(counter.read_text()) + 1 if counter.exists() else 1\n"
            "counter.write_text(str(count))\n"
            "binary.with_name('worldmaker-bytecode').write_text("
            "os.environ.get('PYTHONDONTWRITEBYTECODE', ''))\n"
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
        self.assertEqual((binary / "worldmaker-bytecode").read_text(), "1")
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
        (build_root / "runtime" / "content" / "maps" / "map").write_text(
            "shared content\n", encoding="utf-8"
        )
        (build_root / "runtime" / "resources" / "resource").write_text(
            "shared resource\n", encoding="utf-8"
        )
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
        self.assertTrue(
            (build_root / "runtime" / "content" / "maps" / "map").is_file()
        )
        self.assertTrue(
            (build_root / "runtime" / "resources" / "resource").is_file()
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
        second_state = self.workspace.state_add("second", None)
        try:
            with (
                mock.patch.object(
                    self.workspace, "_build_resolved", return_value=build_root
                ),
                mock.patch.object(
                    self.workspace, "_select_topology_port", return_value=17301
                ),
            ):
                second = self.workspace.topology_up(
                    "server-review-two", "default", "second", ["server"], 17301
                )
            self.assertTrue(second["ready"])
            self.assertEqual(second["endpoint"]["port"], 17301)
            for topology in ("server-review", "server-review-two"):
                snapshot = self.workspace.paths.topologies / topology / "runtime"
                self.assertEqual(
                    (snapshot / "content" / "maps" / "map").read_text(),
                    "shared content\n",
                )
                self.assertEqual(
                    (snapshot / "resources" / "resource").read_text(),
                    "shared resource\n",
                )
            self.workspace.topology_down("server-review-two", timeout=5)
            second_content = (
                self.workspace.paths.topologies
                / "server-review-two"
                / "runtime"
                / "content"
            )
            shutil.rmtree(second_content)
            self.assertEqual(
                (
                    self.workspace.paths.topologies
                    / "server-review"
                    / "runtime"
                    / "content"
                    / "maps"
                    / "map"
                ).read_text(),
                "shared content\n",
            )
            self.assertEqual(
                (build_root / "runtime" / "content" / "maps" / "map").read_text(),
                "shared content\n",
            )
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
            second_remaining = self.workspace.topology_status("server-review-two")
            if second_remaining["supervisor"]["running"] or any(
                service["running"]
                for service in second_remaining["services"].values()
            ):
                self.workspace.topology_down("server-review-two", timeout=5)
            remaining = self.workspace.topology_status("server-review")
            if remaining["supervisor"]["running"] or any(
                service["running"] for service in remaining["services"].values()
            ):
                self.workspace.topology_down("server-review", timeout=5)

        with exclusive_lock(Path(f"{state}.lock"), "server state", nonblocking=True):
            pass
        with exclusive_lock(
            Path(f"{second_state}.lock"), "server state", nonblocking=True
        ):
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
