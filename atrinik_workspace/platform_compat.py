"""Small operating-system adapters used by the workspace boundary.

The wrapper is intentionally conservative about platform differences.  Native
Windows gets a real kernel byte-range lock and handle inheritance for the
commands that are supported there; Linux-only process-tree operations remain
explicitly unavailable instead of being approximated.
"""

from __future__ import annotations

from contextlib import contextmanager
import errno
import os
from pathlib import Path
import subprocess
from types import SimpleNamespace
from typing import Iterator


IS_WINDOWS = os.name == "nt"
O_CLOEXEC = getattr(os, "O_CLOEXEC", 0)
O_BINARY = getattr(os, "O_BINARY", 0)
_WINDOWS_LINK_REPARSE_TAGS = {0xA0000003, 0xA000000C}


class PlatformCapabilityError(RuntimeError):
    """A deliberately unavailable platform capability."""


def require_linux_capability(operation: str, capability: str) -> None:
    """Refuse a Linux-only operation with a stable, actionable diagnostic."""

    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        raise PlatformCapabilityError(
            f"{operation} is unavailable on native Windows: {capability} "
            "requires Linux process and filesystem capabilities; use Linux, "
            "WSL2, or the documented Windows package workflow"
        )


def assert_no_symlink_components(path: Path, description: str) -> None:
    """Reject symlink/junction components in a native Windows path."""

    absolute = Path(os.path.abspath(path))
    current = Path(absolute.anchor)
    for part in absolute.parts[1:]:
        current /= part
        try:
            status = current.lstat()
        except FileNotFoundError:
            continue
        except OSError as error:
            raise OSError(
                f"cannot inspect {description} path {current}: {error}"
            ) from error
        is_junction = getattr(current, "is_junction", lambda: False)()
        is_windows_link = getattr(status, "st_reparse_tag", 0) in _WINDOWS_LINK_REPARSE_TAGS
        if os.path.islink(current) or is_junction or is_windows_link:
            raise OSError(f"refusing symlinked {description} path: {current}")
        if current != absolute and not os.path.isdir(current):
            raise OSError(f"{description} parent is not a directory: {current}")
        del status


if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
    import ctypes
    from ctypes import wintypes
    import msvcrt

    LOCK_SH = 1
    LOCK_EX = 2
    LOCK_NB = 4
    LOCK_UN = 8

    _LOCKFILE_EXCLUSIVE_LOCK = 0x00000002
    _LOCKFILE_FAIL_IMMEDIATELY = 0x00000001
    _ERROR_LOCK_VIOLATION = 33
    _INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

    class _Overlapped(ctypes.Structure):
        _fields_ = [
            ("Internal", ctypes.c_size_t),
            ("InternalHigh", ctypes.c_size_t),
            ("Offset", wintypes.DWORD),
            ("OffsetHigh", wintypes.DWORD),
            ("hEvent", wintypes.HANDLE),
        ]

    _kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    _lock_file_ex = _kernel32.LockFileEx
    _lock_file_ex.argtypes = [
        wintypes.HANDLE,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        ctypes.POINTER(_Overlapped),
    ]
    _lock_file_ex.restype = wintypes.BOOL
    _unlock_file_ex = _kernel32.UnlockFileEx
    _unlock_file_ex.argtypes = [
        wintypes.HANDLE,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        ctypes.POINTER(_Overlapped),
    ]
    _unlock_file_ex.restype = wintypes.BOOL
    _flush_file_buffers = _kernel32.FlushFileBuffers
    _flush_file_buffers.argtypes = [wintypes.HANDLE]
    _flush_file_buffers.restype = wintypes.BOOL

    def _lock_handle(value: object) -> wintypes.HANDLE:
        descriptor = value if isinstance(value, int) else value.fileno()  # type: ignore[union-attr]
        handle = msvcrt.get_osfhandle(descriptor)
        if handle == -1 or handle == _INVALID_HANDLE_VALUE:
            raise OSError(errno.EBADF, "invalid Windows lock handle")
        return wintypes.HANDLE(handle)

    def _win_flock(value: object, operation: int) -> None:
        handle = _lock_handle(value)
        overlapped = _Overlapped()
        if operation & LOCK_UN:
            if not _unlock_file_ex(handle, 0, 0xFFFFFFFF, 0xFFFFFFFF, ctypes.byref(overlapped)):
                error = ctypes.get_last_error()
                raise OSError(error, os.strerror(error))
            return
        flags = 0
        if operation & LOCK_EX:
            flags |= _LOCKFILE_EXCLUSIVE_LOCK
        if operation & LOCK_NB:
            flags |= _LOCKFILE_FAIL_IMMEDIATELY
        if not _lock_file_ex(
            handle,
            flags,
            0,
            0xFFFFFFFF,
            0xFFFFFFFF,
            ctypes.byref(overlapped),
        ):
            error = ctypes.get_last_error()
            if error == _ERROR_LOCK_VIOLATION:
                raise BlockingIOError(errno.EAGAIN, "Windows file lock is busy")
            raise OSError(error, os.strerror(error))

    fcntl = SimpleNamespace(
        LOCK_SH=LOCK_SH,
        LOCK_EX=LOCK_EX,
        LOCK_NB=LOCK_NB,
        LOCK_UN=LOCK_UN,
        flock=_win_flock,
    )

    def flush_file(descriptor: int) -> None:
        handle = _lock_handle(descriptor)
        if not _flush_file_buffers(handle):
            error = ctypes.get_last_error()
            raise OSError(error, os.strerror(error))

    @contextmanager
    def inherited_subprocess_handles(
        descriptors: tuple[int, ...],
    ) -> Iterator[dict[str, object]]:
        """Pass active lock handles to a Windows child process."""

        handles: list[int] = []
        previous: list[tuple[int, bool]] = []
        try:
            for descriptor in dict.fromkeys(descriptors):
                handle = msvcrt.get_osfhandle(descriptor)
                if handle == -1 or handle == _INVALID_HANDLE_VALUE:
                    raise OSError(errno.EBADF, "invalid inherited Windows handle")
                inheritable = os.get_handle_inheritable(handle)
                previous.append((handle, inheritable))
                os.set_handle_inheritable(handle, True)
                handles.append(handle)
            if not handles:
                yield {"close_fds": True}
                return
            startup = subprocess.STARTUPINFO()
            startup.lpAttributeList = {"handle_list": handles}
            yield {"close_fds": True, "startupinfo": startup}
        finally:
            for handle, inheritable in previous:
                os.set_handle_inheritable(handle, inheritable)

else:
    import fcntl

    def flush_file(descriptor: int) -> None:
        os.fsync(descriptor)

    @contextmanager
    def inherited_subprocess_handles(
        descriptors: tuple[int, ...],
    ) -> Iterator[dict[str, object]]:
        yield {"pass_fds": descriptors}
