#!/usr/bin/env python3
"""Load the shared read-only context probe through trusted source descriptors.

No package import or ambient PYTHONPATH lookup executes before source validation.
Native Windows returns its non-authoritative diagnostic before shared source loading.
"""
from __future__ import annotations

import os
from pathlib import Path
import stat


def _native_windows_main() -> int:
    # Windows cannot authorize Linux operations and need not execute their source.
    import argparse
    import json
    parser = argparse.ArgumentParser(description="Probe the read-only Atrinik delivery coordinator context")
    parser.add_argument("--root")
    parser.add_argument("--mutable-root", action="append", default=[])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = {"schema_version": 2, "authoritative": False, "status": "native-windows",
              "entry_mode": "native-host", "diagnostic": "native-windows-boundary",
              "failed_checks": ["native-host-boundary", "posix-ledger-primitives"],
              "next_action": "Bootstrap or attach to the pinned Atrinik Linux devcontainer before "
                             "any delivery-ledger mutation."}
    if args.json:
        print(json.dumps(result, sort_keys=True))
    else:
        print("atrinik coordinator: native-windows (authoritative=false)\n"
              "entry mode: native-host\n"
              "failed checks: " + ", ".join(result["failed_checks"]) + "\n"
              "next action: " + result["next_action"])
    return 2


if os.name == "nt":
    if __name__ == "__main__":
        raise SystemExit(_native_windows_main())
    raise ImportError("Linux authority source is unavailable on native Windows; run the diagnostic CLI")


def _read_probe_source(path: Path) -> bytes:
    uid = os.geteuid() if hasattr(os, "geteuid") else None
    directory_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
    descriptors = []
    try:
        descriptor = os.open(path.anchor, directory_flags)
        descriptors.append(descriptor)
        for index, name in enumerate((None, *path.parts[1:])):
            if name is not None:
                flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK
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
        for index in range(1, len(descriptors)):
            visible = os.stat(path.parts[index], dir_fd=descriptors[index - 1], follow_symlinks=False)
            opened = os.fstat(descriptors[index])
            if (visible.st_dev, visible.st_ino) != (opened.st_dev, opened.st_ino):
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
