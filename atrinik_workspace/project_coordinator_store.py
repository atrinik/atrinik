"""Bounded no-follow project state with stable locks and compare-and-swap."""
from __future__ import annotations

from contextlib import contextmanager
try:
    import fcntl
except ImportError:  # Importable on Windows; mutation has a stable capability error.
    fcntl = None
import hashlib
import json
import os
from pathlib import Path
import stat

from .project_coordinator import ProjectError, canonical, require, validate_project
from .workspace import durable_atomic_json_at, durable_replace_json_at

LIMIT = 2 * 1024 * 1024


def check_publication_size(document: dict) -> None:
    # Match the shared durable JSON writer, not the compact hashing form.
    raw = (json.dumps(document, indent=2, sort_keys=True, allow_nan=False) + "\n").encode("utf-8")
    require(len(raw) <= LIMIT, "project exceeds persisted byte bound")


def decode(raw: bytes, limit: int = LIMIT):
    require(len(raw) <= limit, "project input exceeds bound")

    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result

    try:
        return json.loads(raw, object_pairs_hook=pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(ProjectError("nonfinite JSON")))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise ProjectError("invalid bounded project JSON") from error


def read_input(path: Path, limit: int = LIMIT):
    require(path.is_absolute(), "input path must be absolute")
    parent = open_directory(path.parent)
    try:
        fd = os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=parent)
        try:
            st = os.fstat(fd)
            require(stat.S_ISREG(st.st_mode) and st.st_nlink == 1 and st.st_size <= limit
                    and st.st_uid == os.geteuid() and not st.st_mode & 0o022,
                    "unsafe input")
            raw = os.read(fd, limit + 1)
            after = os.fstat(fd)
            require((after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) ==
                    (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns),
                    "input changed while reading")
            return decode(raw, limit)
        finally:
            os.close(fd)
    finally:
        os.close(parent)


def open_directory(path: Path) -> int:
    require(path.is_absolute() and ".." not in path.parts, "absolute normalized directory required")
    fd = os.open("/", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in path.parts[1:]:
            new = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = new
            st = os.fstat(fd)
            require(st.st_uid in {0, os.geteuid()}, "foreign directory owner")
            require(not st.st_mode & 0o022 or st.st_uid == 0 and st.st_mode & stat.S_ISVTX,
                    "group/world writable directory")
        return fd
    except BaseException:
        os.close(fd)
        raise


class Store:
    """One coordinator's state. It never writes a leaf delivery ledger."""

    def __init__(self, root: Path):
        self.root = root

    @contextmanager
    def locked(self, create: bool = False):
        require(fcntl is not None, "project delivery requires Linux/POSIX locks")
        fd = open_directory(self.root)
        lock = None
        try:
            st = os.fstat(fd)
            require(st.st_uid == os.geteuid() and not st.st_mode & 0o077,
                    "project root must be owned mode 0700")
            flags = os.O_RDWR | os.O_NOFOLLOW | (os.O_CREAT | os.O_EXCL if create else 0)
            lock = os.open("project.lock", flags, 0o600, dir_fd=fd)
            ls = os.fstat(lock)
            require(stat.S_ISREG(ls.st_mode) and ls.st_uid == os.geteuid()
                    and stat.S_IMODE(ls.st_mode) == 0o600 and ls.st_nlink == 1, "unsafe project lock")
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise ProjectError("project busy; retry after owner operation") from error
            def check():
                current = os.stat("project.lock", dir_fd=fd, follow_symlinks=False)
                require((current.st_dev, current.st_ino) == (ls.st_dev, ls.st_ino), "lock replaced")
                root_st = self.root.stat(follow_symlinks=False)
                require((root_st.st_dev, root_st.st_ino) == (st.st_dev, st.st_ino), "root replaced")
            check()
            yield fd, check
            check()
        finally:
            if lock is not None:
                os.close(lock)
            os.close(fd)

    @staticmethod
    def _read(fd: int) -> dict:
        f = os.open("project.json", os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
        try:
            st = os.fstat(f)
            require(stat.S_ISREG(st.st_mode) and st.st_uid == os.geteuid()
                    and stat.S_IMODE(st.st_mode) == 0o600 and st.st_nlink == 1
                    and st.st_size <= LIMIT, "unsafe project record")
            raw = os.read(f, LIMIT + 1)
            after = os.fstat(f)
            require((after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) ==
                    (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns),
                    "project changed while reading")
            doc = decode(raw)
            validate_project(doc)
            return {"document": doc, "digest": hashlib.sha256(raw).hexdigest(),
                    "device": st.st_dev, "inode": st.st_ino, "generation": doc["generation"]}
        finally:
            os.close(f)

    def create(self, document: dict) -> dict:
        validate_project(document)
        check_publication_size(document)
        with self.locked(create=True) as (fd, check):
            require(not os.listdir(fd) or set(os.listdir(fd)) == {"project.lock"},
                    "nonempty project root; preserve uncertain initialization")
            check()
            durable_atomic_json_at(fd, "project.json", document)
            return self._read(fd)

    def inspect(self) -> dict:
        with self.locked() as (fd, _):
            return self._read(fd)

    def update(self, expected: dict, change) -> tuple[dict, object]:
        with self.locked() as (fd, check):
            old = self._read(fd)
            require(all(expected.get(k) == old[k] for k in ("generation", "digest", "device", "inode")),
                    "stale project CAS")
            document = decode(canonical(old["document"]))
            result = change(document)
            for key in ("schema_version", "actor", "authority"):
                require(document[key] == old["document"][key], "authority cannot change")
            document["generation"] = old["generation"] + 1
            document["previous"] = old["digest"]
            validate_project(document)
            check_publication_size(document)
            check()
            now = self._read(fd)
            require(all(now[k] == old[k] for k in ("generation", "digest", "device", "inode")),
                    "project changed during transaction")
            durable_replace_json_at(fd, "project.json", document)
            return self._read(fd), result
