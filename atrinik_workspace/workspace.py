from __future__ import annotations

import binascii
import copy
from collections import deque
from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import AbstractContextManager, ExitStack, contextmanager
from contextvars import copy_context
import ctypes
from dataclasses import dataclass
from datetime import datetime, timezone
import errno
import fcntl
import hashlib
import json
import os
import platform
from pathlib import Path, PurePosixPath
import re
import secrets
import shlex
import shutil
import signal
import socket
import ssl
import stat
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
from typing import Any, Callable, Iterator, TextIO
import zipfile

from .launch_identity import CLIENT_LAUNCH_LABEL_ENV, client_launch_label
from .content_migration import ContentMigration
from .locking import (
    LeaseRequest,
    LockBusyError,
    active_lock_fds,
    exclusive_layout_lock,
    exclusive_lock,
    inherit_lock_fds,
    layout_writer_intent_path as _layout_writer_intent_path,
    resource_lock_path,
    resource_lifetime_reader,
    resource_locks,
    shared_maintenance_lock,
    shared_layout_lock,
    shared_lock,
)
from .process_tree import (
    bound_lease_locked,
    control_socket_path,
    holders_exist,
    initialize_lease,
    lease_locked,
    signal_holders,
)
from .port_reservation import (
    PORT_RESERVATION_DIRECTORY,
    PortReservationError,
    active_owner as active_port_reservation,
    create_lease as create_port_reservation,
    open_transaction as open_port_transaction,
    reservation_locked as port_reservation_locked,
    try_lock as try_lock_port_reservation,
    validate_transaction as validate_port_transaction,
    validate_record as validate_port_reservation,
)

from .model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
    Checkout,
    Component,
    Manifest,
    Paths,
    AtomicJsonCommitUncertain,
    WorkspaceError,
    _reject_duplicate_keys,
    atomic_json,
    durable_atomic_json,
    load_json,
    _managed_path_no_symlinks,
    _open_directory_nofollow,
    managed_directory,
    managed_reset,
    profile_key,
    require_keys,
    validate_name,
)
from .migration import (
    MIGRATED_CONTENT_WORKTREE_KIND,
    PROFILE_IDENTITIES,
    RepositoryMigration,
    classic_lineage,
    rename_no_replace,
)
from .supervisor import process_matches
from .sound import (
    EXPECTED_CONVERTED_OPUS,
    EXPECTED_COPIED_VORBIS,
    EXPECTED_PATHS,
    EXPECTED_SOURCE_FLAC,
    EXPECTED_SOURCE_MIDI,
    PLAYTEST_MODE,
    RELEASED_MODE,
    RELEASE_PRODUCT,
    SOUND_MODES,
    SOURCE_MODE,
    cache_key as sound_cache_key,
    clean_source_inputs,
    download_release_archive,
    extract_release_archive,
    release_cache_key,
    source_record as sound_source_record,
    validate_release_coordinates,
    validate_sound_record,
    verify_playtest_tree,
    verify_release_archive,
    verify_release_tree,
)


PROFILE_SCHEMA_VERSION = 5
LEGACY_PROFILE_SCHEMA_VERSION = 4
OLDEST_PROFILE_SCHEMA_VERSION = 3
PROFILE_KEYS = {
    "schema_version", "name", "stack", "components", "sound_mode",
    "sound_release",
}
LEGACY_PROFILE_KEYS = {"schema_version", "name", "stack", "components", "sound_mode"}
OLDEST_PROFILE_KEYS = {"schema_version", "name", "stack", "components"}
SELECTOR_KEYS = {"kind", "value"}
EXPECTED_SERVER_DATA = {
    "files": ("bans", "motd"),
    "directories": ("keys", "unique-items"),
}
SENSITIVE_ARGUMENTS = {"--join_password", "--join-password"}
SENSITIVE_PREFIXES = ("--join_password=", "--join-password=")
ALL_BUILD_TARGETS = (
    "content",
    "protocol",
    "libatrinik",
    "client",
    "server",
    "metaserver-worker",
)
SOURCE_VIEW_METADATA = ".atrinik-source-view.json"
SOURCE_VIEW_SCHEMA_VERSION = 2
SOURCE_INCLUDE_VIEW_METADATA = ".atrinik-source-includes.json"
CONFIGURE_METADATA = ".atrinik-configure.json"
CONFIGURE_SCHEMA_VERSION = 2
COMPILER_CACHE_PURPOSE = "compiler-cache"
COMPILER_CACHE_MAX_SIZE = "5G"
TOPOLOGY_SERVICES = ("server", "client")
TOPOLOGY_PROCESS_TREE_LEASE = "process-tree.lease"
TOPOLOGY_PORT_RESERVATION_RECORD = "port-reservation.json"
TOPOLOGY_STATUS_SCHEMA_VERSION = 3
LEGACY_RUNTIME_TOPOLOGY_STATUS_SCHEMA_VERSION = 2
RUNTIME_GENERATION_SCHEMA_VERSION = 1
RUNTIME_GENERATION_LEASE = "generation.lease"
RUNTIME_GENERATION_MANIFEST = "manifest.json"
RUNTIME_STATE_OUTPUT_TRANSACTION = "runtime-state-output-transaction.json"
STATE_IMPLEMENTATION_MARKER = ".atrinik-state.json"
STATE_IMPLEMENTATION_SCHEMA_VERSION = 1
TEMPORARY_STATE_METADATA = "state.json"
PROMOTED_STATE_METADATA = ".atrinik-promoted-state.json"
TEMPORARY_STATE_SCHEMA_VERSION = 1
PRE_MONOREPO_REPOSITORIES = {
    "client": "legacy-client",
    "server": "legacy-server",
    "editor": "legacy-editor",
    "libatrinik": "legacy-libatrinik",
    "protocol": "legacy-protocol",
}
RESOURCE_PATHS_MANIFEST = "runtime-paths.txt"
SERVER_IDENTITY_MAX_SIZE = 64 * 1024
SCENARIO_KEYS = {
    "schema_version",
    "name",
    "profile",
    "stack",
    "providers",
    "preset",
    "state",
    "account",
    "character",
    "archetype",
    "resolved",
    "provisioned_at",
}
SCENARIO_SCHEMA_VERSION = 4
SCENARIO_PRESETS = {
    "basic-player": {"archetype": "human_male"},
    "lighting-radiance-day": {"archetype": "human_male"},
    "lighting-radiance-dawn": {"archetype": "human_male"},
    "lighting-radiance-night": {"archetype": "human_male"},
    "lighting-radiance-inside": {"archetype": "human_male"},
}
SCENARIO_PASSWORD_MAX_SIZE = 128
SCENARIO_INERT_HISTORICAL_IDENTITY = "historical_identity"
SCENARIO_INERT_PROFILE_UNRESOLVABLE = "profile_unresolvable"
SCENARIO_INERT_INVALID_RECORD = "invalid_record"
BUILD_METADATA = ".atrinik-build.json"
BUILD_METADATA_SCHEMA_VERSION = 3
PROFILE_RESOLUTION_METADATA = ".atrinik-profile-resolution.json"
PROFILE_RESOLUTION_SCHEMA_VERSION = 3
SOURCE_GENERATION_METADATA = ".atrinik-source-generation.json"
SOURCE_GENERATION_SCHEMA_VERSION = 3
CACHE_METADATA = ".atrinik-cache.json"
WORKER_DEPENDENCY_METADATA = ".atrinik-worker-dependencies.json"
WORKER_VIEW_METADATA = ".atrinik-worker-view.json"
WORKER_DEPENDENCY_SCHEMA_VERSION = 4
WORKER_VIEW_SCHEMA_VERSION = 2
WORKER_DEPENDENCY_FILES = ("package.json", "package-lock.json")
WORKER_SOURCE_EXCLUSIONS = {
    ".git",
    MANAGED_MARKER,
    WORKER_VIEW_METADATA,
    "build",
    "dist",
    "node_modules",
    "publisher-worker-configuration.d.ts",
    "rendezvous-worker-configuration.d.ts",
    "worker-configuration.d.ts",
    "worker-runtime.d.ts",
    ".wrangler",
}
WORKER_VIEW_NODE_MODULES_EXCLUSIONS = {".mf", ".vite", ".vite-temp"}
WORKER_NPM_FILE_CONFIG_KEYS = {
    "cafile",
    "certfile",
    "globalconfig",
    "keyfile",
    "userconfig",
}
RUNTIME_INPUT_METADATA = ".atrinik-dependency.json"
RUNTIME_INPUT_SCHEMA_VERSION = 1


@dataclass
class StateLease:
    path_lock: TextIO
    bind_identity: Callable[[dict[str, int]], TextIO | None]
    physical_lock: TextIO | None = None
    physical_identity: dict[str, int] | None = None

    def fileno(self) -> int:
        return self.path_lock.fileno()

    def bind(self, identity: dict[str, int]) -> None:
        if self.physical_lock is not None:
            if self.physical_identity != identity:
                raise WorkspaceError(
                    "server state identity changed while acquiring its lease"
                )
            return
        self.physical_lock = self.bind_identity(identity)
        if self.physical_lock is not None:
            self.physical_identity = dict(identity)


@dataclass(frozen=True)
class ProfileResolutionSnapshot:
    """Immutable profile bytes and exact source observations for one operation."""

    name: str
    generation: str
    profile_json: str
    selected: tuple[tuple[str, str], ...]
    checkout_states_json: str

    def profile(self) -> dict[str, Any]:
        value = json.loads(self.profile_json)
        assert isinstance(value, dict)
        return value

    def paths(self) -> dict[str, Path]:
        return {role: Path(path) for role, path in self.selected}

    def checkout_states(self) -> dict[str, dict[str, Any]]:
        value = json.loads(self.checkout_states_json)
        assert isinstance(value, dict)
        for state in value.values():
            state["path"] = Path(state["path"])
        return value
REGION_MAP_METADATA = ".atrinik-region-maps.json"
REGION_MAP_SCHEMA_VERSION = 1
EXPECTED_REGION_MAP = "incuna_-1"
WINDOWS_PACKAGE_SCHEMA_VERSION = 1
WINDOWS_PACKAGE_VERSION = "0.0.0"
WINDOWS_PACKAGE_MAX_ENTRIES = 100_000
WINDOWS_PACKAGE_MAX_BYTES = 8 * 1024 * 1024 * 1024


class _InertScenarioError(WorkspaceError):
    def __init__(self, reason: str, message: str) -> None:
        super().__init__(message)
        self.reason = reason


def display_arguments(arguments: list[str]) -> str:
    displayed: list[str] = []
    redact_next = False
    for argument in arguments:
        if redact_next:
            displayed.append("<redacted>")
            redact_next = False
        elif argument in SENSITIVE_ARGUMENTS:
            displayed.append(argument)
            redact_next = True
        elif argument.startswith(SENSITIVE_PREFIXES):
            displayed.append(argument.split("=", 1)[0] + "=<redacted>")
        else:
            displayed.append(argument)
    return shlex.join(displayed)


def run(
    arguments: list[str],
    *,
    cwd: Path | None = None,
    capture: bool = False,
    env: dict[str, str] | None = None,
    trace: bool = True,
    diagnostics_to_stderr: bool = True,
    pass_fds: tuple[int, ...] = (),
) -> str:
    if trace:
        print(f"+ {display_arguments(arguments)}", file=sys.stderr)
    try:
        inherited_fds = tuple(
            dict.fromkeys((*active_lock_fds(), *pass_fds))
        )
        result = subprocess.run(
            arguments,
            cwd=cwd,
            check=True,
            text=True,
            capture_output=capture,
            env=env,
            stdout=sys.stderr if diagnostics_to_stderr and not capture else None,
            pass_fds=inherited_fds,
        )
    except FileNotFoundError as error:
        raise WorkspaceError(f"required command not found: {arguments[0]}") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.strip() if capture and error.stderr else ""
        suffix = f": {detail}" if detail else ""
        raise WorkspaceError(
            f"command failed ({error.returncode}): {display_arguments(arguments)}{suffix}"
        ) from error
    return result.stdout.strip() if capture else ""


def git(
    path: Path, *arguments: str, capture: bool = False, trace: bool = True
) -> str:
    return run(["git", "-C", str(path), *arguments], capture=capture, trace=trace)


def _darwin_descriptor_mount_id(descriptor: int) -> tuple[int, int]:
    buffer = ctypes.create_string_buffer(4096)
    library = ctypes.CDLL(None, use_errno=True)
    fstatfs = library.fstatfs
    fstatfs.argtypes = [ctypes.c_int, ctypes.c_void_p]
    fstatfs.restype = ctypes.c_int
    if fstatfs(descriptor, ctypes.byref(buffer)) != 0:
        error = ctypes.get_errno()
        raise WorkspaceError(
            "cannot inspect filesystem mount for descriptor "
            f"{descriptor}: {os.strerror(error)}"
        )
    # Darwin's fsid_t is two signed 32-bit integers at byte offset 48 in
    # struct statfs, after the size and block/file count fields.
    first = ctypes.c_int32.from_buffer(buffer, 48).value
    second = ctypes.c_int32.from_buffer(buffer, 52).value
    return first, second


def _linux_descriptor_mount_id(descriptor: int) -> int:
    buffer = ctypes.create_string_buffer(256)
    library = ctypes.CDLL(None, use_errno=True)
    try:
        statx = library.statx
    except AttributeError as error:
        raise WorkspaceError("Linux statx mount identity is unavailable") from error
    statx.argtypes = [
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_uint,
        ctypes.c_void_p,
    ]
    statx.restype = ctypes.c_int
    statx_mount_id = 0x1000
    at_empty_path = 0x1000
    if (
        statx(
            descriptor,
            b"",
            at_empty_path,
            statx_mount_id,
            ctypes.byref(buffer),
        )
        != 0
    ):
        error = ctypes.get_errno()
        raise WorkspaceError(
            "cannot inspect filesystem mount for descriptor "
            f"{descriptor}: {os.strerror(error)}"
        )
    returned_mask = ctypes.c_uint32.from_buffer(buffer, 0).value
    if returned_mask & statx_mount_id == 0:
        raise WorkspaceError("Linux statx did not return a mount identity")
    return ctypes.c_uint64.from_buffer(buffer, 144).value


def _descriptor_mount_id(descriptor: int) -> int | tuple[int, int]:
    if sys.platform == "darwin":
        return _darwin_descriptor_mount_id(descriptor)
    if sys.platform == "linux":
        return _linux_descriptor_mount_id(descriptor)
    else:
        raise WorkspaceError(
            f"filesystem mount identity is unavailable on {sys.platform}"
        )


def _descriptor_path(descriptor: int) -> Path:
    """Return the host's stable pathname for an open directory descriptor."""

    root = Path("/proc/self/fd") if sys.platform == "linux" else Path("/dev/fd")
    path = root / str(descriptor)
    try:
        path.resolve(strict=True)
    except OSError as error:
        raise WorkspaceError(
            f"open-descriptor paths are unavailable on {sys.platform}: {path}"
        ) from error
    return path


def _linux_fchmod_path_descriptor(descriptor: int, mode: int) -> None:
    library = ctypes.CDLL(None, use_errno=True)
    syscall = library.syscall
    syscall.restype = ctypes.c_long
    # fchmodat2 is syscall 452 on the Linux generic and x86-64 tables.
    if (
        syscall(
            ctypes.c_long(452),
            ctypes.c_int(descriptor),
            ctypes.c_char_p(b""),
            ctypes.c_uint(mode),
            ctypes.c_int(0x1000),  # AT_EMPTY_PATH
        )
        != 0
    ):
        error = ctypes.get_errno()
        raise WorkspaceError(
            "cannot change owned removal directory mode for descriptor "
            f"{descriptor}: {os.strerror(error)}"
        )


def _open_owned_tree_directory(
    parent_descriptor: int,
    name: str,
    before: os.stat_result,
    mount_id: int | tuple[int, int],
    display: Path,
    *,
    root: bool = False,
) -> int:
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    try:
        readable_descriptor = os.open(name, flags, dir_fd=parent_descriptor)
    except OSError as error:
        if sys.platform == "linux" and error.errno in (errno.EACCES, errno.EPERM):
            return _open_unreadable_linux_owned_tree_directory(
                parent_descriptor, name, before, mount_id, display, root=root
            )
        label = "root" if root else "directory"
        raise WorkspaceError(
            f"owned removal {label} changed: {display}"
        ) from error
    changed_mode = False
    try:
        opened = os.fstat(readable_descriptor)
        if (
            opened.st_dev != before.st_dev
            or opened.st_ino != before.st_ino
            or _descriptor_mount_id(readable_descriptor) != mount_id
        ):
            message = (
                f"owned removal root changed or is mounted: {display}"
                if root
                else f"owned removal encountered a mount: {display}"
            )
            raise WorkspaceError(message)
        os.fchmod(readable_descriptor, stat.S_IRWXU)
        changed_mode = True
        readable = os.fstat(readable_descriptor)
        if (
            readable.st_dev != before.st_dev
            or readable.st_ino != before.st_ino
            or _descriptor_mount_id(readable_descriptor) != mount_id
        ):
            raise WorkspaceError(
                f"owned removal encountered a mount: {display}"
            )
        return readable_descriptor
    except BaseException:
        try:
            if changed_mode:
                os.fchmod(readable_descriptor, stat.S_IMODE(before.st_mode))
        except OSError as restore_error:
            raise WorkspaceError(
                "cannot restore owned removal directory mode after "
                f"open failure: {display}"
            ) from restore_error
        finally:
            os.close(readable_descriptor)
        raise


def _open_unreadable_linux_owned_tree_directory(
    parent_descriptor: int,
    name: str,
    before: os.stat_result,
    mount_id: int | tuple[int, int],
    display: Path,
    *,
    root: bool,
) -> int:
    try:
        bound_descriptor = os.open(
            name,
            os.O_PATH | os.O_DIRECTORY | os.O_NOFOLLOW,
            dir_fd=parent_descriptor,
        )
    except OSError as error:
        label = "root" if root else "directory"
        raise WorkspaceError(
            f"owned removal {label} changed: {display}"
        ) from error
    changed_mode = False
    try:
        opened = os.fstat(bound_descriptor)
        if (
            opened.st_dev != before.st_dev
            or opened.st_ino != before.st_ino
            or _descriptor_mount_id(bound_descriptor) != mount_id
        ):
            message = (
                f"owned removal root changed or is mounted: {display}"
                if root
                else f"owned removal encountered a mount: {display}"
            )
            raise WorkspaceError(message)
        _linux_fchmod_path_descriptor(bound_descriptor, stat.S_IRWXU)
        changed_mode = True
        readable_descriptor = os.open(
            ".",
            os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
            dir_fd=bound_descriptor,
        )
        readable = os.fstat(readable_descriptor)
        if (
            readable.st_dev != before.st_dev
            or readable.st_ino != before.st_ino
            or _descriptor_mount_id(readable_descriptor) != mount_id
        ):
            os.close(readable_descriptor)
            raise WorkspaceError(
                f"owned removal encountered a mount: {display}"
            )
        return readable_descriptor
    except BaseException:
        if changed_mode:
            try:
                _linux_fchmod_path_descriptor(
                    bound_descriptor, stat.S_IMODE(before.st_mode)
                )
            except WorkspaceError as restore_error:
                raise WorkspaceError(
                    "cannot restore owned removal directory mode after "
                    f"open failure: {display}"
                ) from restore_error
        raise
    finally:
        os.close(bound_descriptor)


def _probe_owned_tree_entry_mount(
    descriptor: int,
    name: str,
    child: os.stat_result,
    mount_id: int | tuple[int, int],
    display: Path,
) -> None:
    if stat.S_ISLNK(child.st_mode):
        return
    if not (stat.S_ISREG(child.st_mode) or stat.S_ISDIR(child.st_mode)):
        raise WorkspaceError(f"owned removal entry is unsupported: {display}")
    flags = os.O_NOFOLLOW
    if sys.platform == "linux":
        flags |= os.O_PATH
    else:
        flags |= os.O_RDONLY | os.O_NONBLOCK
    try:
        probe = os.open(name, flags, dir_fd=descriptor)
    except OSError as error:
        raise WorkspaceError(f"owned removal entry changed: {display}") from error
    try:
        opened = os.fstat(probe)
        if (
            opened.st_dev != child.st_dev
            or opened.st_ino != child.st_ino
            or _descriptor_mount_id(probe) != mount_id
        ):
            raise WorkspaceError(f"owned removal encountered a mount: {display}")
    finally:
        os.close(probe)


def _prepare_owned_tree_removal(
    descriptor: int,
    device: int,
    mount_id: int | tuple[int, int],
    display: Path,
    original_mode: int | None = None,
    reject_links: bool = False,
) -> None:
    root = os.fstat(descriptor)
    if (
        not stat.S_ISDIR(root.st_mode)
        or root.st_dev != device
        or _descriptor_mount_id(descriptor) != mount_id
    ):
        raise WorkspaceError(f"owned removal crossed a filesystem boundary: {display}")
    restore_mode = (
        stat.S_IMODE(root.st_mode) if original_mode is None else original_mode
    )
    os.fchmod(descriptor, stat.S_IRWXU)
    try:
        for name in sorted(os.listdir(descriptor)):
            child_display = display / name
            child = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
            if child.st_dev != device:
                raise WorkspaceError(
                    f"owned removal encountered a mount: {child_display}"
                )
            if reject_links and not (
                stat.S_ISDIR(child.st_mode)
                or (stat.S_ISREG(child.st_mode) and child.st_nlink == 1)
            ):
                raise WorkspaceError(
                    f"owned removal encountered linked state: {child_display}"
                )
            if stat.S_ISDIR(child.st_mode):
                child_descriptor = _open_owned_tree_directory(
                    descriptor, name, child, mount_id, child_display
                )
                try:
                    _prepare_owned_tree_removal(
                        child_descriptor,
                        device,
                        mount_id,
                        child_display,
                        stat.S_IMODE(child.st_mode),
                        reject_links,
                    )
                finally:
                    os.close(child_descriptor)
            else:
                _probe_owned_tree_entry_mount(
                    descriptor, name, child, mount_id, child_display
                )
    finally:
        try:
            os.fchmod(descriptor, restore_mode)
        except OSError as restore_error:
            raise WorkspaceError(
                f"cannot restore owned removal directory mode: {display}"
            ) from restore_error


def _owned_tree_tombstone_name(name: str, device: int, inode: int) -> str:
    digest = hashlib.sha256(name.encode("utf-8")).hexdigest()[:16]
    return f".remove-{digest}-{device:x}-{inode:x}"


def _owned_tree_tombstone_path(
    path: Path, identity: dict[str, int]
) -> Path:
    return path.parent / _owned_tree_tombstone_name(
        path.name, identity["device"], identity["inode"]
    )


def _fsync_directory(path: Path) -> None:
    descriptor = _open_directory_nofollow(
        path, os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
    )
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _remove_owned_tree_contents(
    descriptor: int,
    device: int,
    mount_id: int | tuple[int, int],
    display: Path,
    reject_links: bool = False,
) -> None:
    def move_to_tombstone(name: str, child: os.stat_result) -> str:
        tombstone = _owned_tree_tombstone_name(
            name, child.st_dev, child.st_ino
        )
        rename_no_replace_at(descriptor, name, descriptor, tombstone)
        moved = os.stat(tombstone, dir_fd=descriptor, follow_symlinks=False)
        if (moved.st_dev, moved.st_ino) != (
            child.st_dev,
            child.st_ino,
        ):
            try:
                rename_no_replace_at(descriptor, tombstone, descriptor, name)
            except WorkspaceError:
                pass
            raise WorkspaceError(
                f"owned removal entry identity changed: {display / name}"
            )
        if reject_links and not (
            stat.S_ISDIR(moved.st_mode)
            or (stat.S_ISREG(moved.st_mode) and moved.st_nlink == 1)
        ):
            try:
                rename_no_replace_at(descriptor, tombstone, descriptor, name)
            except WorkspaceError:
                pass
            raise WorkspaceError(
                f"owned removal encountered linked state: {display / name}"
            )
        return tombstone

    for name in sorted(os.listdir(descriptor)):
        child_display = display / name
        child = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
        tombstone_match = re.fullmatch(
            r"\.remove-[0-9a-f]{16}-([0-9a-f]+)-([0-9a-f]+)", name
        )
        if tombstone_match and (child.st_dev, child.st_ino) != (
            int(tombstone_match.group(1), 16),
            int(tombstone_match.group(2), 16),
        ):
            raise WorkspaceError(
                f"owned removal has an uncertain tombstone: {child_display}"
            )
        if child.st_dev != device:
            raise WorkspaceError(
                f"owned removal encountered a mount: {child_display}"
            )
        if reject_links and not (
            stat.S_ISDIR(child.st_mode)
            or (stat.S_ISREG(child.st_mode) and child.st_nlink == 1)
        ):
            raise WorkspaceError(
                f"owned removal encountered linked state: {child_display}"
            )
        if stat.S_ISDIR(child.st_mode):
            child_descriptor = _open_owned_tree_directory(
                descriptor, name, child, mount_id, child_display
            )
            try:
                _remove_owned_tree_contents(
                    child_descriptor,
                    device,
                    mount_id,
                    child_display,
                    reject_links,
                )
            finally:
                os.close(child_descriptor)
            tombstone = name if tombstone_match else move_to_tombstone(name, child)
            os.rmdir(tombstone, dir_fd=descriptor)
        else:
            _probe_owned_tree_entry_mount(
                descriptor, name, child, mount_id, child_display
            )
            tombstone = name if tombstone_match else move_to_tombstone(name, child)
            os.unlink(tombstone, dir_fd=descriptor)


def remove_owned_tree(
    path: Path,
    *,
    expected_identity: dict[str, int] | None = None,
    keep_root: bool = False,
    reject_links: bool = False,
    parent_directory_fd: int | None = None,
) -> None:
    if expected_identity is not None and (
        set(expected_identity) != {"device", "inode"}
        or any(
            not isinstance(value, int) or isinstance(value, bool) or value < 0
            for value in expected_identity.values()
        )
    ):
        raise WorkspaceError(f"owned removal root identity is invalid: {path}")
    def root_tombstone_name(identity: dict[str, int] | tuple[int, int]) -> str:
        device, inode = (
            (identity["device"], identity["inode"])
            if isinstance(identity, dict)
            else identity
        )
        return _owned_tree_tombstone_name(path.name, device, inode)

    parent_descriptor = (
        os.dup(parent_directory_fd)
        if parent_directory_fd is not None
        else _open_directory_nofollow(
            path.parent,
            os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
        )
    )
    descriptor: int | None = None
    try:
        entry_name = path.name
        already_tombstoned = False
        try:
            before = os.stat(
                entry_name, dir_fd=parent_descriptor, follow_symlinks=False
            )
        except FileNotFoundError:
            if expected_identity is None:
                raise WorkspaceError(f"owned removal root is invalid: {path}")
            entry_name = root_tombstone_name(expected_identity)
            try:
                before = os.stat(
                    entry_name, dir_fd=parent_descriptor, follow_symlinks=False
                )
            except FileNotFoundError as error:
                raise WorkspaceError(
                    f"owned removal root is invalid: {path}"
                ) from error
            already_tombstoned = True
        if not stat.S_ISDIR(before.st_mode):
            raise WorkspaceError(f"owned removal root is invalid: {path}")
        if expected_identity is not None and (before.st_dev, before.st_ino) != (
            expected_identity["device"],
            expected_identity["inode"],
        ):
            raise WorkspaceError(f"owned removal root identity changed: {path}")
        parent_mount_id = _descriptor_mount_id(parent_descriptor)
        descriptor = _open_owned_tree_directory(
            parent_descriptor,
            entry_name,
            before,
            parent_mount_id,
            path,
            root=True,
        )
        opened = os.fstat(descriptor)
        expected = (opened.st_dev, opened.st_ino)
        root_mount_id = _descriptor_mount_id(descriptor)
        _prepare_owned_tree_removal(
            descriptor,
            opened.st_dev,
            root_mount_id,
            path,
            stat.S_IMODE(before.st_mode),
            reject_links,
        )
        visible = os.stat(
            entry_name, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if (visible.st_dev, visible.st_ino) != expected:
            raise WorkspaceError(f"owned removal root identity changed: {path}")
        os.fchmod(descriptor, stat.S_IRWXU)
        _remove_owned_tree_contents(
            descriptor,
            opened.st_dev,
            root_mount_id,
            path,
            reject_links,
        )
        os.close(descriptor)
        descriptor = None
        visible = os.stat(
            entry_name, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if (visible.st_dev, visible.st_ino) != expected:
            raise WorkspaceError(f"owned removal root identity changed: {path}")
        if keep_root:
            os.fsync(parent_descriptor)
            return
        tombstone = root_tombstone_name(expected)
        if not already_tombstoned:
            rename_no_replace_at(
                parent_descriptor,
                path.name,
                parent_descriptor,
                tombstone,
            )
        moved = os.stat(
            tombstone, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if (moved.st_dev, moved.st_ino) != expected:
            try:
                rename_no_replace_at(
                    parent_descriptor,
                    tombstone,
                    parent_descriptor,
                    path.name,
                )
            except WorkspaceError:
                pass
            raise WorkspaceError(f"owned removal root identity changed: {path}")
        os.rmdir(tombstone, dir_fd=parent_descriptor)
    finally:
        if descriptor is not None:
            os.close(descriptor)
        os.close(parent_descriptor)


def recover_replaced_directory(output: Path, backup_prefix: str) -> None:
    backup_container = output.parent / f"{backup_prefix}pending"
    journal = output.parent / f"{backup_prefix}pending.json"
    expected_metadata = {
        "schema_version": SCHEMA_VERSION,
        "purpose": "replaced-directory-backup",
        "output": output.name,
    }
    if not (journal.exists() or journal.is_symlink()):
        if backup_container.exists() or backup_container.is_symlink():
            raise WorkspaceError(
                f"replaced-directory backup is not managed: {backup_container}"
            )
        return
    if not journal.is_file() or journal.is_symlink():
        raise WorkspaceError(f"replaced-directory journal is invalid: {journal}")
    metadata = load_json(journal)
    if not isinstance(metadata, dict):
        raise WorkspaceError(f"replaced-directory journal is invalid: {journal}")
    phase = metadata.pop("phase", None)
    if metadata != expected_metadata or phase not in {"initializing", "prepared", "committed"}:
        raise WorkspaceError(f"replaced-directory journal is invalid: {journal}")
    previous = backup_container / "previous"
    try:
        if phase == "initializing":
            if backup_container.exists() or backup_container.is_symlink():
                if (
                    not backup_container.is_dir()
                    or backup_container.is_symlink()
                    or any(backup_container.iterdir())
                ):
                    raise WorkspaceError(
                        f"replaced-directory backup is invalid: {backup_container}"
                    )
                remove_owned_tree(backup_container)
        else:
            if not backup_container.is_dir() or backup_container.is_symlink():
                if phase == "committed" and output.is_dir() and not output.is_symlink():
                    journal.unlink()
                    return
                raise WorkspaceError(
                    f"replaced-directory backup is invalid: {backup_container}"
                )
            if previous.exists() or previous.is_symlink():
                if not previous.is_dir() or previous.is_symlink():
                    raise WorkspaceError(
                        "replaced-directory backup payload is invalid: "
                        f"{previous}"
                    )
                if phase == "prepared":
                    if output.exists() or output.is_symlink():
                        if not output.is_dir() or output.is_symlink():
                            raise WorkspaceError(
                                f"uncommitted replacement is invalid: {output}"
                            )
                        remove_owned_tree(output)
                    previous.replace(output)
                else:
                    if not output.is_dir() or output.is_symlink():
                        raise WorkspaceError(
                            f"committed replacement is invalid: {output}"
                        )
                    remove_owned_tree(previous)
            elif phase == "committed":
                if not output.is_dir() or output.is_symlink():
                    raise WorkspaceError(
                        f"committed replacement is invalid: {output}"
                    )
            elif not output.is_dir() or output.is_symlink():
                raise WorkspaceError(
                    f"uncommitted replacement is invalid: {output}"
                )
            if any(backup_container.iterdir()):
                raise WorkspaceError(
                    f"replaced-directory backup is not empty: {backup_container}"
                )
            remove_owned_tree(backup_container)
        journal.unlink()
    except OSError as error:
        raise WorkspaceError(
            f"cannot recover replaced-directory transaction {journal}: {error}"
        ) from error


def replace_runtime_directory(
    output: Path,
    staging: Path,
    backup_prefix: str,
    verify_after_install: Callable[[], None] | None = None,
) -> None:
    recover_replaced_directory(output, backup_prefix)
    backup_container = output.parent / f"{backup_prefix}pending"
    journal = output.parent / f"{backup_prefix}pending.json"
    backup_metadata: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "purpose": "replaced-directory-backup",
        "output": output.name,
        "phase": "initializing",
    }

    backup: Path | None = None
    if output.exists():
        atomic_json(journal, backup_metadata)
        backup_container.mkdir()
        backup_metadata["phase"] = "prepared"
        atomic_json(journal, backup_metadata)
        backup = backup_container / "previous"
        output.replace(backup)
        try:
            staging.replace(output)
        except BaseException:
            backup.replace(output)
            remove_owned_tree(backup_container)
            journal.unlink()
            raise
    else:
        staging.replace(output)
    try:
        if verify_after_install is not None:
            verify_after_install()
    except BaseException:
        output.replace(staging)
        if backup is not None:
            backup.replace(output)
            remove_owned_tree(backup_container)
            journal.unlink()
        raise
    if backup is not None:
        backup_metadata["phase"] = "committed"
        atomic_json(journal, backup_metadata)
        try:
            remove_owned_tree(backup)
            remove_owned_tree(backup_container)
            journal.unlink()
        except (OSError, WorkspaceError) as error:
            print(
                "warning: cannot remove managed replaced-directory backup "
                f"{backup_container}: {error}; the next replacement will retry "
                "before changing output",
                file=sys.stderr,
            )


def replace_directory(
    output: Path,
    staging: Path,
    backup_prefix: str,
    backup_parent: Path | None = None,
    verify_after_install: Callable[[], None] | None = None,
) -> None:
    backup: Path | None = None
    if output.exists():
        backup = Path(
            tempfile.mkdtemp(prefix=backup_prefix, dir=backup_parent or output.parent)
        )
        backup.rmdir()
        output.replace(backup)
        try:
            # Rename preserves the old tree's timestamps. Refresh the backup
            # root so cleanup's no-follow tree age measures when this
            # transaction was created, not when the replaced output last
            # changed.
            os.utime(backup, None, follow_symlinks=False)
            staging.replace(output)
        except BaseException:
            backup.replace(output)
            raise
    else:
        staging.replace(output)
    try:
        if verify_after_install is not None:
            verify_after_install()
    except BaseException:
        output.replace(staging)
        if backup is not None:
            backup.replace(output)
        raise
    if backup is not None:
        try:
            remove_owned_tree(backup)
        except (OSError, WorkspaceError) as error:
            print(
                f"warning: cannot remove managed replacement backup {backup}: "
                f"{error}; cleanup will retry after its grace period",
                file=sys.stderr,
            )


def _file_digest(path: Path, description: str) -> str:
    descriptor: int | None = None
    try:
        descriptor = open_regular_file(path, os.O_RDONLY, description)
        digest = hashlib.sha256()
        with os.fdopen(descriptor, "rb") as stream:
            descriptor = None
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
        return digest.hexdigest()
    except WorkspaceError:
        raise
    except OSError as error:
        raise WorkspaceError(f"cannot read {description} {path}: {error}") from error
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _tree_content_inventory(
    root: Path, description: str
) -> dict[str, tuple[int, str]]:
    """Inventory exact regular-file paths and bytes while ignoring modes."""

    entries: dict[str, tuple[int, str]] = {}
    for directory, names, files in os.walk(root, followlinks=False):
        names.sort()
        files.sort()
        current = Path(directory)
        for name in names:
            child = current / name
            metadata = child.lstat()
            if not stat.S_ISDIR(metadata.st_mode):
                raise WorkspaceError(
                    f"{description} contains a linked or special directory: {child}"
                )
        for name in files:
            child = current / name
            metadata = child.lstat()
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1:
                raise WorkspaceError(
                    f"{description} contains a linked or special file: {child}"
                )
            entries[child.relative_to(root).as_posix()] = (
                metadata.st_size,
                _file_digest(child, description),
            )
    return entries


def _tree_content_digest(root: Path, description: str) -> str:
    """Hash exact regular-file paths and bytes while ignoring package modes."""

    inventory = _tree_content_inventory(root, description)
    return _tree_content_inventory_digest(inventory)


def _tree_content_inventory_digest(
    inventory: dict[str, tuple[int, str]],
) -> str:
    """Hash a previously validated content inventory."""

    digest = hashlib.sha256()
    for path, identity in sorted(inventory.items()):
        encoded = json.dumps((path, *identity), separators=(",", ":")).encode(
            "ascii"
        )
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
    return digest.hexdigest()


def _tree_digest(
    root: Path,
    exclusions: set[str],
    *,
    bounded_symlinks: bool = False,
    reject_hardlinks: bool = False,
    reject_symlinks: bool = False,
    copied_metadata: bool = False,
    ignore_root_mtime: bool = False,
) -> str:
    """Hash a tree as framed records without following links."""

    digest = hashlib.sha256()
    resolved_root = root.resolve()

    def record(*fields: object) -> None:
        encoded = json.dumps(
            fields, ensure_ascii=True, separators=(",", ":")
        ).encode()
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)

    def extended_attributes(path: Path) -> tuple[tuple[str, int, str], ...]:
        if not hasattr(os, "listxattr"):
            return ()
        try:
            values = []
            for name in sorted(os.listxattr(path, follow_symlinks=False)):
                value = os.getxattr(path, name, follow_symlinks=False)
                values.append(
                    (name, len(value), hashlib.sha256(value).hexdigest())
                )
            return tuple(values)
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect Worker source metadata {path}: {error}"
            ) from error

    def visit(directory: Path, relative: PurePosixPath) -> None:
        try:
            entries = sorted(directory.iterdir(), key=lambda entry: entry.name)
        except OSError as error:
            raise WorkspaceError(
                f"cannot inventory Worker source {directory}: {error}"
            ) from error
        for entry in entries:
            if not relative.parts and entry.name in exclusions:
                continue
            child = relative / entry.name
            try:
                status = entry.lstat()
                mode = stat.S_IMODE(status.st_mode)
                metadata: tuple[object, ...] = (
                    (
                        status.st_mtime_ns,
                        getattr(status, "st_flags", 0),
                        extended_attributes(entry),
                    )
                    if copied_metadata
                    else ()
                )
                if stat.S_ISDIR(status.st_mode):
                    record(
                        "directory",
                        child.as_posix(),
                        mode,
                        *metadata,
                    )
                    visit(entry, child)
                elif stat.S_ISREG(status.st_mode):
                    if reject_hardlinks and status.st_nlink != 1:
                        raise WorkspaceError(
                            f"generated source contains a hard-linked file: {entry}"
                        )
                    record(
                        "file",
                        child.as_posix(),
                        mode,
                        status.st_size,
                        _file_digest(entry, "Worker tree file"),
                        *metadata,
                    )
                elif stat.S_ISLNK(status.st_mode):
                    target = os.readlink(entry)
                    if reject_symlinks:
                        raise WorkspaceError(
                            f"Worker source contains a symbolic link: {entry}"
                        )
                    if bounded_symlinks:
                        if Path(target).is_absolute():
                            raise WorkspaceError(
                                f"Worker dependencies contain an absolute link: {entry}"
                            )
                        resolved = entry.parent.joinpath(target).resolve(strict=False)
                        try:
                            resolved.relative_to(resolved_root)
                        except ValueError as error:
                            raise WorkspaceError(
                                f"Worker dependencies contain an escaping link: {entry}"
                            ) from error
                        if not resolved.exists():
                            raise WorkspaceError(
                                f"Worker dependencies contain a dangling link: {entry}"
                            )
                    record(
                        "symlink",
                        child.as_posix(),
                        mode,
                        target,
                        *metadata,
                    )
                else:
                    raise WorkspaceError(
                        f"Worker source contains an unsupported file type: {entry}"
                    )
            except WorkspaceError:
                raise
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect Worker source {entry}: {error}"
                ) from error

    if root.is_symlink() or not root.is_dir():
        raise WorkspaceError(f"Worker source is not a regular directory: {root}")
    root_status = root.lstat()
    root_metadata: tuple[object, ...] = (
        (
            0 if ignore_root_mtime else root_status.st_mtime_ns,
            getattr(root_status, "st_flags", 0),
            extended_attributes(root),
        )
        if copied_metadata
        else ()
    )
    record("root", stat.S_IMODE(root_status.st_mode), *root_metadata)
    visit(root, PurePosixPath())
    return digest.hexdigest()


def _source_closure_digest(generation: Path, includes: Iterable[str]) -> str:
    """Authenticate a generated logical source and its declared sibling inputs."""

    entries: dict[str, object] = {
        "source": _tree_digest(
            generation / "source",
            set(),
            bounded_symlinks=True,
            reject_hardlinks=True,
        )
    }
    for include in sorted(includes):
        path = generation.joinpath(*PurePosixPath(include).parts)
        try:
            status = path.lstat()
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect generated source include {path}: {error}"
            ) from error
        if stat.S_ISDIR(status.st_mode):
            entries[include] = _tree_digest(
                path,
                set(),
                bounded_symlinks=True,
                reject_hardlinks=True,
            )
        elif stat.S_ISREG(status.st_mode):
            if status.st_nlink != 1:
                raise WorkspaceError(
                    f"generated source include is hard-linked: {path}"
                )
            entries[include] = {
                "kind": "file",
                "mode": stat.S_IMODE(status.st_mode),
                "size": status.st_size,
                "sha256": _file_digest(path, "generated source include"),
            }
        else:
            raise WorkspaceError(
                f"generated source include is not a regular file or directory: {path}"
            )
    return hashlib.sha256(
        json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def _tree_digest_descriptor(
    root_fd: int,
    display: Path,
    root_exclusions: set[str] | None = None,
) -> str:
    """Hash an exact pinned regular tree without following child links."""

    digest = hashlib.sha256()
    root = os.fstat(root_fd)
    root_mount = _descriptor_mount_id(root_fd)

    def record(*fields: object) -> None:
        encoded = json.dumps(
            fields, ensure_ascii=True, separators=(",", ":")
        ).encode()
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)

    def visit(directory_fd: int, relative: PurePosixPath) -> None:
        for name in sorted(os.listdir(directory_fd)):
            if not relative.parts and name in (root_exclusions or set()):
                continue
            child = relative / name
            child_display = display / child.as_posix()
            metadata = os.stat(
                name, dir_fd=directory_fd, follow_symlinks=False
            )
            mode = stat.S_IMODE(metadata.st_mode)
            if stat.S_ISDIR(metadata.st_mode):
                if metadata.st_dev != root.st_dev:
                    raise WorkspaceError(
                        f"Worker source contains a mounted directory: {child_display}"
                    )
                record("directory", child.as_posix(), mode)
                descriptor = os.open(
                    name,
                    os.O_RDONLY
                    | os.O_CLOEXEC
                    | os.O_DIRECTORY
                    | os.O_NOFOLLOW,
                    dir_fd=directory_fd,
                )
                try:
                    opened = os.fstat(descriptor)
                    if (opened.st_dev, opened.st_ino) != (
                        metadata.st_dev,
                        metadata.st_ino,
                    ) or _descriptor_mount_id(descriptor) != root_mount:
                        raise WorkspaceError(
                            f"Worker source changed during inventory: {child_display}"
                        )
                    visit(descriptor, child)
                finally:
                    os.close(descriptor)
            elif stat.S_ISREG(metadata.st_mode):
                if metadata.st_nlink != 1 or metadata.st_dev != root.st_dev:
                    raise WorkspaceError(
                        f"Worker source contains a linked file: {child_display}"
                    )
                descriptor = os.open(
                    name,
                    os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW,
                    dir_fd=directory_fd,
                )
                try:
                    opened = os.fstat(descriptor)
                    if (
                        not stat.S_ISREG(opened.st_mode)
                        or opened.st_nlink != 1
                        or (opened.st_dev, opened.st_ino)
                        != (metadata.st_dev, metadata.st_ino)
                        or _descriptor_mount_id(descriptor) != root_mount
                    ):
                        raise WorkspaceError(
                            f"Worker source changed during inventory: {child_display}"
                        )
                    file_digest = hashlib.sha256()
                    while chunk := os.read(descriptor, 1024 * 1024):
                        file_digest.update(chunk)
                    record(
                        "file",
                        child.as_posix(),
                        mode,
                        metadata.st_size,
                        file_digest.hexdigest(),
                    )
                finally:
                    os.close(descriptor)
            elif stat.S_ISLNK(metadata.st_mode):
                raise WorkspaceError(
                    f"Worker source contains a symbolic link: {child_display}"
                )
            else:
                raise WorkspaceError(
                    f"Worker source contains an unsupported file type: {child_display}"
                )

    if not stat.S_ISDIR(root.st_mode):
        raise WorkspaceError(f"Worker source is not a regular directory: {display}")
    record("root", stat.S_IMODE(root.st_mode))
    visit(root_fd, PurePosixPath())
    return digest.hexdigest()


def _copy_worker_source(
    source: Path,
    destination: Path,
    *,
    include_npmrc: bool = True,
) -> None:
    """Copy Worker source while excluding generated names only at its root."""

    def ignore(directory: str, names: list[str]) -> list[str]:
        if Path(directory) != source:
            return []
        excluded = set(names) & WORKER_SOURCE_EXCLUSIONS
        if not include_npmrc and Path(directory) == source and ".npmrc" in names:
            excluded.add(".npmrc")
        return sorted(excluded)

    shutil.copytree(
        source,
        destination,
        dirs_exist_ok=True,
        symlinks=True,
        ignore=ignore,
    )


def _copy_worker_source_metadata(source: Path, destination: Path) -> None:
    """Reapply copied Worker metadata without copying file contents."""

    def visit(source_directory: Path, destination_directory: Path, root: bool) -> None:
        for source_entry in sorted(
            source_directory.iterdir(), key=lambda entry: entry.name
        ):
            if root and source_entry.name in WORKER_SOURCE_EXCLUSIONS:
                continue
            destination_entry = destination_directory / source_entry.name
            source_status = source_entry.lstat()
            destination_status = destination_entry.lstat()
            if stat.S_IFMT(source_status.st_mode) != stat.S_IFMT(
                destination_status.st_mode
            ):
                raise WorkspaceError(
                    f"Worker view entry type changed during checks: {destination_entry}"
                )
            if stat.S_ISDIR(source_status.st_mode):
                visit(source_entry, destination_entry, False)
            elif not stat.S_ISREG(source_status.st_mode):
                raise WorkspaceError(
                    f"Worker source contains an unsupported entry: {source_entry}"
                )
            shutil.copystat(source_entry, destination_entry, follow_symlinks=False)
        shutil.copystat(
            source_directory, destination_directory, follow_symlinks=False
        )

    visit(source, destination, True)


def _worker_owner_writable_mode(mode: int) -> int:
    """Return a mode with owner access and no group or other write bits."""

    return mode & ~(stat.S_IWGRP | stat.S_IWOTH) | stat.S_IRWXU


def _make_worker_staging_owner_writable(staging: Path) -> None:
    """Restore owner access after authenticating copied source metadata."""

    descriptor: int | None = None
    try:
        descriptor = os.open(
            staging,
            os.O_RDONLY | os.O_DIRECTORY | getattr(os, "O_NOFOLLOW", 0),
        )
        opened = os.fstat(descriptor)
        visible = staging.stat(follow_symlinks=False)
        if (
            not stat.S_ISDIR(opened.st_mode)
            or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
            or opened.st_uid != os.geteuid()
        ):
            raise WorkspaceError(
                f"Worker staging root ownership is unsafe: {staging}"
            )
        os.fchmod(
            descriptor,
            _worker_owner_writable_mode(stat.S_IMODE(opened.st_mode)),
        )
        writable = os.fstat(descriptor)
        if stat.S_IMODE(writable.st_mode) & stat.S_IRWXU != stat.S_IRWXU:
            raise WorkspaceError(
                f"Worker staging root is not owner-writable: {staging}"
            )
    except WorkspaceError:
        raise
    except OSError as error:
        raise WorkspaceError(
            f"cannot make Worker staging root writable {staging}: {error}"
        ) from error
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _copy_regular_file(
    source: Path,
    destination: Path,
    description: str,
    destination_mode: int | None = None,
) -> None:
    """Copy one no-follow regular file without inheriting extended metadata."""

    source_descriptor = open_regular_file(source, os.O_RDONLY, description)
    destination_descriptor: int | None = None
    try:
        source_status = os.fstat(source_descriptor)
        mode = (
            stat.S_IMODE(source_status.st_mode)
            if destination_mode is None
            else destination_mode
        )
        destination_descriptor = os.open(
            destination,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL,
            mode,
        )
        os.fchmod(destination_descriptor, mode)
        with os.fdopen(source_descriptor, "rb") as source_stream:
            source_descriptor = -1
            with os.fdopen(destination_descriptor, "wb") as destination_stream:
                destination_descriptor = None
                shutil.copyfileobj(source_stream, destination_stream)
        os.utime(
            destination,
            ns=(source_status.st_atime_ns, source_status.st_mtime_ns),
            follow_symlinks=False,
        )
    except OSError as error:
        raise WorkspaceError(f"cannot stage {description} {source}: {error}") from error
    finally:
        if source_descriptor >= 0:
            os.close(source_descriptor)
        if destination_descriptor is not None:
            os.close(destination_descriptor)


def _normalize_worker_atime(root: Path) -> None:
    """Give lifecycle staging a deterministic initial access time."""

    paths = [root]
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        paths.extend(parent / name for name in (*directories, *files))
    for path in reversed(paths):
        try:
            status = path.lstat()
            os.utime(
                path,
                ns=(0, status.st_mtime_ns),
                follow_symlinks=False,
            )
        except OSError as error:
            raise WorkspaceError(
                f"cannot normalize Worker lifecycle access time {path}: {error}"
            ) from error


def _tree_references_path(root: Path, referenced: Path) -> bool:
    """Return whether a no-follow tree stores one absolute path literally."""

    needle = str(referenced).encode()
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        for name in (*directories, *files):
            path = parent / name
            status = path.lstat()
            if stat.S_ISLNK(status.st_mode):
                if needle in os.readlink(path).encode(
                    "utf-8", "surrogateescape"
                ):
                    return True
            elif stat.S_ISREG(status.st_mode):
                descriptor = open_regular_file(
                    path, os.O_RDONLY, "Worker installed file"
                )
                with os.fdopen(descriptor, "rb") as stream:
                    previous = b""
                    while chunk := stream.read(1024 * 1024):
                        combined = previous + chunk
                        if needle in combined:
                            return True
                        previous = combined[-max(0, len(needle) - 1) :]
    return False


def _remote_matches(url: str, repository: str) -> bool:
    normalized = url.strip().removesuffix(".git")
    expected = f"github.com/{repository}"
    if normalized.startswith("git@github.com:"):
        normalized = "github.com/" + normalized.removeprefix("git@github.com:")
    elif normalized.startswith("ssh://git@github.com/"):
        normalized = "github.com/" + normalized.removeprefix("ssh://git@github.com/")
    elif normalized.startswith("https://"):
        normalized = normalized.removeprefix("https://")
    return normalized == expected


def _github_clone_url(template: str, repository: str) -> str | None:
    template = template.strip()
    if template.startswith("git@github.com:"):
        return f"git@github.com:{repository}.git"
    if template.startswith("ssh://git@github.com/"):
        return f"ssh://git@github.com/{repository}.git"
    if template.startswith("https://github.com/"):
        return f"https://github.com/{repository}.git"
    return None


def _is_clean(path: Path, *, trace: bool = True) -> bool:
    return not git(
        path,
        "status",
        "--porcelain=v1",
        "--untracked-files=all",
        capture=True,
        trace=trace,
    )


def _worktree_records(
    repository: Path, *, trace: bool = True
) -> list[dict[str, str]]:
    output = git(
        repository, "worktree", "list", "--porcelain", capture=True, trace=trace
    )
    records: list[dict[str, str]] = []
    current: dict[str, str] = {}
    for line in [*output.splitlines(), ""]:
        if not line:
            if current:
                records.append(current)
                current = {}
            continue
        key, _, value = line.partition(" ")
        current[key] = value
    return records


def open_regular_file(
    path: Path, flags: int, description: str, mode: int = 0o600
) -> int:
    flags |= os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags, mode)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            os.close(descriptor)
            raise WorkspaceError(f"{description} is not a regular file: {path}")
        return descriptor
    except OSError as error:
        raise WorkspaceError(f"cannot open {description} {path}: {error}") from error


def load_regular_json(path: Path, description: str, *, limit: int = 4 * 1024 * 1024) -> Any:
    """Read one identity-bound regular JSON file without following links."""

    descriptor = open_regular_file(path, os.O_RDONLY, description)
    try:
        opened = os.fstat(descriptor)
        visible = path.stat(follow_symlinks=False)
        if (
            (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
            or opened.st_size > limit
        ):
            raise WorkspaceError(f"{description} identity is unsafe: {path}")
        with os.fdopen(descriptor, encoding="utf-8", closefd=False) as stream:
            return json.load(stream, object_pairs_hook=_reject_duplicate_keys)
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise WorkspaceError(f"cannot read {description} {path}: {error}") from error
    finally:
        os.close(descriptor)


def durable_atomic_json_at(directory_fd: int, name: str, value: Any) -> None:
    """Durably replace one JSON file relative to an already pinned directory."""

    if Path(name).name != name:
        raise WorkspaceError(f"descriptor-relative JSON name is invalid: {name}")
    temporary = f".{name}.{secrets.token_hex(12)}.tmp"
    descriptor: int | None = None
    try:
        descriptor = os.open(
            temporary,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
            0o600,
            dir_fd=directory_fd,
        )
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            descriptor = None
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        rename_no_replace_at(
            directory_fd, temporary, directory_fd, name
        )
        os.fsync(directory_fd)
    except BaseException:
        try:
            os.unlink(temporary, dir_fd=directory_fd)
        except FileNotFoundError:
            pass
        raise
    finally:
        if descriptor is not None:
            os.close(descriptor)


def rename_no_replace_at(
    source_directory_fd: int,
    source: str,
    destination_directory_fd: int,
    destination: str,
) -> None:
    try:
        renameat2 = ctypes.CDLL(None, use_errno=True).renameat2
    except AttributeError as error:
        raise WorkspaceError("atomic descriptor-relative rename is unsupported") from error
    renameat2.argtypes = [
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_uint,
    ]
    renameat2.restype = ctypes.c_int
    if (
        renameat2(
            source_directory_fd,
            os.fsencode(source),
            destination_directory_fd,
            os.fsencode(destination),
            1,
        )
        == 0
    ):
        return
    error_number = ctypes.get_errno()
    if error_number == errno.EEXIST:
        raise WorkspaceError(
            f"descriptor-relative destination already exists: {destination}"
        )
    raise WorkspaceError(
        "cannot move descriptor-relative path without replacement: "
        f"{source} -> {destination}: {os.strerror(error_number)}"
    )


def load_regular_json_at(
    directory_fd: int, name: str, description: str, *, limit: int = 4 * 1024 * 1024
) -> Any:
    descriptor = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory_fd)
    try:
        metadata = os.fstat(descriptor)
        if (
            not stat.S_ISREG(metadata.st_mode)
            or metadata.st_nlink != 1
            or metadata.st_size > limit
        ):
            raise WorkspaceError(f"{description} identity is unsafe: {name}")
        with os.fdopen(descriptor, encoding="utf-8", closefd=False) as stream:
            return json.load(stream, object_pairs_hook=_reject_duplicate_keys)
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise WorkspaceError(f"cannot read {description} {name}: {error}") from error
    finally:
        os.close(descriptor)


class Workspace:
    def __init__(self, repository: Path, *, backfill_references: bool = True):
        self.paths = Paths.discover(repository)
        self.manifest = Manifest.load(self.paths.repository / "components.json")
        self._wrapper_lease: Any = None
        self._build_state = threading.local()
        self._prefix_map_support: dict[
            tuple[str, str, str | None, str | None], bool
        ] = {}
        self._prefix_map_support_lock = threading.Lock()
        repository_identity = self.paths.repository.stat()
        common_identity = self._lease_namespace.parent.stat()
        namespace_identity = self._establish_lease_namespace_identity()
        wrapper_request = self._lease_request(
            "source",
            self._source_coordinate("atrinik", self.paths.repository),
            "shared",
            "use wrapper worktree",
        )
        self._wrapper_lease = resource_lifetime_reader(
            self._lease_namespace, wrapper_request
        )
        self._wrapper_lease.__enter__()
        if not self.paths.repository.is_dir():
            raise WorkspaceError(
                f"wrapper worktree disappeared while acquiring its lease: {self.paths.repository}"
            )
        current_repository = self.paths.repository.stat()
        current_common = self._lease_namespace.parent.stat()
        current_namespace = self._lease_namespace.stat(follow_symlinks=False)
        if (
            (repository_identity.st_dev, repository_identity.st_ino)
            != (current_repository.st_dev, current_repository.st_ino)
            or (common_identity.st_dev, common_identity.st_ino)
            != (current_common.st_dev, current_common.st_ino)
            or namespace_identity
            != (current_namespace.st_dev, current_namespace.st_ino)
        ):
            raise WorkspaceError("wrapper worktree identity changed while acquiring its lease")
        self.manifest = Manifest.load(self.paths.repository / "components.json")
        self._physical_lease_namespace_identity = namespace_identity
        if backfill_references:
            self._backfill_physical_references()

    def close(self) -> None:
        """Release the command-lifetime wrapper and maintenance leases."""

        wrapper_lease = self._wrapper_lease
        if wrapper_lease is not None:
            self._wrapper_lease = None
            wrapper_lease.__exit__(None, None, None)

    def scope_create(
        self,
        components: list[str],
        *,
        name: str | None = None,
        base_profile: str = "default",
        labels: list[str] | None = None,
        branches: list[str] | None = None,
        start_points: list[str] | None = None,
        topology: str | None = None,
        state_mode: str = "temporary",
        state_name: str | None = None,
    ) -> dict[str, Any]:
        from .scopes import ScopeLifecycle

        return ScopeLifecycle(self).create(
            components,
            name=name,
            base_profile=base_profile,
            labels=labels,
            branches=branches,
            start_points=start_points,
            topology=topology,
            state_mode=state_mode,
            state_name=state_name,
        )

    def scope_show(self, name: str) -> dict[str, Any]:
        from .scopes import ScopeLifecycle

        return ScopeLifecycle(self).show(name)

    def scope_list(self) -> list[dict[str, Any]]:
        from .scopes import ScopeLifecycle

        return ScopeLifecycle(self).list()

    def scope_release(
        self, name: str, *, apply: bool, plan_sha256: str | None = None
    ) -> dict[str, Any]:
        from .scopes import ScopeLifecycle

        return ScopeLifecycle(self).release(
            name, apply=apply, plan_sha256=plan_sha256
        )

    def _scope_profile_owner(self, name: str) -> str | None:
        from .scopes import ScopeLifecycle

        return ScopeLifecycle(self).profile_owner(name)

    def _scope_topology_owner(self, name: str) -> dict[str, Any] | None:
        from .scopes import ScopeLifecycle

        return ScopeLifecycle(self).topology_owner(name)

    def __del__(self) -> None:
        try:
            self.close()
        except BaseException:
            pass

    @property
    def _force_reconfigure(self) -> bool:
        return getattr(self._build_state, "force_reconfigure", False)

    @_force_reconfigure.setter
    def _force_reconfigure(self, value: bool) -> None:
        self._build_state.force_reconfigure = value

    @property
    def _use_ccache(self) -> bool:
        return getattr(self._build_state, "use_ccache", True)

    @_use_ccache.setter
    def _use_ccache(self, value: bool) -> None:
        self._build_state.use_ccache = value

    @property
    def _source_view_changed(self) -> bool:
        return getattr(self._build_state, "source_view_changed", False)

    @_source_view_changed.setter
    def _source_view_changed(self, value: bool) -> None:
        self._build_state.source_view_changed = value

    @property
    def _source_view_unchanged(self) -> dict[str, bool]:
        current = getattr(self._build_state, "source_view_unchanged", None)
        if current is None:
            current = {}
            self._build_state.source_view_unchanged = current
        return current

    @_source_view_unchanged.setter
    def _source_view_unchanged(self, value: dict[str, bool]) -> None:
        self._build_state.source_view_unchanged = value

    @property
    def _profile_snapshot(self) -> ProfileResolutionSnapshot | None:
        return getattr(self._build_state, "profile_snapshot", None)

    @_profile_snapshot.setter
    def _profile_snapshot(self, value: ProfileResolutionSnapshot | None) -> None:
        self._build_state.profile_snapshot = value

    @staticmethod
    def _lease_recovery(kind: str, coordinate: str) -> str:
        if kind == "profile":
            return f"inspect `./atrinik profile show {coordinate} --json` and retry"
        if kind in {"source", "git-admin"}:
            return "inspect `./atrinik worktree list --json` and retry after the exact operation finishes"
        if kind == "topology":
            return f"inspect `./atrinik ps {coordinate} --json` and stop only that topology when appropriate"
        return "inspect the exact resource owner and retry after its operation finishes"

    def _lease_request(
        self,
        kind: str,
        coordinate: str,
        mode: str,
        operation: str,
    ) -> LeaseRequest:
        return LeaseRequest(
            kind,
            coordinate,
            mode,
            operation,
            self._lease_recovery(kind, coordinate),
        )

    @staticmethod
    def _source_coordinate(checkout_name: str, root: Path) -> str:
        return f"{checkout_name}:{Path(os.path.abspath(root))}"

    @staticmethod
    def _physical_source_coordinate(root: Path) -> str:
        return f"physical-path:{Path(os.path.abspath(root))}"

    @property
    def _lease_namespace(self) -> Path:
        """Return the common-Git namespace shared by physical wrapper views."""

        cached = getattr(self, "_physical_lease_namespace", None)
        if isinstance(cached, Path):
            self._assert_lease_namespace_identity(cached)
            return cached
        fallback = getattr(self, "_fallback_lease_namespace", None)
        if isinstance(fallback, Path) and not (
            self.paths.repository / ".git"
        ).exists():
            self._assert_lease_namespace_identity(fallback)
            return fallback
        try:
            anchor = self._git_common_directory(
                self.paths.repository, trace=False
            )
        except WorkspaceError:
            # Unit fixtures and a not-yet-materialized wrapper still need a
            # stable pre-Git anchor. A production wrapper is itself a checkout.
            git_marker = self.paths.repository / ".git"
            if git_marker.exists() or git_marker.is_symlink():
                raise
            anchor = self.paths.repository.resolve(strict=False)
        namespace = anchor / "atrinik-resource-leases"
        if (self.paths.repository / ".git").exists():
            if isinstance(fallback, Path) and fallback != namespace:
                raise WorkspaceError(
                    "wrapper Git identity materialized after workspace construction; "
                    "recreate the Workspace before continuing"
                )
            self._physical_lease_namespace = namespace
        else:
            self._fallback_lease_namespace = namespace
        return namespace

    def _establish_lease_namespace_identity(self) -> tuple[int, int]:
        namespace = self._lease_namespace
        namespace.mkdir(mode=0o700, exist_ok=True)
        visible = namespace.stat(follow_symlinks=False)
        if not stat.S_ISDIR(visible.st_mode) or stat.S_IMODE(visible.st_mode) != 0o700:
            raise WorkspaceError(f"physical lease namespace is unsafe: {namespace}")
        identity = (visible.st_dev, visible.st_ino)
        if not (self.paths.repository / ".git").exists():
            return identity
        record_path = namespace.parent / "atrinik-resource-leases.identity.json"
        lock_path = namespace.parent / "atrinik-resource-leases.identity.lock"
        with exclusive_lock(lock_path, "physical lease namespace identity"):
            if record_path.exists() or record_path.is_symlink():
                record = load_regular_json(
                    record_path, "physical lease namespace identity"
                )
                if (
                    not isinstance(record, dict)
                    or set(record) != {"schema_version", "device", "inode"}
                    or record.get("schema_version") != 1
                    or record.get("device") != identity[0]
                    or record.get("inode") != identity[1]
                ):
                    raise WorkspaceError(
                        "physical lease namespace identity changed; restore the "
                        f"original namespace: {namespace}"
                    )
            else:
                durable_atomic_json(
                    record_path,
                    {
                        "schema_version": 1,
                        "device": identity[0],
                        "inode": identity[1],
                    },
                )
        return identity

    def _assert_lease_namespace_identity(self, namespace: Path) -> None:
        expected = getattr(self, "_physical_lease_namespace_identity", None)
        if expected is None:
            return
        try:
            visible = namespace.stat(follow_symlinks=False)
        except OSError as error:
            raise WorkspaceError(
                f"physical lease namespace is unavailable: {namespace}: {error}"
            ) from error
        if (
            not stat.S_ISDIR(visible.st_mode)
            or (visible.st_dev, visible.st_ino) != expected
        ):
            raise WorkspaceError(
                "physical lease namespace identity changed; restore the original "
                f"namespace: {namespace}"
            )

    def command_maintenance(self) -> AbstractContextManager[None]:
        """Protect one non-migration CLI command from physical layout writers."""

        return shared_maintenance_lock(
            self._lease_namespace / "repository-layout.lock"
        )

    def _lease_root(self, request: LeaseRequest) -> Path:
        """Route physical and workspace-local coordinates to stable namespaces."""

        if request.kind in {"registry", "git-admin", "source"}:
            return self._lease_namespace
        return self.paths.workspace

    def _assert_physical_namespace_fd(self, descriptor: int) -> None:
        expected = getattr(self, "_physical_lease_namespace_identity", None)
        if expected is None:
            return
        opened = os.fstat(descriptor)
        if (opened.st_dev, opened.st_ino) != expected:
            raise WorkspaceError(
                "physical lease namespace identity changed; restore the original "
                f"namespace: {self._lease_namespace}"
            )

    @contextmanager
    def _resource_locks(
        self,
        requests: list[LeaseRequest] | tuple[LeaseRequest, ...],
        *,
        nonblocking: bool = False,
        include_wrapper: bool = True,
    ) -> Iterator[tuple[TextIO, ...]]:
        """Acquire exact leases beneath the shared migration barrier."""

        wrapper_request = self._lease_request(
            "source",
            self._source_coordinate("atrinik", self.paths.repository),
            "shared",
            "use wrapper worktree",
        )
        protected_requests = (
            (*requests, wrapper_request) if include_wrapper else tuple(requests)
        )
        with shared_maintenance_lock(
            self._lease_namespace / "repository-layout.lock"
        ):
            with resource_locks(
                self._lease_root, protected_requests, nonblocking=nonblocking
            ) as leases:
                self._assert_lease_namespace_identity(self._lease_namespace)
                yield leases
                self._assert_lease_namespace_identity(self._lease_namespace)

    @contextmanager
    def _resource_locks_all_or_none(
        self, requests: list[LeaseRequest] | tuple[LeaseRequest, ...]
    ) -> Iterator[tuple[TextIO, ...]]:
        """Acquire a set without retaining earlier coordinates while waiting."""

        ordered = tuple(sorted(requests, key=lambda request: request.sort_key))
        while True:
            try:
                with self._resource_locks(ordered, nonblocking=True) as leases:
                    yield leases
                    return
            except LockBusyError:
                pass
            for request in ordered:
                try:
                    with self._resource_locks([request], nonblocking=True):
                        continue
                except LockBusyError:
                    with self._resource_locks([request]):
                        pass
                    break

    def _git_admin_coordinate(self, checkout: Checkout, primary: Path) -> str:
        # Every wrapper-managed worktree for a physical checkout is anchored to
        # its canonical primary. The stable primary coordinate remains usable
        # before clone and avoids reading mutable Git administration state just
        # to discover the lease that protects that state.
        return f"{checkout.name}:{primary.resolve(strict=False)}"

    def _wrapper_git_admin_coordinate(self) -> str:
        return f"atrinik:{self._lease_namespace.parent}"

    def _source_generation_record(self, source: Path) -> dict[str, Any] | None:
        metadata = source.parent / SOURCE_GENERATION_METADATA
        if source.name != "source" or not metadata.is_file() or metadata.is_symlink():
            return None
        value = load_regular_json(metadata, "immutable source generation")
        if (
            not isinstance(value, dict)
            or set(value)
            != {
                "schema_version",
                "repository",
                "checkout",
                "branch",
                "commit",
                "tree",
                "source",
                "source_tree",
                "source_includes",
                "source_tree_sha256",
                "closure_tree_sha256",
            }
            or value.get("schema_version") != SOURCE_GENERATION_SCHEMA_VERSION
            or not all(
                isinstance(value.get(field), str) and value[field]
                for field in ("repository", "checkout", "branch", "source")
            )
            or not isinstance(value.get("source_includes"), dict)
            or not all(
                isinstance(value.get(field), str)
                and re.fullmatch(r"[0-9a-f]{40,64}", value[field])
                for field in (
                    "commit",
                    "tree",
                    "source_tree",
                    "source_tree_sha256",
                    "closure_tree_sha256",
                )
            )
            or not all(
                isinstance(path, str)
                and isinstance(tree, str)
                and re.fullmatch(r"[0-9a-f]{40,64}", tree)
                for path, tree in value.get("source_includes", {}).items()
            )
            or not any(
                component.checkout_name == value.get("checkout")
                and component.repository == value.get("repository")
                and component.branch == value.get("branch")
                and component.source == value.get("source")
                and set(component.source_includes)
                == set(value.get("source_includes", {}))
                for component in self.manifest.components
            )
        ):
            raise WorkspaceError(
                f"immutable source generation metadata is invalid: {metadata}"
            )
        identity = {
            field: value[field]
            for field in value
            if field not in {"source_tree_sha256", "closure_tree_sha256"}
        }
        key = hashlib.sha256(
            json.dumps(identity, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        generation = source.parent
        expected = (
            self.paths.builds / "source-generations" / value["checkout"] / key
        )
        marker = generation / MANAGED_MARKER
        try:
            valid_path = (
                not source.is_symlink()
                and source.is_dir()
                and not generation.is_symlink()
                and generation.resolve(strict=False) == expected.resolve(strict=False)
                and not (stat.S_IMODE(generation.lstat().st_mode) & 0o222)
            )
        except RuntimeError:
            valid_path = False
        if (
            not valid_path
            or not marker.is_file()
            or marker.is_symlink()
            or load_json(marker)
            != {
                "schema_version": SCHEMA_VERSION,
                "purpose": f"source-generation:{key}",
            }
        ):
            raise WorkspaceError(
                f"immutable source generation ownership is invalid: {generation}"
            )
        return value

    @staticmethod
    def _extract_git_source_archive(
        archive_path: Path | int, output: Path, *, existing_output: bool = False
    ) -> None:
        """Extract a local Git archive without trusting archive paths or links."""

        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        parent_fd: int | None = None
        root_fd: int | None = None
        archive_file: Any = None
        try:
            parent_fd = _open_directory_nofollow(output.parent, flags, create=True)
            if not existing_output:
                try:
                    os.mkdir(output.name, 0o755, dir_fd=parent_fd)
                except FileExistsError as error:
                    raise WorkspaceError(
                        f"Git source archive output already exists: {output}"
                    ) from error
            visible_root = os.stat(
                output.name, dir_fd=parent_fd, follow_symlinks=False
            )
            root_fd = os.open(output.name, flags, dir_fd=parent_fd)
            opened_root = os.fstat(root_fd)
            root_mount = _descriptor_mount_id(root_fd)
            if (
                (visible_root.st_dev, visible_root.st_ino)
                != (opened_root.st_dev, opened_root.st_ino)
                or not stat.S_ISDIR(opened_root.st_mode)
                or _descriptor_mount_id(parent_fd) != root_mount
            ):
                raise WorkspaceError(
                    f"Git source archive output is not a regular directory: {output}"
                )
            archive_file = (
                os.fdopen(os.dup(archive_path), "rb")
                if isinstance(archive_path, int)
                else None
            )
            if isinstance(archive_path, int):
                os.lseek(archive_path, 0, os.SEEK_SET)
                archive_file.seek(0)
            seen: set[str] = set()
            with tarfile.open(
                archive_path if archive_file is None else None,
                mode="r:",
                fileobj=archive_file,
            ) as archive:
                for member in archive:
                    relative = PurePosixPath(member.name)
                    if (
                        not member.name
                        or relative.is_absolute()
                        or any(part in {"", ".", ".."} for part in relative.parts)
                    ):
                        raise WorkspaceError(
                            f"Git source archive contains an unsafe path: {member.name!r}"
                        )
                    repeated = relative.as_posix() in seen
                    if repeated and not member.isdir():
                        raise WorkspaceError(
                            f"Git source archive repeats a path: {member.name}"
                        )
                    seen.add(relative.as_posix())
                    directory_fd = os.dup(root_fd)
                    try:
                        for part in relative.parts[:-1]:
                            try:
                                child = os.stat(
                                    part,
                                    dir_fd=directory_fd,
                                    follow_symlinks=False,
                                )
                            except FileNotFoundError:
                                os.mkdir(part, 0o755, dir_fd=directory_fd)
                                child = os.stat(
                                    part,
                                    dir_fd=directory_fd,
                                    follow_symlinks=False,
                                )
                            if not stat.S_ISDIR(child.st_mode):
                                if stat.S_ISLNK(child.st_mode):
                                    raise WorkspaceError(
                                        "Git source archive traverses a symbolic "
                                        f"link: {member.name}"
                                    )
                                raise WorkspaceError(
                                    "Git source archive ancestor is not a directory: "
                                    f"{member.name}"
                                )
                            try:
                                next_fd = os.open(part, flags, dir_fd=directory_fd)
                            except OSError as error:
                                raise WorkspaceError(
                                    "Git source archive ancestor changed or cannot "
                                    f"be opened safely: {member.name}"
                                ) from error
                            opened = os.fstat(next_fd)
                            if (
                                (opened.st_dev, opened.st_ino)
                                != (child.st_dev, child.st_ino)
                                or opened.st_dev != opened_root.st_dev
                                or _descriptor_mount_id(next_fd) != root_mount
                            ):
                                os.close(next_fd)
                                raise WorkspaceError(
                                    "Git source archive ancestor changed or is "
                                    f"mounted: {member.name}"
                                )
                            os.close(directory_fd)
                            directory_fd = next_fd
                        name = relative.parts[-1]
                        try:
                            existing = os.stat(
                                name,
                                dir_fd=directory_fd,
                                follow_symlinks=False,
                            )
                        except FileNotFoundError:
                            existing = None
                        permissions = member.mode & 0o777
                        if member.isdir():
                            if existing is None:
                                os.mkdir(name, permissions, dir_fd=directory_fd)
                                existing = os.stat(
                                    name,
                                    dir_fd=directory_fd,
                                    follow_symlinks=False,
                                )
                            elif (
                                not (existing_output or repeated)
                                or not stat.S_ISDIR(existing.st_mode)
                            ):
                                raise WorkspaceError(
                                    f"Git source archive repeats a path: {member.name}"
                                )
                            try:
                                descriptor = os.open(
                                    name, flags, dir_fd=directory_fd
                                )
                            except OSError as error:
                                raise WorkspaceError(
                                    "Git source archive directory changed or cannot "
                                    f"be opened safely: {member.name}"
                                ) from error
                            try:
                                opened = os.fstat(descriptor)
                                if (
                                    (opened.st_dev, opened.st_ino)
                                    != (existing.st_dev, existing.st_ino)
                                    or opened.st_dev != opened_root.st_dev
                                    or _descriptor_mount_id(descriptor) != root_mount
                                ):
                                    raise WorkspaceError(
                                        "Git source archive directory changed or is "
                                        f"mounted: {member.name}"
                                    )
                                os.fchmod(descriptor, permissions)
                            finally:
                                os.close(descriptor)
                        elif member.isreg():
                            if existing is not None:
                                raise WorkspaceError(
                                    f"Git source archive repeats a path: {member.name}"
                                )
                            stream = archive.extractfile(member)
                            if stream is None:
                                raise WorkspaceError(
                                    f"Git source archive cannot read file: {member.name}"
                                )
                            descriptor = os.open(
                                name,
                                os.O_WRONLY
                                | os.O_CREAT
                                | os.O_EXCL
                                | os.O_NOFOLLOW,
                                permissions,
                                dir_fd=directory_fd,
                            )
                            try:
                                with stream, os.fdopen(
                                    descriptor, "wb", closefd=False
                                ) as target:
                                    shutil.copyfileobj(stream, target, 1024 * 1024)
                                    os.fchmod(descriptor, permissions)
                            finally:
                                os.close(descriptor)
                        elif member.issym():
                            if existing is not None:
                                raise WorkspaceError(
                                    f"Git source archive repeats a path: {member.name}"
                                )
                            target = member.linkname
                            if not target or Path(target).is_absolute():
                                raise WorkspaceError(
                                    "Git source archive contains an unsafe link: "
                                    f"{member.name}"
                                )
                            normalized = list(relative.parent.parts)
                            bounded = True
                            for part in PurePosixPath(target).parts:
                                if part in {"", "."}:
                                    continue
                                if part == "..":
                                    if not normalized:
                                        bounded = False
                                        break
                                    normalized.pop()
                                else:
                                    normalized.append(part)
                            if not bounded:
                                raise WorkspaceError(
                                    "Git source archive link escapes its generation: "
                                    f"{member.name}"
                                )
                            os.symlink(target, name, dir_fd=directory_fd)
                        else:
                            raise WorkspaceError(
                                "Git source archive contains an unsupported entry: "
                                f"{member.name}"
                            )
                    finally:
                        os.close(directory_fd)
            current_root = os.stat(
                output.name, dir_fd=parent_fd, follow_symlinks=False
            )
            if (
                (current_root.st_dev, current_root.st_ino)
                != (opened_root.st_dev, opened_root.st_ino)
                or (os.fstat(root_fd).st_dev, os.fstat(root_fd).st_ino)
                != (opened_root.st_dev, opened_root.st_ino)
            ):
                raise WorkspaceError(
                    f"Git source archive output changed during extraction: {output}"
                )
        except WorkspaceError:
            raise
        except (OSError, tarfile.TarError) as error:
            raise WorkspaceError(
                f"cannot extract immutable Git source archive: {error}"
            ) from error
        finally:
            if archive_file is not None:
                archive_file.close()
            if root_fd is not None:
                os.close(root_fd)
            if parent_fd is not None:
                os.close(parent_fd)

    @staticmethod
    def _complete_git_source_archive(
        checkout: Path,
        archive: Path | int,
        object_id: str,
        prefix: str | None,
        mode: bytes = b"040000",
        kind: bytes = b"tree",
    ) -> None:
        """Restore entries omitted by Git archive's export-ignore attributes."""

        expected: dict[str, tuple[bytes, bytes, bytes]] = {}
        if kind == b"tree":
            try:
                result = subprocess.run(
                    [
                        "git",
                        "--no-replace-objects",
                        "-C",
                        str(checkout),
                        "ls-tree",
                        "-r",
                        "-t",
                        "--full-tree",
                        "-z",
                        object_id,
                    ],
                    check=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    pass_fds=active_lock_fds(),
                )
            except FileNotFoundError as error:
                raise WorkspaceError("required command not found: git") from error
            except subprocess.CalledProcessError as error:
                detail = error.stderr.decode("utf-8", errors="replace").strip()
                suffix = f": {detail}" if detail else ""
                raise WorkspaceError(
                    "cannot inspect immutable Git source archive" + suffix
                ) from error
            if prefix is not None:
                expected[prefix] = (b"040000", b"tree", object_id.encode())
            for entry in result.stdout.split(b"\0"):
                if not entry:
                    continue
                try:
                    metadata, relative_bytes = entry.split(b"\t", 1)
                    entry_mode, entry_kind, entry_object = metadata.split(b" ", 2)
                    relative = os.fsdecode(relative_bytes)
                except ValueError as error:
                    raise WorkspaceError(
                        "recorded immutable Git archive listing is invalid"
                    ) from error
                name = f"{prefix}/{relative}" if prefix is not None else relative
                expected[name] = (entry_mode, entry_kind, entry_object)
        else:
            if prefix is None:
                raise WorkspaceError("recorded immutable Git archive listing is invalid")
            expected[prefix] = (mode, kind, object_id.encode())

        archive_file: Any = None
        try:
            archive_file = (
                os.fdopen(os.dup(archive), "r+b")
                if isinstance(archive, int)
                else None
            )
            if isinstance(archive, int):
                os.lseek(archive, 0, os.SEEK_SET)
                archive_file.seek(0)
            with tarfile.open(
                archive if archive_file is None else None,
                "r:",
                fileobj=archive_file,
            ) as stream:
                present = {member.name.rstrip("/") for member in stream}
            if archive_file is not None:
                archive_file.seek(0)
            with tarfile.open(
                archive if archive_file is None else None,
                "a:",
                fileobj=archive_file,
            ) as stream:
                for name, (entry_mode, entry_kind, entry_object) in expected.items():
                    if name in present:
                        continue
                    relative = PurePosixPath(name)
                    if (
                        not name
                        or relative.is_absolute()
                        or any(part in {"", ".", ".."} for part in relative.parts)
                        or entry_mode
                        not in {b"040000", b"100644", b"100755", b"120000"}
                        or entry_kind not in {b"tree", b"blob"}
                    ):
                        raise WorkspaceError(
                            "recorded immutable Git archive listing is invalid"
                        )
                    member = tarfile.TarInfo(name)
                    member.uid = member.gid = 0
                    member.uname = member.gname = ""
                    member.mtime = 0
                    if entry_kind == b"tree":
                        if entry_mode != b"040000":
                            raise WorkspaceError(
                                "recorded immutable Git archive listing is invalid"
                            )
                        member.type = tarfile.DIRTYPE
                        member.mode = 0o755
                        stream.addfile(member)
                        continue
                    payload = tempfile.TemporaryFile()
                    try:
                        try:
                            subprocess.run(
                                [
                                    "git",
                                    "--no-replace-objects",
                                    "-C",
                                    str(checkout),
                                    "cat-file",
                                    "blob",
                                    entry_object.decode(),
                                ],
                                check=True,
                                stdout=payload,
                                stderr=subprocess.PIPE,
                                pass_fds=active_lock_fds(),
                            )
                        except FileNotFoundError as error:
                            raise WorkspaceError(
                                "required command not found: git"
                            ) from error
                        except subprocess.CalledProcessError as error:
                            detail = error.stderr.decode(
                                "utf-8", errors="replace"
                            ).strip()
                            suffix = f": {detail}" if detail else ""
                            raise WorkspaceError(
                                "cannot read immutable Git source object" + suffix
                            ) from error
                        member.size = payload.tell()
                        payload.seek(0)
                        if entry_mode == b"120000":
                            target = payload.read()
                            member.type = tarfile.SYMTYPE
                            member.linkname = os.fsdecode(target)
                            member.size = 0
                            stream.addfile(member)
                        else:
                            member.mode = (
                                0o755 if entry_mode == b"100755" else 0o644
                            )
                            stream.addfile(member, payload)
                    finally:
                        payload.close()
        except WorkspaceError:
            raise
        except (OSError, tarfile.TarError, UnicodeError) as error:
            raise WorkspaceError(
                f"cannot complete immutable Git source archive: {error}"
            ) from error
        finally:
            if archive_file is not None:
                archive_file.close()

    @staticmethod
    def _source_generation_git_entries(
        checkout: Path, source_tree: str
    ) -> dict[bytes, tuple[bytes, bytes, bytes]]:
        """Return the complete, replacement-free entry inventory for a Git tree."""

        try:
            result = subprocess.run(
                [
                    "git",
                    "--no-replace-objects",
                    "-C",
                    str(checkout),
                    "ls-tree",
                    "-r",
                    "-t",
                    "--full-tree",
                    "-z",
                    source_tree,
                ],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                pass_fds=active_lock_fds(),
            )
        except FileNotFoundError as error:
            raise WorkspaceError("required command not found: git") from error
        except subprocess.CalledProcessError as error:
            detail = error.stderr.decode("utf-8", errors="replace").strip()
            suffix = f": {detail}" if detail else ""
            raise WorkspaceError(
                "cannot inspect recorded immutable Git source tree" + suffix
            ) from error

        expected: dict[bytes, tuple[bytes, bytes, bytes]] = {}
        for entry in result.stdout.split(b"\0"):
            if not entry:
                continue
            try:
                metadata, relative = entry.split(b"\t", 1)
                mode, kind, object_id = metadata.split(b" ", 2)
            except ValueError as error:
                raise WorkspaceError(
                    "recorded immutable Git source tree listing is invalid"
                ) from error
            if (
                not relative
                or relative.startswith(b"/")
                or any(
                    part in {b"", b".", b".."}
                    for part in relative.split(b"/")
                )
                or relative in expected
                or kind not in {b"blob", b"tree", b"commit"}
                or mode
                not in {b"040000", b"100644", b"100755", b"120000", b"160000"}
                or len(object_id) not in {40, 64}
                or not re.fullmatch(b"[0-9a-f]+", object_id)
            ):
                raise WorkspaceError(
                    "recorded immutable Git source tree listing is invalid"
                )
            expected[relative] = (mode, kind, object_id)
        return expected

    @classmethod
    def _validate_source_generation_git_tree(
        cls, checkout: Path, source: Path, source_tree: str
    ) -> None:
        """Prove a materialized source has the recorded Git tree identity."""

        expected = cls._source_generation_git_entries(checkout, source_tree)

        algorithm = hashlib.sha1 if len(source_tree) == 40 else hashlib.sha256
        actual: dict[bytes, tuple[bytes, bytes, bytes]] = {}

        def blob_id(payload: bytes) -> bytes:
            digest = algorithm()
            digest.update(f"blob {len(payload)}\0".encode())
            digest.update(payload)
            return digest.hexdigest().encode()

        def stable_identity(value: os.stat_result) -> tuple[int, ...]:
            return (
                value.st_dev,
                value.st_ino,
                value.st_mode,
                value.st_nlink,
                value.st_size,
                value.st_mtime_ns,
                value.st_ctime_ns,
            )

        def changed(
            before: os.stat_result, after: os.stat_result
        ) -> bool:
            return stable_identity(before) != stable_identity(after)

        def visit(directory_fd: int, prefix: bytes) -> None:
            directory_before = os.fstat(directory_fd)
            try:
                entries = sorted(os.listdir(directory_fd))
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect immutable source generation {source}: {error}"
                ) from error
            for entry_name in entries:
                name = os.fsencode(entry_name)
                relative = prefix + (b"/" if prefix else b"") + name
                display = source / os.fsdecode(relative)
                descriptor: int | None = None
                try:
                    status = os.stat(
                        entry_name,
                        dir_fd=directory_fd,
                        follow_symlinks=False,
                    )
                    if stat.S_ISDIR(status.st_mode):
                        descriptor = os.open(
                            entry_name,
                            os.O_RDONLY
                            | os.O_CLOEXEC
                            | os.O_DIRECTORY
                            | os.O_NOFOLLOW,
                            dir_fd=directory_fd,
                        )
                        opened = os.fstat(descriptor)
                        if (
                            changed(status, opened)
                            or opened.st_dev != root_status.st_dev
                            or _descriptor_mount_id(descriptor) != root_mount
                        ):
                            raise WorkspaceError(
                                "immutable source generation changed while "
                                f"reading: {display}"
                            )
                        value = (b"040000", b"tree", b"")
                        visit(descriptor, relative)
                        if changed(opened, os.fstat(descriptor)):
                            raise WorkspaceError(
                                "immutable source generation changed while "
                                f"reading: {display}"
                            )
                    elif stat.S_ISREG(status.st_mode):
                        if status.st_nlink != 1:
                            raise WorkspaceError(
                                "generated source contains a hard-linked file: "
                                f"{display}"
                            )
                        descriptor = os.open(
                            entry_name,
                            os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW,
                            dir_fd=directory_fd,
                        )
                        opened = os.fstat(descriptor)
                        if (
                            changed(status, opened)
                            or not stat.S_ISREG(opened.st_mode)
                            or opened.st_nlink != 1
                            or opened.st_dev != root_status.st_dev
                            or _descriptor_mount_id(descriptor) != root_mount
                        ):
                            raise WorkspaceError(
                                "immutable source generation changed while "
                                f"reading: {display}"
                            )
                        digest = algorithm()
                        digest.update(f"blob {opened.st_size}\0".encode())
                        observed = 0
                        while chunk := os.read(descriptor, 1024 * 1024):
                            observed += len(chunk)
                            digest.update(chunk)
                        if (
                            observed != opened.st_size
                            or changed(opened, os.fstat(descriptor))
                        ):
                            raise WorkspaceError(
                                "immutable source generation changed while "
                                f"reading: {display}"
                            )
                        mode = b"100755" if opened.st_mode & 0o111 else b"100644"
                        value = (mode, b"blob", digest.hexdigest().encode())
                    elif stat.S_ISLNK(status.st_mode):
                        target = os.fsencode(
                            os.readlink(entry_name, dir_fd=directory_fd)
                        )
                        after = os.stat(
                            entry_name,
                            dir_fd=directory_fd,
                            follow_symlinks=False,
                        )
                        if changed(status, after):
                            raise WorkspaceError(
                                "immutable source generation changed while "
                                f"reading: {display}"
                            )
                        value = (b"120000", b"blob", blob_id(target))
                    else:
                        raise WorkspaceError(
                            "immutable source generation contains an unsupported "
                            f"entry: {display}"
                        )
                except WorkspaceError:
                    raise
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot inspect immutable source generation {display}: {error}"
                    ) from error
                finally:
                    if descriptor is not None:
                        os.close(descriptor)
                if relative in actual:
                    raise WorkspaceError(
                        "immutable source generation repeats a path: "
                        f"{os.fsdecode(relative)}"
                    )
                actual[relative] = value

            if changed(directory_before, os.fstat(directory_fd)):
                raise WorkspaceError(
                    "immutable source generation changed while reading: "
                    f"{source / os.fsdecode(prefix)}"
                )

        container_fd: int | None = None
        parent_fd: int | None = None
        root_fd: int | None = None
        try:
            flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
            container_fd = _open_directory_nofollow(source.parent.parent, flags)
            container_status = os.fstat(container_fd)
            container_mount = _descriptor_mount_id(container_fd)
            visible_parent = os.stat(
                source.parent.name,
                dir_fd=container_fd,
                follow_symlinks=False,
            )
            parent_fd = os.open(
                source.parent.name,
                flags,
                dir_fd=container_fd,
            )
            parent_status = os.fstat(parent_fd)
            parent_mount = _descriptor_mount_id(parent_fd)
            if (
                changed(visible_parent, parent_status)
                or parent_status.st_dev != container_status.st_dev
                or parent_mount != container_mount
            ):
                raise WorkspaceError(
                    "immutable source generation parent changed or is mounted: "
                    f"{source.parent}"
                )
            visible_root = os.stat(
                source.name,
                dir_fd=parent_fd,
                follow_symlinks=False,
            )
            root_fd = os.open(source.name, flags, dir_fd=parent_fd)
            root_status = os.fstat(root_fd)
            root_mount = _descriptor_mount_id(root_fd)
            if (
                changed(visible_root, root_status)
                or root_status.st_dev != parent_status.st_dev
                or root_mount != parent_mount
            ):
                raise WorkspaceError(
                    "immutable source generation root changed or is mounted: "
                    f"{source}"
                )
            visit(root_fd, b"")
            first_inventory = dict(actual)
            actual.clear()
            visit(root_fd, b"")
            if actual != first_inventory:
                raise WorkspaceError(
                    "immutable source generation changed between inventories: "
                    f"{source}"
                )
            if set(actual) != set(expected) or any(
                actual[path][:2] != expected[path][:2]
                or (
                    actual[path][1] != b"tree"
                    and actual[path][2] != expected[path][2]
                )
                for path in actual.keys() & expected.keys()
            ):
                raise WorkspaceError(
                    "immutable source generation does not match its recorded "
                    f"Git tree: {source}"
                )
            visible_after = os.stat(
                source.name,
                dir_fd=parent_fd,
                follow_symlinks=False,
            )
            visible_parent_after = os.stat(
                source.parent.name,
                dir_fd=container_fd,
                follow_symlinks=False,
            )
            if (
                changed(root_status, os.fstat(root_fd))
                or changed(root_status, visible_after)
                or changed(parent_status, os.fstat(parent_fd))
                or changed(parent_status, visible_parent_after)
            ):
                raise WorkspaceError(
                    f"immutable source generation changed while reading: {source}"
                )
        except WorkspaceError:
            raise
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect immutable source generation {source}: {error}"
            ) from error
        finally:
            if root_fd is not None:
                os.close(root_fd)
            if parent_fd is not None:
                os.close(parent_fd)
            if container_fd is not None:
                os.close(container_fd)

    @staticmethod
    def _validate_source_generation_git_closure(
        checkout: Path,
        generation: Path,
        source_tree: str,
        root_tree: str,
        source_includes: dict[str, str],
    ) -> None:
        """Prove every materialized closure input has its recorded Git identity."""

        Workspace._validate_source_generation_git_tree(
            checkout, generation / "source", source_tree
        )
        algorithm = hashlib.sha1 if len(root_tree) == 40 else hashlib.sha256
        for include, expected_object in sorted(source_includes.items()):
            try:
                result = subprocess.run(
                    [
                        "git",
                        "--no-replace-objects",
                        "-C",
                        str(checkout),
                        "ls-tree",
                        "-z",
                        root_tree,
                        "--",
                        include,
                    ],
                    check=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    pass_fds=active_lock_fds(),
                )
            except FileNotFoundError as error:
                raise WorkspaceError("required command not found: git") from error
            except subprocess.CalledProcessError as error:
                detail = error.stderr.decode("utf-8", errors="replace").strip()
                suffix = f": {detail}" if detail else ""
                raise WorkspaceError(
                    "cannot inspect recorded immutable Git source include" + suffix
                ) from error
            entries = [entry for entry in result.stdout.split(b"\0") if entry]
            try:
                metadata, relative = entries[0].split(b"\t", 1)
                mode, kind, object_id = metadata.split(b" ", 2)
            except (IndexError, ValueError) as error:
                raise WorkspaceError(
                    "recorded immutable Git source include is invalid"
                ) from error
            if (
                len(entries) != 1
                or os.fsdecode(relative) != include
                or object_id.decode("ascii", errors="replace") != expected_object
                or len(object_id) not in {40, 64}
                or not re.fullmatch(b"[0-9a-f]+", object_id)
            ):
                raise WorkspaceError(
                    "recorded immutable Git source include is invalid"
                )

            include_path = generation.joinpath(*PurePosixPath(include).parts)
            try:
                include_status = include_path.lstat()
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect immutable source include {include_path}: {error}"
                ) from error
            if stat.S_ISDIR(include_status.st_mode):
                if mode != b"040000" or kind != b"tree":
                    raise WorkspaceError(
                        "immutable source include does not match its recorded "
                        f"Git entry: {include_path}"
                    )
                Workspace._validate_source_generation_git_tree(
                    checkout, include_path, expected_object
                )
                continue
            if (
                not stat.S_ISREG(include_status.st_mode)
                or kind != b"blob"
                or mode not in {b"100644", b"100755"}
            ):
                raise WorkspaceError(
                    "immutable source include does not match its recorded "
                    f"Git entry: {include_path}"
                )

            parent_fd: int | None = None
            descriptor: int | None = None
            try:
                flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
                parent_fd = _open_directory_nofollow(include_path.parent, flags)
                visible = os.stat(
                    include_path.name,
                    dir_fd=parent_fd,
                    follow_symlinks=False,
                )
                descriptor = os.open(
                    include_path.name,
                    os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW,
                    dir_fd=parent_fd,
                )
                opened = os.fstat(descriptor)
                def file_identity(value: os.stat_result) -> tuple[int, ...]:
                    return (
                        value.st_dev,
                        value.st_ino,
                        value.st_mode,
                        value.st_nlink,
                        value.st_size,
                        value.st_mtime_ns,
                        value.st_ctime_ns,
                    )

                if (
                    file_identity(visible) != file_identity(opened)
                    or not stat.S_ISREG(opened.st_mode)
                    or opened.st_nlink != 1
                    or _descriptor_mount_id(descriptor)
                    != _descriptor_mount_id(parent_fd)
                ):
                    raise WorkspaceError(
                        f"immutable source include changed while reading: {include_path}"
                    )
                digest = algorithm()
                digest.update(f"blob {opened.st_size}\0".encode())
                observed = 0
                while chunk := os.read(descriptor, 1024 * 1024):
                    observed += len(chunk)
                    digest.update(chunk)
                after = os.fstat(descriptor)
                actual_mode = b"100755" if opened.st_mode & 0o111 else b"100644"
                if (
                    observed != opened.st_size
                    or file_identity(opened) != file_identity(after)
                    or actual_mode != mode
                    or digest.hexdigest().encode() != object_id
                ):
                    raise WorkspaceError(
                        "immutable source include does not match its recorded "
                        f"Git entry: {include_path}"
                    )
                visible_after = os.stat(
                    include_path.name,
                    dir_fd=parent_fd,
                    follow_symlinks=False,
                )
                if file_identity(visible) != file_identity(visible_after):
                    raise WorkspaceError(
                        f"immutable source include changed while reading: {include_path}"
                    )
            except WorkspaceError:
                raise
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect immutable source include {include_path}: {error}"
                ) from error
            finally:
                if descriptor is not None:
                    os.close(descriptor)
                if parent_fd is not None:
                    os.close(parent_fd)

    def _materialize_primary_source(
        self,
        component: Component,
        checkout: Path,
        source: Path,
        state: dict[str, Any],
    ) -> Path:
        checkout_identity = checkout.stat()
        source_identity = source.stat()
        git_common = self._git_common_directory(checkout, trace=False)
        git_common_identity = git_common.stat()
        expected_source_identity = state.get("sources", {}).get(component.source)
        if (
            (checkout_identity.st_dev, checkout_identity.st_ino)
            != (state.get("device"), state.get("inode"))
            or str(git_common) != state.get("git_common")
            or (git_common_identity.st_dev, git_common_identity.st_ino)
            != (
                state.get("git_common_device"),
                state.get("git_common_inode"),
            )
            or expected_source_identity
            != {
                "path": str(source.resolve()),
                "device": source_identity.st_dev,
                "inode": source_identity.st_ino,
            }
        ):
            raise WorkspaceError(
                f"clean primary source identity changed before materialization: {checkout}"
            )
        commit = state["head"]
        tree = git(
            checkout,
            "--no-replace-objects",
            "rev-parse",
            f"{commit}^{{tree}}",
            capture=True,
            trace=False,
        )
        source_tree = git(
            checkout,
            "--no-replace-objects",
            "rev-parse",
            (
                f"{commit}^{{tree}}"
                if component.source == "."
                else f"{commit}:{component.source}"
            ),
            capture=True,
            trace=False,
        )
        source_includes: dict[str, str] = {}
        source_include_entries: dict[str, tuple[bytes, bytes, bytes]] = {}
        for include in component.source_includes:
            listing = git(
                checkout,
                "--no-replace-objects",
                "ls-tree",
                tree,
                "--",
                include,
                capture=True,
                trace=False,
            )
            try:
                metadata, listed_path = listing.split("\t", 1)
                include_mode, include_kind, include_object = metadata.split(" ", 2)
            except ValueError as error:
                raise WorkspaceError(
                    f"recorded immutable Git source include is invalid: {include}"
                ) from error
            if (
                listed_path != include
                or include_mode not in {"040000", "100644", "100755"}
                or include_kind not in {"tree", "blob"}
                or not re.fullmatch(r"[0-9a-f]{40,64}", include_object)
            ):
                raise WorkspaceError(
                    f"recorded immutable Git source include is invalid: {include}"
                )
            source_includes[include] = include_object
            source_include_entries[include] = (
                include_mode.encode(),
                include_kind.encode(),
                include_object.encode(),
            )
        identity = {
            "schema_version": SOURCE_GENERATION_SCHEMA_VERSION,
            "repository": component.repository,
            "checkout": component.checkout_name,
            "branch": component.branch,
            "commit": commit,
            "tree": tree,
            "source": component.source,
            "source_tree": source_tree,
            "source_includes": source_includes,
        }
        key = hashlib.sha256(
            json.dumps(identity, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        container = self.paths.builds / "source-generations" / component.checkout_name
        container_lock = (
            self.paths.builds
            / "locks"
            / f"source-generation-container-{component.checkout_name}.lock"
        )
        with exclusive_lock(
            container_lock,
            f"immutable source generation container {component.checkout_name}",
        ):
            managed_directory(
                container,
                self.paths.builds,
                f"source-generations:{component.checkout_name}",
            )
        generation = container / key
        lock = self.paths.builds / "locks" / f"source-generation-{key}.lock"
        with exclusive_lock(lock, f"immutable source generation {component.name}"):
            if generation.exists() or generation.is_symlink():
                if generation.is_symlink() or not generation.is_dir():
                    raise WorkspaceError(
                        f"immutable source generation is invalid: {generation}"
                    )
                marker = generation / MANAGED_MARKER
                record = self._source_generation_record(generation / "source")
                expected_record = {
                    **identity,
                    "source_tree_sha256": _tree_digest(
                        generation / "source",
                        set(),
                        bounded_symlinks=True,
                        reject_hardlinks=True,
                    ),
                    "closure_tree_sha256": _source_closure_digest(
                        generation, component.source_includes
                    ),
                }
                if (
                    not marker.is_file()
                    or marker.is_symlink()
                    or load_json(marker)
                    != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"source-generation:{key}",
                    }
                    or record != expected_record
                ):
                    raise WorkspaceError(
                        f"immutable source generation is corrupt: {generation}"
                    )
                current_git_common = self._git_common_directory(
                    checkout, trace=False
                )
                current_checkout = checkout.stat()
                current_source = source.stat()
                current_git_common_identity = current_git_common.stat()
                if (
                    (current_checkout.st_dev, current_checkout.st_ino)
                    != (state["device"], state["inode"])
                    or str(current_git_common) != state["git_common"]
                    or (
                        current_git_common_identity.st_dev,
                        current_git_common_identity.st_ino,
                    )
                    != (
                        state["git_common_device"],
                        state["git_common_inode"],
                    )
                    or state["sources"][component.source]
                    != {
                        "path": str(source.resolve()),
                        "device": current_source.st_dev,
                        "inode": current_source.st_ino,
                    }
                    or not _is_clean(checkout, trace=False)
                    or git(
                        checkout,
                        "rev-parse",
                        "HEAD",
                        capture=True,
                        trace=False,
                    )
                    != commit
                ):
                    raise WorkspaceError(
                        f"clean primary source changed before generation reuse: {checkout}"
                    )
                self._validate_source_generation_git_closure(
                    checkout,
                    generation,
                    source_tree,
                    tree,
                    source_includes,
                )
                return generation / "source"

            staging = Path(
                tempfile.mkdtemp(prefix=f"{key}-staging-", dir=container)
            )
            try:
                atomic_json(
                    staging / MANAGED_MARKER,
                    {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"source-generation:{key}",
                    },
                )
                exports: list[
                    tuple[
                        str,
                        str | None,
                        Path,
                        bool,
                        str,
                        str | None,
                        bytes,
                        bytes,
                    ]
                ] = [
                    (
                        source_tree,
                        None,
                        staging / "source",
                        False,
                        source_tree,
                        None,
                        b"040000",
                        b"tree",
                    ),
                    *(
                        (
                            tree,
                            include,
                            staging,
                            True,
                            source_includes[include],
                            include,
                            source_include_entries[include][0],
                            source_include_entries[include][1],
                        )
                        for include in component.source_includes
                    ),
                ]
                for export_index, (
                    archive_ref,
                    archive_pathspec,
                    destination,
                    existing_output,
                    archive_object,
                    archive_prefix,
                    archive_mode,
                    archive_kind,
                ) in enumerate(exports):
                    archive_descriptor, archive_name = tempfile.mkstemp(
                        prefix=f"atrinik-source-{export_index}-", suffix=".tar"
                    )
                    archive_path = Path(archive_name)
                    try:
                        try:
                            archive_command = [
                                "git",
                                "--no-replace-objects",
                                "-C",
                                str(checkout),
                                "archive",
                                "--format=tar",
                                archive_ref,
                            ]
                            if archive_pathspec is not None:
                                archive_command.extend(["--", archive_pathspec])
                            subprocess.run(
                                archive_command,
                                check=True,
                                stdout=archive_descriptor,
                                stderr=subprocess.PIPE,
                                pass_fds=active_lock_fds(),
                            )
                        except FileNotFoundError as error:
                            raise WorkspaceError(
                                "required command not found: git"
                            ) from error
                        except subprocess.CalledProcessError as error:
                            detail = error.stderr.decode(
                                "utf-8", errors="replace"
                            ).strip()
                            suffix = f": {detail}" if detail else ""
                            raise WorkspaceError(
                                f"cannot export immutable source generation{suffix}"
                            ) from error
                        self._complete_git_source_archive(
                            checkout,
                            archive_descriptor,
                            archive_object,
                            archive_prefix,
                            archive_mode,
                            archive_kind,
                        )
                        self._extract_git_source_archive(
                            archive_descriptor,
                            destination,
                            existing_output=existing_output,
                        )
                    finally:
                        os.close(archive_descriptor)
                        archive_path.unlink(missing_ok=True)
                current_checkout = checkout.stat()
                current_source = source.stat()
                current_git_common = self._git_common_directory(
                    checkout, trace=False
                )
                current_git_common_identity = current_git_common.stat()
                if (
                    (checkout_identity.st_dev, checkout_identity.st_ino)
                    != (current_checkout.st_dev, current_checkout.st_ino)
                    or (source_identity.st_dev, source_identity.st_ino)
                    != (current_source.st_dev, current_source.st_ino)
                    or str(current_git_common) != state["git_common"]
                    or (
                        git_common_identity.st_dev,
                        git_common_identity.st_ino,
                    )
                    != (
                        current_git_common_identity.st_dev,
                        current_git_common_identity.st_ino,
                    )
                    or not _is_clean(checkout, trace=False)
                    or git(checkout, "rev-parse", "HEAD", capture=True, trace=False)
                    != commit
                    or git(
                        checkout,
                        "--no-replace-objects",
                        "rev-parse",
                        f"{commit}^{{tree}}",
                        capture=True,
                        trace=False,
                    )
                    != tree
                ):
                    raise WorkspaceError(
                        f"clean primary source changed during materialization: {checkout}"
                    )
                self._validate_source_generation_git_closure(
                    checkout,
                    staging,
                    source_tree,
                    tree,
                    source_includes,
                )
                self._seal_runtime_generation(staging / "source")
                for include in component.source_includes:
                    include_path = staging.joinpath(
                        *PurePosixPath(include).parts
                    )
                    include_status = include_path.lstat()
                    if stat.S_ISDIR(include_status.st_mode):
                        self._seal_runtime_generation(include_path)
                    elif stat.S_ISREG(include_status.st_mode):
                        include_path.chmod(
                            stat.S_IMODE(include_status.st_mode) & ~0o222
                        )
                    else:
                        raise WorkspaceError(
                            "generated source include is not a regular file or "
                            f"directory: {include_path}"
                        )
                record = {
                    **identity,
                    "source_tree_sha256": _tree_digest(
                        staging / "source",
                        set(),
                        bounded_symlinks=True,
                        reject_hardlinks=True,
                    ),
                    "closure_tree_sha256": _source_closure_digest(
                        staging, component.source_includes
                    ),
                }
                durable_atomic_json(staging / SOURCE_GENERATION_METADATA, record)
                self._seal_runtime_generation(staging)
                rename_no_replace(staging, generation)
            except BaseException:
                if staging.exists() and not staging.is_symlink():
                    remove_owned_tree(staging)
                raise
        return generation / "source"

    def _materialize_clean_primary_sources(
        self,
        profile: dict[str, Any],
        selected: dict[str, Path],
        states: dict[str, dict[str, Any]],
    ) -> tuple[dict[str, Path], set[str], dict[Path, dict[str, Any]]]:
        stack = self.manifest.stack(profile["stack"])
        materialized = dict(selected)
        checkout_results: dict[tuple[str, str, tuple[str, ...]], Path] = {}
        for role in sorted(selected):
            component = stack.providers[role]
            selector = profile["components"][component.name]
            checkout = self._selector_root(profile, component).resolve()
            state = states[component.checkout_name]
            # The authored content publisher currently proves its Classic
            # target against a live Git checkout. Keep that exact source lease
            # until the publisher accepts an immutable-generation identity.
            if (
                role == "content"
                or selector["kind"] != "primary"
                or state["dirty"]
            ):
                continue
            cache_key = (
                component.checkout_name,
                component.source,
                component.source_includes,
            )
            generated = checkout_results.get(cache_key)
            if generated is None:
                generated = self._materialize_primary_source(
                    component, checkout, selected[role], state
                )
                checkout_results[cache_key] = generated
            materialized[role] = generated
        released: set[str] = set()
        for checkout_name in {
            stack.providers[role].checkout_name for role in selected
        }:
            checkout_roles = [
                role
                for role in selected
                if stack.providers[role].checkout_name == checkout_name
            ]
            if all(materialized[role] != selected[role] for role in checkout_roles):
                component = stack.providers[checkout_roles[0]]
                checkout = self._selector_root(profile, component).resolve()
                released.add(self._source_coordinate(checkout_name, checkout))
        generation_records: dict[Path, dict[str, Any]] = {}
        for role, path in materialized.items():
            if path == selected[role] or path in generation_records:
                continue
            try:
                record = self._source_generation_record(path)
            except (OSError, WorkspaceError) as error:
                raise WorkspaceError(
                    "immutable source generation changed before lease handoff: "
                    f"{path}"
                ) from error
            if record is None:
                raise WorkspaceError(
                    "immutable source generation changed before lease handoff: "
                    f"{path}"
                )
            generation_records[path] = record
        return materialized, released, generation_records

    @contextmanager
    def _resolved_profile_operation(
        self,
        profile_name: str,
        required: set[str],
        operation: str,
        *,
        materialize_clean_primaries: bool = False,
    ) -> Iterator[ProfileResolutionSnapshot]:
        """Capture and retain one exact profile/source resolution."""

        profile_request = self._lease_request(
            "profile", profile_name, "shared", operation
        )
        with self._resource_locks([profile_request]):
            profile = self._load_profile_file(profile_name, require_file=False)
            stack = self.manifest.stack(profile["stack"])
            roles = self._dependency_roles(profile, required)
            components = {stack.providers[role].name for role in roles}
            source_requests: list[LeaseRequest] = []
            seen: set[str] = set()
            for component in stack.components:
                if component.name not in components:
                    continue
                root = self._selector_root(profile, component)
                coordinate = self._source_coordinate(component.checkout_name, root)
                if coordinate in seen:
                    continue
                seen.add(coordinate)
                source_requests.append(
                    self._lease_request("source", coordinate, "shared", operation)
                )
            source_contexts: list[
                tuple[str, AbstractContextManager[tuple[TextIO, ...]]]
            ] = []
            generation_contexts: list[AbstractContextManager[TextIO]] = []
            try:
                ordered_requests = sorted(
                    source_requests, key=lambda item: item.sort_key
                )
                while True:
                    waiting: LeaseRequest | None = None
                    try:
                        for request in ordered_requests:
                            context = self._resource_locks(
                                [request], nonblocking=True
                            )
                            context.__enter__()
                            source_contexts.append((request.coordinate, context))
                    except LockBusyError:
                        waiting = request
                    if waiting is None:
                        break
                    for _coordinate, context in reversed(source_contexts):
                        context.__exit__(None, None, None)
                    source_contexts = []
                    with self._resource_locks([waiting]):
                        pass
                confirmed_profile = self._load_profile_file(
                    profile_name, require_file=False
                )
                if confirmed_profile != profile:
                    raise WorkspaceError(
                        f"profile {profile_name} changed while its exact sources were being locked; retry"
                    )
                selected = self._resolve_build_profile(
                    profile_name, required, trace=False, profile=confirmed_profile
                )
                states = self._selected_checkout_states(
                    profile, selected, include_dirty=True, include_identity=True
                )
                released: set[str] = set()
                if materialize_clean_primaries:
                    (
                        selected,
                        released,
                        generation_records,
                    ) = self._materialize_clean_primary_sources(
                        confirmed_profile, selected, states
                    )
                    generation_keys = sorted(
                        {
                            path.parent.name
                            for path in generation_records
                        }
                    )
                    for key in generation_keys:
                        context = shared_lock(
                            self.paths.builds
                            / "locks"
                            / f"source-generation-{key}.lock",
                            f"immutable source generation {key}",
                        )
                        context.__enter__()
                        generation_contexts.append(context)
                    for path, expected_record in generation_records.items():
                        try:
                            current_record = self._source_generation_record(path)
                            current_digest = _tree_digest(
                                path,
                                set(),
                                bounded_symlinks=True,
                                reject_hardlinks=True,
                            )
                            current_closure_digest = _source_closure_digest(
                                path.parent,
                                current_record.get("source_includes", {})
                                if current_record is not None
                                else (),
                            )
                        except (OSError, WorkspaceError) as error:
                            raise WorkspaceError(
                                "immutable source generation changed before lease handoff: "
                                f"{path}"
                            ) from error
                        if (
                            current_record != expected_record
                            or current_digest
                            != expected_record["source_tree_sha256"]
                            or current_closure_digest
                            != expected_record["closure_tree_sha256"]
                        ):
                            raise WorkspaceError(
                                "immutable source generation changed before lease handoff: "
                                f"{path}"
                            )
                        checkout = states[expected_record["checkout"]]["path"]
                        self._validate_source_generation_git_closure(
                            checkout,
                            path.parent,
                            expected_record["source_tree"],
                            expected_record["tree"],
                            expected_record["source_includes"],
                        )
                    retained = []
                    for coordinate, context in reversed(source_contexts):
                        if coordinate in released:
                            context.__exit__(None, None, None)
                        else:
                            retained.append((coordinate, context))
                    source_contexts = list(reversed(retained))
                serializable_states = {
                    name: {**state, "path": str(state["path"])}
                    for name, state in states.items()
                }
                profile_json = json.dumps(
                    profile, sort_keys=True, separators=(",", ":")
                )
                selected_rows = tuple(
                    (role, str(path.resolve()))
                    for role, path in sorted(selected.items())
                )
                state_json = json.dumps(
                    serializable_states, sort_keys=True, separators=(",", ":")
                )
                generation = hashlib.sha256(
                    f"{profile_json}\0{selected_rows!r}\0{state_json}".encode()
                ).hexdigest()
                snapshot = ProfileResolutionSnapshot(
                    profile_name,
                    generation,
                    profile_json,
                    selected_rows,
                    state_json,
                )
                previous = self._profile_snapshot
                self._profile_snapshot = snapshot
                try:
                    yield snapshot
                finally:
                    self._profile_snapshot = previous
            finally:
                for context in reversed(generation_contexts):
                    context.__exit__(None, None, None)
                for _coordinate, context in reversed(source_contexts):
                    context.__exit__(None, None, None)

    def migrate_repositories(self, mode: str) -> dict[str, Any]:
        if mode == "apply":
            self.paths.ensure()
        result = RepositoryMigration(
            self.paths.repository,
            self.paths,
            self.manifest,
            self._lease_namespace / "repository-layout.lock",
            self._publish_migration_profile_references,
        ).execute(mode)
        return result

    def migrate_content(self, mode: str) -> dict[str, Any]:
        if mode in {"apply", "restore"}:
            self.paths.ensure()
        result = ContentMigration(
            self.paths.repository,
            self.paths,
            self.manifest,
            self._lease_namespace / "repository-layout.lock",
            self._publish_migration_profile_references,
        ).execute(mode)
        return result

    def cleanup(
        self,
        scopes: list[str],
        older_than_days: int,
        names: list[str],
        apply: bool,
    ) -> dict[str, Any]:
        # Import lazily so the planner can reuse the workspace lock and metadata
        # helpers without creating a module import cycle.
        from .cleanup import Cleanup

        if not apply:
            return Cleanup(self).execute(scopes, older_than_days, names, False)
        with shared_maintenance_lock(
            self._lease_namespace / "repository-layout.lock"
        ):
            return Cleanup(self).execute(scopes, older_than_days, names, True)

    def _checkout_identity(self, value: Checkout | Component) -> Checkout:
        if isinstance(value, Checkout):
            return value
        return self.manifest.checkout_for(value)

    def _primary_path(self, value: Checkout | Component) -> Path:
        checkout = self._checkout_identity(value)
        return self.paths.repositories / checkout.path

    def _operation_checkouts(
        self,
        names: list[str] | None,
        include_classic: bool,
    ) -> list[Checkout]:
        requested: set[str] = set()
        if names:
            unknown: list[str] = []
            for name in names:
                if name in self.manifest.by_checkout:
                    requested.add(name)
                elif name in self.manifest.by_name:
                    requested.add(self.manifest.by_name[name].checkout_name)
                else:
                    unknown.append(name)
            if unknown:
                raise WorkspaceError(
                    f"unknown components or checkouts: {', '.join(sorted(set(unknown)))}"
                )
        else:
            requested.update(self.manifest.cohorts["default"])
        if include_classic:
            requested.update(self.manifest.cohorts["default"])
            requested.update(self.manifest.cohorts["classic"])
        return [
            checkout
            for checkout in self.manifest.checkouts
            if checkout.name in requested
        ]

    def initialize(
        self,
        names: list[str] | None = None,
        jobs: int = 4,
        *,
        include_classic: bool = False,
    ) -> None:
        self.paths.ensure()
        checkouts = self._operation_checkouts(names, include_classic)
        failures: list[str] = []
        # Validate every occupied destination before starting any clone. This
        # preserves all-or-nothing preflight while the actual operations use
        # disjoint checkout leases and may overlap.
        for checkout in checkouts:
            destination = self._primary_path(checkout)
            if destination.exists() or destination.is_symlink():
                self._validate_primary_checkout(checkout, destination)

        def initialize_checkout(checkout: Checkout) -> Path:
            destination = self._primary_path(checkout)
            requests = [
                self._lease_request(
                    "git-admin",
                    self._git_admin_coordinate(checkout, destination),
                    "exclusive",
                    f"initialize {checkout.name}",
                ),
                self._lease_request(
                    "source",
                    self._source_coordinate(checkout.name, destination),
                    "exclusive",
                    f"initialize {checkout.name}",
                ),
            ]
            with self._resource_locks(requests):
                return self._ensure_repository(checkout)

        with ThreadPoolExecutor(
            max_workers=max(1, min(jobs, len(checkouts)))
        ) as executor:
            futures = {
                executor.submit(copy_context().run, initialize_checkout, checkout): checkout
                for checkout in checkouts
            }
            for future in as_completed(futures):
                checkout = futures[future]
                try:
                    future.result()
                    print(f"{checkout.name}: ready")
                except Exception as error:
                    failures.append(f"{checkout.name}: {error}")
        if failures:
            raise WorkspaceError(
                "repository initialization failed:\n" + "\n".join(sorted(failures))
            )

    def _ensure_repository(self, value: Checkout | Component) -> Path:
        checkout = self._checkout_identity(value)
        destination = self._primary_path(checkout)
        if not destination.exists() and not destination.is_symlink():
            temporary = Path(
                tempfile.mkdtemp(
                    prefix=f".atrinik-clone-{checkout.name}-",
                    dir=self.paths.repositories,
                )
            )
            try:
                run(
                    [
                        "git",
                        "clone",
                        "--branch",
                        checkout.branch,
                        "--single-branch",
                        "--",
                        self._component_clone_url(checkout),
                        str(temporary),
                    ]
                )
                self._validate_primary_checkout(checkout, temporary)
                if destination.exists() or destination.is_symlink():
                    raise WorkspaceError(
                        f"component destination appeared during clone: {destination}"
                    )
                rename_no_replace(temporary, destination)
            except BaseException:
                shutil.rmtree(temporary, ignore_errors=True)
                raise
        self._validate_primary_checkout(checkout, destination)
        return destination

    def _validate_primary_checkout(
        self, value: Checkout | Component, path: Path, *, trace: bool = True
    ) -> str:
        checkout = self._checkout_identity(value)
        remote = self._validate_checkout(checkout, path, trace=trace)
        branch = git(
            path, "branch", "--show-current", capture=True, trace=trace
        )
        if branch != checkout.branch:
            raise WorkspaceError(
                f"primary checkout must be on {checkout.branch}, found "
                f"{branch or 'detached'}: {path}"
            )
        return remote

    def _component_clone_url(self, value: Checkout | Component) -> str:
        checkout = self._checkout_identity(value)
        for remote in ("origin", "upstream"):
            try:
                urls = git(
                    self.paths.repository,
                    "remote",
                    "get-url",
                    "--all",
                    remote,
                    capture=True,
                    trace=False,
                ).splitlines()
            except WorkspaceError:
                continue
            for url in urls:
                clone_url = _github_clone_url(url, checkout.repository)
                if clone_url is not None:
                    return clone_url
        return f"https://github.com/{checkout.repository}.git"

    def _canonical_remote(
        self, value: Checkout | Component, path: Path, *, trace: bool = True
    ) -> str:
        checkout = self._checkout_identity(value)
        for remote in ("origin", "upstream"):
            try:
                urls = git(
                    path,
                    "remote",
                    "get-url",
                    "--all",
                    remote,
                    capture=True,
                    trace=trace,
                ).splitlines()
            except WorkspaceError:
                continue
            # Git fetches the first URL when a remote has multiple fetch URLs.
            # Do not accept a later canonical-looking URL while fetching a fork
            # or unrelated repository from the effective first URL.
            if urls and _remote_matches(urls[0], checkout.repository):
                return remote
        raise WorkspaceError(
            f"checkout has no origin/upstream for {checkout.repository}: {path}"
        )

    def _validate_checkout(
        self, value: Checkout | Component, path: Path, *, trace: bool = True
    ) -> str:
        checkout = self._checkout_identity(value)
        if path.is_symlink() or not path.is_dir():
            raise WorkspaceError(f"component checkout is not a directory: {path}")
        try:
            inside = git(
                path,
                "rev-parse",
                "--is-inside-work-tree",
                capture=True,
                trace=trace,
            )
        except WorkspaceError as error:
            raise WorkspaceError(f"component is not a Git checkout: {path}") from error
        if inside != "true":
            raise WorkspaceError(f"component is not a Git worktree: {path}")
        top_level = Path(
            git(
                path,
                "rev-parse",
                "--show-toplevel",
                capture=True,
                trace=trace,
            )
        ).resolve()
        if top_level != path.resolve():
            raise WorkspaceError(f"component path must be the Git worktree root: {path}")
        self._validate_checkout_lineage(checkout, path)
        try:
            return self._canonical_remote(checkout, path, trace=trace)
        except WorkspaceError as error:
            historical_name = PRE_MONOREPO_REPOSITORIES.get(checkout.name)
            if historical_name is not None:
                historical_repository = f"atrinik/{historical_name}"
                for remote in ("origin", "upstream"):
                    try:
                        urls = git(
                            path,
                            "remote",
                            "get-url",
                            "--all",
                            remote,
                            capture=True,
                            trace=trace,
                        ).splitlines()
                    except WorkspaceError:
                        continue
                    if urls and _remote_matches(urls[0], historical_repository):
                        raise WorkspaceError(
                            f"replacement path contains pre-monorepo classic history: "
                            f"{path}; run ./atrinik migrate repositories --dry-run"
                        ) from error
            raise

    def _validate_checkout_lineage(self, checkout: Checkout, path: Path) -> None:
        old_name = checkout.name if checkout.name in PRE_MONOREPO_REPOSITORIES else None
        if old_name is None:
            return
        try:
            actual_classic = classic_lineage(path, old_name)
        except WorkspaceError as error:
            raise WorkspaceError(
                f"cannot prove repository history for {checkout.name}: {path}: {error}"
            ) from error
        if actual_classic:
            raise WorkspaceError(
                f"replacement path contains pre-monorepo classic history: {path}; "
                "run ./atrinik migrate repositories --dry-run"
            )

    def repository_status(self, names: list[str] | None = None) -> list[dict[str, Any]]:
        """Return quiet, machine-readable primary-checkout status."""
        self.paths.ensure()
        rows: list[dict[str, Any]] = []
        checkouts = (
            self._operation_checkouts(names, False)
            if names
            else self.manifest.checkouts
        )
        for checkout in checkouts:
            modules = [
                component
                for component in self.manifest.components
                if component.checkout_name == checkout.name
            ]
            path = self._primary_path(checkout)
            row: dict[str, Any] = {
                "component": checkout.name,
                "checkout": checkout.name,
                "repository": checkout.repository,
                "default_branch": checkout.branch,
                "destination": checkout.path,
                "cohorts": sorted(self.manifest.checkout_cohorts(checkout.name)),
                "stacks": sorted(self.manifest.checkout_stacks(checkout.name)),
                "modules": [component.name for component in modules],
                "roles": sorted(
                    {role for component in modules for role in component.provides}
                ),
                "license": checkout.license,
                "optional": checkout.name not in self.manifest.cohorts["default"],
                "path": str(path),
                "initialized": False,
                "branch": None,
                "head": None,
                "dirty": None,
                "remote": None,
                "ahead": None,
                "behind": None,
            }
            if not path.exists() and not path.is_symlink():
                rows.append(row)
                continue
            remote = self._validate_primary_checkout(checkout, path, trace=False)
            row.update(
                {
                    "initialized": True,
                    "branch": git(
                        path, "branch", "--show-current", capture=True, trace=False
                    )
                    or None,
                    "head": git(
                        path,
                        "rev-parse",
                        "--short=12",
                        "HEAD",
                        capture=True,
                        trace=False,
                    ),
                    "dirty": not _is_clean(path, trace=False),
                    "remote": remote,
                }
            )
            try:
                counts = git(
                    path,
                    "rev-list",
                    "--left-right",
                    "--count",
                    f"HEAD...{remote}/{checkout.branch}",
                    capture=True,
                    trace=False,
                ).split()
                if len(counts) == 2:
                    row["ahead"], row["behind"] = (int(value) for value in counts)
            except (ValueError, WorkspaceError):
                # A newly created or deliberately minimal checkout may not yet
                # have a cached remote default-branch ref.
                pass
            rows.append(row)
        return rows

    def sync(
        self,
        names: list[str] | None,
        worktree_strategy: str,
        *,
        include_classic: bool = False,
    ) -> None:
        self.paths.ensure()
        if worktree_strategy not in {"none", "merge", "rebase"}:
            raise WorkspaceError(f"unknown worktree strategy: {worktree_strategy}")
        checkouts = self._operation_checkouts(names, include_classic)
        failures: list[str] = []

        # Preserve the command's all-checkout preflight guarantee before any
        # parallel worker can fetch or advance a checkout. Workers repeat this
        # validation under their exact Git-admin/source leases to close races.
        explicitly_requested = {
            checkout.name
            for checkout in self._operation_checkouts(names, False)
        } if names else set()
        migrated_content = (
            self._migrated_content_worktree_paths()
            if worktree_strategy != "none"
            and any(checkout.name == "content" for checkout in checkouts)
            else set()
        )
        for checkout in checkouts:
            repository = self._primary_path(checkout)
            if not repository.exists() and not repository.is_symlink():
                if checkout.name in explicitly_requested:
                    raise WorkspaceError(
                        "component is not initialized; run ./atrinik init "
                        f"{checkout.name}: {repository}"
                    )
                continue
            self._validate_primary_checkout(checkout, repository)
            if not _is_clean(repository):
                raise WorkspaceError(
                    f"refusing to update dirty primary checkout: {repository}"
                )
            self._canonical_remote(checkout, repository)
            if worktree_strategy != "none":
                self._component_worktrees(
                    repository,
                    migrated_content if checkout.name == "content" else set(),
                )

        def sync_checkout(checkout: Checkout) -> None:
            repository = self._primary_path(checkout)
            admin_request = self._lease_request(
                "git-admin",
                self._git_admin_coordinate(checkout, repository),
                "exclusive",
                f"synchronize {checkout.name}",
            )
            def source_roots() -> tuple[Path, ...]:
                roots = [repository]
                if repository.is_dir() and worktree_strategy != "none":
                    roots.extend(
                        Path(record["worktree"])
                        for record in _worktree_records(repository, trace=False)
                        if "branch" in record
                    )
                return tuple(
                    sorted(
                        {root.resolve(strict=False) for root in roots},
                        key=str,
                    )
                )

            while True:
                with self._resource_locks([admin_request]):
                    roots = source_roots()
                requests = [
                    self._lease_request(
                        "source",
                        self._source_coordinate(checkout.name, root),
                        "exclusive",
                        f"synchronize {checkout.name}",
                    )
                    for root in roots
                ]
                with self._resource_locks_all_or_none(requests):
                    try:
                        with self._resource_locks(
                            [admin_request],
                            nonblocking=True,
                        ):
                            if source_roots() != roots:
                                continue
                            self._sync_components(
                                [checkout], names, worktree_strategy
                            )
                            return
                    except LockBusyError:
                        pass
                # Never wait for Git administration while retaining a source
                # lease: let the conflicting bounded mutation complete, then
                # resnapshot and retry the ordered exact set.
                with self._resource_locks([admin_request]):
                    pass

        with ThreadPoolExecutor(max_workers=max(1, len(checkouts))) as executor:
            futures = {
                executor.submit(copy_context().run, sync_checkout, checkout): checkout
                for checkout in checkouts
            }
            for future in as_completed(futures):
                try:
                    future.result()
                except Exception as error:
                    failures.append(f"{futures[future].name}: {error}")
        if failures:
            raise WorkspaceError(
                "repository synchronization failed:\n" + "\n".join(sorted(failures))
            )

    def _sync_components(
        self,
        checkouts: list[Checkout],
        explicit_names: list[str] | None,
        worktree_strategy: str,
    ) -> None:
        migrated_content_worktrees = (
            self._migrated_content_worktree_paths()
            if worktree_strategy != "none"
            and any(checkout.name == "content" for checkout in checkouts)
            else set()
        )
        prepared: list[
            tuple[Checkout, Path, str, list[Path], list[Path]]
        ] = []
        explicitly_requested_checkouts = {
            checkout.name
            for checkout in self._operation_checkouts(explicit_names, False)
        } if explicit_names else set()
        for checkout in checkouts:
            repository = self._primary_path(checkout)
            if not repository.exists() and not repository.is_symlink():
                if checkout.name in explicitly_requested_checkouts:
                    raise WorkspaceError(
                        f"component is not initialized; run ./atrinik init "
                        f"{checkout.name}: {repository}"
                    )
                continue
            self._validate_primary_checkout(checkout, repository)
            if not _is_clean(repository):
                raise WorkspaceError(f"refusing to update dirty primary checkout: {repository}")
            remote = self._canonical_remote(checkout, repository)
            candidates: list[Path] = []
            skipped: list[Path] = []
            if worktree_strategy != "none":
                excluded = (
                    migrated_content_worktrees
                    if checkout.name == "content"
                    else set()
                )
                candidates, skipped = self._component_worktrees(
                    repository, excluded
                )
            prepared.append((checkout, repository, remote, candidates, skipped))
        for checkout, repository, remote, candidates, skipped in prepared:
            git(repository, "fetch", "--prune", "--tags", remote)
            git(repository, "merge", "--ff-only", f"{remote}/{checkout.branch}")
            print(f"{checkout.name}: primary synchronized")
            for path in skipped:
                print(
                    f"{checkout.name}: skipped migration-only classic worktree "
                    f"{path}"
                )
            if worktree_strategy != "none":
                self._sync_component_worktrees(
                    checkout, candidates, worktree_strategy
                )

    def _migrated_content_worktree_paths(self) -> set[Path]:
        """Return profile-owned classic worktrees that main must never update."""

        if not self.paths.profiles.is_dir():
            return set()
        component = self.manifest.by_name.get("content")
        if component is None:
            return set()
        protected: set[Path] = set()
        for path in sorted(self.paths.profiles.glob("*.json")):
            try:
                profile = load_json(path)
            except WorkspaceError:
                continue
            if not isinstance(profile, dict) or not isinstance(
                profile.get("components"), dict
            ):
                continue
            selector = profile["components"].get("content-1x")
            if (
                not isinstance(selector, dict)
                or set(selector) != {"kind", "value"}
                or selector.get("kind") != MIGRATED_CONTENT_WORKTREE_KIND
                or not isinstance(selector.get("value"), str)
            ):
                continue
            selected = Path(selector["value"]).resolve()
            expected_parent = (self.paths.worktrees / "content").resolve()
            if selected.parent != expected_parent:
                raise WorkspaceError(
                    f"invalid migration-only content worktree path: {selected}"
                )
            self._validate_checkout(component, selected, trace=False)
            primary = self._primary_path(component)
            self._validate_primary_checkout(component, primary, trace=False)
            if self._git_common_directory(selected, trace=False) != self._git_common_directory(
                primary, trace=False
            ):
                raise WorkspaceError(
                    f"migration-only content worktree lost canonical lineage: {selected}"
                )
            protected.add(selected)
        return protected

    def _component_worktrees(
        self, repository: Path, excluded: set[Path] | None = None
    ) -> tuple[list[Path], list[Path]]:
        primary = repository.resolve()
        excluded = excluded or set()
        candidates: list[Path] = []
        skipped: list[Path] = []
        for record in _worktree_records(repository):
            path = Path(record["worktree"]).resolve()
            if path == primary or "branch" not in record:
                continue
            if path in excluded:
                skipped.append(path)
                continue
            if not _is_clean(path):
                raise WorkspaceError(f"refusing to update dirty worktree: {path}")
            candidates.append(path)
        return candidates, skipped

    def _sync_component_worktrees(
        self, checkout: Checkout, candidates: list[Path], strategy: str
    ) -> None:
        for path in candidates:
            if strategy == "merge":
                git(path, "merge", "--no-edit", checkout.branch)
            elif strategy == "rebase":
                git(path, "rebase", checkout.branch)
            print(f"{checkout.name}: updated {path}")

    def create_worktree(
        self,
        component_name: str,
        label: str,
        branch: str,
        start_point: str | None,
        existing: bool,
    ) -> Path:
        self.paths.ensure()
        validate_name(label, "worktree label")
        checkout = self._resolve_checkout(component_name)
        repository = self._primary_path(checkout)
        destination = self.paths.worktrees / checkout.name / label
        with shared_maintenance_lock(
            self._lease_namespace / "repository-layout.lock"
        ):
            with self._open_managed_worktree_parent(destination) as (
                stable_destination,
                physical_destination,
            ):
                requests = [
                    self._lease_request(
                        "git-admin",
                        self._git_admin_coordinate(checkout, repository),
                        "exclusive",
                        f"create worktree {checkout.name}/{label}",
                    ),
                    self._lease_request(
                        "source",
                        self._source_coordinate(checkout.name, physical_destination),
                        "exclusive",
                        f"create worktree {checkout.name}/{label}",
                    ),
                ]
                if not repository.exists() and not repository.is_symlink():
                    requests.append(
                        self._lease_request(
                            "source",
                            self._source_coordinate(checkout.name, repository),
                            "exclusive",
                            f"initialize {checkout.name} for worktree creation",
                        )
                    )
                with self._resource_locks(requests):
                    return self._create_worktree(
                        component_name,
                        label,
                        branch,
                        start_point,
                        existing,
                        stable_destination,
                    )

    def _create_worktree(
        self,
        component_name: str,
        label: str,
        branch: str,
        start_point: str | None,
        existing: bool,
        stable_destination: Path | None = None,
        *,
        announce: bool = True,
    ) -> Path:
        self.paths.ensure()
        validate_name(label, "worktree label")
        checkout = self._resolve_checkout(component_name)
        repository = self._ensure_repository(checkout)
        remote = self._canonical_remote(checkout, repository)
        run(["git", "check-ref-format", "--branch", branch], capture=True)
        reported_destination = self.paths.worktrees / checkout.name / label
        destination = stable_destination or reported_destination
        if destination.exists():
            raise WorkspaceError(
                f"worktree destination already exists: {reported_destination}"
            )
        if stable_destination is None:
            destination.parent.mkdir(parents=True, exist_ok=True)
        installed = False
        try:
            if existing:
                git(repository, "worktree", "add", "--", str(destination), branch)
            else:
                if start_point is not None and start_point.startswith("-"):
                    raise WorkspaceError("worktree start point must not begin with '-'")
                point = start_point or f"{remote}/{checkout.branch}"
                git(repository, "fetch", "--prune", remote)
                commit = git(
                    repository,
                    "rev-parse",
                    "--verify",
                    "--end-of-options",
                    f"{point}^{{commit}}",
                    capture=True,
                )
                git(
                    repository,
                    "worktree",
                    "add",
                    "-b",
                    branch,
                    "--",
                    str(destination),
                    commit,
                )
            installed = True
            self._validate_checkout(checkout, destination)
            if stable_destination is not None:
                self._require_visible_worktree_identity(
                    reported_destination, destination
                )
        except BaseException as error:
            if installed:
                try:
                    git(repository, "worktree", "remove", "--force", str(destination))
                    if not existing:
                        git(repository, "branch", "-D", branch)
                except BaseException as rollback_error:
                    raise WorkspaceError(
                        "worktree creation failed and its Git registration could not "
                        f"be rolled back: {rollback_error}"
                    ) from error
            raise
        if announce:
            print(reported_destination)
        return reported_destination

    def remove_worktree(self, component_name: str, label: str) -> None:
        self.paths.ensure()
        validate_name(label, "worktree label")
        checkout = self._resolve_checkout(component_name)
        destination = self.paths.worktrees / checkout.name / label
        repository = self._primary_path(checkout)
        if not repository.is_dir() or repository.is_symlink():
            raise WorkspaceError(
                "component is not initialized; run ./atrinik init "
                f"{checkout.name}: {repository}"
            )
        admin_request = self._lease_request(
            "git-admin",
            self._git_admin_coordinate(checkout, repository),
            "exclusive",
            f"remove worktree {checkout.name}/{label}",
        )
        registry_request = self._lease_request(
            "registry",
            "physical-references",
            "exclusive",
            f"remove worktree {checkout.name}/{label}",
        )
        with self._open_managed_worktree(destination) as (
            stable_destination,
            physical_destination,
        ):
            source_request = self._lease_request(
                "source",
                self._source_coordinate(checkout.name, physical_destination),
                "exclusive",
                f"remove worktree {checkout.name}/{label}",
            )
            physical_request = self._lease_request(
                "source",
                self._physical_source_coordinate(physical_destination),
                "exclusive",
                f"remove worktree {checkout.name}/{label}",
            )
            while True:
                with self._resource_locks(
                    [registry_request, source_request, physical_request]
                ):
                    try:
                        with self._resource_locks(
                            [admin_request], nonblocking=True
                        ):
                            self._remove_worktree(
                                component_name, label, stable_destination
                            )
                            return
                    except LockBusyError:
                        pass
                with self._resource_locks([admin_request]):
                    pass

    def _remove_worktree(
        self, component_name: str, label: str, stable_destination: Path
    ) -> None:
        self.paths.ensure()
        validate_name(label, "worktree label")
        checkout = self._resolve_checkout(component_name)
        repository = self._primary_path(checkout)
        self._validate_primary_checkout(checkout, repository)
        candidates = [self.paths.worktrees / checkout.name / label]
        destination = stable_destination
        if any(candidate.is_symlink() for candidate in candidates):
            raise WorkspaceError(
                "worktree does not exist unambiguously: "
                + ", ".join(str(candidate) for candidate in candidates)
            )
        self._require_visible_worktree_identity(candidates[0], destination)
        if not _is_clean(destination):
            raise WorkspaceError(f"refusing to remove dirty worktree: {destination}")
        references = self._source_references(destination)
        if references:
            raise WorkspaceError(
                f"refusing to remove referenced worktree {destination}: "
                + ", ".join(references)
            )
        self._require_visible_worktree_identity(candidates[0], destination)
        git(repository, "worktree", "remove", str(destination))

    @staticmethod
    def _require_visible_worktree_identity(visible: Path, stable: Path) -> None:
        try:
            visible_identity = visible.stat(follow_symlinks=False)
            stable_identity = stable.stat()
        except OSError as error:
            raise WorkspaceError(
                f"managed worktree path was replaced: {visible}"
            ) from error
        if (
            not stat.S_ISDIR(visible_identity.st_mode)
            or (visible_identity.st_dev, visible_identity.st_ino)
            != (stable_identity.st_dev, stable_identity.st_ino)
        ):
            raise WorkspaceError(f"managed worktree path was replaced: {visible}")

    @contextmanager
    def _open_managed_worktree(
        self, destination: Path
    ) -> Iterator[tuple[Path, Path]]:
        """Retain a no-follow target descriptor through destructive Git use."""

        root = Path(os.path.abspath(self.paths.worktrees))
        candidate = Path(os.path.abspath(destination))
        try:
            relative = candidate.relative_to(root)
        except ValueError as error:
            raise WorkspaceError(f"invalid managed worktree path: {candidate}") from error
        if len(relative.parts) != 2:
            raise WorkspaceError(f"invalid managed worktree path: {candidate}")
        flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        descriptors: list[int] = []
        try:
            current_fd = os.open(root, flags)
            descriptors.append(current_fd)
            current_path = root
            for part in relative.parts:
                next_fd = os.open(part, flags, dir_fd=current_fd)
                descriptors.append(next_fd)
                current_fd = next_fd
                current_path /= part
                opened = os.fstat(current_fd)
                visible = current_path.stat(follow_symlinks=False)
                if (
                    not stat.S_ISDIR(opened.st_mode)
                    or (opened.st_dev, opened.st_ino)
                    != (visible.st_dev, visible.st_ino)
                ):
                    raise WorkspaceError(
                        f"managed worktree path was replaced: {current_path}"
                    )
            retained = os.dup(current_fd)
            try:
                stable = _descriptor_path(retained)
                physical = stable.resolve(strict=True)
                with inherit_lock_fds(retained):
                    yield stable, physical
            finally:
                os.close(retained)
        except OSError as error:
            raise WorkspaceError(
                f"cannot open managed worktree path {candidate}: {error}"
            ) from error
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)

    @contextmanager
    def _open_managed_worktree_parent(
        self, destination: Path
    ) -> Iterator[tuple[Path, Path]]:
        """Retain the destination parent without following managed-path links."""

        root = Path(os.path.abspath(self.paths.worktrees))
        candidate = Path(os.path.abspath(destination))
        try:
            relative = candidate.relative_to(root)
        except ValueError as error:
            raise WorkspaceError(f"invalid managed worktree path: {candidate}") from error
        if len(relative.parts) != 2:
            raise WorkspaceError(f"invalid managed worktree path: {candidate}")
        checkout_name, label = relative.parts
        flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        descriptors: list[int] = []
        try:
            root_fd = os.open(root, flags)
            descriptors.append(root_fd)
            try:
                os.mkdir(checkout_name, dir_fd=root_fd)
            except FileExistsError:
                pass
            parent_fd = os.open(checkout_name, flags, dir_fd=root_fd)
            descriptors.append(parent_fd)
            opened = os.fstat(parent_fd)
            visible_path = root / checkout_name
            visible = visible_path.stat(follow_symlinks=False)
            if (
                not stat.S_ISDIR(opened.st_mode)
                or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
            ):
                raise WorkspaceError(
                    f"managed worktree path was replaced: {visible_path}"
                )
            try:
                os.stat(label, dir_fd=parent_fd, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                raise WorkspaceError(f"worktree destination already exists: {candidate}")
            retained = os.dup(parent_fd)
            try:
                stable_parent = _descriptor_path(retained)
                physical_parent = stable_parent.resolve(strict=True)
                with inherit_lock_fds(retained):
                    yield stable_parent / label, physical_parent / label
            finally:
                os.close(retained)
        except OSError as error:
            raise WorkspaceError(
                f"cannot open managed worktree parent {candidate.parent}: {error}"
            ) from error
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)

    def _source_references(self, source_root: Path) -> list[str]:
        """Return exact persisted references while the caller holds source exclusive."""

        target = source_root.resolve()
        references: list[str] = self._scope_source_references(target)
        for record in self._physical_reference_records():
            if (
                not isinstance(record, dict)
                or set(record) != {"schema_version", "kind", "reference", "sources"}
                or record["schema_version"] != 1
                or record["kind"] not in {"profiles", "scenarios"}
                or not isinstance(record["reference"], str)
                or not isinstance(record["sources"], list)
            ):
                raise WorkspaceError("cannot prove physical reference record")
            if any(
                not isinstance(source, str) or not Path(source).is_absolute()
                for source in record["sources"]
            ):
                raise WorkspaceError("cannot prove physical reference record")
            if target in {
                Path(source).resolve(strict=False) for source in record["sources"]
            }:
                references.append(f"{record['kind'][:-1]}:{record['reference']}")
        if self.paths.profiles.exists():
            for path in sorted(self.paths.profiles.glob("*.json")):
                profile = self._load_profile_file(path.stem, require_file=True)
                stack = self.manifest.stack(profile["stack"])
                roots = {
                    self._selector_root(profile, component).resolve(strict=False)
                    for component in stack.components
                }
                if target in roots:
                    references.append(f"profile:{profile['name']}")
        for container, record_name in (
            (self.paths.topologies, "topology"),
            (self.paths.scenarios, "scenario"),
        ):
            if not container.exists():
                continue
            for directory in sorted(container.iterdir()):
                if (
                    directory.name.startswith(".")
                    or not directory.is_dir()
                    or directory.is_symlink()
                ):
                    continue
                candidates = (
                    (directory / "spec.json", directory / "status.json")
                    if record_name == "topology"
                    else (directory / "scenario.json",)
                )
                for record_path in candidates:
                    if not record_path.exists():
                        continue
                    value = load_json(record_path)
                    resolved = value.get("resolved") if isinstance(value, dict) else None
                    if not isinstance(resolved, dict):
                        raise WorkspaceError(
                            f"cannot prove {record_name} references: {record_path}"
                        )
                    for coordinate in resolved.values():
                        if not isinstance(coordinate, dict):
                            continue
                        raw_path = coordinate.get("checkout_path") or coordinate.get("path")
                        if isinstance(raw_path, str) and Path(raw_path).resolve(
                            strict=False
                        ) == target:
                            references.append(f"{record_name}:{directory.name}")
                            break
                    if references and references[-1] == f"{record_name}:{directory.name}":
                        break
        return sorted(set(references))

    def _scope_source_references(self, target: Path) -> list[str]:
        """Keep complete or recoverable scope inputs visible to cleanup."""

        return [
            f"scope:{name}"
            for name, path in self._scope_reference_records()
            if path.resolve(strict=False) == target
        ]

    def _scope_reference_records(self) -> list[tuple[str, Path]]:
        """Return exact source paths retained by complete or recoverable scopes."""

        from .scopes import (
            SCOPE_JOURNAL_SCHEMA_VERSION,
            SCOPE_RELEASE_SCHEMA_VERSION,
            ScopeLifecycle,
        )

        root = self.paths.scopes
        if not root.exists():
            return []
        if root.is_symlink() or not root.is_dir():
            raise WorkspaceError(f"cannot prove scope references: {root}")
        references: list[tuple[str, Path]] = []
        lifecycle = ScopeLifecycle(self)
        for directory in sorted(root.iterdir()):
            if (
                directory.name.startswith(".")
                or directory.is_symlink()
                or not directory.is_dir()
            ):
                raise WorkspaceError(
                    f"cannot prove scope reference directory: {directory}"
                )
            record_path = directory / "scope.json"
            journal_path = directory / "creation-journal.json"
            release_path = directory / "release-journal.json"
            completed: set[str] = set()
            if release_path.exists() or release_path.is_symlink():
                release = load_regular_json(
                    release_path, f"scope release {directory.name}"
                )
                if (
                    not isinstance(release, dict)
                    or release.get("schema_version")
                    != SCOPE_RELEASE_SCHEMA_VERSION
                    or release.get("scope") != directory.name
                    or not isinstance(release.get("completed"), list)
                    or not all(
                        isinstance(item, str) for item in release["completed"]
                    )
                ):
                    raise WorkspaceError(
                        f"cannot prove scope release references: {release_path}"
                    )
                completed = set(release["completed"])
            if record_path.exists() or record_path.is_symlink():
                record = lifecycle._load_record(directory.name)
                rows = [
                    row
                    for row in record["worktrees"]
                    if f"worktree:{row['checkout']}" not in completed
                ]
            elif journal_path.exists() or journal_path.is_symlink():
                journal = load_regular_json(
                    journal_path, f"scope creation journal {directory.name}"
                )
                if (
                    not isinstance(journal, dict)
                    or journal.get("schema_version")
                    != SCOPE_JOURNAL_SCHEMA_VERSION
                    or journal.get("name") != directory.name
                    or not isinstance(journal.get("worktrees"), list)
                ):
                    raise WorkspaceError(
                        f"cannot prove scope creation references: {journal_path}"
                    )
                rows = [
                    row
                    for row in journal["worktrees"]
                    if isinstance(row, dict)
                    and row.get("status")
                    in {"created", "preserved-changed", "preserved-uncertain"}
                ]
                if any(
                    not isinstance(row, dict)
                    or not isinstance(row.get("path"), str)
                    for row in journal["worktrees"]
                ):
                    raise WorkspaceError(
                        f"cannot prove scope creation references: {journal_path}"
                    )
            else:
                continue
            references.extend(
                (directory.name, Path(row["path"])) for row in rows
            )
        return references

    def list_worktrees(self, names: list[str] | None = None) -> list[tuple[str, dict[str, str]]]:
        self.paths.ensure()
        result: list[tuple[str, dict[str, str]]] = []
        checkouts = (
            self._operation_checkouts(names, False)
            if names
            else self.manifest.checkouts
        )
        for checkout in checkouts:
            repository = self._primary_path(checkout)
            if not repository.is_dir():
                continue
            self._validate_checkout(checkout, repository, trace=False)
            result.extend(
                (checkout.name, record)
                for record in _worktree_records(repository, trace=False)
            )
        return result

    def create_profile(self, name: str, source: str = "default") -> Path:
        self.paths.ensure()
        validate_name(name, "profile name")
        validate_name(source, "profile name")
        scope_owner = self._scope_profile_owner(name)
        if scope_owner is not None:
            raise WorkspaceError(
                f"profile name is reserved by scope {scope_owner}: {name}"
            )
        requests = [
            self._lease_request(
                "profile", name, "exclusive", f"create profile {name}"
            ),
            self._lease_request(
                "profile", source, "shared", f"copy profile {source} to {name}"
            ),
        ]
        with self._resource_locks(requests):
            source_profile = self._load_profile_file(source, require_file=False)
            stack = self.manifest.stack(source_profile["stack"])
            source_requests: list[LeaseRequest] = []
            seen: set[str] = set()
            for component in stack.components:
                root = self._selector_root(source_profile, component)
                coordinate = self._source_coordinate(
                    component.checkout_name, root
                )
                if coordinate in seen:
                    continue
                seen.add(coordinate)
                source_requests.append(
                    self._lease_request(
                        "source", coordinate, "shared", f"create profile {name}"
                    )
                )
            with self._resource_locks(source_requests):
                confirmed_profile = self._load_profile_file(
                    source, require_file=False
                )
                if confirmed_profile != source_profile:
                    raise WorkspaceError(
                        f"profile {source} changed while its exact sources were being locked; retry"
                    )
                return self._create_profile(name, confirmed_profile)

    def _create_profile(
        self,
        name: str,
        source_profile: dict[str, Any],
    ) -> Path:
        self.paths.ensure()
        validate_name(name, "profile name")
        if name in self.manifest.stacks:
            raise WorkspaceError(f"{name} is a built-in profile")
        path = self.paths.profiles / f"{name}.json"
        if path.exists():
            raise WorkspaceError(f"profile already exists: {name}")
        value = {
            "schema_version": PROFILE_SCHEMA_VERSION,
            "name": name,
            "stack": source_profile["stack"],
            "sound_mode": source_profile["sound_mode"],
            "sound_release": (
                dict(source_profile["sound_release"])
                if source_profile["sound_release"] is not None
                else None
            ),
            "components": {
                component_name: dict(selector)
                for component_name, selector in source_profile["components"].items()
            },
        }
        self._publish_profile_references(name, value)
        try:
            durable_atomic_json(path, value)
        except AtomicJsonCommitUncertain:
            # The authored profile is visible. Keep its conservative physical
            # references rather than pretending publication did not occur.
            raise
        except BaseException:
            self._remove_physical_reference(path)
            raise
        print(path)
        return path

    def set_profile(
        self, name: str, component_name: str, kind: str, value: str = ""
    ) -> None:
        self.paths.ensure()
        validate_name(name, "profile name")
        scope_owner = self._scope_profile_owner(name)
        if scope_owner is not None:
            raise WorkspaceError(
                f"profile is immutable while owned by scope {scope_owner}: {name}"
            )
        profile_request = self._lease_request(
            "profile", name, "exclusive", f"update profile {name}"
        )
        with self._resource_locks([profile_request]):
            profile = self._load_profile_file(name, require_file=True)
            components = self._profile_components(profile, component_name)
            checkout_names = {component.checkout_name for component in components}
            if len(checkout_names) != 1:
                raise WorkspaceError(
                    f"profile selection spans multiple checkouts: {component_name}"
                )
            checkout_name = checkout_names.pop()
            checkout = self.manifest.by_checkout[checkout_name]
            if kind == "primary":
                selected_root = self._primary_path(checkout)
            elif kind == "worktree":
                validate_name(value, "worktree label")
                selected_root = self.paths.worktrees / checkout_name / value
            elif kind == "path":
                selected_root = Path(value).expanduser()
                if not selected_root.is_absolute():
                    raise WorkspaceError("profile checkout path must be absolute")
                if selected_root.is_symlink():
                    raise WorkspaceError(
                        f"component checkout is not a directory: {selected_root}"
                    )
                selected_root = selected_root.resolve(strict=False)
                value = str(selected_root)
            else:
                raise WorkspaceError(f"invalid profile selector kind: {kind}")
            source_request = self._lease_request(
                "source",
                self._source_coordinate(checkout_name, selected_root),
                "shared",
                f"publish profile {name}",
            )
            with self._resource_locks([source_request]):
                self._set_profile(name, component_name, kind, value)

    def _backfill_profile_references(self, *, already_locked: bool = False) -> None:
        if not self.paths.profiles.is_dir() or self.paths.profiles.is_symlink():
            return
        for path in sorted(self.paths.profiles.glob("*.json")):
            if already_locked:
                profile = self._load_profile_file(path.stem, require_file=True)
                self._publish_profile_references(path.stem, profile)
                continue
            profile = load_regular_json(path, f"profile {path.stem}")
            sources = self._raw_profile_reference_sources(path.stem, profile)
            requests = [
                self._lease_request(
                    "profile", path.stem, "shared", "backfill profile reference"
                ),
                *[
                    self._lease_request(
                        "source",
                        self._source_coordinate(checkout, source),
                        "shared",
                        "backfill profile reference",
                    )
                    for checkout, source in sources
                ],
            ]
            with resource_locks(self._lease_root, requests, nonblocking=True):
                confirmed = load_regular_json(path, f"profile {path.stem}")
                if confirmed != profile:
                    raise WorkspaceError(
                        "profile changed during physical reference backfill: "
                        f"{path.stem}; stop editing that profile and retry"
                    )
                # Missing selectors are historical authored coordinates. Publish
                # them conservatively so construction can finish without making
                # a source at that exact path eligible for later reclamation.
                self._publish_raw_profile_references(
                    path.stem, path, profile=confirmed
                )

    def _raw_profile_reference_sources(
        self, name: str, profile: Any
    ) -> list[tuple[str, Path]]:
        if not isinstance(profile, dict) or not isinstance(profile.get("components"), dict):
            raise WorkspaceError(f"profile must be an object with components: {name}")
        sources: dict[tuple[str, str], tuple[str, Path]] = {}
        for component_name, selector in profile["components"].items():
            if not isinstance(selector, dict) or selector.get("kind") == "primary":
                continue
            component = self.manifest.by_name.get(component_name)
            kind, value = selector.get("kind"), selector.get("value")
            legacy_component = component is None and (
                component_name == "content-1x" or component_name in PROFILE_IDENTITIES
            )
            if (component is None and not legacy_component) or not isinstance(value, str):
                raise WorkspaceError(f"profile selector is invalid: {name}/{component_name}")
            legacy_checkout = (
                "content-1x"
                if component_name in {"content", "content-1x"}
                else component_name.removeprefix("legacy-")
            )
            root = (
                self.paths.worktrees
                / (component.checkout_name if component is not None else legacy_checkout)
                / value
                if kind == "worktree"
                else Path(value)
            )
            if not root.is_absolute():
                raise WorkspaceError(f"profile selector is invalid: {name}/{component_name}")
            resolved = root.resolve(strict=False)
            checkout_name = (
                component.checkout_name if component is not None else legacy_checkout
            )
            sources[(checkout_name, str(resolved))] = (
                checkout_name,
                resolved,
            )
        return [sources[key] for key in sorted(sources)]

    def _publish_raw_profile_references(
        self, name: str, path: Path, *, profile: Any | None = None
    ) -> None:
        if profile is None:
            profile = load_regular_json(path, f"profile {name}")
        sources = self._raw_profile_reference_sources(name, profile)
        identity = hashlib.sha256(str(path.resolve()).encode()).hexdigest()
        self._atomic_physical_reference(
            f"{identity}.json",
            {
                "schema_version": 1,
                "kind": "profiles",
                "reference": name,
                "sources": sorted(str(source) for _checkout, source in sources),
            },
        )

    def _publish_migration_profile_references(
        self, transitions: dict[str, tuple[bytes, bytes]] | None = None
    ) -> None:
        """Publish conservative profile refs while a migration owns the barrier."""

        retained: dict[str, set[str]] = {}
        for name, generations in (transitions or {}).items():
            sources: set[str] = set()
            for raw in generations:
                try:
                    profile = json.loads(
                        raw, object_pairs_hook=_reject_duplicate_keys
                    )
                    sources.update(
                        str(source)
                        for _checkout, source in self._raw_profile_reference_sources(
                            name, profile
                        )
                    )
                except (KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
                    raise WorkspaceError(
                        f"cannot publish migration references for profile {name}: {error}"
                    ) from error
            retained[name] = sources
        if not self.paths.profiles.is_dir():
            return
        for path in sorted(self.paths.profiles.glob("*.json")):
            profile = load_regular_json(path, f"profile {path.stem}")
            current = {
                str(source)
                for _checkout, source in self._raw_profile_reference_sources(
                    path.stem, profile
                )
            }
            identity = hashlib.sha256(str(path.resolve()).encode()).hexdigest()
            self._atomic_physical_reference(
                f"{identity}.json",
                {
                    "schema_version": 1,
                    "kind": "profiles",
                    "reference": path.stem,
                    "sources": sorted(current | retained.get(path.stem, set())),
                },
            )

    def _backfill_physical_references(self) -> None:
        state_identity = hashlib.sha256(
            str(self.paths.workspace.resolve()).encode()
        ).hexdigest()
        marker_reference = f"__backfill__:{state_identity}"
        if self._physical_backfill_complete(state_identity, marker_reference):
            return
        registry_request = self._lease_request(
            "registry",
            f"physical-reference-backfill:{state_identity}",
            "exclusive",
            "backfill physical references",
        )
        publication_request = self._lease_request(
            "registry",
            "physical-references",
            "shared",
            "backfill physical references",
        )
        with shared_maintenance_lock(
            self._lease_namespace / "repository-layout.lock"
        ):
            with resource_locks(
                self._lease_root,
                [publication_request, registry_request],
            ):
                if self._physical_backfill_complete(
                    state_identity, marker_reference
                ):
                    return
                self._backfill_profile_references()
                self._backfill_scenario_references()
                self._atomic_physical_reference(
                    f"{state_identity}.json",
                    {
                        "schema_version": 1,
                        "kind": "profiles",
                        "reference": marker_reference,
                        "sources": [],
                    },
                )

    def _physical_backfill_complete(
        self, state_identity: str, marker_reference: str
    ) -> bool:
        for marker in self._physical_reference_records(
            only=f"{state_identity}.json"
        ):
            if not isinstance(marker, dict):
                raise WorkspaceError("physical reference backfill marker is invalid")
            if (
                set(marker)
                == {"schema_version", "kind", "reference", "sources"}
                and marker.get("schema_version") == 1
                and marker.get("kind") == "profiles"
                and marker.get("reference") == marker_reference
                and marker.get("sources") == []
            ):
                return True
            raise WorkspaceError("physical reference backfill marker is invalid")
        return False

    def _atomic_physical_reference(
        self, name: str, value: dict[str, Any]
    ) -> None:
        namespace = self._lease_namespace
        flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        parent_fd = os.open(namespace, flags)
        directory_fd: int | None = None
        temporary = f".{name}.{secrets.token_hex(12)}.tmp"
        try:
            self._assert_physical_namespace_fd(parent_fd)
            self._validate_physical_reference_directory(
                parent_fd, namespace, "physical lease namespace"
            )
            try:
                os.mkdir("profile-references", mode=0o700, dir_fd=parent_fd)
            except FileExistsError:
                pass
            directory_fd = os.open("profile-references", flags, dir_fd=parent_fd)
            registry = namespace / "profile-references"
            self._validate_physical_reference_directory(
                directory_fd,
                registry,
                "physical reference registry",
                expected_mode=0o700,
            )
            # Persist (or re-prove) the registry directory entry before any
            # reference is accepted as durable within it. This is required on
            # EEXIST too: a prior creator may have failed before its parent
            # fsync completed.
            os.fsync(parent_fd)
            descriptor = os.open(
                temporary,
                os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC,
                0o600,
                dir_fd=directory_fd,
            )
            try:
                with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
                    json.dump(value, stream, indent=2, sort_keys=True)
                    stream.write("\n")
                    stream.flush()
                    os.fsync(stream.fileno())
                os.rename(temporary, name, src_dir_fd=directory_fd, dst_dir_fd=directory_fd)
                os.fsync(directory_fd)
                self._validate_physical_reference_directory(
                    directory_fd,
                    registry,
                    "physical reference registry",
                    expected_mode=0o700,
                )
                self._validate_physical_reference_directory(
                    parent_fd, namespace, "physical lease namespace"
                )
            except BaseException:
                try:
                    os.unlink(temporary, dir_fd=directory_fd)
                except FileNotFoundError:
                    pass
                raise
        except OSError as error:
            raise WorkspaceError(
                f"cannot publish physical reference {name}: {error}"
            ) from error
        finally:
            if directory_fd is not None:
                os.close(directory_fd)
            os.close(parent_fd)

    @staticmethod
    def _validate_physical_reference_directory(
        descriptor: int,
        path: Path,
        description: str,
        *,
        expected_mode: int | None = None,
    ) -> None:
        opened = os.fstat(descriptor)
        visible = path.stat(follow_symlinks=False)
        if (
            not stat.S_ISDIR(opened.st_mode)
            or not stat.S_ISDIR(visible.st_mode)
            or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
            or opened.st_uid != os.geteuid()
            or (
                expected_mode is not None
                and stat.S_IMODE(opened.st_mode) != expected_mode
            )
        ):
            raise WorkspaceError(f"{description} was replaced or is unsafe: {path}")

    def _physical_reference_records(
        self, *, only: str | None = None
    ) -> list[dict[str, Any]]:
        registry = self._lease_namespace / "profile-references"
        flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        namespace_fd: int | None = None
        try:
            namespace_fd = os.open(self._lease_namespace, flags)
            self._assert_physical_namespace_fd(namespace_fd)
            self._validate_physical_reference_directory(
                namespace_fd, self._lease_namespace, "physical lease namespace"
            )
            try:
                directory_fd = os.open(
                    "profile-references", flags, dir_fd=namespace_fd
                )
            except FileNotFoundError:
                self._validate_physical_reference_directory(
                    namespace_fd,
                    self._lease_namespace,
                    "physical lease namespace",
                )
                os.close(namespace_fd)
                return []
            self._validate_physical_reference_directory(
                directory_fd,
                registry,
                "physical reference registry",
                expected_mode=0o700,
            )
        except (OSError, WorkspaceError) as error:
            if namespace_fd is not None:
                os.close(namespace_fd)
            if isinstance(error, WorkspaceError):
                raise
            raise WorkspaceError(
                f"cannot open physical reference registry {registry}: {error}"
            ) from error
        records: list[dict[str, Any]] = []
        try:
            names = [only] if only is not None else sorted(os.listdir(directory_fd))
            for name in names:
                if re.fullmatch(r"\.[0-9a-f]{64}\.json\.[0-9a-f]{24}\.tmp", name):
                    continue
                if not re.fullmatch(r"[0-9a-f]{64}\.json", name):
                    raise WorkspaceError(
                        f"cannot prove physical reference registry entry: {name}"
                    )
                try:
                    descriptor = os.open(
                        name,
                        os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0),
                        dir_fd=directory_fd,
                    )
                except FileNotFoundError:
                    if only is not None:
                        continue
                    raise
                try:
                    metadata = os.fstat(descriptor)
                    visible = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
                    if (
                        not stat.S_ISREG(metadata.st_mode)
                        or (metadata.st_dev, metadata.st_ino)
                        != (visible.st_dev, visible.st_ino)
                        or metadata.st_size > 4 * 1024 * 1024
                    ):
                        raise WorkspaceError(
                            f"cannot prove physical reference registry entry: {name}"
                        )
                    with os.fdopen(descriptor, encoding="utf-8", closefd=False) as stream:
                        records.append(
                            json.load(
                                stream, object_pairs_hook=_reject_duplicate_keys
                            )
                        )
                finally:
                    os.close(descriptor)
            self._validate_physical_reference_directory(
                directory_fd,
                registry,
                "physical reference registry",
                expected_mode=0o700,
            )
            self._validate_physical_reference_directory(
                namespace_fd,
                self._lease_namespace,
                "physical lease namespace",
            )
        except (OSError, UnicodeError, ValueError, RecursionError) as error:
            raise WorkspaceError(
                f"cannot read physical reference registry {registry}: {error}"
            ) from error
        finally:
            os.close(directory_fd)
            os.close(namespace_fd)
        return records

    def _remove_physical_reference(self, authored_path: Path) -> None:
        """Roll back a prepublished reference when authored creation fails."""

        name = hashlib.sha256(str(authored_path.resolve()).encode()).hexdigest() + ".json"
        namespace = self._lease_namespace
        registry = namespace / "profile-references"
        flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        namespace_fd: int | None = None
        try:
            namespace_fd = os.open(namespace, flags)
            self._assert_physical_namespace_fd(namespace_fd)
            self._validate_physical_reference_directory(
                namespace_fd, namespace, "physical lease namespace"
            )
            directory_fd = os.open("profile-references", flags, dir_fd=namespace_fd)
            try:
                self._validate_physical_reference_directory(
                    directory_fd,
                    registry,
                    "physical reference registry",
                    expected_mode=0o700,
                )
                try:
                    os.unlink(name, dir_fd=directory_fd)
                except FileNotFoundError:
                    return
                os.fsync(directory_fd)
                self._validate_physical_reference_directory(
                    directory_fd,
                    registry,
                    "physical reference registry",
                    expected_mode=0o700,
                )
                self._assert_physical_namespace_fd(namespace_fd)
                self._validate_physical_reference_directory(
                    namespace_fd, namespace, "physical lease namespace"
                )
            finally:
                os.close(directory_fd)
        except OSError as error:
            raise WorkspaceError(
                f"cannot roll back physical reference {name}: {error}"
            ) from error
        finally:
            if namespace_fd is not None:
                os.close(namespace_fd)

    def _publish_profile_references(
        self,
        name: str,
        profile: dict[str, Any],
        retained_sources: set[str] | None = None,
    ) -> None:
        stack = self.manifest.stack(profile["stack"])
        sources = sorted(
            ({
                str(self._selector_root(profile, component).resolve(strict=False))
                for component in stack.components
                if profile["components"][component.name]["kind"] != "primary"
            } | (retained_sources or set()))
        )
        identity = hashlib.sha256(
            str((self.paths.profiles / f"{name}.json").resolve()).encode()
        ).hexdigest()
        self._atomic_physical_reference(
            f"{identity}.json",
            {
                "schema_version": 1,
                "kind": "profiles",
                "reference": name,
                "sources": sources,
            },
        )

    def _publish_scenario_references(
        self,
        name: str,
        metadata: dict[str, Any],
        retained_sources: set[str] | None = None,
    ) -> None:
        resolved = metadata.get("resolved")
        if not isinstance(resolved, dict):
            raise WorkspaceError(f"scenario resolved references are invalid: {name}")
        if any(
            not isinstance(value, dict)
            or not isinstance(value.get("checkout"), str)
            or not isinstance(value.get("checkout_path"), str)
            for value in resolved.values()
        ):
            raise WorkspaceError(f"scenario resolved references are invalid: {name}")
        sources = sorted(
            {
                str(Path(value["checkout_path"]).resolve(strict=False))
                for value in resolved.values()
            }
            | (retained_sources or set())
        )
        self._publish_scenario_reference_sources(name, sources)

    def _publish_scenario_reference_sources(
        self, name: str, sources: list[str] | set[str]
    ) -> None:
        record = (self.paths.scenarios / name / "scenario.json").resolve()
        identity = hashlib.sha256(str(record).encode()).hexdigest()
        self._atomic_physical_reference(
            f"{identity}.json",
            {
                "schema_version": 1,
                "kind": "scenarios",
                "reference": name,
                "sources": sorted(sources),
            },
        )

    def _backfill_scenario_references(self) -> None:
        if not self.paths.scenarios.is_dir() or self.paths.scenarios.is_symlink():
            return
        for root in sorted(self.paths.scenarios.iterdir()):
            record = root / "scenario.json"
            if root.is_dir() and not root.is_symlink() and record.is_file():
                metadata = load_regular_json(record, "scenario metadata")
                resolved = metadata.get("resolved") if isinstance(metadata, dict) else None
                if not isinstance(resolved, dict):
                    raise WorkspaceError(
                        f"scenario resolved references are invalid: {root.name}"
                    )
                scenario_request = self._lease_request(
                    "scenario", root.name, "shared", "backfill scenario reference"
                )
                with resource_locks(
                    self._lease_root, [scenario_request], nonblocking=True
                ):
                    confirmed = load_regular_json(record, "scenario metadata")
                    if confirmed != metadata:
                        raise WorkspaceError(
                            "scenario changed during physical reference backfill: "
                            f"{root.name}; stop editing that scenario and retry"
                        )
                    # The caller holds the common physical-reference registry
                    # barrier against removal, so the complete source set can be
                    # published once without retaining one descriptor per path.
                    self._publish_scenario_references(root.name, confirmed)

    def _set_profile(
        self, name: str, component_name: str, kind: str, value: str = ""
    ) -> None:
        self.paths.ensure()
        profile = self._load_profile(name, require_file=True)
        old_profile = copy.deepcopy(profile)
        components = self._profile_components(profile, component_name)
        checkout_names = {component.checkout_name for component in components}
        if len(checkout_names) != 1:
            raise WorkspaceError(
                f"profile selection spans multiple checkouts: {component_name}"
            )
        checkout = self.manifest.by_checkout[checkout_names.pop()]
        if kind == "primary":
            value = ""
        elif kind == "worktree":
            validate_name(value, "worktree label")
            path = self.paths.worktrees / checkout.name / value
            self._validate_selected_checkout(components[0], path, kind)
        elif kind == "path":
            path = Path(value).expanduser()
            if not path.is_absolute():
                raise WorkspaceError("profile checkout path must be absolute")
            self._validate_selected_checkout(components[0], path, kind)
            value = str(path.resolve())
        else:
            raise WorkspaceError(f"invalid profile selector kind: {kind}")
        for component in components:
            profile["components"][component.name] = {"kind": kind, "value": value}
        old_sources = {
            str(self._selector_root(old_profile, component).resolve(strict=False))
            for component in self.manifest.stack(old_profile["stack"]).components
            if old_profile["components"][component.name]["kind"] != "primary"
        }
        self._publish_profile_references(name, profile, old_sources)
        durable_atomic_json(self.paths.profiles / f"{name}.json", profile)
        self._publish_profile_references(name, profile)

    def set_profile_sound_mode(
        self,
        name: str,
        mode: str,
        release_coordinates: dict[str, Any] | None = None,
    ) -> None:
        self.paths.ensure()
        scope_owner = self._scope_profile_owner(name)
        if scope_owner is not None:
            raise WorkspaceError(
                f"profile is immutable while owned by scope {scope_owner}: {name}"
            )
        with self._resource_locks(
            [
                self._lease_request(
                    "profile", name, "exclusive", f"update profile {name} sound mode"
                )
            ],
        ):
            if name in self.manifest.stacks:
                raise WorkspaceError(
                    "sound mode can be changed only on a saved derived profile"
                )
            profile = self._load_profile(name, require_file=True)
            if profile["stack"] != "classic":
                raise WorkspaceError(
                    "non-source sound modes are available only to Classic-derived profiles"
                )
            if mode not in SOUND_MODES:
                raise WorkspaceError(f"invalid profile sound mode: {mode}")
            if mode == RELEASED_MODE:
                if release_coordinates is None:
                    raise WorkspaceError(
                        "released sound mode requires complete immutable release coordinates"
                    )
                release_coordinates = dict(
                    validate_release_coordinates(release_coordinates)
                )
            elif release_coordinates is not None:
                raise WorkspaceError(
                    f"profile sound mode {mode} does not accept release coordinates"
                )
            profile["schema_version"] = PROFILE_SCHEMA_VERSION
            profile["sound_mode"] = mode
            profile["sound_release"] = release_coordinates
            atomic_json(self.paths.profiles / f"{name}.json", profile)

    def _profile_components(
        self, profile: dict[str, Any], component_checkout_or_role: str
    ) -> list[Component]:
        stack = self.manifest.stack(profile["stack"])
        if component_checkout_or_role in self.manifest.by_checkout:
            selected = [
                component
                for component in stack.components
                if component.checkout_name == component_checkout_or_role
            ]
            if selected:
                return selected
        component = self._profile_component(profile, component_checkout_or_role)
        return [
            candidate
            for candidate in stack.components
            if candidate.checkout_name == component.checkout_name
        ]

    def _profile_component(
        self, profile: dict[str, Any], component_or_role: str
    ) -> Component:
        stack = self.manifest.stack(profile["stack"])
        components = {component.name: component for component in stack.components}
        if component_or_role in components:
            return components[component_or_role]
        provider = stack.providers.get(component_or_role)
        if provider is not None:
            return provider
        raise WorkspaceError(
            f"component or role is not part of {stack.name} stack: "
            f"{component_or_role}"
        )

    def _selector_root(
        self, profile: dict[str, Any], component: Component
    ) -> Path:
        selector = profile["components"][component.name]
        if selector["kind"] == "primary":
            return self._primary_path(component)
        if selector["kind"] == "worktree":
            return self.paths.worktrees / component.checkout_name / selector["value"]
        return Path(selector["value"])

    def _component_source(self, component: Component, checkout_root: Path) -> Path:
        if checkout_root.is_symlink() or not checkout_root.is_dir():
            raise WorkspaceError(
                f"component checkout is not a directory: {checkout_root}"
            )
        root = checkout_root.resolve()
        parts = (
            ()
            if component.source == "."
            else PurePosixPath(component.source).parts
        )
        source = checkout_root
        for part in parts:
            source = source / part
            if source.is_symlink():
                raise WorkspaceError(
                    f"component source is not a normal directory: "
                    f"{component.name}: {source}"
                )
        if not source.is_dir():
            raise WorkspaceError(
                f"component source is not a normal directory: {component.name}: {source}"
            )
        resolved = source.resolve()
        if resolved != root and root not in resolved.parents:
            raise WorkspaceError(
                f"component source escapes checkout: {component.name}: {source}"
            )
        return resolved

    def resolve_profile(
        self,
        name: str,
        component_names: set[str] | None = None,
        *,
        trace: bool = True,
        profile: dict[str, Any] | None = None,
    ) -> dict[str, Path]:
        self.paths.ensure()
        if profile is None:
            profile = self._load_profile(name, require_file=False)
        result: dict[str, Path] = {}
        stack = self.manifest.stack(profile["stack"])
        stack_components = {component.name: component for component in stack.components}
        if component_names is None:
            components = list(stack.components)
        else:
            unknown = sorted(component_names - set(stack_components))
            if unknown:
                raise WorkspaceError(
                    f"components are not part of {stack.name} stack: "
                    f"{', '.join(unknown)}"
                )
            components = [
                component
                for component in stack.components
                if component.name in component_names
            ]
        validated_checkouts: set[str] = set()
        for component in components:
            root = self._selector_root(profile, component)
            if component.checkout_name not in validated_checkouts:
                selector = profile["components"][component.name]
                self._validate_selected_checkout(
                    component, root, selector["kind"], trace=trace
                )
                validated_checkouts.add(component.checkout_name)
            result[component.name] = self._component_source(component, root)
        return result

    def _validate_selected_checkout(
        self,
        component: Component,
        path: Path,
        selector_kind: str,
        *,
        trace: bool = True,
    ) -> str:
        checkout = self.manifest.checkout_for(component)
        if selector_kind == "primary":
            return self._validate_primary_checkout(checkout, path, trace=trace)
        remote = self._validate_checkout(checkout, path, trace=trace)
        if selector_kind == MIGRATED_CONTENT_WORKTREE_KIND:
            if component.name != "content-1x":
                raise WorkspaceError(
                    "migration-only worktree selector is valid only for content-1x"
                )
            expected_parent = (self.paths.worktrees / "content").resolve()
            if path.resolve().parent != expected_parent:
                raise WorkspaceError(
                    "migrated content worktree must remain directly below "
                    f"{expected_parent}: {path}"
                )
            content_checkout = self.manifest.by_checkout.get("content")
            if content_checkout is None:
                raise WorkspaceError(
                    "manifest has no canonical content provider for migrated worktree"
                )
            primary = self._primary_path(content_checkout)
            if not primary.is_dir() or primary.is_symlink():
                raise WorkspaceError(
                    "cannot prove migrated content worktree lineage; initialize "
                    f"canonical content first: {primary}"
                )
            self._validate_primary_checkout(content_checkout, primary, trace=trace)
            selected_common = self._git_common_directory(path, trace=trace)
            content_common = self._git_common_directory(primary, trace=trace)
            if selected_common != content_common:
                raise WorkspaceError(
                    "migrated content worktree is no longer attached to canonical "
                    f"content: {path}"
                )
            return remote
        variants = [
            candidate
            for candidate in self.manifest.checkouts
            if candidate.repository == checkout.repository
        ]
        if len(variants) < 2:
            return remote

        branch = git(
            path, "branch", "--show-current", capture=True, trace=trace
        )
        if branch == checkout.branch:
            return remote

        primary = self._primary_path(checkout)
        if not primary.is_dir() or primary.is_symlink():
            raise WorkspaceError(
                f"cannot prove {checkout.name}@{checkout.branch} lineage for {path}; "
                f"initialize its primary checkout first: {primary}"
            )
        self._validate_primary_checkout(checkout, primary, trace=trace)
        selected_common = self._git_common_directory(path, trace=trace)
        primary_common = self._git_common_directory(primary, trace=trace)
        if selected_common != primary_common:
            raise WorkspaceError(
                f"checkout cannot be proven to belong to {checkout.name}@"
                f"{checkout.branch}: {path}; use that exact branch or a worktree "
                f"attached to {primary}"
            )
        return remote

    @staticmethod
    def _git_common_directory(path: Path, *, trace: bool = True) -> Path:
        value = Path(
            git(
                path,
                "rev-parse",
                "--git-common-dir",
                capture=True,
                trace=trace,
            )
        )
        return value.resolve() if value.is_absolute() else (path / value).resolve()

    def _load_profile(self, name: str, require_file: bool) -> dict[str, Any]:
        snapshot = self._profile_snapshot
        if snapshot is not None and snapshot.name == name and not require_file:
            return snapshot.profile()
        return self._load_profile_file(name, require_file)

    def _load_profile_file(self, name: str, require_file: bool) -> dict[str, Any]:
        validate_name(name, "profile name")
        if name in self.manifest.stacks and not require_file:
            stack = self.manifest.stack(name)
            return {
                "schema_version": PROFILE_SCHEMA_VERSION,
                "name": name,
                "stack": name,
                "sound_mode": SOURCE_MODE,
                "sound_release": None,
                "components": {
                    component.name: {"kind": "primary", "value": ""}
                    for component in stack.components
                },
            }
        path = self.paths.profiles / f"{name}.json"
        if path.is_symlink() or not path.is_file():
            raise WorkspaceError(f"profile does not exist: {name}")
        profile = load_regular_json(path, f"profile {name}")
        if not isinstance(profile, dict):
            raise WorkspaceError(f"profile must be an object: {name}")
        schema_version = profile.get("schema_version")
        if schema_version == OLDEST_PROFILE_SCHEMA_VERSION:
            require_keys(profile, OLDEST_PROFILE_KEYS, f"profile {name}")
            profile = {
                **profile,
                "schema_version": PROFILE_SCHEMA_VERSION,
                "sound_mode": SOURCE_MODE,
                "sound_release": None,
            }
        elif schema_version == LEGACY_PROFILE_SCHEMA_VERSION:
            require_keys(profile, LEGACY_PROFILE_KEYS, f"profile {name}")
            profile = {
                **profile,
                "schema_version": PROFILE_SCHEMA_VERSION,
                "sound_release": None,
            }
        else:
            require_keys(profile, PROFILE_KEYS, f"profile {name}")
        if profile["schema_version"] != PROFILE_SCHEMA_VERSION or profile["name"] != name:
            raise WorkspaceError(f"profile identity/schema mismatch: {name}")
        if (
            not isinstance(profile["sound_mode"], str)
            or profile["sound_mode"] not in SOUND_MODES
        ):
            raise WorkspaceError(f"profile sound mode is invalid: {name}")
        stack_name = profile["stack"]
        if not isinstance(stack_name, str) or stack_name not in self.manifest.stacks:
            raise WorkspaceError(f"profile stack is invalid: {name}")
        if profile["sound_mode"] != SOURCE_MODE and stack_name != "classic":
            raise WorkspaceError(
                f"non-source sound mode requires a Classic-derived profile: {name}"
            )
        if profile["sound_mode"] == RELEASED_MODE:
            try:
                validate_release_coordinates(profile["sound_release"])
            except WorkspaceError as error:
                raise WorkspaceError(
                    f"profile released sound coordinates are invalid: {name}: {error}"
                ) from error
        elif profile["sound_release"] is not None:
            raise WorkspaceError(
                f"profile sound release must be null outside released mode: {name}"
            )
        stack = self.manifest.stack(stack_name)
        selectors = profile["components"]
        expected = {component.name for component in stack.components}
        if not isinstance(selectors, dict) or set(selectors) != expected:
            raise WorkspaceError(f"profile component set does not match manifest: {name}")
        checkout_selectors: dict[str, dict[str, Any]] = {}
        for component_name, selector in selectors.items():
            if not isinstance(selector, dict):
                raise WorkspaceError(f"profile selector must be an object: {component_name}")
            require_keys(selector, SELECTOR_KEYS, f"profile selector {component_name}")
            kind = selector["kind"]
            value = selector["value"]
            if (
                not isinstance(kind, str)
                or kind
                not in {
                    "primary",
                    "worktree",
                    "path",
                    MIGRATED_CONTENT_WORKTREE_KIND,
                }
                or not isinstance(value, str)
            ):
                raise WorkspaceError(f"invalid profile selector: {component_name}")
            if kind == "primary" and value:
                raise WorkspaceError(f"primary selector must not have a value: {component_name}")
            if kind == "worktree":
                validate_name(value, f"profile selector {component_name}")
            selected_path: Path | None = None
            if kind in {"path", MIGRATED_CONTENT_WORKTREE_KIND}:
                selected_path = Path(value)
                if kind == "path" and not selected_path.is_absolute():
                    raise WorkspaceError(
                        f"profile path must be absolute: {component_name}"
                    )
                try:
                    selected_path = selected_path.resolve(strict=False)
                except (OSError, RuntimeError, ValueError) as error:
                    raise WorkspaceError(
                        f"invalid profile selector: {component_name}"
                    ) from error
                if kind == "path":
                    selector = {"kind": kind, "value": str(selected_path)}
                    profile["components"][component_name] = selector
            if kind == MIGRATED_CONTENT_WORKTREE_KIND:
                expected_parent = (self.paths.worktrees / "content").resolve()
                if (
                    component_name != "content-1x"
                    or selected_path is None
                    or not Path(value).is_absolute()
                    or selected_path.parent != expected_parent
                ):
                    raise WorkspaceError(
                        "invalid migrated content worktree selector: "
                        f"{component_name}"
                    )
            checkout_name = self.manifest.by_name[component_name].checkout_name
            previous = checkout_selectors.setdefault(checkout_name, selector)
            if selector != previous:
                raise WorkspaceError(
                    "profile selectors for components in one checkout must match: "
                    f"{name}/{checkout_name}"
                )
        return profile

    def profile_summary(self, name: str) -> dict[str, Any]:
        profile = self._load_profile(name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        rows: list[dict[str, Any]] = []
        for component in stack.components:
            selector_root = self._selector_root(profile, component)
            checkout_root = selector_root.resolve()
            path = (
                checkout_root
                if component.source == "."
                else checkout_root.joinpath(*PurePosixPath(component.source).parts)
            )
            row: dict[str, Any] = {
                "component": component.name,
                "checkout": component.checkout_name,
                "repository": component.repository,
                "branch": component.branch,
                "source": component.source,
                "build": self.manifest.effective_build(stack.name, component),
                "roles": sorted(component.provides),
                "path": str(path),
                "checkout_path": str(checkout_root),
                "initialized": False,
                "head": None,
                "dirty": None,
            }
            if selector_root.exists() or selector_root.is_symlink():
                selector = profile["components"][component.name]
                self._validate_selected_checkout(
                    component, selector_root, selector["kind"], trace=False
                )
                path = self._component_source(component, selector_root)
                row["path"] = str(path)
                row.update(
                    {
                        "initialized": True,
                        "head": git(
                            checkout_root,
                            "rev-parse",
                            "--short=12",
                            "HEAD",
                            capture=True,
                            trace=False,
                        ),
                        "dirty": not _is_clean(checkout_root, trace=False),
                    }
                )
            rows.append(row)
        return {
            "name": name,
            "stack": stack.name,
            "sound_mode": profile["sound_mode"],
            "sound_release": profile["sound_release"],
            "components": rows,
        }

    def component_path(self, component_name: str, profile_name: str) -> Path:
        profile = self._load_profile(profile_name, require_file=False)
        component = self._profile_component(profile, component_name)
        return self.resolve_profile(
            profile_name, {component.name}, trace=False
        )[component.name]

    def _classic_requires(
        self, component: Component, stack_name: str
    ) -> tuple[str, ...]:
        if component.requires:
            return component.requires
        return {
            "classic-client": ("sound", "libatrinik", "protocol"),
            "classic-server": ("content", "resources", "libatrinik", "protocol"),
            "classic-library": ("protocol",),
        }.get(self.manifest.effective_build(stack_name, component), ())

    def _dependency_roles(
        self, profile: dict[str, Any], requested: set[str]
    ) -> set[str]:
        stack = self.manifest.stack(profile["stack"])
        unknown = sorted(requested - set(stack.providers))
        if unknown:
            raise WorkspaceError(
                f"{stack.name} stack has no provider for roles: {', '.join(unknown)}"
            )
        resolved: set[str] = set()
        pending = deque(sorted(requested))
        while pending:
            role = pending.popleft()
            if role in resolved:
                continue
            resolved.add(role)
            component = stack.providers[role]
            for requirement in self._classic_requires(component, stack.name):
                if requirement not in stack.providers:
                    raise WorkspaceError(
                        f"{component.name} requires role {requirement}, which has no "
                        f"provider in {stack.name} stack"
                    )
                if requirement not in resolved:
                    pending.append(requirement)
        return resolved

    def _require_classic_contracts(
        self, profile_name: str, requested: set[str]
    ) -> None:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        expected = {
            "client": "classic-client",
            "server": "classic-server",
        }
        for role in sorted(requested):
            component = stack.providers.get(role)
            adapter = expected.get(role)
            if component is None:
                raise WorkspaceError(
                    f"{stack.name} stack has no provider for runtime role {role}"
                )
            if (
                adapter is not None
                and self.manifest.effective_build(stack.name, component) != adapter
            ):
                raise WorkspaceError(
                    f"{component.name} has no wrapper build/runtime contract yet "
                    f"for the {stack.name} stack"
                )

    def _resolve_build_profile(
        self,
        profile_name: str,
        required: set[str],
        *,
        trace: bool = True,
        profile: dict[str, Any] | None = None,
    ) -> dict[str, Path]:
        if profile is None:
            profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        roles = self._dependency_roles(profile, required)
        component_names = {stack.providers[role].name for role in roles}
        present_paths = self.resolve_profile(
            profile_name,
            component_names,
            trace=trace,
            profile=profile,
        )
        paths = {
            component_name: present_paths[component_name]
            for component_name in component_names
        }
        return {role: paths[stack.providers[role].name] for role in roles}

    def build(
        self,
        target: str,
        profile_name: str,
        tests: bool,
        *,
        force_reconfigure: bool = False,
        use_ccache: bool = True,
    ) -> Path:
        self.paths.ensure()
        targets = self._expand_build_target(target, profile_name)
        with self._resolved_profile_operation(
            profile_name,
            set(targets),
            f"build {target}",
            materialize_clean_primaries=True,
        ) as snapshot:
            return self._build_resolved(
                target,
                profile_name,
                tests,
                targets,
                snapshot.paths(),
                force_reconfigure=force_reconfigure,
                use_ccache=use_ccache,
            )

    def _build(
        self,
        target: str,
        profile_name: str,
        tests: bool,
        *,
        force_reconfigure: bool = False,
        use_ccache: bool = True,
    ) -> Path:
        targets = self._expand_build_target(target, profile_name)
        required = set(targets)
        selected = self._resolve_build_profile(profile_name, required)
        return self._build_resolved(
            target,
            profile_name,
            tests,
            targets,
            selected,
            force_reconfigure=force_reconfigure,
            use_ccache=use_ccache,
        )

    def _build_resolved(
        self,
        target: str,
        profile_name: str,
        tests: bool,
        targets: list[str],
        selected: dict[str, Path],
        *,
        force_reconfigure: bool = False,
        use_ccache: bool = True,
    ) -> Path:
        key = self._profile_build_key(profile_name, selected)
        root = self.paths.builds / "profiles" / f"{profile_name}-{key}"
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        with self._profile_build_lock(root, profile_name):
            self._force_reconfigure = force_reconfigure
            self._use_ccache = use_ccache
            self._source_view_unchanged = {}
            managed_directory(root, self.paths.builds, f"profile:{profile_name}:{key}")
            sound_root = selected.get("sound")
            sound_record: dict[str, Any] | None = None
            if "client" in targets:
                if sound_root is not None:
                    sound_root, sound_record = self._prepare_sound(
                        root, selected, profile_name
                    )
            elif sound_root is not None:
                profile = self._load_profile(profile_name, require_file=False)
                if profile["sound_mode"] == SOURCE_MODE:
                    sound_record = self._sound_source_record(sound_root)
            self._refresh_build_metadata(
                root, profile_name, key, selected, sound_record
            )
            if "content" in targets or "server" in targets:
                self._collect_content(root, selected, profile_name)
            if "server" in targets:
                self._stage_resources(root, selected, profile_name)
            integrated_classic = self._uses_integrated_classic_build(
                targets, selected
            )
            if integrated_classic:
                if sound_root is None:
                    raise WorkspaceError(
                        f"integrated Classic profile {profile_name} has no sound provider"
                    )
                self._build_integrated_classic(
                    root, selected, tests, sound_root=sound_root
                )
            else:
                if "protocol" in targets:
                    self._build_protocol(root, selected, tests)
                if "libatrinik" in targets:
                    self._build_library(root, selected, tests)
                if "client" in targets:
                    self._build_client(
                        root,
                        selected,
                        tests,
                        component=stack.providers["client"],
                        sound_root=sound_root,
                    )
                if "server" in targets:
                    self._build_server(
                        root,
                        selected,
                        tests,
                        component=stack.providers["server"],
                    )
            if "server" in targets:
                self._generate_region_maps(root, profile_name, selected)
            if "metaserver-worker" in targets:
                self._build_worker(root, selected)
            if target in {"sound", "resources"}:
                print(f"{target}: selected {selected[target]}")
        return root

    @contextmanager
    def _profile_build_lock(
        self, root: Path, profile_name: str
    ) -> Iterator[TextIO]:
        lock = self.paths.builds / "locks" / f"{root.name}.lock"
        with exclusive_lock(lock, f"profile build {profile_name}") as lease:
            yield lease

    def _refresh_build_metadata(
        self,
        root: Path,
        profile_name: str,
        key: str,
        selected: dict[str, Path],
        sound: dict[str, Any] | None = None,
    ) -> None:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        checkout_states = self._selected_checkout_states(
            profile, selected, include_dirty=False
        )
        coordinates: dict[str, dict[str, Any]] = {}
        for role in sorted(selected):
            component = stack.providers[role]
            checkout_state = checkout_states[component.checkout_name]
            checkout_path = checkout_state["path"]
            coordinates[role] = {
                "component": component.name,
                "checkout": component.checkout_name,
                "repository": component.repository,
                "branch": component.branch,
                "source": component.source,
                "checkout_path": str(checkout_path),
                "source_path": str(selected[role].resolve()),
                "head": checkout_state["head"],
            }
            generation = self._source_generation_record(selected[role])
            if generation is not None:
                coordinates[role]["source_generation"] = {
                    **generation,
                    "path": str(selected[role].resolve()),
                }
        atomic_json(
            root / BUILD_METADATA,
            {
                "schema_version": BUILD_METADATA_SCHEMA_VERSION,
                "profile": profile_name,
                "key": key,
                "purpose": f"profile:{profile_name}:{key}",
                "coordinates": coordinates,
                "sound": sound,
                "last_used_at": datetime.now(timezone.utc).isoformat(),
            },
        )
        snapshot = self._profile_snapshot
        if snapshot is not None and snapshot.name == profile_name:
            atomic_json(
                root / PROFILE_RESOLUTION_METADATA,
                {
                    "schema_version": PROFILE_RESOLUTION_SCHEMA_VERSION,
                    "profile": profile_name,
                    "generation": snapshot.generation,
                    "stack": stack.name,
                    "stack_generation": stack.generation,
                    "sound_mode": profile["sound_mode"],
                    "selected": coordinates,
                    "checkouts": {
                        name: {**state, "path": str(state["path"])}
                        for name, state in snapshot.checkout_states().items()
                    },
                },
            )

    def _prepare_sound(
        self,
        root: Path,
        selected: dict[str, Path],
        profile_name: str,
    ) -> tuple[Path, dict[str, Any]]:
        source = selected["sound"]
        profile = self._load_profile(profile_name, require_file=False)
        mode = profile["sound_mode"]
        if mode == SOURCE_MODE:
            return source, self._sound_source_record(source)
        if profile["stack"] != "classic":
            raise WorkspaceError(
                f"profile {profile_name} has unsupported sound mode {mode}"
            )
        if mode == RELEASED_MODE:
            identity = "invalid or incomplete"
            try:
                coordinates = validate_release_coordinates(profile["sound_release"])
                identity = json.dumps(
                    coordinates, sort_keys=True, separators=(",", ":")
                )
                contract = (
                    f"product={RELEASE_PRODUCT}; paths={EXPECTED_PATHS}; "
                    f"vorbis={EXPECTED_COPIED_VORBIS}; opus={EXPECTED_CONVERTED_OPUS}; "
                    f"midi-sources={EXPECTED_SOURCE_MIDI}; "
                    f"flac-sources={EXPECTED_SOURCE_FLAC}"
                )
                root_fd = os.open(root, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
                runtime_fd: int | None = None
                try:
                    root_identity = os.fstat(root_fd)

                    @contextmanager
                    def pinned_temporary(prefix: str) -> Iterator[Path]:
                        created = Path(
                            tempfile.mkdtemp(
                                prefix=prefix, dir=f"/proc/self/fd/{runtime_fd}"
                            )
                        )
                        name = created.name
                        descriptor = os.open(
                            name,
                            os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                            dir_fd=runtime_fd,
                        )
                        identity = os.fstat(descriptor)
                        mount_id = _descriptor_mount_id(descriptor)
                        try:
                            yield Path(f"/proc/self/fd/{descriptor}")
                        finally:
                            try:
                                _prepare_owned_tree_removal(
                                    descriptor,
                                    identity.st_dev,
                                    mount_id,
                                    created,
                                    stat.S_IMODE(identity.st_mode),
                                    reject_links=True,
                                )
                                _remove_owned_tree_contents(
                                    descriptor,
                                    identity.st_dev,
                                    mount_id,
                                    created,
                                    reject_links=True,
                                )
                                visible = os.stat(
                                    name,
                                    dir_fd=runtime_fd,
                                    follow_symlinks=False,
                                )
                                if (
                                    not stat.S_ISDIR(visible.st_mode)
                                    or (visible.st_dev, visible.st_ino)
                                    != (identity.st_dev, identity.st_ino)
                                ):
                                    raise WorkspaceError(
                                        "released sound temporary directory changed"
                                    )
                                os.rmdir(name, dir_fd=runtime_fd)
                            finally:
                                os.close(descriptor)

                    try:
                        os.mkdir("runtime", dir_fd=root_fd)
                    except FileExistsError:
                        pass
                    runtime_fd = os.open(
                        "runtime",
                        os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                        dir_fd=root_fd,
                    )
                    runtime_identity = os.fstat(runtime_fd)
                    runtime = Path(f"/proc/self/fd/{runtime_fd}")
                    archive_name = (
                        f"sound-released-{release_cache_key(coordinates)}.tar.gz"
                    )
                    archive_path = runtime / archive_name
                    staged = runtime / "sound-released"
                    archive_present = archive_path.exists() or archive_path.is_symlink()
                    tree_present = staged.exists() or staged.is_symlink()
                    if tree_present and not archive_present:
                        raise WorkspaceError(
                            "released sound cache lacks its verified archive; "
                            "use preview-first build cleanup"
                        )
                    if archive_present:
                        verify_release_archive(
                            archive_path, coordinates, "cached released sound archive"
                        )
                        if not tree_present:
                            with pinned_temporary(
                                ".sound-released-recovery-"
                            ) as temporary:
                                candidate_tree = temporary / "tree"
                                extract_release_archive(
                                    archive_path, candidate_tree, coordinates
                                )
                                verify_release_tree(candidate_tree, coordinates)
                                rename_no_replace(candidate_tree, staged)
                        record = verify_release_tree(staged, coordinates)
                    else:
                        with pinned_temporary(".sound-released-") as temporary_root:
                            candidate_archive = temporary_root / "archive.tar.gz"
                            candidate_tree = temporary_root / "tree"
                            download_release_archive(
                                coordinates["asset_url"], candidate_archive
                            )
                            verify_release_archive(
                                candidate_archive,
                                coordinates,
                                "downloaded released sound archive",
                            )
                            extract_release_archive(
                                candidate_archive, candidate_tree, coordinates
                            )
                            candidate_record = verify_release_tree(
                                candidate_tree, coordinates
                            )
                            rename_no_replace(candidate_archive, archive_path)
                            try:
                                rename_no_replace(candidate_tree, staged)
                            except BaseException:
                                archive_path.unlink(missing_ok=True)
                                raise
                        record = verify_release_tree(staged, coordinates)
                        if {
                            **record,
                            "root": "<verified-root>",
                        } != {
                            **candidate_record,
                            "root": "<verified-root>",
                        }:
                            raise WorkspaceError(
                                "installed released sound handoff differs from verified download"
                            )
                    current_runtime = os.stat(
                        "runtime", dir_fd=root_fd, follow_symlinks=False
                    )
                    if (
                        not stat.S_ISDIR(current_runtime.st_mode)
                        or (current_runtime.st_dev, current_runtime.st_ino)
                        != (runtime_identity.st_dev, runtime_identity.st_ino)
                    ):
                        raise WorkspaceError(
                            "released sound handoff parent changed during publication"
                        )
                    visible_root = os.stat(root, follow_symlinks=False)
                    if (
                        not stat.S_ISDIR(visible_root.st_mode)
                        or (visible_root.st_dev, visible_root.st_ino)
                        != (root_identity.st_dev, root_identity.st_ino)
                    ):
                        raise WorkspaceError(
                            "profile build root changed during released sound publication"
                        )
                    staged = root / "runtime" / "sound-released"
                    record["root"] = str(staged)
                finally:
                    if runtime_fd is not None:
                        os.close(runtime_fd)
                    os.close(root_fd)
            except (OSError, WorkspaceError) as error:
                raise WorkspaceError(
                    f"profile {profile_name} mode {mode} coordinates ({identity}) "
                    f"expected contract ({contract if 'contract' in locals() else RELEASE_PRODUCT}) "
                    f"failed check: {error}"
                ) from error
            print(
                "sound: staged released tree "
                f"{record['output_tree_sha256']} for {coordinates['tag']}"
            )
            return staged, record
        if mode != PLAYTEST_MODE:
            raise WorkspaceError(
                f"profile {profile_name} has unsupported sound mode {mode}"
            )
        source_identity = "unavailable"
        try:
            inputs = self._clean_sound_source_inputs(source)
            source_identity = (
                f"commit {inputs['source_commit']} tree {inputs['source_tree']}"
            )
            if self._source_generation_record(source) is not None:
                producer = root / "producers" / "sound"
                producer.mkdir(parents=True, exist_ok=True)
                output = producer / sound_cache_key(inputs)
            else:
                output = source / "build" / "atrinik-workspace" / sound_cache_key(inputs)
            builder = source / "tools" / "sound_release.py"
            if not builder.is_file() or builder.is_symlink():
                raise WorkspaceError(
                    "selected sound checkout lacks the public "
                    "tools/sound_release.py builder"
                )
            run(
                [
                    sys.executable,
                    str(builder),
                    "build-playtest-tree",
                    str(output),
                ],
                cwd=source,
            )
            run(
                [
                    sys.executable,
                    str(builder),
                    "verify-playtest-tree",
                    str(output),
                ],
                cwd=source,
            )
            record = verify_playtest_tree(source, output, inputs)
            if self._clean_sound_source_inputs(source) != inputs:
                raise WorkspaceError(
                    "selected sound inputs changed after playtest-tree generation"
                )
            staged = root / "runtime" / "sound-local-playtest"
            runtime = staged.parent
            if runtime.exists() or runtime.is_symlink():
                if runtime.is_symlink() or not runtime.is_dir():
                    raise WorkspaceError(
                        "local-playtest handoff parent is not a safe directory"
                    )
            else:
                runtime.mkdir()
            try:
                if runtime.resolve() != root.resolve() / "runtime":
                    raise WorkspaceError(
                        "local-playtest handoff parent escapes the profile build"
                    )
            except RuntimeError as error:
                raise WorkspaceError(
                    "local-playtest handoff parent cannot be resolved"
                ) from error

            def same_tree(left: dict[str, Any], right: dict[str, Any]) -> bool:
                return {**left, "root": "<verified-root>"} == {
                    **right,
                    "root": "<verified-root>",
                }

            if staged.exists() or staged.is_symlink():
                staged_record = verify_playtest_tree(source, staged, inputs)
                if not same_tree(record, staged_record):
                    raise WorkspaceError(
                        "cached local-playtest handoff differs from producer verification"
                    )
            else:
                with tempfile.TemporaryDirectory(
                    prefix=".sound-local-playtest-", dir=staged.parent
                ) as temporary:
                    candidate = Path(temporary) / "tree"
                    shutil.copytree(output, candidate, symlinks=True)
                    staged_record = verify_playtest_tree(source, candidate, inputs)
                    if not same_tree(record, staged_record):
                        raise WorkspaceError(
                            "local-playtest sound changed while creating the verified handoff"
                        )
                    if self._clean_sound_source_inputs(source) != inputs:
                        raise WorkspaceError(
                            "selected sound inputs changed while staging the verified handoff"
                        )
                    rename_no_replace(candidate, staged)
                staged_record = verify_playtest_tree(source, staged, inputs)
                if not same_tree(record, staged_record):
                    raise WorkspaceError(
                        "installed local-playtest handoff differs from producer verification"
                    )
            record = staged_record
            output = staged
        except (OSError, WorkspaceError) as error:
            raise WorkspaceError(
                f"profile {profile_name} mode {mode} sound source {source} "
                f"({source_identity}) failed contract: {error}"
            ) from error
        print(
            "sound: staged local-playtest tree "
            f"{record['output_tree_sha256']} at {output}"
        )
        return output, record

    def _clean_sound_source_inputs(self, source: Path) -> dict[str, str]:
        generation = self._source_generation_record(source)
        if generation is None:
            return clean_source_inputs(source)
        files = {
            "builder_sha256": source / "tools" / "sound_release.py",
            "source_manifest_sha256": source / "manifests" / "source-assets.json",
            "toolchain_sha256": source
            / "manifests"
            / "playtest-audio-toolchain.json",
            "schema_sha256": source
            / "schemas"
            / "playtest-manifest-v1.schema.json",
        }
        return {
            "source_commit": generation["commit"],
            "source_tree": generation["tree"],
            **{
                name: _file_digest(path, name.replace("_", " "))
                for name, path in files.items()
            },
        }

    def _sound_source_record(self, source: Path) -> dict[str, Any]:
        generation = self._source_generation_record(source)
        if generation is None:
            return sound_source_record(source)
        return {
            "mode": SOURCE_MODE,
            "root": str(source.resolve()),
            "source_commit": generation["commit"],
            "source_tree": generation["tree"],
            "source_clean": True,
        }

    def _selected_checkout_states(
        self,
        profile: dict[str, Any],
        selected: dict[str, Path],
        *,
        include_dirty: bool,
        include_identity: bool = False,
    ) -> dict[str, dict[str, Any]]:
        snapshot = self._profile_snapshot
        if snapshot is not None and snapshot.paths() == {
            role: path.resolve() for role, path in selected.items()
        }:
            states = snapshot.checkout_states()
            if not include_dirty:
                for state in states.values():
                    state.pop("dirty", None)
            return states
        stack = self.manifest.stack(profile["stack"])
        states: dict[str, dict[str, Any]] = {}
        for role in sorted(selected):
            component = stack.providers[role]
            if component.checkout_name in states:
                continue
            checkout = self._selector_root(profile, component).resolve()
            state: dict[str, Any] = {
                "path": checkout,
                "head": git(
                    checkout,
                    "rev-parse",
                    "HEAD",
                    capture=True,
                    trace=False,
                ),
            }
            if include_identity:
                identity = checkout.stat()
                git_common = self._git_common_directory(checkout, trace=False)
                git_common_identity = git_common.stat()
                state.update(
                    {
                        "device": identity.st_dev,
                        "inode": identity.st_ino,
                        "git_common": str(git_common),
                        "git_common_device": git_common_identity.st_dev,
                        "git_common_inode": git_common_identity.st_ino,
                        "sources": {},
                    }
                )
            if include_dirty:
                state["dirty"] = not _is_clean(checkout, trace=False)
            states[component.checkout_name] = state
        if include_identity:
            for role in sorted(selected):
                component = stack.providers[role]
                source = selected[role].resolve()
                identity = source.stat()
                states[component.checkout_name]["sources"][component.source] = {
                    "path": str(source),
                    "device": identity.st_dev,
                    "inode": identity.st_ino,
                }
        return states

    def _profile_build_key(
        self, profile_name: str, selected: dict[str, Path]
    ) -> str:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        providers = json.dumps(
            {
                role: {
                    "name": stack.providers[role].name,
                    "repository": stack.providers[role].repository,
                    "branch": stack.providers[role].branch,
                    "checkout": stack.providers[role].checkout_name,
                    "source": stack.providers[role].source,
                    "source_includes": stack.providers[role].source_includes,
                }
                for role in sorted(selected)
            },
            sort_keys=True,
            separators=(",", ":"),
        )
        namespace = (
            f"profile-schema:{PROFILE_SCHEMA_VERSION};stack:{stack.name};"
            f"generation:{stack.generation};sound-mode:{profile['sound_mode']};"
            "sound-release:"
            f"{json.dumps(profile['sound_release'], sort_keys=True, separators=(',', ':'))};"
            f"providers:{providers}"
        )
        return profile_key(selected, namespace=namespace)

    def _expand_build_target(self, target: str, profile_name: str) -> list[str]:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        if target == "all":
            targets = [role for role in ALL_BUILD_TARGETS if role in stack.providers]
        elif target in stack.providers:
            targets = [target]
        else:
            component = self._profile_component(profile, target)
            targets = [
                role
                for role, provider in stack.providers.items()
                if provider.name == component.name
            ]
        for role in targets:
            component = stack.providers[role]
            if self.manifest.effective_build(stack.name, component) == "none":
                raise WorkspaceError(
                    f"{component.name} has no wrapper build/runtime contract yet "
                    f"for the {stack.name} stack"
                )
        return targets

    def _profile_source_view(
        self,
        root: Path,
        component: str,
        source: Path,
        exclusions: set[str],
        copied_directories: set[str] | None = None,
        copy_all: bool = False,
        preserved_entries: set[str] | None = None,
    ) -> Path:
        view = root / "sources" / component
        purpose = f"source-view:{component}"
        managed_directory(view, self.paths.builds, purpose)
        self._source_view_changed = False
        exclusions = {
            *exclusions,
            ".git",
            MANAGED_MARKER,
            SOURCE_VIEW_METADATA,
            SOURCE_INCLUDE_VIEW_METADATA,
        }
        copied_directories = copied_directories or set()
        mutable_copies = self._source_generation_record(source) is not None
        try:
            source_head: str | None = git(
                source, "rev-parse", "HEAD", capture=True, trace=False
            )
            if not isinstance(source_head, str) or len(source_head) != 40 or any(
                character not in "0123456789abcdef" for character in source_head
            ):
                raise WorkspaceError(f"invalid Git HEAD for source view: {source}")
            source_clean: bool | None = _is_clean(source, trace=False)
        except WorkspaceError:
            source_head = None
            source_clean = None
        expected: dict[str, dict[str, Any]] = {}
        for entry in sorted(source.iterdir(), key=lambda path: path.name):
            if entry.name in exclusions:
                continue
            destination = view / entry.name
            copy_entry = copy_all or entry.name in copied_directories
            if copy_entry:
                try:
                    mode = entry.lstat().st_mode
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot inspect source-view input {entry}: {error}"
                    ) from error
                if not stat.S_ISDIR(mode) and not copy_all:
                    raise WorkspaceError(
                        f"source-view copy input is not a directory: {entry}"
                    )
                if stat.S_ISDIR(mode):
                    digest = self._reconcile_source_tree(
                        entry,
                        destination,
                        entry,
                        source,
                        exclusions if copy_all else set(),
                        view,
                        exclusions,
                        mutable_copies,
                    )
                elif stat.S_ISREG(mode):
                    permissions = stat.S_IMODE(mode)
                    if mutable_copies:
                        permissions |= 0o600
                    value = hashlib.sha256(entry.read_bytes()).hexdigest()
                    same = (
                        destination.is_file()
                        and not destination.is_symlink()
                        and hashlib.sha256(destination.read_bytes()).hexdigest() == value
                        and stat.S_IMODE(destination.lstat().st_mode) == permissions
                    )
                    if not same:
                        self._source_view_changed = True
                        self._remove_source_view_entry(destination)
                        shutil.copy2(entry, destination, follow_symlinks=False)
                        destination.chmod(permissions)
                    digest = f"{permissions:o}:{value}"
                elif stat.S_ISLNK(mode):
                    target, resolved_target = self._copied_source_symlink_target(
                        entry, destination, source, view, exclusions, exclusions
                    )
                    if not destination.is_symlink() or os.readlink(destination) != target:
                        self._source_view_changed = True
                        self._remove_source_view_entry(destination)
                        destination.symlink_to(target)
                    digest = hashlib.sha256(
                        f"symlink:{os.readlink(entry)}:{resolved_target}".encode()
                    ).hexdigest()
                else:
                    raise WorkspaceError(
                        f"source-view copy input is not a regular entry: {entry}"
                    )
                expected[entry.name] = {"kind": "copy", "digest": digest}
            else:
                try:
                    mode = entry.lstat().st_mode
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot inspect source-view input {entry}: {error}"
                    ) from error
                if stat.S_ISLNK(mode):
                    self._validate_source_symlink(entry, source)
                    source_symlinks = {".": os.readlink(entry)}
                    source_structure = hashlib.sha256(
                        f"symlink:{os.readlink(entry)}".encode()
                    ).hexdigest()
                elif not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                    raise WorkspaceError(
                        f"source-view input is not a file or directory: {entry}"
                    )
                elif stat.S_ISDIR(mode):
                    source_symlinks, source_structure = (
                        self._validate_source_tree_symlinks(
                            entry, source, hash_contents=source_clean is None
                        )
                    )
                else:
                    source_symlinks = {}
                    source_structure = (
                        hashlib.sha256(entry.read_bytes()).hexdigest()
                        if source_clean is None
                        else f"file:{stat.S_IMODE(mode):o}"
                    )
                target = str(entry)
                if not destination.is_symlink() or os.readlink(destination) != target:
                    self._source_view_changed = True
                    self._remove_source_view_entry(destination)
                    destination.symlink_to(target, target_is_directory=stat.S_ISDIR(mode))
                expected[entry.name] = {
                    "kind": "link",
                    "target": target,
                    "source_symlinks": source_symlinks,
                    "source_structure": source_structure,
                }
        reserved = {
            MANAGED_MARKER,
            SOURCE_VIEW_METADATA,
            *(preserved_entries or set()),
        }
        for destination in sorted(view.iterdir(), key=lambda path: path.name):
            if destination.name not in reserved and destination.name not in expected:
                self._remove_source_view_entry(destination)
        metadata = {
            "schema_version": SOURCE_VIEW_SCHEMA_VERSION,
            "purpose": purpose,
            "source": str(source.resolve()),
            "source_head": source_head,
            "entries": expected,
        }
        metadata_path = view / SOURCE_VIEW_METADATA
        unchanged = False
        if metadata_path.is_file() and not metadata_path.is_symlink():
            try:
                unchanged = (
                    load_json(metadata_path) == metadata
                    and not self._source_view_changed
                    and source_clean is not False
                )
            except WorkspaceError:
                unchanged = False
        if not unchanged:
            atomic_json(metadata_path, metadata)
        self._source_view_unchanged[str(view.resolve())] = unchanged
        return view

    def _runtime_input_coordinates(
        self,
        profile_name: str,
        selected: dict[str, Path],
        role: str,
    ) -> tuple[dict[str, Any], bool]:
        profile = self._load_profile(profile_name, require_file=False)
        component = self.manifest.stack(profile["stack"]).providers[role]
        checkout = self._selector_root(profile, component).resolve()
        generation = self._source_generation_record(selected[role])
        if generation is not None:
            clean = True
            head = generation["commit"]
        else:
            state = self._selected_checkout_states(
                profile, selected, include_dirty=True
            )[component.checkout_name]
            clean = not state["dirty"]
            head = state["head"]
        coordinate = {
            "component": component.name,
            "repository": component.repository,
            "branch": component.branch,
            "checkout": component.checkout_name,
            "source": component.source,
            "checkout_path": str(checkout),
            "source_path": str(selected[role].resolve()),
            "head": head,
        }
        return (
            {
                "schema_version": RUNTIME_INPUT_SCHEMA_VERSION,
                "cacheable": clean,
                "coordinate": coordinate,
            },
            clean,
        )

    def _source_view_link(
        self,
        view: Path,
        relative: str,
        target: Path,
        *,
        target_is_directory: bool,
    ) -> Path:
        relative_path = PurePosixPath(relative)
        if (
            relative_path.is_absolute()
            or not relative_path.parts
            or any(part in {"", ".", ".."} for part in relative_path.parts)
        ):
            raise WorkspaceError(f"invalid source-view link path: {relative}")
        destination = view.joinpath(*relative_path.parts)
        parent = destination.parent
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC
        try:
            descriptor = _open_directory_nofollow(parent, flags, create=True)
        except OSError as error:
            raise WorkspaceError(
                f"source-view link parent is unsafe: {parent}: {error}"
            ) from error
        expected = str(target)
        name = relative_path.name
        try:
            try:
                metadata = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
            except FileNotFoundError:
                metadata = None
            if (
                metadata is not None
                and stat.S_ISLNK(metadata.st_mode)
                and os.readlink(name, dir_fd=descriptor) == expected
            ):
                return destination
            if metadata is not None:
                if stat.S_ISDIR(metadata.st_mode):
                    raise WorkspaceError(
                        "source-view link destination is an unexpected directory: "
                        f"{destination}"
                    )
                os.unlink(name, dir_fd=descriptor)
            os.symlink(
                expected,
                name,
                target_is_directory=target_is_directory,
                dir_fd=descriptor,
            )
            self._source_view_unchanged[str(view.resolve())] = False
        finally:
            os.close(descriptor)
        return destination

    def _source_view_directory(
        self,
        view: Path,
        relative: str,
        preserved_entries: set[str] | None = None,
    ) -> Path:
        destination = view / relative
        if destination.is_symlink() or (
            destination.exists() and not destination.is_dir()
        ):
            self._remove_source_view_entry(destination)
        if not destination.exists():
            destination.mkdir(parents=True)
            self._source_view_unchanged[str(view.resolve())] = False
        for entry in sorted(destination.iterdir(), key=lambda path: path.name):
            if entry.name not in (preserved_entries or set()):
                self._remove_source_view_entry(entry)
                self._source_view_unchanged[str(view.resolve())] = False
        return destination

    def _remove_source_view_entry(self, path: Path) -> None:
        if not path.exists() and not path.is_symlink():
            return
        self._source_view_changed = True
        if path.is_symlink() or not path.is_dir():
            path.unlink()
        else:
            shutil.rmtree(path)

    @staticmethod
    def _validate_source_symlink(path: Path, root: Path) -> None:
        try:
            resolved = path.resolve(strict=True)
            source = root.resolve(strict=True)
        except (OSError, RuntimeError) as error:
            raise WorkspaceError(f"unsafe source-view symlink {path}: {error}") from error
        if resolved != source and source not in resolved.parents:
            raise WorkspaceError(f"source-view symlink escapes its source root: {path}")

    @classmethod
    def _validate_source_tree_symlinks(
        cls, path: Path, root: Path, *, hash_contents: bool
    ) -> tuple[dict[str, str], str]:
        def traversal_error(error: OSError) -> None:
            raise error

        symlinks: dict[str, str] = {}
        structure = hashlib.sha256()
        try:
            for directory, directories, files in os.walk(
                path, followlinks=False, onerror=traversal_error
            ):
                parent = Path(directory)
                directories.sort()
                files.sort()
                for name in (*directories, *files):
                    candidate = parent / name
                    relative = candidate.relative_to(path).as_posix()
                    mode = candidate.lstat().st_mode
                    structure.update(relative.encode())
                    if candidate.is_symlink():
                        cls._validate_source_symlink(candidate, root)
                        target = os.readlink(candidate)
                        symlinks[relative] = target
                        structure.update(f"\0symlink\0{target}\0".encode())
                    elif stat.S_ISDIR(mode):
                        structure.update(b"\0directory\0")
                    elif stat.S_ISREG(mode):
                        structure.update(b"\0file\0")
                        if hash_contents:
                            structure.update(
                                hashlib.sha256(candidate.read_bytes()).digest()
                            )
                    else:
                        raise WorkspaceError(
                            f"source-view input is not a regular entry: {candidate}"
                        )
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect source-view directory {path}: {error}"
            ) from error
        return symlinks, structure.hexdigest()

    def _reconcile_source_tree(
        self,
        source: Path,
        destination: Path,
        root: Path,
        safety_root: Path,
        exclusions: set[str],
        view: Path,
        top_level_exclusions: set[str],
        mutable_copies: bool = False,
    ) -> str:
        if destination.is_symlink() or (destination.exists() and not destination.is_dir()):
            self._remove_source_view_entry(destination)
        if not destination.exists():
            self._source_view_changed = True
        destination.mkdir(parents=True, exist_ok=True)
        source_permissions = stat.S_IMODE(source.lstat().st_mode)
        target_permissions = source_permissions | 0o700 if mutable_copies else source_permissions
        destination_permissions = stat.S_IMODE(destination.lstat().st_mode)
        if destination_permissions != target_permissions:
            self._source_view_changed = True
        working_permissions = target_permissions | 0o700
        if destination_permissions != working_permissions:
            destination.chmod(working_permissions)
        digest = hashlib.sha256()
        digest.update(f"directory:{target_permissions:o}\0".encode())
        expected: set[str] = set()
        for entry in sorted(source.iterdir(), key=lambda path: path.name):
            if entry.name in exclusions:
                continue
            expected.add(entry.name)
            output = destination / entry.name
            relative = entry.relative_to(root).as_posix()
            try:
                mode = entry.lstat().st_mode
            except OSError as error:
                raise WorkspaceError(f"cannot inspect copied source {entry}: {error}") from error
            digest.update(relative.encode())
            if stat.S_ISDIR(mode):
                digest.update(b"\0directory\0")
                digest.update(
                    self._reconcile_source_tree(
                        entry,
                        output,
                        root,
                        safety_root,
                        exclusions,
                        view,
                        top_level_exclusions,
                        mutable_copies,
                    ).encode()
                )
            elif stat.S_ISREG(mode):
                permissions = stat.S_IMODE(mode)
                if mutable_copies:
                    permissions |= 0o600
                file_digest = hashlib.sha256()
                try:
                    with entry.open("rb") as stream:
                        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                            file_digest.update(chunk)
                except OSError as error:
                    raise WorkspaceError(f"cannot read copied source {entry}: {error}") from error
                value = file_digest.hexdigest()
                digest.update(b"\0file\0")
                digest.update(f"{permissions:o}\0".encode())
                digest.update(value.encode())
                same = False
                if output.is_file() and not output.is_symlink():
                    existing = hashlib.sha256()
                    try:
                        with output.open("rb") as stream:
                            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                                existing.update(chunk)
                        same = (
                            existing.hexdigest() == value
                            and stat.S_IMODE(output.lstat().st_mode) == permissions
                        )
                    except OSError:
                        same = False
                if not same:
                    self._source_view_changed = True
                    self._remove_source_view_entry(output)
                    shutil.copy2(entry, output, follow_symlinks=False)
                    output.chmod(permissions)
            elif stat.S_ISLNK(mode):
                target, resolved_target = self._copied_source_symlink_target(
                    entry,
                    output,
                    safety_root,
                    view,
                    exclusions,
                    top_level_exclusions,
                )
                digest.update(b"\0symlink\0")
                digest.update(os.readlink(entry).encode())
                digest.update(b"\0resolved\0")
                digest.update(resolved_target.encode())
                if not output.is_symlink() or os.readlink(output) != target:
                    self._source_view_changed = True
                    self._remove_source_view_entry(output)
                    output.symlink_to(target)
            else:
                raise WorkspaceError(f"copied source is not a regular entry: {entry}")
        for output in sorted(destination.iterdir(), key=lambda path: path.name):
            if output.name not in expected:
                self._remove_source_view_entry(output)
        destination.chmod(target_permissions)
        return digest.hexdigest()

    @classmethod
    def _copied_source_symlink_target(
        cls,
        source: Path,
        destination: Path,
        safety_root: Path,
        view: Path,
        recursive_exclusions: set[str],
        top_level_exclusions: set[str],
    ) -> tuple[str, str]:
        cls._validate_source_symlink(source, safety_root)
        try:
            relative = source.resolve(strict=True).relative_to(
                safety_root.resolve(strict=True)
            )
        except (OSError, RuntimeError, ValueError) as error:
            raise WorkspaceError(
                f"unsafe copied source-view symlink {source}: {error}"
            ) from error
        if (
            (relative.parts and relative.parts[0] in top_level_exclusions)
            or any(part in recursive_exclusions for part in relative.parts)
        ):
            raise WorkspaceError(
                f"copied source-view symlink targets an excluded entry: {source}"
            )
        mapped = view / relative
        return os.path.relpath(mapped, destination.parent), relative.as_posix()

    @staticmethod
    def _validate_collected_content(
        path: Path,
        coordinate: dict[str, str],
        adapter: str,
        *,
        require_metadata: bool = True,
    ) -> None:
        if not path.is_dir() or path.is_symlink():
            raise WorkspaceError(f"collected content is not a directory: {path}")
        for name in ("lib", "maps"):
            required = path / name
            if not required.is_dir() or required.is_symlink():
                raise WorkspaceError(
                    f"collected content lacks required directory: {required}"
                )
        required_files = ["manifest.json"]
        if adapter == "classic-content":
            required_files.append("compatibility.json")
        for name in required_files:
            required = path / name
            if not required.is_file() or required.is_symlink():
                raise WorkspaceError(
                    f"collected content lacks required file: {required}"
                )
        manifest = load_json(path / "manifest.json")
        if not isinstance(manifest, dict):
            raise WorkspaceError("collected content manifest is invalid")
        if adapter == "classic-content":
            expected_contract = {
                "schema_version": 1,
                "target": "classic",
                "component": "content",
                "repository": "atrinik/content",
                "branch": "main",
                "content_format": "classic-ads-v1",
                "artifact_format": "atrinik-classic-runtime-content-v1",
                "compatible_classic_releases": ">=5.10.1 <6.0.0",
                "consumers": [
                    "classic/client",
                    "classic/editor",
                    "classic/server",
                ],
                "replacement_ready": False,
                "replacement_toolkit_package": False,
            }
            compatibility = load_json(path / "compatibility.json")
            if compatibility != expected_contract:
                raise WorkspaceError(
                    "collected Classic content compatibility contract is invalid"
                )
            if (
                set(manifest)
                != {
                    "schema_version",
                    "target",
                    "source",
                    "release_version",
                    "content_format",
                    "artifact_format",
                    "compatible_classic_releases",
                    "consumers",
                    "replacement_ready",
                    "replacement_toolkit_package",
                    "license_files",
                    "files",
                }
                or manifest.get("schema_version") != 2
                or manifest.get("target") != "classic"
                or manifest.get("source")
                != {
                    "repository": coordinate["repository"],
                    "branch": coordinate["branch"],
                    "commit": coordinate["head"],
                }
                or manifest.get("release_version") != "unreleased"
                or any(
                    manifest.get(key) != expected_contract[key]
                    for key in (
                        "content_format",
                        "artifact_format",
                        "compatible_classic_releases",
                        "consumers",
                        "replacement_ready",
                        "replacement_toolkit_package",
                    )
                )
                or not isinstance(manifest.get("license_files"), list)
                or not isinstance(manifest.get("files"), list)
            ):
                raise WorkspaceError("collected Classic content manifest is invalid")
        elif adapter == "none":
            if (
                set(manifest) != {"schema_version", "source_commit", "files"}
                or manifest.get("schema_version") != 1
                or manifest.get("source_commit") != coordinate["head"]
                or not isinstance(manifest.get("files"), list)
            ):
                raise WorkspaceError("collected default content manifest is invalid")
        else:
            raise WorkspaceError(f"unsupported content build adapter: {adapter}")
        expected = {MANAGED_MARKER, "manifest.json"}
        if require_metadata:
            expected.add(RUNTIME_INPUT_METADATA)
        for entry in manifest["files"]:
            if not isinstance(entry, dict) or set(entry) != {"path", "sha256", "size"}:
                raise WorkspaceError("collected content manifest file entry is invalid")
            relative = entry["path"]
            relative_path = PurePosixPath(relative) if isinstance(relative, str) else None
            if (
                relative_path is None
                or relative_path.is_absolute()
                or relative != relative_path.as_posix()
                or any(part in {"", ".", ".."} for part in relative_path.parts)
                or relative in expected
                or not isinstance(entry["size"], int)
                or isinstance(entry["size"], bool)
                or entry["size"] < 0
                or not isinstance(entry["sha256"], str)
                or re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) is None
            ):
                raise WorkspaceError("collected content manifest file entry is invalid")
            expected.add(relative)
        actual: set[str] = set()
        for directory, dirnames, filenames in os.walk(path, followlinks=False):
            directory_path = Path(directory)
            for name in dirnames:
                child = directory_path / name
                if child.is_symlink():
                    raise WorkspaceError(f"collected content contains a link: {child}")
            for name in filenames:
                child = directory_path / name
                try:
                    mode = child.lstat().st_mode
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot inspect collected content {child}: {error}"
                    ) from error
                if not stat.S_ISREG(mode):
                    raise WorkspaceError(
                        f"collected content contains a non-regular file: {child}"
                    )
                relative = child.relative_to(path).as_posix()
                actual.add(relative)
        if actual != expected:
            raise WorkspaceError("collected content does not match its manifest")
        entries = {entry["path"]: entry for entry in manifest["files"]}
        if adapter == "classic-content":
            licenses = manifest["license_files"]
            license_entries: dict[str, dict[str, Any]] = {}
            for entry in licenses:
                if (
                    not isinstance(entry, dict)
                    or set(entry) != {"path", "sha256", "size"}
                    or not isinstance(entry.get("path"), str)
                    or not entry["path"].startswith("attribution/")
                    or PurePosixPath(entry["path"]).name not in {"COPYING", "LICENSE"}
                    or entries.get(entry["path"]) != entry
                ):
                    raise WorkspaceError(
                        "collected Classic content license entry is invalid"
                )
                license_entries[entry["path"]] = entry
            expected_license_paths = {
                relative
                for relative in entries
                if relative.startswith("attribution/")
                and PurePosixPath(relative).name in {"COPYING", "LICENSE"}
            }
            if (
                len(license_entries) != len(licenses)
                or set(license_entries) != expected_license_paths
                or not license_entries
            ):
                raise WorkspaceError(
                    "collected Classic content license inventory is invalid"
                )
            payload_roots = {
                PurePosixPath(relative).parts[0]
                for relative in entries
                if PurePosixPath(relative).parts
            }
            if not {"lib", "maps"} <= payload_roots:
                raise WorkspaceError(
                    "collected Classic content payload is incomplete"
                )
        for relative, entry in entries.items():
            candidate = path.joinpath(*PurePosixPath(relative).parts)
            digest = hashlib.sha256()
            descriptor = open_regular_file(
                candidate, os.O_RDONLY, "collected content file"
            )
            with os.fdopen(descriptor, "rb") as stream:
                if os.fstat(stream.fileno()).st_size != entry["size"]:
                    raise WorkspaceError(
                        "collected content file size does not match manifest"
                    )
                while chunk := stream.read(1024 * 1024):
                    digest.update(chunk)
            if digest.hexdigest() != entry["sha256"]:
                raise WorkspaceError(
                    "collected content file digest does not match manifest"
                )

    @staticmethod
    def _validate_resource_view(
        path: Path,
        source: Path,
        tracked: list[str],
        *,
        require_metadata: bool = True,
    ) -> None:
        if not path.is_dir() or path.is_symlink():
            raise WorkspaceError(f"resource view is not a directory: {path}")
        expected = {MANAGED_MARKER, *tracked}
        if require_metadata:
            expected.add(RUNTIME_INPUT_METADATA)
        actual: set[str] = set()
        actual_directories: set[str] = set()
        for directory, dirnames, filenames in os.walk(path, followlinks=False):
            directory_path = Path(directory)
            for name in dirnames:
                child = directory_path / name
                if child.is_symlink():
                    raise WorkspaceError(f"resource view contains a link: {child}")
                actual_directories.add(child.relative_to(path).as_posix())
            for name in filenames:
                child = directory_path / name
                try:
                    mode = child.lstat().st_mode
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot inspect staged resource {child}: {error}"
                    ) from error
                if not stat.S_ISREG(mode):
                    raise WorkspaceError(
                        f"resource view contains a non-regular file: {child}"
                    )
                actual.add(child.relative_to(path).as_posix())
        if actual != expected:
            raise WorkspaceError("resource view does not match its tracked file set")
        expected_directories = {
            parent.as_posix()
            for relative in tracked
            for parent in PurePosixPath(relative).parents
            if parent != PurePosixPath(".")
        }
        if actual_directories != expected_directories:
            raise WorkspaceError("resource view does not match its tracked directories")
        for relative in tracked:
            parts = PurePosixPath(relative).parts
            identities: list[tuple[int, str]] = []
            for candidate, label in (
                (source.joinpath(*parts), "source resource"),
                (path.joinpath(*parts), "staged resource"),
            ):
                digest = hashlib.sha256()
                descriptor = open_regular_file(candidate, os.O_RDONLY, label)
                with os.fdopen(descriptor, "rb") as stream:
                    size = os.fstat(stream.fileno()).st_size
                    while chunk := stream.read(1024 * 1024):
                        digest.update(chunk)
                identities.append((size, digest.hexdigest()))
            if identities[0] != identities[1]:
                raise WorkspaceError(
                    f"staged resource does not match selected source: {relative}"
                )

    def _runtime_input_cache_matches(
        self,
        output: Path,
        purpose: str,
        inputs: dict[str, Any],
        cacheable: bool,
        validate: Callable[[Path], None],
    ) -> bool:
        if not cacheable or not output.is_dir() or output.is_symlink():
            return False
        marker = output / MANAGED_MARKER
        metadata = output / RUNTIME_INPUT_METADATA
        if (
            not marker.is_file()
            or marker.is_symlink()
            or not metadata.is_file()
            or metadata.is_symlink()
        ):
            return False
        try:
            if load_json(marker) != {
                "schema_version": SCHEMA_VERSION,
                "purpose": purpose,
            } or load_json(metadata) != inputs:
                return False
            validate(output)
        except WorkspaceError:
            return False
        return True

    def _resource_runtime_files(self, source: Path) -> tuple[list[str], list[str]]:
        manifest = source / RESOURCE_PATHS_MANIFEST
        try:
            lines = manifest.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError) as error:
            raise WorkspaceError(
                f"cannot read resource runtime manifest: {manifest}: {error}"
            ) from error
        if not lines:
            raise WorkspaceError(f"resource runtime manifest is empty: {manifest}")
        runtime_paths: list[str] = []
        for line in lines:
            path = PurePosixPath(line)
            if (
                not line
                or path.is_absolute()
                or line != path.as_posix()
                or any(part in {"", ".", ".."} for part in path.parts)
                or line in runtime_paths
            ):
                raise WorkspaceError(
                    f"invalid resource runtime path in {manifest}: {line!r}"
                )
            runtime_paths.append(line)

        if self._source_generation_record(source) is not None:
            tracked: list[str] = []
            for runtime_path in runtime_paths:
                selected = source.joinpath(*PurePosixPath(runtime_path).parts)
                if not selected.exists() or selected.is_symlink():
                    continue
                if selected.is_file():
                    tracked.append(runtime_path)
                    continue
                if not selected.is_dir():
                    raise WorkspaceError(
                        f"tracked runtime resource is not a regular path: {selected}"
                    )
                for directory, directories, files in os.walk(
                    selected, followlinks=False
                ):
                    parent = Path(directory)
                    directories.sort()
                    files.sort()
                    for name in directories:
                        path = parent / name
                        if path.is_symlink():
                            raise WorkspaceError(
                                f"tracked runtime resource is not a regular path: {path}"
                            )
                    for name in files:
                        path = parent / name
                        if path.is_symlink() or not path.is_file():
                            raise WorkspaceError(
                                f"tracked runtime resource is not a regular file: {path}"
                            )
                        tracked.append(path.relative_to(source).as_posix())
            tracked = sorted(set(tracked))
        else:
            try:
                result = subprocess.run(
                    [
                        "git",
                        "-C",
                        str(source),
                        "ls-files",
                        "-z",
                        "--cached",
                        "--",
                        *runtime_paths,
                    ],
                    check=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    pass_fds=active_lock_fds(),
                )
            except FileNotFoundError as error:
                raise WorkspaceError("required command not found: git") from error
            except subprocess.CalledProcessError as error:
                detail = error.stderr.decode("utf-8", errors="replace").strip()
                suffix = f": {detail}" if detail else ""
                raise WorkspaceError(
                    f"cannot list tracked runtime resources{suffix}"
                ) from error

            try:
                tracked = [
                    item.decode("utf-8")
                    for item in result.stdout.split(b"\0")
                    if item
                ]
            except UnicodeDecodeError as error:
                raise WorkspaceError(
                    "runtime resource paths must use UTF-8 names"
                ) from error
        if not tracked:
            raise WorkspaceError(
                f"resource runtime manifest selects no tracked files: {manifest}"
            )
        reserved = {MANAGED_MARKER, RUNTIME_INPUT_METADATA}
        if selected_reserved := sorted(reserved.intersection(tracked)):
            raise WorkspaceError(
                "resource runtime manifest selects reserved generated paths: "
                + ", ".join(selected_reserved)
            )
        return runtime_paths, tracked

    def _collect_content(
        self,
        root: Path,
        selected: dict[str, Path],
        profile_name: str = "default",
    ) -> Path:
        output = root / "runtime" / "content"
        output.parent.mkdir(parents=True, exist_ok=True)
        inputs, cacheable = self._runtime_input_coordinates(
            profile_name, selected, "content"
        )
        profile = self._load_profile(profile_name, require_file=False)
        component = self.manifest.provider(profile["stack"], "content")
        adapter = self.manifest.effective_build(profile["stack"], component)
        matched = self._runtime_input_cache_matches(
            output,
            "collected-content",
            inputs,
            cacheable,
            lambda path: self._validate_collected_content(
                path, inputs["coordinate"], adapter
            ),
        )
        validated_inputs, validated_cacheable = self._runtime_input_coordinates(
            profile_name, selected, "content"
        )
        if matched and (
            validated_inputs == inputs
            and validated_cacheable == cacheable
            and validated_cacheable
        ):
            print(f"content: cached {output}")
            return output
        inputs, cacheable = validated_inputs, validated_cacheable
        if output.exists() or output.is_symlink():
            managed_directory(output, self.paths.builds, "collected-content")
        staging = Path(tempfile.mkdtemp(prefix=".content-", dir=output.parent))
        staging.rmdir()
        source = selected["content"]
        commit = inputs["coordinate"]["head"]
        try:
            command = [
                sys.executable,
                str(source / "tools" / "build_runtime.py"),
                "--source",
                str(source),
                "--output",
                str(staging),
                "--source-commit",
                commit,
            ]
            if adapter == "classic-content":
                command.extend(("--target", "classic"))
            run(command)
            atomic_json(
                staging / MANAGED_MARKER,
                {"schema_version": SCHEMA_VERSION, "purpose": "collected-content"},
            )
            self._validate_collected_content(
                staging,
                inputs["coordinate"],
                adapter,
                require_metadata=False,
            )
            final_inputs, final_cacheable = self._runtime_input_coordinates(
                profile_name, selected, "content"
            )
            if final_inputs != inputs or final_cacheable != cacheable:
                raise WorkspaceError("selected content input changed during collection")
            if cacheable:
                atomic_json(staging / RUNTIME_INPUT_METADATA, inputs)

            def verify_content_install() -> None:
                installed_inputs, installed_cacheable = (
                    self._runtime_input_coordinates(
                        profile_name, selected, "content"
                    )
                )
                if (
                    installed_inputs != inputs
                    or installed_cacheable != cacheable
                ):
                    raise WorkspaceError(
                        "selected content input changed during collection"
                    )

            replace_runtime_directory(
                output,
                staging,
                ".content-previous-",
                verify_content_install,
            )
        except BaseException:
            if staging.exists():
                shutil.rmtree(staging)
            raise
        print(f"content: collected {output}")
        return output

    def _stage_resources(
        self,
        root: Path,
        selected: dict[str, Path],
        profile_name: str = "default",
    ) -> Path:
        output = root / "runtime" / "resources"
        source = selected["resources"]
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists() or output.is_symlink():
            managed_directory(output, self.paths.builds, "resource-view")
        inputs, cacheable = self._runtime_input_coordinates(
            profile_name, selected, "resources"
        )
        runtime_paths, tracked = self._resource_runtime_files(source)
        matched = self._runtime_input_cache_matches(
            output,
            "resource-view",
            inputs,
            cacheable,
            lambda path: self._validate_resource_view(path, source, tracked),
        )
        validated_inputs, validated_cacheable = self._runtime_input_coordinates(
            profile_name, selected, "resources"
        )
        validated_runtime_paths, validated_tracked = self._resource_runtime_files(
            source
        )
        if matched and (
            validated_inputs == inputs
            and validated_cacheable == cacheable
            and validated_cacheable
            and validated_runtime_paths == runtime_paths
            and validated_tracked == tracked
        ):
            print(f"resources: cached {output}")
            return output
        inputs, cacheable = validated_inputs, validated_cacheable
        runtime_paths, tracked = validated_runtime_paths, validated_tracked
        staging = Path(tempfile.mkdtemp(prefix=".resources-", dir=output.parent))
        selected_roots = tuple(PurePosixPath(path) for path in runtime_paths)
        try:
            atomic_json(
                staging / MANAGED_MARKER,
                {"schema_version": SCHEMA_VERSION, "purpose": "resource-view"},
            )
            for relative in tracked:
                path = PurePosixPath(relative)
                if not any(
                    path == root_path or root_path in path.parents
                    for root_path in selected_roots
                ):
                    raise WorkspaceError(
                        f"Git returned an unexpected resource path: {relative}"
                    )
                source_path = source.joinpath(*path.parts)
                try:
                    mode = source_path.lstat().st_mode
                except FileNotFoundError:
                    continue
                if not stat.S_ISREG(mode):
                    raise WorkspaceError(
                        f"tracked runtime resource is not a regular file: {source_path}"
                    )
                destination = staging.joinpath(*path.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_path, destination, follow_symlinks=False)
            final_inputs, final_cacheable = self._runtime_input_coordinates(
                profile_name, selected, "resources"
            )
            final_runtime_paths, final_tracked = self._resource_runtime_files(source)
            if (
                final_inputs != inputs
                or final_cacheable != cacheable
                or final_runtime_paths != runtime_paths
                or final_tracked != tracked
            ):
                raise WorkspaceError("selected resource input changed during staging")
            if cacheable:
                atomic_json(staging / RUNTIME_INPUT_METADATA, inputs)
            self._validate_resource_view(
                staging, source, tracked, require_metadata=cacheable
            )
            installed_inputs, installed_cacheable = self._runtime_input_coordinates(
                profile_name, selected, "resources"
            )
            installed_runtime_paths, installed_tracked = (
                self._resource_runtime_files(source)
            )
            if (
                installed_inputs != inputs
                or installed_cacheable != cacheable
                or installed_runtime_paths != runtime_paths
                or installed_tracked != tracked
            ):
                raise WorkspaceError("selected resource input changed during staging")

            def verify_resource_install() -> None:
                published_inputs, published_cacheable = (
                    self._runtime_input_coordinates(
                        profile_name, selected, "resources"
                    )
                )
                published_runtime_paths, published_tracked = (
                    self._resource_runtime_files(source)
                )
                if (
                    published_inputs != inputs
                    or published_cacheable != cacheable
                    or published_runtime_paths != runtime_paths
                    or published_tracked != tracked
                ):
                    raise WorkspaceError(
                        "selected resource input changed during staging"
                    )

            replace_runtime_directory(
                output,
                staging,
                ".resources-previous-",
                verify_resource_install,
            )
        except BaseException:
            if staging.exists():
                shutil.rmtree(staging)
            raise
        print(f"resources: staged {output}")
        return output

    def _cmake(
        self,
        source: Path,
        binary: Path,
        arguments: list[str],
        tests: bool,
    ) -> None:
        self._prepare_cmake_binary(binary)
        environment = os.environ.copy()
        ccache = shutil.which("ccache") if self._use_ccache else None
        cache_arguments = [
            "-DCMAKE_C_COMPILER_LAUNCHER=",
            "-DCMAKE_CXX_COMPILER_LAUNCHER=",
        ]
        prefix_map_support = self._add_debug_prefix_environment(
            source, binary, environment, arguments
        )
        if ccache is not None:
            cache = self.paths.builds / COMPILER_CACHE_PURPOSE
            with exclusive_lock(
                self.paths.builds / "locks" / "compiler-cache.lock",
                "compiler cache initialization",
            ):
                managed_directory(cache, self.paths.builds, COMPILER_CACHE_PURPOSE)
                atomic_json(
                    cache / CACHE_METADATA,
                    {
                        "schema_version": BUILD_METADATA_SCHEMA_VERSION,
                        "purpose": COMPILER_CACHE_PURPOSE,
                        "last_used_at": datetime.now(timezone.utc).isoformat(),
                        "max_size": COMPILER_CACHE_MAX_SIZE,
                    },
                )
            environment.update(
                {
                    "CCACHE_DIR": str(cache),
                    "CCACHE_MAXSIZE": COMPILER_CACHE_MAX_SIZE,
                    "CCACHE_BASEDIR": str(binary.parent.parent.resolve()),
                }
            )
            environment.pop("CCACHE_HASHDIR", None)
            environment.pop("CCACHE_NOHASHDIR", None)
            if all(prefix_map_support.values()):
                environment["CCACHE_NOHASHDIR"] = "true"
            else:
                environment["CCACHE_HASHDIR"] = "true"
            cache_arguments = [
                f"-DCMAKE_C_COMPILER_LAUNCHER={ccache}",
                f"-DCMAKE_CXX_COMPILER_LAUNCHER={ccache}",
            ]
            print(
                f"ccache: enabled at {cache} (maximum {COMPILER_CACHE_MAX_SIZE})",
                file=sys.stderr,
            )
        elif self._use_ccache:
            print(
                "ccache: command not found; install ccache or pass --no-ccache "
                "to disable this diagnostic",
                file=sys.stderr,
            )

        configure = [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(binary),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Debug",
            f"-DBUILD_TESTING={'ON' if tests else 'OFF'}",
            *cache_arguments,
            *arguments,
        ]
        fingerprint = self._configure_fingerprint(
            source, binary, configure, tests, environment, ccache
        )
        metadata_path = binary / CONFIGURE_METADATA
        source_resolved = source.resolve()
        unchanged_view = all(
            unchanged
            for view, unchanged in self._source_view_unchanged.items()
            if Path(view) == source_resolved or source_resolved in Path(view).parents
        )
        previous: dict[str, Any] = {}
        if metadata_path.is_file() and not metadata_path.is_symlink():
            try:
                loaded = load_json(metadata_path)
                if isinstance(loaded, dict):
                    previous = loaded
            except WorkspaceError:
                pass
        configured = previous == fingerprint
        build_tree_changed = (
            previous.get("build_tree_identity")
            != fingerprint["build_tree_identity"]
        )
        toolchain_identity = fingerprint["build_tree_identity"]["toolchain_file"]
        if toolchain_identity is not None and not toolchain_identity["complete"]:
            build_tree_changed = True
        if build_tree_changed or not self._cmake_state_valid(
            source, binary, configure
        ):
            managed_reset(binary, self.paths.builds, "cmake-binary")
            configured = False
        if (
            self._force_reconfigure
            or not unchanged_view
            or not fingerprint["source"].get("configure_skip_safe", True)
            or not configured
        ):
            run(configure, env=environment)
            fingerprint = self._configure_fingerprint(
                source, binary, configure, tests, environment, ccache
            )
            atomic_json(metadata_path, fingerprint)
        else:
            print(f"cmake: configure unchanged for {binary}; skipping", file=sys.stderr)
        run(["cmake", "--build", str(binary), "--parallel"], env=environment)
        if tests:
            run(
                ["ctest", "--test-dir", str(binary), "--output-on-failure"],
                env=environment,
            )

    def _prepare_cmake_binary(self, binary: Path) -> None:
        binary = _managed_path_no_symlinks(binary, self.paths.builds)
        marker = binary / MANAGED_MARKER
        if not binary.exists():
            managed_directory(binary, self.paths.builds, "cmake-binary")
            return
        if not binary.is_dir():
            raise WorkspaceError(f"CMake binary path is not a directory: {binary}")
        if marker.exists() or marker.is_symlink():
            managed_directory(binary, self.paths.builds, "cmake-binary")
            return

        # Adopt only the exact legacy location inside an already marker-owned
        # profile build. Older wrapper releases created these CMake trees
        # without a nested ownership marker.
        profile_root = binary.parent.parent
        profile_marker = profile_root / MANAGED_MARKER
        try:
            profile_metadata = load_json(profile_marker)
        except WorkspaceError as error:
            raise WorkspaceError(
                f"refusing unmanaged CMake binary path: {binary}"
            ) from error
        if (
            binary.parent.name != "build"
            or profile_root.is_symlink()
            or not profile_root.is_dir()
            or profile_marker.is_symlink()
            or not isinstance(profile_metadata, dict)
            or profile_metadata.get("schema_version") != SCHEMA_VERSION
            or not isinstance(profile_metadata.get("purpose"), str)
            or not profile_metadata["purpose"].startswith("profile:")
        ):
            raise WorkspaceError(f"refusing unmanaged CMake binary path: {binary}")
        atomic_json(
            marker,
            {"schema_version": SCHEMA_VERSION, "purpose": "cmake-binary"},
        )

    @staticmethod
    def _cmake_state_valid(
        source: Path, binary: Path, configure: list[str]
    ) -> bool:
        cache = binary / "CMakeCache.txt"
        ninja = binary / "build.ninja"
        if (
            cache.is_symlink()
            or not cache.is_file()
            or ninja.is_symlink()
            or not ninja.is_file()
        ):
            return False
        values: dict[str, str] = {}
        try:
            for line in cache.read_text(encoding="utf-8").splitlines():
                if "=" in line and ":" in line.split("=", 1)[0]:
                    key, value = line.split("=", 1)
                    values[key.split(":", 1)[0]] = value
                if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
                    values["source"] = line.split("=", 1)[1]
                elif line.startswith("CMAKE_GENERATOR:INTERNAL="):
                    values["generator"] = line.split("=", 1)[1]
            cached_source = Path(values["source"]).resolve(strict=True)
        except (KeyError, OSError, RuntimeError, UnicodeError):
            return False
        if cached_source != source.resolve() or values.get("generator") != "Ninja":
            return False
        expected: dict[str, str] = {}
        for argument in configure:
            if not argument.startswith("-D") or "=" not in argument:
                continue
            key, value = argument[2:].split("=", 1)
            expected[key.split(":", 1)[0]] = value
        if any(values.get(key) != value for key, value in expected.items()):
            return False
        try:
            graph = run(
                ["ninja", "-C", str(binary), "-t", "query", "build.ninja"],
                capture=True,
                trace=False,
            )
        except WorkspaceError:
            return False
        return graph.startswith("build.ninja:\n  input: RERUN_CMAKE")

    def _add_debug_prefix_environment(
        self,
        source: Path,
        binary: Path,
        environment: dict[str, str],
        arguments: list[str],
    ) -> dict[str, bool]:
        if environment.get("CMAKE_TOOLCHAIN_FILE") or any(
            argument.startswith(
                ("-DCMAKE_TOOLCHAIN_FILE=", "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=")
            )
            for argument in arguments
        ):
            return {"c": False, "cxx": False}
        source_path = source.resolve()
        binary_path = binary.resolve()
        relative_source = os.path.relpath(source_path, binary_path)
        options = (
            f"-fdebug-prefix-map={source_path}=/atrinik/source",
            f"-ffile-prefix-map={source_path}=/atrinik/source",
            f"-fdebug-prefix-map={relative_source}=/atrinik/source",
            f"-ffile-prefix-map={relative_source}=/atrinik/source",
            f"-fdebug-prefix-map={binary_path}=/atrinik/build",
            f"-ffile-prefix-map={binary_path}=/atrinik/build",
        )
        mappings = " ".join(shlex.quote(option) for option in options)
        compilers = (
            ("c", "CFLAGS", environment.get("CC", "cc"), "c"),
            ("cxx", "CXXFLAGS", environment.get("CXX", "c++"), "c++"),
        )
        support = {
            name: self._compiler_supports_prefix_maps(command, language)
            for name, _variable, command, language in compilers
        }
        for name, variable, _command, _language in compilers:
            if support[name]:
                environment[variable] = " ".join(
                    filter(None, (environment.get(variable, ""), mappings))
                )
        return support

    def _compiler_supports_prefix_maps(self, command: str, language: str) -> bool:
        identity = self._tool_identity(command)
        key = (command, language, identity.get("resolved_path"), identity.get("sha256"))
        with self._prefix_map_support_lock:
            cached = self._prefix_map_support.get(key)
        if cached is not None:
            return cached
        try:
            words = shlex.split(command)
        except ValueError:
            words = [command]
        executable = shutil.which(words[0]) if words else None
        supported = False
        if executable is not None:
            with tempfile.TemporaryDirectory(prefix="atrinik-prefix-probe-") as temporary:
                root = Path(temporary)
                source = root / ("probe.cc" if language == "c++" else "probe.c")
                output = root / "probe.o"
                source.write_text("int atrinik_prefix_probe;\n", encoding="utf-8")
                try:
                    run(
                        [
                            executable,
                            *words[1:],
                            "-x",
                            language,
                            "-fdebug-prefix-map=/atrinik-prefix-probe=/atrinik/probe",
                            "-ffile-prefix-map=/atrinik-prefix-probe=/atrinik/probe",
                            "-c",
                            str(source),
                            "-o",
                            str(output),
                        ],
                        trace=False,
                    )
                    supported = output.is_file()
                except WorkspaceError:
                    supported = False
        with self._prefix_map_support_lock:
            return self._prefix_map_support.setdefault(key, supported)

    @staticmethod
    def _tool_identity(command: str) -> dict[str, str | None]:
        try:
            words = shlex.split(command)
        except ValueError:
            words = [command]
        executable = shutil.which(words[0]) if words else None
        version = None
        resolved = None
        sha256 = None
        if executable is not None:
            try:
                resolved_path = Path(executable).resolve(strict=True)
                resolved = str(resolved_path)
                if resolved_path.is_file():
                    sha256 = hashlib.sha256(resolved_path.read_bytes()).hexdigest()
            except (OSError, RuntimeError):
                pass
            try:
                output = run(
                    [executable, *words[1:], "--version"],
                    capture=True,
                    trace=False,
                )
                version = (
                    output.splitlines()[0]
                    if output
                    else "unavailable: empty --version output"
                )
            except WorkspaceError as error:
                version = f"unavailable: {error}"
        return {
            "command": command,
            "path": executable,
            "resolved_path": resolved,
            "sha256": sha256,
            "version": version,
        }

    def _configure_fingerprint(
        self,
        source: Path,
        binary: Path,
        configure: list[str],
        tests: bool,
        environment: dict[str, str],
        ccache: str | None,
    ) -> dict[str, Any]:
        relevant_names = (
            "ATRINIK_PACKAGE_VERSION",
            "CC",
            "CXX",
            "CPPFLAGS",
            "CFLAGS",
            "CXXFLAGS",
            "LDFLAGS",
            "PATH",
            "CPATH",
            "C_INCLUDE_PATH",
            "CPLUS_INCLUDE_PATH",
            "LIBRARY_PATH",
            "CMAKE_PREFIX_PATH",
            "CMAKE_INCLUDE_PATH",
            "CMAKE_LIBRARY_PATH",
            "CMAKE_FRAMEWORK_PATH",
            "CMAKE_APPBUNDLE_PATH",
            "CMAKE_PROGRAM_PATH",
            "CMAKE_IGNORE_PATH",
            "CMAKE_SYSTEM_IGNORE_PATH",
            "CMAKE_FIND_ROOT_PATH",
            "CMAKE_TOOLCHAIN_FILE",
            "CMAKE_GENERATOR_PLATFORM",
            "CMAKE_GENERATOR_TOOLSET",
            "PKG_CONFIG_PATH",
            "PKG_CONFIG_LIBDIR",
            "PKG_CONFIG_SYSROOT_DIR",
            "PKG_CONFIG_SYSTEM_INCLUDE_PATH",
            "PKG_CONFIG_SYSTEM_LIBRARY_PATH",
            "PKG_CONFIG_ALLOW_SYSTEM_CFLAGS",
            "PKG_CONFIG_ALLOW_SYSTEM_LIBS",
            "SDKROOT",
            "MACOSX_DEPLOYMENT_TARGET",
        )
        identity = self._cmake_source_identity(source)
        environment_identity = {
            name: environment[name]
            for name in relevant_names
            if name in environment
        }
        # CMake caches discovery results from all of these inputs.  Re-running
        # configure in the old tree does not reliably invalidate find_* or
        # pkg-config cache entries, so every relevant environment change is a
        # build-tree identity change.
        initialization_environment = environment_identity
        generator_tool = self._tool_identity("ninja")
        cmake_tool = self._tool_identity("cmake")
        compilers = {
            "c": self._tool_identity(environment.get("CC", "cc")),
            "cxx": self._tool_identity(environment.get("CXX", "c++")),
        }
        tree_argument_prefixes = (
            "-DCMAKE_C_COMPILER=",
            "-DCMAKE_CXX_COMPILER=",
            "-DCMAKE_TOOLCHAIN_FILE=",
            "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=",
            "-DCMAKE_GENERATOR_PLATFORM=",
            "-DCMAKE_GENERATOR_TOOLSET=",
            "-DCMAKE_SYSROOT=",
        )
        build_tree_identity = {
            "generator": "Ninja",
            "generator_tool": generator_tool,
            "cmake": cmake_tool,
            "compilers": compilers,
            "configured_toolchain": self._cmake_configured_toolchain(binary),
            "environment": initialization_environment,
            "arguments": [
                argument
                for argument in configure
                if argument.startswith(tree_argument_prefixes)
            ],
            "toolchain_file": self._cmake_toolchain_identity(
                source, binary, configure, environment
            ),
        }
        return {
            "schema_version": CONFIGURE_SCHEMA_VERSION,
            "purpose": "cmake-configure",
            "source": identity,
            "binary": str(binary.resolve()),
            "generator": "Ninja",
            "generator_tool": generator_tool,
            "cmake": cmake_tool,
            "compilers": compilers,
            "configure_arguments": configure[1:],
            "build_testing": tests,
            "environment": environment_identity,
            "build_tree_identity": build_tree_identity,
            "ccache": (
                {
                    "path": ccache,
                    "version": self._tool_identity(ccache)["version"],
                    "max_size": COMPILER_CACHE_MAX_SIZE,
                }
                if ccache is not None
                else None
            ),
        }

    def _cmake_toolchain_identity(
        self,
        source: Path,
        binary: Path,
        configure: list[str],
        environment: dict[str, str],
    ) -> dict[str, Any] | None:
        value = environment.get("CMAKE_TOOLCHAIN_FILE")
        for argument in configure:
            for prefix in (
                "-DCMAKE_TOOLCHAIN_FILE=",
                "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=",
            ):
                if argument.startswith(prefix):
                    value = argument[len(prefix) :]
        if not value:
            return None
        raw = Path(value)
        candidates = (raw,) if raw.is_absolute() else (binary / raw, source / raw)
        for candidate in candidates:
            try:
                resolved = candidate.resolve(strict=True)
                if resolved.is_file() and not resolved.is_symlink():
                    return self._cmake_toolchain_closure(value, candidate, resolved)
            except (OSError, RuntimeError):
                continue
        return {
            "value": value,
            "path": None,
            "files": [],
            "complete": False,
        }

    @staticmethod
    def _cmake_toolchain_closure(
        value: str, requested: Path, resolved: Path
    ) -> dict[str, Any]:
        records: list[dict[str, Any]] = []
        seen: set[Path] = set()
        complete = True

        def visit(path: Path, requested_path: Path) -> None:
            nonlocal complete
            try:
                target = path.resolve(strict=True)
            except (OSError, RuntimeError):
                records.append(
                    {
                        "requested": str(requested_path),
                        "path": None,
                        "sha256": None,
                    }
                )
                complete = False
                return
            if target in seen:
                return
            seen.add(target)
            try:
                data = target.read_bytes()
                text = data.decode("utf-8")
            except (OSError, UnicodeError):
                records.append(
                    {
                        "requested": str(requested_path),
                        "path": str(target),
                        "sha256": None,
                    }
                )
                complete = False
                return
            records.append(
                {
                    "requested": str(requested_path),
                    "path": str(target),
                    "symlink": (
                        os.readlink(requested_path)
                        if requested_path.is_symlink()
                        else None
                    ),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )
            command_names = re.findall(
                r"(?im)^\s*([a-z_][a-z0-9_]*)\s*\(", text
            )
            if "$" in text or any(
                name.lower() not in {"include", "set"} for name in command_names
            ):
                complete = False
            pattern = re.compile(
                r"(?ims)^\s*include\s*\(\s*(?:\"([^\"]+)\"|"
                r"\[(=*)\[(.*?)\]\2\]|([^\s\)]+))"
            )
            matches = list(pattern.finditer(text))
            if sum(name.lower() == "include" for name in command_names) != len(matches):
                complete = False
            for match in matches:
                included = match.group(1) or match.group(3) or match.group(4)
                if "$" in included:
                    complete = False
                    continue
                child = Path(included)
                if not child.is_absolute():
                    child = target.parent / child
                candidates = (child, child.with_suffix(".cmake"))
                selected = next(
                    (candidate for candidate in candidates if candidate.exists()),
                    None,
                )
                if selected is None:
                    records.append(
                        {
                            "requested": str(child),
                            "path": None,
                            "sha256": None,
                        }
                    )
                    complete = False
                    continue
                visit(selected, child)

        visit(resolved, requested)
        return {
            "value": value,
            "path": str(resolved),
            "files": records,
            "complete": complete,
        }

    def _cmake_configured_toolchain(self, binary: Path) -> dict[str, Any] | None:
        cache = binary / "CMakeCache.txt"
        if cache.is_symlink() or not cache.is_file():
            return None
        values: dict[str, str] = {}
        names = {
            "CMAKE_C_COMPILER",
            "CMAKE_CXX_COMPILER",
            "CMAKE_C_COMPILER_TARGET",
            "CMAKE_CXX_COMPILER_TARGET",
            "CMAKE_SYSROOT",
            "CMAKE_OSX_SYSROOT",
        }
        try:
            for line in cache.read_text(encoding="utf-8").splitlines():
                if "=" not in line or ":" not in line.split("=", 1)[0]:
                    continue
                key, value = line.split("=", 1)
                name = key.split(":", 1)[0]
                if name in names:
                    values[name] = value
            # CMake does not consistently expose the selected compiler as a
            # cache entry.  Its generated platform files are the authoritative
            # record after language detection, including toolchain-selected
            # compilers.
            compiler_patterns = {
                "CMAKE_C_COMPILER": "CMakeCCompiler.cmake",
                "CMAKE_CXX_COMPILER": "CMakeCXXCompiler.cmake",
            }
            for name, filename in compiler_patterns.items():
                matches = sorted((binary / "CMakeFiles").glob(f"*/{filename}"))
                if not matches:
                    continue
                compiler_file = matches[-1]
                if compiler_file.is_symlink() or not compiler_file.is_file():
                    return {"valid": False}
                pattern = re.compile(
                    rf'^set\({re.escape(name)}\s+"([^"]*)"\)\s*$'
                )
                for line in compiler_file.read_text(encoding="utf-8").splitlines():
                    match = pattern.match(line)
                    if match:
                        values[name] = match.group(1)
                        break
        except (OSError, UnicodeError):
            return {"valid": False}
        return {
            "valid": True,
            "values": values,
            "compilers": {
                name: self._tool_identity(values[name])
                for name in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER")
                if values.get(name)
            },
        }

    def _cmake_source_identity(self, source: Path) -> dict[str, Any]:
        source_metadata = source / SOURCE_VIEW_METADATA
        marker = source / MANAGED_MARKER
        builds = self.paths.builds.resolve()
        try:
            resolved = source.resolve(strict=True)
            metadata = load_json(source_metadata)
            ownership = load_json(marker)
            managed = builds in resolved.parents
            valid = (
                managed
                and not source_metadata.is_symlink()
                and not marker.is_symlink()
                and isinstance(metadata, dict)
                and set(metadata)
                == {"schema_version", "purpose", "source", "source_head", "entries"}
                and metadata.get("schema_version") == SOURCE_VIEW_SCHEMA_VERSION
                and isinstance(metadata.get("purpose"), str)
                and metadata["purpose"].startswith("source-view:")
                and ownership
                == {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": metadata["purpose"],
                }
                and isinstance(metadata.get("source"), str)
                and (
                    metadata.get("source_head") is None
                    or (
                        isinstance(metadata["source_head"], str)
                        and len(metadata["source_head"]) == 40
                        and all(
                            character in "0123456789abcdef"
                            for character in metadata["source_head"]
                        )
                    )
                )
                and isinstance(metadata.get("entries"), dict)
            )
            if valid:
                return metadata
        except (OSError, RuntimeError, WorkspaceError):
            pass
        cmakelists = source / "CMakeLists.txt"
        identity: dict[str, Any] = {
            "path": str(source.resolve()),
            "cmakelists": (
                hashlib.sha256(cmakelists.read_bytes()).hexdigest()
                if cmakelists.is_file() and not cmakelists.is_symlink()
                else None
            ),
        }
        try:
            git_root = Path(
                git(source, "rev-parse", "--show-toplevel", capture=True, trace=False)
            ).resolve(strict=True)
            head = git(source, "rev-parse", "HEAD", capture=True, trace=False)
            if len(head) != 40 or any(
                character not in "0123456789abcdef" for character in head
            ):
                raise WorkspaceError(
                    f"invalid Git HEAD for direct CMake source: {source}"
                )
            clean = _is_clean(git_root, trace=False)
            identity["git"] = {
                "root": str(git_root),
                "head": head,
                "clean": clean,
            }
            identity["configure_skip_safe"] = clean
        except (OSError, RuntimeError, WorkspaceError):
            identity["git"] = None
            identity["configure_skip_safe"] = False
        return identity

    def _build_protocol(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        source = self._mutable_cmake_source_view(
            root, "protocol", selected["protocol"]
        )
        self._cmake(source, root / "build" / "protocol", [], tests)

    def _mutable_cmake_source_view(
        self, root: Path, role: str, source: Path
    ) -> Path:
        """Copy sealed generated CMake inputs whose tests mutate local fixtures."""

        if self._source_generation_record(source) is None:
            return source
        return self._profile_source_view(
            root,
            role,
            source,
            set(),
            copy_all=True,
        )

    @staticmethod
    def _uses_integrated_classic_build(
        targets: list[str], selected: dict[str, Path]
    ) -> bool:
        shared_roles = {"client", "server", "protocol", "libatrinik"}
        if not {"client", "server"}.issubset(targets) or not shared_roles.issubset(
            selected
        ):
            return False
        checkout = selected["client"].parent.resolve()
        return (checkout / "CMakeLists.txt").is_file() and all(
            selected[role].resolve() == checkout / role for role in shared_roles
        )

    @staticmethod
    def _record_classic_graph(root: Path, roles: set[str], graph: str) -> None:
        for role in roles:
            atomic_json(
                root / "build" / f".{role}-graph.json",
                {
                    "schema_version": 1,
                    "purpose": "classic-build-graph",
                    "graph": graph,
                },
            )

    @staticmethod
    def _classic_binary_directory(root: Path, role: str) -> Path:
        marker = root / "build" / f".{role}-graph.json"
        try:
            record = load_json(marker)
        except (OSError, ValueError, WorkspaceError):
            record = {}
        if (
            record.get("schema_version") == 1
            and record.get("purpose") == "classic-build-graph"
            and record.get("graph") == "integrated"
        ):
            return root / "build" / "integrated" / role
        return root / "build" / role

    def _build_integrated_classic(
        self,
        root: Path,
        selected: dict[str, Path],
        tests: bool,
        *,
        sound_root: Path | None = None,
    ) -> None:
        checkout = selected["client"].parent.resolve()
        view = self._profile_source_view(
            root,
            "integrated",
            checkout,
            {"build", "client", "server"},
            preserved_entries={"client", "server"},
        )
        client = self._profile_source_view(
            root,
            "integrated/client",
            selected["client"],
            {"build", "sound"},
            preserved_entries={"sound"},
        )
        self._source_view_link(
            client,
            "sound",
            sound_root or selected["sound"],
            target_is_directory=True,
        )
        server = self._profile_source_view(
            root,
            "integrated/server",
            selected["server"],
            {
                "atrinik-server",
                "build",
                "data",
                "lib",
                "maps",
                "resources",
                "runtime",
                "libplugin_arena.so",
                "libplugin_python.so",
            },
            {"install_data"},
            preserved_entries={"runtime", "resources"},
        )
        self._source_view_directory(server, "runtime", {"content"})
        self._source_view_link(
            server,
            "runtime/content",
            root / "runtime" / "content",
            target_is_directory=True,
        )
        self._source_view_link(
            server,
            "resources",
            root / "runtime" / "resources",
            target_is_directory=True,
        )
        self._cmake(
            view,
            root / "build" / "integrated",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            tests,
        )
        self._record_classic_graph(root, {"client", "server"}, "integrated")

    def _build_library(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        self._cmake(
            selected["libatrinik"],
            root / "build" / "libatrinik",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                f"-DATRINIK_PROTOCOL_SOURCE_DIR={selected['protocol']}",
            ],
            tests,
        )

    def _prepare_component_source_includes(
        self, root: Path, component: Component, source: Path, consumer: Path
    ) -> None:
        if not component.source_includes:
            return
        generation = self._source_generation_record(source)
        if generation is not None:
            closure_root = source.parent
        else:
            closure_root = source
            if component.source != ".":
                for _part in PurePosixPath(component.source).parts:
                    closure_root = closure_root.parent
        records: dict[str, dict[str, Any]] = {}
        includes_unchanged = True
        for include in component.source_includes:
            include_source = closure_root.joinpath(*PurePosixPath(include).parts)
            try:
                status = include_source.lstat()
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect component source include {include_source}: {error}"
                ) from error
            if stat.S_ISDIR(status.st_mode):
                include_view = self._profile_source_view(
                    root, include, include_source, set()
                )
                include_key = str(include_view.resolve())
                includes_unchanged = (
                    includes_unchanged
                    and self._source_view_unchanged.get(include_key, False)
                )
                records[include] = {
                    "kind": "directory",
                    "view": load_regular_json(
                        include_view / SOURCE_VIEW_METADATA,
                        "component source include view",
                    ),
                }
            elif stat.S_ISREG(status.st_mode):
                destination = root.joinpath(
                    "sources", *PurePosixPath(include).parts
                )
                expected_target = str(include_source)
                link_unchanged = (
                    destination.is_symlink()
                    and os.readlink(destination) == expected_target
                )
                self._source_view_link(
                    root / "sources",
                    include,
                    include_source,
                    target_is_directory=False,
                )
                includes_unchanged = includes_unchanged and link_unchanged
                records[include] = {
                    "kind": "file",
                    "source": str(include_source.resolve()),
                    "mode": stat.S_IMODE(status.st_mode),
                    "size": status.st_size,
                    "sha256": _file_digest(
                        include_source, "component source include"
                    ),
                }
            else:
                raise WorkspaceError(
                    "component source include is not a regular file or directory: "
                    f"{include_source}"
                )
        metadata = {
            "schema_version": 1,
            "purpose": f"source-includes:{component.name}",
            "entries": records,
        }
        metadata_path = consumer / SOURCE_INCLUDE_VIEW_METADATA
        try:
            previous = load_regular_json(
                metadata_path, "component source include metadata"
            )
        except WorkspaceError:
            previous = None
        includes_unchanged = includes_unchanged and previous == metadata
        if previous != metadata:
            atomic_json(metadata_path, metadata)
        consumer_key = str(consumer.resolve())
        self._source_view_unchanged[consumer_key] = (
            self._source_view_unchanged.get(consumer_key, False)
            and includes_unchanged
        )

    def _build_client(
        self,
        root: Path,
        selected: dict[str, Path],
        tests: bool,
        *,
        component: Component,
        sound_root: Path | None = None,
    ) -> None:
        view = self._profile_source_view(
            root,
            "client",
            selected["client"],
            {"build", "sound"},
            preserved_entries={"sound", SOURCE_INCLUDE_VIEW_METADATA},
        )
        self._prepare_component_source_includes(
            root, component, selected["client"], view
        )
        self._source_view_link(
            view,
            "sound",
            sound_root or selected["sound"],
            target_is_directory=True,
        )
        protocol = self._mutable_cmake_source_view(
            root, "protocol", selected["protocol"]
        )
        library = self._mutable_cmake_source_view(
            root, "libatrinik", selected["libatrinik"]
        )
        self._cmake(
            view,
            root / "build" / "client",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                f"-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL={protocol}",
                f"-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK={library}",
            ],
            tests,
        )
        self._record_classic_graph(root, {"client"}, "standalone")

    def _build_server(
        self,
        root: Path,
        selected: dict[str, Path],
        tests: bool,
        *,
        component: Component,
    ) -> None:
        view = self._profile_source_view(
            root,
            "server",
            selected["server"],
            {
                "atrinik-server",
                "build",
                "data",
                "lib",
                "maps",
                "resources",
                "runtime",
                "libplugin_arena.so",
                "libplugin_python.so",
            },
            # The server CTest setup copies this tree with CMake's file(COPY).
            # CMake treats a top-level directory symlink as the object to copy,
            # which conflicts with the destination directory it just created.
            {"install_data"},
            preserved_entries={
                "runtime",
                "resources",
                SOURCE_INCLUDE_VIEW_METADATA,
            },
        )
        self._prepare_component_source_includes(
            root, component, selected["server"], view
        )
        self._source_view_directory(view, "runtime", {"content"})
        self._source_view_link(
            view,
            "runtime/content",
            root / "runtime" / "content",
            target_is_directory=True,
        )
        self._source_view_link(
            view,
            "resources",
            root / "runtime" / "resources",
            target_is_directory=True,
        )
        protocol = self._mutable_cmake_source_view(
            root, "protocol", selected["protocol"]
        )
        library = self._mutable_cmake_source_view(
            root, "libatrinik", selected["libatrinik"]
        )
        self._cmake(
            view,
            root / "build" / "server",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                f"-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL={protocol}",
                f"-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK={library}",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            tests,
        )
        self._record_classic_graph(root, {"server"}, "standalone")

    def _region_map_inputs(
        self, profile_name: str, selected: dict[str, Path]
    ) -> tuple[dict[str, Any], bool]:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        required = self._dependency_roles(profile, {"server"})
        coordinates: dict[str, dict[str, str]] = {}
        cacheable = True
        checkout_states = self._selected_checkout_states(
            profile, selected, include_dirty=True
        )
        for role in sorted(required & set(selected)):
            source = selected[role]
            component = stack.providers[role]
            checkout = self._selector_root(profile, component).resolve()
            generation = self._source_generation_record(source)
            if generation is not None:
                clean = True
                head = generation["commit"]
            else:
                state = checkout_states[component.checkout_name]
                clean = not state["dirty"]
                head = state["head"]
            cacheable = cacheable and clean
            coordinates[role] = {
                "component": component.name,
                "repository": component.repository,
                "branch": component.branch,
                "checkout": component.checkout_name,
                "source": component.source,
                "checkout_path": str(checkout),
                "source_path": str(source.resolve()),
                "head": head,
            }
        return (
            {
                "schema_version": REGION_MAP_SCHEMA_VERSION,
                "cacheable": cacheable,
                "coordinates": coordinates,
            },
            cacheable,
        )

    @staticmethod
    def _validate_region_maps(path: Path) -> None:
        if not path.is_dir() or path.is_symlink():
            raise WorkspaceError(f"region-map output is not a directory: {path}")
        png_names: set[str] = set()
        definition_names: set[str] = set()
        for entry in path.iterdir():
            if entry.name in {MANAGED_MARKER, REGION_MAP_METADATA}:
                continue
            try:
                mode = entry.lstat().st_mode
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect generated region map {entry}: {error}"
                ) from error
            if not stat.S_ISREG(mode) or entry.suffix not in {".png", ".def"}:
                raise WorkspaceError(
                    f"generated region-map output is invalid: {entry}"
                )
            descriptor = open_regular_file(
                entry, os.O_RDONLY, "generated region map"
            )
            try:
                with os.fdopen(descriptor, "rb") as stream:
                    if os.fstat(stream.fileno()).st_size == 0:
                        raise WorkspaceError(
                            f"generated region map is empty: {entry}"
                        )
                    if entry.suffix == ".png":
                        signature = stream.read(8)
                        if signature != b"\x89PNG\r\n\x1a\n":
                            raise WorkspaceError(
                                f"generated region map is not a PNG file: {entry}"
                            )
                        png_names.add(entry.stem)
                    else:
                        stream.read().decode("utf-8")
                        definition_names.add(entry.stem)
            except UnicodeError as error:
                raise WorkspaceError(
                    f"generated region-map definition is not UTF-8: {entry}"
                ) from error
        if png_names != definition_names:
            missing_png = sorted(definition_names - png_names)
            missing_definition = sorted(png_names - definition_names)
            details = []
            if missing_png:
                details.append(f"missing PNG: {', '.join(missing_png)}")
            if missing_definition:
                details.append(
                    f"missing definition: {', '.join(missing_definition)}"
                )
            raise WorkspaceError(
                f"generated region-map pairs are incomplete ({'; '.join(details)})"
            )
        if EXPECTED_REGION_MAP not in png_names:
            raise WorkspaceError(
                f"generated region maps lack required {EXPECTED_REGION_MAP} pair"
            )

    def _region_map_cache_matches(
        self, output: Path, inputs: dict[str, Any], cacheable: bool
    ) -> bool:
        if not cacheable or not output.is_dir() or output.is_symlink():
            return False
        marker = output / MANAGED_MARKER
        metadata = output / REGION_MAP_METADATA
        expected_marker = {
            "schema_version": SCHEMA_VERSION,
            "purpose": "region-map-cache",
        }
        if (
            not marker.is_file()
            or marker.is_symlink()
            or not metadata.is_file()
            or metadata.is_symlink()
        ):
            return False
        try:
            if load_json(marker) != expected_marker or load_json(metadata) != inputs:
                return False
            self._validate_region_maps(output)
        except WorkspaceError:
            return False
        return True

    def _generate_region_maps(
        self, root: Path, profile_name: str, selected: dict[str, Path]
    ) -> Path:
        output = root / "runtime" / "client-maps"
        output.parent.mkdir(parents=True, exist_ok=True)
        inputs, cacheable = self._region_map_inputs(profile_name, selected)
        if self._region_map_cache_matches(output, inputs, cacheable):
            print(f"region maps: cached {output}")
            return output
        if output.exists() or output.is_symlink():
            managed_directory(output, self.paths.builds, "region-map-cache")

        staging_root = Path(
            tempfile.mkdtemp(prefix=".region-maps-", dir=output.parent)
        )
        try:
            working = staging_root / "server"
            working.mkdir()
            data = staging_root / "data"
            shutil.copytree(selected["server"] / "install_data", data)
            self._make_tree_owner_writable(data)
            (data / "tmp").mkdir(exist_ok=True)
            assets = staging_root / "assets"
            assets.mkdir()
            generated = assets / "client-maps"
            content = root / "runtime" / "content"
            resources = root / "runtime" / "resources"
            self._link_server_runtime_inputs(
                working, root, selected, data, content, resources
            )
            executable = working / "atrinik-server"
            environment = os.environ.copy()
            environment["PYTHONDONTWRITEBYTECODE"] = "1"
            run(
                [
                    str(executable),
                    "--worldmaker",
                    f"--datapath={data}",
                    f"--assetspath={assets}",
                    f"--libpath={working / 'lib'}",
                    f"--mapspath={working / 'maps'}",
                    f"--resourcespath={working / 'resources'}",
                ],
                cwd=working,
                env=environment,
            )
            self._validate_region_maps(generated)
            final_inputs, final_cacheable = self._region_map_inputs(
                profile_name, selected
            )
            if final_inputs != inputs or final_cacheable != cacheable:
                raise WorkspaceError(
                    "selected region-map inputs changed during generation"
                )
            atomic_json(
                generated / MANAGED_MARKER,
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "region-map-cache",
                },
            )
            atomic_json(generated / REGION_MAP_METADATA, inputs)
            replace_directory(output, generated, ".client-maps-previous-")
        finally:
            if staging_root.exists():
                shutil.rmtree(staging_root)
        print(f"region maps: generated {output}")
        return output

    @staticmethod
    def _worker_required_packages(source: Path) -> tuple[str, ...]:
        try:
            package = load_json(source / "package.json")
        except WorkspaceError:
            raise
        if not isinstance(package, dict):
            raise WorkspaceError("Worker package.json root is not an object")
        names: set[str] = set()
        for field in ("dependencies", "devDependencies"):
            value = package.get(field, {})
            if not isinstance(value, dict) or not all(
                isinstance(name, str) and isinstance(version, str)
                for name, version in value.items()
            ):
                raise WorkspaceError(f"Worker package.json {field} is invalid")
            names.update(value)
        for name in names:
            if not re.fullmatch(
                r"(?:@[a-z0-9][a-z0-9._-]*/)?[a-z0-9][a-z0-9._-]*",
                name,
            ):
                raise WorkspaceError(f"Worker package name is unsafe: {name}")
        return tuple(sorted(names))

    @staticmethod
    def _worker_environment_digest(environment: dict[str, str]) -> str:
        # Lifecycle scripts can observe arbitrary environment variables. Hash the
        # complete install environment, without persisting names or values, so an
        # unrepresented input fails closed as a cache miss.
        payload = json.dumps(
            sorted(environment.items()),
            ensure_ascii=True,
            separators=(",", ":"),
        )
        return hashlib.sha256(payload.encode()).hexdigest()

    @staticmethod
    def _worker_npm_file_config_digest(
        source: Path,
        environment: dict[str, str],
        config: dict[str, Any],
    ) -> str:
        """Hash file-backed effective npm configuration without storing secrets."""

        execution_options = {
            name: value
            for name, value in config.items()
            if name in {"node-options", "script-shell"}
            or name.rsplit(":", 1)[-1] in {"node-options", "script-shell"}
        }
        for name, value in sorted(execution_options.items()):
            if value is None or value == "":
                continue
            if not isinstance(value, str) or "\0" in value:
                raise WorkspaceError(f"npm configuration {name} path is invalid")
            raise WorkspaceError(f"custom npm {name} configuration is unsupported")

        records: list[tuple[str, str, str | None]] = []
        file_values = {
            name: value
            for name, value in config.items()
            if name in WORKER_NPM_FILE_CONFIG_KEYS
            or name.rsplit(":", 1)[-1] in {"cafile", "certfile", "keyfile"}
        }
        for name, value in sorted(file_values.items()):
            if value is None or value == "":
                continue
            if not isinstance(value, str) or "\0" in value:
                raise WorkspaceError(f"npm configuration {name} path is invalid")
            if value == "~" or value.startswith("~/"):
                home = environment.get("HOME")
                if not home:
                    raise WorkspaceError(
                        f"npm configuration {name} requires HOME to resolve"
                    )
                path = Path(home) / value.removeprefix("~/")
            else:
                path = Path(value)
            if not path.is_absolute():
                path = source / path
            path = Path(os.path.abspath(path))
            path_digest = hashlib.sha256(str(path).encode()).hexdigest()
            if path.exists() or path.is_symlink():
                _file_digest(path, f"npm {name}")
                raise WorkspaceError(
                    "external file-backed npm configuration is unsupported; "
                    "use the project .npmrc"
                )
            else:
                content_digest = None
            records.append((name, path_digest, content_digest))
        payload = json.dumps(records, separators=(",", ":"))
        return hashlib.sha256(payload.encode()).hexdigest()

    def _worker_dependency_inputs(
        self, source: Path, environment: dict[str, str]
    ) -> dict[str, Any]:
        for name in ("NODE_OPTIONS", "npm_config_node_options"):
            if environment.get(name):
                raise WorkspaceError(
                    f"custom Node execution options in {name} are unsupported"
                )
        files = {
            name: _file_digest(source / name, f"Worker {name}")
            for name in WORKER_DEPENDENCY_FILES
        }
        npmrc = source / ".npmrc"
        files[".npmrc"] = (
            _file_digest(npmrc, "Worker .npmrc")
            if npmrc.exists() or npmrc.is_symlink()
            else None
        )
        versions: dict[str, str] = {}
        for command in ("node", "npm"):
            value = run(
                [command, "--version"],
                cwd=source,
                capture=True,
                env=environment,
                trace=False,
            )
            if not value or "\n" in value or len(value) > 256:
                raise WorkspaceError(f"{command} returned an invalid version")
            versions[command] = value
        raw_node_runtime = run(
            [
                "node",
                "-p",
                "JSON.stringify({platform:process.platform,arch:process.arch,"
                "versions:process.versions})",
            ],
            cwd=source,
            capture=True,
            env=environment,
            trace=False,
        )
        try:
            node_runtime = json.loads(raw_node_runtime)
        except json.JSONDecodeError as error:
            raise WorkspaceError("Node runtime identity is not valid JSON") from error
        if (
            not isinstance(node_runtime, dict)
            or set(node_runtime) != {"platform", "arch", "versions"}
            or not isinstance(node_runtime.get("platform"), str)
            or not re.fullmatch(r"[a-z0-9._-]{1,64}", node_runtime["platform"])
            or not isinstance(node_runtime.get("arch"), str)
            or not re.fullmatch(r"[a-z0-9._-]{1,64}", node_runtime["arch"])
            or not isinstance(node_runtime.get("versions"), dict)
            or not node_runtime["versions"]
            or not all(
                isinstance(name, str)
                and re.fullmatch(r"[a-z0-9._-]{1,64}", name)
                and isinstance(value, str)
                and len(value) <= 256
                and "\n" not in value
                for name, value in node_runtime["versions"].items()
            )
        ):
            raise WorkspaceError("Node runtime identity is invalid")
        node_versions_digest = hashlib.sha256(
            json.dumps(
                node_runtime["versions"], sort_keys=True, separators=(",", ":")
            ).encode()
        ).hexdigest()
        raw_config = run(
            ["npm", "config", "list", "--json"],
            cwd=source,
            capture=True,
            env=environment,
            trace=False,
        )
        try:
            config = json.loads(raw_config)
        except json.JSONDecodeError as error:
            raise WorkspaceError("npm configuration is not valid JSON") from error
        if not isinstance(config, dict):
            raise WorkspaceError("npm configuration root is not an object")
        config_digest = hashlib.sha256(
            json.dumps(config, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        package = load_json(source / "package.json")
        scripts = package.get("scripts", {}) if isinstance(package, dict) else None
        if not isinstance(scripts, dict) or not all(
            isinstance(name, str) and isinstance(value, str)
            for name, value in scripts.items()
        ):
            raise WorkspaceError("Worker package.json scripts are invalid")
        lifecycle_names = (
            "preinstall",
            "install",
            "postinstall",
            "prepublish",
            "preprepare",
            "prepare",
            "postprepare",
        )
        root_lifecycle = tuple(name for name in lifecycle_names if name in scripts)
        return {
            "schema_version": WORKER_DEPENDENCY_SCHEMA_VERSION,
            "files": files,
            "node_version": versions["node"],
            "npm_version": versions["npm"],
            "node_platform": node_runtime["platform"],
            "node_architecture": node_runtime["arch"],
            "node_versions_sha256": node_versions_digest,
            "os": platform.system(),
            "architecture": platform.machine(),
            "python_platform": sys.platform,
            "npm_config_sha256": config_digest,
            "npm_file_config_sha256": self._worker_npm_file_config_digest(
                source, environment, config
            ),
            "environment_sha256": self._worker_environment_digest(environment),
            "install_command": ["npm", "ci"],
            "lifecycle_scripts": "enabled; complete environment participates in key",
            "root_lifecycle_scripts": list(root_lifecycle),
            # Dependency lifecycle scripts can observe INIT_CWD and arbitrary
            # root files even when package.json has no root hook. Preserve npm's
            # enabled-script semantics by staging and keying the complete source.
            "lifecycle_source_sha256": _tree_digest(
                source,
                WORKER_SOURCE_EXCLUSIONS,
                reject_symlinks=True,
                copied_metadata=True,
            ),
            "lifecycle_source_without_npmrc_sha256": _tree_digest(
                source,
                WORKER_SOURCE_EXCLUSIONS | {".npmrc"},
                reject_symlinks=True,
                copied_metadata=True,
            ),
            "install_root_sha256": hashlib.sha256(
                str(
                    self.paths.builds
                    / "worker-dependencies"
                    / ".transactions"
                ).encode()
            ).hexdigest(),
        }

    @staticmethod
    def _worker_dependency_key(inputs: dict[str, Any]) -> str:
        payload = json.dumps(inputs, sort_keys=True, separators=(",", ":"))
        return hashlib.sha256(payload.encode()).hexdigest()

    @staticmethod
    def _validate_worker_node_modules(
        node_modules: Path,
        hidden_lock_digest: str,
        tree_digest: str,
        required: tuple[str, ...],
        generated_exclusions: set[str] | None = None,
    ) -> None:
        if node_modules.is_symlink() or not node_modules.is_dir():
            raise WorkspaceError("Worker node_modules is not a regular directory")
        hidden_lock = node_modules / ".package-lock.json"
        if _file_digest(hidden_lock, "Worker installed lockfile") != hidden_lock_digest:
            raise WorkspaceError(
                "Worker installed lockfile does not match cache metadata"
            )
        installed = load_json(hidden_lock)
        packages = installed.get("packages") if isinstance(installed, dict) else None
        if not isinstance(packages, dict):
            raise WorkspaceError("Worker installed lockfile packages are invalid")
        for relative in packages:
            if not isinstance(relative, str) or not relative.startswith(
                "node_modules/"
            ):
                raise WorkspaceError("Worker installed package path is invalid")
            package_path = PurePosixPath(relative)
            if (
                package_path.is_absolute()
                or ".." in package_path.parts
                or "\\" in relative
                or len(package_path.parts) < 2
            ):
                raise WorkspaceError("Worker installed package path is unsafe")
            dependency = node_modules.joinpath(*package_path.parts[1:])
            if dependency.is_symlink() or not dependency.is_dir():
                raise WorkspaceError(
                    f"Worker installed package is missing or unsafe: {relative}"
                )
        for name in required:
            dependency = node_modules.joinpath(*name.split("/"))
            if dependency.is_symlink() or not dependency.is_dir():
                raise WorkspaceError(f"Worker dependency is missing or unsafe: {name}")
        if (
            _tree_digest(
                node_modules,
                generated_exclusions or set(),
                bounded_symlinks=True,
                copied_metadata=True,
                ignore_root_mtime=bool(generated_exclusions),
            )
            != tree_digest
        ):
            raise WorkspaceError("Worker node_modules does not match cache metadata")

    def _worker_dependency_cache_matches(
        self,
        entry: Path,
        key: str,
        inputs: dict[str, Any],
        required: tuple[str, ...],
    ) -> dict[str, Any] | None:
        marker = entry / MANAGED_MARKER
        metadata_path = entry / WORKER_DEPENDENCY_METADATA
        try:
            if (
                entry.is_symlink()
                or not entry.is_dir()
                or marker.is_symlink()
                or load_json(marker)
                != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": f"worker-dependencies:{key}",
                }
                or metadata_path.is_symlink()
                or not metadata_path.is_file()
            ):
                return None
            metadata = load_json(metadata_path)
            if (
                not isinstance(metadata, dict)
                or set(metadata)
                != {
                    "schema_version",
                    "purpose",
                    "key",
                    "inputs",
                    "node_modules_lock_sha256",
                    "node_modules_sha256",
                    "node_modules_view_sha256",
                    "last_used_at",
                }
                or metadata.get("schema_version") != WORKER_DEPENDENCY_SCHEMA_VERSION
                or metadata.get("purpose") != "worker-dependencies"
                or metadata.get("key") != key
                or metadata.get("inputs") != inputs
                or not isinstance(metadata.get("node_modules_lock_sha256"), str)
                or not re.fullmatch(
                    r"[0-9a-f]{64}", metadata["node_modules_lock_sha256"]
                )
                or not isinstance(metadata.get("node_modules_sha256"), str)
                or not re.fullmatch(r"[0-9a-f]{64}", metadata["node_modules_sha256"])
                or not isinstance(metadata.get("node_modules_view_sha256"), str)
                or not re.fullmatch(
                    r"[0-9a-f]{64}", metadata["node_modules_view_sha256"]
                )
                or not isinstance(metadata.get("last_used_at"), str)
            ):
                return None
            for name, expected in inputs["files"].items():
                path = entry / name
                if name == ".npmrc":
                    if path.exists() or path.is_symlink():
                        return None
                    continue
                if expected is None:
                    if path.exists() or path.is_symlink():
                        return None
                elif _file_digest(path, f"cached Worker {name}") != expected:
                    return None
            self._validate_worker_node_modules(
                entry / "node_modules",
                metadata["node_modules_lock_sha256"],
                metadata["node_modules_sha256"],
                required,
            )
            if (
                _tree_digest(
                    entry / "node_modules",
                    WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
                    bounded_symlinks=True,
                    copied_metadata=True,
                    ignore_root_mtime=True,
                )
                != metadata["node_modules_view_sha256"]
            ):
                return None
            return metadata
        except (OSError, WorkspaceError):
            return None

    def _worker_dependencies(
        self,
        source: Path,
        environment: dict[str, str],
        consume: Callable[[Path, str, dict[str, Any]], Any] | None = None,
    ) -> tuple[Path, str, dict[str, Any], bool, float, Any]:
        npm_cache = self.paths.builds / "npm-cache"
        npm_cache_lock = self.paths.builds / "locks" / "npm-cache.lock"
        with exclusive_lock(npm_cache_lock, "npm download cache"):
            marker = npm_cache / MANAGED_MARKER
            if npm_cache.exists() and not marker.exists() and not marker.is_symlink():
                if npm_cache.is_symlink() or not npm_cache.is_dir():
                    raise WorkspaceError(f"npm cache path is invalid: {npm_cache}")
                atomic_json(
                    marker,
                    {"schema_version": SCHEMA_VERSION, "purpose": "npm-cache"},
                )
            managed_directory(npm_cache, self.paths.builds, "npm-cache")
            atomic_json(
                npm_cache / CACHE_METADATA,
                {
                    "schema_version": BUILD_METADATA_SCHEMA_VERSION,
                    "purpose": "npm-cache",
                    "last_used_at": datetime.now(timezone.utc).isoformat(),
                },
            )
        environment["npm_config_cache"] = str(npm_cache)
        inputs = self._worker_dependency_inputs(source, environment)
        key = self._worker_dependency_key(inputs)
        required = self._worker_required_packages(source)
        cache_root = self.paths.builds / "worker-dependencies"
        with exclusive_lock(
            self.paths.builds / "locks" / "worker-dependency-cache.lock",
            "Worker dependency cache",
        ):
            managed_directory(
                cache_root, self.paths.builds, "worker-dependency-cache"
            )
            transactions = cache_root / ".transactions"
            managed_directory(
                transactions,
                self.paths.builds,
                "worker-dependency-transactions",
            )
        entry = cache_root / key
        lock = self.paths.builds / "locks" / f"worker-dependencies-{key}.lock"
        started = time.monotonic()
        with exclusive_lock(lock, f"Worker dependencies {key}"):
            metadata = self._worker_dependency_cache_matches(
                entry, key, inputs, required
            )
            recoverable: list[Path] = []
            if metadata is None:
                for candidate in transactions.iterdir():
                    if not re.fullmatch(
                        rf"{key}-(?:staging|backup)-[a-z0-9_]+",
                        candidate.name,
                    ):
                        continue
                    if self._worker_dependency_cache_matches(
                        candidate, key, inputs, required
                    ) is not None:
                        recoverable.append(candidate)
            if recoverable:
                if entry.exists() or entry.is_symlink():
                    managed_directory(
                        entry,
                        self.paths.builds,
                        f"worker-dependencies:{key}",
                    )
                recovered = max(
                    recoverable,
                    key=lambda candidate: (
                        candidate.stat().st_mtime_ns,
                        candidate.name,
                    ),
                )
                replace_directory(
                    entry,
                    recovered,
                    f"{key}-backup-",
                    backup_parent=transactions,
                )
                metadata = self._worker_dependency_cache_matches(
                    entry, key, inputs, required
                )
            if metadata is not None:
                metadata["last_used_at"] = datetime.now(timezone.utc).isoformat()
                atomic_json(entry / WORKER_DEPENDENCY_METADATA, metadata)
                elapsed = time.monotonic() - started
                consumed = (
                    consume(entry / "node_modules", key, metadata)
                    if consume is not None
                    else None
                )
                return entry / "node_modules", key, metadata, True, elapsed, consumed
            if entry.is_symlink() or entry.exists() and not entry.is_dir():
                raise WorkspaceError(f"Worker dependency cache path is unsafe: {entry}")
            if entry.exists():
                managed_directory(
                    entry,
                    self.paths.builds,
                    f"worker-dependencies:{key}",
                )
            staging = transactions / f"{key}-staging-install"
            if staging.exists() or staging.is_symlink():
                marker = staging / MANAGED_MARKER
                if marker.exists() or marker.is_symlink():
                    try:
                        managed_directory(
                            staging,
                            self.paths.builds,
                            f"worker-dependency-transaction:{key}",
                        )
                    except WorkspaceError:
                        managed_directory(
                            staging,
                            self.paths.builds,
                            f"worker-dependencies:{key}",
                        )
                elif staging.is_symlink() or not staging.is_dir():
                    raise WorkspaceError(
                        f"Worker dependency transaction path is unsafe: {staging}"
                    )
                # npm runs without the marker so lifecycle scripts cannot
                # observe wrapper-created metadata. A crash can therefore
                # leave this one exact unmarked path behind. Its marker-owned
                # parent, deterministic key-derived name, and held key lock
                # provide the ownership proof needed to recover it safely.
                remove_owned_tree(staging)
            managed_directory(
                staging,
                self.paths.builds,
                f"worker-dependency-transaction:{key}",
            )
            try:
                # Ownership is proven by the marker-owned transaction parent,
                # exact per-key name, and held key lock. Keep wrapper metadata
                # outside the lifecycle-visible input tree while npm runs.
                (staging / MANAGED_MARKER).unlink()
                _copy_worker_source(
                    source,
                    staging,
                    include_npmrc=False,
                )
                if inputs["files"][".npmrc"] is not None:
                    _copy_regular_file(
                        source / ".npmrc",
                        staging / ".npmrc",
                        "Worker .npmrc",
                        0o600,
                    )
                shutil.copystat(source, staging, follow_symlinks=False)
                if (
                    _tree_digest(
                        staging,
                        WORKER_SOURCE_EXCLUSIONS | {".npmrc"},
                        reject_symlinks=True,
                        copied_metadata=True,
                    )
                    != inputs["lifecycle_source_without_npmrc_sha256"]
                ):
                    raise WorkspaceError(
                        "staged Worker lifecycle source does not match its cache key"
                    )
                # Clean primary sources are immutable generations whose root
                # has no write bits. Authenticate that copied mode first, then
                # restore full effective-owner access while disabling group
                # and other writes so npm can create node_modules.
                _make_worker_staging_owner_writable(staging)
                if (staging / ".npmrc").exists():
                    (staging / ".npmrc").chmod(0o600)
                    if (
                        _file_digest(staging / ".npmrc", "staged Worker .npmrc")
                        != inputs["files"][".npmrc"]
                    ):
                        raise WorkspaceError(
                            "staged Worker .npmrc does not match its cache key"
                        )
                _normalize_worker_atime(staging)
                run(["npm", "ci"], cwd=staging, env=environment)
                if (staging / MANAGED_MARKER).exists() or (
                    staging / MANAGED_MARKER
                ).is_symlink():
                    raise WorkspaceError(
                        "Worker lifecycle created reserved workspace metadata"
                    )
                if _tree_references_path(staging / "node_modules", staging):
                    raise WorkspaceError(
                        "Worker dependency output embeds its install path"
                    )
                final_inputs = self._worker_dependency_inputs(source, environment)
                if final_inputs != inputs:
                    raise WorkspaceError(
                        "Worker dependency inputs changed during installation"
                    )
                for child in staging.iterdir():
                    if child.name == "node_modules":
                        continue
                    if child.is_symlink() or not child.is_dir():
                        child.unlink()
                    else:
                        remove_owned_tree(child)
                for name, expected in inputs["files"].items():
                    if expected is not None and name != ".npmrc":
                        shutil.copy2(source / name, staging / name)
                hidden_digest = _file_digest(
                    staging / "node_modules" / ".package-lock.json",
                    "Worker installed lockfile",
                )
                modules_digest = _tree_digest(
                    staging / "node_modules",
                    set(),
                    bounded_symlinks=True,
                    copied_metadata=True,
                )
                modules_view_digest = _tree_digest(
                    staging / "node_modules",
                    WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
                    bounded_symlinks=True,
                    copied_metadata=True,
                    ignore_root_mtime=True,
                )
                self._validate_worker_node_modules(
                    staging / "node_modules",
                    hidden_digest,
                    modules_digest,
                    required,
                )
                metadata = {
                    "schema_version": WORKER_DEPENDENCY_SCHEMA_VERSION,
                    "purpose": "worker-dependencies",
                    "key": key,
                    "inputs": inputs,
                    "node_modules_lock_sha256": hidden_digest,
                    "node_modules_sha256": modules_digest,
                    "node_modules_view_sha256": modules_view_digest,
                    "last_used_at": datetime.now(timezone.utc).isoformat(),
                }
                atomic_json(
                    staging / MANAGED_MARKER,
                    {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"worker-dependencies:{key}",
                    },
                )
                atomic_json(staging / WORKER_DEPENDENCY_METADATA, metadata)

                def verify_dependency_install() -> None:
                    installed = self._worker_dependency_cache_matches(
                        entry, key, inputs, required
                    )
                    if installed != metadata:
                        raise WorkspaceError(
                            "published Worker dependencies failed validation"
                        )

                replace_directory(
                    entry,
                    staging,
                    f"{key}-backup-",
                    backup_parent=transactions,
                    verify_after_install=verify_dependency_install,
                )
            finally:
                if staging.exists():
                    remove_owned_tree(staging)
            elapsed = time.monotonic() - started
            consumed = (
                consume(entry / "node_modules", key, metadata)
                if consume is not None
                else None
            )
        return (
            entry / "node_modules",
            key,
            metadata,
            False,
            elapsed,
            consumed,
        )

    def _worker_view(
        self,
        root: Path,
        source: Path,
        dependencies: Path,
        dependency_key: str,
        dependency_metadata: dict[str, Any],
    ) -> tuple[Path, bool, float]:
        started = time.monotonic()
        view = root / "sources" / "metaserver-worker"
        lifecycle_source_digest = _tree_digest(
            source,
            WORKER_SOURCE_EXCLUSIONS,
            reject_symlinks=True,
            copied_metadata=True,
        )
        source_digest = _tree_digest(
            source, WORKER_SOURCE_EXCLUSIONS, reject_symlinks=True
        )
        source_view_digest = _tree_digest(
            source,
            WORKER_SOURCE_EXCLUSIONS,
            reject_symlinks=True,
            copied_metadata=True,
            ignore_root_mtime=True,
        )
        dependency_source_digest = dependency_metadata.get("inputs", {}).get(
            "lifecycle_source_sha256"
        )
        if lifecycle_source_digest != dependency_source_digest:
            raise WorkspaceError(
                "Worker source does not match dependency lifecycle inputs"
            )
        expected = {
            "schema_version": WORKER_VIEW_SCHEMA_VERSION,
            "purpose": "worker-view",
            "source_sha256": source_digest,
            "source_view_sha256": source_view_digest,
            "dependency_key": dependency_key,
            "node_modules_lock_sha256": dependency_metadata[
                "node_modules_lock_sha256"
            ],
        }
        marker_path = view / MANAGED_MARKER
        metadata_path = view / WORKER_VIEW_METADATA

        def validate_installed_view() -> None:
            if (
                view.is_symlink()
                or not view.is_dir()
                or not marker_path.is_file()
                or marker_path.is_symlink()
                or not metadata_path.is_file()
                or metadata_path.is_symlink()
                or load_json(marker_path)
                != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "source-view:metaserver-worker",
                }
                or load_json(metadata_path) != expected
                or _tree_digest(view, WORKER_SOURCE_EXCLUSIONS) != source_digest
                or _tree_digest(
                    view,
                    WORKER_SOURCE_EXCLUSIONS,
                    copied_metadata=True,
                    ignore_root_mtime=True,
                )
                != source_view_digest
            ):
                raise WorkspaceError("published Worker view failed validation")
            self._validate_worker_node_modules(
                view / "node_modules",
                dependency_metadata["node_modules_lock_sha256"],
                dependency_metadata["node_modules_view_sha256"],
                self._worker_required_packages(source),
                WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
            )

        try:
            validate_installed_view()
            return view, True, time.monotonic() - started
        except (OSError, WorkspaceError):
            pass
        if view.exists() or view.is_symlink():
            managed_directory(
                view, self.paths.builds, "source-view:metaserver-worker"
            )
        view.parent.mkdir(parents=True, exist_ok=True)
        staging = Path(
            tempfile.mkdtemp(prefix=".metaserver-worker-", dir=view.parent)
        )
        try:
            _copy_worker_source(source, staging)
            if (
                _tree_digest(
                    staging,
                    WORKER_SOURCE_EXCLUSIONS,
                    reject_symlinks=True,
                    copied_metadata=True,
                )
                != lifecycle_source_digest
            ):
                raise WorkspaceError(
                    "staged Worker view source does not match its fingerprint"
                )
            _make_worker_staging_owner_writable(staging)
            shutil.copytree(dependencies, staging / "node_modules", symlinks=True)
            if (
                _tree_digest(
                    source,
                    WORKER_SOURCE_EXCLUSIONS,
                    reject_symlinks=True,
                    copied_metadata=True,
                )
                != lifecycle_source_digest
            ):
                raise WorkspaceError("Worker source changed during view preparation")
            self._validate_worker_node_modules(
                staging / "node_modules",
                dependency_metadata["node_modules_lock_sha256"],
                dependency_metadata["node_modules_view_sha256"],
                self._worker_required_packages(source),
                WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
            )
            atomic_json(
                staging / MANAGED_MARKER,
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "source-view:metaserver-worker",
                },
            )
            atomic_json(staging / WORKER_VIEW_METADATA, expected)
            shutil.copystat(source, staging, follow_symlinks=False)
            replace_directory(
                view,
                staging,
                ".metaserver-worker-previous-",
                verify_after_install=validate_installed_view,
            )
        finally:
            if staging.exists():
                remove_owned_tree(staging)
        return view, False, time.monotonic() - started

    @staticmethod
    def _reconcile_worker_view_source(
        source: Path,
        view: Path,
        dependency_key: str,
        dependency_metadata: dict[str, Any],
    ) -> None:
        lifecycle_source_digest = dependency_metadata.get("inputs", {}).get(
            "lifecycle_source_sha256"
        )
        source_digest = _tree_digest(
            source, WORKER_SOURCE_EXCLUSIONS, reject_symlinks=True
        )
        source_view_digest = _tree_digest(
            source,
            WORKER_SOURCE_EXCLUSIONS,
            reject_symlinks=True,
            copied_metadata=True,
            ignore_root_mtime=True,
        )
        expected_marker = {
            "schema_version": SCHEMA_VERSION,
            "purpose": "source-view:metaserver-worker",
        }
        expected_metadata = {
            "schema_version": WORKER_VIEW_SCHEMA_VERSION,
            "purpose": "worker-view",
            "source_sha256": source_digest,
            "source_view_sha256": source_view_digest,
            "dependency_key": dependency_key,
            "node_modules_lock_sha256": dependency_metadata[
                "node_modules_lock_sha256"
            ],
        }

        def validate_controls() -> None:
            marker_path = view / MANAGED_MARKER
            metadata_path = view / WORKER_VIEW_METADATA
            if (
                not marker_path.is_file()
                or marker_path.is_symlink()
                or not metadata_path.is_file()
                or metadata_path.is_symlink()
                or load_json(marker_path) != expected_marker
                or load_json(metadata_path) != expected_metadata
            ):
                raise WorkspaceError("Worker view control metadata changed during checks")

        validate_controls()
        if (
            _tree_digest(
                source,
                WORKER_SOURCE_EXCLUSIONS,
                reject_symlinks=True,
                copied_metadata=True,
            )
            != lifecycle_source_digest
            or _tree_digest(
                view, WORKER_SOURCE_EXCLUSIONS, reject_symlinks=True
            )
            != source_digest
        ):
            raise WorkspaceError("Worker source changed while running checks")
        if (
            _tree_digest(
                view,
                WORKER_SOURCE_EXCLUSIONS,
                reject_symlinks=True,
                copied_metadata=True,
                ignore_root_mtime=True,
            )
            != source_view_digest
        ):
            _copy_worker_source_metadata(source, view)
        validate_controls()
        if (
            _tree_digest(
                source,
                WORKER_SOURCE_EXCLUSIONS,
                reject_symlinks=True,
                copied_metadata=True,
            )
            != lifecycle_source_digest
            or _tree_digest(
                view,
                WORKER_SOURCE_EXCLUSIONS,
                reject_symlinks=True,
                copied_metadata=True,
                ignore_root_mtime=True,
            )
            != source_view_digest
        ):
            raise WorkspaceError("Worker source changed during view reconciliation")

    @staticmethod
    def _run_worker_checks(
        view: Path,
        environment: dict[str, str],
        dependency_key: str,
        dependency_metadata: dict[str, Any],
    ) -> None:
        marker_path = view / MANAGED_MARKER
        metadata_path = view / WORKER_VIEW_METADATA
        expected_marker = {
            "schema_version": SCHEMA_VERSION,
            "purpose": "source-view:metaserver-worker",
        }
        descriptor = os.open(
            view,
            os.O_RDONLY
            | getattr(os, "O_DIRECTORY", 0)
            | getattr(os, "O_NOFOLLOW", 0),
        )
        try:
            opened_status = os.fstat(descriptor)
            path_status = view.lstat()
            if (
                (path_status.st_dev, path_status.st_ino)
                != (opened_status.st_dev, opened_status.st_ino)
                or not stat.S_ISDIR(path_status.st_mode)
                or marker_path.is_symlink()
                or not marker_path.is_file()
                or load_json(marker_path) != expected_marker
                or metadata_path.is_symlink()
                or not metadata_path.is_file()
            ):
                raise WorkspaceError("Worker view controls are invalid before checks")
            control_metadata = load_json(metadata_path)
            if (
                not isinstance(control_metadata, dict)
                or set(control_metadata)
                != {
                    "schema_version",
                    "purpose",
                    "source_sha256",
                    "source_view_sha256",
                    "dependency_key",
                    "node_modules_lock_sha256",
                }
                or control_metadata.get("schema_version")
                != WORKER_VIEW_SCHEMA_VERSION
                or control_metadata.get("purpose") != "worker-view"
                or control_metadata.get("dependency_key") != dependency_key
                or control_metadata.get("node_modules_lock_sha256")
                != dependency_metadata["node_modules_lock_sha256"]
            ):
                raise WorkspaceError("Worker view controls are invalid before checks")
            if opened_status.st_uid != os.geteuid():
                raise WorkspaceError("Worker view ownership is unsafe before checks")
            original_mode = stat.S_IMODE(opened_status.st_mode)
            os.fchmod(descriptor, _worker_owner_writable_mode(original_mode))
            try:
                run(
                    ["npm", "run", "check"],
                    cwd=view,
                    env={**environment, "PYTHONDONTWRITEBYTECODE": "1"},
                )
            finally:
                try:
                    try:
                        current_status = view.lstat()
                    except OSError:
                        current_status = None
                    if (
                        current_status is not None
                        and not view.is_symlink()
                        and stat.S_ISDIR(current_status.st_mode)
                        and (current_status.st_dev, current_status.st_ino)
                        == (opened_status.st_dev, opened_status.st_ino)
                    ):
                        for control_path in (marker_path, metadata_path):
                            if control_path.is_symlink() or not control_path.is_dir():
                                control_path.unlink(missing_ok=True)
                            else:
                                remove_owned_tree(control_path)
                        atomic_json(marker_path, expected_marker)
                        atomic_json(metadata_path, control_metadata)
                finally:
                    os.fchmod(descriptor, original_mode)
        finally:
            os.close(descriptor)

    def _reconcile_worker_view_after_checks(
        self,
        source: Path,
        view: Path,
        dependency_key: str,
        dependency_metadata: dict[str, Any],
    ) -> None:
        self._reconcile_worker_view_source(
            source, view, dependency_key, dependency_metadata
        )
        self._validate_worker_node_modules(
            view / "node_modules",
            dependency_metadata["node_modules_lock_sha256"],
            dependency_metadata["node_modules_view_sha256"],
            self._worker_required_packages(source),
            WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
        )

    def _build_worker(self, root: Path, selected: dict[str, Path]) -> None:
        source = selected["metaserver-worker"]
        environment = os.environ.copy()
        dependencies, key, metadata, cache_hit, install_seconds, view_result = (
            self._worker_dependencies(
                source,
                environment,
                lambda dependencies, key, metadata: self._worker_view(
                    root, source, dependencies, key, metadata
                ),
            )
        )
        view, view_hit, view_seconds = view_result
        print(
            f"worker dependencies: {'cached' if cache_hit else 'installed'} "
            f"{key} ({install_seconds:.2f}s)"
        )
        print(
            f"worker view: {'reused' if view_hit else 'prepared'} {view} "
            f"({view_seconds:.2f}s)"
        )
        self._run_worker_checks(view, environment, key, metadata)
        self._reconcile_worker_view_after_checks(source, view, key, metadata)

    def state_add(self, name: str, path: Path | None) -> Path:
        self.paths.ensure()
        with shared_maintenance_lock(
            self._lease_namespace / "repository-layout.lock"
        ):
            return self._state_add(name, path)

    def _state_add(self, name: str, path: Path | None) -> Path:
        validate_name(name, "state name")
        if path is None:
            resolved = self._canonical_state_path(
                self.paths.state / "server" / name
            )
        else:
            if not path.is_absolute():
                raise WorkspaceError("state path must be absolute")
            resolved = self._canonical_state_path(path)
        if resolved == Path("/") or resolved == self.paths.repository:
            raise WorkspaceError(f"refusing unsafe state path: {resolved}")
        if resolved.exists() and not resolved.is_dir():
            raise WorkspaceError(f"state path is not a directory: {resolved}")
        if resolved.exists():
            self._validate_state(resolved)
            temporary_marker = resolved / TEMPORARY_STATE_METADATA
            ownership_marker = resolved / MANAGED_MARKER
            ownership = (
                load_json(ownership_marker)
                if ownership_marker.is_file() and not ownership_marker.is_symlink()
                else None
            )
            if (
                temporary_marker.exists()
                or temporary_marker.is_symlink()
                or (
                    isinstance(ownership, dict)
                    and ownership.get("purpose") == "temporary-topology-state"
                )
            ):
                raise WorkspaceError(
                    "temporary topology state can only be registered through "
                    "state promote"
                )
        self._register_state(name, resolved)
        print(resolved)
        return resolved

    def _register_state(self, name: str, resolved: Path) -> None:
        with exclusive_lock(self.paths.workspace / "states.lock", "states registry"):
            states = self._load_states()
            if name in states:
                raise WorkspaceError(f"state already exists: {name}")
            states[name] = str(resolved)
            atomic_json(
                self.paths.states_file,
                {"schema_version": SCHEMA_VERSION, "states": states},
            )

    def _scenario_directory(self, name: str) -> Path:
        validate_name(name, "scenario name")
        directory = (self.paths.scenarios / name).resolve(strict=False)
        if directory.parent != self.paths.scenarios.resolve():
            raise WorkspaceError(f"invalid scenario path: {directory}")
        return directory

    @staticmethod
    def _write_scenario_password(path: Path, password: str) -> None:
        descriptor = open_regular_file(
            path,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL,
            "scenario password",
            0o600,
        )
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
                stream.write(password)
                stream.flush()
                os.fsync(stream.fileno())
        except BaseException:
            path.unlink(missing_ok=True)
            raise

    @staticmethod
    def _open_scenario_password(path: Path) -> int:
        descriptor = open_regular_file(path, os.O_RDONLY, "scenario password")
        metadata = os.fstat(descriptor)
        mode = stat.S_IMODE(metadata.st_mode)
        wrong_owner = hasattr(os, "geteuid") and metadata.st_uid != os.geteuid()
        if wrong_owner or mode != 0o600:
            os.close(descriptor)
            raise WorkspaceError(
                f"scenario password must be owned by this user and mode 0600: {path}"
            )
        if not 0 < metadata.st_size <= SCENARIO_PASSWORD_MAX_SIZE:
            os.close(descriptor)
            raise WorkspaceError(f"scenario password file is invalid: {path}")
        return descriptor

    @classmethod
    def _read_scenario_password(cls, path: Path) -> str:
        descriptor = cls._open_scenario_password(path)
        try:
            with os.fdopen(descriptor, encoding="utf-8") as stream:
                password = stream.read(SCENARIO_PASSWORD_MAX_SIZE + 1)
                descriptor = -1
        except UnicodeError as error:
            raise WorkspaceError(
                f"scenario password file is not UTF-8: {path}"
            ) from error
        finally:
            if descriptor >= 0:
                os.close(descriptor)
        if (
            not password
            or len(password) > SCENARIO_PASSWORD_MAX_SIZE
            or "\n" in password
            or "\r" in password
        ):
            raise WorkspaceError(f"scenario password file is invalid: {path}")
        return password

    def _load_scenario(
        self,
        name: str,
        registered_states: dict[str, str] | None = None,
    ) -> dict[str, Any]:
        self.paths.ensure()
        root = self._scenario_directory(name)
        if not root.is_dir() or root.is_symlink():
            raise WorkspaceError(f"scenario does not exist: {name}")
        marker = root / MANAGED_MARKER
        if marker.is_symlink() or load_regular_json(marker, "scenario ownership marker") != {
            "schema_version": SCHEMA_VERSION,
            "purpose": "test-scenario",
        }:
            raise WorkspaceError(f"scenario ownership marker is invalid: {name}")
        metadata_path = root / "scenario.json"
        if metadata_path.is_symlink():
            raise WorkspaceError(f"scenario metadata is invalid: {name}")
        metadata = load_regular_json(metadata_path, "scenario metadata")
        if not isinstance(metadata, dict):
            raise WorkspaceError(f"scenario metadata must be an object: {name}")
        actual_keys = set(metadata)
        historical_keys = SCENARIO_KEYS - {"stack", "providers"}
        if (
            actual_keys == historical_keys
            and metadata.get("schema_version") == SCHEMA_VERSION
        ):
            raise _InertScenarioError(
                SCENARIO_INERT_HISTORICAL_IDENTITY,
                "historical scenario lacks immutable stack/provider identity and is "
                f"inert; recreate it explicitly: {name}"
            )
        if actual_keys != SCENARIO_KEYS:
            raise WorkspaceError(f"scenario fields are invalid: {name}")
        if metadata.get("schema_version") != SCENARIO_SCHEMA_VERSION:
            raise _InertScenarioError(
                SCENARIO_INERT_HISTORICAL_IDENTITY,
                "historical scenario lacks immutable repository/branch identity and "
                f"is inert; recreate it explicitly: {name}"
            )
        resolved = metadata.get("resolved")
        try:
            profile = self._load_profile(
                metadata.get("profile", ""), require_file=False
            )
        except WorkspaceError as error:
            raise _InertScenarioError(
                SCENARIO_INERT_PROFILE_UNRESOLVABLE, str(error)
            ) from error
        stack = self.manifest.stack(profile["stack"])
        required = self._dependency_roles(profile, {"server"})
        expected_providers = {
            role: stack.providers[role].name for role in sorted(required)
        }
        if (
            metadata.get("name") != name
            or not isinstance(metadata.get("profile"), str)
            or not isinstance(metadata.get("preset"), str)
            or metadata["preset"] not in SCENARIO_PRESETS
            or metadata.get("state") != f"scenario-{name}"
            or not isinstance(metadata.get("account"), str)
            or not isinstance(metadata.get("character"), str)
            or not isinstance(metadata.get("archetype"), str)
            or not isinstance(metadata.get("provisioned_at"), str)
            or not metadata["provisioned_at"]
            or not isinstance(resolved, dict)
            or set(resolved) != required
            or metadata.get("stack") != stack.name
            or metadata.get("providers") != expected_providers
        ):
            raise WorkspaceError(f"scenario metadata is invalid: {name}")
        for component, record in resolved.items():
            if (
                not isinstance(record, dict)
                or set(record)
                != {
                    "path",
                    "checkout_path",
                    "checkout",
                    "repository",
                    "branch",
                    "source",
                    "head",
                    "dirty",
                }
                or not isinstance(record.get("path"), str)
                or not Path(record["path"]).is_absolute()
                or not isinstance(record.get("checkout_path"), str)
                or not Path(record["checkout_path"]).is_absolute()
                or not isinstance(record.get("checkout"), str)
                or not isinstance(record.get("repository"), str)
                or not isinstance(record.get("branch"), str)
                or not isinstance(record.get("source"), str)
                or not isinstance(record.get("head"), str)
                or not re.fullmatch(r"[0-9a-f]{40,64}", record["head"])
                or not isinstance(record.get("dirty"), bool)
            ):
                raise WorkspaceError(
                    f"scenario component metadata is invalid: {name}/{component}"
                )
            provider = stack.providers[component]
            try:
                checkout_path = Path(record["checkout_path"]).resolve(strict=False)
                expected_path = (
                    checkout_path
                    if provider.source == "."
                    else checkout_path.joinpath(
                        *PurePosixPath(provider.source).parts
                    )
                ).resolve(strict=False)
                resolved_path = Path(record["path"]).resolve(strict=False)
            except (OSError, RuntimeError, ValueError) as error:
                raise WorkspaceError(
                    f"scenario component metadata is invalid: {name}/{component}"
                ) from error
            if (
                record["checkout"] != provider.checkout_name
                or record["repository"] != provider.repository
                or record["branch"] != provider.branch
                or record["source"] != provider.source
                or resolved_path != expected_path
            ):
                raise WorkspaceError(
                    f"scenario component identity is invalid: {name}/{component}"
                )
        state = root / "state"
        if state.is_symlink():
            raise WorkspaceError(f"scenario state is invalid: {name}")
        self._validate_state(state)
        states = (
            self._load_states() if registered_states is None else registered_states
        )
        registered = states.get(metadata["state"])
        try:
            registered_path = (
                None
                if registered is None
                else Path(registered).resolve(strict=False)
            )
        except (OSError, RuntimeError, ValueError) as error:
            raise WorkspaceError(
                f"scenario state registration is invalid: {name}"
            ) from error
        if registered_path is None or registered_path != state.resolve():
            raise WorkspaceError(f"scenario state registration is invalid: {name}")
        os.close(self._open_scenario_password(root / "password"))
        return metadata

    def _scenario_provision_state(
        self,
        metadata: dict[str, Any],
        state: Path,
        password_file: Path,
    ) -> dict[str, dict[str, Any]]:
        profile = self._load_profile(metadata["profile"], require_file=False)
        required = self._dependency_roles(profile, {"server"})
        selected = self._resolve_build_profile(metadata["profile"], required)
        root = self._build_resolved(
            "server", metadata["profile"], False, ["server"], selected
        )
        with self._profile_build_lock(root, metadata["profile"]):
            runtime = self._prepare_server_runtime(
                root, selected, state, metadata["state"]
            )
            executable = runtime / "atrinik-server"
            arguments = [
                str(executable),
                "--provision_scenario",
                f"--provision_account={metadata['account']}",
                f"--provision_character={metadata['character']}",
                f"--provision_archetype={metadata['archetype']}",
                f"--provision_password_file={password_file}",
                f"--assetspath={runtime / 'assets'}",
            ]
            preset = metadata.get("preset", "basic-player")
            if preset != "basic-player":
                arguments.append(f"--provision_preset={preset}")
            run(arguments, cwd=runtime)
        profile = self._load_profile(metadata["profile"], require_file=False)
        stack = self.manifest.stack(profile["stack"])
        audited = {role: selected[role] for role in required}
        checkout_states = self._selected_checkout_states(
            profile, audited, include_dirty=True
        )
        return {
            role: {
                "path": str(path),
                "checkout_path": str(
                    checkout_states[stack.providers[role].checkout_name]["path"]
                ),
                "checkout": stack.providers[role].checkout_name,
                "repository": stack.providers[role].repository,
                "branch": stack.providers[role].branch,
                "source": stack.providers[role].source,
                "head": checkout_states[stack.providers[role].checkout_name]["head"],
                "dirty": checkout_states[stack.providers[role].checkout_name]["dirty"],
            }
            for role, path in sorted(
                (role, selected[role]) for role in required
            )
        }

    def scenario_create(
        self, name: str, profile: str, preset: str = "basic-player"
    ) -> dict[str, Any]:
        self.paths.ensure()
        validate_name(name, "scenario name")
        with self._resolved_profile_operation(
            profile, {"server"}, f"create scenario {name}"
        ):
            with self._resource_locks(
                [
                    self._lease_request(
                        "scenario", name, "exclusive", f"create scenario {name}"
                    )
                ],
            ):
                return self._scenario_create(name, profile, preset)

    def _scenario_create(
        self, name: str, profile: str, preset: str = "basic-player"
    ) -> dict[str, Any]:
        self.paths.ensure()
        validate_name(name, "scenario name")
        if preset not in SCENARIO_PRESETS:
            raise WorkspaceError(f"unknown scenario preset: {preset}")
        root = self._scenario_directory(name)
        state_name = f"scenario-{name}"
        digest = hashlib.sha256(name.encode()).hexdigest()[:8]
        metadata: dict[str, Any] = {
            "schema_version": SCENARIO_SCHEMA_VERSION,
            "name": name,
            "profile": profile,
            "stack": self._load_profile(profile, require_file=False)["stack"],
            "preset": preset,
            "state": state_name,
            "account": f"scenario{digest}",
            "character": f"Scenario {digest}",
            "archetype": SCENARIO_PRESETS[preset]["archetype"],
            "resolved": {},
            "provisioned_at": "pending",
        }
        selected_profile = self._load_profile(profile, require_file=False)
        selected_stack = self.manifest.stack(selected_profile["stack"])
        required = self._dependency_roles(selected_profile, {"server"})
        self._require_classic_contracts(profile, {"server"})
        metadata["providers"] = {
            role: selected_stack.providers[role].name for role in sorted(required)
        }
        operation_lock = self.paths.scenarios / ".locks" / f"{name}.lock"
        with exclusive_lock(operation_lock, "scenario operation"):
            if root.exists() or root.is_symlink():
                raise WorkspaceError(f"scenario already exists: {name}")
            if state_name in self._load_states():
                raise WorkspaceError(f"state already exists: {state_name}")
            staging = Path(
                tempfile.mkdtemp(prefix=f".{name}-", dir=self.paths.scenarios)
            )
            try:
                atomic_json(
                    staging / MANAGED_MARKER,
                    {"schema_version": SCHEMA_VERSION, "purpose": "test-scenario"},
                )
                password = secrets.token_urlsafe(12)
                password_file = staging / "password"
                self._write_scenario_password(password_file, password)
                server_source = self.component_path("server", profile)
                state = self.state_path(
                    state_name, server_source, resolved_path=staging / "state"
                )
                metadata["resolved"] = self._scenario_provision_state(
                    metadata, state, password_file
                )
                metadata["provisioned_at"] = datetime.now(timezone.utc).isoformat()
                self._publish_scenario_references(name, metadata)
                try:
                    durable_atomic_json(staging / "scenario.json", metadata)
                    staging.replace(root)
                    try:
                        self._register_state(state_name, (root / "state").resolve())
                    except BaseException:
                        shutil.rmtree(root)
                        raise
                except BaseException:
                    self._remove_physical_reference(root / "scenario.json")
                    raise
            except BaseException:
                if staging.exists():
                    shutil.rmtree(staging)
                raise
        return self.scenario_show(name)

    def scenario_show(self, name: str) -> dict[str, Any]:
        metadata = self._load_scenario(name)
        return {**metadata, "path": str(self._scenario_directory(name))}

    def scenario_list(self) -> list[dict[str, Any]]:
        self.paths.ensure()
        registered_states = self._load_states()
        scenarios: list[dict[str, Any]] = []
        for path in sorted(self.paths.scenarios.iterdir()):
            if path.is_dir() and not path.name.startswith("."):
                scenario_path = path
                try:
                    scenario_path = self._scenario_directory(path.name)
                    metadata = self._load_scenario(path.name, registered_states)
                    scenarios.append({**metadata, "path": str(scenario_path)})
                except _InertScenarioError as error:
                    scenarios.append(
                        {
                            "name": path.name,
                            "path": str(scenario_path),
                            "inert": True,
                            "inert_reason": error.reason,
                        }
                    )
                except WorkspaceError:
                    scenarios.append(
                        {
                            "name": path.name,
                            "path": str(scenario_path),
                            "inert": True,
                            "inert_reason": SCENARIO_INERT_INVALID_RECORD,
                        }
                    )
        return scenarios

    def scenario_credentials(self, name: str) -> dict[str, str]:
        metadata = self._load_scenario(name)
        return {
            "account": metadata["account"],
            "character": metadata["character"],
            "password": self._read_scenario_password(
                self._scenario_directory(name) / "password"
            ),
        }

    def scenario_reset(self, name: str) -> dict[str, Any]:
        self.paths.ensure()
        validate_name(name, "scenario name")
        metadata = self._load_scenario(name)
        with self._resolved_profile_operation(
            metadata["profile"], {"server"}, f"reset scenario {name}"
        ):
            with self._resource_locks(
                [
                    self._lease_request(
                        "scenario", name, "exclusive", f"reset scenario {name}"
                    )
                ],
            ):
                return self._scenario_reset(name)

    def _scenario_reset(self, name: str) -> dict[str, Any]:
        self.paths.ensure()
        root = self._scenario_directory(name)
        operation_lock = self.paths.scenarios / ".locks" / f"{name}.lock"
        with exclusive_lock(operation_lock, "scenario operation"):
            metadata = self._load_scenario(name)
            old_sources = {
                str(Path(value["checkout_path"]).resolve(strict=False))
                for value in metadata["resolved"].values()
                if isinstance(value, dict)
                and isinstance(value.get("checkout_path"), str)
            }
            state = root / "state"
            with self._topology_state_lock(state):
                state_identity = self._state_identity(state)
                state_fd = self._lock_state_directory_mutation(
                    state, state_identity, "scenario server state"
                )
                staging_root: Path | None = None
                try:
                    staging_root = Path(
                        tempfile.mkdtemp(
                            prefix=f".{name}-reset-", dir=self.paths.scenarios
                        )
                    )
                    server_source = self.component_path("server", metadata["profile"])
                    staging_state = self.state_path(
                        metadata["state"],
                        server_source,
                        resolved_path=staging_root / "state",
                    )
                    metadata["resolved"] = self._scenario_provision_state(
                        metadata, staging_state, root / "password"
                    )
                    metadata["provisioned_at"] = datetime.now(timezone.utc).isoformat()
                    self._publish_scenario_references(
                        name, metadata, old_sources
                    )
                    replace_directory(state, staging_state, f".{name}-state-previous-")
                    durable_atomic_json(root / "scenario.json", metadata)
                    self._publish_scenario_references(name, metadata)
                finally:
                    os.close(state_fd)
                    if staging_root is not None and staging_root.exists():
                        shutil.rmtree(staging_root)
        return self.scenario_show(name)

    def _load_states(self) -> dict[str, str]:
        if not self.paths.states_file.exists():
            return {}
        value = load_json(self.paths.states_file)
        if not isinstance(value, dict):
            raise WorkspaceError("states registry must be an object")
        require_keys(value, {"schema_version", "states"}, "states registry")
        if value["schema_version"] != SCHEMA_VERSION or not isinstance(value["states"], dict):
            raise WorkspaceError("states registry schema is invalid")
        states: dict[str, str] = {}
        for name, path in value["states"].items():
            validate_name(name, "state name")
            if not isinstance(path, str) or not Path(path).is_absolute():
                raise WorkspaceError(f"state path must be absolute: {name}")
            states[name] = path
        return states

    @staticmethod
    def _canonical_state_path(path: Path) -> Path:
        """Return a lexical absolute state path after rejecting existing links."""

        candidate = Path(os.path.abspath(path.expanduser()))
        current = Path(candidate.anchor)
        for part in candidate.parts[1:]:
            current /= part
            try:
                metadata = current.lstat()
            except FileNotFoundError:
                break
            except (OSError, ValueError) as error:
                raise WorkspaceError(
                    f"cannot inspect server state path {current}: {error}"
                ) from error
            if stat.S_ISLNK(metadata.st_mode):
                raise WorkspaceError(f"server state path contains a symlink: {current}")
            if current != candidate and not stat.S_ISDIR(metadata.st_mode):
                raise WorkspaceError(
                    f"server state parent is not a directory: {current}"
                )
        return candidate

    @staticmethod
    def _state_identity(path: Path) -> dict[str, int]:
        metadata = path.stat(follow_symlinks=False)
        if not stat.S_ISDIR(metadata.st_mode):
            raise WorkspaceError(f"server state is not a directory: {path}")
        return {"device": metadata.st_dev, "inode": metadata.st_ino}

    @staticmethod
    def _state_implementation(
        stack: str,
        providers: dict[str, str],
        resolved: dict[str, dict[str, Any]],
    ) -> dict[str, str]:
        provider = providers["server"]
        coordinate = resolved[provider]
        return {
            "stack": stack,
            "provider": provider,
            "repository": coordinate["repository"],
        }

    @staticmethod
    def _validate_state_implementation(
        path: Path,
        implementation: dict[str, str] | None,
    ) -> None:
        marker = path / STATE_IMPLEMENTATION_MARKER
        if not marker.exists() and not marker.is_symlink():
            return
        if marker.is_symlink() or not marker.is_file():
            raise WorkspaceError(f"server state implementation marker is invalid: {path}")
        value = load_json(marker)
        expected = (
            {
                "schema_version": STATE_IMPLEMENTATION_SCHEMA_VERSION,
                **implementation,
            }
            if implementation is not None
            else None
        )
        if expected is None or value != expected:
            raise WorkspaceError(
                f"server state implementation does not match the selected server: {path}"
            )

    @staticmethod
    def _load_state_json_at(
        directory_fd: int, name: str, description: str
    ) -> Any:
        descriptor: int | None = None
        try:
            descriptor = os.open(
                name,
                os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC | os.O_NOFOLLOW,
                dir_fd=directory_fd,
            )
            metadata = os.fstat(descriptor)
            if (
                not stat.S_ISREG(metadata.st_mode)
                or metadata.st_nlink != 1
                or metadata.st_size > 4 * 1024 * 1024
            ):
                raise WorkspaceError(f"{description} is invalid")
            with os.fdopen(descriptor, encoding="utf-8", closefd=False) as stream:
                return json.load(stream, object_pairs_hook=_reject_duplicate_keys)
        except (OSError, UnicodeError, ValueError, RecursionError) as error:
            raise WorkspaceError(f"cannot read {description}: {error}") from error
        finally:
            if descriptor is not None:
                os.close(descriptor)

    @staticmethod
    def _write_state_json_no_replace_at(
        directory_fd: int, name: str, value: Any
    ) -> None:
        temporary = f".{name}.{secrets.token_hex(12)}.tmp"
        descriptor: int | None = None
        try:
            descriptor = os.open(
                temporary,
                os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC | os.O_NOFOLLOW,
                0o600,
                dir_fd=directory_fd,
            )
            with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
                descriptor = None
                json.dump(value, stream, indent=2, sort_keys=True)
                stream.write("\n")
                stream.flush()
                os.fsync(stream.fileno())
            rename_no_replace_at(
                directory_fd, temporary, directory_fd, name
            )
            os.fsync(directory_fd)
        except OSError as error:
            raise WorkspaceError(
                f"cannot publish server state implementation marker: {error}"
            ) from error
        finally:
            if descriptor is not None:
                os.close(descriptor)
            try:
                os.unlink(temporary, dir_fd=directory_fd)
            except FileNotFoundError:
                pass

    def _validate_state_descriptor(
        self,
        directory_fd: int,
        path: Path,
        implementation: dict[str, str] | None,
        *,
        write_implementation: bool,
    ) -> None:
        for name in EXPECTED_SERVER_DATA["files"]:
            try:
                metadata = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            except OSError as error:
                raise WorkspaceError(
                    f"server state lacks required file {name}: {path}"
                ) from error
            if not stat.S_ISREG(metadata.st_mode):
                raise WorkspaceError(f"server state lacks required file {name}: {path}")
        for name in EXPECTED_SERVER_DATA["directories"]:
            try:
                metadata = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            except OSError as error:
                raise WorkspaceError(
                    f"server state lacks required directory {name}: {path}"
                ) from error
            if not stat.S_ISDIR(metadata.st_mode):
                raise WorkspaceError(
                    f"server state lacks required directory {name}: {path}"
                )
        marker_missing = False
        try:
            marker_metadata = os.stat(
                STATE_IMPLEMENTATION_MARKER,
                dir_fd=directory_fd,
                follow_symlinks=False,
            )
        except FileNotFoundError:
            marker_missing = True
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect server state implementation marker: {path}: {error}"
            ) from error
        if marker_missing and implementation is not None and write_implementation:
            self._write_state_json_no_replace_at(
                directory_fd,
                STATE_IMPLEMENTATION_MARKER,
                {
                    "schema_version": STATE_IMPLEMENTATION_SCHEMA_VERSION,
                    **implementation,
                },
            )
            marker_metadata = os.stat(
                STATE_IMPLEMENTATION_MARKER,
                dir_fd=directory_fd,
                follow_symlinks=False,
            )
            marker_missing = False
        if not marker_missing:
            if not stat.S_ISREG(marker_metadata.st_mode):
                raise WorkspaceError(
                    f"server state implementation marker is invalid: {path}"
                )
            expected = (
                {
                    "schema_version": STATE_IMPLEMENTATION_SCHEMA_VERSION,
                    **implementation,
                }
                if implementation is not None
                else None
            )
            if self._load_state_json_at(
                directory_fd,
                STATE_IMPLEMENTATION_MARKER,
                f"server state implementation marker {path}",
            ) != expected:
                raise WorkspaceError(
                    "server state implementation does not match the selected "
                    f"server: {path}"
                )
        try:
            os.mkdir("tmp", dir_fd=directory_fd)
        except FileExistsError:
            pass
        tmp_metadata = os.stat("tmp", dir_fd=directory_fd, follow_symlinks=False)
        if not stat.S_ISDIR(tmp_metadata.st_mode):
            raise WorkspaceError(f"server state tmp path is invalid: {path}")

    def _open_validated_state_directory(
        self,
        path: Path,
        implementation: dict[str, str] | None,
        *,
        write_implementation: bool,
    ) -> int:
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        try:
            descriptor = _open_directory_nofollow(path, flags)
        except OSError as error:
            raise WorkspaceError(
                f"cannot open server state without following links: {path}: {error}"
            ) from error
        try:
            opened = os.fstat(descriptor)
            visible = path.stat(follow_symlinks=False)
            if (
                not stat.S_ISDIR(opened.st_mode)
                or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
                or self._canonical_state_path(path) != path
            ):
                raise WorkspaceError(
                    f"server state identity changed during validation: {path}"
                )
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError as error:
                if error.errno in {errno.EACCES, errno.EAGAIN}:
                    raise WorkspaceError(
                        f"physical server state is already in use: {path}"
                    ) from error
                raise WorkspaceError(
                    f"cannot lock physical server state: {path}: {error}"
                ) from error
            self._validate_state_descriptor(
                descriptor,
                path,
                implementation,
                write_implementation=write_implementation,
            )
            visible = path.stat(follow_symlinks=False)
            if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
                raise WorkspaceError(
                    f"server state identity changed during validation: {path}"
                )
            return descriptor
        except BaseException:
            os.close(descriptor)
            raise

    @staticmethod
    def _temporary_state_metadata_matches(
        policy: dict[str, Any], creation_policy: object
    ) -> bool:
        """Match mutable lifecycle policy to its immutable creation record."""

        immutable = {
            "mode",
            "path",
            "owner",
            "created_at",
            "identity",
            "implementation",
            "profile",
            "server",
        }
        return bool(
            isinstance(creation_policy, dict)
            and set(creation_policy) == immutable | {"name", "lifecycle"}
            and creation_policy.get("name") is None
            and creation_policy.get("lifecycle") == "disposable"
            and all(creation_policy.get(key) == policy.get(key) for key in immutable)
        )

    def list_states(self) -> dict[str, str]:
        self.paths.ensure()
        states = self._load_states()
        if "default" not in states:
            states = {
                "default": str(self.paths.state / "server" / "default"),
                **states,
            }
        return states

    def _state_location(self, name: str) -> Path:
        validate_name(name, "state name")
        states = self._load_states()
        if name in states:
            return self._canonical_state_path(Path(states[name]))
        if name == "default":
            return self._canonical_state_path(
                self.paths.state / "server" / "default"
            )
        raise WorkspaceError(f"state does not exist: {name}")

    def state_path(
        self,
        name: str,
        server_source: Path,
        resolved_path: Path | None = None,
        *,
        implementation: dict[str, str] | None = None,
        write_implementation: bool = False,
        keep_descriptor: bool = False,
    ) -> Path | tuple[Path, int]:
        validate_name(name, "state name")
        path = resolved_path or self._state_location(name)
        path = self._canonical_state_path(path)
        server_source = server_source.resolve()
        if server_source == path or server_source in path.parents:
            raise WorkspaceError(f"server state must be outside its source worktree: {path}")
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            staging = Path(tempfile.mkdtemp(prefix=f".{path.name}.", dir=path.parent))
            try:
                shutil.copytree(server_source / "install_data", staging, dirs_exist_ok=True)
                self._make_tree_owner_writable(staging)
                (staging / "tmp").mkdir()
                if implementation is not None and write_implementation:
                    durable_atomic_json(
                        staging / STATE_IMPLEMENTATION_MARKER,
                        {
                            "schema_version": STATE_IMPLEMENTATION_SCHEMA_VERSION,
                            **implementation,
                        },
                    )
                self._validate_state(staging)
                if path.exists():
                    raise WorkspaceError(f"state appeared during initialization: {path}")
                rename_no_replace(staging, path)
            except BaseException:
                shutil.rmtree(staging, ignore_errors=True)
                raise
        descriptor = self._open_validated_state_directory(
            path,
            implementation,
            write_implementation=write_implementation,
        )
        if keep_descriptor:
            return path, descriptor
        os.close(descriptor)
        return path

    def _prepared_state_path(
        self,
        name: str,
        server_source: Path,
        resolved_path: Path,
        implementation: dict[str, str],
        expected_identity: dict[str, int] | None,
    ) -> tuple[Path, int]:
        prepared = self.state_path(
            name,
            server_source,
            resolved_path=resolved_path,
            implementation=implementation,
            write_implementation=True,
            keep_descriptor=True,
        )
        assert isinstance(prepared, tuple)
        path, descriptor = prepared
        opened = os.fstat(descriptor)
        try:
            visible = path.stat(follow_symlinks=False)
            canonical = self._canonical_state_path(path)
        except BaseException:
            os.close(descriptor)
            raise
        if (
            canonical != path
            or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
        ):
            os.close(descriptor)
            raise WorkspaceError(
                f"server state identity changed while preparing topology: {path}"
            )
        if expected_identity is not None and expected_identity != {
            "device": opened.st_dev,
            "inode": opened.st_ino,
        }:
            os.close(descriptor)
            raise WorkspaceError(
                f"server state identity changed while preparing topology: {path}"
            )
        return path, descriptor

    def _persistent_state_policy(
        self,
        state_name: str,
        path: Path,
        implementation: dict[str, str],
    ) -> dict[str, Any]:
        owner, lifecycle = self._persistent_state_ownership(state_name, path)
        return {
            "mode": "default" if state_name == "default" else "named",
            "name": state_name,
            "path": str(path),
            "owner": owner,
            "lifecycle": lifecycle,
            "identity": self._state_identity(path),
            "implementation": implementation,
        }

    def _persistent_state_ownership(
        self, state_name: str, path: Path
    ) -> tuple[dict[str, str], str]:
        registered = self._load_states()
        scenario_root = self.paths.scenarios / state_name.removeprefix("scenario-")
        scenario_path = scenario_root / "state"
        if (
            state_name.startswith("scenario-")
            and state_name in registered
            and self._canonical_state_path(scenario_path) == path
        ):
            owner = {"kind": "scenario", "name": state_name.removeprefix("scenario-")}
            lifecycle = "scenario-owned"
        elif (promoted := self._promoted_state_owner(state_name, path)) is not None:
            owner = promoted
            lifecycle = "persistent-promoted"
        elif path.is_relative_to(self.paths.state.resolve(strict=False)):
            owner = {"kind": "workspace"}
            lifecycle = "persistent"
        else:
            owner = {"kind": "external"}
            lifecycle = "persistent-external"
        return owner, lifecycle

    def _promoted_state_owner(
        self, state_name: str, path: Path
    ) -> dict[str, str] | None:
        record_path = path / PROMOTED_STATE_METADATA
        if record_path.is_symlink() or not record_path.is_file():
            ownership_path = path / MANAGED_MARKER
            ownership = (
                load_regular_json(ownership_path, "promoted state ownership")
                if ownership_path.is_file() and not ownership_path.is_symlink()
                else None
            )
            if (
                isinstance(ownership, dict)
                and ownership.get("purpose") == "temporary-topology-state"
            ):
                raise WorkspaceError(
                    f"promoted state provenance is missing: {record_path}; "
                    "retry its originating state promote command"
                )
            creation_path = path / TEMPORARY_STATE_METADATA
            if creation_path.exists() or creation_path.is_symlink():
                if creation_path.is_symlink() or not creation_path.is_file():
                    raise WorkspaceError(
                        f"promoted state creation metadata is invalid: {creation_path}"
                    )
                creation = load_regular_json(
                    creation_path, "promoted state creation metadata"
                )
                creation_policy = (
                    creation.get("state_policy")
                    if isinstance(creation, dict)
                    else None
                )
                if (
                    not isinstance(creation, dict)
                    or creation.get("schema_version")
                    != TEMPORARY_STATE_SCHEMA_VERSION
                    or not isinstance(creation_policy, dict)
                    or creation_policy.get("mode") != "temporary"
                    or creation_policy.get("path") != str(path)
                ):
                    raise WorkspaceError(
                        f"promoted state creation metadata is invalid: {creation_path}"
                    )
                raise WorkspaceError(
                    f"promoted state provenance is missing: {record_path}; "
                    "retry its originating state promote command"
                )
            if self.paths.topologies.is_dir() and not self.paths.topologies.is_symlink():
                for topology in self.paths.topologies.iterdir():
                    status_path = topology / "status.json"
                    if status_path.is_symlink() or not status_path.is_file():
                        continue
                    try:
                        status = load_regular_json(
                            status_path, "promoted state origin status"
                        )
                    except WorkspaceError:
                        continue
                    policy = (
                        status.get("state_policy")
                        if isinstance(status, dict)
                        else None
                    )
                    if (
                        isinstance(policy, dict)
                        and policy.get("mode") == "temporary"
                        and policy.get("lifecycle")
                        in {"promotion-pending", "promoted"}
                        and policy.get("name") == state_name
                        and policy.get("path") == str(path)
                    ):
                        raise WorkspaceError(
                            f"promoted state provenance is missing: {record_path}; "
                            f"retry ./atrinik state promote {topology.name} {state_name}"
                        )
            return None
        record = load_regular_json(record_path, "promoted state provenance")
        identity = record.get("identity") if isinstance(record, dict) else None
        if (
            not isinstance(record, dict)
            or record.get("schema_version") != SCHEMA_VERSION
            or record.get("name") != state_name
            or record.get("path") != str(path)
            or not isinstance(record.get("topology"), str)
            or not isinstance(record.get("generation"), str)
            or not re.fullmatch(r"[0-9a-f]{64}", record["generation"])
            or not isinstance(identity, dict)
            or set(identity) != {"device", "inode"}
            or not all(
                isinstance(value, int)
                and not isinstance(value, bool)
                and value >= 0
                for value in identity.values()
            )
        ):
            raise WorkspaceError(
                f"promoted state provenance is invalid: {record_path}"
            )
        try:
            visible = path.stat(follow_symlinks=False)
        except OSError as error:
            raise WorkspaceError(
                f"promoted state provenance is invalid: {record_path}: {error}"
            ) from error
        if (
            not stat.S_ISDIR(visible.st_mode)
            or {"device": visible.st_dev, "inode": visible.st_ino} != identity
        ):
            raise WorkspaceError(
                f"promoted state provenance is invalid: {record_path}"
            )
        return {
            "kind": "promoted-topology-state",
            "topology": record["topology"],
            "generation": record["generation"],
        }

    def _temporary_state_container(self, topology_root: Path) -> tuple[Path, int]:
        container = topology_root / "temporary-states"
        expected = {
            "schema_version": SCHEMA_VERSION,
            "purpose": "topology-temporary-states",
        }
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        root_fd = _open_directory_nofollow(topology_root, flags)
        container_fd: int | None = None
        try:
            root_marker = self._load_state_json_at(
                root_fd, MANAGED_MARKER, "topology ownership marker"
            )
            if root_marker != {
                "schema_version": SCHEMA_VERSION,
                "purpose": f"topology:{topology_root.name}",
            }:
                raise WorkspaceError(
                    f"topology ownership marker is invalid: {topology_root}"
                )
            try:
                visible = os.stat(
                    "temporary-states", dir_fd=root_fd, follow_symlinks=False
                )
            except FileNotFoundError:
                staging = f".temporary-states.{secrets.token_hex(12)}.tmp"
                staging_fd: int | None = None
                staging_identity: dict[str, int] | None = None
                try:
                    os.mkdir(staging, mode=0o700, dir_fd=root_fd)
                    created = os.stat(
                        staging, dir_fd=root_fd, follow_symlinks=False
                    )
                    staging_identity = {
                        "device": created.st_dev,
                        "inode": created.st_ino,
                    }
                    staging_fd = os.open(staging, flags, dir_fd=root_fd)
                    opened_staging = os.fstat(staging_fd)
                    if (
                        not stat.S_ISDIR(created.st_mode)
                        or (opened_staging.st_dev, opened_staging.st_ino)
                        != (created.st_dev, created.st_ino)
                    ):
                        raise WorkspaceError(
                            "temporary state container staging changed"
                        )
                    durable_atomic_json_at(
                        staging_fd, MANAGED_MARKER, expected
                    )
                    visible_staging = os.stat(
                        staging, dir_fd=root_fd, follow_symlinks=False
                    )
                    if (visible_staging.st_dev, visible_staging.st_ino) != (
                        created.st_dev,
                        created.st_ino,
                    ):
                        raise WorkspaceError(
                            "temporary state container staging changed"
                        )
                    rename_no_replace_at(
                        root_fd, staging, root_fd, "temporary-states"
                    )
                    os.fsync(root_fd)
                except BaseException:
                    if staging_fd is not None:
                        os.close(staging_fd)
                    if staging_identity is not None:
                        try:
                            remove_owned_tree(
                                topology_root / staging,
                                expected_identity=staging_identity,
                                parent_directory_fd=root_fd,
                            )
                        except FileNotFoundError:
                            pass
                    raise
                else:
                    if staging_fd is not None:
                        os.close(staging_fd)
                visible = os.stat(
                    "temporary-states", dir_fd=root_fd, follow_symlinks=False
                )
                container_fd = os.open("temporary-states", flags, dir_fd=root_fd)
            else:
                container_fd = os.open("temporary-states", flags, dir_fd=root_fd)
            opened = os.fstat(container_fd)
            if (
                not stat.S_ISDIR(visible.st_mode)
                or (visible.st_dev, visible.st_ino)
                != (opened.st_dev, opened.st_ino)
                or _descriptor_mount_id(container_fd)
                != _descriptor_mount_id(root_fd)
                or self._load_state_json_at(
                    container_fd,
                    MANAGED_MARKER,
                    "temporary state container marker",
                )
                != expected
            ):
                raise WorkspaceError(
                    f"temporary state container is invalid: {container}"
                )
            result = container_fd
            container_fd = None
            return container, result
        finally:
            if container_fd is not None:
                os.close(container_fd)
            os.close(root_fd)

    @contextmanager
    def _topology_state_lock(
        self,
        path: Path,
        *,
        preparing_topology: str | None = None,
        physical_identity: bool = True,
    ) -> Iterator[StateLease]:
        requested_identity = (
            self._state_identity(path)
            if physical_identity and path.exists() and not path.is_symlink()
            else None
        )
        try:
            with ExitStack() as leases:
                state_lease: StateLease

                def bind_identity(identity: dict[str, int]) -> TextIO | None:
                    if not physical_identity:
                        return None
                    return leases.enter_context(
                        exclusive_lock(
                            self._lease_namespace
                            / f"state-identity-{identity['device']}-"
                            f"{identity['inode']}.lock",
                            f"physical server state {path}",
                            nonblocking=True,
                        )
                    )
                path_lock = leases.enter_context(
                    exclusive_lock(
                        Path(f"{path}.lock"),
                        f"server state {path}",
                        nonblocking=True,
                    )
                )
                state_lease = StateLease(path_lock, bind_identity)
                if requested_identity is not None:
                    state_lease.bind(requested_identity)
                for topology in sorted(self.paths.topologies.iterdir()):
                    if topology.name == preparing_topology:
                        continue
                    if topology.is_symlink() or not topology.is_dir():
                        continue
                    status_path = topology / "status.json"
                    if not status_path.is_file() or status_path.is_symlink():
                        continue
                    try:
                        raw_status = load_regular_json(
                            status_path, "topology state ownership status"
                        )
                    except WorkspaceError as error:
                        raise WorkspaceError(
                            "cannot confirm exact server state ownership while "
                            f"topology status is uncertain: {path}"
                        ) from error
                    raw_policy = (
                        raw_status.get("state_policy")
                        if isinstance(raw_status, dict)
                        else None
                    )
                    same_state = isinstance(raw_status, dict) and (
                        raw_status.get("state") == str(path)
                        or (
                            requested_identity is not None
                            and isinstance(raw_policy, dict)
                            and raw_policy.get("identity") == requested_identity
                        )
                    )
                    if not same_state:
                        continue
                    try:
                        status = self.topology_status(topology.name)
                    except WorkspaceError as error:
                        control = raw_status.get("control")
                        if (
                            isinstance(control, dict)
                            and not self._topology_process_tree_active(
                                topology, control
                            )
                        ):
                            continue
                        raise WorkspaceError(
                            "cannot confirm exact server state ownership while "
                            f"topology {topology.name} status is uncertain: {path}"
                        ) from error
                    observation = status.get("observation")
                    control = status.get("control")
                    if (
                        (
                            status.get("state") == str(path)
                            or (
                                requested_identity is not None
                                and status.get("state_policy", {}).get("identity")
                                == requested_identity
                            )
                        )
                        and isinstance(observation, dict)
                        and observation.get("process_tree_lease") == "retained"
                        and isinstance(control, dict)
                    ):
                        raise WorkspaceError(
                            f"server state {path} is owned by topology "
                            f"{status['name']} generation {control['generation']}; "
                            f"run ./atrinik down {status['name']} and retry"
                        )
                yield state_lease
                return
        except LockBusyError as error:
            owner: tuple[str, str] | None = None
            try:
                for status in self.topology_statuses():
                    observation = status.get("observation")
                    control = status.get("control")
                    if (
                        (
                            status.get("state") == str(path)
                            or (
                                requested_identity is not None
                                and status.get("state_policy", {}).get("identity")
                                == requested_identity
                            )
                        )
                        and isinstance(observation, dict)
                        and observation.get("process_tree_lease") == "retained"
                        and isinstance(control, dict)
                    ):
                        owner = (status["name"], control["generation"])
                        break
            except WorkspaceError:
                pass
            if owner is not None:
                raise WorkspaceError(
                    f"server state {path} is owned by topology {owner[0]} "
                    f"generation {owner[1]}; run ./atrinik down {owner[0]} and retry"
                ) from error
            raise WorkspaceError(
                f"server state {path} is busy but its exact owner cannot be "
                "confirmed; inspect ./atrinik ps --json and preserve the state"
            ) from error

    def _create_temporary_state(
        self,
        topology_root: Path,
        topology_name: str,
        profile_name: str,
        generation: str,
        server_source: Path,
        implementation: dict[str, str],
        server_coordinate: dict[str, Any],
    ) -> tuple[Path, dict[str, Any]]:
        container, container_fd = self._temporary_state_container(topology_root)
        destination = container / generation
        try:
            os.stat(generation, dir_fd=container_fd, follow_symlinks=False)
        except FileNotFoundError:
            pass
        else:
            os.close(container_fd)
            raise WorkspaceError(
                f"temporary topology state already exists for generation {generation}"
            )
        staging: Path | None = None
        staging_identity: dict[str, int] | None = None
        try:
            staging = Path(tempfile.mkdtemp(
                prefix=f".{generation}.", dir=f"/proc/self/fd/{container_fd}"
            ))
            staging_name = staging.name
            created = os.stat(
                staging_name, dir_fd=container_fd, follow_symlinks=False
            )
            staging_identity = {
                "device": created.st_dev,
                "inode": created.st_ino,
            }
            staging_fd = os.open(
                staging_name,
                os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
                dir_fd=container_fd,
            )
            opened_staging = os.fstat(staging_fd)
            if (
                not stat.S_ISDIR(created.st_mode)
                or (opened_staging.st_dev, opened_staging.st_ino)
                != (created.st_dev, created.st_ino)
            ):
                raise WorkspaceError(
                    "temporary topology state staging changed"
                )
        except BaseException:
            if staging is not None and staging_identity is not None:
                try:
                    remove_owned_tree(
                        staging,
                        expected_identity=staging_identity,
                        parent_directory_fd=container_fd,
                    )
                except FileNotFoundError:
                    pass
            os.close(container_fd)
            raise
        published_identity: dict[str, int] | None = None
        created_at = datetime.now(timezone.utc).isoformat()
        install_data = server_source / "install_data"
        staging_access = Path(f"/proc/self/fd/{staging_fd}")
        try:
            source_digest = _tree_digest(
                install_data, set(), reject_symlinks=True
            )
            shutil.copytree(
                install_data, staging_access, dirs_exist_ok=True
            )
            if (
                _tree_digest_descriptor(staging_fd, destination) != source_digest
                or _tree_digest(install_data, set(), reject_symlinks=True)
                != source_digest
                or _tree_digest(install_data, set(), reject_symlinks=True)
                != source_digest
            ):
                raise WorkspaceError(
                    "selected server install_data changed during temporary state "
                    "initialization"
                )
            os.mkdir("tmp", dir_fd=staging_fd)
            durable_atomic_json_at(
                staging_fd,
                STATE_IMPLEMENTATION_MARKER,
                {
                    "schema_version": STATE_IMPLEMENTATION_SCHEMA_VERSION,
                    **implementation,
                },
            )
            self._validate_state_descriptor(
                staging_fd,
                destination,
                implementation,
                write_implementation=False,
            )
            staging_metadata = os.fstat(staging_fd)
            identity = {
                "device": staging_metadata.st_dev,
                "inode": staging_metadata.st_ino,
            }
            policy = {
                "mode": "temporary",
                "name": None,
                "path": str(destination),
                "owner": {
                    "kind": "topology-generation",
                    "topology": topology_name,
                    "generation": generation,
                },
                "lifecycle": "disposable",
                "created_at": created_at,
                "identity": identity,
                "implementation": implementation,
                "profile": profile_name,
                "server": server_coordinate,
            }
            durable_atomic_json_at(
                staging_fd,
                MANAGED_MARKER,
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "temporary-topology-state",
                    "topology": topology_name,
                    "generation": generation,
                },
            )
            durable_atomic_json_at(
                staging_fd,
                TEMPORARY_STATE_METADATA,
                {
                    "schema_version": TEMPORARY_STATE_SCHEMA_VERSION,
                    "state_policy": policy,
                },
            )
            managed_record = {
                "schema_version": SCHEMA_VERSION,
                "purpose": "temporary-topology-state",
                "topology": topology_name,
                "generation": generation,
            }
            creation_record = {
                "schema_version": TEMPORARY_STATE_SCHEMA_VERSION,
                "state_policy": policy,
            }

            def validate_publication_state() -> None:
                self._validate_temporary_state_integrity(
                    staging_fd,
                    destination,
                    implementation,
                    container_fd,
                    staging_name if published_identity is None else generation,
                )
                if self._load_state_json_at(
                    staging_fd,
                    MANAGED_MARKER,
                    "temporary state ownership marker",
                ) != managed_record or self._load_state_json_at(
                    staging_fd,
                    TEMPORARY_STATE_METADATA,
                    "temporary state creation record",
                ) != creation_record:
                    raise WorkspaceError(
                        "temporary topology state metadata changed before publication"
                    )
                tmp_fd = os.open(
                    "tmp",
                    os.O_RDONLY
                    | os.O_CLOEXEC
                    | os.O_DIRECTORY
                    | os.O_NOFOLLOW,
                    dir_fd=staging_fd,
                )
                try:
                    tmp_metadata = os.fstat(tmp_fd)
                    if (
                        tmp_metadata.st_dev != staging_metadata.st_dev
                        or _descriptor_mount_id(tmp_fd)
                        != _descriptor_mount_id(staging_fd)
                        or os.listdir(tmp_fd)
                    ):
                        raise WorkspaceError(
                            "temporary topology state mutable output is not empty"
                        )
                finally:
                    os.close(tmp_fd)

            validate_publication_state()
            copied_exclusions = {
                "tmp",
                STATE_IMPLEMENTATION_MARKER,
                MANAGED_MARKER,
                TEMPORARY_STATE_METADATA,
            }
            if (
                _tree_digest_descriptor(
                    staging_fd,
                    destination,
                    copied_exclusions,
                )
                != source_digest
                or _tree_digest(install_data, set(), reject_symlinks=True)
                != source_digest
            ):
                raise WorkspaceError(
                    "selected server install_data changed before temporary state "
                    "publication"
                )
            full_tree_digest = _tree_digest_descriptor(staging_fd, destination)
            validate_publication_state()
            if (
                _tree_digest_descriptor(staging_fd, destination)
                != full_tree_digest
            ):
                raise WorkspaceError(
                    "temporary topology state changed before publication"
                )
            visible_staging = os.stat(
                staging_name, dir_fd=container_fd, follow_symlinks=False
            )
            if (visible_staging.st_dev, visible_staging.st_ino) != (
                identity["device"], identity["inode"]
            ):
                raise WorkspaceError(
                    "temporary topology state staging changed before publication"
                )
            rename_no_replace_at(
                container_fd, staging_name, container_fd, generation
            )
            published_identity = identity
            if (
                _tree_digest_descriptor(
                    staging_fd,
                    destination,
                    copied_exclusions,
                )
                != source_digest
                or _tree_digest(install_data, set(), reject_symlinks=True)
                != source_digest
            ):
                raise WorkspaceError(
                    "selected server install_data changed during temporary state "
                    "publication"
                )
            validate_publication_state()
            if (
                _tree_digest_descriptor(staging_fd, destination)
                != full_tree_digest
            ):
                raise WorkspaceError(
                    "temporary topology state changed during publication"
                )
            published = os.stat(
                generation, dir_fd=container_fd, follow_symlinks=False
            )
            visible_container = container.stat(follow_symlinks=False)
            opened_container = os.fstat(container_fd)
            if (
                (published.st_dev, published.st_ino)
                != (identity["device"], identity["inode"])
                or (visible_container.st_dev, visible_container.st_ino)
                != (opened_container.st_dev, opened_container.st_ino)
            ):
                raise WorkspaceError(
                    f"temporary topology state identity changed during publication: {destination}"
                )
            return destination, policy
        except BaseException:
            if published_identity is not None:
                remove_owned_tree(
                    destination,
                    expected_identity=published_identity,
                    parent_directory_fd=container_fd,
                )
            else:
                try:
                    os.stat(
                        staging_name,
                        dir_fd=container_fd,
                        follow_symlinks=False,
                    )
                except FileNotFoundError:
                    pass
                else:
                    remove_owned_tree(
                        staging,
                        expected_identity=staging_identity,
                        parent_directory_fd=container_fd,
                    )
            raise
        finally:
            os.close(staging_fd)
            os.close(container_fd)

    def _validate_state(self, path: Path) -> None:
        if not path.is_dir():
            raise WorkspaceError(f"server state is not a directory: {path}")
        for name in EXPECTED_SERVER_DATA["files"]:
            if not (path / name).is_file():
                raise WorkspaceError(f"server state lacks required file {name}: {path}")
        for name in EXPECTED_SERVER_DATA["directories"]:
            if not (path / name).is_dir():
                raise WorkspaceError(f"server state lacks required directory {name}: {path}")

    @staticmethod
    def _validate_temporary_state_integrity(
        directory_fd: int,
        path: Path,
        implementation: dict[str, str] | None = None,
        parent_directory_fd: int | None = None,
        entry_name: str | None = None,
    ) -> None:
        """Fail closed before deleting wrapper-owned mutable server state."""

        root = os.fstat(directory_fd)
        root_mount = _descriptor_mount_id(directory_fd)
        parent_fd = (
            os.dup(parent_directory_fd)
            if parent_directory_fd is not None
            else _open_directory_nofollow(
                path.parent,
                os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
            )
        )
        try:
            visible = os.stat(
                entry_name or path.name,
                dir_fd=parent_fd,
                follow_symlinks=False,
            )
            if (
                (visible.st_dev, visible.st_ino) != (root.st_dev, root.st_ino)
                or _descriptor_mount_id(parent_fd) != root_mount
            ):
                raise WorkspaceError(
                    f"temporary server state root changed or crossed a mount: {path}"
                )
        finally:
            os.close(parent_fd)
        marker = Workspace._load_state_json_at(
            directory_fd,
            STATE_IMPLEMENTATION_MARKER,
            "temporary state implementation marker",
        )
        if implementation is None:
            marker_valid = bool(
                isinstance(marker, dict)
                and set(marker)
                == {"schema_version", "stack", "provider", "repository"}
                and marker.get("schema_version")
                == STATE_IMPLEMENTATION_SCHEMA_VERSION
                and all(
                    isinstance(marker.get(key), str) and marker[key]
                    for key in ("stack", "provider", "repository")
                )
            )
        else:
            marker_valid = marker == {
                "schema_version": STATE_IMPLEMENTATION_SCHEMA_VERSION,
                **implementation,
            }
        if not marker_valid:
            raise WorkspaceError(
                f"temporary state implementation marker is invalid: {path}"
            )
        for name in EXPECTED_SERVER_DATA["files"]:
            try:
                metadata = os.stat(
                    name, dir_fd=directory_fd, follow_symlinks=False
                )
            except OSError as error:
                raise WorkspaceError(
                    f"temporary server state lacks required file {name}: {path}"
                ) from error
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1:
                raise WorkspaceError(
                    f"temporary server state required file is linked or invalid "
                    f"{name}: {path}"
                )
        for name in EXPECTED_SERVER_DATA["directories"]:
            try:
                metadata = os.stat(
                    name, dir_fd=directory_fd, follow_symlinks=False
                )
            except OSError as error:
                raise WorkspaceError(
                    f"temporary server state lacks required directory {name}: {path}"
                ) from error
            if not stat.S_ISDIR(metadata.st_mode):
                raise WorkspaceError(
                    "temporary server state required directory is linked or "
                    f"invalid {name}: {path}"
                )

        def validate_directory(descriptor: int, display: Path) -> None:
            for name in os.listdir(descriptor):
                child_display = display / name
                metadata = os.stat(
                    name, dir_fd=descriptor, follow_symlinks=False
                )
                if stat.S_ISLNK(metadata.st_mode):
                    raise WorkspaceError(
                        f"temporary server state contains a symbolic link: "
                        f"{child_display}"
                    )
                if stat.S_ISREG(metadata.st_mode):
                    if metadata.st_nlink != 1:
                        raise WorkspaceError(
                            f"temporary server state contains a hard-linked file: "
                            f"{child_display}"
                        )
                    flags = os.O_NOFOLLOW
                    if sys.platform == "linux":
                        flags |= os.O_PATH
                    else:
                        flags |= os.O_RDONLY | os.O_NONBLOCK
                    child_fd = os.open(name, flags, dir_fd=descriptor)
                    try:
                        opened = os.fstat(child_fd)
                        if (
                            opened.st_nlink != 1
                            or (opened.st_dev, opened.st_ino)
                            != (metadata.st_dev, metadata.st_ino)
                            or _descriptor_mount_id(child_fd) != root_mount
                        ):
                            raise WorkspaceError(
                                "temporary server state file changed or crossed "
                                f"a mount during validation: {child_display}"
                            )
                    finally:
                        os.close(child_fd)
                    continue
                if not stat.S_ISDIR(metadata.st_mode):
                    raise WorkspaceError(
                        f"temporary server state contains a special entry: "
                        f"{child_display}"
                    )
                if metadata.st_dev != root.st_dev:
                    raise WorkspaceError(
                        f"temporary server state contains a mounted directory: "
                        f"{child_display}"
                    )
                child_fd = os.open(
                    name,
                    os.O_RDONLY
                    | os.O_CLOEXEC
                    | os.O_DIRECTORY
                    | os.O_NOFOLLOW,
                    dir_fd=descriptor,
                )
                try:
                    opened = os.fstat(child_fd)
                    if (opened.st_dev, opened.st_ino) != (
                        metadata.st_dev,
                        metadata.st_ino,
                    ) or _descriptor_mount_id(child_fd) != root_mount:
                        raise WorkspaceError(
                            "temporary server state entry changed or crossed a "
                            "mount during "
                            f"validation: {child_display}"
                        )
                    validate_directory(child_fd, child_display)
                finally:
                    os.close(child_fd)

        validate_directory(directory_fd, path)

    def _topology_services(self, services: list[str] | None) -> list[str]:
        requested = set(services or TOPOLOGY_SERVICES)
        unknown = sorted(requested - set(TOPOLOGY_SERVICES))
        if unknown:
            raise WorkspaceError(f"unknown topology services: {', '.join(unknown)}")
        if not requested:
            raise WorkspaceError("a topology must contain at least one service")
        return [service for service in TOPOLOGY_SERVICES if service in requested]

    @staticmethod
    def _portable_windows_package_members(
        archive: zipfile.ZipFile, executable: str
    ) -> tuple[PurePosixPath, list[tuple[zipfile.ZipInfo, PurePosixPath]]]:
        infos = archive.infolist()
        if not infos or len(infos) > WINDOWS_PACKAGE_MAX_ENTRIES:
            raise WorkspaceError("portable Windows package entry count is invalid")
        if sum(info.file_size for info in infos) > WINDOWS_PACKAGE_MAX_BYTES:
            raise WorkspaceError("portable Windows package is too large")
        members: list[tuple[zipfile.ZipInfo, PurePosixPath]] = []
        names: set[str] = set()
        executables: list[PurePosixPath] = []
        for info in infos:
            if info.flag_bits & 0x1:
                raise WorkspaceError("portable Windows package is encrypted")
            if "\\" in info.filename:
                raise WorkspaceError(
                    f"portable Windows package path is invalid: {info.filename}"
                )
            path = PurePosixPath(info.filename.rstrip("/"))
            if (
                not path.parts
                or path.is_absolute()
                or any(part in {"", ".", ".."} for part in path.parts)
                or any(":" in part for part in path.parts)
            ):
                raise WorkspaceError(
                    f"portable Windows package path is unsafe: {info.filename}"
                )
            folded = path.as_posix().casefold()
            if folded in names:
                raise WorkspaceError(
                    f"portable Windows package path is duplicated: {info.filename}"
                )
            names.add(folded)
            unix_mode = info.external_attr >> 16
            file_type = stat.S_IFMT(unix_mode)
            if file_type not in {0, stat.S_IFREG, stat.S_IFDIR}:
                raise WorkspaceError(
                    f"portable Windows package contains a special entry: {info.filename}"
                )
            if info.is_dir() and file_type == stat.S_IFREG:
                raise WorkspaceError(
                    f"portable Windows package directory is invalid: {info.filename}"
                )
            if not info.is_dir() and path.name.casefold() == executable.casefold():
                executables.append(path)
            members.append((info, path))
        if len(executables) != 1:
            raise WorkspaceError(
                f"portable Windows package must contain exactly one {executable}"
            )
        root = executables[0].parent
        return root, members

    @classmethod
    def _extract_portable_windows_package(
        cls, archive_path: Path, destination: Path, executable: str
    ) -> None:
        try:
            with zipfile.ZipFile(archive_path) as archive:
                root, members = cls._portable_windows_package_members(
                    archive, executable
                )
                destination.mkdir()
                for info, path in members:
                    if path == root or root not in path.parents:
                        continue
                    relative = path.relative_to(root)
                    output = destination.joinpath(*relative.parts)
                    if info.is_dir():
                        output.mkdir(parents=True, exist_ok=True)
                        continue
                    output.parent.mkdir(parents=True, exist_ok=True)
                    descriptor = os.open(
                        output,
                        os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                        0o600,
                    )
                    try:
                        with archive.open(info) as source:
                            with os.fdopen(descriptor, "wb", closefd=False) as target:
                                shutil.copyfileobj(source, target)
                        mode = info.external_attr >> 16
                        permissions = stat.S_IMODE(mode) if mode else 0o644
                        os.fchmod(descriptor, permissions or 0o644)
                    finally:
                        os.close(descriptor)
        except (OSError, zipfile.BadZipFile) as error:
            raise WorkspaceError(
                f"cannot extract portable Windows package {archive_path}: {error}"
            ) from error
        executable_path = destination / executable
        if not executable_path.is_file() or executable_path.is_symlink():
            raise WorkspaceError(
                f"portable Windows package executable is missing: {executable_path}"
            )

    @staticmethod
    def _windows_package_output(
        builds: Path, profile_name: str, state_name: str, output: Path | None
    ) -> Path:
        selected = output or (
            builds
            / "packages"
            / f"atrinik-{profile_name}-{state_name}-windows.zip"
        )
        selected = Path(os.path.abspath(selected.expanduser()))
        if selected.suffix.lower() != ".zip":
            raise WorkspaceError("Windows profile package output must end in .zip")
        selected.parent.mkdir(parents=True, exist_ok=True)
        try:
            parent = selected.parent.resolve(strict=True)
        except OSError as error:
            raise WorkspaceError(
                f"cannot resolve Windows package output directory: {error}"
            ) from error
        selected = parent / selected.name
        if selected.exists() or selected.is_symlink():
            raise WorkspaceError(f"Windows profile package already exists: {selected}")
        return selected

    def _stage_windows_profile_sources(
        self,
        staging: Path,
        build_root: Path,
        selected: dict[str, Path],
    ) -> None:
        source_exclusions = frozenset({".git", "build", MANAGED_MARKER})
        classic_root = selected["client"].parent
        if any(
            selected[role].parent != classic_root
            for role in ("server", "protocol", "libatrinik")
        ):
            raise WorkspaceError(
                "Windows package Classic sources do not share one monorepo root"
            )
        self._copy_runtime_tree(
            classic_root / "cmake", staging / "cmake", source_exclusions
        )
        for document in ("LICENSE.md", "ATTRIBUTIONS.md"):
            self._copy_runtime_regular_file(
                classic_root / document, staging / document
            )
        for role in ("client", "server", "protocol", "libatrinik"):
            exclusions = source_exclusions
            if role == "client":
                exclusions = frozenset({*exclusions, "sound"})
            elif role == "server":
                exclusions = frozenset({*exclusions, "resources", "runtime"})
            self._copy_runtime_tree(
                selected[role], staging / role, exclusions
            )

        # Materialized clean generations are sealed read-only. The package
        # staging copies are private, so make them writable before adding the
        # selected sound and collected server runtime overlays.
        self._make_tree_owner_writable(staging)
        metadata = load_json(build_root / BUILD_METADATA)
        try:
            sound = validate_sound_record(metadata.get("sound"))
        except (AttributeError, WorkspaceError) as error:
            raise WorkspaceError("profile build sound metadata is invalid") from error
        self._copy_runtime_tree(
            Path(sound["root"]), staging / "client" / "sound", source_exclusions
        )
        server = staging / "server"
        (server / "runtime").mkdir()
        self._copy_runtime_tree(
            build_root / "runtime" / "content",
            server / "runtime" / "content",
        )
        self._copy_runtime_tree(
            build_root / "runtime" / "resources", server / "resources"
        )
        for private_input in (
            staging / "client" / "data" / "discord-application-id",
            server / "server-custom.cfg",
        ):
            if private_input.is_symlink() or (
                private_input.exists() and not private_input.is_file()
            ):
                raise WorkspaceError(
                    f"private Windows package input is invalid: {private_input}"
                )
            private_input.unlink(missing_ok=True)
        self._make_tree_owner_writable(staging)
        run(["git", "init", "--quiet"], cwd=staging)

    def _snapshot_windows_profile_state(
        self, state_fd: int, state: Path, destination: Path
    ) -> str:
        """Copy state through the descriptor that already holds its flock."""

        self._copy_runtime_tree_from_descriptor(
            state_fd, state, destination
        )
        return self._server_identity_fingerprint(destination)

    def _windows_build_image(self) -> str:
        config_path = self.paths.repository / ".devcontainer/windows-cross/devcontainer.json"
        config = load_json(config_path)
        image = config.get("image") if isinstance(config, dict) else None
        if (
            not isinstance(image, str)
            or re.fullmatch(r"[^\s@]+@sha256:[0-9a-f]{64}", image) is None
        ):
            raise WorkspaceError(
                f"pinned Windows build image is invalid: {config_path}"
            )
        return image

    @staticmethod
    def _local_windows_build_environment() -> dict[str, str] | None:
        environment = os.environ.copy()
        required = (
            "MXE_TOOLCHAIN_FILE",
            "MXE_RUNTIME_DIR",
            "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR",
            "ATRINIK_WINDOWS_PYTHON_LIBRARY",
            "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR",
        )
        target = environment.get("MXE_TARGET", "x86_64-w64-mingw32.shared")
        if shutil.which(f"{target}-cmake") is None or any(
            not environment.get(name) or not Path(environment[name]).exists()
            for name in required
        ):
            return None
        environment["ATRINIK_PACKAGE_VERSION"] = WINDOWS_PACKAGE_VERSION
        environment["ATRINIK_DISCORD_APPLICATION_ID_FILE"] = ""
        return environment

    def _build_windows_profile_archives(
        self, staging: Path
    ) -> tuple[Path, Path, dict[str, str]]:
        packages = staging / "packages"
        packages.mkdir()
        local_environment = self._local_windows_build_environment()
        if local_environment is not None:
            local_environment["ATRINIK_PROFILE_SOUND_DIR"] = str(
                staging / "client" / "sound"
            )
            local_environment["ATRINIK_PROFILE_CONTENT_DIR"] = str(
                staging / "server" / "runtime" / "content"
            )
            local_environment["ATRINIK_PROFILE_RESOURCES_DIR"] = str(
                staging / "server" / "resources"
            )
            for component in ("client", "server"):
                run(
                    [
                        "bash",
                        "tools/build-windows-package.sh",
                        str(packages),
                    ],
                    cwd=staging / component,
                    env=local_environment,
                )
            build = {"mode": "local", "image": ""}
        else:
            if shutil.which("docker") is None:
                raise WorkspaceError(
                    "Windows packaging requires either the windows-cross "
                    "devcontainer toolchain or Docker"
                )
            image = self._windows_build_image()
            script = staging / ".atrinik-build-windows.sh"
            script.write_text(
                "#!/usr/bin/env bash\n"
                "set -euo pipefail\n"
                "cd /workspace/client\n"
                "bash tools/build-windows-package.sh /workspace/packages\n"
                "cd /workspace/server\n"
                "bash tools/build-windows-package.sh /workspace/packages\n",
                encoding="utf-8",
            )
            script.chmod(0o700)
            run(
                [
                    "docker",
                    "run",
                    "--rm",
                    "--user",
                    f"{os.getuid()}:{os.getgid()}",
                    "--env",
                    f"ATRINIK_PACKAGE_VERSION={WINDOWS_PACKAGE_VERSION}",
                    "--env",
                    "ATRINIK_DISCORD_APPLICATION_ID_FILE=",
                    "--env",
                    "ATRINIK_PROFILE_SOUND_DIR=/workspace/client/sound",
                    "--env",
                    "ATRINIK_PROFILE_CONTENT_DIR=/workspace/server/runtime/content",
                    "--env",
                    "ATRINIK_PROFILE_RESOURCES_DIR=/workspace/server/resources",
                    "--volume",
                    f"{staging}:/workspace",
                    "--workdir",
                    "/workspace",
                    image,
                    "bash",
                    "/workspace/.atrinik-build-windows.sh",
                ]
            )
            build = {"mode": "container", "image": image}

        def one(pattern: str, label: str) -> Path:
            matches = sorted(packages.glob(pattern))
            if len(matches) != 1 or matches[0].is_symlink():
                raise WorkspaceError(
                    f"Windows {label} build produced {len(matches)} packages"
                )
            return matches[0]

        client = one(
            f"atrinik-classic-client-{WINDOWS_PACKAGE_VERSION}-windows-x86_64.zip",
            "client",
        )
        server = one(
            f"atrinik-classic-server-{WINDOWS_PACKAGE_VERSION}-windows-x86_64.zip",
            "server",
        )
        return client, server, build

    @staticmethod
    def _write_windows_launch_files(
        root: Path, profile_name: str, state_name: str, port: int, fingerprint: str
    ) -> None:
        launch = (
            "@echo off\r\n"
            "setlocal\r\n"
            "cd /d \"%~dp0\"\r\n"
            "start \"Atrinik Server\" /D \"%~dp0server\" cmd /k call "
            f"server.bat --port_quic={port} --port_mapping=off --stun_server=off\r\n"
            "timeout /t 2 /nobreak >nul\r\n"
            "start \"Atrinik Client\" /D \"%~dp0client\" atrinik.exe "
            f"--server=\"127.0.0.1 {port} {fingerprint}\" "
            "--stun_server=off --nometa\r\n"
        )
        (root / "run.bat").write_bytes(launch.encode("ascii"))
        readme = (
            "Atrinik portable Windows review topology\r\n"
            "==========================================\r\n\r\n"
            f"Profile: {profile_name}\r\n"
            f"Server state: {state_name}\r\n"
            f"Local UDP port: {port}\r\n\r\n"
            "Double-click run.bat. It starts the packaged server, waits briefly, "
            "and starts the client pinned to that server identity. Close the "
            "server console when finished.\r\n\r\n"
            "SENSITIVE: server/data contains private player data, credentials, "
            "and the server private identity. Do not publish or attach this ZIP "
            "to a public issue or pull request.\r\n"
        )
        (root / "README.txt").write_bytes(readme.encode("utf-8"))

    @staticmethod
    def _archive_windows_profile(root: Path, output: Path) -> None:
        entries: list[tuple[Path, str, os.stat_result]] = []
        for directory, names, files in os.walk(root, followlinks=False):
            names.sort()
            files.sort()
            current = Path(directory)
            for name in (*names, *files):
                path = current / name
                metadata = path.lstat()
                if stat.S_ISLNK(metadata.st_mode) or not (
                    stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode)
                ):
                    raise WorkspaceError(
                        f"Windows profile package contains a special entry: {path}"
                    )
                if stat.S_ISREG(metadata.st_mode) and metadata.st_nlink != 1:
                    raise WorkspaceError(
                        f"Windows profile package contains a hard-linked file: {path}"
                    )
                entries.append(
                    (path, path.relative_to(root.parent).as_posix(), metadata)
                )
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{output.name}.", suffix=".tmp", dir=output.parent
        )
        os.close(descriptor)
        temporary = Path(temporary_name)
        try:
            with zipfile.ZipFile(
                temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6
            ) as archive:
                for path, relative, metadata in entries:
                    name = relative + ("/" if stat.S_ISDIR(metadata.st_mode) else "")
                    info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.create_system = 3
                    info.external_attr = metadata.st_mode << 16
                    if stat.S_ISDIR(metadata.st_mode):
                        archive.writestr(info, b"")
                        continue
                    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
                    try:
                        opened = os.fstat(descriptor)
                        if (
                            (opened.st_dev, opened.st_ino)
                            != (metadata.st_dev, metadata.st_ino)
                            or opened.st_nlink != 1
                        ):
                            raise WorkspaceError(
                                f"Windows profile package input changed: {path}"
                            )
                        with os.fdopen(descriptor, "rb", closefd=False) as source:
                            with archive.open(info, "w") as target:
                                shutil.copyfileobj(source, target)
                        retained = os.fstat(descriptor)
                        if Workspace._runtime_tree_identity(retained) != (
                            Workspace._runtime_tree_identity(opened)
                        ):
                            raise WorkspaceError(
                                f"Windows profile package input changed: {path}"
                            )
                    finally:
                        os.close(descriptor)
            temporary.chmod(0o600)
            with temporary.open("rb") as stream:
                os.fsync(stream.fileno())
            rename_no_replace(temporary, output)
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise

    def package_windows_profile(
        self,
        profile_name: str,
        state_name: str,
        output: Path | None = None,
        *,
        port: int = 1730,
    ) -> dict[str, Any]:
        self.paths.ensure()
        self._validate_run_port(port)
        validate_name(profile_name, "profile name")
        validate_name(state_name, "state name")
        self._require_classic_contracts(profile_name, {"client", "server"})
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        if stack.name != "classic":
            raise WorkspaceError(
                "portable Windows profile packages require the classic stack"
            )
        destination = self._windows_package_output(
            self.paths.builds, profile_name, state_name, output
        )
        required = self._dependency_roles(profile, {"client", "server"})
        targets = [role for role in ALL_BUILD_TARGETS if role in required]
        with tempfile.TemporaryDirectory(
            prefix=f"atrinik-{profile_name}-windows-"
        ) as temporary_name:
            staging = Path(temporary_name)
            with self._resolved_profile_operation(
                profile_name,
                {"client", "server"},
                f"package Windows profile {profile_name}",
                materialize_clean_primaries=True,
            ) as snapshot:
                selected = snapshot.paths()
                build_root = self._build_resolved(
                    "windows-package", profile_name, False, targets, selected
                )
                resolved = self._topology_resolved_status(profile_name, selected)
                providers = {
                    role: stack.providers[role].name for role in sorted(required)
                }
                implementation = self._state_implementation(
                    stack.name, providers, resolved
                )
                state_location = self._state_location(state_name)
                with self._topology_state_lock(state_location) as state_lock:
                    prepared = self.state_path(
                        state_name,
                        selected["server"],
                        resolved_path=state_location,
                        implementation=implementation,
                        write_implementation=True,
                        keep_descriptor=True,
                    )
                    assert isinstance(prepared, tuple)
                    state, state_fd = prepared
                    state_identity = {
                        "device": os.fstat(state_fd).st_dev,
                        "inode": os.fstat(state_fd).st_ino,
                    }
                    state_lock.bind(state_identity)
                    try:
                        fingerprint = self._snapshot_windows_profile_state(
                            state_fd, state, staging / "state"
                        )
                    finally:
                        os.close(state_fd)

                with self._profile_build_lock(build_root, profile_name):
                    (staging / "sources").mkdir()
                    self._stage_windows_profile_sources(
                        staging / "sources", build_root, selected
                    )
                    staged_server = staging / "sources" / "server"
                    staged_runtime_paths = {
                        "content_maps": staged_server
                        / "runtime"
                        / "content"
                        / "maps",
                        "content_lib": staged_server
                        / "runtime"
                        / "content"
                        / "lib",
                        "resources": staged_server / "resources",
                    }
                    expected_runtime_inventories = {
                        name: _tree_content_inventory(
                            path, f"staged profile runtime input {name}"
                        )
                        for name, path in staged_runtime_paths.items()
                    }
                    expected_runtime_digests = {
                        name: _tree_content_inventory_digest(inventory)
                        for name, inventory in expected_runtime_inventories.items()
                    }
                    build_metadata = load_json(build_root / BUILD_METADATA)

            client_archive, server_archive, build = (
                self._build_windows_profile_archives(staging / "sources")
            )
            package_root = staging / f"atrinik-{profile_name}-windows-review"
            package_root.mkdir()
            self._extract_portable_windows_package(
                client_archive, package_root / "client", "atrinik.exe"
            )
            self._extract_portable_windows_package(
                server_archive, package_root / "server", "atrinik-server.exe"
            )
            packaged_runtime_paths = {
                "content_maps": package_root / "server" / "maps",
                "content_lib": package_root / "server" / "lib",
                "resources": package_root / "server" / "resources",
            }
            for name, path in packaged_runtime_paths.items():
                actual_inventory = _tree_content_inventory(
                    path, f"packaged profile runtime input {name}"
                )
                expected_inventory = expected_runtime_inventories[name]
                if actual_inventory != expected_inventory:
                    expected_paths = set(expected_inventory)
                    actual_paths = set(actual_inventory)
                    missing = sorted(expected_paths - actual_paths)
                    extra = sorted(actual_paths - expected_paths)
                    changed = sorted(
                        candidate
                        for candidate in expected_paths & actual_paths
                        if expected_inventory[candidate]
                        != actual_inventory[candidate]
                    )
                    raise WorkspaceError(
                        "Windows server package changed the selected profile "
                        f"runtime input {name}: missing={missing[:5]}, "
                        f"extra={extra[:5]}, changed={changed[:5]}"
                    )
            (package_root / "server" / "data").parent.mkdir(exist_ok=True)
            staging_state = staging / "state"
            staging_state.replace(package_root / "server" / "data")
            self._write_windows_launch_files(
                package_root, profile_name, state_name, port, fingerprint
            )
            coordinates = build_metadata.get("coordinates", {})
            public_coordinates = {
                role: {
                    key: value
                    for key, value in coordinate.items()
                    if key in {"component", "checkout", "repository", "branch", "source", "head"}
                }
                for role, coordinate in sorted(coordinates.items())
                if isinstance(coordinate, dict)
            }
            atomic_json(
                package_root / "manifest.json",
                {
                    "schema_version": WINDOWS_PACKAGE_SCHEMA_VERSION,
                    "profile": profile_name,
                    "state": state_name,
                    "stack": stack.name,
                    "port": port,
                    "server_fingerprint": fingerprint,
                    "contains_private_server_state": True,
                    "build": build,
                    "runtime_input_sha256": expected_runtime_digests,
                    "components": public_coordinates,
                },
            )
            self._archive_windows_profile(package_root, destination)

        digest = _file_digest(destination, "Windows profile package")
        return {
            "schema_version": WINDOWS_PACKAGE_SCHEMA_VERSION,
            "profile": profile_name,
            "state": state_name,
            "path": str(destination),
            "sha256": digest,
            "bytes": destination.stat().st_size,
            "contains_private_server_state": True,
            "build": build,
        }

    def topology_summary(
        self,
        profile_name: str,
        state_name: str | None,
        services: list[str] | None = None,
        state_mode: str | None = None,
    ) -> dict[str, Any]:
        selected_services = self._topology_services(services)
        state_mode, state_name = self._normalize_topology_state_request(
            state_mode, state_name, selected_services
        )
        requested = set(selected_services)
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        resolved = self._resolve_build_profile(profile_name, requested)
        required = set(resolved)
        key = self._profile_build_key(profile_name, resolved)
        checkout_states = self._selected_checkout_states(
            profile, resolved, include_dirty=True
        )
        state: str | None = None
        state_policy: dict[str, Any] | None = None
        if "server" in selected_services:
            if state_mode == "temporary":
                state_policy = {
                    "mode": "temporary",
                    "name": None,
                    "path": None,
                    "owner": {"kind": "topology-generation"},
                    "lifecycle": "disposable",
                }
            else:
                assert state_name is not None
                state = str(self._state_location(state_name))
                owner, lifecycle = self._persistent_state_ownership(
                    state_name, Path(state)
                )
                state_policy = {
                    "mode": state_mode,
                    "name": state_name,
                    "path": state,
                    "owner": owner,
                    "lifecycle": lifecycle,
                }
        return {
            "profile": profile_name,
            "stack": stack.name,
            "sound": {
                "mode": profile["sound_mode"],
                "release": profile["sound_release"],
                "source_path": str(resolved["sound"])
                if "sound" in resolved
                else None,
            },
            "services": selected_services,
            "dependencies": sorted(required),
            "providers": {
                role: stack.providers[role].name for role in sorted(required)
            },
            "state": state,
            "state_policy": state_policy,
            "build_root": str(
                self.paths.builds / "profiles" / f"{profile_name}-{key}"
            ),
            "components": {
                stack.providers[role].name: {
                    "checkout": stack.providers[role].checkout_name,
                    "repository": stack.providers[role].repository,
                    "branch": stack.providers[role].branch,
                    "source": stack.providers[role].source,
                    "roles": sorted(stack.providers[role].provides),
                    "path": str(path),
                    "checkout_path": str(
                        checkout_states[stack.providers[role].checkout_name]["path"]
                    ),
                    "head": checkout_states[stack.providers[role].checkout_name]["head"],
                    "dirty": checkout_states[stack.providers[role].checkout_name]["dirty"],
                }
                for role, path in sorted(resolved.items())
            },
        }

    @staticmethod
    def _normalize_topology_state_request(
        state_mode: str | None,
        state_name: str | None,
        services: list[str],
    ) -> tuple[str, str | None]:
        if "server" not in services:
            if state_mode == "temporary":
                raise WorkspaceError("temporary state requires the server service")
            return "default", None
        if state_mode is None:
            state_mode = (
                "temporary"
                if state_name is None
                else "default"
                if state_name == "default"
                else "named"
            )
        if state_mode not in {"temporary", "named", "default"}:
            raise WorkspaceError(f"unknown topology state policy: {state_mode}")
        if state_mode == "temporary":
            if state_name is not None:
                raise WorkspaceError("temporary state cannot also name persistent state")
            return state_mode, None
        if state_mode == "default":
            if state_name not in {None, "default"}:
                raise WorkspaceError("default state cannot also name persistent state")
            return state_mode, "default"
        if state_name is None or state_name == "default":
            raise WorkspaceError("named state policy requires a non-default state name")
        validate_name(state_name, "state name")
        return state_mode, state_name

    def _topology_directory(self, name: str, create: bool = False) -> Path:
        validate_name(name, "topology name")
        self.paths.ensure()
        path = self.paths.topologies / name
        marker = path / MANAGED_MARKER
        metadata = {"schema_version": SCHEMA_VERSION, "purpose": f"topology:{name}"}
        if path.exists() or path.is_symlink():
            if not path.is_dir() or path.is_symlink():
                raise WorkspaceError(f"topology path is not a managed directory: {path}")
            if not marker.is_file() or marker.is_symlink() or load_json(marker) != metadata:
                raise WorkspaceError(f"topology ownership marker is invalid: {path}")
        elif create:
            path.mkdir()
            atomic_json(marker, metadata)
        else:
            raise WorkspaceError(f"topology does not exist: {name}")
        return path

    def _reset_topology_subdirectory(
        self, topology_root: Path, name: str, purpose: str
    ) -> Path:
        path = topology_root / name
        metadata = {"schema_version": SCHEMA_VERSION, "purpose": purpose}
        if path.exists() or path.is_symlink():
            marker = path / MANAGED_MARKER
            if (
                not path.is_dir()
                or path.is_symlink()
                or not marker.is_file()
                or marker.is_symlink()
                or load_json(marker) != metadata
            ):
                raise WorkspaceError(
                    f"topology runtime path is not managed for {purpose}: {path}"
                )
            shutil.rmtree(path)
        path.mkdir()
        atomic_json(path / MANAGED_MARKER, metadata)
        return path

    @staticmethod
    def _runtime_tree_identity(value: os.stat_result) -> tuple[int, ...]:
        return (
            value.st_dev,
            value.st_ino,
            value.st_mode,
            value.st_size,
            value.st_mtime_ns,
            value.st_ctime_ns,
        )

    def _compare_topology_runtime_directories(
        self,
        source_fd: int,
        destination_fd: int,
        source_display: Path,
        destination_display: Path,
    ) -> None:
        source_before = os.fstat(source_fd)
        destination_before = os.fstat(destination_fd)
        if (
            not stat.S_ISDIR(source_before.st_mode)
            or not stat.S_ISDIR(destination_before.st_mode)
            or stat.S_IMODE(source_before.st_mode)
            != stat.S_IMODE(destination_before.st_mode)
        ):
            raise WorkspaceError(
                "copied topology runtime directory metadata differs: "
                f"{destination_display}"
            )
        try:
            source_names = sorted(os.listdir(source_fd))
            destination_names = sorted(os.listdir(destination_fd))
        except OSError as error:
            raise WorkspaceError(
                f"cannot validate copied topology runtime input: {error}"
            ) from error
        if source_names != destination_names:
            raise WorkspaceError(
                f"copied topology runtime tree differs: {destination_display}"
            )
        for name in source_names:
            source_child = source_display / name
            destination_child = destination_display / name
            try:
                source_entry = os.stat(
                    name, dir_fd=source_fd, follow_symlinks=False
                )
                destination_entry = os.stat(
                    name, dir_fd=destination_fd, follow_symlinks=False
                )
            except OSError as error:
                raise WorkspaceError(
                    f"cannot validate copied topology runtime input: {error}"
                ) from error
            if (
                stat.S_IFMT(source_entry.st_mode)
                != stat.S_IFMT(destination_entry.st_mode)
                or stat.S_IMODE(source_entry.st_mode)
                != stat.S_IMODE(destination_entry.st_mode)
            ):
                raise WorkspaceError(
                    f"copied topology runtime entry differs: {destination_child}"
                )
            if stat.S_ISDIR(source_entry.st_mode):
                flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
                try:
                    source_child_fd = os.open(name, flags, dir_fd=source_fd)
                except OSError as error:
                    raise WorkspaceError(
                        "copied topology runtime source changed or contains a link: "
                        f"{source_child}"
                    ) from error
                try:
                    destination_child_fd = os.open(
                        name, flags, dir_fd=destination_fd
                    )
                except OSError as error:
                    os.close(source_child_fd)
                    raise WorkspaceError(
                        "copied topology runtime input changed or contains a link: "
                        f"{destination_child}"
                    ) from error
                try:
                    self._compare_topology_runtime_directories(
                        source_child_fd,
                        destination_child_fd,
                        source_child,
                        destination_child,
                    )
                finally:
                    os.close(source_child_fd)
                    os.close(destination_child_fd)
            elif stat.S_ISREG(source_entry.st_mode):
                flags = os.O_RDONLY | os.O_NOFOLLOW
                try:
                    source_child_fd = os.open(name, flags, dir_fd=source_fd)
                except OSError as error:
                    raise WorkspaceError(
                        "copied topology runtime source changed or contains a link: "
                        f"{source_child}"
                    ) from error
                try:
                    destination_child_fd = os.open(
                        name, flags, dir_fd=destination_fd
                    )
                except OSError as error:
                    os.close(source_child_fd)
                    raise WorkspaceError(
                        "copied topology runtime input changed or contains a link: "
                        f"{destination_child}"
                    ) from error
                try:
                    source_file_before = os.fstat(source_child_fd)
                    destination_file_before = os.fstat(destination_child_fd)
                    if source_file_before.st_size != destination_file_before.st_size:
                        raise WorkspaceError(
                            f"copied topology runtime file differs: {destination_child}"
                        )
                    with os.fdopen(
                        source_child_fd, "rb", closefd=False
                    ) as source_file:
                        with os.fdopen(
                            destination_child_fd, "rb", closefd=False
                        ) as destination_file:
                            while True:
                                source_chunk = source_file.read(1024 * 1024)
                                destination_chunk = destination_file.read(1024 * 1024)
                                if source_chunk != destination_chunk:
                                    raise WorkspaceError(
                                        "copied topology runtime file differs: "
                                        f"{destination_child}"
                                    )
                                if not source_chunk:
                                    break
                    if (
                        self._runtime_tree_identity(os.fstat(source_child_fd))
                        != self._runtime_tree_identity(source_file_before)
                        or self._runtime_tree_identity(os.fstat(destination_child_fd))
                        != self._runtime_tree_identity(destination_file_before)
                    ):
                        raise WorkspaceError(
                            "topology runtime input changed during validation: "
                            f"{destination_child}"
                        )
                finally:
                    os.close(source_child_fd)
                    os.close(destination_child_fd)
            else:
                raise WorkspaceError(
                    "copied topology runtime input contains a link or non-regular "
                    f"file: {destination_child}"
                )
        if (
            self._runtime_tree_identity(os.fstat(source_fd))
            != self._runtime_tree_identity(source_before)
            or self._runtime_tree_identity(os.fstat(destination_fd))
            != self._runtime_tree_identity(destination_before)
        ):
            raise WorkspaceError(
                "topology runtime input changed during validation: "
                f"{destination_display}"
            )

    def _copy_topology_runtime_directory(
        self,
        source_fd: int,
        source_display: Path,
        destination_fd: int,
        destination_display: Path,
        exclusions: frozenset[str] = frozenset(),
    ) -> None:
        directory_before = os.fstat(source_fd)
        if not stat.S_ISDIR(directory_before.st_mode):
            raise WorkspaceError(
                f"topology runtime input is not a directory: {source_display}"
            )
        try:
            names = sorted(os.listdir(source_fd))
        except OSError as error:
            raise WorkspaceError(
                f"cannot list topology runtime input {source_display}: {error}"
            ) from error
        for name in names:
            if name in exclusions:
                continue
            source_child = source_display / name
            destination_child = destination_display / name
            try:
                child_before = os.stat(name, dir_fd=source_fd, follow_symlinks=False)
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect topology runtime input {source_child}: {error}"
                ) from error
            if stat.S_ISDIR(child_before.st_mode):
                source_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
                try:
                    child_fd = os.open(name, source_flags, dir_fd=source_fd)
                except OSError as error:
                    raise WorkspaceError(
                        "topology runtime input changed or contains a link: "
                        f"{source_child}"
                    ) from error
                try:
                    if self._runtime_tree_identity(os.fstat(child_fd)) != (
                        self._runtime_tree_identity(child_before)
                    ):
                        raise WorkspaceError(
                            f"topology runtime input changed during copy: {source_child}"
                        )
                    try:
                        os.mkdir(
                            name,
                            stat.S_IMODE(child_before.st_mode) | 0o700,
                            dir_fd=destination_fd,
                        )
                        destination_child_fd = os.open(
                            name,
                            os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                            dir_fd=destination_fd,
                        )
                    except OSError as error:
                        raise WorkspaceError(
                            "topology runtime staging destination changed during "
                            f"copy: {destination_child}: {error}"
                        ) from error
                    try:
                        self._copy_topology_runtime_directory(
                            child_fd,
                            source_child,
                            destination_child_fd,
                            destination_child,
                            frozenset(),
                        )
                    finally:
                        os.close(destination_child_fd)
                finally:
                    os.close(child_fd)
            elif stat.S_ISREG(child_before.st_mode):
                flags = os.O_RDONLY | os.O_NOFOLLOW
                try:
                    child_fd = os.open(name, flags, dir_fd=source_fd)
                except OSError as error:
                    raise WorkspaceError(
                        "topology runtime input changed or contains a link: "
                        f"{source_child}"
                    ) from error
                try:
                    opened = os.fstat(child_fd)
                    if (
                        not stat.S_ISREG(opened.st_mode)
                        or self._runtime_tree_identity(opened)
                        != self._runtime_tree_identity(child_before)
                    ):
                        raise WorkspaceError(
                            f"topology runtime input changed during copy: {source_child}"
                        )
                    try:
                        destination_file_fd = os.open(
                            name,
                            os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                            stat.S_IMODE(opened.st_mode),
                            dir_fd=destination_fd,
                        )
                    except OSError as error:
                        raise WorkspaceError(
                            "topology runtime staging destination changed during "
                            f"copy: {destination_child}: {error}"
                        ) from error
                    try:
                        with os.fdopen(child_fd, "rb", closefd=False) as source_file:
                            with os.fdopen(
                                destination_file_fd, "wb", closefd=False
                            ) as destination_file:
                                shutil.copyfileobj(source_file, destination_file)
                        copied = os.fstat(child_fd)
                        if self._runtime_tree_identity(copied) != (
                            self._runtime_tree_identity(opened)
                        ):
                            raise WorkspaceError(
                                "topology runtime input changed during copy: "
                                f"{source_child}"
                            )
                        os.fchmod(
                            destination_file_fd, stat.S_IMODE(opened.st_mode)
                        )
                    finally:
                        os.close(destination_file_fd)
                finally:
                    os.close(child_fd)
                os.utime(
                    name,
                    ns=(opened.st_atime_ns, opened.st_mtime_ns),
                    dir_fd=destination_fd,
                    follow_symlinks=False,
                )
            else:
                raise WorkspaceError(
                    "topology runtime input contains a link or non-regular file: "
                    f"{source_child}"
                )
        directory_after = os.fstat(source_fd)
        if self._runtime_tree_identity(directory_after) != self._runtime_tree_identity(
            directory_before
        ):
            raise WorkspaceError(
                f"topology runtime input changed during copy: {source_display}"
            )
        os.fchmod(destination_fd, stat.S_IMODE(directory_before.st_mode))
        os.utime(
            destination_fd,
            ns=(directory_before.st_atime_ns, directory_before.st_mtime_ns),
        )

    def _copy_topology_runtime_tree(
        self,
        source: Path,
        destination: Path,
        exclusions: frozenset[str] = frozenset(),
        pinned_destination_parent_fd: int | None = None,
    ) -> int:
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
        retained_destination_fd: int | None = None
        try:
            source_before = source.stat(follow_symlinks=False)
            source_fd = os.open(source, flags)
        except OSError as error:
            raise WorkspaceError(
                f"cannot open topology runtime input {source}: {error}"
            ) from error
        destination_parent_fd: int | None = None
        destination_fd: int | None = None
        try:
            if self._runtime_tree_identity(os.fstat(source_fd)) != (
                self._runtime_tree_identity(source_before)
            ):
                raise WorkspaceError(
                    f"topology runtime input changed during copy: {source}"
                )
            destination_parent_before = (
                os.fstat(pinned_destination_parent_fd)
                if pinned_destination_parent_fd is not None
                else destination.parent.stat(follow_symlinks=False)
            )
            destination_parent_fd = (
                os.dup(pinned_destination_parent_fd)
                if pinned_destination_parent_fd is not None
                else os.open(destination.parent, flags)
            )
            if self._runtime_tree_identity(os.fstat(destination_parent_fd)) != (
                self._runtime_tree_identity(destination_parent_before)
            ):
                raise WorkspaceError(
                    "topology runtime staging directory changed during copy: "
                    f"{destination.parent}"
                )
            try:
                os.mkdir(
                    destination.name,
                    stat.S_IMODE(source_before.st_mode) | 0o700,
                    dir_fd=destination_parent_fd,
                )
                destination_fd = os.open(
                    destination.name,
                    flags,
                    dir_fd=destination_parent_fd,
                )
            except OSError as error:
                raise WorkspaceError(
                    "topology runtime staging destination changed during copy: "
                    f"{destination}: {error}"
                ) from error
            self._copy_topology_runtime_directory(
                source_fd,
                source,
                destination_fd,
                destination,
                exclusions,
            )
            destination_parent_after = os.fstat(destination_parent_fd)
            if (
                destination_parent_after.st_dev != destination_parent_before.st_dev
                or destination_parent_after.st_ino != destination_parent_before.st_ino
            ):
                raise WorkspaceError(
                    "topology runtime staging directory changed during copy: "
                    f"{destination.parent}"
                )
            source_after = source.stat(follow_symlinks=False)
            if self._runtime_tree_identity(source_after) != (
                self._runtime_tree_identity(source_before)
            ):
                raise WorkspaceError(
                    f"topology runtime input changed during copy: {source}"
                )
            retained_destination_fd = os.dup(destination_fd)
            return retained_destination_fd
        finally:
            if destination_fd is not None:
                os.close(destination_fd)
            if destination_parent_fd is not None:
                os.close(destination_parent_fd)
            os.close(source_fd)

    def _copy_runtime_directory_contents(
        self,
        source: Path,
        destination: Path,
        exclusions: frozenset[str] = frozenset(),
    ) -> None:
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
        source_fd: int | None = None
        destination_fd: int | None = None
        try:
            source_before = source.stat(follow_symlinks=False)
            source_fd = os.open(source, flags)
            destination_before = destination.stat(follow_symlinks=False)
            destination_fd = os.open(destination, flags)
        except OSError as error:
            if destination_fd is not None:
                os.close(destination_fd)
            if source_fd is not None:
                os.close(source_fd)
            raise WorkspaceError(
                f"cannot open runtime publication directory: {error}"
            ) from error
        try:
            if (
                self._runtime_tree_identity(os.fstat(source_fd))
                != self._runtime_tree_identity(source_before)
                or self._runtime_tree_identity(os.fstat(destination_fd))
                != self._runtime_tree_identity(destination_before)
            ):
                raise WorkspaceError(
                    "runtime publication directory changed before copy"
                )
            self._copy_topology_runtime_directory(
                source_fd,
                source,
                destination_fd,
                destination,
                exclusions,
            )
        finally:
            os.close(destination_fd)
            os.close(source_fd)

    def _copy_runtime_tree(
        self,
        source: Path,
        destination: Path,
        exclusions: frozenset[str] = frozenset(),
        pinned_destination_parent_fd: int | None = None,
    ) -> None:
        descriptor = self._copy_topology_runtime_tree(
            source,
            destination,
            exclusions,
            pinned_destination_parent_fd,
        )
        os.close(descriptor)

    def _copy_runtime_tree_from_descriptor(
        self,
        source_fd: int,
        source_display: Path,
        destination: Path,
        exclusions: frozenset[str] = frozenset(),
    ) -> None:
        source_before = os.fstat(source_fd)
        if not stat.S_ISDIR(source_before.st_mode):
            raise WorkspaceError(
                f"runtime publication input is not a directory: {source_display}"
            )
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
        destination_parent_before = destination.parent.stat(
            follow_symlinks=False
        )
        destination_parent_fd: int | None = None
        destination_fd: int | None = None
        try:
            destination_parent_fd = os.open(destination.parent, flags)
            if self._runtime_tree_identity(os.fstat(destination_parent_fd)) != (
                self._runtime_tree_identity(destination_parent_before)
            ):
                raise WorkspaceError(
                    "runtime staging directory changed before descriptor copy: "
                    f"{destination.parent}"
                )
            try:
                os.mkdir(
                    destination.name,
                    stat.S_IMODE(source_before.st_mode) | 0o700,
                    dir_fd=destination_parent_fd,
                )
                destination_fd = os.open(
                    destination.name, flags, dir_fd=destination_parent_fd
                )
            except OSError as error:
                raise WorkspaceError(
                    "runtime staging destination changed during descriptor copy: "
                    f"{destination}: {error}"
                ) from error
            self._copy_topology_runtime_directory(
                source_fd,
                source_display,
                destination_fd,
                destination,
                exclusions,
            )
            if self._runtime_tree_identity(os.fstat(source_fd)) != (
                self._runtime_tree_identity(source_before)
            ):
                raise WorkspaceError(
                    f"runtime publication input changed during copy: {source_display}"
                )
            destination_parent_after = os.fstat(destination_parent_fd)
            if (
                destination_parent_after.st_dev
                != destination_parent_before.st_dev
                or destination_parent_after.st_ino
                != destination_parent_before.st_ino
            ):
                raise WorkspaceError(
                    "runtime staging directory changed during descriptor copy: "
                    f"{destination.parent}"
                )
        finally:
            if destination_fd is not None:
                os.close(destination_fd)
            if destination_parent_fd is not None:
                os.close(destination_parent_fd)

    def _copy_runtime_regular_file(self, source: Path, destination: Path) -> None:
        flags = os.O_RDONLY | os.O_NOFOLLOW
        source_fd: int | None = None
        destination_fd: int | None = None
        try:
            before = source.stat(follow_symlinks=False)
            source_fd = os.open(source, flags)
            opened = os.fstat(source_fd)
            if (
                not stat.S_ISREG(opened.st_mode)
                or self._runtime_tree_identity(opened)
                != self._runtime_tree_identity(before)
            ):
                raise WorkspaceError(
                    f"runtime publication input changed or is not regular: {source}"
                )
            destination_fd = os.open(
                destination,
                os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                stat.S_IMODE(opened.st_mode),
            )
            with os.fdopen(source_fd, "rb", closefd=False) as source_stream:
                with os.fdopen(
                    destination_fd, "wb", closefd=False
                ) as destination_stream:
                    shutil.copyfileobj(source_stream, destination_stream)
                    destination_stream.flush()
                    os.fsync(destination_stream.fileno())
            if self._runtime_tree_identity(os.fstat(source_fd)) != (
                self._runtime_tree_identity(opened)
            ):
                raise WorkspaceError(
                    f"runtime publication input changed during copy: {source}"
                )
            os.fchmod(destination_fd, stat.S_IMODE(opened.st_mode))
            os.utime(
                destination,
                ns=(opened.st_atime_ns, opened.st_mtime_ns),
                follow_symlinks=False,
            )
        except OSError as error:
            raise WorkspaceError(
                f"cannot copy runtime publication input {source}: {error}"
            ) from error
        finally:
            if destination_fd is not None:
                os.close(destination_fd)
            if source_fd is not None:
                os.close(source_fd)

    def _runtime_generation_entries(
        self,
        root: Path,
        state: Path | None,
    ) -> list[dict[str, Any]]:
        entries: list[dict[str, Any]] = []
        for directory, names, files in os.walk(root, followlinks=False):
            current = Path(directory)
            names.sort()
            files.sort()
            linked_directories: set[str] = set()
            for name in list(names):
                path = current / name
                relative = path.relative_to(root).as_posix()
                metadata = path.lstat()
                if stat.S_ISLNK(metadata.st_mode):
                    if state is not None and relative == "server/data":
                        target = state
                        kind = "external-state"
                    else:
                        raise WorkspaceError(
                            f"runtime publication contains a link: {path}"
                        )
                    if path.resolve(strict=False) != state.resolve(strict=False):
                        raise WorkspaceError(
                            f"runtime publication state link changed: {path}"
                        )
                    linked_directories.add(name)
                    entries.append(
                        {
                            "kind": kind,
                            "path": relative,
                            "target": str(target),
                        }
                    )
                elif not stat.S_ISDIR(metadata.st_mode):
                    raise WorkspaceError(
                        f"runtime publication contains a special file: {path}"
                    )
                else:
                    entries.append(
                        {
                            "kind": "directory",
                            "path": relative,
                            "mode": stat.S_IMODE(metadata.st_mode),
                        }
                    )
            names[:] = [name for name in names if name not in linked_directories]
            for name in files:
                path = current / name
                relative = path.relative_to(root).as_posix()
                metadata = path.lstat()
                if not stat.S_ISREG(metadata.st_mode):
                    raise WorkspaceError(
                        f"runtime publication contains a link or special file: {path}"
                    )
                entries.append(
                    {
                        "kind": "file",
                        "path": relative,
                        "mode": stat.S_IMODE(metadata.st_mode),
                        "size": metadata.st_size,
                        "sha256": _file_digest(path, "runtime publication file"),
                    }
                )
        return sorted(entries, key=lambda entry: entry["path"])

    @staticmethod
    def _runtime_state_output_identity_at(
        state_directory_fd: int, generation: str
    ) -> dict[str, int]:
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        descriptors: list[int] = []
        state_mount_id = _descriptor_mount_id(state_directory_fd)
        try:
            parent = state_directory_fd
            for name in ("tmp", "runtime-assets", generation):
                descriptor = os.open(name, flags, dir_fd=parent)
                descriptors.append(descriptor)
                if _descriptor_mount_id(descriptor) != state_mount_id:
                    raise WorkspaceError(
                        "server runtime state output crosses a mount"
                    )
                parent = descriptor
            metadata = os.fstat(descriptors[-1])
            return {"device": metadata.st_dev, "inode": metadata.st_ino}
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)

    @staticmethod
    def _prepare_runtime_state_output(
        state: Path,
        generation: str,
        state_directory_fd: int | None = None,
        cleanup_proof: list[bool] | None = None,
    ) -> tuple[Path, int, dict[str, int]]:
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
        descriptors: list[int] = []
        created_generation = False
        if cleanup_proof is not None:
            cleanup_proof[0] = True
        output = state / "tmp" / "runtime-assets" / generation
        state_mount_id: int | None = None

        def open_directory(parent: int, name: str, create: bool) -> int:
            if create:
                try:
                    os.mkdir(name, 0o700, dir_fd=parent)
                except FileExistsError:
                    pass
            metadata = os.stat(
                name, dir_fd=parent, follow_symlinks=False
            )
            if not stat.S_ISDIR(metadata.st_mode):
                raise WorkspaceError(
                    f"server runtime state output path is invalid: {output}"
                )
            descriptor = os.open(name, flags, dir_fd=parent)
            if Workspace._runtime_tree_identity(os.fstat(descriptor)) != (
                Workspace._runtime_tree_identity(metadata)
            ):
                os.close(descriptor)
                raise WorkspaceError(
                    f"server runtime state output path changed: {output}"
                )
            if (
                state_mount_id is not None
                and _descriptor_mount_id(descriptor) != state_mount_id
            ):
                os.close(descriptor)
                raise WorkspaceError(
                    f"server runtime state output crosses a mount: {output}"
                )
            return descriptor

        try:
            state_fd = (
                os.dup(state_directory_fd)
                if state_directory_fd is not None
                else os.open(state, flags)
            )
            descriptors.append(state_fd)
            state_metadata = os.fstat(state_fd)
            state_mount_id = _descriptor_mount_id(state_fd)
            if not stat.S_ISDIR(state_metadata.st_mode):
                raise WorkspaceError(f"server state is invalid: {state}")
            if state_directory_fd is None and Workspace._runtime_tree_identity(
                state_metadata
            ) != Workspace._runtime_tree_identity(
                state.stat(follow_symlinks=False)
            ):
                raise WorkspaceError(
                    f"server state changed before runtime publication: {state}"
                )
            tmp_fd = open_directory(state_fd, "tmp", True)
            descriptors.append(tmp_fd)
            container_fd = open_directory(tmp_fd, "runtime-assets", True)
            descriptors.append(container_fd)
            if cleanup_proof is not None:
                cleanup_proof[0] = False
            try:
                os.mkdir(generation, 0o700, dir_fd=container_fd)
            except FileExistsError as error:
                raise WorkspaceError(
                    f"server runtime state output already exists: {output}"
                ) from error
            created_generation = True
            generation_fd = open_directory(container_fd, generation, False)
            descriptors.append(generation_fd)
            marker = json.dumps(
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": f"runtime-state-output:{generation}",
                },
                indent=2,
                sort_keys=True,
            ).encode() + b"\n"
            marker_fd = os.open(
                MANAGED_MARKER,
                os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                0o600,
                dir_fd=generation_fd,
            )
            try:
                with os.fdopen(marker_fd, "wb", closefd=False) as stream:
                    stream.write(marker)
                    stream.flush()
                    os.fsync(stream.fileno())
            finally:
                os.close(marker_fd)
            metadata = os.fstat(generation_fd)
            result_fd = os.dup(generation_fd)
            return output, result_fd, {
                "device": metadata.st_dev,
                "inode": metadata.st_ino,
            }
        except BaseException as error:
            if created_generation and len(descriptors) >= 4:
                generation_fd = descriptors[-1]
                metadata = os.fstat(generation_fd)
                mount_id = _descriptor_mount_id(generation_fd)
                _prepare_owned_tree_removal(
                    generation_fd,
                    metadata.st_dev,
                    mount_id,
                    output,
                    stat.S_IMODE(metadata.st_mode),
                )
                _remove_owned_tree_contents(
                    generation_fd, metadata.st_dev, mount_id, output
                )
                parent_fd = descriptors[-2]
                tombstone = _owned_tree_tombstone_name(
                    generation, metadata.st_dev, metadata.st_ino
                )
                rename_no_replace_at(
                    parent_fd, generation, parent_fd, tombstone
                )
                moved = os.stat(
                    tombstone, dir_fd=parent_fd, follow_symlinks=False
                )
                if (moved.st_dev, moved.st_ino) != (
                    metadata.st_dev,
                    metadata.st_ino,
                ):
                    try:
                        rename_no_replace_at(
                            parent_fd, tombstone, parent_fd, generation
                        )
                    except WorkspaceError:
                        pass
                    raise WorkspaceError(
                        f"server runtime state output path changed: {output}"
                    )
                os.rmdir(tombstone, dir_fd=parent_fd)
                os.fsync(parent_fd)
                if cleanup_proof is not None:
                    cleanup_proof[0] = True
            if isinstance(error, WorkspaceError):
                raise
            if not isinstance(error, (OSError, ValueError)):
                raise
            raise WorkspaceError(
                f"cannot prepare server runtime state output {output}: {error}"
            ) from error
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)

    @staticmethod
    def _remove_runtime_state_output(
        path: Path,
        generation: str,
        state_directory_fd: int | None = None,
        expected_identity: dict[str, int] | None = None,
        keep_tombstone: bool = False,
    ) -> None:
        if state_directory_fd is not None:
            flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
            descriptors = [os.dup(state_directory_fd)]
            state_mount_id = _descriptor_mount_id(descriptors[0])

            def open_exact_directory(parent: int, name: str) -> int:
                visible = os.stat(name, dir_fd=parent, follow_symlinks=False)
                if not stat.S_ISDIR(visible.st_mode):
                    raise WorkspaceError(
                        f"server runtime state output path is invalid: {path}"
                    )
                descriptor = os.open(name, flags, dir_fd=parent)
                opened = os.fstat(descriptor)
                if Workspace._runtime_tree_identity(opened) != (
                    Workspace._runtime_tree_identity(visible)
                ):
                    os.close(descriptor)
                    raise WorkspaceError(
                        f"server runtime state output path changed: {path}"
                    )
                if _descriptor_mount_id(descriptor) != state_mount_id:
                    os.close(descriptor)
                    raise WorkspaceError(
                        f"server runtime state output crosses a mount: {path}"
                    )
                return descriptor

            try:
                for name in ("tmp", "runtime-assets"):
                    descriptors.append(open_exact_directory(descriptors[-1], name))
                parent_fd = descriptors[-1]
                entry_name = generation
                already_tombstoned = False
                try:
                    generation_fd = open_exact_directory(parent_fd, entry_name)
                except FileNotFoundError:
                    digest = hashlib.sha256(
                        generation.encode("utf-8")
                    ).hexdigest()[:16]
                    candidates = [
                        name
                        for name in os.listdir(parent_fd)
                        if re.fullmatch(
                            rf"\.remove-{digest}-[0-9a-f]+-[0-9a-f]+", name
                        )
                    ]
                    if len(candidates) != 1:
                        raise
                    entry_name = candidates[0]
                    generation_fd = open_exact_directory(parent_fd, entry_name)
                    already_tombstoned = True
                descriptors.append(generation_fd)
                metadata = os.fstat(generation_fd)
                if expected_identity is None or expected_identity != {
                    "device": metadata.st_dev,
                    "inode": metadata.st_ino,
                }:
                    raise WorkspaceError(
                        f"server runtime state output identity changed: {path}"
                    )
                if already_tombstoned:
                    match = re.fullmatch(
                        r"\.remove-[0-9a-f]{16}-([0-9a-f]+)-([0-9a-f]+)",
                        entry_name,
                    )
                    if match is None or (metadata.st_dev, metadata.st_ino) != (
                        int(match.group(1), 16),
                        int(match.group(2), 16),
                    ):
                        raise WorkspaceError(
                            f"server runtime state output tombstone is invalid: {path}"
                        )
                if not already_tombstoned:
                    marker = Workspace._load_state_json_at(
                        generation_fd,
                        MANAGED_MARKER,
                        "server runtime state output ownership",
                    )
                    if marker != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"runtime-state-output:{generation}",
                    }:
                        raise WorkspaceError(
                            "server runtime state output ownership is invalid: "
                            f"{path}"
                        )
                    tombstone = _owned_tree_tombstone_name(
                        generation, metadata.st_dev, metadata.st_ino
                    )
                    rename_no_replace_at(
                        parent_fd, generation, parent_fd, tombstone
                    )
                    moved = os.stat(
                        tombstone, dir_fd=parent_fd, follow_symlinks=False
                    )
                    if (moved.st_dev, moved.st_ino) != (
                        metadata.st_dev,
                        metadata.st_ino,
                    ):
                        try:
                            rename_no_replace_at(
                                parent_fd, tombstone, parent_fd, generation
                            )
                        except WorkspaceError:
                            pass
                        raise WorkspaceError(
                            f"server runtime state output path changed: {path}"
                        )
                    entry_name = tombstone
                else:
                    tombstone = entry_name
                mount_id = _descriptor_mount_id(generation_fd)
                _prepare_owned_tree_removal(
                    generation_fd,
                    metadata.st_dev,
                    mount_id,
                    path,
                    stat.S_IMODE(metadata.st_mode),
                )
                _remove_owned_tree_contents(
                    generation_fd, metadata.st_dev, mount_id, path
                )
                visible = os.stat(tombstone, dir_fd=parent_fd, follow_symlinks=False)
                opened = os.fstat(generation_fd)
                if (visible.st_dev, visible.st_ino) != (
                    opened.st_dev,
                    opened.st_ino,
                ):
                    raise WorkspaceError(
                        f"server runtime state output path changed: {path}"
                    )
                if not keep_tombstone:
                    os.rmdir(tombstone, dir_fd=parent_fd)
                return
            finally:
                for descriptor in reversed(descriptors):
                    os.close(descriptor)
        marker = path / MANAGED_MARKER
        if (
            path.is_symlink()
            or not path.is_dir()
            or marker.is_symlink()
            or not marker.is_file()
            or load_json(marker)
            != {
                "schema_version": SCHEMA_VERSION,
                "purpose": f"runtime-state-output:{generation}",
            }
        ):
            raise WorkspaceError(
                f"server runtime state output ownership is invalid: {path}"
            )
        remove_owned_tree(path)

    @staticmethod
    def _finish_runtime_state_output_tombstone(
        state_directory_fd: int,
        generation: str,
        expected_identity: dict[str, int],
    ) -> bool:
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        descriptors = [os.dup(state_directory_fd)]
        state_mount_id = _descriptor_mount_id(descriptors[0])
        try:
            for name in ("tmp", "runtime-assets"):
                try:
                    descriptor = os.open(
                        name, flags, dir_fd=descriptors[-1]
                    )
                except FileNotFoundError:
                    return False
                if _descriptor_mount_id(descriptor) != state_mount_id:
                    os.close(descriptor)
                    raise WorkspaceError(
                        "server runtime state output tombstone crosses a mount"
                    )
                descriptors.append(descriptor)
            parent_fd = descriptors[-1]
            tombstone = _owned_tree_tombstone_name(
                generation,
                expected_identity["device"],
                expected_identity["inode"],
            )
            try:
                visible = os.stat(
                    tombstone, dir_fd=parent_fd, follow_symlinks=False
                )
            except FileNotFoundError:
                return False
            descriptor = os.open(tombstone, flags, dir_fd=parent_fd)
            descriptors.append(descriptor)
            opened = os.fstat(descriptor)
            if (
                not stat.S_ISDIR(visible.st_mode)
                or (visible.st_dev, visible.st_ino)
                != (expected_identity["device"], expected_identity["inode"])
                or (opened.st_dev, opened.st_ino)
                != (visible.st_dev, visible.st_ino)
                or _descriptor_mount_id(descriptor) != state_mount_id
                or os.listdir(descriptor)
            ):
                raise WorkspaceError(
                    "server runtime state output tombstone is invalid"
                )
            os.rmdir(tombstone, dir_fd=parent_fd)
            os.fsync(parent_fd)
            return True
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)

    @staticmethod
    def _runtime_state_output_entry_exists(
        state_directory_fd: int, generation: str
    ) -> bool:
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        descriptors = [os.dup(state_directory_fd)]
        state_mount_id = _descriptor_mount_id(descriptors[0])
        try:
            for name in ("tmp", "runtime-assets"):
                try:
                    descriptor = os.open(
                        name, flags, dir_fd=descriptors[-1]
                    )
                except FileNotFoundError:
                    return False
                if _descriptor_mount_id(descriptor) != state_mount_id:
                    os.close(descriptor)
                    raise WorkspaceError(
                        "server runtime state output parent crosses a mount"
                    )
                descriptors.append(descriptor)
            try:
                os.stat(
                    generation,
                    dir_fd=descriptors[-1],
                    follow_symlinks=False,
                )
            except FileNotFoundError:
                return False
            return True
        finally:
            for descriptor in reversed(descriptors):
                os.close(descriptor)

    @staticmethod
    def _valid_state_identity(value: Any) -> bool:
        return bool(
            isinstance(value, dict)
            and set(value) == {"device", "inode"}
            and all(
                isinstance(value[key], int)
                and not isinstance(value[key], bool)
                and value[key] >= 0
                for key in ("device", "inode")
            )
        )

    @staticmethod
    def _clear_runtime_state_output_transaction(topology_root: Path) -> None:
        transaction = topology_root / RUNTIME_STATE_OUTPUT_TRANSACTION
        descriptor = _open_directory_nofollow(
            topology_root,
            os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
        )
        try:
            try:
                visible = os.stat(
                    RUNTIME_STATE_OUTPUT_TRANSACTION,
                    dir_fd=descriptor,
                    follow_symlinks=False,
                )
            except FileNotFoundError:
                return
            if not stat.S_ISREG(visible.st_mode) or visible.st_nlink != 1:
                raise WorkspaceError(
                    f"runtime state output transaction is invalid: {transaction}"
                )
            opened_fd = os.open(
                RUNTIME_STATE_OUTPUT_TRANSACTION,
                os.O_PATH | os.O_CLOEXEC | os.O_NOFOLLOW,
                dir_fd=descriptor,
            )
            try:
                opened = os.fstat(opened_fd)
                if (opened.st_dev, opened.st_ino) != (
                    visible.st_dev,
                    visible.st_ino,
                ):
                    raise WorkspaceError(
                        "runtime state output transaction changed before removal"
                    )
                tombstone = (
                    f".{RUNTIME_STATE_OUTPUT_TRANSACTION}.remove-"
                    f"{opened.st_dev:x}-{opened.st_ino:x}"
                )
                rename_no_replace_at(
                    descriptor,
                    RUNTIME_STATE_OUTPUT_TRANSACTION,
                    descriptor,
                    tombstone,
                )
                moved = os.stat(
                    tombstone, dir_fd=descriptor, follow_symlinks=False
                )
                if (moved.st_dev, moved.st_ino) != (
                    opened.st_dev,
                    opened.st_ino,
                ):
                    try:
                        rename_no_replace_at(
                            descriptor,
                            tombstone,
                            descriptor,
                            RUNTIME_STATE_OUTPUT_TRANSACTION,
                        )
                    except WorkspaceError:
                        pass
                    raise WorkspaceError(
                        "runtime state output transaction changed before removal"
                    )
                os.unlink(tombstone, dir_fd=descriptor)
            finally:
                os.close(opened_fd)
            os.fsync(descriptor)
        finally:
            os.close(descriptor)

    def _rollback_runtime_state_output_transaction(
        self,
        topology_root: Path,
        output: Path,
        generation: str,
        state_directory_fd: int,
        output_identity: dict[str, int],
    ) -> None:
        transaction_path = topology_root / RUNTIME_STATE_OUTPUT_TRANSACTION
        transaction = load_regular_json(
            transaction_path, "runtime state output transaction"
        )
        if (
            not isinstance(transaction, dict)
            or transaction.get("generation") != generation
            or transaction.get("phase") != "prepared"
            or transaction.get("output_identity") != output_identity
        ):
            raise WorkspaceError(
                f"runtime state output transaction is invalid: {transaction_path}"
            )
        self._remove_runtime_state_output(
            output,
            generation,
            state_directory_fd,
            output_identity,
            keep_tombstone=True,
        )
        durable_atomic_json(
            transaction_path, {**transaction, "phase": "complete"}
        )
        if not self._finish_runtime_state_output_tombstone(
            state_directory_fd, generation, output_identity
        ):
            raise WorkspaceError(
                f"runtime state output cleanup tombstone is missing: {output}"
            )
        self._clear_runtime_state_output_transaction(topology_root)

    def _recover_runtime_state_output_transaction(
        self, topology_root: Path, topology_name: str
    ) -> None:
        transaction_path = topology_root / RUNTIME_STATE_OUTPUT_TRANSACTION
        if not transaction_path.exists() and not transaction_path.is_symlink():
            return
        transaction = load_regular_json(
            transaction_path, "runtime state output transaction"
        )
        if (
            not isinstance(transaction, dict)
            or set(transaction)
            != {
                "schema_version",
                "generation",
                "state",
                "state_identity",
                "phase",
                "output_identity",
            }
            or transaction.get("schema_version") != SCHEMA_VERSION
            or not isinstance(transaction.get("generation"), str)
            or re.fullmatch(r"[0-9a-f]{64}", transaction["generation"]) is None
            or not isinstance(transaction.get("state"), str)
            or not Path(transaction["state"]).is_absolute()
            or transaction.get("phase") not in {"creating", "prepared", "complete"}
            or not self._valid_state_identity(transaction.get("state_identity"))
            or (
                transaction["phase"] in {"prepared", "complete"}
                and not self._valid_state_identity(
                    transaction.get("output_identity")
                )
            )
            or (
                transaction["phase"] == "creating"
                and transaction.get("output_identity") is not None
            )
        ):
            raise WorkspaceError(
                f"runtime state output transaction is invalid: {transaction_path}"
            )
        generation = transaction["generation"]
        status_path = topology_root / "status.json"
        if status_path.is_file() and not status_path.is_symlink():
            try:
                status = self.topology_status(topology_name)
            except WorkspaceError:
                status = None
            runtime = status.get("runtime") if isinstance(status, dict) else None
            control = status.get("control") if isinstance(status, dict) else None
            if (
                isinstance(runtime, dict)
                and isinstance(control, dict)
                and control.get("generation") == generation
                and runtime.get("mutable_state_output_identities")
                == [transaction.get("output_identity")]
            ):
                self._clear_runtime_state_output_transaction(topology_root)
                return
        state = Path(transaction["state"])
        identity = transaction["state_identity"]
        with self._topology_state_lock(
            state, preparing_topology=topology_name
        ) as lease:
            descriptor = self._lock_state_directory_mutation(
                state, identity, "server state"
            )
            try:
                lease.bind(identity)
                if transaction["phase"] == "creating":
                    raise WorkspaceError(
                        "runtime state output creation was interrupted before "
                        "exact ownership was recorded; preserve the state and "
                        f"inspect {transaction_path}"
                    )
                output = state / "tmp" / "runtime-assets" / generation
                output_identity = transaction["output_identity"]
                if transaction["phase"] == "prepared":
                    self._remove_runtime_state_output(
                        output,
                        generation,
                        descriptor,
                        output_identity,
                        keep_tombstone=True,
                    )
                    durable_atomic_json(
                        transaction_path,
                        {**transaction, "phase": "complete"},
                    )
                if self._runtime_state_output_entry_exists(descriptor, generation):
                    raise WorkspaceError(
                        f"completed runtime state output reappeared: {output}"
                    )
                self._finish_runtime_state_output_tombstone(
                    descriptor, generation, output_identity
                )
                self._clear_runtime_state_output_transaction(topology_root)
            finally:
                os.close(descriptor)

    @staticmethod
    def _make_tree_owner_writable(root: Path) -> None:
        directories: list[Path] = []
        for directory, names, files in os.walk(root, followlinks=False):
            current = Path(directory)
            directories.append(current)
            for name in names:
                path = current / name
                metadata = path.lstat()
                if stat.S_ISLNK(metadata.st_mode):
                    continue
                if not stat.S_ISDIR(metadata.st_mode):
                    raise WorkspaceError(
                        f"mutable source copy contains a special entry: {path}"
                    )
            for name in files:
                path = current / name
                metadata = path.lstat()
                if not stat.S_ISREG(metadata.st_mode):
                    raise WorkspaceError(
                        f"mutable source copy contains a special entry: {path}"
                    )
                path.chmod(stat.S_IMODE(metadata.st_mode) | 0o600)
        for directory in directories:
            directory.chmod(stat.S_IMODE(directory.lstat().st_mode) | 0o700)

    @staticmethod
    def _seal_runtime_generation(root: Path) -> None:
        directories: list[Path] = []
        for directory, names, files in os.walk(root, followlinks=False):
            current = Path(directory)
            directories.append(current)
            for name in names:
                path = current / name
                metadata = path.lstat()
                if stat.S_ISLNK(metadata.st_mode):
                    continue
                if not stat.S_ISDIR(metadata.st_mode):
                    raise WorkspaceError(
                        f"runtime generation contains a special entry: {path}"
                    )
            for name in files:
                path = current / name
                metadata = path.lstat()
                if not stat.S_ISREG(metadata.st_mode):
                    raise WorkspaceError(
                        f"runtime generation contains a special entry: {path}"
                    )
                if path.name == RUNTIME_GENERATION_LEASE and path.parent == root:
                    path.chmod(0o600)
                else:
                    path.chmod(stat.S_IMODE(metadata.st_mode) & ~0o222)
        for directory in reversed(directories):
            directory.chmod(stat.S_IMODE(directory.lstat().st_mode) & ~0o222)

    def _runtime_publication_input_digests(
        self,
        build_root: Path,
        selected: dict[str, Path],
        services: list[str],
        sound_root: Path | None,
    ) -> dict[str, str]:
        directories: dict[str, tuple[Path, set[str]]] = {}
        files: dict[str, Path] = {}
        if "client" in services:
            directories.update(
                {
                    "client-source": (
                        selected["client"],
                        {".git", "build", "sound", MANAGED_MARKER},
                    ),
                    "client-binary": (
                        self._classic_binary_directory(build_root, "client"),
                        {"src"},
                    ),
                    "sound": (
                        sound_root or selected["sound"],
                        {".git", "build", MANAGED_MARKER},
                    ),
                }
            )
        if "server" in services:
            source = selected["server"]
            directories.update(
                {
                    "server-binary": (
                        self._classic_binary_directory(build_root, "server"),
                        set(),
                    ),
                    "server-tools": (source / "tools", set()),
                    "content-lib": (build_root / "runtime" / "content" / "lib", set()),
                    "content-maps": (build_root / "runtime" / "content" / "maps", set()),
                    "resources": (build_root / "runtime" / "resources", set()),
                    "client-maps": (build_root / "runtime" / "client-maps", set()),
                }
            )
            for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
                files[f"server-{name}"] = source / name
            custom = source / "server-custom.cfg"
            if custom.is_file() and not custom.is_symlink():
                files["server-server-custom.cfg"] = custom
        return {
            **{
                name: _tree_digest(path, exclusions, reject_symlinks=True)
                for name, (path, exclusions) in sorted(directories.items())
            },
            **{
                name: _file_digest(path, "runtime publication input")
                for name, path in sorted(files.items())
            },
        }

    def _publish_runtime_generation(
        self,
        owner_root: Path,
        generation: str,
        profile_name: str,
        build_root: Path,
        selected: dict[str, Path],
        resolved: dict[str, dict[str, Any]],
        services: list[str],
        *,
        identity: dict[str, Any],
        state: Path | None = None,
        state_directory_fd: int | None = None,
        sound_root: Path | None = None,
    ) -> tuple[Path, int, dict[str, Any], int | None]:
        generations = owner_root / "generations"
        generations.mkdir(exist_ok=True)
        if generations.is_symlink() or not generations.is_dir():
            raise WorkspaceError(
                f"runtime generation container is invalid: {generations}"
            )
        published = generations / generation
        if published.exists() or published.is_symlink():
            raise WorkspaceError(f"runtime generation already exists: {published}")
        staging = Path(
            tempfile.mkdtemp(prefix=f".{generation}-", dir=generations)
        )
        lease_fd: int | None = None
        state_output: Path | None = None
        state_output_access: Path | None = None
        state_output_identity: dict[str, int] | None = None
        state_output_fd: int | None = None
        preparation_cleanup_proof = [False]
        output_transaction = owner_root / RUNTIME_STATE_OUTPUT_TRANSACTION
        topology_output = identity.get("kind") == "topology" and "server" in services
        try:
            input_digests = self._runtime_publication_input_digests(
                build_root, selected, services, sound_root
            )
            atomic_json(
                staging / MANAGED_MARKER,
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "immutable-runtime-generation",
                },
            )
            if "client" in services:
                client_runtime = staging / "client"
                client_runtime.mkdir()
                self._copy_runtime_directory_contents(
                    selected["client"],
                    client_runtime,
                    frozenset({".git", "build", "sound", MANAGED_MARKER}),
                )
                self._copy_runtime_tree(
                    sound_root or selected["sound"],
                    client_runtime / "sound",
                    frozenset({".git", "build", MANAGED_MARKER}),
                )
                self._copy_runtime_directory_contents(
                    self._classic_binary_directory(build_root, "client"),
                    client_runtime,
                    frozenset({"src"}),
                )
            if "server" in services:
                if state is None:
                    raise WorkspaceError("server runtime generation lacks state")
                server_runtime = staging / "server"
                server_runtime.mkdir()
                self._copy_runtime_directory_contents(
                    self._classic_binary_directory(build_root, "server"),
                    server_runtime,
                )
                source = selected["server"]
                self._copy_runtime_tree(
                    source / "tools", server_runtime / "tools"
                )
                for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
                    self._copy_runtime_regular_file(source / name, server_runtime / name)
                custom = source / "server-custom.cfg"
                if custom.is_file() and not custom.is_symlink():
                    self._copy_runtime_regular_file(
                        custom, server_runtime / "server-custom.cfg"
                    )
                content = build_root / "runtime" / "content"
                self._copy_runtime_tree(
                    content / "lib", server_runtime / "lib"
                )
                self._copy_runtime_tree(
                    content / "maps", server_runtime / "maps"
                )
                self._copy_runtime_tree(
                    build_root / "runtime" / "resources",
                    server_runtime / "resources",
                )
                (server_runtime / "data").symlink_to(
                    (
                        Path(f"/proc/self/fd/{state_directory_fd}")
                        if state_directory_fd is not None
                        else state
                    ),
                    target_is_directory=True,
                )
                if topology_output:
                    state_metadata = (
                        os.fstat(state_directory_fd)
                        if state_directory_fd is not None
                        else state.stat(follow_symlinks=False)
                    )
                    durable_atomic_json(
                        output_transaction,
                        {
                            "schema_version": SCHEMA_VERSION,
                            "generation": generation,
                            "state": str(state),
                            "state_identity": {
                                "device": state_metadata.st_dev,
                                "inode": state_metadata.st_ino,
                            },
                            "phase": "creating",
                            "output_identity": None,
                        },
                    )
                (
                    state_output,
                    state_output_fd,
                    state_output_identity,
                ) = self._prepare_runtime_state_output(
                    state,
                    generation,
                    state_directory_fd,
                    preparation_cleanup_proof,
                )
                if topology_output:
                    durable_atomic_json(
                        output_transaction,
                        {
                            "schema_version": SCHEMA_VERSION,
                            "generation": generation,
                            "state": str(state),
                            "state_identity": {
                                "device": os.fstat(state_directory_fd).st_dev,
                                "inode": os.fstat(state_directory_fd).st_ino,
                            },
                            "phase": "prepared",
                            "output_identity": state_output_identity,
                        },
                    )
                state_output_access = Path(f"/proc/self/fd/{state_output_fd}")
                os.mkdir("data", dir_fd=state_output_fd)
                client_maps = build_root / "runtime" / "client-maps"
                self._validate_region_maps(client_maps)
                self._copy_runtime_tree(
                    client_maps,
                    state_output / "client-maps",
                    pinned_destination_parent_fd=state_output_fd,
                )
                state_output_entries = self._runtime_generation_entries(
                    state_output_access / "client-maps", None
                )
                self._seal_runtime_generation(state_output_access / "client-maps")
            else:
                state_output_entries = []

            if self._runtime_publication_input_digests(
                build_root, selected, services, sound_root
            ) != input_digests:
                raise WorkspaceError(
                    "runtime publication inputs changed during staging"
                )
            if state_output is not None and state_output_identity is not None:
                visible_output_identity = (
                    self._runtime_state_output_identity_at(
                        state_directory_fd, generation
                    )
                    if state_directory_fd is not None
                    else self._state_identity(state_output)
                )
                if visible_output_identity != state_output_identity:
                    raise WorkspaceError(
                        "server runtime state output changed during publication: "
                        f"{state_output}"
                    )

            build_metadata = build_root / BUILD_METADATA
            source_trees: dict[str, str] = {}
            for component, coordinate in resolved.items():
                tree = git(
                    Path(coordinate["checkout_path"]),
                    "rev-parse",
                    f"{coordinate['head']}^{{tree}}",
                    capture=True,
                    trace=False,
                )
                if not re.fullmatch(r"[0-9a-f]{40,64}", tree):
                    raise WorkspaceError(
                        f"runtime source tree identity is invalid: {component}"
                    )
                source_trees[component] = tree
            manifest = {
                "schema_version": RUNTIME_GENERATION_SCHEMA_VERSION,
                "generation": generation,
                "profile": profile_name,
                "identity": identity,
                "services": services,
                "resolved": resolved,
                "source_trees": source_trees,
                "input_digests": input_digests,
                "build": {
                    "root": str(build_root),
                    "metadata_sha256": (
                        _file_digest(build_metadata, "build metadata")
                        if build_metadata.is_file() and not build_metadata.is_symlink()
                        else None
                    ),
                },
                "external_state": str(state) if state is not None else None,
                "mutable_state_outputs": (
                    [str(state_output)] if state_output is not None else []
                ),
                "mutable_state_output_identities": (
                    [state_output_identity]
                    if state_output_identity is not None
                    else []
                ),
                "mutable_state_output_entries": state_output_entries,
                "entries": self._runtime_generation_entries(staging, state),
            }
            atomic_json(staging / RUNTIME_GENERATION_MANIFEST, manifest)
            lease_fd = open_regular_file(
                staging / RUNTIME_GENERATION_LEASE,
                os.O_RDWR | os.O_CREAT,
                "runtime generation lease",
            )
            lease_identity = initialize_lease(lease_fd, generation)
            try:
                fcntl.flock(lease_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError as error:
                raise WorkspaceError(
                    f"cannot lock runtime generation lease: {error}"
                ) from error
            manifest_sha256 = _file_digest(
                staging / RUNTIME_GENERATION_MANIFEST,
                "runtime generation manifest",
            )
            self._seal_runtime_generation(staging)
            staging.replace(published)
            runtime_record = {
                "schema_version": RUNTIME_GENERATION_SCHEMA_VERSION,
                "generation": generation,
                "path": str(published),
                "manifest_sha256": manifest_sha256,
                "lease": lease_identity,
                "external_state": str(state) if state is not None else None,
                "mutable_state_outputs": (
                    [str(state_output)] if state_output is not None else []
                ),
                "mutable_state_output_identities": (
                    [state_output_identity]
                    if state_output_identity is not None
                    else []
                ),
            }
            result_fd = lease_fd
            lease_fd = None
            result_state_output_fd = state_output_fd
            state_output_fd = None
            return published, result_fd, runtime_record, result_state_output_fd
        except BaseException:
            if staging.exists():
                remove_owned_tree(staging)
            cleanup_output = state_output_access or state_output
            output_cleanup_complete = (
                state_output is None and preparation_cleanup_proof[0]
            )
            if cleanup_output is not None and cleanup_output.exists():
                if (
                    state_output_identity is None
                    and state_directory_fd is not None
                ):
                    state_output_identity = self._runtime_state_output_identity_at(
                        state_directory_fd, generation
                    )
                self._remove_runtime_state_output(
                    state_output or cleanup_output,
                    generation,
                    state_directory_fd,
                    state_output_identity,
                    keep_tombstone=topology_output,
                )
                if topology_output and state_directory_fd is not None:
                    transaction = load_regular_json(
                        output_transaction, "runtime state output transaction"
                    )
                    durable_atomic_json(
                        output_transaction, {**transaction, "phase": "complete"}
                    )
                    self._finish_runtime_state_output_tombstone(
                        state_directory_fd, generation, state_output_identity
                    )
                    output_cleanup_complete = True
            if (
                topology_output
                and output_cleanup_complete
                and output_transaction.exists()
            ):
                self._clear_runtime_state_output_transaction(owner_root)
            raise
        finally:
            if lease_fd is not None:
                os.close(lease_fd)
            if state_output_fd is not None:
                os.close(state_output_fd)

    def _copy_topology_runtime_inputs(
        self,
        topology_root: Path,
        inputs: tuple[tuple[str, Path, str], ...],
    ) -> dict[str, Path]:
        container = topology_root / "runtime"
        expected = {
            name: {"schema_version": SCHEMA_VERSION, "purpose": purpose}
            for name, _source, purpose in inputs
        }
        if container.exists() or container.is_symlink():
            if not container.is_dir() or container.is_symlink():
                raise WorkspaceError(
                    f"topology runtime container is invalid: {container}"
                )
            for entry in container.iterdir():
                metadata = expected.get(entry.name)
                marker = entry / MANAGED_MARKER
                if (
                    metadata is None
                    or not entry.is_dir()
                    or entry.is_symlink()
                    or not marker.is_file()
                    or marker.is_symlink()
                    or load_json(marker) != metadata
                ):
                    raise WorkspaceError(
                        f"topology runtime container has an unmanaged entry: {entry}"
                    )

        staging = Path(tempfile.mkdtemp(prefix=".runtime-", dir=topology_root))
        staging.rmdir()
        staging.mkdir()
        copied_descriptors: dict[str, int] = {}
        try:
            for name, source, purpose in inputs:
                metadata = expected[name]
                marker = source / MANAGED_MARKER
                if (
                    not source.is_dir()
                    or source.is_symlink()
                    or not marker.is_file()
                    or marker.is_symlink()
                    or load_json(marker) != metadata
                ):
                    raise WorkspaceError(
                        f"topology runtime input is not managed for {purpose}: {source}"
                    )
                destination = staging / name
                copied_descriptors[name] = self._copy_topology_runtime_tree(
                    source, destination
                )
                copied_marker = destination / MANAGED_MARKER
                if (
                    not destination.is_dir()
                    or destination.is_symlink()
                    or not copied_marker.is_file()
                    or copied_marker.is_symlink()
                    or load_json(copied_marker) != metadata
                ):
                    raise WorkspaceError(
                        f"copied topology runtime input is invalid: {destination}"
                    )

            def verify_runtime_install() -> None:
                flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
                for name, source, _purpose in inputs:
                    retained_fd = copied_descriptors[name]
                    try:
                        installed_fd = os.open(container / name, flags)
                    except OSError as error:
                        raise WorkspaceError(
                            "cannot validate installed topology runtime input "
                            f"{container / name}: {error}"
                        ) from error
                    try:
                        source_fd = os.open(source, flags)
                    except OSError as error:
                        os.close(installed_fd)
                        raise WorkspaceError(
                            "cannot validate topology runtime source "
                            f"{source}: {error}"
                        ) from error
                    try:
                        retained = os.fstat(retained_fd)
                        installed = os.fstat(installed_fd)
                        if (
                            retained.st_dev != installed.st_dev
                            or retained.st_ino != installed.st_ino
                        ):
                            raise WorkspaceError(
                                "installed topology runtime input changed: "
                                f"{container / name}"
                            )
                        self._compare_topology_runtime_directories(
                            source_fd,
                            installed_fd,
                            source,
                            container / name,
                        )
                    finally:
                        os.close(source_fd)
                        os.close(installed_fd)

            replace_runtime_directory(
                container,
                staging,
                ".runtime-previous-",
                verify_runtime_install,
            )
        except BaseException:
            if staging.exists():
                remove_owned_tree(staging)
            raise
        finally:
            for descriptor in copied_descriptors.values():
                os.close(descriptor)
        return {name: container / name for name in expected}

    def _prepare_topology_client_runtime(
        self,
        topology_root: Path,
        selected: dict[str, Path],
        sound_root: Path | None = None,
    ) -> Path:
        runtime = self._reset_topology_subdirectory(
            topology_root, "client-runtime", "topology-client-runtime"
        )
        source = selected["client"]
        for entry in source.iterdir():
            if entry.name in {".git", "build", "sound", MANAGED_MARKER}:
                continue
            (runtime / entry.name).symlink_to(
                entry, target_is_directory=entry.is_dir()
            )
        (runtime / "sound").symlink_to(
            sound_root or selected["sound"], target_is_directory=True
        )
        return runtime

    @staticmethod
    def _recorded_process_running(record: Any) -> bool:
        if not isinstance(record, dict):
            return False
        pid = record.get("pid")
        start_time = record.get("start_time")
        return (
            isinstance(pid, int)
            and not isinstance(pid, bool)
            and isinstance(start_time, str)
            and process_matches(pid, start_time)
        )

    @staticmethod
    def _topology_control_request(
        name: str,
        control: dict[str, str],
        action: str,
    ) -> bool:
        request = {
            "action": action,
            "name": name,
            "generation": control["generation"],
        }
        try:
            endpoint = Path(control["socket"])
            metadata = endpoint.lstat()
            if (
                not stat.S_ISSOCK(metadata.st_mode)
                or stat.S_IMODE(metadata.st_mode) != 0o600
                or metadata.st_uid != os.geteuid()
            ):
                return False
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.settimeout(0.5)
                client.connect(str(endpoint))
                client.sendall(json.dumps(request, sort_keys=True).encode())
                client.shutdown(socket.SHUT_WR)
                payload = bytearray()
                while len(payload) <= 4096:
                    chunk = client.recv(4097 - len(payload))
                    if not chunk:
                        break
                    payload.extend(chunk)
                    if b"\n" in chunk:
                        break
            if len(payload) > 4096:
                return False
            response = json.loads(payload)
        except (FileNotFoundError, OSError, TimeoutError, json.JSONDecodeError):
            return False
        return bool(
            isinstance(response, dict)
            and set(response) == {"generation", "name", "ok"}
            and response.get("ok") is True
            and response.get("name") == name
            and response.get("generation") == control["generation"]
        )

    @staticmethod
    def _topology_process_tree_active(
        root: Path, control: dict[str, Any] | None = None
    ) -> bool:
        path = root / TOPOLOGY_PROCESS_TREE_LEASE
        if control is None and (path.is_symlink() or not path.is_file()):
            return False
        try:
            if control is not None:
                return bound_lease_locked(
                    path, control["generation"], control["lease"]
                )
            return lease_locked(path)
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect topology process-tree lease {path}: {error}"
            ) from error

    def _validate_topology_state_policy(
        self,
        name: str,
        root: Path,
        status: dict[str, Any],
        control: dict[str, Any] | None,
    ) -> None:
        policy = status.get("state_policy")
        state = status.get("state")
        services = status.get("services")
        server_service = isinstance(services, dict) and "server" in services
        pre_service_failure = (
            bool(status.get("error"))
            and services == {}
            and state is not None
            and policy is not None
        )
        server_present = server_service or pre_service_failure
        if not server_present:
            if policy is not None or state is not None:
                raise WorkspaceError(f"topology state policy is invalid: {name}")
            return
        if not isinstance(policy, dict) or not isinstance(state, str):
            raise WorkspaceError(f"topology state policy is invalid: {name}")
        common = {
            "mode",
            "name",
            "path",
            "owner",
            "lifecycle",
            "identity",
            "lease_identity",
            "implementation",
        }
        mode = policy.get("mode")
        expected_keys = common | (
            {"created_at", "profile", "server"}
            if mode == "temporary"
            else set()
        )
        identity = policy.get("identity")
        lease_identity = policy.get("lease_identity")
        implementation = policy.get("implementation")
        providers = status.get("providers")
        resolved = status.get("resolved")
        if (
            mode not in {"temporary", "named", "default"}
            or set(policy) != expected_keys
            or policy.get("path") != state
            or not isinstance(policy.get("owner"), dict)
            or not isinstance(policy.get("lifecycle"), str)
            or not isinstance(identity, dict)
            or set(identity) != {"device", "inode"}
            or not all(
                isinstance(value, int) and not isinstance(value, bool) and value >= 0
                for value in identity.values()
            )
            or not isinstance(lease_identity, dict)
            or set(lease_identity) != {"device", "inode"}
            or not all(
                isinstance(value, int) and not isinstance(value, bool) and value >= 0
                for value in lease_identity.values()
            )
            or not isinstance(implementation, dict)
            or set(implementation) != {"stack", "provider", "repository"}
            or not isinstance(providers, dict)
            or not isinstance(resolved, dict)
            or "server" not in providers
            or providers["server"] not in resolved
            or implementation
            != self._state_implementation(status["stack"], providers, resolved)
        ):
            raise WorkspaceError(f"topology state policy is invalid: {name}")
        path = self._canonical_state_path(Path(state))
        lifecycle = policy["lifecycle"]
        if mode == "temporary":
            generation = control.get("generation") if isinstance(control, dict) else None
            expected_path = root / "temporary-states" / str(generation)
            promoted_name = policy.get("name")
            if (
                (
                    lifecycle in {"promotion-pending", "promoted"}
                    and (
                        not isinstance(promoted_name, str)
                        or promoted_name == "default"
                    )
                )
                or lifecycle not in {"promotion-pending", "promoted"}
                and promoted_name is not None
                or generation is None
                or path != expected_path
                or policy["owner"]
                != {
                    "kind": "topology-generation",
                    "topology": name,
                    "generation": generation,
                }
                or lifecycle
                not in {
                    "disposable",
                    "retained",
                    "removal-pending",
                    "promotion-pending",
                    "promoted",
                    "removed",
                }
                or not isinstance(policy.get("created_at"), str)
                or not policy["created_at"]
                or policy.get("profile") != status.get("profile")
                or policy.get("server") != resolved[providers["server"]]
            ):
                raise WorkspaceError(f"temporary topology state policy is invalid: {name}")
            if (
                lifecycle not in {"removed", "removal-pending"}
                or path.exists()
                or path.is_symlink()
            ):
                if lifecycle == "removed":
                    raise WorkspaceError(
                        f"removed temporary topology state still exists: {name}"
                    )
                marker = path / MANAGED_MARKER
                metadata_path = path / TEMPORARY_STATE_METADATA
                if (
                    path.is_symlink()
                    or not path.is_dir()
                    or self._state_identity(path) != identity
                    or marker.is_symlink()
                    or not marker.is_file()
                    or load_json(marker)
                    != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": "temporary-topology-state",
                        "topology": name,
                        "generation": generation,
                    }
                    or metadata_path.is_symlink()
                    or not metadata_path.is_file()
                ):
                    raise WorkspaceError(
                        f"temporary topology state identity is invalid: {name}"
                    )
                metadata = load_json(metadata_path)
                if (
                    not isinstance(metadata, dict)
                    or metadata.get("schema_version")
                    != TEMPORARY_STATE_SCHEMA_VERSION
                    or not self._temporary_state_metadata_matches(
                        policy, metadata.get("state_policy")
                    )
                ):
                    raise WorkspaceError(
                        f"temporary topology state metadata is invalid: {name}"
                    )
                self._validate_state_implementation(path, implementation)
                if lifecycle == "promoted" and self._canonical_state_path(
                    Path(self._load_states().get(str(promoted_name), "/"))
                ) != path:
                    raise WorkspaceError(
                        f"promoted temporary state registration is invalid: {name}"
                    )
        else:
            expected_name = "default" if mode == "default" else policy.get("name")
            expected_persistent = (
                self._persistent_state_policy(
                    expected_name, path, implementation
                )
                if isinstance(expected_name, str)
                else None
            )
            if (
                not isinstance(expected_name, str)
                or policy.get("name") != expected_name
                or not isinstance(expected_persistent, dict)
                or policy.get("owner") != expected_persistent["owner"]
                or lifecycle != expected_persistent["lifecycle"]
                or path != self._state_location(expected_name)
                or self._state_identity(path) != identity
            ):
                raise WorkspaceError(f"persistent topology state policy is invalid: {name}")
            self._validate_state_implementation(path, implementation)

    def topology_status(self, name: str) -> dict[str, Any]:
        root = self._topology_directory(name)
        status_path = root / "status.json"
        if not status_path.is_file() or status_path.is_symlink():
            raise WorkspaceError(f"topology has not been started: {name}")
        status = load_json(status_path)
        required = {
            "schema_version",
            "name",
            "profile",
            "dependencies",
            "state",
            "build_root",
            "resolved",
            "endpoint",
            "ready",
            "started_at",
            "stopped_at",
            "supervisor",
            "services",
        }
        optional = {
            "stack",
            "providers",
            "sound",
            "control",
            "port_reservation",
            "runtime",
            "state_policy",
            "shutdown",
            "mutable_state_cleanup",
        }
        topology_schema = status.get("schema_version") if isinstance(status, dict) else None
        current_runtime_record = topology_schema in {
            LEGACY_RUNTIME_TOPOLOGY_STATUS_SCHEMA_VERSION,
            TOPOLOGY_STATUS_SCHEMA_VERSION,
        }
        if current_runtime_record:
            required.add("runtime")
        if topology_schema == TOPOLOGY_STATUS_SCHEMA_VERSION:
            required.add("state_policy")
        historical_record = isinstance(status, dict) and not (
            {"stack", "providers"} & set(status)
        )
        historical_coordinate_keys = {
            "path",
            "checkout_path",
            "checkout",
            "source",
            "head",
            "dirty",
        }
        coordinate_historical_record = (
            isinstance(status, dict)
            and not historical_record
            and isinstance(status.get("resolved"), dict)
            and bool(status["resolved"])
            and all(
                isinstance(record, dict)
                and set(record) == historical_coordinate_keys
                for record in status["resolved"].values()
            )
        )
        retired_content_record = (
            isinstance(status, dict)
            and not historical_record
            and status.get("stack") == "classic"
            and isinstance(status.get("providers"), dict)
            and status["providers"].get("content") == "content-1x"
            and isinstance(status.get("resolved"), dict)
            and isinstance(status["resolved"].get("content-1x"), dict)
            and status["resolved"]["content-1x"].get("repository")
            == "atrinik/content"
            and status["resolved"]["content-1x"].get("branch") == "1.x"
            and status["resolved"]["content-1x"].get("checkout") == "content-1x"
            and status["resolved"]["content-1x"].get("source") == "."
            and isinstance(
                status["resolved"]["content-1x"].get("path"), str
            )
            and isinstance(
                status["resolved"]["content-1x"].get("checkout_path"), str
            )
            and Path(
                status["resolved"]["content-1x"]["path"]
            ).resolve(strict=False)
            == Path(
                status["resolved"]["content-1x"]["checkout_path"]
            ).resolve(strict=False)
        )
        if (
            not isinstance(status, dict)
            or topology_schema not in {
                SCHEMA_VERSION,
                LEGACY_RUNTIME_TOPOLOGY_STATUS_SCHEMA_VERSION,
                TOPOLOGY_STATUS_SCHEMA_VERSION,
            }
            or status.get("name") != name
            or not required <= set(status) <= required | optional | {"error"}
            or not isinstance(status.get("dependencies"), list)
            or not isinstance(status.get("resolved"), dict)
            or not isinstance(status.get("ready"), bool)
            or not isinstance(status.get("profile"), str)
            or not status["profile"]
            or not all(
                isinstance(dependency, str)
                and validate_name(dependency, "topology dependency")
                for dependency in status["dependencies"]
            )
            or len(status["dependencies"]) != len(set(status["dependencies"]))
            or status.get("state") is not None
            and (
                not isinstance(status.get("state"), str)
                or not Path(status["state"]).is_absolute()
            )
            or not isinstance(status.get("build_root"), str)
            or not Path(status["build_root"]).is_absolute()
            or not isinstance(status.get("started_at"), str)
            or not status["started_at"]
            or status.get("stopped_at") is not None
            and not isinstance(status.get("stopped_at"), str)
            or "error" in status
            and not isinstance(status.get("error"), str)
        ):
            raise WorkspaceError(f"topology status is invalid: {name}")
        if "sound" in status:
            try:
                validate_sound_record(status["sound"])
            except WorkspaceError as error:
                raise WorkspaceError(f"topology status is invalid: {name}") from error
        shutdown = status.get("shutdown")
        if shutdown is not None and (
            not isinstance(shutdown, dict)
            or set(shutdown) != {"control_requested", "clean"}
            or not isinstance(shutdown.get("control_requested"), bool)
            or not isinstance(shutdown.get("clean"), bool)
            or shutdown["clean"] and not shutdown["control_requested"]
            or shutdown["clean"] and status.get("error") is not None
        ):
            raise WorkspaceError(f"topology shutdown status is invalid: {name}")
        mutable_state_cleanup = status.get("mutable_state_cleanup")
        if mutable_state_cleanup is not None:
            cleanup_entries = (
                mutable_state_cleanup.get("entries")
                if isinstance(mutable_state_cleanup, dict)
                else None
            )
            if (
                topology_schema != TOPOLOGY_STATUS_SCHEMA_VERSION
                or not isinstance(mutable_state_cleanup, dict)
                or set(mutable_state_cleanup)
                != {"schema_version", "generation", "entries"}
                or mutable_state_cleanup.get("schema_version") != SCHEMA_VERSION
                or not isinstance(cleanup_entries, list)
                or any(
                    not isinstance(entry, dict)
                    or set(entry) != {"path", "identity", "status"}
                    or not isinstance(entry.get("path"), str)
                    or not isinstance(entry.get("identity"), dict)
                    or set(entry["identity"]) != {"device", "inode"}
                    or any(
                        not isinstance(value, int)
                        or isinstance(value, bool)
                        or value < 0
                        for value in entry["identity"].values()
                    )
                    or entry.get("status") not in {"pending", "complete"}
                    for entry in cleanup_entries
                )
            ):
                raise WorkspaceError(
                    f"topology mutable state cleanup status is invalid: {name}"
                )
        if historical_record:
            status["stack"] = "classic"
            status["providers"] = {
                role: {
                    "client": "classic-client",
                    "server": "classic-server",
                    "protocol": "classic-protocol",
                    "libatrinik": "classic-libatrinik",
                    "content": "content-1x",
                }.get(role, role)
                for role in status["dependencies"]
            }
            status["inert_historical_record"] = True
        elif coordinate_historical_record or retired_content_record:
            stack_name = status.get("stack")
            providers = status.get("providers")
            if (
                not isinstance(stack_name, str)
                or not isinstance(providers, dict)
                or set(providers) != set(status["dependencies"])
                or any(
                    not isinstance(role, str)
                    or not isinstance(component, str)
                    for role, component in providers.items()
                )
                or set(status["resolved"]) != set(providers.values())
            ):
                raise WorkspaceError(
                    f"historical topology identity status is invalid: {name}"
                )
            status["inert_historical_record"] = True
        else:
            stack_name = status.get("stack")
            providers = status.get("providers")
            if (
                not isinstance(stack_name, str)
                or stack_name not in self.manifest.stacks
                or not isinstance(providers, dict)
                or set(providers) != set(status["dependencies"])
                or any(
                    not isinstance(component, str)
                    or component
                    != self.manifest.provider(stack_name, role).name
                    for role, component in providers.items()
                )
            ):
                raise WorkspaceError(f"topology stack/provider status is invalid: {name}")
            if set(status["resolved"]) != set(providers.values()):
                raise WorkspaceError(
                    f"topology resolution/provider set is invalid: {name}"
                )
        for component, resolved in status["resolved"].items():
            expected_resolved = (
                {"path", "head", "dirty"}
                if historical_record
                else historical_coordinate_keys
                if coordinate_historical_record
                else {
                    "path",
                    "checkout_path",
                    "checkout",
                    "repository",
                    "branch",
                    "source",
                    "head",
                    "dirty",
                }
            )
            if (
                (
                    not historical_record
                    and component not in status["providers"].values()
                )
                or not isinstance(resolved, dict)
                or set(resolved) != expected_resolved
                or not isinstance(resolved.get("path"), str)
                or not Path(resolved["path"]).is_absolute()
                or not historical_record
                and (
                    not isinstance(resolved.get("checkout_path"), str)
                    or not Path(resolved["checkout_path"]).is_absolute()
                    or not isinstance(resolved.get("checkout"), str)
                    or (
                        not coordinate_historical_record
                        and (
                            not isinstance(resolved.get("repository"), str)
                            or not isinstance(resolved.get("branch"), str)
                        )
                    )
                    or not isinstance(resolved.get("source"), str)
                )
                or not isinstance(resolved.get("head"), str)
                or not re.fullmatch(r"[0-9a-f]{40,64}", resolved["head"])
                or not isinstance(resolved.get("dirty"), bool)
            ):
                raise WorkspaceError(f"topology resolution status is invalid: {name}")
            if (
                not historical_record
                and not coordinate_historical_record
                and not (retired_content_record and component == "content-1x")
            ):
                provider = self.manifest.by_name[component]
                checkout_path = Path(resolved["checkout_path"]).resolve(
                    strict=False
                )
                expected_path = (
                    checkout_path
                    if provider.source == "."
                    else checkout_path.joinpath(
                        *PurePosixPath(provider.source).parts
                    )
                ).resolve(strict=False)
                if (
                    resolved["checkout"] != provider.checkout_name
                    or resolved["repository"] != provider.repository
                    or resolved["branch"] != provider.branch
                    or resolved["source"] != provider.source
                    or Path(resolved["path"]).resolve(strict=False)
                    != expected_path
                ):
                    raise WorkspaceError(
                        f"topology component identity is invalid: {name}/{component}"
                    )
        supervisor = status.get("supervisor")
        control = status.get("control")
        current_control = control is not None
        if current_control and (
            not isinstance(control, dict)
            or set(control) != {"socket", "generation", "lease"}
            or not isinstance(control.get("generation"), str)
            or not re.fullmatch(r"[0-9a-f]{64}", control["generation"])
            or control.get("socket")
            != str(control_socket_path(root, control["generation"]))
            or not isinstance(control.get("lease"), dict)
            or set(control["lease"]) != {"device", "inode"}
            or not all(
                isinstance(value, int)
                and not isinstance(value, bool)
                and value >= 0
                for value in control["lease"].values()
            )
        ):
            raise WorkspaceError(f"topology control identity is invalid: {name}")
        process_keys = (
            {"pid", "start_time", "generation"}
            if current_control
            else {"pid", "start_time"}
        )
        if (
            not isinstance(supervisor, dict)
            or set(supervisor) != process_keys
            or not isinstance(supervisor.get("pid"), int)
            or isinstance(supervisor.get("pid"), bool)
            or supervisor["pid"] <= 0
            or not isinstance(supervisor.get("start_time"), str)
            or not supervisor["start_time"].isdigit()
            or current_control
            and supervisor.get("generation") != control["generation"]
        ):
            raise WorkspaceError(f"topology supervisor status is invalid: {name}")
        control_reachable = bool(
            current_control
            and self._topology_control_request(name, control, "status")
        )
        process_tree_active = self._topology_process_tree_active(
            root, control if current_control else None
        )
        runtime = status.get("runtime")
        runtime_active = False
        if current_runtime_record:
            if not current_control:
                raise WorkspaceError(f"topology runtime identity is invalid: {name}")
            expected_runtime_path = root / "generations" / control["generation"]
            runtime_keys = {
                "schema_version",
                "generation",
                "path",
                "manifest_sha256",
                "lease",
                "external_state",
                "mutable_state_outputs",
            }
            if topology_schema == TOPOLOGY_STATUS_SCHEMA_VERSION:
                runtime_keys.add("mutable_state_output_identities")
            if (
                not isinstance(runtime, dict)
                or set(runtime) != runtime_keys
                or runtime.get("schema_version")
                != RUNTIME_GENERATION_SCHEMA_VERSION
                or runtime.get("generation") != control["generation"]
                or runtime.get("path") != str(expected_runtime_path)
                or not isinstance(runtime.get("manifest_sha256"), str)
                or not re.fullmatch(r"[0-9a-f]{64}", runtime["manifest_sha256"])
                or runtime.get("external_state") != status.get("state")
                or runtime.get("mutable_state_outputs")
                != (
                    [
                        str(
                            Path(status["state"])
                            / "tmp"
                            / "runtime-assets"
                            / control["generation"]
                        )
                    ]
                    if status.get("state") is not None
                    else []
                )
                or topology_schema == TOPOLOGY_STATUS_SCHEMA_VERSION
                and (
                    not isinstance(
                        runtime.get("mutable_state_output_identities"), list
                    )
                    or len(runtime["mutable_state_output_identities"])
                    != len(runtime["mutable_state_outputs"])
                    or any(
                        not isinstance(identity, dict)
                        or set(identity) != {"device", "inode"}
                        or any(
                            not isinstance(value, int)
                            or isinstance(value, bool)
                            or value < 0
                            for value in identity.values()
                        )
                        for identity in runtime[
                            "mutable_state_output_identities"
                        ]
                    )
                )
                or not isinstance(runtime.get("lease"), dict)
                or set(runtime["lease"]) != {"device", "inode"}
                or not all(
                    isinstance(value, int)
                    and not isinstance(value, bool)
                    and value >= 0
                    for value in runtime["lease"].values()
                )
            ):
                raise WorkspaceError(f"topology runtime identity is invalid: {name}")
            if mutable_state_cleanup is not None and (
                mutable_state_cleanup.get("generation")
                != control["generation"]
                or [entry["path"] for entry in mutable_state_cleanup["entries"]]
                != runtime["mutable_state_outputs"]
                or [
                    entry["identity"]
                    for entry in mutable_state_cleanup["entries"]
                ]
                != runtime.get("mutable_state_output_identities")
            ):
                raise WorkspaceError(
                    f"topology mutable state cleanup identity is invalid: {name}"
                )
            marker = expected_runtime_path / MANAGED_MARKER
            manifest = expected_runtime_path / RUNTIME_GENERATION_MANIFEST
            if (
                expected_runtime_path.is_symlink()
                or not expected_runtime_path.is_dir()
                or marker.is_symlink()
                or not marker.is_file()
                or load_json(marker)
                != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "immutable-runtime-generation",
                }
                or manifest.is_symlink()
                or not manifest.is_file()
                or _file_digest(manifest, "runtime generation manifest")
                != runtime["manifest_sha256"]
            ):
                raise WorkspaceError(f"topology runtime publication is invalid: {name}")
            runtime_manifest = load_json(manifest)
            expected_identity = {
                "kind": "topology",
                "name": name,
                "stack": status["stack"],
                "providers": status["providers"],
            }
            manifest_services = (
                runtime_manifest.get("services")
                if isinstance(runtime_manifest, dict)
                else None
            )
            source_trees = (
                runtime_manifest.get("source_trees")
                if isinstance(runtime_manifest, dict)
                else None
            )
            input_digests = (
                runtime_manifest.get("input_digests")
                if isinstance(runtime_manifest, dict)
                else None
            )
            build = (
                runtime_manifest.get("build")
                if isinstance(runtime_manifest, dict)
                else None
            )
            entries = (
                runtime_manifest.get("entries")
                if isinstance(runtime_manifest, dict)
                else None
            )
            state_output_entries = (
                runtime_manifest.get("mutable_state_output_entries")
                if isinstance(runtime_manifest, dict)
                else None
            )
            manifest_keys = {
                "schema_version",
                "generation",
                "profile",
                "identity",
                "services",
                "resolved",
                "source_trees",
                "input_digests",
                "build",
                "external_state",
                "mutable_state_outputs",
                "mutable_state_output_entries",
                "entries",
            }
            if topology_schema == TOPOLOGY_STATUS_SCHEMA_VERSION:
                manifest_keys.add("mutable_state_output_identities")
            if (
                not isinstance(runtime_manifest, dict)
                or set(runtime_manifest) != manifest_keys
                or runtime_manifest.get("schema_version")
                != RUNTIME_GENERATION_SCHEMA_VERSION
                or runtime_manifest.get("generation") != control["generation"]
                or runtime_manifest.get("profile") != status["profile"]
                or runtime_manifest.get("identity") != expected_identity
                or runtime_manifest.get("resolved") != status["resolved"]
                or runtime_manifest.get("external_state") != status.get("state")
                or runtime_manifest.get("mutable_state_outputs")
                != runtime["mutable_state_outputs"]
                or topology_schema == TOPOLOGY_STATUS_SCHEMA_VERSION
                and runtime_manifest.get("mutable_state_output_identities")
                != runtime["mutable_state_output_identities"]
                or not isinstance(manifest_services, list)
                or not manifest_services
                or len(manifest_services) != len(set(manifest_services))
                or not set(manifest_services) <= set(TOPOLOGY_SERVICES)
                or not isinstance(source_trees, dict)
                or set(source_trees) != set(status["resolved"])
                or any(
                    not isinstance(value, str)
                    or not re.fullmatch(r"[0-9a-f]{40,64}", value)
                    for value in source_trees.values()
                )
                or not isinstance(input_digests, dict)
                or not input_digests
                or any(
                    not isinstance(key, str)
                    or not key
                    or not isinstance(value, str)
                    or not re.fullmatch(r"[0-9a-f]{64}", value)
                    for key, value in input_digests.items()
                )
                or not isinstance(build, dict)
                or set(build) != {"root", "metadata_sha256"}
                or build.get("root") != status["build_root"]
                or (
                    build.get("metadata_sha256") is not None
                    and (
                        not isinstance(build.get("metadata_sha256"), str)
                        or not re.fullmatch(
                            r"[0-9a-f]{64}", build["metadata_sha256"]
                        )
                    )
                )
                or not isinstance(entries, list)
                or not entries
                or any(not isinstance(entry, dict) for entry in entries)
                or not isinstance(state_output_entries, list)
                or (
                    bool(runtime["mutable_state_outputs"])
                    != bool(state_output_entries)
                )
                or any(
                    not isinstance(entry, dict)
                    for entry in state_output_entries
                )
            ):
                raise WorkspaceError(
                    f"topology runtime manifest identity is invalid: {name}"
                )
            try:
                runtime_active = bound_lease_locked(
                    expected_runtime_path / RUNTIME_GENERATION_LEASE,
                    control["generation"],
                    runtime["lease"],
                )
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect topology runtime generation lease: {error}"
                ) from error
        elif runtime is not None:
            raise WorkspaceError(f"historical topology runtime is invalid: {name}")
        supervisor_local = bool(
            not current_control and self._recorded_process_running(supervisor)
        )
        if control_reachable or supervisor_local:
            supervisor_liveness = "live"
        elif process_tree_active:
            supervisor_liveness = "unreachable"
        elif status.get("stopped_at") is not None:
            supervisor_liveness = "exited"
        else:
            supervisor_liveness = "stale"
        supervisor_running = supervisor_liveness in {"live", "unreachable"}
        supervisor["running"] = supervisor_running
        supervisor["liveness"] = supervisor_liveness
        endpoint = status.get("endpoint")
        if endpoint is not None:
            fingerprint = endpoint.get("fingerprint") if isinstance(endpoint, dict) else None
            if (
                not isinstance(endpoint, dict)
                or set(endpoint) != {"host", "port", "fingerprint"}
                or endpoint.get("host") != "127.0.0.1"
                or not isinstance(endpoint.get("port"), int)
                or isinstance(endpoint.get("port"), bool)
                or not 1 <= endpoint["port"] <= 65535
                or (
                    fingerprint is not None
                    and (
                        not isinstance(fingerprint, str)
                        or len(fingerprint) != 64
                        or any(character not in "0123456789abcdef" for character in fingerprint)
                    )
                )
            ):
                raise WorkspaceError(f"topology endpoint status is invalid: {name}")
        port_reservation = status.get("port_reservation")
        if port_reservation is not None:
            reservation_port = (
                port_reservation.get("port")
                if isinstance(port_reservation, dict)
                else None
            )
            try:
                validate_port_reservation(
                    port_reservation,
                    expected_path=(
                        self._lease_namespace
                        / PORT_RESERVATION_DIRECTORY
                        / f"{reservation_port}-{port_reservation.get('generation')}.lease"
                    ),
                )
                reservation_retained = port_reservation_locked(port_reservation)
            except PortReservationError as error:
                raise WorkspaceError(
                    f"topology port reservation status is invalid: {name}"
                ) from error
            if (
                endpoint is None
                or port_reservation["port"] != endpoint["port"]
                or port_reservation["topology"] != name
                or not current_control
                or port_reservation["generation"] != control["generation"]
            ):
                raise WorkspaceError(
                    f"topology port reservation status is invalid: {name}"
                )
        else:
            reservation_retained = False
        services = status.get("services")
        if (
            not isinstance(services, dict)
            or (not services and not status.get("error"))
            or not set(services) <= set(TOPOLOGY_SERVICES)
        ):
            raise WorkspaceError(f"topology service status is invalid: {name}")
        if topology_schema == TOPOLOGY_STATUS_SCHEMA_VERSION:
            self._validate_topology_state_policy(name, root, status, control)
        if not historical_record and "client" in services and "sound" not in status:
            raise WorkspaceError(f"topology status is invalid: {name}")
        for service in services.values():
            if (
                not isinstance(service, dict)
                or set(service)
                != process_keys | {"status", "exit_code", "log", "cwd"}
                or not isinstance(service.get("pid"), int)
                or isinstance(service.get("pid"), bool)
                or service["pid"] <= 0
                or not isinstance(service.get("start_time"), str)
                or not service["start_time"].isdigit()
                or service.get("status") not in {"starting", "running", "exited"}
                or (
                    service.get("exit_code") is not None
                    and (
                        not isinstance(service.get("exit_code"), int)
                        or isinstance(service.get("exit_code"), bool)
                    )
                )
                or not isinstance(service.get("log"), str)
                or not Path(service["log"]).is_absolute()
                or not isinstance(service.get("cwd"), str)
                or not Path(service["cwd"]).is_absolute()
                or current_control
                and service.get("generation") != control["generation"]
            ):
                raise WorkspaceError(f"topology service status is invalid: {name}")
            service_local = bool(
                not current_control and self._recorded_process_running(service)
            )
            if service.get("status") == "exited":
                service_liveness = "exited"
            elif control_reachable or service_local:
                service_liveness = "live"
            elif process_tree_active:
                service_liveness = "unreachable"
            elif status.get("stopped_at") is not None:
                service_liveness = "exited"
            else:
                service_liveness = "stale"
            service["liveness"] = service_liveness
            service["running"] = service_liveness in {"live", "unreachable"}
            if (
                service.get("status") in {"starting", "running"}
                and service_liveness in {"stale", "exited"}
            ):
                service["status"] = service_liveness
        if (
            not status.get("error")
            and ("server" in services) != (endpoint is not None)
            or (
                status["ready"]
                and endpoint is not None
                and endpoint["fingerprint"] is None
            )
        ):
            raise WorkspaceError(f"topology endpoint status is invalid: {name}")
        if not supervisor_running:
            status["ready"] = False
        retained = (
            reservation_retained
            or process_tree_active
            or supervisor_running
            or any(service["running"] for service in services.values())
        )
        if current_runtime_record and retained and not runtime_active:
            raise WorkspaceError(
                f"topology runtime generation lease is not retained: {name}"
            )
        if current_runtime_record and (
            not set(services) <= set(manifest_services)
            or status["ready"] and set(services) != set(manifest_services)
        ):
            raise WorkspaceError(
                f"topology runtime manifest services are invalid: {name}"
            )
        if control_reachable:
            safe_action = f"run ./atrinik down {name} from any supported session"
        elif process_tree_active:
            safe_action = (
                "wait for bounded orphan recovery, then retry; if the exact "
                "lease remains retained, preserve it for operator diagnosis"
            )
        else:
            safe_action = f"restart topology {name} or retain its historical record"
        status["observation"] = {
            "control": (
                "reachable" if control_reachable else "unreachable"
                if current_control
                else "legacy"
            ),
            "generation": control["generation"] if current_control else None,
            "process_tree_lease": "retained" if retained else "released",
            "runtime_generation": (
                runtime["generation"] if current_runtime_record else None
            ),
            "runtime_bundle_lease": (
                "retained" if runtime_active else "released"
                if current_runtime_record
                else "historical"
            ),
            "server_state_lease_owner": (
                name if retained and status.get("state") is not None else None
            ),
            "port_reservation": (
                status["endpoint"]["port"]
                if retained and status.get("endpoint") is not None
                else None
            ),
            "repository_layout_lease_owner": (
                name if retained and not current_runtime_record else None
            ),
            "safe_action": safe_action,
        }
        if port_reservation is not None:
            status["observation"]["port_reservation"] = {
                "port": port_reservation["port"],
                "owner": port_reservation["topology"],
                "generation": port_reservation["generation"],
                "lease": "retained" if reservation_retained else "released",
            }
        return status

    def topology_statuses(self) -> list[dict[str, Any]]:
        self.paths.ensure()
        names = [
            path.name
            for path in sorted(self.paths.topologies.iterdir())
            if path.is_dir()
            and not path.is_symlink()
            and (path / "status.json").is_file()
        ]
        with ThreadPoolExecutor(max_workers=min(8, len(names) or 1)) as executor:
            return list(executor.map(self.topology_status, names))

    @staticmethod
    def _select_topology_port(requested: int | None) -> int:
        if requested is not None and (
            not isinstance(requested, int)
            or isinstance(requested, bool)
            or not 0 <= requested <= 65535
        ):
            raise WorkspaceError("topology port must be between 0 and 65535")
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as candidate:
                candidate.bind(("0.0.0.0", requested or 0))
                return int(candidate.getsockname()[1])
        except OSError as error:
            if requested:
                raise WorkspaceError(
                    f"topology UDP port {requested} is unavailable: {error}"
                ) from error
            raise WorkspaceError(f"cannot allocate a topology UDP port: {error}") from error

    def _reserve_topology_port(
        self, requested: int | None, topology: str, generation: str
    ) -> tuple[int, dict[str, Any]]:
        if requested is not None and (
            not isinstance(requested, int)
            or isinstance(requested, bool)
            or not 0 <= requested <= 65535
        ):
            raise WorkspaceError("topology port must be between 0 and 65535")

        automatic = requested in (None, 0)
        reservation_root = self._lease_namespace

        def conflict(owner: dict[str, Any]) -> None:
            raise WorkspaceError(
                f"topology UDP port {owner['port']} is reserved by topology "
                f"{owner['topology']} generation {owner['generation']}; "
                "retry after its startup transaction completes; if contention "
                f"persists inspect the shared reservation {owner['path']}"
            )

        def validate_known_evidence(port: int) -> None:
            for root in sorted(self.paths.topologies.iterdir()):
                record_path = root / TOPOLOGY_PORT_RESERVATION_RECORD
                if not record_path.exists() and not record_path.is_symlink():
                    continue
                if record_path.is_symlink() or not record_path.is_file():
                    raise WorkspaceError(
                        f"topology port reservation evidence is invalid: {record_path}"
                    )
                value = load_json(record_path)
                if not isinstance(value, dict) or value.get("port") != port:
                    continue
                try:
                    owner = validate_port_reservation(value)
                    if owner["port"] != port:
                        continue
                    expected_path = (
                        reservation_root
                        / PORT_RESERVATION_DIRECTORY
                        / f"{port}-{owner['generation']}.lease"
                    )
                    validate_port_reservation(owner, expected_path=expected_path)
                except PortReservationError as error:
                    raise WorkspaceError(
                        f"topology port reservation evidence is invalid: {record_path}"
                    ) from error
                try:
                    port_reservation_locked(owner)
                except PortReservationError as error:
                    raise WorkspaceError(
                        f"topology port reservation evidence for port {port} "
                        "does not match its exact lease; preserve it for diagnosis"
                    ) from error

        def claim(port: int) -> tuple[int, dict[str, Any]] | None:
            try:
                validate_known_evidence(port)
                transaction, directory_fd, directory, directory_identity = (
                    open_port_transaction(
                        reservation_root,
                        port,
                        root_identity=self._physical_lease_namespace_identity,
                    )
                )
                try:
                    if not try_lock_port_reservation(transaction):
                        if automatic:
                            return None
                        deadline = time.monotonic() + 1
                        while not try_lock_port_reservation(transaction):
                            if time.monotonic() >= deadline:
                                raise WorkspaceError(
                                    f"topology UDP port {port} reservation "
                                    "transaction is busy; retry"
                                )
                            time.sleep(0.01)
                    validate_port_transaction(transaction, directory_fd, port)
                    owner = active_port_reservation(directory_fd, directory, port)
                    if owner is not None:
                        if automatic:
                            return None
                        conflict(owner)
                    self._select_topology_port(port)
                    descriptor, record = create_port_reservation(
                        directory_fd,
                        directory,
                        directory_identity,
                        port=port,
                        topology=topology,
                        generation=generation,
                    )
                    try:
                        validate_port_transaction(transaction, directory_fd, port)
                    except BaseException:
                        os.close(descriptor)
                        raise
                finally:
                    os.close(transaction)
                    os.close(directory_fd)
                return descriptor, record
            except PortReservationError as error:
                raise WorkspaceError(str(error)) from error

        if not automatic:
            reserved = claim(requested)
            assert reserved is not None
            return reserved

        with exclusive_lock(
            reservation_root / "ports.lock",
            "topology automatic port allocation",
        ):
            for _attempt in range(64):
                candidate = self._select_topology_port(None)
                reserved = claim(candidate)
                if reserved is not None:
                    return reserved
        raise WorkspaceError("cannot allocate a unique topology UDP port after 64 attempts")

    @staticmethod
    def _require_client_display() -> None:
        wayland = os.environ.get("WAYLAND_DISPLAY")
        runtime = os.environ.get("XDG_RUNTIME_DIR")
        if wayland and runtime and (Path(runtime) / wayland).is_socket():
            return
        display = os.environ.get("DISPLAY", "")
        if display.startswith(":"):
            display_number = display[1:].split(".", 1)[0]
            if display_number.isdigit() and Path(
                f"/tmp/.X11-unix/X{display_number}"
            ).is_socket():
                return
        raise WorkspaceError(
            "client display forwarding is unavailable; reload or reopen the "
            "devcontainer before starting the client"
        )

    def topology_up(
        self,
        name: str,
        profile_name: str,
        state_name: str | None,
        services: list[str] | None = None,
        port: int | None = None,
        state_mode: str | None = None,
    ) -> dict[str, Any]:
        self.paths.ensure()
        selected_services = self._topology_services(services)
        normalized_mode, normalized_state = self._normalize_topology_state_request(
            state_mode, state_name, selected_services
        )
        scope = self._scope_topology_owner(name)
        if scope is not None:
            policy = scope["state_policy"]
            if (
                profile_name != scope["profile"]["name"]
                or "server" in selected_services
                and (
                    normalized_mode != policy["mode"]
                    or normalized_state != policy["name"]
                )
            ):
                raise WorkspaceError(
                    f"topology name is reserved by scope {scope['name']} with exact profile and state coordinates: {name}"
                )
        with self._resolved_profile_operation(
            profile_name,
            set(selected_services),
            f"prepare topology {name}",
        ):
            with self._resource_locks(
                [
                    self._lease_request(
                        "topology", name, "exclusive", f"prepare topology {name}"
                    )
                ],
            ):
                return self._topology_up(
                    name,
                    profile_name,
                    normalized_state,
                    services,
                    port,
                    normalized_mode,
                )

    def _topology_resolved_status(
        self, profile_name: str, selected: dict[str, Path]
    ) -> dict[str, dict[str, Any]]:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        checkout_states = self._selected_checkout_states(
            profile, selected, include_dirty=True
        )
        return {
            stack.providers[role].name: {
                "path": str(path),
                "checkout_path": str(
                    checkout_states[stack.providers[role].checkout_name]["path"]
                ),
                "checkout": stack.providers[role].checkout_name,
                "repository": stack.providers[role].repository,
                "branch": stack.providers[role].branch,
                "source": stack.providers[role].source,
                "head": checkout_states[stack.providers[role].checkout_name]["head"],
                "dirty": checkout_states[stack.providers[role].checkout_name]["dirty"],
            }
            for role, path in sorted(selected.items())
        }

    def _topology_up(
        self,
        name: str,
        profile_name: str,
        state_name: str | None,
        services: list[str] | None = None,
        port: int | None = None,
        state_mode: str | None = None,
    ) -> dict[str, Any]:
        selected_services = self._topology_services(services)
        state_mode, state_name = self._normalize_topology_state_request(
            state_mode, state_name, selected_services
        )
        if "client" in selected_services:
            client_launch_label(profile_name, name)
        if "server" not in selected_services and port is not None:
            raise WorkspaceError("--port requires the server service")
        self._require_classic_contracts(profile_name, set(selected_services))
        topology_root = self._topology_directory(name, create=True)
        operation_lock = topology_root / "operation.lock"
        with exclusive_lock(
            operation_lock, f"topology {name} operation", nonblocking=True
        ):
            process_tree_path = topology_root / TOPOLOGY_PROCESS_TREE_LEASE
            status_path = topology_root / "status.json"
            startup_error_path = topology_root / "startup-error.json"
            self._recover_runtime_state_output_transaction(
                topology_root, name
            )
            if status_path.is_file():
                if status_path.is_symlink():
                    raise WorkspaceError(f"topology status is invalid: {name}")
                previous = self.topology_status(name)
                if previous.get("inert_historical_record"):
                    raise WorkspaceError(
                        f"topology {name} is an inert pre-migration record; "
                        "choose a new topology name"
                    )
                if previous["supervisor"]["running"] or any(
                    service["running"] for service in previous["services"].values()
                ):
                    raise WorkspaceError(f"topology is already running: {name}")
                previous_policy = previous.get("state_policy")
                if (
                    isinstance(previous_policy, dict)
                    and previous_policy.get("mode") == "temporary"
                    and previous_policy.get("lifecycle")
                    not in {"removed", "promoted"}
                ):
                    raise WorkspaceError(
                        f"topology {name} retains temporary generation state; "
                        "promote it or complete its safe cleanup before restart"
                    )
                previous_runtime = previous.get("runtime")
                if (
                    (
                        not isinstance(previous_policy, dict)
                        or previous_policy.get("mode") != "temporary"
                    )
                    and isinstance(previous_runtime, dict)
                    and previous_runtime.get("mutable_state_outputs")
                ):
                    identities = previous_runtime.get(
                        "mutable_state_output_identities"
                    )
                    if not isinstance(identities, list) or not identities:
                        raise WorkspaceError(
                            f"topology {name} retains legacy mutable state "
                            "output without exact ownership evidence; preserve "
                            "the historical record and choose a new topology name"
                        )
                    else:
                        previous = self._cleanup_topology_mutable_state_outputs(
                            previous
                        )

            if "client" in selected_services:
                self._require_client_display()

            selected = self._resolve_build_profile(
                profile_name, set(selected_services)
            )
            required = set(selected)
            targets = [
                service
                for service in ("client", "server")
                if service in selected_services
            ]
            profile = self._load_profile(profile_name, require_file=False)
            selected_stack = self.manifest.stack(profile["stack"])
            providers = {
                role: selected_stack.providers[role].name
                for role in sorted(required)
            }

            with ExitStack() as stack:
                process_tree_fd = open_regular_file(
                    process_tree_path,
                    os.O_RDWR | os.O_CREAT,
                    "topology process-tree lease",
                )
                process_tree_owner = [process_tree_fd]
                stack.callback(
                    lambda: os.close(process_tree_owner.pop())
                    if process_tree_owner
                    else None
                )
                try:
                    fcntl.flock(
                        process_tree_fd, fcntl.LOCK_EX | fcntl.LOCK_NB
                    )
                except BlockingIOError as error:
                    raise WorkspaceError(
                        f"topology is already running: {name}"
                    ) from error
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot lock topology process-tree lease: {error}"
                    ) from error
                if holders_exist(process_tree_fd, exclude=(os.getpid(),)):
                    raise WorkspaceError(f"topology is already running: {name}")
                for _attempt in range(16):
                    generation = secrets.token_hex(32)
                    control_path = control_socket_path(topology_root, generation)
                    if not control_path.exists() and not control_path.is_symlink():
                        break
                else:
                    raise WorkspaceError(
                        "cannot allocate a unique topology control endpoint"
                    )
                control_directory = control_path.parent
                control_directory.mkdir(mode=0o700, exist_ok=True)
                control_metadata = control_directory.lstat()
                if (
                    not stat.S_ISDIR(control_metadata.st_mode)
                    or stat.S_IMODE(control_metadata.st_mode) != 0o700
                    or control_metadata.st_uid != os.geteuid()
                ):
                    raise WorkspaceError(
                        f"topology control directory is invalid: {control_directory}"
                    )

                endpoint: dict[str, Any] | None = None
                port_reservation: dict[str, Any] | None = None
                port_reservation_owner: list[int] = []
                if "server" in selected_services:
                    port_reservation_fd, port_reservation = (
                        self._reserve_topology_port(port, name, generation)
                    )
                    port_reservation_owner.append(port_reservation_fd)
                    stack.callback(
                        lambda: os.close(port_reservation_owner.pop())
                        if port_reservation_owner
                        else None
                    )
                    endpoint = {
                        "host": "127.0.0.1",
                        "port": port_reservation["port"],
                    }
                    atomic_json(
                        topology_root / TOPOLOGY_PORT_RESERVATION_RECORD,
                        port_reservation,
                    )

                state_location: Path | None = None
                state_lock: TextIO | None = None
                state_expected_identity: dict[str, int] | None = None
                if "server" in selected_services and state_mode != "temporary":
                    assert state_name is not None
                    state_location = self._state_location(state_name)
                    state_lock = stack.enter_context(
                        self._topology_state_lock(
                            state_location, preparing_topology=name
                        )
                    )
                    if state_location.exists() or state_location.is_symlink():
                        state_expected_identity = self._state_identity(state_location)

                root = self._build_resolved(
                    "topology", profile_name, False, targets, selected
                )
                resolved_status = self._topology_resolved_status(
                    profile_name, selected
                )
                implementation = (
                    self._state_implementation(
                        selected_stack.name, providers, resolved_status
                    )
                    if "server" in selected_services
                    else None
                )
                state: Path | None = None
                state_policy: dict[str, Any] | None = None
                state_directory_fd: int | None = None
                temporary_state_owner: list[
                    tuple[
                        Path,
                        TextIO,
                        dict[str, int],
                        dict[str, int],
                        int,
                        dict[str, str],
                    ]
                ] = []
                if "server" in selected_services:
                    assert implementation is not None
                    if state_mode == "temporary":
                        server_provider = providers["server"]
                        state, state_policy = self._create_temporary_state(
                            topology_root,
                            name,
                            profile_name,
                            generation,
                            selected["server"],
                            implementation,
                            resolved_status[server_provider],
                        )
                        state_location = state
                        try:
                            state_lock = stack.enter_context(
                                self._topology_state_lock(
                                    state_location,
                                    preparing_topology=name,
                                    physical_identity=False,
                                )
                            )
                        except BaseException:
                            with exclusive_lock(
                                Path(f"{state_location}.lock"),
                                f"temporary topology state {state_location}",
                                nonblocking=True,
                            ) as rollback_lease:
                                rollback_metadata = os.fstat(
                                    rollback_lease.fileno()
                                )
                                self._rollback_temporary_state_creation(
                                    state_location,
                                    rollback_lease,
                                    state_policy["identity"],
                                    {
                                        "device": rollback_metadata.st_dev,
                                        "inode": rollback_metadata.st_ino,
                                    },
                                    implementation=state_policy[
                                        "implementation"
                                    ],
                                )
                            raise
                    else:
                        assert state_name is not None and state_location is not None
                        state, state_directory_fd = self._prepared_state_path(
                            state_name,
                            selected["server"],
                            state_location,
                            implementation,
                            state_expected_identity,
                        )
                        stack.callback(os.close, state_directory_fd)
                        opened_state = os.fstat(state_directory_fd)
                        state_lock.bind(
                            {
                                "device": opened_state.st_dev,
                                "inode": opened_state.st_ino,
                            }
                        )
                        state_policy = self._persistent_state_policy(
                            state_name, state, implementation
                        )
                    assert state_lock is not None
                    lock_metadata = os.fstat(state_lock.fileno())
                    if (
                        not stat.S_ISREG(lock_metadata.st_mode)
                        or lock_metadata.st_nlink != 1
                    ):
                        raise WorkspaceError(
                            f"server state lease identity is invalid: {state}.lock"
                        )
                    state_policy = {
                        **state_policy,
                        "lease_identity": {
                            "device": lock_metadata.st_dev,
                            "inode": lock_metadata.st_ino,
                        },
                    }
                    try:
                        visible_lock = Path(f"{state}.lock").stat(
                            follow_symlinks=False
                        )
                    except OSError as error:
                        raise WorkspaceError(
                            f"server state lease changed before publication: {state}.lock"
                        ) from error
                    if (
                        visible_lock.st_dev,
                        visible_lock.st_ino,
                    ) != (
                        lock_metadata.st_dev,
                        lock_metadata.st_ino,
                    ):
                        raise WorkspaceError(
                            f"server state lease changed before publication: {state}.lock"
                        )
                    if state_directory_fd is None:
                        state_directory_fd = self._open_validated_state_directory(
                            state,
                            implementation,
                            write_implementation=False,
                        )
                        stack.callback(os.close, state_directory_fd)
                    if state_mode == "temporary":
                        temporary_state_owner.append(
                            (
                                state,
                                state_lock.path_lock,
                                state_policy["identity"],
                                state_policy["lease_identity"],
                                state_directory_fd,
                                state_policy["implementation"],
                            )
                        )
                        stack.callback(
                            lambda: self._rollback_temporary_state_creation(
                                *temporary_state_owner.pop()
                            )
                            if temporary_state_owner
                            else None
                        )
                    state_metadata = os.fstat(state_directory_fd)
                    if state_policy["identity"] != {
                        "device": state_metadata.st_dev,
                        "inode": state_metadata.st_ino,
                    }:
                        raise WorkspaceError(
                            f"server state identity changed before runtime publication: {state}"
                        )
                build_lock = stack.enter_context(
                    self._profile_build_lock(root, profile_name)
                )
                build_metadata = load_json(root / BUILD_METADATA)
                sound_status = (
                    build_metadata.get("sound")
                    if isinstance(build_metadata, dict)
                    else None
                )
                sound_root: Path | None = None
                if "client" in selected_services:
                    try:
                        validated_sound = validate_sound_record(sound_status)
                    except WorkspaceError as error:
                        raise WorkspaceError(
                            f"build sound metadata is invalid for profile {profile_name}"
                        ) from error
                    if validated_sound["mode"] != profile["sound_mode"]:
                        raise WorkspaceError(
                            f"profile {profile_name} sound mode does not match build metadata"
                        )
                    sound_root = Path(validated_sound["root"])
                    if validated_sound["mode"] == PLAYTEST_MODE:
                        inputs = clean_source_inputs(selected["sound"])
                        verified = verify_playtest_tree(
                            selected["sound"], sound_root, inputs
                        )
                        if verified != validated_sound:
                            raise WorkspaceError(
                                f"profile {profile_name} local-playtest sound "
                                "record changed before topology preparation"
                            )
                    elif validated_sound["mode"] == RELEASED_MODE:
                        coordinates = validate_release_coordinates(
                            profile["sound_release"]
                        )
                        verified = verify_release_tree(sound_root, coordinates)
                        if verified != validated_sound:
                            raise WorkspaceError(
                                f"profile {profile_name} released sound record "
                                "changed before topology preparation"
                            )
                    elif sound_root.resolve() != selected["sound"].resolve():
                        raise WorkspaceError(
                            f"profile {profile_name} source sound root changed before topology preparation"
                        )
                (
                    generation_root,
                    runtime_lock_fd,
                    runtime_record,
                    state_output_fd,
                ) = (
                    self._publish_runtime_generation(
                        topology_root,
                        generation,
                        profile_name,
                        root,
                        selected,
                        resolved_status,
                        selected_services,
                        identity={
                            "kind": "topology",
                            "name": name,
                            "stack": selected_stack.name,
                            "providers": providers,
                        },
                        state=state,
                        state_directory_fd=state_directory_fd,
                        sound_root=sound_root,
                    )
                )
                if state_output_fd is not None:
                    stack.callback(os.close, state_output_fd)
                published_generation_owner = [generation_root]
                stack.callback(
                    lambda: remove_owned_tree(published_generation_owner.pop())
                    if published_generation_owner
                    else None
                )
                published_state_output_owner = [
                    (
                        Path(f"/proc/self/fd/{state_directory_fd}")
                        / "tmp"
                        / "runtime-assets"
                        / generation
                        if state_directory_fd is not None
                        else Path(runtime_record["mutable_state_outputs"][0])
                    )
                ] if runtime_record["mutable_state_outputs"] else []

                def rollback_published_state_output() -> None:
                    if not published_state_output_owner:
                        return
                    assert state_directory_fd is not None
                    output = published_state_output_owner.pop()
                    try:
                        self._rollback_runtime_state_output_transaction(
                            topology_root,
                            output,
                            generation,
                            state_directory_fd,
                            runtime_record[
                                "mutable_state_output_identities"
                            ][0],
                        )
                    except BaseException:
                        temporary_state_owner.clear()
                        raise

                stack.callback(rollback_published_state_output)
                runtime_lock_owner = [runtime_lock_fd]
                stack.callback(
                    lambda: os.close(runtime_lock_owner.pop())
                    if runtime_lock_owner
                    else None
                )
                service_specs: dict[str, dict[str, Any]] = {}
                if "server" in selected_services:
                    server_runtime = generation_root / "server"
                    executable = server_runtime / "atrinik-server"
                    service_specs["server"] = {
                        "command": [
                            str(executable),
                            f"--port_quic={endpoint['port']}",
                            "--port_mapping=off",
                            "--stun_server=off",
                            *(
                                [f"--datapath=/proc/self/fd/{state_directory_fd}"]
                                if state_directory_fd is not None
                                else []
                            ),
                            "--assetspath="
                            + (
                                f"/proc/self/fd/{state_output_fd}"
                                if state_output_fd is not None
                                else runtime_record["mutable_state_outputs"][0]
                            ),
                            "--no_console",
                        ],
                        "cwd": str(server_runtime),
                        "log": str(topology_root / "server.log"),
                    }
                if "client" in selected_services:
                    client_runtime = generation_root / "client"
                    executable = client_runtime / "atrinik"
                    if not executable.is_file():
                        raise WorkspaceError(f"client executable is missing: {executable}")
                    client_config = topology_root / "client-config"
                    if client_config.exists() or client_config.is_symlink():
                        if not client_config.is_dir() or client_config.is_symlink():
                            raise WorkspaceError(
                                f"client configuration path is invalid: {client_config}"
                            )
                    else:
                        client_config.mkdir()
                    service_specs["client"] = {
                        "command": [str(executable)],
                        "cwd": str(client_runtime),
                        "log": str(topology_root / "client.log"),
                        "environment": {
                            "ATRINIK_CONFIG_DIR": str(client_config.resolve())
                        },
                    }
                if state is not None and state_directory_fd is not None:
                    pinned = os.fstat(state_directory_fd)
                    try:
                        visible = state.stat(follow_symlinks=False)
                        canonical = self._canonical_state_path(state)
                    except OSError as error:
                        raise WorkspaceError(
                            f"server state changed before topology launch: {state}"
                        ) from error
                    if canonical != state or (pinned.st_dev, pinned.st_ino) != (
                        visible.st_dev,
                        visible.st_ino,
                    ):
                        raise WorkspaceError(
                            f"server state changed before topology launch: {state}"
                        )
                # All fallible preparation is complete. Retire the stopped
                # record and bind/publish the new generation without exposing
                # old status against rewritten lease contents.
                status_path.unlink(missing_ok=True)
                lease_identity = initialize_lease(process_tree_fd, generation)
                spec: dict[str, Any] = {
                    "schema_version": TOPOLOGY_STATUS_SCHEMA_VERSION,
                    "name": name,
                    "profile": profile_name,
                    "stack": selected_stack.name,
                    "providers": {
                        role: selected_stack.providers[role].name
                        for role in sorted(required)
                    },
                    "dependencies": sorted(required),
                    "state": str(state_location) if state_location else None,
                    "state_policy": state_policy,
                    "build_root": str(root),
                    "resolved": resolved_status,
                    "endpoint": endpoint,
                    "control": {
                        "socket": str(control_path),
                        "generation": generation,
                        "lease": lease_identity,
                    },
                    "runtime": runtime_record,
                    "services": service_specs,
                }
                if port_reservation is not None:
                    spec["port_reservation"] = port_reservation
                if sound_status is not None:
                    spec["sound"] = sound_status
                spec_path = topology_root / "spec.json"
                try:
                    control_mode = control_path.lstat().st_mode
                except FileNotFoundError:
                    pass
                else:
                    raise WorkspaceError(
                        f"topology control endpoint already exists: {control_path}"
                    )
                atomic_json(spec_path, spec)
                startup_error_path.unlink(missing_ok=True)

                supervisor_log_path = topology_root / "supervisor.log"
                supervisor_log = os.fdopen(
                    open_regular_file(
                        supervisor_log_path,
                        os.O_WRONLY | os.O_CREAT | os.O_APPEND,
                        "topology supervisor log",
                    ),
                    "ab",
                    buffering=0,
                )
                try:
                    command = [
                        sys.executable,
                        "-m",
                        "atrinik_workspace.supervisor",
                        "--daemonize",
                        "--spec",
                        str(spec_path),
                    ]
                    inherited_locks: list[int] = []
                    if state_lock is not None:
                        command.extend(["--lock-fd", str(state_lock.fileno())])
                        inherited_locks.append(state_lock.fileno())
                        if state_lock.physical_lock is not None:
                            command.extend(
                                [
                                    "--physical-state-lock-fd",
                                    str(state_lock.physical_lock.fileno()),
                                ]
                            )
                            inherited_locks.append(
                                state_lock.physical_lock.fileno()
                            )
                    if state_directory_fd is not None:
                        command.extend(
                            ["--state-directory-fd", str(state_directory_fd)]
                        )
                        inherited_locks.append(state_directory_fd)
                    if state_output_fd is not None:
                        command.extend(
                            ["--state-output-fd", str(state_output_fd)]
                        )
                        inherited_locks.append(state_output_fd)
                    command.extend(
                        ["--runtime-lock-fd", str(runtime_lock_fd)]
                    )
                    inherited_locks.append(runtime_lock_fd)
                    command.extend(
                        ["--process-tree-fd", str(process_tree_fd)]
                    )
                    inherited_locks.append(process_tree_fd)
                    if port_reservation_owner:
                        command.extend(
                            [
                                "--port-reservation-fd",
                                str(port_reservation_owner[0]),
                            ]
                        )
                        inherited_locks.append(port_reservation_owner[0])
                    environment = os.environ.copy()
                    source_root = str(Path(__file__).resolve().parents[1])
                    python_path = environment.get("PYTHONPATH")
                    environment["PYTHONPATH"] = (
                        source_root
                        if not python_path
                        else source_root + os.pathsep + python_path
                    )
                    process = subprocess.Popen(
                        command,
                        cwd=self.paths.repository,
                        env=environment,
                        stdin=subprocess.DEVNULL,
                        stdout=supervisor_log,
                        stderr=subprocess.STDOUT,
                        start_new_session=True,
                        pass_fds=tuple(inherited_locks),
                    )
                    published_generation_owner.clear()
                    published_state_output_owner.clear()
                    temporary_state_owner.clear()
                    os.close(process_tree_owner.pop())
                    if port_reservation_owner:
                        os.close(port_reservation_owner.pop())
                    os.close(runtime_lock_owner.pop())
                except OSError as error:
                    raise WorkspaceError(f"cannot start topology supervisor: {error}") from error
                finally:
                    supervisor_log.close()

                deadline = time.monotonic() + 45
                while time.monotonic() < deadline:
                    if startup_error_path.is_file():
                        if startup_error_path.is_symlink():
                            raise WorkspaceError(
                                f"topology startup error is invalid: {name}"
                            )
                        failure = load_json(startup_error_path)
                        if (
                            not isinstance(failure, dict)
                            or set(failure) != {"error"}
                            or not isinstance(failure.get("error"), str)
                        ):
                            raise WorkspaceError(
                                f"topology startup error is invalid: {name}"
                            )
                        raise WorkspaceError(
                            f"topology supervisor failed: {failure['error']}"
                        )
                    if status_path.is_file():
                        status = self.topology_status(name)
                        runtime = status.get("runtime")
                        if (
                            isinstance(runtime, dict)
                            and runtime.get("generation") == generation
                        ):
                            self._clear_runtime_state_output_transaction(
                                topology_root
                            )
                        if status.get("error"):
                            raise WorkspaceError(
                                f"topology supervisor failed: {status['error']}"
                            )
                        if status["supervisor"]["running"] and status["ready"]:
                            process.wait(timeout=2)
                            return status
                        if not status["supervisor"]["running"]:
                            raise WorkspaceError(
                                "topology supervisor exited during startup; inspect "
                                f"{topology_root / 'supervisor.log'}"
                            )
                    if process.poll() not in (None, 0):
                        break
                    time.sleep(0.1)
                raise WorkspaceError(
                    f"topology supervisor failed to start; inspect "
                    f"{topology_root / 'supervisor.log'}"
                )

    def topology_down(
        self, name: str, timeout: float = 15, *, retain_state: bool = False
    ) -> dict[str, Any]:
        root = self._topology_directory(name)
        with exclusive_lock(
            root / "operation.lock", f"topology {name} operation", nonblocking=True
        ):
            status = self.topology_status(name)
            policy = status.get("state_policy")
            if retain_state and (
                not isinstance(policy, dict) or policy.get("mode") != "temporary"
            ):
                raise WorkspaceError(
                    "--retain-state requires a temporary topology state"
                )
            if "control" in status:
                stopped, confirmed_clean = self._controlled_topology_down(
                    name, status, timeout
                )
                if confirmed_clean:
                    stopped = self._cleanup_topology_mutable_state_outputs(stopped)
                return self._finish_temporary_state_down(
                    name, stopped, retain_state, confirmed_clean
                )
            process_tree_path = root / TOPOLOGY_PROCESS_TREE_LEASE
            if process_tree_path.is_symlink():
                raise WorkspaceError(
                    f"topology process-tree lease is invalid: {name}"
                )
            if not process_tree_path.is_file():
                return self._legacy_topology_down(name, status, timeout)
            if not hasattr(os, "O_PATH"):
                raise WorkspaceError(
                    "topology process-tree observation is unavailable on this platform"
                )
            process_tree_fd = open_regular_file(
                process_tree_path,
                os.O_PATH,
                "topology process-tree lease",
            )
            try:
                active_holders = holders_exist(
                    process_tree_fd, exclude=(os.getpid(),)
                )
                if not active_holders:
                    recorded_running = status["supervisor"]["running"] or any(
                        service["running"]
                        for service in status["services"].values()
                    )
                    if recorded_running:
                        return self._legacy_topology_down(name, status, timeout)
                    return self._finish_temporary_state_down(
                        name, status, retain_state, False
                    )
                signal_holders(
                    process_tree_fd, signal.SIGTERM, exclude=(os.getpid(),)
                )
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    current = self.topology_status(name)
                    recorded_running = current["supervisor"]["running"] or any(
                        service["running"]
                        for service in current["services"].values()
                    )
                    if not holders_exist(
                        process_tree_fd, exclude=(os.getpid(),)
                    ) and not recorded_running:
                        return self._finish_temporary_state_down(
                            name, current, retain_state, False
                        )
                    time.sleep(0.1)
                signal_holders(
                    process_tree_fd, signal.SIGKILL, exclude=(os.getpid(),)
                )
                kill_deadline = time.monotonic() + min(max(timeout, 0.1), 2.0)
                while time.monotonic() < kill_deadline:
                    signal_holders(
                        process_tree_fd,
                        signal.SIGKILL,
                        exclude=(os.getpid(),),
                    )
                    current = self.topology_status(name)
                    recorded_running = current["supervisor"]["running"] or any(
                        service["running"]
                        for service in current["services"].values()
                    )
                    if not holders_exist(
                        process_tree_fd, exclude=(os.getpid(),)
                    ) and not recorded_running:
                        return self._finish_temporary_state_down(
                            name, current, retain_state, False
                        )
                    time.sleep(0.05)
            finally:
                os.close(process_tree_fd)
            raise WorkspaceError(
                f"topology did not stop within {timeout:g} seconds: {name}"
            )

    def _cleanup_topology_mutable_state_outputs(
        self, status: dict[str, Any]
    ) -> dict[str, Any]:
        policy = status.get("state_policy")
        runtime = status.get("runtime")
        control = status.get("control")
        if (
            not isinstance(policy, dict)
            or policy.get("mode") == "temporary"
            or not isinstance(runtime, dict)
            or not isinstance(control, dict)
        ):
            return status
        outputs = runtime.get("mutable_state_outputs")
        identities = runtime.get("mutable_state_output_identities")
        if (
            not isinstance(outputs, list)
            or not outputs
            or not isinstance(identities, list)
            or len(identities) != len(outputs)
        ):
            return status
        name = status["name"]
        status_path = self._topology_directory(name) / "status.json"
        cleanup = status.get("mutable_state_cleanup")
        if cleanup is None:
            cleanup = {
                "schema_version": SCHEMA_VERSION,
                "generation": control["generation"],
                "entries": [
                    {
                        "path": value,
                        "identity": identity,
                        "status": "pending",
                    }
                    for value, identity in zip(outputs, identities, strict=True)
                ],
            }
            raw = load_json(status_path)
            if raw.get("control") != status.get("control"):
                raise WorkspaceError(
                    f"topology changed before mutable state cleanup: {name}"
                )
            durable_atomic_json(
                status_path, {**raw, "mutable_state_cleanup": cleanup}
            )
            status = self.topology_status(name)
        entries = cleanup["entries"]
        state = Path(policy["path"])
        descriptor = _open_directory_nofollow(
            state,
            os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
        )
        try:
            opened = os.fstat(descriptor)
            visible = state.stat(follow_symlinks=False)
            identity = policy.get("identity")
            if (
                {"device": opened.st_dev, "inode": opened.st_ino} != identity
                or {"device": visible.st_dev, "inode": visible.st_ino}
                != identity
            ):
                raise WorkspaceError(
                    f"server state identity changed before runtime cleanup: {state}"
                )
            for index, (value, output_identity) in enumerate(
                zip(outputs, identities, strict=True)
            ):
                if entries[index]["status"] == "complete":
                    if self._runtime_state_output_entry_exists(
                        descriptor, control["generation"]
                    ):
                        raise WorkspaceError(
                            "completed server runtime state output reappeared: "
                            f"{value}"
                        )
                    self._finish_runtime_state_output_tombstone(
                        descriptor, control["generation"], output_identity
                    )
                    continue
                output = Path(value)
                try:
                    self._remove_runtime_state_output(
                        output,
                        control["generation"],
                        descriptor,
                        output_identity,
                        keep_tombstone=True,
                    )
                except FileNotFoundError as error:
                    raise WorkspaceError(
                        "server runtime state output ownership evidence is "
                        f"missing before cleanup: {output}"
                    ) from error
                updated_entries = [dict(entry) for entry in entries]
                updated_entries[index]["status"] = "complete"
                cleanup = {**cleanup, "entries": updated_entries}
                raw = load_json(status_path)
                if raw.get("mutable_state_cleanup") != status.get(
                    "mutable_state_cleanup"
                ):
                    raise WorkspaceError(
                        f"topology changed during mutable state cleanup: {name}"
                    )
                durable_atomic_json(
                    status_path, {**raw, "mutable_state_cleanup": cleanup}
                )
                status = self.topology_status(name)
                entries = updated_entries
                if not self._finish_runtime_state_output_tombstone(
                    descriptor, control["generation"], output_identity
                ):
                    raise WorkspaceError(
                        "server runtime state output cleanup tombstone is missing: "
                        f"{output}"
                    )
        finally:
            os.close(descriptor)
        return status

    def _controlled_topology_down(
        self, name: str, status: dict[str, Any], timeout: float
    ) -> tuple[dict[str, Any], bool]:
        root = self._topology_directory(name)
        control = status["control"]
        requested = self._topology_control_request(name, control, "stop")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            current = self.topology_status(name)
            active = self._topology_process_tree_active(root, control)
            if not active and not current["supervisor"]["running"] and not any(
                service["running"] for service in current["services"].values()
            ):
                shutdown = current.get("shutdown")
                confirmed_clean = bool(
                    isinstance(shutdown, dict)
                    and shutdown.get("control_requested") is True
                    and shutdown.get("clean") is True
                    and current.get("error") is None
                )
                return current, confirmed_clean
            time.sleep(0.1)
        if not requested:
            raise WorkspaceError(
                f"topology {name} retains its exact runtime generation but its "
                "supervisor control endpoint is unreachable; wait for bounded "
                "orphan recovery and retry; preserve a retained exact lease "
                "for operator diagnosis"
            )
        raise WorkspaceError(
            f"topology did not stop within {timeout:g} seconds: {name}"
        )

    def _write_temporary_state_policy(
        self,
        name: str,
        status: dict[str, Any],
        policy: dict[str, Any],
    ) -> dict[str, Any]:
        root = self._topology_directory(name)
        status_path = root / "status.json"
        raw_status = load_json(status_path)
        if (
            not isinstance(raw_status, dict)
            or raw_status.get("state_policy") != status.get("state_policy")
        ):
            raise WorkspaceError(
                f"temporary topology state status changed before update: {name}"
            )
        durable_atomic_json(status_path, {**raw_status, "state_policy": policy})
        return self.topology_status(name)

    @staticmethod
    def _temporary_state_removal_path(policy: dict[str, Any]) -> Path:
        state = Path(policy["path"])
        generation = policy["owner"]["generation"]
        return state.parent / f".{generation}.removal-pending"

    @staticmethod
    def _unlink_temporary_state_lock(
        state: Path,
        state_lease: TextIO,
        lease_identity: dict[str, int],
        parent_directory_fd: int | None = None,
    ) -> None:
        Workspace._validate_temporary_state_lock(
            state, state_lease, lease_identity
        )
        lock = Path(f"{state}.lock")

        parent_fd = (
            os.dup(parent_directory_fd)
            if parent_directory_fd is not None
            else os.open(
                lock.parent,
                os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
            )
        )
        try:
            tombstone = (
                f".{lock.name}.remove-{lease_identity['device']:x}-"
                f"{lease_identity['inode']:x}"
            )
            rename_no_replace_at(
                parent_fd, lock.name, parent_fd, tombstone
            )
            moved = os.stat(
                tombstone, dir_fd=parent_fd, follow_symlinks=False
            )
            expected = (
                lease_identity["device"],
                lease_identity["inode"],
            )
            if (moved.st_dev, moved.st_ino) != expected:
                try:
                    rename_no_replace_at(
                        parent_fd, tombstone, parent_fd, lock.name
                    )
                except WorkspaceError:
                    pass
                raise WorkspaceError(
                    f"temporary topology state lease changed before removal: {lock}"
                )
            if not stat.S_ISREG(moved.st_mode) or moved.st_nlink != 1:
                raise WorkspaceError(
                    f"temporary topology state lease changed before removal: {lock}"
                )
            tombstone_fd = os.open(
                tombstone, os.O_PATH | os.O_CLOEXEC | os.O_NOFOLLOW,
                dir_fd=parent_fd,
            )
            try:
                opened = os.fstat(tombstone_fd)
                visible = os.stat(
                    tombstone, dir_fd=parent_fd, follow_symlinks=False
                )
                if (
                    not stat.S_ISREG(opened.st_mode)
                    or opened.st_nlink != 1
                    or (opened.st_dev, opened.st_ino) != expected
                    or (visible.st_dev, visible.st_ino) != expected
                    or visible.st_nlink != 1
                ):
                    raise WorkspaceError(
                        f"temporary topology state lease changed before removal: {lock}"
                    )
                os.unlink(tombstone, dir_fd=parent_fd)
            finally:
                os.close(tombstone_fd)
            os.fsync(parent_fd)
        finally:
            os.close(parent_fd)

    @staticmethod
    def _validate_temporary_state_lock(
        state: Path,
        state_lease: TextIO,
        lease_identity: dict[str, int],
    ) -> None:
        lock = Path(f"{state}.lock")
        try:
            visible = lock.stat(follow_symlinks=False)
        except FileNotFoundError as error:
            raise WorkspaceError(
                f"temporary topology state lease is missing: {lock}"
            ) from error
        opened = os.fstat(state_lease.fileno())
        expected = (lease_identity["device"], lease_identity["inode"])
        if (
            not stat.S_ISREG(visible.st_mode)
            or visible.st_nlink != 1
            or (visible.st_dev, visible.st_ino) != expected
            or (opened.st_dev, opened.st_ino) != expected
        ):
            raise WorkspaceError(
                "temporary topology state lease changed before lifecycle "
                f"mutation: {lock}"
            )

    @staticmethod
    def _finish_temporary_state_lock_tombstone(
        state: Path,
        lease_identity: dict[str, int],
        parent_directory_fd: int | None = None,
    ) -> bool:
        lock = Path(f"{state}.lock")
        tombstone = lock.parent / (
            f".{lock.name}.remove-{lease_identity['device']:x}-"
            f"{lease_identity['inode']:x}"
        )
        if parent_directory_fd is None:
            if not tombstone.exists() and not tombstone.is_symlink():
                return False
        else:
            try:
                os.stat(
                    tombstone.name,
                    dir_fd=parent_directory_fd,
                    follow_symlinks=False,
                )
            except FileNotFoundError:
                return False
        parent_fd = (
            os.dup(parent_directory_fd)
            if parent_directory_fd is not None
            else os.open(
                tombstone.parent,
                os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
            )
        )
        try:
            descriptor = os.open(
                tombstone.name,
                os.O_PATH | os.O_CLOEXEC | os.O_NOFOLLOW,
                dir_fd=parent_fd,
            )
            try:
                metadata = os.fstat(descriptor)
                visible = os.stat(
                    tombstone.name, dir_fd=parent_fd, follow_symlinks=False
                )
                expected = (lease_identity["device"], lease_identity["inode"])
                if (
                    not stat.S_ISREG(metadata.st_mode)
                    or metadata.st_nlink != 1
                    or (metadata.st_dev, metadata.st_ino) != expected
                    or (visible.st_dev, visible.st_ino) != expected
                    or visible.st_nlink != 1
                ):
                    raise WorkspaceError(
                        "temporary topology state lease tombstone is invalid: "
                        f"{tombstone}"
                    )
                os.unlink(tombstone.name, dir_fd=parent_fd)
                os.fsync(parent_fd)
            finally:
                os.close(descriptor)
        finally:
            os.close(parent_fd)
        return True

    @staticmethod
    def _lock_state_directory_mutation(
        path: Path,
        identity: dict[str, int],
        description: str = "temporary state",
    ) -> int:
        descriptor = _open_directory_nofollow(
            path, os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        )
        try:
            opened = os.fstat(descriptor)
            visible = path.stat(follow_symlinks=False)
            expected = (identity["device"], identity["inode"])
            if (
                not stat.S_ISDIR(opened.st_mode)
                or (opened.st_dev, opened.st_ino) != expected
                or (visible.st_dev, visible.st_ino) != expected
            ):
                raise WorkspaceError(
                    f"{description} identity changed before mutation: {path}"
                )
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError as error:
                if error.errno in {errno.EACCES, errno.EAGAIN}:
                    raise WorkspaceError(
                        f"{description} is already in use: {path}"
                    ) from error
                raise WorkspaceError(
                    f"cannot lock {description} for mutation: {path}: {error}"
                ) from error
            return descriptor
        except BaseException:
            os.close(descriptor)
            raise

    def _open_temporary_state_container_for_mutation(
        self, name: str, state: Path
    ) -> int:
        root = self._topology_directory(name)
        if state.parent != root / "temporary-states":
            raise WorkspaceError("temporary state container path is invalid")
        flags = os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW
        root_fd = _open_directory_nofollow(root, flags)
        container_fd: int | None = None
        try:
            visible = os.stat(
                "temporary-states", dir_fd=root_fd, follow_symlinks=False
            )
            container_fd = os.open(
                "temporary-states", flags, dir_fd=root_fd
            )
            opened = os.fstat(container_fd)
            if (
                not stat.S_ISDIR(visible.st_mode)
                or (visible.st_dev, visible.st_ino)
                != (opened.st_dev, opened.st_ino)
                or _descriptor_mount_id(container_fd)
                != _descriptor_mount_id(root_fd)
            ):
                raise WorkspaceError(
                    "temporary state container changed or crossed a mount"
                )
            marker = self._load_state_json_at(
                container_fd,
                MANAGED_MARKER,
                "temporary state container marker",
            )
            if marker != {
                "schema_version": SCHEMA_VERSION,
                "purpose": "topology-temporary-states",
            }:
                raise WorkspaceError("temporary state container marker is invalid")
            result = container_fd
            container_fd = None
            return result
        finally:
            if container_fd is not None:
                os.close(container_fd)
            os.close(root_fd)

    @staticmethod
    def _rollback_temporary_state_creation(
        state: Path,
        state_lease: TextIO,
        state_identity: dict[str, int],
        lease_identity: dict[str, int],
        state_directory_fd: int | None = None,
        implementation: dict[str, str] | None = None,
    ) -> None:
        Workspace._validate_temporary_state_lock(
            state, state_lease, lease_identity
        )
        owned_descriptor = state_directory_fd
        close_descriptor = False
        if owned_descriptor is None:
            owned_descriptor = os.open(
                state,
                os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
            )
            close_descriptor = True
        try:
            Workspace._validate_temporary_state_integrity(
                owned_descriptor, state, implementation
            )
            remove_owned_tree(
                state,
                expected_identity=state_identity,
                reject_links=True,
            )
        finally:
            if close_descriptor:
                os.close(owned_descriptor)
        Workspace._unlink_temporary_state_lock(
            state, state_lease, lease_identity
        )

    def _commit_temporary_state_removal(
        self,
        name: str,
        status: dict[str, Any],
        state_lease: TextIO,
        state_directory_fd: int | None = None,
        state_container_fd: int | None = None,
    ) -> dict[str, Any]:
        policy = status["state_policy"]
        state = Path(policy["path"])
        tombstone = self._temporary_state_removal_path(policy)
        identity = policy["identity"]
        lifecycle = policy["lifecycle"]
        current = status

        def entry_present(candidate: Path) -> bool:
            if state_container_fd is None:
                return candidate.exists() or candidate.is_symlink()
            try:
                os.stat(
                    candidate.name,
                    dir_fd=state_container_fd,
                    follow_symlinks=False,
                )
            except FileNotFoundError:
                return False
            return True
        if lifecycle not in {"removal-pending", "removed"}:
            if state_directory_fd is None:
                raise WorkspaceError(
                    "temporary state integrity lease is missing before removal"
                )
            self._validate_temporary_state_integrity(
                state_directory_fd, state, policy["implementation"]
            )
            pending = {**policy, "lifecycle": "removal-pending"}
            current = self._write_temporary_state_policy(name, current, pending)
            policy = current["state_policy"]
            lifecycle = "removal-pending"
        if lifecycle == "removal-pending":
            state_present = entry_present(state)
            tombstone_present = entry_present(tombstone)
            removal_tombstone = _owned_tree_tombstone_path(tombstone, identity)
            removal_tombstone_present = (
                entry_present(removal_tombstone)
            )
            if state_present and tombstone_present:
                raise WorkspaceError(
                    f"temporary state removal paths conflict: {state}"
                )
            if state_present:
                observed_state = (
                    self._state_identity(state)
                    if state_container_fd is None
                    else {
                        "device": (
                            state_metadata := os.stat(
                                state.name,
                                dir_fd=state_container_fd,
                                follow_symlinks=False,
                            )
                        ).st_dev,
                        "inode": state_metadata.st_ino,
                    }
                )
                if observed_state != identity:
                    raise WorkspaceError(
                        f"temporary state identity changed before removal: {state}"
                    )
                if state_directory_fd is None:
                    raise WorkspaceError(
                        "temporary state integrity lease is missing while "
                        "resuming removal"
                    )
                self._validate_temporary_state_integrity(
                    state_directory_fd, state, policy["implementation"]
                )
                if state_container_fd is None:
                    rename_no_replace(state, tombstone)
                else:
                    rename_no_replace_at(
                        state_container_fd,
                        state.name,
                        state_container_fd,
                        tombstone.name,
                    )
                tombstone_present = True
            if tombstone_present:
                tombstone_metadata = (
                    tombstone.stat(follow_symlinks=False)
                    if state_container_fd is None
                    else os.stat(
                        tombstone.name,
                        dir_fd=state_container_fd,
                        follow_symlinks=False,
                    )
                )
                if (
                    not stat.S_ISDIR(tombstone_metadata.st_mode)
                    or {
                        "device": tombstone_metadata.st_dev,
                        "inode": tombstone_metadata.st_ino,
                    }
                    != identity
                ):
                    raise WorkspaceError(
                        f"temporary state removal identity is invalid: {tombstone}"
                    )
            if not (tombstone_present or removal_tombstone_present):
                raise WorkspaceError(
                    f"temporary state removal ownership evidence is missing: {state}"
                )
            if tombstone_present or removal_tombstone_present:
                remove_owned_tree(
                    tombstone,
                    expected_identity=identity,
                    keep_root=True,
                    reject_links=True,
                    parent_directory_fd=state_container_fd,
                )
                if removal_tombstone_present and not entry_present(tombstone):
                    if state_container_fd is None:
                        rename_no_replace(removal_tombstone, tombstone)
                    else:
                        rename_no_replace_at(
                            state_container_fd,
                            removal_tombstone.name,
                            state_container_fd,
                            tombstone.name,
                        )
            removed = {**policy, "lifecycle": "removed"}
            current = self._write_temporary_state_policy(name, current, removed)
            policy = current["state_policy"]
        if entry_present(tombstone):
            metadata = (
                tombstone.stat(follow_symlinks=False)
                if state_container_fd is None
                else os.stat(
                    tombstone.name,
                    dir_fd=state_container_fd,
                    follow_symlinks=False,
                )
            )
            tombstone_fd = (
                None
                if state_container_fd is None
                else os.open(
                    tombstone.name,
                    os.O_RDONLY
                    | os.O_CLOEXEC
                    | os.O_DIRECTORY
                    | os.O_NOFOLLOW,
                    dir_fd=state_container_fd,
                )
            )
            if (
                not stat.S_ISDIR(metadata.st_mode)
                or (metadata.st_dev, metadata.st_ino)
                != (identity["device"], identity["inode"])
                or (
                    any(tombstone.iterdir())
                    if tombstone_fd is None
                    else bool(os.listdir(tombstone_fd))
                )
            ):
                if tombstone_fd is not None:
                    os.close(tombstone_fd)
                raise WorkspaceError(
                    f"temporary state removal root is invalid: {tombstone}"
                )
            if tombstone_fd is not None:
                os.close(tombstone_fd)
                os.rmdir(tombstone.name, dir_fd=state_container_fd)
                os.fsync(state_container_fd)
            else:
                tombstone.rmdir()
                _fsync_directory(tombstone.parent)
        self._unlink_temporary_state_lock(
            state,
            state_lease,
            policy["lease_identity"],
            state_container_fd,
        )
        return self.topology_status(name)

    def _finish_temporary_state_down(
        self,
        name: str,
        status: dict[str, Any],
        retain_state: bool,
        confirmed_clean: bool,
    ) -> dict[str, Any]:
        policy = status.get("state_policy")
        if not isinstance(policy, dict) or policy.get("mode") != "temporary":
            return status
        if policy.get("lifecycle") in {
            "promoted",
            "promotion-pending",
            "retained",
        }:
            return status
        if retain_state and policy.get("lifecycle") in {
            "removal-pending",
            "removed",
        }:
            raise WorkspaceError(
                "temporary state removal has already begun and cannot be retained"
            )
        if not confirmed_clean and policy.get("lifecycle") != "removal-pending":
            if retain_state:
                raise WorkspaceError(
                    f"topology {name} did not complete a confirmed clean down; "
                    "temporary state was retained for diagnosis"
                )
            return status
        state = Path(policy["path"])
        lock_path = Path(f"{state}.lock")
        if policy.get("lifecycle") == "removed":
            if not (lock_path.exists() or lock_path.is_symlink()):
                container_fd = self._open_temporary_state_container_for_mutation(
                    name, state
                )
                try:
                    self._finish_temporary_state_lock_tombstone(
                        state, policy["lease_identity"], container_fd
                    )
                finally:
                    os.close(container_fd)
                return status
        with exclusive_lock(
            lock_path,
            f"temporary topology state {state}",
            nonblocking=True,
        ) as state_lease:
            self._validate_temporary_state_lock(
                state, state_lease, policy["lease_identity"]
            )
            current = self.topology_status(name)
            if current.get("state_policy") != policy:
                raise WorkspaceError(
                    f"temporary topology state changed before down finalization: {name}"
                )
            if retain_state:
                retained = {**policy, "lifecycle": "retained"}
                return self._write_temporary_state_policy(name, current, retained)
            removal_path = self._temporary_state_removal_path(policy)
            removal_root_tombstone = _owned_tree_tombstone_path(
                removal_path, policy["identity"]
            )
            mutation_path = next(
                (
                    candidate
                    for candidate in (state, removal_path, removal_root_tombstone)
                    if candidate.exists() or candidate.is_symlink()
                ),
                None,
            )
            if mutation_path is None:
                container_fd = self._open_temporary_state_container_for_mutation(
                    name, state
                )
                try:
                    return self._commit_temporary_state_removal(
                        name,
                        current,
                        state_lease,
                        state_container_fd=container_fd,
                    )
                finally:
                    os.close(container_fd)
            container_fd = self._open_temporary_state_container_for_mutation(
                name, state
            )
            mutation_fd: int | None = None
            try:
                mutation_fd = self._lock_state_directory_mutation(
                    mutation_path, policy["identity"]
                )
                if policy.get("lifecycle") not in {
                    "removal-pending",
                    "removed",
                }:
                    try:
                        self._validate_temporary_state_integrity(
                            mutation_fd, state, policy["implementation"]
                        )
                    except WorkspaceError as error:
                        retained = {**policy, "lifecycle": "retained"}
                        self._write_temporary_state_policy(
                            name, current, retained
                        )
                        raise WorkspaceError(
                            f"temporary state was retained because its integrity "
                            f"could not be proved: {state}: {error}"
                        ) from error
                return self._commit_temporary_state_removal(
                    name,
                    current,
                    state_lease,
                    mutation_fd,
                    container_fd,
                )
            finally:
                if mutation_fd is not None:
                    os.close(mutation_fd)
                os.close(container_fd)

    def state_promote(self, topology_name: str, state_name: str) -> dict[str, Any]:
        """Promote one stopped, explicitly retained temporary state in place."""

        validate_name(state_name, "state name")
        if state_name == "default":
            raise WorkspaceError("temporary state cannot be promoted as default")
        root = self._topology_directory(topology_name)
        with exclusive_lock(
            root / "operation.lock",
            f"topology {topology_name} operation",
            nonblocking=True,
        ):
            status = self.topology_status(topology_name)
            policy = status.get("state_policy")
            if (
                isinstance(policy, dict)
                and policy.get("mode") == "temporary"
                and policy.get("lifecycle") == "promoted"
            ):
                state = Path(policy["path"])
                states = self._load_states()
                if (
                    policy.get("name") == state_name
                    and states.get(state_name) is not None
                    and self._canonical_state_path(Path(states[state_name])) == state
                    and (state / PROMOTED_STATE_METADATA).is_file()
                    and not (state / PROMOTED_STATE_METADATA).is_symlink()
                ):
                    self._promoted_state_owner(state_name, state)
                    return {
                        "topology": topology_name,
                        "name": state_name,
                        "path": str(state),
                        "state_policy": policy,
                    }
            if (
                not isinstance(policy, dict)
                or policy.get("mode") != "temporary"
                or policy.get("lifecycle")
                not in {"retained", "promotion-pending", "promoted"}
            ):
                raise WorkspaceError(
                    f"topology does not have a retained temporary state: {topology_name}"
                )
            if (
                status["supervisor"]["running"]
                or any(service["running"] for service in status["services"].values())
                or status["observation"]["process_tree_lease"] == "retained"
            ):
                raise WorkspaceError(
                    f"temporary topology state is still active: {topology_name}"
                )
            if (
                policy.get("lifecycle") == "promotion-pending"
                and policy.get("name") != state_name
            ):
                raise WorkspaceError(
                    "temporary state promotion target cannot change while "
                    f"promotion is pending: {policy.get('name')}"
                )
            state = Path(policy["path"])
            with ExitStack() as promotion:
                state_lease = promotion.enter_context(
                    exclusive_lock(
                        Path(f"{state}.lock"),
                        f"temporary topology state {state}",
                        nonblocking=True,
                    )
                )
                self._validate_temporary_state_lock(
                    state, state_lease, policy["lease_identity"]
                )
                state_fd = os.open(
                    state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
                )
                promotion.callback(os.close, state_fd)
                opened_state = os.fstat(state_fd)
                try:
                    fcntl.flock(state_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except OSError as error:
                    if error.errno in {errno.EACCES, errno.EAGAIN}:
                        raise WorkspaceError(
                            f"temporary state is already in use: {state}"
                        ) from error
                    raise WorkspaceError(
                        f"cannot lock temporary state for promotion: {state}: {error}"
                    ) from error

                def verify_visible_state() -> None:
                    visible_state = state.stat(follow_symlinks=False)
                    if (
                        not stat.S_ISDIR(visible_state.st_mode)
                        or self._canonical_state_path(state) != state
                        or {
                            "device": opened_state.st_dev,
                            "inode": opened_state.st_ino,
                        }
                        != policy.get("identity")
                        or (visible_state.st_dev, visible_state.st_ino)
                        != (opened_state.st_dev, opened_state.st_ino)
                    ):
                        raise WorkspaceError(
                            f"temporary state changed during promotion: {state}"
                        )

                verify_visible_state()
                self._validate_temporary_state_integrity(
                    state_fd, state, policy["implementation"]
                )
                with exclusive_lock(
                    self.paths.workspace / "states.lock", "states registry"
                ):
                    states = self._load_states()
                    existing = states.get(state_name)
                    if existing is not None and self._canonical_state_path(
                        Path(existing)
                    ) != state:
                        raise WorkspaceError(f"state already exists: {state_name}")
                    aliases = [
                        name
                        for name, value in states.items()
                        if name != state_name
                        and self._canonical_state_path(Path(value)) == state
                    ]
                    if aliases:
                        raise WorkspaceError(
                            f"temporary state is already registered as: {aliases[0]}"
                        )
                    pending = {
                        **policy,
                        "name": state_name,
                        "lifecycle": "promotion-pending",
                    }
                    if policy != pending:
                        status = self._write_temporary_state_policy(
                            topology_name, status, pending
                        )
                    owner = pending.get("owner")
                    if not isinstance(owner, dict) or not isinstance(
                        owner.get("generation"), str
                    ):
                        raise WorkspaceError(
                            "temporary state promotion owner is invalid"
                        )
                    provenance = {
                        "schema_version": SCHEMA_VERSION,
                        "name": state_name,
                        "path": str(state),
                        "topology": topology_name,
                        "generation": owner["generation"],
                        "identity": policy["identity"],
                    }
                    provenance_path = state / PROMOTED_STATE_METADATA
                    try:
                        provenance_metadata = os.stat(
                            PROMOTED_STATE_METADATA,
                            dir_fd=state_fd,
                            follow_symlinks=False,
                        )
                    except FileNotFoundError:
                        provenance_metadata = None
                    if provenance_metadata is not None:
                        if not stat.S_ISREG(provenance_metadata.st_mode) or (
                            load_regular_json_at(
                                state_fd,
                                PROMOTED_STATE_METADATA,
                                "promoted state provenance",
                            )
                            != provenance
                        ):
                            raise WorkspaceError(
                                f"promoted state provenance is invalid: {provenance_path}"
                            )
                    else:
                        durable_atomic_json_at(
                            state_fd, PROMOTED_STATE_METADATA, provenance
                        )
                    verify_visible_state()
                    registry_added = False
                    if existing is None:
                        states[state_name] = str(state)
                        durable_atomic_json(
                            self.paths.states_file,
                            {"schema_version": SCHEMA_VERSION, "states": states},
                        )
                        registry_added = True
                    try:
                        verify_visible_state()
                    except BaseException:
                        if registry_added:
                            del states[state_name]
                            durable_atomic_json(
                                self.paths.states_file,
                                {
                                    "schema_version": SCHEMA_VERSION,
                                    "states": states,
                                },
                            )
                        raise
                    promoted = {**pending, "lifecycle": "promoted"}
                    try:
                        status = self._write_temporary_state_policy(
                            topology_name, status, promoted
                        )
                        verify_visible_state()
                    except BaseException:
                        status_path = root / "status.json"
                        raw_status = load_regular_json(
                            status_path, "temporary state promotion status"
                        )
                        if raw_status.get("state_policy") == promoted:
                            durable_atomic_json(
                                status_path,
                                {**raw_status, "state_policy": pending},
                            )
                        if registry_added and states.get(state_name) is not None:
                            del states[state_name]
                            durable_atomic_json(
                                self.paths.states_file,
                                {
                                    "schema_version": SCHEMA_VERSION,
                                    "states": states,
                                },
                            )
                        raise
                    return {
                        "topology": topology_name,
                        "name": state_name,
                        "path": str(state),
                        "state_policy": status["state_policy"],
                    }

    def _legacy_topology_down(
        self, name: str, status: dict[str, Any], timeout: float
    ) -> dict[str, Any]:
        supervisor = status["supervisor"]
        supervisor_owned = supervisor["running"]
        targets = [supervisor] if supervisor_owned else [
            service
            for service in status["services"].values()
            if service["running"]
        ]
        verified: list[tuple[dict[str, Any], int]] = []
        try:
            for record in targets:
                try:
                    pidfd = os.pidfd_open(record["pid"])
                except ProcessLookupError:
                    continue
                if not process_matches(record["pid"], record["start_time"]):
                    os.close(pidfd)
                    continue
                verified.append((record, pidfd))
                if supervisor_owned:
                    signal.pidfd_send_signal(pidfd, signal.SIGTERM)
                else:
                    try:
                        os.killpg(record["pid"], signal.SIGTERM)
                    except ProcessLookupError:
                        pass
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if supervisor_owned:
                    running = any(
                        self._recorded_process_running(record)
                        for record, _pidfd in verified
                    )
                else:
                    running = any(
                        self._process_group_running(record["pid"])
                        for record, _pidfd in verified
                    )
                if not running:
                    return self.topology_status(name)
                time.sleep(0.1)
            if not supervisor_owned:
                for record, _pidfd in verified:
                    if not self._process_group_running(record["pid"]):
                        continue
                    try:
                        os.killpg(record["pid"], signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                kill_deadline = time.monotonic() + min(max(timeout, 0.1), 2.0)
                while time.monotonic() < kill_deadline:
                    if not any(
                        self._process_group_running(record["pid"])
                        for record, _pidfd in verified
                    ):
                        return self.topology_status(name)
                    time.sleep(0.05)
        finally:
            for _record, pidfd in verified:
                os.close(pidfd)
        raise WorkspaceError(
            f"topology did not stop within {timeout:g} seconds: {name}"
        )

    @staticmethod
    def _process_group_running(process_group: int) -> bool:
        try:
            os.killpg(process_group, 0)
        except ProcessLookupError:
            return False
        except PermissionError:
            return True
        return True

    def topology_logs(
        self,
        name: str,
        service: str | None,
        tail: int,
        follow: bool,
    ) -> None:
        if tail < 0:
            raise WorkspaceError("log tail must not be negative")
        root = self._topology_directory(name)
        services = [service] if service else list(TOPOLOGY_SERVICES)
        unknown = sorted(set(services) - set(TOPOLOGY_SERVICES))
        if unknown:
            raise WorkspaceError(f"unknown topology services: {', '.join(unknown)}")
        paths = [(item, root / f"{item}.log") for item in services]
        paths = [(item, path) for item, path in paths if path.is_file()]
        if not paths:
            raise WorkspaceError(f"topology has no matching logs: {name}")
        policy: object = None
        status_path = root / "status.json"
        if status_path.is_file() and not status_path.is_symlink():
            raw_status = load_regular_json(status_path, "topology status")
            if isinstance(raw_status, dict):
                policy = raw_status.get("state_policy")
        if isinstance(policy, dict) and self._log_state_policy_is_safe(policy):
            print(
                "==> state-policy "
                f"mode={policy['mode']} owner={json.dumps(policy['owner'], sort_keys=True)} "
                f"path={policy['path']} lifecycle={policy['lifecycle']} <=="
            )
        positions: dict[Path, tuple[int, int, int]] = {}
        for item, path in paths:
            with os.fdopen(
                open_regular_file(path, os.O_RDONLY, "topology log"),
                encoding="utf-8",
                errors="replace",
            ) as stream:
                lines = deque(stream, maxlen=tail) if tail else ()
                if len(paths) > 1:
                    print(f"==> {item} <==")
                for line in lines:
                    print(line, end="")
                metadata = os.fstat(stream.fileno())
                positions[path] = (metadata.st_dev, metadata.st_ino, stream.tell())
        while follow:
            changed = False
            for item, path in paths:
                try:
                    descriptor = open_regular_file(
                        path, os.O_RDONLY, "topology log"
                    )
                except WorkspaceError:
                    continue
                with os.fdopen(
                    descriptor, encoding="utf-8", errors="replace"
                ) as stream:
                    metadata = os.fstat(stream.fileno())
                    device, inode, offset = positions[path]
                    if (
                        (metadata.st_dev, metadata.st_ino) != (device, inode)
                        or metadata.st_size < offset
                    ):
                        offset = 0
                    stream.seek(offset)
                    content = stream.read()
                    positions[path] = (
                        metadata.st_dev,
                        metadata.st_ino,
                        stream.tell(),
                    )
                if content:
                    if len(paths) > 1:
                        print(f"==> {item} <==")
                    print(content, end="", flush=True)
                    changed = True
            try:
                status = self.topology_status(name)
                running = status["supervisor"]["running"]
            except WorkspaceError:
                running = False
            if not running and not changed:
                break
            time.sleep(0.25)

    @staticmethod
    def _log_state_policy_is_safe(policy: dict[str, Any]) -> bool:
        mode = policy.get("mode")
        path = policy.get("path")
        owner = policy.get("owner")
        lifecycle = policy.get("lifecycle")
        return bool(
            mode in {"temporary", "named", "default"}
            and isinstance(path, str)
            and Path(path).is_absolute()
            and "\n" not in path
            and "\r" not in path
            and isinstance(owner, dict)
            and all(
                isinstance(key, str)
                and isinstance(value, str)
                and "\n" not in value
                and "\r" not in value
                for key, value in owner.items()
            )
            and isinstance(lifecycle, str)
            and "\n" not in lifecycle
            and "\r" not in lifecycle
        )

    def run_client(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
    ) -> Path:
        self.paths.ensure()
        with self._resolved_profile_operation(
            profile_name, {"client"}, "publish foreground client runtime"
        ):
            prepared = self._run_client(
                profile_name,
                state_name,
                port,
                arguments,
                dry_run,
            )
        if not isinstance(prepared, dict):
            return prepared
        runtime_fd = prepared["runtime_fd"]
        generation_root = prepared["generation_root"]
        try:
            if not dry_run:
                self._require_client_display()
                environment = os.environ.copy()
                environment[CLIENT_LAUNCH_LABEL_ENV] = prepared["launch_label"]
                run(
                    prepared["command"],
                    cwd=prepared["cwd"],
                    env=environment,
                    diagnostics_to_stderr=False,
                    pass_fds=(runtime_fd,),
                )
            return prepared["executable"]
        finally:
            os.close(runtime_fd)
            remove_owned_tree(generation_root)

    def _run_client(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
        *,
        layout_lock: TextIO | None = None,
    ) -> dict[str, Any] | Path:
        self._validate_run_port(port)
        launch_label = client_launch_label(profile_name)
        self._require_classic_contracts(profile_name, {"client"})
        state = self._state_location(state_name)
        self._validate_state(state)
        fingerprint = self._server_identity_fingerprint(state)
        targets = self._expand_build_target("client", profile_name)
        selected = self._resolve_build_profile(profile_name, {"client"})
        root = self._build_resolved(
            "client", profile_name, False, targets, selected
        )
        with self._profile_build_lock(root, profile_name):
            build_metadata = load_json(root / BUILD_METADATA)
            try:
                validated_sound = validate_sound_record(build_metadata.get("sound"))
            except WorkspaceError as error:
                raise WorkspaceError(
                    f"build sound metadata is invalid for profile {profile_name}"
                ) from error
            profile = self._load_profile(profile_name, require_file=False)
            selected_stack = self.manifest.stack(profile["stack"])
            if validated_sound["mode"] != profile["sound_mode"]:
                raise WorkspaceError(
                    f"profile {profile_name} sound mode does not match build metadata"
                )
            sound_root = Path(validated_sound["root"])
            if validated_sound["mode"] == PLAYTEST_MODE:
                inputs = clean_source_inputs(selected["sound"])
                if verify_playtest_tree(selected["sound"], sound_root, inputs) != (
                    validated_sound
                ):
                    raise WorkspaceError(
                        f"profile {profile_name} local-playtest sound record changed "
                        "before foreground publication"
                    )
            elif validated_sound["mode"] == RELEASED_MODE:
                coordinates = validate_release_coordinates(profile["sound_release"])
                if verify_release_tree(sound_root, coordinates) != validated_sound:
                    raise WorkspaceError(
                        f"profile {profile_name} released sound record changed "
                        "before foreground publication"
                    )
            elif sound_root.resolve() != selected["sound"].resolve():
                raise WorkspaceError(
                    f"profile {profile_name} source sound root changed before "
                    "foreground publication"
                )
            resolved = self._topology_resolved_status(profile_name, selected)
            generation = secrets.token_hex(32)
            generation_root, runtime_fd, _runtime_record, state_output_fd = (
                self._publish_runtime_generation(
                    self._foreground_runtime_owner(),
                    generation,
                    profile_name,
                    root,
                    selected,
                    resolved,
                    ["client"],
                    identity={
                        "kind": "foreground-client",
                        "stack": selected_stack.name,
                        "providers": {
                            role: selected_stack.providers[role].name
                            for role in sorted(selected)
                        },
                    },
                    sound_root=sound_root,
                )
            )
            if state_output_fd is not None:
                os.close(state_output_fd)
            working = generation_root / "client"
            executable = working / "atrinik"
            command = [
                str(executable),
                f"--server=127.0.0.1 {port} {fingerprint}",
                "--stun_server=off",
                "--nometa",
                *arguments,
            ]
            print(f"state: {state}")
            print(f"cwd: {working}")
            print(f"runtime generation: {generation}")
            print(f"launch label: {launch_label}")
            print(f"command: {display_arguments(command)}")
            return {
                "command": command,
                "cwd": working,
                "executable": executable,
                "generation_root": generation_root,
                "runtime_fd": runtime_fd,
                "launch_label": launch_label,
            }

    def run_server(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
    ) -> Path:
        self.paths.ensure()
        with self._resolved_profile_operation(
            profile_name, {"server"}, "publish foreground server runtime"
        ):
            prepared = self._run_server(
                profile_name,
                state_name,
                port,
                arguments,
                dry_run,
            )
        if not isinstance(prepared, dict):
            return prepared
        runtime_fd = prepared["runtime_fd"]
        state_fd = prepared["state_fd"]
        state_lock_fd = prepared["state_lock_fd"]
        physical_state_lock_fd = prepared["physical_state_lock_fd"]
        state_output_fd = prepared["state_output_fd"]
        generation_root = prepared["generation_root"]
        state_output = prepared["state_output"]
        try:
            if not dry_run:
                run(
                    prepared["command"],
                    cwd=prepared["cwd"],
                    diagnostics_to_stderr=False,
                    pass_fds=tuple(
                        descriptor
                        for descriptor in (
                            runtime_fd,
                            state_fd,
                            state_lock_fd,
                            physical_state_lock_fd,
                            state_output_fd,
                        )
                        if descriptor is not None
                    ),
                )
            return prepared["executable"]
        finally:
            try:
                if state_output is not None:
                    self._remove_runtime_state_output(
                        state_output,
                        generation_root.name,
                        state_fd,
                        prepared["state_output_identity"],
                    )
            finally:
                if physical_state_lock_fd is not None:
                    os.close(physical_state_lock_fd)
                os.close(state_output_fd)
                os.close(state_lock_fd)
                os.close(state_fd)
                os.close(runtime_fd)
                remove_owned_tree(generation_root)

    def _run_server(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
        *,
        layout_lock: TextIO | None = None,
    ) -> dict[str, Any] | Path:
        self._validate_run_port(port)
        self._require_classic_contracts(profile_name, {"server"})
        targets = self._expand_build_target("server", profile_name)
        selected = self._resolve_build_profile(profile_name, {"server"})
        state_location = self._state_location(state_name)
        with self._topology_state_lock(state_location) as state_lock:
            root = self._build_resolved(
                "server", profile_name, False, targets, selected
            )
            with self._profile_build_lock(root, profile_name):
                resolved = self._topology_resolved_status(profile_name, selected)
                profile = self._load_profile(profile_name, require_file=False)
                selected_stack = self.manifest.stack(profile["stack"])
                providers = {
                    role: selected_stack.providers[role].name
                    for role in sorted(selected)
                }
                implementation = self._state_implementation(
                    selected_stack.name, providers, resolved
                )
                prepared_state = self.state_path(
                    state_name,
                    selected["server"],
                    resolved_path=state_location,
                    implementation=implementation,
                    write_implementation=True,
                    keep_descriptor=True,
                )
                assert isinstance(prepared_state, tuple)
                state, state_fd = prepared_state
                opened_state = os.fstat(state_fd)
                try:
                    state_lock.bind(
                        {
                            "device": opened_state.st_dev,
                            "inode": opened_state.st_ino,
                        }
                    )
                except BaseException:
                    os.close(state_fd)
                    raise
                generation = secrets.token_hex(32)
                try:
                    (
                        generation_root,
                        runtime_fd,
                        _runtime_record,
                        state_output_fd,
                    ) = (
                        self._publish_runtime_generation(
                            self._foreground_runtime_owner(),
                            generation,
                            profile_name,
                            root,
                            selected,
                            resolved,
                            ["server"],
                            identity={
                                "kind": "foreground-server",
                                "stack": selected_stack.name,
                                "providers": providers,
                            },
                            state=state,
                            state_directory_fd=state_fd,
                        )
                    )
                except BaseException:
                    os.close(state_fd)
                    raise
                state_output = (
                    Path(_runtime_record["mutable_state_outputs"][0])
                    if _runtime_record["mutable_state_outputs"]
                    else None
                )
                state_output_identity = (
                    _runtime_record["mutable_state_output_identities"][0]
                    if _runtime_record["mutable_state_output_identities"]
                    else None
                )
                assert state_output_fd is not None
                state_lock_fd: int | None = None
                physical_state_lock_fd: int | None = None
                try:
                    state_lock_fd = os.dup(state_lock.fileno())
                    physical_state_lock_fd = (
                        os.dup(state_lock.physical_lock.fileno())
                        if state_lock.physical_lock is not None
                        else None
                    )
                except BaseException:
                    if physical_state_lock_fd is not None:
                        os.close(physical_state_lock_fd)
                    if state_lock_fd is not None:
                        os.close(state_lock_fd)
                    try:
                        if state_output is not None:
                            self._remove_runtime_state_output(
                                state_output,
                                generation,
                                state_fd,
                                state_output_identity,
                            )
                    finally:
                        os.close(state_output_fd)
                        os.close(state_fd)
                        os.close(runtime_fd)
                        remove_owned_tree(generation_root)
                    raise
                assert state_lock_fd is not None
                runtime = generation_root / "server"
                executable = runtime / "atrinik-server"
                command = [
                    str(executable),
                    f"--port_quic={port}",
                    "--port_mapping=off",
                    "--stun_server=off",
                    *arguments,
                    f"--datapath=/proc/self/fd/{state_fd}",
                    f"--assetspath=/proc/self/fd/{state_output_fd}",
                ]
                print(f"state: {state}")
                print(f"cwd: {runtime}")
                print(f"runtime generation: {generation}")
                print(f"command: {display_arguments(command)}")
                return {
                    "command": command,
                    "cwd": runtime,
                    "executable": executable,
                    "generation_root": generation_root,
                    "runtime_fd": runtime_fd,
                    "state_fd": state_fd,
                    "state_lock_fd": state_lock_fd,
                    "physical_state_lock_fd": physical_state_lock_fd,
                    "state_output": state_output,
                    "state_output_identity": state_output_identity,
                    "state_output_fd": state_output_fd,
                }

    def _foreground_runtime_owner(self) -> Path:
        root = self.paths.workspace / "foreground-runs"
        metadata = {
            "schema_version": SCHEMA_VERSION,
            "purpose": "foreground-runtime-generations",
        }
        marker = root / MANAGED_MARKER
        if root.exists() or root.is_symlink():
            if (
                not root.is_dir()
                or root.is_symlink()
                or not marker.is_file()
                or marker.is_symlink()
                or load_json(marker) != metadata
            ):
                raise WorkspaceError(
                    f"foreground runtime container is invalid: {root}"
                )
        else:
            root.mkdir()
            atomic_json(marker, metadata)
        return root

    @staticmethod
    def _validate_run_port(port: int) -> None:
        if (
            not isinstance(port, int)
            or isinstance(port, bool)
            or not 1 <= port <= 65535
        ):
            raise WorkspaceError("server UDP port must be between 1 and 65535")

    @staticmethod
    def _server_identity_fingerprint(state: Path) -> str:
        identity = state / "quic-identity.pem"
        if not identity.exists():
            raise WorkspaceError(
                f"server QUIC identity does not exist: {identity}; start the "
                "matching server before launching the foreground client"
            )
        try:
            descriptor = open_regular_file(
                identity, os.O_RDONLY, "server QUIC identity"
            )
            with os.fdopen(descriptor, encoding="ascii") as stream:
                contents = stream.read(SERVER_IDENTITY_MAX_SIZE + 1)
        except UnicodeError as error:
            raise WorkspaceError(
                f"server QUIC identity is not ASCII PEM: {identity}"
            ) from error
        if len(contents) > SERVER_IDENTITY_MAX_SIZE:
            raise WorkspaceError(f"server QUIC identity is too large: {identity}")

        begin = "-----BEGIN CERTIFICATE-----"
        end = "-----END CERTIFICATE-----"
        start = contents.find(begin)
        finish = contents.find(end, start + len(begin)) if start >= 0 else -1
        if start < 0 or finish < 0:
            raise WorkspaceError(f"server QUIC identity lacks a certificate: {identity}")
        certificate = contents[start : finish + len(end)]
        try:
            encoded = ssl.PEM_cert_to_DER_cert(certificate)
        except (binascii.Error, ValueError) as error:
            raise WorkspaceError(
                f"server QUIC identity contains an invalid certificate: {identity}"
            ) from error
        return hashlib.sha256(encoded).hexdigest()

    def _link_server_runtime_inputs(
        self,
        runtime: Path,
        root: Path,
        selected: dict[str, Path],
        state: Path,
        content: Path,
        resources: Path,
    ) -> None:
        source = selected["server"]
        binary = self._classic_binary_directory(root, "server")
        links = {
            "atrinik-server": binary / "atrinik-server",
            "libplugin_arena.so": binary / "libplugin_arena.so",
            "libplugin_python.so": binary / "libplugin_python.so",
            "lib": content / "lib",
            "maps": content / "maps",
            "resources": resources,
            "data": state,
            "tools": source / "tools",
        }
        for name, target in links.items():
            if not target.exists():
                raise WorkspaceError(f"server runtime input is missing: {target}")
            (runtime / name).symlink_to(
                target, target_is_directory=target.is_dir()
            )
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            target = source / name
            if not target.is_file():
                raise WorkspaceError(f"server runtime input is missing: {target}")
            (runtime / name).symlink_to(target)
        custom = source / "server-custom.cfg"
        if custom.is_file():
            (runtime / "server-custom.cfg").symlink_to(custom)

    def _prepare_server_runtime(
        self,
        root: Path,
        selected: dict[str, Path],
        state: Path,
        state_name: str,
        content: Path | None = None,
        resources: Path | None = None,
        client_maps: Path | None = None,
    ) -> Path:
        state_key = profile_key({"state": state})
        runtime = root / "run" / "server" / f"{state_name}-{state_key}"
        managed_reset(runtime, self.paths.builds, f"server-runtime:{state_key}")
        content = content or root / "runtime" / "content"
        resources = resources or root / "runtime" / "resources"
        client_maps = client_maps or root / "runtime" / "client-maps"
        self._validate_region_maps(client_maps)
        self._link_server_runtime_inputs(
            runtime, root, selected, state, content, resources
        )
        assets = runtime / "assets"
        self._prepare_asset_staging_directory(assets)
        self._prepare_asset_staging_directory(assets / "data")
        shutil.copytree(client_maps, assets / "client-maps")
        return runtime

    @staticmethod
    def _prepare_asset_staging_directory(path: Path) -> None:
        if path.is_symlink():
            raise WorkspaceError(f"server asset staging path is invalid: {path}")
        try:
            mode = path.lstat().st_mode
        except FileNotFoundError:
            path.mkdir()
            return
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect server asset staging path {path}: {error}"
            ) from error
        if not stat.S_ISDIR(mode):
            raise WorkspaceError(f"server asset staging path is invalid: {path}")

    def _component(self, name: str) -> Component:
        try:
            return self.manifest.by_name[name]
        except KeyError as error:
            raise WorkspaceError(f"unknown component: {name}") from error

    def _resolve_checkout(self, name: str) -> Checkout:
        if name in self.manifest.by_checkout:
            return self.manifest.by_checkout[name]
        if name in self.manifest.by_name:
            return self.manifest.checkout_for(name)
        raise WorkspaceError(f"unknown component or checkout: {name}")
