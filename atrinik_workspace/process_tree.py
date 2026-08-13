from __future__ import annotations

import fcntl
import os
from pathlib import Path
import signal
import stat
from typing import Iterable


def initialize_lease(descriptor: int, generation: str) -> dict[str, int]:
    """Bind a locked lease inode to one topology generation."""
    payload = f"{generation}\n".encode()
    os.ftruncate(descriptor, 0)
    os.lseek(descriptor, 0, os.SEEK_SET)
    if os.write(descriptor, payload) != len(payload):
        raise OSError("short write while initializing process-tree lease")
    os.fsync(descriptor)
    metadata = os.fstat(descriptor)
    return {"device": metadata.st_dev, "inode": metadata.st_ino}


def bound_lease_locked(
    path: Path, generation: str, identity: dict[str, int]
) -> bool:
    """Observe the exact generation-bound lease named by a status record."""
    if (
        set(identity) != {"device", "inode"}
        or not all(
            isinstance(value, int) and not isinstance(value, bool) and value >= 0
            for value in identity.values()
        )
    ):
        raise OSError("process-tree lease identity is invalid")
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags)
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            raise OSError(f"process-tree lease is not a regular file: {path}")
        if (metadata.st_dev, metadata.st_ino) != (
            identity["device"],
            identity["inode"],
        ):
            raise OSError(f"process-tree lease identity changed: {path}")
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


def _holds_identity(pid: int, identity: tuple[int, int]) -> bool:
    directory = Path("/proc") / str(pid) / "fd"
    try:
        descriptors = list(directory.iterdir())
    except OSError:
        return False
    for descriptor in descriptors:
        try:
            metadata = descriptor.stat()
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
        if (metadata.st_dev, metadata.st_ino) == identity:
            return True
    return False


def signal_holders(
    lease_fd: int,
    signum: signal.Signals,
    *,
    exclude: Iterable[int] = (),
) -> int:
    """Signal live holders of one inherited process-tree lease via pidfds."""
    metadata = os.fstat(lease_fd)
    identity = (metadata.st_dev, metadata.st_ino)
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
        if pid in excluded or not _holds_identity(pid, identity):
            continue
        try:
            pidfd = os.pidfd_open(pid)
        except ProcessLookupError:
            continue
        try:
            if not _holds_identity(pid, identity):
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
    metadata = os.fstat(lease_fd)
    identity = (metadata.st_dev, metadata.st_ino)
    excluded = set(exclude)
    try:
        entries = list(Path("/proc").iterdir())
    except OSError:
        return False
    return any(
        entry.name.isdigit()
        and int(entry.name) not in excluded
        and _holds_identity(int(entry.name), identity)
        for entry in entries
    )
