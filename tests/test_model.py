from __future__ import annotations

import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import (
    Manifest,
    Paths,
    WorkspaceError,
    atomic_json,
    managed_reset,
    profile_key,
)


class ManifestTests(unittest.TestCase):
    def write_manifest(self, root: Path, value: object) -> Path:
        path = root / "components.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def valid_components(self) -> list[dict[str, str]]:
        return [
            {"name": name, "repository": f"atrinik/{name}", "branch": "master", "build": build}
            for name, build in (
                ("client", "client"),
                ("server", "server"),
                ("protocol", "protocol"),
                ("libatrinik", "library"),
                ("content", "content"),
                ("sound", "assets"),
                ("resources", "assets"),
            )
        ]

    def test_loads_strict_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(
                Path(temporary), {"schema_version": 1, "components": self.valid_components()}
            )
            manifest = Manifest.load(path)
            self.assertEqual(len(manifest.components), 7)
            self.assertEqual(manifest.by_name["server"].build, "server")

    def test_rejects_duplicate_json_keys(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "components.json"
            path.write_text(
                '{"schema_version":1,"schema_version":1,"components":[]}',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(WorkspaceError, "duplicate JSON key"):
                Manifest.load(path)

    def test_rejects_unknown_fields(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            components = self.valid_components()
            components[0]["extra"] = "no"
            path = self.write_manifest(
                Path(temporary), {"schema_version": 1, "components": components}
            )
            with self.assertRaisesRegex(WorkspaceError, "unexpected extra"):
                Manifest.load(path)

    def test_rejects_unhashable_build_kind_cleanly(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            components = self.valid_components()
            components[0]["build"] = []  # type: ignore[assignment]
            path = self.write_manifest(
                Path(temporary), {"schema_version": 1, "components": components}
            )
            with self.assertRaisesRegex(WorkspaceError, "build is invalid"):
                Manifest.load(path)


class ReleaseConfigurationTests(unittest.TestCase):
    def test_release_rules_preserve_conventional_commit_precedence(self) -> None:
        root = Path(__file__).resolve().parents[1]
        config = json.loads((root / ".releaserc.json").read_text(encoding="utf-8"))
        analyzer = config["plugins"][0]

        self.assertEqual(analyzer[0], "@semantic-release/commit-analyzer")
        self.assertEqual(
            analyzer[1]["releaseRules"],
            [
                {"breaking": True, "release": "major"},
                {"type": "feat", "release": "minor"},
                {"type": "*", "release": "patch"},
            ],
        )


class PathSafetyTests(unittest.TestCase):
    def test_refuses_nonempty_unmanaged_workspace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace = root / "external"
            workspace.mkdir()
            (workspace / "valuable").write_text("keep", encoding="utf-8")
            with mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(workspace)}):
                paths = Paths.discover(root / "wrapper")
                with self.assertRaisesRegex(WorkspaceError, "unmanaged non-empty"):
                    paths.ensure()
            self.assertEqual((workspace / "valuable").read_text(), "keep")

    def test_managed_reset_refuses_unmarked_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            builds = Path(temporary) / "build"
            target = builds / "profile"
            target.mkdir(parents=True)
            (target / "valuable").write_text("keep", encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, "unmanaged build path"):
                managed_reset(target, builds, "test")
            self.assertTrue((target / "valuable").is_file())

    def test_refuses_malformed_workspace_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace = root / "external"
            workspace.mkdir()
            atomic_json(workspace / ".atrinik-workspace.json", {"schema_version": 99})
            with mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(workspace)}):
                paths = Paths.discover(root / "wrapper")
                with self.assertRaisesRegex(WorkspaceError, "marker is invalid"):
                    paths.ensure()

    def test_refuses_workspace_that_contains_wrapper(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wrapper = root / "wrapper"
            wrapper.mkdir()
            with mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(root)}):
                paths = Paths.discover(wrapper)
                with self.assertRaisesRegex(WorkspaceError, "unsafe workspace path"):
                    paths.ensure()

    def test_refuses_symlinked_workspace_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace = root / "workspace"
            workspace.mkdir()
            actual = root / "marker.json"
            atomic_json(actual, {"schema_version": 1})
            (workspace / ".atrinik-workspace.json").symlink_to(actual)
            with mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(workspace)}):
                with self.assertRaisesRegex(WorkspaceError, "unmanaged non-empty"):
                    Paths.discover(root / "wrapper").ensure()

    def test_profile_key_is_unambiguous_for_paths_with_newlines(self) -> None:
        first = {"a": Path("/x\nb=/y"), "b": Path("/z")}
        second = {"a": Path("/x"), "b": Path("/y\nb=/z")}

        self.assertNotEqual(profile_key(first), profile_key(second))


if __name__ == "__main__":
    unittest.main()
