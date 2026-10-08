"""Executable-bound Classic inherited-state contract (Linux only)."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys

from .model import WorkspaceError, _open_directory_nofollow, _reject_duplicate_keys
from .path_identity import canonical_path, descriptor_path

CAPABILITY_FILE = "atrinik-server-capabilities.json"


def _identity(metadata: os.stat_result) -> tuple[int, ...]:
    return (metadata.st_dev, metadata.st_ino, metadata.st_mode, metadata.st_uid,
            metadata.st_nlink, metadata.st_size, metadata.st_mtime_ns, metadata.st_ctime_ns)


def _trusted(metadata: os.stat_result, *, directory: bool = False) -> bool:
    return ((stat.S_ISDIR(metadata.st_mode) if directory else stat.S_ISREG(metadata.st_mode))
            and metadata.st_uid in {0, os.geteuid()} and not metadata.st_mode & 0o022)


def _read_file(directory: int, name: str, *, digest: bool = False) -> bytes | str:
    descriptor = os.open(name, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK,
                         dir_fd=directory)
    try:
        before = os.fstat(descriptor)
        if not _trusted(before) or before.st_nlink != 1:
            raise WorkspaceError(f"invalid Classic server capability input: {name}")
        if not digest and before.st_size > 4096:
            raise WorkspaceError("Classic server capability artifact is too large")
        result = hashlib.sha256() if digest else bytearray()
        while chunk := os.read(descriptor, 1024 * 1024 if digest else 4097):
            if digest:
                result.update(chunk)
            else:
                result.extend(chunk)
                if len(result) > 4096:
                    raise WorkspaceError("Classic server capability artifact is too large")
        if (_identity(before) != _identity(os.fstat(descriptor))
                or _identity(before) != _identity(os.stat(name, dir_fd=directory, follow_symlinks=False))):
            raise WorkspaceError(f"Classic server capability input changed: {name}")
        return result.hexdigest() if digest else bytes(result)
    finally:
        os.close(descriptor)


def _require_locked(descriptor: int, label: str) -> None:
    # fdinfo reports locks attached to this open-file description, including an
    # inherited or dup'ed descriptor. Never reopen or acquire the state lock here.
    lines = Path(f"/proc/self/fdinfo/{descriptor}").read_text().splitlines()
    if not any(re.fullmatch(r"lock:\s+\d+: FLOCK\s+ADVISORY\s+WRITE\s+.* 0 EOF", line)
               for line in lines):
        raise WorkspaceError(f"Classic server {label} descriptor lacks its exclusive lock")


def server_datapath_arguments(runtime: Path, state_fd: int | None,
                             runtime_lease_fd: int | None) -> list[str]:
    """Select explicit capabilities under the caller's immutable generation lease."""
    artifact = runtime / CAPABILITY_FILE
    try:
        artifact.lstat()
    except FileNotFoundError:
        return [f"--datapath=/proc/self/fd/{state_fd}"] if state_fd is not None else []
    if sys.platform != "linux":
        raise WorkspaceError("Classic server datapath_fd capability requires Linux")
    directory = None
    try:
        directory = _open_directory_nofollow(
            runtime, os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW)
        root = os.fstat(directory)
        if not _trusted(root, directory=True) or descriptor_path(directory) != canonical_path(runtime):
            raise WorkspaceError("invalid Classic server capability generation directory")
        if type(runtime_lease_fd) is not int or runtime_lease_fd <= 2:
            raise WorkspaceError("Classic server capability requires its runtime lease")
        lease = os.fstat(runtime_lease_fd)
        visible_lease = (runtime.parent / "generation.lease").stat(follow_symlinks=False)
        if (not _trusted(lease) or _identity(lease) != _identity(visible_lease)
                or descriptor_path(runtime_lease_fd) != canonical_path(runtime.parent / "generation.lease")):
            raise WorkspaceError("Classic server capability runtime lease changed")
        _require_locked(runtime_lease_fd, "runtime lease")
        value = json.loads(_read_file(directory, CAPABILITY_FILE), object_pairs_hook=_reject_duplicate_keys)
        if (not isinstance(value, dict)
                or set(value) != {"schema_version", "datapath_fd", "server_sha256"}
                or type(value["schema_version"]) is not int or value["schema_version"] != 1
                or value["datapath_fd"] is not True
                or not isinstance(value["server_sha256"], str)
                or re.fullmatch(r"[0-9a-f]{64}", value["server_sha256"]) is None
                or value["server_sha256"] != _read_file(directory, "atrinik-server", digest=True)):
            raise WorkspaceError("invalid Classic server capability artifact or executable digest")
        if type(state_fd) is not int or state_fd <= 2:
            raise WorkspaceError("Classic server capability requires its state descriptor")
        state = os.fstat(state_fd)
        if (not stat.S_ISDIR(state.st_mode) or state.st_uid != os.geteuid()
                or stat.S_IMODE(state.st_mode) != 0o700):
            raise WorkspaceError("Classic server capability state must be an owned private directory")
        _require_locked(state_fd, "state")
        target = f"/proc/self/fd/{state_fd}"
        if os.readlink("data", dir_fd=directory) != target:
            raise WorkspaceError("Classic server capability data link differs from its descriptor")
        if _identity(state) != _identity(os.stat("data", dir_fd=directory)):
            raise WorkspaceError("Classic server capability data identity changed")
        if (_identity(root) != _identity(os.fstat(directory))
                or descriptor_path(directory) != canonical_path(runtime)):
            raise WorkspaceError("Classic server capability generation changed")
        return [f"--datapath_fd={state_fd}", "--datapath=./data"]
    except (OSError, ValueError, UnicodeError, RecursionError) as error:
        raise WorkspaceError(f"cannot validate Classic server capability: {error}") from error
    finally:
        if directory is not None:
            os.close(directory)


def validate_server_datapath_launch(command: list[str], runtime: Path,
                                   state_fd: int | None, runtime_lease_fd: int | None,
                                   state_path: Path | None = None) -> None:
    """Repeat capability, argv and physical-state binding checks at process spawn."""
    expected = server_datapath_arguments(runtime, state_fd, runtime_lease_fd)
    actual = [argument for argument in command if argument.startswith("--datapath")]
    capability = any(argument.startswith("--datapath_fd") for argument in actual)
    if not capability and not any(argument.startswith("--datapath_fd") for argument in expected):
        return  # Preserve historical launches without an advertised capability.
    if actual != expected or command[0] != str(runtime / "atrinik-server"):
        raise WorkspaceError("Classic server capability command differs from inherited state descriptor")
    try:
        if state_path is not None and (
                descriptor_path(state_fd) != canonical_path(state_path)
                or _identity(os.fstat(state_fd)) != _identity(state_path.stat(follow_symlinks=False))):
            raise WorkspaceError("Classic server capability state path identity changed")
    except OSError as error:
        raise WorkspaceError(f"cannot validate Classic server capability state identity: {error}") from error
