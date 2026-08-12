from __future__ import annotations

import copy
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
    managed_directory,
    managed_remove,
    managed_reset,
    profile_key,
)


ROOT = Path(__file__).resolve().parents[1]


class ManifestTests(unittest.TestCase):
    def write_manifest(self, root: Path, value: object) -> Path:
        path = root / "components.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def valid_components(self) -> list[dict[str, str]]:
        return [
            {"name": name, "repository": f"atrinik/{name}", "branch": "main", "build": build}
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

    def valid_v3_manifest(self) -> dict[str, object]:
        value = json.loads((ROOT / "components.json").read_text(encoding="utf-8"))
        by_name = {component["name"]: component for component in value["components"]}
        value["components"] = [
            by_name["content"],
            *(
                component
                for component in value["components"]
                if component["name"] != "content"
            ),
        ]
        return value

    def test_loads_strict_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(
                Path(temporary), {"schema_version": 1, "components": self.valid_components()}
            )
            manifest = Manifest.load(path)
            self.assertEqual(len(manifest.components), 7)
            self.assertEqual(manifest.by_name["server"].build, "classic-server")
            self.assertEqual(manifest.by_name["server"].checkout, "server")
            self.assertEqual(manifest.cohorts["default"][0], "client")

    def test_loads_v3_manifest_with_one_shared_content_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), self.valid_v3_manifest())
            manifest = Manifest.load(path)

            self.assertNotIn("content-1x", manifest.by_checkout)
            self.assertEqual(manifest.component_cohorts("content"), ("default",))
            self.assertEqual(
                manifest.component_stacks("content"), ("default", "classic")
            )
            self.assertEqual(
                manifest.provider("classic", "content").checkout, "content"
            )
            self.assertIn(
                "content",
                tuple(
                    component.name
                    for component in manifest.stack("classic").components
                ),
            )
            self.assertEqual(
                manifest.stack("classic").providers["content"].name,
                "content",
            )
            self.assertEqual(
                manifest.by_name["classic-server"].checkout_name, "classic"
            )
            self.assertEqual(manifest.by_name["classic-server"].checkout, "classic")
            self.assertEqual(manifest.by_name["classic-server"].source, "server")
            self.assertEqual(
                manifest.checkout_for("classic-server").repository,
                "atrinik/classic",
            )
            self.assertEqual(
                {checkout.name for checkout in manifest.cohort("classic")},
                {"classic", "playtester", "tools"},
            )
            self.assertIn(
                "content",
                [component.name for component in manifest.cohort("default")],
            )

    def test_v3_rejects_unsafe_and_overlapping_component_sources(self) -> None:
        for source, expected in (
            ("../server", "safe checkout-relative directory"),
            ("client", "component sources overlap in checkout classic"),
        ):
            with self.subTest(source=source):
                manifest = self.valid_v3_manifest()
                components = manifest["components"]
                self.assertIsInstance(components, list)
                server = next(
                    component
                    for component in components
                    if component["name"] == "classic-server"
                )
                server["source"] = source
                with tempfile.TemporaryDirectory() as temporary:
                    path = self.write_manifest(Path(temporary), manifest)
                    with self.assertRaisesRegex(WorkspaceError, expected):
                        Manifest.load(path)

    def test_v3_rejects_duplicate_repository_and_branch(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        self.assertIsInstance(checkouts, list)
        duplicate = copy.deepcopy(
            next(checkout for checkout in checkouts if checkout["name"] == "content")
        )
        duplicate.update({"name": "content-copy", "path": "content-copy"})
        checkouts.append(duplicate)
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "duplicate checkout repository and branch"
            ):
                Manifest.load(path)

    def test_v3_rejects_duplicate_checkout_paths(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        self.assertIsInstance(checkouts, list)
        duplicate = copy.deepcopy(
            next(checkout for checkout in checkouts if checkout["name"] == "content")
        )
        duplicate.update(
            {
                "name": "content-copy",
                "repository": "atrinik/content-copy",
            }
        )
        checkouts.append(duplicate)
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "duplicate checkout path"):
                Manifest.load(path)

    def test_v3_rejects_repository_variants_in_the_same_cohort_and_stack(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        components = manifest["components"]
        cohorts = manifest["cohorts"]
        stacks = manifest["stacks"]
        self.assertIsInstance(checkouts, list)
        self.assertIsInstance(components, list)
        self.assertIsInstance(cohorts, dict)
        self.assertIsInstance(stacks, dict)
        checkouts.append(  # type: ignore[union-attr]
            {
                "name": "content-1x",
                "repository": "atrinik/content",
                "branch": "1.x",
                "path": "content-1x",
                "generation": "shared",
                "license": "LicenseRef-Atrinik-Content",
            }
        )
        components.append(  # type: ignore[union-attr]
            {
                "name": "content-1x",
                "checkout": "content-1x",
                "source": ".",
                "build": "none",
                "generation": "shared",
                "provides": ["resources"],
                "requires": [],
                "license": "LicenseRef-Atrinik-Content",
            }
        )
        cohorts["default"].append("content-1x")  # type: ignore[union-attr]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "distinct cohort and stack membership"
            ):
                Manifest.load(path)

    def test_v3_rejects_unsafe_checkout_path(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        self.assertIsInstance(checkouts, list)
        checkouts[0]["path"] = "../content"  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "checkout 0.path must use"):
                Manifest.load(path)

    def test_v3_rejects_missing_cohort_membership(self) -> None:
        manifest = self.valid_v3_manifest()
        cohorts = manifest["cohorts"]
        self.assertIsInstance(cohorts, dict)
        cohorts["classic"] = []
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "lack cohort membership"):
                Manifest.load(path)

    def test_v3_rejects_provider_map_that_disagrees_with_contracts(self) -> None:
        manifest = self.valid_v3_manifest()
        stacks = manifest["stacks"]
        self.assertIsInstance(stacks, dict)
        stacks["default"]["providers"] = {"wrong-role": "content"}  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "providers does not match component contracts"
            ):
                Manifest.load(path)

    def test_v3_rejects_multiple_role_providers(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        components = manifest["components"]
        cohorts = manifest["cohorts"]
        stacks = manifest["stacks"]
        self.assertIsInstance(checkouts, list)
        self.assertIsInstance(components, list)
        self.assertIsInstance(cohorts, dict)
        self.assertIsInstance(stacks, dict)
        duplicate = copy.deepcopy(components[0])
        duplicate.update(
            {
                "name": "other-content",
                "checkout": "other-content",
                "source": ".",
                "generation": "replacement",
                "build_by_stack": {},
            }
        )
        checkouts.append(  # type: ignore[union-attr]
            {
                "name": "other-content",
                "repository": "atrinik/other-content",
                "branch": "main",
                "path": "other-content",
                "generation": duplicate["generation"],
                "license": duplicate["license"],
            }
        )
        components.append(duplicate)
        cohorts["default"].append("other-content")  # type: ignore[union-attr]
        stacks["default"]["components"].append("other-content")  # type: ignore[index, union-attr]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "multiple providers for role content"):
                Manifest.load(path)

    def test_v3_rejects_unsatisfied_required_role(self) -> None:
        manifest = self.valid_v3_manifest()
        components = manifest["components"]
        self.assertIsInstance(components, list)
        components[0]["requires"] = ["libatrinik"]  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "cannot satisfy required role libatrinik"):
                Manifest.load(path)

    def test_v3_rejects_incompatible_implementation_roles(self) -> None:
        manifest = self.valid_v3_manifest()
        components = manifest["components"]
        stacks = manifest["stacks"]
        self.assertIsInstance(components, list)
        self.assertIsInstance(stacks, dict)
        components[0]["provides"] = ["content", "server"]  # type: ignore[index]
        stacks["default"]["providers"]["server"] = "content"  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "incompatible implementation roles"
            ):
                Manifest.load(path)

    def test_v3_rejects_unknown_logical_roles(self) -> None:
        manifest = self.valid_v3_manifest()
        components = manifest["components"]
        stacks = manifest["stacks"]
        self.assertIsInstance(components, list)
        self.assertIsInstance(stacks, dict)
        components[0]["provides"] = ["contennt"]  # type: ignore[index]
        stacks["default"]["providers"] = {"contennt": "content"}  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "unknown logical roles: contennt"):
                Manifest.load(path)

    def test_v3_rejects_component_claiming_editor_and_renderer(self) -> None:
        manifest = self.valid_v3_manifest()
        components = manifest["components"]
        self.assertIsInstance(components, list)
        components[0]["provides"] = ["editor", "renderer"]  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "incompatible implementation roles"
            ):
                Manifest.load(path)

    def test_v3_rejects_shared_implementation_provider_across_stacks(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        components = manifest["components"]
        cohorts = manifest["cohorts"]
        stacks = manifest["stacks"]
        self.assertIsInstance(checkouts, list)
        self.assertIsInstance(components, list)
        self.assertIsInstance(cohorts, dict)
        self.assertIsInstance(stacks, dict)
        checkouts.append(
            {
                "name": "shared-server",
                "repository": "atrinik/shared-server",
                "branch": "main",
                "path": "shared-server",
                "generation": "shared",
                "license": "MIT",
            }
        )
        components.append(
            {
                "name": "shared-server",
                "checkout": "shared-server",
                "source": ".",
                "build": "none",
                "generation": "shared",
                "provides": ["server"],
                "requires": [],
                "license": "MIT",
            }
        )
        cohorts["default"].append("shared-server")  # type: ignore[index, union-attr]
        for stack_name in ("default", "classic"):
            stacks[stack_name]["components"].append("shared-server")  # type: ignore[index, union-attr]
            stacks[stack_name]["providers"]["server"] = "shared-server"  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "shared component claims implementation role: server"
            ):
                Manifest.load(path)

    def test_v3_rejects_classic_adapter_on_replacement_component(self) -> None:
        manifest = self.valid_v3_manifest()
        components = manifest["components"]
        self.assertIsInstance(components, list)
        components[0]["build_by_stack"] = {"default": "classic-server"}  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError,
                "effective build classic-server is incompatible with default stack",
            ):
                Manifest.load(path)

    def test_v3_rejects_unknown_redundant_and_unused_stack_build_overrides(self) -> None:
        for override, message, component_name in (
            ({"future": "assets"}, "unknown stack future", "content"),
            ({"default": "none"}, "redundantly repeats build", "content"),
            ({"default": "assets"}, "targets stacks where the component is absent", "classic-client"),
        ):
            with self.subTest(override=override):
                manifest = self.valid_v3_manifest()
                component = next(
                    row
                    for row in manifest["components"]  # type: ignore[index]
                    if row["name"] == component_name
                )
                component["build_by_stack"] = override
                with tempfile.TemporaryDirectory() as temporary:
                    path = self.write_manifest(Path(temporary), manifest)
                    with self.assertRaisesRegex(WorkspaceError, message):
                        Manifest.load(path)

    def test_v3_rejects_stack_adapter_for_an_unprovided_role(self) -> None:
        manifest = self.valid_v3_manifest()
        content = next(
            row
            for row in manifest["components"]  # type: ignore[index]
            if row["name"] == "content"
        )
        content["build_by_stack"] = {"classic": "classic-server"}
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "requires provided role server"
            ):
                Manifest.load(path)

    def test_v3_rejects_generation_mixing(self) -> None:
        manifest = self.valid_v3_manifest()
        stacks = manifest["stacks"]
        self.assertIsInstance(stacks, dict)
        stacks["default"]["components"].append("classic-client")  # type: ignore[index, union-attr]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "mixes classic component classic-client"):
                Manifest.load(path)

    def test_v3_rejects_dependency_cycles(self) -> None:
        manifest = self.valid_v3_manifest()
        components = manifest["components"]
        cohorts = manifest["cohorts"]
        stacks = manifest["stacks"]
        self.assertIsInstance(components, list)
        self.assertIsInstance(cohorts, dict)
        self.assertIsInstance(stacks, dict)
        components[0]["requires"] = ["server"]  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(WorkspaceError, "dependency cycle"):
                Manifest.load(path)

    def test_v3_rejects_missing_required_default_provider(self) -> None:
        manifest = self.valid_v3_manifest()
        checkouts = manifest["checkouts"]
        components = manifest["components"]
        cohorts = manifest["cohorts"]
        stacks = manifest["stacks"]
        self.assertIsInstance(checkouts, list)
        self.assertIsInstance(components, list)
        self.assertIsInstance(cohorts, dict)
        self.assertIsInstance(stacks, dict)
        components[:] = [
            component for component in components if component["name"] != "server"
        ]
        checkouts[:] = [
            checkout for checkout in checkouts if checkout["name"] != "server"
        ]
        cohorts["default"].remove("server")  # type: ignore[index, union-attr]
        stacks["default"]["components"].remove("server")  # type: ignore[index, union-attr]
        del stacks["default"]["providers"]["server"]  # type: ignore[index]
        with tempfile.TemporaryDirectory() as temporary:
            path = self.write_manifest(Path(temporary), manifest)
            with self.assertRaisesRegex(
                WorkspaceError, "cohorts.default lacks required checkouts: server"
            ):
                Manifest.load(path)

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

    def test_v1_rejects_duplicate_repository(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            components = self.valid_components()
            components[1]["repository"] = components[0]["repository"]
            path = self.write_manifest(
                Path(temporary), {"schema_version": 1, "components": components}
            )
            with self.assertRaisesRegex(WorkspaceError, "duplicate component identity"):
                Manifest.load(path)


class ReleaseConfigurationTests(unittest.TestCase):
    def test_release_rules_preserve_conventional_commit_precedence(self) -> None:
        root = Path(__file__).resolve().parents[1]
        config = json.loads((root / ".releaserc.json").read_text(encoding="utf-8"))
        analyzer = config["plugins"][0]

        self.assertEqual(config["branches"], ["main"])
        self.assertEqual(analyzer[0], "@semantic-release/commit-analyzer")
        self.assertEqual(
            analyzer[1]["releaseRules"],
            [
                {"breaking": True, "release": "major"},
                {"type": "feat", "release": "minor"},
                {"type": "*", "release": "patch"},
            ],
        )

    def test_every_component_checkout_is_ignored_at_wrapper_root(self) -> None:
        root = Path(__file__).resolve().parents[1]
        manifest = Manifest.load(root / "components.json")
        ignored = set((root / ".gitignore").read_text(encoding="utf-8").splitlines())

        self.assertTrue(
            {f"/{checkout.path}/" for checkout in manifest.checkouts} <= ignored
        )
        self.assertIn("/classic/", ignored)
        self.assertIn("/libatrinik/", ignored)

    def test_repository_manifest_separates_default_and_classic_cohorts(self) -> None:
        root = Path(__file__).resolve().parents[1]
        manifest = Manifest.load(root / "components.json")

        self.assertEqual(
            set(manifest.cohorts["default"]),
            {
                "client",
                "server",
                "protocol",
                "editor",
                "renderer",
                "content-toolkit",
                "website",
                "content",
                "sound",
                "resources",
                "metaserver-worker",
                "devcontainer",
                "github-settings",
            },
        )
        self.assertEqual(
            set(manifest.cohorts["classic"]),
            {
                "classic",
                "playtester",
                "tools",
            },
        )
        self.assertEqual(manifest.by_name["content"].branch, "main")
        self.assertEqual(manifest.effective_build("default", "content"), "none")
        self.assertEqual(
            manifest.effective_build("classic", "content"), "classic-content"
        )
        self.assertEqual(
            manifest.stack("classic").providers["libatrinik"].name,
            "classic-libatrinik",
        )
        self.assertEqual(
            manifest.stack("classic").providers["playtester"].name,
            "playtester",
        )
        self.assertEqual(
            manifest.by_name["playtester"].requires,
            ("content", "libatrinik", "protocol"),
        )
        self.assertEqual(
            {
                role: manifest.stack("default").providers[role].name
                for role in ("client", "server", "protocol", "content")
            },
            {
                "client": "client",
                "server": "server",
                "protocol": "protocol",
                "content": "content",
            },
        )
        self.assertEqual(
            {
                role: manifest.stack("classic").providers[role].name
                for role in ("client", "server", "protocol", "libatrinik", "content")
            },
            {
                "client": "classic-client",
                "server": "classic-server",
                "protocol": "classic-protocol",
                "libatrinik": "classic-libatrinik",
                "content": "content",
            },
        )
        self.assertFalse(
            any(
                component.license.startswith("GPL-")
                for component in manifest.cohort("default")
            )
        )


class PathSafetyTests(unittest.TestCase):
    def test_component_repositories_stay_beside_wrapper(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wrapper = root / "wrapper"
            workspace = root / "external"
            wrapper.mkdir()
            with mock.patch.dict(os.environ, {"ATRINIK_WORKSPACE_DIR": str(workspace)}):
                paths = Paths.discover(wrapper)
                paths.ensure()

            self.assertEqual(paths.repositories, wrapper.resolve())
            self.assertEqual(paths.workspace, workspace.resolve())
            self.assertFalse((workspace / "repos").exists())
            self.assertTrue((workspace / "worktrees").is_dir())

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

    def test_managed_reset_refuses_symlinked_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            builds = Path(temporary) / "build"
            real = builds / "real"
            target = builds / "profile"
            managed_directory(real, builds, "test")
            target.symlink_to(real, target_is_directory=True)

            with self.assertRaisesRegex(
                WorkspaceError, "symlinked managed build path"
            ):
                managed_reset(target, builds, "test")

            self.assertTrue(real.is_dir())

    def test_managed_remove_revalidates_marker_purpose_and_containment(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            builds = root / "build"
            target = builds / "profiles" / "review"
            managed_directory(target, builds, "profile:review:key")
            (target / "artifact").write_text("generated", encoding="utf-8")

            with self.assertRaisesRegex(WorkspaceError, "does not match"):
                managed_remove(target, builds, "profile:other:key")
            self.assertTrue((target / "artifact").is_file())

            managed_remove(target, builds, "profile:review:key")
            self.assertFalse(target.exists())

            outside = root / "outside"
            managed_directory(outside, root, "outside")
            with self.assertRaisesRegex(WorkspaceError, "outside workspace builds"):
                managed_remove(outside, builds, "outside")
            self.assertTrue(outside.is_dir())

    def test_managed_remove_rejects_a_symlinked_build_container(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            external = root / "external"
            external.mkdir()
            target = external / "profiles" / "review"
            managed_directory(target, external, "profile:review:key")
            builds = root / "build"
            builds.symlink_to(external, target_is_directory=True)

            with self.assertRaisesRegex(WorkspaceError, "not a regular directory"):
                managed_remove(
                    builds / "profiles" / "review",
                    builds,
                    "profile:review:key",
                )

            self.assertTrue(target.is_dir())

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

    def test_profile_key_namespace_separates_schema_and_stack_generations(self) -> None:
        paths = {"resources": Path("/workspace/resources")}

        self.assertNotEqual(
            profile_key(paths),
            profile_key(
                paths,
                namespace=(
                    "profile-schema:3;stack:default;generation:replacement;"
                    "providers:resources=resources"
                ),
            ),
        )
        self.assertNotEqual(
            profile_key(
                paths,
                namespace=(
                    "profile-schema:3;stack:default;generation:replacement;"
                    "providers:resources=resources"
                ),
            ),
            profile_key(
                paths,
                namespace=(
                    "profile-schema:3;stack:classic;generation:classic;"
                    "providers:resources=resources"
                ),
            ),
        )


if __name__ == "__main__":
    unittest.main()
