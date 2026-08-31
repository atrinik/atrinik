"""Portable durable identities for wrapper-managed filesystem records.

``st_dev`` is a useful *live* identity component, but it is a property of a
mount namespace.  It is therefore deliberately absent from identities that
are written to workspace records.  Callers that are fencing a live operation
should continue to compare the descriptor's complete ``(st_dev, st_ino)``
identity and, where applicable, its mount id.

The portable identity is intentionally small.  Directories are identified by
their inode and file type; their ctime is not retained because normal child
creation changes a directory's ctime.  Regular files also retain ctime, so an
atomic replacement cannot be accepted merely because it happens to reuse an
inode.  Content digests remain a separate integrity assertion when a record
already has one.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import stat
from typing import Any, Mapping


PORTABLE_IDENTITY_SCHEMA_VERSION = 1
PORTABLE_DIRECTORY_IDENTITY_KEYS = {
    "schema_version",
    "kind",
    "inode",
    "mode",
}
PORTABLE_FILE_IDENTITY_KEYS = PORTABLE_DIRECTORY_IDENTITY_KEYS
PORTABLE_FILE_OPTIONAL_KEYS = {"ctime_ns"}
LEGACY_IDENTITY_KEYS = {"device", "inode"}


class FilesystemIdentityError(ValueError):
    """A durable filesystem identity is invalid or cannot be migrated."""


class FilesystemIdentityMigrationRequired(FilesystemIdentityError):
    """A pre-portable record needs the explicit remount migration command."""


@dataclass(frozen=True)
class LegacyIdentityEvidence:
    """The old mount-bound identity retained in a migration audit record."""

    device: int
    inode: int

    def json(self) -> dict[str, int]:
        return {"device": self.device, "inode": self.inode}


def live_identity(metadata: os.stat_result) -> tuple[int, int]:
    """Return the complete identity used only for a live descriptor fence."""

    return metadata.st_dev, metadata.st_ino


def portable_identity(
    metadata: os.stat_result,
    *,
    content_sha256: str | None = None,
    include_ctime: bool = True,
) -> dict[str, Any]:
    """Build a mount-independent identity from one already-opened object.

    ``include_ctime`` is retained for regular files whose replacement must be
    detected.  Lease files that are deliberately renamed during their normal
    lifecycle should use ``include_ctime=False`` and retain their separate
    live descriptor, content, or creation-token fences.
    """

    if not (stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode)):
        raise FilesystemIdentityError("portable identity requires a regular file or directory")
    value: dict[str, Any] = {
        "schema_version": PORTABLE_IDENTITY_SCHEMA_VERSION,
        "kind": "directory" if stat.S_ISDIR(metadata.st_mode) else "file",
        "inode": _nonnegative_integer(metadata.st_ino, "inode"),
        "mode": stat.S_IFMT(metadata.st_mode),
    }
    if stat.S_ISREG(metadata.st_mode):
        if include_ctime:
            value["ctime_ns"] = _nonnegative_integer(
                metadata.st_ctime_ns, "ctime_ns"
            )
        if content_sha256 is not None:
            if not isinstance(content_sha256, str) or not _SHA256.fullmatch(
                content_sha256
            ):
                raise FilesystemIdentityError("content digest is not a SHA-256 value")
            value["sha256"] = content_sha256
    elif content_sha256 is not None:
        raise FilesystemIdentityError("directory identities cannot retain a content digest")
    return value


def portable_identity_from_path(
    path: Path,
    *,
    content_sha256: str | None = None,
    follow_symlinks: bool = False,
) -> dict[str, Any]:
    """Build a portable identity after a no-follow path observation."""

    return portable_identity(
        path.stat(follow_symlinks=follow_symlinks),
        content_sha256=content_sha256,
    )


def validate_identity(
    value: Any,
    context: str = "filesystem identity",
    *,
    allow_legacy: bool = True,
) -> dict[str, Any]:
    """Validate a portable identity or an explicitly supported legacy pair."""

    if not isinstance(value, dict):
        raise FilesystemIdentityError(f"{context} must be an object")
    keys = set(value)
    if allow_legacy and keys == LEGACY_IDENTITY_KEYS:
        for key in keys:
            _nonnegative_integer(value[key], f"{context}.{key}")
        return value
    if value.get("schema_version") != PORTABLE_IDENTITY_SCHEMA_VERSION:
        raise FilesystemIdentityError(f"{context} has an unsupported schema")
    kind = value.get("kind")
    expected = (
        PORTABLE_DIRECTORY_IDENTITY_KEYS
        if kind == "directory"
        else PORTABLE_FILE_IDENTITY_KEYS
        if kind == "file"
        else None
    )
    if expected is None:
        raise FilesystemIdentityError(f"{context}.kind is invalid")
    allowed = expected | {"sha256"}
    if kind == "file":
        allowed |= PORTABLE_FILE_OPTIONAL_KEYS
    if not keys <= allowed or not expected <= keys:
        raise FilesystemIdentityError(f"{context} has an invalid shape")
    _nonnegative_integer(value["inode"], f"{context}.inode")
    mode = value["mode"]
    if not isinstance(mode, int) or isinstance(mode, bool) or mode not in {
        stat.S_IFREG,
        stat.S_IFDIR,
    }:
        raise FilesystemIdentityError(f"{context}.mode is invalid")
    if kind == "file" and "ctime_ns" in value:
        _nonnegative_integer(value["ctime_ns"], f"{context}.ctime_ns")
    if "sha256" in value and (
        kind != "file"
        or not isinstance(value["sha256"], str)
        or not _SHA256.fullmatch(value["sha256"])
    ):
        raise FilesystemIdentityError(f"{context}.sha256 is invalid")
    return value


def is_portable_identity(value: Any) -> bool:
    try:
        validate_identity(value, allow_legacy=False)
    except FilesystemIdentityError:
        return False
    return True


def is_legacy_identity(value: Any) -> bool:
    return isinstance(value, dict) and set(value) == LEGACY_IDENTITY_KEYS and all(
        isinstance(item, int) and not isinstance(item, bool) and item >= 0
        for item in value.values()
    )


def identity_matches(
    value: Mapping[str, Any],
    metadata: os.stat_result,
    *,
    content_sha256: str | None = None,
) -> bool:
    """Compare a durable identity with current metadata.

    Legacy records deliberately still require the old device number.  A
    mismatch is not guessed to be a remount: the caller must use the explicit
    migration/rebind operation, which records the old evidence first.
    """

    validated = validate_identity(value)
    if is_legacy_identity(validated):
        return live_identity(metadata) == (
            validated["device"],
            validated["inode"],
        )
    current = portable_identity(metadata, content_sha256=content_sha256)
    for key, expected in validated.items():
        if key == "sha256" and content_sha256 is None:
            return False
        if current.get(key) != expected:
            return False
    return True


def require_identity_match(
    value: Mapping[str, Any],
    metadata: os.stat_result,
    context: str,
    *,
    content_sha256: str | None = None,
) -> None:
    """Raise a migration-specific error for a stale legacy mount identity."""

    if is_legacy_identity(value) and live_identity(metadata) != (
        value["device"],
        value["inode"],
    ):
        raise FilesystemIdentityMigrationRequired(
            f"{context} uses a pre-portable filesystem identity; run "
            "./atrinik migrate filesystem --apply --confirm-remount"
        )
    if not identity_matches(value, metadata, content_sha256=content_sha256):
        raise FilesystemIdentityError(f"{context} changed")


def migrate_legacy_identity(
    value: Mapping[str, Any],
    metadata: os.stat_result,
    context: str,
    *,
    confirm_remount: bool,
) -> tuple[dict[str, Any], LegacyIdentityEvidence | None]:
    """Convert one legacy identity with explicit remount confirmation.

    The inode must still match.  A changed device is accepted only when the
    operator explicitly selects the migration command; the returned evidence
    is intended for the atomic migration journal and rollback record.
    """

    if not is_legacy_identity(value):
        validated = validate_identity(value, context, allow_legacy=False)
        return dict(validated), None
    old = LegacyIdentityEvidence(value["device"], value["inode"])
    current = live_identity(metadata)
    if current[1] != old.inode:
        raise FilesystemIdentityError(f"{context} inode changed; refusing rebind")
    if current[0] != old.device and not confirm_remount:
        raise FilesystemIdentityMigrationRequired(
            f"{context} device changed; rerun with --confirm-remount"
        )
    return portable_identity(metadata), old


def identity_digest(value: Mapping[str, Any]) -> str:
    """Return a deterministic digest suitable for durable lock filenames."""

    validate_identity(value, allow_legacy=False)
    raw = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("ascii")
    return hashlib.sha256(raw).hexdigest()


def portable_device(metadata: os.stat_result) -> int:
    """Return a stable numeric compatibility value for old ``device`` fields.

    A few schema-v1 records expose a ``path_device``/``device`` pair as part
    of a larger record shape.  New writers retain that shape for readers that
    have not migrated yet, but the value is now a digest-derived portable
    token, never ``st_dev``.  The projection omits regular-file ``ctime_ns``:
    link/rename operations legitimately update ctime while preserving the
    opened object.  New code should prefer the full identity object, with a
    content digest and live descriptor fence when file replacement matters.
    """

    if stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode):
        identity = portable_identity(metadata)
        return portable_device_from_components(
            identity["inode"], identity["mode"], identity["kind"]
        )
    # Owned-tree cleanup can quarantine symlinks and other entries when the
    # caller has not requested link rejection.  They cannot be represented by
    # the durable directory/file schema, but their tombstone name still needs
    # a mount-independent projection of the opened no-follow metadata.
    value = {
        "schema_version": PORTABLE_IDENTITY_SCHEMA_VERSION,
        "kind": "special",
        "inode": _nonnegative_integer(metadata.st_ino, "inode"),
        "mode": stat.S_IFMT(metadata.st_mode),
    }
    raw = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("ascii")
    return int.from_bytes(hashlib.sha256(raw).digest()[:8], "big")


def portable_device_from_components(inode: int, mode: int, kind: str) -> int:
    """Project known durable type/inode evidence without a live ``stat``.

    This is used only for historical records whose object was intentionally
    removed before an explicit migration could run.  The result is the same
    projection as :func:`portable_device`; it is not a live identity proof.
    Callers must retain the historical bytes and continue to fail closed when
    a live object is required.
    """

    _nonnegative_integer(inode, "inode")
    if kind not in {"file", "directory"}:
        raise FilesystemIdentityError("portable identity kind is invalid")
    if mode not in {stat.S_IFREG, stat.S_IFDIR}:
        raise FilesystemIdentityError("portable identity mode is invalid")
    if (kind == "file") != (mode == stat.S_IFREG):
        raise FilesystemIdentityError("portable identity kind and mode disagree")
    identity = {
        "schema_version": PORTABLE_IDENTITY_SCHEMA_VERSION,
        "kind": kind,
        "inode": inode,
        "mode": mode,
    }
    raw = json.dumps(identity, sort_keys=True, separators=(",", ":")).encode("ascii")
    return int.from_bytes(hashlib.sha256(raw).digest()[:8], "big")


def portable_pair(metadata: os.stat_result) -> dict[str, int]:
    """Build the backwards-compatible two-integer projection of an identity."""

    return {"device": portable_device(metadata), "inode": metadata.st_ino}


def pair_matches(value: Mapping[str, Any], metadata: os.stat_result) -> bool:
    """Match a portable pair while still accepting an untouched v1 pair."""

    if set(value) != LEGACY_IDENTITY_KEYS:
        return False
    device = value.get("device")
    inode = value.get("inode")
    if (
        not isinstance(device, int)
        or isinstance(device, bool)
        or device < 0
        or not isinstance(inode, int)
        or isinstance(inode, bool)
        or inode < 0
    ):
        return False
    if inode != metadata.st_ino:
        return False
    return device in {metadata.st_dev, portable_device(metadata)}


def _nonnegative_integer(value: Any, context: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise FilesystemIdentityError(f"{context} must be a non-negative integer")
    return value


_SHA256 = re.compile(r"^[0-9a-f]{64}$")
