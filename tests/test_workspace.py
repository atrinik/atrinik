from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.workspace import Workspace, display_arguments


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

    def tearDown(self) -> None:
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

    def test_profile_rejects_checkout_for_another_component(self) -> None:
        self.workspace.create_profile("review")
        with self.assertRaisesRegex(WorkspaceError, "no origin/upstream"):
            self.workspace.set_profile(
                "review",
                "content",
                "path",
                str(self.workspace.paths.repositories / "client"),
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

    def test_redacts_join_passwords(self) -> None:
        displayed = display_arguments(
            ["server", "--join_password=secret", "--join-password", "also-secret"]
        )
        self.assertNotIn("secret", displayed)
        self.assertIn("<redacted>", displayed)


if __name__ == "__main__":
    unittest.main()
