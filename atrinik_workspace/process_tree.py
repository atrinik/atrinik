from __future__ import annotations

import os
from pathlib import Path
import signal
import stat
from typing import Iterable

from .path_identity import canonical_path, descriptor_path
from .platform_compat import fcntl


def control_socket_path(topology_root: Path, generation: str) -> Path:
    """Return the bounded workspace-shared endpoint for one topology generation."""
    workspace = topology_root.parent.parent
    path = workspace / "c" / generation[:12]
    if len(os.fsencode(path)) > 107:
        raise OSError(f"workspace path is too long for topology control: {workspace}")
    return path


def initialize_lease(descriptor: int, generation: str) -> dict[str, object]:
    """Bind a locked lease path to one topology generation."""
    payload = f"{generation}\n".encode()
    os.ftruncate(descriptor, 0)
    os.lseek(descriptor, 0, os.SEEK_SET)
    if os.write(descriptor, payload) != len(payload):
        raise OSError("short write while initializing process-tree lease")
    os.fsync(descriptor)
    return {"path": descriptor_path(descriptor)}


def bound_lease_locked(
    path: Path, generation: str, identity: dict[str, object]
) -> bool:
    """Observe the exact generation-bound lease named by a status record."""
    if not isinstance(identity, dict) or (
        "path" in identity and identity["path"] != canonical_path(path)
    ):
        raise OSError(f"process-tree lease path changed: {path}")
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags)
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise OSError(f"process-tree lease is not a regular file: {path}")
        if descriptor_path(descriptor) != canonical_path(path):
            raise OSError(f"process-tree lease path changed: {path}")
        os.lseek(descriptor, 0, os.SEEK_SET)
        if os.read(descriptor, 66) != f"{generation}\n".encode():
            raise OSError(f"process-tree lease generation changed: {path}")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
        fcntl.flock(descriptor, fcntl.LOCK_UN)
        return False
    finally:
        os.close(descriptor)


def lease_locked(path: Path) -> bool:
    """Observe one inherited lease without depending on visible process IDs."""
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags)
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            raise OSError(f"process-tree lease is not a regular file: {path}")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return True
        fcntl.flock(descriptor, fcntl.LOCK_UN)
        return False
    finally:
        os.close(descriptor)


def _holds_lease(pid: int, path: str, payload: bytes) -> bool:
    directory = Path("/proc") / str(pid) / "fd"
    try:
        descriptors = list(directory.iterdir())
    except OSError:
        return False
    for descriptor in descriptors:
        try:
            flags_line = next(
                line
                for line in (
                    directory.parent / "fdinfo" / descriptor.name
                ).read_text().splitlines()
                if line.startswith("flags:")
            )
            flags = int(flags_line.split()[1], 8)
        except OSError:
            continue
        except (StopIteration, ValueError):
            continue
        # O_PATH descriptors are observers, not inherited process-tree leases.
        # This lets the controlling `down` process inspect and signal holders
        # without becoming a target of the supervisor's own descendant cleanup.
        if flags & getattr(os, "O_PATH", 0):
            continue
        # Services inherit the topology's read/write open-file description.
        # Read-only liveness observers must never become cleanup targets.
        if flags & os.O_ACCMODE != os.O_RDWR:
            continue
        try:
            target = os.readlink(descriptor)
            if target.endswith(" (deleted)") or canonical_path(target) != path:
                continue
            observer = os.open(descriptor, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
            try:
                if (
                    stat.S_ISREG(os.fstat(observer).st_mode)
                    and descriptor_path(observer) == path
                    and os.pread(observer, 66, 0) == payload
                ):
                    return True
            finally:
                os.close(observer)
        except OSError:
            continue
    return False


def _lease_coordinate(lease_fd: int) -> tuple[str, bytes]:
    """Read generation evidence even when the caller holds an O_PATH observer."""
    path = descriptor_path(lease_fd)
    observer = os.open(
        Path("/proc/self/fd") / str(lease_fd),
        os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC,
    )
    try:
        if not stat.S_ISREG(os.fstat(observer).st_mode):
            raise OSError(f"process-tree lease is not a regular file: {path}")
        if descriptor_path(observer) != path or descriptor_path(lease_fd) != path:
            raise OSError(f"process-tree lease path changed: {path}")
        return path, os.pread(observer, 66, 0)
    finally:
        os.close(observer)


def signal_holders(
    lease_fd: int,
    signum: signal.Signals,
    *,
    exclude: Iterable[int] = (),
) -> int:
    """Signal live holders of one inherited process-tree lease via pidfds."""
    path, payload = _lease_coordinate(lease_fd)
    excluded = set(exclude)
    signaled = 0
    try:
        entries = list(Path("/proc").iterdir())
    except OSError:
        return 0
    for entry in entries:
        if not entry.name.isdigit():
            continue
        pid = int(entry.name)
        if pid in excluded or not _holds_lease(pid, path, payload):
            continue
        try:
            pidfd = os.pidfd_open(pid)
        except ProcessLookupError:
            continue
        try:
            if not _holds_lease(pid, path, payload):
                continue
            try:
                signal.pidfd_send_signal(pidfd, signum)
            except ProcessLookupError:
                continue
            signaled += 1
        finally:
            os.close(pidfd)
    return signaled


def holders_exist(lease_fd: int, *, exclude: Iterable[int] = ()) -> bool:
    path, payload = _lease_coordinate(lease_fd)
    excluded = set(exclude)
    try:
        entries = list(Path("/proc").iterdir())
    except OSError:
        return False
    return any(
        entry.name.isdigit()
        and int(entry.name) not in excluded
        and _holds_lease(int(entry.name), path, payload)
        for entry in entries
    )
