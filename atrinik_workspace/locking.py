from __future__ import annotations

from contextlib import ExitStack, contextmanager
from contextvars import ContextVar
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import socket
import stat
import sys
import threading
from datetime import datetime, timezone
from typing import BinaryIO, Callable, Iterator, TextIO

from .model import WorkspaceError
from .platform_compat import (
    IS_WINDOWS,
    O_CLOEXEC,
    assert_no_symlink_components,
    fcntl,
)


LAYOUT_WRITER_INTENT_SUFFIX = ".writer-intent"
LAYOUT_WRITER_PENDING_SUFFIX = ".writer-pending"
LOCK_WAIT_DIAGNOSTIC_SECONDS = 10.0
RESOURCE_LEASE_SCHEMA_VERSION = 2
RESOURCE_KIND_ORDER = {
    "maintenance": 0,
    "registry": 10,
    "profile": 20,
    "git-admin": 30,
    "source": 40,
    "topology": 50,
    "scenario": 55,
    "state": 60,
    "build-root": 70,
    "cache": 80,
}


@dataclass(frozen=True)
class LeaseRequest:
    """One exact resource in the workspace lease graph."""

    kind: str
    coordinate: str
    mode: str
    operation: str
    recovery: str

    def __post_init__(self) -> None:
        if self.kind not in RESOURCE_KIND_ORDER:
            raise ValueError(f"unknown resource lease kind: {self.kind}")
        if self.mode not in {"shared", "exclusive"}:
            raise ValueError(f"unknown resource lease mode: {self.mode}")
        for label, value in (
            ("coordinate", self.coordinate),
            ("operation", self.operation),
            ("recovery", self.recovery),
        ):
            if not value or "\n" in value or "\r" in value:
                raise ValueError(f"resource lease {label} must be one non-empty line")

    @property
    def sort_key(self) -> tuple[int, str]:
        return RESOURCE_KIND_ORDER[self.kind], self.coordinate


class LockBusyError(WorkspaceError):
    """Raised when a requested nonblocking advisory lock is held."""


_ACTIVE_LOCK_FDS: ContextVar[tuple[int, ...]] = ContextVar(
    "atrinik_active_lock_fds", default=()
)
_ACTIVE_MAINTENANCE_PATHS: ContextVar[tuple[str, ...]] = ContextVar(
    "atrinik_active_maintenance_paths", default=()
)


def active_lock_fds() -> tuple[int, ...]:
    return _ACTIVE_LOCK_FDS.get()


@contextmanager
def inherit_lock_fds(*leases: BinaryIO | int) -> Iterator[None]:
    descriptors = tuple(
        lease if isinstance(lease, int) else lease.fileno() for lease in leases
    )
    current = active_lock_fds()
    token = _ACTIVE_LOCK_FDS.set(tuple(dict.fromkeys((*current, *descriptors))))
    try:
        yield
    finally:
        _ACTIVE_LOCK_FDS.reset(token)


@contextmanager
def shared_maintenance_lock(
    path: Path, *, inherit: bool = True
) -> Iterator[None]:
    """Retain the migration barrier once across nested exact-lease scopes."""

    coordinate = str(path.resolve(strict=False))
    active = _ACTIVE_MAINTENANCE_PATHS.get()
    if coordinate in active:
        yield
        return
    with shared_layout_lock(path, "repository maintenance", inherit=inherit):
        token = _ACTIVE_MAINTENANCE_PATHS.set((*active, coordinate))
        try:
            yield
        finally:
            _ACTIVE_MAINTENANCE_PATHS.reset(token)


def _open_lock(
    path: Path,
    description: str,
    *,
    directory_fd: int | None = None,
) -> TextIO:
    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        if directory_fd is not None:
            raise WorkspaceError(
                "descriptor-relative lock paths are unavailable on native Windows"
            )
        lock: TextIO | None = None
        try:
            assert_no_symlink_components(path, "lock")
            opened_parent = path.parent.stat(follow_symlinks=False)
            if not stat.S_ISDIR(opened_parent.st_mode):
                raise OSError(f"lock parent is not a directory: {path.parent}")
            lock = path.open("a+", encoding="utf-8")
            opened = os.fstat(lock.fileno())
            assert_no_symlink_components(path, "lock")
            visible = path.stat(follow_symlinks=False)
            visible_parent = path.parent.stat(follow_symlinks=False)
            if (
                not stat.S_ISREG(opened.st_mode)
                or not stat.S_ISREG(visible.st_mode)
                or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
                or (opened_parent.st_dev, opened_parent.st_ino)
                != (visible_parent.st_dev, visible_parent.st_ino)
            ):
                raise WorkspaceError(f"{description} lock identity changed during open: {path}")
            return lock
        except WorkspaceError:
            if lock is not None:
                lock.close()
            raise
        except OSError as error:
            if lock is not None:
                lock.close()
            raise WorkspaceError(
                f"cannot open {description} lock {path}: {error}"
            ) from error

    flags = os.O_RDWR | os.O_CREAT | O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    directory_flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_DIRECTORY"):
        directory_flags |= os.O_DIRECTORY
    if hasattr(os, "O_NOFOLLOW"):
        directory_flags |= os.O_NOFOLLOW
    parent_descriptor: int | None = None
    descriptor: int | None = None
    try:
        if directory_fd is None:
            parent_descriptor = os.open(path.parent, directory_flags)
            parent_status = os.fstat(parent_descriptor)
            visible_status = path.parent.stat(follow_symlinks=False)
            if (
                not stat.S_ISDIR(visible_status.st_mode)
                or (parent_status.st_dev, parent_status.st_ino)
                != (visible_status.st_dev, visible_status.st_ino)
            ):
                raise OSError("lock parent directory changed during open")
        else:
            parent_descriptor = directory_fd
        descriptor = os.open(path.name, flags, 0o600, dir_fd=parent_descriptor)
        opened_status = os.fstat(descriptor)
        visible_lock = os.stat(
            path.name, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if (
            not stat.S_ISREG(visible_lock.st_mode)
            or (opened_status.st_dev, opened_status.st_ino)
            != (visible_lock.st_dev, visible_lock.st_ino)
        ):
            os.close(descriptor)
            descriptor = None
            raise OSError("lock file changed during open")
    except OSError as error:
        if descriptor is not None:
            os.close(descriptor)
        raise WorkspaceError(
            f"cannot open {description} lock {path}: {error}"
        ) from error
    finally:
        if directory_fd is None and parent_descriptor is not None:
            os.close(parent_descriptor)
    if not stat.S_ISREG(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise WorkspaceError(f"{description} lock is not a regular file: {path}")
    return os.fdopen(descriptor, "a+")


@contextmanager
def _advisory_lock(
    path: Path,
    description: str,
    operation: int,
    *,
    nonblocking: bool = False,
    directory_fd: int | None = None,
) -> Iterator[TextIO]:
    if directory_fd is None:
        if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
            try:
                assert_no_symlink_components(path.parent, "lock")
            except OSError as error:
                raise WorkspaceError(str(error)) from error
        path.parent.mkdir(parents=True, exist_ok=True)
    with _open_lock(path, description, directory_fd=directory_fd) as lock:
        try:
            fcntl.flock(lock, operation | fcntl.LOCK_NB)
        except BlockingIOError as error:
            if nonblocking:
                raise LockBusyError(f"{description} is already in use") from error
            try:
                fcntl.flock(lock, operation)
            except OSError as wait_error:
                raise WorkspaceError(
                    f"cannot acquire {description} lock: {wait_error}"
                ) from wait_error
        except OSError as error:
            raise WorkspaceError(
                f"cannot acquire {description} lock: {error}"
            ) from error
        yield lock


def _reap_staged_resource_owners(
    owners_descriptor: int,
    owners: Path,
) -> None:
    staging_descriptor: int | None = None
    try:
        staging_descriptor = _open_or_create_directory_at(
            owners_descriptor, ".pending", owners / ".pending"
        )
        fcntl.flock(staging_descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        for metadata_name in sorted(os.listdir(staging_descriptor)):
            descriptor: int | None = None
            try:
                flags = os.O_RDWR | os.O_CLOEXEC
                if hasattr(os, "O_NOFOLLOW"):
                    flags |= os.O_NOFOLLOW
                descriptor = os.open(
                    metadata_name, flags, dir_fd=staging_descriptor
                )
                opened = os.fstat(descriptor)
                visible = os.stat(
                    metadata_name,
                    dir_fd=staging_descriptor,
                    follow_symlinks=False,
                )
                if (
                    not stat.S_ISREG(visible.st_mode)
                    or (opened.st_dev, opened.st_ino)
                    != (visible.st_dev, visible.st_ino)
                ):
                    continue
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                os.unlink(metadata_name, dir_fd=staging_descriptor)
            except (FileNotFoundError, BlockingIOError, OSError):
                continue
            finally:
                if descriptor is not None:
                    os.close(descriptor)
    except (OSError, WorkspaceError):
        pass
    finally:
        if staging_descriptor is not None:
            os.close(staging_descriptor)


def _lease_owner_summary(
    path: Path, *, wait_for_transition: bool = True
) -> str:
    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        return (
            "native Windows lock owner metadata is unavailable; "
            "the kernel lock itself remains authoritative"
        )
    owners = path.with_name(f"{path.name}.owners")
    parent_descriptor: int | None = None
    owners_descriptor: int | None = None
    try:
        parent_descriptor = _open_directory(
            path.parent, "resource lease parent"
        )
        flags = os.O_RDONLY | os.O_CLOEXEC
        if hasattr(os, "O_DIRECTORY"):
            flags |= os.O_DIRECTORY
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        owners_descriptor = os.open(
            owners.name, flags, dir_fd=parent_descriptor
        )
        fcntl.flock(owners_descriptor, fcntl.LOCK_EX)
        _reap_staged_resource_owners(owners_descriptor, owners)
        paths = sorted(os.listdir(owners_descriptor))
    except (FileNotFoundError, NotADirectoryError, PermissionError, OSError, WorkspaceError):
        if owners_descriptor is not None:
            os.close(owners_descriptor)
        if parent_descriptor is not None:
            os.close(parent_descriptor)
        return "owner metadata unavailable"
    descriptions: list[tuple[int, str, str]] = []

    def reap(metadata_name: str) -> bool:
        try:
            os.unlink(metadata_name, dir_fd=owners_descriptor)
        except OSError:
            return False
        return True

    for metadata_name in paths:
        if metadata_name == ".pending":
            continue
        descriptor: int | None = None
        reapable = False
        try:
            flags = os.O_RDWR | os.O_CLOEXEC
            if hasattr(os, "O_NOFOLLOW"):
                flags |= os.O_NOFOLLOW
            descriptor = os.open(
                metadata_name, flags, dir_fd=owners_descriptor
            )
            opened = os.fstat(descriptor)
            visible = os.stat(
                metadata_name,
                dir_fd=owners_descriptor,
                follow_symlinks=False,
            )
            if (
                not stat.S_ISREG(visible.st_mode)
                or (opened.st_dev, opened.st_ino)
                != (visible.st_dev, visible.st_ino)
            ):
                continue
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                pass
            else:
                reapable = True
            with os.fdopen(descriptor, encoding="utf-8") as stream:
                descriptor = None
                value = json.load(stream)
        except (OSError, UnicodeError, json.JSONDecodeError):
            if reapable:
                reap(metadata_name)
            continue
        finally:
            if descriptor is not None:
                os.close(descriptor)
        if not isinstance(value, dict):
            if reapable:
                reap(metadata_name)
            continue
        operation = value.get("operation")
        owner = value.get("owner")
        mode = value.get("mode")
        phase = value.get("phase", "admitted")
        valid_owner = (
            all(isinstance(item, str) and item for item in (operation, owner, mode))
            and phase in {"waiting", "admitted", "release-uncertain"}
        )
        current_schema = (
            value.get("schema_version") == RESOURCE_LEASE_SCHEMA_VERSION
        )
        if (
            current_schema
            and reapable
            and (not valid_owner or phase != "release-uncertain")
        ):
            reap(metadata_name)
            continue
        if valid_owner:
            prefix = (
                ""
                if phase == "admitted"
                else f"{phase.replace('-', ' ')} "
            )
            descriptions.append(
                (
                    -1 if phase == "release-uncertain" else (
                        0 if phase == "admitted" else 1
                    ),
                    metadata_name,
                    f"{prefix}{mode} {operation} by {owner}",
                )
            )
    os.close(owners_descriptor)
    os.close(parent_descriptor)
    descriptions.sort()
    if (
        wait_for_transition
        and any(item[0] == 1 for item in descriptions)
    ):
        transition = path.with_name(f"{path.name}.owner-transition.lock")
        try:
            with _advisory_lock(
                transition,
                "resource lease owner transition",
                fcntl.LOCK_SH,
            ):
                return _lease_owner_summary(
                    path, wait_for_transition=False
                )
        except WorkspaceError:
            pass
    return (
        "; ".join(item[2] for item in descriptions[:8])
        if descriptions
        else "owner metadata unavailable"
    )


@contextmanager
def _diagnose_layout_wait(
    path: Path,
    description: str,
    recovery_action: str | None = None,
) -> Iterator[None]:
    acquired = threading.Event()

    def warn_about_wait() -> None:
        if acquired.wait(LOCK_WAIT_DIAGNOSTIC_SECONDS):
            return
        owner = _lease_owner_summary(path)
        recovery = recovery_action or (
            "inspect `./atrinik ps --json` and "
            "`./atrinik worktree list --json`; do not bypass the wrapper "
            "or stop unrelated processes"
        )
        print(
            f"waiting more than {LOCK_WAIT_DIAGNOSTIC_SECONDS:g}s "
            f"for {description} lock at {path}; known owner: {owner}; {recovery}",
            file=sys.stderr,
        )

    warning = threading.Thread(target=warn_about_wait, daemon=True)
    warning.start()
    try:
        yield
    finally:
        acquired.set()
        warning.join(timeout=0.1)


@contextmanager
def exclusive_lock(
    path: Path,
    description: str,
    nonblocking: bool = False,
    *,
    directory_fd: int | None = None,
    inherit: bool = True,
) -> Iterator[TextIO]:
    with _advisory_lock(
        path,
        description,
        fcntl.LOCK_EX,
        nonblocking=nonblocking,
        directory_fd=directory_fd,
    ) as lock:
        if inherit:
            with inherit_lock_fds(lock):
                yield lock
        else:
            yield lock


@contextmanager
def shared_lock(
    path: Path,
    description: str,
    nonblocking: bool = False,
    *,
    directory_fd: int | None = None,
) -> Iterator[TextIO]:
    operation = getattr(fcntl, "LOCK_SH", None)
    if not isinstance(operation, int):
        raise WorkspaceError(
            f"shared locking is unavailable for {description}; refusing to continue"
        )
    with _advisory_lock(
        path,
        description,
        operation,
        nonblocking=nonblocking,
        directory_fd=directory_fd,
    ) as lock:
        with inherit_lock_fds(lock):
            yield lock


def layout_writer_intent_path(path: Path) -> Path:
    return path.with_name(
        f"{path.stem}{LAYOUT_WRITER_INTENT_SUFFIX}{path.suffix}"
    )


def layout_writer_pending_path(path: Path) -> Path:
    return path.with_name(
        f"{path.stem}{LAYOUT_WRITER_PENDING_SUFFIX}{path.suffix}"
    )


@contextmanager
def exclusive_layout_lock(
    path: Path,
    description: str,
    nonblocking: bool = False,
    *,
    recovery_action: str | None = None,
    directory_fd: int | None = None,
) -> Iterator[TextIO]:
    with ExitStack() as locks:
        with _diagnose_layout_wait(path, description, recovery_action):
            locks.enter_context(
                shared_lock(
                    layout_writer_pending_path(path),
                    f"{description} writer pending",
                    nonblocking,
                    directory_fd=directory_fd,
                )
            )
            locks.enter_context(
                exclusive_lock(
                    layout_writer_intent_path(path),
                    f"{description} writer intent",
                    nonblocking,
                    directory_fd=directory_fd,
                )
            )
            lock = locks.enter_context(
                exclusive_lock(
                    path,
                    description,
                    nonblocking,
                    directory_fd=directory_fd,
                )
            )
        yield lock


@contextmanager
def shared_layout_lock(
    path: Path,
    description: str,
    nonblocking: bool = False,
    *,
    recovery_action: str | None = None,
    directory_fd: int | None = None,
    inherit: bool = True,
) -> Iterator[TextIO]:
    operation = getattr(fcntl, "LOCK_EX", None)
    if not isinstance(operation, int):
        raise WorkspaceError(
            f"exclusive locking is unavailable for {description} writer intent; "
            "refusing to continue"
        )
    shared_operation = getattr(fcntl, "LOCK_SH", None)
    if not isinstance(shared_operation, int):
        raise WorkspaceError(
            f"shared locking is unavailable for {description}; refusing to continue"
        )
    with ExitStack() as layout_stack:
        with _diagnose_layout_wait(path, description, recovery_action):
            pending_busy = False
            with _advisory_lock(
                layout_writer_intent_path(path),
                f"{description} reader admission",
                operation,
                nonblocking=nonblocking,
                directory_fd=directory_fd,
            ):
                pending_stack = ExitStack()
                try:
                    pending_stack.enter_context(
                        _advisory_lock(
                            layout_writer_pending_path(path),
                            f"{description} writer pending",
                            operation,
                            nonblocking=True,
                            directory_fd=directory_fd,
                        )
                    )
                except LockBusyError:
                    pending_stack.close()
                    pending_busy = True
                else:
                    with pending_stack:
                        lock = layout_stack.enter_context(
                            _advisory_lock(
                                path,
                                description,
                                shared_operation,
                                nonblocking=nonblocking,
                                directory_fd=directory_fd,
                            )
                        )
            if pending_busy:
                with _advisory_lock(
                    layout_writer_pending_path(path),
                    f"{description} writer pending",
                    operation,
                    nonblocking=nonblocking,
                    directory_fd=directory_fd,
                ):
                    with _advisory_lock(
                        layout_writer_intent_path(path),
                        f"{description} reader admission",
                        operation,
                        nonblocking=nonblocking,
                        directory_fd=directory_fd,
                    ):
                        lock = layout_stack.enter_context(
                            _advisory_lock(
                                path,
                                description,
                            shared_operation,
                            nonblocking=nonblocking,
                            directory_fd=directory_fd,
                            )
                        )
        if inherit:
            with inherit_lock_fds(lock):
                yield lock
        else:
            yield lock


def resource_lock_path(root: Path, kind: str, coordinate: str) -> Path:
    """Return a bounded, stable path for an exact resource coordinate."""

    if kind not in RESOURCE_KIND_ORDER:
        raise WorkspaceError(f"unknown resource lease kind: {kind}")
    digest = hashlib.sha256(f"{kind}\0{coordinate}".encode()).hexdigest()
    slug = re.sub(r"[^a-z0-9]+", "-", coordinate.lower()).strip("-")[:48]
    return root / "leases" / kind / f"{slug or 'resource'}-{digest[:20]}.lock"


def _ensure_resource_lock_directory(root: Path, kind: str) -> int:
    """Create and retain the exact no-follow lease-kind directory."""

    if not root.exists() and not root.is_symlink():
        try:
            root.mkdir(mode=0o700, parents=True)
        except FileExistsError:
            pass
        except OSError as error:
            raise WorkspaceError(
                f"cannot create resource lease directory {root}: {error}"
            ) from error
    root_descriptor = _open_directory(root, "resource lease")
    leases_descriptor: int | None = None
    try:
        leases_descriptor = _open_or_create_directory_at(
            root_descriptor, "leases", root / "leases"
        )
        return _open_or_create_directory_at(
            leases_descriptor, kind, root / "leases" / kind
        )
    finally:
        if leases_descriptor is not None:
            os.close(leases_descriptor)
        os.close(root_descriptor)


def _open_directory(path: Path, description: str) -> int:
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_DIRECTORY"):
        flags |= os.O_DIRECTORY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags)
        opened = os.fstat(descriptor)
        visible = path.stat(follow_symlinks=False)
    except OSError as error:
        if "descriptor" in locals():
            os.close(descriptor)
        raise WorkspaceError(
            f"cannot open {description} directory {path}: {error}"
        ) from error
    if (
        not stat.S_ISDIR(visible.st_mode)
        or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
    ):
        os.close(descriptor)
        raise WorkspaceError(f"{description} directory is unsafe: {path}")
    return descriptor


def _open_or_create_directory_at(
    parent_descriptor: int, name: str, display_path: Path
) -> int:
    try:
        os.mkdir(name, mode=0o700, dir_fd=parent_descriptor)
    except FileExistsError:
        pass
    except OSError as error:
        raise WorkspaceError(
            f"cannot create resource lease directory {display_path}: {error}"
        ) from error
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_DIRECTORY"):
        flags |= os.O_DIRECTORY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor: int | None = None
    try:
        descriptor = os.open(name, flags, dir_fd=parent_descriptor)
        opened = os.fstat(descriptor)
        visible = os.stat(
            name, dir_fd=parent_descriptor, follow_symlinks=False
        )
    except OSError as error:
        if descriptor is not None:
            os.close(descriptor)
        raise WorkspaceError(
            f"cannot open resource lease directory {display_path}: {error}"
        ) from error
    if (
        not stat.S_ISDIR(visible.st_mode)
        or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
    ):
        os.close(descriptor)
        raise WorkspaceError(
            f"resource lease directory is unsafe: {display_path}"
        )
    return descriptor


@contextmanager
def _resource_owner(
    path: Path,
    request: LeaseRequest,
    *,
    directory_fd: int | None = None,
    inherit: bool = True,
) -> Iterator[
    tuple[Callable[[], None], Callable[[Callable[[], None]], None]]
]:
    owners = path.with_name(f"{path.name}.owners")
    parent_descriptor = (
        os.dup(directory_fd)
        if directory_fd is not None
        else _open_directory(path.parent, "resource lease parent")
    )
    owner_descriptor: int | None = None
    try:
        try:
            os.mkdir(owners.name, mode=0o700, dir_fd=parent_descriptor)
        except FileExistsError:
            pass
        except OSError as error:
            raise WorkspaceError(
                f"cannot create resource lease owner directory {owners}: {error}"
            ) from error
        owner_flags = os.O_RDONLY | os.O_CLOEXEC
        if hasattr(os, "O_DIRECTORY"):
            owner_flags |= os.O_DIRECTORY
        if hasattr(os, "O_NOFOLLOW"):
            owner_flags |= os.O_NOFOLLOW
        owner_descriptor = os.open(
            owners.name, owner_flags, dir_fd=parent_descriptor
        )
        opened = os.fstat(owner_descriptor)
        visible = os.stat(
            owners.name, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if (
            not stat.S_ISDIR(visible.st_mode)
            or (opened.st_dev, opened.st_ino)
            != (visible.st_dev, visible.st_ino)
        ):
            os.close(owner_descriptor)
            owner_descriptor = None
            raise WorkspaceError(
                f"resource lease owner directory is unsafe: {owners}"
            )
    except (OSError, WorkspaceError) as error:
        if owner_descriptor is not None:
            os.close(owner_descriptor)
        os.close(parent_descriptor)
        if isinstance(error, WorkspaceError):
            raise
        raise WorkspaceError(
            f"cannot open resource lease owner directory {owners}: {error}"
        ) from error
    staging_descriptor: int | None = None
    try:
        staging_descriptor = _open_or_create_directory_at(
            owner_descriptor, ".pending", owners / ".pending"
        )
    except (OSError, WorkspaceError) as error:
        if staging_descriptor is not None:
            os.close(staging_descriptor)
        os.close(owner_descriptor)
        os.close(parent_descriptor)
        raise WorkspaceError(
            f"cannot open resource lease owner staging directory "
            f"{owners / '.pending'}: {error}"
        ) from error
    token = secrets.token_hex(16)
    metadata_path = owners / f"{token}.json"
    owner = f"{socket.gethostname()} uid={os.geteuid()} token={token}"
    value = {
        "schema_version": RESOURCE_LEASE_SCHEMA_VERSION,
        "kind": request.kind,
        "coordinate": request.coordinate,
        "mode": request.mode,
        "phase": "waiting",
        "operation": request.operation,
        "owner": owner,
        "started_at": datetime.now(timezone.utc).isoformat(),
        "recovery": request.recovery,
    }
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    publication_locked = False
    staging_locked = False
    descriptor: int | None = None
    metadata_created = False
    metadata_published = False
    try:
        fcntl.flock(staging_descriptor, fcntl.LOCK_EX)
        staging_locked = True
        fcntl.flock(owner_descriptor, fcntl.LOCK_EX)
        publication_locked = True
        descriptor = os.open(
            metadata_path.name, flags, 0o600, dir_fd=staging_descriptor
        )
        metadata_created = True
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with os.fdopen(descriptor, "w+", encoding="utf-8", closefd=False) as stream:
            json.dump(value, stream, sort_keys=True)
            stream.write("\n")
            stream.flush()
        os.link(
            metadata_path.name,
            metadata_path.name,
            src_dir_fd=staging_descriptor,
            dst_dir_fd=owner_descriptor,
            follow_symlinks=False,
        )
        metadata_published = True
        os.unlink(metadata_path.name, dir_fd=staging_descriptor)
        metadata_created = False
    except OSError as error:
        if descriptor is not None:
            os.close(descriptor)
            descriptor = None
        cleanup_error: OSError | None = None
        if metadata_published:
            try:
                os.unlink(metadata_path.name, dir_fd=owner_descriptor)
            except FileNotFoundError:
                pass
            except OSError as unlink_error:
                cleanup_error = unlink_error
        if metadata_created:
            try:
                os.unlink(metadata_path.name, dir_fd=staging_descriptor)
            except FileNotFoundError:
                pass
            except OSError as unlink_error:
                cleanup_error = cleanup_error or unlink_error
        publication_locked = False
        staging_locked = False
        os.close(staging_descriptor)
        os.close(owner_descriptor)
        os.close(parent_descriptor)
        cleanup_detail = (
            f"; cannot remove partial owner metadata: {cleanup_error}"
            if cleanup_error is not None
            else ""
        )
        raise WorkspaceError(
            f"cannot publish resource lease owner metadata {metadata_path}: "
            f"{error}{cleanup_detail}"
        ) from error
    finally:
        if publication_locked:
            fcntl.flock(owner_descriptor, fcntl.LOCK_UN)
        if staging_locked:
            fcntl.flock(staging_descriptor, fcntl.LOCK_UN)
    admitted = False
    release_main_lock: Callable[[], None] | None = None
    owner_lease = os.fdopen(os.dup(descriptor), "a+")
    inherit_stack = ExitStack()

    def admit() -> None:
        nonlocal admitted
        if admitted:
            return
        transition_error: OSError | None = None
        cleanup_error: OSError | None = None
        try:
            fcntl.flock(owner_descriptor, fcntl.LOCK_EX)
            value["phase"] = "admitted"
            os.lseek(descriptor, 0, os.SEEK_SET)
            os.ftruncate(descriptor, 0)
            with os.fdopen(
                descriptor, "w+", encoding="utf-8", closefd=False
            ) as stream:
                json.dump(value, stream, sort_keys=True)
                stream.write("\n")
                stream.flush()
        except OSError as error:
            transition_error = error
            try:
                os.unlink(metadata_path.name, dir_fd=owner_descriptor)
            except FileNotFoundError:
                pass
            except OSError as unlink_error:
                cleanup_error = unlink_error
        finally:
            fcntl.flock(owner_descriptor, fcntl.LOCK_UN)
        if transition_error is not None:
            cleanup_detail = (
                f"; cannot remove partial owner metadata: {cleanup_error}"
                if cleanup_error is not None
                else ""
            )
            raise WorkspaceError(
                f"cannot admit resource lease owner metadata {metadata_path}: "
                f"{transition_error}{cleanup_detail}"
            ) from transition_error
        if inherit:
            inherit_stack.enter_context(inherit_lock_fds(owner_lease))
        admitted = True

    def bind_release(callback: Callable[[], None]) -> None:
        nonlocal release_main_lock
        if release_main_lock is not None:
            raise WorkspaceError(
                f"resource lease owner release is already bound: {metadata_path}"
            )
        release_main_lock = callback

    try:
        yield admit, bind_release
    finally:
        teardown_errors: list[tuple[str, BaseException]] = []
        try:
            inherit_stack.close()
        except BaseException as error:
            teardown_errors.append(("cannot close inherited owner lease", error))
        try:
            owner_lease.close()
        except BaseException as error:
            teardown_errors.append(("cannot close owner lease", error))
        owner_directory_locked = False
        try:
            fcntl.flock(owner_descriptor, fcntl.LOCK_EX)
            owner_directory_locked = True
        except BaseException as error:
            teardown_errors.append(("cannot lock owner directory", error))
        release_error: BaseException | None = None
        if release_main_lock is not None:
            try:
                release_main_lock()
            except BaseException as error:
                release_error = error
        uncertain_published = False
        if release_error is not None and owner_directory_locked:
            uncertain_descriptor: int | None = None
            uncertain_name = (
                f".{metadata_path.name}.{secrets.token_hex(8)}.uncertain"
            )
            try:
                uncertain_value = {**value, "phase": "release-uncertain"}
                uncertain_flags = (
                    os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC
                )
                if hasattr(os, "O_NOFOLLOW"):
                    uncertain_flags |= os.O_NOFOLLOW
                uncertain_descriptor = os.open(
                    uncertain_name,
                    uncertain_flags,
                    0o600,
                    dir_fd=owner_descriptor,
                )
                fcntl.flock(
                    uncertain_descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB
                )
                with os.fdopen(
                    uncertain_descriptor, "w+", encoding="utf-8", closefd=False
                ) as stream:
                    json.dump(uncertain_value, stream, sort_keys=True)
                    stream.write("\n")
                    stream.flush()
                os.replace(
                    uncertain_name,
                    metadata_path.name,
                    src_dir_fd=owner_descriptor,
                    dst_dir_fd=owner_descriptor,
                )
                previous_descriptor = descriptor
                descriptor = uncertain_descriptor
                uncertain_descriptor = None
                uncertain_published = True
                try:
                    os.close(previous_descriptor)
                except OSError as error:
                    teardown_errors.append(
                        ("cannot close replaced owner metadata", error)
                    )
            except (OSError, UnicodeError, TypeError, ValueError) as error:
                teardown_errors.append(
                    ("cannot publish release-uncertain owner metadata", error)
                )
            finally:
                if uncertain_descriptor is not None:
                    try:
                        os.close(uncertain_descriptor)
                    except OSError as error:
                        teardown_errors.append(
                            ("cannot close uncertain owner metadata", error)
                        )
                    try:
                        os.unlink(uncertain_name, dir_fd=owner_descriptor)
                    except FileNotFoundError:
                        pass
                    except OSError as error:
                        teardown_errors.append(
                            (
                                "cannot remove partial release-uncertain "
                                "owner metadata",
                                error,
                            )
                        )
        if release_error is None:
            try:
                os.close(descriptor)
            except OSError as error:
                teardown_errors.append(("cannot close owner metadata", error))
        cleanup_descriptor: int | None = None
        try:
            if owner_directory_locked and release_error is None:
                cleanup_flags = os.O_RDWR | os.O_CLOEXEC
                if hasattr(os, "O_NOFOLLOW"):
                    cleanup_flags |= os.O_NOFOLLOW
                cleanup_descriptor = os.open(
                    metadata_path.name,
                    cleanup_flags,
                    dir_fd=owner_descriptor,
                )
                fcntl.flock(
                    cleanup_descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB
                )
                os.unlink(metadata_path.name, dir_fd=owner_descriptor)
        except (FileNotFoundError, BlockingIOError):
            # An inherited descriptor deliberately keeps the owner record live;
            # a later diagnostic scan reaps it after the child exits.
            pass
        except OSError as error:
            teardown_errors.append(("cannot remove owner metadata", error))
        finally:
            if cleanup_descriptor is not None:
                os.close(cleanup_descriptor)
            try:
                os.unlink(metadata_path.name, dir_fd=staging_descriptor)
            except FileNotFoundError:
                pass
            except OSError as error:
                teardown_errors.append(
                    ("cannot remove staged owner metadata", error)
                )
            os.close(staging_descriptor)
            os.close(owner_descriptor)
            os.close(parent_descriptor)
        if release_error is not None:
            detail = "; ".join(
                f"{label}: {error}" for label, error in teardown_errors
            )
            suffix = f"; {detail}" if detail else ""
            raise WorkspaceError(
                f"cannot confirm main resource lease release for "
                f"{metadata_path}; retained locked owner metadata is "
                f"{'release uncertain' if uncertain_published else 'admitted'}: "
                f"{release_error}{suffix}"
            ) from release_error
        if teardown_errors:
            detail = "; ".join(
                f"{label}: {error}" for label, error in teardown_errors
            )
            raise WorkspaceError(
                f"cannot finalize resource lease owner metadata "
                f"{metadata_path}; main lease released; {detail}"
            ) from teardown_errors[0][1]


@contextmanager
def resource_locks(
    root: Path | Callable[[LeaseRequest], Path],
    requests: list[LeaseRequest] | tuple[LeaseRequest, ...],
    *,
    nonblocking: bool = False,
) -> Iterator[tuple[TextIO, ...]]:
    """Acquire de-duplicated exact resources in deterministic graph order."""

    combined: dict[tuple[str, str], LeaseRequest] = {}
    for request in requests:
        identity = (request.kind, request.coordinate)
        previous = combined.get(identity)
        if previous is None:
            combined[identity] = request
            continue
        mode = (
            "exclusive"
            if "exclusive" in {previous.mode, request.mode}
            else "shared"
        )
        combined[identity] = LeaseRequest(
            request.kind,
            request.coordinate,
            mode,
            ", ".join(dict.fromkeys((previous.operation, request.operation))),
            request.recovery,
        )
    ordered = sorted(combined.values(), key=lambda request: request.sort_key)
    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        # Windows has no descriptor-relative directory API.  Use the same
        # kernel-backed LockFileEx leases and fair layout lock sequence, but do
        # not publish POSIX owner sidecars whose identity cannot be proven
        # without directory descriptors.  A held kernel lock is still the
        # authority for admission and release.
        with ExitStack() as stack:
            leases: list[TextIO] = []
            for request in ordered:
                request_root = root(request) if callable(root) else root
                path = resource_lock_path(request_root, request.kind, request.coordinate)
                description = (
                    f"resource {request.kind} coordinate {request.coordinate}"
                )
                if request.mode == "exclusive":
                    context = exclusive_layout_lock(
                        path,
                        description,
                        nonblocking,
                        recovery_action=request.recovery,
                    )
                else:
                    context = shared_layout_lock(
                        path,
                        description,
                        nonblocking,
                        recovery_action=request.recovery,
                    )
                leases.append(stack.enter_context(context))
            yield tuple(leases)
        return
    with ExitStack() as stack:
        leases: list[TextIO] = []
        for request in ordered:
            request_root = root(request) if callable(root) else root
            directory_descriptor = _ensure_resource_lock_directory(
                request_root, request.kind
            )
            stack.callback(os.close, directory_descriptor)
            path = resource_lock_path(
                request_root, request.kind, request.coordinate
            )
            description = (
                f"resource {request.kind} coordinate {request.coordinate}"
            )
            transition = path.with_name(
                f"{path.name}.owner-transition.lock"
            )
            request_stack = ExitStack()
            try:
                admit, bind_release = request_stack.enter_context(
                    _resource_owner(
                        path, request, directory_fd=directory_descriptor
                    )
                )
                if request.mode == "exclusive":
                    layout_context = exclusive_layout_lock(
                        path,
                        description,
                        nonblocking,
                        recovery_action=request.recovery,
                        directory_fd=directory_descriptor,
                    )
                else:
                    layout_context = shared_layout_lock(
                        path,
                        description,
                        nonblocking,
                        recovery_action=request.recovery,
                        directory_fd=directory_descriptor,
                    )
                lease = layout_context.__enter__()
                bind_release(
                    lambda context=layout_context: context.__exit__(
                        None, None, None
                    )
                )
                with exclusive_lock(
                    transition,
                    "resource lease owner transition",
                    directory_fd=directory_descriptor,
                    inherit=False,
                ):
                    try:
                        admit()
                    except BaseException:
                        request_stack.close()
                        raise
            except LockBusyError as error:
                request_stack.close()
                # Do not retain earlier coordinates while producing the busy
                # coordinate's stable owner summary. This preserves the
                # all-or-none contract even when a publication is transitioning.
                stack.close()
                raise LockBusyError(
                    f"{request.kind} {request.coordinate} is already in use by "
                    f"{_lease_owner_summary(path)}; {request.recovery}"
                ) from error
            except BaseException:
                request_stack.close()
                raise
            stack.enter_context(request_stack.pop_all())
            leases.append(lease)
        yield tuple(leases)


@contextmanager
def resource_lifetime_reader(
    root: Path,
    request: LeaseRequest,
) -> Iterator[TextIO]:
    """Hold a fair diagnosed resource reader without subprocess registration."""

    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        path = resource_lock_path(root, request.kind, request.coordinate)
        with shared_layout_lock(
            path,
            f"resource {request.kind} coordinate {request.coordinate}",
            recovery_action=request.recovery,
        ) as lease:
            yield lease
        return

    directory_fd = _ensure_resource_lock_directory(root, request.kind)
    path = resource_lock_path(root, request.kind, request.coordinate)
    transition = path.with_name(f"{path.name}.owner-transition.lock")
    try:
        with ExitStack() as stack:
            admit, bind_release = stack.enter_context(
                _resource_owner(
                    path,
                    request,
                    directory_fd=directory_fd,
                    inherit=False,
                )
            )
            layout_context = shared_layout_lock(
                path,
                f"resource {request.kind} coordinate {request.coordinate}",
                recovery_action=request.recovery,
                directory_fd=directory_fd,
                inherit=False,
            )
            lease = layout_context.__enter__()
            bind_release(
                lambda context=layout_context: context.__exit__(
                    None, None, None
                )
            )
            with exclusive_lock(
                transition,
                "resource lease owner transition",
                directory_fd=directory_fd,
                inherit=False,
            ):
                try:
                    admit()
                except BaseException:
                    stack.close()
                    raise
            yield lease
    finally:
        os.close(directory_fd)
