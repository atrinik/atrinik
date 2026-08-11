from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
from typing import Any

from .model import WorkspaceError, load_json


PLAYTEST_MODE = "local-playtest"
SOURCE_MODE = "source"
SOUND_MODES = {PLAYTEST_MODE, SOURCE_MODE}
PLAYTEST_MANIFEST = "playtest-manifest.json"
PLAYTEST_BLOCKERS = "playtest-blockers.json"
PLAYTEST_MARKER = ".atrinik-playtest-tree.json"
PLAYTEST_SCHEMA = "schemas/playtest-manifest-v1.schema.json"
EXPECTED_PATHS = 339
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
OBJECT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
LOGICAL_PATH_PATTERN = re.compile(
    r"^(background|effects)/(?:[a-z0-9][a-z0-9_.-]*/)*"
    r"[a-z0-9][a-z0-9_.-]*\.(mid|mod|s3m|xm|ogg)$"
)
TOOL_NAMES = {
    "ffmpeg",
    "openmpt123",
    "opusenc",
    "opusinfo",
    "sdl3_mixer_probe",
    "wildmidi",
}


def _canonical_json(value: object) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _hash_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _read_regular(path: Path, description: str) -> bytes:
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    except OSError as error:
        raise WorkspaceError(f"{description} is not a readable regular file: {path}") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise WorkspaceError(f"{description} is not a regular file: {path}")
        chunks: list[bytes] = []
        while chunk := os.read(descriptor, 1024 * 1024):
            chunks.append(chunk)
        return b"".join(chunks)
    finally:
        os.close(descriptor)


def _hash_regular(path: Path, description: str) -> tuple[str, int, bytes]:
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    except OSError as error:
        raise WorkspaceError(
            f"{description} is not a readable regular file: {path}"
        ) from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise WorkspaceError(f"{description} is not a regular file: {path}")
        digest = hashlib.sha256()
        prefix = bytearray()
        size = 0
        while chunk := os.read(descriptor, 1024 * 1024):
            digest.update(chunk)
            size += len(chunk)
            if len(prefix) < 65536:
                prefix.extend(chunk[: 65536 - len(prefix)])
        return digest.hexdigest(), size, bytes(prefix)
    finally:
        os.close(descriptor)


def _git(source: Path, *arguments: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(source), *arguments],
            check=True,
            text=True,
            capture_output=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        raise WorkspaceError(
            f"cannot inspect local-playtest sound source identity: {source}"
        ) from error
    return result.stdout.strip()


def clean_source_inputs(source: Path) -> dict[str, str]:
    """Return exact immutable builder inputs from one clean sound checkout."""

    before = _git(source, "status", "--porcelain", "--untracked-files=all")
    commit = _git(source, "rev-parse", "HEAD")
    tree = _git(source, "rev-parse", f"{commit}^{{tree}}")
    files = {
        "builder_sha256": source / "tools" / "sound_release.py",
        "source_manifest_sha256": source / "manifests" / "source-assets.json",
        "toolchain_sha256": source / "manifests" / "playtest-audio-toolchain.json",
        "schema_sha256": source / "schemas" / "playtest-manifest-v1.schema.json",
    }
    hashes = {
        name: _hash_regular(path, name.replace("_", " "))[0]
        for name, path in files.items()
    }
    after = _git(source, "status", "--porcelain", "--untracked-files=all")
    final_commit = _git(source, "rev-parse", "HEAD")
    if before or after:
        raise WorkspaceError(
            f"local-playtest sound mode requires a clean selected checkout: {source}"
        )
    if commit != final_commit:
        raise WorkspaceError(
            f"local-playtest sound source changed while reading its identity: {source}"
        )
    if not OBJECT_PATTERN.fullmatch(commit) or not OBJECT_PATTERN.fullmatch(tree):
        raise WorkspaceError(
            f"local-playtest sound source has invalid Git coordinates: {source}"
        )
    return {"source_commit": commit, "source_tree": tree, **hashes}


def cache_key(inputs: dict[str, str]) -> str:
    payload = json.dumps(inputs, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:20]


def source_record(source: Path) -> dict[str, Any]:
    commit = _git(source, "rev-parse", "HEAD")
    tree = _git(source, "rev-parse", f"{commit}^{{tree}}")
    return {
        "mode": SOURCE_MODE,
        "root": str(source.resolve()),
        "source_commit": commit,
        "source_tree": tree,
        "source_clean": not bool(
            _git(source, "status", "--porcelain", "--untracked-files=all")
        ),
    }


def _require_dict(value: object, keys: set[str], description: str) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != keys:
        raise WorkspaceError(f"local-playtest {description} fields are invalid")
    return value


def _tree_files(root: Path) -> set[str]:
    result: set[str] = set()
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        for name in directories:
            path = parent / name
            try:
                mode = path.lstat().st_mode
            except OSError as error:
                raise WorkspaceError(f"cannot inspect local-playtest tree: {path}") from error
            if not stat.S_ISDIR(mode):
                raise WorkspaceError(
                    f"local-playtest tree contains a non-directory or symlink: {path}"
                )
        for name in files:
            path = parent / name
            relative = path.relative_to(root).as_posix()
            try:
                descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
            except OSError as error:
                raise WorkspaceError(
                    f"local-playtest tree entry is not a readable regular file: {path}"
                ) from error
            try:
                if not stat.S_ISREG(os.fstat(descriptor).st_mode):
                    raise WorkspaceError(
                        f"local-playtest tree entry is not a regular file: {path}"
                    )
            finally:
                os.close(descriptor)
            result.add(relative)
    return result


def _validate_payload_codec(path: str, codec: str, prefix: bytes) -> None:
    if not prefix.startswith(b"OggS"):
        raise WorkspaceError(f"local-playtest payload is not an Ogg stream: {path}")
    signature = b"OpusHead" if codec == "opus" else b"\x01vorbis"
    if signature not in prefix:
        raise WorkspaceError(
            f"local-playtest payload codec does not match its manifest: {path}"
        )


def verify_playtest_tree(
    source: Path, root: Path, expected_inputs: dict[str, str]
) -> dict[str, Any]:
    """Independently verify the public sound playtest-tree version 1 contract."""

    if root.is_symlink() or not root.is_dir():
        raise WorkspaceError(f"local-playtest sound root is not a regular directory: {root}")
    manifest_payload = _read_regular(root / PLAYTEST_MANIFEST, "playtest manifest")
    try:
        manifest_value = json.loads(manifest_payload)
    except json.JSONDecodeError as error:
        raise WorkspaceError("local-playtest manifest is invalid JSON") from error
    if manifest_payload != _canonical_json(manifest_value):
        raise WorkspaceError("local-playtest manifest is not canonical JSON")
    manifest = _require_dict(
        manifest_value,
        {
            "$schema",
            "assets",
            "blocker_count",
            "blocker_report_sha256",
            "converted_opus_count",
            "copied_vorbis_count",
            "logical_path_count",
            "marker_sha256",
            "output_tree_sha256",
            "playtest_only",
            "publishable",
            "schema_sha256",
            "schema_version",
            "source_commit",
            "source_manifest_sha256",
            "source_tree",
            "tool_versions",
            "toolchain_sha256",
        },
        "manifest",
    )
    if (
        manifest["$schema"] != PLAYTEST_SCHEMA
        or manifest["schema_version"] != 1
        or manifest["playtest_only"] is not True
        or manifest["publishable"] is not False
    ):
        raise WorkspaceError(
            "local-playtest manifest must use schema 1 with "
            "playtest_only=true and publishable=false"
        )
    for key in (
        "source_commit",
        "source_tree",
        "source_manifest_sha256",
        "toolchain_sha256",
        "schema_sha256",
    ):
        if manifest.get(key) != expected_inputs.get(key):
            raise WorkspaceError(f"local-playtest manifest has stale or tampered {key}")
    toolchain = load_json(source / "manifests" / "playtest-audio-toolchain.json")
    if (
        not isinstance(toolchain, dict)
        or toolchain.get("$schema")
        != "../schemas/playtest-audio-toolchain-v1.schema.json"
        or toolchain.get("schema_version") != 1
    ):
        raise WorkspaceError("selected sound toolchain schema is invalid")
    tool_versions = manifest.get("tool_versions")
    if (
        not isinstance(tool_versions, dict)
        or set(tool_versions) != TOOL_NAMES
        or not all(isinstance(value, str) and value for value in tool_versions.values())
    ):
        raise WorkspaceError("local-playtest toolchain versions are invalid")

    marker_payload = _read_regular(root / PLAYTEST_MARKER, "playtest marker")
    expected_marker = _canonical_json(
        {
            "format": "atrinik-sound-playtest-tree",
            "playtest_only": True,
            "publishable": False,
            "schema_version": 1,
        }
    )
    if marker_payload != expected_marker or manifest["marker_sha256"] != _hash_bytes(
        marker_payload
    ):
        raise WorkspaceError("local-playtest ownership marker is missing or tampered")
    schema_hash, _schema_size, _schema_prefix = _hash_regular(
        root / PLAYTEST_SCHEMA, "packaged playtest schema"
    )
    if schema_hash != manifest["schema_sha256"]:
        raise WorkspaceError("local-playtest packaged schema is missing or tampered")
    blocker_payload = _read_regular(root / PLAYTEST_BLOCKERS, "playtest blocker report")
    if _hash_bytes(blocker_payload) != manifest["blocker_report_sha256"]:
        raise WorkspaceError("local-playtest blocker report is missing or tampered")
    try:
        blockers = json.loads(blocker_payload)
    except json.JSONDecodeError as error:
        raise WorkspaceError("local-playtest blocker report is invalid JSON") from error
    blocker_report = _require_dict(
        blockers,
        {"schema_version", "source_manifest_sha256", "source_count", "count", "findings"},
        "blocker report",
    )
    if (
        blocker_payload != _canonical_json(blocker_report)
        or blocker_report["schema_version"] != 1
        or blocker_report["source_manifest_sha256"]
        != manifest["source_manifest_sha256"]
        or blocker_report["source_count"] != EXPECTED_PATHS
        or blocker_report["count"] != manifest["blocker_count"]
        or not isinstance(blocker_report["findings"], list)
        or len(blocker_report["findings"]) != blocker_report["count"]
    ):
        raise WorkspaceError("local-playtest blocker report contract is invalid")

    source_manifest = load_json(source / "manifests" / "source-assets.json")
    if not isinstance(source_manifest, dict) or not isinstance(
        source_manifest.get("assets"), list
    ):
        raise WorkspaceError("selected sound source manifest is invalid")
    source_assets = source_manifest["assets"]
    expected_by_path: dict[str, dict[str, Any]] = {}
    for value in source_assets:
        if not isinstance(value, dict) or not isinstance(value.get("logical_path"), str):
            raise WorkspaceError("selected sound source manifest assets are invalid")
        logical_path = value["logical_path"]
        if logical_path in expected_by_path:
            raise WorkspaceError("selected sound source manifest has duplicate logical paths")
        expected_by_path[logical_path] = value
    expected_copied = sum(
        isinstance(value.get("source"), dict)
        and value["source"].get("codec") == "vorbis"
        for value in expected_by_path.values()
    )
    expected_converted = len(expected_by_path) - expected_copied

    assets = manifest.get("assets")
    if not isinstance(assets, list):
        raise WorkspaceError("local-playtest manifest assets must be an array")
    actual_by_path: dict[str, dict[str, Any]] = {}
    tree_digest = hashlib.sha256()
    copied = 0
    converted = 0
    for value in assets:
        asset = _require_dict(
            value,
            {"logical_path", "mapping", "output", "source", "source_path"},
            "asset",
        )
        logical_path = asset.get("logical_path")
        if (
            not isinstance(logical_path, str)
            or not LOGICAL_PATH_PATTERN.fullmatch(logical_path)
            or PurePosixPath(logical_path).is_absolute()
            or ".." in PurePosixPath(logical_path).parts
            or logical_path in actual_by_path
        ):
            raise WorkspaceError("local-playtest manifest has an unsafe or duplicate path")
        actual_by_path[logical_path] = asset
        expected = expected_by_path.get(logical_path)
        if expected is None:
            raise WorkspaceError(f"local-playtest tree has an extra logical path: {logical_path}")
        expected_source = expected.get("source")
        if not isinstance(expected_source, dict):
            raise WorkspaceError("selected sound source manifest asset is invalid")
        source_value = _require_dict(
            asset.get("source"), {"sha256", "codec", "container"}, "asset source"
        )
        output = _require_dict(
            asset.get("output"),
            {
                "channels",
                "codec",
                "container",
                "duration_seconds",
                "sample_rate",
                "sha256",
                "size_bytes",
            },
            "asset output",
        )
        source_codec = expected_source.get("codec")
        expected_mapping = "copy" if source_codec == "vorbis" else "render-opus"
        expected_codec = "vorbis" if source_codec == "vorbis" else "opus"
        if (
            asset.get("source_path") != expected.get("source_path")
            or asset.get("mapping") != expected_mapping
            or source_value
            != {
                "sha256": expected_source.get("sha256"),
                "codec": source_codec,
                "container": expected_source.get("container"),
            }
            or output.get("codec") != expected_codec
            or output.get("container") != "ogg"
            or not isinstance(output.get("size_bytes"), int)
            or isinstance(output.get("size_bytes"), bool)
            or output["size_bytes"] < 1
            or not isinstance(output.get("sample_rate"), int)
            or isinstance(output.get("sample_rate"), bool)
            or output["sample_rate"] < 1
            or output.get("channels") not in {1, 2}
            or not isinstance(output.get("duration_seconds"), (int, float))
            or isinstance(output.get("duration_seconds"), bool)
            or output["duration_seconds"] <= 0
            or not isinstance(output.get("sha256"), str)
            or not SHA256_PATTERN.fullmatch(output["sha256"])
        ):
            raise WorkspaceError(f"local-playtest mapping is invalid: {logical_path}")
        payload_hash, payload_size, payload_prefix = _hash_regular(
            root / logical_path, f"local-playtest payload {logical_path}"
        )
        if payload_hash != output["sha256"] or payload_size != output["size_bytes"]:
            raise WorkspaceError(f"local-playtest payload hash or size mismatch: {logical_path}")
        if expected_mapping == "copy" and output["sha256"] != source_value["sha256"]:
            raise WorkspaceError(f"local-playtest Vorbis copy differs from source: {logical_path}")
        _validate_payload_codec(logical_path, expected_codec, payload_prefix)
        copied += expected_mapping == "copy"
        converted += expected_mapping == "render-opus"
        tree_digest.update(f"{payload_hash}  {logical_path}\n".encode("ascii"))

    if set(actual_by_path) != set(expected_by_path):
        missing = sorted(set(expected_by_path) - set(actual_by_path))
        raise WorkspaceError(f"local-playtest tree is missing logical path: {missing[0]}")
    if (
        len(assets) != EXPECTED_PATHS
        or manifest["logical_path_count"] != EXPECTED_PATHS
        or copied != expected_copied
        or manifest["copied_vorbis_count"] != expected_copied
        or converted != expected_converted
        or manifest["converted_opus_count"] != expected_converted
    ):
        raise WorkspaceError(
            "local-playtest tree counts do not match the exact 339-path source corpus"
        )
    allowed = set(actual_by_path) | {
        PLAYTEST_MANIFEST,
        PLAYTEST_BLOCKERS,
        PLAYTEST_MARKER,
        PLAYTEST_SCHEMA,
    }
    if _tree_files(root) != allowed:
        raise WorkspaceError("local-playtest tree has missing or unexpected files")
    digest = tree_digest.hexdigest()
    if manifest.get("output_tree_sha256") != digest:
        raise WorkspaceError("local-playtest output-tree digest mismatch")
    return {
        "mode": PLAYTEST_MODE,
        "root": str(root.resolve()),
        "playtest_manifest_sha256": _hash_bytes(manifest_payload),
        "playtest_schema_version": manifest["schema_version"],
        "source_commit": manifest["source_commit"],
        "source_tree": manifest["source_tree"],
        "source_clean": True,
        "source_manifest_sha256": manifest["source_manifest_sha256"],
        "toolchain_sha256": manifest["toolchain_sha256"],
        "toolchain_schema": toolchain["$schema"],
        "toolchain_schema_version": toolchain["schema_version"],
        "schema_sha256": manifest["schema_sha256"],
        "marker_sha256": manifest["marker_sha256"],
        "blocker_report_sha256": manifest["blocker_report_sha256"],
        "output_tree_sha256": digest,
        "logical_path_count": EXPECTED_PATHS,
        "copied_vorbis_count": expected_copied,
        "converted_opus_count": expected_converted,
    }


SOURCE_RECORD_KEYS = {
    "mode", "root", "source_commit", "source_tree", "source_clean",
}
PLAYTEST_RECORD_KEYS = {
    "mode", "root", "playtest_manifest_sha256", "playtest_schema_version",
    "source_commit", "source_tree", "source_clean", "source_manifest_sha256",
    "toolchain_sha256", "toolchain_schema", "toolchain_schema_version",
    "schema_sha256", "marker_sha256", "blocker_report_sha256",
    "output_tree_sha256", "logical_path_count", "copied_vorbis_count",
    "converted_opus_count",
}


def validate_sound_record(value: object) -> dict[str, Any]:
    """Validate the exact persisted provenance record for either sound mode."""

    if not isinstance(value, dict):
        raise WorkspaceError("sound provenance record is not an object")
    mode = value.get("mode")
    expected = SOURCE_RECORD_KEYS if mode == SOURCE_MODE else PLAYTEST_RECORD_KEYS
    if mode not in SOUND_MODES or set(value) != expected:
        raise WorkspaceError("sound provenance record fields are invalid")
    root = value.get("root")
    if not isinstance(root, str) or not root or not Path(root).is_absolute():
        raise WorkspaceError("sound provenance root is invalid")
    if (
        not isinstance(value.get("source_commit"), str)
        or not OBJECT_PATTERN.fullmatch(value["source_commit"])
        or not isinstance(value.get("source_tree"), str)
        or not OBJECT_PATTERN.fullmatch(value["source_tree"])
        or not isinstance(value.get("source_clean"), bool)
    ):
        raise WorkspaceError("sound provenance source identity is invalid")
    if mode == SOURCE_MODE:
        return value
    hashes = PLAYTEST_RECORD_KEYS - {
        "mode", "root", "playtest_schema_version", "source_commit",
        "source_tree", "source_clean", "toolchain_schema_version",
        "logical_path_count", "copied_vorbis_count", "converted_opus_count",
        "toolchain_schema",
    }
    if (
        value["source_clean"] is not True
        or any(
            not isinstance(value.get(name), str)
            or not SHA256_PATTERN.fullmatch(value[name])
            for name in hashes
        )
        or value.get("playtest_schema_version") != 1
        or value.get("toolchain_schema_version") != 1
        or value.get("toolchain_schema")
        != "../schemas/playtest-audio-toolchain-v1.schema.json"
        or value.get("logical_path_count") != EXPECTED_PATHS
        or not isinstance(value.get("copied_vorbis_count"), int)
        or isinstance(value.get("copied_vorbis_count"), bool)
        or value["copied_vorbis_count"] < 0
        or not isinstance(value.get("converted_opus_count"), int)
        or isinstance(value.get("converted_opus_count"), bool)
        or value["converted_opus_count"] < 0
        or value["copied_vorbis_count"] + value["converted_opus_count"]
        != EXPECTED_PATHS
    ):
        raise WorkspaceError("local-playtest sound provenance is invalid")
    return value
