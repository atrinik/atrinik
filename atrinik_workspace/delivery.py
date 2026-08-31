"""Read-only protection for active issue-delivery evidence.

The delivery-ledger helper owns the ledger schema and its trust boundary.  The
wrapper only projects the helper's bounded inventory into path references for
cleanup and repository-layout transitions; it never parses or rewrites ledger
bytes itself.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

from .locking import active_lock_fds
from .model import WorkspaceError
from .platform_compat import inherited_subprocess_handles


_INVENTORY_LIMIT = 32 * 1024 * 1024
_INVENTORY_TIMEOUT_SECONDS = 30
_LEDGER_SUFFIX = ".md.ledger.json"
_HELPER_RELATIVE = Path(
    ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
)


@dataclass(frozen=True)
class ActiveDeliveryEvidence:
    """The exact paths reserved by active delivery ledgers."""

    review_root: Path
    references: dict[Path, tuple[str, ...]]
    ledgers: tuple[str, ...]
    transition_blockers: tuple[str, ...]


def _regular_path(path: Path, context: str) -> None:
    try:
        metadata = path.lstat()
    except FileNotFoundError as error:
        raise WorkspaceError(f"{context} is missing: {path}") from error
    except OSError as error:
        raise WorkspaceError(f"cannot inspect {context}: {path}: {error}") from error
    if not path.is_file() or path.is_symlink():
        raise WorkspaceError(f"{context} is not a regular file: {path}")
    if metadata.st_uid != os.geteuid():
        raise WorkspaceError(f"{context} is not owned by the current user: {path}")


def _absolute_path(value: Any, context: str) -> Path:
    if not isinstance(value, str) or not value:
        raise WorkspaceError(f"{context} is not an absolute path")
    path = Path(value)
    if not path.is_absolute() or "\x00" in value:
        raise WorkspaceError(f"{context} is not an absolute path")
    try:
        return path.resolve(strict=False)
    except (OSError, RuntimeError) as error:
        raise WorkspaceError(f"cannot resolve {context}: {path}: {error}") from error


def _add_reference(
    references: dict[Path, list[str]], path: Path, ledger: str
) -> None:
    references.setdefault(path, []).append(ledger)


def inventory_active_delivery_evidence(wrapper_root: Path) -> ActiveDeliveryEvidence:
    """Inventory active delivery evidence without mutating any ledger bytes.

    An absent review root means that this wrapper has no delivery evidence to
    protect.  Once the root exists, every active ledger must retain its
    canonical report and lock.  Any helper, schema, or evidence failure is
    raised so callers can preserve all candidate cleanup/migration targets.
    """

    root = Path(wrapper_root).resolve()
    review_root = root / "build" / "reviews"
    if not review_root.exists() and not review_root.is_symlink():
        return ActiveDeliveryEvidence(review_root, {}, (), ())
    if review_root.is_symlink() or not review_root.is_dir():
        raise WorkspaceError(f"delivery review root is not a regular directory: {review_root}")

    helper = root / _HELPER_RELATIVE
    _regular_path(helper, "delivery-ledger helper")
    try:
        with inherited_subprocess_handles(active_lock_fds()) as inheritance:
            result = subprocess.run(
                [sys.executable, "-B", str(helper), "inventory", str(review_root)],
                cwd=root,
                check=False,
                capture_output=True,
                text=True,
                timeout=_INVENTORY_TIMEOUT_SECONDS,
                **inheritance,
            )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise WorkspaceError(f"delivery-ledger inventory failed: {error}") from error
    if (
        len(result.stdout.encode()) > _INVENTORY_LIMIT
        or len(result.stderr.encode()) > _INVENTORY_LIMIT
    ):
        raise WorkspaceError("delivery-ledger inventory exceeded its output limit")
    if result.returncode:
        detail = result.stderr.strip()
        suffix = f": {detail}" if detail else ""
        raise WorkspaceError(
            f"delivery-ledger inventory failed ({result.returncode}){suffix}"
        )
    try:
        inventory = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise WorkspaceError("delivery-ledger inventory was not valid JSON") from error
    if not isinstance(inventory, dict) or inventory.get("schema_version") != 1:
        raise WorkspaceError("delivery-ledger inventory schema is unsupported")
    ledgers = inventory.get("ledgers")
    if not isinstance(ledgers, list):
        raise WorkspaceError("delivery-ledger inventory ledgers are invalid")

    references: dict[Path, list[str]] = {}
    active_names: list[str] = []
    for index, snapshot in enumerate(ledgers):
        context = f"delivery-ledger inventory ledgers[{index}]"
        if not isinstance(snapshot, dict):
            raise WorkspaceError(f"{context} is invalid")
        name = snapshot.get("name")
        document = snapshot.get("document")
        if (
            not isinstance(name, str)
            or not name.endswith(_LEDGER_SUFFIX)
            or Path(name).name != name
            or not isinstance(document, dict)
        ):
            raise WorkspaceError(f"{context} is invalid")
        ledger_path = review_root / name
        report_path = review_root / name.removesuffix(".ledger.json")
        lock_path = review_root / f".{name}.lock"
        for path, label in (
            (ledger_path, "active delivery ledger"),
            (report_path, "active delivery report"),
            (lock_path, "active delivery lock"),
        ):
            _regular_path(path, f"{label} for {name}")
        _add_reference(references, review_root, name)
        _add_reference(references, ledger_path.resolve(), name)
        _add_reference(references, report_path.resolve(), name)
        _add_reference(references, lock_path.resolve(), name)
        artifacts = document.get("artifacts")
        if not isinstance(artifacts, list):
            raise WorkspaceError(f"{context}.document.artifacts is invalid")
        for artifact_index, artifact in enumerate(artifacts):
            if not isinstance(artifact, dict) or artifact.get("kind") != "worktree":
                continue
            current = artifact.get("current")
            if current is None:
                request = artifact.get("primitive_request")
                if isinstance(request, dict):
                    roots = request.get("roots")
                    if isinstance(roots, dict):
                        workspace_path = roots.get("workspace", {}).get("path")
                        checkout = request.get("physical_checkout")
                        label = request.get("label")
                        if (
                            isinstance(workspace_path, str)
                            and isinstance(checkout, str)
                            and isinstance(label, str)
                        ):
                            planned = _absolute_path(
                                str(Path(workspace_path) / "worktrees" / checkout / label),
                                f"{context}.document.artifacts[{artifact_index}].primitive_request.path",
                            )
                            _add_reference(references, planned, name)
                continue
            if not isinstance(current, dict):
                raise WorkspaceError(
                    f"{context}.document.artifacts[{artifact_index}].current is invalid"
                )
            path = _absolute_path(
                current.get("path"),
                f"{context}.document.artifacts[{artifact_index}].current.path",
            )
            _add_reference(references, path, name)
        active_names.append(name)

    preserved_labels: list[str] = [*active_names]
    transition_blockers: list[str] = [*active_names]
    for field, prefix in (
        ("pending", "pending"),
        ("legacy_reports", "legacy"),
        ("releases", "release"),
        ("archives", "archive"),
        ("reclaims", "reclaim"),
        ("historical_ledgers", "historical"),
    ):
        rows = inventory.get(field)
        if not isinstance(rows, list):
            raise WorkspaceError(f"delivery-ledger inventory {field} is invalid")
        for row in rows:
            if not isinstance(row, dict):
                raise WorkspaceError(f"delivery-ledger inventory {field} contains invalid evidence")
            identity = row.get("name") or row.get("target") or row.get("ledger_name")
            if not isinstance(identity, str) or not identity:
                raise WorkspaceError(f"delivery-ledger inventory {field} lacks an identity")
            label = f"{prefix}:{identity}"
            preserved_labels.append(label)
            if prefix == "pending":
                transition_blockers.append(label)
    if preserved_labels:
        for label in preserved_labels:
            _add_reference(references, review_root.resolve(), label)

    return ActiveDeliveryEvidence(
        review_root.resolve(),
        {path: tuple(sorted(set(values))) for path, values in references.items()},
        tuple(sorted(active_names)),
        tuple(sorted(set(transition_blockers))),
    )
