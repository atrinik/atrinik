from __future__ import annotations

import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.sound import (
    PLAYTEST_BLOCKERS,
    PLAYTEST_MANIFEST,
    PLAYTEST_MARKER,
    PLAYTEST_SCHEMA,
    cache_key,
    clean_source_inputs,
    validate_sound_record,
    verify_playtest_tree,
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
        for asset in playtest_assets:
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


if __name__ == "__main__":
    unittest.main()
