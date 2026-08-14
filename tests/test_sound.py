from __future__ import annotations

import copy
import gzip
import hashlib
import http.client
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import urllib.error
from unittest import mock

from atrinik_workspace import sound as sound_module
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.sound import (
    PLAYTEST_BLOCKERS,
    PLAYTEST_MANIFEST,
    PLAYTEST_MARKER,
    PLAYTEST_SCHEMA,
    RELEASE_CHECKSUMS,
    RELEASE_MANIFEST,
    RELEASE_MARKER,
    RELEASE_PRODUCT,
    RELEASE_SCHEMA,
    cache_key,
    clean_source_inputs,
    extract_release_archive,
    validate_release_coordinates,
    validate_sound_record,
    verify_playtest_tree,
    verify_release_tree,
)
from atrinik_workspace.workspace import BUILD_METADATA, Workspace


def canonical(value: object) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class PlaytestSoundTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source = self.root / "sound"
        self.output = self.source / "build" / "tree"
        (self.source / "manifests").mkdir(parents=True)
        (self.source / "schemas").mkdir()
        self.output.mkdir(parents=True)

        source_assets: list[dict[str, object]] = []
        playtest_assets: list[dict[str, object]] = []
        for index in range(339):
            copied = index < 196
            logical = (
                f"effects/copy-{index:03}.ogg"
                if copied
                else f"background/convert-{index:03}.mid"
            )
            source_codec = "vorbis" if copied else "midi"
            source_payload = f"source-{index}".encode()
            source_hash = hashlib.sha256(source_payload).hexdigest()
            payload = (
                b"OggS\x00fixture\x01vorbis" + source_payload
                if copied
                else b"OggS\x00fixtureOpusHead" + source_payload
            )
            if copied:
                source_hash = hashlib.sha256(payload).hexdigest()
            destination = self.output / logical
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(payload)
            source = {
                "sha256": source_hash,
                "codec": source_codec,
                "container": "ogg" if copied else "midi",
            }
            source_assets.append(
                {
                    "logical_path": logical,
                    "source_path": logical,
                    "source": source,
                    "render": {},
                }
            )
            playtest_assets.append(
                {
                    "logical_path": logical,
                    "source_path": logical,
                    "mapping": "copy" if copied else "render-opus",
                    "source": source,
                    "output": {
                        "sha256": hashlib.sha256(payload).hexdigest(),
                        "size_bytes": len(payload),
                        "codec": "vorbis" if copied else "opus",
                        "container": "ogg",
                        "sample_rate": 48000,
                        "channels": 2,
                        "duration_seconds": 1.0,
                    },
                }
            )

        source_manifest = {"audio_source_count": 339, "assets": source_assets}
        source_manifest_path = self.source / "manifests" / "source-assets.json"
        source_manifest_path.write_bytes(canonical(source_manifest))
        toolchain = self.source / "manifests" / "playtest-audio-toolchain.json"
        toolchain.write_bytes(
            canonical(
                {
                    "$schema": "../schemas/playtest-audio-toolchain-v1.schema.json",
                    "schema_version": 1,
                }
            )
        )
        schema = self.source / "schemas" / "playtest-manifest-v1.schema.json"
        schema.write_bytes(b"{}\n")
        packaged_schema = self.output / PLAYTEST_SCHEMA
        packaged_schema.parent.mkdir()
        packaged_schema.write_bytes(schema.read_bytes())
        marker = {
            "format": "atrinik-sound-playtest-tree",
            "playtest_only": True,
            "publishable": False,
            "schema_version": 1,
        }
        (self.output / PLAYTEST_MARKER).write_bytes(canonical(marker))
        blockers = {
            "schema_version": 1,
            "source_manifest_sha256": digest(source_manifest_path),
            "source_count": 339,
            "count": 0,
            "findings": [],
        }
        (self.output / PLAYTEST_BLOCKERS).write_bytes(canonical(blockers))
        tree_hash = hashlib.sha256()
        for asset in sorted(playtest_assets, key=lambda value: str(value["logical_path"])):
            logical = str(asset["logical_path"])
            tree_hash.update(f"{digest(self.output / logical)}  {logical}\n".encode())
        self.inputs = {
            "source_commit": "a" * 40,
            "source_tree": "b" * 40,
            "source_manifest_sha256": digest(source_manifest_path),
            "toolchain_sha256": digest(toolchain),
            "schema_sha256": digest(schema),
            "builder_sha256": "c" * 64,
        }
        self.manifest = {
            "$schema": PLAYTEST_SCHEMA,
            "schema_version": 1,
            "playtest_only": True,
            "publishable": False,
            "source_commit": self.inputs["source_commit"],
            "source_tree": self.inputs["source_tree"],
            "source_manifest_sha256": self.inputs["source_manifest_sha256"],
            "toolchain_sha256": self.inputs["toolchain_sha256"],
            "tool_versions": {
                name: "fixture"
                for name in (
                    "ffmpeg",
                    "openmpt123",
                    "opusenc",
                    "opusinfo",
                    "sdl3_mixer_probe",
                    "wildmidi",
                )
            },
            "schema_sha256": self.inputs["schema_sha256"],
            "marker_sha256": digest(self.output / PLAYTEST_MARKER),
            "blocker_report_sha256": digest(self.output / PLAYTEST_BLOCKERS),
            "blocker_count": 0,
            "logical_path_count": 339,
            "copied_vorbis_count": 196,
            "converted_opus_count": 143,
            "output_tree_sha256": tree_hash.hexdigest(),
            "assets": playtest_assets,
        }
        (self.output / PLAYTEST_MANIFEST).write_bytes(canonical(self.manifest))

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def rewrite_manifest(self, value: dict[str, object]) -> None:
        (self.output / PLAYTEST_MANIFEST).write_bytes(canonical(value))

    def test_complete_current_tree_is_accepted(self) -> None:
        record = verify_playtest_tree(self.source, self.output, self.inputs)

        self.assertEqual(record["mode"], "local-playtest")
        self.assertEqual(record["logical_path_count"], 339)
        self.assertEqual(record["output_tree_sha256"], self.manifest["output_tree_sha256"])
        self.assertEqual(record["toolchain_schema_version"], 1)
        self.assertEqual(record["playtest_manifest_sha256"], digest(self.output / PLAYTEST_MANIFEST))
        self.assertEqual(validate_sound_record(record), record)

        replacement_record = dict(record)
        replacement_record["copied_vorbis_count"] = 189
        replacement_record["converted_opus_count"] = 150
        self.assertEqual(
            validate_sound_record(replacement_record), replacement_record
        )


        replacement_record["converted_opus_count"] = 149
        with self.assertRaisesRegex(WorkspaceError, "provenance is invalid"):
            validate_sound_record(replacement_record)

        incomplete = dict(record)
        incomplete.pop("blocker_report_sha256")
        with self.assertRaisesRegex(WorkspaceError, "fields are invalid"):
            validate_sound_record(incomplete)

    def test_output_tree_digest_is_independent_of_manifest_asset_order(self) -> None:
        self.manifest["assets"] = list(reversed(self.manifest["assets"]))
        self.rewrite_manifest(self.manifest)

        record = verify_playtest_tree(self.source, self.output, self.inputs)

        self.assertEqual(
            record["output_tree_sha256"], self.manifest["output_tree_sha256"]
        )

    def test_sound_record_validation_rejects_each_invalid_identity(self) -> None:
        record = verify_playtest_tree(self.source, self.output, self.inputs)
        cases: list[tuple[str, object, str]] = [
            ("not-object", [], "not an object"),
            ("unknown-mode", {**record, "mode": "released"}, "fields are invalid"),
            ("relative-root", {**record, "root": "relative"}, "root is invalid"),
            ("bad-commit", {**record, "source_commit": "bad"}, "identity is invalid"),
            ("bad-tree", {**record, "source_tree": "bad"}, "identity is invalid"),
            ("unclean", {**record, "source_clean": False}, "provenance is invalid"),
            (
                "bad-manifest-hash",
                {**record, "playtest_manifest_sha256": "bad"},
                "provenance is invalid",
            ),
            (
                "bad-playtest-schema",
                {**record, "playtest_schema_version": 2},
                "provenance is invalid",
            ),
            (
                "bad-toolchain-schema-version",
                {**record, "toolchain_schema_version": 2},
                "provenance is invalid",
            ),
            (
                "bad-toolchain-schema",
                {**record, "toolchain_schema": "schema.json"},
                "provenance is invalid",
            ),
            (
                "bad-path-count",
                {**record, "logical_path_count": 338},
                "provenance is invalid",
            ),
            (
                "boolean-copy-count",
                {**record, "copied_vorbis_count": True},
                "provenance is invalid",
            ),
            (
                "negative-copy-count",
                {**record, "copied_vorbis_count": -1},
                "provenance is invalid",
            ),
            (
                "boolean-converted-count",
                {**record, "converted_opus_count": True},
                "provenance is invalid",
            ),
            (
                "negative-converted-count",
                {**record, "converted_opus_count": -1},
                "provenance is invalid",
            ),
            (
                "incomplete-count",
                {**record, "converted_opus_count": 142},
                "provenance is invalid",
            ),
        ]
        for description, value, error in cases:
            with self.subTest(description=description):
                with self.assertRaisesRegex(WorkspaceError, error):
                    validate_sound_record(value)

    def test_manifest_identity_and_mapping_mutations_fail_closed(self) -> None:
        def set_asset(value: dict[str, object], key: str, replacement: object) -> None:
            assets = value["assets"]
            assert isinstance(assets, list) and isinstance(assets[0], dict)
            assets[0][key] = replacement

        def set_output(value: dict[str, object], key: str, replacement: object) -> None:
            assets = value["assets"]
            assert isinstance(assets, list) and isinstance(assets[0], dict)
            output = assets[0]["output"]
            assert isinstance(output, dict)
            output[key] = replacement

        cases = [
            (
                "schema",
                lambda value: value.__setitem__("schema_version", 2),
                "must use schema 1",
            ),
            (
                "stale-source",
                lambda value: value.__setitem__("source_commit", "d" * 40),
                "stale or tampered source_commit",
            ),
            (
                "tool-versions",
                lambda value: value.__setitem__("tool_versions", {}),
                "toolchain versions are invalid",
            ),
            (
                "assets-type",
                lambda value: value.__setitem__("assets", {}),
                "assets must be an array",
            ),
            (
                "unsafe-path",
                lambda value: set_asset(value, "logical_path", "../unsafe.ogg"),
                "unsafe or duplicate path",
            ),
            (
                "extra-path",
                lambda value: set_asset(value, "logical_path", "effects/extra.ogg"),
                "extra logical path",
            ),
            (
                "source-path",
                lambda value: set_asset(value, "source_path", "effects/other.ogg"),
                "mapping is invalid",
            ),
            (
                "channels",
                lambda value: set_output(value, "channels", 3),
                "mapping is invalid",
            ),
            (
                "missing-path",
                lambda value: value["assets"].pop(),
                "missing logical path",
            ),
            (
                "counts",
                lambda value: value.__setitem__("copied_vorbis_count", 195),
                "counts do not match",
            ),
            (
                "tree-digest",
                lambda value: value.__setitem__("output_tree_sha256", "f" * 64),
                "output-tree digest mismatch",
            ),
        ]
        for description, mutate, error in cases:
            with self.subTest(description=description):
                value = copy.deepcopy(self.manifest)
                mutate(value)
                self.rewrite_manifest(value)
                try:
                    with self.assertRaisesRegex(WorkspaceError, error):
                        verify_playtest_tree(self.source, self.output, self.inputs)
                finally:
                    self.rewrite_manifest(self.manifest)

    def test_auxiliary_contract_mutations_fail_closed(self) -> None:
        manifest_path = self.output / PLAYTEST_MANIFEST
        blocker_path = self.output / PLAYTEST_BLOCKERS
        schema_path = self.output / PLAYTEST_SCHEMA
        toolchain_path = self.source / "manifests" / "playtest-audio-toolchain.json"
        source_manifest_path = self.source / "manifests" / "source-assets.json"
        originals = {
            path: path.read_bytes()
            for path in (
                manifest_path,
                blocker_path,
                schema_path,
                toolchain_path,
                source_manifest_path,
            )
        }

        def reject(description: str, error: str, mutate: object) -> None:
            with self.subTest(description=description):
                try:
                    assert callable(mutate)
                    candidate_inputs = mutate()
                    inputs = (
                        candidate_inputs
                        if isinstance(candidate_inputs, dict)
                        else self.inputs
                    )
                    with self.assertRaisesRegex(WorkspaceError, error):
                        verify_playtest_tree(
                            self.source,
                            self.output,
                            inputs,
                        )
                finally:
                    for path, payload in originals.items():
                        path.write_bytes(payload)

        def install_toolchain_payload(payload: bytes) -> dict[str, str]:
            toolchain_path.write_bytes(payload)
            toolchain_hash = digest(toolchain_path)
            manifest = copy.deepcopy(self.manifest)
            manifest["toolchain_sha256"] = toolchain_hash
            self.rewrite_manifest(manifest)
            return {**self.inputs, "toolchain_sha256": toolchain_hash}

        def install_toolchain(value: object) -> dict[str, str]:
            return install_toolchain_payload(canonical(value))

        def install_source_manifest_payload(payload: bytes) -> dict[str, str]:
            source_manifest_path.write_bytes(payload)
            source_manifest_hash = digest(source_manifest_path)
            blocker = json.loads(originals[blocker_path])
            blocker["source_manifest_sha256"] = source_manifest_hash
            blocker_path.write_bytes(canonical(blocker))
            manifest = copy.deepcopy(self.manifest)
            manifest["source_manifest_sha256"] = source_manifest_hash
            manifest["blocker_report_sha256"] = digest(blocker_path)
            self.rewrite_manifest(manifest)
            return {**self.inputs, "source_manifest_sha256": source_manifest_hash}

        def install_source_manifest(value: object) -> dict[str, str]:
            return install_source_manifest_payload(canonical(value))

        reject(
            "manifest-json",
            "manifest is invalid JSON",
            lambda: manifest_path.write_bytes(b"{"),
        )
        reject(
            "manifest-canonical",
            "manifest is not canonical JSON",
            lambda: manifest_path.write_text(json.dumps(self.manifest), encoding="utf-8"),
        )
        reject(
            "toolchain-schema",
            "toolchain schema is invalid",
            lambda: install_toolchain({"schema_version": 2}),
        )
        reject(
            "toolchain-json",
            "toolchain is invalid JSON",
            lambda: install_toolchain_payload(b"{"),
        )
        reject(
            "toolchain-hash",
            "toolchain is stale or tampered",
            lambda: toolchain_path.write_bytes(
                canonical({**json.loads(originals[toolchain_path]), "unused": True})
            ),
        )
        reject(
            "packaged-schema",
            "packaged schema is missing or tampered",
            lambda: schema_path.write_bytes(b"changed\n"),
        )
        reject(
            "blocker-hash",
            "blocker report is missing or tampered",
            lambda: blocker_path.write_bytes(b"changed\n"),
        )

        def invalid_blocker_json() -> None:
            blocker_path.write_bytes(b"{")
            value = copy.deepcopy(self.manifest)
            value["blocker_report_sha256"] = digest(blocker_path)
            self.rewrite_manifest(value)

        reject("blocker-json", "blocker report is invalid JSON", invalid_blocker_json)

        def invalid_blocker_contract() -> None:
            blocker = json.loads(originals[blocker_path])
            blocker["source_count"] = 338
            blocker_path.write_bytes(canonical(blocker))
            value = copy.deepcopy(self.manifest)
            value["blocker_report_sha256"] = digest(blocker_path)
            self.rewrite_manifest(value)

        reject(
            "blocker-contract",
            "blocker report contract is invalid",
            invalid_blocker_contract,
        )
        reject(
            "source-manifest",
            "source manifest is invalid",
            lambda: install_source_manifest({"assets": {}}),
        )
        reject(
            "source-manifest-json",
            "source manifest is invalid JSON",
            lambda: install_source_manifest_payload(b"{"),
        )
        reject(
            "source-manifest-hash",
            "source manifest is stale or tampered",
            lambda: source_manifest_path.write_bytes(
                canonical(
                    {**json.loads(originals[source_manifest_path]), "unused": True}
                )
            ),
        )

        def invalid_source_asset() -> dict[str, str]:
            source_manifest = json.loads(originals[source_manifest_path])
            source_manifest["assets"][0] = []
            return install_source_manifest(source_manifest)

        reject(
            "source-asset",
            "source manifest assets are invalid",
            invalid_source_asset,
        )

        def invalid_expected_source() -> dict[str, str]:
            source_manifest = json.loads(originals[source_manifest_path])
            source_manifest["assets"][0]["source"] = []
            return install_source_manifest(source_manifest)

        reject(
            "source-asset-identity",
            "source manifest asset is invalid",
            invalid_expected_source,
        )

        def duplicate_source_asset() -> dict[str, str]:
            source_manifest = json.loads(originals[source_manifest_path])
            source_manifest["assets"][1]["logical_path"] = source_manifest["assets"][0][
                "logical_path"
            ]
            return install_source_manifest(source_manifest)

        reject(
            "duplicate-source-asset",
            "duplicate logical paths",
            duplicate_source_asset,
        )

    def test_vorbis_copy_must_match_the_source_hash(self) -> None:
        logical_path = "effects/copy-000.ogg"
        payload_path = self.output / logical_path
        original_payload = payload_path.read_bytes()
        replacement = b"OggS\x00fixture\x01vorbisreplacement"
        payload_path.write_bytes(replacement)
        value = copy.deepcopy(self.manifest)
        asset = value["assets"][0]
        assert isinstance(asset, dict)
        output = asset["output"]
        assert isinstance(output, dict)
        output["sha256"] = digest(payload_path)
        output["size_bytes"] = len(replacement)
        self.rewrite_manifest(value)
        try:
            with self.assertRaisesRegex(WorkspaceError, "copy differs from source"):
                verify_playtest_tree(self.source, self.output, self.inputs)
        finally:
            payload_path.write_bytes(original_payload)
            self.rewrite_manifest(self.manifest)

    def test_payload_corruption_and_legacy_raw_bytes_are_rejected(self) -> None:
        payload = self.output / "background" / "convert-196.mid"
        payload.write_bytes(b"MThd" + b"\0" * 20)
        with self.assertRaisesRegex(WorkspaceError, "hash or size mismatch"):
            verify_playtest_tree(self.source, self.output, self.inputs)

        asset = self.manifest["assets"][196]
        assert isinstance(asset, dict) and isinstance(asset["output"], dict)
        asset["output"]["sha256"] = digest(payload)
        asset["output"]["size_bytes"] = payload.stat().st_size
        self.rewrite_manifest(self.manifest)
        with self.assertRaisesRegex(WorkspaceError, "not an Ogg stream"):
            verify_playtest_tree(self.source, self.output, self.inputs)

    def test_marker_schema_and_exact_closure_fail_closed(self) -> None:
        marker = json.loads((self.output / PLAYTEST_MARKER).read_text())
        marker["publishable"] = True
        (self.output / PLAYTEST_MARKER).write_bytes(canonical(marker))
        with self.assertRaisesRegex(WorkspaceError, "marker"):
            verify_playtest_tree(self.source, self.output, self.inputs)

        (self.output / PLAYTEST_MARKER).write_bytes(
            canonical(
                {
                    "format": "atrinik-sound-playtest-tree",
                    "playtest_only": True,
                    "publishable": False,
                    "schema_version": 1,
                }
            )
        )
        extra = self.output / "unexpected.bin"
        extra.write_bytes(b"no")
        with self.assertRaisesRegex(WorkspaceError, "missing or unexpected"):
            verify_playtest_tree(self.source, self.output, self.inputs)

    def test_cache_key_binds_every_exact_input(self) -> None:
        baseline = cache_key(self.inputs)
        for key in self.inputs:
            changed = copy.deepcopy(self.inputs)
            changed[key] = "f" * len(changed[key])
            self.assertNotEqual(cache_key(changed), baseline, key)

    def test_clean_source_identity_rejects_dirty_checkout(self) -> None:
        tools = self.source / "tools"
        tools.mkdir()
        (tools / "sound_release.py").write_text("# builder\n", encoding="utf-8")
        (self.source / ".gitignore").write_text("/build/\n", encoding="utf-8")
        subprocess.run(["git", "init", "-b", "main"], cwd=self.source, check=True)
        subprocess.run(
            ["git", "config", "user.name", "Tests"], cwd=self.source, check=True
        )
        subprocess.run(
            ["git", "config", "user.email", "tests@example.invalid"],
            cwd=self.source,
            check=True,
        )
        subprocess.run(["git", "add", "."], cwd=self.source, check=True)
        subprocess.run(
            ["git", "commit", "-m", "feat: fixture"],
            cwd=self.source,
            check=True,
            stdout=subprocess.DEVNULL,
        )

        inputs = clean_source_inputs(self.source)
        self.assertEqual(inputs["source_commit"], subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=self.source,
            check=True,
            text=True,
            capture_output=True,
        ).stdout.strip())
        (tools / "sound_release.py").write_text("# dirty\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "requires a clean"):
            clean_source_inputs(self.source)

    def test_low_level_sound_inputs_fail_closed(self) -> None:
        missing = self.root / "missing"
        directory = self.root / "directory"
        directory.mkdir()
        for description, invoke in (
            (
                "missing read",
                lambda: sound_module._read_regular(missing, "fixture"),
            ),
            (
                "directory read",
                lambda: sound_module._read_regular(directory, "fixture"),
            ),
            (
                "missing hash",
                lambda: sound_module._hash_regular(missing, "fixture"),
            ),
            (
                "directory hash",
                lambda: sound_module._hash_regular(directory, "fixture"),
            ),
            (
                "git failure",
                lambda: sound_module._git(missing, "rev-parse", "HEAD"),
            ),
            (
                "non-object contract",
                lambda: sound_module._require_dict([], set(), "fixture"),
            ),
            (
                "wrong codec",
                lambda: sound_module._validate_payload_codec(
                    "fixture.ogg", "opus", b"OggS\x00\x01vorbis"
                ),
            ),
            (
                "missing playtest root",
                lambda: verify_playtest_tree(self.source, missing, self.inputs),
            ),
        ):
            with self.subTest(description=description):
                with self.assertRaises(WorkspaceError):
                    invoke()

    def test_tree_closure_rejects_symlinks_and_nonregular_entries(self) -> None:
        closure = self.root / "closure"
        closure.mkdir()
        real_directory = closure / "real"
        real_directory.mkdir()
        (closure / "linked-directory").symlink_to(real_directory, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "non-directory or symlink"):
            sound_module._tree_files(closure)

        (closure / "linked-directory").unlink()
        payload = closure / "payload"
        payload.write_bytes(b"payload")
        (closure / "linked-file").symlink_to(payload)
        with self.assertRaisesRegex(WorkspaceError, "not a readable regular file"):
            sound_module._tree_files(closure)

        (closure / "linked-file").unlink()
        with (
            mock.patch.object(sound_module.os, "open", return_value=17),
            mock.patch.object(
                sound_module.os,
                "fstat",
                return_value=mock.Mock(st_mode=0),
            ),
            mock.patch.object(sound_module.os, "close"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "not a regular file"):
                sound_module._tree_files(closure)

    def test_clean_source_identity_rejects_races_and_invalid_coordinates(self) -> None:
        coordinates = "a" * 40
        tree = "b" * 40
        with (
            mock.patch.object(
                sound_module,
                "_hash_regular",
                return_value=("c" * 64, 1, b"x"),
            ),
            mock.patch.object(
                sound_module,
                "_git",
                side_effect=["", coordinates, tree, "", "d" * 40],
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed while reading"):
                clean_source_inputs(self.source)

        with (
            mock.patch.object(
                sound_module,
                "_hash_regular",
                return_value=("c" * 64, 1, b"x"),
            ),
            mock.patch.object(
                sound_module,
                "_git",
                side_effect=["", "invalid", tree, "", "invalid"],
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "invalid Git coordinates"):
                clean_source_inputs(self.source)

    def test_wrapper_reuses_offline_tree_and_topology_prepares_same_root(self) -> None:
        tools = self.source / "tools"
        tools.mkdir()
        builder = tools / "sound_release.py"
        builder.write_text(
            "#!/usr/bin/env python3\n"
            "from pathlib import Path\n"
            "import shutil, sys\n"
            "root = Path(__file__).resolve().parents[1]\n"
            "command, output = sys.argv[1], Path(sys.argv[2])\n"
            "with (root / 'build' / 'calls').open('a') as stream:\n"
            "    stream.write(command + '\\n')\n"
            "if command == 'build-playtest-tree' and not output.exists():\n"
            "    shutil.copytree(root / 'build' / 'tree', output)\n"
            "elif command == 'verify-playtest-tree':\n"
            "    assert (output / '.atrinik-playtest-tree.json').is_file()\n",
            encoding="utf-8",
        )
        (self.source / ".gitignore").write_text("/build/\n", encoding="utf-8")
        subprocess.run(["git", "init", "-b", "main"], cwd=self.source, check=True)
        subprocess.run(
            ["git", "config", "user.name", "Tests"], cwd=self.source, check=True
        )
        subprocess.run(
            ["git", "config", "user.email", "tests@example.invalid"],
            cwd=self.source,
            check=True,
        )
        subprocess.run(["git", "add", "."], cwd=self.source, check=True)
        subprocess.run(
            ["git", "commit", "-m", "feat: fixture producer"],
            cwd=self.source,
            check=True,
            stdout=subprocess.DEVNULL,
        )
        subprocess.run(
            [
                "git", "remote", "add", "origin",
                "https://github.com/atrinik/sound.git",
            ],
            cwd=self.source,
            check=True,
        )
        self.manifest["source_commit"] = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=self.source,
            check=True,
            text=True,
            capture_output=True,
        ).stdout.strip()
        self.manifest["source_tree"] = subprocess.run(
            ["git", "rev-parse", "HEAD^{tree}"],
            cwd=self.source,
            check=True,
            text=True,
            capture_output=True,
        ).stdout.strip()
        self.rewrite_manifest(self.manifest)

        wrapper = self.root / "wrapper"
        wrapper.mkdir()
        shutil.copy2(Path(__file__).resolve().parents[1] / "components.json", wrapper)
        subprocess.run(["git", "init", "-b", "main"], cwd=wrapper, check=True)
        workspace_root = self.root / "workspace"
        previous = os.environ.get("ATRINIK_WORKSPACE_DIR")
        os.environ["ATRINIK_WORKSPACE_DIR"] = str(workspace_root)
        try:
            workspace = Workspace(wrapper)
            workspace.paths.ensure()
            workspace.create_profile("classic-audio", "classic")
            workspace.set_profile(
                "classic-audio", "sound", "path", str(self.source)
            )
            workspace.set_profile_sound_mode("classic-audio", "local-playtest")
            profile_build = workspace.paths.builds / "profiles" / "fixture"
            profile_build.mkdir(parents=True)
            first_root, first_record = workspace._prepare_sound(
                profile_build, {"sound": self.source}, "classic-audio"
            )
            second_root, second_record = workspace._prepare_sound(
                profile_build, {"sound": self.source}, "classic-audio"
            )
            workspace._refresh_build_metadata(
                profile_build,
                "classic-audio",
                "fixture",
                {"sound": self.source},
                second_record,
            )
            client = self.root / "client"
            client.mkdir()
            (client / "tracked").write_text("fixture\n", encoding="utf-8")
            topology = workspace.paths.topologies / "fixture"
            topology.mkdir(parents=True)
            runtime = workspace._prepare_topology_client_runtime(
                topology, {"client": client, "sound": self.source}, second_root
            )
        finally:
            if previous is None:
                os.environ.pop("ATRINIK_WORKSPACE_DIR", None)
            else:
                os.environ["ATRINIK_WORKSPACE_DIR"] = previous

        self.assertEqual(first_root, second_root)
        self.assertEqual(first_record, second_record)
        self.assertEqual(
            (runtime / "sound").resolve(),
            first_root.resolve(),
        )
        metadata = json.loads((profile_build / BUILD_METADATA).read_text())
        self.assertEqual(metadata["sound"], first_record)
        self.assertEqual(
            (self.source / "build" / "calls").read_text().splitlines(),
            [
                "build-playtest-tree",
                "verify-playtest-tree",
                "build-playtest-tree",
                "verify-playtest-tree",
            ],
        )


class ReleasedSoundTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.tree = self.root / "tree"
        self.tree.mkdir()
        schema = self.tree / RELEASE_SCHEMA
        schema.parent.mkdir()
        schema_fields = [
            "$schema", "assets", "converted_opus_count",
            "copied_vorbis_count", "logical_path_count", "marker_sha256",
            "notices", "output_tree_sha256", "playtest_only", "product",
            "product_version", "publishable", "release_tag", "repository",
            "schema_sha256", "schema_version", "source_commit",
            "source_manifest_sha256", "source_tree", "toolchain_sha256",
        ]
        schema.write_bytes(canonical({
            "$id": f"https://atrinik.org/{RELEASE_SCHEMA}",
            "$schema": "https://json-schema.org/draft/2020-12/schema",
            "additionalProperties": False,
            "properties": {name: {} for name in schema_fields},
            "required": schema_fields,
            "type": "object",
        }))
        marker = {
            "format": RELEASE_PRODUCT,
            "playtest_only": False,
            "product_version": "1.4.0",
            "publishable": True,
            "schema_version": 1,
        }
        (self.tree / RELEASE_MARKER).write_bytes(canonical(marker))
        notices = []
        for path in ("background/LICENSE", "effects/LICENSE"):
            notice = self.tree / path
            notice.parent.mkdir(exist_ok=True)
            notice.write_text(f"fixture notice {path}\n", encoding="utf-8")
            notices.append({"path": path, "sha256": digest(notice)})
        assets: list[dict[str, object]] = []
        for index in range(339):
            copied = index >= 150
            logical = (
                f"effects/copy-{index - 150:03}.ogg"
                if copied else f"background/convert-{index:03}.mid"
            )
            payload = (
                b"OggS\x00fixture\x01vorbis" if copied
                else b"OggS\x00fixtureOpusHead"
            ) + str(index).encode()
            path = self.tree / logical
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(payload)
            payload_hash = digest(path)
            source_codec = "vorbis" if copied else ("flac" if index < 28 else "midi")
            assets.append({
                "logical_path": logical,
                "source_path": logical if copied else f"background/source-{index:03}.flac",
                "mapping": "copy" if copied else "render-opus",
                "source": {
                    "codec": source_codec,
                    "container": "ogg" if copied else source_codec,
                    "sha256": payload_hash if copied else hashlib.sha256(
                        f"source-{index}".encode()
                    ).hexdigest(),
                },
                "output": {
                    "sha256": payload_hash,
                    "size_bytes": len(payload),
                    "codec": "vorbis" if copied else "opus",
                    "container": "ogg",
                    "sample_rate": 48000,
                    "channels": 2,
                    "duration_seconds": 1.0,
                },
            })
        assets.sort(key=lambda asset: str(asset["logical_path"]))
        tree_hash = hashlib.sha256()
        for asset in assets:
            output = asset["output"]
            assert isinstance(output, dict)
            tree_hash.update(
                f"{output['sha256']}  {asset['logical_path']}\n".encode()
            )
        self.manifest = {
            "$schema": RELEASE_SCHEMA,
            "schema_version": 1,
            "product": RELEASE_PRODUCT,
            "product_version": "1.4.0",
            "release_tag": "v1.4.0",
            "repository": "atrinik/sound",
            "playtest_only": False,
            "publishable": True,
            "source_commit": "a" * 40,
            "source_tree": "b" * 40,
            "source_manifest_sha256": "c" * 64,
            "toolchain_sha256": "d" * 64,
            "schema_sha256": digest(schema),
            "marker_sha256": digest(self.tree / RELEASE_MARKER),
            "logical_path_count": 339,
            "copied_vorbis_count": 189,
            "converted_opus_count": 150,
            "output_tree_sha256": tree_hash.hexdigest(),
            "notices": notices,
            "assets": assets,
        }
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.rewrite_checksums()
        self.archive = self.root / f"{RELEASE_PRODUCT}-1.4.0.tar.gz"
        with tarfile.open(
            self.archive, "w:gz", format=tarfile.USTAR_FORMAT
        ) as archive:
            archive.add(self.tree, arcname=f"{RELEASE_PRODUCT}-1.4.0")
        self.coordinates = {
            "repository": "atrinik/sound",
            "tag": "v1.4.0",
            "product": RELEASE_PRODUCT,
            "product_version": "1.4.0",
            "manifest_schema_version": 1,
            "source_commit": "a" * 40,
            "source_tree": "b" * 40,
            "asset_url": (
                "https://github.com/atrinik/sound/releases/download/v1.4.0/"
                f"{RELEASE_PRODUCT}-1.4.0.tar.gz"
            ),
            "archive_sha256": digest(self.archive),
            "release_manifest_sha256": digest(self.tree / RELEASE_MANIFEST),
            "source_manifest_sha256": "c" * 64,
            "schema_sha256": digest(schema),
            "toolchain_sha256": "d" * 64,
            "output_tree_sha256": tree_hash.hexdigest(),
        }

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def rewrite_checksums(self) -> None:
        files = sorted(
            path for path in self.tree.rglob("*")
            if path.is_file() and path.name != RELEASE_CHECKSUMS
        )
        (self.tree / RELEASE_CHECKSUMS).write_text(
            "".join(
                f"{digest(path)}  {path.relative_to(self.tree).as_posix()}\n"
                for path in files
            ), encoding="ascii"
        )

    def test_complete_tree_archive_and_record_are_accepted(self) -> None:
        self.assertEqual(validate_release_coordinates(self.coordinates), self.coordinates)
        record = verify_release_tree(self.tree, self.coordinates)
        self.assertEqual(record["mode"], "released")
        self.assertEqual(validate_sound_record(record), record)
        extracted = self.root / "extracted"
        extract_release_archive(self.archive, extracted, self.coordinates)
        self.assertEqual(verify_release_tree(extracted, self.coordinates), {
            **record, "root": str(extracted.resolve())
        })

    def test_wrapper_downloads_once_and_reuses_exact_verified_cache(self) -> None:
        wrapper = self.root / "wrapper"
        wrapper.mkdir()
        shutil.copy2(Path(__file__).resolve().parents[1] / "components.json", wrapper)
        workspace = Workspace(wrapper)
        build = self.root / "build"
        build.mkdir()
        profile = {
            "stack": "classic",
            "sound_mode": "released",
            "sound_release": self.coordinates,
        }
        with (
            mock.patch.object(workspace, "_load_profile", return_value=profile),
            mock.patch(
                "atrinik_workspace.workspace.download_release_archive",
                side_effect=lambda _url, destination: shutil.copy2(
                    self.archive, destination
                ),
            ) as download,
        ):
            first_root, first_record = workspace._prepare_sound(
                build, {"sound": self.root / "unused-source"}, "classic-release"
            )
            shutil.rmtree(first_root)
            second_root, second_record = workspace._prepare_sound(
                build, {"sound": self.root / "unused-source"}, "classic-release"
            )
        self.assertEqual(download.call_count, 1)
        self.assertEqual(first_root, second_root)
        self.assertEqual(first_record, second_record)
        self.assertEqual(first_record["archive_sha256"], self.coordinates["archive_sha256"])

    def test_coordinates_marker_payload_and_archive_fail_closed(self) -> None:
        for coordinates in (
            {**self.coordinates, "asset_url": "https://example.com/a"},
            {**self.coordinates, "repository": "atrinik/classic"},
            {**self.coordinates, "tag": "v1.4.1"},
            {**self.coordinates, "archive_sha256": "bad"},
        ):
            with self.assertRaises(WorkspaceError):
                validate_release_coordinates(coordinates)

        payload = self.tree / "background/convert-000.mid"
        payload.write_bytes(b"MThd" + b"\0" * 20)
        first = self.manifest["assets"][0]
        assert isinstance(first, dict) and isinstance(first["output"], dict)
        first["output"]["sha256"] = digest(payload)
        first["output"]["size_bytes"] = payload.stat().st_size
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(self.tree / RELEASE_MANIFEST)
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "not an Ogg stream"):
            verify_release_tree(self.tree, self.coordinates)

        unsafe = self.root / "unsafe.tar.gz"
        with tarfile.open(unsafe, "w:gz") as archive:
            member = tarfile.TarInfo(f"{RELEASE_PRODUCT}-1.4.0/../escape")
            member.size = 0
            archive.addfile(member)
        with self.assertRaisesRegex(WorkspaceError, "unsafe path"):
            extract_release_archive(unsafe, self.root / "unsafe", self.coordinates)

    def test_archive_member_and_stream_failures_are_bounded_and_safe(self) -> None:
        prefix = f"{RELEASE_PRODUCT}-1.4.0"

        def archive_with(name: str, members: list[tarfile.TarInfo]) -> Path:
            path = self.root / f"{name}.tar.gz"
            with tarfile.open(path, "w:gz") as archive:
                root = tarfile.TarInfo(prefix)
                root.type = tarfile.DIRTYPE
                archive.addfile(root)
                for member in members:
                    payload = b"x" * member.size
                    archive.addfile(member, io.BytesIO(payload) if member.size else None)
            return path

        cases: list[tuple[str, list[tarfile.TarInfo], str]] = []
        for name in (f"{prefix}/..\\escape", "/absolute"):
            member = tarfile.TarInfo(name)
            cases.append((name.replace("/", "-").replace("\\", "-"), [member], "unsafe path"))
        first = tarfile.TarInfo(f"{prefix}/sound.ogg")
        second = tarfile.TarInfo(f"{prefix}/SOUND.ogg")
        cases.append(("case-collision", [first, second], "case-colliding"))
        link = tarfile.TarInfo(f"{prefix}/escape")
        link.type = tarfile.SYMTYPE
        link.linkname = "../../escape"
        cases.append(("symlink", [link], "special member"))
        device = tarfile.TarInfo(f"{prefix}/device")
        device.type = tarfile.CHRTYPE
        cases.append(("device", [device], "special member"))
        duplicate_root = tarfile.TarInfo(prefix)
        duplicate_root.type = tarfile.DIRTYPE
        cases.append(("duplicate-root", [duplicate_root], "duplicate"))
        other = tarfile.TarInfo("other-product/file")
        cases.append(("multiple-prefixes", [other], "one directory prefix"))
        for index, (name, members, message) in enumerate(cases):
            with self.subTest(name=name):
                archive = archive_with(f"unsafe-{index}", members)
                with self.assertRaisesRegex(WorkspaceError, message):
                    extract_release_archive(
                        archive, self.root / f"destination-{index}", self.coordinates
                    )

        truncated = self.root / "truncated.tar.gz"
        truncated.write_bytes(self.archive.read_bytes()[:128])
        with self.assertRaisesRegex(WorkspaceError, "invalid|truncated|corrupt"):
            extract_release_archive(truncated, self.root / "truncated", self.coordinates)

        extended = tarfile.TarInfo(f"{prefix}/file")
        extended.pax_headers = {"comment": "x" * (sound_module.MAX_RELEASE_METADATA_BYTES + 1)}
        oversized = archive_with("oversized-pax", [extended])
        with self.assertRaisesRegex(WorkspaceError, "extended metadata"):
            extract_release_archive(oversized, self.root / "oversized", self.coordinates)
        solaris = tarfile.TarInfo(f"{prefix}/solaris-metadata")
        solaris.type = tarfile.SOLARIS_XHDTYPE
        solaris.size = 1
        solaris_archive = archive_with("solaris-pax", [solaris])
        with self.assertRaisesRegex(WorkspaceError, "extended metadata"):
            extract_release_archive(
                solaris_archive, self.root / "solaris", self.coordinates
            )

    def test_archive_envelope_and_layout_failures_are_bounded(self) -> None:
        def compressed(name: str, payload: bytes) -> Path:
            path = self.root / name
            with gzip.open(path, "wb") as stream:
                stream.write(payload)
            return path

        for name, payload, message in (
            ("empty.tar.gz", b"", "truncated"),
            ("short-header.tar.gz", b"x" * 100, "truncated header"),
            (
                "invalid-trailer.tar.gz",
                tarfile.NUL * tarfile.BLOCKSIZE + b"x" * tarfile.BLOCKSIZE,
                "invalid trailer",
            ),
            ("invalid-header.tar.gz", b"x" * tarfile.BLOCKSIZE, "header is invalid"),
        ):
            with self.subTest(name=name):
                with self.assertRaisesRegex(WorkspaceError, message):
                    sound_module._prescan_release_archive(
                        compressed(name, payload)
                    )

        with mock.patch.object(sound_module, "MAX_RELEASE_MEMBERS", 0):
            with self.assertRaisesRegex(WorkspaceError, "member count"):
                sound_module._prescan_release_archive(self.archive)
        with mock.patch.object(sound_module, "MAX_RELEASE_MEMBER_BYTES", 0):
            with self.assertRaisesRegex(WorkspaceError, "member exceeds"):
                sound_module._prescan_release_archive(self.archive)
        with mock.patch.object(sound_module, "MAX_RELEASE_TAR_BYTES", 1):
            with self.assertRaisesRegex(WorkspaceError, "extraction limit"):
                sound_module._prescan_release_archive(self.archive)

        existing = self.root / "existing-destination"
        existing.mkdir()
        with self.assertRaisesRegex(WorkspaceError, "already exists"):
            extract_release_archive(self.archive, existing, self.coordinates)
        with mock.patch.object(sound_module, "MAX_RELEASE_ARCHIVE_BYTES", 1):
            with self.assertRaisesRegex(WorkspaceError, "extraction input limit"):
                extract_release_archive(
                    self.archive, self.root / "input-too-large", self.coordinates
                )

        empty = self.root / "empty-members.tar.gz"
        with tarfile.open(empty, "w:gz", format=tarfile.USTAR_FORMAT):
            pass
        with self.assertRaisesRegex(WorkspaceError, "member count"):
            extract_release_archive(empty, self.root / "empty-members", self.coordinates)

        wrong_prefix = self.root / "wrong-prefix.tar.gz"
        with tarfile.open(wrong_prefix, "w:gz", format=tarfile.USTAR_FORMAT) as archive:
            root = tarfile.TarInfo("wrong-product-1.4.0")
            root.type = tarfile.DIRTYPE
            archive.addfile(root)
        with self.assertRaisesRegex(WorkspaceError, "wrong product prefix"):
            extract_release_archive(
                wrong_prefix, self.root / "wrong-prefix", self.coordinates
            )

        missing_directory = self.root / "missing-directory.tar.gz"
        prefix = f"{RELEASE_PRODUCT}-1.4.0"
        with tarfile.open(
            missing_directory, "w:gz", format=tarfile.USTAR_FORMAT
        ) as archive:
            root = tarfile.TarInfo(prefix)
            root.type = tarfile.DIRTYPE
            archive.addfile(root)
            member = tarfile.TarInfo(f"{prefix}/nested/file")
            member.size = 1
            archive.addfile(member, io.BytesIO(b"x"))
        with self.assertRaisesRegex(WorkspaceError, "directories"):
            extract_release_archive(
                missing_directory, self.root / "missing-directory", self.coordinates
            )

    def test_interrupted_download_is_a_workspace_error(self) -> None:
        response = mock.Mock()
        response.headers = {}
        response.read.side_effect = http.client.IncompleteRead(b"partial", 100)
        with mock.patch("urllib.request.urlopen", return_value=response):
            with self.assertRaisesRegex(WorkspaceError, "interrupted"):
                sound_module.download_release_archive(
                    self.coordinates["asset_url"], self.root / "partial.tar.gz"
                )
        response.close.assert_called_once()

    def test_download_headers_limits_and_completion_fail_closed(self) -> None:
        with mock.patch(
            "urllib.request.urlopen", side_effect=urllib.error.URLError("offline")
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot download"):
                sound_module.download_release_archive(
                    self.coordinates["asset_url"], self.root / "offline.tar.gz"
                )
        for index, (length, chunks, message) in enumerate((
            ("invalid", [b""], "invalid Content-Length"),
            ("0", [b""], "download limit"),
            ("2", [b"x", b""], "incomplete"),
            (None, [b""], "incomplete"),
        )):
            response = mock.Mock()
            response.headers = {} if length is None else {"Content-Length": length}
            response.read.side_effect = chunks
            with self.subTest(length=length):
                with mock.patch("urllib.request.urlopen", return_value=response):
                    with self.assertRaisesRegex(WorkspaceError, message):
                        sound_module.download_release_archive(
                            self.coordinates["asset_url"],
                            self.root / f"download-{index}.tar.gz",
                        )
            response.close.assert_called_once()
        response = mock.Mock()
        response.headers = {"Content-Length": "3"}
        response.read.side_effect = [b"abc", b""]
        response.close.side_effect = OSError("close failed")
        with mock.patch("urllib.request.urlopen", return_value=response):
            sound_module.download_release_archive(
                self.coordinates["asset_url"], self.root / "complete.tar.gz"
            )
        self.assertEqual((self.root / "complete.tar.gz").read_bytes(), b"abc")
        response = mock.Mock()
        response.headers = {}
        response.read.side_effect = [b"abc", b""]
        with (
            mock.patch("urllib.request.urlopen", return_value=response),
            mock.patch.object(sound_module, "MAX_RELEASE_ARCHIVE_BYTES", 2),
        ):
            with self.assertRaisesRegex(WorkspaceError, "download limit"):
                sound_module.download_release_archive(
                    self.coordinates["asset_url"], self.root / "too-large.tar.gz"
                )

    def test_marker_tampering_fails_closed(self) -> None:
        (self.tree / RELEASE_MARKER).write_text("{}\n", encoding="utf-8")
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "marker"):
            verify_release_tree(self.tree, self.coordinates)

    def test_notice_tampering_fails_closed(self) -> None:
        notice = self.tree / "background/LICENSE"
        notice.write_text("tampered\n", encoding="utf-8")
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "notice"):
            verify_release_tree(self.tree, self.coordinates)

    def test_missing_checksums_fail_closed(self) -> None:
        (self.tree / RELEASE_CHECKSUMS).unlink()
        with self.assertRaisesRegex(WorkspaceError, "checksums"):
            verify_release_tree(self.tree, self.coordinates)

    def test_manifest_counts_and_schema_contract_fail_closed(self) -> None:
        self.manifest["logical_path_count"] = 338
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "counts.*339-path"):
            verify_release_tree(self.tree, self.coordinates)

    def test_manifest_and_schema_envelope_failures_are_distinguished(self) -> None:
        manifest_path = self.tree / RELEASE_MANIFEST
        schema_path = self.tree / RELEASE_SCHEMA
        original_manifest = canonical(self.manifest)
        original_schema = schema_path.read_bytes()
        original_coordinates = dict(self.coordinates)

        def restore() -> None:
            self.manifest = json.loads(original_manifest)
            manifest_path.write_bytes(original_manifest)
            schema_path.write_bytes(original_schema)
            self.coordinates.clear()
            self.coordinates.update(original_coordinates)
            self.rewrite_checksums()

        try:
            for name, payload, message in (
                ("invalid-json", b"{\n", "invalid JSON"),
                (
                    "noncanonical-json",
                    json.dumps(self.manifest, indent=2).encode() + b"\n",
                    "not canonical JSON",
                ),
            ):
                with self.subTest(name=name):
                    manifest_path.write_bytes(payload)
                    self.coordinates["release_manifest_sha256"] = hashlib.sha256(
                        payload
                    ).hexdigest()
                    with self.assertRaisesRegex(WorkspaceError, message):
                        verify_release_tree(self.tree, self.coordinates)
                    restore()

            del self.manifest["assets"]
            manifest_path.write_bytes(canonical(self.manifest))
            self.coordinates["release_manifest_sha256"] = digest(manifest_path)
            with self.assertRaisesRegex(WorkspaceError, "manifest fields"):
                verify_release_tree(self.tree, self.coordinates)
            restore()

            self.manifest["product"] = "wrong-product"
            manifest_path.write_bytes(canonical(self.manifest))
            self.coordinates["release_manifest_sha256"] = digest(manifest_path)
            with self.assertRaisesRegex(WorkspaceError, "manifest identity"):
                verify_release_tree(self.tree, self.coordinates)
            restore()

            with mock.patch.object(
                sound_module, "MAX_RELEASE_SCHEMA_BYTES", len(original_schema) - 1
            ):
                with self.assertRaisesRegex(WorkspaceError, "schema exceeds"):
                    verify_release_tree(self.tree, self.coordinates)
            restore()

            wrong_hash = "e" * 64
            self.coordinates["schema_sha256"] = wrong_hash
            self.manifest["schema_sha256"] = wrong_hash
            manifest_path.write_bytes(canonical(self.manifest))
            self.coordinates["release_manifest_sha256"] = digest(manifest_path)
            with self.assertRaisesRegex(WorkspaceError, "schema.*tampered"):
                verify_release_tree(self.tree, self.coordinates)
            restore()

            schema_path.write_bytes(b"{\n")
            self.coordinates["schema_sha256"] = digest(schema_path)
            self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
            manifest_path.write_bytes(canonical(self.manifest))
            self.coordinates["release_manifest_sha256"] = digest(manifest_path)
            self.rewrite_checksums()
            with self.assertRaisesRegex(WorkspaceError, "schema is invalid JSON"):
                verify_release_tree(self.tree, self.coordinates)
        finally:
            restore()

    def test_exact_source_codec_split_and_finite_metadata_are_required(self) -> None:
        for asset in self.manifest["assets"]:
            if asset["source"]["codec"] == "flac":
                asset["source"]["codec"] = "midi"
                asset["source"]["container"] = "midi"
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "counts.*339-path"):
            verify_release_tree(self.tree, self.coordinates)

    def test_nonstandard_json_numbers_are_rejected(self) -> None:
        self.manifest["assets"][0]["output"]["duration_seconds"] = float("nan")
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "invalid JSON"):
            verify_release_tree(self.tree, self.coordinates)

    def test_packaged_schema_must_be_valid_and_applicable(self) -> None:
        schema = self.tree / RELEASE_SCHEMA
        schema.write_text("{}\n", encoding="utf-8")
        self.coordinates["schema_sha256"] = digest(schema)
        self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "schema contract"):
            verify_release_tree(self.tree, self.coordinates)

    def test_packaged_schema_must_be_an_object(self) -> None:
        schema_path = self.tree / RELEASE_SCHEMA
        schema_path.write_text("[]\n", encoding="utf-8")
        self.coordinates["schema_sha256"] = digest(schema_path)
        self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "schema contract"):
            verify_release_tree(self.tree, self.coordinates)

    def test_packaged_schema_property_constraints_are_applied(self) -> None:
        schema_path = self.tree / RELEASE_SCHEMA
        schema = json.loads(schema_path.read_text())
        schema["properties"]["assets"] = False
        schema_path.write_bytes(canonical(schema))
        self.coordinates["schema_sha256"] = digest(schema_path)
        self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "violates its schema"):
            verify_release_tree(self.tree, self.coordinates)

    def test_packaged_schema_reference_cycles_fail_closed(self) -> None:
        schema_path = self.tree / RELEASE_SCHEMA
        schema = json.loads(schema_path.read_text())
        schema["$defs"] = {"loop": {"$ref": "#/$defs/loop"}}
        schema["properties"]["assets"] = {"$ref": "#/$defs/loop"}
        schema_path.write_bytes(canonical(schema))
        self.coordinates["schema_sha256"] = digest(schema_path)
        self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "schema reference"):
            verify_release_tree(self.tree, self.coordinates)

    def test_combinators_do_not_swallow_invalid_schema_branches(self) -> None:
        schema_path = self.tree / RELEASE_SCHEMA
        schema = json.loads(schema_path.read_text())
        schema["$defs"] = {"loop": {"$ref": "#/$defs/loop"}}
        schema["properties"]["assets"] = {
            "anyOf": [True, {"$ref": "#/$defs/loop"}]
        }
        schema_path.write_bytes(canonical(schema))
        self.coordinates["schema_sha256"] = digest(schema_path)
        self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
        (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
        self.coordinates["release_manifest_sha256"] = digest(
            self.tree / RELEASE_MANIFEST
        )
        self.rewrite_checksums()
        with self.assertRaisesRegex(WorkspaceError, "schema reference"):
            verify_release_tree(self.tree, self.coordinates)

    def test_combinators_do_not_swallow_malformed_keyword_values(self) -> None:
        for index, invalid in enumerate((
            {"enum": "invalid"},
            {"type": "bogus"},
            {"required": "invalid"},
            {"maxItems": -1},
            {"additionalProperties": "invalid"},
            {"pattern": "["},
            {"items": "invalid"},
            {"properties": {"absent": {"$ref": "invalid"}}},
            {"anyOf": None},
            {"maxItems": None},
            {"type": None},
            {"type": [{}]},
        )):
            with self.subTest(invalid=invalid):
                schema_path = self.tree / RELEASE_SCHEMA
                schema = json.loads(schema_path.read_text())
                schema["properties"]["assets"] = {"anyOf": [True, invalid]}
                schema_path.write_bytes(canonical(schema))
                self.coordinates["schema_sha256"] = digest(schema_path)
                self.manifest["schema_sha256"] = self.coordinates["schema_sha256"]
                (self.tree / RELEASE_MANIFEST).write_bytes(canonical(self.manifest))
                self.coordinates["release_manifest_sha256"] = digest(
                    self.tree / RELEASE_MANIFEST
                )
                self.rewrite_checksums()
                with self.assertRaisesRegex(WorkspaceError, "packaged schema"):
                    verify_release_tree(self.tree, self.coordinates)

    def test_schema_subset_validation_covers_supported_boundaries(self) -> None:
        structure_failures = (
            ([], "node"),
            ({"unknown": True}, "unsupported"),
            ({"maximum": float("inf")}, "maximum"),
            ({"minimum": False}, "minimum"),
            ({"enum": []}, "enum"),
            ({"required": ["a", "a"]}, "required"),
            ({"type": []}, "type"),
            ({"uniqueItems": 1}, "uniqueItems"),
            ({"pattern": "["}, "pattern"),
            ({"properties": []}, "properties"),
            ({"$defs": []}, "properties"),
            ({"additionalProperties": []}, "node"),
            ({"allOf": []}, "allOf"),
        )
        for schema, message in structure_failures:
            with self.subTest(schema=schema):
                with self.assertRaisesRegex(WorkspaceError, message):
                    sound_module._validate_release_schema_structure(schema)
        with self.assertRaisesRegex(WorkspaceError, "evaluation limits"):
            sound_module._validate_release_schema_structure({}, budget=[0])
        sound_module._validate_release_schema_structure(
            {
                "$defs": {"value": {"type": "string"}},
                "properties": {"name": {"$ref": "#/$defs/value"}},
                "items": True,
                "additionalProperties": False,
                "allOf": [True],
                "maxItems": 1,
                "minimum": 0,
                "required": ["name"],
                "type": ["object", "null"],
                "uniqueItems": False,
            }
        )

        validate = sound_module._validate_release_schema_instance
        with self.assertRaises(sound_module._ReleaseSchemaMismatch):
            validate("value", False, {})
        with self.assertRaisesRegex(WorkspaceError, "node"):
            validate("value", [], {})
        with self.assertRaisesRegex(WorkspaceError, "evaluation limits"):
            validate("value", {}, {}, budget=[0])
        with self.assertRaisesRegex(WorkspaceError, "unsupported"):
            validate("value", {"unknown": True}, {})
        for schema, message in (
            ({"maxItems": -1}, "maxItems"),
            ({"maximum": float("inf")}, "maximum"),
            ({"enum": []}, "enum"),
            ({"required": "invalid"}, "required"),
            ({"properties": []}, "properties"),
            ({"pattern": 1}, "pattern"),
            ({"uniqueItems": 1}, "uniqueItems"),
            ({"type": "invalid"}, "type"),
        ):
            with self.subTest(instance_schema=schema):
                with self.assertRaisesRegex(WorkspaceError, message):
                    validate("value", schema, {})
        with self.assertRaisesRegex(WorkspaceError, "reference is invalid"):
            validate("value", {"$ref": "invalid"}, {})
        with self.assertRaisesRegex(WorkspaceError, "reference is unresolved"):
            validate("value", {"$ref": "#/$defs/missing"}, {"$defs": {}})
        root = {"$defs": {"text": {"type": "string"}}}
        validate("value", {"$ref": "#/$defs/text"}, root)
        for keyword in ("allOf", "anyOf", "oneOf"):
            with self.subTest(keyword=keyword):
                with self.assertRaisesRegex(WorkspaceError, keyword):
                    validate("value", {keyword: []}, {})
        validate("value", {"allOf": [True, True]}, {})
        validate("value", {"anyOf": [False, True, True]}, {})
        validate("value", {"oneOf": [False, True]}, {})
        for schema, value, message in (
            ({"allOf": [True, False]}, "x", "allOf"),
            ({"anyOf": [False, False]}, "x", "anyOf"),
            ({"oneOf": [True, True]}, "x", "oneOf"),
            ({"const": "expected"}, "other", "const"),
            ({"enum": ["expected"]}, "other", "enum"),
            ({"type": "integer"}, "other", "wrong type"),
            ({"type": "number"}, float("inf"), "wrong type"),
            ({"type": "object", "required": ["name"]}, {}, "object"),
            ({"type": "object", "minProperties": 2}, {"a": 1}, "minProperties"),
            ({"type": "object", "maxProperties": 0}, {"a": 1}, "maxProperties"),
            ({"type": "array", "minItems": 2}, [1], "minItems"),
            ({"type": "array", "maxItems": 0}, [1], "maxItems"),
            ({"type": "array", "uniqueItems": True}, [1, 1], "unique"),
            ({"type": "string", "minLength": 2}, "x", "minLength"),
            ({"type": "string", "maxLength": 0}, "x", "maxLength"),
            ({"type": "string", "pattern": "^a"}, "x", "pattern"),
            ({"type": "number", "minimum": 2}, 1, "minimum"),
            ({"type": "number", "maximum": 0}, 1, "maximum"),
        ):
            with self.subTest(schema=schema, value=value):
                with self.assertRaisesRegex(
                    sound_module._ReleaseSchemaMismatch, message
                ):
                    validate(value, schema, {})
        validate(
            {"name": "value", "extra": 1},
            {
                "type": "object",
                "properties": {"name": {"type": "string"}},
                "additionalProperties": {"type": "integer"},
            },
            {},
        )
        validate(["a", "b"], {"type": "array", "items": {"type": "string"}}, {})

    def test_build_metadata_and_supervised_topology_reuse_verified_root(self) -> None:
        wrapper = self.root / "topology-wrapper"
        wrapper.mkdir()
        shutil.copy2(Path(__file__).resolve().parents[1] / "components.json", wrapper)
        workspace_root = self.root / "topology-workspace"
        previous = os.environ.get("ATRINIK_WORKSPACE_DIR")
        os.environ["ATRINIK_WORKSPACE_DIR"] = str(workspace_root)
        topology_name = "released-client"
        workspace: Workspace | None = None
        try:
            workspace = Workspace(wrapper)
            workspace.paths.ensure()
            workspace.create_profile("classic-release", "classic")
            workspace.set_profile_sound_mode(
                "classic-release", "released", self.coordinates
            )
            build_root = workspace.paths.builds / "released-fixture"
            build_root.mkdir(parents=True)
            with mock.patch(
                "atrinik_workspace.workspace.download_release_archive",
                side_effect=lambda _url, destination: shutil.copy2(
                    self.archive, destination
                ),
            ):
                sound_root, sound_record = workspace._prepare_sound(
                    build_root,
                    {"sound": self.root / "unused-source"},
                    "classic-release",
                )
            executable = build_root / "build" / "client" / "atrinik"
            executable.parent.mkdir(parents=True)
            executable.write_text(
                "#!/usr/bin/env python3\nimport time\n"
                "print('released client ready', flush=True)\ntime.sleep(5)\n",
                encoding="utf-8",
            )
            executable.chmod(0o755)
            (build_root / BUILD_METADATA).write_text(
                json.dumps({"sound": sound_record}), encoding="utf-8"
            )
            classic = self.root / "classic"
            sound = self.root / "unused-source"
            for source in ("client", "libatrinik", "protocol"):
                component_path = classic / source
                component_path.mkdir(parents=True, exist_ok=True)
                (component_path / "tracked").write_text(
                    "fixture\n", encoding="utf-8"
                )
            heads: dict[str, str] = {}
            for checkout, path in (("classic", classic), ("sound", sound)):
                path.mkdir(exist_ok=True)
                (path / "tracked").write_text("fixture\n", encoding="utf-8")
                subprocess.run(["git", "init", "-q", str(path)], check=True)
                subprocess.run(["git", "-C", str(path), "add", "."], check=True)
                subprocess.run(
                    [
                        "git",
                        "-C",
                        str(path),
                        "-c",
                        "user.name=Fixture",
                        "-c",
                        "user.email=fixture@example.invalid",
                        "commit",
                        "-qm",
                        "test: seed fixture",
                    ],
                    check=True,
                )
                heads[checkout] = subprocess.run(
                    ["git", "-C", str(path), "rev-parse", "HEAD"],
                    check=True,
                    capture_output=True,
                    text=True,
                ).stdout.strip()
            selected = {
                "client": classic / "client",
                "libatrinik": classic / "libatrinik",
                "protocol": classic / "protocol",
                "sound": sound,
            }
            resolved: dict[str, dict[str, object]] = {}
            for role, path in selected.items():
                component = workspace.manifest.by_name[
                    role if role == "sound" else f"classic-{role}"
                ]
                checkout_path = sound if role == "sound" else classic
                resolved[component.name] = {
                    "path": str(path),
                    "checkout_path": str(checkout_path),
                    "checkout": component.checkout_name,
                    "repository": component.repository,
                    "branch": "main",
                    "source": component.source,
                    "head": heads[component.checkout_name],
                    "dirty": False,
                }
            classic_stack = mock.Mock()
            classic_stack.name = "classic"
            classic_stack.components = workspace.manifest.stack("classic").components
            classic_stack.providers = {
                "client": workspace.manifest.by_name["classic-client"],
                "libatrinik": workspace.manifest.by_name["classic-libatrinik"],
                "protocol": workspace.manifest.by_name["classic-protocol"],
                "sound": workspace.manifest.by_name["sound"],
            }
            with (
                mock.patch.object(
                    workspace.manifest, "stack", return_value=classic_stack
                ),
                mock.patch.object(workspace, "_require_classic_contracts"),
                mock.patch.object(workspace, "_require_client_display"),
                mock.patch.object(
                    workspace, "_resolve_build_profile", return_value=selected
                ),
                mock.patch.object(
                    workspace,
                    "_selected_checkout_states",
                    return_value={
                        "classic": {
                            "path": classic,
                            "head": heads["classic"],
                            "dirty": False,
                        },
                        "sound": {
                            "path": sound,
                            "head": heads["sound"],
                            "dirty": False,
                        },
                    },
                ),
                mock.patch.object(workspace, "_build_resolved", return_value=build_root),
                mock.patch.object(
                    workspace, "_topology_resolved_status", return_value=resolved
                ),
            ):
                status = workspace.topology_up(
                    topology_name, "classic-release", "default", ["client"]
                )
            self.assertEqual(status["sound"], sound_record)
            runtime = Path(status["services"]["client"]["cwd"])
            runtime_sound = verify_release_tree(runtime / "sound", self.coordinates)
            self.assertEqual(
                {**runtime_sound, "root": "<verified-root>"},
                {**sound_record, "root": "<verified-root>"},
            )
            spec = json.loads(
                (workspace.paths.topologies / topology_name / "spec.json").read_text()
            )
            self.assertEqual(spec["sound"], sound_record)
        finally:
            if workspace is not None:
                try:
                    if "classic_stack" in locals():
                        with mock.patch.object(
                            workspace.manifest, "stack", return_value=classic_stack
                        ):
                            workspace.topology_down(topology_name, timeout=5)
                    else:
                        workspace.topology_down(topology_name, timeout=5)
                except WorkspaceError:
                    pass
            if previous is None:
                os.environ.pop("ATRINIK_WORKSPACE_DIR", None)
            else:
                os.environ["ATRINIK_WORKSPACE_DIR"] = previous

    def test_racing_cache_install_leaves_no_partial_runtime(self) -> None:
        wrapper = self.root / "race-wrapper"
        wrapper.mkdir()
        shutil.copy2(Path(__file__).resolve().parents[1] / "components.json", wrapper)
        workspace = Workspace(wrapper)
        build = self.root / "race-build"
        build.mkdir()
        profile = {
            "stack": "classic",
            "sound_mode": "released",
            "sound_release": self.coordinates,
        }
        with (
            mock.patch.object(workspace, "_load_profile", return_value=profile),
            mock.patch(
                "atrinik_workspace.workspace.download_release_archive",
                side_effect=lambda _url, destination: shutil.copy2(
                    self.archive, destination
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace.rename_no_replace",
                side_effect=FileExistsError("raced install"),
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "raced install"):
                workspace._prepare_sound(
                    build, {"sound": self.root / "unused-source"}, "classic-release"
                )
        runtime = build / "runtime"
        self.assertEqual(list(runtime.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
