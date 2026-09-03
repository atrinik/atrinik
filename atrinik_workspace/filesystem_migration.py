"""Explicit, atomic migration of pre-portable filesystem identities.

The workspace intentionally keeps legacy ``{"device", "inode"}`` values
readable, but never guesses that a changed device is a remount.  This module
is the only supported path for converting those records.  It plans every
conversion first, keeps the old bytes as historical rollback evidence, and
publishes the converted JSON records one at a time behind a durable journal.
"""

from __future__ import annotations

import base64
import binascii
from contextlib import nullcontext
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import stat
from typing import Any, Iterable, Mapping

from .filesystem_identity import (
    FilesystemIdentityError,
    identity_matches,
    identity_digest,
    migrate_legacy_identity,
    portable_device_from_components,
    portable_pair,
    portable_identity,
    validate_identity,
)
from .locking import exclusive_lock
from .model import Paths, WorkspaceError, durable_atomic_json


FILESYSTEM_MIGRATION_SCHEMA_VERSION = 1
FILESYSTEM_MIGRATION_TRANSACTION = "filesystem-identity-migration-v1"
FILESYSTEM_MIGRATION_RECORD = "filesystem-identity-migration.json"
FILESYSTEM_MIGRATION_LOCK = "filesystem-identity-migration.lock"
MAX_MIGRATION_SNAPSHOT_BYTES = 64 * 1024 * 1024
_COMPACT_UPDATE_RECEIPT_PREFIX = ".delivery-update-receipt-"
_JOURNAL_KEYS = {
    "schema_version",
    "transaction",
    "state",
    "created_at",
    "completed_at",
    "confirm_remount",
    "records",
    "rollback_error",
}
_JOURNAL_RECORD_KEYS = {
    "path",
    "before_sha256",
    "after_sha256",
    "before_base64",
    "after_base64",
    "before_identity",
    "after_identity",
    "rollback_identity",
    "legacy_evidence",
}
_LEGACY_JOURNAL_RECORD_KEYS = {
    "path",
    "before_sha256",
    "after_sha256",
    "before_base64",
    "after_base64",
    "legacy_evidence",
}
_FULL_IDENTITY_CONTEXTS = {
    "control",
    "filesystem_identity",
    "identity",
    "lease",
    "lease_identity",
    "output_identity",
    "state_identity",
}
_HISTORICAL_CONTEXTS = {
    "archive",
    "archived",
    "erroneous_snapshot",
    "historical",
    "predecessor_snapshot",
    "preview",
    "rollback",
}


class FilesystemMigrationError(WorkspaceError):
    """A filesystem identity migration cannot be proven safe."""


def migrate_filesystem_records(
    repository: Path,
    mode: str,
    *,
    confirm_remount: bool = False,
) -> dict[str, Any]:
    """Audit, plan, or apply the wrapper's legacy filesystem identities."""

    if mode not in {"dry-run", "audit", "apply"}:
        raise ValueError(f"unsupported filesystem migration mode: {mode}")
    paths = Paths.discover(repository)
    if mode == "apply":
        paths.workspace.mkdir(parents=True, exist_ok=True)
    lock_path = paths.workspace / FILESYSTEM_MIGRATION_LOCK
    lock_context = (
        exclusive_lock(lock_path, "filesystem identity migration", nonblocking=True)
        if mode == "apply"
        else nullcontext()
    )
    with lock_context:
        journal_path = paths.workspace / FILESYSTEM_MIGRATION_RECORD
        if journal_path.exists() or journal_path.is_symlink():
            journal = _read_journal(journal_path)
            if mode == "audit":
                return _audit_journal(journal_path, journal)
            if journal["state"] in {"prepared", "applying", "rollback-required"}:
                if mode != "apply":
                    return _journal_plan(journal_path, journal, mode)
                if not confirm_remount:
                    raise FilesystemMigrationError(
                        "an unfinished filesystem identity migration exists; "
                        "rerun with --apply --confirm-remount"
                    )
                return _resume_journal(journal_path, journal)
            if journal["state"] == "complete":
                return _audit_journal(journal_path, journal)
            raise FilesystemMigrationError(
                f"filesystem identity migration journal has unsupported state: "
                f"{journal['state']}"
            )

        # Planning is read-only and should explain what a remount confirmation
        # would do.  The confirmation gate belongs to apply, not to discovery.
        plan = _plan(paths, confirm_remount=True)
        if mode != "apply":
            return {
                "migration": FILESYSTEM_MIGRATION_TRANSACTION,
                "status": "dry-run" if mode == "dry-run" else "audit",
                "records": [_public_record(record) for record in plan],
                "requires_confirm_remount": bool(plan),
            }
        if plan and not confirm_remount:
            raise FilesystemMigrationError(
                "legacy filesystem identities require explicit remount confirmation; "
                "rerun with --apply --confirm-remount"
            )
        if not plan:
            return {
                "migration": FILESYSTEM_MIGRATION_TRANSACTION,
                "status": "complete",
                "records": [],
                "journal": None,
            }
        journal = _new_journal(plan, confirm_remount=confirm_remount)
        durable_atomic_json(journal_path, journal)
        return _apply_journal(journal_path, journal)


def _plan(paths: Paths, *, confirm_remount: bool) -> list[dict[str, Any]]:
    plans: list[dict[str, Any]] = []
    namespace = _physical_lease_namespace(paths.repository)
    identity_record = namespace.parent / "atrinik-resource-leases.identity.json"
    if identity_record.exists() or identity_record.is_symlink():
        if identity_record.is_symlink() or not identity_record.is_file():
            raise FilesystemMigrationError(
                f"physical lease namespace identity is not a regular file: "
                f"{identity_record}"
            )
        value = _read_json(identity_record)
        if (
            isinstance(value, dict)
            and set(value) == {"schema_version", "device", "inode"}
            and value.get("schema_version") == 1
        ):
            metadata = _metadata(namespace, identity_record)
            try:
                replacement, evidence = migrate_legacy_identity(
                    {"device": value["device"], "inode": value["inode"]},
                    metadata,
                    str(identity_record),
                    confirm_remount=confirm_remount,
                )
            except FilesystemIdentityError as error:
                raise FilesystemMigrationError(str(error)) from error
            replacement_record = {
                "schema_version": 2,
                "identity": replacement,
            }
            plans.append(
                _plan_record(
                    identity_record,
                    value,
                    replacement_record,
                    evidence=evidence.json() if evidence is not None else None,
                )
            )
        elif isinstance(value, dict) and set(value) == {"schema_version", "identity"}:
            # The new namespace record is already portable.  Validate it here
            # so an audit cannot silently accept a malformed durable anchor.
            _validate_portable_record_identity(value["identity"], identity_record)
        else:
            raise FilesystemMigrationError(
                f"physical lease namespace identity has an unsupported schema: "
                f"{identity_record}"
            )

    record_roots = [paths.workspace, paths.repository / "build" / "reviews"]
    skip = {
        identity_record,
        paths.workspace / FILESYSTEM_MIGRATION_RECORD,
        paths.workspace / FILESYSTEM_MIGRATION_LOCK,
    }
    seen: set[Path] = set()
    for root in record_roots:
        for path in _workspace_json_records(root, skip=skip):
            canonical = path.resolve(strict=False)
            if canonical in seen:
                continue
            seen.add(canonical)
            value = _read_json(path)
            transformed = _transform_document(path, value)
            if transformed is None:
                continue
            plans.append(_plan_record(path, value, transformed))
    return sorted(plans, key=_migration_record_sort_key)


def _workspace_json_records(root: Path, *, skip: set[Path]) -> Iterable[Path]:
    if not root.exists():
        return ()
    if root.is_symlink() or not root.is_dir():
        raise FilesystemMigrationError(
            f"filesystem migration record root is not a directory: {root}"
        )
    result: list[Path] = []
    review_root = root.name == "reviews" and root.parent.name == "build"
    try:
        candidates = sorted(root.rglob("*.json"))
    except OSError as error:
        raise FilesystemMigrationError(
            f"cannot enumerate filesystem migration records under {root}: {error}"
        ) from error
    for path in candidates:
        if path in skip:
            continue
        if path.is_symlink():
            raise FilesystemMigrationError(
                f"filesystem migration record is a symlink: {path}"
            )
        if not path.is_file():
            raise FilesystemMigrationError(
                f"filesystem migration record is not a regular file: {path}"
            )
        if review_root and not _is_delivery_json_record(path.name):
            continue
        if path.name.endswith(".lock") or ".tmp" in path.name:
            continue
        result.append(path)
    return result


def _is_delivery_json_record(name: str) -> bool:
    """Select canonical delivery records and their persisted JSON sidecars."""

    if name == ".delivery-ledger-reclaim-complete.json":
        return True
    if _is_compact_update_receipt_name(name):
        return True
    if name.endswith(".md.ledger.json"):
        return True
    return (
        name.startswith(".")
        and ".md.ledger.json." in name
        and name.endswith(".json")
    )


def _migration_record_sort_key(record: Mapping[str, Any]) -> tuple[int, str]:
    """Apply canonical delivery ledgers before sidecars that refer to them."""

    path = str(record["path"])
    return (0 if Path(path).name.endswith(".md.ledger.json") else 1, path)


def _transform_document(
    path: Path,
    value: Any,
    *,
    remapped_targets: Mapping[Path, tuple[Mapping[str, Any], Mapping[str, Any]]] | None = None,
) -> dict[str, Any] | None:
    if remapped_targets is None:
        remapped_targets = {}
    if (
        path.name == "atrinik-resource-leases.identity.json"
        and isinstance(value, dict)
        and set(value) == {"schema_version", "device", "inode"}
        and value.get("schema_version") == 1
    ):
        namespace = path.with_name("atrinik-resource-leases")
        metadata = _metadata(namespace, path)
        try:
            replacement, _evidence = migrate_legacy_identity(
                {"device": value["device"], "inode": value["inode"]},
                metadata,
                str(path),
                confirm_remount=True,
            )
        except FilesystemIdentityError as error:
            raise FilesystemMigrationError(str(error)) from error
        return {"schema_version": 2, "identity": replacement}
    changed = False
    evidence: list[dict[str, Any]] = []

    def visit(current: Any, ancestors: tuple[tuple[str | int | None, Any], ...]) -> Any:
        nonlocal changed
        if isinstance(current, list):
            return [
                visit(item, (*ancestors, (index, current)))
                for index, item in enumerate(current)
            ]
        if not isinstance(current, dict):
            return current

        result = {
            key: visit(item, (*ancestors, (key, current)))
            for key, item in current.items()
        }
        key = _current_key(ancestors)
        if _contains_legacy_pair(current) and _is_filesystem_pair_context(
            current, key, ancestors, path
        ):
            result = _rewrite_pair(
                path,
                current,
                result,
                key,
                ancestors,
                evidence,
                remapped_targets,
                mark_changed=lambda: _mark_changed(),
            )
        if _contains_path_pair(current):
            result = _rewrite_path_pair(
                path,
                current,
                result,
                key,
                ancestors,
                evidence,
                remapped_targets,
                mark_changed=lambda: _mark_changed(),
            )
        if _contains_git_common_pair(current):
            result = _rewrite_named_pair(
                path,
                current,
                result,
                "git_common",
                "git_common_device",
                "git_common_inode",
                "git_common_pair",
                ancestors,
                evidence,
                remapped_targets,
                mark_changed=lambda: _mark_changed(),
            )
        return result

    def _mark_changed() -> None:
        nonlocal changed
        changed = True

    transformed = visit(value, ())
    if not changed:
        return None
    if not isinstance(transformed, dict):
        raise FilesystemMigrationError(f"filesystem record root is not an object: {path}")
    return transformed


def _rewrite_pair(
    document_path: Path,
    current: dict[str, Any],
    result: Any,
    key: str | None,
    ancestors: tuple[tuple[str | int | None, Any], ...],
    evidence: list[dict[str, Any]],
    remapped_targets: Mapping[Path, tuple[Mapping[str, Any], Mapping[str, Any]]],
    *,
    mark_changed: Any,
) -> Any:
    target, projection = _identity_target(document_path, current, key, ancestors)
    if target is not None and not target.exists() and not target.is_symlink():
        target = None
    if target is None:
        if not _is_historical_pair(document_path, current, ancestors):
            raise FilesystemMigrationError(
                f"cannot locate a safe filesystem object for {document_path}:{key or 'identity'}"
            )
        replacement = _historical_pair(current, key, ancestors, document_path)
        if replacement is None:
            raise FilesystemMigrationError(
                f"historical filesystem identity lacks type evidence: "
                f"{document_path}:{key or 'identity'}"
            )
        if set(current) == {"device", "inode"} and projection == "full":
            raise FilesystemMigrationError(
                f"cannot synthesize a full live identity for historical record "
                f"{document_path}:{key or 'identity'}"
            )
        if set(current) == {"device", "inode"}:
            if _pair_is_same(current, replacement):
                return result
            mark_changed()
            evidence.append(
                {
                    "context": key or "identity",
                    "legacy_pair_digest": _pair_digest(current),
                    "portable_pair_digest": _pair_digest(replacement),
                    "historical": True,
                }
            )
            return replacement
        if not _pair_is_same(current, replacement):
            result = {**result, "device": replacement["device"], "inode": replacement["inode"]}
            mark_changed()
            evidence.append(
                {
                    "context": key or "identity",
                    "legacy_pair_digest": _pair_digest(current),
                    "portable_pair_digest": _pair_digest(replacement),
                    "historical": True,
                }
            )
        return result

    metadata = _metadata(target, document_path)
    portable = portable_pair(metadata)
    if set(current) == {"device", "inode"} and projection == "pair" and _pair_is_same(
        current, portable
    ):
        return result
    if set(current) != {"device", "inode"} and _pair_is_same(current, portable):
        return result
    remapped = _target_was_migrated(
        target, current["inode"], metadata, remapped_targets
    )
    if remapped:
        replacement, old = portable_identity(metadata), None
    else:
        try:
            replacement, old = migrate_legacy_identity(
                {"device": current["device"], "inode": current["inode"]},
                metadata,
                f"{document_path}:{key or 'identity'}",
                confirm_remount=True,
            )
        except FilesystemIdentityError as error:
            raise FilesystemMigrationError(str(error)) from error
    if projection == "stable":
        replacement = portable_identity(metadata, include_ctime=False)
    mark_changed()
    evidence.append(
        {
            "context": key or "identity",
            "legacy_digest": _pair_digest(current),
            "portable_digest": identity_digest(replacement),
            "old": old.json() if old is not None else None,
            "remapped_target": str(target) if remapped else None,
        }
    )
    if set(current) == {"device", "inode"}:
        return portable if projection == "pair" else replacement
    return {**result, "device": portable["device"], "inode": portable["inode"]}


def _rewrite_path_pair(
    document_path: Path,
    current: dict[str, Any],
    result: Any,
    key: str | None,
    ancestors: tuple[tuple[str | int | None, Any], ...],
    evidence: list[dict[str, Any]],
    remapped_targets: Mapping[Path, tuple[Mapping[str, Any], Mapping[str, Any]]],
    *,
    mark_changed: Any,
) -> Any:
    target, _ = _identity_target(document_path, current, key, ancestors)
    if target is not None and not target.exists() and not target.is_symlink():
        target = None
    if target is None:
        if not _is_historical_pair(document_path, current, ancestors):
            raise FilesystemMigrationError(
                f"cannot locate a safe path identity target for {document_path}:{key or 'path'}"
            )
        replacement = _historical_pair(current, key, ancestors, document_path)
        if replacement is None:
            raise FilesystemMigrationError(
                f"historical path identity lacks type evidence: {document_path}"
            )
        if (
            result.get("path_device") != replacement["device"]
            or result.get("path_inode") != replacement["inode"]
        ):
            result = {
                **result,
                "path_device": replacement["device"],
                "path_inode": replacement["inode"],
            }
            mark_changed()
            evidence.append(
                {
                    "context": "path_pair",
                    "legacy_pair_digest": _pair_digest(
                        {
                            "device": current["path_device"],
                            "inode": current["path_inode"],
                        }
                    ),
                    "portable_pair_digest": _pair_digest(replacement),
                    "historical": True,
                }
            )
        return result
    metadata = _metadata(target, document_path)
    replacement = portable_pair(metadata)
    remapped = _target_was_migrated(
        target, current["path_inode"], metadata, remapped_targets
    )
    if current["path_inode"] != replacement["inode"] and not remapped:
        raise FilesystemMigrationError(
            f"path identity inode changed; refusing rebind: {target}"
        )
    if (
        current["path_device"] == replacement["device"]
        and current["path_inode"] == replacement["inode"]
    ):
        return result
    result = {
        **result,
        "path_device": replacement["device"],
        "path_inode": replacement["inode"],
    }
    mark_changed()
    evidence.append(
        {
            "context": "path_pair",
            "legacy_pair_digest": _pair_digest(
                {"device": current["path_device"], "inode": current["path_inode"]}
            ),
            "portable_pair_digest": _pair_digest(replacement),
            "remapped_target": str(target) if remapped else None,
        }
    )
    return result


def _rewrite_named_pair(
    document_path: Path,
    current: dict[str, Any],
    result: Any,
    name_key: str,
    device_key: str,
    inode_key: str,
    context: str,
    ancestors: tuple[tuple[str | int | None, Any], ...],
    evidence: list[dict[str, Any]],
    remapped_targets: Mapping[Path, tuple[Mapping[str, Any], Mapping[str, Any]]],
    *,
    mark_changed: Any,
) -> Any:
    candidate = current.get(name_key)
    if not isinstance(candidate, str) or not _valid_absolute_path(candidate):
        raise FilesystemMigrationError(
            f"filesystem identity has no safe {name_key} target: {document_path}"
        )
    target = Path(candidate)
    target_metadata: os.stat_result | None = None
    if not target.exists() and not target.is_symlink():
        if not _is_historical_pair(document_path, current, ancestors):
            raise FilesystemMigrationError(f"filesystem identity target is missing: {target}")
        replacement = _historical_pair(
            {"device": current[device_key], "inode": current[inode_key]},
            _current_key(ancestors),
            ancestors,
            document_path,
        )
        if replacement is None:
            raise FilesystemMigrationError(
                f"historical {context} lacks type evidence: {document_path}"
            )
    else:
        target_metadata = _metadata(target, document_path)
        replacement = portable_pair(target_metadata)
    if target_metadata is None:
        remapped = False
    else:
        remapped = _target_was_migrated(
            target, current[inode_key], target_metadata, remapped_targets
        )
    if current[inode_key] != replacement["inode"] and not remapped:
        raise FilesystemMigrationError(
            f"{context} inode changed; refusing rebind: {target}"
        )
    if (
        current[device_key] == replacement["device"]
        and current[inode_key] == replacement["inode"]
    ):
        return result
    mark_changed()
    evidence.append(
        {
            "context": context,
            "legacy_pair_digest": _pair_digest(
                {"device": current[device_key], "inode": current[inode_key]}
            ),
            "portable_pair_digest": _pair_digest(replacement),
            "remapped_target": str(target) if remapped else None,
        }
    )
    return {**result, device_key: replacement["device"], inode_key: replacement["inode"]}


def _identity_target(
    document_path: Path,
    current: dict[str, Any],
    key: str | None,
    ancestors: tuple[tuple[str | int | None, Any], ...],
) -> tuple[Path | None, str]:
    if key is None and _is_compact_update_receipt_name(document_path.name):
        return None, "pair"
    candidate = current.get("path")
    if isinstance(candidate, str) and _valid_absolute_path(candidate):
        return Path(candidate), _projection_for(key, ancestors)
    candidate = current.get("physical_path")
    if isinstance(candidate, str) and _valid_absolute_path(candidate):
        return Path(candidate), _projection_for(key, ancestors)

    port_reservation = _ancestor_value(ancestors, "port_reservation")
    if isinstance(port_reservation, dict):
        reservation_path = port_reservation.get("path")
        if isinstance(reservation_path, str) and _valid_absolute_path(reservation_path):
            if key == "directory":
                return Path(reservation_path).parent, "full"
            if key == "lease":
                return Path(reservation_path), "stable"

    runtime = _ancestor_value(ancestors, "runtime")
    if key == "lease" and isinstance(runtime, dict):
        runtime_path = runtime.get("path")
        if isinstance(runtime_path, str) and _valid_absolute_path(runtime_path):
            return Path(runtime_path) / "generation.lease", "full"
    if key == "lease" and _ancestor_value(ancestors, "control") is not None:
        return document_path.parent / "process-tree.lease", "full"

    if key == "output_identity":
        state = _ancestor_value(ancestors, "state")
        generation = _ancestor_value(ancestors, "generation")
        if isinstance(state, str) and _valid_absolute_path(state) and isinstance(generation, str):
            return Path(state) / "tmp" / "runtime-assets" / generation, "full"

    if key == "mutable_state_output_identities":
        index = _current_index(ancestors)
        owner = _parallel_output_owner(ancestors)
        if isinstance(owner, dict) and isinstance(index, int):
            outputs = owner.get("mutable_state_outputs")
            if isinstance(outputs, list) and 0 <= index < len(outputs):
                output = outputs[index]
                if isinstance(output, str) and _valid_absolute_path(output):
                    return Path(output), "full"

    if key == "lease_identity":
        state_policy = _ancestor_value(ancestors, "state_policy")
        if isinstance(state_policy, dict):
            candidate = state_policy.get("path")
            if isinstance(candidate, str) and _valid_absolute_path(candidate):
                return Path(f"{candidate}.lock"), "stable"

    for ancestor_key, ancestor in reversed(ancestors):
        if not isinstance(ancestor, dict):
            continue
        candidate = ancestor.get("path")
        if isinstance(candidate, str) and _valid_absolute_path(candidate):
            if key == "lease_identity":
                return Path(f"{candidate}.lock"), "stable"
            return Path(candidate), _projection_for(key, ancestors)
        candidate = ancestor.get("physical_path")
        if isinstance(candidate, str) and _valid_absolute_path(candidate):
            return Path(candidate), _projection_for(key, ancestors)

    if key == "mutable_state_output_identities":
        return None, "full"
    if key in {"ledger", "source", "installed", "predecessor_snapshot", "erroneous_snapshot"}:
        if document_path.name.endswith(".ledger.json"):
            return document_path, "pair"
    if document_path.name.endswith(".ledger.json") and _current_key(ancestors) in {
        "device",
        "inode",
    }:
        return document_path, "pair"
    return _nearby_named_target(document_path, current, ancestors), _projection_for(
        key, ancestors
    )


def _ancestor_value(
    ancestors: tuple[tuple[str | int | None, Any], ...], key: str
) -> Any:
    for ancestor_key, ancestor in reversed(ancestors):
        if isinstance(ancestor, dict) and ancestor_key == key:
            return ancestor.get(key)
    return None


def _contains_legacy_pair(value: dict[str, Any]) -> bool:
    return (
        isinstance(value.get("device"), int)
        and not isinstance(value.get("device"), bool)
        and value["device"] >= 0
        and isinstance(value.get("inode"), int)
        and not isinstance(value.get("inode"), bool)
        and value["inode"] >= 0
    )


def _contains_path_pair(value: dict[str, Any]) -> bool:
    return (
        isinstance(value.get("path_device"), int)
        and not isinstance(value.get("path_device"), bool)
        and value["path_device"] >= 0
        and isinstance(value.get("path_inode"), int)
        and not isinstance(value.get("path_inode"), bool)
        and value["path_inode"] >= 0
    )


def _contains_git_common_pair(value: dict[str, Any]) -> bool:
    return (
        isinstance(value.get("git_common_device"), int)
        and not isinstance(value.get("git_common_device"), bool)
        and value["git_common_device"] >= 0
        and isinstance(value.get("git_common_inode"), int)
        and not isinstance(value.get("git_common_inode"), bool)
        and value["git_common_inode"] >= 0
    )


def _is_filesystem_pair_context(
    value: dict[str, Any],
    key: str | None,
    ancestors: tuple[tuple[str | int | None, Any], ...],
    document_path: Path | None = None,
) -> bool:
    if document_path is not None and key is None and _is_compact_update_receipt_name(
        document_path.name
    ):
        return True
    if set(value) == {"device", "inode"}:
        return True
    if any(
        isinstance(value.get(name), str) and _valid_absolute_path(value[name])
        for name in ("path", "physical_path", "name")
    ):
        return True
    if key in _FULL_IDENTITY_CONTEXTS | _HISTORICAL_CONTEXTS | {
        "directory",
        "primary",
        "workspace",
        "wrapper",
        "source",
        "installed",
        "predecessor_snapshot",
        "erroneous_snapshot",
        "repositories",
    }:
        return True
    keys = {frame_key for frame_key, _ in ancestors if isinstance(frame_key, str)}
    return bool(keys & (_FULL_IDENTITY_CONTEXTS | _HISTORICAL_CONTEXTS | {"roots", "ledger"}))


def _current_key(
    ancestors: tuple[tuple[str | int | None, Any], ...]
) -> str | None:
    for key, _ in reversed(ancestors):
        if isinstance(key, str):
            return key
    return None


def _current_index(
    ancestors: tuple[tuple[str | int | None, Any], ...]
) -> int | None:
    for key, _ in reversed(ancestors):
        if isinstance(key, int):
            return key
    return None


def _parallel_output_owner(
    ancestors: tuple[tuple[str | int | None, Any], ...]
) -> dict[str, Any] | None:
    for _, ancestor in reversed(ancestors):
        if isinstance(ancestor, dict) and isinstance(
            ancestor.get("mutable_state_outputs"), list
        ):
            return ancestor
    return None


def _projection_for(
    key: str | None,
    ancestors: tuple[tuple[str | int | None, Any], ...],
) -> str:
    if key in _FULL_IDENTITY_CONTEXTS:
        return "full"
    if key == "mutable_state_output_identities":
        return "full"
    if key == "directory" and _ancestor_value(ancestors, "port_reservation") is not None:
        return "full"
    if key == "lease" and (
        _ancestor_value(ancestors, "control") is not None
        or _ancestor_value(ancestors, "runtime") is not None
        or _ancestor_value(ancestors, "port_reservation") is not None
    ):
        return "full"
    return "pair"


def _pair_is_same(current: dict[str, Any], replacement: dict[str, int]) -> bool:
    return (
        current.get("device") == replacement["device"]
        and current.get("inode") == replacement["inode"]
    )


def _target_was_migrated(
    target: Path,
    current_inode: Any,
    metadata: os.stat_result,
    remapped_targets: Mapping[Path, tuple[Mapping[str, Any], Mapping[str, Any]]],
) -> bool:
    """Authorize a cross-record reference after its target was atomically replaced."""

    record = remapped_targets.get(target.resolve(strict=False))
    if record is None:
        return False
    before, after = record
    return (
        current_inode == before.get("inode")
        and metadata.st_ino == after.get("inode")
        and identity_matches(after, metadata)
    )


def _historical_pair(
    current: dict[str, Any],
    key: str | None,
    ancestors: tuple[tuple[str | int | None, Any], ...],
    document_path: Path,
) -> dict[str, int] | None:
    inode = current.get("inode")
    if not isinstance(inode, int) or isinstance(inode, bool) or inode < 0:
        return None
    kind = current.get("kind")
    if kind not in {"file", "directory"}:
        kind = current.get("file_type")
    if kind not in {"file", "directory"}:
        if key in {"lease", "lease_identity", "source", "installed", "predecessor_snapshot", "erroneous_snapshot"}:
            kind = "file"
        elif key in {"directory", "identity", "output_identity", "primary", "workspace", "wrapper"}:
            kind = "directory"
    if kind not in {"file", "directory"}:
        keys = {frame_key for frame_key, _ in ancestors if isinstance(frame_key, str)}
        if "roots" in keys or "state_policy" in keys:
            kind = "directory"
        elif keys & {"control", "runtime", "port_reservation"}:
            kind = "file"
    if kind not in {"file", "directory"}:
        if document_path.suffix in {".lock", ".lease"} or document_path.name.endswith(
            ".json"
        ):
            kind = "file"
    if kind not in {"file", "directory"}:
        return None
    mode = stat.S_IFREG if kind == "file" else stat.S_IFDIR
    return {
        "device": portable_device_from_components(inode, mode, kind),
        "inode": inode,
    }


def _is_historical_pair(
    document_path: Path,
    current: dict[str, Any],
    ancestors: tuple[tuple[str | int | None, Any], ...],
) -> bool:
    if _historical_only(document_path):
        return True
    keys = {frame_key for frame_key, _ in ancestors if isinstance(frame_key, str)}
    if keys & _HISTORICAL_CONTEXTS:
        return True
    return any(
        key in current
        for key in {"raw_base64", "content_base64", "snapshot", "removed_at", "archived_at"}
    )


def _nearby_named_target(
    document_path: Path,
    current: dict[str, Any],
    ancestors: tuple[tuple[str | int | None, Any], ...],
) -> Path | None:
    candidates: list[Any] = [current.get("name"), current.get("target")]
    for _, ancestor in reversed(ancestors):
        if isinstance(ancestor, dict):
            candidates.extend((ancestor.get("name"), ancestor.get("target")))
    for candidate in candidates:
        if not isinstance(candidate, str) or not candidate or "\x00" in candidate:
            continue
        if _valid_absolute_path(candidate):
            return Path(candidate)
        if Path(candidate).name != candidate or candidate in {".", ".."}:
            continue
        target = document_path.parent / candidate
        if target.exists() or target.is_symlink():
            return target
    return None


def _plan_record(
    path: Path,
    before_value: Any,
    after_value: Any,
    *,
    evidence: list[dict[str, Any]] | dict[str, Any] | None = None,
) -> dict[str, Any]:
    del before_value
    before, before_identity = _read_snapshot(path)
    after = _canonical_json(after_value)
    if len(before) > MAX_MIGRATION_SNAPSHOT_BYTES or len(after) > MAX_MIGRATION_SNAPSHOT_BYTES:
        raise FilesystemMigrationError(f"filesystem migration record is too large: {path}")
    if before == after:
        raise FilesystemMigrationError(f"filesystem migration record has no change: {path}")
    return {
        "path": str(path),
        "before_sha256": hashlib.sha256(before).hexdigest(),
        "after_sha256": hashlib.sha256(after).hexdigest(),
        "before_base64": base64.b64encode(before).decode("ascii"),
        "after_base64": base64.b64encode(after).decode("ascii"),
        "before_identity": before_identity,
        "after_identity": None,
        "rollback_identity": None,
        "legacy_evidence": evidence or [],
    }


def _new_journal(records: list[dict[str, Any]], *, confirm_remount: bool) -> dict[str, Any]:
    return {
        "schema_version": FILESYSTEM_MIGRATION_SCHEMA_VERSION,
        "transaction": FILESYSTEM_MIGRATION_TRANSACTION,
        "state": "prepared",
        "created_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "confirm_remount": confirm_remount,
        "records": records,
    }


def _apply_journal(path: Path, journal: dict[str, Any]) -> dict[str, Any]:
    journal = {**journal, "state": "applying"}
    durable_atomic_json(path, journal)
    applied: list[dict[str, Any]] = []
    remapped_targets = _journal_remapped_targets(journal)
    try:
        for record in journal["records"]:
            target = Path(record["path"])
            current, current_identity = _read_snapshot(target)
            current_digest = hashlib.sha256(current).hexdigest()
            if current_digest == record["after_sha256"]:
                _require_record_identity(
                    record.get("after_identity"), current_identity, target, "after"
                )
                applied.append(record)
                _remember_remapped_target(remapped_targets, record)
                continue
            if current_digest != record["before_sha256"]:
                raise FilesystemMigrationError(
                    f"filesystem migration input changed before apply: {target}"
                )
            _require_record_identity(
                record.get("rollback_identity") or record.get("before_identity"),
                current_identity,
                target,
                "before",
            )
            current_value = _decode_json(current, target)
            transformed = _transform_document(
                target, current_value, remapped_targets=remapped_targets
            )
            if transformed is None:
                raise FilesystemMigrationError(
                    f"filesystem migration plan no longer applies: {target}"
                )
            after = _canonical_json(transformed)
            if len(after) > MAX_MIGRATION_SNAPSHOT_BYTES:
                raise FilesystemMigrationError(
                    f"filesystem migration record is too large: {target}"
                )
            if after != base64.b64decode(record["after_base64"], validate=True):
                record["after_base64"] = base64.b64encode(after).decode("ascii")
                record["after_sha256"] = hashlib.sha256(after).hexdigest()
                record["after_identity"] = None
                record["rollback_identity"] = None
                durable_atomic_json(path, journal)
            applied.append(record)
            _write_json_bytes(target, base64.b64decode(record["after_base64"]))
            after_bytes, after_identity = _read_snapshot(target)
            if hashlib.sha256(after_bytes).hexdigest() != record["after_sha256"]:
                raise FilesystemMigrationError(
                    f"filesystem migration output could not be verified: {target}"
                )
            record["after_identity"] = after_identity
            record["rollback_identity"] = None
            durable_atomic_json(path, journal)
            _remember_remapped_target(remapped_targets, record)
    except BaseException as error:
        rollback_error: BaseException | None = None
        for record in reversed(applied):
            try:
                _rollback_record(record)
            except BaseException as rollback_failure:
                rollback_error = rollback_failure
                break
        failed = {
            **journal,
            "state": "rollback-required" if rollback_error is not None else "prepared",
            "rollback_error": str(rollback_error) if rollback_error is not None else None,
        }
        try:
            durable_atomic_json(path, failed)
        except BaseException as journal_failure:
            raise FilesystemMigrationError(
                "filesystem migration failed and its rollback journal could not be "
                f"persisted: {journal_failure}"
            ) from error
        if rollback_error is not None:
            raise FilesystemMigrationError(
                f"filesystem migration failed and rollback is uncertain: {rollback_error}"
            ) from error
        raise
    complete = {**journal, "state": "complete", "completed_at": _now()}
    durable_atomic_json(path, complete)
    return _journal_plan(path, complete, "apply")


def _journal_remapped_targets(
    journal: Mapping[str, Any],
) -> dict[Path, tuple[Mapping[str, Any], Mapping[str, Any]]]:
    """Recover target replacements already durably recorded by a prior attempt."""

    result: dict[Path, tuple[Mapping[str, Any], Mapping[str, Any]]] = {}
    for record in journal["records"]:
        _remember_remapped_target(result, record)
    return result


def _remember_remapped_target(
    remapped_targets: dict[Path, tuple[Mapping[str, Any], Mapping[str, Any]]],
    record: Mapping[str, Any],
) -> None:
    before = record.get("before_identity")
    after = record.get("after_identity")
    if isinstance(before, Mapping) and isinstance(after, Mapping):
        remapped_targets[Path(str(record["path"])).resolve(strict=False)] = (
            before,
            after,
        )


def _resume_journal(path: Path, journal: dict[str, Any]) -> dict[str, Any]:
    for record in journal["records"]:
        target = Path(record["path"])
        current, current_identity = _read_snapshot(target)
        current_digest = hashlib.sha256(current).hexdigest()
        if current_digest == record["before_sha256"]:
            _require_record_identity(
                record.get("rollback_identity") or record.get("before_identity"),
                current_identity,
                target,
                "before",
            )
        elif current_digest == record["after_sha256"]:
            _require_record_identity(
                record.get("after_identity"), current_identity, target, "after"
            )
        else:
            raise FilesystemMigrationError(
                f"filesystem migration journal input is ambiguous: {record['path']}"
            )
    return _apply_journal(path, {**journal, "state": "prepared"})


def _audit_journal(path: Path, journal: dict[str, Any]) -> dict[str, Any]:
    records = journal["records"]
    states: list[str] = []
    for record in records:
        target = Path(record["path"])
        current, current_identity = _read_snapshot(target)
        digest = hashlib.sha256(current).hexdigest()
        if digest == record["after_sha256"]:
            _require_record_identity(
                record.get("after_identity"), current_identity, target, "after"
            )
            states.append("converted")
        elif digest == record["before_sha256"]:
            _require_record_identity(
                record.get("rollback_identity") or record.get("before_identity"),
                current_identity,
                target,
                "before",
            )
            states.append("legacy")
        else:
            raise FilesystemMigrationError(
                f"filesystem migration journal record changed: {record['path']}"
            )
    return {
        "migration": FILESYSTEM_MIGRATION_TRANSACTION,
        "status": journal["state"],
        "journal": str(path),
        "records": [
            {"path": record["path"], "status": state}
            for record, state in zip(records, states, strict=True)
        ],
    }


def _journal_plan(path: Path, journal: dict[str, Any], mode: str) -> dict[str, Any]:
    return {
        "migration": FILESYSTEM_MIGRATION_TRANSACTION,
        "status": journal["state"] if mode == "apply" else mode,
        "journal": str(path),
        "records": [
            {
                "path": record["path"],
                "before_sha256": record["before_sha256"],
                "after_sha256": record["after_sha256"],
            }
            for record in journal["records"]
        ],
    }


def _public_record(record: dict[str, Any]) -> dict[str, Any]:
    return {
        "path": record["path"],
        "before_sha256": record["before_sha256"],
        "after_sha256": record["after_sha256"],
        "legacy_evidence": record.get("legacy_evidence", []),
    }


def _read_journal(path: Path) -> dict[str, Any]:
    value = _read_json(path)
    if (
        not isinstance(value, dict)
        or set(value) - _JOURNAL_KEYS
        or value.get("schema_version") != FILESYSTEM_MIGRATION_SCHEMA_VERSION
        or value.get("transaction") != FILESYSTEM_MIGRATION_TRANSACTION
        or value.get("state") not in {"prepared", "applying", "rollback-required", "complete"}
        or not isinstance(value.get("created_at"), str)
        or not isinstance(value.get("confirm_remount"), bool)
        or not isinstance(value.get("records"), list)
    ):
        raise FilesystemMigrationError(f"filesystem migration journal is invalid: {path}")
    _validate_timestamp(value["created_at"], path)
    if "completed_at" in value:
        if not isinstance(value["completed_at"], str):
            raise FilesystemMigrationError(f"filesystem migration journal is invalid: {path}")
        _validate_timestamp(value["completed_at"], path)
        if value["state"] != "complete":
            raise FilesystemMigrationError(
                f"completed filesystem migration timestamp has unfinished state: {path}"
            )
    if "rollback_error" in value and value["rollback_error"] is not None and not isinstance(
        value["rollback_error"], str
    ):
        raise FilesystemMigrationError(f"filesystem migration journal is invalid: {path}")
    normalized_records: list[dict[str, Any]] = []
    paths: list[str] = []
    for record in value["records"]:
        if (
            not isinstance(record, dict)
            or set(record) not in (_JOURNAL_RECORD_KEYS, _LEGACY_JOURNAL_RECORD_KEYS)
            or not isinstance(record["path"], str)
            or not _valid_absolute_path(record["path"])
            or not _sha256(record["before_sha256"])
            or not _sha256(record["after_sha256"])
            or record["before_sha256"] == record["after_sha256"]
            or not isinstance(record["legacy_evidence"], (list, dict))
        ):
            raise FilesystemMigrationError(f"filesystem migration journal record is invalid: {path}")
        target = Path(record["path"])
        if target == path or target == path.parent / FILESYSTEM_MIGRATION_LOCK:
            raise FilesystemMigrationError(
                f"filesystem migration journal targets its own control file: {target}"
            )
        if target.resolve(strict=False) != target:
            raise FilesystemMigrationError(
                f"filesystem migration journal path is not canonical: {target}"
            )
        before = _decode_snapshot(record["before_base64"], target)
        after = _decode_snapshot(record["after_base64"], target)
        if hashlib.sha256(before).hexdigest() != record["before_sha256"]:
            raise FilesystemMigrationError(f"filesystem migration before snapshot is invalid: {path}")
        if hashlib.sha256(after).hexdigest() != record["after_sha256"]:
            raise FilesystemMigrationError(f"filesystem migration after snapshot is invalid: {path}")
        if before == after:
            raise FilesystemMigrationError(f"filesystem migration record has no change: {target}")
        current_identity = _metadata(target, path)
        normalized = dict(record)
        if set(record) == _LEGACY_JOURNAL_RECORD_KEYS:
            normalized.update(
                {
                    "before_identity": portable_identity(current_identity),
                    "after_identity": (
                        portable_identity(current_identity)
                        if hashlib.sha256(_read_bytes(target)).hexdigest()
                        == record["after_sha256"]
                        else None
                    ),
                    "rollback_identity": None,
                }
            )
        else:
            _validate_journal_identity(normalized.get("before_identity"), target, "before")
            if normalized.get("after_identity") is not None:
                _validate_journal_identity(normalized["after_identity"], target, "after")
            if normalized.get("rollback_identity") is not None:
                _validate_journal_identity(
                    normalized["rollback_identity"], target, "rollback"
                )
        normalized_records.append(normalized)
        paths.append(record["path"])
    if paths != [
        str(record["path"])
        for record in sorted(
            normalized_records, key=_migration_record_sort_key
        )
    ] or len(paths) != len(set(paths)):
        raise FilesystemMigrationError(
            f"filesystem migration journal records are not unique and ordered: {path}"
        )
    return {**value, "records": normalized_records}


def _physical_lease_namespace(repository: Path) -> Path:
    marker = repository / ".git"
    if marker.is_dir():
        return marker.resolve() / "atrinik-resource-leases"
    if marker.is_file():
        try:
            value = marker.read_text(encoding="utf-8").strip()
        except OSError as error:
            raise FilesystemMigrationError(f"cannot read Git worktree marker: {marker}") from error
        prefix = "gitdir: "
        if value.startswith(prefix):
            gitdir = Path(value.removeprefix(prefix))
            if not gitdir.is_absolute():
                gitdir = marker.parent / gitdir
            return gitdir.resolve().parent.parent / "atrinik-resource-leases"
    return repository / "workspace" / "atrinik-resource-leases"


def _metadata(path: Path, context: Path) -> os.stat_result:
    _assert_safe_path(path)
    try:
        metadata = path.stat(follow_symlinks=False)
    except OSError as error:
        raise FilesystemMigrationError(
            f"cannot inspect filesystem identity target {path} for {context}: {error}"
        ) from error
    if not (stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode)):
        raise FilesystemMigrationError(f"filesystem identity target is not a file/directory: {path}")
    if metadata.st_uid != os.geteuid():
        raise FilesystemMigrationError(f"filesystem identity target has foreign ownership: {path}")
    return metadata


def _validate_portable_record_identity(value: Any, path: Path) -> None:
    try:
        validate_identity(value, str(path), allow_legacy=False)
    except FilesystemIdentityError as error:
        raise FilesystemMigrationError(str(error)) from error


def _read_json(path: Path) -> Any:
    return _decode_json(_read_bytes(path), path)


def _read_bytes(path: Path) -> bytes:
    return _read_snapshot(path)[0]


def _read_snapshot(path: Path) -> tuple[bytes, dict[str, Any]]:
    _assert_safe_path(path)
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
    descriptor: int | None = None
    try:
        descriptor = os.open(path, flags)
        opened = os.fstat(descriptor)
        if not stat.S_ISREG(opened.st_mode) or opened.st_uid != os.geteuid():
            raise FilesystemMigrationError(
                f"filesystem migration record is not an owned regular file: {path}"
            )
        if opened.st_size > MAX_MIGRATION_SNAPSHOT_BYTES:
            raise FilesystemMigrationError(
                f"filesystem migration record is too large: {path}"
            )
        chunks: list[bytes] = []
        remaining = opened.st_size
        while remaining > 0:
            chunk = os.read(descriptor, min(1024 * 1024, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        raw = b"".join(chunks)
        after_read = os.fstat(descriptor)
        if (
            (opened.st_dev, opened.st_ino, opened.st_size, opened.st_ctime_ns)
            != (
                after_read.st_dev,
                after_read.st_ino,
                after_read.st_size,
                after_read.st_ctime_ns,
            )
            or len(raw) != opened.st_size
        ):
            raise FilesystemMigrationError(
                f"filesystem migration record changed while reading: {path}"
            )
        if len(raw) > MAX_MIGRATION_SNAPSHOT_BYTES:
            raise FilesystemMigrationError(
                f"filesystem migration record is too large: {path}"
            )
        visible = path.stat(follow_symlinks=False)
        if (
            not stat.S_ISREG(visible.st_mode)
            or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
        ):
            raise FilesystemMigrationError(
                f"filesystem migration record was replaced: {path}"
            )
        return raw, portable_identity(opened)
    except FilesystemMigrationError:
        raise
    except OSError as error:
        raise FilesystemMigrationError(
            f"cannot read filesystem migration record {path}: {error}"
        ) from error
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _write_json_bytes(path: Path, raw: bytes) -> None:
    value = _decode_json(raw, path)
    if _canonical_json(value) != raw:
        raise FilesystemMigrationError(
            f"migration snapshot is not canonical JSON: {path}"
        )
    durable_atomic_json(path, value)


def _decode_json(raw: bytes, path: Path) -> Any:
    try:
        return json.loads(
            raw.decode("utf-8"), object_pairs_hook=_reject_duplicate_keys
        )
    except (UnicodeError, ValueError, RecursionError) as error:
        raise FilesystemMigrationError(f"migration snapshot is not JSON: {path}") from error


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ValueError(f"duplicate JSON key: {key}")
        value[key] = item
    return value


def _decode_snapshot(encoded: Any, path: Path) -> bytes:
    if not isinstance(encoded, str):
        raise FilesystemMigrationError(f"migration snapshot encoding is invalid: {path}")
    try:
        raw = base64.b64decode(encoded, validate=True)
    except (ValueError, binascii.Error) as error:
        raise FilesystemMigrationError(
            f"migration snapshot is not valid base64: {path}"
        ) from error
    if len(raw) > MAX_MIGRATION_SNAPSHOT_BYTES:
        raise FilesystemMigrationError(f"filesystem migration record is too large: {path}")
    value = _decode_json(raw, path)
    if _canonical_json(value) != raw:
        raise FilesystemMigrationError(
            f"migration snapshot is not canonical JSON: {path}"
        )
    return raw


def _rollback_record(record: dict[str, Any]) -> None:
    target = Path(record["path"])
    current, current_identity = _read_snapshot(target)
    digest = hashlib.sha256(current).hexdigest()
    if digest == record["after_sha256"]:
        _require_record_identity(
            record.get("after_identity"), current_identity, target, "after"
        )
        _write_json_bytes(
            target, base64.b64decode(record["before_base64"], validate=True)
        )
    elif digest == record["before_sha256"]:
        _require_record_identity(
            record.get("rollback_identity") or record.get("before_identity"),
            current_identity,
            target,
            "before",
        )
    else:
        raise FilesystemMigrationError(
            f"filesystem migration rollback target changed: {target}"
        )
    restored, restored_identity = _read_snapshot(target)
    if hashlib.sha256(restored).hexdigest() != record["before_sha256"]:
        raise FilesystemMigrationError(
            f"filesystem migration rollback could not be verified: {target}"
        )
    record["rollback_identity"] = restored_identity
    record["after_identity"] = None


def _require_record_identity(
    expected: Any,
    current: dict[str, Any],
    target: Path,
    label: str,
) -> None:
    _validate_journal_identity(expected, target, label)
    if any(current.get(key) != value for key, value in expected.items()):
        raise FilesystemMigrationError(
            f"filesystem migration {label} identity changed: {target}"
        )


def _validate_journal_identity(value: Any, target: Path, label: str) -> None:
    try:
        validated = validate_identity(
            value,
            f"filesystem migration {label} identity at {target}",
            allow_legacy=False,
        )
    except FilesystemIdentityError as error:
        raise FilesystemMigrationError(str(error)) from error
    if validated.get("kind") != "file":
        raise FilesystemMigrationError(
            f"filesystem migration {label} identity is not a file: {target}"
        )


def _validate_timestamp(value: str, path: Path) -> None:
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise FilesystemMigrationError(
            f"filesystem migration journal timestamp is invalid: {path}"
        ) from error
    if parsed.tzinfo is None:
        raise FilesystemMigrationError(
            f"filesystem migration journal timestamp has no timezone: {path}"
        )


def _assert_safe_path(path: Path) -> None:
    if not path.is_absolute() or path == Path("/"):
        raise FilesystemMigrationError(f"filesystem migration path is unsafe: {path}")
    for parent in path.parents:
        if parent == Path("/"):
            break
        if parent.is_symlink():
            raise FilesystemMigrationError(
                f"filesystem migration path has a symlinked parent: {path}"
            )


def _canonical_json(value: Any) -> bytes:
    return (
        json.dumps(value, ensure_ascii=True, sort_keys=True, indent=2) + "\n"
    ).encode("ascii")


def _valid_absolute_path(value: str) -> bool:
    return (
        value.startswith("/")
        and value != "/"
        and "\x00" not in value
        and os.path.normpath(value) == value
    )


def _historical_only(path: Path) -> bool:
    return (
        ".archive-" in path.name
        or "historical" in path.parts
        or _is_compact_update_receipt_name(path.name)
    )


def _is_compact_update_receipt_name(name: str) -> bool:
    """Recognize the bounded JSON receipt without accepting aliases."""

    if not name.startswith(_COMPACT_UPDATE_RECEIPT_PREFIX) or not name.endswith(
        ".json"
    ):
        return False
    marker = name[len(_COMPACT_UPDATE_RECEIPT_PREFIX) : -len(".json")]
    return len(marker) == 64 and all(
        character in "0123456789abcdef" for character in marker
    )


def _pair_digest(value: dict[str, Any]) -> str:
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode("ascii")
    ).hexdigest()


def _sha256(value: Any) -> bool:
    return isinstance(value, str) and len(value) == 64 and all(
        character in "0123456789abcdef" for character in value
    )


def _now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
