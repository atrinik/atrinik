from __future__ import annotations

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "scripts" / "components.py"
SPEC = importlib.util.spec_from_file_location("atrinik_components", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
dependencies = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dependencies)


class DependencyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        (self.root / "client").mkdir()
        (self.root / "build").mkdir()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def make_archive(self, members: list[tuple[str, bytes, str]]) -> Path:
        path = self.root / "asset.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            for name, contents, kind in members:
                info = tarfile.TarInfo(name)
                if kind == "file":
                    info.size = len(contents)
                    archive.addfile(info, io.BytesIO(contents))
                elif kind == "symlink":
                    info.type = tarfile.SYMTYPE
                    info.linkname = contents.decode()
                    archive.addfile(info)
                else:
                    raise AssertionError(kind)
        return path

    def dependency(self, archive: Path) -> dict[str, object]:
        return {
            "name": "sound",
            "repository": "atrinik/sound",
            "tag": "v1.0.0",
            "commit": "1" * 40,
            "url": archive.as_uri(),
            "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
            "destination": "client/sound",
            "strip_components": 1,
        }

    def test_installs_and_verifies_pinned_archive(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/effects/test.ogg", b"sound", "file")])
        dependency = self.dependency(archive)
        status = dependencies.install_dependency(
            self.root,
            self.root / "build/cache",
            dependency,
        )
        self.assertEqual(status, "installed")
        self.assertEqual((self.root / "client/sound/effects/test.ogg").read_bytes(), b"sound")
        dependencies.verify_dependency(self.root, dependency)
        self.assertEqual(
            dependencies.install_dependency(self.root, self.root / "build/cache", dependency),
            "current",
        )

    def test_refuses_unmanaged_destination(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test.ogg", b"sound", "file")])
        (self.root / "client/sound").mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "unmanaged"):
            dependencies.install_dependency(
                self.root,
                self.root / "build/cache",
                self.dependency(archive),
            )

    def test_refuses_malformed_management_marker(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test.ogg", b"sound", "file")])
        destination = self.root / "client/sound"
        destination.mkdir()
        (destination / dependencies.MARKER_NAME).write_text("{}\n")
        with self.assertRaisesRegex(dependencies.DependencyError, "invalid managed"):
            dependencies.install_dependency(
                self.root,
                self.root / "build/cache",
                self.dependency(archive),
            )

    def test_rejects_parent_traversal(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/../../escape", b"bad", "file")])
        staging = self.root / "staging"
        staging.mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "unsafe archive member"):
            dependencies.extract_archive(archive, staging, 1)

    def test_rejects_symbolic_links(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/link", b"../../escape", "symlink")])
        staging = self.root / "staging"
        staging.mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "unsupported archive member"):
            dependencies.extract_archive(archive, staging, 1)

    def test_rejects_hard_links(self) -> None:
        archive = self.root / "hard-link.tar.gz"
        with tarfile.open(archive, "w:gz") as stream:
            target = tarfile.TarInfo("sound-v1.0.0/target")
            target.size = 2
            stream.addfile(target, io.BytesIO(b"ok"))
            link = tarfile.TarInfo("sound-v1.0.0/link")
            link.type = tarfile.LNKTYPE
            link.linkname = "sound-v1.0.0/target"
            stream.addfile(link)
        staging = self.root / "staging"
        staging.mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "unsupported archive member"):
            dependencies.extract_archive(archive, staging, 1)

    def test_rejects_archive_without_files(self) -> None:
        path = self.root / "empty.tar.gz"
        with tarfile.open(path, "w:gz") as archive:
            info = tarfile.TarInfo("sound-v1.0.0/effects")
            info.type = tarfile.DIRTYPE
            archive.addfile(info)
        staging = self.root / "staging"
        staging.mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "contains no files"):
            dependencies.extract_archive(path, staging, 1)

    def test_rejects_windows_path_separators(self) -> None:
        archive = self.make_archive([("sound-v1.0.0\\..\\escape", b"bad", "file")])
        staging = self.root / "staging"
        staging.mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "unsafe archive member"):
            dependencies.extract_archive(archive, staging, 1)

    def test_rejects_case_colliding_paths(self) -> None:
        archive = self.make_archive(
            [
                ("sound-v1.0.0/A.ogg", b"a", "file"),
                ("sound-v1.0.0/a.ogg", b"b", "file"),
            ]
        )
        staging = self.root / "staging"
        staging.mkdir()
        with self.assertRaisesRegex(dependencies.DependencyError, "duplicate archive output"):
            dependencies.extract_archive(archive, staging, 1)

    def test_lock_rejects_duplicate_keys(self) -> None:
        lock = self.root / "lock.json"
        lock.write_text('{"schema_version": 1, "schema_version": 1, "dependencies": []}')
        with self.assertRaisesRegex(dependencies.DependencyError, "duplicate JSON key"):
            dependencies.load_lock(lock, allow_file_urls=True)

    def test_loads_strict_lock(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test", b"ok", "file")])
        lock = self.root / "lock.json"
        lock.write_text(
            json.dumps({"schema_version": 1, "components": [self.dependency(archive)]})
        )
        loaded = dependencies.load_lock(lock, allow_file_urls=True)
        self.assertEqual(loaded[0]["name"], "sound")

    def test_rejects_release_url_for_another_repository(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test", b"ok", "file")])
        dependency = self.dependency(archive)
        dependency["url"] = (
            "https://github.com/atrinik/resources/releases/download/"
            "v1.0.0/atrinik-sound-1.0.0.tar.gz"
        )
        lock = self.root / "lock.json"
        lock.write_text(json.dumps({"schema_version": 1, "components": [dependency]}))
        with self.assertRaisesRegex(dependencies.DependencyError, "repository and tag"):
            dependencies.load_lock(lock)

    def test_rejects_repository_outside_atrinik_organization(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test", b"ok", "file")])
        dependency = self.dependency(archive)
        dependency["repository"] = "someone/sound"
        dependency["url"] = (
            "https://github.com/someone/sound/releases/download/"
            "v1.0.0/atrinik-sound-1.0.0.tar.gz"
        )
        lock = self.root / "lock.json"
        lock.write_text(json.dumps({"schema_version": 1, "components": [dependency]}))
        with self.assertRaisesRegex(dependencies.DependencyError, "atrinik organization"):
            dependencies.load_lock(lock)

    def test_rejects_malformed_https_port(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test", b"ok", "file")])
        dependency = self.dependency(archive)
        dependency["url"] = (
            "https://github.com:not-a-port/atrinik/sound/releases/"
            "download/v1.0.0/atrinik-sound-1.0.0.tar.gz"
        )
        lock = self.root / "lock.json"
        lock.write_text(json.dumps({"schema_version": 1, "components": [dependency]}))
        with self.assertRaisesRegex(dependencies.DependencyError, "invalid port"):
            dependencies.load_lock(lock)

    def test_rejects_noncanonical_destination(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test", b"ok", "file")])
        dependency = self.dependency(archive)
        dependency["destination"] = "client//sound"
        lock = self.root / "lock.json"
        lock.write_text(
            json.dumps({"schema_version": 1, "components": [dependency]})
        )
        with self.assertRaisesRegex(dependencies.DependencyError, "canonical"):
            dependencies.load_lock(lock, allow_file_urls=True)

    def test_rejects_case_colliding_destinations(self) -> None:
        archive = self.make_archive([("sound-v1.0.0/test", b"ok", "file")])
        first = self.dependency(archive)
        second = {
            **first,
            "name": "music",
            "destination": "client/Sound",
        }
        lock = self.root / "lock.json"
        lock.write_text(
            json.dumps({"schema_version": 1, "components": [first, second]})
        )
        with self.assertRaisesRegex(dependencies.DependencyError, "duplicate"):
            dependencies.load_lock(lock, allow_file_urls=True)

    def test_consumer_locks_must_match_integration_pins(self) -> None:
        def pin(
            name: str,
            repository: str,
            destination: str,
            *,
            runtime: bool = False,
        ) -> dict[str, str]:
            suffix = "-runtime" if runtime else ""
            return {
                "name": name,
                "repository": repository,
                "tag": "v1.0.0",
                "commit": "1" * 40,
                "url": (
                    f"https://github.com/{repository}/releases/download/"
                    f"v1.0.0/{name}-1.0.0{suffix}.tar.gz"
                ),
                "sha256": "2" * 64,
                "destination": destination,
            }

        protocol = pin(
            "protocol", "atrinik/protocol", "build/components/protocol"
        )
        library = pin(
            "libatrinik", "atrinik/libatrinik", "build/components/libatrinik"
        )
        sound = pin(
            "sound", "atrinik/sound", "build/components/client/sound"
        )
        content = pin(
            "content",
            "atrinik/content",
            "build/components/server/runtime/content",
            runtime=True,
        )
        resources = pin(
            "resources",
            "atrinik/resources",
            "build/components/server/resources",
        )
        integration = [protocol, library, sound, content, resources]
        for name in ("client", "server"):
            destination = f"build/components/{name}"
            integration.append(
                {
                    "name": name,
                    "repository": f"atrinik/{name}",
                    "tag": "v5.1.0",
                    "commit": "3" * 40,
                    "url": (
                        f"https://github.com/atrinik/{name}/releases/download/"
                        f"v5.1.0/atrinik-{name}-5.1.0.tar.gz"
                    ),
                    "sha256": "4" * 64,
                    "destination": destination,
                }
            )
            component = self.root / destination
            (component / "cmake").mkdir(parents=True)
            runtime_lock = {
                "schema_version": 1,
                "dependencies": (
                    [sound] if name == "client" else [content, resources]
                ),
            }
            cmake_lock = {
                "schema_version": 1,
                "dependencies": {
                    "atrinik_protocol": protocol,
                    "libatrinik": library,
                },
            }
            (component / "dependencies.lock.json").write_text(
                json.dumps(runtime_lock)
            )
            (component / "cmake/dependencies.lock.json").write_text(
                json.dumps(cmake_lock)
            )

        dependencies.verify_consumer_locks(self.root, integration)
        protocol["tag"] = "v1.0.1"
        with self.assertRaisesRegex(dependencies.DependencyError, "tag differs"):
            dependencies.verify_consumer_locks(self.root, integration)
        protocol["tag"] = "v1.0.0"

        client_lock = self.root / "build/components/client/dependencies.lock.json"
        client_lock.write_text(
            json.dumps({"schema_version": 1, "dependencies": [protocol]})
        )
        with self.assertRaisesRegex(
            dependencies.DependencyError, "do not declare required dependency sound"
        ):
            dependencies.verify_consumer_locks(self.root, integration)

    def test_reinstalls_nested_dependency_after_parent_changes(self) -> None:
        nested_archive = self.make_archive(
            [("sound-v1.0.0/effects/test.ogg", b"good", "file")]
        )
        nested = self.dependency(nested_archive)
        nested["destination"] = "build/components/client/sound"
        injected_marker = json.dumps(dependencies.marker_for(nested)).encode()
        parent_archive = self.root / "client.tar.gz"
        with tarfile.open(parent_archive, "w:gz") as archive:
            for name, contents in (
                ("client-v5.1.0/sound/.atrinik-dependency.json", injected_marker),
                ("client-v5.1.0/sound/effects/test.ogg", b"stale"),
            ):
                info = tarfile.TarInfo(name)
                info.size = len(contents)
                archive.addfile(info, io.BytesIO(contents))
        parent = {
            **self.dependency(parent_archive),
            "name": "client",
            "repository": "atrinik/client",
            "destination": "build/components/client",
        }

        statuses = dependencies.sync_dependencies(
            self.root,
            self.root / "build/cache",
            [nested, parent],
        )

        self.assertEqual(statuses, [("client", "installed"), ("sound", "installed")])
        self.assertEqual(
            (self.root / "build/components/client/sound/effects/test.ogg").read_bytes(),
            b"good",
        )

    def test_restores_previous_install_when_atomic_swap_fails(self) -> None:
        first_archive = self.make_archive(
            [("sound-v1.0.0/effects/test.ogg", b"original", "file")]
        )
        first = self.dependency(first_archive)
        dependencies.install_dependency(self.root, self.root / "build/cache", first)

        second_archive = self.root / "updated.tar.gz"
        with tarfile.open(second_archive, "w:gz") as stream:
            info = tarfile.TarInfo("sound-v1.0.1/effects/test.ogg")
            info.size = len(b"updated")
            stream.addfile(info, io.BytesIO(b"updated"))
        second = {
            **first,
            "tag": "v1.0.1",
            "url": second_archive.as_uri(),
            "sha256": hashlib.sha256(second_archive.read_bytes()).hexdigest(),
        }

        original_replace = Path.replace

        def fail_staging_swap(source: Path, target: Path) -> Path:
            if source.name.startswith(".sound-staging-"):
                raise OSError("simulated swap failure")
            return original_replace(source, target)

        with mock.patch.object(Path, "replace", fail_staging_swap):
            with self.assertRaisesRegex(OSError, "simulated swap failure"):
                dependencies.install_dependency(
                    self.root,
                    self.root / "build/cache",
                    second,
                )

        destination = self.root / "client/sound"
        self.assertEqual(
            (destination / "effects/test.ogg").read_bytes(),
            b"original",
        )
        self.assertEqual(
            dependencies.read_marker(destination),
            dependencies.marker_for(first),
        )
        self.assertEqual(list((self.root / "client").glob(".sound-*-*")), [])


if __name__ == "__main__":
    unittest.main()
