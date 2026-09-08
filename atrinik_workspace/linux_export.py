"""Verify the materialized byte closure of a portable Linux client directory.

The producer must supply already-proven source coordinates and dependency/legal
closure. This verifier detects corruption and nonportable filesystem entries; it
does not turn caller-provided hashes into provenance or gameplay evidence.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
from typing import BinaryIO

MAX_MANIFEST_BYTES = 16 * 1024 * 1024
MAX_FILES = 100_000
LFS_HEADER = b"version https://git-lfs.github.com/spec/v1\n"
MANIFEST_NAME = "atrinik-export.json"


class ExportError(ValueError):
    """A missing, corrupt, escaping or unverified portable payload."""


def relative_path(value: object) -> str:
    if not isinstance(value, str) or not value or "\\" in value or any(ord(c) < 32 for c in value):
        raise ExportError("export-path: invalid relative filename")
    path = PurePosixPath(value)
    if path.is_absolute() or str(path) != value or any(p in (".", "..") for p in path.parts):
        raise ExportError("export-path: escaping or noncanonical filename")
    return value


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ExportError("export-manifest: duplicate JSON member")
        result[key] = value
    return result


def load_manifest(data: bytes) -> dict[str, object]:
    if len(data) > MAX_MANIFEST_BYTES:
        raise ExportError("export-manifest: size limit exceeded")
    try:
        value = json.loads(data, object_pairs_hook=_unique_object)
    except (ValueError, UnicodeError) as error:
        raise ExportError("export-manifest: malformed JSON") from error
    if not isinstance(value, dict) or set(value) != {"schema_version", "files", "sources", "application_libraries", "notices"}:
        raise ExportError("export-manifest: unexpected schema")
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        raise ExportError("export-manifest: unsupported version")
    files = value["files"]
    if not isinstance(files, dict) or not files or len(files) > MAX_FILES:
        raise ExportError("export-manifest: invalid file inventory")
    for name, record in files.items():
        relative_path(name)
        if name == MANIFEST_NAME:
            raise ExportError("export-manifest: manifest cannot inventory itself")
        if not isinstance(record, dict) or set(record) != {"sha256", "size", "executable"}:
            raise ExportError("export-manifest: invalid file record")
        if not isinstance(record["sha256"], str) or not re.fullmatch("[0-9a-f]{64}", record["sha256"]):
            raise ExportError("export-manifest: invalid digest")
        if type(record["size"]) is not int or record["size"] < 0 or type(record["executable"]) is not bool:
            raise ExportError("export-manifest: invalid size or mode")
    for field in ("application_libraries", "notices"):
        names = value[field]
        if not isinstance(names, list) or not names or not all(isinstance(name, str) for name in names) or len(names) != len(set(names)):
            raise ExportError(f"export-manifest: incomplete {field}")
        for name in names:
            if relative_path(name) not in files:
                raise ExportError(f"export-manifest: uninventoried {field}")
    sources = value["sources"]
    if not isinstance(sources, dict) or not sources:
        raise ExportError("export-manifest: missing source coordinates")
    for repository, sha in sources.items():
        if not isinstance(repository, str) or not re.fullmatch(r"atrinik/[a-z0-9-]+@main", repository):
            raise ExportError("export-manifest: invalid source repository")
        if not isinstance(sha, str) or not re.fullmatch("[0-9a-f]{40}", sha):
            raise ExportError("export-manifest: invalid source commit")
    return value


def _open_beneath(root_fd: int, name: str) -> int:
    parts = relative_path(name).split("/")
    parent = os.dup(root_fd)
    try:
        for part in parts[:-1]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent)
            os.close(parent)
            parent = child
        return os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=parent)
    finally:
        os.close(parent)


def _hash_file(stream: BinaryIO, size: int) -> str:
    prefix = stream.read(min(1024, size + 1))
    if prefix.startswith(LFS_HEADER):
        raise ExportError("export-lfs-pointer: materialized media bytes are required")
    digest = hashlib.sha256(prefix)
    count = len(prefix)
    while block := stream.read(min(1024 * 1024, max(1, size - count + 1))):
        count += len(block)
        if count > size:
            raise ExportError("export-payload: size mismatch")
        digest.update(block)
    if count != size:
        raise ExportError("export-payload: size mismatch")
    return digest.hexdigest()


def _identity(info: os.stat_result) -> tuple[int, ...]:
    return (info.st_dev, info.st_ino, info.st_mode, info.st_size,
            info.st_mtime_ns, info.st_ctime_ns)


def _inventory(root_fd: int) -> tuple[set[str], dict[str, tuple[int, ...]]]:
    files_seen: set[str] = set()
    identities = {".": _identity(os.fstat(root_fd))}

    def visit(directory_fd: int, prefix: str, depth: int) -> None:
        if depth > 128:
            raise ExportError("export-payload: directory depth limit exceeded")
        # Iterate instead of fwalk's eager directory lists; fail on unreadable
        # directories and bound empty directories as well as payload files.
        with os.scandir(directory_fd) as entries:
            for entry in entries:
                if len(identities) >= 2 * MAX_FILES + 1:
                    raise ExportError("export-payload: inventory limit exceeded")
                name = entry.name if not prefix else prefix + "/" + entry.name
                info = os.stat(entry.name, dir_fd=directory_fd, follow_symlinks=False)
                identities[name] = _identity(info)
                if stat.S_ISDIR(info.st_mode):
                    child = os.open(entry.name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                                    dir_fd=directory_fd)
                    try:
                        if _identity(os.fstat(child)) != identities[name]:
                            raise ExportError("export-payload: directory changed during inventory")
                        visit(child, name, depth + 1)
                    finally:
                        os.close(child)
                elif stat.S_ISREG(info.st_mode):
                    files_seen.add(name)
                    if len(files_seen) > MAX_FILES + 1:
                        raise ExportError("export-payload: inventory limit exceeded")
                else:
                    raise ExportError("export-payload: symlink or special file")

    visit(root_fd, "", 0)
    return files_seen, identities


def _open_root(root: Path) -> int:
    if not root.is_absolute() or ".." in root.parts or root == Path(root.anchor):
        raise ExportError("export-root: absolute canonical non-root directory required")
    descriptor = os.open(root.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in root.parts[1:]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def verify_export(root: Path) -> dict[str, object]:
    """Verify an exact no-symlink inventory using descriptor-relative opens."""
    root_fd = -1
    try:
        root_fd = _open_root(root)
        observed, identities = _inventory(root_fd)
        with os.fdopen(_open_beneath(root_fd, MANIFEST_NAME), "rb") as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or _identity(info) != identities.get(MANIFEST_NAME):
                raise ExportError("export-manifest: changed or nonregular file")
            manifest = load_manifest(stream.read(MAX_MANIFEST_BYTES + 1))
        expected = set(manifest["files"]) | {MANIFEST_NAME}
        if observed != expected:
            raise ExportError("export-payload: missing or unexpected file")
        for name, record in manifest["files"].items():
            with os.fdopen(_open_beneath(root_fd, name), "rb") as stream:
                before = os.fstat(stream.fileno())
                if not stat.S_ISREG(before.st_mode) or _identity(before) != identities.get(name):
                    raise ExportError("export-payload: changed or nonregular file")
                if bool(before.st_mode & 0o111) != record["executable"]:
                    raise ExportError("export-payload: executable mode mismatch")
                if before.st_mode & (stat.S_ISUID | stat.S_ISGID):
                    raise ExportError("export-payload: set-id files forbidden")
                if _hash_file(stream, record["size"]) != record["sha256"]:
                    raise ExportError("export-payload: digest mismatch")
                after = os.fstat(stream.fileno())
                if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
                    after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
                    raise ExportError("export-payload: changed during verification")
        if _inventory(root_fd) != (observed, identities):
            raise ExportError("export-payload: inventory changed during verification")
        reopened = _open_root(root)
        try:
            if _identity(os.fstat(reopened)) != identities["."]:
                raise ExportError("export-root: pathname changed during verification")
        finally:
            os.close(reopened)
        return manifest
    except OSError as error:
        raise ExportError("export-payload: missing or unsafe filesystem entry") from error
    finally:
        if root_fd >= 0:
            os.close(root_fd)
