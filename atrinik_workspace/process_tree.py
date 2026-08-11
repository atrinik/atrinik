from __future__ import annotations

import os
from pathlib import Path
import signal
from typing import Iterable


def _holds_identity(pid: int, identity: tuple[int, int]) -> bool:
    directory = Path("/proc") / str(pid) / "fd"
    try:
        descriptors = list(directory.iterdir())
    except OSError:
        return False
    for descriptor in descriptors:
        try:
            metadata = descriptor.stat()
        except OSError:
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
