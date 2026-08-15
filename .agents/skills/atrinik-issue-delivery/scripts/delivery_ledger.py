#!/usr/bin/env python3
"""Durable, deterministic delivery-ledger sidecars.

Markdown review reports are intentionally outside this trust boundary.  This
module owns only canonical schema-v1 JSON sidecars in one already-proven review
directory.  Every public operation takes an exclusive root lock; mutating
operations additionally take a stable per-ledger lock.
"""

from __future__ import annotations

import argparse
import base64
import binascii
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime
import fcntl
import hashlib
import importlib
import importlib.abc
import importlib.util
import json
import os
from pathlib import Path
import re
import selectors
import stat
import subprocess
import sys
import threading
import time
from typing import Any, Callable, Iterable, Iterator, Mapping, Sequence


SCHEMA_VERSION = 1
MAX_BYTES = 1024 * 1024
MAX_RETAINED_RESULT_BYTES = 512 * 1024
MAX_INVENTORY_ENTRIES = 4096
MAX_INVENTORY_BYTES = 32 * 1024 * 1024
LEDGER_SUFFIX = ".md.ledger.json"
ENTRY_MODES = {"issue", "pr"}
ARTIFACT_KINDS = {"branch", "worktree", "pull_request"}
ARTIFACT_STATES = {"planned", "created", "adopted"}
MIGRATION_KINDS = {"legacy", "pre-schema"}
RESOURCE_KINDS = {
    "build",
    "profile",
    "reference",
    "runtime",
    "scenario",
    "scope",
    "state",
    "topology",
}
RESOURCE_LIFECYCLES = {
    "active",
    "consumed",
    "ready",
    "released",
    "running",
    "static",
    "stopped",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
OWNER_RE = re.compile(r"^[a-z0-9](?:[a-z0-9-]{0,38}[a-z0-9])?$")
REPOSITORY_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,99}$")
LOGIN_RE = re.compile(r"^[a-z0-9](?:[a-z0-9-]{0,37}[a-z0-9])?$")
NODE_RE = re.compile(r"^[A-Za-z0-9_=-]{2,256}$")
SLOT_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,127}$")
BRANCH_RE = re.compile(r"^(?!-)[A-Za-z0-9_+][A-Za-z0-9._+/-]{0,254}$")
REFERENCE_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9:/._#+-]{0,511}$")
TIMESTAMP_RE = re.compile(
    r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}"
    r"(?:\.[0-9]{1,9})?Z$"
)
EXTERNAL_GENERATION_RE = re.compile(r"^[0-9a-f]{32}$")
COMMENT_MARKER_RE = re.compile(
    r"^<!-- atrinik-delivery:comment:[0-9a-f]{64} -->$"
)
_WORKSPACE_LOAD_LOCK = threading.RLock()
_MIGRATION_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.migration\.json$"
)
_CREATE_STAGE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.create-(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_MIGRATE_STAGE_RE = re.compile(r"^\.(?P<target>.+\.md\.ledger\.json)\.migrate\.tmp$")
_UPDATE_STAGE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.update"
    r"(?P<operation>-bind-(?:worktree|scope)-[a-z0-9][a-z0-9._-]{0,127})?"
    r"-g(?P<generation>[0-9]+)-"
    r"from-(?P<digest>[0-9a-f]{64})-to-(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_UPDATE_RECEIPT_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.update-proof"
    r"(?P<operation>-bind-(?:worktree|scope)-[a-z0-9][a-z0-9._-]{0,127})?"
    r"-g"
    r"(?P<generation>[0-9]+)-from-(?P<digest>[0-9a-f]{64})-"
    r"d(?P<device>[0-9]+)-i(?P<inode>[0-9]+)-"
    r"to-(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_MARKER_COMPLETE_STAGE_RE = re.compile(
    r"^(?P<marker>\..+\.md\.ledger\.json\.migration\.json)\.complete\.tmp$"
)
_MARKER_PLAN_STAGE_RE = re.compile(
    r"^(?P<marker>\..+\.md\.ledger\.json\.migration\.json)\.planned-"
    r"(?P<operation>[0-9a-f]{64})\.tmp$"
)
_MARKER_PREPARE_STAGE_RE = re.compile(
    r"^(?P<marker>\..+\.md\.ledger\.json\.migration\.json)\.prepared\.tmp$"
)
_SNAPSHOT_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.migration-source\.snapshot$"
)
_LEDGER_LOCK_RE = re.compile(r"^\..+\.md\.ledger\.json\.lock$")
_CANONICAL_REPORT_RE = re.compile(
    r"^(?P<owner>[a-z0-9][a-z0-9._-]*)-(?P<repo>[a-z0-9][a-z0-9._-]*)-"
    r"(?P<mode>issue|pr)-(?P<number>[1-9][0-9]*)\.md$"
)
_LEGACY_REPORT_RE = re.compile(
    r"^(?P<owner>[a-z0-9][a-z0-9._-]*)-(?P<repo>[a-z0-9][a-z0-9._-]*)-"
    r"(?P<number>[1-9][0-9]*)\.md$"
)
_ISSUE_URL_RE = re.compile(
    r"https://github\.com/(?P<owner>[a-z0-9-]+)/(?P<repo>[a-z0-9._-]+)/issues/(?P<number>[1-9][0-9]*)",
    re.IGNORECASE,
)
_PR_URL_RE = re.compile(
    r"https://github\.com/(?P<owner>[a-z0-9-]+)/(?P<repo>[a-z0-9._-]+)/pull/(?P<number>[1-9][0-9]*)",
    re.IGNORECASE,
)
BODY_NAMESPACE = b"atrinik-delivery"
SAFE_ARTIFACT_STATE = {
    "clean": True,
    "detached": False,
    "locked": False,
    "active": False,
    "unowned_reference": False,
    "foreign": False,
    "certain": True,
}
_GIT_OPERATION_MARKERS = (
    "MERGE_HEAD",
    "CHERRY_PICK_HEAD",
    "REVERT_HEAD",
    "BISECT_LOG",
    "rebase-apply",
    "rebase-merge",
    "sequencer",
)


class LedgerError(RuntimeError):
    """A ledger operation could not be proven safe."""


class InjectedCrash(BaseException):
    """Test-only failpoint which deliberately bypasses normal exception cleanup."""


Failpoint = str | Callable[[str], None] | None


@dataclass(frozen=True)
class Snapshot:
    name: str
    document: dict[str, Any]
    raw: bytes
    digest: str
    device: int
    inode: int

    def json(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "digest": self.digest,
            "device": self.device,
            "inode": self.inode,
            "document": self.document,
        }


_ATOMIC_BIND_TOKEN = object()


@dataclass(frozen=True)
class _AtomicBindingCapability:
    """In-process authority for one helper-projected atomic initial bind."""

    token: object
    kind: str
    slot_id: str
    name: str
    before_raw: bytes | None
    after_raw: bytes
    expected_generation: int
    expected_digest: str
    expected_device: int
    expected_inode: int


@dataclass(frozen=True)
class PendingOperation:
    kind: str
    target: str
    staging: str

    def json(self) -> dict[str, str]:
        return {"kind": self.kind, "target": self.target, "staging": self.staging}


@dataclass(frozen=True)
class LegacyClaim:
    name: str
    canonical_target: str | None
    digest: str
    issues: tuple[tuple[str, str, int], ...]
    pull_requests: tuple[tuple[str, str, int], ...]
    repository_heads: tuple[tuple[str, str, str], ...]
    worktrees: tuple[str, ...]
    ambiguous: bool

    def json(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "canonical_target": self.canonical_target,
            "digest": self.digest,
            "issues": [list(value) for value in self.issues],
            "pull_requests": [list(value) for value in self.pull_requests],
            "repository_heads": [list(value) for value in self.repository_heads],
            "worktrees": list(self.worktrees),
            "ambiguous": self.ambiguous,
        }


@dataclass
class _LiveWorktreeGuard:
    """Pinned no-follow roots for one exact live Git worktree proof."""

    request: Mapping[str, Any]
    path: str
    descriptors: dict[str, int]
    allowed_references: frozenset[str] = frozenset()
    authority_recheck: Callable[[], None] | None = None

    def prove(self, *, expected_tree: str | None = None) -> dict[str, Any]:
        if self.authority_recheck is not None:
            self.authority_recheck()
        return _prove_live_worktree(
            self.request,
            self.path,
            self.descriptors,
            expected_tree=expected_tree,
        )


@dataclass
class _PinnedGitAuthority:
    """No-follow descriptors and immutable snapshots of Git authority inputs."""

    common_path: str = ""
    directories: list[tuple[int, str, tuple[int, int], str]] | None = None
    files: list[tuple[int, str, bytes, tuple[int, int], str]] | None = None
    absences: list[tuple[int, str, str]] | None = None

    def __post_init__(self) -> None:
        if self.directories is None:
            self.directories = []
        if self.files is None:
            self.files = []
        if self.absences is None:
            self.absences = []

    def add_directory(self, descriptor: int, path: str, context: str) -> None:
        status = _recheck_pinned_directory(descriptor, path, context)
        assert self.directories is not None
        self.directories.append(
            (descriptor, path, (status.st_dev, status.st_ino), context)
        )

    def add_file(self, directory: int, name: str, context: str) -> bytes:
        raw, status = _read_regular(directory, name)
        _require_trusted_regular(status, context)
        assert self.files is not None
        self.files.append(
            (directory, name, raw, (status.st_dev, status.st_ino), context)
        )
        return raw

    def add_absence(self, directory: int, name: str, context: str) -> None:
        direct = _direct_name(name, context)
        try:
            os.stat(direct, dir_fd=directory, follow_symlinks=False)
        except FileNotFoundError:
            assert self.absences is not None
            self.absences.append((directory, direct, context))
            return
        except OSError as error:
            raise LedgerError(f"cannot prove {context} absence: {error}") from error
        raise LedgerError(f"{context} appeared before absence could be retained")

    def recheck(self) -> None:
        assert self.directories is not None
        assert self.files is not None
        assert self.absences is not None
        for descriptor, path, identity, context in self.directories:
            _recheck_pinned_directory(descriptor, path, context, identity)
        for directory, name, raw, identity, context in self.files:
            current, status = _read_regular(directory, name)
            _require_trusted_regular(status, context)
            if current != raw or (status.st_dev, status.st_ino) != identity:
                raise LedgerError(f"{context} changed during live Git proof")
        for directory, name, context in self.absences:
            try:
                os.stat(name, dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                continue
            except OSError as error:
                raise LedgerError(f"cannot recheck {context} absence: {error}") from error
            raise LedgerError(f"{context} appeared during live Git proof")

    def close(self) -> None:
        assert self.directories is not None
        for descriptor, _, _, _ in reversed(self.directories):
            os.close(descriptor)
        self.directories.clear()


@dataclass(frozen=True)
class Inventory:
    ledgers: tuple[Snapshot, ...]
    pending: tuple[PendingOperation, ...]
    legacy_reports: tuple[LegacyClaim, ...] = ()

    def json(self) -> dict[str, Any]:
        return {
            "schema_version": SCHEMA_VERSION,
            "ledgers": [item.json() for item in self.ledgers],
            "pending": [item.json() for item in self.pending],
            "legacy_reports": [item.json() for item in self.legacy_reports],
        }


def _hit(failpoint: Failpoint, name: str) -> None:
    if callable(failpoint):
        failpoint(name)
    elif failpoint == name:
        raise InjectedCrash(name)


def _duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise LedgerError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _decode_value(raw: bytes, context: str) -> Any:
    if len(raw) > MAX_BYTES:
        raise LedgerError(f"{context} exceeds {MAX_BYTES} bytes")
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=_duplicate_keys)
    except LedgerError:
        raise
    except (UnicodeError, ValueError, RecursionError) as error:
        raise LedgerError(f"invalid JSON in {context}: {error}") from error


def _decode(raw: bytes, context: str) -> dict[str, Any]:
    value = _decode_value(raw, context)
    if not isinstance(value, dict):
        raise LedgerError(f"{context} root must be an object")
    return value


def canonical_bytes(document: Mapping[str, Any]) -> bytes:
    return (
        json.dumps(document, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
        + "\n"
    ).encode("ascii")


def byte_digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def canonical_object_digest(value: Any) -> str:
    """Match the wrapper's canonical request digest (which has no final newline)."""

    try:
        raw = json.dumps(
            value, ensure_ascii=True, sort_keys=True, separators=(",", ":")
        ).encode("ascii")
    except (TypeError, ValueError, RecursionError, UnicodeError) as error:
        raise LedgerError(f"value cannot be canonically hashed: {error}") from error
    return byte_digest(raw)


def _exact(value: Any, keys: set[str], context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise LedgerError(f"{context} must be an object")
    actual = set(value)
    if actual != keys:
        details: list[str] = []
        if keys - actual:
            details.append("missing " + ", ".join(sorted(keys - actual)))
        if actual - keys:
            details.append("unexpected " + ", ".join(sorted(actual - keys)))
        raise LedgerError(f"{context}: {'; '.join(details)}")
    return value


def _string(value: Any, context: str, pattern: re.Pattern[str]) -> str:
    if not isinstance(value, str) or not pattern.fullmatch(value):
        raise LedgerError(f"{context} is invalid")
    return value


def _integer(value: Any, context: str, minimum: int = 1) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise LedgerError(f"{context} must be an integer >= {minimum}")
    return value


def _contains_control(value: str) -> bool:
    return any(ord(character) < 0x20 or ord(character) == 0x7F for character in value)


def _absolute_path(value: Any, context: str) -> str:
    if (
        not isinstance(value, str)
        or not value.startswith("/")
        or _contains_control(value)
        or "//" in value
        or os.path.normpath(value) != value
        or value == "/"
    ):
        raise LedgerError(f"{context} must be an absolute canonical path and non-root")
    return value


def _path_identity(value: Any, context: str) -> tuple[str, int, int]:
    item = _exact(value, {"path", "device", "inode"}, context)
    return (
        _absolute_path(item["path"], f"{context}.path"),
        _integer(item["device"], f"{context}.device", minimum=0),
        _integer(item["inode"], f"{context}.inode", minimum=0),
    )


def _request_roots(
    value: Any, physical_checkout: str, context: str
) -> tuple[tuple[str, int, int], ...]:
    item = _exact(value, {"wrapper", "workspace", "primary"}, context)
    wrapper = _path_identity(item["wrapper"], f"{context}.wrapper")
    workspace = _path_identity(item["workspace"], f"{context}.workspace")
    primary = _path_identity(item["primary"], f"{context}.primary")
    wrapper_path = Path(wrapper[0])
    workspace_path = Path(workspace[0])
    if workspace_path == wrapper_path or wrapper_path.is_relative_to(workspace_path):
        raise LedgerError(f"{context}.workspace is an unsafe wrapper root/ancestor")
    wrapper_self = physical_checkout == "atrinik" and Path(primary[0]) == wrapper_path
    if not wrapper_self and Path(primary[0]) != wrapper_path / physical_checkout:
        raise LedgerError(f"{context}.primary does not match the physical checkout")
    return wrapper, workspace, primary


def _managed_worktree_result_path(
    value: Any,
    workspace_root: str,
    physical_checkout: str,
    label: str,
    context: str,
) -> str:
    path = _absolute_path(value, context)
    expected = Path(workspace_root) / "worktrees" / physical_checkout / label
    if Path(path) != expected:
        raise LedgerError(
            f"{context} does not match the precommitted workspace worktree path"
        )
    return path


def _managed_worktree_coordinate(value: str) -> tuple[str, str, str] | None:
    parts = Path(value).parts
    if len(parts) >= 4 and parts[-3] == "worktrees":
        workspace = str(Path(*parts[:-3])) or "/"
        return workspace.casefold(), parts[-2].casefold(), parts[-1].casefold()
    return None


def _timestamp_key(value: str, context: str) -> tuple[datetime, int]:
    timestamp = _string(value, context, TIMESTAMP_RE)
    whole = timestamp[:-1]
    seconds, separator, fraction = whole.partition(".")
    try:
        parsed = datetime.strptime(seconds, "%Y-%m-%dT%H:%M:%S")
    except ValueError as error:
        raise LedgerError(f"{context} is not a real UTC timestamp") from error
    nanoseconds = int(fraction.ljust(9, "0")) if separator else 0
    return parsed, nanoseconds


def _timestamp_is_after(after: str, before: str) -> bool:
    return _timestamp_key(after, "later timestamp") > _timestamp_key(
        before, "earlier timestamp"
    )


def _timestamp_is_equal(left: str, right: str) -> bool:
    return _timestamp_key(left, "left timestamp") == _timestamp_key(
        right, "right timestamp"
    )


def _repository(value: Any, context: str) -> tuple[str, str, str]:
    item = _exact(value, {"owner", "name", "node_id"}, context)
    owner = _string(item["owner"], f"{context}.owner", OWNER_RE)
    name = _string(item["name"], f"{context}.name", REPOSITORY_RE)
    node_id = _string(item["node_id"], f"{context}.node_id", NODE_RE)
    if owner != owner.casefold() or name != name.casefold():
        raise LedgerError(f"{context} owner/name must be normalized lowercase")
    return owner, name, node_id


def _issue(value: Any, context: str) -> tuple[str, str, int, str, str]:
    item = _exact(value, {"repository", "number", "node_id"}, context)
    owner, name, repository_node = _repository(item["repository"], f"{context}.repository")
    number = _integer(item["number"], f"{context}.number")
    node = _string(item["node_id"], f"{context}.node_id", NODE_RE)
    return owner, name, number, node, repository_node


def _optional_digest(value: Any, context: str) -> str | None:
    if value is None:
        return None
    return _string(value, context, SHA256_RE)


def _optional_timestamp(value: Any, context: str) -> str | None:
    if value is None:
        return None
    timestamp = _string(value, context, TIMESTAMP_RE)
    _timestamp_key(timestamp, context)
    return timestamp


def _inline_payload(value: Any, context: str, *, utf8: bool = False) -> bytes:
    """Validate and decode one bounded, canonical inline byte payload."""

    item = _exact(value, {"encoding", "raw_base64", "sha256"}, context)
    if item["encoding"] != "base64":
        raise LedgerError(f"{context}.encoding must be base64")
    encoded = item["raw_base64"]
    if not isinstance(encoded, str) or len(encoded) > MAX_BYTES:
        raise LedgerError(f"{context}.raw_base64 is invalid")
    try:
        raw = base64.b64decode(encoded.encode("ascii"), validate=True)
    except (UnicodeError, ValueError, binascii.Error) as error:
        raise LedgerError(f"{context}.raw_base64 is invalid") from error
    if len(raw) > MAX_RETAINED_RESULT_BYTES:
        raise LedgerError(
            f"{context} exceeds {MAX_RETAINED_RESULT_BYTES} retained bytes"
        )
    if base64.b64encode(raw).decode("ascii") != encoded:
        raise LedgerError(f"{context}.raw_base64 is noncanonical")
    digest = _string(item["sha256"], f"{context}.sha256", SHA256_RE)
    if byte_digest(raw) != digest:
        raise LedgerError(f"{context} digest does not match retained bytes")
    if utf8:
        try:
            raw.decode("utf-8")
        except UnicodeError as error:
            raise LedgerError(f"{context} is not valid UTF-8") from error
    return raw


def _body(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "ownership",
            "state",
            "observed_digest",
            "intended_digest",
            "intended_payload",
            "current_digest",
            "outside_digest",
            "section_digest",
            "updated_at",
        },
        context,
    )
    ownership = item["ownership"]
    if ownership not in {
        "delivery-created",
        "delivery-section",
        "contributor-owned",
    }:
        raise LedgerError(f"{context}.ownership is invalid")
    state = item["state"]
    if state not in {"observed", "update-planned", "written"}:
        raise LedgerError(f"{context}.state is invalid")
    observed = _optional_digest(item["observed_digest"], f"{context}.observed_digest")
    intended = _optional_digest(item["intended_digest"], f"{context}.intended_digest")
    payload = item["intended_payload"]
    payload_raw = None
    if payload is not None:
        payload_raw = _inline_payload(payload, f"{context}.intended_payload", utf8=True)
    current = _string(item["current_digest"], f"{context}.current_digest", SHA256_RE)
    outside = _string(item["outside_digest"], f"{context}.outside_digest", SHA256_RE)
    section = _optional_digest(item["section_digest"], f"{context}.section_digest")
    updated = _optional_timestamp(item["updated_at"], f"{context}.updated_at")
    if (intended is None) != (payload is None):
        raise LedgerError(f"{context} intended digest/payload must appear together")
    if payload is not None and payload["sha256"] != intended:
        raise LedgerError(f"{context} intended digest differs from payload")
    if state == "observed":
        if (
            ownership != "contributor-owned"
            or intended is not None
            or payload_raw is not None
            or observed is None
            or observed != current
            or outside != current
            or section is not None
            or updated is None
        ):
            raise LedgerError(f"{context} observed body digests must match")
    elif state == "update-planned":
        if (
            ownership not in {"delivery-created", "delivery-section"}
            or observed is None
            or current != observed
            or intended is None
            or payload_raw is None
            or intended == current
            or updated is None
        ):
            raise LedgerError(f"{context} update intent is incomplete")
    elif (
        ownership not in {"delivery-created", "delivery-section"}
        or intended is not None
        or payload_raw is not None
        or (ownership == "delivery-section" and section is None)
        or (section is None and outside != current)
        or updated is None
    ):
        raise LedgerError(f"{context} written body identity is incomplete")
    return ownership, state, observed, intended, current, outside, section, updated


def _comment(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "state",
            "marker",
            "intended_digest",
            "intended_payload",
            "node_id",
            "current_digest",
        },
        context,
    )
    state = item["state"]
    if state not in {"none", "planned", "in-flight", "bound"}:
        raise LedgerError(f"{context}.state is invalid")
    marker = item["marker"]
    if marker is not None:
        marker = _string(marker, f"{context}.marker", COMMENT_MARKER_RE)
    intended = _optional_digest(item["intended_digest"], f"{context}.intended_digest")
    payload = item["intended_payload"]
    if payload is not None:
        _inline_payload(payload, f"{context}.intended_payload", utf8=True)
    if (intended is None) != (payload is None):
        raise LedgerError(f"{context} intended digest/payload must appear together")
    if payload is not None and payload["sha256"] != intended:
        raise LedgerError(f"{context} intended digest differs from payload")
    node = item["node_id"]
    if node is not None:
        node = _string(node, f"{context}.node_id", NODE_RE)
    current = _optional_digest(item["current_digest"], f"{context}.current_digest")
    if state == "none" and any(
        field is not None for field in (marker, intended, payload, node, current)
    ):
        raise LedgerError(f"{context} none state must have no identity")
    if state in {"planned", "in-flight"} and (
        marker is None
        or intended is None
        or ((node is None) != (current is None))
        or (current is not None and current == intended)
    ):
        raise LedgerError(f"{context} intent state is incomplete")
    if state == "bound" and (
        marker is None
        or intended is not None
        or payload is not None
        or node is None
        or current is None
    ):
        raise LedgerError(f"{context} bound state is incomplete")
    return state, marker, intended, node, current


def _pull_request(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "repository",
            "head_repository",
            "number",
            "node_id",
            "author_node_id",
            "base_branch",
            "head_branch",
            "draft",
            "draft_intent",
            "body",
            "comment",
        },
        context,
    )
    owner, name, _ = _repository(item["repository"], f"{context}.repository")
    head_owner, head_name, head_repository_node = _repository(
        item["head_repository"], f"{context}.head_repository"
    )
    if (head_owner, head_name) != (owner, name):
        raise LedgerError(f"{context} head repository is foreign")
    number = _integer(item["number"], f"{context}.number")
    node = _string(item["node_id"], f"{context}.node_id", NODE_RE)
    _string(item["author_node_id"], f"{context}.author_node_id", NODE_RE)
    base = _branch(item["base_branch"], f"{context}.base_branch")
    head = _branch(item["head_branch"], f"{context}.head_branch")
    if not isinstance(item["draft"], bool):
        raise LedgerError(f"{context}.draft must be a boolean")
    if item["draft_intent"] not in {None, "ready"}:
        raise LedgerError(f"{context}.draft_intent is invalid")
    if item["draft"] is False and item["draft_intent"] is not None:
        raise LedgerError(f"{context} ready PR cannot retain a draft intent")
    _body(item["body"], f"{context}.body")
    _comment(item["comment"], f"{context}.comment")
    return owner, name, number, node, base, head


def _branch(value: Any, context: str) -> str:
    branch = _string(value, context, BRANCH_RE)
    if (
        branch == "HEAD"
        or ".." in branch
        or "//" in branch
        or "@{" in branch
        or branch.endswith((".", "/"))
        or branch.startswith("/")
        or any(
            component.startswith(".") or component.endswith(".lock")
            for component in branch.split("/")
        )
    ):
        raise LedgerError(f"{context} must be a canonical Git branch")
    return branch


def _ordered_unique(
    rows: Any, context: str, validator: Callable[[Any, str], tuple[Any, ...]]
) -> list[tuple[Any, ...]]:
    if not isinstance(rows, list):
        raise LedgerError(f"{context} must be an array")
    keys = [validator(row, f"{context}[{index}]") for index, row in enumerate(rows)]
    if len(set(keys)) != len(keys):
        raise LedgerError(f"{context} contains duplicate identities")
    if keys != sorted(keys, key=lambda item: tuple(str(part).casefold() for part in item)):
        raise LedgerError(f"{context} must be deterministically sorted")
    return keys


def _head(value: Any, context: str) -> tuple[str, str, str, tuple[str, ...]]:
    item = _exact(
        value, {"branch", "initial_sha", "current_sha", "lineage"}, context
    )
    branch = _branch(item["branch"], f"{context}.branch")
    initial = _string(item["initial_sha"], f"{context}.initial_sha", COMMIT_RE)
    current = _string(item["current_sha"], f"{context}.current_sha", COMMIT_RE)
    if not isinstance(item["lineage"], list) or not item["lineage"]:
        raise LedgerError(f"{context}.lineage must be a non-empty array")
    lineage = tuple(
        _string(sha, f"{context}.lineage[{index}]", COMMIT_RE)
        for index, sha in enumerate(item["lineage"])
    )
    if len(set(lineage)) != len(lineage):
        raise LedgerError(f"{context}.lineage contains a repeated commit")
    if lineage[0] != initial or lineage[-1] != current:
        raise LedgerError(f"{context}.lineage must connect initial_sha to current_sha")
    return branch, initial, current, lineage


def _target(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(value, {"repository", "base", "head", "merge_base"}, context)
    repository = _repository(item["repository"], f"{context}.repository")
    base = _head(item["base"], f"{context}.base")
    head = _head(item["head"], f"{context}.head")
    merge = _exact(
        item["merge_base"], {"initial_sha", "current_sha"}, f"{context}.merge_base"
    )
    merge_initial = _string(
        merge["initial_sha"], f"{context}.merge_base.initial_sha", COMMIT_RE
    )
    merge_current = _string(
        merge["current_sha"], f"{context}.merge_base.current_sha", COMMIT_RE
    )
    return (*repository, head[0])


def _identity(value: Any, context: str, *, current: bool) -> tuple[Any, ...]:
    keys = {"repository", "branch", "path", "number", "node_id", "body_digest"}
    if current:
        keys.add("head_sha")
    item = _exact(value, keys, context)
    repository = _repository(item["repository"], f"{context}.repository")
    branch = item["branch"]
    if branch is not None:
        branch = _branch(branch, f"{context}.branch")
    path = item["path"]
    if path is not None:
        path = _absolute_path(path, f"{context}.path")
    number = item["number"]
    if number is not None:
        number = _integer(number, f"{context}.number")
    node = item["node_id"]
    if node is not None:
        node = _string(node, f"{context}.node_id", NODE_RE)
    body_digest = _optional_digest(item["body_digest"], f"{context}.body_digest")
    head_sha: str | None = None
    if current:
        head_sha = item["head_sha"]
        if head_sha is not None:
            head_sha = _string(head_sha, f"{context}.head_sha", COMMIT_RE)
    return (*repository, branch, path, number, node, body_digest, head_sha)


def _primitive_worktree_request(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "component",
            "physical_checkout",
            "label",
            "repository",
            "branch",
            "expected_head_sha",
            "roots",
        },
        context,
    )
    component = _string(item["component"], f"{context}.component", SLOT_RE)
    checkout = _string(
        item["physical_checkout"], f"{context}.physical_checkout", SLOT_RE
    )
    label = _string(item["label"], f"{context}.label", SLOT_RE)
    repository_identity = _repository(item["repository"], f"{context}.repository")
    branch = _branch(item["branch"], f"{context}.branch")
    head = _string(item["expected_head_sha"], f"{context}.expected_head_sha", COMMIT_RE)
    roots = _request_roots(item["roots"], checkout, f"{context}.roots")
    return component, checkout, label, *repository_identity, branch, head, roots


def _retained_result(value: Any, context: str) -> bytes:
    return _inline_payload(value, context)


def _worktree_create_path(
    value: Any, request: Mapping[str, Any], context: str
) -> str:
    raw = _retained_result(value, context)
    try:
        rendered = raw.decode("utf-8")
    except UnicodeError as error:
        raise LedgerError(f"{context} is not UTF-8") from error
    if not rendered.endswith("\n") or rendered.count("\n") != 1 or "\r" in rendered:
        raise LedgerError(f"{context} must retain one exact wrapper stdout path line")
    workspace = request["roots"]["workspace"]["path"]
    return _managed_worktree_result_path(
        rendered[:-1],
        workspace,
        request["physical_checkout"],
        request["label"],
        f"{context}.path",
    )


def _expected_worktree_path(request: Mapping[str, Any]) -> str:
    return str(
        Path(request["roots"]["workspace"]["path"])
        / "worktrees"
        / request["physical_checkout"]
        / request["label"]
    )


def _worktree_list_path(
    value: Any, request: Mapping[str, Any], context: str
) -> tuple[str, str]:
    raw = _retained_result(value, context)
    rows = _decode_value(raw, context)
    if not isinstance(rows, list) or not rows or len(rows) > MAX_INVENTORY_ENTRIES:
        raise LedgerError(f"{context} must be one bounded complete worktree array")
    allowed = {
        "component",
        "worktree",
        "HEAD",
        "branch",
        "bare",
        "detached",
        "locked",
        "prunable",
    }
    normalized: list[dict[str, str]] = []
    seen_paths: set[str] = set()
    for index, row in enumerate(rows):
        if (
            not isinstance(row, dict)
            or not {"component", "worktree", "HEAD"}.issubset(row)
            or not set(row).issubset(allowed)
            or not all(isinstance(item, str) and not _contains_control(item) for item in row.values())
        ):
            raise LedgerError(f"{context}[{index}] is not a wrapper worktree-list row")
        path = _absolute_path(row["worktree"], f"{context}[{index}].worktree")
        folded = path.casefold()
        if folded in seen_paths:
            raise LedgerError(f"{context} contains a duplicate/case-alias path")
        seen_paths.add(folded)
        normalized.append({**row, "worktree": path})
    expected = _expected_worktree_path(request)
    matches = [row for row in normalized if row["worktree"] == expected]
    if len(matches) != 1:
        raise LedgerError(f"{context} lacks one exact requested worktree")
    match = matches[0]
    if (
        set(match) != {"component", "worktree", "HEAD", "branch"}
        or match["component"] != request["physical_checkout"]
        or match["HEAD"] != request["expected_head_sha"]
        or match["branch"] != f"refs/heads/{request['branch']}"
    ):
        raise LedgerError(f"{context} requested worktree is detached, locked, or drifted")
    primary = request["roots"]["primary"]["path"]
    primary_matches = [
        row
        for row in normalized
        if row["component"] == request["physical_checkout"]
        and row["worktree"] == primary
    ]
    if len(primary_matches) != 1:
        raise LedgerError(f"{context} lacks the exact precommitted primary checkout")
    return expected, byte_digest(raw)


def _recheck_pinned_directory(
    descriptor: int,
    path: str,
    context: str,
    expected: tuple[int, int] | None = None,
) -> os.stat_result:
    """Require one open no-follow directory to remain its visible path inode."""

    try:
        opened = os.fstat(descriptor)
        visible = os.stat(path, follow_symlinks=False)
    except OSError as error:
        raise LedgerError(f"{context} is not a live exact directory: {error}") from error
    identity = (opened.st_dev, opened.st_ino)
    _require_trusted_directory(opened, context)
    _require_trusted_directory(visible, context)
    if (
        not stat.S_ISDIR(opened.st_mode)
        or not stat.S_ISDIR(visible.st_mode)
        or identity != (visible.st_dev, visible.st_ino)
        or (expected is not None and identity != expected)
    ):
        raise LedgerError(f"{context} live path identity drifted")
    return opened


def _open_trusted_child_directory(
    parent: int, name: str, path: str, context: str
) -> int:
    """Open one direct no-follow directory and require its visible trusted inode."""

    direct = _direct_name(name, context)
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(direct, flags, dir_fd=parent)
    except OSError as error:
        raise LedgerError(f"{context} is not a live no-follow directory: {error}") from error
    try:
        opened = os.fstat(descriptor)
        visible = os.stat(direct, dir_fd=parent, follow_symlinks=False)
        _require_trusted_directory(opened, context)
        _require_trusted_directory(visible, context)
        if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
            raise LedgerError(f"{context} live path identity drifted")
        _recheck_pinned_directory(descriptor, path, context)
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def _recheck_live_worktree_paths(
    request: Mapping[str, Any],
    path: str,
    descriptors: Mapping[str, int],
) -> None:
    """Recheck every root and exact managed-path ancestor held by a live guard."""

    for name in ("wrapper", "workspace", "primary"):
        root_path, device, inode = _path_identity(
            request["roots"][name], f"live proof roots.{name}"
        )
        _recheck_pinned_directory(
            descriptors[name], root_path, f"live proof roots.{name}", (device, inode)
        )
    workspace = Path(request["roots"]["workspace"]["path"])
    managed = (
        ("worktrees_root", workspace / "worktrees", "live proof worktrees root"),
        (
            "physical_root",
            workspace / "worktrees" / request["physical_checkout"],
            "live proof physical-checkout worktree root",
        ),
        ("worktree", Path(path), "live proof worktree"),
    )
    for key, expected, context in managed:
        _recheck_pinned_directory(descriptors[key], str(expected), context)


@contextmanager
def _pinned_live_worktree(
    request: Mapping[str, Any],
    path: str,
    context: str,
    *,
    allowed_references: Iterable[str] = (),
    scope_record: Mapping[str, Any] | None = None,
) -> Iterator[_LiveWorktreeGuard]:
    """Open every precommitted root and the worktree without following links."""

    descriptors: dict[str, int] = {}
    try:
        for name in ("wrapper", "workspace", "primary"):
            identity = request["roots"][name]
            root_path, device, inode = _path_identity(
                identity, f"{context}.roots.{name}"
            )
            try:
                descriptors[name] = _directory_fd(Path(root_path))
            except OSError as error:
                raise LedgerError(
                    f"{context}.roots.{name} is not a live no-follow directory: {error}"
                ) from error
            _recheck_pinned_directory(
                descriptors[name],
                root_path,
                f"{context}.roots.{name}",
                (device, inode),
            )
        expected_path = _expected_worktree_path(request)
        if path != expected_path:
            raise LedgerError(f"{context}.worktree is outside its managed request path")
        workspace = Path(request["roots"]["workspace"]["path"])
        descriptors["worktrees_root"] = _open_trusted_child_directory(
            descriptors["workspace"],
            "worktrees",
            str(workspace / "worktrees"),
            f"{context}.worktrees root",
        )
        descriptors["physical_root"] = _open_trusted_child_directory(
            descriptors["worktrees_root"],
            request["physical_checkout"],
            str(workspace / "worktrees" / request["physical_checkout"]),
            f"{context}.physical-checkout worktree root",
        )
        descriptors["worktree"] = _open_trusted_child_directory(
            descriptors["physical_root"],
            request["label"],
            path,
            f"{context}.worktree",
        )
        _recheck_live_worktree_paths(request, path, descriptors)
        _manifest_checkout(descriptors["wrapper"], request, f"{context} preflight")
        primary_authority = _pin_checkout_git_authority(
            descriptors["primary"],
            request["roots"]["primary"]["path"],
            f"{context} primary preflight",
        )
        worktree_authority = None
        try:
            worktree_authority = _pin_checkout_git_authority(
                descriptors["worktree"],
                path,
                f"{context} worktree preflight",
                expected_branch=request["branch"],
                expected_head=request["expected_head_sha"],
                require_index=True,
            )
            if primary_authority.common_path != worktree_authority.common_path:
                raise LedgerError(f"{context} worktree common Git directory is foreign")
            primary_authority.recheck()
            worktree_authority.recheck()
        finally:
            if worktree_authority is not None:
                worktree_authority.close()
            primary_authority.close()
        allowed = frozenset(allowed_references)
        with _workspace_safety_lease(
            request, path, allowed, context, scope_record=scope_record
        ) as authority_recheck:
            guard = _LiveWorktreeGuard(
                request, path, descriptors, allowed, authority_recheck
            )
            guard.prove()
            yield guard
    finally:
        for descriptor in reversed(tuple(descriptors.values())):
            os.close(descriptor)


def _enter_workspace_environment(workspace_root: str) -> dict[str, str]:
    """Serialize and install one minimal wrapper/Git authority environment."""

    _WORKSPACE_LOAD_LOCK.acquire()
    try:
        saved = {
            key: value
            for key, value in os.environ.items()
            if key == "ATRINIK_WORKSPACE_DIR" or key.startswith("GIT_")
        }
        for key in tuple(os.environ):
            if key == "ATRINIK_WORKSPACE_DIR" or key.startswith("GIT_"):
                os.environ.pop(key, None)
        os.environ.update(
            ATRINIK_WORKSPACE_DIR=workspace_root,
            GIT_OPTIONAL_LOCKS="0",
            GIT_CONFIG_NOSYSTEM="1",
            GIT_CONFIG_GLOBAL="/dev/null",
            GIT_CONFIG_SYSTEM="/dev/null",
            GIT_NO_REPLACE_OBJECTS="1",
            GIT_CONFIG_COUNT="2",
            GIT_CONFIG_KEY_0="core.fsmonitor",
            GIT_CONFIG_VALUE_0="false",
            GIT_CONFIG_KEY_1="core.untrackedCache",
            GIT_CONFIG_VALUE_1="false",
        )
        return saved
    except BaseException:
        _WORKSPACE_LOAD_LOCK.release()
        raise


def _leave_workspace_environment(saved: Mapping[str, str]) -> None:
    try:
        for key in tuple(os.environ):
            if key == "ATRINIK_WORKSPACE_DIR" or key.startswith("GIT_"):
                os.environ.pop(key, None)
        os.environ.update(saved)
    finally:
        _WORKSPACE_LOAD_LOCK.release()


@contextmanager
def _workspace_safety_lease(
    request: Mapping[str, Any],
    path: str,
    allowed_references: frozenset[str],
    context: str,
    *,
    scope_record: Mapping[str, Any] | None = None,
) -> Iterator[Callable[[], None]]:
    """Use wrapper leases/reference logic to prove inactive, owned reuse."""

    wrapper_root = request["roots"]["wrapper"]["path"]
    workspace_root = request["roots"]["workspace"]["path"]
    saved_environment = _enter_workspace_environment(workspace_root)
    workspace = None
    try:
        module = _load_workspace_module(wrapper_root)
        workspace = module.Workspace(Path(wrapper_root), backfill_references=False)
    except Exception as error:
        _leave_workspace_environment(saved_environment)
        raise LedgerError(f"{context} cannot establish wrapper safety proof: {error}") from error
    try:
        if (
            str(workspace.paths.repository) != wrapper_root
            or str(workspace.paths.workspace) != workspace_root
        ):
            raise LedgerError(f"{context} wrapper/workspace roots differ from live request")
        wrapper_self = (
            request["component"] == "atrinik"
            and request["physical_checkout"] == "atrinik"
            and request["roots"]["primary"] == request["roots"]["wrapper"]
        )
        if wrapper_self:
            checkout = None
            admin_coordinate = workspace._wrapper_git_admin_coordinate()
        else:
            checkout = workspace._resolve_checkout(request["component"])
            if (
                checkout.name != request["physical_checkout"]
                or checkout.repository
                != f"{request['repository']['owner']}/{request['repository']['name']}"
                or str(workspace._primary_path(checkout))
                != request["roots"]["primary"]["path"]
            ):
                raise LedgerError(
                    f"{context} wrapper component/checkout/repository differs"
                )
            workspace._validate_checkout(
                checkout, Path(request["roots"]["primary"]["path"]), trace=False
            )
            admin_coordinate = workspace._git_admin_coordinate(
                checkout, Path(request["roots"]["primary"]["path"])
            )
        checkout_name = request["physical_checkout"]
        requests = [
            workspace._lease_request(
                "git-admin", admin_coordinate, "shared", "delivery live proof"
            ),
            workspace._lease_request(
                "registry", "physical-references", "shared", "delivery live proof"
            ),
            workspace._lease_request(
                "source",
                workspace._source_coordinate(checkout_name, Path(path)),
                "exclusive",
                "delivery live proof",
            ),
            workspace._lease_request(
                "source",
                workspace._physical_source_coordinate(Path(path)),
                "exclusive",
                "delivery live proof",
            ),
        ]
        if scope_record is not None:
            requests.extend(
                (
                    workspace._lease_request(
                        "registry",
                        f"scope:{scope_record['name']}",
                        "shared",
                        "delivery scope proof",
                    ),
                    workspace._lease_request(
                        "profile",
                        scope_record["profile"]["name"],
                        "shared",
                        "delivery scope proof",
                    ),
                    workspace._lease_request(
                        "topology",
                        scope_record["topology"]["name"],
                        "shared",
                        "delivery scope proof",
                    ),
                )
            )
        try:
            locks = workspace._resource_locks(requests, nonblocking=True)
            with locks:
                def recheck() -> None:
                    try:
                        if scope_record is not None:
                            _verify_live_scope(workspace, scope_record, context)
                        references = set(workspace._source_references(Path(path)))
                        references.update(
                            _manual_scope_references(
                                workspace_root, path, allowed_references
                            )
                        )
                        if references != allowed_references:
                            raise LedgerError(
                                f"{context} worktree reference set differs: expected "
                                f"{sorted(allowed_references)}, observed "
                                f"{sorted(references)}"
                            )
                    except LedgerError:
                        raise
                    except Exception as error:
                        raise LedgerError(
                            f"{context} wrapper authority recheck failed: {error}"
                        ) from error

                recheck()
                yield recheck
        except LedgerError:
            raise
        except Exception as error:
            raise LedgerError(
                f"{context} worktree is active or wrapper safety is uncertain: {error}"
            ) from error
    finally:
        try:
            workspace.close()
        finally:
            _leave_workspace_environment(saved_environment)


def _verify_live_scope(
    workspace: Any, retained: Mapping[str, Any], context: str
) -> None:
    """Require the pinned wrapper to return the retained complete live scope."""

    name = retained.get("name")
    if not isinstance(name, str) or not name:
        raise LedgerError(f"{context} retained scope name is invalid")
    workspace_directory = None
    scopes_directory = None
    scope_directory = None
    profiles_directory = None
    try:
        workspace_path = Path(workspace.paths.workspace)
        workspace_directory = _directory_fd(workspace_path)
        _recheck_pinned_directory(
            workspace_directory, str(workspace_path), f"{context} workspace root"
        )
        scopes_path = workspace_path / "scopes"
        scopes_directory = _open_trusted_child_directory(
            workspace_directory,
            "scopes",
            str(scopes_path),
            f"{context} scopes root",
        )
        scope_path = scopes_path / name
        scope_directory = _open_trusted_child_directory(
            scopes_directory, name, str(scope_path), f"{context} scope directory"
        )
        raw, scope_status = _read_regular(scope_directory, "scope.json")
        _require_trusted_regular(
            scope_status, f"{context} authoritative scope.json"
        )
        direct = _decode(raw, f"{context} live scope")
        if direct != retained:
            raise LedgerError(f"{context} live scope file differs from retained scope show")
        scope_show = getattr(workspace, "scope_show", None)
        live = scope_show(name) if callable(scope_show) else direct
        profile = retained.get("profile")
        if not isinstance(profile, dict):
            raise LedgerError(f"{context} retained scope profile is invalid")
        profile_path = Path(profile.get("path", ""))
        profiles_path = workspace_path / "profiles"
        if profile_path.parent != profiles_path:
            raise LedgerError(f"{context} scope profile path escaped profiles root")
        profiles_directory = _open_trusted_child_directory(
            workspace_directory,
            "profiles",
            str(profiles_path),
            f"{context} scope profiles root",
        )
        profile_raw, profile_status = _read_regular(
            profiles_directory, profile_path.name
        )
        _require_trusted_regular(
            profile_status, f"{context} scope profile {profile_path}"
        )
        if (
            (profile_status.st_dev, profile_status.st_ino)
            != (profile.get("path_device"), profile.get("path_inode"))
            or byte_digest(profile_raw) != profile.get("sha256")
        ):
            raise LedgerError(
                f"{context} live scope profile differs from retained identity"
            )
        try:
            os.stat(
                "release-journal.json",
                dir_fd=scope_directory,
                follow_symlinks=False,
            )
        except FileNotFoundError:
            pass
        else:
            raise LedgerError(f"{context} scope release has started")
        _recheck_pinned_directory(
            scope_directory, str(scope_path), f"{context} scope directory"
        )
        _recheck_pinned_directory(
            scopes_directory, str(scopes_path), f"{context} scopes root"
        )
        _recheck_pinned_directory(
            profiles_directory, str(profiles_path), f"{context} scope profiles root"
        )
        _recheck_pinned_directory(
            workspace_directory, str(workspace_path), f"{context} workspace root"
        )
    except Exception as error:
        if isinstance(error, LedgerError):
            raise
        raise LedgerError(f"{context} cannot prove live scope: {error}") from error
    finally:
        if profiles_directory is not None:
            os.close(profiles_directory)
        if scope_directory is not None:
            os.close(scope_directory)
        if scopes_directory is not None:
            os.close(scopes_directory)
        if workspace_directory is not None:
            os.close(workspace_directory)
    if live != retained:
        raise LedgerError(f"{context} live scope differs from retained scope show")


@dataclass(frozen=True)
class _WorkspacePackageSnapshot:
    """Exact trusted source bytes and full-tree identity for one package load."""

    device: int
    inode: int
    fingerprint: str
    sources: tuple[tuple[str, bytes], ...]


class _SnapshotSourceLoader(importlib.abc.Loader):
    """Execute only bytes retained by package prevalidation."""

    def __init__(
        self,
        source_path: Path,
        relative_path: str,
        raw: bytes,
    ) -> None:
        self.source_path = source_path
        self.relative_path = relative_path
        self.raw = raw

    def create_module(self, spec: Any) -> Any:
        return None

    def exec_module(self, module: Any) -> None:
        code = compile(
            self.raw,
            str(self.source_path),
            "exec",
            dont_inherit=True,
        )
        exec(code, module.__dict__)


class _SnapshotPackageFinder(importlib.abc.MetaPathFinder):
    """Resolve one private alias exclusively from a retained source snapshot."""

    def __init__(
        self,
        package_name: str,
        package_root: Path,
        snapshot: _WorkspacePackageSnapshot,
    ) -> None:
        self.package_name = package_name
        self.package_root = package_root
        self.sources = dict(snapshot.sources)

    def find_spec(
        self,
        fullname: str,
        path: Sequence[str] | None = None,
        target: Any = None,
    ) -> Any:
        del path, target
        if fullname == self.package_name:
            relative = "__init__.py"
            is_package = True
        elif fullname.startswith(self.package_name + "."):
            suffix = fullname[len(self.package_name) + 1 :]
            parts = suffix.split(".")
            package_relative = "/".join((*parts, "__init__.py"))
            module_relative = "/".join(parts) + ".py"
            if package_relative in self.sources:
                relative = package_relative
                is_package = True
            elif module_relative in self.sources:
                relative = module_relative
                is_package = False
            else:
                raise ModuleNotFoundError(
                    f"module {fullname!r} is absent from the validated wrapper snapshot"
                )
        else:
            return None
        raw = self.sources.get(relative)
        if raw is None:
            raise ModuleNotFoundError(
                f"module {fullname!r} is absent from the validated wrapper snapshot"
            )
        source_path = self.package_root / relative
        loader = _SnapshotSourceLoader(source_path, relative, raw)
        spec = importlib.util.spec_from_loader(
            fullname,
            loader,
            origin=str(source_path),
            is_package=is_package,
        )
        if spec is None:
            raise ImportError(f"cannot create validated wrapper spec for {fullname!r}")
        spec.has_location = True
        if is_package:
            spec.submodule_search_locations = [str(source_path.parent)]
        return spec


def _discard_snapshot_package(package_name: str) -> None:
    """Remove one failed private package and its retained finder."""

    for name in tuple(sys.modules):
        if name == package_name or name.startswith(package_name + "."):
            sys.modules.pop(name, None)
    sys.meta_path[:] = [
        finder
        for finder in sys.meta_path
        if not (
            isinstance(finder, _SnapshotPackageFinder)
            and finder.package_name == package_name
        )
    ]


def _prevalidate_workspace_package(package_root: Path) -> _WorkspacePackageSnapshot:
    """Trust a bounded no-follow package tree before Python can execute any of it."""

    root = _directory_fd(package_root)
    entries = 0
    total_bytes = 0
    fingerprint = hashlib.sha256()
    sources: list[tuple[str, bytes]] = []
    root_status = os.fstat(root)
    _require_trusted_directory(root_status, "wrapper authority package")

    def bind_entry(
        relative: str, kind: str, status: os.stat_result, raw: bytes = b""
    ) -> None:
        metadata = canonical_bytes(
            {
                "path": relative,
                "kind": kind,
                "mode": status.st_mode,
                "device": status.st_dev,
                "inode": status.st_ino,
                "size": len(raw),
            }
        )
        fingerprint.update(len(metadata).to_bytes(8, "big"))
        fingerprint.update(metadata)
        fingerprint.update(len(raw).to_bytes(8, "big"))
        fingerprint.update(raw)

    bind_entry(".", "directory", root_status)

    def walk(directory: int, path: Path) -> None:
        nonlocal entries, total_bytes
        try:
            children = sorted(os.scandir(directory), key=lambda item: item.name)
        except OSError as error:
            raise LedgerError(f"cannot inventory wrapper authority package: {error}") from error
        for child in children:
            entries += 1
            if entries > MAX_INVENTORY_ENTRIES:
                raise LedgerError("wrapper authority package has too many entries")
            _direct_name(child.name, "wrapper authority package entry")
            try:
                status = child.stat(follow_symlinks=False)
            except OSError as error:
                raise LedgerError(
                    f"cannot inspect wrapper authority package entry: {error}"
                ) from error
            child_path = path / child.name
            relative = child_path.relative_to(package_root).as_posix()
            if stat.S_ISDIR(status.st_mode):
                descriptor = _open_trusted_child_directory(
                    directory,
                    child.name,
                    str(child_path),
                    f"wrapper authority package directory {child_path}",
                )
                try:
                    bind_entry(relative, "directory", os.fstat(descriptor))
                    walk(descriptor, child_path)
                    _recheck_pinned_directory(
                        descriptor,
                        str(child_path),
                        f"wrapper authority package directory {child_path}",
                    )
                finally:
                    os.close(descriptor)
                continue
            if not stat.S_ISREG(status.st_mode):
                raise LedgerError(
                    f"wrapper authority package entry is not regular: {child_path}"
                )
            raw, read_status = _read_regular(directory, child.name)
            _require_trusted_regular(
                read_status, f"wrapper authority package file {child_path}"
            )
            bind_entry(relative, "file", read_status, raw)
            if relative.endswith(".py"):
                sources.append((relative, raw))
            total_bytes += len(raw)
            if total_bytes > MAX_INVENTORY_BYTES:
                raise LedgerError("wrapper authority package bytes are not bounded")

    try:
        walk(root, package_root)
        _recheck_pinned_directory(
            root, str(package_root), "wrapper authority package"
        )
        source_names = {name for name, _ in sources}
        if not {"__init__.py", "workspace.py"}.issubset(source_names):
            raise LedgerError(
                "wrapper authority package lacks required source modules"
            )
        return _WorkspacePackageSnapshot(
            root_status.st_dev,
            root_status.st_ino,
            fingerprint.hexdigest(),
            tuple(sources),
        )
    finally:
        os.close(root)


def _load_workspace_module(wrapper_root: str) -> Any:
    """Load pretrusted wrapper authority under an inode-specific package."""

    package_root = Path(wrapper_root) / "atrinik_workspace"
    snapshot = _prevalidate_workspace_package(package_root)
    path_token = hashlib.sha256(wrapper_root.encode("utf-8")).hexdigest()[:16]
    identity = (
        f"{snapshot.device:x}_{snapshot.inode:x}_{path_token}_"
        f"{snapshot.fingerprint}"
    )
    package_name = f"_atrinik_delivery_workspace_{identity}"
    workspace_name = f"{package_name}.workspace"
    existing = sys.modules.get(workspace_name)
    if existing is None:
        finder = _SnapshotPackageFinder(package_name, package_root, snapshot)
        sys.meta_path.insert(0, finder)
        try:
            importlib.import_module(package_name)
            existing = importlib.import_module(workspace_name)
        except BaseException:
            _discard_snapshot_package(package_name)
            raise
    after = _prevalidate_workspace_package(package_root)
    if after.fingerprint != snapshot.fingerprint:
        _discard_snapshot_package(package_name)
        raise LedgerError("wrapper authority package changed during import")
    for name, module in tuple(sys.modules.items()):
        if name != package_name and not name.startswith(package_name + "."):
            continue
        source = getattr(module, "__file__", None)
        if not isinstance(source, str):
            continue
        source_path = Path(os.path.abspath(source))
        try:
            source_path.relative_to(package_root)
            metadata = source_path.stat(follow_symlinks=False)
        except (ValueError, OSError) as error:
            raise LedgerError("wrapper authority module escaped its pinned package") from error
        if (
            not stat.S_ISREG(metadata.st_mode)
            or metadata.st_uid != os.geteuid()
            or stat.S_IMODE(metadata.st_mode) & 0o022
        ):
            raise LedgerError("wrapper authority module source is unsafe")
    return existing


def _manual_scope_references(
    workspace_root: str, path: str, allowed: frozenset[str]
) -> set[str]:
    """Anchor the owning scope record on branches whose Workspace is older."""

    result: set[str] = set()
    for reference in allowed:
        if not reference.startswith("scope:"):
            continue
        name = reference.removeprefix("scope:")
        record_path = Path(workspace_root) / "scopes" / name / "scope.json"
        raw = _read_bytes_input(str(record_path))
        record = _decode(raw, f"scope reference {name}")
        rows = record.get("worktrees")
        if (
            record.get("schema_version") != 1
            or record.get("status") != "complete"
            or record.get("name") != name
            or not isinstance(rows, list)
            or len(rows) != 1
            or not isinstance(rows[0], dict)
            or rows[0].get("path") != path
        ):
            raise LedgerError(f"scope reference {name} is not exact and complete")
        result.add(reference)
    return result


def _git(
    descriptor: int,
    arguments: Sequence[str],
    context: str,
    *,
    accepted: set[int] | None = None,
    effective_config: bool = False,
) -> tuple[int, bytes]:
    """Run one bounded read-only Git query against a pinned directory."""

    allowed = {0} if accepted is None else accepted
    command = [
        "git",
        "--no-pager",
        "-c",
        "core.hooksPath=/dev/null",
        "-c",
        "core.fsmonitor=false",
        "-c",
        "core.untrackedCache=false",
        "-C",
        f"/proc/self/fd/{descriptor}",
        *arguments,
    ]
    environment = {
        key: value for key, value in os.environ.items() if not key.startswith("GIT_")
    }
    environment.update(
        LC_ALL="C",
        LANG="C",
        GIT_OPTIONAL_LOCKS="0",
        GIT_NO_REPLACE_OBJECTS="1",
        GIT_TERMINAL_PROMPT="0",
    )
    if not effective_config:
        environment.update(
            GIT_CONFIG_NOSYSTEM="1",
            GIT_CONFIG_GLOBAL="/dev/null",
            GIT_CONFIG_SYSTEM="/dev/null",
        )
    try:
        process = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            pass_fds=(descriptor,),
            env=environment,
        )
    except OSError as error:
        raise LedgerError(f"cannot prove {context}: {error}") from error
    if process.stdout is None or process.stderr is None:  # pragma: no cover - PIPE invariant
        process.kill()
        raise LedgerError(f"cannot prove {context}: Git pipes are unavailable")
    stdout_fd = process.stdout.fileno()
    stderr_fd = process.stderr.fileno()
    output = {stdout_fd: bytearray(), stderr_fd: bytearray()}
    limits = {
        stdout_fd: MAX_INVENTORY_BYTES,
        stderr_fd: MAX_BYTES,
    }
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    selector.register(process.stderr, selectors.EVENT_READ)
    deadline = time.monotonic() + 30
    try:
        while selector.get_map():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise LedgerError(f"{context} Git query timed out")
            events = selector.select(remaining)
            if not events:
                raise LedgerError(f"{context} Git query timed out")
            for key, _ in events:
                chunk = os.read(key.fd, 65536)
                if not chunk:
                    selector.unregister(key.fileobj)
                    continue
                output[key.fd].extend(chunk)
                if len(output[key.fd]) > limits[key.fd]:
                    raise LedgerError(f"{context} output is not bounded")
        remaining = max(0.001, deadline - time.monotonic())
        returncode = process.wait(timeout=remaining)
    except (LedgerError, subprocess.TimeoutExpired) as error:
        process.kill()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        if isinstance(error, LedgerError):
            raise
        raise LedgerError(f"{context} Git query timed out") from error
    finally:
        selector.close()
        process.stdout.close()
        process.stderr.close()
    stdout = bytes(output[stdout_fd])
    stderr = bytes(output[stderr_fd])
    if returncode not in allowed:
        detail = stderr.decode("utf-8", "replace").strip()
        raise LedgerError(f"cannot prove {context}: {detail or returncode}")
    return returncode, stdout


def _one_git_line(raw: bytes, context: str) -> str:
    try:
        value = raw.decode("utf-8")
    except UnicodeError as error:
        raise LedgerError(f"{context} is not UTF-8") from error
    if not value.endswith("\n") or value.count("\n") != 1 or "\r" in value:
        raise LedgerError(f"{context} is not one canonical line")
    result = value[:-1]
    if not result or _contains_control(result):
        raise LedgerError(f"{context} is invalid")
    return result


def _git_worktree_records(raw: bytes, context: str) -> list[dict[str, str]]:
    if not raw.endswith(b"\0\0"):
        raise LedgerError(f"{context} is not NUL-delimited porcelain")
    records: list[dict[str, str]] = []
    current: dict[str, str] = {}
    for index, token in enumerate(raw.split(b"\0")):
        if not token:
            if current:
                records.append(current)
                current = {}
            continue
        try:
            line = token.decode("utf-8")
        except UnicodeError as error:
            raise LedgerError(f"{context}[{index}] is not UTF-8") from error
        key, separator, value = line.partition(" ")
        if (
            not key
            or _contains_control(key)
            or _contains_control(value)
            or key in current
            or (not separator and key not in {"bare", "detached"})
        ):
            raise LedgerError(f"{context}[{index}] is malformed")
        current[key] = value
    if current or not records or len(records) > MAX_INVENTORY_ENTRIES:
        raise LedgerError(f"{context} is incomplete or oversized")
    return records


def _normalized_github_remote(value: str) -> str | None:
    normalized = value.strip().removesuffix("/").removesuffix(".git")
    if normalized.startswith("git@github.com:"):
        normalized = "github.com/" + normalized.removeprefix("git@github.com:")
    elif normalized.startswith("ssh://git@github.com/"):
        normalized = "github.com/" + normalized.removeprefix("ssh://git@github.com/")
    elif normalized.startswith("https://"):
        normalized = normalized.removeprefix("https://")
    else:
        return None
    prefix = "github.com/"
    return normalized.removeprefix(prefix).casefold() if normalized.startswith(prefix) else None


def _git_authority_entry_exists(directory: int, name: str) -> bool:
    try:
        os.stat(_direct_name(name), dir_fd=directory, follow_symlinks=False)
    except FileNotFoundError:
        return False
    except OSError as error:
        raise LedgerError(f"cannot inspect Git authority entry {name}: {error}") from error
    return True


def _open_git_authority_path(
    authority: _PinnedGitAuthority,
    parent: int,
    parent_path: str,
    parts: Sequence[str],
    context: str,
) -> tuple[int, str] | None:
    """Open and retain one exact trusted relative directory chain."""

    current = parent
    current_path = Path(parent_path)
    for index, part in enumerate(parts):
        direct = _direct_name(part, f"{context}[{index}]")
        next_path = current_path / direct
        flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        try:
            descriptor = os.open(direct, flags, dir_fd=current)
        except FileNotFoundError:
            authority.add_absence(current, direct, f"{context} component")
            return None
        except OSError as error:
            raise LedgerError(f"{context} is not a trusted no-follow path: {error}") from error
        try:
            authority.add_directory(descriptor, str(next_path), context)
        except BaseException:
            os.close(descriptor)
            raise
        current = descriptor
        current_path = next_path
    return current, str(current_path)


def _packed_ref_matches(raw: bytes, reference: str, expected_sha: str) -> bool:
    """Parse a bounded SHA-1 packed-refs file and find one exact branch row."""

    try:
        lines = raw.decode("ascii").splitlines()
    except UnicodeError as error:
        raise LedgerError("packed-refs is not ASCII") from error
    matches = 0
    for index, line in enumerate(lines):
        if not line or line.startswith("#"):
            if _contains_control(line):
                raise LedgerError(f"packed-refs[{index}] is malformed")
            continue
        if line.startswith("^"):
            if not COMMIT_RE.fullmatch(line[1:]):
                raise LedgerError(f"packed-refs[{index}] peel row is malformed")
            continue
        sha, separator, name = line.partition(" ")
        if not separator or not COMMIT_RE.fullmatch(sha):
            raise LedgerError(f"packed-refs[{index}] is malformed")
        _git_symbolic_ref(name, f"packed-refs[{index}] reference")
        if name == reference:
            matches += 1
            if sha != expected_sha:
                raise LedgerError("packed requested branch differs from expected HEAD")
    return matches == 1


def _pin_checkout_git_authority(
    checkout_descriptor: int,
    checkout_path: str,
    context: str,
    *,
    expected_branch: str | None = None,
    expected_head: str | None = None,
    require_index: bool = False,
) -> _PinnedGitAuthority:
    """Pin exact local Git config/admin/ref authority before invoking Git."""

    authority = _PinnedGitAuthority()
    try:
        git_status = os.stat(
            ".git", dir_fd=checkout_descriptor, follow_symlinks=False
        )
        if stat.S_ISDIR(git_status.st_mode):
            _require_trusted_directory(git_status, f"{context} .git")
            admin_path = os.path.normpath(os.path.join(checkout_path, ".git"))
        elif stat.S_ISREG(git_status.st_mode):
            git_raw = authority.add_file(
                checkout_descriptor, ".git", f"{context} .git gitfile"
            )
            line = _single_ascii_line(git_raw, f"{context} .git gitfile")
            prefix = "gitdir: "
            if not line.startswith(prefix):
                raise LedgerError(f"{context} .git gitfile lacks gitdir identity")
            admin_path = line.removeprefix(prefix)
            if not os.path.isabs(admin_path):
                admin_path = os.path.join(checkout_path, admin_path)
            admin_path = os.path.normpath(admin_path)
        else:
            raise LedgerError(f"{context} .git is not a directory or regular gitfile")
        if admin_path == "/" or _contains_control(admin_path):
            raise LedgerError(f"{context} Git admin directory is unsafe")

        try:
            admin = _directory_fd(Path(admin_path))
        except OSError as error:
            raise LedgerError(f"{context} Git admin directory is unsafe: {error}") from error
        try:
            authority.add_directory(admin, admin_path, f"{context} Git admin directory")
        except BaseException:
            os.close(admin)
            raise

        commondir_present = _git_authority_entry_exists(admin, "commondir")
        if commondir_present:
            commondir_raw = authority.add_file(
                admin, "commondir", f"{context} commondir"
            )
            common_path = _single_ascii_line(commondir_raw, f"{context} commondir")
            if not os.path.isabs(common_path):
                common_path = os.path.join(admin_path, common_path)
            common_path = os.path.normpath(common_path)
        else:
            authority.add_absence(admin, "commondir", f"{context} commondir")
            common_path = admin_path
        if common_path == "/" or _contains_control(common_path):
            raise LedgerError(f"{context} common Git directory is unsafe")
        authority.common_path = common_path

        try:
            common = _directory_fd(Path(common_path))
        except OSError as error:
            raise LedgerError(f"{context} common Git directory is unsafe: {error}") from error
        try:
            authority.add_directory(
                common, common_path, f"{context} common Git directory"
            )
        except BaseException:
            os.close(common)
            raise

        if commondir_present:
            admin_object = Path(admin_path)
            registration_parent = Path(common_path) / "worktrees"
            if admin_object.parent != registration_parent:
                raise LedgerError(
                    f"{context} Git admin is not one direct common worktree registration"
                )
            registered = _open_git_authority_path(
                authority,
                common,
                common_path,
                ("worktrees", admin_object.name),
                f"{context} common worktree registration",
            )
            if registered is None or (
                os.fstat(registered[0]).st_dev,
                os.fstat(registered[0]).st_ino,
            ) != (os.fstat(admin).st_dev, os.fstat(admin).st_ino):
                raise LedgerError(
                    f"{context} Git admin differs from its common worktree registration"
                )

        config_raw = authority.add_file(common, "config", f"{context} local config")
        if not config_raw:
            raise LedgerError(f"{context} local config is empty")
        if _git_authority_entry_exists(admin, "config.worktree"):
            authority.add_file(
                admin, "config.worktree", f"{context} worktree config"
            )
        else:
            authority.add_absence(
                admin, "config.worktree", f"{context} worktree config"
            )

        head_raw = authority.add_file(admin, "HEAD", f"{context} admin HEAD")
        head_line = _single_ascii_line(head_raw, f"{context} admin HEAD")
        if head_line.startswith("ref: "):
            head_reference = _git_symbolic_ref(
                head_line.removeprefix("ref: "), f"{context} admin HEAD"
            )
        elif COMMIT_RE.fullmatch(head_line):
            head_reference = None
        else:
            raise LedgerError(f"{context} admin HEAD is not canonical")
        if expected_branch is not None:
            expected_reference = f"refs/heads/{expected_branch}"
            if head_reference != expected_reference:
                raise LedgerError(f"{context} admin HEAD differs from requested branch")

        if commondir_present:
            gitdir_raw = authority.add_file(admin, "gitdir", f"{context} gitdir")
            gitdir_path = _single_ascii_line(gitdir_raw, f"{context} gitdir")
            if not os.path.isabs(gitdir_path):
                gitdir_path = os.path.join(admin_path, gitdir_path)
            if os.path.normpath(gitdir_path) != os.path.join(checkout_path, ".git"):
                raise LedgerError(f"{context} gitdir backlink differs from checkout")
        elif _git_authority_entry_exists(admin, "gitdir"):
            raise LedgerError(f"{context} has unsupported gitdir without commondir")
        else:
            authority.add_absence(admin, "gitdir", f"{context} gitdir")

        if require_index:
            authority.add_file(admin, "index", f"{context} worktree index")

        if expected_branch is not None:
            if expected_head is None:
                raise LedgerError(f"{context} expected branch lacks expected HEAD")
            reference = f"refs/heads/{expected_branch}"
            branch_parts = expected_branch.split("/")
            parent = _open_git_authority_path(
                authority,
                common,
                common_path,
                ("refs", "heads", *branch_parts[:-1]),
                f"{context} requested branch directory",
            )
            loose = False
            if parent is not None:
                parent_descriptor, _ = parent
                name = branch_parts[-1]
                if _git_authority_entry_exists(parent_descriptor, name):
                    ref_raw = authority.add_file(
                        parent_descriptor, name, f"{context} requested loose branch"
                    )
                    if _single_ascii_line(
                        ref_raw, f"{context} requested loose branch"
                    ) != expected_head:
                        raise LedgerError(
                            f"{context} requested loose branch differs from expected HEAD"
                        )
                    loose = True
                else:
                    authority.add_absence(
                        parent_descriptor,
                        name,
                        f"{context} requested loose branch",
                    )
            if not loose:
                packed = authority.add_file(
                    common, "packed-refs", f"{context} packed-refs"
                )
                if not _packed_ref_matches(packed, reference, expected_head):
                    raise LedgerError(
                        f"{context} requested branch has no exact trusted backing"
                    )
            for marker in ("locked", *_GIT_OPERATION_MARKERS):
                if _git_authority_entry_exists(admin, marker):
                    if marker == "locked":
                        raise LedgerError(f"{context} is locked")
                    raise LedgerError(
                        f"live Git worktree has operation in progress: {marker}"
                    )
                authority.add_absence(
                    admin,
                    marker,
                    f"{context} {marker} marker",
                )
        authority.recheck()
        return authority
    except BaseException:
        authority.close()
        raise


def _absolute_git_common_directory(
    descriptor: int, repository_path: str, context: str
) -> str:
    """Return Git's canonical absolute common directory for one pinned checkout."""

    _, raw = _git(
        descriptor,
        ("rev-parse", "--path-format=absolute", "--git-common-dir"),
        context,
    )
    value = _one_git_line(raw, context)
    if not os.path.isabs(value):
        value = os.path.join(repository_path, value)
    normalized = os.path.normpath(value)
    if normalized == "/" or _contains_control(normalized):
        raise LedgerError(f"{context} is not a safe absolute directory")
    return normalized


def _pin_checkout_common_directory(
    checkout_descriptor: int,
    checkout_path: str,
    reported_common: str,
    context: str,
) -> tuple[int, os.stat_result]:
    """Pin the no-follow Git metadata route and its exact common directory."""

    git_status = os.stat(".git", dir_fd=checkout_descriptor, follow_symlinks=False)
    if stat.S_ISDIR(git_status.st_mode):
        _require_trusted_directory(git_status, f"{context} .git")
        git_directory_path = os.path.normpath(os.path.join(checkout_path, ".git"))
    elif stat.S_ISREG(git_status.st_mode):
        git_raw, gitfile_status = _read_regular(checkout_descriptor, ".git")
        _require_trusted_regular(gitfile_status, f"{context} .git gitfile")
        line = _single_ascii_line(git_raw, f"{context} .git gitfile")
        prefix = "gitdir: "
        if not line.startswith(prefix):
            raise LedgerError(f"{context} .git gitfile lacks gitdir identity")
        git_directory_path = line.removeprefix(prefix)
        if not os.path.isabs(git_directory_path):
            git_directory_path = os.path.join(checkout_path, git_directory_path)
        git_directory_path = os.path.normpath(git_directory_path)
    else:
        raise LedgerError(f"{context} .git is not a directory or regular gitfile")
    if git_directory_path == "/" or _contains_control(git_directory_path):
        raise LedgerError(f"{context} Git directory is unsafe")

    git_directory = _directory_fd(Path(git_directory_path))
    try:
        _recheck_pinned_directory(
            git_directory, git_directory_path, f"{context} Git directory"
        )
        try:
            os.stat("commondir", dir_fd=git_directory, follow_symlinks=False)
        except FileNotFoundError:
            common_path = git_directory_path
        else:
            common_raw, common_status = _read_regular(git_directory, "commondir")
            _require_trusted_regular(common_status, f"{context} commondir")
            common_path = _single_ascii_line(common_raw, f"{context} commondir")
            if not os.path.isabs(common_path):
                common_path = os.path.join(git_directory_path, common_path)
            common_path = os.path.normpath(common_path)
    finally:
        os.close(git_directory)
    if common_path != reported_common:
        raise LedgerError(f"{context} common Git directory is foreign")
    common_descriptor = _directory_fd(Path(common_path))
    try:
        common_status = _recheck_pinned_directory(
            common_descriptor, common_path, f"{context} common Git directory"
        )
        return common_descriptor, common_status
    except BaseException:
        os.close(common_descriptor)
        raise


def _manifest_checkout(
    wrapper_descriptor: int, request: Mapping[str, Any], context: str
) -> Mapping[str, Any]:
    raw, manifest_status = _read_regular(wrapper_descriptor, "components.json")
    _require_trusted_regular(
        manifest_status, f"{context}.components.json manifest authority"
    )
    manifest = _decode(raw, f"{context}.components.json")
    checkouts = manifest.get("checkouts")
    components = manifest.get("components")
    if (
        not isinstance(checkouts, list)
        or not isinstance(components, list)
        or not checkouts
        or not components
        or len(checkouts) > MAX_INVENTORY_ENTRIES
        or len(components) > MAX_INVENTORY_ENTRIES
    ):
        raise LedgerError(f"{context} manifest checkout/component inventory is invalid")
    wrapper_self = (
        request["component"] == "atrinik"
        and request["physical_checkout"] == "atrinik"
        and request["repository"]["owner"] == "atrinik"
        and request["repository"]["name"] == "atrinik"
        and request["roots"]["primary"] == request["roots"]["wrapper"]
    )
    if wrapper_self:
        return {"name": "atrinik", "path": ".", "repository": "atrinik/atrinik"}
    checkout_matches = [
        item
        for item in checkouts
        if isinstance(item, dict) and item.get("name") == request["physical_checkout"]
    ]
    component_matches = [
        item
        for item in components
        if isinstance(item, dict) and item.get("name") == request["component"]
    ]
    if len(checkout_matches) != 1 or len(component_matches) != 1:
        raise LedgerError(f"{context} request is not one exact manifest component/checkout")
    checkout = checkout_matches[0]
    component = component_matches[0]
    expected_repository = (
        f"{request['repository']['owner']}/{request['repository']['name']}"
    )
    if (
        component.get("checkout") != request["physical_checkout"]
        or checkout.get("repository") != expected_repository
        or checkout.get("path") != request["physical_checkout"]
    ):
        raise LedgerError(f"{context} manifest component/checkout/repository differs")
    return checkout


def _prove_live_worktree(
    request: Mapping[str, Any],
    path: str,
    descriptors: Mapping[str, int],
    *,
    expected_tree: str | None = None,
) -> dict[str, Any]:
    """Pin authority inputs before any Git query, then recheck every live path."""

    _recheck_live_worktree_paths(request, path, descriptors)
    primary_path = request["roots"]["primary"]["path"]
    primary_authority = _pin_checkout_git_authority(
        descriptors["primary"],
        primary_path,
        "live primary checkout",
    )
    worktree_authority = None
    try:
        worktree_authority = _pin_checkout_git_authority(
            descriptors["worktree"],
            path,
            "live worktree checkout",
            expected_branch=request["branch"],
            expected_head=request["expected_head_sha"],
            require_index=True,
        )
        if primary_authority.common_path != worktree_authority.common_path:
            raise LedgerError("live worktree common Git directory is foreign")
        proof = _prove_live_worktree_core(
            request, path, descriptors, expected_tree=expected_tree
        )
        primary_authority.recheck()
        worktree_authority.recheck()
        _recheck_live_worktree_paths(request, path, descriptors)
        return proof
    finally:
        if worktree_authority is not None:
            worktree_authority.close()
        primary_authority.close()


def _prove_live_worktree_core(
    request: Mapping[str, Any],
    path: str,
    descriptors: Mapping[str, int],
    *,
    expected_tree: str | None = None,
) -> dict[str, Any]:
    """Recompute every Git-backed reusable-safe fact from pinned live roots."""

    for name in ("wrapper", "workspace", "primary"):
        root_path, device, inode = _path_identity(
            request["roots"][name], f"live proof roots.{name}"
        )
        _recheck_pinned_directory(
            descriptors[name], root_path, f"live proof roots.{name}", (device, inode)
        )
    worktree_status = _recheck_pinned_directory(
        descriptors["worktree"], path, "live proof worktree"
    )
    _manifest_checkout(descriptors["wrapper"], request, "live proof")

    primary = descriptors["primary"]
    worktree = descriptors["worktree"]
    expected_repository = (
        f"{request['repository']['owner']}/{request['repository']['name']}".casefold()
    )
    for route_descriptor, route_context in (
        (primary, "primary"),
        (worktree, "worktree"),
    ):
        _, remote_raw = _git(
            route_descriptor,
            (
                "config",
                "--local",
                "--no-includes",
                "--get-all",
                "remote.origin.url",
            ),
            f"{route_context} origin URL",
        )
        try:
            remote_lines = remote_raw.decode("utf-8").splitlines()
        except UnicodeError as error:
            raise LedgerError(f"{route_context} origin URL is not UTF-8") from error
        if len(remote_lines) != 1 or any(
            _normalized_github_remote(value) != expected_repository
            for value in remote_lines
        ):
            raise LedgerError(
                f"live Git {route_context} origin is foreign to the requested repository"
            )
        for arguments, label in (
            (
                ("remote", "get-url", "--all", "origin"),
                f"resolved {route_context} origin URL",
            ),
            (
                ("remote", "get-url", "--push", "--all", "origin"),
                f"resolved {route_context} origin push URL",
            ),
        ):
            _, resolved_raw = _git(
                route_descriptor, arguments, label, effective_config=True
            )
            try:
                resolved_lines = resolved_raw.decode("utf-8").splitlines()
            except UnicodeError as error:
                raise LedgerError(f"{label} is not UTF-8") from error
            if len(resolved_lines) != 1 or (
                _normalized_github_remote(resolved_lines[0]) != expected_repository
            ):
                raise LedgerError(
                    "live Git origin fetch/push route is foreign to the "
                    "requested repository"
                )

    _, head_raw = _git(worktree, ("rev-parse", "--verify", "HEAD"), "worktree HEAD")
    head = _one_git_line(head_raw, "worktree HEAD")
    _, branch_raw = _git(worktree, ("symbolic-ref", "--quiet", "HEAD"), "worktree branch")
    branch = _one_git_line(branch_raw, "worktree branch")
    if head != request["expected_head_sha"] or branch != f"refs/heads/{request['branch']}":
        raise LedgerError("live Git worktree head/branch differs from the request")
    _, shared_index_raw = _git(
        worktree,
        ("rev-parse", "--shared-index-path"),
        "worktree shared index",
    )
    if shared_index_raw not in {b"", b"\n"}:
        _one_git_line(shared_index_raw, "worktree shared index")
        raise LedgerError("live Git worktree split index is unsupported")
    _, index_raw = _git(
        worktree, ("ls-files", "--stage", "-z"), "worktree index"
    )
    if any(record.startswith(b"160000 ") for record in index_raw.split(b"\0") if record):
        raise LedgerError("live Git worktree contains a forbidden submodule/gitlink")
    _, status_raw = _git(
        worktree,
        (
            "status",
            "--porcelain=v1",
            "--untracked-files=all",
            "--ignore-submodules=none",
        ),
        "worktree cleanliness",
    )
    if status_raw:
        raise LedgerError("live Git worktree is dirty")
    for marker in _GIT_OPERATION_MARKERS:
        _, marker_raw = _git(
            worktree,
            ("rev-parse", "--git-path", marker),
            f"Git operation marker {marker}",
        )
        marker_path = _one_git_line(marker_raw, f"Git operation marker {marker}")
        if not os.path.isabs(marker_path):
            marker_path = os.path.normpath(os.path.join(path, marker_path))
        try:
            os.lstat(marker_path)
        except FileNotFoundError:
            pass
        except OSError as error:
            raise LedgerError(f"cannot prove Git operation marker {marker}: {error}") from error
        else:
            raise LedgerError(f"live Git worktree has operation in progress: {marker}")
    _, top_raw = _git(worktree, ("rev-parse", "--show-toplevel"), "worktree root")
    if _one_git_line(top_raw, "worktree root") != path:
        raise LedgerError("live Git worktree root differs from its pinned path")
    primary_path = request["roots"]["primary"]["path"]
    expected_common = _absolute_git_common_directory(
        primary, primary_path, "primary common Git directory"
    )
    common_git_dir = _absolute_git_common_directory(
        worktree, path, "worktree common Git directory"
    )
    if common_git_dir != expected_common:
        raise LedgerError("live worktree common Git directory is foreign")
    primary_common_descriptor = None
    worktree_common_descriptor = None
    try:
        primary_common_descriptor, primary_common_status = (
            _pin_checkout_common_directory(
                primary,
                primary_path,
                expected_common,
                "live primary checkout",
            )
        )
        worktree_common_descriptor, worktree_common_status = (
            _pin_checkout_common_directory(
                worktree,
                path,
                common_git_dir,
                "live worktree checkout",
            )
        )
        _recheck_pinned_directory(
            primary_common_descriptor,
            expected_common,
            "live primary common Git directory",
            (primary_common_status.st_dev, primary_common_status.st_ino),
        )
        _recheck_pinned_directory(
            worktree_common_descriptor,
            common_git_dir,
            "live worktree common Git directory",
            (worktree_common_status.st_dev, worktree_common_status.st_ino),
        )
        if (primary_common_status.st_dev, primary_common_status.st_ino) != (
            worktree_common_status.st_dev,
            worktree_common_status.st_ino,
        ):
            raise LedgerError("live worktree common Git directory is foreign")
    except (LedgerError, OSError) as error:
        if worktree_common_descriptor is not None:
            os.close(worktree_common_descriptor)
        if primary_common_descriptor is not None:
            os.close(primary_common_descriptor)
        raise LedgerError(f"live common Git directory is unsafe: {error}") from error
    try:
        _, list_raw = _git(
            primary,
            ("worktree", "list", "--porcelain", "-z"),
            "Git worktree registration",
        )
        records = _git_worktree_records(list_raw, "Git worktree registration")
        matches = [record for record in records if record.get("worktree") == path]
        primary_matches = [
            record
            for record in records
            if record.get("worktree") == request["roots"]["primary"]["path"]
        ]
        if len(matches) != 1 or len(primary_matches) != 1:
            raise LedgerError("live Git worktree registration is missing or ambiguous")
        match = matches[0]
        branch_matches = [
            record
            for record in records
            if record.get("branch") == f"refs/heads/{request['branch']}"
        ]
        if (
            len(branch_matches) != 1
            or branch_matches[0] is not match
            or set(match) != {"worktree", "HEAD", "branch"}
            or match["HEAD"] != request["expected_head_sha"]
            or match["branch"] != f"refs/heads/{request['branch']}"
        ):
            raise LedgerError("live Git worktree is detached, locked, prunable, or drifted")
        tree = None
        if expected_tree is not None:
            _, tree_raw = _git(
                worktree, ("rev-parse", "--verify", "HEAD^{tree}"), "worktree tree"
            )
            tree = _one_git_line(tree_raw, "worktree tree")
            if tree != expected_tree:
                raise LedgerError("live Git worktree tree differs from the scope result")
        _reprove_dynamic_worktree_state(primary, worktree, path, request)
        _recheck_pinned_directory(
            primary_common_descriptor,
            expected_common,
            "live primary common Git directory",
            (primary_common_status.st_dev, primary_common_status.st_ino),
        )
        _recheck_pinned_directory(
            worktree_common_descriptor,
            common_git_dir,
            "live worktree common Git directory",
            (worktree_common_status.st_dev, worktree_common_status.st_ino),
        )
    finally:
        if worktree_common_descriptor is not None:
            os.close(worktree_common_descriptor)
        if primary_common_descriptor is not None:
            os.close(primary_common_descriptor)
    return {
        "path_device": worktree_status.st_dev,
        "path_inode": worktree_status.st_ino,
        "common_git_dir": common_git_dir,
        "tree": tree,
        "safety": dict(SAFE_ARTIFACT_STATE),
    }


def _reprove_dynamic_worktree_state(
    primary: int,
    worktree: int,
    path: str,
    request: Mapping[str, Any],
) -> None:
    """Repeat mutable Git state checks immediately before proof completion."""

    _, list_raw = _git(
        primary,
        ("worktree", "list", "--porcelain", "-z"),
        "final Git worktree registration",
    )
    records = _git_worktree_records(list_raw, "final Git worktree registration")
    matches = [record for record in records if record.get("worktree") == path]
    primary_matches = [
        record
        for record in records
        if record.get("worktree") == request["roots"]["primary"]["path"]
    ]
    if len(matches) != 1 or len(primary_matches) != 1:
        raise LedgerError("live Git worktree registration is missing or ambiguous")
    match = matches[0]
    branch_matches = [
        record
        for record in records
        if record.get("branch") == f"refs/heads/{request['branch']}"
    ]
    if (
        len(branch_matches) != 1
        or branch_matches[0] is not match
        or set(match) != {"worktree", "HEAD", "branch"}
        or match["HEAD"] != request["expected_head_sha"]
        or match["branch"] != f"refs/heads/{request['branch']}"
    ):
        raise LedgerError("live Git worktree is detached, locked, prunable, or drifted")

    _, head_raw = _git(
        worktree, ("rev-parse", "--verify", "HEAD"), "final worktree HEAD"
    )
    head = _one_git_line(head_raw, "final worktree HEAD")
    _, branch_raw = _git(
        worktree,
        ("symbolic-ref", "--quiet", "HEAD"),
        "final worktree branch",
    )
    branch = _one_git_line(branch_raw, "final worktree branch")
    if head != request["expected_head_sha"] or branch != (
        f"refs/heads/{request['branch']}"
    ):
        raise LedgerError("live Git worktree head/branch differs from the request")
    _, shared_index_raw = _git(
        worktree,
        ("rev-parse", "--shared-index-path"),
        "final worktree shared index",
    )
    if shared_index_raw not in {b"", b"\n"}:
        _one_git_line(shared_index_raw, "final worktree shared index")
        raise LedgerError("live Git worktree split index is unsupported")
    _, index_raw = _git(
        worktree,
        ("ls-files", "--stage", "-z"),
        "final worktree index",
    )
    if any(record.startswith(b"160000 ") for record in index_raw.split(b"\0") if record):
        raise LedgerError("live Git worktree contains a forbidden submodule/gitlink")
    _, status_raw = _git(
        worktree,
        (
            "status",
            "--porcelain=v1",
            "--untracked-files=all",
            "--ignore-submodules=none",
        ),
        "final worktree cleanliness",
    )
    if status_raw:
        raise LedgerError("live Git worktree is dirty")


def _verify_live_observation(
    item: Mapping[str, Any], proof: Mapping[str, Any], context: str
) -> None:
    if (
        (item["path_device"], item["path_inode"])
        != (proof["path_device"], proof["path_inode"])
        or item["safety"] != proof["safety"]
    ):
        raise LedgerError(f"{context} differs from helper-owned live proof")


def _safety_observation(
    value: Any,
    request: Mapping[str, Any],
    path: str,
    list_digest: str,
    producer_kind: str,
    producer_digest: str | None,
    context: str,
    *,
    live: bool,
    expected_tree: str | None = None,
    expected_common_git_dir: str | None = None,
    guard: _LiveWorktreeGuard | None = None,
    allowed_references: Iterable[str] = (),
    scope_record: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
    raw = _retained_result(value, context)
    item = _exact(
        _decode(raw, context),
        {
            "schema_version",
            "observed_at",
            "repository",
            "component",
            "physical_checkout",
            "roots",
            "path",
            "path_device",
            "path_inode",
            "branch",
            "head_sha",
            "worktree_list_sha256",
            "producer",
            "safety",
        },
        context,
    )
    _timestamp_key(item["observed_at"], f"{context}.observed_at")
    producer = _exact(item["producer"], {"kind", "result_sha256"}, f"{context}.producer")
    if item["schema_version"] != 1 or isinstance(item["schema_version"], bool):
        raise LedgerError(f"{context}.schema_version must be 1")
    if (
        item["repository"] != request["repository"]
        or item["component"] != request["component"]
        or item["physical_checkout"] != request["physical_checkout"]
        or item["roots"] != request["roots"]
        or _absolute_path(item["path"], f"{context}.path") != path
        or _integer(item["path_device"], f"{context}.path_device", minimum=0) < 0
        or _integer(item["path_inode"], f"{context}.path_inode", minimum=0) < 0
        or item["branch"] != request["branch"]
        or item["head_sha"] != request["expected_head_sha"]
        or item["worktree_list_sha256"] != list_digest
        or producer != {"kind": producer_kind, "result_sha256": producer_digest}
        or item["safety"] != SAFE_ARTIFACT_STATE
    ):
        raise LedgerError(f"{context} is not exact safe live binding evidence")
    if live:
        if guard is None:
            with _pinned_live_worktree(
                request,
                path,
                context,
                allowed_references=allowed_references,
                scope_record=scope_record,
            ) as opened:
                proof = opened.prove(expected_tree=expected_tree)
        else:
            if guard.request != request or guard.path != path:
                raise LedgerError(f"{context} pinned worktree guard differs from request")
            proof = guard.prove(expected_tree=expected_tree)
        _verify_live_observation(item, proof, context)
        if (
            expected_common_git_dir is not None
            and proof["common_git_dir"] != expected_common_git_dir
        ):
            raise LedgerError(f"{context} common Git directory differs from scope result")
    return item


def _live_observation_document(
    request: Mapping[str, Any],
    path: str,
    list_digest: str,
    producer_kind: str,
    producer_digest: str | None,
    observed_at: str,
    *,
    expected_tree: str | None = None,
    expected_common_git_dir: str | None = None,
    allowed_references: Iterable[str] = (),
    scope_record: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
    _timestamp_key(observed_at, "live observation timestamp")
    with _pinned_live_worktree(
        request,
        path,
        "live observation",
        allowed_references=allowed_references,
        scope_record=scope_record,
    ) as guard:
        proof = guard.prove(expected_tree=expected_tree)
    if (
        expected_common_git_dir is not None
        and proof["common_git_dir"] != expected_common_git_dir
    ):
        raise LedgerError("live observation common Git directory differs from scope result")
    return {
        "schema_version": 1,
        "observed_at": observed_at,
        "repository": _json_object_copy(request["repository"], "live repository"),
        "component": request["component"],
        "physical_checkout": request["physical_checkout"],
        "roots": _json_object_copy(request["roots"], "live roots"),
        "path": path,
        "path_device": proof["path_device"],
        "path_inode": proof["path_inode"],
        "branch": request["branch"],
        "head_sha": request["expected_head_sha"],
        "worktree_list_sha256": list_digest,
        "producer": {"kind": producer_kind, "result_sha256": producer_digest},
        "safety": dict(SAFE_ARTIFACT_STATE),
    }


def observe_primitive_worktree(
    document: Mapping[str, Any],
    slot_id: str,
    worktree_list_raw: bytes,
    observed_at: str,
    create_output_raw: bytes | None = None,
) -> dict[str, Any]:
    """Produce, rather than trust, one primitive live-safety observation."""

    item = validate(document)
    slot_id = _string(slot_id, "slot_id", SLOT_RE)
    slots = [slot for slot in item["artifacts"] if slot["slot_id"] == slot_id]
    if len(slots) != 1 or slots[0]["kind"] != "worktree":
        raise LedgerError("primitive observation requires one exact worktree slot")
    request = slots[0]["primitive_request"]
    if request is None or slots[0]["producer_resource_slot"] is not None:
        raise LedgerError("primitive observation requires a deferred primitive request")
    path, list_digest = _worktree_list_path(
        _retained_result_document(worktree_list_raw, "worktree list output"),
        request,
        "worktree observation list",
    )
    producer_digest = None
    if create_output_raw is not None:
        retained = _retained_result_document(
            create_output_raw, "worktree create output"
        )
        if _worktree_create_path(
            retained, request, "worktree observation create output"
        ) != path:
            raise LedgerError("worktree create output differs from live list path")
        producer_digest = retained["sha256"]
    return _live_observation_document(
        request,
        path,
        list_digest,
        "primitive",
        producer_digest,
        observed_at,
    )


def observe_scope_worktree(
    document: Mapping[str, Any],
    slot_id: str,
    scope_show_raw: bytes,
    worktree_list_raw: bytes,
    observed_at: str,
) -> dict[str, Any]:
    """Produce one scope worktree observation from exact result plus live Git."""

    item = validate(document)
    slot_id = _string(slot_id, "slot_id", SLOT_RE)
    slots = [resource for resource in item["resources"] if resource["slot_id"] == slot_id]
    if len(slots) != 1 or slots[0]["kind"] != "scope":
        raise LedgerError("scope observation requires one exact scope resource slot")
    scope = slots[0]
    request = scope["request"]
    retained = _retained_result_document(scope_show_raw, "scope show output")
    record, row = _scope_show_record(
        scope_show_raw,
        request,
        scope["immutable"]["repository"],
        "scope show output",
    )
    worktree_request = _scope_worktree_request(
        request, scope["immutable"]["repository"]
    )
    path, list_digest = _worktree_list_path(
        _retained_result_document(worktree_list_raw, "scope worktree list output"),
        worktree_request,
        "scope observation list",
    )
    if path != row["path"]:
        raise LedgerError("scope result and live list path differ")
    return _live_observation_document(
        worktree_request,
        path,
        list_digest,
        "scope",
        retained["sha256"],
        observed_at,
        expected_tree=row["tree"],
        expected_common_git_dir=row["common_git_dir"],
        allowed_references=_scope_owned_references(request),
        scope_record=record,
    )


def _primitive_result_path(
    value: Any,
    request: Mapping[str, Any],
    context: str,
    *,
    live: bool,
    guard: _LiveWorktreeGuard | None = None,
) -> str:
    item = _exact(
        value,
        {"create_output", "worktree_list", "safety_observation"},
        context,
    )
    create_digest = None
    if item["create_output"] is not None:
        if _worktree_create_path(
            item["create_output"], request, f"{context}.create_output"
        ) != _expected_worktree_path(request):
            raise LedgerError(f"{context}.create_output differs from request")
        create_digest = item["create_output"]["sha256"]
    path, list_digest = _worktree_list_path(
        item["worktree_list"], request, f"{context}.worktree_list"
    )
    _safety_observation(
        item["safety_observation"],
        request,
        path,
        list_digest,
        "primitive",
        create_digest,
        f"{context}.safety_observation",
        live=live,
        guard=guard,
    )
    return path


def _artifact(value: Any, context: str) -> tuple[str, str]:
    base_keys = {
        "slot_id",
        "kind",
        "state",
        "immutable",
        "current",
        "safety",
        "producer_resource_slot",
    }
    worktree = isinstance(value, dict) and value.get("kind") == "worktree"
    pull_request = isinstance(value, dict) and value.get("kind") == "pull_request"
    item = _exact(
        value,
        base_keys
        | ({"primitive_request", "primitive_result"} if worktree else set())
        | ({"initial_body_payload"} if pull_request else set()),
        context,
    )
    slot = _string(item["slot_id"], f"{context}.slot_id", SLOT_RE)
    kind = item["kind"]
    if kind not in ARTIFACT_KINDS:
        raise LedgerError(f"{context}.kind is invalid")
    state = item["state"]
    if state not in ARTIFACT_STATES:
        raise LedgerError(f"{context}.state is invalid")
    immutable = _identity(item["immutable"], f"{context}.immutable", current=False)
    initial_body_payload = item.get("initial_body_payload")
    if initial_body_payload is not None:
        _inline_payload(
            initial_body_payload, f"{context}.initial_body_payload", utf8=True
        )
    producer = item["producer_resource_slot"]
    if producer is not None:
        producer = _string(producer, f"{context}.producer_resource_slot", SLOT_RE)
    primitive_request = item.get("primitive_request")
    if primitive_request is not None:
        _primitive_worktree_request(
            primitive_request, f"{context}.primitive_request"
        )
    primitive_result = item.get("primitive_result")
    result_path = None
    if primitive_result is not None:
        if primitive_request is None:
            raise LedgerError(f"{context} primitive result requires its immutable request")
        result_path = _primitive_result_path(
            primitive_result,
            primitive_request,
            f"{context}.primitive_result",
            live=False,
        )
    current = None
    if item["current"] is not None:
        current = _identity(item["current"], f"{context}.current", current=True)
    if state == "planned" and current is not None:
        raise LedgerError(f"{context} planned slot must not have current identity")
    if state != "planned" and current is None:
        raise LedgerError(f"{context} bound slot requires current identity")
    safety = item["safety"]
    if state == "planned":
        if safety is not None:
            raise LedgerError(f"{context} planned slot must not claim safety state")
    else:
        safety = _exact(
            safety,
            {
                "clean",
                "detached",
                "locked",
                "active",
                "unowned_reference",
                "foreign",
                "certain",
            },
            f"{context}.safety",
        )
        for field, field_value in safety.items():
            if not isinstance(field_value, bool):
                raise LedgerError(f"{context}.safety.{field} must be a boolean")
    _, _, _, branch, path, number, node, body_digest, _ = immutable
    if kind == "branch":
        if branch is None or any(
            value is not None for value in (path, number, node, body_digest)
        ):
            raise LedgerError(f"{context} branch immutable identity is invalid")
    elif kind == "worktree":
        if (
            branch is None
            or (path is None and (producer is None) == (primitive_request is None))
            or number is not None
            or node is not None
            or body_digest is not None
        ):
            raise LedgerError(f"{context} worktree immutable identity is invalid")
        if path is not None and (producer is not None or primitive_request is not None):
            raise LedgerError(
                f"{context} known-path worktree cannot have deferred production intent"
            )
        if primitive_request is not None and (
            primitive_request["repository"] != item["immutable"]["repository"]
            or primitive_request["branch"] != branch
        ):
            raise LedgerError(
                f"{context} primitive request differs from immutable repository/branch"
            )
    else:
        if branch is None or path is not None or body_digest is None:
            raise LedgerError(f"{context} pull-request immutable identity is invalid")
        if (number is None) != (node is None):
            raise LedgerError(f"{context} pull-request number/node must appear together")
        if state in {"planned", "created"} and initial_body_payload is None:
            raise LedgerError(
                f"{context} planned/created PR requires its exact initial body payload"
            )
        if (
            initial_body_payload is not None
            and initial_body_payload["sha256"] != body_digest
        ):
            raise LedgerError(
                f"{context} initial body payload differs from immutable body digest"
            )
        if initial_body_payload is not None and BODY_NAMESPACE in _inline_payload(
            initial_body_payload, f"{context}.initial_body_payload", utf8=True
        ):
            raise LedgerError(
                f"{context} initial PR body contains the reserved delivery namespace"
            )
    if current is not None:
        if current[:3] != immutable[:3]:
            raise LedgerError(f"{context} repository identity changed")
        (
            _,
            _,
            _,
            current_branch,
            current_path,
            current_number,
            current_node,
            current_body_digest,
            head_sha,
        ) = current
        if current_branch != branch or (
            current_path != path
            and not (
                kind == "worktree"
                and path is None
                and (producer is not None or primitive_request is not None)
            )
        ):
            raise LedgerError(f"{context} branch/path identity changed")
        if kind != "pull_request" and any(
            value is not None
            for value in (current_number, current_node, current_body_digest)
        ):
            raise LedgerError(f"{context} current identity contains invalid PR fields")
        if kind == "pull_request" and (
            current_number is None
            or current_node is None
            or current_body_digest is None
            or head_sha is None
        ):
            raise LedgerError(f"{context} bound PR identity is incomplete")
        if kind == "pull_request" and number is not None and (
            current_number != number or current_node != node
        ):
            raise LedgerError(f"{context} planned/current PR identity does not match")
        if kind != "pull_request" and head_sha is None:
            raise LedgerError(f"{context} bound identity requires head_sha")
        if kind == "worktree" and current_path is None:
            raise LedgerError(f"{context} bound worktree requires a canonical path")
        if kind == "worktree" and primitive_request is not None:
            if result_path is None or current_path != result_path:
                raise LedgerError(
                    f"{context} bound primitive worktree path lacks exact wrapper result"
                )
    if kind != "worktree" and producer is not None:
        raise LedgerError(f"{context} only a worktree may have a producer resource")
    if kind == "worktree" and path is not None and producer is not None:
        raise LedgerError(f"{context} primitive worktree path cannot have a producer")
    if kind == "worktree" and primitive_request is None and primitive_result is not None:
        raise LedgerError(f"{context} primitive result has no request")
    if kind == "worktree" and state == "planned" and primitive_result is not None:
        raise LedgerError(f"{context} planned worktree cannot pre-record a result")
    return (slot,)


def _source(value: Any, context: str) -> tuple[str, str, int, int]:
    item = _exact(value, {"name", "sha256", "device", "inode"}, context)
    name = _direct_name(item["name"], f"{context}.name")
    digest = _string(item["sha256"], f"{context}.sha256", SHA256_RE)
    device = _integer(item["device"], f"{context}.device", minimum=0)
    inode = _integer(item["inode"], f"{context}.inode")
    return name, digest, device, inode


def _sorted_node_ids(value: Any, context: str, *, allow_empty: bool = True) -> tuple[str, ...]:
    if not isinstance(value, list) or (not allow_empty and not value):
        raise LedgerError(f"{context} must be a deterministically sorted array")
    nodes = tuple(
        _string(node, f"{context}[{index}]", NODE_RE)
        for index, node in enumerate(value)
    )
    if len(set(nodes)) != len(nodes) or list(nodes) != sorted(nodes):
        raise LedgerError(f"{context} must contain unique sorted node IDs")
    return nodes


def _authority(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "kind",
            "reference",
            "objective_sha256",
            "issued_at",
            "actor_node_id",
            "allowed",
        },
        context,
    )
    if item["kind"] not in {
        "durable-goal",
        "explicit-invocation",
        "explicit-recovery",
    }:
        raise LedgerError(f"{context}.kind is invalid")
    reference = _string(item["reference"], f"{context}.reference", REFERENCE_RE)
    objective = _string(
        item["objective_sha256"], f"{context}.objective_sha256", SHA256_RE
    )
    issued = _optional_timestamp(item["issued_at"], f"{context}.issued_at")
    if issued is None:
        raise LedgerError(f"{context}.issued_at is required")
    actor = _string(item["actor_node_id"], f"{context}.actor_node_id", NODE_RE)
    allowed = _exact(
        item["allowed"], {"repositories", "issues", "pull_requests"}, f"{context}.allowed"
    )
    repositories = _sorted_node_ids(
        allowed["repositories"], f"{context}.allowed.repositories", allow_empty=False
    )
    issues = _sorted_node_ids(allowed["issues"], f"{context}.allowed.issues")
    pulls = _sorted_node_ids(
        allowed["pull_requests"], f"{context}.allowed.pull_requests"
    )
    return item["kind"], reference, objective, issued, actor, repositories, issues, pulls


def _program(value: Any, context: str) -> tuple[Any, ...] | None:
    if value is None:
        return None
    item = _exact(
        value, {"master_issue", "leaf_issue", "leaf_position"}, context
    )
    master = _issue(item["master_issue"], f"{context}.master_issue")
    leaf = _issue(item["leaf_issue"], f"{context}.leaf_issue")
    position = _integer(item["leaf_position"], f"{context}.leaf_position", minimum=0)
    return master, leaf, position


def _resource_identity(
    value: Any, context: str, *, current: bool, scope: bool = False
) -> tuple[Any, ...]:
    keys = {"repository", "name", "path"}
    if current:
        keys |= {
            "generation",
            "external_generation",
            "identity_digest",
            "history",
            "lifecycle",
        }
        if scope:
            keys |= {"binding", "observation"}
    item = _exact(value, keys, context)
    repository_identity = _repository(item["repository"], f"{context}.repository")
    name = _string(item["name"], f"{context}.name", REFERENCE_RE)
    path = item["path"]
    if path is not None:
        path = _absolute_path(path, f"{context}.path")
    generation = external_generation = digest = lifecycle = None
    history: tuple[str, ...] | None = None
    if current:
        generation = _integer(item["generation"], f"{context}.generation")
        if item["external_generation"] is not None:
            external_generation = _string(
                item["external_generation"],
                f"{context}.external_generation",
                EXTERNAL_GENERATION_RE,
            )
        digest = _string(item["identity_digest"], f"{context}.identity_digest", SHA256_RE)
        if not isinstance(item["history"], list):
            raise LedgerError(f"{context}.history must be an array")
        history = tuple(
            _string(value, f"{context}.history[{index}]", SHA256_RE)
            for index, value in enumerate(item["history"])
        )
        if len(history) != generation - 1:
            raise LedgerError(f"{context}.history length must equal generation minus one")
        lifecycle = item["lifecycle"]
        if lifecycle not in RESOURCE_LIFECYCLES:
            raise LedgerError(f"{context}.lifecycle is invalid")
    return (
        *repository_identity,
        name,
        path,
        generation,
        external_generation,
        digest,
        history,
        lifecycle,
    )


def _temporary_state_policy(value: Any, context: str) -> dict[str, Any]:
    item = _exact(value, {"mode", "name", "ownership", "lifecycle"}, context)
    expected = {
        "mode": "temporary",
        "name": None,
        "ownership": "topology-generation",
        "lifecycle": "remove-on-clean-stop",
    }
    if item != expected:
        raise LedgerError(f"{context} must be the exact temporary state policy")
    return item


def _scope_request(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "name",
            "component",
            "profile",
            "physical_checkout",
            "label",
            "branch",
            "start_sha",
            "temporary_state",
            "state_policy",
            "topology",
            "roots",
        },
        context,
    )
    name = _string(item["name"], f"{context}.name", SLOT_RE)
    component = _string(item["component"], f"{context}.component", SLOT_RE)
    profile = _string(item["profile"], f"{context}.profile", SLOT_RE)
    checkout = _string(
        item["physical_checkout"], f"{context}.physical_checkout", SLOT_RE
    )
    label = _string(item["label"], f"{context}.label", SLOT_RE)
    branch = _branch(item["branch"], f"{context}.branch")
    start = _string(item["start_sha"], f"{context}.start_sha", COMMIT_RE)
    if item["temporary_state"] is not True:
        raise LedgerError(f"{context}.temporary_state must be true")
    _temporary_state_policy(item["state_policy"], f"{context}.state_policy")
    topology = _string(item["topology"], f"{context}.topology", SLOT_RE)
    roots = _request_roots(item["roots"], checkout, f"{context}.roots")
    return name, component, profile, checkout, label, branch, start, topology, roots


def _sorted_names(value: Any, context: str, *, nonempty: bool = True) -> list[str]:
    if not isinstance(value, list) or (nonempty and not value):
        raise LedgerError(f"{context} must be a sorted name array")
    names = [
        _string(name, f"{context}[{index}]", SLOT_RE)
        for index, name in enumerate(value)
    ]
    if names != sorted(set(names)):
        raise LedgerError(f"{context} must contain unique sorted names")
    return names


def _scope_show_record(
    raw: bytes,
    request: Mapping[str, Any],
    repository: Mapping[str, Any],
    context: str,
) -> tuple[dict[str, Any], dict[str, Any]]:
    record = _exact(
        _decode(raw, context),
        {
            "schema_version",
            "status",
            "name",
            "generation",
            "request_sha256",
            "created_at",
            "base_profile",
            "stack",
            "requested_components",
            "worktrees",
            "profile",
            "topology",
            "state_policy",
            "commands",
            "cleanup",
        },
        context,
    )
    if type(record["schema_version"]) is not int or record["schema_version"] != 1:
        raise LedgerError(f"{context}.schema_version must be 1")
    if record["status"] != "complete" or record["name"] != request["name"]:
        raise LedgerError(f"{context} is not the exact complete requested scope")
    generation = _string(
        record["generation"], f"{context}.generation", EXTERNAL_GENERATION_RE
    )
    _string(record["request_sha256"], f"{context}.request_sha256", SHA256_RE)
    _timestamp_key(record["created_at"], f"{context}.created_at")
    if record["base_profile"] != request["profile"]:
        raise LedgerError(f"{context} base profile differs from scope request")
    stack = _string(record["stack"], f"{context}.stack", SLOT_RE)
    requested = _sorted_names(
        record["requested_components"], f"{context}.requested_components"
    )
    if requested != [request["component"]]:
        raise LedgerError(f"{context} requested components differ from scope request")
    if not isinstance(record["worktrees"], list) or len(record["worktrees"]) != 1:
        raise LedgerError(f"{context} must contain exactly one worktree row")
    row = _exact(
        record["worktrees"][0],
        {
            "checkout",
            "repository",
            "logical_components",
            "label",
            "branch",
            "start_point",
            "commit",
            "tree",
            "path",
            "primary_path",
            "common_git_dir",
            "path_device",
            "path_inode",
            "created_by_scope",
        },
        f"{context}.worktrees[0]",
    )
    checkout = _string(row["checkout"], f"{context}.worktrees[0].checkout", SLOT_RE)
    expected_repository = f"{repository['owner']}/{repository['name']}"
    logical_components = _sorted_names(
        row["logical_components"], f"{context}.worktrees[0].logical_components"
    )
    if (
        checkout != request["physical_checkout"]
        or row["repository"] != expected_repository
        or (
            request["component"] != checkout
            and request["component"] not in logical_components
        )
        or row["label"] != request["label"]
        or row["branch"] != request["branch"]
        or row["start_point"] != request["start_sha"]
        or row["commit"] != request["start_sha"]
        or row["created_by_scope"] is not True
    ):
        raise LedgerError(f"{context} worktree row differs from exact scope request")
    _branch(row["branch"], f"{context}.worktrees[0].branch")
    _string(row["commit"], f"{context}.worktrees[0].commit", COMMIT_RE)
    _string(row["tree"], f"{context}.worktrees[0].tree", COMMIT_RE)
    workspace_root = request["roots"]["workspace"]["path"]
    path = _managed_worktree_result_path(
        row["path"],
        workspace_root,
        checkout,
        request["label"],
        f"{context}.worktrees[0].path",
    )
    expected_primary = request["roots"]["primary"]["path"]
    if _absolute_path(row["primary_path"], f"{context}.worktrees[0].primary_path") != str(
        expected_primary
    ):
        raise LedgerError(f"{context} primary checkout differs from requested checkout")
    _absolute_path(row["common_git_dir"], f"{context}.worktrees[0].common_git_dir")
    for field in ("path_device", "path_inode"):
        _integer(row[field], f"{context}.worktrees[0].{field}", minimum=0)

    profile = _exact(
        record["profile"],
        {"name", "path", "sha256", "path_device", "path_inode", "immutable"},
        f"{context}.profile",
    )
    expected_profile_name = f"scope-{request['name']}"
    expected_profile_path = (
        Path(workspace_root) / "profiles" / f"{expected_profile_name}.json"
    )
    if (
        profile["name"] != expected_profile_name
        or _absolute_path(profile["path"], f"{context}.profile.path")
        != str(expected_profile_path)
        or profile["immutable"] is not True
    ):
        raise LedgerError(f"{context} profile differs from exact scope request")
    _string(profile["sha256"], f"{context}.profile.sha256", SHA256_RE)
    for field in ("path_device", "path_inode"):
        _integer(profile[field], f"{context}.profile.{field}", minimum=0)

    topology = _exact(record["topology"], {"name", "path"}, f"{context}.topology")
    expected_topology_path = Path(workspace_root) / "topologies" / request["topology"]
    if (
        topology["name"] != request["topology"]
        or _absolute_path(topology["path"], f"{context}.topology.path")
        != str(expected_topology_path)
    ):
        raise LedgerError(f"{context} topology differs from exact scope request")
    if record["state_policy"] != request["state_policy"]:
        raise LedgerError(f"{context} state policy differs from exact scope request")
    _temporary_state_policy(record["state_policy"], f"{context}.state_policy")

    commands = _exact(
        record["commands"],
        {
            "paths",
            "builds",
            "topology_show",
            "up",
            "ps",
            "logs",
            "down",
            "release_preview",
            "release_apply",
        },
        f"{context}.commands",
    )
    for mapping_name in ("paths", "builds"):
        mapping = commands[mapping_name]
        if not isinstance(mapping, dict) or not all(
            isinstance(key, str)
            and SLOT_RE.fullmatch(key)
            and isinstance(value, str)
            and value
            == (
                f"./atrinik path {key} --profile {expected_profile_name}"
                if mapping_name == "paths"
                else f"./atrinik build {key} --profile {expected_profile_name} --test"
            )
            for key, value in mapping.items()
        ):
            raise LedgerError(f"{context}.commands.{mapping_name} is invalid")
    if not set(logical_components).issubset(commands["paths"]):
        raise LedgerError(f"{context} commands omit a selected component path")
    logs = _exact(commands["logs"], {"client", "server"}, f"{context}.commands.logs")
    expected_commands = {
        "topology_show": f"./atrinik topology show {expected_profile_name} --temporary-state --json",
        "up": f"./atrinik up --name {request['topology']} --profile {expected_profile_name} --temporary-state --json",
        "ps": f"./atrinik ps {request['topology']} --json",
        "down": f"./atrinik down {request['topology']} --json",
        "release_preview": f"./atrinik scope release {request['name']} --dry-run --json",
        "release_apply": f"./atrinik scope release {request['name']} --apply --plan PLAN_SHA256 --json",
    }
    if any(commands[key] != value for key, value in expected_commands.items()):
        raise LedgerError(f"{context} commands do not match exact coordinates")
    expected_logs = {
        service: f"./atrinik logs {request['topology']} {service} --tail 100"
        for service in ("server", "client")
    }
    if logs != expected_logs:
        raise LedgerError(f"{context}.commands.logs do not match exact coordinates")

    cleanup = _exact(
        record["cleanup"], {"policy", "journal", "release_journal"}, f"{context}.cleanup"
    )
    scope_root = Path(workspace_root) / "scopes" / request["name"]
    if cleanup != {
        "policy": "explicit-preview-first",
        "journal": str(scope_root / "creation-journal.json"),
        "release_journal": str(scope_root / "release-journal.json"),
    }:
        raise LedgerError(f"{context} cleanup coordinates are invalid")

    request_document = {
        "name": record["name"],
        "base_profile": record["base_profile"],
        "stack": stack,
        "requested_components": requested,
        "profile": {"name": profile["name"], "path": profile["path"]},
        "topology": topology,
        "state_policy": record["state_policy"],
        "worktrees": [
            {
                key: row[key]
                for key in (
                    "checkout",
                    "repository",
                    "logical_components",
                    "label",
                    "branch",
                    "start_point",
                    "commit",
                    "tree",
                    "path",
                    "primary_path",
                )
            }
        ],
    }
    if canonical_object_digest(request_document) != record["request_sha256"]:
        raise LedgerError(f"{context} request digest does not match exact coordinates")
    # Keep the type checker honest and make the externally anchored value explicit.
    record["generation"] = generation
    return record, row


def _scope_worktree_request(
    request: Mapping[str, Any], repository: Mapping[str, Any]
) -> dict[str, Any]:
    return {
        "component": request["component"],
        "physical_checkout": request["physical_checkout"],
        "label": request["label"],
        "repository": repository,
        "branch": request["branch"],
        "expected_head_sha": request["start_sha"],
        "roots": request["roots"],
    }


def _scope_owned_references(request: Mapping[str, Any]) -> frozenset[str]:
    return frozenset(
        {
            f"scope:{request['name']}",
            f"profile:scope-{request['name']}",
        }
    )


def _scope_binding_observation(
    value: Any,
    request: Mapping[str, Any],
    repository: Mapping[str, Any],
    row: Mapping[str, Any],
    scope_digest: str,
    scope_record: Mapping[str, Any],
    context: str,
    *,
    live: bool,
    guard: _LiveWorktreeGuard | None = None,
) -> None:
    item = _exact(value, {"worktree_list", "safety_observation"}, context)
    worktree_request = _scope_worktree_request(request, repository)
    path, list_digest = _worktree_list_path(
        item["worktree_list"], worktree_request, f"{context}.worktree_list"
    )
    observation = _safety_observation(
        item["safety_observation"],
        worktree_request,
        path,
        list_digest,
        "scope",
        scope_digest,
        f"{context}.safety_observation",
        live=live,
        expected_tree=row["tree"],
        expected_common_git_dir=row["common_git_dir"],
        guard=guard,
        allowed_references=_scope_owned_references(request),
        scope_record=scope_record,
    )
    if (observation["path_device"], observation["path_inode"]) != (
        row["path_device"],
        row["path_inode"],
    ):
        raise LedgerError(f"{context} path identity differs from scope result")


def _resource(value: Any, context: str) -> tuple[str, str]:
    scope = isinstance(value, dict) and value.get("kind") == "scope"
    item = _exact(
        value,
        {"slot_id", "kind", "state", "immutable", "current"}
        | ({"request"} if scope else set()),
        context,
    )
    slot = _string(item["slot_id"], f"{context}.slot_id", SLOT_RE)
    if item["kind"] not in RESOURCE_KINDS:
        raise LedgerError(f"{context}.kind is invalid")
    if item["state"] not in ARTIFACT_STATES:
        raise LedgerError(f"{context}.state is invalid")
    immutable = _resource_identity(
        item["immutable"], f"{context}.immutable", current=False, scope=scope
    )
    request = item.get("request")
    if scope:
        _scope_request(request, f"{context}.request")
        if (
            request["name"] != item["immutable"]["name"]
            or item["immutable"]["path"] is not None
        ):
            raise LedgerError(
                f"{context} scope request/name/path identity is inconsistent"
            )
    current = None
    if item["current"] is not None:
        current = _resource_identity(
            item["current"], f"{context}.current", current=True, scope=scope
        )
    if item["state"] == "planned" and current is not None:
        raise LedgerError(f"{context} planned resource must not have current identity")
    if item["state"] != "planned" and current is None:
        raise LedgerError(f"{context} bound resource requires current identity")
    if current is not None and current[:5] != immutable[:5]:
        raise LedgerError(f"{context} immutable resource identity changed")
    if current is not None:
        lifecycle = current[-1]
        if item["kind"] in {"topology", "runtime"} and lifecycle not in {
            "stopped",
            "running",
        }:
            raise LedgerError(f"{context} runtime lifecycle is invalid")
        if item["kind"] in {"profile", "reference", "build"} and lifecycle != "static":
            raise LedgerError(f"{context} static resource lifecycle is invalid")
        if item["kind"] == "scope" and lifecycle not in {"active", "released"}:
            raise LedgerError(f"{context} scope lifecycle is invalid")
        if item["kind"] == "scope" and current[6] is None:
            raise LedgerError(f"{context} scope external_generation is required")
        if item["kind"] != "scope" and current[6] is not None:
            raise LedgerError(f"{context} external_generation is scope-only")
        if item["kind"] in {"state", "scenario"} and lifecycle not in {
            "active",
            "consumed",
            "ready",
            "released",
            "stopped",
        }:
            raise LedgerError(f"{context} handoff lifecycle is invalid")
        if item["kind"] == "scope":
            raw = _retained_result(item["current"]["binding"], f"{context}.current.binding")
            record, row = _scope_show_record(
                raw,
                request,
                item["immutable"]["repository"],
                f"{context}.current.binding.scope_show",
            )
            raw_digest = byte_digest(raw)
            _scope_binding_observation(
                item["current"]["observation"],
                request,
                item["immutable"]["repository"],
                row,
                raw_digest,
                record,
                f"{context}.current.observation",
                live=False,
            )
            if item["current"]["external_generation"] != record["generation"]:
                raise LedgerError(
                    f"{context} scope external generation differs from retained result"
                )
            # Initial/active binding is always the exact retained scope-show bytes.
            # A later terminal release may carry a new observation digest, but its
            # history must retain this exact first binding anchor.
            if lifecycle == "active" or item["current"]["generation"] == 1:
                if item["current"]["identity_digest"] != raw_digest:
                    raise LedgerError(
                        f"{context} scope identity digest differs from retained result"
                    )
            elif not item["current"]["history"] or item["current"]["history"][0] != raw_digest:
                raise LedgerError(
                    f"{context} released scope history lost retained binding digest"
                )
    return (slot,)


def validate(document: Any) -> dict[str, Any]:
    item = _exact(
        document,
        {
            "schema_version",
            "ledger_id",
            "entry_mode",
            "actor",
            "authority",
            "program",
            "issues",
            "selected_prs",
            "targets",
            "closing_scope",
            "artifacts",
            "resources",
            "generation",
            "previous_byte_digest",
            "history",
            "migration",
        },
        "ledger",
    )
    if (
        isinstance(item["schema_version"], bool)
        or not isinstance(item["schema_version"], int)
        or item["schema_version"] != SCHEMA_VERSION
    ):
        raise LedgerError("ledger.schema_version is unsupported")
    mode = item["entry_mode"]
    if mode not in ENTRY_MODES:
        raise LedgerError("ledger.entry_mode must be issue or pr")
    actor = _exact(
        item["actor"], {"login", "node_id", "push_repository_node_ids"}, "ledger.actor"
    )
    login = _string(actor["login"], "ledger.actor.login", LOGIN_RE)
    if login != login.casefold():
        raise LedgerError("ledger.actor.login must be normalized lowercase")
    actor_node = _string(actor["node_id"], "ledger.actor.node_id", NODE_RE)
    push_repositories = _sorted_node_ids(
        actor["push_repository_node_ids"],
        "ledger.actor.push_repository_node_ids",
        allow_empty=False,
    )
    authority = _authority(item["authority"], "ledger.authority")
    if authority[4] != actor_node:
        raise LedgerError("ledger authority actor does not match authenticated actor")
    program = _program(item["program"], "ledger.program")
    issues = _exact(item["issues"], {"explicit", "incidental"}, "ledger.issues")
    explicit = _ordered_unique(issues["explicit"], "ledger.issues.explicit", _issue)
    incidental = _ordered_unique(issues["incidental"], "ledger.issues.incidental", _issue)
    issue_keys = {(a, b, c, d) for a, b, c, d, _ in explicit}
    incidental_keys = {(a, b, c, d) for a, b, c, d, _ in incidental}
    if issue_keys & incidental_keys:
        raise LedgerError("explicit and incidental issue mappings overlap")
    selected = _ordered_unique(item["selected_prs"], "ledger.selected_prs", _pull_request)
    if mode == "issue" and len(explicit) != 1:
        raise LedgerError("issue mode requires exactly one explicit issue")
    if mode == "pr" and len(explicit) > 1:
        raise LedgerError("PR mode permits at most one explicit issue")
    if mode == "pr" and len(selected) != 1:
        raise LedgerError("PR mode requires exactly one selected PR")
    targets = _ordered_unique(item["targets"], "ledger.targets", _target)
    if not targets:
        raise LedgerError("ledger.targets must not be empty")
    if mode == "pr" and len(targets) != 1:
        raise LedgerError("PR mode requires exactly one target")
    target_owners: set[tuple[str, str, str]] = set()
    for target in item["targets"]:
        repository_identity = _repository(target["repository"], "ledger target repository")
        ownership = (
            repository_identity[0].casefold(),
            repository_identity[1].casefold(),
            target["head"]["branch"].casefold(),
        )
        if ownership in target_owners:
            raise LedgerError("ledger targets contain a repository/head case alias")
        target_owners.add(ownership)
    closing = _ordered_unique(item["closing_scope"], "ledger.closing_scope", _issue)
    known_issues = issue_keys | incidental_keys
    closing_keys = {(a, b, c, d) for a, b, c, d, _ in closing}
    if closing_keys - known_issues:
        raise LedgerError("closing_scope contains an unmapped issue")
    if mode == "issue" and closing_keys != issue_keys:
        raise LedgerError("issue-mode closing_scope must be exactly the explicit issue")
    artifacts = _ordered_unique(item["artifacts"], "ledger.artifacts", _artifact)
    if not artifacts:
        raise LedgerError("ledger.artifacts must not be empty")
    target_repositories = {(row[0], row[1], row[2]) for row in targets}
    target_repository_nodes = {row[2] for row in targets}
    if set(push_repositories) != target_repository_nodes:
        raise LedgerError("actor push authority allowlist must exactly match target repositories")
    if set(authority[5]) != target_repository_nodes:
        raise LedgerError("delivery repository authority must exactly match targets")
    selected_issue_nodes = {row[3] for row in explicit}
    selected_pr_nodes = {row[3] for row in selected}
    reserved_pr_nodes = {
        slot["immutable"]["node_id"]
        for slot in item["artifacts"]
        if slot["kind"] == "pull_request"
        and slot["immutable"]["node_id"] is not None
    }
    expected_pr_authority = selected_pr_nodes | reserved_pr_nodes
    unauthorized_pr_nodes = selected_pr_nodes - set(authority[7])
    if mode == "issue":
        issue_created_exception = bool(unauthorized_pr_nodes)
        if unauthorized_pr_nodes and (
            authority[7]
            or _integer(item["generation"], "ledger.generation") < 2
        ):
            raise LedgerError(
                "issue-created PR exception requires generation 2+ and an empty PR allowlist"
            )
        for pull in item["selected_prs"]:
            if pull["node_id"] not in unauthorized_pr_nodes:
                continue
            created_slots = [
                slot
                for slot in item["artifacts"]
                if slot["kind"] == "pull_request"
                and slot["state"] == "created"
                and slot["current"] is not None
                and slot["current"]["node_id"] == pull["node_id"]
                and slot["current"]["number"] == pull["number"]
                and slot["current"]["repository"] == pull["repository"]
                and slot["current"]["branch"] == pull["head_branch"]
                and slot["current"]["body_digest"] == pull["body"]["current_digest"]
                and slot["immutable"]["repository"] == pull["repository"]
                and slot["immutable"]["branch"] == pull["head_branch"]
            ]
            body = pull["body"]
            if (
                len(created_slots) != 1
                or pull["author_node_id"] != actor_node
                or pull["head_repository"] != pull["repository"]
                or body["ownership"] != "delivery-created"
            ):
                raise LedgerError(
                    "issue-mode authority requires exact actor-created PR provenance"
                )
        unauthorized_pr_nodes = set()
        if not issue_created_exception and set(authority[7]) != expected_pr_authority:
            raise LedgerError(
                "issue-mode PR authority must exactly match selected/reserved PRs"
            )
    elif set(authority[7]) != expected_pr_authority:
        raise LedgerError("PR authority must exactly match selected/reserved PRs")
    if unauthorized_pr_nodes:
        raise LedgerError("delivery authority omits a selected PR")
    if program is not None:
        master, leaf, _ = program
        if master[:4] == leaf[:4] or master[3] == leaf[3]:
            raise LedgerError("program master and leaf issues must be distinct")
        if mode != "issue" or not explicit or leaf != explicit[0]:
            raise LedgerError("program leaf must be the explicit issue-mode coordinate")
        if {master[3], leaf[3]} - set(authority[6]):
            raise LedgerError("delivery authority omits a program master or leaf issue")
    expected_issue_authority = set(selected_issue_nodes)
    if program is not None:
        expected_issue_authority.update({program[0][3], program[1][3]})
    if set(authority[6]) != expected_issue_authority:
        raise LedgerError("delivery issue authority must exactly match explicit/program issues")
    for index, artifact in enumerate(item["artifacts"]):
        repository = _repository(
            artifact["immutable"]["repository"],
            f"ledger.artifacts[{index}].immutable.repository",
        )
        if repository not in target_repositories:
            raise LedgerError("artifact repository is not present in targets")
        if artifact["state"] != "planned":
            current_identity = artifact["current"]
            matching_targets = [
                target
                for target in item["targets"]
                if target["repository"]["node_id"] == repository[2]
                and target["head"]["branch"] == current_identity["branch"]
            ]
            if len(matching_targets) != 1:
                raise LedgerError("bound artifact does not match exactly one target")
            if (
                current_identity["head_sha"]
                != matching_targets[0]["head"]["current_sha"]
            ):
                raise LedgerError("bound artifact head does not equal target current head")
    for target in item["targets"]:
        matches = [
            slot
            for slot in item["artifacts"]
            if slot["immutable"]["repository"]["node_id"]
            == target["repository"]["node_id"]
            and slot["immutable"]["branch"] == target["head"]["branch"]
        ]
        for kind in sorted(ARTIFACT_KINDS):
            if sum(slot["kind"] == kind for slot in matches) != 1:
                raise LedgerError(
                    f"target requires exactly one matching {kind} artifact slot"
                )
        if len(matches) != len(ARTIFACT_KINDS):
            raise LedgerError("target contains duplicate artifact coordinates")
    artifact_coordinates: set[tuple[Any, ...]] = set()
    for artifact_item in item["artifacts"]:
        identity = artifact_item["immutable"]
        coordinate = (
            artifact_item["kind"],
            identity["repository"]["node_id"],
            identity["branch"].casefold(),
            None if identity["path"] is None else identity["path"].casefold(),
            identity["number"],
        )
        if coordinate in artifact_coordinates:
            raise LedgerError("artifacts contain a duplicate coordinate")
        artifact_coordinates.add(coordinate)
    worktree_paths: set[str] = set()
    deferred_worktrees: set[tuple[str, str, str]] = set()
    for artifact_item in item["artifacts"]:
        if artifact_item["kind"] == "worktree":
            path_value = artifact_item["immutable"]["path"]
            if path_value is not None:
                folded_path = path_value.casefold()
                if folded_path in worktree_paths:
                    raise LedgerError("worktree artifact paths contain a case alias")
                worktree_paths.add(folded_path)
                managed_coordinate = _managed_worktree_coordinate(path_value)
                if managed_coordinate is not None:
                    if managed_coordinate in deferred_worktrees:
                        raise LedgerError("managed/deferred worktree request overlaps")
                    deferred_worktrees.add(managed_coordinate)
            request = artifact_item["primitive_request"]
            if request is not None:
                coordinate = (
                    request["roots"]["workspace"]["path"].casefold(),
                    request["physical_checkout"].casefold(),
                    request["label"].casefold(),
                )
                if coordinate in deferred_worktrees:
                    raise LedgerError("deferred worktree requests contain a case alias")
                deferred_worktrees.add(coordinate)
                matching_targets = [
                    target
                    for target in item["targets"]
                    if target["repository"] == request["repository"]
                    and target["head"]["branch"] == request["branch"]
                ]
                if (
                    len(matching_targets) != 1
                    or matching_targets[0]["head"]["initial_sha"]
                    != request["expected_head_sha"]
                ):
                    raise LedgerError(
                        "deferred primitive request differs from target initial head"
                    )
                branch_slots = [
                    slot
                    for slot in item["artifacts"]
                    if slot["kind"] == "branch"
                    and slot["immutable"]["repository"] == request["repository"]
                    and slot["immutable"]["branch"] == request["branch"]
                ]
                if len(branch_slots) != 1:
                    raise LedgerError(
                        "deferred primitive request lacks one exact branch slot"
                    )
                branch_slot = branch_slots[0]
                if artifact_item["state"] == "planned":
                    if artifact_item["primitive_result"] is not None:
                        raise LedgerError("planned primitive worktree has a result")
                elif (
                    artifact_item["state"] != "created"
                    or branch_slot["state"] == "planned"
                ):
                    raise LedgerError(
                        "deferred primitive worktree must bind as created after its branch"
                    )
    resources = _ordered_unique(item["resources"], "ledger.resources", _resource)
    resource_names: set[str] = set()
    resource_paths: set[str] = set()
    for index, resource in enumerate(item["resources"]):
        repository = _repository(
            resource["immutable"]["repository"],
            f"ledger.resources[{index}].immutable.repository",
        )
        if repository not in target_repositories:
            raise LedgerError("resource repository is not present in targets")
        identity = resource["immutable"]
        name_coordinate = identity["name"].casefold()
        if name_coordinate in resource_names:
            raise LedgerError("resources contain a duplicate singleton name")
        resource_names.add(name_coordinate)
        if identity["path"] is not None:
            path_coordinate = identity["path"].casefold()
            if path_coordinate in resource_paths:
                raise LedgerError("resources contain a duplicate path coordinate")
            resource_paths.add(path_coordinate)
    resources_by_slot = {resource["slot_id"]: resource for resource in item["resources"]}
    scope_resources = [
        resource for resource in item["resources"] if resource["kind"] == "scope"
    ]
    if scope_resources and mode != "issue":
        raise LedgerError("scope resources are supported only in issue mode")
    scope_repositories: set[str] = set()
    for resource in scope_resources:
        repository_node = resource["immutable"]["repository"]["node_id"]
        if repository_node in scope_repositories:
            raise LedgerError("at most one scope resource is permitted per repository")
        scope_repositories.add(repository_node)
    for artifact_item in item["artifacts"]:
        producer = artifact_item["producer_resource_slot"]
        if producer is None:
            continue
        resource = resources_by_slot.get(producer)
        if resource is None or resource["kind"] != "scope":
            raise LedgerError("worktree producer must be one exact scope resource slot")
        if (
            resource["immutable"]["repository"]["node_id"]
            != artifact_item["immutable"]["repository"]["node_id"]
        ):
            raise LedgerError("scope-produced worktree repository does not match")
    for scope in scope_resources:
        scope_request_coordinate = (
            scope["request"]["roots"]["workspace"]["path"].casefold(),
            scope["request"]["physical_checkout"].casefold(),
            scope["request"]["label"].casefold(),
        )
        if scope_request_coordinate in deferred_worktrees:
            raise LedgerError("scope/primitive deferred worktree request overlaps")
        deferred_worktrees.add(scope_request_coordinate)
        produced = [
            artifact_item
            for artifact_item in item["artifacts"]
            if artifact_item["kind"] == "worktree"
            and artifact_item["producer_resource_slot"] == scope["slot_id"]
        ]
        if len(produced) != 1:
            raise LedgerError("scope resource must produce exactly one worktree")
        worktree = produced[0]
        request = scope["request"]
        target_matches = [
            target
            for target in item["targets"]
            if target["repository"] == scope["immutable"]["repository"]
            and target["head"]["branch"] == request["branch"]
        ]
        if (
            len(target_matches) != 1
            or request["name"] != scope["immutable"]["name"]
            or request["branch"] != worktree["immutable"]["branch"]
            or target_matches[0]["head"]["initial_sha"] != request["start_sha"]
        ):
            raise LedgerError("scope request differs from target/worktree immutable intent")
        branches = [
            artifact_item
            for artifact_item in item["artifacts"]
            if artifact_item["kind"] == "branch"
            and artifact_item["immutable"]["repository"]["node_id"]
            == scope["immutable"]["repository"]["node_id"]
            and artifact_item["immutable"]["branch"]
            == worktree["immutable"]["branch"]
        ]
        if len(branches) != 1:
            raise LedgerError("scope production requires one exact matching branch")
        states = (scope["state"], worktree["state"], branches[0]["state"])
        if states not in {
            ("planned", "planned", "planned"),
            ("created", "created", "created"),
        }:
            raise LedgerError(
                "scope, produced worktree, and branch must bind atomically as created"
            )
        if scope["current"] is not None:
            raw = _retained_result(
                scope["current"]["binding"],
                f"scope resource {scope['slot_id']} binding",
            )
            _, row = _scope_show_record(
                raw,
                request,
                scope["immutable"]["repository"],
                f"scope resource {scope['slot_id']} scope show",
            )
            if (
                worktree["current"]["path"] != row["path"]
                or worktree["current"]["head_sha"]
                != target_matches[0]["head"]["current_sha"]
                or branches[0]["current"]["head_sha"]
                != target_matches[0]["head"]["current_sha"]
            ):
                raise LedgerError(
                    "scope result path/head differs from atomic branch/worktree binding"
                )
    coordinate_nodes: dict[tuple[str, str], str] = {}
    node_coordinates: dict[str, tuple[str, str]] = {}
    issue_coordinate_nodes: dict[tuple[str, int], str] = {}
    issue_node_coordinates: dict[str, tuple[str, int]] = {}
    pr_coordinate_nodes: dict[tuple[str, int], str] = {}
    pr_node_coordinates: dict[str, tuple[str, int]] = {}

    def remember_repository(repository_value: Mapping[str, Any]) -> None:
        owner, name, node = _repository(repository_value, "ledger repository identity")
        coordinate = (owner.casefold(), name.casefold())
        if coordinate in coordinate_nodes and coordinate_nodes[coordinate] != node:
            raise LedgerError("repository coordinate has conflicting node IDs")
        if node in node_coordinates and node_coordinates[node] != coordinate:
            raise LedgerError("repository node ID aliases multiple coordinates")
        coordinate_nodes[coordinate] = node
        node_coordinates[node] = coordinate

    program_issues = (
        []
        if item["program"] is None
        else [item["program"]["master_issue"], item["program"]["leaf_issue"]]
    )
    for issue_item in [
        *item["issues"]["explicit"],
        *item["issues"]["incidental"],
        *item["closing_scope"],
        *program_issues,
    ]:
        remember_repository(issue_item["repository"])
        coordinate = (issue_item["repository"]["node_id"], issue_item["number"])
        node = issue_item["node_id"]
        if coordinate in issue_coordinate_nodes and issue_coordinate_nodes[coordinate] != node:
            raise LedgerError("issue coordinate has conflicting node IDs")
        if node in issue_node_coordinates and issue_node_coordinates[node] != coordinate:
            raise LedgerError("issue node ID aliases multiple coordinates")
        issue_coordinate_nodes[coordinate] = node
        issue_node_coordinates[node] = coordinate
    for pull in item["selected_prs"]:
        remember_repository(pull["repository"])
        remember_repository(pull["head_repository"])
        if pull["head_repository"]["node_id"] not in push_repositories:
            raise LedgerError("authenticated actor lacks push authority for PR head")
        if (
            pull["body"]["ownership"] == "delivery-created"
            and pull["author_node_id"] != actor_node
        ):
            raise LedgerError("delivery-created PR author does not match actor")
        body = pull["body"]
        if body["state"] == "update-planned":
            intended_raw = _inline_payload(
                body["intended_payload"],
                "selected PR body intended payload",
                utf8=True,
            )
            start_marker, end_marker = _body_markers_for(item, pull)
            parts = _body_section_parts(intended_raw, start_marker, end_marker)
            if parts is None:
                raise LedgerError("planned PR body lacks its exact delivery section")
            owned_start, _, _ = parts
            if byte_digest(intended_raw[:owned_start]) != body["outside_digest"]:
                raise LedgerError("planned PR body changes bytes outside its owned section")
        if pull["comment"]["state"] != "none":
            expected_comment_marker = (
                f"<!-- {_delivery_surface_marker(item, pull, 'comment')} -->"
            )
            if pull["comment"]["marker"] != expected_comment_marker:
                raise LedgerError("selected PR comment marker is not coordinate-bound")
            comment = pull["comment"]
            if comment["state"] in {"planned", "in-flight"}:
                intended_raw = _inline_payload(
                    comment["intended_payload"],
                    "selected PR comment intended payload",
                    utf8=True,
                )
                marker_raw = expected_comment_marker.encode("ascii")
                if (
                    not intended_raw.startswith(marker_raw + b"\n")
                    or intended_raw.count(b"atrinik-delivery:comment:") != 1
                    or intended_raw.count(marker_raw) != 1
                ):
                    raise LedgerError(
                        "planned PR comment payload lacks its exact first-line marker"
                    )
        pr_coordinate = (pull["repository"]["node_id"], pull["number"])
        pr_node = pull["node_id"]
        if pr_coordinate in pr_coordinate_nodes and pr_coordinate_nodes[pr_coordinate] != pr_node:
            raise LedgerError("PR coordinate has conflicting node IDs")
        if pr_node in pr_node_coordinates and pr_node_coordinates[pr_node] != pr_coordinate:
            raise LedgerError("PR node ID aliases multiple coordinates")
        pr_coordinate_nodes[pr_coordinate] = pr_node
        pr_node_coordinates[pr_node] = pr_coordinate
        matching_targets = [
            target
            for target in item["targets"]
            if target["repository"]["node_id"] == pull["repository"]["node_id"]
            and target["base"]["branch"] == pull["base_branch"]
            and target["head"]["branch"] == pull["head_branch"]
        ]
        if len(matching_targets) != 1:
            raise LedgerError("selected PR does not match exactly one target")
        matching_slots = [
            slot
            for slot in item["artifacts"]
            if slot["kind"] == "pull_request"
            and slot["state"] in {"created", "adopted"}
            and slot["current"] is not None
            and slot["current"]["repository"]["node_id"]
            == pull["repository"]["node_id"]
            and slot["current"]["number"] == pull["number"]
            and slot["current"]["node_id"] == pull["node_id"]
            and slot["current"]["branch"] == pull["head_branch"]
            and slot["current"]["body_digest"]
            == pull["body"]["current_digest"]
        ]
        if len(matching_slots) != 1:
            raise LedgerError("selected PR does not match exactly one bound PR slot")
    bound_pr_slots = [
        slot
        for slot in item["artifacts"]
        if slot["kind"] == "pull_request" and slot["state"] != "planned"
    ]
    if len(bound_pr_slots) != len(item["selected_prs"]):
        raise LedgerError("every bound PR slot must be selected and vice versa")
    for slot in bound_pr_slots:
        current_identity = slot["current"]
        matching_pulls = [
            pull
            for pull in item["selected_prs"]
            if pull["repository"]["node_id"]
            == current_identity["repository"]["node_id"]
            and pull["number"] == current_identity["number"]
            and pull["node_id"] == current_identity["node_id"]
            and pull["head_branch"] == current_identity["branch"]
            and pull["body"]["current_digest"]
            == current_identity["body_digest"]
        ]
        if len(matching_pulls) != 1:
            raise LedgerError("bound PR slot does not match exactly one selected PR")
    for target in item["targets"]:
        remember_repository(target["repository"])
    for slot in item["artifacts"]:
        remember_repository(slot["immutable"]["repository"])
        if slot["current"] is not None:
            remember_repository(slot["current"]["repository"])
        if slot["kind"] == "pull_request":
            identity = slot["current"] or slot["immutable"]
            if identity["number"] is not None:
                pr_coordinate = (
                    identity["repository"]["node_id"],
                    identity["number"],
                )
                pr_node = identity["node_id"]
                if (
                    pr_coordinate in pr_coordinate_nodes
                    and pr_coordinate_nodes[pr_coordinate] != pr_node
                ):
                    raise LedgerError("PR coordinate has conflicting node IDs")
                if (
                    pr_node in pr_node_coordinates
                    and pr_node_coordinates[pr_node] != pr_coordinate
                ):
                    raise LedgerError("PR node ID aliases multiple coordinates")
                pr_coordinate_nodes[pr_coordinate] = pr_node
                pr_node_coordinates[pr_node] = pr_coordinate
    for resource in item["resources"]:
        remember_repository(resource["immutable"]["repository"])
        if resource["current"] is not None:
            remember_repository(resource["current"]["repository"])
    generation = _integer(item["generation"], "ledger.generation")
    previous = item["previous_byte_digest"]
    history = item["history"]
    if not isinstance(history, list):
        raise LedgerError("ledger.history must be an array")
    history_digests = [
        _string(digest, f"ledger.history[{index}]", SHA256_RE)
        for index, digest in enumerate(history)
    ]
    if len(history_digests) != generation - 1:
        raise LedgerError("ledger.history length must equal generation minus one")
    if generation == 1:
        if previous is not None or history_digests:
            raise LedgerError("generation 1 must not have a previous byte digest")
    else:
        _string(previous, "ledger.previous_byte_digest", SHA256_RE)
        if history_digests[-1] != previous:
            raise LedgerError("ledger history tail must equal previous byte digest")
    migration = item["migration"]
    if migration is not None:
        migration = _exact(
            migration,
            {"kind", "state", "source", "snapshot", "canonical_report", "marker_name"},
            "ledger.migration",
        )
        if migration["kind"] not in MIGRATION_KINDS:
            raise LedgerError("ledger.migration.kind is invalid")
        if migration["state"] != "complete":
            raise LedgerError("canonical ledger migration state must be complete")
        _source(migration["source"], "ledger.migration.source")
        snapshot = _source(migration["snapshot"], "ledger.migration.snapshot")
        if not _SNAPSHOT_RE.fullmatch(snapshot[0]):
            raise LedgerError("ledger.migration.snapshot name is noncanonical")
        canonical_report = _direct_name(
            migration["canonical_report"], "ledger.migration.canonical_report"
        )
        if not canonical_report.endswith(".md"):
            raise LedgerError("ledger.migration.canonical_report must be Markdown")
        marker = _direct_name(migration["marker_name"], "ledger.migration.marker_name")
        if not _MIGRATION_RE.fullmatch(marker):
            raise LedgerError("ledger.migration.marker_name is noncanonical")
    primary = explicit[0][3] if mode == "issue" else selected[0][3]
    expected_id = f"delivery-v1:{mode}:{primary}"
    if item["ledger_id"] != expected_id:
        raise LedgerError(f"ledger.ledger_id must be {expected_id}")
    # Re-encode to prove the document contains only JSON values and is bounded.
    raw = canonical_bytes(item)
    if len(raw) > MAX_BYTES:
        raise LedgerError(f"ledger exceeds {MAX_BYTES} bytes")
    return item


def prepare(document: Mapping[str, Any]) -> dict[str, Any]:
    """Return an independent, validated, canonical schema-v1 document."""

    raw = canonical_bytes(document)
    result = _decode(raw, "prepared ledger")
    validate(result)
    return result


def _require_genesis(document: Mapping[str, Any], context: str) -> None:
    if (
        document["generation"] != 1
        or document["history"] != []
        or document["previous_byte_digest"] is not None
        or document["migration"] is not None
    ):
        raise LedgerError(
            f"{context} requires generation 1, empty history, no predecessor, and no migration metadata"
        )


def _require_create_genesis(document: Mapping[str, Any]) -> None:
    """Prove a fresh ledger precedes every delivery-owned external mutation."""

    _require_genesis(document, "create")
    if document["authority"]["kind"] not in {
        "durable-goal",
        "explicit-invocation",
    }:
        raise LedgerError("fresh create requires current durable-goal or explicit-invocation authority")
    for slot in document["artifacts"]:
        if slot["kind"] != "worktree" or slot["state"] != "planned":
            continue
        if slot["immutable"]["path"] is not None or (
            slot["producer_resource_slot"] is None
        ) == (slot.get("primitive_request") is None):
            raise LedgerError(
                "fresh planned worktree requires one deferred scope or primitive request"
            )
    if document["entry_mode"] == "issue":
        if document["selected_prs"]:
            raise LedgerError("fresh issue-mode create cannot preselect a PR")
        if any(slot["state"] != "planned" for slot in document["artifacts"]):
            raise LedgerError("fresh issue-mode create requires all artifacts planned")
        if any(slot["state"] != "planned" for slot in document["resources"]):
            raise LedgerError("fresh issue-mode create requires all resources planned")
        for slot in document["artifacts"]:
            if slot["kind"] == "pull_request" and (
                slot["immutable"]["number"] is not None
                or slot["immutable"]["node_id"] is not None
            ):
                raise LedgerError("fresh issue-mode create cannot reserve a known PR")
        for target in document["targets"]:
            base = target["base"]
            head = target["head"]
            merge = target["merge_base"]
            anchor = base["initial_sha"]
            if (
                base["current_sha"] != anchor
                or base["lineage"] != [anchor]
                or head["initial_sha"] != anchor
                or head["current_sha"] != anchor
                or head["lineage"] != [anchor]
                or merge["initial_sha"] != anchor
                or merge["current_sha"] != anchor
            ):
                raise LedgerError(
                    "fresh issue-mode target must anchor base, head, and merge-base to one commit"
                )
            if head["branch"] != head["branch"].casefold():
                raise LedgerError("fresh issue-mode head branch must be normalized lowercase")
            if (
                head["branch"] == base["branch"]
                or head["branch"].casefold() == "head"
            ):
                raise LedgerError(
                    "fresh issue-mode head branch must be a distinct non-symbolic branch"
                )
        return
    if len(document["targets"]) != 1 or len(document["selected_prs"]) != 1:
        raise LedgerError("fresh PR-mode create requires one target and selected PR")
    slots = {slot["kind"]: slot for slot in document["artifacts"]}
    states = {kind: slot["state"] for kind, slot in slots.items()}
    if (
        states.get("pull_request") != "adopted"
        or states.get("branch") not in {"planned", "adopted"}
        or states.get("worktree") not in {"planned", "adopted"}
        or (
            states.get("worktree") == "adopted"
            and states.get("branch") != "adopted"
        )
    ):
        raise LedgerError(
            "fresh PR-mode create requires adopted PR and coherent planned/adopted local artifacts"
        )
    for kind in ("pull_request", "branch", "worktree"):
        slot = slots[kind]
        if slot["state"] == "adopted" and slot["safety"] != SAFE_ARTIFACT_STATE:
            raise LedgerError(
                f"fresh PR-mode adopted {kind} must have exact safe bound state"
            )
    if any(slot["state"] != "planned" for slot in document["resources"]):
        raise LedgerError("fresh PR-mode create requires all resources planned")
    target = document["targets"][0]
    for field in ("base", "head"):
        head = target[field]
        if (
            head["initial_sha"] != head["current_sha"]
            or head["lineage"] != [head["initial_sha"]]
        ):
            raise LedgerError(f"fresh PR-mode {field} must be one exact observed commit")
    if target["merge_base"]["initial_sha"] != target["merge_base"]["current_sha"]:
        raise LedgerError("fresh PR-mode merge-base observation must be exact")
    body = document["selected_prs"][0]["body"]
    if body["ownership"] != "contributor-owned" or body["state"] != "observed":
        raise LedgerError("fresh PR-mode create requires an observed contributor body")
    pr_slots = [slot for slot in document["artifacts"] if slot["kind"] == "pull_request"]
    if len(pr_slots) != 1 or pr_slots[0]["initial_body_payload"] is not None:
        raise LedgerError("fresh PR-mode adopted PR cannot carry a creation payload")
    if document["selected_prs"][0]["comment"]["state"] != "none":
        raise LedgerError("fresh PR-mode create cannot pre-own a delivery comment")
    if document["selected_prs"][0]["draft_intent"] is not None:
        raise LedgerError("fresh PR-mode create cannot pre-record a readiness intent")


def _require_migration_genesis(document: Mapping[str, Any], kind: str) -> None:
    _require_genesis(document, "migration")
    if kind == "legacy" and document["entry_mode"] != "issue":
        raise LedgerError("legacy migration is issue-mode only")
    if any(
        slot["kind"] == "worktree"
        and slot["state"] == "planned"
        and slot["immutable"]["path"] is not None
        for slot in document["artifacts"]
    ):
        raise LedgerError(
            "migration planned worktree requires a deferred scope or primitive request"
        )


def require_reusable_resources(document: Mapping[str, Any]) -> None:
    """Fail unless every recorded optional resource is bound and safe to reuse."""

    item = validate(document)
    for resource in item["resources"]:
        if resource["state"] == "planned" or resource["current"] is None:
            raise LedgerError(f"resource is not bound for reuse: {resource['slot_id']}")
        lifecycle = resource["current"]["lifecycle"]
        unsafe = (
            resource["kind"] in {"topology", "runtime"} and lifecycle == "running"
        ) or (
            resource["kind"] in {"state", "scenario"}
            and lifecycle in {"active", "consumed", "released"}
        ) or (resource["kind"] == "scope" and lifecycle == "released")
        if unsafe:
            raise LedgerError(
                f"resource lifecycle is unsafe for reuse: {resource['slot_id']}"
            )


def require_reusable_artifacts(document: Mapping[str, Any]) -> None:
    """Fail unless every bound local artifact is exact and safely reusable."""

    item = validate(document)
    for artifact in item["artifacts"]:
        if artifact["state"] == "planned":
            continue
        safety = artifact["safety"]
        if (
            not safety["certain"]
            or not safety["clean"]
            or safety["detached"]
            or safety["locked"]
            or safety["active"]
            or safety["unowned_reference"]
            or safety["foreign"]
        ):
            raise LedgerError(f"artifact is unsafe to reuse: {artifact['slot_id']}")


def body_recovery_action(
    document: Mapping[str, Any],
    pr_node_id: str,
    live_digest: str | None,
    live_updated_at: str | None,
) -> str:
    """Classify a live PR body against its durable intent without mutating it."""

    return classify_body_recovery(
        document, pr_node_id, live_digest, live_updated_at
    )["action"]


def classify_body_recovery(
    document: Mapping[str, Any],
    pr_node_id: str,
    live_digest: str | None,
    live_updated_at: str | None,
) -> dict[str, Any]:
    """Return an exact body recovery action, durable payload, and CAS projection."""

    item = validate(document)
    _string(pr_node_id, "pr_node_id", NODE_RE)
    if live_digest is not None:
        _string(live_digest, "live_digest", SHA256_RE)
    live_timestamp = _optional_timestamp(live_updated_at, "live_updated_at")
    if (live_digest is None) != (live_timestamp is None):
        raise LedgerError("live body digest and timestamp must both be present or absent")
    matches = [pull for pull in item["selected_prs"] if pull["node_id"] == pr_node_id]
    if len(matches) != 1:
        raise LedgerError("PR body recovery requires one exact selected PR")
    pull = matches[0]
    body = pull["body"]
    result: dict[str, Any] = {
        "pr_node_id": pr_node_id,
        "live_digest": live_digest,
        "live_updated_at": live_timestamp,
        "intended_digest": body["intended_digest"],
        "intended_payload": body["intended_payload"],
        "cas_body": None,
        "cas_artifact_body_digest": None,
    }
    if live_timestamp is None:
        raise LedgerError("selected PR body is absent")
    timestamp_later = _timestamp_is_after(live_timestamp, body["updated_at"])
    timestamp_equal = _timestamp_is_equal(live_timestamp, body["updated_at"])
    if body["ownership"] == "contributor-owned":
        if live_digest == body["current_digest"] and timestamp_equal:
            return {**result, "action": "read-only-match"}
        if timestamp_later:
            replacement = {
                **body,
                "observed_digest": live_digest,
                "current_digest": live_digest,
                "outside_digest": live_digest,
                "updated_at": live_timestamp,
            }
            return {
                **result,
                "action": "refresh-observation",
                "cas_body": replacement,
                "cas_artifact_body_digest": live_digest,
            }
    if body["state"] == "update-planned":
        if live_digest == body["current_digest"]:
            if timestamp_equal:
                return {**result, "action": "apply-intended"}
            if timestamp_later:
                return {
                    **result,
                    "action": "refresh-intent-observation",
                    "cas_body": {**body, "updated_at": live_timestamp},
                    "cas_artifact_body_digest": live_digest,
                }
        if live_digest == body["intended_digest"] and (
            timestamp_equal or timestamp_later
        ):
            intended_raw = _inline_payload(
                body["intended_payload"], "body recovery intended payload", utf8=True
            )
            markers = _body_markers_for(item, pull)
            parts = _body_section_parts(intended_raw, *markers)
            if parts is None:
                raise LedgerError("body recovery intent lacks its delivery section")
            _, _, section = parts
            replacement = {
                **body,
                "state": "written",
                "intended_digest": None,
                "intended_payload": None,
                "current_digest": live_digest,
                "section_digest": byte_digest(section),
                "updated_at": live_timestamp,
            }
            return {
                **result,
                "action": "bind-intended",
                "cas_body": replacement,
                "cas_artifact_body_digest": live_digest,
            }
    elif body["state"] == "written" and live_digest == body["current_digest"]:
        if timestamp_equal:
            return {**result, "action": "none"}
        if timestamp_later:
            return {
                **result,
                "action": "refresh-observation",
                "cas_body": {**body, "updated_at": live_timestamp},
                "cas_artifact_body_digest": live_digest,
            }
    raise LedgerError(
        "live PR body digest/timestamp matches neither exact current nor intended result"
    )


def _delivery_surface_marker(
    item: Mapping[str, Any], pull: Mapping[str, Any], surface: str
) -> str:
    if surface not in {"body", "comment"}:
        raise LedgerError("delivery marker surface must be body or comment")
    coordinate = {
        "ledger_id": item["ledger_id"],
        "surface": surface,
        "repository": pull["repository"],
        "number": pull["number"],
        "node_id": pull["node_id"],
    }
    token = byte_digest(canonical_bytes(coordinate))
    return f"atrinik-delivery:{surface}:{token}"


def delivery_surface_marker(
    document: Mapping[str, Any], pr_node_id: str, surface: str
) -> str:
    """Derive a coordinate-bound marker; live marker text never grants ownership."""

    item = validate(document)
    _string(pr_node_id, "pr_node_id", NODE_RE)
    matches = [pull for pull in item["selected_prs"] if pull["node_id"] == pr_node_id]
    if len(matches) != 1:
        raise LedgerError("delivery marker requires one exact selected PR")
    return _delivery_surface_marker(item, matches[0], surface)


def _body_markers(
    document: Mapping[str, Any], pr_node_id: str
) -> tuple[bytes, bytes]:
    item = validate(document)
    matches = [pull for pull in item["selected_prs"] if pull["node_id"] == pr_node_id]
    if len(matches) != 1:
        raise LedgerError("body markers require one exact selected PR")
    return _body_markers_for(item, matches[0])


def _body_markers_for(
    item: Mapping[str, Any], pull: Mapping[str, Any]
) -> tuple[bytes, bytes]:
    marker = _delivery_surface_marker(item, pull, "body").encode("ascii")
    return b"<!-- " + marker + b":start -->", b"<!-- " + marker + b":end -->"


def _body_section_parts(
    raw: bytes, start_marker: bytes, end_marker: bytes
) -> tuple[int, int, bytes] | None:
    if not isinstance(raw, bytes):
        raise LedgerError("PR body must be bytes")
    if len(raw) > MAX_BYTES:
        raise LedgerError(f"PR body exceeds {MAX_BYTES} bytes")
    try:
        raw.decode("utf-8")
    except UnicodeError as error:
        raise LedgerError("PR body must be valid UTF-8") from error
    starts = raw.count(start_marker)
    ends = raw.count(end_marker)
    namespace_count = raw.count(BODY_NAMESPACE)
    if starts == 0 and ends == 0:
        if namespace_count:
            raise LedgerError("PR body contains a malformed delivery marker")
        return None
    if starts != 1 or ends != 1 or namespace_count != 2:
        raise LedgerError("PR body contains duplicate or malformed delivery markers")
    start = raw.find(start_marker)
    end = raw.find(end_marker)
    content_start = start + len(start_marker)
    body_end = end + len(end_marker)
    if start < 0 or end <= content_start:
        raise LedgerError("PR body delivery markers are reversed or overlapping")
    if body_end != len(raw):
        raise LedgerError("PR body delivery section must be terminal")
    if raw[content_start : content_start + 1] != b"\n" or raw[end - 1 : end] != b"\n":
        raise LedgerError("PR body delivery markers require canonical newline framing")
    if start == 0:
        owned_start = 0
    elif raw[start - 1 : start] == b"\n":
        owned_start = start - 1
    else:
        raise LedgerError("PR body delivery section lacks its owned newline separator")
    section = raw[content_start + 1 : end - 1]
    return owned_start, body_end, section


def check_body_section(
    document: Mapping[str, Any], pr_node_id: str, raw: bytes
) -> dict[str, Any]:
    """Check exact delivery marker bytes without decoding or rewriting the body."""

    item = validate(document)
    pulls = [pull for pull in item["selected_prs"] if pull["node_id"] == pr_node_id]
    if len(pulls) != 1:
        raise LedgerError("body checking requires one exact selected PR")
    start_marker, end_marker = _body_markers(item, pr_node_id)
    parts = _body_section_parts(raw, start_marker, end_marker)
    result: dict[str, Any] = {"body_digest": byte_digest(raw)}
    if parts is None:
        result.update(state="absent", section_digest=None, outside_digest=byte_digest(raw))
        return result
    if pulls[0]["body"]["ownership"] not in {
        "delivery-created",
        "delivery-section",
    }:
        raise LedgerError("live marker in contributor-owned body is unowned")
    start, end, section = parts
    result.update(
        state="present",
        section_digest=byte_digest(section),
        outside_digest=byte_digest(raw[:start]),
    )
    return result


def plan_body_section(
    document: Mapping[str, Any], pr_node_id: str, raw: bytes, section: bytes
) -> bytes:
    """Return an exact marker-section edit while preserving every outside byte."""

    item = validate(document)
    matches = [pull for pull in item["selected_prs"] if pull["node_id"] == pr_node_id]
    if len(matches) != 1:
        raise LedgerError("body planning requires one exact selected PR")
    body = matches[0]["body"]
    if body["state"] not in {"observed", "written"}:
        raise LedgerError("body planning requires an intent-free body observation")
    if byte_digest(raw) != body["current_digest"]:
        raise LedgerError("body planning input differs from the recorded current body bytes")
    if not isinstance(section, bytes):
        raise LedgerError("delivery section must be bytes")
    try:
        section.decode("utf-8")
    except UnicodeError as error:
        raise LedgerError("delivery section must be valid UTF-8") from error
    if len(section) > MAX_BYTES or BODY_NAMESPACE in section:
        raise LedgerError("delivery section is oversized or contains a reserved marker")
    start_marker, end_marker = _body_markers(document, pr_node_id)
    parts = _body_section_parts(raw, start_marker, end_marker)
    if body["ownership"] == "contributor-owned":
        if parts is not None or body["section_digest"] is not None:
            raise LedgerError("contributor body cannot already contain an owned section")
        outside = raw
    elif parts is None:
        if body["ownership"] != "delivery-created" or body["section_digest"] is not None:
            raise LedgerError("recorded delivery body lost its owned section")
        outside = raw
    else:
        start, _, current_section = parts
        outside = raw[:start]
        if (
            byte_digest(outside) != body["outside_digest"]
            or byte_digest(current_section) != body["section_digest"]
        ):
            raise LedgerError("recorded delivery body section/outside bytes drifted")
    framed = (
        (b"\n" if outside else b"")
        + start_marker
        + b"\n"
        + section
        + b"\n"
        + end_marker
    )
    result = outside + framed
    if len(result) > MAX_BYTES:
        raise LedgerError(f"planned PR body exceeds {MAX_BYTES} bytes")
    _body_section_parts(result, start_marker, end_marker)
    if result == raw:
        raise LedgerError("planned PR body edit is a no-op")
    return result


def describe_body_plan(
    document: Mapping[str, Any], pr_node_id: str, raw: bytes, section: bytes
) -> dict[str, Any]:
    """Return exact intended bytes and the complete body object ready for CAS."""

    item = validate(document)
    pull = next(
        (value for value in item["selected_prs"] if value["node_id"] == pr_node_id),
        None,
    )
    if pull is None:
        raise LedgerError("body planning requires one exact selected PR")
    planned = plan_body_section(item, pr_node_id, raw, section)
    parts = _body_section_parts(planned, *_body_markers_for(item, pull))
    if parts is None:
        raise LedgerError("planned body lacks its delivery section")
    owned_start, _, intended_section = parts
    digest = byte_digest(planned)
    payload = _retained_result_document(planned, "planned PR body")
    body = pull["body"]
    planned_body = {
        **body,
        "ownership": (
            "delivery-section"
            if body["ownership"] == "contributor-owned"
            else body["ownership"]
        ),
        "state": "update-planned",
        "observed_digest": body["current_digest"],
        "intended_digest": digest,
        "intended_payload": payload,
        "outside_digest": byte_digest(planned[:owned_start]),
    }
    return {
        "pr_node_id": pr_node_id,
        "body_digest": digest,
        "outside_digest": planned_body["outside_digest"],
        "section_digest": byte_digest(intended_section),
        "body_base64": payload["raw_base64"],
        "body": planned_body,
    }


def classify_comments(
    document: Mapping[str, Any],
    pr_node_id: str,
    inventory: Mapping[str, Any],
) -> dict[str, Any]:
    """Classify a fully paginated comment inventory for exactly-once recovery."""

    item = validate(document)
    pulls = [pull for pull in item["selected_prs"] if pull["node_id"] == pr_node_id]
    if len(pulls) != 1:
        raise LedgerError("comment recovery requires one exact selected PR")
    comment_state = pulls[0]["comment"]
    marker = f"<!-- {delivery_surface_marker(item, pr_node_id, 'comment')} -->"
    value = _exact(inventory, {"pagination_complete", "comments"}, "comment inventory")
    if value["pagination_complete"] is not True:
        raise LedgerError("comment inventory is not fully paginated")
    if not isinstance(value["comments"], list):
        raise LedgerError("comment inventory comments must be an array")
    matches: list[tuple[str, str]] = []
    seen_nodes: set[str] = set()
    for index, raw_comment in enumerate(value["comments"]):
        comment = _exact(
            raw_comment,
            {"node_id", "author_node_id", "body"},
            f"comment inventory.comments[{index}]",
        )
        node = _string(comment["node_id"], f"comment[{index}].node_id", NODE_RE)
        if node in seen_nodes:
            raise LedgerError("comment inventory contains duplicate node IDs")
        seen_nodes.add(node)
        author = _string(
            comment["author_node_id"], f"comment[{index}].author_node_id", NODE_RE
        )
        body = comment["body"]
        if not isinstance(body, str):
            raise LedgerError(f"comment[{index}].body is invalid")
        try:
            body_raw = body.encode("utf-8")
        except UnicodeError as error:
            raise LedgerError(f"comment[{index}].body is not valid UTF-8") from error
        if len(body_raw) > MAX_BYTES:
            raise LedgerError(f"comment[{index}].body is invalid")
        marker_count = body.count(marker)
        namespace_count = body.count("atrinik-delivery:comment:")
        if namespace_count and (marker_count != 1 or namespace_count != 1):
            raise LedgerError("comment inventory contains a malformed or foreign marker")
        if marker_count:
            if author != item["actor"]["node_id"]:
                raise LedgerError("delivery comment marker has the wrong author")
            matches.append((node, byte_digest(body_raw)))
    if len(matches) > 1:
        raise LedgerError("duplicate delivery comments block recovery")
    if comment_state["state"] == "none":
        if matches:
            raise LedgerError("unowned live comment marker does not grant ownership")
        return {
            "action": "plan-required",
            "marker": marker,
            "comment_node_id": None,
            "intended_digest": None,
            "intended_payload": None,
        }
    if comment_state["marker"] != marker:
        raise LedgerError("recorded comment marker differs from the exact coordinate")
    if comment_state["state"] == "planned":
        if comment_state["node_id"] is None:
            if matches:
                raise LedgerError("planned first comment unexpectedly already exists")
        else:
            if matches != [
                (comment_state["node_id"], comment_state["current_digest"])
            ]:
                raise LedgerError("planned comment update lost its exact bound comment")
        return {
            "action": "mark-in-flight-before-write",
            "marker": marker,
            "comment_node_id": comment_state["node_id"],
            "intended_digest": comment_state["intended_digest"],
            "intended_payload": comment_state["intended_payload"],
        }
    if comment_state["state"] == "in-flight":
        intended = comment_state["intended_digest"]
        intended_matches = [match for match in matches if match[1] == intended]
        if intended_matches:
            if comment_state["node_id"] is not None and intended_matches[0][0] != comment_state["node_id"]:
                raise LedgerError("comment update appeared under a different node")
            return {
                "action": "bind-observed",
                "marker": marker,
                "comment_node_id": intended_matches[0][0],
                "current_digest": intended,
                "intended_digest": intended,
                "intended_payload": comment_state["intended_payload"],
            }
        if comment_state["node_id"] is not None and matches == [
            (comment_state["node_id"], comment_state["current_digest"])
        ]:
            return {
                "action": "apply-intended",
                "marker": marker,
                "comment_node_id": comment_state["node_id"],
                "intended_digest": intended,
                "intended_payload": comment_state["intended_payload"],
            }
        raise LedgerError("in-flight comment write has no safely retryable live result")
    expected = [(comment_state["node_id"], comment_state["current_digest"])]
    if matches != expected:
        raise LedgerError("bound delivery comment drifted or disappeared")
    return {
        "action": "bound-match",
        "marker": marker,
        "comment_node_id": comment_state["node_id"],
        "current_digest": comment_state["current_digest"],
        "intended_digest": None,
        "intended_payload": None,
    }


def classify_pr_binding(
    document: Mapping[str, Any], slot_id: str, identity: Mapping[str, Any]
) -> str:
    """Classify one remote PR result against a durable planned PR slot."""

    item = validate(document)
    slot_id = _string(slot_id, "slot_id", SLOT_RE)
    candidate = _exact(
        identity,
        {"repository", "head_branch", "number", "node_id", "head_sha", "body_digest"},
        "PR binding candidate",
    )
    repository_identity = _repository(
        candidate["repository"], "PR binding candidate.repository"
    )
    head_branch = _branch(candidate["head_branch"], "PR binding candidate.head_branch")
    number = _integer(candidate["number"], "PR binding candidate.number")
    node = _string(candidate["node_id"], "PR binding candidate.node_id", NODE_RE)
    head_sha = _string(candidate["head_sha"], "PR binding candidate.head_sha", COMMIT_RE)
    body_digest = _string(
        candidate["body_digest"], "PR binding candidate.body_digest", SHA256_RE
    )
    matches = [slot for slot in item["artifacts"] if slot["slot_id"] == slot_id]
    if len(matches) != 1 or matches[0]["kind"] != "pull_request":
        raise LedgerError("PR binding requires one exact pull-request slot")
    slot = matches[0]
    if slot["state"] != "planned" or slot["current"] is not None:
        raise LedgerError("PR binding slot is already bound")
    immutable = slot["immutable"]
    if (
        immutable["repository"] != candidate["repository"]
        or repository_identity[2] != immutable["repository"]["node_id"]
        or immutable["branch"] != head_branch
        or immutable["body_digest"] != body_digest
        or (immutable["number"] is not None and immutable["number"] != number)
        or (immutable["node_id"] is not None and immutable["node_id"] != node)
    ):
        raise LedgerError("PR binding candidate differs from planned immutable identity")
    targets = [
        target
        for target in item["targets"]
        if target["repository"]["node_id"] == repository_identity[2]
        and target["head"]["branch"] == head_branch
    ]
    if len(targets) != 1 or targets[0]["head"]["current_sha"] != head_sha:
        raise LedgerError("PR binding candidate head differs from the exact target")
    return "bind-exact"


def planned_pr_payload(document: Mapping[str, Any], slot_id: str) -> bytes:
    """Return the exact durable initial body for one still-planned PR slot."""

    item = validate(document)
    slot_id = _string(slot_id, "slot_id", SLOT_RE)
    matches = [slot for slot in item["artifacts"] if slot["slot_id"] == slot_id]
    if (
        len(matches) != 1
        or matches[0]["kind"] != "pull_request"
        or matches[0]["state"] != "planned"
        or matches[0]["current"] is not None
        or matches[0]["initial_body_payload"] is None
    ):
        raise LedgerError("PR creation payload requires one exact unbound planned PR slot")
    return _inline_payload(
        matches[0]["initial_body_payload"],
        "planned PR initial body payload",
        utf8=True,
    )


def describe_planned_pr_payload(
    document: Mapping[str, Any], slot_id: str
) -> dict[str, Any]:
    raw = planned_pr_payload(document, slot_id)
    item = validate(document)
    slot = next(value for value in item["artifacts"] if value["slot_id"] == slot_id)
    return {
        "slot_id": slot_id,
        "body_digest": byte_digest(raw),
        "body_payload": slot["initial_body_payload"],
    }


def _retained_result_document(raw: bytes, context: str) -> dict[str, str]:
    if not isinstance(raw, bytes):
        raise LedgerError(f"{context} must be bytes")
    if len(raw) > MAX_RETAINED_RESULT_BYTES:
        raise LedgerError(
            f"{context} exceeds {MAX_RETAINED_RESULT_BYTES} retained result bytes"
        )
    return {
        "encoding": "base64",
        "raw_base64": base64.b64encode(raw).decode("ascii"),
        "sha256": byte_digest(raw),
    }


def _json_object_copy(value: Mapping[str, Any], context: str) -> dict[str, Any]:
    return _decode(canonical_bytes(value), context)


def classify_worktree_output(
    document: Mapping[str, Any],
    slot_id: str,
    worktree_list_raw: bytes,
    safety_observation_raw: bytes,
    create_output_raw: bytes | None = None,
) -> dict[str, Any]:
    """Bind one deferred primitive worktree from retained fresh live evidence."""

    item = validate(document)
    slot_id = _string(slot_id, "slot_id", SLOT_RE)
    matches = [slot for slot in item["artifacts"] if slot["slot_id"] == slot_id]
    if len(matches) != 1 or matches[0]["kind"] != "worktree":
        raise LedgerError("worktree binding requires one exact worktree slot")
    worktree = matches[0]
    request = worktree["primitive_request"]
    if request is None or worktree["producer_resource_slot"] is not None:
        raise LedgerError("worktree binding requires deferred primitive request intent")
    result = {
        "create_output": (
            None
            if create_output_raw is None
            else _retained_result_document(create_output_raw, "worktree create output")
        ),
        "worktree_list": _retained_result_document(
            worktree_list_raw, "worktree list output"
        ),
        "safety_observation": _retained_result_document(
            safety_observation_raw, "worktree safety observation"
        ),
    }
    path = _primitive_result_path(result, request, "worktree binding", live=True)
    branch_matches = [
        slot
        for slot in item["artifacts"]
        if slot["kind"] == "branch"
        and slot["immutable"]["repository"] == request["repository"]
        and slot["immutable"]["branch"] == request["branch"]
    ]
    if worktree["state"] != "planned":
        if (
            worktree["primitive_result"] != result
            or worktree["current"]["path"] != path
        ):
            raise LedgerError("deferred primitive worktree is already bound differently")
        if (
            worktree["safety"] != SAFE_ARTIFACT_STATE
            or len(branch_matches) != 1
            or branch_matches[0]["state"] == "planned"
            or branch_matches[0]["safety"] != SAFE_ARTIFACT_STATE
        ):
            raise LedgerError("bound primitive worktree is not safely reusable")
        return {
            "classification": "bound-match",
            "slot_id": slot_id,
            "request_sha256": canonical_object_digest(request),
            "result_sha256": canonical_object_digest(result),
            "path": path,
            "branch_artifact": None,
            "worktree_artifact": worktree,
        }
    targets = [
        target
        for target in item["targets"]
        if target["repository"] == request["repository"]
        and target["head"]["branch"] == request["branch"]
    ]
    if (
        len(targets) != 1
        or targets[0]["head"]["initial_sha"] != request["expected_head_sha"]
        or targets[0]["head"]["current_sha"] != request["expected_head_sha"]
    ):
        raise LedgerError("worktree request/result head differs from current target")
    if len(branch_matches) != 1:
        raise LedgerError("worktree request lacks one exact branch artifact")
    branch = branch_matches[0]
    if branch["state"] not in {"planned", "adopted"}:
        raise LedgerError("worktree request branch is not bindable")
    if branch["state"] == "adopted" and (
        branch["current"]["head_sha"] != request["expected_head_sha"]
        or branch["safety"] != SAFE_ARTIFACT_STATE
    ):
        raise LedgerError("adopted worktree request branch is not exact and safe")
    branch_result = _json_object_copy(branch, "worktree branch binding")
    if branch_result["state"] == "planned":
        branch_result.update(
            state="created",
            current={
                **_json_object_copy(branch_result["immutable"], "branch identity"),
                "head_sha": request["expected_head_sha"],
            },
            safety=dict(SAFE_ARTIFACT_STATE),
        )
    worktree_result = _json_object_copy(worktree, "worktree binding")
    current_identity = _json_object_copy(
        worktree_result["immutable"], "worktree identity"
    )
    current_identity.update(path=path, head_sha=request["expected_head_sha"])
    worktree_result.update(
        state="created",
        current=current_identity,
        safety=dict(SAFE_ARTIFACT_STATE),
        primitive_result=result,
    )
    candidate = _json_object_copy(item, "worktree binding candidate")
    candidate["artifacts"] = [
        branch_result
        if slot["slot_id"] == branch_result["slot_id"]
        else worktree_result
        if slot["slot_id"] == worktree_result["slot_id"]
        else slot
        for slot in candidate["artifacts"]
    ]
    validate(candidate)
    return {
        "classification": "bind-exact",
        "slot_id": slot_id,
        "request_sha256": canonical_object_digest(request),
        "result_sha256": canonical_object_digest(result),
        "path": path,
        "branch_artifact": branch_result,
        "worktree_artifact": worktree_result,
    }


def classify_scope_output(
    document: Mapping[str, Any],
    slot_id: str,
    raw: bytes,
    worktree_list_raw: bytes,
    safety_observation_raw: bytes,
) -> dict[str, Any]:
    """Bind one planned scope from exact scope and fresh worktree evidence."""

    item = validate(document)
    slot_id = _string(slot_id, "slot_id", SLOT_RE)
    matches = [resource for resource in item["resources"] if resource["slot_id"] == slot_id]
    if len(matches) != 1 or matches[0]["kind"] != "scope":
        raise LedgerError("scope binding requires one exact scope resource slot")
    scope = matches[0]
    request = scope["request"]
    retained = _retained_result_document(raw, "scope show output")
    record, row = _scope_show_record(
        raw, request, scope["immutable"]["repository"], "scope show output"
    )
    observation = {
        "worktree_list": _retained_result_document(
            worktree_list_raw, "scope worktree list output"
        ),
        "safety_observation": _retained_result_document(
            safety_observation_raw, "scope safety observation"
        ),
    }
    _scope_binding_observation(
        observation,
        request,
        scope["immutable"]["repository"],
        row,
        retained["sha256"],
        record,
        "scope binding observation",
        live=True,
    )
    worktrees = [
        slot
        for slot in item["artifacts"]
        if slot["kind"] == "worktree"
        and slot["producer_resource_slot"] == slot_id
    ]
    branches = [
        slot
        for slot in item["artifacts"]
        if slot["kind"] == "branch"
        and slot["immutable"]["repository"] == scope["immutable"]["repository"]
        and slot["immutable"]["branch"] == request["branch"]
    ]
    if scope["state"] != "planned":
        if scope["current"]["lifecycle"] != "active":
            raise LedgerError("released scope result is not reusable or bindable")
        if (
            scope["current"]["binding"] != retained
            or scope["current"]["observation"] != observation
            or scope["current"]["external_generation"] != record["generation"]
        ):
            raise LedgerError("scope resource is already bound differently")
        if (
            len(worktrees) != 1
            or len(branches) != 1
            or worktrees[0]["safety"] != SAFE_ARTIFACT_STATE
            or branches[0]["safety"] != SAFE_ARTIFACT_STATE
        ):
            raise LedgerError("bound scope branch/worktree is not safely reusable")
        return {
            "classification": "bound-match",
            "slot_id": slot_id,
            "request_sha256": record["request_sha256"],
            "result_sha256": retained["sha256"],
            "path": row["path"],
            "resource": scope,
            "branch_artifact": None,
            "worktree_artifact": None,
        }
    targets = [
        target
        for target in item["targets"]
        if target["repository"] == scope["immutable"]["repository"]
        and target["head"]["branch"] == request["branch"]
    ]
    if (
        len(targets) != 1
        or targets[0]["head"]["initial_sha"] != request["start_sha"]
        or targets[0]["head"]["current_sha"] != request["start_sha"]
    ):
        raise LedgerError("scope result head differs from the exact current target")
    if (
        len(worktrees) != 1
        or len(branches) != 1
        or worktrees[0]["state"] != "planned"
        or branches[0]["state"] != "planned"
    ):
        raise LedgerError("scope binding requires one planned branch/worktree pair")
    resource_result = _json_object_copy(scope, "scope resource binding")
    resource_result.update(
        state="created",
        current={
            **_json_object_copy(scope["immutable"], "scope identity"),
            "generation": 1,
            "external_generation": record["generation"],
            "identity_digest": retained["sha256"],
            "history": [],
            "lifecycle": "active",
            "binding": retained,
            "observation": observation,
        },
    )
    branch_result = _json_object_copy(branches[0], "scope branch binding")
    branch_result.update(
        state="created",
        current={
            **_json_object_copy(branch_result["immutable"], "scope branch identity"),
            "head_sha": request["start_sha"],
        },
        safety=dict(SAFE_ARTIFACT_STATE),
    )
    worktree_result = _json_object_copy(worktrees[0], "scope worktree binding")
    worktree_identity = _json_object_copy(
        worktree_result["immutable"], "scope worktree identity"
    )
    worktree_identity.update(path=row["path"], head_sha=request["start_sha"])
    worktree_result.update(
        state="created",
        current=worktree_identity,
        safety=dict(SAFE_ARTIFACT_STATE),
    )
    candidate = _json_object_copy(item, "scope binding candidate")
    candidate["resources"] = [
        resource_result if resource["slot_id"] == slot_id else resource
        for resource in candidate["resources"]
    ]
    candidate["artifacts"] = [
        branch_result
        if slot["slot_id"] == branch_result["slot_id"]
        else worktree_result
        if slot["slot_id"] == worktree_result["slot_id"]
        else slot
        for slot in candidate["artifacts"]
    ]
    validate(candidate)
    return {
        "classification": "bind-exact",
        "slot_id": slot_id,
        "request_sha256": record["request_sha256"],
        "result_sha256": retained["sha256"],
        "path": row["path"],
        "resource": resource_result,
        "branch_artifact": branch_result,
        "worktree_artifact": worktree_result,
    }


def _next_binding_candidate(
    snapshot: Snapshot, classification: Mapping[str, Any], *, scope: bool
) -> dict[str, Any]:
    candidate = _json_object_copy(snapshot.document, "atomic binding candidate")
    candidate["generation"] += 1
    candidate["previous_byte_digest"] = snapshot.digest
    candidate["history"].append(snapshot.digest)
    replacements = {
        value["slot_id"]: value
        for value in (
            classification["branch_artifact"],
            classification["worktree_artifact"],
        )
        if value is not None
    }
    candidate["artifacts"] = [
        replacements.get(value["slot_id"], value) for value in candidate["artifacts"]
    ]
    if scope:
        resource = classification["resource"]
        candidate["resources"] = [
            resource if value["slot_id"] == resource["slot_id"] else value
            for value in candidate["resources"]
        ]
    return candidate


def _initial_binding_candidate(
    snapshot: Snapshot,
    classification: Mapping[str, Any],
    *,
    scope: bool,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
) -> tuple[dict[str, Any], _AtomicBindingCapability]:
    """Create one exact purpose-scoped projection and its private CAS authority."""

    candidate = prepare(
        _next_binding_candidate(snapshot, classification, scope=scope)
    )
    raw = canonical_bytes(candidate)
    capability = _AtomicBindingCapability(
        _ATOMIC_BIND_TOKEN,
        "scope" if scope else "worktree",
        classification["slot_id"],
        snapshot.name,
        snapshot.raw,
        raw,
        expected_generation,
        expected_digest,
        expected_device,
        expected_inode,
    )
    return candidate, capability


def _binding_recovery_capability(
    snapshot: Snapshot,
    classification: Mapping[str, Any],
    *,
    scope: bool,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
) -> _AtomicBindingCapability:
    """Authorize only a tagged post-rename recovery of exact installed bytes."""

    return _AtomicBindingCapability(
        _ATOMIC_BIND_TOKEN,
        "scope" if scope else "worktree",
        classification["slot_id"],
        snapshot.name,
        None,
        snapshot.raw,
        expected_generation,
        expected_digest,
        expected_device,
        expected_inode,
    )


def _atomic_binding_output(
    classification: Mapping[str, Any], snapshot: Snapshot
) -> dict[str, Any]:
    return {
        "classification": classification["classification"],
        "slot_id": classification["slot_id"],
        "request_sha256": classification["request_sha256"],
        "result_sha256": classification["result_sha256"],
        "path": classification["path"],
        "snapshot": snapshot.json(),
    }


def _snapshot_matches_tuple(
    snapshot: Snapshot,
    generation: int,
    digest: str,
    device: int,
    inode: int,
) -> bool:
    return (
        snapshot.document["generation"],
        snapshot.digest,
        snapshot.device,
        snapshot.inode,
    ) == (generation, digest, device, inode)


def _require_clean_bound_snapshot(root: Path | str, snapshot: Snapshot) -> Snapshot:
    """Prove an idempotent bound-match has no unrecovered target receipt."""

    with _locked_root(Path(root)) as directory:
        current_inventory = _inventory_locked(directory)
        current = _snapshot(directory, snapshot.name)
        if (
            current.raw != snapshot.raw
            or current.device != snapshot.device
            or current.inode != snapshot.inode
        ):
            raise LedgerError("bound-match snapshot changed during recovery check")
        pending = [
            item for item in current_inventory.pending if item.target == snapshot.name
        ]
        if pending:
            raise LedgerError(
                "bound-match has a pending predecessor receipt; recover with the "
                "original CAS tuple"
            )
        return current


def bind_worktree_cas(
    root: Path | str,
    name: str,
    slot_id: str,
    worktree_list_raw: bytes,
    safety_observation_raw: bytes,
    *,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
    create_output_raw: bytes | None = None,
    failpoint: Failpoint = None,
) -> dict[str, Any]:
    """Classify, re-prove, and CAS one primitive worktree as one operation."""

    snapshot = inspect(root, name)
    classification = classify_worktree_output(
        snapshot.document,
        slot_id,
        worktree_list_raw,
        safety_observation_raw,
        create_output_raw,
    )
    worktree = next(
        slot
        for slot in snapshot.document["artifacts"]
        if slot["slot_id"] == classification["slot_id"]
    )
    request = worktree["primitive_request"]
    if request is None:
        raise LedgerError("atomic primitive binding lost its immutable request")
    result = {
        "create_output": (
            None
            if create_output_raw is None
            else _retained_result_document(
                create_output_raw, "worktree create output"
            )
        ),
        "worktree_list": _retained_result_document(
            worktree_list_raw, "worktree list output"
        ),
        "safety_observation": _retained_result_document(
            safety_observation_raw, "worktree safety observation"
        ),
    }
    _hit(failpoint, "worktree-bind:classified")
    with _pinned_live_worktree(
        request, classification["path"], "atomic primitive binding"
    ) as guard:
        def revalidate() -> None:
            if (
                _primitive_result_path(
                    result,
                    request,
                    "atomic primitive binding",
                    live=True,
                    guard=guard,
                )
                != classification["path"]
            ):
                raise LedgerError("atomic primitive binding path changed")

        revalidate()
        if (
            classification["classification"] == "bound-match"
            and _snapshot_matches_tuple(
                snapshot,
                expected_generation,
                expected_digest,
                expected_device,
                expected_inode,
            )
        ):
            installed = _require_clean_bound_snapshot(root, snapshot)
        else:
            if classification["classification"] == "bound-match":
                candidate = snapshot.document
                capability = _binding_recovery_capability(
                    snapshot,
                    classification,
                    scope=False,
                    expected_generation=expected_generation,
                    expected_digest=expected_digest,
                    expected_device=expected_device,
                    expected_inode=expected_inode,
                )
            else:
                candidate, capability = _initial_binding_candidate(
                    snapshot,
                    classification,
                    scope=False,
                    expected_generation=expected_generation,
                    expected_digest=expected_digest,
                    expected_device=expected_device,
                    expected_inode=expected_inode,
                )
            installed = cas(
                root,
                snapshot.name,
                candidate,
                expected_generation=expected_generation,
                expected_digest=expected_digest,
                expected_device=expected_device,
                expected_inode=expected_inode,
                failpoint=failpoint,
                _precommit=revalidate,
                _binding_capability=capability,
            )
    return _atomic_binding_output(classification, installed)


def bind_scope_cas(
    root: Path | str,
    name: str,
    slot_id: str,
    scope_show_raw: bytes,
    worktree_list_raw: bytes,
    safety_observation_raw: bytes,
    *,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
    failpoint: Failpoint = None,
) -> dict[str, Any]:
    """Classify, re-prove, and CAS one scope/branch/worktree atomically."""

    snapshot = inspect(root, name)
    classification = classify_scope_output(
        snapshot.document,
        slot_id,
        scope_show_raw,
        worktree_list_raw,
        safety_observation_raw,
    )
    scope = next(
        resource
        for resource in snapshot.document["resources"]
        if resource["slot_id"] == classification["slot_id"]
    )
    request = scope["request"]
    retained = _retained_result_document(scope_show_raw, "scope show output")
    record, row = _scope_show_record(
        scope_show_raw,
        request,
        scope["immutable"]["repository"],
        "scope show output",
    )
    observation = {
        "worktree_list": _retained_result_document(
            worktree_list_raw, "scope worktree list output"
        ),
        "safety_observation": _retained_result_document(
            safety_observation_raw, "scope safety observation"
        ),
    }
    worktree_request = _scope_worktree_request(
        request, scope["immutable"]["repository"]
    )
    _hit(failpoint, "scope-bind:classified")
    with _pinned_live_worktree(
        worktree_request,
        classification["path"],
        "atomic scope binding",
        allowed_references=_scope_owned_references(request),
        scope_record=record,
    ) as guard:
        def revalidate() -> None:
            _scope_binding_observation(
                observation,
                request,
                scope["immutable"]["repository"],
                row,
                retained["sha256"],
                record,
                "atomic scope binding",
                live=True,
                guard=guard,
            )

        revalidate()
        if (
            classification["classification"] == "bound-match"
            and _snapshot_matches_tuple(
                snapshot,
                expected_generation,
                expected_digest,
                expected_device,
                expected_inode,
            )
        ):
            installed = _require_clean_bound_snapshot(root, snapshot)
        else:
            if classification["classification"] == "bound-match":
                candidate = snapshot.document
                capability = _binding_recovery_capability(
                    snapshot,
                    classification,
                    scope=True,
                    expected_generation=expected_generation,
                    expected_digest=expected_digest,
                    expected_device=expected_device,
                    expected_inode=expected_inode,
                )
            else:
                candidate, capability = _initial_binding_candidate(
                    snapshot,
                    classification,
                    scope=True,
                    expected_generation=expected_generation,
                    expected_digest=expected_digest,
                    expected_device=expected_device,
                    expected_inode=expected_inode,
                )
            installed = cas(
                root,
                snapshot.name,
                candidate,
                expected_generation=expected_generation,
                expected_digest=expected_digest,
                expected_device=expected_device,
                expected_inode=expected_inode,
                failpoint=failpoint,
                _precommit=revalidate,
                _binding_capability=capability,
            )
    return _atomic_binding_output(classification, installed)


def canonical_name(document: Mapping[str, Any]) -> str:
    item = validate(document)
    mode = item["entry_mode"]
    primary = item["issues"]["explicit"][0] if mode == "issue" else item["selected_prs"][0]
    repository = primary["repository"]
    return (
        f"{repository['owner']}-{repository['name']}-{mode}-{primary['number']}"
        f"{LEDGER_SUFFIX}"
    )


def _direct_name(value: Any, context: str = "filename") -> str:
    if (
        not isinstance(value, str)
        or not value
        or value in {".", ".."}
        or Path(value).name != value
        or "/" in value
        or _contains_control(value)
    ):
        raise LedgerError(f"{context} must be one direct child name")
    return value


def _directory_fd(path: Path) -> int:
    absolute = Path(os.path.abspath(path))
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open("/", flags)
    try:
        for part in absolute.parts[1:]:
            next_descriptor = os.open(part, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = next_descriptor
        opened = os.fstat(descriptor)
        visible = os.stat(absolute, follow_symlinks=False)
        if not stat.S_ISDIR(opened.st_mode) or (opened.st_dev, opened.st_ino) != (
            visible.st_dev,
            visible.st_ino,
        ):
            raise LedgerError(f"review root is unsafe: {path}")
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def _require_trusted_directory(status: os.stat_result, context: str) -> None:
    if not stat.S_ISDIR(status.st_mode):
        raise LedgerError(f"{context} is not a directory")
    if status.st_uid != os.geteuid():
        raise LedgerError(f"{context} has a foreign owner")
    if stat.S_IMODE(status.st_mode) & 0o022:
        raise LedgerError(f"{context} is group/world writable")


def _fsync(directory: int, context: str) -> None:
    try:
        os.fsync(directory)
    except OSError as error:
        raise LedgerError(f"cannot fsync {context}: {error}") from error


def _require_names_fit(directory: int, names: Iterable[str]) -> None:
    try:
        limit = os.fpathconf(directory, "PC_NAME_MAX")
    except (OSError, ValueError) as error:
        raise LedgerError(f"cannot determine review-root NAME_MAX: {error}") from error
    for name in names:
        encoded = _direct_name(name).encode("utf-8")
        if len(encoded) > limit:
            raise LedgerError(f"derived delivery filename exceeds NAME_MAX: {name}")


def _open_lock(directory: int, name: str) -> int:
    flags = os.O_RDWR | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
    created = False
    try:
        descriptor = os.open(name, flags | os.O_CREAT | os.O_EXCL, 0o600, dir_fd=directory)
        created = True
    except FileExistsError:
        descriptor = os.open(name, flags, dir_fd=directory)
    opened = os.fstat(descriptor)
    visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
    if not stat.S_ISREG(opened.st_mode) or (opened.st_dev, opened.st_ino) != (
        visible.st_dev,
        visible.st_ino,
    ):
        os.close(descriptor)
        raise LedgerError(f"lock is unsafe: {name}")
    if (
        opened.st_uid != os.geteuid()
        or stat.S_IMODE(opened.st_mode) != 0o600
        or opened.st_nlink != 1
    ):
        os.close(descriptor)
        raise LedgerError(f"lock ownership, mode, or link count is unsafe: {name}")
    if created:
        _fsync(directory, "review root after lock creation")
    fcntl.flock(descriptor, fcntl.LOCK_EX)
    visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
    if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
        os.close(descriptor)
        raise LedgerError(f"lock was replaced: {name}")
    return descriptor


@contextmanager
def _locked_root(root: Path) -> Iterator[int]:
    try:
        directory = _directory_fd(root)
    except OSError as error:
        raise LedgerError(f"cannot open review root {root}: {error}") from error
    try:
        _require_trusted_directory(os.fstat(directory), f"review root {root}")
        # Lock the already-open no-follow directory inode itself.  Read-only
        # inventory therefore creates no persistent lock artifact.
        fcntl.flock(directory, fcntl.LOCK_EX)
        opened = os.fstat(directory)
        _require_trusted_directory(opened, f"review root {root}")
        visible = os.stat(Path(os.path.abspath(root)), follow_symlinks=False)
        if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
            raise LedgerError(f"review root was replaced while locking: {root}")
        yield directory
        visible_after = os.stat(Path(os.path.abspath(root)), follow_symlinks=False)
        _require_trusted_directory(visible_after, f"review root {root}")
        if (opened.st_dev, opened.st_ino) != (
            visible_after.st_dev,
            visible_after.st_ino,
        ):
            raise LedgerError(f"review root was replaced during operation: {root}")
    finally:
        fcntl.flock(directory, fcntl.LOCK_UN)
        os.close(directory)


@contextmanager
def _ledger_lock(directory: int, target: str) -> Iterator[None]:
    name = f".{_direct_name(target)}.lock"
    descriptor = _open_lock(directory, name)
    try:
        yield
    finally:
        fcntl.flock(descriptor, fcntl.LOCK_UN)
        os.close(descriptor)


def _managed_status(
    status: os.stat_result,
    name: str,
    expected_nlinks: set[int],
) -> None:
    if status.st_uid != os.geteuid():
        raise LedgerError(f"managed file has a foreign owner: {name}")
    if stat.S_IMODE(status.st_mode) != 0o600:
        raise LedgerError(f"managed file mode is not 0600: {name}")
    if status.st_nlink not in expected_nlinks:
        expected = "/".join(str(value) for value in sorted(expected_nlinks))
        raise LedgerError(f"managed file link count is not {expected}: {name}")


def _read_regular(
    directory: int,
    name: str,
    *,
    canonical: bool = False,
    managed: bool = False,
    expected_nlinks: set[int] | None = None,
    sync: bool = False,
) -> tuple[bytes, os.stat_result]:
    name = _direct_name(name)
    flags = (
        os.O_RDONLY
        | os.O_CLOEXEC
        | getattr(os, "O_NOFOLLOW", 0)
        | getattr(os, "O_NONBLOCK", 0)
    )
    try:
        descriptor = os.open(name, flags, dir_fd=directory)
    except OSError as error:
        raise LedgerError(f"cannot open regular file {name}: {error}") from error
    try:
        before = os.fstat(descriptor)
        visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
        if not stat.S_ISREG(before.st_mode) or (before.st_dev, before.st_ino) != (
            visible.st_dev,
            visible.st_ino,
        ):
            raise LedgerError(f"file identity is unsafe: {name}")
        if managed:
            _managed_status(before, name, expected_nlinks or {1})
        chunks: list[bytes] = []
        size = 0
        while True:
            chunk = os.read(descriptor, min(65536, MAX_BYTES + 1 - size))
            if not chunk:
                break
            chunks.append(chunk)
            size += len(chunk)
            if size > MAX_BYTES:
                raise LedgerError(f"{name} exceeds {MAX_BYTES} bytes")
        after = os.fstat(descriptor)
        visible_after = os.stat(name, dir_fd=directory, follow_symlinks=False)
        identity_before = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
        identity_after = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
        visible_identity = (
            visible_after.st_dev,
            visible_after.st_ino,
            visible_after.st_size,
            visible_after.st_mtime_ns,
        )
        if identity_before != identity_after or identity_after != visible_identity:
            raise LedgerError(f"file changed while read: {name}")
        if managed:
            _managed_status(after, name, expected_nlinks or {1})
        if sync:
            os.fsync(descriptor)
            synced = os.fstat(descriptor)
            if (synced.st_dev, synced.st_ino, synced.st_size) != (
                after.st_dev,
                after.st_ino,
                after.st_size,
            ):
                raise LedgerError(f"file changed while syncing: {name}")
        raw = b"".join(chunks)
        if canonical:
            document = _decode(raw, name)
            validate(document)
            if raw != canonical_bytes(document):
                raise LedgerError(f"ledger bytes are noncanonical: {name}")
        return raw, after
    finally:
        os.close(descriptor)


def _snapshot(directory: int, name: str) -> Snapshot:
    raw, status = _read_regular(
        directory,
        name,
        canonical=True,
        managed=True,
        expected_nlinks={1, 2},
    )
    document = _decode(raw, name)
    expected_name = canonical_name(document)
    if name != expected_name:
        raise LedgerError(f"ledger filename is noncanonical: {name}; expected {expected_name}")
    return Snapshot(name, document, raw, byte_digest(raw), status.st_dev, status.st_ino)


def _exists(directory: int, name: str) -> bool:
    try:
        status = os.stat(name, dir_fd=directory, follow_symlinks=False)
    except FileNotFoundError:
        return False
    if not stat.S_ISREG(status.st_mode):
        raise LedgerError(f"expected regular file, found unsafe entry: {name}")
    return True


def _write_exclusive(directory: int, name: str, raw: bytes) -> os.stat_result:
    flags = (
        os.O_WRONLY
        | os.O_CREAT
        | os.O_EXCL
        | os.O_CLOEXEC
        | getattr(os, "O_NOFOLLOW", 0)
    )
    descriptor = os.open(name, flags, 0o600, dir_fd=directory)
    try:
        view = memoryview(raw)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise LedgerError(f"short write while creating {name}")
            view = view[written:]
        os.fsync(descriptor)
        status = os.fstat(descriptor)
        if not stat.S_ISREG(status.st_mode):
            raise LedgerError(f"staging file is not regular: {name}")
        _managed_status(status, name, {1})
        return status
    finally:
        os.close(descriptor)


def _ensure_stage(
    directory: int,
    name: str,
    raw: bytes,
    *,
    allow_prefix_resume: bool = False,
    expected_nlinks: set[int] | None = None,
) -> os.stat_result:
    allowed_links = expected_nlinks or {1}
    try:
        status = _write_exclusive(directory, name, raw)
        _fsync(directory, f"review root after staging {name}")
        return status
    except FileExistsError:
        existing, status = _read_regular(
            directory,
            name,
            managed=True,
            expected_nlinks=allowed_links,
        )
        if existing != raw and not (
            allow_prefix_resume and len(existing) < len(raw) and raw.startswith(existing)
        ):
            raise LedgerError(f"staging content mismatch: {name}")
        if existing != raw:
            flags = os.O_WRONLY | os.O_APPEND | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
            descriptor = os.open(name, flags, dir_fd=directory)
            try:
                opened = os.fstat(descriptor)
                if (opened.st_dev, opened.st_ino) != (status.st_dev, status.st_ino):
                    raise LedgerError(f"staging file was replaced: {name}")
                _managed_status(opened, name, {1})
                view = memoryview(raw)[len(existing) :]
                while view:
                    written = os.write(descriptor, view)
                    if written <= 0:
                        raise LedgerError(f"short write while resuming {name}")
                    view = view[written:]
                os.fsync(descriptor)
            finally:
                os.close(descriptor)
        exact, exact_status = _read_regular(
            directory,
            name,
            managed=True,
            expected_nlinks=allowed_links,
            sync=True,
        )
        if exact != raw:
            raise LedgerError(f"staging content mismatch after resume: {name}")
        _fsync(directory, f"review root after reusing staging {name}")
        return exact_status


def _unlink_exact(directory: int, name: str, expected: os.stat_result) -> None:
    visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
    if (visible.st_dev, visible.st_ino) != (expected.st_dev, expected.st_ino):
        raise LedgerError(f"staging file was replaced: {name}")
    os.unlink(name, dir_fd=directory)
    _fsync(directory, f"review root after removing {name}")


def _discard_exact_cas_pending(
    directory: int, stage: str, proof: str, raw: bytes
) -> bool:
    """Remove only the exact uninstalled stage/proof hard-link pair."""

    try:
        stage_raw, stage_status = _read_regular(
            directory, stage, managed=True, expected_nlinks={2}
        )
        proof_raw, proof_status = _read_regular(
            directory, proof, managed=True, expected_nlinks={2}
        )
        if (
            stage_raw != raw
            or proof_raw != raw
            or (stage_status.st_dev, stage_status.st_ino)
            != (proof_status.st_dev, proof_status.st_ino)
        ):
            return False
        _unlink_exact(directory, proof, proof_status)
        stage_raw, stage_status = _read_regular(
            directory, stage, managed=True, expected_nlinks={1}
        )
        if stage_raw != raw:
            return False
        _unlink_exact(directory, stage, stage_status)
        return True
    except LedgerError:
        return False


def _reserved_worktree_paths(document: Mapping[str, Any]) -> set[str]:
    """Return every bound or precommitted managed worktree path."""

    paths: set[str] = set()
    for slot in document["artifacts"]:
        if slot["kind"] != "worktree":
            continue
        path = slot["immutable"]["path"]
        if path is None and slot["current"] is not None:
            path = slot["current"]["path"]
        if path is not None:
            paths.add(path.casefold())
        request = slot.get("primitive_request")
        if request is not None:
            paths.add(_expected_worktree_path(request).casefold())
    for resource in document["resources"]:
        if resource["kind"] == "scope":
            paths.add(_expected_worktree_path(resource["request"]).casefold())
    return paths


def _conflict_keys(snapshot: Snapshot) -> Iterator[tuple[str, tuple[Any, ...]]]:
    document = snapshot.document
    if document["program"] is not None:
        master = document["program"]["master_issue"]
        yield "program/master-position", (
            master["repository"]["owner"].casefold(),
            master["repository"]["name"].casefold(),
            master["number"],
            document["program"]["leaf_position"],
        )
    for issue in document["issues"]["explicit"]:
        repository = issue["repository"]
        yield "issue", (
            repository["owner"].casefold(),
            repository["name"].casefold(),
            issue["number"],
        )
    for pull in document["selected_prs"]:
        repository = pull["repository"]
        yield "pr", (
            repository["owner"].casefold(),
            repository["name"].casefold(),
            pull["number"],
        )
    for slot in document["artifacts"]:
        if slot["kind"] != "pull_request":
            continue
        identity = slot["immutable"]
        if identity["number"] is None:
            continue
        repository = identity["repository"]
        yield "pr", (
            repository["owner"].casefold(),
            repository["name"].casefold(),
            identity["number"],
        )
    for target in document["targets"]:
        repository = target["repository"]
        yield "repository/head", (
            repository["owner"].casefold(),
            repository["name"].casefold(),
            target["head"]["branch"].casefold(),
        )
    for path in sorted(_reserved_worktree_paths(document)):
        yield "worktree", (path,)
        managed_coordinate = _managed_worktree_coordinate(path)
        if managed_coordinate is not None:
            yield "worktree-request", managed_coordinate
    for slot in document["artifacts"]:
        if slot["kind"] == "worktree":
            request = slot["primitive_request"]
            if request is not None:
                yield "worktree-request", (
                    request["roots"]["workspace"]["path"].casefold(),
                    request["physical_checkout"].casefold(),
                    request["label"].casefold(),
                )
    for slot in document["resources"]:
        identity = slot["immutable"]
        repository = identity["repository"]
        yield "resource/name", (identity["name"].casefold(),)
        if identity["path"] is not None:
            yield "resource/path", (identity["path"].casefold(),)
        if slot["kind"] == "scope":
            request = slot["request"]
            yield "worktree-request", (
                request["roots"]["workspace"]["path"].casefold(),
                request["physical_checkout"].casefold(),
                request["label"].casefold(),
            )


def _reject_overlaps(snapshots: Sequence[Snapshot], *, allow_name: str | None = None) -> None:
    owners: dict[tuple[str, tuple[Any, ...]], Snapshot] = {}
    ledger_ids: dict[str, Snapshot] = {}
    repository_coordinates: dict[tuple[str, str], str] = {}
    repository_nodes: dict[str, tuple[str, str]] = {}
    issue_coordinates: dict[tuple[str, int], str] = {}
    issue_nodes: dict[str, tuple[str, int]] = {}
    pr_coordinates: dict[tuple[str, int], str] = {}
    pr_nodes: dict[str, tuple[str, int]] = {}
    direct_issue_owners: dict[tuple[str, str, int], Snapshot] = {}
    program_master_owners: dict[
        tuple[str, str, int], tuple[tuple[Any, ...], int, Snapshot]
    ] = {}

    def remember_repository(value: Mapping[str, Any]) -> None:
        coordinate = (value["owner"].casefold(), value["name"].casefold())
        node = value["node_id"]
        if repository_coordinates.get(coordinate, node) != node:
            raise LedgerError("repository coordinate has conflicting node IDs across ledgers")
        if repository_nodes.get(node, coordinate) != coordinate:
            raise LedgerError("repository node ID aliases coordinates across ledgers")
        repository_coordinates[coordinate] = node
        repository_nodes[node] = coordinate

    def remember_issue(value: Mapping[str, Any]) -> None:
        remember_repository(value["repository"])
        coordinate = (value["repository"]["node_id"], value["number"])
        node = value["node_id"]
        if issue_coordinates.get(coordinate, node) != node:
            raise LedgerError("issue coordinate has conflicting node IDs across ledgers")
        if issue_nodes.get(node, coordinate) != coordinate:
            raise LedgerError("issue node ID aliases coordinates across ledgers")
        issue_coordinates[coordinate] = node
        issue_nodes[node] = coordinate

    def remember_pr(identity: Mapping[str, Any]) -> None:
        remember_repository(identity["repository"])
        if identity["number"] is None:
            return
        coordinate = (identity["repository"]["node_id"], identity["number"])
        node = identity["node_id"]
        if pr_coordinates.get(coordinate, node) != node:
            raise LedgerError("PR coordinate has conflicting node IDs across ledgers")
        if pr_nodes.get(node, coordinate) != coordinate:
            raise LedgerError("PR node ID aliases coordinates across ledgers")
        pr_coordinates[coordinate] = node
        pr_nodes[node] = coordinate

    for snapshot in snapshots:
        document = snapshot.document
        for target in document["targets"]:
            remember_repository(target["repository"])
        program_issues = [] if document["program"] is None else [
            document["program"]["master_issue"],
            document["program"]["leaf_issue"],
        ]
        for issue in [
            *document["issues"]["explicit"],
            *document["issues"]["incidental"],
            *document["closing_scope"],
            *program_issues,
        ]:
            remember_issue(issue)
        for pull in document["selected_prs"]:
            remember_repository(pull["head_repository"])
            remember_pr(pull)
        for slot in document["artifacts"]:
            remember_repository(slot["immutable"]["repository"])
            if slot["kind"] == "pull_request":
                remember_pr(slot["immutable"])
                if slot["current"] is not None:
                    remember_pr(slot["current"])
        for resource in document["resources"]:
            remember_repository(resource["immutable"]["repository"])
        for issue in document["issues"]["explicit"]:
            coordinate = (
                issue["repository"]["owner"].casefold(),
                issue["repository"]["name"].casefold(),
                issue["number"],
            )
            reserved = program_master_owners.get(coordinate)
            if reserved is not None and reserved[2].name != snapshot.name:
                raise LedgerError(
                    "direct issue ownership overlaps a program master reservation"
                )
            prior_direct = direct_issue_owners.get(coordinate)
            if prior_direct is None or prior_direct.name == snapshot.name:
                direct_issue_owners[coordinate] = snapshot
        if document["program"] is not None:
            master = document["program"]["master_issue"]
            coordinate = (
                master["repository"]["owner"].casefold(),
                master["repository"]["name"].casefold(),
                master["number"],
            )
            prior_direct = direct_issue_owners.get(coordinate)
            if prior_direct is not None and prior_direct.name != snapshot.name:
                raise LedgerError(
                    "program master reservation overlaps direct issue ownership"
                )
            authority = document["authority"]
            position = document["program"]["leaf_position"]
            family = (
                master["repository"]["node_id"],
                master["node_id"],
                authority["kind"],
                authority["reference"],
                authority["objective_sha256"],
                _timestamp_key(authority["issued_at"], "program authority issued_at"),
                authority["actor_node_id"],
            )
            prior_program = program_master_owners.get(coordinate)
            if (
                prior_program is not None
                and prior_program[2].name != snapshot.name
                and prior_program[1] != position
                and prior_program[0] != family
            ):
                raise LedgerError(
                    "program master leaves have conflicting authority families"
                )
            if prior_program is None or prior_program[2].name == snapshot.name:
                program_master_owners[coordinate] = (family, position, snapshot)
        prior_id = ledger_ids.get(snapshot.document["ledger_id"])
        if prior_id is not None and prior_id.name != snapshot.name:
            raise LedgerError(
                f"ledger identity is duplicated by {prior_id.name} and {snapshot.name}"
            )
        ledger_ids[snapshot.document["ledger_id"]] = snapshot
        for kind, key in _conflict_keys(snapshot):
            prior = owners.get((kind, key))
            if prior is not None and prior.name != snapshot.name:
                # An operation may present the exact already-installed target twice
                # (committed plus its crash-recovery staging inode), but no other
                # cross-ledger overlap is resumable.
                if allow_name is not None and {prior.name, snapshot.name} == {allow_name}:
                    continue
                raise LedgerError(
                    f"{kind} ownership overlap between {prior.name} and {snapshot.name}"
                )
            owners[(kind, key)] = snapshot


def _marker_document(raw: bytes, name: str) -> dict[str, Any]:
    value = _decode(raw, name)
    item = _exact(
        value,
        {
            "transaction",
            "state",
            "kind",
            "candidate_digest",
            "source",
            "snapshot_name",
            "snapshot",
            "canonical_report",
            "destination",
            "destination_digest",
            "staging",
        },
        "migration marker",
    )
    if item["transaction"] != "delivery-ledger-migration-v1":
        raise LedgerError("migration marker transaction is invalid")
    if item["state"] not in {"planned", "prepared", "complete"}:
        raise LedgerError("migration marker state is invalid")
    if item["kind"] not in MIGRATION_KINDS:
        raise LedgerError("migration marker kind is invalid")
    _string(item["candidate_digest"], "migration marker candidate digest", SHA256_RE)
    _source(item["source"], "migration marker source")
    snapshot_name = _direct_name(item["snapshot_name"], "migration marker snapshot name")
    if not _SNAPSHOT_RE.fullmatch(snapshot_name):
        raise LedgerError("migration marker snapshot name is invalid")
    if item["state"] == "planned":
        if item["snapshot"] is not None or item["destination_digest"] is not None:
            raise LedgerError("planned migration marker contains prepared identity")
    else:
        snapshot = _source(item["snapshot"], "migration marker snapshot")
        if snapshot[0] != snapshot_name:
            raise LedgerError("migration marker snapshot identity/name mismatch")
        _string(item["destination_digest"], "migration marker digest", SHA256_RE)
    canonical_report = _direct_name(
        item["canonical_report"], "migration marker canonical report"
    )
    if not canonical_report.endswith(".md"):
        raise LedgerError("migration marker canonical report is invalid")
    destination = _direct_name(item["destination"], "migration marker destination")
    if not destination.endswith(LEDGER_SUFFIX):
        raise LedgerError("migration marker destination is invalid")
    staging = _direct_name(item["staging"], "migration marker staging")
    if staging != f".{destination}.migrate.tmp":
        raise LedgerError("migration marker staging name is invalid")
    if raw != canonical_bytes(item):
        raise LedgerError("migration marker bytes are noncanonical")
    return item


def _legacy_claim(name: str, raw: bytes, canonical_target: str | None) -> LegacyClaim:
    try:
        text = raw.decode("utf-8")
    except UnicodeError as error:
        raise LedgerError(f"legacy report is not UTF-8: {name}: {error}") from error
    issues = {
        (match.group("owner").casefold(), match.group("repo").casefold(), int(match.group("number")))
        for match in _ISSUE_URL_RE.finditer(text)
    }
    pulls = {
        (match.group("owner").casefold(), match.group("repo").casefold(), int(match.group("number")))
        for match in _PR_URL_RE.finditer(text)
    }
    repository_heads: set[tuple[str, str, str]] = set()
    worktrees: set[str] = set()
    for line in text.splitlines():
        if not line.lstrip().startswith("|"):
            continue
        cells = [cell.strip().strip("`") for cell in line.strip().strip("|").split("|")]
        if len(cells) < 5 or "@" not in cells[0] or " / " not in cells[2]:
            continue
        repository_text = cells[0].split("@", 1)[0]
        if "/" not in repository_text:
            continue
        owner, repository_name = repository_text.split("/", 1)
        branch = cells[2].split(" / ", 1)[0].strip().strip("`")
        try:
            _string(owner.casefold(), "legacy owner", OWNER_RE)
            _string(repository_name.casefold(), "legacy repository", REPOSITORY_RE)
            _branch(branch, "legacy head branch")
        except LedgerError:
            continue
        repository_heads.add((owner.casefold(), repository_name.casefold(), branch.casefold()))
        worktree = cells[4].strip().strip("`")
        if worktree.startswith("/") and worktree != "/" and os.path.normpath(worktree) == worktree:
            worktrees.add(worktree.casefold())
    filename_claim = bool(
        _CANONICAL_REPORT_RE.fullmatch(name) or _LEGACY_REPORT_RE.fullmatch(name)
    )
    ambiguous = not any(
        (filename_claim, canonical_target, issues, pulls, repository_heads, worktrees)
    )
    return LegacyClaim(
        name=name,
        canonical_target=canonical_target,
        digest=byte_digest(raw),
        issues=tuple(sorted(issues)),
        pull_requests=tuple(sorted(pulls)),
        repository_heads=tuple(sorted(repository_heads)),
        worktrees=tuple(sorted(worktrees)),
        ambiguous=ambiguous,
    )


def _inventory_locked(directory: int) -> Inventory:
    names: list[str] = []
    with os.scandir(directory) as entries:
        for entry in entries:
            names.append(entry.name)
            if len(names) > MAX_INVENTORY_ENTRIES:
                raise LedgerError(
                    f"review inventory exceeds {MAX_INVENTORY_ENTRIES} directory entries"
                )
    names.sort(key=str.casefold)
    name_set = set(names)
    case_names: dict[str, str] = {}
    ledgers: list[Snapshot] = []
    committed: list[Snapshot] = []
    pending: list[PendingOperation] = []
    markers: dict[str, dict[str, Any]] = {}
    plan_stages: dict[
        str, tuple[str, str, dict[str, Any] | None, bytes, os.stat_result]
    ] = {}
    completion_stages: dict[str, dict[str, Any] | None] = {}
    preparation_stages: dict[str, dict[str, Any] | None] = {}
    snapshots: dict[str, tuple[str, bytes, os.stat_result]] = {}
    legacy_reports: list[LegacyClaim] = []
    managed_stats: dict[str, os.stat_result] = {}
    inventory_bytes = 0
    for name in names:
        folded = name.casefold()
        if folded.endswith(LEDGER_SUFFIX) and not name.endswith(LEDGER_SUFFIX):
            raise LedgerError(f"ledger filename has a case alias: {name}")
        canonical_report_match = _CANONICAL_REPORT_RE.fullmatch(folded)
        legacy_report_match = _LEGACY_REPORT_RE.fullmatch(folded)
        if (canonical_report_match or legacy_report_match) and name != folded:
            raise LedgerError(f"report filename has a case alias: {name}")
        report_candidate = bool(canonical_report_match or legacy_report_match)
        relevant = (
            folded.endswith(LEDGER_SUFFIX)
            or ".md.ledger.json." in folded
            or report_candidate
        )
        if relevant:
            prior = case_names.get(folded)
            if prior is not None and prior != name:
                raise LedgerError(f"case-alias entries collide: {prior} and {name}")
            case_names[folded] = name
            status = os.stat(name, dir_fd=directory, follow_symlinks=False)
            if not stat.S_ISREG(status.st_mode):
                raise LedgerError(f"delivery entry is not a regular file: {name}")
            inventory_bytes += status.st_size
            if inventory_bytes > MAX_INVENTORY_BYTES:
                raise LedgerError(
                    f"delivery inventory exceeds {MAX_INVENTORY_BYTES} bytes"
                )
        if report_candidate:
            paired_target = f"{name}.ledger.json" if canonical_report_match else None
            paired = paired_target is not None and (
                paired_target in name_set
                or f".{paired_target}.migration.json" in name_set
            )
            if not paired:
                report_raw, _ = _read_regular(directory, name)
                legacy_reports.append(_legacy_claim(name, report_raw, paired_target))
            continue
        if name.endswith(LEDGER_SUFFIX):
            snapshot = _snapshot(directory, name)
            managed_stats[name] = os.stat(name, dir_fd=directory, follow_symlinks=False)
            ledgers.append(snapshot)
            committed.append(snapshot)
            continue
        marker_match = _MIGRATION_RE.fullmatch(name)
        if marker_match:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1, 2}
            )
            managed_stats[name] = status
            marker = _marker_document(raw, name)
            if name != f".{marker['destination']}.migration.json":
                raise LedgerError(f"migration marker filename is noncanonical: {name}")
            markers[marker["destination"]] = marker
            if marker["state"] != "complete":
                pending.append(PendingOperation("migration", marker["destination"], name))
            continue
        plan_match = _MARKER_PLAN_STAGE_RE.fullmatch(name)
        if plan_match:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1, 2}
            )
            managed_stats[name] = status
            marker_name = plan_match.group("marker")
            operation_digest = plan_match.group("operation")
            marker_name_match = _MIGRATION_RE.fullmatch(marker_name)
            if marker_name_match is None:
                raise LedgerError(f"migration plan staging name is invalid: {name}")
            target = marker_name_match.group("target")
            try:
                marker = _marker_document(raw, name)
            except LedgerError:
                marker = None
            if marker is not None and (
                marker["state"] != "planned"
                or marker_name != f".{marker['destination']}.migration.json"
                or marker["destination"] != target
                or byte_digest(raw) != operation_digest
            ):
                raise LedgerError(f"migration plan staging target mismatch: {name}")
            if marker_name in plan_stages:
                raise LedgerError(
                    f"multiple migration plan candidates exist for {target}"
                )
            plan_stages[marker_name] = (
                name,
                operation_digest,
                marker,
                raw,
                status,
            )
            pending.append(PendingOperation("migration-plan", target, name))
            continue
        preparation_match = _MARKER_PREPARE_STAGE_RE.fullmatch(name)
        if preparation_match:
            raw, status = _read_regular(directory, name, managed=True)
            managed_stats[name] = status
            expected_marker = preparation_match.group("marker")
            marker_match = _MIGRATION_RE.fullmatch(expected_marker)
            target = marker_match.group("target")
            try:
                marker = _marker_document(raw, name)
            except LedgerError:
                marker = None
            if marker is not None and (
                marker["state"] != "prepared"
                or expected_marker != f".{marker['destination']}.migration.json"
            ):
                raise LedgerError(f"migration preparation target mismatch: {name}")
            preparation_stages[expected_marker] = marker
            pending.append(
                PendingOperation("migration-prepare", target, name)
            )
            continue
        completion_match = _MARKER_COMPLETE_STAGE_RE.fullmatch(name)
        if completion_match:
            raw, status = _read_regular(directory, name, managed=True)
            managed_stats[name] = status
            expected_marker = completion_match.group("marker")
            marker_match = _MIGRATION_RE.fullmatch(expected_marker)
            target = marker_match.group("target")
            try:
                marker = _marker_document(raw, name)
            except LedgerError:
                marker = None
            if marker is not None and (
                marker["state"] != "complete"
                or expected_marker != f".{marker['destination']}.migration.json"
            ):
                raise LedgerError(f"migration completion target mismatch: {name}")
            completion_stages[expected_marker] = marker
            pending.append(
                PendingOperation("migration-complete", target, name)
            )
            continue
        snapshot_match = _SNAPSHOT_RE.fullmatch(name)
        if snapshot_match:
            raw, status = _read_regular(directory, name, managed=True)
            managed_stats[name] = status
            snapshots[snapshot_match.group("target")] = (name, raw, status)
            pending.append(
                PendingOperation("migration-snapshot", snapshot_match.group("target"), name)
            )
            continue
        if _LEDGER_LOCK_RE.fullmatch(name):
            status = os.stat(name, dir_fd=directory, follow_symlinks=False)
            if not stat.S_ISREG(status.st_mode):
                raise LedgerError(f"delivery lock is not regular: {name}")
            _managed_status(status, name, {1})
            managed_stats[name] = status
            continue
        receipt_match = _UPDATE_RECEIPT_RE.fullmatch(name)
        if receipt_match:
            raw, status = _read_regular(
                directory,
                name,
                managed=True,
                expected_nlinks={1, 2},
            )
            managed_stats[name] = status
            target = receipt_match.group("target")
            pending.append(PendingOperation("update-proof", target, name))
            try:
                document = _decode(raw, name)
                validate(document)
                if raw != canonical_bytes(document):
                    raise LedgerError(f"ledger bytes are noncanonical: {name}")
            except LedgerError:
                # The candidate-bound proof never permits prefix repair.  A
                # malformed proof is retained as blocking evidence.
                continue
            if (
                canonical_name(document) != target
                or document["generation"] != int(receipt_match.group("generation")) + 1
                or document["previous_byte_digest"] != receipt_match.group("digest")
                or byte_digest(raw) != receipt_match.group("candidate")
            ):
                raise LedgerError(f"update proof identity is invalid: {name}")
            ledgers.append(
                Snapshot(
                    target,
                    document,
                    raw,
                    byte_digest(raw),
                    status.st_dev,
                    status.st_ino,
                )
            )
            continue
        for kind, pattern in (
            ("create", _CREATE_STAGE_RE),
            ("update", _UPDATE_STAGE_RE),
            ("migration", _MIGRATE_STAGE_RE),
        ):
            match = pattern.fullmatch(name)
            if match:
                expected_links = {1, 2} if kind in {"update", "create", "migration"} else {1}
                raw, status = _read_regular(
                    directory,
                    name,
                    managed=True,
                    expected_nlinks=expected_links,
                )
                managed_stats[name] = status
                pending.append(PendingOperation(kind, match.group("target"), name))
                try:
                    document = _decode(raw, name)
                    validate(document)
                    if raw != canonical_bytes(document):
                        raise LedgerError(f"ledger bytes are noncanonical: {name}")
                except LedgerError:
                    # A deterministic short-write prefix is resumable only by
                    # the exact operation whose candidate bytes contain it.
                    break
                if canonical_name(document) != match.group("target"):
                    raise LedgerError(f"staging target mismatch: {name}")
                if kind == "create" and (
                    document["generation"] != 1
                    or byte_digest(raw) != match.group("candidate")
                ):
                    raise LedgerError(f"create staging identity is invalid: {name}")
                if kind == "update" and (
                    document["generation"] != int(match.group("generation"))
                    or document["previous_byte_digest"] is None
                    or document["previous_byte_digest"] != match.group("digest")
                    or byte_digest(raw) != match.group("candidate")
                ):
                    raise LedgerError(f"update staging CAS identity mismatch: {name}")
                if kind == "migration" and document["migration"] is None:
                    raise LedgerError(f"migration staging lacks metadata: {name}")
                ledgers.append(
                    Snapshot(
                        match.group("target"),
                        document,
                        raw,
                        byte_digest(raw),
                        status.st_dev,
                        status.st_ino,
                    )
                )
                break
        else:
            if relevant:
                raise LedgerError(f"unexpected delivery helper entry: {name}")
    for operation in pending:
        if (
            _MIGRATE_STAGE_RE.fullmatch(operation.staging) is not None
            and operation.target not in markers
        ):
            raise LedgerError(f"orphaned migration destination stage: {operation.staging}")
    inode_names: dict[tuple[int, int], list[str]] = {}
    for managed_name, status in managed_stats.items():
        inode_names.setdefault((status.st_dev, status.st_ino), []).append(managed_name)
    for managed_name, status in managed_stats.items():
        if status.st_nlink == 1:
            continue
        linked_names = sorted(inode_names[(status.st_dev, status.st_ino)])
        allowed_pair = False
        if len(linked_names) == 2:
            first, second = linked_names
            for pattern in (_CREATE_STAGE_RE, _MIGRATE_STAGE_RE):
                match = pattern.fullmatch(first)
                if match is not None and second == match.group("target"):
                    allowed_pair = True
                match = pattern.fullmatch(second)
                if match is not None and first == match.group("target"):
                    allowed_pair = True
            plan_name = next(
                (value for value in linked_names if _MARKER_PLAN_STAGE_RE.fullmatch(value)),
                None,
            )
            if plan_name is not None:
                plan_match = _MARKER_PLAN_STAGE_RE.fullmatch(plan_name)
                other = linked_names[0] if linked_names[1] == plan_name else linked_names[1]
                allowed_pair = other == plan_match.group("marker")
            proof_name = next(
                (value for value in linked_names if _UPDATE_RECEIPT_RE.fullmatch(value)),
                None,
            )
            if proof_name is not None:
                proof_match = _UPDATE_RECEIPT_RE.fullmatch(proof_name)
                other = linked_names[0] if linked_names[1] == proof_name else linked_names[1]
                stage_match = _UPDATE_STAGE_RE.fullmatch(other)
                allowed_pair = other == proof_match.group("target") or (
                    stage_match is not None
                    and stage_match.group("target") == proof_match.group("target")
                    and stage_match.group("generation")
                    == str(int(proof_match.group("generation")) + 1)
                    and stage_match.group("digest") == proof_match.group("digest")
                    and stage_match.group("candidate")
                    == proof_match.group("candidate")
                    and stage_match.group("operation")
                    == proof_match.group("operation")
                )
        if not allowed_pair:
            raise LedgerError(f"managed file has an unmodeled hard link: {managed_name}")
    for marker_name, (
        _plan_name,
        operation_digest,
        planned,
        planned_raw,
        planned_status,
    ) in plan_stages.items():
        destination = _MIGRATION_RE.fullmatch(marker_name).group("target")
        installed = markers.get(destination)
        if installed is None:
            continue
        marker_status = managed_stats[marker_name]
        if (
            planned is None
            or installed["state"] != "planned"
            or byte_digest(canonical_bytes(installed)) != operation_digest
            or planned != installed
            or planned_raw != canonical_bytes(installed)
            or (planned_status.st_dev, planned_status.st_ino)
            != (marker_status.st_dev, marker_status.st_ino)
        ):
            raise LedgerError(f"migration plan staging/install mismatch: {destination}")
    for destination, marker in markers.items():
        source_name, source_digest, source_device, source_inode = _source(
            marker["source"], "migration marker source"
        )
        snapshot_name = marker["snapshot_name"]
        snapshot_entry = snapshots.get(destination)
        source_raw, source_status = _read_regular(directory, source_name)
        canonical_report = marker["canonical_report"]
        if marker["kind"] == "legacy" or marker["state"] != "complete":
            if (
            byte_digest(source_raw) != source_digest
            or source_status.st_dev != source_device
            or source_status.st_ino != source_inode
            ):
                raise LedgerError(f"migration source changed: {source_name}")
        snapshot_raw: bytes | None = None
        if snapshot_entry is not None:
            if snapshot_entry[0] != snapshot_name:
                raise LedgerError(f"migration snapshot name mismatch: {snapshot_name}")
            snapshot_raw, snapshot_status = snapshot_entry[1:]
            if byte_digest(snapshot_raw) != source_digest and not (
                marker["state"] == "planned"
                and len(snapshot_raw) < len(source_raw)
                and source_raw.startswith(snapshot_raw)
            ):
                raise LedgerError(f"migration snapshot digest mismatch: {snapshot_name}")
        if marker["state"] != "planned":
            if snapshot_raw is None:
                raise LedgerError(f"migration snapshot disappeared: {snapshot_name}")
            _, snapshot_digest, snapshot_device, snapshot_inode = _source(
                marker["snapshot"], "migration marker snapshot"
            )
            if (
                byte_digest(snapshot_raw) != snapshot_digest
                or snapshot_status.st_dev != snapshot_device
                or snapshot_status.st_ino != snapshot_inode
            ):
                raise LedgerError(f"migration snapshot changed: {snapshot_name}")
        try:
            canonical_raw, _ = _read_regular(directory, canonical_report)
        except LedgerError:
            if marker["state"] == "planned" and marker["kind"] == "legacy":
                canonical_raw = None
            else:
                raise
        if canonical_raw is not None and marker["kind"] == "legacy" and marker["state"] != "complete":
            if canonical_raw != source_raw and not (
                marker["state"] == "planned"
                and len(canonical_raw) < len(source_raw)
                and source_raw.startswith(canonical_raw)
            ):
                raise LedgerError(f"legacy canonical report changed: {canonical_report}")
        if (
            canonical_raw is not None
            and marker["kind"] == "pre-schema"
            and marker["state"] != "complete"
            and canonical_raw != source_raw
        ):
            raise LedgerError(f"pre-schema report changed before completion: {canonical_report}")
        destination_snapshots = [item for item in ledgers if item.name == destination]
        if marker["state"] == "complete" and not destination_snapshots:
            raise LedgerError(f"completed migration lost canonical ledger: {destination}")
        if marker["state"] == "planned" and destination_snapshots:
            raise LedgerError(f"planned migration unexpectedly has a destination: {destination}")
        if marker["state"] != "planned":
            anchor = marker["destination_digest"]
            for destination_snapshot in destination_snapshots:
                if destination_snapshot.document["generation"] == 1:
                    if destination_snapshot.digest != anchor:
                        raise LedgerError(f"migration destination digest mismatch: {destination}")
                elif not destination_snapshot.document["history"] or destination_snapshot.document["history"][0] != anchor:
                    raise LedgerError(f"migration destination history lost its anchor: {destination}")
        marker_name = f".{destination}.migration.json"
        if marker_name in preparation_stages:
            preparation = preparation_stages[marker_name]
            if marker["state"] != "planned" or (
                preparation is not None
                and {
                    **preparation,
                    "state": "planned",
                    "snapshot": None,
                    "destination_digest": None,
                }
                != marker
            ):
                raise LedgerError(f"migration preparation stage mismatch: {destination}")
        if marker_name in completion_stages:
            completion = completion_stages[marker_name]
            if marker["state"] != "prepared" or (
                completion is not None
                and completion != {**marker, "state": "complete"}
            ):
                raise LedgerError(f"migration completion stage mismatch: {destination}")
    for marker_name in preparation_stages:
        if marker_name not in {f".{destination}.migration.json" for destination in markers}:
            raise LedgerError(f"orphaned migration preparation stage: {marker_name}")
    for marker_name in completion_stages:
        if marker_name not in {
            f".{destination}.migration.json" for destination in markers
        }:
            raise LedgerError(f"orphaned migration completion stage: {marker_name}")
    for destination in snapshots:
        if destination not in markers:
            raise LedgerError(f"orphaned migration snapshot: {snapshots[destination][0]}")
    for snapshot in committed:
        migration = snapshot.document["migration"]
        if migration is None:
            continue
        marker = markers.get(snapshot.name)
        if marker is None:
            raise LedgerError(f"migrated ledger lost marker: {snapshot.name}")
        if marker["state"] not in {"prepared", "complete"}:
            raise LedgerError(f"migrated ledger candidate identity mismatch: {snapshot.name}")
        if snapshot.document["generation"] == 1 and marker[
            "candidate_digest"
        ] != byte_digest(canonical_bytes({**snapshot.document, "migration": None})):
            raise LedgerError(f"migrated ledger candidate digest mismatch: {snapshot.name}")
        if (
            marker["kind"] != migration["kind"]
            or marker["source"] != migration["source"]
            or marker["snapshot"] != migration["snapshot"]
            or marker["canonical_report"] != migration["canonical_report"]
            or migration["marker_name"] != f".{snapshot.name}.migration.json"
        ):
            raise LedgerError(f"migrated ledger metadata mismatch: {snapshot.name}")
    managed_sources = {marker["source"]["name"] for marker in markers.values()}
    legacy_reports = [
        claim for claim in legacy_reports if claim.name not in managed_sources
    ]
    unique: dict[tuple[str, str], Snapshot] = {}
    for snapshot in ledgers:
        unique[(snapshot.name, snapshot.digest)] = snapshot
    result = list(unique.values())
    _reject_overlaps(result)
    committed_keys = {(item.name, item.digest) for item in committed}
    committed_result = tuple(
        item for item in result if (item.name, item.digest) in committed_keys
    )
    pending = [
        item
        for item in pending
        if not (
            item.kind == "migration-snapshot"
            and item.target in markers
            and markers[item.target]["state"] == "complete"
        )
    ]
    return Inventory(
        committed_result,
        tuple(sorted(pending, key=lambda item: (item.target, item.kind))),
        tuple(sorted(legacy_reports, key=lambda item: item.name)),
    )


def _require_exact_pending(
    current: Inventory, allowed: set[tuple[str, str, str]]
) -> None:
    unexpected = [
        item
        for item in current.pending
        if (item.kind, item.target, item.staging) not in allowed
    ]
    if unexpected:
        rendered = ", ".join(item.staging for item in unexpected)
        raise LedgerError(f"unrelated or ambiguous pending delivery operation: {rendered}")


def _reject_legacy_claims(
    current: Inventory,
    candidate: Mapping[str, Any],
    *,
    allowed_source: str | None = None,
) -> None:
    issues = {
        (
            value["repository"]["owner"].casefold(),
            value["repository"]["name"].casefold(),
            value["number"],
        )
        for value in candidate["issues"]["explicit"]
    }
    if candidate["program"] is not None:
        master = candidate["program"]["master_issue"]
        issues.add(
            (
                master["repository"]["owner"].casefold(),
                master["repository"]["name"].casefold(),
                master["number"],
            )
        )
    pulls = {
        (
            value["repository"]["owner"].casefold(),
            value["repository"]["name"].casefold(),
            value["number"],
        )
        for value in candidate["selected_prs"]
    }
    pulls.update(
        (
            slot["immutable"]["repository"]["owner"].casefold(),
            slot["immutable"]["repository"]["name"].casefold(),
            slot["immutable"]["number"],
        )
        for slot in candidate["artifacts"]
        if slot["kind"] == "pull_request"
        and slot["immutable"]["number"] is not None
    )
    reserved_report_names = {
        f"{owner}-{repository}-issue-{number}.md"
        for owner, repository, number in issues
    }
    reserved_report_names.update(
        f"{owner}-{repository}-{number}.md"
        for owner, repository, number in issues
    )
    reserved_report_names.update(
        f"{owner}-{repository}-pr-{number}.md"
        for owner, repository, number in pulls
    )
    repository_heads = {
        (
            value["repository"]["owner"].casefold(),
            value["repository"]["name"].casefold(),
            value["head"]["branch"].casefold(),
        )
        for value in candidate["targets"]
    }
    worktrees = _reserved_worktree_paths(candidate)
    target = canonical_name(candidate)
    for claim in current.legacy_reports:
        if claim.name == allowed_source:
            continue
        if claim.ambiguous:
            raise LedgerError(f"ambiguous unpaired legacy report blocks mutation: {claim.name}")
        if (
            claim.canonical_target == target
            or claim.name in reserved_report_names
            or issues.intersection(claim.issues)
            or pulls.intersection(claim.pull_requests)
            or repository_heads.intersection(claim.repository_heads)
            or worktrees.intersection(claim.worktrees)
        ):
            raise LedgerError(f"legacy report ownership overlaps delivery: {claim.name}")


def _legacy_source_name(candidate: Mapping[str, Any]) -> str:
    selected = candidate["issues"]["explicit"]
    if candidate["entry_mode"] != "issue" or len(selected) != 1:
        raise LedgerError("legacy migration requires one exact issue-mode identity")
    repository = selected[0]["repository"]
    return f"{repository['owner']}-{repository['name']}-{selected[0]['number']}.md"


def _require_exact_legacy_claim(
    source_name: str, source_raw: bytes, candidate: Mapping[str, Any]
) -> None:
    expected_name = _legacy_source_name(candidate)
    if source_name != expected_name:
        raise LedgerError(f"legacy migration source must be exactly {expected_name}")
    claim = _legacy_claim(source_name, source_raw, None)
    expected_issues = {
        (
            issue["repository"]["owner"].casefold(),
            issue["repository"]["name"].casefold(),
            issue["number"],
        )
        for issue in [
            *candidate["issues"]["explicit"],
            *candidate["issues"]["incidental"],
        ]
    }
    expected_pulls = {
        (
            pull["repository"]["owner"].casefold(),
            pull["repository"]["name"].casefold(),
            pull["number"],
        )
        for pull in candidate["selected_prs"]
    }
    expected_heads = {
        (
            target["repository"]["owner"].casefold(),
            target["repository"]["name"].casefold(),
            target["head"]["branch"].casefold(),
        )
        for target in candidate["targets"]
    }
    expected_worktrees = _reserved_worktree_paths(candidate)
    if (
        set(claim.issues) != expected_issues
        or set(claim.pull_requests) != expected_pulls
        or set(claim.repository_heads) != expected_heads
        or set(claim.worktrees) != expected_worktrees
    ):
        raise LedgerError(
            "legacy report claim does not exactly match candidate issue/PR/head/worktree coordinates"
        )


def _check_candidate_inventory(
    current: Inventory,
    candidate: Mapping[str, Any],
    raw: bytes,
    *,
    allowed_pending: set[tuple[str, str, str]],
    allowed_legacy_source: str | None = None,
) -> None:
    _require_exact_pending(current, allowed_pending)
    _reject_legacy_claims(
        current, candidate, allowed_source=allowed_legacy_source
    )
    target = canonical_name(candidate)
    proposed = Snapshot(target, dict(candidate), raw, byte_digest(raw), 0, 0)
    _reject_overlaps([*current.ledgers, proposed])


def _require_trusted_regular(
    status: os.stat_result, context: str, *, executable: bool = False
) -> None:
    if not stat.S_ISREG(status.st_mode):
        raise LedgerError(f"{context} is not a regular file")
    if status.st_uid != os.geteuid():
        raise LedgerError(f"{context} has a foreign owner")
    mode = stat.S_IMODE(status.st_mode)
    if mode & 0o022:
        raise LedgerError(f"{context} is group/world writable")
    if executable and not mode & stat.S_IXUSR:
        raise LedgerError(f"{context} is not owner-executable")


def _git_symbolic_ref(value: str, context: str) -> str:
    if (
        not value.startswith("refs/")
        or len(value.encode("ascii", "strict")) > 1024
        or not re.fullmatch(r"[A-Za-z0-9._+/-]+", value)
        or ".." in value
        or "//" in value
        or "@{" in value
        or value.endswith((".", "/"))
        or any(
            not component
            or component.startswith(".")
            or component.endswith(".lock")
            for component in value.split("/")
        )
    ):
        raise LedgerError(f"{context} is not a canonical Git symbolic ref")
    return value


def _single_ascii_line(raw: bytes, context: str) -> str:
    try:
        text = raw.decode("ascii")
    except UnicodeError as error:
        raise LedgerError(f"{context} is not ASCII") from error
    if text.endswith("\n"):
        text = text[:-1]
    if not text or "\n" in text or "\r" in text or _contains_control(text):
        raise LedgerError(f"{context} is not one canonical line")
    return text


def _require_canonical_git_head(directory: int, context: str) -> None:
    head_raw, head_status = _read_regular(directory, "HEAD")
    _require_trusted_regular(head_status, f"{context}/HEAD")
    head = _single_ascii_line(head_raw, f"{context}/HEAD")
    if COMMIT_RE.fullmatch(head):
        return
    prefix = "ref: "
    if not head.startswith(prefix):
        raise LedgerError(f"{context}/HEAD has no commit or symbolic ref")
    _git_symbolic_ref(head.removeprefix(prefix), f"{context}/HEAD ref")


def _require_recognizable_wrapper(directory: int, wrapper: Path) -> None:
    manifest_raw, manifest_status = _read_regular(directory, "components.json")
    _require_trusted_regular(manifest_status, f"{wrapper}/components.json")
    manifest = _exact(
        _decode(manifest_raw, "components.json"),
        {"schema_version", "cohorts", "stacks", "checkouts", "components"},
        "components.json",
    )
    if (
        isinstance(manifest["schema_version"], bool)
        or not isinstance(manifest["schema_version"], int)
        or not isinstance(manifest["cohorts"], dict)
        or not isinstance(manifest["stacks"], dict)
        or not isinstance(manifest["checkouts"], list)
        or not manifest["checkouts"]
        or not isinstance(manifest["components"], list)
        or not manifest["components"]
    ):
        raise LedgerError("components.json is not a recognizable Atrinik manifest")

    launcher_raw, launcher_status = _read_regular(directory, "atrinik")
    _require_trusted_regular(
        launcher_status, f"{wrapper}/atrinik", executable=True
    )
    if b"from atrinik_workspace.cli import main" not in launcher_raw.splitlines():
        raise LedgerError("atrinik launcher lacks the canonical CLI import")

    try:
        git_status = os.stat(".git", dir_fd=directory, follow_symlinks=False)
    except OSError as error:
        raise LedgerError(f"wrapper root lacks safe .git: {error}") from error
    if stat.S_ISREG(git_status.st_mode):
        git_raw, gitfile_status = _read_regular(directory, ".git")
        _require_trusted_regular(gitfile_status, f"{wrapper}/.git")
        git_line = _single_ascii_line(git_raw, "wrapper .git gitfile")
        prefix = "gitdir: "
        if not git_line.startswith(prefix):
            raise LedgerError("wrapper .git gitfile lacks gitdir identity")
        gitdir = git_line.removeprefix(prefix)
        if (
            not gitdir.startswith("/")
            or gitdir == "/"
            or os.path.normpath(gitdir) != gitdir
            or "//" in gitdir
            or _contains_control(gitdir)
        ):
            raise LedgerError("wrapper .git gitdir must be an absolute canonical path")
        try:
            git_directory = _directory_fd(Path(gitdir))
        except (LedgerError, OSError) as error:
            raise LedgerError(
                f"wrapper .git gitdir is not a safe live directory: {error}"
            ) from error
        try:
            opened = os.fstat(git_directory)
            visible = os.stat(gitdir, follow_symlinks=False)
            if (opened.st_dev, opened.st_ino) != (
                visible.st_dev,
                visible.st_ino,
            ):
                raise LedgerError("wrapper .git gitdir changed during validation")
            _require_trusted_directory(opened, f"wrapper .git gitdir {gitdir}")
            _require_trusted_directory(visible, f"wrapper .git gitdir {gitdir}")
            _require_canonical_git_head(git_directory, f"wrapper .git gitdir {gitdir}")
            rechecked = os.stat(gitdir, follow_symlinks=False)
            if (opened.st_dev, opened.st_ino) != (
                rechecked.st_dev,
                rechecked.st_ino,
            ):
                raise LedgerError("wrapper .git gitdir changed during validation")
        finally:
            os.close(git_directory)
        return
    if not stat.S_ISDIR(git_status.st_mode):
        raise LedgerError("wrapper root .git is neither a directory nor a regular gitfile")
    _require_trusted_directory(git_status, f"{wrapper}/.git")
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    git_directory = os.open(".git", flags, dir_fd=directory)
    try:
        opened = os.fstat(git_directory)
        visible = os.stat(".git", dir_fd=directory, follow_symlinks=False)
        if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
            raise LedgerError("wrapper .git directory changed during validation")
        _require_trusted_directory(opened, f"{wrapper}/.git")
        _require_canonical_git_head(git_directory, f"{wrapper}/.git")
    finally:
        os.close(git_directory)


def init_root(wrapper_root: Path | str) -> dict[str, Any]:
    """Safely initialize the exact ignored ``build/reviews`` chain."""

    wrapper = Path(os.path.abspath(wrapper_root))
    if wrapper == Path("/"):
        raise LedgerError("wrapper root cannot be the filesystem root")
    try:
        descriptor = _directory_fd(wrapper)
    except OSError as error:
        raise LedgerError(f"cannot open wrapper root {wrapper}: {error}") from error
    wrapper_status = os.fstat(descriptor)
    _require_trusted_directory(wrapper_status, f"wrapper root {wrapper}")
    try:
        _require_recognizable_wrapper(descriptor, wrapper)
    except (LedgerError, OSError) as error:
        os.close(descriptor)
        raise LedgerError(
            f"wrapper root lacks safe recognizable Atrinik metadata: {error}"
        ) from error
    except BaseException:
        os.close(descriptor)
        raise
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    current_path = wrapper
    try:
        for component in ("build", "reviews"):
            created = False
            try:
                os.mkdir(component, 0o700, dir_fd=descriptor)
                created = True
            except FileExistsError:
                pass
            if created:
                _fsync(descriptor, f"{current_path} after creating {component}")
            try:
                child = os.open(component, flags, dir_fd=descriptor)
            except OSError as error:
                raise LedgerError(
                    f"unsafe initialized directory component {current_path / component}: {error}"
                ) from error
            opened = os.fstat(child)
            visible = os.stat(component, dir_fd=descriptor, follow_symlinks=False)
            if not stat.S_ISDIR(opened.st_mode) or (opened.st_dev, opened.st_ino) != (
                visible.st_dev,
                visible.st_ino,
            ):
                os.close(child)
                raise LedgerError(f"initialized directory component is unsafe: {component}")
            _require_trusted_directory(
                opened, f"initialized directory component {current_path / component}"
            )
            os.close(descriptor)
            descriptor = child
            current_path /= component
        status = os.fstat(descriptor)
        wrapper_visible = os.stat(wrapper, follow_symlinks=False)
        if (wrapper_status.st_dev, wrapper_status.st_ino) != (
            wrapper_visible.st_dev,
            wrapper_visible.st_ino,
        ):
            raise LedgerError("wrapper root changed during initialization")
        review_visible = os.stat(current_path, follow_symlinks=False)
        if (status.st_dev, status.st_ino) != (
            review_visible.st_dev,
            review_visible.st_ino,
        ):
            raise LedgerError("initialized review root identity changed")
        return {
            "root": str(current_path),
            "device": status.st_dev,
            "inode": status.st_ino,
        }
    finally:
        os.close(descriptor)


def inventory(root: Path | str) -> Inventory:
    with _locked_root(Path(root)) as directory:
        return _inventory_locked(directory)


def inspect(root: Path | str, name: str) -> Snapshot:
    name = _direct_name(name)
    with _locked_root(Path(root)) as directory:
        _inventory_locked(directory)
        return _snapshot(directory, name)


def create(
    root: Path | str,
    document: Mapping[str, Any],
    *,
    failpoint: Failpoint = None,
) -> Snapshot:
    prepared = prepare(document)
    _require_create_genesis(prepared)
    target = canonical_name(prepared)
    raw = canonical_bytes(prepared)
    stage = f".{target}.create-{byte_digest(raw)}.tmp"
    with _locked_root(Path(root)) as directory:
        _require_names_fit(directory, (target, stage, f".{target}.lock"))
        initial_inventory = _inventory_locked(directory)
        _check_candidate_inventory(
            initial_inventory,
            prepared,
            raw,
            allowed_pending={("create", target, stage)},
        )
        with _ledger_lock(directory, target):
            current_inventory = _inventory_locked(directory)
            _check_candidate_inventory(
                current_inventory,
                prepared,
                raw,
                allowed_pending={("create", target, stage)},
            )
            if _exists(directory, target):
                snapshot = _snapshot(directory, target)
                if snapshot.raw != raw:
                    raise LedgerError(f"ledger already exists with different bytes: {target}")
                if _exists(directory, stage):
                    staged_raw, staged_status = _read_regular(
                        directory, stage, managed=True, expected_nlinks={2}
                    )
                    if staged_raw != raw:
                        raise LedgerError(f"staging content mismatch: {stage}")
                    _unlink_exact(directory, stage, staged_status)
                return snapshot
            stage_status = _ensure_stage(
                directory, stage, raw, allow_prefix_resume=True
            )
            _hit(failpoint, "create:staged")
            # Re-inventory while still holding both locks so the staged ledger
            # participates in cross-mode ownership checks.
            current = _inventory_locked(directory)
            _check_candidate_inventory(
                current,
                prepared,
                raw,
                allowed_pending={("create", target, stage)},
            )
            staged_snapshot = Snapshot(
                target,
                prepared,
                raw,
                byte_digest(raw),
                stage_status.st_dev,
                stage_status.st_ino,
            )
            _reject_overlaps([*current.ledgers, staged_snapshot])
            try:
                os.link(
                    stage,
                    target,
                    src_dir_fd=directory,
                    dst_dir_fd=directory,
                    follow_symlinks=False,
                )
            except FileExistsError as error:
                raise LedgerError(f"ledger destination appeared: {target}") from error
            installed = os.stat(target, dir_fd=directory, follow_symlinks=False)
            if (installed.st_dev, installed.st_ino) != (
                stage_status.st_dev,
                stage_status.st_ino,
            ):
                raise LedgerError(f"no-clobber install identity mismatch: {target}")
            _hit(failpoint, "create:linked")
            _fsync(directory, f"review root after installing {target}")
            _hit(failpoint, "create:installed")
            _unlink_exact(directory, stage, stage_status)
            _hit(failpoint, "create:cleaned")
            return _snapshot(directory, target)


def _transition(
    old: Mapping[str, Any],
    new: Mapping[str, Any],
    old_digest: str,
    *,
    _binding_capability: _AtomicBindingCapability | None = None,
) -> None:
    immutable = {
        "schema_version",
        "ledger_id",
        "entry_mode",
        "actor",
        "authority",
        "program",
        "issues",
        "closing_scope",
        "migration",
    }
    for key in immutable:
        if old[key] != new[key]:
            raise LedgerError(f"immutable ledger field changed: {key}")
    if new["generation"] != old["generation"] + 1:
        raise LedgerError("CAS replacement generation must increment by one")
    if new["previous_byte_digest"] != old_digest:
        raise LedgerError("CAS previous byte digest does not match current bytes")
    if new["history"] != [*old["history"], old_digest]:
        raise LedgerError("CAS digest history does not extend the current ledger")
    old_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in old["targets"]
    }
    new_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in new["targets"]
    }
    if set(old_targets) != set(new_targets):
        raise LedgerError("target set changed")
    target_drift = False
    for key, before in old_targets.items():
        after = new_targets[key]
        if before["repository"] != after["repository"]:
            raise LedgerError("target repository changed")
        advanced = False
        for field in ("base", "head"):
            if (
                before[field]["branch"] != after[field]["branch"]
                or before[field]["initial_sha"] != after[field]["initial_sha"]
            ):
                raise LedgerError(f"target {field} anchor changed")
            old_lineage = before[field]["lineage"]
            new_lineage = after[field]["lineage"]
            if new_lineage[: len(old_lineage)] != old_lineage:
                raise LedgerError(f"{field} lineage was rewritten instead of advanced")
            advanced = advanced or len(new_lineage) > len(old_lineage)
            target_drift = target_drift or (
                before[field]["current_sha"] != after[field]["current_sha"]
            )
        if before["merge_base"]["initial_sha"] != after["merge_base"]["initial_sha"]:
            raise LedgerError("merge-base initial anchor changed")
        if (
            before["merge_base"]["current_sha"]
            != after["merge_base"]["current_sha"]
            and not advanced
        ):
            raise LedgerError("merge-base refresh requires base or head advancement")
        target_drift = target_drift or (
            before["merge_base"]["current_sha"]
            != after["merge_base"]["current_sha"]
        )
    old_artifacts = {row["slot_id"]: row for row in old["artifacts"]}
    new_artifacts = {row["slot_id"]: row for row in new["artifacts"]}
    if set(old_artifacts) != set(new_artifacts):
        raise LedgerError("artifact slot set changed")
    protected_branches = {
        (
            slot["immutable"]["repository"]["node_id"],
            slot["immutable"]["branch"],
        ): "scope" if slot["producer_resource_slot"] is not None else "worktree"
        for slot in old["artifacts"]
        if slot["kind"] == "worktree"
        and slot["state"] == "planned"
        and (
            slot["producer_resource_slot"] is not None
            or slot.get("primitive_request") is not None
        )
    }
    protected_binding_kinds: set[str] = set()
    for slot, before in old_artifacts.items():
        after = new_artifacts[slot]
        if before["producer_resource_slot"] != after["producer_resource_slot"]:
            raise LedgerError(f"artifact producer resource changed: {slot}")
        if before.get("initial_body_payload") != after.get("initial_body_payload"):
            raise LedgerError(f"artifact initial body payload changed: {slot}")
        if before.get("primitive_request") != after.get("primitive_request"):
            raise LedgerError(f"artifact primitive request changed: {slot}")
        if before.get("primitive_result") is not None and (
            before["primitive_result"] != after.get("primitive_result")
        ):
            raise LedgerError(f"artifact primitive result changed: {slot}")
        if (
            before.get("primitive_result") is None
            and after.get("primitive_result") is not None
            and before["state"] != "planned"
        ):
            raise LedgerError(f"artifact primitive result was bound too late: {slot}")
        if before["kind"] != after["kind"] or before["immutable"] != after["immutable"]:
            raise LedgerError(f"artifact immutable identity changed: {slot}")
        protected_kind = None
        if before["kind"] == "worktree":
            coordinate = (
                before["immutable"]["repository"]["node_id"],
                before["immutable"]["branch"],
            )
            protected_kind = protected_branches.get(coordinate)
        elif before["kind"] == "branch":
            coordinate = (
                before["immutable"]["repository"]["node_id"],
                before["immutable"]["branch"],
            )
            protected_kind = protected_branches.get(coordinate)
        if (
            protected_kind is not None
            and before["state"] == "planned"
            and after["state"] != "planned"
        ):
            protected_binding_kinds.add(protected_kind)
        if before["state"] == "planned":
            if after["state"] not in ARTIFACT_STATES:
                raise LedgerError(f"invalid artifact transition: {slot}")
            if (
                before["kind"] == "pull_request"
                and after["state"] != "planned"
                and after["current"]["body_digest"]
                != before["immutable"]["body_digest"]
            ):
                raise LedgerError(
                    f"planned PR binding body differs from immutable intent: {slot}"
                )
        elif after["state"] != before["state"]:
            raise LedgerError(f"bound artifact state changed: {slot}")
        if before["current"] is not None and after["current"] is not None:
            before_identity = dict(before["current"])
            after_identity = dict(after["current"])
            before_sha = before_identity.pop("head_sha")
            after_sha = after_identity.pop("head_sha")
            before_identity.pop("body_digest")
            after_identity.pop("body_digest")
            if before_identity != after_identity:
                raise LedgerError(f"artifact current identity changed: {slot}")
            if before_sha != after_sha:
                relevant = [
                    row["head"]["lineage"]
                    for row in new["targets"]
                    if row["repository"]["node_id"]
                    == after["current"]["repository"]["node_id"]
                    and row["head"]["branch"] == after["current"]["branch"]
                ]
                if not relevant or not any(
                    before_sha in lineage
                    and after_sha in lineage
                    and lineage.index(before_sha) <= lineage.index(after_sha)
                    for lineage in relevant
                ):
                    raise LedgerError(f"artifact head was rewritten: {slot}")
    old_prs = {
        (row["repository"]["node_id"], row["number"], row["node_id"]): row
        for row in old["selected_prs"]
    }
    new_prs = {
        (row["repository"]["node_id"], row["number"], row["node_id"]): row
        for row in new["selected_prs"]
    }
    if not set(old_prs).issubset(new_prs):
        raise LedgerError("selected PR was removed or replaced")
    for key in set(old_prs) & set(new_prs):
        before = old_prs[key]
        after = new_prs[key]
        if {
            name: value
            for name, value in before.items()
            if name not in {"body", "comment", "draft", "draft_intent"}
        } != {
            name: value
            for name, value in after.items()
            if name not in {"body", "comment", "draft", "draft_intent"}
        }:
            raise LedgerError("selected PR immutable identity changed")
        if before["draft"] is False and after["draft"] is True:
            raise LedgerError("ready PR cannot be demoted to draft")
        if before["draft"] is True and after["draft"] is False:
            if before["draft_intent"] != "ready" or after["draft_intent"] is not None:
                raise LedgerError("ready transition lacks a recorded draft intent")
        elif before["draft"] is True and after["draft"] is True:
            if before["draft_intent"] is None and after["draft_intent"] not in {None, "ready"}:
                raise LedgerError("invalid ready intent")
        _body_transition(before["body"], after["body"], old, before)
        _comment_transition(before["comment"], after["comment"])
    added_pr_keys = set(new_prs) - set(old_prs)
    issue_created_prs: list[
        tuple[tuple[str, int, str], Mapping[str, Any], Mapping[str, Any], Mapping[str, Any]]
    ] = []
    for key in added_pr_keys:
        pull = new_prs[key]
        matching_new_slots = [
            slot["kind"] == "pull_request"
            and slot["state"] in {"created", "adopted"}
            and slot["current"] is not None
            and slot["current"]["repository"]["node_id"] == key[0]
            and slot["current"]["number"] == key[1]
            and slot["current"]["node_id"] == key[2]
            and slot["current"]["branch"] == pull["head_branch"]
            for slot in new["artifacts"]
        ]
        if sum(matching_new_slots) != 1:
            raise LedgerError("new selected PR is not bound by an artifact slot")
        if key[2] not in old["authority"]["allowed"]["pull_requests"]:
            after_slots = [
                slot
                for slot in new["artifacts"]
                if slot["kind"] == "pull_request"
                and slot["current"] is not None
                and slot["current"]["node_id"] == key[2]
            ]
            if len(after_slots) != 1:
                raise LedgerError("unauthorized PR bind lacks one exact artifact slot")
            after_slot = after_slots[0]
            before_slot = old_artifacts[after_slot["slot_id"]]
            targets = [
                target
                for target in new["targets"]
                if target["repository"] == pull["repository"]
                and target["base"]["branch"] == pull["base_branch"]
                and target["head"]["branch"] == pull["head_branch"]
            ]
            body = pull["body"]
            if (
                old["entry_mode"] != "issue"
                or old["authority"]["allowed"]["pull_requests"]
                or before_slot["state"] != "planned"
                or after_slot["state"] != "created"
                or len(targets) != 1
                or targets[0]
                != old_targets[
                    (
                        targets[0]["repository"]["node_id"],
                        targets[0]["head"]["branch"],
                    )
                ]
                or before_slot["immutable"]["repository"] != pull["repository"]
                or before_slot["immutable"]["branch"] != pull["head_branch"]
                or pull["head_repository"] != pull["repository"]
                or pull["author_node_id"] != old["actor"]["node_id"]
                or body["ownership"] != "delivery-created"
                or body["state"] != "written"
                or body["observed_digest"] is not None
                or body["intended_digest"] is not None
                or body["intended_payload"] is not None
                or body["current_digest"]
                != before_slot["immutable"]["body_digest"]
                or body["outside_digest"] != body["current_digest"]
                or body["section_digest"] is not None
                or pull["draft"] is not True
                or pull["draft_intent"] is not None
                or pull["comment"]["state"] != "none"
            ):
                raise LedgerError("unauthorized PR bind is not exact issue-created output")
            issue_created_prs.append((key, pull, before_slot, after_slot))
    if issue_created_prs:
        if len(issue_created_prs) != 1 or added_pr_keys != {issue_created_prs[0][0]}:
            raise LedgerError("issue-created PR bind must add exactly one selected PR")
        key, _, before_slot, after_slot = issue_created_prs[0]
        normalized = _decode(canonical_bytes(new), "issue-created PR transition")
        normalized["generation"] = old["generation"]
        normalized["previous_byte_digest"] = old["previous_byte_digest"]
        normalized["history"] = old["history"]
        normalized["selected_prs"] = [
            pull
            for pull in normalized["selected_prs"]
            if (
                pull["repository"]["node_id"],
                pull["number"],
                pull["node_id"],
            )
            != key
        ]
        normalized["artifacts"] = [
            before_slot if slot["slot_id"] == after_slot["slot_id"] else slot
            for slot in normalized["artifacts"]
        ]
        if normalized != old:
            raise LedgerError(
                "issue-created PR bind CAS changed unrelated ledger state"
            )
    if target_drift and any(
        pull["draft_intent"] is not None
        or pull["body"]["state"] == "update-planned"
        or pull["comment"]["state"] in {"planned", "in-flight"}
        for document in (old, new)
        for pull in document["selected_prs"]
    ):
        raise LedgerError(
            "target drift requires a separate prior CAS canceling body/readiness/comment intent"
        )
    old_resources = {row["slot_id"]: row for row in old["resources"]}
    new_resources = {row["slot_id"]: row for row in new["resources"]}
    if not set(old_resources).issubset(new_resources):
        raise LedgerError("resource slot was removed")
    for slot in set(new_resources) - set(old_resources):
        appended = new_resources[slot]
        if appended["state"] != "planned" or appended["current"] is not None:
            raise LedgerError(f"new resource slot must be planned before mutation: {slot}")
        if appended["kind"] == "scope":
            raise LedgerError("scope resources must be reserved by fresh issue-mode create")
    for slot, before in old_resources.items():
        after = new_resources[slot]
        if before.get("request") != after.get("request"):
            raise LedgerError(f"resource request changed: {slot}")
        if before["kind"] != after["kind"] or before["immutable"] != after["immutable"]:
            raise LedgerError(f"resource immutable identity changed: {slot}")
        if (
            before["kind"] == "scope"
            and before["state"] == "planned"
            and after["state"] != "planned"
        ):
            protected_binding_kinds.add("scope")
        if before["state"] != "planned" and after["state"] != before["state"]:
            raise LedgerError(f"bound resource state changed: {slot}")
        if before["current"] is None and after["current"] is not None:
            if (
                after["current"]["generation"] != 1
                or after["current"]["history"] != []
            ):
                raise LedgerError(f"first resource binding must start at generation 1: {slot}")
        if before["current"] is not None and after["current"] is not None:
            before_current = before["current"]
            after_current = after["current"]
            for field in ("repository", "name", "path", "binding", "observation"):
                if field not in before_current and field not in after_current:
                    continue
                if before_current[field] != after_current[field]:
                    raise LedgerError(f"bound resource identity changed: {slot}")
            if before_current["external_generation"] != after_current["external_generation"]:
                raise LedgerError(f"resource external generation changed: {slot}")
            if (
                before_current["lifecycle"] == "released"
                and after_current["lifecycle"] != "released"
            ):
                raise LedgerError(f"released resource lifecycle is terminal: {slot}")
            observation_changed = (
                after_current["identity_digest"] != before_current["identity_digest"]
                or after_current["lifecycle"] != before_current["lifecycle"]
            )
            if not observation_changed:
                if (
                    after_current["generation"] != before_current["generation"]
                    or after_current["history"] != before_current["history"]
                ):
                    raise LedgerError(f"unchanged resource observation advanced: {slot}")
            elif (
                after_current["generation"] != before_current["generation"] + 1
                or after_current["identity_digest"] == before_current["identity_digest"]
                or after_current["history"] != [
                    *before_current["history"],
                    before_current["identity_digest"],
                ]
            ):
                raise LedgerError(
                    f"changed resource observation requires new digest/generation/history: {slot}"
                )
    if protected_binding_kinds:
        if (
            _binding_capability is None
            or _binding_capability.token is not _ATOMIC_BIND_TOKEN
            or protected_binding_kinds != {_binding_capability.kind}
        ):
            rendered = "/".join(sorted(protected_binding_kinds))
            raise LedgerError(
                f"initial {rendered} binding requires its purpose-specific atomic binder"
            )


def _body_transition(
    before: Mapping[str, Any],
    after: Mapping[str, Any],
    document: Mapping[str, Any],
    pull: Mapping[str, Any],
) -> None:
    if before == after:
        return
    if (
        {name: value for name, value in after.items() if name != "updated_at"}
        == {name: value for name, value in before.items() if name != "updated_at"}
        and _timestamp_is_equal(after["updated_at"], before["updated_at"])
    ):
        return
    if before["ownership"] == "contributor-owned":
        if (
            after["ownership"] == "contributor-owned"
            and before["state"] == after["state"] == "observed"
            and after["observed_digest"] == after["current_digest"]
            and after["intended_digest"] is None
            and _timestamp_is_after(after["updated_at"], before["updated_at"])
        ):
            return
        if (
            before["state"] == "observed"
            and after["ownership"] == "delivery-section"
            and after["state"] == "update-planned"
            and after["observed_digest"] == before["current_digest"]
            and after["current_digest"] == before["current_digest"]
            and after["outside_digest"] == before["current_digest"]
            and after["section_digest"] is None
            and _timestamp_is_equal(after["updated_at"], before["updated_at"])
        ):
            return
        raise LedgerError(
            "invalid contributor-owned PR body timestamp refresh or section insertion plan"
        )
    first_insertion_cancel = (
        before["ownership"] == "delivery-section"
        and before["state"] == "update-planned"
        and before["section_digest"] is None
        and after["ownership"] == "contributor-owned"
        and after["state"] == "observed"
    )
    if before["ownership"] != after["ownership"] and not first_insertion_cancel:
        raise LedgerError("PR body ownership changed")
    if before["state"] == after["state"] == "written":
        if (
            {name: value for name, value in after.items() if name != "updated_at"}
            == {name: value for name, value in before.items() if name != "updated_at"}
            and _timestamp_is_after(after["updated_at"], before["updated_at"])
        ):
            return
    if before["state"] == after["state"] == "update-planned":
        if (
            {name: value for name, value in after.items() if name != "updated_at"}
            == {name: value for name, value in before.items() if name != "updated_at"}
            and _timestamp_is_after(after["updated_at"], before["updated_at"])
        ):
            return
    if (
        before["state"] == "written"
        and after["state"] == "update-planned"
        and after["observed_digest"] == before["current_digest"]
        and after["current_digest"] == before["current_digest"]
        and after["outside_digest"] == before["outside_digest"]
        and after["section_digest"] == before["section_digest"]
        and _timestamp_is_equal(after["updated_at"], before["updated_at"])
    ):
        return
    if before["state"] == "update-planned":
        if after["current_digest"] == before["current_digest"]:
            if (
                before["ownership"] == "delivery-section"
                and before["section_digest"] is None
                and after["ownership"] == "contributor-owned"
                and after["state"] == "observed"
                and after["observed_digest"] == before["current_digest"]
                and after["outside_digest"] == before["current_digest"]
                and after["section_digest"] is None
                and after["intended_digest"] is None
                and after["intended_payload"] is None
                and _timestamp_is_equal(after["updated_at"], before["updated_at"])
            ):
                return
            if (
                before["section_digest"] is not None
                or before["ownership"] == "delivery-created"
            ) and (
                after["ownership"] == before["ownership"]
                and after["state"] == "written"
                and after["observed_digest"] == before["current_digest"]
                and after["outside_digest"] == before["outside_digest"]
                and after["section_digest"] == before["section_digest"]
                and after["intended_digest"] is None
                and after["intended_payload"] is None
                and _timestamp_is_equal(after["updated_at"], before["updated_at"])
            ):
                return
        if (
            after["ownership"] == before["ownership"]
            and after["state"] == "written"
            and after["observed_digest"] == before["current_digest"]
            and after["current_digest"] == before["intended_digest"]
            and after["outside_digest"] == before["outside_digest"]
            and after["intended_digest"] is None
            and after["intended_payload"] is None
            and (
                _timestamp_is_equal(after["updated_at"], before["updated_at"])
                or _timestamp_is_after(after["updated_at"], before["updated_at"])
            )
        ):
            intended_raw = _inline_payload(
                before["intended_payload"], "body transition intended payload", utf8=True
            )
            parts = _body_section_parts(
                intended_raw, *_body_markers_for(document, pull)
            )
            if parts is not None and after["section_digest"] == byte_digest(parts[2]):
                return
    raise LedgerError("invalid delivery PR body transition")


def _comment_transition(before: Mapping[str, Any], after: Mapping[str, Any]) -> None:
    if before == after:
        return
    if before["state"] == "none" and after["state"] == "planned":
        if after["node_id"] is None and after["current_digest"] is None:
            return
    if before["state"] == "bound" and after["state"] == "planned":
        if (
            after["marker"] == before["marker"]
            and after["node_id"] == before["node_id"]
            and after["current_digest"] == before["current_digest"]
        ):
            return
    if before["state"] == "planned":
        if after["state"] == "in-flight" and after == {**before, "state": "in-flight"}:
            return
        if before["node_id"] is None and after["state"] == "none":
            return
        if before["node_id"] is not None and after["state"] == "bound" and after == {
            **before,
            "state": "bound",
            "intended_digest": None,
            "intended_payload": None,
        }:
            return
    if before["state"] == "in-flight" and after["state"] == "bound":
        if (
            after["marker"] == before["marker"]
            and after["node_id"] is not None
            and (
                before["node_id"] is None
                or after["node_id"] == before["node_id"]
            )
            and after["current_digest"] == before["intended_digest"]
            and after["intended_digest"] is None
            and after["intended_payload"] is None
        ):
            return
    raise LedgerError("invalid delivery comment transition")


def cas(
    root: Path | str,
    name: str,
    document: Mapping[str, Any],
    *,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
    failpoint: Failpoint = None,
    _precommit: Callable[[], None] | None = None,
    _binding_capability: _AtomicBindingCapability | None = None,
) -> Snapshot:
    name = _direct_name(name)
    prepared = prepare(document)
    if canonical_name(prepared) != name:
        raise LedgerError("CAS replacement has a different canonical filename")
    _integer(expected_generation, "expected_generation")
    _string(expected_digest, "expected_digest", SHA256_RE)
    _integer(expected_device, "expected_device", minimum=0)
    _integer(expected_inode, "expected_inode")
    raw = canonical_bytes(prepared)
    operation = ""
    if _binding_capability is not None:
        capability = _binding_capability
        if (
            not isinstance(capability, _AtomicBindingCapability)
            or capability.token is not _ATOMIC_BIND_TOKEN
            or capability.kind not in {"worktree", "scope"}
            or not SLOT_RE.fullmatch(capability.slot_id)
            or capability.name != name
            or capability.after_raw != raw
            or (
                capability.expected_generation,
                capability.expected_digest,
                capability.expected_device,
                capability.expected_inode,
            )
            != (
                expected_generation,
                expected_digest,
                expected_device,
                expected_inode,
            )
        ):
            raise LedgerError("invalid internal atomic-binding capability")
        operation = f"-bind-{capability.kind}-{capability.slot_id}"
    stage = (
        f".{name}.update{operation}-g{prepared['generation']}-from-{expected_digest}-"
        f"to-{byte_digest(raw)}.tmp"
    )
    proof = (
        f".{name}.update-proof{operation}-g{expected_generation}-from-{expected_digest}-"
        f"d{expected_device}-i{expected_inode}-to-{byte_digest(raw)}.tmp"
    )
    allowed_pending = {
        ("update", name, stage),
        ("update-proof", name, proof),
    }
    with _locked_root(Path(root)) as directory:
        _require_names_fit(directory, (name, stage, proof, f".{name}.lock"))
        initial_inventory = _inventory_locked(directory)
        _check_candidate_inventory(
            initial_inventory,
            prepared,
            raw,
            allowed_pending=allowed_pending,
        )
        with _ledger_lock(directory, name):
            current_inventory = _inventory_locked(directory)
            _check_candidate_inventory(
                current_inventory,
                prepared,
                raw,
                allowed_pending=allowed_pending,
            )
            current = _snapshot(directory, name)
            # Retrying after the rename is permitted only while the installed
            # candidate remains hard-linked to the exact predecessor-tuple proof.
            if (
                current.raw == raw
                and prepared["generation"] == expected_generation + 1
                and prepared["previous_byte_digest"] == expected_digest
            ):
                if not _exists(directory, proof):
                    raise LedgerError("stale CAS tuple has no durable post-rename proof")
                proof_raw, proof_status = _read_regular(
                    directory,
                    proof,
                    managed=True,
                    expected_nlinks={2},
                    sync=True,
                )
                if proof_raw != raw or (
                    proof_status.st_dev,
                    proof_status.st_ino,
                ) != (current.device, current.inode):
                    raise LedgerError("post-rename CAS proof does not match installed bytes")
                if _precommit is not None:
                    _precommit()
                _fsync(directory, f"review root while resuming update of {name}")
                _unlink_exact(directory, proof, proof_status)
                return current
            if (
                current.document["generation"] != expected_generation
                or current.digest != expected_digest
                or current.device != expected_device
                or current.inode != expected_inode
            ):
                raise LedgerError("stale CAS generation, digest, or inode")
            if _binding_capability is not None:
                if _binding_capability.before_raw is None:
                    raise LedgerError(
                        "atomic-binding recovery candidate is not already installed"
                    )
                if _binding_capability.before_raw != current.raw:
                    raise LedgerError("atomic-binding predecessor bytes changed")
            _transition(
                current.document,
                prepared,
                current.digest,
                _binding_capability=_binding_capability,
            )
            stage_status = _ensure_stage(
                directory,
                stage,
                raw,
                allow_prefix_resume=True,
                expected_nlinks={1, 2},
            )
            _hit(failpoint, "cas:staged")
            if _exists(directory, proof):
                proof_raw, proof_status = _read_regular(
                    directory,
                    proof,
                    managed=True,
                    expected_nlinks={2},
                )
                if proof_raw != raw or (
                    proof_status.st_dev,
                    proof_status.st_ino,
                ) != (stage_status.st_dev, stage_status.st_ino):
                    raise LedgerError("CAS predecessor proof differs from staging")
            else:
                os.link(
                    stage,
                    proof,
                    src_dir_fd=directory,
                    dst_dir_fd=directory,
                    follow_symlinks=False,
                )
                _fsync(directory, f"review root after proving update of {name}")
                proof_status = os.stat(
                    proof, dir_fd=directory, follow_symlinks=False
                )
                if (
                    proof_status.st_nlink != 2
                    or (proof_status.st_dev, proof_status.st_ino)
                    != (stage_status.st_dev, stage_status.st_ino)
                ):
                    raise LedgerError("CAS predecessor proof link mismatch")
            _hit(failpoint, "cas:proofed")
            # Recheck the expected inode/generation/digest immediately before
            # the atomic replacement.  The protocol requires all writers to
            # hold these locks; an uncooperative external writer remains outside
            # that protocol and is not claimed safe here.
            rechecked = _snapshot(directory, name)
            if (
                rechecked.document["generation"] != expected_generation
                or rechecked.digest != expected_digest
                or rechecked.device != expected_device
                or rechecked.inode != expected_inode
            ):
                raise LedgerError("CAS target changed after staging")
            staged_raw, staged_visible = _read_regular(
                directory,
                stage,
                managed=True,
                expected_nlinks={2},
                sync=True,
            )
            if staged_raw != raw or (staged_visible.st_dev, staged_visible.st_ino) != (
                stage_status.st_dev,
                stage_status.st_ino,
            ):
                raise LedgerError("CAS staging changed before rename")
            if _precommit is not None:
                try:
                    _precommit()
                except LedgerError as error:
                    if not _discard_exact_cas_pending(directory, stage, proof, raw):
                        raise LedgerError(
                            f"{error}; exact uninstalled CAS pending files could not "
                            "be safely removed"
                        ) from error
                    raise
            os.replace(stage, name, src_dir_fd=directory, dst_dir_fd=directory)
            _hit(failpoint, "cas:renamed")
            installed = _snapshot(directory, name)
            if installed.raw != raw:
                raise LedgerError("CAS replacement bytes mismatch")
            _fsync(directory, f"review root after updating {name}")
            _hit(failpoint, "cas:installed")
            proof_visible = os.stat(proof, dir_fd=directory, follow_symlinks=False)
            if (
                proof_visible.st_dev,
                proof_visible.st_ino,
            ) != (installed.device, installed.inode):
                raise LedgerError("installed CAS lost its predecessor proof")
            _unlink_exact(directory, proof, proof_visible)
            return installed


def _migration_marker(
    *,
    state: str,
    kind: str,
    candidate_digest: str,
    source: Mapping[str, Any],
    snapshot_name: str,
    snapshot: Mapping[str, Any] | None,
    canonical_report: str,
    destination: str,
    destination_digest: str | None,
) -> dict[str, Any]:
    return {
        "transaction": "delivery-ledger-migration-v1",
        "state": state,
        "kind": kind,
        "candidate_digest": candidate_digest,
        "source": dict(source),
        "snapshot_name": snapshot_name,
        "snapshot": None if snapshot is None else dict(snapshot),
        "canonical_report": canonical_report,
        "destination": destination,
        "destination_digest": destination_digest,
        "staging": f".{destination}.migrate.tmp",
    }


def migrate(
    root: Path | str,
    source_name: str,
    document: Mapping[str, Any],
    *,
    kind: str,
    expected_source_digest: str | None = None,
    failpoint: Failpoint = None,
) -> Snapshot:
    if kind not in MIGRATION_KINDS:
        raise LedgerError("migration kind must be legacy or pre-schema")
    if expected_source_digest is None:
        raise LedgerError("migration requires an exact expected source digest")
    _string(expected_source_digest, "expected_source_digest", SHA256_RE)
    source_name = _direct_name(source_name, "migration source")
    candidate = prepare(document)
    _require_migration_genesis(candidate, kind)
    if candidate["authority"]["kind"] not in {"durable-goal", "explicit-recovery"}:
        raise LedgerError(
            "migration requires durable-goal or explicit-recovery authority"
        )
    candidate_raw = canonical_bytes(candidate)
    candidate_digest = byte_digest(candidate_raw)
    target = canonical_name(candidate)
    marker_name = f".{target}.migration.json"
    stage = f".{target}.migrate.tmp"
    marker_prepare_stage = f"{marker_name}.prepared.tmp"
    marker_complete_stage = f"{marker_name}.complete.tmp"
    snapshot_name = f".{target}.migration-source.snapshot"
    canonical_report = target.removesuffix(".ledger.json")
    if kind == "pre-schema" and source_name != canonical_report:
        raise LedgerError(
            "pre-schema migration source must be the canonical Markdown report"
        )
    if kind == "legacy" and source_name != _legacy_source_name(candidate):
        raise LedgerError(
            f"legacy migration source must be exactly {_legacy_source_name(candidate)}"
        )
    with _locked_root(Path(root)) as directory:
        preflight_marker = None
        if _exists(directory, marker_name):
            preflight_marker_raw, _ = _read_regular(
                directory,
                marker_name,
                managed=True,
                expected_nlinks={1, 2},
            )
            preflight_marker = _marker_document(preflight_marker_raw, marker_name)
            if (
                preflight_marker["kind"] != kind
                or preflight_marker["candidate_digest"] != candidate_digest
                or preflight_marker["source"]["name"] != source_name
                or preflight_marker["canonical_report"] != canonical_report
                or preflight_marker["destination"] != target
                or preflight_marker["snapshot_name"] != snapshot_name
            ):
                raise LedgerError("migration marker addresses a different transition")
            if preflight_marker["source"]["sha256"] != expected_source_digest:
                raise LedgerError("expected source digest differs from planned migration")
        preflight_raw, preflight_status = _read_regular(directory, source_name)
        preflight_digest = byte_digest(preflight_raw)
        mutable_completed_pre_schema = (
            kind == "pre-schema"
            and preflight_marker is not None
            and preflight_marker["state"] == "complete"
            and preflight_marker["source"]["sha256"] == expected_source_digest
        )
        if preflight_digest != expected_source_digest and not mutable_completed_pre_schema:
            raise LedgerError("migration source digest does not match expectation")
        if kind == "legacy":
            _require_exact_legacy_claim(source_name, preflight_raw, candidate)
        if preflight_marker is None:
            planned_marker = _migration_marker(
                state="planned",
                kind=kind,
                candidate_digest=candidate_digest,
                source={
                    "name": source_name,
                    "sha256": preflight_digest,
                    "device": preflight_status.st_dev,
                    "inode": preflight_status.st_ino,
                },
                snapshot_name=snapshot_name,
                snapshot=None,
                canonical_report=canonical_report,
                destination=target,
                destination_digest=None,
            )
        else:
            planned_marker = {
                **preflight_marker,
                "state": "planned",
                "snapshot": None,
                "destination_digest": None,
            }
        planned_marker_raw = canonical_bytes(planned_marker)
        operation_digest = byte_digest(planned_marker_raw)
        marker_plan_stage = f"{marker_name}.planned-{operation_digest}.tmp"
        _require_names_fit(
            directory,
            (
                target,
                marker_name,
                marker_plan_stage,
                stage,
                marker_prepare_stage,
                marker_complete_stage,
                snapshot_name,
                canonical_report,
                f".{target}.lock",
            ),
        )
        allowed_pending = {
            ("migration-plan", target, marker_plan_stage),
            ("migration-snapshot", target, snapshot_name),
            ("migration", target, marker_name),
            ("migration-prepare", target, marker_prepare_stage),
            ("migration", target, stage),
            ("migration-complete", target, marker_complete_stage),
        }
        initial_inventory = _inventory_locked(directory)
        _check_candidate_inventory(
            initial_inventory,
            candidate,
            candidate_raw,
            allowed_pending=allowed_pending,
            allowed_legacy_source=source_name,
        )
        with _ledger_lock(directory, target):
            current_inventory = _inventory_locked(directory)
            _check_candidate_inventory(
                current_inventory,
                candidate,
                candidate_raw,
                allowed_pending=allowed_pending,
                allowed_legacy_source=source_name,
            )
            source_raw, source_status = _read_regular(directory, source_name)
            source_digest = byte_digest(source_raw)
            if (
                source_raw != preflight_raw
                or (source_status.st_dev, source_status.st_ino)
                != (preflight_status.st_dev, preflight_status.st_ino)
            ):
                raise LedgerError("migration source changed after preflight")
            marker: dict[str, Any] | None = None
            marker_status: os.stat_result | None = None
            if _exists(directory, marker_name):
                marker_raw, marker_status = _read_regular(
                    directory,
                    marker_name,
                    managed=True,
                    expected_nlinks={1, 2},
                )
                marker = _marker_document(marker_raw, marker_name)
                if (
                    marker["kind"] != kind
                    or marker["candidate_digest"] != candidate_digest
                    or marker["source"]["name"] != source_name
                    or marker["canonical_report"] != canonical_report
                    or marker["destination"] != target
                    or marker["snapshot_name"] != snapshot_name
                ):
                    raise LedgerError("migration marker addresses a different transition")
                if marker["source"]["sha256"] != expected_source_digest:
                    raise LedgerError("expected source digest differs from planned migration")
                if _exists(directory, marker_plan_stage):
                    planned_raw, planned_status = _read_regular(
                        directory,
                        marker_plan_stage,
                        managed=True,
                        expected_nlinks={2},
                        sync=True,
                    )
                    if planned_raw != marker_raw or (
                        planned_status.st_dev,
                        planned_status.st_ino,
                    ) != (marker_status.st_dev, marker_status.st_ino):
                        raise LedgerError("migration plan staging/install identity mismatch")
                    _unlink_exact(directory, marker_plan_stage, planned_status)
                    marker_raw, marker_status = _read_regular(
                        directory, marker_name, managed=True, sync=True
                    )
            if marker is None:
                if source_digest != expected_source_digest:
                    raise LedgerError("migration source digest does not match expectation")
                marker = planned_marker
                marker_raw = planned_marker_raw
                plan_status = _ensure_stage(
                    directory,
                    marker_plan_stage,
                    marker_raw,
                    allow_prefix_resume=True,
                )
                _hit(failpoint, "migration:plan-staged")
                planned_raw, planned_visible = _read_regular(
                    directory,
                    marker_plan_stage,
                    managed=True,
                    sync=True,
                )
                if planned_raw != marker_raw or (
                    planned_visible.st_dev,
                    planned_visible.st_ino,
                ) != (plan_status.st_dev, plan_status.st_ino):
                    raise LedgerError("migration plan staging changed before publish")
                try:
                    os.link(
                        marker_plan_stage,
                        marker_name,
                        src_dir_fd=directory,
                        dst_dir_fd=directory,
                        follow_symlinks=False,
                    )
                except FileExistsError as error:
                    raise LedgerError("migration plan destination appeared") from error
                _hit(failpoint, "migration:plan-linked")
                _fsync(directory, "review root after migration plan")
                marker_status = os.stat(
                    marker_name, dir_fd=directory, follow_symlinks=False
                )
                if (
                    marker_status.st_nlink != 2
                    or (marker_status.st_dev, marker_status.st_ino)
                    != (plan_status.st_dev, plan_status.st_ino)
                ):
                    raise LedgerError("migration plan no-clobber publish mismatch")
                _unlink_exact(directory, marker_plan_stage, plan_status)
                marker_raw, marker_status = _read_regular(
                    directory, marker_name, managed=True, sync=True
                )
                _hit(failpoint, "migration:planned")
            source = marker["source"]
            if kind == "legacy" or marker["state"] != "complete":
                if (
                    source_digest != source["sha256"]
                    or source_status.st_dev != source["device"]
                    or source_status.st_ino != source["inode"]
                ):
                    raise LedgerError("migration source changed before safe completion")
            if marker["state"] == "planned":
                snapshot_status = _ensure_stage(
                    directory, snapshot_name, source_raw, allow_prefix_resume=True
                )
                snapshot_raw, snapshot_visible = _read_regular(
                    directory, snapshot_name, managed=True, sync=True
                )
                if (
                    snapshot_raw != source_raw
                    or (snapshot_visible.st_dev, snapshot_visible.st_ino)
                    != (snapshot_status.st_dev, snapshot_status.st_ino)
                ):
                    raise LedgerError("immutable migration snapshot mismatch")
                snapshot = {
                    "name": snapshot_name,
                    "sha256": byte_digest(snapshot_raw),
                    "device": snapshot_visible.st_dev,
                    "inode": snapshot_visible.st_ino,
                }
                _hit(failpoint, "migration:snapshot")
                if kind == "legacy":
                    canonical_status = _ensure_stage(
                        directory,
                        canonical_report,
                        snapshot_raw,
                        allow_prefix_resume=True,
                    )
                    canonical_raw, canonical_visible = _read_regular(
                        directory, canonical_report
                    )
                    if canonical_raw != snapshot_raw or (
                        canonical_visible.st_dev,
                        canonical_visible.st_ino,
                    ) != (canonical_status.st_dev, canonical_status.st_ino):
                        raise LedgerError("legacy canonical report copy mismatch")
                else:
                    canonical_raw, _ = _read_regular(directory, canonical_report)
                    if canonical_raw != snapshot_raw:
                        raise LedgerError("pre-schema report changed before migration")
                _hit(failpoint, "migration:report")
                migration = {
                    "kind": kind,
                    "state": "complete",
                    "source": source,
                    "snapshot": snapshot,
                    "canonical_report": canonical_report,
                    "marker_name": marker_name,
                }
                prepared = prepare({**candidate, "migration": migration})
                raw = canonical_bytes(prepared)
                destination_digest = byte_digest(raw)
                prepared_marker = _migration_marker(
                    state="prepared",
                    kind=kind,
                    candidate_digest=candidate_digest,
                    source=source,
                    snapshot_name=snapshot_name,
                    snapshot=snapshot,
                    canonical_report=canonical_report,
                    destination=target,
                    destination_digest=destination_digest,
                )
                prepared_marker_raw = canonical_bytes(prepared_marker)
                prepare_status = _ensure_stage(
                    directory,
                    marker_prepare_stage,
                    prepared_marker_raw,
                    allow_prefix_resume=True,
                )
                _hit(failpoint, "migration:prepared-staged")
                marker_recheck_raw, marker_recheck_status = _read_regular(
                    directory, marker_name, managed=True
                )
                if marker_recheck_raw != marker_raw or (
                    marker_recheck_status.st_dev,
                    marker_recheck_status.st_ino,
                ) != (marker_status.st_dev, marker_status.st_ino):
                    raise LedgerError("migration plan changed before preparation")
                prepared_check, prepared_visible = _read_regular(
                    directory, marker_prepare_stage, managed=True, sync=True
                )
                if prepared_check != prepared_marker_raw or (
                    prepared_visible.st_dev,
                    prepared_visible.st_ino,
                ) != (prepare_status.st_dev, prepare_status.st_ino):
                    raise LedgerError("migration preparation staging changed")
                os.replace(
                    marker_prepare_stage,
                    marker_name,
                    src_dir_fd=directory,
                    dst_dir_fd=directory,
                )
                _hit(failpoint, "migration:prepared-renamed")
                _fsync(directory, "review root after preparing migration")
                marker = prepared_marker
            else:
                snapshot = marker["snapshot"]
                snapshot_raw, snapshot_visible = _read_regular(
                    directory, snapshot_name, managed=True
                )
                if (
                    byte_digest(snapshot_raw) != snapshot["sha256"]
                    or snapshot_visible.st_dev != snapshot["device"]
                    or snapshot_visible.st_ino != snapshot["inode"]
                ):
                    raise LedgerError("immutable migration snapshot changed")
                canonical_raw, _ = _read_regular(directory, canonical_report)
                if (
                    kind == "legacy"
                    and marker["state"] != "complete"
                    and canonical_raw != snapshot_raw
                ):
                    raise LedgerError("legacy canonical report changed")
                migration = {
                    "kind": kind,
                    "state": "complete",
                    "source": source,
                    "snapshot": snapshot,
                    "canonical_report": canonical_report,
                    "marker_name": marker_name,
                }
                prepared = prepare({**candidate, "migration": migration})
                raw = canonical_bytes(prepared)
                destination_digest = byte_digest(raw)
                if destination_digest != marker["destination_digest"]:
                    raise LedgerError("migration destination plan digest changed")
            if marker["state"] == "complete":
                _fsync(directory, "review root while resuming completed migration")
                final_inventory = _inventory_locked(directory)
                _require_exact_pending(final_inventory, set())
                return _snapshot(directory, target)
            if _exists(directory, target):
                installed = _snapshot(directory, target)
                if installed.raw != raw:
                    raise LedgerError("migration destination already differs")
                if _exists(directory, stage):
                    staged_raw, staged_status = _read_regular(
                        directory, stage, managed=True, expected_nlinks={2}
                    )
                    target_status = os.stat(
                        target, dir_fd=directory, follow_symlinks=False
                    )
                    if staged_raw != raw or (
                        staged_status.st_dev,
                        staged_status.st_ino,
                    ) != (target_status.st_dev, target_status.st_ino):
                        raise LedgerError("migration staging/install identity mismatch")
                    _unlink_exact(directory, stage, staged_status)
            else:
                stage_status = _ensure_stage(
                    directory, stage, raw, allow_prefix_resume=True
                )
                _hit(failpoint, "migration:staged")
                os.link(
                    stage,
                    target,
                    src_dir_fd=directory,
                    dst_dir_fd=directory,
                    follow_symlinks=False,
                )
                installed_status = os.stat(target, dir_fd=directory, follow_symlinks=False)
                if (installed_status.st_dev, installed_status.st_ino) != (
                    stage_status.st_dev,
                    stage_status.st_ino,
                ):
                    raise LedgerError("migration no-clobber install identity mismatch")
                _hit(failpoint, "migration:linked")
                _fsync(directory, "review root after migration destination")
                _unlink_exact(directory, stage, stage_status)
            _hit(failpoint, "migration:installed")
            current_marker_raw, current_marker_status = _read_regular(
                directory, marker_name, managed=True
            )
            current_marker = _marker_document(current_marker_raw, marker_name)
            if current_marker["state"] == "prepared":
                complete = {**current_marker, "state": "complete"}
                complete_raw = canonical_bytes(complete)
                complete_status = _ensure_stage(
                    directory,
                    marker_complete_stage,
                    complete_raw,
                    allow_prefix_resume=True,
                )
                _hit(failpoint, "migration:complete-staged")
                marker_recheck_raw, marker_recheck_status = _read_regular(
                    directory, marker_name, managed=True
                )
                if marker_recheck_raw != current_marker_raw or (
                    marker_recheck_status.st_dev,
                    marker_recheck_status.st_ino,
                ) != (current_marker_status.st_dev, current_marker_status.st_ino):
                    raise LedgerError("migration marker changed before completion")
                complete_check, complete_visible = _read_regular(
                    directory, marker_complete_stage, managed=True, sync=True
                )
                if complete_check != complete_raw or (
                    complete_visible.st_dev,
                    complete_visible.st_ino,
                ) != (complete_status.st_dev, complete_status.st_ino):
                    raise LedgerError("migration completion staging changed")
                os.replace(
                    marker_complete_stage,
                    marker_name,
                    src_dir_fd=directory,
                    dst_dir_fd=directory,
                )
                _hit(failpoint, "migration:completed-renamed")
                _fsync(directory, "review root after completing migration")
            elif current_marker["state"] != "complete":
                raise LedgerError("migration marker is not resumable")
            else:
                _fsync(directory, "review root while resuming completed migration")
            _hit(failpoint, "migration:complete")
            # A final inventory proves source, marker, destination, ownership,
            # and absence/validity of every recognized transaction file.
            final_inventory = _inventory_locked(directory)
            _require_exact_pending(final_inventory, set())
            return _snapshot(directory, target)


def _read_bytes_input(path: str, *, allow_stdin: bool = False) -> bytes:
    if path == "-":
        if not allow_stdin:
            raise LedgerError("stdin is permitted only for prepare")
        raw = sys.stdin.buffer.read(MAX_BYTES + 1)
    else:
        absolute = Path(os.path.abspath(path))
        try:
            directory = _directory_fd(absolute.parent)
        except OSError as error:
            raise LedgerError(f"cannot open input parent {absolute.parent}: {error}") from error
        try:
            raw, _ = _read_regular(directory, _direct_name(absolute.name, "input name"))
        finally:
            os.close(directory)
    if len(raw) > MAX_BYTES:
        raise LedgerError(f"{path} exceeds {MAX_BYTES} bytes")
    return raw


def _read_input(path: str, *, allow_stdin: bool = False) -> dict[str, Any]:
    raw = _read_bytes_input(path, allow_stdin=allow_stdin)
    return _decode(raw, path)


def _print(value: Any) -> None:
    json.dump(value, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description="Manage deterministic delivery ledgers")
    commands = result.add_subparsers(dest="command", required=True)
    init_parser = commands.add_parser(
        "init-root", help="safely create WRAPPER_ROOT/build/reviews"
    )
    init_parser.add_argument("root", metavar="WRAPPER_ROOT", help="existing wrapper root")
    prepare_parser = commands.add_parser("prepare", help="validate and canonicalize JSON")
    prepare_parser.add_argument("input", help="bounded no-follow JSON file, or - for stdin")
    inspect_parser = commands.add_parser("inspect", help="inspect one canonical ledger")
    inspect_parser.add_argument("root", help="initialized review root")
    inspect_parser.add_argument("name", help="direct canonical ledger filename")
    inventory_parser = commands.add_parser(
        "inventory", help="read-only inventory of issue and PR ledgers"
    )
    inventory_parser.add_argument("root", help="initialized review root")
    create_parser = commands.add_parser(
        "create", help="atomically create a strict generation-1 ledger"
    )
    create_parser.add_argument("root", help="initialized review root")
    create_parser.add_argument("input", help="bounded no-follow JSON file")
    cas_parser = commands.add_parser("cas", help="generation/digest/inode CAS update")
    cas_parser.add_argument("root", help="initialized review root")
    cas_parser.add_argument("name", help="direct canonical ledger filename")
    cas_parser.add_argument("input", help="bounded no-follow replacement JSON file")
    cas_parser.add_argument("--expected-generation", required=True, type=int)
    cas_parser.add_argument("--expected-digest", required=True)
    cas_parser.add_argument("--expected-device", required=True, type=int)
    cas_parser.add_argument("--expected-inode", required=True, type=int)
    migration_parser = commands.add_parser(
        "migrate", help="recover one exact legacy or pre-schema report"
    )
    migration_parser.add_argument("root", help="initialized review root")
    migration_parser.add_argument("source", help="direct report filename")
    migration_parser.add_argument("input", help="generation-1 migration-null JSON file")
    migration_parser.add_argument("--kind", choices=sorted(MIGRATION_KINDS), required=True)
    migration_parser.add_argument("--expected-source-digest", required=True)
    reuse_parser = commands.add_parser(
        "check-reuse", help="fail unless recorded artifacts/resources are safely reusable"
    )
    reuse_parser.add_argument("root", help="initialized review root")
    reuse_parser.add_argument("name", help="direct canonical ledger filename")
    reuse_parser.add_argument(
        "--kind", choices=("all", "artifacts", "resources"), default="all"
    )
    body_check_parser = commands.add_parser(
        "body-check", help="validate exact delivery-section marker bytes"
    )
    body_check_parser.add_argument("root", help="initialized review root")
    body_check_parser.add_argument("name", help="direct canonical ledger filename")
    body_check_parser.add_argument("pr_node_id", help="selected PR node ID")
    body_check_parser.add_argument("body", help="bounded no-follow body file")
    body_plan_parser = commands.add_parser(
        "body-plan", help="plan an exact delivery-section byte replacement"
    )
    body_plan_parser.add_argument("root", help="initialized review root")
    body_plan_parser.add_argument("name", help="direct canonical ledger filename")
    body_plan_parser.add_argument("pr_node_id", help="selected PR node ID")
    body_plan_parser.add_argument("body", help="bounded no-follow current body file")
    body_plan_parser.add_argument("section", help="bounded no-follow section payload file")
    recovery_parser = commands.add_parser(
        "body-recovery", help="classify live body digest against durable intent"
    )
    recovery_parser.add_argument("root", help="initialized review root")
    recovery_parser.add_argument("name", help="direct canonical ledger filename")
    recovery_parser.add_argument("pr_node_id", help="selected PR node ID")
    recovery_parser.add_argument("live_digest", help="SHA-256 digest or the word absent")
    recovery_parser.add_argument(
        "live_updated_at", help="exact live UTC updatedAt timestamp or the word absent"
    )
    pr_payload_parser = commands.add_parser(
        "pr-create-payload", help="show one exact durable planned PR body payload"
    )
    pr_payload_parser.add_argument("root", help="initialized review root")
    pr_payload_parser.add_argument("name", help="direct canonical ledger filename")
    pr_payload_parser.add_argument("slot_id", help="unbound planned pull-request slot")
    bind_parser = commands.add_parser(
        "bind-check", help="classify a remote PR against one planned PR slot"
    )
    bind_parser.add_argument("root", help="initialized review root")
    bind_parser.add_argument("name", help="direct canonical ledger filename")
    bind_parser.add_argument("slot_id", help="planned pull-request artifact slot")
    bind_parser.add_argument("input", help="bounded no-follow remote identity JSON file")
    worktree_bind_parser = commands.add_parser(
        "worktree-bind", help="classify retained fresh wrapper worktree evidence"
    )
    worktree_bind_parser.add_argument("root", help="initialized review root")
    worktree_bind_parser.add_argument("name", help="direct canonical ledger filename")
    worktree_bind_parser.add_argument("slot_id", help="deferred primitive worktree slot")
    worktree_bind_parser.add_argument("worktree_list", help="bounded worktree-list JSON")
    worktree_bind_parser.add_argument("safety", help="bounded live safety JSON")
    worktree_bind_parser.add_argument(
        "--create-output", help="optional bounded worktree-create stdout corroboration"
    )
    worktree_observe_parser = commands.add_parser(
        "worktree-observe", help="produce helper-owned live primitive safety evidence"
    )
    worktree_observe_parser.add_argument("root", help="initialized review root")
    worktree_observe_parser.add_argument("name", help="direct canonical ledger filename")
    worktree_observe_parser.add_argument("slot_id", help="deferred primitive worktree slot")
    worktree_observe_parser.add_argument(
        "worktree_list", help="bounded complete wrapper worktree-list JSON"
    )
    worktree_observe_parser.add_argument("observed_at", help="exact UTC observation timestamp")
    worktree_observe_parser.add_argument(
        "--create-output", help="optional bounded worktree-create stdout corroboration"
    )
    worktree_cas_parser = commands.add_parser(
        "worktree-bind-cas", help="atomically live-prove and bind a primitive worktree"
    )
    worktree_cas_parser.add_argument("root", help="initialized review root")
    worktree_cas_parser.add_argument("name", help="direct canonical ledger filename")
    worktree_cas_parser.add_argument("slot_id", help="deferred primitive worktree slot")
    worktree_cas_parser.add_argument("worktree_list", help="bounded worktree-list JSON")
    worktree_cas_parser.add_argument("safety", help="helper-produced bounded safety JSON")
    worktree_cas_parser.add_argument(
        "--create-output", help="optional bounded worktree-create stdout corroboration"
    )
    worktree_cas_parser.add_argument("--expected-generation", required=True, type=int)
    worktree_cas_parser.add_argument("--expected-digest", required=True)
    worktree_cas_parser.add_argument("--expected-device", required=True, type=int)
    worktree_cas_parser.add_argument("--expected-inode", required=True, type=int)
    scope_bind_parser = commands.add_parser(
        "scope-bind", help="classify retained scope-show schema-v1 JSON"
    )
    scope_bind_parser.add_argument("root", help="initialized review root")
    scope_bind_parser.add_argument("name", help="direct canonical ledger filename")
    scope_bind_parser.add_argument("slot_id", help="planned scope resource slot")
    scope_bind_parser.add_argument("scope_show", help="bounded no-follow scope-show JSON")
    scope_bind_parser.add_argument("worktree_list", help="bounded worktree-list JSON")
    scope_bind_parser.add_argument("safety", help="bounded live safety JSON")
    scope_observe_parser = commands.add_parser(
        "scope-observe", help="produce helper-owned live scope safety evidence"
    )
    scope_observe_parser.add_argument("root", help="initialized review root")
    scope_observe_parser.add_argument("name", help="direct canonical ledger filename")
    scope_observe_parser.add_argument("slot_id", help="planned scope resource slot")
    scope_observe_parser.add_argument("scope_show", help="bounded scope-show JSON")
    scope_observe_parser.add_argument("worktree_list", help="bounded worktree-list JSON")
    scope_observe_parser.add_argument("observed_at", help="exact UTC observation timestamp")
    scope_cas_parser = commands.add_parser(
        "scope-bind-cas", help="atomically live-prove and bind a scope worktree"
    )
    scope_cas_parser.add_argument("root", help="initialized review root")
    scope_cas_parser.add_argument("name", help="direct canonical ledger filename")
    scope_cas_parser.add_argument("slot_id", help="planned scope resource slot")
    scope_cas_parser.add_argument("scope_show", help="bounded scope-show JSON")
    scope_cas_parser.add_argument("worktree_list", help="bounded worktree-list JSON")
    scope_cas_parser.add_argument("safety", help="helper-produced bounded safety JSON")
    scope_cas_parser.add_argument("--expected-generation", required=True, type=int)
    scope_cas_parser.add_argument("--expected-digest", required=True)
    scope_cas_parser.add_argument("--expected-device", required=True, type=int)
    scope_cas_parser.add_argument("--expected-inode", required=True, type=int)
    comment_parser = commands.add_parser(
        "comment-check", help="classify a fully paginated marked-comment inventory"
    )
    comment_parser.add_argument("root", help="initialized review root")
    comment_parser.add_argument("name", help="direct canonical ledger filename")
    comment_parser.add_argument("pr_node_id", help="selected PR node ID")
    comment_parser.add_argument("input", help="bounded no-follow comment inventory JSON")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parser().parse_args(argv)
    try:
        if arguments.command == "init-root":
            _print(init_root(arguments.root))
        elif arguments.command == "prepare":
            _print(prepare(_read_input(arguments.input, allow_stdin=True)))
        elif arguments.command == "inspect":
            _print(inspect(arguments.root, arguments.name).json())
        elif arguments.command == "inventory":
            _print(inventory(arguments.root).json())
        elif arguments.command == "create":
            _print(create(arguments.root, _read_input(arguments.input)).json())
        elif arguments.command == "cas":
            _print(
                cas(
                    arguments.root,
                    arguments.name,
                    _read_input(arguments.input),
                    expected_generation=arguments.expected_generation,
                    expected_digest=arguments.expected_digest,
                    expected_device=arguments.expected_device,
                    expected_inode=arguments.expected_inode,
                ).json()
            )
        elif arguments.command == "migrate":
            _print(
                migrate(
                    arguments.root,
                    arguments.source,
                    _read_input(arguments.input),
                    kind=arguments.kind,
                    expected_source_digest=arguments.expected_source_digest,
                ).json()
            )
        elif arguments.command == "check-reuse":
            snapshot = inspect(arguments.root, arguments.name)
            if arguments.kind in {"all", "artifacts"}:
                require_reusable_artifacts(snapshot.document)
            if arguments.kind in {"all", "resources"}:
                require_reusable_resources(snapshot.document)
            _print({"name": snapshot.name, "kind": arguments.kind, "reusable": True})
        elif arguments.command == "body-check":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                check_body_section(
                    snapshot.document,
                    arguments.pr_node_id,
                    _read_bytes_input(arguments.body),
                )
            )
        elif arguments.command == "body-plan":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                {
                    "name": snapshot.name,
                    **describe_body_plan(
                        snapshot.document,
                        arguments.pr_node_id,
                        _read_bytes_input(arguments.body),
                        _read_bytes_input(arguments.section),
                    ),
                }
            )
        elif arguments.command == "body-recovery":
            snapshot = inspect(arguments.root, arguments.name)
            live = None if arguments.live_digest == "absent" else arguments.live_digest
            live_updated_at = (
                None
                if arguments.live_updated_at == "absent"
                else arguments.live_updated_at
            )
            _print(
                {
                    "name": snapshot.name,
                    **classify_body_recovery(
                        snapshot.document,
                        arguments.pr_node_id,
                        live,
                        live_updated_at,
                    ),
                }
            )
        elif arguments.command == "pr-create-payload":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                {
                    "name": snapshot.name,
                    **describe_planned_pr_payload(
                        snapshot.document, arguments.slot_id
                    ),
                }
            )
        elif arguments.command == "bind-check":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                {
                    "name": snapshot.name,
                    "classification": classify_pr_binding(
                        snapshot.document,
                        arguments.slot_id,
                        _read_input(arguments.input),
                    ),
                }
            )
        elif arguments.command == "worktree-bind":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                classify_worktree_output(
                    snapshot.document,
                    arguments.slot_id,
                    _read_bytes_input(arguments.worktree_list),
                    _read_bytes_input(arguments.safety),
                    (
                        None
                        if arguments.create_output is None
                        else _read_bytes_input(arguments.create_output)
                    ),
                )
            )
        elif arguments.command == "worktree-observe":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                observe_primitive_worktree(
                    snapshot.document,
                    arguments.slot_id,
                    _read_bytes_input(arguments.worktree_list),
                    arguments.observed_at,
                    (
                        None
                        if arguments.create_output is None
                        else _read_bytes_input(arguments.create_output)
                    ),
                )
            )
        elif arguments.command == "worktree-bind-cas":
            _print(
                bind_worktree_cas(
                    arguments.root,
                    arguments.name,
                    arguments.slot_id,
                    _read_bytes_input(arguments.worktree_list),
                    _read_bytes_input(arguments.safety),
                    expected_generation=arguments.expected_generation,
                    expected_digest=arguments.expected_digest,
                    expected_device=arguments.expected_device,
                    expected_inode=arguments.expected_inode,
                    create_output_raw=(
                        None
                        if arguments.create_output is None
                        else _read_bytes_input(arguments.create_output)
                    ),
                )
            )
        elif arguments.command == "scope-bind":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                classify_scope_output(
                    snapshot.document,
                    arguments.slot_id,
                    _read_bytes_input(arguments.scope_show),
                    _read_bytes_input(arguments.worktree_list),
                    _read_bytes_input(arguments.safety),
                )
            )
        elif arguments.command == "scope-observe":
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                observe_scope_worktree(
                    snapshot.document,
                    arguments.slot_id,
                    _read_bytes_input(arguments.scope_show),
                    _read_bytes_input(arguments.worktree_list),
                    arguments.observed_at,
                )
            )
        elif arguments.command == "scope-bind-cas":
            _print(
                bind_scope_cas(
                    arguments.root,
                    arguments.name,
                    arguments.slot_id,
                    _read_bytes_input(arguments.scope_show),
                    _read_bytes_input(arguments.worktree_list),
                    _read_bytes_input(arguments.safety),
                    expected_generation=arguments.expected_generation,
                    expected_digest=arguments.expected_digest,
                    expected_device=arguments.expected_device,
                    expected_inode=arguments.expected_inode,
                )
            )
        else:
            snapshot = inspect(arguments.root, arguments.name)
            _print(
                classify_comments(
                    snapshot.document,
                    arguments.pr_node_id,
                    _read_input(arguments.input),
                )
            )
    except (LedgerError, OSError) as error:
        print(f"delivery-ledger: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
