from __future__ import annotations

import hashlib
import gzip
import http.client
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tarfile
from typing import Any
import urllib.error
import urllib.parse
import urllib.request

from .model import WorkspaceError


PLAYTEST_MODE = "local-playtest"
RELEASED_MODE = "released"
SOURCE_MODE = "source"
SOUND_MODES = {PLAYTEST_MODE, RELEASED_MODE, SOURCE_MODE}
PLAYTEST_MANIFEST = "playtest-manifest.json"
PLAYTEST_BLOCKERS = "playtest-blockers.json"
PLAYTEST_MARKER = ".atrinik-playtest-tree.json"
PLAYTEST_SCHEMA = "schemas/playtest-manifest-v1.schema.json"
EXPECTED_PATHS = 339
EXPECTED_COPIED_VORBIS = 189
EXPECTED_CONVERTED_OPUS = 150
EXPECTED_SOURCE_MIDI = 122
EXPECTED_SOURCE_FLAC = 28
RELEASE_PRODUCT = "atrinik-sound-classic-runtime"
RELEASE_MANIFEST = "classic-runtime-manifest.json"
RELEASE_MARKER = ".atrinik-classic-runtime.json"
RELEASE_SCHEMA = "schemas/classic-runtime-manifest-v1.schema.json"
RELEASE_CHECKSUMS = "SHA256SUMS"
RELEASE_COORDINATE_KEYS = {
    "archive_sha256",
    "asset_url",
    "manifest_schema_version",
    "product",
    "product_version",
    "release_manifest_sha256",
    "repository",
    "schema_sha256",
    "source_commit",
    "source_manifest_sha256",
    "source_tree",
    "tag",
    "toolchain_sha256",
    "output_tree_sha256",
}
MAX_RELEASE_ARCHIVE_BYTES = 1024 * 1024 * 1024
MAX_RELEASE_EXTRACTED_BYTES = 4 * 1024 * 1024 * 1024
MAX_RELEASE_MEMBER_BYTES = 256 * 1024 * 1024
MAX_RELEASE_MEMBERS = 4096
MAX_RELEASE_TAR_BYTES = MAX_RELEASE_EXTRACTED_BYTES + MAX_RELEASE_MEMBERS * 1024
MAX_RELEASE_MANIFEST_BYTES = 8 * 1024 * 1024
MAX_RELEASE_CHECKSUM_BYTES = 256 * 1024
MAX_RELEASE_MARKER_BYTES = 4096
MAX_RELEASE_SCHEMA_BYTES = 1024 * 1024
MAX_RELEASE_METADATA_BYTES = 64 * 1024
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


def _read_regular(
    path: Path, description: str, *, maximum_bytes: int | None = None
) -> bytes:
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    except OSError as error:
        raise WorkspaceError(f"{description} is not a readable regular file: {path}") from error
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise WorkspaceError(f"{description} is not a regular file: {path}")
        if maximum_bytes is not None and metadata.st_size > maximum_bytes:
            raise WorkspaceError(f"{description} exceeds its size limit")
        chunks: list[bytes] = []
        size = 0
        while chunk := os.read(descriptor, 1024 * 1024):
            size += len(chunk)
            if maximum_bytes is not None and size > maximum_bytes:
                raise WorkspaceError(f"{description} exceeds its size limit")
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


def validate_release_coordinates(value: object) -> dict[str, Any]:
    """Validate the complete immutable identity of one released sound archive."""

    if not isinstance(value, dict) or set(value) != RELEASE_COORDINATE_KEYS:
        raise WorkspaceError("released sound coordinates fields are invalid")
    hashes = RELEASE_COORDINATE_KEYS - {
        "asset_url",
        "manifest_schema_version",
        "product",
        "product_version",
        "repository",
        "source_commit",
        "source_tree",
        "tag",
    }
    if any(
        not isinstance(value.get(name), str)
        or not SHA256_PATTERN.fullmatch(value[name])
        for name in hashes
    ):
        raise WorkspaceError("released sound coordinates contain an invalid SHA-256")
    if (
        value.get("repository") != "atrinik/sound"
        or value.get("product") != RELEASE_PRODUCT
        or value.get("manifest_schema_version") != 1
        or not isinstance(value.get("product_version"), str)
        or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", value["product_version"])
        or value.get("tag") != f"v{value.get('product_version')}"
        or not isinstance(value.get("source_commit"), str)
        or not OBJECT_PATTERN.fullmatch(value["source_commit"])
        or not isinstance(value.get("source_tree"), str)
        or not OBJECT_PATTERN.fullmatch(value["source_tree"])
    ):
        raise WorkspaceError("released sound coordinates contain an invalid product identity")
    url = value.get("asset_url")
    if not isinstance(url, str):
        raise WorkspaceError("released sound asset URL is invalid")
    parsed = urllib.parse.urlsplit(url)
    expected_path = (
        f"/atrinik/sound/releases/download/{value['tag']}/"
        f"{RELEASE_PRODUCT}-{value['product_version']}.tar.gz"
    )
    if (
        parsed.scheme != "https"
        or parsed.hostname != "github.com"
        or parsed.username is not None
        or parsed.password is not None
        or parsed.port is not None
        or parsed.query
        or parsed.fragment
        or parsed.path != expected_path
    ):
        raise WorkspaceError(
            "released sound asset URL must be the exact atrinik/sound release asset"
        )
    return value


def release_cache_key(coordinates: dict[str, Any]) -> str:
    validated = validate_release_coordinates(coordinates)
    payload = json.dumps(validated, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:20]


def verify_release_archive(
    archive_path: Path, coordinates: dict[str, Any], description: str
) -> None:
    expected = validate_release_coordinates(coordinates)
    archive_hash, size, _prefix = _hash_regular(archive_path, description)
    if size < 1 or archive_hash != expected["archive_sha256"]:
        raise WorkspaceError(f"{description} checksum mismatch")


def download_release_archive(url: str, destination: Path) -> None:
    """Download one bounded release asset into a new caller-owned path."""

    request = urllib.request.Request(
        url,
        headers={"User-Agent": "atrinik-workspace-released-sound/1"},
    )
    try:
        response = urllib.request.urlopen(request, timeout=60)
    except (OSError, urllib.error.URLError) as error:
        raise WorkspaceError(f"cannot download released sound archive: {error}") from error
    try:
        length = response.headers.get("Content-Length")
        if length is not None:
            try:
                declared = int(length)
            except ValueError as error:
                raise WorkspaceError(
                    "released sound download has an invalid Content-Length"
                ) from error
            if declared < 1 or declared > MAX_RELEASE_ARCHIVE_BYTES:
                raise WorkspaceError("released sound archive exceeds the download limit")
        descriptor = os.open(
            destination,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
            0o600,
        )
        size = 0
        with os.fdopen(descriptor, "wb") as output:
            while chunk := response.read(1024 * 1024):
                size += len(chunk)
                if size > MAX_RELEASE_ARCHIVE_BYTES:
                    raise WorkspaceError(
                        "released sound archive exceeds the download limit"
                    )
                output.write(chunk)
        if size < 1 or length is not None and size != declared:
            raise WorkspaceError("released sound download is incomplete")
    except WorkspaceError:
        raise
    except (OSError, urllib.error.URLError, http.client.HTTPException) as error:
        raise WorkspaceError("released sound download was interrupted") from error
    finally:
        try:
            response.close()
        except (OSError, http.client.HTTPException):
            pass


def _prescan_release_archive(archive_path: Path) -> None:
    """Bound raw tar expansion and reject metadata parsed eagerly by tarfile."""

    consumed = 0
    members = 0
    try:
        with gzip.open(archive_path, "rb") as stream:
            while True:
                header = stream.read(tarfile.BLOCKSIZE)
                consumed += len(header)
                if consumed > MAX_RELEASE_TAR_BYTES:
                    raise WorkspaceError("released sound archive exceeds the extraction limit")
                if not header:
                    raise WorkspaceError("released sound archive is truncated")
                if len(header) != tarfile.BLOCKSIZE:
                    raise WorkspaceError("released sound archive has a truncated header")
                if header == tarfile.NUL * tarfile.BLOCKSIZE:
                    trailer = stream.read(tarfile.BLOCKSIZE)
                    consumed += len(trailer)
                    if trailer != tarfile.NUL * tarfile.BLOCKSIZE:
                        raise WorkspaceError("released sound archive has an invalid trailer")
                    while chunk := stream.read(1024 * 1024):
                        consumed += len(chunk)
                        if consumed > MAX_RELEASE_TAR_BYTES or any(chunk):
                            raise WorkspaceError("released sound archive has invalid trailing data")
                    return
                try:
                    member = tarfile.TarInfo.frombuf(
                        header, encoding="utf-8", errors="surrogateescape"
                    )
                except tarfile.HeaderError as error:
                    raise WorkspaceError("released sound archive header is invalid") from error
                members += 1
                if members > MAX_RELEASE_MEMBERS:
                    raise WorkspaceError("released sound archive member count is invalid")
                if member.type in {
                    tarfile.XHDTYPE,
                    tarfile.XGLTYPE,
                    tarfile.SOLARIS_XHDTYPE,
                    tarfile.GNUTYPE_LONGNAME,
                    tarfile.GNUTYPE_LONGLINK,
                }:
                    raise WorkspaceError(
                        "released sound archive contains unsupported extended metadata"
                    )
                if member.size < 0 or member.size > MAX_RELEASE_MEMBER_BYTES:
                    raise WorkspaceError("released sound archive member exceeds the size limit")
                padded = (member.size + tarfile.BLOCKSIZE - 1) // tarfile.BLOCKSIZE
                remaining = padded * tarfile.BLOCKSIZE
                consumed += remaining
                if consumed > MAX_RELEASE_TAR_BYTES:
                    raise WorkspaceError("released sound archive exceeds the extraction limit")
                while remaining:
                    chunk = stream.read(min(1024 * 1024, remaining))
                    if not chunk:
                        raise WorkspaceError("released sound archive member is truncated")
                    remaining -= len(chunk)
    except WorkspaceError:
        raise
    except (OSError, EOFError, gzip.BadGzipFile) as error:
        raise WorkspaceError("released sound archive is invalid or corrupt") from error


def _extract_release_archive(
    archive_path: Path, destination: Path, coordinates: dict[str, Any]
) -> Path:
    """Safely extract a bounded single-prefix release archive."""

    expected = validate_release_coordinates(coordinates)
    _prescan_release_archive(archive_path)
    if destination.exists() or destination.is_symlink():
        raise WorkspaceError("released sound extraction destination already exists")
    _archive_hash, archive_size, _archive_prefix = _hash_regular(
        archive_path, "released sound archive"
    )
    if archive_size < 1 or archive_size > MAX_RELEASE_ARCHIVE_BYTES:
        raise WorkspaceError("released sound archive exceeds the extraction input limit")
    try:
        archive = tarfile.open(archive_path, mode="r:gz")
    except (OSError, tarfile.TarError) as error:
        raise WorkspaceError("released sound archive is invalid or corrupt") from error
    with archive:
        prefix: str | None = None
        seen: set[str] = set()
        folded: set[str] = set()
        directories: set[str] = set()
        root_seen = False
        files: list[tuple[tarfile.TarInfo, PurePosixPath]] = []
        total = 0
        member_count = 0
        for member in archive:
            member_count += 1
            if member_count > MAX_RELEASE_MEMBERS:
                raise WorkspaceError("released sound archive member count is invalid")
            path = PurePosixPath(member.name)
            if (
                path.is_absolute()
                or not path.parts
                or any(part in {"", ".", ".."} for part in path.parts)
                or "\\" in member.name
                or member.name.endswith("/") and not member.isdir()
            ):
                raise WorkspaceError("released sound archive contains an unsafe path")
            if prefix is None:
                prefix = path.parts[0]
            if path.parts[0] != prefix or len(path.parts) == 1 and not member.isdir():
                raise WorkspaceError("released sound archive must use one directory prefix")
            if member.issym() or member.islnk() or member.isdev() or member.isfifo():
                raise WorkspaceError("released sound archive contains a special member")
            if not member.isdir() and not member.isfile():
                raise WorkspaceError("released sound archive contains an unsupported member")
            relative = PurePosixPath(*path.parts[1:])
            if not relative.parts:
                if root_seen:
                    raise WorkspaceError(
                        "released sound archive has duplicate or case-colliding paths"
                    )
                root_seen = True
                continue
            rendered = relative.as_posix()
            folded_name = rendered.casefold()
            if rendered in seen or folded_name in folded:
                raise WorkspaceError("released sound archive has duplicate or case-colliding paths")
            seen.add(rendered)
            folded.add(folded_name)
            if member.isfile():
                if member.size < 0 or member.size > MAX_RELEASE_MEMBER_BYTES:
                    raise WorkspaceError("released sound archive member exceeds the size limit")
                total += member.size
                if total > MAX_RELEASE_EXTRACTED_BYTES:
                    raise WorkspaceError("released sound archive exceeds the extraction limit")
                files.append((member, relative))
            else:
                directories.add(rendered)
        if member_count == 0:
            raise WorkspaceError("released sound archive member count is invalid")
        if prefix != f"{RELEASE_PRODUCT}-{expected['product_version']}":
            raise WorkspaceError("released sound archive has the wrong product prefix")
        required_directories = {
            PurePosixPath(*relative.parts[:index]).as_posix()
            for _member, relative in files
            for index in range(1, len(relative.parts))
        }
        if directories != required_directories:
            raise WorkspaceError(
                "released sound archive has missing or unexpected directories"
            )
        destination.mkdir(mode=0o700)
        try:
            for member, relative in files:
                target = destination.joinpath(*relative.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                stream = archive.extractfile(member)
                if stream is None:
                    raise WorkspaceError("released sound archive member cannot be read")
                descriptor = os.open(
                    target,
                    os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                    0o600,
                )
                written = 0
                try:
                    with os.fdopen(descriptor, "wb") as output:
                        descriptor = -1
                        while chunk := stream.read(1024 * 1024):
                            written += len(chunk)
                            if written > member.size:
                                raise WorkspaceError(
                                    "released sound archive member exceeds its declared size"
                                )
                            output.write(chunk)
                finally:
                    if descriptor >= 0:
                        os.close(descriptor)
                    stream.close()
                if written != member.size:
                    raise WorkspaceError("released sound archive member is truncated")
        except BaseException:
            import shutil

            shutil.rmtree(destination, ignore_errors=True)
            raise
    return destination


def extract_release_archive(
    archive_path: Path, destination: Path, coordinates: dict[str, Any]
) -> Path:
    """Extract a verified release archive with privacy-safe failure diagnostics."""

    try:
        return _extract_release_archive(archive_path, destination, coordinates)
    except WorkspaceError:
        raise
    except (OSError, EOFError, RecursionError, tarfile.TarError) as error:
        raise WorkspaceError("released sound archive is invalid or corrupt") from error


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


def _tree_files(root: Path, *, contract: str = "local-playtest") -> set[str]:
    result: set[str] = set()
    def walk_error(error: OSError) -> None:
        raise WorkspaceError(f"cannot inspect {contract} tree: {error.filename}") from error

    for directory, directories, files in os.walk(
        root, followlinks=False, onerror=walk_error
    ):
        parent = Path(directory)
        for name in directories:
            path = parent / name
            try:
                mode = path.lstat().st_mode
            except OSError as error:
                raise WorkspaceError(f"cannot inspect {contract} tree: {path}") from error
            if not stat.S_ISDIR(mode):
                raise WorkspaceError(
                    f"{contract} tree contains a non-directory or symlink: {path}"
                )
        for name in files:
            path = parent / name
            relative = path.relative_to(root).as_posix()
            try:
                descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
            except OSError as error:
                raise WorkspaceError(
                    f"{contract} tree entry is not a readable regular file: {path}"
                ) from error
            try:
                if not stat.S_ISREG(os.fstat(descriptor).st_mode):
                    raise WorkspaceError(
                        f"{contract} tree entry is not a regular file: {path}"
                    )
            finally:
                os.close(descriptor)
            result.add(relative)
    return result


def _validate_payload_codec(
    path: str, codec: str, prefix: bytes, *, contract: str = "local-playtest"
) -> None:
    if not prefix.startswith(b"OggS"):
        raise WorkspaceError(f"{contract} payload is not an Ogg stream: {path}")
    signature = b"OpusHead" if codec == "opus" else b"\x01vorbis"
    if signature not in prefix:
        raise WorkspaceError(
            f"{contract} payload codec does not match its manifest: {path}"
        )


def verify_playtest_tree(
    source: Path, root: Path, expected_inputs: dict[str, str]
) -> dict[str, Any]:
    """Independently verify the public sound playtest-tree version 1 contract."""

    if root.is_symlink() or not root.is_dir():
        raise WorkspaceError(f"local-playtest sound root is not a regular directory: {root}")
    manifest_payload = _read_regular(root / PLAYTEST_MANIFEST, "playtest manifest")
    try:
        manifest_value = json.loads(
            manifest_payload,
            parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)),
        )
    except (json.JSONDecodeError, ValueError) as error:
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
    toolchain_payload = _read_regular(
        source / "manifests" / "playtest-audio-toolchain.json",
        "selected sound playtest toolchain",
    )
    if _hash_bytes(toolchain_payload) != expected_inputs.get("toolchain_sha256"):
        raise WorkspaceError("selected sound playtest toolchain is stale or tampered")
    try:
        toolchain = json.loads(toolchain_payload)
    except json.JSONDecodeError as error:
        raise WorkspaceError("selected sound playtest toolchain is invalid JSON") from error
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

    source_manifest_payload = _read_regular(
        source / "manifests" / "source-assets.json",
        "selected sound source manifest",
    )
    if _hash_bytes(source_manifest_payload) != expected_inputs.get(
        "source_manifest_sha256"
    ):
        raise WorkspaceError("selected sound source manifest is stale or tampered")
    try:
        source_manifest = json.loads(source_manifest_payload)
    except json.JSONDecodeError as error:
        raise WorkspaceError("selected sound source manifest is invalid JSON") from error
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


def _safe_release_path(value: object, description: str) -> str:
    if not isinstance(value, str):
        raise WorkspaceError(f"released sound {description} path is invalid")
    path = PurePosixPath(value)
    if (
        path.is_absolute()
        or not path.parts
        or any(part in {"", ".", ".."} for part in path.parts)
        or "\\" in value
    ):
        raise WorkspaceError(f"released sound {description} path is unsafe")
    return path.as_posix()


class _ReleaseSchemaMismatch(WorkspaceError):
    """The instance does not match an otherwise structurally valid schema."""


def _validate_release_schema_instance(
    instance: object,
    schema: object,
    root_schema: dict[str, Any],
    location: str = "$",
    *,
    active_refs: frozenset[str] = frozenset(),
    budget: list[int] | None = None,
    depth: int = 0,
) -> None:
    """Apply the bounded JSON Schema subset used by the sound release contract."""

    if budget is None:
        budget = [100_000]
    budget[0] -= 1
    if budget[0] < 0 or depth > 128:
        raise WorkspaceError("released sound packaged schema exceeds evaluation limits")
    if schema is True:
        return
    if schema is False:
        raise _ReleaseSchemaMismatch(
            f"released sound manifest violates its schema at {location}"
        )
    if not isinstance(schema, dict):
        raise WorkspaceError("released sound packaged schema node is invalid")
    supported = {
        "$defs", "$id", "$ref", "$schema", "additionalProperties", "allOf",
        "anyOf", "const", "default", "description", "enum", "examples", "format",
        "items", "maxItems", "maxLength", "maxProperties", "maximum", "minItems",
        "minLength", "minProperties", "minimum", "oneOf", "pattern", "properties",
        "required", "title", "type", "uniqueItems",
    }
    if set(schema) - supported:
        raise WorkspaceError("released sound packaged schema uses unsupported keywords")
    for keyword in (
        "maxItems", "maxLength", "maxProperties", "minItems", "minLength",
        "minProperties",
    ):
        limit = schema.get(keyword)
        if limit is not None and (
            not isinstance(limit, int) or isinstance(limit, bool) or limit < 0
        ):
            raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
    for keyword in ("maximum", "minimum"):
        limit = schema.get(keyword)
        if limit is not None and (
            not isinstance(limit, (int, float))
            or isinstance(limit, bool)
            or not math.isfinite(limit)
        ):
            raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
    if "enum" in schema and (
        not isinstance(schema["enum"], list) or not schema["enum"]
    ):
        raise WorkspaceError("released sound packaged schema enum is invalid")
    required_value = schema.get("required")
    if required_value is not None and (
        not isinstance(required_value, list)
        or any(not isinstance(name, str) for name in required_value)
    ):
        raise WorkspaceError("released sound packaged schema required is invalid")
    if "properties" in schema and not isinstance(schema["properties"], dict):
        raise WorkspaceError("released sound packaged schema properties is invalid")
    if "pattern" in schema and not isinstance(schema["pattern"], str):
        raise WorkspaceError("released sound packaged schema pattern is invalid")
    if "uniqueItems" in schema and not isinstance(schema["uniqueItems"], bool):
        raise WorkspaceError("released sound packaged schema uniqueItems is invalid")
    reference = schema.get("$ref")
    if reference is not None:
        if not isinstance(reference, str) or not reference.startswith("#/$defs/"):
            raise WorkspaceError("released sound packaged schema reference is invalid")
        name = reference.removeprefix("#/$defs/")
        definitions = root_schema.get("$defs")
        if (
            reference in active_refs
            or not isinstance(definitions, dict)
            or name not in definitions
        ):
            raise WorkspaceError("released sound packaged schema reference is unresolved")
        _validate_release_schema_instance(
            instance,
            definitions[name],
            root_schema,
            location,
            active_refs=active_refs | {reference},
            budget=budget,
            depth=depth + 1,
        )
    for keyword in ("allOf", "anyOf", "oneOf"):
        branches = schema.get(keyword)
        if branches is None:
            continue
        if not isinstance(branches, list) or not branches:
            raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
        matches = 0
        for branch in branches:
            try:
                _validate_release_schema_instance(
                    instance,
                    branch,
                    root_schema,
                    location,
                    active_refs=active_refs,
                    budget=budget,
                    depth=depth + 1,
                )
            except _ReleaseSchemaMismatch:
                continue
            matches += 1
        if (
            keyword == "allOf" and matches != len(branches)
            or keyword == "anyOf" and matches < 1
            or keyword == "oneOf" and matches != 1
        ):
            raise _ReleaseSchemaMismatch(
                f"released sound manifest violates {keyword} at {location}"
            )
    if "const" in schema and instance != schema["const"]:
        raise _ReleaseSchemaMismatch(
            f"released sound manifest violates const at {location}"
        )
    if "enum" in schema:
        enum = schema["enum"]
        if not isinstance(enum, list) or not enum:
            raise WorkspaceError("released sound packaged schema enum is invalid")
        if instance not in enum:
            raise _ReleaseSchemaMismatch(
                f"released sound manifest violates enum at {location}"
            )
    type_name = schema.get("type")
    type_checks = {
        "object": lambda value: isinstance(value, dict),
        "array": lambda value: isinstance(value, list),
        "string": lambda value: isinstance(value, str),
        "integer": lambda value: isinstance(value, int) and not isinstance(value, bool),
        "number": lambda value: isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value),
        "boolean": lambda value: isinstance(value, bool),
        "null": lambda value: value is None,
    }
    if type_name is not None:
        allowed = [type_name] if isinstance(type_name, str) else type_name
        if not isinstance(allowed, list) or not allowed or any(
            name not in type_checks for name in allowed
        ):
            raise WorkspaceError("released sound packaged schema type is invalid")
        if not any(type_checks[name](instance) for name in allowed):
            raise _ReleaseSchemaMismatch(
                f"released sound manifest has wrong type at {location}"
            )
    if isinstance(instance, dict):
        required = schema.get("required", [])
        properties = schema.get("properties", {})
        if (
            not isinstance(required, list)
            or any(not isinstance(name, str) for name in required)
            or not isinstance(properties, dict)
        ):
            raise WorkspaceError("released sound packaged schema object keywords are invalid")
        if any(name not in instance for name in required):
            raise _ReleaseSchemaMismatch(
                f"released sound manifest object is invalid at {location}"
            )
        additional = schema.get("additionalProperties", True)
        if not isinstance(additional, (bool, dict)):
            raise WorkspaceError("released sound packaged schema additionalProperties is invalid")
        for name, value in instance.items():
            child = properties.get(name, additional)
            _validate_release_schema_instance(
                value,
                child,
                root_schema,
                f"{location}.{name}",
                active_refs=active_refs,
                budget=budget,
                depth=depth + 1,
            )
        for keyword, comparison in (("minProperties", lambda a, b: a >= b), ("maxProperties", lambda a, b: a <= b)):
            limit = schema.get(keyword)
            if limit is not None and (not isinstance(limit, int) or isinstance(limit, bool) or limit < 0):
                raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
            if limit is not None and not comparison(len(instance), limit):
                raise _ReleaseSchemaMismatch(f"released sound manifest violates {keyword} at {location}")
    if isinstance(instance, list):
        item_schema = schema.get("items", True)
        for index, value in enumerate(instance):
            _validate_release_schema_instance(
                value,
                item_schema,
                root_schema,
                f"{location}[{index}]",
                active_refs=active_refs,
                budget=budget,
                depth=depth + 1,
            )
        for keyword, comparison in (("minItems", lambda a, b: a >= b), ("maxItems", lambda a, b: a <= b)):
            limit = schema.get(keyword)
            if limit is not None and (not isinstance(limit, int) or isinstance(limit, bool) or limit < 0):
                raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
            if limit is not None and not comparison(len(instance), limit):
                raise _ReleaseSchemaMismatch(f"released sound manifest violates {keyword} at {location}")
        unique_items = schema.get("uniqueItems")
        if unique_items is not None and not isinstance(unique_items, bool):
            raise WorkspaceError("released sound packaged schema uniqueItems is invalid")
        if unique_items is True and len({_canonical_json(value) for value in instance}) != len(instance):
            raise _ReleaseSchemaMismatch(f"released sound manifest items are not unique at {location}")
    if isinstance(instance, str):
        for keyword, comparison in (("minLength", lambda a, b: a >= b), ("maxLength", lambda a, b: a <= b)):
            limit = schema.get(keyword)
            if limit is not None and (not isinstance(limit, int) or isinstance(limit, bool) or limit < 0):
                raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
            if limit is not None and not comparison(len(instance), limit):
                raise _ReleaseSchemaMismatch(f"released sound manifest violates {keyword} at {location}")
        pattern = schema.get("pattern")
        if pattern is not None:
            try:
                matches = isinstance(pattern, str) and re.search(pattern, instance)
            except re.error as error:
                raise WorkspaceError("released sound packaged schema pattern is invalid") from error
            if not matches:
                raise _ReleaseSchemaMismatch(f"released sound manifest violates pattern at {location}")
    if isinstance(instance, (int, float)) and not isinstance(instance, bool):
        for keyword, comparison in (("minimum", lambda a, b: a >= b), ("maximum", lambda a, b: a <= b)):
            limit = schema.get(keyword)
            if limit is not None and (not isinstance(limit, (int, float)) or isinstance(limit, bool) or not math.isfinite(limit)):
                raise WorkspaceError(f"released sound packaged schema {keyword} is invalid")
            if limit is not None and not comparison(instance, limit):
                raise _ReleaseSchemaMismatch(f"released sound manifest violates {keyword} at {location}")


def _release_checksums(root: Path) -> dict[str, str]:
    payload = _read_regular(
        root / RELEASE_CHECKSUMS,
        "released sound checksums",
        maximum_bytes=MAX_RELEASE_CHECKSUM_BYTES,
    )
    try:
        text = payload.decode("ascii")
    except UnicodeDecodeError as error:
        raise WorkspaceError("released sound checksums are not ASCII") from error
    checksums: dict[str, str] = {}
    folded: set[str] = set()
    lines = text.splitlines(keepends=True)
    if not lines or any(not line.endswith("\n") for line in lines):
        raise WorkspaceError("released sound checksums are not canonical")
    for line in lines:
        fields = line[:-1].split("  ", 1)
        if len(fields) != 2 or not SHA256_PATTERN.fullmatch(fields[0]):
            raise WorkspaceError("released sound checksums contain an invalid entry")
        path = _safe_release_path(fields[1], "checksum")
        folded_path = path.casefold()
        if path == RELEASE_CHECKSUMS or path in checksums or folded_path in folded:
            raise WorkspaceError("released sound checksums contain a duplicate entry")
        checksums[path] = fields[0]
        folded.add(folded_path)
    canonical = "".join(
        f"{checksums[path]}  {path}\n" for path in sorted(checksums)
    ).encode("ascii")
    if payload != canonical:
        raise WorkspaceError("released sound checksums are not canonical")
    return checksums


def verify_release_tree(root: Path, coordinates: dict[str, Any]) -> dict[str, Any]:
    """Independently verify the publishable Classic compatibility product."""

    expected = validate_release_coordinates(coordinates)
    if root.is_symlink() or not root.is_dir():
        raise WorkspaceError(f"released sound root is not a regular directory: {root}")
    manifest_payload = _read_regular(
        root / RELEASE_MANIFEST,
        "released sound manifest",
        maximum_bytes=MAX_RELEASE_MANIFEST_BYTES,
    )
    if _hash_bytes(manifest_payload) != expected["release_manifest_sha256"]:
        raise WorkspaceError("released sound manifest hash does not match the profile")
    try:
        manifest_value = json.loads(
            manifest_payload,
            parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)),
        )
    except (json.JSONDecodeError, ValueError) as error:
        raise WorkspaceError("released sound manifest is invalid JSON") from error
    if manifest_payload != _canonical_json(manifest_value):
        raise WorkspaceError("released sound manifest is not canonical JSON")
    manifest_keys = {
        "$schema",
        "assets",
        "converted_opus_count",
        "copied_vorbis_count",
        "logical_path_count",
        "marker_sha256",
        "notices",
        "output_tree_sha256",
        "playtest_only",
        "product",
        "product_version",
        "publishable",
        "release_tag",
        "repository",
        "schema_sha256",
        "schema_version",
        "source_commit",
        "source_manifest_sha256",
        "source_tree",
        "toolchain_sha256",
    }
    if not isinstance(manifest_value, dict) or set(manifest_value) != manifest_keys:
        raise WorkspaceError("released sound manifest fields are invalid")
    manifest = manifest_value
    identity_fields = {
        "product": "product",
        "product_version": "product_version",
        "release_tag": "tag",
        "repository": "repository",
        "schema_version": "manifest_schema_version",
        "source_commit": "source_commit",
        "source_manifest_sha256": "source_manifest_sha256",
        "source_tree": "source_tree",
        "toolchain_sha256": "toolchain_sha256",
        "schema_sha256": "schema_sha256",
        "output_tree_sha256": "output_tree_sha256",
    }
    if (
        manifest.get("$schema") != RELEASE_SCHEMA
        or manifest.get("publishable") is not True
        or manifest.get("playtest_only") is not False
        or any(
            manifest.get(manifest_name) != expected[coordinate_name]
            for manifest_name, coordinate_name in identity_fields.items()
        )
    ):
        raise WorkspaceError("released sound manifest identity does not match the profile")
    marker_payload = _read_regular(
        root / RELEASE_MARKER,
        "released sound marker",
        maximum_bytes=MAX_RELEASE_MARKER_BYTES,
    )
    expected_marker = _canonical_json(
        {
            "format": RELEASE_PRODUCT,
            "playtest_only": False,
            "product_version": expected["product_version"],
            "publishable": True,
            "schema_version": 1,
        }
    )
    if (
        marker_payload != expected_marker
        or manifest.get("marker_sha256") != _hash_bytes(marker_payload)
    ):
        raise WorkspaceError("released sound ownership marker is missing or tampered")
    schema_hash, _schema_size, _schema_prefix = _hash_regular(
        root / RELEASE_SCHEMA, "released sound packaged schema"
    )
    if _schema_size > MAX_RELEASE_SCHEMA_BYTES:
        raise WorkspaceError("released sound packaged schema exceeds its size limit")
    if schema_hash != expected["schema_sha256"]:
        raise WorkspaceError("released sound packaged schema is missing or tampered")
    schema_payload = _read_regular(
        root / RELEASE_SCHEMA,
        "released sound packaged schema",
        maximum_bytes=MAX_RELEASE_SCHEMA_BYTES,
    )
    try:
        schema = json.loads(
            schema_payload,
            parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)),
        )
    except (json.JSONDecodeError, ValueError) as error:
        raise WorkspaceError("released sound packaged schema is invalid JSON") from error
    if not isinstance(schema, dict):
        raise WorkspaceError("released sound packaged schema contract is invalid")
    required = schema.get("required")
    properties = schema.get("properties")
    if (
        schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema"
        or schema.get("$id") != f"https://atrinik.org/{RELEASE_SCHEMA}"
        or schema.get("type") != "object"
        or schema.get("additionalProperties") is not False
        or not isinstance(required, list)
        or any(not isinstance(name, str) for name in required)
        or set(required) != manifest_keys
        or not isinstance(properties, dict)
        or set(properties) != manifest_keys
    ):
        raise WorkspaceError("released sound packaged schema contract is invalid")
    _validate_release_schema_instance(manifest, schema, schema)

    notices = manifest.get("notices")
    if not isinstance(notices, list) or not notices:
        raise WorkspaceError("released sound notices must be a nonempty array")
    notice_paths: set[str] = set()
    notice_folded: set[str] = set()
    for raw_notice in notices:
        if not isinstance(raw_notice, dict) or set(raw_notice) != {"path", "sha256"}:
            raise WorkspaceError("released sound notice fields are invalid")
        path = _safe_release_path(raw_notice.get("path"), "notice")
        digest = raw_notice.get("sha256")
        if (
            not isinstance(digest, str)
            or not SHA256_PATTERN.fullmatch(digest)
            or path in notice_paths
            or path.casefold() in notice_folded
            or path.casefold().endswith("-blocked.json")
        ):
            raise WorkspaceError("released sound notice identity is invalid")
        actual_hash, _size, _prefix = _hash_regular(
            root / path, f"released sound notice {path}"
        )
        if actual_hash != digest:
            raise WorkspaceError(f"released sound notice hash mismatch: {path}")
        notice_paths.add(path)
        notice_folded.add(path.casefold())
    if [notice["path"] for notice in notices] != sorted(notice_paths):
        raise WorkspaceError("released sound notices are not canonically ordered")

    assets = manifest.get("assets")
    if not isinstance(assets, list):
        raise WorkspaceError("released sound manifest assets must be an array")
    logical_paths: set[str] = set()
    logical_folded: set[str] = set()
    copied = 0
    converted = 0
    source_midi = 0
    source_flac = 0
    tree_entries: list[tuple[str, str]] = []
    for raw_asset in assets:
        if not isinstance(raw_asset, dict) or set(raw_asset) != {
            "logical_path", "mapping", "output", "source", "source_path"
        }:
            raise WorkspaceError("released sound asset fields are invalid")
        logical_path = raw_asset.get("logical_path")
        if (
            not isinstance(logical_path, str)
            or not LOGICAL_PATH_PATTERN.fullmatch(logical_path)
            or logical_path in logical_paths
            or logical_path.casefold() in logical_folded
        ):
            raise WorkspaceError("released sound manifest has an unsafe or duplicate path")
        source_path = _safe_release_path(raw_asset.get("source_path"), "source")
        source = raw_asset.get("source")
        output = raw_asset.get("output")
        if (
            not isinstance(source, dict)
            or set(source) != {"codec", "container", "sha256"}
            or not isinstance(output, dict)
            or set(output) != {
                "channels", "codec", "container", "duration_seconds",
                "sample_rate", "sha256", "size_bytes",
            }
        ):
            raise WorkspaceError(f"released sound mapping is invalid: {logical_path}")
        source_codec = source.get("codec")
        mapping = raw_asset.get("mapping")
        output_codec = output.get("codec")
        if source_codec == "vorbis":
            expected_mapping = "copy"
            expected_codec = "vorbis"
            expected_source_container = "ogg"
        elif source_codec in {"flac", "midi"}:
            expected_mapping = "render-opus"
            expected_codec = "opus"
            expected_source_container = source_codec
        else:
            raise WorkspaceError(f"released sound source codec is invalid: {logical_path}")
        if (
            mapping != expected_mapping
            or output_codec != expected_codec
            or output.get("container") != "ogg"
            or source.get("container") != expected_source_container
            or not isinstance(source.get("sha256"), str)
            or not SHA256_PATTERN.fullmatch(source["sha256"])
            or not isinstance(output.get("sha256"), str)
            or not SHA256_PATTERN.fullmatch(output["sha256"])
            or not isinstance(output.get("size_bytes"), int)
            or isinstance(output.get("size_bytes"), bool)
            or output["size_bytes"] < 1
            or not isinstance(output.get("sample_rate"), int)
            or isinstance(output.get("sample_rate"), bool)
            or output["sample_rate"] < 1
            or output.get("channels") not in {1, 2}
            or not isinstance(output.get("duration_seconds"), (int, float))
            or isinstance(output.get("duration_seconds"), bool)
            or not math.isfinite(output["duration_seconds"])
            or output["duration_seconds"] <= 0
            or expected_mapping == "copy" and output["sha256"] != source["sha256"]
        ):
            raise WorkspaceError(f"released sound mapping is invalid: {logical_path}")
        payload_hash, payload_size, payload_prefix = _hash_regular(
            root / logical_path, f"released sound payload {logical_path}"
        )
        if payload_hash != output["sha256"] or payload_size != output["size_bytes"]:
            raise WorkspaceError(
                f"released sound payload hash or size mismatch: {logical_path}"
            )
        _validate_payload_codec(
            logical_path, expected_codec, payload_prefix, contract="released sound"
        )
        logical_paths.add(logical_path)
        logical_folded.add(logical_path.casefold())
        copied += expected_mapping == "copy"
        converted += expected_mapping == "render-opus"
        source_midi += source_codec == "midi"
        source_flac += source_codec == "flac"
        tree_entries.append((logical_path, payload_hash))
        if not source_path:
            raise WorkspaceError(f"released sound source path is invalid: {logical_path}")
    if [asset["logical_path"] for asset in assets] != sorted(logical_paths):
        raise WorkspaceError("released sound assets are not canonically ordered")
    if (
        len(assets) != EXPECTED_PATHS
        or manifest.get("logical_path_count") != EXPECTED_PATHS
        or copied != EXPECTED_COPIED_VORBIS
        or manifest.get("copied_vorbis_count") != EXPECTED_COPIED_VORBIS
        or converted != EXPECTED_CONVERTED_OPUS
        or manifest.get("converted_opus_count") != EXPECTED_CONVERTED_OPUS
        or source_midi != EXPECTED_SOURCE_MIDI
        or source_flac != EXPECTED_SOURCE_FLAC
    ):
        raise WorkspaceError("released sound tree counts do not match the 339-path contract")
    tree_digest = hashlib.sha256()
    for logical_path, payload_hash in sorted(tree_entries):
        tree_digest.update(f"{payload_hash}  {logical_path}\n".encode("ascii"))
    if tree_digest.hexdigest() != expected["output_tree_sha256"]:
        raise WorkspaceError("released sound output-tree digest mismatch")

    checksums = _release_checksums(root)
    files = _tree_files(root, contract="released sound")
    expected_files = logical_paths | notice_paths | {
        RELEASE_CHECKSUMS,
        RELEASE_MANIFEST,
        RELEASE_MARKER,
        RELEASE_SCHEMA,
    }
    if files != expected_files or set(checksums) != files - {RELEASE_CHECKSUMS}:
        raise WorkspaceError("released sound archive has missing or unexpected files")
    for path, expected_hash in checksums.items():
        actual_hash, _size, _prefix = _hash_regular(
            root / path, f"released sound checksum member {path}"
        )
        if actual_hash != expected_hash:
            raise WorkspaceError(f"released sound internal checksum mismatch: {path}")
    return {
        "mode": RELEASED_MODE,
        "root": str(root.resolve()),
        "repository": expected["repository"],
        "tag": expected["tag"],
        "product": expected["product"],
        "product_version": expected["product_version"],
        "asset_url": expected["asset_url"],
        "archive_sha256": expected["archive_sha256"],
        "release_manifest_sha256": expected["release_manifest_sha256"],
        "manifest_schema_version": expected["manifest_schema_version"],
        "source_commit": expected["source_commit"],
        "source_tree": expected["source_tree"],
        "source_manifest_sha256": expected["source_manifest_sha256"],
        "toolchain_sha256": expected["toolchain_sha256"],
        "schema_sha256": expected["schema_sha256"],
        "marker_sha256": manifest["marker_sha256"],
        "output_tree_sha256": expected["output_tree_sha256"],
        "logical_path_count": EXPECTED_PATHS,
        "copied_vorbis_count": EXPECTED_COPIED_VORBIS,
        "converted_opus_count": EXPECTED_CONVERTED_OPUS,
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
RELEASE_RECORD_KEYS = {
    "mode", "root", "repository", "tag", "product", "product_version",
    "asset_url", "archive_sha256", "release_manifest_sha256",
    "manifest_schema_version", "source_commit", "source_tree",
    "source_manifest_sha256", "toolchain_sha256", "schema_sha256",
    "marker_sha256", "output_tree_sha256", "logical_path_count",
    "copied_vorbis_count", "converted_opus_count",
}


def validate_sound_record(value: object) -> dict[str, Any]:
    """Validate the exact persisted provenance record for either sound mode."""

    if not isinstance(value, dict):
        raise WorkspaceError("sound provenance record is not an object")
    mode = value.get("mode")
    expected = {
        SOURCE_MODE: SOURCE_RECORD_KEYS,
        PLAYTEST_MODE: PLAYTEST_RECORD_KEYS,
        RELEASED_MODE: RELEASE_RECORD_KEYS,
    }.get(mode)
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
        or mode != RELEASED_MODE and not isinstance(value.get("source_clean"), bool)
    ):
        raise WorkspaceError("sound provenance source identity is invalid")
    if mode == SOURCE_MODE:
        return value
    if mode == RELEASED_MODE:
        coordinates = {
            key: value[key] for key in RELEASE_COORDINATE_KEYS
        }
        validate_release_coordinates(coordinates)
        hashes = RELEASE_RECORD_KEYS - {
            "asset_url", "copied_vorbis_count", "converted_opus_count",
            "logical_path_count", "manifest_schema_version", "mode", "product",
            "product_version", "repository", "root", "source_commit",
            "source_tree", "tag",
        }
        if (
            any(
                not isinstance(value.get(name), str)
                or not SHA256_PATTERN.fullmatch(value[name])
                for name in hashes
            )
            or value.get("logical_path_count") != EXPECTED_PATHS
            or value.get("copied_vorbis_count") != EXPECTED_COPIED_VORBIS
            or value.get("converted_opus_count") != EXPECTED_CONVERTED_OPUS
        ):
            raise WorkspaceError("released sound provenance is invalid")
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
