#!/usr/bin/env python3
"""Load the shared read-only context probe through trusted source descriptors.

No package import or ambient PYTHONPATH lookup executes before source validation.
Windows remains non-authoritative; reparse points are rejected there as well.
"""
from __future__ import annotations

import os
from pathlib import Path
import stat


def _read_probe_source(path: Path) -> bytes:
    uid = os.geteuid() if hasattr(os, "geteuid") else None
    directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
    descriptors = []
    try:
        # Windows does not provide descriptor-relative opens; verify every
        # reparse-free ancestor before opening. It cannot gain Linux authority.
        if os.name == "nt":
            for parent in (*reversed(path.parents), path):
                info = parent.lstat()
                if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
                    raise OSError("probe source has a reparse point")
            descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_BINARY", 0))
            descriptors.append(descriptor)
        else:
            descriptor = os.open(path.anchor, directory_flags)
            descriptors.append(descriptor)
            for index, name in enumerate((None, *path.parts[1:])):
                if name is not None:
                    flags = os.O_RDONLY | os.O_NOFOLLOW
                    if index != len(path.parts) - 1:
                        flags |= os.O_DIRECTORY
                    descriptor = os.open(name, flags, dir_fd=descriptor)
                    descriptors.append(descriptor)
                info = os.fstat(descriptor)
                if info.st_uid not in (0, uid) or info.st_mode & 0o022:
                    raise OSError("probe source ancestry is not trusted")
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode) or not 0 < before.st_size <= 128 * 1024:
            raise OSError("probe source is not a bounded regular file")
        data = bytearray()
        while len(data) <= before.st_size:
            block = os.read(descriptor, min(65536, before.st_size - len(data) + 1))
            if not block:
                break
            data.extend(block)
        after = os.fstat(descriptor)
        identity = lambda info: (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)
        if len(data) != before.st_size or identity(before) != identity(after):
            raise OSError("probe source changed during read")
        # Check all opened named ancestors at the same operation boundary.
        if os.name != "nt":
            for index in range(1, len(descriptors)):
                visible = os.stat(path.parts[index], dir_fd=descriptors[index - 1], follow_symlinks=False)
                opened = os.fstat(descriptors[index])
                if (visible.st_dev, visible.st_ino) != (opened.st_dev, opened.st_ino):
                    raise OSError("probe source path changed during read")
        elif identity(path.lstat()) != identity(after):
            raise OSError("probe source path changed during read")
        return bytes(data)
    finally:
        for descriptor in reversed(descriptors):
            os.close(descriptor)


_probe_source_path = Path(__file__).absolute().parent.parent / "atrinik_workspace/coordinator_context.py"
try:
    _probe_source = _read_probe_source(_probe_source_path)
except OSError as error:
    if __name__ != "__main__":
        raise
    import json
    print(json.dumps({"schema_version": 2, "authoritative": False,
                      "status": "unknown-or-unsafe", "entry_mode": "unknown",
                      "diagnostic": "probe-source-untrusted", "failed_checks": ["probe-source-untrusted"],
                      "next_action": "Restore trusted source paths before probing."}, sort_keys=True))
    raise SystemExit(2) from error
exec(compile(_probe_source, str(_probe_source_path), "exec"), globals())

if __name__ == "__main__":
    raise SystemExit(main())
