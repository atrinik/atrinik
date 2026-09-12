"""Path coordinates for workspace records and open descriptors.

Paths name resources; contents, generations and ordinary locks govern changes.
Copying a resource to new storage at the same path needs no identity migration.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import stat
import sys
from typing import Any, Mapping


class PathRecordError(ValueError):
    """A resource path record is malformed."""


def canonical_path(path: str | Path) -> str:
    """Normalize a named coordinate without following symlinks.

    Callers retain their no-follow checks at filesystem access boundaries.
    """
    value = os.fspath(path)
    if not isinstance(value, str) or not value or "\x00" in value:
        raise PathRecordError("path must be nonempty text without NUL bytes")
    return os.path.normcase(os.path.abspath(value))


def descriptor_path(descriptor: int) -> str:
    """Return the current named path of an open descriptor, or raise OSError.

    Unlinked resources have no usable path coordinate. Native Windows callers
    keep the explicit path used to open a handle instead of using this helper.
    """
    if sys.platform == "darwin":
        import fcntl
        raw = fcntl.fcntl(descriptor, 50, bytes(1024))  # F_GETPATH
        value = os.fsdecode(raw.split(b"\x00", 1)[0])
    elif sys.platform.startswith("linux"):
        value = os.readlink(f"/proc/self/fd/{descriptor}")
        if value.endswith(" (deleted)"):
            raise OSError("descriptor path has been unlinked")
    else:
        raise OSError("descriptor paths are unavailable on this platform")
    if not os.path.isabs(value):
        raise OSError("descriptor does not name an absolute filesystem path")
    return canonical_path(value)


def path_record(
    path: str | Path,
    *,
    kind: str | None = None,
    content_sha256: str | None = None,
) -> dict[str, Any]:
    """Describe a path, optionally retaining its expected type or contents."""
    value: dict[str, Any] = {"path": canonical_path(path)}
    if kind is not None:
        value["kind"] = kind
    if content_sha256 is not None:
        value["sha256"] = content_sha256
    return validate_path_record(value)


def validate_path_record(value: Any, context: str = "path record") -> dict[str, Any]:
    if not isinstance(value, dict) or not {"path"} <= value.keys() <= {"path", "kind", "sha256"}:
        raise PathRecordError(f"{context} has an invalid shape")
    path = value["path"]
    if not isinstance(path, str) or not os.path.isabs(path) or canonical_path(path) != path:
        raise PathRecordError(f"{context}.path must be a canonical absolute path")
    if "kind" in value and (
        not isinstance(value["kind"], str) or value["kind"] not in {"file", "directory"}
    ):
        raise PathRecordError(f"{context}.kind is invalid")
    if "sha256" in value and (
        value.get("kind") == "directory"
        or not isinstance(value["sha256"], str)
        or re.fullmatch(r"[0-9a-f]{64}", value["sha256"]) is None
    ):
        raise PathRecordError(f"{context}.sha256 is invalid")
    return value


def path_record_matches(
    value: Mapping[str, Any],
    path: str | Path,
    *,
    content_sha256: str | None = None,
) -> bool:
    expected = validate_path_record(value)
    if expected["path"] != canonical_path(path):
        return False
    if "sha256" in expected and expected["sha256"] != content_sha256:
        return False
    if "kind" in expected:
        try:
            mode = Path(path).stat(follow_symlinks=False).st_mode
        except OSError:
            return False
        if expected["kind"] == "directory":
            return stat.S_ISDIR(mode)
        return stat.S_ISREG(mode)
    return True
