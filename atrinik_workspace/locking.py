from __future__ import annotations

from contextlib import ExitStack, contextmanager
from contextvars import ContextVar
import fcntl
import os
from pathlib import Path
import stat
import sys
import threading
from typing import BinaryIO, Iterator, TextIO

from .model import WorkspaceError


LAYOUT_WRITER_INTENT_SUFFIX = ".writer-intent"
LAYOUT_WRITER_PENDING_SUFFIX = ".writer-pending"
LOCK_WAIT_DIAGNOSTIC_SECONDS = 10.0


class LockBusyError(WorkspaceError):
    """Raised when a requested nonblocking advisory lock is held."""


_ACTIVE_LOCK_FDS: ContextVar[tuple[int, ...]] = ContextVar(
    "atrinik_active_lock_fds", default=()
)


def active_lock_fds() -> tuple[int, ...]:
    return _ACTIVE_LOCK_FDS.get()


@contextmanager
def inherit_lock_fds(*leases: BinaryIO) -> Iterator[None]:
    descriptors = tuple(lease.fileno() for lease in leases)
    current = active_lock_fds()
    token = _ACTIVE_LOCK_FDS.set(tuple(dict.fromkeys((*current, *descriptors))))
    try:
        yield
    finally:
        _ACTIVE_LOCK_FDS.reset(token)


def _open_lock(path: Path, description: str) -> TextIO:
    flags = os.O_RDWR | os.O_CREAT | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags, 0o600)
    except OSError as error:
        raise WorkspaceError(
            f"cannot open {description} lock {path}: {error}"
        ) from error
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
) -> Iterator[TextIO]:
    path.parent.mkdir(parents=True, exist_ok=True)
    with _open_lock(path, description) as lock:
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


@contextmanager
def _diagnose_layout_wait(path: Path, description: str) -> Iterator[None]:
    acquired = threading.Event()

    def warn_about_wait() -> None:
        if acquired.wait(LOCK_WAIT_DIAGNOSTIC_SECONDS):
            return
        print(
            f"waiting more than {LOCK_WAIT_DIAGNOSTIC_SECONDS:g}s "
            f"for {description} lock at {path}; inspect "
            "`./atrinik ps --json` and "
            "`./atrinik worktree list --json`; do not bypass the "
            "wrapper or stop unrelated processes",
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
) -> Iterator[TextIO]:
    with _advisory_lock(
        path,
        description,
        fcntl.LOCK_EX,
        nonblocking=nonblocking,
    ) as lock:
        with inherit_lock_fds(lock):
            yield lock


@contextmanager
def shared_lock(
    path: Path,
    description: str,
    nonblocking: bool = False,
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
    path: Path, description: str, nonblocking: bool = False
) -> Iterator[TextIO]:
    with ExitStack() as locks:
        with _diagnose_layout_wait(path, description):
            locks.enter_context(
                shared_lock(
                    layout_writer_pending_path(path),
                    f"{description} writer pending",
                    nonblocking,
                )
            )
            locks.enter_context(
                exclusive_lock(
                    layout_writer_intent_path(path),
                    f"{description} writer intent",
                    nonblocking,
                )
            )
            lock = locks.enter_context(
                exclusive_lock(
                    path,
                    description,
                    nonblocking,
                )
            )
        yield lock


@contextmanager
def shared_layout_lock(path: Path, description: str) -> Iterator[TextIO]:
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
        with _diagnose_layout_wait(path, description):
            pending_busy = False
            with _advisory_lock(
                layout_writer_intent_path(path),
                f"{description} reader admission",
                operation,
            ):
                pending_stack = ExitStack()
                try:
                    pending_stack.enter_context(
                        _advisory_lock(
                            layout_writer_pending_path(path),
                            f"{description} writer pending",
                            operation,
                            nonblocking=True,
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
                            )
                        )
            if pending_busy:
                with _advisory_lock(
                    layout_writer_pending_path(path),
                    f"{description} writer pending",
                    operation,
                ):
                    with _advisory_lock(
                        layout_writer_intent_path(path),
                        f"{description} reader admission",
                        operation,
                    ):
                        lock = layout_stack.enter_context(
                            _advisory_lock(
                                path,
                                description,
                                shared_operation,
                            )
                        )
        with inherit_lock_fds(lock):
            yield lock
