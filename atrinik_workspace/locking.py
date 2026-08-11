from __future__ import annotations

from contextlib import contextmanager
from contextvars import ContextVar
from typing import BinaryIO, Iterator


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
