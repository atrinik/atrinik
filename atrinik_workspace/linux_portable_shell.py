# Copyright (c) 2026 Atrinik contributors
# SPDX-License-Identifier: MIT
"""Admit the one measured /bin/sh chain in the fixed portable SDK.

Only six known filesystem entries are inspected; no resolver, shell or ELF
inspector is executed. The OS loader, libraries and standard data remain trusted.
These checks do not attest historical execution or a privileged swap-and-restore.
"""
from contextlib import ExitStack
import hashlib
import json
import os
import stat
import struct
import sys


class LinuxPortableShellError(ValueError):
    """The fixed command shell is missing, unsafe or changed."""


_IMAGE = "ghcr.io/atrinik/classic-portable-build@sha256:74761e930f5d57face2d81efb0dc50dd4ce32ce1204810e56580bf68a5972ebc"
_PLATFORM = "sha256:5e30957e54277bdaadeeffa42246258dc9df2086f3fb41f2125d041735ac72aa"
_CONFIG = "sha256:c35dbeac2679d79dab524a13c9a3b74a8187878ecd3b3dff52bd2892418d7a79"
_PRODUCER = "eb1f6af78216adb4375826cdb70ef78a61ab403a"
_CONSUMER = "bd5317a03cdc82f029105c5dc26428b09fcd54b3"
_SHA256 = "f5adb8bf0100ed0f8c7782ca5f92814e9229525a4b4e0d401cf3bea09ac960a6"
_SIZE = 125640
# Public, immutable collector provenance; no private evidence path is required.
_PROVENANCE = {
    "repository": "atrinik/atrinik",
    "collector_commit": "6ef68fb3dc46e482fc4a0ccd6dee390727619a4d",
    "collector_sha256": "e7057ecc9013d761e8c423a7da5072ba215b8bc1a0af01047755d549afbe81b8",
    "run_id": 38525579823,
    "run_attempt": 1,
    "artifact_id": 11874924218,
    "artifact_sha256": "aa5c00473b31a6bd768aa5413763ada4c04b39789682949ba9e77bf70c64f06f",
}
_ENTRIES = {
    "/": ("directory", 0o755, None),
    "/bin": ("symlink", 0o777, "usr/bin"),
    "/usr": ("directory", 0o755, None),
    "/usr/bin": ("directory", 0o755, None),
    "/usr/bin/sh": ("symlink", 0o777, "dash"),
    "/usr/bin/dash": ("regular", 0o755, None),
}


def _require(value, code):
    if not value:
        raise LinuxPortableShellError("portable-shell: " + code)


def verify_cohort(*, image, platform_manifest, producer_commit, consumer_commit):
    """Require the caller's existing producer/consumer constants before admission."""
    _require((image, platform_manifest, producer_commit, consumer_commit) ==
             (_IMAGE, _PLATFORM, _PRODUCER, _CONSUMER)
             and all(type(value) is str for value in
                     (image, platform_manifest, producer_commit, consumer_commit)),
             "immutable producer cohort differs")


def _cohort():
    return {"image": _IMAGE, "platform_manifest": _PLATFORM, "config_digest": _CONFIG,
            "producer_commit": _PRODUCER, "consumer_commit": _CONSUMER}


def _identity(info):
    return [info.st_dev, info.st_ino, info.st_mode, info.st_uid, info.st_gid,
            info.st_size, info.st_mtime_ns, info.st_ctime_ns, info.st_nlink]


def _metadata(info, path):
    kind, mode, _target = _ENTRIES[path]
    expected = {"directory": stat.S_IFDIR, "symlink": stat.S_IFLNK,
                "regular": stat.S_IFREG}[kind] | mode
    _require(info.st_mode == expected and info.st_uid == 0 and info.st_gid == 0,
             "entry owner, type or mode differs")
    if kind == "regular":
        _require(info.st_size == _SIZE and info.st_nlink == 1,
                 "executable size or link count differs")
    return {"type": kind, "uid": 0, "gid": 0, "mode": mode,
            "identity": _identity(info)}


def _open_root():
    return os.open("/", os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC)


def _observe():
    """Hold every entry descriptor through hashing and finite pathname rechecks."""
    with ExitStack() as stack:
        descriptors = {}
        records = {}
        parents = {}

        def retain(path, fd):
            stack.callback(os.close, fd)
            descriptors[path] = fd
            records[path] = _metadata(os.fstat(fd), path)
            return fd

        root = retain("/", _open_root())

        def child(path, parent, name):
            kind, _mode, target = _ENTRIES[path]
            before = os.stat(name, dir_fd=parent, follow_symlinks=False)
            _metadata(before, path)
            flags = os.O_NOFOLLOW | os.O_CLOEXEC
            flags |= (os.O_PATH if kind == "symlink" else
                      os.O_RDONLY | (os.O_DIRECTORY if kind == "directory" else os.O_NONBLOCK))
            fd = retain(path, os.open(name, flags, dir_fd=parent))
            _require(records[path]["identity"] == _identity(before), "entry changed during open")
            parents[path] = (parent, name)
            if kind == "symlink":
                # Linux readlinkat with an empty path reads the held O_PATH link,
                # not a newly resolved path. Its known parent is held separately.
                text = os.readlink("", dir_fd=fd)
                _require(text == target, "link text differs")
                records[path]["link_target"] = text
            return fd

        child("/bin", root, "bin")
        usr = child("/usr", root, "usr")
        binary = child("/usr/bin", usr, "bin")
        child("/usr/bin/sh", binary, "sh")
        executable = child("/usr/bin/dash", binary, "dash")
        # One extra byte detects growth; the known finite size bounds all reads.
        raw = bytearray()
        while len(raw) <= _SIZE:
            part = os.read(executable, min(65536, _SIZE + 1 - len(raw)))
            if not part:
                break
            raw.extend(part)
        _require(len(raw) == _SIZE and hashlib.sha256(raw).hexdigest() == _SHA256,
                 "executable bytes differ")
        _require(len(raw) >= 64 and raw[:7] == b"\x7fELF\x02\x01\x01"
                 and struct.unpack_from("<HHI", raw, 16) == (3, 62, 1)
                 and struct.unpack_from("<H", raw, 52)[0] == 64,
                 "executable ELF header differs")
        for path, fd in descriptors.items():
            _require(_metadata(os.fstat(fd), path)["identity"] == records[path]["identity"],
                     "entry changed during read")
            if path != "/":
                parent, name = parents[path]
                current = os.stat(name, dir_fd=parent, follow_symlinks=False)
                _require(_metadata(current, path)["identity"] == records[path]["identity"],
                         "entry pathname changed")
                if _ENTRIES[path][0] == "symlink":
                    _require(os.readlink("", dir_fd=fd) == records[path]["link_target"],
                             "link changed during read")
        reopened = _open_root()
        try:
            _require(_metadata(os.fstat(reopened), "/")["identity"] == records["/"]["identity"],
                     "root pathname changed")
        finally:
            os.close(reopened)
        return records


def capture_identity():
    """Capture one operation's fixed shell identity after caller cohort admission."""
    _require(sys.platform == "linux" and all(type(getattr(os, name, None)) is int
             for name in ("O_PATH", "O_NOFOLLOW", "O_DIRECTORY", "O_CLOEXEC", "O_NONBLOCK", "O_RDONLY")),
             "Linux descriptor capabilities required")
    try:
        records = _observe()
    except OSError:
        raise LinuxPortableShellError("portable-shell: missing or unsafe filesystem entry") from None
    return {"schema_version": 1, "path": "/bin/sh", "resolved_path": "/usr/bin/dash",
            "sha256": _SHA256, "size": _SIZE, "executable": True,
            "cohort": _cohort(), "provenance": dict(_PROVENANCE), "entries": records}


def verify_identity(receipt):
    """Rehash and compare to the same operation's receipt; never accept a fallback."""
    try:
        _require(type(receipt) is dict and set(receipt) == {
            "schema_version", "path", "resolved_path", "sha256", "size", "executable",
            "cohort", "provenance", "entries"}, "invalid receipt")
        # Accept only a small JSON tree, including exact list/dict types. JSON
        # encoding then preserves bool/int distinctions against the fresh schema.
        pending = [(receipt, 0)]
        count = 0
        while pending:
            value, depth = pending.pop()
            count += 1
            _require(count <= 512 and depth <= 8, "invalid receipt")
            if type(value) is dict:
                _require(all(type(key) is str for key in value), "invalid receipt")
                pending.extend((item, depth + 1) for item in value.values())
            elif type(value) is list:
                pending.extend((item, depth + 1) for item in value)
            else:
                _require(type(value) in (str, int, bool), "invalid receipt")
        expected = json.dumps(receipt, sort_keys=True, separators=(",", ":"), allow_nan=False)
        actual = json.dumps(capture_identity(), sort_keys=True, separators=(",", ":"), allow_nan=False)
        _require(expected == actual, "operation identity differs")
    except (TypeError, ValueError, RecursionError) as error:
        if isinstance(error, LinuxPortableShellError):
            raise
        raise LinuxPortableShellError("portable-shell: invalid receipt") from None
