"""Verify the materialized byte closure of a portable Linux client directory.

The producer must supply already-proven source coordinates and dependency/legal
closure. This verifier detects corruption and nonportable filesystem entries; it
does not turn caller-provided hashes into provenance or gameplay evidence.
"""
from __future__ import annotations

import hashlib
import json
import os
import posixpath
from pathlib import Path, PurePosixPath
import re
import stat
import selectors
import shlex
import subprocess
import time
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


MAX_ELF_REPORT_BYTES = 4 * 1024 * 1024


def _readelf(descriptor: int, *, timeout: float = 15) -> str:
    """Inspect an inherited descriptor without executing the payload."""
    process = subprocess.Popen(
        ["readelf", "--wide", "--file-header", "--program-headers", "--dynamic",
         "--version-info", f"/proc/self/fd/{descriptor}"],
        pass_fds=(descriptor,), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        env={"PATH": os.defpath, "LC_ALL": "C"})
    output = bytearray()
    deadline = time.monotonic() + timeout
    try:
        assert process.stdout is not None
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise ExportError("export-elf: inspection timeout")
                if not selector.select(remaining):
                    raise ExportError("export-elf: inspection timeout")
                block = os.read(process.stdout.fileno(), min(65536, MAX_ELF_REPORT_BYTES - len(output) + 1))
                if not block:
                    break
                output.extend(block)
                if len(output) > MAX_ELF_REPORT_BYTES:
                    raise ExportError("export-elf: report size limit exceeded")
        try:
            result = process.wait(timeout=max(.001, deadline - time.monotonic()))
        except subprocess.TimeoutExpired as error:
            raise ExportError("export-elf: inspection timeout") from error
        if result:
            raise ExportError("export-elf: malformed or unsupported ELF")
        try:
            return output.decode("utf-8", errors="strict")
        except UnicodeError as error:
            raise ExportError("export-elf: invalid inspection output") from error
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        if process.stdout is not None:
            process.stdout.close()


def inspect_elf(descriptor: int) -> dict[str, object]:
    """Return structural ABI facts, never provenance or runtime qualification.

    The owner keeps its source lease and derives this descriptor from the verified
    build/materialization operation. No caller hash map becomes source authority.
    """
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or before.st_size > 512 * 1024 * 1024:
            raise ExportError("export-elf: bounded regular payload required")
        if os.pread(descriptor, 4, 0) != b"\x7fELF":
            raise ExportError("export-elf: ELF magic required")
        text = _readelf(descriptor)
        if _identity(os.fstat(descriptor)) != _identity(before):
            raise ExportError("export-elf: payload changed during inspection")
    except OSError as error:
        raise ExportError("export-elf: inspection unavailable") from error

    def header(name: str) -> str:
        values = re.findall(r"^\s*" + re.escape(name) + r":\s*(.+)$", text, re.MULTILINE)
        if len(values) != 1:
            raise ExportError("export-elf: missing or ambiguous header")
        return values[0].strip()

    elf_class, endian, machine = header("Class"), header("Data"), header("Machine")
    if elf_class not in ("ELF32", "ELF64") or endian not in (
            "2's complement, little endian", "2's complement, big endian"):
        raise ExportError("export-elf: unsupported ELF encoding")
    kind = header("Type").split()[0]
    if kind not in ("EXEC", "DYN"):
        raise ExportError("export-elf: executable or shared object required")
    needed: list[str] = []
    soname: str | None = None
    search_paths: list[str] = []
    for line in text.splitlines():
        if not re.search(r"\((?:NEEDED|SONAME|RPATH|RUNPATH)\)", line):
            continue
        match = re.fullmatch(r"\s*0x[0-9a-f]+\s+\((NEEDED|SONAME|RPATH|RUNPATH)\)\s+"
                             r"(?:Shared library|Library soname|Library rpath|Library runpath): \[([^\[\]\r\n]*)\]\s*", line)
        if match is None:
            raise ExportError("export-elf: malformed dynamic dependency record")
        tag, value = match.groups()
        if tag in ("NEEDED", "SONAME"):
            if not re.fullmatch(r"[A-Za-z0-9_+.-]+", value) or value in (".", ".."):
                raise ExportError("export-elf: dependency must be a library basename")
            if tag == "NEEDED":
                if value in needed:
                    raise ExportError("export-elf: duplicate dependency")
                needed.append(value)
            elif soname is not None:
                raise ExportError("export-elf: duplicate SONAME")
            else:
                soname = value
        else:
            if search_paths:
                raise ExportError("export-elf: ambiguous runtime search path")
            search_paths = value.split(":")
            if any(not item or not re.fullmatch(r"\$ORIGIN(?:/[A-Za-z0-9_.+-]+)*", item)
                   for item in search_paths):
                raise ExportError("export-elf: runtime search path must use explicit ORIGIN paths")
    interpreter_lines = [line for line in text.splitlines() if "Requesting program interpreter:" in line]
    interpreter_count = len(re.findall(r"^\s*INTERP\s", text, re.MULTILINE))
    if len(interpreter_lines) != interpreter_count or interpreter_count > 1:
        raise ExportError("export-elf: missing or ambiguous interpreter")
    interpreter = None
    if interpreter_lines:
        match = re.fullmatch(r"\s*\[Requesting program interpreter: (/[^\[\]\s]+)\]\s*", interpreter_lines[0])
        if match is None or ".." in Path(match[1]).parts:
            raise ExportError("export-elf: invalid interpreter")
        interpreter = match[1]
    versions: dict[str, list[str]] = {}
    counts: dict[str, int] = {}
    in_needs = False
    declared_count: int | None = None
    provider: str | None = None
    for line in text.splitlines():
        if line.startswith("Version "):
            in_needs = line.startswith("Version needs section ")
            provider = None
            if in_needs:
                match = re.fullmatch(r"Version needs section '[^']+' contains ([0-9]+) entr(?:y|ies):", line)
                if match is None or declared_count is not None:
                    raise ExportError("export-elf: malformed version-needs section")
                declared_count = int(match[1])
            continue
        if not in_needs or not line.strip():
            continue
        if re.fullmatch(r"\s*Addr: 0x[0-9a-f]+\s+Offset: 0x[0-9a-f]+\s+Link: [0-9]+ \([^()]+\)", line):
            continue
        match = re.fullmatch(r"\s*[0-9a-fx]+: Version: [0-9]+\s+File: ([A-Za-z0-9_.+-]+)\s+Cnt: ([0-9]+)", line)
        if match:
            provider = match[1]
            if provider not in needed or provider in versions:
                raise ExportError("export-elf: version provider not uniquely required")
            versions[provider] = []
            counts[provider] = int(match[2])
            continue
        match = re.fullmatch(r"\s*[0-9a-fx]+:\s+Name: ([A-Za-z0-9_.+-]+)\s+Flags: [A-Za-z0-9_ |+-]+\s+Version: [0-9]+", line)
        if match is None or provider is None or match[1] in versions[provider]:
            raise ExportError("export-elf: malformed version requirement")
        versions[provider].append(match[1])
    if (declared_count is not None and declared_count != len(versions)) or any(
            counts[name] != len(values) for name, values in versions.items()):
        raise ExportError("export-elf: incomplete version requirements")
    dynamic_version_count = re.findall(r"\(VERNEEDNUM\)\s+([0-9]+)\s*$", text, re.MULTILINE)
    if len(dynamic_version_count) > 1 or (dynamic_version_count and int(dynamic_version_count[0]) != declared_count):
        raise ExportError("export-elf: missing version-needs section")
    return {"class": elf_class, "endianness": endian, "machine": machine,
            "type": kind, "interpreter": interpreter, "needed": needed,
            "soname": soname, "search_paths": search_paths, "required_versions": versions}


def elf_dependency_report(objects: dict[str, dict[str, object]], *, entrypoint: str,
                          host_libraries: frozenset[str],
                          library_directories: tuple[str, ...] = ("lib",)) -> dict[str, object]:
    """Resolve inspected static dependencies against explicit bundled/host sets.

    This report does not cover dlopen plugins, license closure or source identity.
    The producer must bind facts to actual copied bytes and qualify the loader.
    """
    if entrypoint not in objects or not objects or len(objects) > MAX_FILES:
        raise ExportError("export-elf: missing entrypoint or excessive object inventory")
    for directory in library_directories:
        relative_path(directory)
    providers: dict[str, str] = {}
    signature = tuple(objects[entrypoint][key] for key in ("class", "endianness", "machine"))
    for path, facts in objects.items():
        relative_path(path)
        if tuple(facts[key] for key in ("class", "endianness", "machine")) != signature:
            raise ExportError("export-elf: incompatible object ABI")
        if facts["soname"] not in (None, PurePosixPath(path).name):
            raise ExportError("export-elf: SONAME filename alias must be materialized")
        for alias in {PurePosixPath(path).name}:
            if alias in providers or alias in host_libraries:
                raise ExportError("export-elf: ambiguous library provider")
            providers[alias] = path
        for search in facts["search_paths"]:
            depth = len(PurePosixPath(path).parent.parts)
            for part in search.split("/")[1:]:
                depth += -1 if part == ".." else 0 if part == "." else 1
                if depth < 0:
                    raise ExportError("export-elf: runtime search path escapes export")
    edges: dict[str, dict[str, str]] = {}
    for path, facts in objects.items():
        edges[path] = {}
        for needed in facts["needed"]:
            if needed in providers:
                directories = set(library_directories)
                directories.update(posixpath.normpath(str(PurePosixPath(path).parent) +
                                                      search[len("$ORIGIN"):])
                                   for search in facts["search_paths"])
                if str(PurePosixPath(providers[needed]).parent) not in directories:
                    raise ExportError("export-elf: dependency outside explicit loader search path")
                edges[path][needed] = providers[needed]
            elif needed in host_libraries:
                edges[path][needed] = "host:" + needed
            else:
                raise ExportError("export-elf: unresolved dependency " + needed)
    return {"schema_version": 1, "entrypoint": entrypoint,
            "abi": dict(zip(("class", "endianness", "machine"), signature)),
            "interpreter": objects[entrypoint]["interpreter"], "objects": objects,
            "dependencies": edges, "host_libraries": sorted(host_libraries),
            "loader_library_directories": list(library_directories),
            "symbol_versions_verified": False,
            "dynamic_plugins_verified": False, "runtime_qualified": False}



def copy_payload(source_fd: int, destination_fd: int, *, sha256: str, size: int,
                 executable: bool = False) -> dict[str, object]:
    """Copy proven bytes to an exclusively created staging descriptor.

    The source owner supplies expected bytes while holding its authenticated
    source/build lease, and revalidates the source inventory before publication.
    This primitive grants no provenance and never publishes. On failure, retain
    or discard only the caller-owned staging file through its normal workflow.
    """
    if (not isinstance(sha256, str) or not re.fullmatch(r"[0-9a-f]{64}", sha256) or type(size) is not int or
            not 0 <= size <= 16 * 1024 ** 3 or type(executable) is not bool):
        raise ExportError("export-copy: invalid expected byte record")
    try:
        source = os.fstat(source_fd)
        destination = os.fstat(destination_fd)
        if not stat.S_ISREG(source.st_mode) or source.st_size != size:
            raise ExportError("export-copy: source size or type mismatch")
        if (not stat.S_ISREG(destination.st_mode) or destination.st_size or
                destination.st_nlink != 1 or destination.st_uid != os.geteuid() or
                (source.st_dev, source.st_ino) == (destination.st_dev, destination.st_ino)):
            raise ExportError("export-copy: exclusive empty owned staging file required")
        if os.pread(source_fd, min(1024, size), 0).startswith(LFS_HEADER):
            raise ExportError("export-lfs-pointer: materialized media bytes are required")
        os.lseek(destination_fd, 0, os.SEEK_SET)
        digest = hashlib.sha256()
        offset = 0
        while offset < size:
            block = os.pread(source_fd, min(1024 * 1024, size - offset), offset)
            if not block:
                raise ExportError("export-copy: source truncated")
            digest.update(block)
            pending = memoryview(block)
            while pending:
                written = os.write(destination_fd, pending)
                if written <= 0:
                    raise ExportError("export-copy: incomplete write")
                pending = pending[written:]
            offset += len(block)
        if digest.hexdigest() != sha256 or _identity(os.fstat(source_fd)) != _identity(source):
            raise ExportError("export-copy: source bytes changed or digest mismatch")
        os.fchmod(destination_fd, 0o755 if executable else 0o644)
        os.fsync(destination_fd)
        completed = os.fstat(destination_fd)
        copied_digest = hashlib.sha256()
        offset = 0
        while block := os.pread(destination_fd, min(1024 * 1024, size - offset + 1), offset):
            offset += len(block)
            if offset > size:
                raise ExportError("export-copy: staging size changed")
            copied_digest.update(block)
        if (offset != size or copied_digest.hexdigest() != sha256 or
                _identity(os.fstat(destination_fd)) != _identity(completed)):
            raise ExportError("export-copy: staging bytes changed")
        return {"sha256": copied_digest.hexdigest(), "size": offset, "executable": executable}
    except OSError as error:
        raise ExportError("export-copy: unsafe or unavailable descriptor") from error


def launcher_script(*, executable: str = "bin/atrinik",
                    data_directory: str = "share/games/atrinik") -> bytes:
    """Build a movable launcher; the producer verifies binary/media separately."""
    binary = shlex.quote(relative_path(executable))
    data = shlex.quote(relative_path(data_directory))
    return ("""#!/bin/sh
set -eu
umask 077
root=$(CDPATH='' cd -P -- "$(dirname -- "$0")" && pwd)
config=${ATRINIK_CONFIG_DIR:-${XDG_STATE_HOME:-${HOME:?HOME required}/.local/state}/atrinik-client}
case "$config" in /*) ;; *) echo 'client config directory must be absolute' >&2; exit 2 ;; esac
config=$(realpath -m -- "$config")
case "$config/" in "$root/"*) echo 'client state must be outside the export' >&2; exit 2 ;; esac
mkdir -p -- "$config"
config=$(CDPATH='' cd -P -- "$config" && pwd)
case "$config/" in "$root/"*) echo 'client state must be outside the export' >&2; exit 2 ;; esac
ATRINIK_CONFIG_DIR=$config
LD_LIBRARY_PATH=$root/lib
export ATRINIK_CONFIG_DIR LD_LIBRARY_PATH
unset LD_PRELOAD LD_AUDIT
cd -- "$root"/""" + data + """
exec "$root"/""" + binary + """ "$@"
""").encode("utf-8")
