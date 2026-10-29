from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import Manifest, WorkspaceError
from atrinik_workspace.supply_chain import Inventory, _git_repository_remote
from atrinik_workspace import workspace as workspace_module
from atrinik_workspace.workspace import Workspace, _remote_matches


ROOT = Path(__file__).resolve().parents[1]
TRANSFERS = {
    "editor": "seraphinastudios/tilewright-editor",
    "renderer": "seraphinastudios/tilewright-renderer",
}


def git(path: Path, *arguments: str) -> str:
    return subprocess.run(
        ["git", "-C", str(path), *arguments], check=True,
        capture_output=True, text=True,
    ).stdout.strip()


class TransferredRepositoryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        self.payload = json.loads((ROOT / "components.json").read_text())
        (self.wrapper / "components.json").write_text(json.dumps(self.payload))
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.root / "workspace")}
        )
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.workspace = Workspace(self.wrapper, backfill_references=False)
        self.addCleanup(self.workspace.close)

    def checkout(self, name: str, url: str) -> Path:
        path = self.wrapper / name
        path.mkdir()
        git(path, "init", "-b", "main")
        git(path, "-c", "user.name=Test", "-c", "user.email=test@example.invalid",
            "commit", "--allow-empty", "-m", "replacement fixture")
        git(path, "remote", "add", "origin", url)
        return path

    def test_manifest_keeps_logical_paths_profiles_and_classic_selection(self) -> None:
        manifest = self.workspace.manifest
        for name, repository in TRANSFERS.items():
            with self.subTest(name=name):
                checkout = manifest.by_checkout[name]
                self.assertEqual(checkout.repository, repository)
                self.assertEqual(checkout.path, name)
                self.assertEqual(manifest.provider("default", name).checkout, name)
        for role in ("client", "server", "protocol", "editor", "libatrinik"):
            component = manifest.provider("classic", role)
            self.assertEqual(component.checkout, "classic")
            self.assertEqual(component.repository, "atrinik/classic")
        self.assertEqual(manifest.by_checkout["classic"].path, "classic")
        self.assertTrue(set(TRANSFERS).isdisjoint(manifest.cohorts["classic"]))

    def test_manifest_transfer_exception_is_bound_to_exact_logical_checkout(self) -> None:
        for name, repository in (
            ("editor", TRANSFERS["renderer"]),
            ("renderer", TRANSFERS["editor"]),
            ("client", TRANSFERS["editor"]),
            ("editor", "seraphinastudios/editor"),
            ("editor", "seraphinastudios/tilewright-editor-fork"),
            ("editor", "foreign/tilewright-editor"),
            ("editor", "seraphinastudios/tilewright-edito"),
        ):
            with self.subTest(name=name, repository=repository):
                payload = copy.deepcopy(self.payload)
                next(row for row in payload["checkouts"]
                     if row["name"] == name)["repository"] = repository
                path = self.root / "invalid.json"
                path.write_text(json.dumps(payload))
                with self.assertRaisesRegex(WorkspaceError, "repository must name"):
                    Manifest.load(path)

    def test_exact_canonical_and_old_urls_match_only_their_expected_repository(self) -> None:
        for name, repository in TRANSFERS.items():
            for coordinate in (repository, f"atrinik/{name}"):
                for url in (
                    f"https://github.com/{coordinate}.git",
                    f"git@github.com:{coordinate}.git",
                    f"ssh://git@github.com/{coordinate}.git",
                ):
                    with self.subTest(url=url):
                        self.assertTrue(_remote_matches(url, repository))
                        other = TRANSFERS["renderer" if name == "editor" else "editor"]
                        self.assertFalse(_remote_matches(url, other))
            self.assertFalse(_remote_matches(
                f"https://github.com/{repository}.git", f"atrinik/{name}"
            ))

    def test_init_and_offline_status_accept_existing_checkout_without_rewriting_remote(self) -> None:
        for canonical in (False, True):
            for name, repository in TRANSFERS.items():
                coordinate = repository if canonical else f"atrinik/{name}"
                url = f"https://github.com/{coordinate}.git"
                path = self.wrapper / name
                if not path.exists():
                    self.checkout(name, url)
                else:
                    git(path, "remote", "set-url", "origin", url)
                before = (path / ".git/config").read_bytes()
                with self.subTest(name=name, canonical=canonical):
                    with mock.patch.object(workspace_module, "run", wraps=workspace_module.run) as run:
                        self.workspace.initialize([name], jobs=1)
                        row, = self.workspace.repository_status([name])
                    self.assertTrue(row["initialized"])
                    self.assertEqual(row["repository"], repository)
                    self.assertEqual(row["remote"], "origin")
                    self.assertEqual(_git_repository_remote(path, repository), "origin")
                    self.assertFalse(row["dirty"])
                    self.assertEqual(before, (path / ".git/config").read_bytes())
                    for call in run.call_args_list:
                        self.assertFalse({"clone", "fetch", "ls-remote"}.intersection(call.args[0]))

    def test_existing_checkout_rejects_foreign_host_fork_partial_and_cross_component_urls(self) -> None:
        for name, repository in TRANSFERS.items():
            path = self.checkout(name, f"https://github.com/{repository}.git")
            other = "renderer" if name == "editor" else "editor"
            for url in (
                f"https://github.example/{repository}.git",
                f"git@github.example:atrinik/{name}.git",
                f"https://github.com/foreign/tilewright-{name}.git",
                f"https://github.com/atrinik/{name}-fork.git",
                f"https://github.com/{repository[:-1]}.git",
                f"https://github.com/atrinik/{name[:-1]}.git",
                f"https://github.com/atrinik/{other}.git",
                f"https://github.com/{TRANSFERS[other]}.git",
            ):
                with self.subTest(name=name, url=url):
                    git(path, "remote", "set-url", "origin", url)
                    with self.assertRaisesRegex(WorkspaceError, "no origin/upstream"):
                        self.workspace.repository_status([name])
                    with self.assertRaisesRegex(WorkspaceError, "no origin/upstream"):
                        self.workspace.initialize([name], jobs=1)
                    with self.assertRaisesRegex(WorkspaceError, "no effective origin/upstream"):
                        _git_repository_remote(path, repository)

    def test_later_valid_fetch_url_does_not_override_foreign_first_url(self) -> None:
        for name, repository in TRANSFERS.items():
            path = self.checkout(name, "https://github.com/foreign/fork.git")
            git(path, "remote", "set-url", "--add", "origin",
                f"https://github.com/atrinik/{name}.git")
            git(path, "remote", "set-url", "--add", "origin",
                f"https://github.com/{repository}.git")
            with self.assertRaisesRegex(WorkspaceError, "no origin/upstream"):
                self.workspace.repository_status([name])
            with self.assertRaisesRegex(WorkspaceError, "no effective origin/upstream"):
                _git_repository_remote(path, repository)

    def test_current_supply_chain_inventory_uses_same_canonical_repositories(self) -> None:
        inventory = Inventory.load(ROOT / "supply-chain/inventory.json", ROOT / "components.json")
        inventory.validate_schema(ROOT / "supply-chain/schema.json")
        for name, repository in TRANSFERS.items():
            self.assertEqual(inventory.repositories_by_name[name].repository, repository)
        for name, repository in (
            ("client", TRANSFERS["editor"]),
            ("editor", TRANSFERS["renderer"]),
            ("editor", "seraphinastudios/tilewright-editor-fork"),
        ):
            with self.subTest(name=name, repository=repository):
                payload = json.loads((ROOT / "supply-chain/inventory.json").read_text())
                next(row for row in payload["repositories"]
                     if row["name"] == name)["repository"] = repository
                path = self.root / "invalid-inventory.json"
                path.write_text(json.dumps(payload))
                with self.assertRaisesRegex(WorkspaceError, "repository must name"):
                    Inventory.load(path, ROOT / "components.json")

    def test_fresh_clone_urls_use_canonical_coordinates_with_existing_transport(self) -> None:
        for name, repository in TRANSFERS.items():
            self.assertEqual(self.workspace._component_clone_url(
                self.workspace.manifest.by_checkout[name]),
                f"https://github.com/{repository}.git")
        git(self.wrapper, "init", "-b", "main")
        for template in (
            "https://github.com/atrinik/atrinik.git",
            "git@github.com:atrinik/atrinik.git",
            "ssh://git@github.com/atrinik/atrinik.git",
        ):
            git(self.wrapper, "config", "remote.origin.url", template)
            for name, repository in TRANSFERS.items():
                with self.subTest(template=template, name=name):
                    self.assertEqual(self.workspace._component_clone_url(
                        self.workspace.manifest.by_checkout[name]),
                        template.replace("atrinik/atrinik", repository))
                    self.assertFalse((self.wrapper / name).exists())


if __name__ == "__main__":
    unittest.main()
