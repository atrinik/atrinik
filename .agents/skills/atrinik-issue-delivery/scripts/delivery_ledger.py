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
import copy
from contextlib import ExitStack, contextmanager, nullcontext
from dataclasses import dataclass
from datetime import datetime, timezone
import fcntl
import hashlib
import importlib
import importlib.abc
import importlib.util
from inspect import Parameter, signature
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
MAX_ARCHIVE_BYTES = 16 * 1024 * 1024
LEDGER_SUFFIX = ".md.ledger.json"
ENTRY_MODES = {"issue", "pr"}
ARTIFACT_KINDS = {"branch", "worktree", "pull_request"}
ARTIFACT_STATES = {"planned", "created", "adopted"}
LEGACY_MIGRATION_KINDS = {"legacy", "legacy-rebind"}
MIGRATION_KINDS = {*LEGACY_MIGRATION_KINDS, "pre-schema"}
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
    r"(?P<operation>-refresh-target|-bind-(?:worktree|scope)-[a-z0-9][a-z0-9._-]{0,127})?"
    r"-g(?P<generation>[0-9]+)-"
    r"from-(?P<digest>[0-9a-f]{64})-to-(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_UPDATE_RECEIPT_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.update-proof"
    r"(?P<operation>-refresh-target|-bind-(?:worktree|scope)-[a-z0-9][a-z0-9._-]{0,127})?"
    r"-g"
    r"(?P<generation>[0-9]+)-from-(?P<digest>[0-9a-f]{64})-"
    r"d(?P<device>[0-9]+)-i(?P<inode>[0-9]+)-"
    r"to-(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_HEAD_CORRECTION_PREDECESSOR_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.correct-target-head-"
    r"(?P<source>[0-9a-f]{64})\.predecessor\.snapshot$"
)
_HEAD_CORRECTION_ERRONEOUS_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.correct-target-head-"
    r"(?P<source>[0-9a-f]{64})\.erroneous\.snapshot$"
)
_HEAD_CORRECTION_STAGE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.correct-target-head-"
    r"(?P<source>[0-9a-f]{64})\.tmp$"
)
_HEAD_CORRECTION_RECEIPT_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.correct-target-head-"
    r"(?P<source>[0-9a-f]{64})\.json$"
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
_RELEASE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.release\.json$"
)
_RELEASE_STAGE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.release-"
    r"(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_ARCHIVE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.archive-"
    r"(?P<ledger>[0-9a-f]{64})\.json$"
)
_ARCHIVE_STAGE_RE = re.compile(
    r"^\.(?P<target>.+\.md\.ledger\.json)\.archive-"
    r"(?P<ledger>[0-9a-f]{64})-to-(?P<candidate>[0-9a-f]{64})\.tmp$"
)
_RECLAIM_RECEIPT_RE = re.compile(
    r"^\.(?P<archive>\..+\.md\.ledger\.json\.archive-[0-9a-f]{64}\.json)"
    r"\.reclaim-(?P<plan>[0-9a-f]{64})\.json$"
)
_RECLAIM_COMPLETE_NAME = ".delivery-ledger-reclaim-complete.json"
_RECLAIM_COMPLETE_STAGE_RE = re.compile(
    r"^\.delivery-ledger-reclaim-complete-(?P<plan>[0-9a-f]{64})\.tmp$"
)
_UNLINK_QUARANTINE = ".delivery-ledger-unlink"
_GH_EXECUTABLE = "/usr/bin/gh"
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
_GITHUB_COORDINATE_TOKEN_RE = re.compile(
    r"(?<![a-z0-9.-])"
    r"(?:(?:[a-z][a-z0-9+.-]*:)?//)?(?:www\.)?github\.com/"
    r"(?P<owner>[a-z0-9-]+)/(?P<repo>[a-z0-9._-]+)/"
    r"(?P<kind>issues?|pulls?)/(?P<number>[^\s<>()`/]+)",
    re.IGNORECASE,
)
_GITHUB_TOKEN_RE = re.compile(
    r"(?<![a-z0-9.-])"
    r"(?:(?:[a-z][a-z0-9+.-]*:)?//)?(?:www\.)?github\.com/"
    r"[^\s<>()`]+",
    re.IGNORECASE,
)
_GITHUB_COORDINATE_PATH_RE = re.compile(r"/(?:issues?|pulls?)(?:/|$)", re.IGNORECASE)
_LEGACY_REPOSITORY_LINE_RE = re.compile(
    r"^- Repository: `(?P<owner>[a-z0-9-]+)/(?P<repo>[a-z0-9._-]+)`$",
    re.IGNORECASE,
)
_LEGACY_HEAD_LINE_RE = re.compile(
    r"^- Remote head: `(?P<branch>[^`]+)` / `(?P<sha>[0-9a-f]{40})`$",
    re.IGNORECASE,
)
_LEGACY_WORKTREE_LINE_RE = re.compile(r"^- Worktree: `(?P<path>[^`]+)`$")
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
_TARGET_REFRESH_TOKEN = object()


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
class _TargetRefreshCapability:
    """In-process authority for one live-proved target-coordinate refresh."""

    token: object
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
    repository_head_commits: tuple[tuple[str, str, str, str], ...]
    worktrees: tuple[str, ...]
    evidence_invalid: bool
    ambiguous: bool

    def json(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "canonical_target": self.canonical_target,
            "digest": self.digest,
            "issues": [list(value) for value in self.issues],
            "pull_requests": [list(value) for value in self.pull_requests],
            "repository_heads": [list(value) for value in self.repository_heads],
            "repository_head_commits": [
                list(value) for value in self.repository_head_commits
            ],
            "worktrees": list(self.worktrees),
            "evidence_invalid": self.evidence_invalid,
            "ambiguous": self.ambiguous,
        }


@dataclass(frozen=True)
class ReleaseRecord:
    name: str
    ledger_name: str
    document: dict[str, Any]
    raw: bytes
    digest: str
    device: int
    inode: int

    def json(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "ledger_name": self.ledger_name,
            "digest": self.digest,
            "device": self.device,
            "inode": self.inode,
            "document": self.document,
        }


@dataclass(frozen=True)
class ArchiveRecord:
    name: str
    ledger_name: str
    document: dict[str, Any]
    raw: bytes
    digest: str
    device: int
    inode: int
    status: os.stat_result

    def json(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "ledger_name": self.ledger_name,
            "digest": self.digest,
            "device": self.device,
            "inode": self.inode,
            "document": self.document,
        }


@dataclass(frozen=True)
class ReclaimRecord:
    name: str
    archive_name: str
    plan: str
    preview: dict[str, Any]
    raw: bytes

    def json(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "archive": self.archive_name,
            "plan_sha256": self.plan,
            "preview": self.preview,
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
class _LiveResourceGuard:
    recheck: Callable[[], None]

    def prove(self) -> None:
        self.recheck()


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
    releases: tuple[ReleaseRecord, ...] = ()
    archives: tuple[ArchiveRecord, ...] = ()
    reclaims: tuple[ReclaimRecord, ...] = ()
    historical_ledgers: tuple[Snapshot, ...] = ()

    def json(self) -> dict[str, Any]:
        return {
            "schema_version": SCHEMA_VERSION,
            "ledgers": [item.json() for item in self.ledgers],
            "pending": [item.json() for item in self.pending],
            "legacy_reports": [item.json() for item in self.legacy_reports],
            "releases": [item.json() for item in self.releases],
            "archives": [item.json() for item in self.archives],
            "reclaims": [item.json() for item in self.reclaims],
            "historical_ledgers": [item.json() for item in self.historical_ledgers],
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


def _decode_value(raw: bytes, context: str, *, limit: int = MAX_BYTES) -> Any:
    if len(raw) > limit:
        raise LedgerError(f"{context} exceeds {limit} bytes")
    try:
        return json.loads(raw.decode("utf-8"), object_pairs_hook=_duplicate_keys)
    except LedgerError:
        raise
    except (UnicodeError, ValueError, RecursionError) as error:
        raise LedgerError(f"invalid JSON in {context}: {error}") from error


def _decode(raw: bytes, context: str, *, limit: int = MAX_BYTES) -> dict[str, Any]:
    value = _decode_value(raw, context, limit=limit)
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


def _release_name(ledger_name: str) -> str:
    return f".{_direct_name(ledger_name, 'release ledger name')}.release.json"


def _release_ledger_identity(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {"name", "ledger_id", "generation", "sha256", "device", "inode"},
        context,
    )
    name = _direct_name(item["name"], f"{context}.name")
    if not name.endswith(LEDGER_SUFFIX):
        raise LedgerError(f"{context}.name is not a canonical ledger name")
    ledger_id = _string(item["ledger_id"], f"{context}.ledger_id", REFERENCE_RE)
    generation = _integer(item["generation"], f"{context}.generation")
    digest = _string(item["sha256"], f"{context}.sha256", SHA256_RE)
    device = _integer(item["device"], f"{context}.device", minimum=0)
    inode = _integer(item["inode"], f"{context}.inode")
    return name, ledger_id, generation, digest, device, inode


def _release_authority(value: Any, context: str) -> tuple[Any, ...]:
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
    if item["kind"] != "explicit-post-merge":
        raise LedgerError(f"{context}.kind must be explicit-post-merge")
    reference = _string(item["reference"], f"{context}.reference", REFERENCE_RE)
    objective = _string(
        item["objective_sha256"], f"{context}.objective_sha256", SHA256_RE
    )
    issued_at = _string(item["issued_at"], f"{context}.issued_at", TIMESTAMP_RE)
    _timestamp_key(issued_at, f"{context}.issued_at")
    actor = _string(item["actor_node_id"], f"{context}.actor_node_id", NODE_RE)
    allowed = _exact(
        item["allowed"], {"ledger_ids", "pull_requests", "issues"}, f"{context}.allowed"
    )
    ledgers = tuple(
        _string(row, f"{context}.allowed.ledger_ids[{index}]", REFERENCE_RE)
        for index, row in enumerate(allowed["ledger_ids"])
    ) if isinstance(allowed["ledger_ids"], list) else ()
    pulls = _sorted_node_ids(
        allowed["pull_requests"], f"{context}.allowed.pull_requests"
    )
    issues = _sorted_node_ids(allowed["issues"], f"{context}.allowed.issues")
    if list(ledgers) != sorted(set(ledgers)) or len(ledgers) != 1:
        raise LedgerError(f"{context}.allowed.ledger_ids must contain one sorted ledger ID")
    return reference, objective, issued_at, actor, ledgers, pulls, issues


def _release_pull(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "repository",
            "number",
            "node_id",
            "state",
            "recorded_head_sha",
            "observed_head_sha",
            "merge_commit_sha",
            "merged_at",
            "ancestry",
        },
        context,
    )
    repository = _repository(item["repository"], f"{context}.repository")
    number = _integer(item["number"], f"{context}.number")
    node = _string(item["node_id"], f"{context}.node_id", NODE_RE)
    if item["state"] != "merged":
        raise LedgerError(f"{context}.state must be merged")
    recorded = _string(
        item["recorded_head_sha"], f"{context}.recorded_head_sha", COMMIT_RE
    )
    observed = _string(
        item["observed_head_sha"], f"{context}.observed_head_sha", COMMIT_RE
    )
    merge = _string(item["merge_commit_sha"], f"{context}.merge_commit_sha", COMMIT_RE)
    merged_at = _string(item["merged_at"], f"{context}.merged_at", TIMESTAMP_RE)
    _timestamp_key(merged_at, f"{context}.merged_at")
    ancestry = _exact(
        item["ancestry"], {"method", "result"}, f"{context}.ancestry"
    )
    if ancestry not in (
        {"method": "git-merge-base-is-ancestor", "result": "ancestor"},
        {"method": "git-squash-change-equivalent", "result": "equivalent"},
    ):
        raise LedgerError(f"{context}.ancestry is not a successful Git ancestry proof")
    if observed != recorded:
        raise LedgerError(f"{context} observed head differs from the recorded head")
    return (*repository, number, node)


def _release_issue(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(value, {"repository", "number", "node_id", "state"}, context)
    repository = _repository(item["repository"], f"{context}.repository")
    number = _integer(item["number"], f"{context}.number")
    node = _string(item["node_id"], f"{context}.node_id", NODE_RE)
    if item["state"] not in {"open", "closed"}:
        raise LedgerError(f"{context}.state must be open or closed")
    return (*repository, number, node)


def _validate_release_document(value: Any, context: str) -> dict[str, Any]:
    item = _exact(
        value,
        {
            "transaction",
            "state",
            "ledger",
            "authority",
            "observed_at",
            "pull_requests",
            "issues",
            "mutation_state",
            "cleanup",
        },
        context,
    )
    if item["transaction"] != "delivery-ledger-release-v1" or item["state"] != "complete":
        raise LedgerError(f"{context} transaction/state is invalid")
    _release_ledger_identity(item["ledger"], f"{context}.ledger")
    _release_authority(item["authority"], f"{context}.authority")
    observed_at = _string(item["observed_at"], f"{context}.observed_at", TIMESTAMP_RE)
    _timestamp_key(observed_at, f"{context}.observed_at")
    pulls = _ordered_unique(
        item["pull_requests"], f"{context}.pull_requests", _release_pull
    )
    if not pulls:
        raise LedgerError(f"{context}.pull_requests must not be empty")
    _ordered_unique(item["issues"], f"{context}.issues", _release_issue)
    mutation = _exact(item["mutation_state"], {"local", "remote"}, f"{context}.mutation_state")
    if mutation != {"local": "idle", "remote": "idle"}:
        raise LedgerError(f"{context} has a delivery-owned mutation in flight")
    cleanup = _exact(
        item["cleanup"], {"policy", "preview_command", "apply_command"}, f"{context}.cleanup"
    )
    if cleanup != {
        "policy": "explicit-preview-first",
        "preview_command": "./atrinik cleanup --dry-run --json",
        "apply_command": "./atrinik cleanup --apply --json",
    }:
        raise LedgerError(
            f"{context}.cleanup does not preserve the explicit preview-first boundary"
        )
    if len(canonical_bytes(item)) > MAX_BYTES:
        raise LedgerError(f"{context} exceeds {MAX_BYTES} bytes")
    return item


def _release_document(
    snapshot: Snapshot, request: Mapping[str, Any], *, live_proof: bool = True
) -> dict[str, Any]:
    request_copy = _decode(canonical_bytes(request), "release request")
    document = {
        "transaction": "delivery-ledger-release-v1",
        "state": "complete",
        "ledger": {
            "name": snapshot.name,
            "ledger_id": snapshot.document["ledger_id"],
            "generation": snapshot.document["generation"],
            "sha256": snapshot.digest,
            "device": snapshot.device,
            "inode": snapshot.inode,
        },
        **request_copy,
    }
    validated = _validate_release_document(document, "release")
    authority = _release_authority(validated["authority"], "release.authority")
    expected_objective = canonical_object_digest(
        {
            "operation": "release",
            "ledger_id": snapshot.document["ledger_id"],
            "ledger_sha256": snapshot.digest,
        }
    )
    if authority[1] != expected_objective:
        raise LedgerError("release authority objective does not bind the current ledger")
    if authority[3] != snapshot.document["actor"]["node_id"]:
        raise LedgerError("release authority actor differs from the delivery actor")
    if authority[4] != (snapshot.document["ledger_id"],):
        raise LedgerError("release authority does not allow exactly this ledger")
    if not _timestamp_is_after(authority[2], snapshot.document["authority"]["issued_at"]):
        raise LedgerError("release authority must postdate active delivery authority")
    if _timestamp_key(validated["observed_at"], "release observed_at") < _timestamp_key(
        authority[2], "release authority issued_at"
    ):
        raise LedgerError("release observation predates release authority")
    expected_pulls: dict[tuple[str, int, str], tuple[dict[str, Any], str]] = {}
    for pull in snapshot.document["selected_prs"]:
        targets = [
            target for target in snapshot.document["targets"]
            if target["repository"] == pull["repository"]
            and target["head"]["branch"] == pull["head_branch"]
        ]
        if len(targets) != 1:
            raise LedgerError("release selected PR lacks one exact target")
        expected_pulls[(pull["repository"]["node_id"], pull["number"], pull["node_id"])] = (
            pull,
            targets[0]["head"]["current_sha"],
        )
    observed_pulls = {
        (row["repository"]["node_id"], row["number"], row["node_id"]): row
        for row in validated["pull_requests"]
    }
    if set(observed_pulls) != set(expected_pulls):
        raise LedgerError("release PR evidence differs from the exact selected PR set")
    for key, (_pull, head_sha) in expected_pulls.items():
        row = observed_pulls[key]
        if row["repository"] != _pull["repository"] or row["recorded_head_sha"] != head_sha:
            raise LedgerError("release PR evidence differs from recorded coordinates")
        if _timestamp_key(row["merged_at"], "release PR merged_at") > _timestamp_key(
            validated["observed_at"], "release observed_at"
        ):
            raise LedgerError("release PR merge time follows its observation")
        if not _timestamp_is_after(authority[2], row["merged_at"]):
            raise LedgerError("release authority must be issued after every selected PR merge")
        if live_proof:
            _prove_release_git(snapshot.document, _pull, row)
    if set(authority[5]) != {pull["node_id"] for pull in snapshot.document["selected_prs"]}:
        raise LedgerError("release authority PR allowlist differs from selected PRs")

    selected_issues: dict[tuple[str, int, str], tuple[dict[str, Any], str]] = {}
    closing_nodes = {row["node_id"] for row in snapshot.document["closing_scope"]}
    issue_rows = list(snapshot.document["issues"]["explicit"])
    if snapshot.document["program"] is not None:
        issue_rows.extend(
            [
                snapshot.document["program"]["master_issue"],
                snapshot.document["program"]["leaf_issue"],
            ]
        )
    for issue in issue_rows:
        key = (issue["repository"]["node_id"], issue["number"], issue["node_id"])
        selected_issues[key] = (issue, "closed" if issue["node_id"] in closing_nodes else "open")
    observed_issues = {
        (row["repository"]["node_id"], row["number"], row["node_id"]): row
        for row in validated["issues"]
    }
    if set(observed_issues) != set(selected_issues):
        raise LedgerError("release issue evidence differs from the exact selected issue set")
    for key, (issue, state) in selected_issues.items():
        if (
            observed_issues[key]["repository"] != issue["repository"]
            or observed_issues[key]["state"] != state
        ):
            raise LedgerError("release issue final state is not the expected state")
    if set(authority[6]) != {
        issue["node_id"] for issue, _state in selected_issues.values()
    }:
        raise LedgerError("release authority issue allowlist differs from selected issues")

    for pull in snapshot.document["selected_prs"]:
        if (
            pull["draft"] is not False
            or pull["draft_intent"] is not None
            or pull["body"]["state"] == "update-planned"
            or pull["comment"]["state"] in {"planned", "in-flight"}
        ):
            raise LedgerError("release is blocked by an in-flight remote intent")
    for artifact in snapshot.document["artifacts"]:
        if artifact["state"] == "planned" or artifact["safety"] != SAFE_ARTIFACT_STATE:
            raise LedgerError("release requires every artifact to be bound and safe")
    for resource in snapshot.document["resources"]:
        current = resource["current"]
        if resource["state"] == "planned":
            continue
        lifecycle = None if current is None else current["lifecycle"]
        unsafe = lifecycle == "running" or (
            lifecycle == "active" and resource["kind"] != "scope"
        )
        if current is None or unsafe:
            raise LedgerError("release is blocked by an active delivery resource")
    # Remote state is the last mutable external observation.  Local Git and
    # wrapper proofs deliberately precede this authenticated final sweep.
    if live_proof:
        _prove_release_github(snapshot.document, validated)
    return validated


def _gh_json(arguments: Sequence[str], context: str) -> Any:
    """Run one bounded authenticated GitHub CLI observation."""

    try:
        executable = os.stat(_GH_EXECUTABLE, follow_symlinks=False)
    except OSError as error:
        raise LedgerError(f"cannot prove {context}: trusted gh is unavailable: {error}") from error
    if (
        not stat.S_ISREG(executable.st_mode)
        or executable.st_mode & (stat.S_IWGRP | stat.S_IWOTH)
    ):
        raise LedgerError("trusted gh executable is not a protected regular file")
    environment = {
        key: os.environ[key]
        for key in (
            "HOME",
            "GH_TOKEN",
            "GITHUB_TOKEN",
            "HTTPS_PROXY",
            "HTTP_PROXY",
            "NO_PROXY",
            "SSL_CERT_FILE",
            "SSL_CERT_DIR",
        )
        if key in os.environ
    }
    environment.update(LC_ALL="C", LANG="C", GH_PAGER="cat", PAGER="cat", NO_COLOR="1")
    try:
        process = subprocess.run(
            [_GH_EXECUTABLE, *arguments],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=30,
            env=environment,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise LedgerError(f"cannot prove {context} through authenticated gh: {error}") from error
    if len(process.stdout) > MAX_BYTES or len(process.stderr) > MAX_BYTES:
        raise LedgerError(f"{context} gh output is not bounded")
    if process.returncode != 0:
        detail = process.stderr.decode("utf-8", "replace").strip()
        raise LedgerError(f"cannot prove {context} through authenticated gh: {detail or process.returncode}")
    return _decode_value(process.stdout, f"{context} gh output")


def _prove_release_github(
    document: Mapping[str, Any], observation: Mapping[str, Any]
) -> None:
    """Re-observe exact terminal GitHub state with the authenticated CLI actor."""

    actor = _gh_json(("api", "--hostname", "github.com", "user"), "release actor")
    if not isinstance(actor, dict) or actor.get("node_id") != document["actor"]["node_id"]:
        raise LedgerError("authenticated GitHub actor differs from release authority")
    for row in observation["pull_requests"]:
        repository = row["repository"]
        live = _gh_json(
            (
                "pr", "view", str(row["number"]),
                "--repo", f"github.com/{repository['owner']}/{repository['name']}",
                "--json", "id,state,isDraft,headRefOid,mergeCommit,mergedAt",
            ),
            f"release PR {row['number']}",
        )
        if not isinstance(live, dict):
            raise LedgerError("release PR observation is not an object")
        merge = live.get("mergeCommit")
        if (
            live.get("id") != row["node_id"]
            or live.get("state") != "MERGED"
            or live.get("isDraft") is not False
            or live.get("headRefOid") != row["observed_head_sha"]
            or not isinstance(merge, dict)
            or merge.get("oid") != row["merge_commit_sha"]
            or live.get("mergedAt") != row["merged_at"]
        ):
            raise LedgerError("authenticated GitHub PR state differs from release evidence")
    for row in observation["issues"]:
        repository = row["repository"]
        live = _gh_json(
            (
                "issue", "view", str(row["number"]),
                "--repo", f"github.com/{repository['owner']}/{repository['name']}",
                "--json", "id,state",
            ),
            f"release issue {row['number']}",
        )
        if (
            not isinstance(live, dict)
            or live.get("id") != row["node_id"]
            or live.get("state") != row["state"].upper()
        ):
            raise LedgerError("authenticated GitHub issue state differs from release evidence")


def _prove_release_git(
    document: Mapping[str, Any],
    pull: Mapping[str, Any],
    observation: Mapping[str, Any],
) -> None:
    """Prove the recorded worktree is quiescent and its head reached the merge."""

    worktrees = [
        slot
        for slot in document["artifacts"]
        if slot["kind"] == "worktree"
        and slot["current"] is not None
        and slot["current"]["repository"] == pull["repository"]
        and slot["current"]["branch"] == pull["head_branch"]
    ]
    if len(worktrees) != 1:
        raise LedgerError("release requires one exact bound worktree per selected PR")
    current = worktrees[0]["current"]
    path = current["path"]
    if path is None:
        raise LedgerError("release worktree lacks a path")
    try:
        descriptor = _directory_fd(Path(path))
    except OSError as error:
        raise LedgerError(f"cannot open release worktree {path}: {error}") from error
    authority: _PinnedGitAuthority | None = None
    try:
        opened = os.fstat(descriptor)
        visible = os.stat(path, follow_symlinks=False)
        _require_trusted_directory(opened, f"release worktree {path}")
        if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
            raise LedgerError("release worktree changed while opening")
        authority = _pin_checkout_git_authority(
            descriptor,
            path,
            "release worktree",
            expected_branch=pull["head_branch"],
            expected_head=observation["recorded_head_sha"],
            require_index=True,
        )
        _, status_raw = _git(
            descriptor,
            (
                "status",
                "--porcelain=v1",
                "--untracked-files=all",
                "--ignore-submodules=none",
            ),
            "release worktree cleanliness",
        )
        if status_raw:
            raise LedgerError("release worktree is dirty")
        method = observation["ancestry"]["method"]
        if method == "git-merge-base-is-ancestor":
            result, _ = _git(
                descriptor,
                (
                    "merge-base",
                    "--is-ancestor",
                    observation["recorded_head_sha"],
                    observation["merge_commit_sha"],
                ),
                "release merge ancestry",
                accepted={0, 1},
            )
            if result != 0:
                raise LedgerError("recorded PR head is not an ancestor of the merge result")
        else:
            _, parents_raw = _git(
                descriptor,
                ("rev-list", "--parents", "-n", "1", observation["merge_commit_sha"]),
                "release squash parents",
            )
            parents = parents_raw.decode("ascii", "strict").strip().split()
            if len(parents) != 2 or parents[0] != observation["merge_commit_sha"]:
                raise LedgerError("squash merge result must have exactly one parent")
            merge_parent = parents[1]
            _, base_raw = _git(
                descriptor,
                ("merge-base", merge_parent, observation["recorded_head_sha"]),
                "release squash merge base",
            )
            merge_base = base_raw.decode("ascii", "strict").strip()
            if COMMIT_RE.fullmatch(merge_base) is None:
                raise LedgerError("squash merge base is invalid")
            diff_arguments = ("diff", "--binary", "--full-index", "--no-ext-diff", "--no-textconv")
            _, head_change = _git(
                descriptor,
                (*diff_arguments, merge_base, observation["recorded_head_sha"], "--"),
                "release recorded squash change",
            )
            _, merged_change = _git(
                descriptor,
                (*diff_arguments, merge_parent, observation["merge_commit_sha"], "--"),
                "release integrated squash change",
            )
            if head_change != merged_change:
                raise LedgerError("squash merge change differs from the recorded PR head")
        authority.recheck()
        after = os.stat(path, follow_symlinks=False)
        if (opened.st_dev, opened.st_ino) != (after.st_dev, after.st_ino):
            raise LedgerError("release worktree changed during proof")
    finally:
        if authority is not None:
            authority.close()
        os.close(descriptor)


def _release_record(name: str, raw: bytes, status: os.stat_result) -> ReleaseRecord:
    document = _validate_release_document(_decode(raw, name), name)
    if raw != canonical_bytes(document):
        raise LedgerError(f"release marker bytes are noncanonical: {name}")
    ledger_name = document["ledger"]["name"]
    if name != _release_name(ledger_name):
        raise LedgerError(f"release marker filename is noncanonical: {name}")
    return ReleaseRecord(
        name, ledger_name, document, raw, byte_digest(raw), status.st_dev, status.st_ino
    )


def _archive_authority(value: Any, context: str) -> tuple[Any, ...]:
    item = _exact(
        value,
        {
            "kind",
            "reference",
            "objective_sha256",
            "issued_at",
            "actor_node_id",
            "allowed_ledger_ids",
        },
        context,
    )
    if item["kind"] != "explicit-post-cleanup":
        raise LedgerError(f"{context}.kind must be explicit-post-cleanup")
    reference = _string(item["reference"], f"{context}.reference", REFERENCE_RE)
    objective = _string(
        item["objective_sha256"], f"{context}.objective_sha256", SHA256_RE
    )
    issued = _string(item["issued_at"], f"{context}.issued_at", TIMESTAMP_RE)
    _timestamp_key(issued, f"{context}.issued_at")
    actor = _string(item["actor_node_id"], f"{context}.actor_node_id", NODE_RE)
    ledger_ids = item["allowed_ledger_ids"]
    if not isinstance(ledger_ids, list):
        raise LedgerError(f"{context}.allowed_ledger_ids must be an array")
    validated = tuple(
        _string(row, f"{context}.allowed_ledger_ids[{index}]", REFERENCE_RE)
        for index, row in enumerate(ledger_ids)
    )
    if list(validated) != sorted(set(validated)) or len(validated) != 1:
        raise LedgerError(f"{context}.allowed_ledger_ids must contain one sorted ledger ID")
    return reference, objective, issued, actor, validated


def _cleanup_observation(
    value: Any, context: str, mode: str
) -> tuple[str, str, str, frozenset[tuple[str, str]]]:
    item = _exact(value, {"output", "observed_at"}, context)
    raw = _inline_payload(item["output"], f"{context}.output", utf8=True)
    output = _decode(raw, f"{context}.output")
    required = {
        "schema_version",
        "mode",
        "scopes",
        "older_than_days",
        "filters",
        "inventory_errors",
        "items",
        "summary",
    }
    allowed = required | {
        "aborted",
        "completed_actions",
        "journal",
        "journal_error",
        "mutated",
        "mutation_attempted",
    }
    if not required.issubset(output) or set(output) - allowed:
        raise LedgerError(f"{context}.output is not a cleanup report")
    if output["schema_version"] != 1 or isinstance(output["schema_version"], bool):
        raise LedgerError(f"{context}.output schema is invalid")
    if output["mode"] != mode:
        raise LedgerError(f"{context}.output mode must be {mode}")
    if output["inventory_errors"] != []:
        raise LedgerError(f"{context}.output contains inventory errors")
    summary = output["summary"]
    if not isinstance(summary, dict) or summary.get("error_count") != 0:
        raise LedgerError(f"{context}.output reports cleanup errors")
    if not isinstance(output["items"], list) or len(output["items"]) > MAX_INVENTORY_ENTRIES:
        raise LedgerError(f"{context}.output items are invalid or oversized")
    disposition = "eligible" if mode == "dry-run" else "removed"
    selected: set[tuple[str, str]] = set()
    for index, row in enumerate(output["items"]):
        if not isinstance(row, dict):
            raise LedgerError(f"{context}.output.items[{index}] is not an object")
        if row.get("disposition") != disposition:
            continue
        kind = _string(
            row.get("kind"), f"{context}.output.items[{index}].kind", REFERENCE_RE
        )
        path = _absolute_path(
            row.get("path"), f"{context}.output.items[{index}].path"
        )
        if (kind, path) in selected:
            raise LedgerError(f"{context}.output contains a duplicate cleanup target")
        selected.add((kind, path))
    if mode == "apply":
        if output.get("aborted", False) is not False:
            raise LedgerError(f"{context}.output cleanup apply was aborted")
        completed = output.get("completed_actions")
        if not isinstance(completed, list):
            raise LedgerError(f"{context}.output lacks completed cleanup actions")
        completed_set: set[tuple[str, str]] = set()
        for index, row in enumerate(completed):
            action = _exact(row, {"kind", "path"}, f"{context}.completed_actions[{index}]")
            completed_set.add(
                (
                    _string(action["kind"], f"{context}.completed kind", REFERENCE_RE),
                    _absolute_path(action["path"], f"{context}.completed path"),
                )
            )
        if completed_set != selected or len(completed_set) != len(completed):
            raise LedgerError(f"{context}.output completed actions differ from removals")
    digest = byte_digest(raw)
    selection = canonical_object_digest(
        [{"kind": kind, "path": path} for kind, path in sorted(selected)]
    )
    observed = _string(item["observed_at"], f"{context}.observed_at", TIMESTAMP_RE)
    _timestamp_key(observed, f"{context}.observed_at")
    return digest, selection, observed, frozenset(selected)


def _archive_worktree(value: Any, context: str) -> tuple[str]:
    item = _exact(value, {"path", "disposition", "safety"}, context)
    path = _absolute_path(item["path"], f"{context}.path")
    if item["disposition"] != "removed":
        raise LedgerError(f"{context}.disposition must be removed")
    safety = _exact(item["safety"], set(SAFE_ARTIFACT_STATE), f"{context}.safety")
    if safety != SAFE_ARTIFACT_STATE:
        raise LedgerError(
            f"{context} was not clean, attached, unlocked, inactive, owned, and certain"
        )
    return (path,)


def _archive_resource(value: Any, context: str) -> tuple[str]:
    item = _exact(value, {"slot_id", "disposition", "lifecycle"}, context)
    slot = _string(item["slot_id"], f"{context}.slot_id", SLOT_RE)
    if item["disposition"] not in {"removed", "retained"}:
        raise LedgerError(f"{context}.disposition is invalid")
    if item["lifecycle"] not in {None, "consumed", "ready", "released", "static", "stopped"}:
        raise LedgerError(f"{context}.lifecycle is active or invalid")
    return (slot,)


def _archive_cleanup(value: Any, context: str) -> dict[str, Any]:
    item = _exact(
        value,
        {"policy", "preview", "apply", "worktrees", "resources"},
        context,
    )
    if item["policy"] != "explicit-preview-first":
        raise LedgerError(f"{context}.policy must be explicit-preview-first")
    preview = _cleanup_observation(
        item["preview"], f"{context}.preview", "dry-run"
    )
    applied = _cleanup_observation(item["apply"], f"{context}.apply", "apply")
    preview_output = _decode(
        _inline_payload(
            item["preview"]["output"], f"{context}.preview.output", utf8=True
        ),
        f"{context}.preview.output",
    )
    apply_output = _decode(
        _inline_payload(
            item["apply"]["output"], f"{context}.apply.output", utf8=True
        ),
        f"{context}.apply.output",
    )
    for field in ("scopes", "older_than_days", "filters"):
        if preview_output[field] != apply_output[field]:
            raise LedgerError(f"{context} preview/apply {field} differ")
    if preview[1] != applied[1]:
        raise LedgerError(f"{context} preview/apply selections differ")
    if preview[3] != applied[3]:
        raise LedgerError(f"{context} preview/apply targets differ")
    if not _timestamp_is_after(applied[2], preview[2]):
        raise LedgerError(f"{context} apply must postdate preview")
    _ordered_unique(item["worktrees"], f"{context}.worktrees", _archive_worktree)
    _ordered_unique(item["resources"], f"{context}.resources", _archive_resource)
    return item


def _cleanup_journal_lease_coordinate(path: Path) -> str:
    digest = hashlib.sha256(str(path).encode("utf-8")).hexdigest()
    return f"cleanup-journal:{digest}"


def _prove_cleanup_journal(
    cleanup: Mapping[str, Any], workspace_root: str
) -> Path:
    """Bind caller-retained cleanup output to the wrapper's durable receipt."""

    raw = _inline_payload(
        cleanup["apply"]["output"], "archive cleanup apply.output", utf8=True
    )
    output = _decode(raw, "archive cleanup apply.output")
    journal_value = output.get("journal")
    if not isinstance(journal_value, str):
        raise LedgerError("archive cleanup apply lacks a journal")
    journal_path = Path(journal_value)
    journal_root = Path(workspace_root) / "cleanup-journals"
    if journal_path.parent != journal_root or journal_path.name in {"", ".", ".."}:
        raise LedgerError("archive cleanup journal escaped the wrapper journal root")
    journal_value = _decode(
        _read_bytes_input(str(journal_path)), "archive cleanup journal"
    )
    if not isinstance(journal_value, dict):
        raise LedgerError("archive cleanup journal is not an object")
    if journal_value.get("schema_version") != 2:
        raise LedgerError("archive cleanup journal must use schema 2")
    fields = {
        "schema_version",
        "started_at",
        "status",
        "request",
        "report",
        "targets",
        "completed",
        "in_flight",
        "finished_at",
        "result",
        "result_sha256",
    }
    status = journal_value.get("status")
    if status == "complete-delivered":
        fields.add("delivered_at")
    journal = _exact(
        journal_value,
        fields,
        "archive cleanup journal",
    )
    if journal["status"] not in {"complete-pending-output", "complete-delivered"}:
        raise LedgerError("archive cleanup journal is not complete")
    if journal["in_flight"] is not None:
        raise LedgerError("archive cleanup journal has an unfinished action")
    if (
        journal["result"] != output
        or canonical_object_digest(output) != journal["result_sha256"]
    ):
        raise LedgerError("archive cleanup journal terminal result differs from apply output")
    for field in ("started_at", "finished_at"):
        _timestamp_key(
            _string(journal[field], f"archive cleanup journal.{field}", TIMESTAMP_RE),
            f"archive cleanup journal.{field}",
        )
    if journal["status"] == "complete-delivered":
        _timestamp_key(
            _string(
                journal["delivered_at"],
                "archive cleanup journal.delivered_at",
                TIMESTAMP_RE,
            ),
            "archive cleanup journal.delivered_at",
        )
    preview_observed = _string(
        cleanup["preview"]["observed_at"],
        "archive cleanup preview.observed_at",
        TIMESTAMP_RE,
    )
    apply_observed = _string(
        cleanup["apply"]["observed_at"],
        "archive cleanup apply.observed_at",
        TIMESTAMP_RE,
    )
    if (
        not _timestamp_is_after(journal["started_at"], preview_observed)
        or not _timestamp_is_after(journal["finished_at"], journal["started_at"])
        or not _timestamp_is_after(apply_observed, journal["finished_at"])
        or journal["status"] == "complete-delivered"
        and (
            not _timestamp_is_after(journal["delivered_at"], journal["finished_at"])
            or not _timestamp_is_after(apply_observed, journal["delivered_at"])
        )
    ):
        raise LedgerError("archive cleanup journal timestamps are out of order")

    request = _exact(
        journal["request"],
        {"scopes", "older_than_days", "filters"},
        "archive cleanup journal.request",
    )
    if request != {
        field: output[field] for field in ("scopes", "older_than_days", "filters")
    }:
        raise LedgerError("archive cleanup journal request differs from the apply report")
    report = journal["report"]
    if not isinstance(report, dict):
        raise LedgerError("archive cleanup journal report is not an object")
    report_required = {
        "schema_version",
        "mode",
        "scopes",
        "older_than_days",
        "filters",
        "inventory_errors",
        "items",
        "summary",
    }
    if (
        not report_required.issubset(report)
        or set(report) - report_required
        or report["schema_version"] != 1
        or isinstance(report["schema_version"], bool)
        or report["mode"] != "apply"
        or any(report[field] != request[field] for field in request)
        or report["inventory_errors"] != []
        or not isinstance(report["summary"], dict)
        or report["summary"].get("error_count") != 0
        or not isinstance(report["items"], list)
        or len(report["items"]) > MAX_INVENTORY_ENTRIES
        or len(report["items"]) != len(output.get("items", []))
    ):
        raise LedgerError("archive cleanup journal report is invalid")

    def actions(value: Any, context: str) -> list[dict[str, str]]:
        if not isinstance(value, list) or len(value) > MAX_INVENTORY_ENTRIES:
            raise LedgerError(f"{context} is invalid or oversized")
        result: list[dict[str, str]] = []
        for index, row in enumerate(value):
            item = _exact(row, {"kind", "path"}, f"{context}[{index}]")
            result.append(
                {
                    "kind": _string(item["kind"], f"{context}.kind", REFERENCE_RE),
                    "path": _absolute_path(item["path"], f"{context}.path"),
                }
            )
        if len({(row["kind"], row["path"]) for row in result}) != len(result):
            raise LedgerError(f"{context} contains duplicate actions")
        return result

    targets = actions(journal["targets"], "archive cleanup journal.targets")
    completed = actions(journal["completed"], "archive cleanup journal.completed")
    reported = actions(
        output.get("completed_actions"), "archive cleanup completed_actions"
    )
    planned_targets: list[dict[str, str]] = []
    for index, (planned, final) in enumerate(
        zip(report["items"], output["items"], strict=True)
    ):
        if not isinstance(planned, dict) or not isinstance(final, dict):
            raise LedgerError("archive cleanup journal report items are invalid")
        planned_identity = (
            _string(
                planned.get("kind"),
                f"archive cleanup plan item[{index}].kind",
                REFERENCE_RE,
            ),
            _absolute_path(
                planned.get("path"),
                f"archive cleanup plan item[{index}].path",
            ),
        )
        final_identity = (
            _string(
                final.get("kind"),
                f"archive cleanup output item[{index}].kind",
                REFERENCE_RE,
            ),
            _absolute_path(
                final.get("path"),
                f"archive cleanup output item[{index}].path",
            ),
        )
        if planned_identity != final_identity:
            raise LedgerError("archive cleanup journal report item identity changed")
        if planned.get("disposition") == "eligible":
            planned_targets.append(
                {"kind": planned_identity[0], "path": planned_identity[1]}
            )
            if final.get("disposition") != "removed":
                raise LedgerError("archive cleanup journal target was not removed")
        elif final.get("disposition") != planned.get("disposition"):
            raise LedgerError("archive cleanup non-target disposition changed")
    target_set = {(row["kind"], row["path"]) for row in targets}
    if (
        completed != reported
        or target_set != {(row["kind"], row["path"]) for row in completed}
        or target_set != {(row["kind"], row["path"]) for row in planned_targets}
    ):
        raise LedgerError("archive cleanup journal differs from the apply report")
    return journal_path


def _archive_request(
    snapshot: Snapshot,
    release: ReleaseRecord,
    value: Mapping[str, Any],
    *,
    live_scope_proof: bool = True,
) -> dict[str, Any]:
    request = _exact(
        _decode(canonical_bytes(value), "archive request"),
        {"authority", "archived_at", "retain_until", "cleanup"},
        "archive request",
    )
    authority = _archive_authority(request["authority"], "archive request.authority")
    archived_at = _string(
        request["archived_at"], "archive request.archived_at", TIMESTAMP_RE
    )
    retain_until = _string(
        request["retain_until"], "archive request.retain_until", TIMESTAMP_RE
    )
    _timestamp_key(archived_at, "archive request.archived_at")
    if not _timestamp_is_after(retain_until, archived_at):
        raise LedgerError("archive retention must end after archival")
    cleanup = _archive_cleanup(request["cleanup"], "archive request.cleanup")
    if _timestamp_key(archived_at, "archive request.archived_at") < _timestamp_key(
        cleanup["apply"]["observed_at"], "archive cleanup apply observed_at"
    ):
        raise LedgerError("archive time predates cleanup apply")
    expected_objective = canonical_object_digest(
        {
            "operation": "archive",
            "ledger_id": snapshot.document["ledger_id"],
            "release_sha256": release.digest,
        }
    )
    if authority[1] != expected_objective:
        raise LedgerError("archive authority objective does not bind the release")
    if authority[3] != snapshot.document["actor"]["node_id"] or authority[4] != (
        snapshot.document["ledger_id"],
    ):
        raise LedgerError("archive authority is unrelated to the released ledger")
    if not _timestamp_is_after(authority[2], release.document["observed_at"]):
        raise LedgerError("archive authority must postdate terminal release evidence")
    if not _timestamp_is_after(authority[2], cleanup["apply"]["observed_at"]):
        raise LedgerError("archive authority must be issued after cleanup apply")
    if _timestamp_key(archived_at, "archive time") < _timestamp_key(
        authority[2], "archive authority"
    ):
        raise LedgerError("archive time predates archive authority")

    expected_worktrees = {
        (slot["current"] or slot["immutable"])["path"]
        for slot in snapshot.document["artifacts"]
        if slot["kind"] == "worktree"
    }
    observed_worktrees = {row["path"] for row in cleanup["worktrees"]}
    if observed_worktrees != expected_worktrees:
        raise LedgerError("archive cleanup worktrees differ from the ledger")
    cleanup_selection = _cleanup_observation(
        cleanup["apply"], "archive request.cleanup.apply", "apply"
    )[3]
    expected_resources = {row["slot_id"]: row for row in snapshot.document["resources"]}
    observed_resources = {row["slot_id"]: row for row in cleanup["resources"]}
    if set(observed_resources) != set(expected_resources):
        raise LedgerError("archive cleanup resources differ from the ledger")
    for slot_id, resource in expected_resources.items():
        lifecycle = None if resource["current"] is None else resource["current"]["lifecycle"]
        observed = observed_resources[slot_id]
        released_scope = (
            resource["kind"] == "scope"
            and lifecycle == "active"
            and observed == {
                "slot_id": slot_id,
                "disposition": "removed",
                "lifecycle": "released",
            }
        )
        if released_scope and live_scope_proof:
            _prove_scope_release(resource, f"archive resource {slot_id}")
        if not released_scope and (
            lifecycle in {"active", "running"}
            or observed["lifecycle"] != lifecycle
        ):
            raise LedgerError("archive cleanup resource lifecycle is unsafe or mismatched")
    scope_released_worktrees = {
        (slot["current"] or slot["immutable"])["path"]
        for slot in snapshot.document["artifacts"]
        if slot["kind"] == "worktree"
        and slot.get("producer_resource_slot") is not None
        and any(
            resource["slot_id"] == slot["producer_resource_slot"]
            and resource["kind"] == "scope"
            and resource["current"] is not None
            and resource["current"]["lifecycle"] == "active"
            and observed_resources[resource["slot_id"]]
            == {
                "slot_id": resource["slot_id"],
                "disposition": "removed",
                "lifecycle": "released",
            }
            for resource in snapshot.document["resources"]
        )
    }
    expected_cleanup_targets = {
        ("worktree", path)
        for path in expected_worktrees
        if path not in scope_released_worktrees
    }
    expected_cleanup_targets.update(
        (resource["kind"], resource["current"]["path"])
        for slot_id, resource in expected_resources.items()
        if observed_resources[slot_id]["disposition"] == "removed"
        and resource["kind"] != "scope"
        and resource["current"] is not None
        and resource["current"]["path"] is not None
    )
    if cleanup_selection != expected_cleanup_targets:
        raise LedgerError("raw cleanup output does not match exact ledger-owned targets")
    return request


def _scope_release_coordinate(item: Mapping[str, Any]) -> tuple[Any, ...]:
    kind = item.get("kind")
    if kind == "worktree":
        return (kind, item.get("checkout"), item.get("path"))
    return (kind, item.get("path"))


def _validate_scope_release_plan_coverage(
    record: Mapping[str, Any], plan: Mapping[str, Any], context: str
) -> None:
    """Bind a release plan to every immutable non-build scope coordinate."""

    items = plan["items"]
    actual: list[tuple[Any, ...]] = []
    build_paths: set[str] = set()
    build_parent = Path(record["profile"]["path"]).parent.parent / "build" / "profiles"
    build_name = re.compile(
        re.escape(record["profile"]["name"]) + r"-[0-9a-f]{64}"
    )
    for item in items:
        if not isinstance(item, dict):
            raise LedgerError(f"{context} scope release plan is unsafe")
        kind = item.get("kind")
        if kind == "build":
            path = item.get("path")
            root = Path(path) if isinstance(path, str) else None
            if (
                root is None
                or root.parent != build_parent
                or not build_name.fullmatch(root.name)
                or path in build_paths
            ):
                raise LedgerError(f"{context} scope release plan coordinates are invalid")
            build_paths.add(path)
            continue
        if kind not in {"profile", "worktree", "topology", "state"}:
            raise LedgerError(f"{context} scope release plan coordinates are invalid")
        actual.append(_scope_release_coordinate(item))
    expected = [
        ("topology", record["topology"]["path"]),
        ("state", None),
        ("profile", record["profile"]["path"]),
        *[
            ("worktree", row["checkout"], row["path"])
            for row in record["worktrees"]
        ],
    ]
    if len(actual) != len(set(actual)) or set(actual) != set(expected):
        raise LedgerError(f"{context} scope release plan omits or adds scope coordinates")


def _prove_fresh_scope_release(
    workspace: Any,
    record: Mapping[str, Any],
    plan: Mapping[str, Any],
    context: str,
) -> None:
    fresh = workspace._scope_release_live_plan(record["name"])
    if (
        not isinstance(fresh, dict)
        or fresh.get("scope") != record["name"]
        or fresh.get("generation") != record["generation"]
        or not isinstance(fresh.get("items"), list)
    ):
        raise LedgerError(f"{context} fresh scope release observation is invalid")
    _validate_scope_release_plan_coverage(record, fresh, context)
    if any(item.get("kind") == "build" for item in fresh["items"]):
        raise LedgerError(f"{context} released scope gained a profile build")
    live_items = {
        _scope_release_coordinate(item): item for item in fresh["items"]
    }
    for item in plan["items"]:
        if item.get("kind") == "build":
            continue
        live_item = live_items[_scope_release_coordinate(item)]
        if item.get("kind") in {"profile", "worktree"}:
            if live_item.get("disposition") != "absent":
                raise LedgerError(f"{context} released scope coordinate reappeared")
        elif live_item != item:
            raise LedgerError(f"{context} retained scope coordinate changed")


def _prove_scope_release(resource: Mapping[str, Any], context: str) -> None:
    """Verify the scope subsystem's durable completed release journal."""

    current = resource["current"]
    raw = _retained_result(current["binding"], f"{context}.binding")
    record, _row = _scope_show_record(
        raw,
        resource["request"],
        resource["immutable"]["repository"],
        f"{context}.scope_show",
    )
    journal_path = record["cleanup"]["release_journal"]
    journal = _decode(_read_bytes_input(journal_path), f"{context}.release_journal")
    journal = _exact(
        journal,
        {
            "schema_version",
            "scope",
            "generation",
            "plan_sha256",
            "plan",
            "status",
            "completed",
            "in_flight",
            "pending_builds",
            "updated_at",
        },
        f"{context}.release_journal",
    )
    if (
        journal["schema_version"] != 1
        or journal["scope"] != record["name"]
        or journal["generation"] != record["generation"]
        or journal["status"] != "complete"
        or journal["in_flight"] is not None
    ):
        raise LedgerError(f"{context} scope release journal is not complete and exact")
    _string(journal["plan_sha256"], f"{context}.release_journal.plan", SHA256_RE)
    _string(journal["updated_at"], f"{context}.release_journal.updated_at", TIMESTAMP_RE)
    _timestamp_key(journal["updated_at"], f"{context}.release_journal.updated_at")
    plan = _exact(
        journal["plan"],
        {"schema_version", "scope", "generation", "items"},
        f"{context}.release_journal.plan",
    )
    if (
        plan["schema_version"] != 1
        or plan["scope"] != record["name"]
        or plan["generation"] != record["generation"]
        or not isinstance(plan["items"], list)
        or canonical_object_digest(plan) != journal["plan_sha256"]
    ):
        raise LedgerError(f"{context} scope release plan is invalid")
    _validate_scope_release_plan_coverage(record, plan, context)
    expected_completed: list[str] = []
    expected_pending_builds: list[dict[str, Any]] = []
    completed_worktrees: list[str] = []
    for item in plan["items"]:
        if (
            not isinstance(item, dict)
            or item.get("disposition") not in {"eligible", "absent", "retained"}
        ):
            raise LedgerError(f"{context} scope release plan is unsafe")
        if item.get("disposition") != "eligible":
            continue
        if item.get("kind") == "build" and isinstance(item.get("path"), str):
            expected_completed.append(f"build:{item['path']}")
            expected_pending_builds.append(
                {
                    key: item.get(key)
                    for key in (
                        "path",
                        "device",
                        "inode",
                        "metadata_sha256",
                        "marker_sha256",
                    )
                }
            )
        elif item.get("kind") == "profile":
            expected_completed.append("profile")
        elif item.get("kind") == "worktree" and isinstance(item.get("checkout"), str):
            completed_worktrees.append(f"worktree:{item['checkout']}")
    expected_completed.extend(reversed(completed_worktrees))
    expected_pending_builds.sort(key=lambda row: row["path"])
    if (
        not isinstance(journal["completed"], list)
        or not all(isinstance(row, str) and row for row in journal["completed"])
        or len(journal["completed"]) != len(set(journal["completed"]))
        or journal["completed"] != expected_completed
    ):
        raise LedgerError(f"{context} scope release journal actions are invalid")
    if journal["pending_builds"] != expected_pending_builds:
        raise LedgerError(f"{context} scope release build intents are invalid")
    request = resource["request"]
    wrapper_root = request["roots"]["wrapper"]["path"]
    workspace_root = request["roots"]["workspace"]["path"]
    saved = _enter_workspace_environment(workspace_root)
    workspace = None
    try:
        module = _load_workspace_module(wrapper_root)
        workspace = module.Workspace(Path(wrapper_root), backfill_references=False)
        _prove_fresh_scope_release(workspace, record, plan, context)
        for item in plan["items"]:
            if item.get("disposition") != "eligible":
                continue
            path = item.get("path")
            if not isinstance(path, str) or Path(path).exists() or Path(path).is_symlink():
                raise LedgerError(f"{context} released scope coordinate still exists: {path}")
    except LedgerError:
        raise
    except Exception as error:
        raise LedgerError(f"{context} cannot prove live scope release: {error}") from error
    finally:
        if workspace is not None:
            workspace.close()
        _leave_workspace_environment(saved)


def _archive_member(name: str, raw: bytes, status: os.stat_result) -> dict[str, Any]:
    _require_trusted_regular(status, f"archive member {name}")
    return {
        "name": _direct_name(name, "archive member name"),
        "mode": stat.S_IMODE(status.st_mode),
        "device": status.st_dev,
        "inode": status.st_ino,
        "sha256": byte_digest(raw),
        "raw_base64": base64.b64encode(raw).decode("ascii"),
    }


def _archive_member_bytes(value: Any, context: str) -> tuple[str, bytes, int, int, int]:
    item = _exact(
        value,
        {"name", "mode", "device", "inode", "sha256", "raw_base64"},
        context,
    )
    name = _direct_name(item["name"], f"{context}.name")
    mode = _integer(item["mode"], f"{context}.mode", minimum=0)
    if mode > 0o777 or mode & 0o022:
        raise LedgerError(f"{context}.mode is unsafe")
    device = _integer(item["device"], f"{context}.device", minimum=0)
    inode = _integer(item["inode"], f"{context}.inode")
    digest = _string(item["sha256"], f"{context}.sha256", SHA256_RE)
    encoded = item["raw_base64"]
    if not isinstance(encoded, str) or len(encoded) > MAX_ARCHIVE_BYTES:
        raise LedgerError(f"{context}.raw_base64 is invalid")
    try:
        raw = base64.b64decode(encoded.encode("ascii"), validate=True)
    except (UnicodeError, ValueError, binascii.Error) as error:
        raise LedgerError(f"{context}.raw_base64 is invalid") from error
    if base64.b64encode(raw).decode("ascii") != encoded or byte_digest(raw) != digest:
        raise LedgerError(f"{context} bytes are noncanonical or mismatched")
    return name, raw, mode, device, inode


def _validate_archived_head_correction(
    snapshot: Snapshot,
    members: Sequence[tuple[str, bytes, int, int, int]],
    context: str,
) -> frozenset[str]:
    """Validate one complete correction evidence set embedded in an archive."""

    patterns = (
        ("predecessor", _HEAD_CORRECTION_PREDECESSOR_RE),
        ("erroneous", _HEAD_CORRECTION_ERRONEOUS_RE),
        ("stage", _HEAD_CORRECTION_STAGE_RE),
        ("receipt", _HEAD_CORRECTION_RECEIPT_RE),
    )
    evidence: dict[str, tuple[str, str, bytes, int, int, int]] = {}
    for name, raw, mode, device, inode in members:
        matched = next(
            (
                (kind, match)
                for kind, pattern in patterns
                if (match := pattern.fullmatch(name)) is not None
                and match.group("target") == snapshot.name
            ),
            None,
        )
        if matched is None:
            continue
        kind, match = matched
        if kind in evidence:
            raise LedgerError(f"{context} has duplicate correction {kind} evidence")
        evidence[kind] = (name, match.group("source"), raw, mode, device, inode)
    if not evidence:
        return frozenset()
    if set(evidence) != {"predecessor", "erroneous", "receipt"}:
        raise LedgerError(f"{context} has incomplete correction evidence")
    predecessor_name, source, predecessor_raw, _mode, predecessor_device, predecessor_inode = evidence[
        "predecessor"
    ]
    erroneous_name, erroneous_source, erroneous_raw, _mode, erroneous_device, erroneous_inode = evidence[
        "erroneous"
    ]
    receipt_name, receipt_source, receipt_raw, _mode, _device, _inode = evidence[
        "receipt"
    ]
    if len({source, erroneous_source, receipt_source}) != 1:
        raise LedgerError(f"{context} correction source digests differ")
    predecessor = validate(_decode(predecessor_raw, predecessor_name))
    erroneous = validate(_decode(erroneous_raw, erroneous_name))
    receipt = _head_correction_receipt(_decode(receipt_raw, receipt_name), receipt_name)
    if (
        predecessor_raw != canonical_bytes(predecessor)
        or erroneous_raw != canonical_bytes(erroneous)
        or receipt_raw != canonical_bytes(receipt)
        or byte_digest(erroneous_raw) != source
        or receipt["source"]
        != {
            "generation": erroneous["generation"],
            "sha256": source,
            "device": erroneous_device,
            "inode": erroneous_inode,
        }
        or receipt["predecessor_snapshot"]
        != {
            "name": predecessor_name,
            "sha256": byte_digest(predecessor_raw),
            "device": predecessor_device,
            "inode": predecessor_inode,
        }
        or receipt["erroneous_snapshot"]
        != {
            "name": erroneous_name,
            "sha256": source,
            "device": erroneous_device,
            "inode": erroneous_inode,
        }
    ):
        raise LedgerError(f"{context} correction member identity differs")
    corrected, metadata, _ = _head_correction_document(
        predecessor,
        erroneous,
        source,
        bad_head=receipt["bad_head"],
        actual_head=receipt["actual_head"],
        actual_merge_base=receipt.get("actual_merge_base"),
    )
    corrected_raw = canonical_bytes(corrected)
    corrected_digest = byte_digest(corrected_raw)
    expected_intent = _head_correction_intent(
        erroneous,
        snapshot.name,
        receipt["source"],
        byte_digest(predecessor_raw),
        metadata,
        bad_head=receipt["bad_head"],
        actual_head=receipt["actual_head"],
        actual_merge_base=receipt.get("actual_merge_base"),
    )
    _require_head_correction_recovery(
        receipt["recovery"], erroneous, expected_intent, context
    )
    generation = corrected["generation"]
    current = snapshot.document
    if not (
        (current["generation"] == generation and snapshot.raw == corrected_raw)
        or (
            current["generation"] > generation
            and len(current["history"]) >= generation
            and current["history"][generation - 1] == corrected_digest
        )
    ) or receipt["correction"] != {
        "generation": generation,
        "sha256": corrected_digest,
    }:
        raise LedgerError(f"{context} correction is absent from ledger history")
    if (
        receipt["repository"] != metadata["repository"]
        or receipt["branch"] != metadata["branch"]
        or receipt["worktree"] != metadata["worktree"]
        or receipt["predecessor_head"] != metadata["predecessor_head"]
        or receipt["base_head"] != metadata["base_head"]
        or receipt["merge_base"] != metadata["merge_base"]
        or receipt.get("actual_merge_base") != metadata.get("actual_merge_base")
        or receipt["authority_sha256"] != metadata["authority_sha256"]
    ):
        raise LedgerError(f"{context} correction receipt semantics differ")
    return frozenset(row[0] for row in evidence.values())


def _validate_archive_document(value: Any, context: str) -> dict[str, Any]:
    item = _exact(
        value,
        {
            "transaction",
            "state",
            "ledger",
            "release_sha256",
            "authority",
            "archived_at",
            "retain_until",
            "cleanup",
            "members",
        },
        context,
    )
    if item["transaction"] != "delivery-ledger-archive-v1" or item["state"] != "complete":
        raise LedgerError(f"{context} transaction/state is invalid")
    ledger_identity = _release_ledger_identity(item["ledger"], f"{context}.ledger")
    _string(item["release_sha256"], f"{context}.release_sha256", SHA256_RE)
    _archive_authority(item["authority"], f"{context}.authority")
    _string(item["archived_at"], f"{context}.archived_at", TIMESTAMP_RE)
    _string(item["retain_until"], f"{context}.retain_until", TIMESTAMP_RE)
    _archive_cleanup(item["cleanup"], f"{context}.cleanup")
    if not isinstance(item["members"], list) or not item["members"]:
        raise LedgerError(f"{context}.members must be a non-empty array")
    members = [
        _archive_member_bytes(row, f"{context}.members[{index}]")
        for index, row in enumerate(item["members"])
    ]
    names = [row[0] for row in members]
    if names != sorted(set(names), key=str.casefold):
        raise LedgerError(f"{context}.members must have unique sorted names")
    by_name = {name: row for name, *row in members}
    ledger_member = by_name.get(ledger_identity[0])
    release_member = by_name.get(_release_name(ledger_identity[0]))
    if ledger_member is None or release_member is None:
        raise LedgerError(f"{context} lost its canonical ledger or release marker")
    ledger_raw, _ledger_mode, ledger_device, ledger_inode = ledger_member
    release_raw, _release_mode, release_device, release_inode = release_member
    if (
        byte_digest(ledger_raw) != ledger_identity[3]
        or byte_digest(release_raw) != item["release_sha256"]
    ):
        raise LedgerError(f"{context} member identities do not match archive metadata")
    ledger_document = _decode(ledger_raw, f"{context} ledger member")
    validate(ledger_document)
    if (
        ledger_raw != canonical_bytes(ledger_document)
        or canonical_name(ledger_document) != ledger_identity[0]
        or ledger_document["ledger_id"] != ledger_identity[1]
        or ledger_document["generation"] != ledger_identity[2]
        or (ledger_device, ledger_inode) != (ledger_identity[4], ledger_identity[5])
    ):
        raise LedgerError(f"{context} canonical ledger member is mismatched")
    release_document = _validate_release_document(
        _decode(release_raw, f"{context} release member"),
        f"{context} release member",
    )
    if release_raw != canonical_bytes(release_document):
        raise LedgerError(f"{context} release member is noncanonical")
    archived_snapshot = Snapshot(
        ledger_identity[0],
        ledger_document,
        ledger_raw,
        ledger_identity[3],
        ledger_identity[4],
        ledger_identity[5],
    )
    correction_members = _validate_archived_head_correction(
        archived_snapshot, members, context
    )
    allowed_members = {
        ledger_identity[0],
        _release_name(ledger_identity[0]),
        f".{ledger_identity[0]}.lock",
        ledger_identity[0].removesuffix(".ledger.json"),
        *correction_members,
    }
    migration = ledger_document["migration"]
    if migration is not None:
        allowed_members.update(
            {
                migration["source"]["name"],
                migration["snapshot"]["name"],
                migration["canonical_report"],
                migration["marker_name"],
            }
        )
    unexpected_members = set(names) - allowed_members
    if unexpected_members:
        raise LedgerError(
            f"{context} has unrelated archive members: "
            + ", ".join(sorted(unexpected_members, key=str.casefold))
        )
    release_request = {
        key: release_document[key]
        for key in (
            "authority",
            "observed_at",
            "pull_requests",
            "issues",
            "mutation_state",
            "cleanup",
        )
    }
    if _release_document(
        archived_snapshot, release_request, live_proof=False
    ) != release_document:
        raise LedgerError(f"{context} release member does not bind the archived ledger")
    archived_release = ReleaseRecord(
        _release_name(ledger_identity[0]),
        ledger_identity[0],
        release_document,
        release_raw,
        item["release_sha256"],
        release_device,
        release_inode,
    )
    archive_request = {
        key: item[key]
        for key in ("authority", "archived_at", "retain_until", "cleanup")
    }
    if _archive_request(
        archived_snapshot,
        archived_release,
        archive_request,
        live_scope_proof=False,
    ) != archive_request:
        raise LedgerError(f"{context} request does not bind the archived ledger")
    if len(canonical_bytes(item)) > MAX_ARCHIVE_BYTES:
        raise LedgerError(f"{context} exceeds {MAX_ARCHIVE_BYTES} bytes")
    return item


def _archive_record(name: str, raw: bytes, status: os.stat_result) -> ArchiveRecord:
    document = _validate_archive_document(
        _decode(raw, name, limit=MAX_ARCHIVE_BYTES), name
    )
    if raw != canonical_bytes(document):
        raise LedgerError(f"archive bytes are noncanonical: {name}")
    ledger_name = document["ledger"]["name"]
    expected = f".{ledger_name}.archive-{document['ledger']['sha256']}.json"
    if name != expected:
        raise LedgerError(f"archive filename is noncanonical: {name}")
    return ArchiveRecord(
        name,
        ledger_name,
        document,
        raw,
        byte_digest(raw),
        status.st_dev,
        status.st_ino,
        status,
    )


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


def _utc_now() -> str:
    """Return one helper-owned current UTC observation."""

    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


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
        # Candidate wrapper compatibility code must never execute before the
        # candidate is independently proven to be the requested clean tree.
        # The full guard repeats this proof after the workspace lease exists.
        _prove_live_worktree_core(request, path, descriptors)
        allowed = frozenset(allowed_references)
        with _workspace_safety_lease(
            request,
            path,
            allowed,
            context,
            wrapper_directory=descriptors["wrapper"],
            workspace_directory=descriptors["workspace"],
            worktree_directory=descriptors["worktree"],
            scope_record=scope_record,
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


def _profile_inventory_snapshot(
    directory: int, context: str
) -> tuple[tuple[tuple[Any, ...], ...], tuple[tuple[str, bytes], ...]]:
    """Bind the bounded profile-directory namespace and every JSON profile."""

    names: list[str] = []
    try:
        with os.scandir(directory) as entries:
            for entry in entries:
                if len(names) == 4096:
                    raise LedgerError(f"{context} profile inventory is oversized")
                names.append(entry.name)
    except OSError as error:
        raise LedgerError(f"{context} cannot enumerate profile inventory: {error}") from error
    rows: list[tuple[Any, ...]] = []
    profiles: list[tuple[str, bytes]] = []
    total_json_bytes = 0
    for name in sorted(names):
        try:
            status = os.stat(name, dir_fd=directory, follow_symlinks=False)
        except OSError as error:
            raise LedgerError(
                f"{context} profile inventory entry changed: {name}: {error}"
            ) from error
        digest = None
        if name.endswith(".json"):
            if not stat.S_ISREG(status.st_mode) or status.st_nlink != 1:
                raise LedgerError(f"{context} profile inventory identity is unsafe")
            if total_json_bytes + status.st_size > MAX_INVENTORY_BYTES:
                raise LedgerError(f"{context} profile inventory is oversized")
            raw, status = _read_regular(directory, name)
            if status.st_nlink != 1:
                raise LedgerError(f"{context} profile inventory identity is unsafe")
            if total_json_bytes + len(raw) > MAX_INVENTORY_BYTES:
                raise LedgerError(f"{context} profile inventory is oversized")
            total_json_bytes += len(raw)
            digest = byte_digest(raw)
            profiles.append((name, raw))
        rows.append(
            (
                name,
                status.st_dev,
                status.st_ino,
                status.st_mode,
                status.st_nlink,
                status.st_size,
                status.st_mtime_ns,
                digest,
            )
        )
    return tuple(rows), tuple(profiles)


@contextmanager
def _workspace_safety_lease(
    request: Mapping[str, Any],
    path: str,
    allowed_references: frozenset[str],
    context: str,
    *,
    wrapper_directory: int,
    workspace_directory: int,
    worktree_directory: int,
    scope_record: Mapping[str, Any] | None = None,
) -> Iterator[Callable[[], None]]:
    """Use wrapper leases/reference logic to prove inactive, owned reuse."""

    wrapper_root = request["roots"]["wrapper"]["path"]
    workspace_root = request["roots"]["workspace"]["path"]
    saved_environment = _enter_workspace_environment(workspace_root)
    workspace = None
    profiles_directory = None
    profiles_snapshot = None
    try:
        manifest_raw, manifest_status = _read_regular(
            wrapper_directory, "components.json"
        )
        _require_trusted_regular(
            manifest_status, f"{context} workspace manifest authority"
        )
        manifest_identity = (manifest_status.st_dev, manifest_status.st_ino)

        def recheck_manifest() -> None:
            current_raw, current_status = _read_regular(
                wrapper_directory, "components.json"
            )
            _require_trusted_regular(
                current_status, f"{context} workspace manifest authority"
            )
            if (
                current_raw != manifest_raw
                or (current_status.st_dev, current_status.st_ino)
                != manifest_identity
            ):
                raise LedgerError(
                    f"{context} workspace manifest authority changed during live proof"
                )

        module = _load_workspace_module(wrapper_root)
        source_references = getattr(module.Workspace, "_source_references", None)
        source_reference_parameters = (
            signature(source_references).parameters
            if source_references is not None
            else {
                "profiles_directory_fd": None,
                "profiles_directory_absent": None,
            }
        )
        supports_reference_authority = {
            "profiles_directory_fd",
            "profiles_directory_absent",
            "profiles_inventory",
        }.issubset(source_reference_parameters) or any(
            parameter is not None and parameter.kind is Parameter.VAR_KEYWORD
            for parameter in source_reference_parameters.values()
        )
        if not supports_reference_authority:
            wrapper_self = (
                request["component"] == "atrinik"
                and request["physical_checkout"] == "atrinik"
                and request["repository"]["owner"] == "atrinik"
                and request["repository"]["name"] == "atrinik"
                and request["roots"]["primary"] == request["roots"]["wrapper"]
                and path != wrapper_root
            )
            if not wrapper_self:
                raise LedgerError(
                    f"{context} primary wrapper lacks profile inventory authority"
                )
            module = _load_workspace_module_from_git(
                path,
                worktree_directory,
                request["expected_head_sha"],
            )
        recheck_manifest()
        retained_manifest = module.Manifest.from_value(
            _decode(manifest_raw, f"{context} retained workspace manifest")
        )
        workspace_parameters = signature(module.Workspace).parameters
        supports_manifest = "manifest" in workspace_parameters or any(
            parameter.kind is Parameter.VAR_KEYWORD
            for parameter in workspace_parameters.values()
        )
        if supports_manifest:
            workspace = module.Workspace(
                Path(wrapper_root),
                backfill_references=False,
                manifest=retained_manifest,
            )
        else:
            workspace = module.Workspace(
                Path(wrapper_root),
                backfill_references=False,
            )
            workspace.manifest = retained_manifest
        recheck_manifest()
    except Exception as error:
        try:
            if workspace is not None:
                workspace.close()
        finally:
            _leave_workspace_environment(saved_environment)
        raise LedgerError(f"{context} cannot establish wrapper safety proof: {error}") from error
    try:
        if (
            str(workspace.paths.repository) != wrapper_root
            or str(workspace.paths.workspace) != workspace_root
        ):
            raise LedgerError(f"{context} wrapper/workspace roots differ from live request")
        profiles_path = Path(workspace_root) / "profiles"
        try:
            profiles_directory = _open_trusted_child_directory(
                workspace_directory,
                "profiles",
                str(profiles_path),
                f"{context} profiles root",
            )
            profiles_absent = False
            profiles_snapshot = _profile_inventory_snapshot(
                profiles_directory, context
            )
        except LedgerError:
            try:
                os.stat(
                    "profiles",
                    dir_fd=workspace_directory,
                    follow_symlinks=False,
                )
            except FileNotFoundError:
                profiles_absent = True
            else:
                raise
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
                        recheck_manifest()
                        if profiles_directory is None:
                            try:
                                os.stat(
                                    "profiles",
                                    dir_fd=workspace_directory,
                                    follow_symlinks=False,
                                )
                            except FileNotFoundError:
                                pass
                            else:
                                raise LedgerError(
                                    f"{context} profiles root appeared during live proof"
                                )
                        else:
                            _recheck_pinned_directory(
                                profiles_directory,
                                str(profiles_path),
                                f"{context} profiles root",
                            )
                            if (
                                _profile_inventory_snapshot(
                                    profiles_directory, context
                                )
                                != profiles_snapshot
                            ):
                                raise LedgerError(
                                    f"{context} profile inventory authority changed"
                                )
                        if scope_record is not None:
                            _verify_live_scope(workspace, scope_record, context)
                        references = set(
                            workspace._source_references(
                                Path(path),
                                profiles_directory_fd=profiles_directory,
                                profiles_directory_absent=profiles_absent,
                                profiles_inventory=(
                                    None
                                    if profiles_snapshot is None
                                    else profiles_snapshot[1]
                                ),
                            )
                        )
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
                        if profiles_directory is not None:
                            _recheck_pinned_directory(
                                profiles_directory,
                                str(profiles_path),
                                f"{context} profiles root",
                            )
                            if (
                                _profile_inventory_snapshot(
                                    profiles_directory, context
                                )
                                != profiles_snapshot
                            ):
                                raise LedgerError(
                                    f"{context} profile inventory authority changed"
                                )
                        recheck_manifest()
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
            if workspace is not None:
                workspace.close()
        finally:
            try:
                if profiles_directory is not None:
                    os.close(profiles_directory)
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


def _git_workspace_package_snapshot(
    package_root: Path,
    worktree: int,
    expected_head: str,
) -> _WorkspacePackageSnapshot:
    """Retain executable package bytes from one already-proven Git tree."""

    root = _directory_fd(package_root)
    try:
        root_status = os.fstat(root)
        _require_trusted_directory(root_status, "candidate wrapper authority package")
        _, listed_raw = _git(
            worktree,
            (
                "ls-tree",
                "-r",
                "-z",
                expected_head,
                "--",
                "atrinik_workspace",
            ),
            "candidate wrapper authority Git inventory",
        )
        entries_raw = listed_raw.split(b"\0")
        if entries_raw and entries_raw[-1] == b"":
            entries_raw.pop()
        if not entries_raw or len(entries_raw) > MAX_INVENTORY_ENTRIES:
            raise LedgerError("candidate wrapper authority Git inventory is invalid")
        sources: list[tuple[str, bytes]] = []
        total_bytes = 0
        fingerprint = hashlib.sha256()
        for entry_raw in entries_raw:
            metadata_raw, separator, raw_name = entry_raw.partition(b"\t")
            metadata_parts = metadata_raw.split(b" ")
            if (
                separator != b"\t"
                or len(metadata_parts) != 3
                or metadata_parts[0] not in {b"100644", b"100755"}
                or metadata_parts[1] != b"blob"
            ):
                raise LedgerError(
                    "candidate wrapper authority Git entry is not a regular file"
                )
            try:
                object_id = metadata_parts[2].decode("ascii")
            except UnicodeError as error:
                raise LedgerError(
                    "candidate wrapper authority Git object is invalid"
                ) from error
            if COMMIT_RE.fullmatch(object_id) is None:
                raise LedgerError(
                    "candidate wrapper authority Git object is invalid"
                )
            try:
                name = raw_name.decode("utf-8")
            except UnicodeError as error:
                raise LedgerError(
                    "candidate wrapper authority Git path is not UTF-8"
                ) from error
            prefix = "atrinik_workspace/"
            if not name.startswith(prefix):
                raise LedgerError("candidate wrapper authority Git path escaped")
            relative = name.removeprefix(prefix)
            parts = relative.split("/")
            if (
                not relative
                or any(not part or part in {".", ".."} for part in parts)
                or _contains_control(relative)
            ):
                raise LedgerError("candidate wrapper authority Git path is unsafe")
            _, raw = _git(
                worktree,
                ("cat-file", "blob", object_id),
                f"candidate wrapper authority Git object {relative}",
            )
            object_header = f"blob {len(raw)}\0".encode("ascii")
            if hashlib.sha1(object_header + raw).hexdigest() != object_id:
                raise LedgerError(
                    "candidate wrapper authority Git object digest differs"
                )
            total_bytes += len(raw)
            if total_bytes > MAX_INVENTORY_BYTES:
                raise LedgerError("candidate wrapper authority Git bytes are not bounded")
            metadata = canonical_bytes(
                {
                    "mode": metadata_parts[0].decode("ascii"),
                    "object_id": object_id,
                    "path": relative,
                    "size": len(raw),
                }
            )
            fingerprint.update(len(metadata).to_bytes(8, "big"))
            fingerprint.update(metadata)
            fingerprint.update(len(raw).to_bytes(8, "big"))
            fingerprint.update(raw)
            if relative.endswith(".py"):
                sources.append((relative, raw))
        source_names = {name for name, _ in sources}
        if not {"__init__.py", "workspace.py"}.issubset(source_names):
            raise LedgerError("candidate wrapper authority Git tree lacks source modules")
        _recheck_pinned_directory(
            root, str(package_root), "candidate wrapper authority package"
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
    if (
        after.device,
        after.inode,
        after.fingerprint,
    ) != (
        snapshot.device,
        snapshot.inode,
        snapshot.fingerprint,
    ):
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


def _load_workspace_module_from_git(
    wrapper_root: str,
    worktree: int,
    expected_head: str,
) -> Any:
    """Load wrapper authority exclusively from one expected committed tree."""

    package_root = Path(wrapper_root) / "atrinik_workspace"
    snapshot = _git_workspace_package_snapshot(
        package_root, worktree, expected_head
    )
    path_token = hashlib.sha256(wrapper_root.encode("utf-8")).hexdigest()[:16]
    package_name = (
        f"_atrinik_delivery_workspace_git_{snapshot.device:x}_{snapshot.inode:x}_"
        f"{path_token}_{snapshot.fingerprint}"
    )
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
    after = _git_workspace_package_snapshot(package_root, worktree, expected_head)
    if (
        after.device,
        after.inode,
        after.fingerprint,
    ) != (
        snapshot.device,
        snapshot.inode,
        snapshot.fingerprint,
    ):
        _discard_snapshot_package(package_name)
        raise LedgerError("candidate wrapper authority Git tree changed during import")
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
    input_bytes: bytes | None = None,
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
        GIT_NO_LAZY_FETCH="1",
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
            stdin=subprocess.DEVNULL if input_bytes is None else subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            pass_fds=(descriptor,),
            env=environment,
        )
    except OSError as error:
        raise LedgerError(f"cannot prove {context}: {error}") from error
    if input_bytes is not None:
        if len(input_bytes) > MAX_BYTES:
            process.kill()
            process.wait()
            raise LedgerError(f"{context} input is not bounded")
        if process.stdin is None:  # pragma: no cover - PIPE invariant
            process.kill()
            process.wait()
            raise LedgerError(f"cannot prove {context}: Git input pipe is unavailable")
        try:
            process.stdin.write(input_bytes)
            process.stdin.close()
        except OSError as error:
            process.kill()
            process.wait()
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
    if len(checkout_matches) != 1:
        raise LedgerError(f"{context} request is not one exact manifest component/checkout")
    checkout = checkout_matches[0]
    expected_repository = (
        f"{request['repository']['owner']}/{request['repository']['name']}"
    )
    if (
        checkout.get("repository") != expected_repository
        or checkout.get("path") != request["physical_checkout"]
    ):
        raise LedgerError(f"{context} manifest component/checkout/repository differs")
    # A physical checkout may be the scope selector itself. Classic is one
    # checkout containing several logical components, so it has no logical
    # component named "classic". Logical selectors retain the stricter
    # component-to-checkout relationship below.
    if request["component"] == request["physical_checkout"]:
        return checkout
    if len(component_matches) != 1:
        raise LedgerError(f"{context} request is not one exact manifest component/checkout")
    component = component_matches[0]
    if component.get("checkout") != request["physical_checkout"]:
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


def _head_correction_recovery(value: Any, context: str) -> dict[str, Any]:
    item = _exact(value, {"grant", "intent"}, context)
    grant = item["grant"]
    _authority(grant, f"{context}.grant")
    if grant["kind"] != "explicit-recovery":
        raise LedgerError(f"{context}.grant must be explicit-recovery authority")
    raw_intent = item["intent"]
    if not isinstance(raw_intent, dict):
        raise LedgerError(f"{context}.intent must be an object")
    transaction = raw_intent.get("transaction")
    intent_fields = {
        "transaction",
        "target",
        "installed",
        "predecessor_sha256",
        "repository",
        "branch",
        "worktree",
        "bad_head",
        "actual_head",
        "ledger_scope",
    }
    if transaction == "delivery-ledger-correct-target-coordinates-intent-v1":
        intent_fields.update(
            {
                "predecessor_head",
                "base_head",
                "recorded_merge_base",
                "actual_merge_base",
            }
        )
    elif transaction != "delivery-ledger-correct-target-head-intent-v1":
        raise LedgerError(f"{context}.intent transaction is invalid")
    intent = _exact(raw_intent, intent_fields, f"{context}.intent")
    _direct_name(intent["target"], f"{context}.intent.target")
    installed = _exact(
        intent["installed"],
        {"generation", "sha256", "device", "inode"},
        f"{context}.intent.installed",
    )
    _integer(installed["generation"], f"{context}.intent.installed.generation", minimum=2)
    _string(installed["sha256"], f"{context}.intent.installed.sha256", SHA256_RE)
    _integer(installed["device"], f"{context}.intent.installed.device", minimum=0)
    _integer(installed["inode"], f"{context}.intent.installed.inode")
    _string(intent["predecessor_sha256"], f"{context}.intent.predecessor_sha256", SHA256_RE)
    _repository(intent["repository"], f"{context}.intent.repository")
    _branch(intent["branch"], f"{context}.intent.branch")
    _absolute_path(intent["worktree"], f"{context}.intent.worktree")
    _string(intent["bad_head"], f"{context}.intent.bad_head", COMMIT_RE)
    _string(intent["actual_head"], f"{context}.intent.actual_head", COMMIT_RE)
    if transaction == "delivery-ledger-correct-target-coordinates-intent-v1":
        for field in (
            "predecessor_head",
            "base_head",
            "recorded_merge_base",
            "actual_merge_base",
        ):
            _string(intent[field], f"{context}.intent.{field}", COMMIT_RE)
        if intent["recorded_merge_base"] == intent["actual_merge_base"]:
            raise LedgerError(
                f"{context}.intent does not describe a stale merge base"
            )
        if grant["reference"] != "recovery:issue-460-stale-merge-base-target-head":
            raise LedgerError(f"{context}.grant reference is invalid")
    scope = _exact(
        intent["ledger_scope"],
        {
            "ledger_id",
            "entry_mode",
            "actor",
            "repositories",
            "issues",
            "pull_requests",
        },
        f"{context}.intent.ledger_scope",
    )
    if not isinstance(scope["ledger_id"], str) or _contains_control(scope["ledger_id"]):
        raise LedgerError(f"{context}.intent.ledger_scope.ledger_id is invalid")
    if scope["entry_mode"] not in ENTRY_MODES:
        raise LedgerError(f"{context}.intent.ledger_scope.entry_mode is invalid")
    actor = _exact(
        scope["actor"],
        {"login", "node_id", "push_repository_node_ids"},
        f"{context}.intent.ledger_scope.actor",
    )
    _string(actor["login"], f"{context}.intent.ledger_scope.actor.login", LOGIN_RE)
    _string(actor["node_id"], f"{context}.intent.ledger_scope.actor.node_id", NODE_RE)
    _sorted_node_ids(
        actor["push_repository_node_ids"],
        f"{context}.intent.ledger_scope.actor.push_repository_node_ids",
        allow_empty=False,
    )
    _ordered_unique(
        scope["repositories"],
        f"{context}.intent.ledger_scope.repositories",
        _repository,
    )
    _ordered_unique(
        scope["issues"],
        f"{context}.intent.ledger_scope.issues",
        _issue,
    )
    _sorted_node_ids(
        scope["pull_requests"],
        f"{context}.intent.ledger_scope.pull_requests",
    )
    if grant["objective_sha256"] != canonical_object_digest(intent):
        raise LedgerError(f"{context}.grant objective does not bind the exact intent")
    return item


def _head_correction_receipt(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise LedgerError(f"{context} must be an object")
    transaction = value.get("transaction")
    fields = {
            "transaction",
            "target",
            "source",
            "predecessor_snapshot",
            "erroneous_snapshot",
            "correction",
            "repository",
            "branch",
            "worktree",
            "predecessor_head",
            "base_head",
            "merge_base",
            "actual_head",
            "bad_head",
            "authority_sha256",
            "recovery",
            "staging",
        }
    if transaction == "delivery-ledger-correct-target-coordinates-v1":
        fields.add("actual_merge_base")
    elif transaction != "delivery-ledger-correct-target-head-v1":
        raise LedgerError(f"{context} transaction is invalid")
    item = _exact(value, fields, context)
    target = _direct_name(item["target"], f"{context}.target")
    source = _exact(
        item["source"],
        {"generation", "sha256", "device", "inode"},
        f"{context}.source",
    )
    source_generation = _integer(source["generation"], f"{context}.source.generation", minimum=2)
    source_digest = _string(source["sha256"], f"{context}.source.sha256", SHA256_RE)
    _integer(source["device"], f"{context}.source.device", minimum=0)
    _integer(source["inode"], f"{context}.source.inode")
    predecessor = _source(item["predecessor_snapshot"], f"{context}.predecessor_snapshot")
    erroneous = _source(item["erroneous_snapshot"], f"{context}.erroneous_snapshot")
    correction = _exact(
        item["correction"], {"generation", "sha256"}, f"{context}.correction"
    )
    if _integer(correction["generation"], f"{context}.correction.generation", minimum=3) != source_generation + 1:
        raise LedgerError(f"{context} correction generation is invalid")
    _string(correction["sha256"], f"{context}.correction.sha256", SHA256_RE)
    _repository(item["repository"], f"{context}.repository")
    _branch(item["branch"], f"{context}.branch")
    _absolute_path(item["worktree"], f"{context}.worktree")
    for key in ("predecessor_head", "base_head", "merge_base", "actual_head", "bad_head"):
        _string(item[key], f"{context}.{key}", COMMIT_RE)
    if transaction == "delivery-ledger-correct-target-coordinates-v1":
        _string(item["actual_merge_base"], f"{context}.actual_merge_base", COMMIT_RE)
        if item["actual_merge_base"] == item["merge_base"]:
            raise LedgerError(f"{context} does not record a stale merge base")
    _string(item["authority_sha256"], f"{context}.authority_sha256", SHA256_RE)
    recovery = _head_correction_recovery(item["recovery"], f"{context}.recovery")
    if len({item["predecessor_head"], item["actual_head"], item["bad_head"]}) != 3:
        raise LedgerError(f"{context} head identities must differ")
    expected_prefix = f".{target}.correct-target-head-{source_digest}"
    if (
        predecessor[0] != f"{expected_prefix}.predecessor.snapshot"
        or erroneous[0] != f"{expected_prefix}.erroneous.snapshot"
        or erroneous[1] != source_digest
        or recovery["intent"]["target"] != target
        or recovery["intent"]["installed"] != source
        or recovery["intent"]["predecessor_sha256"] != predecessor[1]
        or recovery["intent"]["repository"] != item["repository"]
        or recovery["intent"]["branch"] != item["branch"]
        or recovery["intent"]["worktree"] != item["worktree"]
        or recovery["intent"]["bad_head"] != item["bad_head"]
        or recovery["intent"]["actual_head"] != item["actual_head"]
        or (
            transaction == "delivery-ledger-correct-target-coordinates-v1"
            and (
                recovery["intent"].get("predecessor_head")
                != item["predecessor_head"]
                or recovery["intent"].get("base_head") != item["base_head"]
                or recovery["intent"].get("recorded_merge_base")
                != item["merge_base"]
                or recovery["intent"].get("actual_merge_base")
                != item["actual_merge_base"]
            )
        )
        or item["staging"] != f"{expected_prefix}.tmp"
    ):
        raise LedgerError(f"{context} artifact names are invalid")
    return item


def _related_sources(value: Any, context: str) -> tuple[tuple[str, str, int, int], ...]:
    if not isinstance(value, list) or not value:
        raise LedgerError(f"{context} must be a non-empty array")
    sources = tuple(
        _source(source, f"{context}[{index}]") for index, source in enumerate(value)
    )
    names = [source[0].casefold() for source in sources]
    if names != sorted(names) or len(names) != len(set(names)):
        raise LedgerError(f"{context} must have unique case-sorted names")
    return sources


def _historical_heads(
    value: Any, context: str
) -> tuple[tuple[str, str, str, str], ...]:
    if not isinstance(value, list) or not value:
        raise LedgerError(f"{context} must be a non-empty array")
    heads: list[tuple[str, str, str, str]] = []
    for index, row in enumerate(value):
        item = _exact(
            row,
            {"owner", "repository", "branch", "sha"},
            f"{context}[{index}]",
        )
        heads.append(
            (
                _string(item["owner"].casefold(), f"{context}[{index}].owner", OWNER_RE),
                _string(
                    item["repository"].casefold(),
                    f"{context}[{index}].repository",
                    REPOSITORY_RE,
                ),
                _branch(item["branch"], f"{context}[{index}].branch").casefold(),
                _string(
                    item["sha"].casefold(), f"{context}[{index}].sha", COMMIT_RE
                ),
            )
        )
    if heads != sorted(heads) or len(heads) != len(set(heads)):
        raise LedgerError(f"{context} must have unique case-sorted coordinates")
    return tuple(heads)


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
    live_request: Mapping[str, Any] | None = None,
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
        live=live and live_request is None,
        expected_tree=row["tree"],
        expected_common_git_dir=row["common_git_dir"],
        guard=guard,
        allowed_references=_scope_owned_references(request),
        scope_record=scope_record,
    )
    if live_request is not None:
        if not live or guard is None:
            raise LedgerError(f"{context} correction live proof is incomplete")
        if guard.request != live_request or guard.path != path:
            raise LedgerError(f"{context} correction worktree guard differs")
        proof = guard.prove()
        _verify_live_observation(observation, proof, context)
        if proof["common_git_dir"] != row["common_git_dir"]:
            raise LedgerError(f"{context} common Git directory differs from scope result")
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
        migration_keys = {
            "kind",
            "state",
            "source",
            "snapshot",
            "canonical_report",
            "marker_name",
        }
        if isinstance(migration, dict) and "related_sources" in migration:
            migration_keys.add("related_sources")
        if isinstance(migration, dict) and "historical_heads" in migration:
            migration_keys.add("historical_heads")
        migration = _exact(
            migration,
            migration_keys,
            "ledger.migration",
        )
        if migration["kind"] not in MIGRATION_KINDS:
            raise LedgerError("ledger.migration.kind is invalid")
        if migration["state"] != "complete":
            raise LedgerError("canonical ledger migration state must be complete")
        _source(migration["source"], "ledger.migration.source")
        if migration["kind"] == "legacy-rebind":
            if not {"related_sources", "historical_heads"}.issubset(migration):
                raise LedgerError("legacy rebind migration lacks recovery evidence")
            _related_sources(
                migration["related_sources"], "ledger.migration.related_sources"
            )
            _historical_heads(
                migration["historical_heads"], "ledger.migration.historical_heads"
            )
        elif {"related_sources", "historical_heads"}.intersection(migration):
            raise LedgerError("only legacy rebind migration has recovery evidence")
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
    if kind in LEGACY_MIGRATION_KINDS and document["entry_mode"] != "issue":
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
    limit: int = MAX_BYTES,
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
            chunk = os.read(descriptor, min(65536, limit + 1 - size))
            if not chunk:
                break
            chunks.append(chunk)
            size += len(chunk)
            if size > limit:
                raise LedgerError(f"{name} exceeds {limit} bytes")
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
    limit: int = MAX_BYTES,
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
            limit=limit,
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
            limit=limit,
        )
        if exact != raw:
            raise LedgerError(f"staging content mismatch after resume: {name}")
        _fsync(directory, f"review root after reusing staging {name}")
        return exact_status


def _open_unlink_transaction(quarantine: int, token: str) -> int:
    """Open and restore one transaction through an identity-pinned O_PATH fd."""

    path_flags = (
        getattr(os, "O_PATH", os.O_RDONLY)
        | os.O_DIRECTORY
        | os.O_CLOEXEC
        | getattr(os, "O_NOFOLLOW", 0)
    )
    pinned = os.open(token, path_flags, dir_fd=quarantine)
    try:
        expected = os.fstat(pinned)
        visible = os.stat(token, dir_fd=quarantine, follow_symlinks=False)
        _require_trusted_directory(expected, "unlink transaction")
        if (expected.st_dev, expected.st_ino) != (visible.st_dev, visible.st_ino):
            raise LedgerError("unlink transaction changed before recovery")
        if stat.S_IMODE(expected.st_mode) != 0o700:
            os.chmod(f"/proc/self/fd/{pinned}", 0o700)
        transaction = os.open(
            f"/proc/self/fd/{pinned}",
            os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC,
        )
        opened = os.fstat(transaction)
        if (opened.st_dev, opened.st_ino) != (expected.st_dev, expected.st_ino):
            os.close(transaction)
            raise LedgerError("unlink transaction changed while opening")
        return transaction
    finally:
        os.close(pinned)


def _ensure_head_correction_source_link(
    directory: int,
    target: str,
    snapshot: str,
    raw: bytes,
    expected_device: int,
    expected_inode: int,
) -> os.stat_result:
    """Retain the exact installed bad-generation inode before replacement."""

    if not _exists(directory, snapshot):
        try:
            os.link(
                target,
                snapshot,
                src_dir_fd=directory,
                dst_dir_fd=directory,
                follow_symlinks=False,
            )
        except OSError as error:
            raise LedgerError(
                f"cannot retain exact head-correction source inode: {error}"
            ) from error
        _fsync(directory, f"review root after retaining {snapshot}")
    existing, status = _read_regular(
        directory,
        snapshot,
        managed=True,
        expected_nlinks={2},
        sync=True,
    )
    target_raw, target_status = _read_regular(
        directory,
        target,
        managed=True,
        expected_nlinks={2},
    )
    if (
        existing != raw
        or target_raw != raw
        or (status.st_dev, status.st_ino) != (expected_device, expected_inode)
        or (target_status.st_dev, target_status.st_ino)
        != (expected_device, expected_inode)
    ):
        raise LedgerError("head-correction source hard link differs from exact CAS inode")
    return status


def _unlink_exact(directory: int, name: str, expected: os.stat_result) -> None:
    """Atomically quarantine the named inode before irreversible removal."""

    token = hashlib.sha256(
        f"{name}\0{expected.st_dev}\0{expected.st_ino}".encode("utf-8")
    ).hexdigest()
    receipt_document = {
        "schema_version": 1,
        "name": name,
        "device": expected.st_dev,
        "inode": expected.st_ino,
    }
    receipt_raw = canonical_bytes(receipt_document)
    try:
        os.mkdir(_UNLINK_QUARANTINE, 0o700, dir_fd=directory)
        _fsync(directory, "review root after creating unlink quarantine")
    except FileExistsError:
        pass
    root_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
    quarantine = os.open(_UNLINK_QUARANTINE, root_flags, dir_fd=directory)
    transaction = None
    try:
        _require_trusted_directory(os.fstat(quarantine), "unlink quarantine")
        try:
            os.mkdir(token, 0o700, dir_fd=quarantine)
            _fsync(quarantine, "unlink quarantine after creating transaction")
        except FileExistsError:
            pass
        transaction = _open_unlink_transaction(quarantine, token)
        transaction_status = os.fstat(transaction)
        _require_trusted_directory(transaction_status, "unlink transaction")
        if _exists(transaction, "receipt.json"):
            existing_receipt, _ = _read_regular(
                transaction, "receipt.json", managed=True, expected_nlinks={1}
            )
            if existing_receipt != receipt_raw:
                raise LedgerError(f"unlink transaction identity changed: {name}")
        else:
            _write_exclusive(transaction, "receipt.json", receipt_raw)
            _fsync(transaction, "unlink transaction after recording intent")
        # Keep traversal and mutation rights through the pinned descriptor but
        # withhold directory listing while the exact inode is quarantined.
        os.fchmod(transaction, stat.S_IWUSR | stat.S_IXUSR)
        try:
            payload = os.stat("payload", dir_fd=transaction, follow_symlinks=False)
        except FileNotFoundError:
            try:
                visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                # The payload unlink is the deletion commit. An exact receipt
                # plus an absent source is its durable retry state.
                os.unlink("receipt.json", dir_fd=transaction)
                _fsync(transaction, f"unlink transaction after recovering {name}")
                os.fchmod(transaction, 0o700)
                os.close(transaction)
                transaction = None
                os.rmdir(token, dir_fd=quarantine)
                _fsync(quarantine, "unlink quarantine after recovering transaction")
                _fsync(directory, f"review root after recovering removal of {name}")
                return
            if (visible.st_dev, visible.st_ino) != (expected.st_dev, expected.st_ino):
                raise LedgerError(f"staging file was replaced: {name}")
            os.rename(
                name,
                "payload",
                src_dir_fd=directory,
                dst_dir_fd=transaction,
            )
            _fsync(directory, f"review root after quarantining {name}")
            _fsync(transaction, f"unlink transaction after quarantining {name}")
            payload = os.stat("payload", dir_fd=transaction, follow_symlinks=False)
        else:
            try:
                os.stat(name, dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                raise LedgerError(f"unlink transaction and source both exist: {name}")
        if (payload.st_dev, payload.st_ino) != (expected.st_dev, expected.st_ino):
            raise LedgerError(f"quarantined file has the wrong identity: {name}")
        os.unlink("payload", dir_fd=transaction)
        _fsync(transaction, f"unlink transaction after removing {name}")
        os.unlink("receipt.json", dir_fd=transaction)
        _fsync(transaction, f"unlink transaction after completing {name}")
        os.fchmod(transaction, 0o700)
        os.close(transaction)
        transaction = None
        os.rmdir(token, dir_fd=quarantine)
        _fsync(quarantine, "unlink quarantine after completing transaction")
        _fsync(directory, f"review root after removing {name}")
    finally:
        if transaction is not None:
            try:
                os.fchmod(transaction, 0o700)
            finally:
                os.close(transaction)
        os.close(quarantine)


def _recover_unlink_quarantine(directory: int) -> None:
    """Finish exact helper-owned unlink transactions before inventory."""

    root_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
    try:
        quarantine = os.open(_UNLINK_QUARANTINE, root_flags, dir_fd=directory)
    except FileNotFoundError:
        return
    except OSError as error:
        raise LedgerError(f"unlink quarantine is unsafe: {error}") from error
    try:
        _require_trusted_directory(os.fstat(quarantine), "unlink quarantine")
        names = sorted(os.listdir(quarantine))
        for token in names:
            if not re.fullmatch(r"[0-9a-f]{64}", token):
                raise LedgerError(f"unsafe unlink transaction name: {token}")
            try:
                token_status = os.stat(token, dir_fd=quarantine, follow_symlinks=False)
                if not stat.S_ISDIR(token_status.st_mode):
                    raise LedgerError(f"unlink transaction is not a directory: {token}")
                _require_trusted_directory(token_status, "unlink transaction")
            except OSError as error:
                raise LedgerError(f"cannot restore unlink transaction {token}: {error}") from error
            transaction = _open_unlink_transaction(quarantine, token)
            try:
                opened_transaction = os.fstat(transaction)
                visible_transaction = os.stat(
                    token, dir_fd=quarantine, follow_symlinks=False
                )
                _require_trusted_directory(opened_transaction, "unlink transaction")
                if (
                    (opened_transaction.st_dev, opened_transaction.st_ino)
                    != (visible_transaction.st_dev, visible_transaction.st_ino)
                    or (opened_transaction.st_dev, opened_transaction.st_ino)
                    != (token_status.st_dev, token_status.st_ino)
                ):
                    raise LedgerError("unlink transaction changed during recovery")
                entries = set(os.listdir(transaction))
                if not entries:
                    os.close(transaction)
                    transaction = -1
                    os.rmdir(token, dir_fd=quarantine)
                    _fsync(quarantine, "unlink quarantine after removing empty transaction")
                    continue
                if not entries <= {"receipt.json", "payload"} or "receipt.json" not in entries:
                    raise LedgerError(f"unlink transaction has unsafe entries: {token}")
                raw, _ = _read_regular(
                    transaction, "receipt.json", managed=True, expected_nlinks={1}
                )
                receipt = _exact(
                    _decode(raw, "unlink receipt"),
                    {"schema_version", "name", "device", "inode"},
                    "unlink receipt",
                )
                name = _direct_name(receipt["name"], "unlink receipt name")
                device = _integer(receipt["device"], "unlink receipt device", minimum=0)
                inode = _integer(receipt["inode"], "unlink receipt inode", minimum=1)
                expected_token = hashlib.sha256(
                    f"{name}\0{device}\0{inode}".encode("utf-8")
                ).hexdigest()
                if receipt["schema_version"] != 1 or token != expected_token:
                    raise LedgerError("unlink receipt identity is invalid")
                expected = os.stat_result(
                    (stat.S_IFREG | 0o600, inode, device, 1, 0, 0, 0, 0, 0, 0)
                )
                has_payload = "payload" in entries
            finally:
                if transaction != -1:
                    os.close(transaction)
            if not has_payload:
                try:
                    visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
                except FileNotFoundError:
                    _unlink_exact(directory, name, expected)
                    continue
                if (visible.st_dev, visible.st_ino) != (device, inode):
                    raise LedgerError(f"unlink recovery source identity changed: {name}")
                # Intent was durable but the target was never quarantined. Do
                # not let an inventory scan turn a forgeable same-UID receipt
                # into deletion authority; abort this pre-commit transaction.
                transaction = os.open(token, root_flags, dir_fd=quarantine)
                try:
                    os.unlink("receipt.json", dir_fd=transaction)
                    _fsync(transaction, "unlink transaction after aborting pre-commit intent")
                finally:
                    os.close(transaction)
                os.rmdir(token, dir_fd=quarantine)
                _fsync(quarantine, "unlink quarantine after aborting pre-commit intent")
                continue
            _unlink_exact(directory, name, expected)
        _fsync(directory, "review root after unlink recovery")
    finally:
        os.close(quarantine)


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


def _reserved_repository_worktree_paths(
    document: Mapping[str, Any], repository_node_id: str
) -> set[str]:
    """Return managed worktree paths owned by one exact target repository."""

    paths: set[str] = set()
    for slot in document["artifacts"]:
        if (
            slot["kind"] != "worktree"
            or slot["immutable"]["repository"]["node_id"] != repository_node_id
        ):
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
        if (
            resource["kind"] == "scope"
            and resource["immutable"]["repository"]["node_id"]
            == repository_node_id
        ):
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


def _reject_overlaps(
    snapshots: Sequence[Snapshot],
    *,
    allow_name: str | None = None,
    inert_keys: frozenset[tuple[str, str]] = frozenset(),
) -> None:
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
        if (snapshot.name, snapshot.digest) in inert_keys:
            continue
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
    marker_keys = {
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
    }
    if isinstance(value, dict) and "related_sources" in value:
        marker_keys.add("related_sources")
    if isinstance(value, dict) and "historical_heads" in value:
        marker_keys.add("historical_heads")
    item = _exact(
        value,
        marker_keys,
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
    if item["kind"] == "legacy-rebind":
        if not {"related_sources", "historical_heads"}.issubset(item):
            raise LedgerError("legacy rebind marker lacks recovery evidence")
        _related_sources(item["related_sources"], "migration marker related sources")
        _historical_heads(item["historical_heads"], "migration marker historical heads")
    elif {"related_sources", "historical_heads"}.intersection(item):
        raise LedgerError("only legacy rebind marker has recovery evidence")
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
    parsed_issues: list[tuple[str, str, int]] = []
    parsed_pulls: list[tuple[str, str, int]] = []
    malformed_evidence = False
    canonical_closing_lines = 0
    canonical_closing_pull: tuple[str, str, int] | None = None
    for line in text.splitlines():
        stripped = line.strip()
        canonical_closing = stripped.startswith("- Canonical closing PR:")
        canonical_closing_accepted = False
        line_urls = [
            match.group(0).rstrip(".,;:!?")
            for match in _GITHUB_TOKEN_RE.finditer(line)
        ]
        if canonical_closing:
            canonical_closing_lines += 1
            canonical_match = (
                _PR_URL_RE.fullmatch(line_urls[0]) if len(line_urls) == 1 else None
            )
            if (
                len(line_urls) != 1
                or stripped != f"- Canonical closing PR: {line_urls[0]}"
                or canonical_match is None
            ):
                malformed_evidence = True
            else:
                canonical_closing_accepted = True
                canonical_closing_pull = (
                    canonical_match.group("owner").casefold(),
                    canonical_match.group("repo").casefold(),
                    int(canonical_match.group("number")),
                )
        for url in line_urls:
            issue_match = _ISSUE_URL_RE.fullmatch(url)
            pull_match = _PR_URL_RE.fullmatch(url)
            evidence_match = _GITHUB_COORDINATE_TOKEN_RE.fullmatch(url)
            if evidence_match is None:
                if _GITHUB_COORDINATE_PATH_RE.search(url):
                    malformed_evidence = True
                continue
            number_match = re.match(r"[1-9][0-9]*", evidence_match.group("number"))
            coordinate = (
                evidence_match.group("owner").casefold(),
                evidence_match.group("repo").casefold(),
                int(number_match.group(0)) if number_match is not None else None,
            )
            kind = evidence_match.group("kind").casefold()
            if kind.startswith("issue") and coordinate[2] is not None:
                parsed_issues.append(
                    (coordinate[0], coordinate[1], coordinate[2])
                )
            elif kind.startswith("pull") and coordinate[2] is not None:
                if canonical_closing_accepted and pull_match is not None:
                    continue
                parsed_pulls.append(
                    (coordinate[0], coordinate[1], coordinate[2])
                )
            if issue_match is None and pull_match is None:
                malformed_evidence = True
    if canonical_closing_lines > 1:
        malformed_evidence = True
    if (
        canonical_closing_pull is not None
        and canonical_closing_pull not in parsed_pulls
    ):
        malformed_evidence = True
        parsed_pulls.append(canonical_closing_pull)
    issues = set(parsed_issues)
    pulls = set(parsed_pulls)
    parsed_repository_heads: list[tuple[str, str, str]] = []
    parsed_repository_head_commits: list[tuple[str, str, str, str]] = []
    parsed_worktrees: list[str] = []
    coordinate_repositories: list[tuple[str, str]] = []
    coordinate_heads: list[tuple[str, str]] = []
    coordinate_worktrees: list[str] = []
    in_coordinates = False
    for line in text.splitlines():
        stripped = line.strip()
        if stripped == "## Coordinates":
            in_coordinates = True
            continue
        if in_coordinates and stripped.startswith("## "):
            in_coordinates = False
        if in_coordinates and stripped.startswith("- Repository:"):
            match = _LEGACY_REPOSITORY_LINE_RE.fullmatch(stripped)
            if match is None:
                malformed_evidence = True
            else:
                coordinate_repositories.append(
                    (match.group("owner").casefold(), match.group("repo").casefold())
                )
            continue
        if in_coordinates and stripped.startswith("- Remote head:"):
            match = _LEGACY_HEAD_LINE_RE.fullmatch(stripped)
            if match is None:
                malformed_evidence = True
            else:
                branch = match.group("branch")
                try:
                    _branch(branch, "legacy head branch")
                except LedgerError:
                    malformed_evidence = True
                else:
                    coordinate_heads.append(
                        (branch.casefold(), match.group("sha").casefold())
                    )
            continue
        if in_coordinates and stripped.startswith("- Worktree:"):
            match = _LEGACY_WORKTREE_LINE_RE.fullmatch(stripped)
            if match is None:
                malformed_evidence = True
            else:
                worktree = match.group("path")
                if (
                    not worktree.startswith("/")
                    or worktree == "/"
                    or os.path.normpath(worktree) != worktree
                ):
                    malformed_evidence = True
                else:
                    coordinate_worktrees.append(worktree.casefold())
            continue
        if not line.lstrip().startswith("|"):
            continue
        cells = [cell.strip().strip("`") for cell in line.strip().strip("|").split("|")]
        evidence_shaped = (
            "@" in cells[0]
            or (
                len(cells) >= 5
                and "/" in cells[0]
                and " / " in cells[2]
            )
        )
        if not evidence_shaped:
            continue
        if len(cells) < 5:
            malformed_evidence = True
            continue
        if "@" in cells[0]:
            repository_text = cells[0].split("@", 1)[0]
        else:
            repository_text = cells[0]
            malformed_evidence = True
        repository_coordinate: tuple[str, str] | None = None
        if "/" in repository_text:
            owner, repository_name = repository_text.split("/", 1)
            try:
                _string(owner.casefold(), "legacy owner", OWNER_RE)
                _string(repository_name.casefold(), "legacy repository", REPOSITORY_RE)
            except LedgerError:
                malformed_evidence = True
            else:
                repository_coordinate = (owner.casefold(), repository_name.casefold())
        else:
            malformed_evidence = True

        branch_value: str | None = None
        head_sha_value: str | None = None
        if " / " in cells[2]:
            branch, head_sha = (
                value.strip().strip("`") for value in cells[2].split(" / ", 1)
            )
            try:
                _branch(branch, "legacy head branch")
            except LedgerError:
                malformed_evidence = True
            else:
                branch_value = branch.casefold()
            try:
                _string(head_sha.casefold(), "legacy head commit", COMMIT_RE)
            except LedgerError:
                malformed_evidence = True
            else:
                head_sha_value = head_sha.casefold()
        else:
            malformed_evidence = True
        if repository_coordinate is not None and branch_value is not None:
            parsed_repository_heads.append((*repository_coordinate, branch_value))
            if head_sha_value is not None:
                parsed_repository_head_commits.append(
                    (*repository_coordinate, branch_value, head_sha_value)
                )
        worktree = cells[4].strip().strip("`")
        if worktree.startswith("/") and worktree != "/" and os.path.normpath(worktree) == worktree:
            parsed_worktrees.append(worktree.casefold())
        else:
            malformed_evidence = True
    if coordinate_repositories or coordinate_heads:
        if len(coordinate_repositories) != len(coordinate_heads):
            malformed_evidence = True
        else:
            parsed_repository_heads.extend(
                (owner, repository, branch)
                for (owner, repository), (branch, _sha) in zip(
                    coordinate_repositories, coordinate_heads, strict=True
                )
            )
            parsed_repository_head_commits.extend(
                (owner, repository, branch, sha)
                for (owner, repository), (branch, sha) in zip(
                    coordinate_repositories, coordinate_heads, strict=True
                )
            )
    parsed_worktrees.extend(coordinate_worktrees)
    repository_heads = set(parsed_repository_heads)
    repository_head_commits = set(parsed_repository_head_commits)
    worktrees = set(parsed_worktrees)
    filename_claim = bool(
        _CANONICAL_REPORT_RE.fullmatch(name) or _LEGACY_REPORT_RE.fullmatch(name)
    )
    duplicate_evidence = any(
        (
            len(parsed_issues) != len(issues),
            len(parsed_pulls) != len(pulls),
            len(parsed_repository_heads) != len(repository_heads),
            len(parsed_repository_head_commits) != len(repository_head_commits),
            len(parsed_worktrees) != len(worktrees),
        )
    )
    evidence_invalid = malformed_evidence or duplicate_evidence
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
        repository_head_commits=tuple(sorted(repository_head_commits)),
        worktrees=tuple(sorted(worktrees)),
        evidence_invalid=evidence_invalid,
        ambiguous=ambiguous,
    )


def _head_correction_ledger_scope(document: Mapping[str, Any]) -> dict[str, Any]:
    repositories = {
        target["repository"]["node_id"]: target["repository"]
        for target in document["targets"]
    }
    issue_rows = [*document["issues"]["explicit"]]
    if document["program"] is not None:
        issue_rows.extend(
            (
                document["program"]["master_issue"],
                document["program"]["leaf_issue"],
            )
        )
    issues = {issue["node_id"]: issue for issue in issue_rows}
    allowed = document["authority"]["allowed"]
    if (
        set(repositories) != set(allowed["repositories"])
        or set(issues) != set(allowed["issues"])
    ):
        raise LedgerError("head correction ledger authority scope is inconsistent")
    return {
        "ledger_id": document["ledger_id"],
        "entry_mode": document["entry_mode"],
        "actor": copy.deepcopy(document["actor"]),
        "repositories": sorted(
            (copy.deepcopy(repository) for repository in repositories.values()),
            key=lambda row: (row["owner"], row["name"], row["node_id"]),
        ),
        "issues": sorted(
            (copy.deepcopy(issue) for issue in issues.values()),
            key=lambda row: (
                row["repository"]["owner"],
                row["repository"]["name"],
                row["number"],
                row["node_id"],
            ),
        ),
        "pull_requests": copy.deepcopy(allowed["pull_requests"]),
    }


def _head_correction_intent(
    document: Mapping[str, Any],
    name: str,
    source: Mapping[str, Any],
    predecessor_digest: str,
    metadata: Mapping[str, Any],
    *,
    bad_head: str,
    actual_head: str,
    actual_merge_base: str | None = None,
) -> dict[str, Any]:
    intent = {
        "transaction": (
            "delivery-ledger-correct-target-coordinates-intent-v1"
            if actual_merge_base is not None
            else "delivery-ledger-correct-target-head-intent-v1"
        ),
        "target": name,
        "installed": dict(source),
        "predecessor_sha256": predecessor_digest,
        "repository": copy.deepcopy(metadata["repository"]),
        "branch": metadata["branch"],
        "worktree": metadata["worktree"],
        "bad_head": bad_head,
        "actual_head": actual_head,
        "ledger_scope": _head_correction_ledger_scope(document),
    }
    if actual_merge_base is not None:
        intent.update(
            predecessor_head=metadata["predecessor_head"],
            base_head=metadata["base_head"],
            recorded_merge_base=metadata["merge_base"],
            actual_merge_base=actual_merge_base,
        )
    return intent


def _require_head_correction_recovery(
    recovery: Mapping[str, Any],
    document: Mapping[str, Any],
    expected_intent: Mapping[str, Any],
    context: str,
) -> None:
    if recovery["intent"] != expected_intent:
        raise LedgerError(f"{context} does not authorize the exact correction intent")
    grant = recovery["grant"]
    if (
        grant["actor_node_id"] != document["actor"]["node_id"]
        or grant["allowed"] != document["authority"]["allowed"]
    ):
        raise LedgerError(f"{context} actor or ledger scope differs")


def _head_correction_document(
    predecessor: Mapping[str, Any],
    erroneous: Mapping[str, Any],
    erroneous_digest: str,
    *,
    bad_head: str,
    actual_head: str,
    actual_merge_base: str | None = None,
) -> tuple[dict[str, Any], dict[str, Any], Mapping[str, Any]]:
    """Build the sole permitted correction of one mistyped target advancement."""

    predecessor_raw = canonical_bytes(predecessor)
    predecessor_digest = byte_digest(predecessor_raw)
    if (
        erroneous["generation"] != predecessor["generation"] + 1
        or erroneous["previous_byte_digest"] != predecessor_digest
        or erroneous["history"] != [*predecessor["history"], predecessor_digest]
    ):
        raise LedgerError("head correction input is not the exact immediate predecessor")
    if actual_head == bad_head:
        raise LedgerError("head correction actual and bad SHAs are equal")
    predecessor_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in predecessor["targets"]
    }
    erroneous_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in erroneous["targets"]
    }
    if set(predecessor_targets) != set(erroneous_targets):
        raise LedgerError("head correction target set differs")
    changed_targets = []
    expected_erroneous = copy.deepcopy(predecessor)
    expected_erroneous.update(
        generation=erroneous["generation"],
        previous_byte_digest=predecessor_digest,
        history=[*predecessor["history"], predecessor_digest],
    )
    expected_target_rows = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in expected_erroneous["targets"]
    }
    for key, before in predecessor_targets.items():
        after = erroneous_targets[key]
        if before == after:
            continue
        if (
            after["repository"] != before["repository"]
            or after["base"] != before["base"]
            or after["merge_base"] != before["merge_base"]
            or after["head"]["branch"] != before["head"]["branch"]
            or after["head"]["initial_sha"] != before["head"]["initial_sha"]
            or after["head"]["current_sha"] != bad_head
            or after["head"]["lineage"] != [*before["head"]["lineage"], bad_head]
        ):
            raise LedgerError("head correction source is not one mistyped advancement")
        changed_targets.append((key, before, after))
        expected_target = expected_target_rows[key]
        expected_target["head"]["current_sha"] = bad_head
        expected_target["head"]["lineage"].append(bad_head)
    if len(changed_targets) != 1:
        raise LedgerError("head correction requires one changed target")
    key, before_target, _after_target = changed_targets[0]
    predecessor_artifacts = {slot["slot_id"]: slot for slot in predecessor["artifacts"]}
    expected_artifacts = {slot["slot_id"]: slot for slot in expected_erroneous["artifacts"]}
    erroneous_artifacts = {slot["slot_id"]: slot for slot in erroneous["artifacts"]}
    if set(predecessor_artifacts) != set(erroneous_artifacts):
        raise LedgerError("head correction artifact set differs")
    changed_artifacts: list[Mapping[str, Any]] = []
    for slot_id, before in predecessor_artifacts.items():
        after = erroneous_artifacts[slot_id]
        if before == after:
            continue
        expected = expected_artifacts[slot_id]
        delivery_created_pr = False
        if before["kind"] == "pull_request" and before["current"] is not None:
            selected_prs = [
                row
                for row in predecessor["selected_prs"]
                if row["repository"] == before["current"]["repository"]
                and row["head_repository"] == before["current"]["repository"]
                and row["number"] == before["current"]["number"]
                and row["node_id"] == before["current"]["node_id"]
                and row["head_branch"] == before["current"]["branch"]
            ]
            delivery_created_pr = (
                len(selected_prs) == 1
                and before["state"] == "created"
                and before["immutable"]["number"] is None
                and before["immutable"]["node_id"] is None
                and selected_prs[0]["author_node_id"] == predecessor["actor"]["node_id"]
                and selected_prs[0]["body"]["ownership"] == "delivery-created"
            )
        unsupported_kind = (
            before["kind"] not in {"branch", "worktree"}
            and not delivery_created_pr
        )
        if unsupported_kind or (
            before["current"] is None
            or after["current"] is None
            or before["current"].get("head_sha") != before_target["head"]["current_sha"]
            or after["current"] != {**before["current"], "head_sha": bad_head}
            or after != {**before, "current": {**before["current"], "head_sha": bad_head}}
            or before["current"]["repository"]["node_id"] != key[0]
            or before["current"]["branch"] != key[1]
        ):
            raise LedgerError("head correction source changed an unsupported artifact")
        expected["current"]["head_sha"] = bad_head
        changed_artifacts.append(after)
    changed_kinds = sorted(slot["kind"] for slot in changed_artifacts)
    if changed_kinds not in (
        ["branch", "worktree"],
        ["branch", "pull_request", "worktree"],
    ):
        raise LedgerError(
            "head correction requires mirrored bound branch/worktree heads and "
            "at most one exact delivery-created PR head"
        )
    if erroneous != expected_erroneous:
        raise LedgerError("head correction source contains unrelated semantic changes")
    worktree_slot = next(slot for slot in changed_artifacts if slot["kind"] == "worktree")
    worktree = worktree_slot["current"]["path"]
    request, allowed, scope_record, scope_proof = _target_refresh_worktree_provenance(
        erroneous, worktree_slot
    )
    if worktree != _expected_worktree_path(request):
        raise LedgerError("head correction lacks one exact bound worktree request")
    if request["repository"] != before_target["repository"] or request["branch"] != key[1]:
        raise LedgerError("head correction worktree request differs from its target")
    corrected = copy.deepcopy(erroneous)
    corrected["generation"] += 1
    corrected["previous_byte_digest"] = erroneous_digest
    corrected["history"] = [*erroneous["history"], erroneous_digest]
    corrected_target = next(
        row
        for row in corrected["targets"]
        if (row["repository"]["node_id"], row["head"]["branch"]) == key
    )
    corrected_target["head"]["current_sha"] = actual_head
    corrected_target["head"]["lineage"] = [
        *before_target["head"]["lineage"],
        actual_head,
    ]
    if actual_merge_base is not None:
        if actual_merge_base == before_target["merge_base"]["current_sha"]:
            raise LedgerError("coordinate correction merge base is not stale")
        corrected_target["merge_base"]["current_sha"] = actual_merge_base
    for slot in corrected["artifacts"]:
        if slot["slot_id"] in {value["slot_id"] for value in changed_artifacts}:
            slot["current"]["head_sha"] = actual_head
    corrected = prepare(corrected)
    metadata = {
        "repository": copy.deepcopy(before_target["repository"]),
        "branch": key[1],
        "worktree": worktree,
        "predecessor_head": before_target["head"]["current_sha"],
        "base_head": before_target["base"]["current_sha"],
        "merge_base": before_target["merge_base"]["current_sha"],
        "authority_sha256": canonical_object_digest(erroneous["authority"]),
    }
    if actual_merge_base is not None:
        metadata["actual_merge_base"] = actual_merge_base
    live_request = copy.deepcopy(request)
    live_request["expected_head_sha"] = actual_head
    return corrected, metadata, {
        "request": live_request,
        "allowed_references": allowed,
        "scope_record": scope_record,
        "scope_proof": scope_proof,
    }


def _inventory_locked(directory: int) -> Inventory:
    _recover_unlink_quarantine(directory)
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
    head_corrections: dict[str, dict[str, tuple[str, str, bytes, os.stat_result]]] = {}
    legacy_reports: list[LegacyClaim] = []
    releases: list[ReleaseRecord] = []
    archives: list[ArchiveRecord] = []
    reclaims: list[ReclaimRecord] = []
    managed_stats: dict[str, os.stat_result] = {}
    inventory_bytes = 0
    counted_inodes: set[tuple[int, int]] = set()
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
            or name == _RECLAIM_COMPLETE_NAME
            or _RECLAIM_COMPLETE_STAGE_RE.fullmatch(name) is not None
        )
        if relevant:
            prior = case_names.get(folded)
            if prior is not None and prior != name:
                raise LedgerError(f"case-alias entries collide: {prior} and {name}")
            case_names[folded] = name
            status = os.stat(name, dir_fd=directory, follow_symlinks=False)
            if not stat.S_ISREG(status.st_mode):
                raise LedgerError(f"delivery entry is not a regular file: {name}")
            identity = (status.st_dev, status.st_ino)
            if identity not in counted_inodes:
                counted_inodes.add(identity)
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
                or any(
                    archive_name.startswith(f".{paired_target}.archive-")
                    for archive_name in name_set
                )
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
        archive_match = _ARCHIVE_RE.fullmatch(name)
        if archive_match:
            raw, status = _read_regular(
                directory,
                name,
                managed=True,
                expected_nlinks={1, 2},
                limit=MAX_ARCHIVE_BYTES,
            )
            managed_stats[name] = status
            archive = _archive_record(name, raw, status)
            if (
                archive.ledger_name != archive_match.group("target")
                or archive.document["ledger"]["sha256"] != archive_match.group("ledger")
            ):
                raise LedgerError(f"archive target mismatch: {name}")
            archives.append(archive)
            continue
        archive_stage_match = _ARCHIVE_STAGE_RE.fullmatch(name)
        if archive_stage_match:
            raw, status = _read_regular(
                directory,
                name,
                managed=True,
                expected_nlinks={1, 2},
                limit=MAX_ARCHIVE_BYTES,
            )
            managed_stats[name] = status
            pending.append(
                PendingOperation("archive", archive_stage_match.group("target"), name)
            )
            try:
                _validate_archive_document(
                    _decode(raw, name, limit=MAX_ARCHIVE_BYTES), name
                )
            except LedgerError:
                continue
            if byte_digest(raw) != archive_stage_match.group("candidate"):
                raise LedgerError(f"archive staging identity is invalid: {name}")
            continue
        reclaim_receipt_match = _RECLAIM_RECEIPT_RE.fullmatch(name)
        if reclaim_receipt_match:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1}
            )
            managed_stats[name] = status
            preview = _validate_reclaim_preview(_decode(raw, name))
            if (
                preview["archive"] != reclaim_receipt_match.group("archive")
                or preview["plan_sha256"] != reclaim_receipt_match.group("plan")
                or raw != canonical_bytes(preview)
            ):
                raise LedgerError(f"reclaim receipt identity is invalid: {name}")
            pending.append(
                PendingOperation("reclaim", preview["archive"], name)
            )
            continue
        if name == _RECLAIM_COMPLETE_NAME:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1}
            )
            managed_stats[name] = status
            preview = _validate_reclaim_preview(_decode(raw, name))
            if raw != canonical_bytes(preview):
                raise LedgerError(f"reclaim completion identity is invalid: {name}")
            reclaims.append(
                ReclaimRecord(name, preview["archive"], preview["plan_sha256"], preview, raw)
            )
            continue
        reclaim_complete_stage = _RECLAIM_COMPLETE_STAGE_RE.fullmatch(name)
        if reclaim_complete_stage:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1}
            )
            managed_stats[name] = status
            preview = _validate_reclaim_preview(_decode(raw, name))
            if (
                preview["plan_sha256"] != reclaim_complete_stage.group("plan")
                or raw != canonical_bytes(preview)
            ):
                raise LedgerError(f"reclaim completion stage is invalid: {name}")
            pending.append(PendingOperation("reclaim-complete", preview["archive"], name))
            continue
        release_match = _RELEASE_RE.fullmatch(name)
        if release_match:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1, 2}
            )
            managed_stats[name] = status
            release = _release_record(name, raw, status)
            if release.ledger_name != release_match.group("target"):
                raise LedgerError(f"release marker target mismatch: {name}")
            releases.append(release)
            continue
        release_stage_match = _RELEASE_STAGE_RE.fullmatch(name)
        if release_stage_match:
            raw, status = _read_regular(
                directory, name, managed=True, expected_nlinks={1, 2}
            )
            managed_stats[name] = status
            pending.append(
                PendingOperation("release", release_stage_match.group("target"), name)
            )
            try:
                release = _release_record(
                    _release_name(release_stage_match.group("target")), raw, status
                )
            except LedgerError:
                continue
            if byte_digest(raw) != release_stage_match.group("candidate"):
                raise LedgerError(f"release staging identity is invalid: {name}")
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
        correction_patterns = (
            ("predecessor", _HEAD_CORRECTION_PREDECESSOR_RE),
            ("erroneous", _HEAD_CORRECTION_ERRONEOUS_RE),
            ("stage", _HEAD_CORRECTION_STAGE_RE),
            ("receipt", _HEAD_CORRECTION_RECEIPT_RE),
        )
        correction_match = next(
            ((kind, pattern.fullmatch(name)) for kind, pattern in correction_patterns if pattern.fullmatch(name)),
            None,
        )
        if correction_match is not None:
            kind, match = correction_match
            assert match is not None
            raw, status = _read_regular(
                directory,
                name,
                managed=True,
                expected_nlinks={1, 2} if kind == "erroneous" else {1},
            )
            managed_stats[name] = status
            target = match.group("target")
            entries = head_corrections.setdefault(target, {})
            if kind in entries:
                raise LedgerError(f"multiple head-correction {kind} files exist: {target}")
            entries[kind] = (name, match.group("source"), raw, status)
            pending.append(PendingOperation(f"correct-target-head-{kind}", target, name))
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
    completed_head_corrections: set[str] = set()
    committed_by_name = {item.name: item for item in committed}
    archived_head_corrections = {
        archive.ledger_name: {
            member["name"]
            for member in archive.document["members"]
            if any(
                (match := pattern.fullmatch(member["name"])) is not None
                and match.group("target") == archive.ledger_name
                for pattern in (
                    _HEAD_CORRECTION_PREDECESSOR_RE,
                    _HEAD_CORRECTION_ERRONEOUS_RE,
                    _HEAD_CORRECTION_RECEIPT_RE,
                )
            )
        }
        for archive in archives
    }
    for target, entries in head_corrections.items():
        archived_names = archived_head_corrections.get(target, set())
        entries = {
            kind: entry
            for kind, entry in entries.items()
            if entry[0] not in archived_names
        }
        if not entries:
            continue
        current = committed_by_name.get(target)
        if current is None or "predecessor" not in entries:
            raise LedgerError(f"head-correction target/predecessor is missing: {target}")
        sources = {entry[1] for entry in entries.values()}
        if len(sources) != 1:
            raise LedgerError(f"head-correction source digests differ: {target}")
        source_digest = next(iter(sources))
        predecessor_name, _, predecessor_raw, predecessor_status = entries["predecessor"]
        try:
            predecessor = validate(_decode(predecessor_raw, predecessor_name))
            if predecessor_raw != canonical_bytes(predecessor) or canonical_name(predecessor) != target:
                raise LedgerError(f"head-correction predecessor is noncanonical: {target}")
        except LedgerError:
            if current.digest == source_digest:
                continue
            raise
        receipt = None
        if "receipt" in entries:
            receipt_name, _, receipt_raw, _ = entries["receipt"]
            try:
                receipt = _head_correction_receipt(_decode(receipt_raw, receipt_name), receipt_name)
                if receipt_raw != canonical_bytes(receipt):
                    raise LedgerError(f"head-correction receipt is noncanonical: {target}")
            except LedgerError:
                receipt = None
        if current.digest == source_digest:
            if receipt is not None:
                if "erroneous" not in entries:
                    raise LedgerError(
                        f"head-correction receipt lacks erroneous snapshot: {target}"
                    )
                erroneous_name, _, erroneous_raw, erroneous_status = entries["erroneous"]
                erroneous = validate(_decode(erroneous_raw, erroneous_name))
                if (
                    erroneous_raw != canonical_bytes(erroneous)
                    or byte_digest(erroneous_raw) != source_digest
                    or current.raw != erroneous_raw
                    or (erroneous_status.st_dev, erroneous_status.st_ino)
                    != (current.device, current.inode)
                    or receipt["source"]
                    != {
                        "generation": current.document["generation"],
                        "sha256": current.digest,
                        "device": current.device,
                        "inode": current.inode,
                    }
                ):
                    raise LedgerError(f"head-correction source identity changed: {target}")
                _, metadata, _ = _head_correction_document(
                    predecessor,
                    erroneous,
                    source_digest,
                    bad_head=receipt["bad_head"],
                    actual_head=receipt["actual_head"],
                    actual_merge_base=receipt.get("actual_merge_base"),
                )
                expected_intent = _head_correction_intent(
                    erroneous,
                    target,
                    receipt["source"],
                    byte_digest(predecessor_raw),
                    metadata,
                    bad_head=receipt["bad_head"],
                    actual_head=receipt["actual_head"],
                    actual_merge_base=receipt.get("actual_merge_base"),
                )
                _require_head_correction_recovery(
                    receipt["recovery"],
                    erroneous,
                    expected_intent,
                    f"head-correction receipt for {target}",
                )
            continue
        if receipt is None or "erroneous" not in entries:
            raise LedgerError(f"installed head correction lacks evidence: {target}")
        erroneous_name, _, erroneous_raw, erroneous_status = entries["erroneous"]
        erroneous = validate(_decode(erroneous_raw, erroneous_name))
        if erroneous_raw != canonical_bytes(erroneous) or byte_digest(erroneous_raw) != source_digest:
            raise LedgerError(f"head-correction erroneous snapshot mismatch: {target}")
        corrected, metadata, _ = _head_correction_document(
            predecessor,
            erroneous,
            source_digest,
            bad_head=receipt["bad_head"],
            actual_head=receipt["actual_head"],
            actual_merge_base=receipt.get("actual_merge_base"),
        )
        corrected_raw = canonical_bytes(corrected)
        corrected_digest = byte_digest(corrected_raw)
        expected_intent = _head_correction_intent(
            erroneous,
            target,
            receipt["source"],
            byte_digest(predecessor_raw),
            metadata,
            bad_head=receipt["bad_head"],
            actual_head=receipt["actual_head"],
            actual_merge_base=receipt.get("actual_merge_base"),
        )
        _require_head_correction_recovery(
            receipt["recovery"],
            erroneous,
            expected_intent,
            f"head-correction receipt for {target}",
        )
        correction_generation = corrected["generation"]
        correction_is_current = current.document["generation"] == correction_generation
        correction_is_historical = (
            current.document["generation"] > correction_generation
            and len(current.document["history"]) >= correction_generation
            and current.document["history"][correction_generation - 1]
            == corrected_digest
        )
        if (
            (correction_is_current and current.raw != corrected_raw)
            or not (correction_is_current or correction_is_historical)
            or receipt["correction"]
            != {
                "generation": correction_generation,
                "sha256": corrected_digest,
            }
            or receipt["repository"] != metadata["repository"]
            or receipt["branch"] != metadata["branch"]
            or receipt["worktree"] != metadata["worktree"]
            or receipt["predecessor_head"] != metadata["predecessor_head"]
            or receipt["base_head"] != metadata["base_head"]
            or receipt["merge_base"] != metadata["merge_base"]
            or receipt.get("actual_merge_base")
            != metadata.get("actual_merge_base")
            or receipt["authority_sha256"] != metadata["authority_sha256"]
            or receipt["predecessor_snapshot"] != {
                "name": predecessor_name,
                "sha256": byte_digest(predecessor_raw),
                "device": predecessor_status.st_dev,
                "inode": predecessor_status.st_ino,
            }
            or receipt["erroneous_snapshot"] != {
                "name": erroneous_name,
                "sha256": source_digest,
                "device": erroneous_status.st_dev,
                "inode": erroneous_status.st_ino,
            }
            or (
                receipt["source"]["device"],
                receipt["source"]["inode"],
            )
            != (erroneous_status.st_dev, erroneous_status.st_ino)
            or "stage" in entries
        ):
            raise LedgerError(f"completed head correction evidence mismatch: {target}")
        completed_head_corrections.add(target)
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
            for pattern in (
                _CREATE_STAGE_RE,
                _MIGRATE_STAGE_RE,
                _RELEASE_STAGE_RE,
                _ARCHIVE_STAGE_RE,
            ):
                match = pattern.fullmatch(first)
                if match is not None and second == (
                    _release_name(match.group("target"))
                    if pattern is _RELEASE_STAGE_RE
                    else (
                        f".{match.group('target')}.archive-{match.group('ledger')}.json"
                        if pattern is _ARCHIVE_STAGE_RE
                        else match.group("target")
                    )
                ):
                    allowed_pair = True
                match = pattern.fullmatch(second)
                if match is not None and first == (
                    _release_name(match.group("target"))
                    if pattern is _RELEASE_STAGE_RE
                    else (
                        f".{match.group('target')}.archive-{match.group('ledger')}.json"
                        if pattern is _ARCHIVE_STAGE_RE
                        else match.group("target")
                    )
                ):
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
            erroneous_name = next(
                (
                    value
                    for value in linked_names
                    if _HEAD_CORRECTION_ERRONEOUS_RE.fullmatch(value)
                ),
                None,
            )
            if erroneous_name is not None:
                erroneous_match = _HEAD_CORRECTION_ERRONEOUS_RE.fullmatch(
                    erroneous_name
                )
                other = (
                    linked_names[0]
                    if linked_names[1] == erroneous_name
                    else linked_names[1]
                )
                allowed_pair = other == erroneous_match.group("target")
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
    archived_targets = {archive.ledger_name for archive in archives}
    for destination, marker in markers.items():
        if destination in archived_targets:
            continue
        source_name, source_digest, source_device, source_inode = _source(
            marker["source"], "migration marker source"
        )
        snapshot_name = marker["snapshot_name"]
        snapshot_entry = snapshots.get(destination)
        source_raw, source_status = _read_regular(directory, source_name)
        for related_name, related_digest, related_device, related_inode in (
            _related_sources(
                marker["related_sources"], "migration marker related sources"
            )
            if marker["kind"] == "legacy-rebind"
            else ()
        ):
            related_raw, related_status = _read_regular(directory, related_name)
            if (
                byte_digest(related_raw) != related_digest
                or related_status.st_dev != related_device
                or related_status.st_ino != related_inode
            ):
                raise LedgerError(f"migration related source changed: {related_name}")
        canonical_report = marker["canonical_report"]
        if (
            marker["kind"] in LEGACY_MIGRATION_KINDS
            or marker["state"] != "complete"
        ):
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
            if (
                marker["state"] == "planned"
                and marker["kind"] in LEGACY_MIGRATION_KINDS
            ):
                canonical_raw = None
            else:
                raise
        if (
            canonical_raw is not None
            and marker["kind"] in LEGACY_MIGRATION_KINDS
            and marker["state"] != "complete"
        ):
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
        if destination not in markers and destination not in archived_targets:
            raise LedgerError(f"orphaned migration snapshot: {snapshots[destination][0]}")
    for snapshot in committed:
        migration = snapshot.document["migration"]
        if migration is None or snapshot.name in archived_targets:
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
            or marker.get("related_sources") != migration.get("related_sources")
            or marker.get("historical_heads") != migration.get("historical_heads")
            or marker["snapshot"] != migration["snapshot"]
            or marker["canonical_report"] != migration["canonical_report"]
            or migration["marker_name"] != f".{snapshot.name}.migration.json"
        ):
            raise LedgerError(f"migrated ledger metadata mismatch: {snapshot.name}")
    managed_sources = {marker["source"]["name"] for marker in markers.values()}
    managed_sources.update(
        source["name"]
        for marker in markers.values()
        for source in marker.get("related_sources", [])
    )
    legacy_reports = [
        claim for claim in legacy_reports if claim.name not in managed_sources
    ]
    unique: dict[tuple[str, str], Snapshot] = {}
    for snapshot in ledgers:
        unique[(snapshot.name, snapshot.digest)] = snapshot
    result = list(unique.values())
    release_unique: dict[tuple[str, str], ReleaseRecord] = {}
    for release in releases:
        release_unique[(release.ledger_name, release.digest)] = release
    release_result = list(release_unique.values())
    releases_by_ledger: dict[str, ReleaseRecord] = {}
    snapshots_by_name = {snapshot.name: snapshot for snapshot in committed}
    for release in release_result:
        prior = releases_by_ledger.get(release.ledger_name)
        if prior is not None and prior.digest != release.digest:
            raise LedgerError(f"multiple release candidates exist for {release.ledger_name}")
        snapshot = snapshots_by_name.get(release.ledger_name)
        if snapshot is None:
            if release.ledger_name in archived_targets:
                continue
            raise LedgerError(f"release marker lost its canonical ledger: {release.ledger_name}")
        identity = release.document["ledger"]
        if (
            identity["ledger_id"] != snapshot.document["ledger_id"]
            or identity["generation"] != snapshot.document["generation"]
            or identity["sha256"] != snapshot.digest
            or identity["device"] != snapshot.device
            or identity["inode"] != snapshot.inode
        ):
            raise LedgerError(f"release marker is stale for {release.ledger_name}")
        request = {
            key: release.document[key]
            for key in (
                "authority",
                "observed_at",
                "pull_requests",
                "issues",
                "mutation_state",
                "cleanup",
            )
        }
        if _release_document(snapshot, request, live_proof=False) != release.document:
            raise LedgerError(f"release marker does not match {release.ledger_name}")
        releases_by_ledger[release.ledger_name] = release
    archive_by_ledger: dict[str, ArchiveRecord] = {}
    for archive in archives:
        prior = archive_by_ledger.get(archive.ledger_name)
        if prior is not None and prior.digest != archive.digest:
            raise LedgerError(f"multiple archives exist for {archive.ledger_name}")
        archive_by_ledger[archive.ledger_name] = archive
    active = [
        snapshot
        for snapshot in result
        if snapshot.name not in releases_by_ledger
        and snapshot.name not in archive_by_ledger
    ]
    archived_snapshots: list[Snapshot] = []
    for archive in archive_by_ledger.values():
        member = next(
            row
            for row in archive.document["members"]
            if row["name"] == archive.ledger_name
        )
        ledger_raw = base64.b64decode(member["raw_base64"].encode("ascii"))
        archived_snapshots.append(
            Snapshot(
                archive.ledger_name,
                _decode(ledger_raw, f"archive {archive.name} ledger"),
                ledger_raw,
                archive.document["ledger"]["sha256"],
                archive.document["ledger"]["device"],
                archive.document["ledger"]["inode"],
            )
        )
    all_by_identity = {
        (snapshot.name, snapshot.digest): snapshot
        for snapshot in [*result, *archived_snapshots]
    }
    _reject_overlaps(
        list(all_by_identity.values()),
        inert_keys=frozenset(
            {
                *(
                    (name, release.document["ledger"]["sha256"])
                    for name, release in releases_by_ledger.items()
                ),
                *(
                    (name, archive.document["ledger"]["sha256"])
                    for name, archive in archive_by_ledger.items()
                ),
            }
        ),
    )
    committed_keys = {(item.name, item.digest) for item in committed}
    committed_result = tuple(
        item
        for item in result
        if (item.name, item.digest) in committed_keys
        and item.name not in releases_by_ledger
        and item.name not in archive_by_ledger
    )
    historical_by_identity = {
        (snapshot.name, snapshot.digest): snapshot
        for snapshot in [
            *(
                snapshot
                for snapshot in result
                if snapshot.name in releases_by_ledger
                or snapshot.name in archive_by_ledger
            ),
            *archived_snapshots,
        ]
    }
    pending = [
        item
        for item in pending
        if not (
            item.kind == "migration-snapshot"
            and item.target in markers
            and markers[item.target]["state"] == "complete"
        )
        and not (
            (
                item.target in completed_head_corrections
                or item.staging in archived_head_corrections.get(item.target, set())
            )
            and item.kind in {
                "correct-target-head-predecessor",
                "correct-target-head-erroneous",
                "correct-target-head-receipt",
            }
        )
    ]
    return Inventory(
        committed_result,
        tuple(sorted(pending, key=lambda item: (item.target, item.kind))),
        tuple(sorted(legacy_reports, key=lambda item: item.name)),
        tuple(
            sorted(
                (
                    release for name, release in releases_by_ledger.items()
                    if name not in archive_by_ledger
                ),
                key=lambda item: item.ledger_name,
            )
        ),
        tuple(sorted(archive_by_ledger.values(), key=lambda item: item.ledger_name)),
        tuple(sorted(reclaims, key=lambda item: (item.archive_name, item.plan))),
        tuple(
            sorted(
                historical_by_identity.values(),
                key=lambda item: (item.name, item.digest),
            )
        ),
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
    allowed_sources: set[str] | None = None,
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
    allowed = set() if allowed_sources is None else allowed_sources
    for claim in current.legacy_reports:
        if claim.name in allowed:
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


def _require_legacy_rebind_source(
    source_name: str, candidate: Mapping[str, Any]
) -> None:
    selected = candidate["issues"]["explicit"]
    if candidate["entry_mode"] != "issue" or len(selected) != 1:
        raise LedgerError("legacy rebind requires one exact issue-mode identity")
    match = _LEGACY_REPORT_RE.fullmatch(source_name)
    if match is None:
        raise LedgerError("legacy rebind source must be one mode-less legacy report")
    issue = selected[0]
    issue_repository = issue["repository"]
    source_coordinate = (
        match.group("owner").casefold(),
        match.group("repo").casefold(),
        int(match.group("number")),
    )
    issue_coordinate = (
        issue_repository["owner"].casefold(),
        issue_repository["name"].casefold(),
        issue["number"],
    )
    if source_coordinate == issue_coordinate:
        raise LedgerError("matching legacy filename must use ordinary legacy migration")
    target_repositories = {
        (
            target["repository"]["owner"].casefold(),
            target["repository"]["name"].casefold(),
        )
        for target in candidate["targets"]
    }
    if (
        source_coordinate[2] != issue_coordinate[2]
        or source_coordinate[:2] not in target_repositories
    ):
        raise LedgerError(
            "legacy rebind filename must name one exact target repository and selected issue number"
        )


def _require_exact_legacy_claim(
    source_name: str,
    source_raw: bytes,
    candidate: Mapping[str, Any],
    *,
    require_canonical_name: bool = True,
    related_sources: Sequence[tuple[str, bytes]] = (),
) -> set[tuple[str, str, str, str]]:
    expected_name = _legacy_source_name(candidate)
    if require_canonical_name and source_name != expected_name:
        raise LedgerError(f"legacy migration source must be exactly {expected_name}")
    claims = [
        _legacy_claim(name, raw, None)
        for name, raw in [(source_name, source_raw), *related_sources]
    ]
    if any(claim.ambiguous or claim.evidence_invalid for claim in claims):
        raise LedgerError("legacy report evidence is duplicate or ambiguous")
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
    claimed_issues = {issue for claim in claims for issue in claim.issues}
    claimed_pulls = {pull for claim in claims for pull in claim.pull_requests}
    claimed_heads = {head for claim in claims for head in claim.repository_heads}
    claimed_worktrees = {path for claim in claims for path in claim.worktrees}
    expected_heads = {
        (
            target["repository"]["owner"].casefold(),
            target["repository"]["name"].casefold(),
            target["head"]["branch"].casefold(),
        )
        for target in candidate["targets"]
    }
    expected_worktrees = _reserved_worktree_paths(candidate)
    if not require_canonical_name:
        for claim in claims:
            filename = _LEGACY_REPORT_RE.fullmatch(claim.name)
            if filename is None:
                raise LedgerError("legacy rebind source filename is invalid")
            source_repository = (
                filename.group("owner").casefold(),
                filename.group("repo").casefold(),
            )
            matching_targets = [
                target
                for target in candidate["targets"]
                if (
                    target["repository"]["owner"].casefold(),
                    target["repository"]["name"].casefold(),
                )
                == source_repository
            ]
            if len(matching_targets) != 1:
                raise LedgerError(
                    "legacy rebind source must prove one exact target repository"
                )
            source_target = matching_targets[0]
            source_head_value = (
                source_repository[0],
                source_repository[1],
                source_target["head"]["branch"].casefold(),
            )
            source_head = {source_head_value}
            source_worktrees = _reserved_repository_worktree_paths(
                candidate, source_target["repository"]["node_id"]
            )
            if (
                set(claim.issues) != expected_issues
                or set(claim.pull_requests) != expected_pulls
                or set(claim.repository_heads) != source_head
                or set(claim.worktrees) != source_worktrees
            ):
                raise LedgerError(
                    "legacy rebind source does not exactly prove its target coordinates"
                )
    if (
        claimed_issues != expected_issues
        or claimed_pulls != expected_pulls
        or claimed_heads != expected_heads
        or claimed_worktrees != expected_worktrees
    ):
        raise LedgerError(
            "legacy report claim does not exactly match candidate issue/PR/head/worktree coordinates"
        )
    return {
        head for claim in claims for head in claim.repository_head_commits
    }


def _check_candidate_inventory(
    current: Inventory,
    candidate: Mapping[str, Any],
    raw: bytes,
    *,
    allowed_pending: set[tuple[str, str, str]],
    allowed_legacy_sources: set[str] | None = None,
) -> None:
    _require_exact_pending(current, allowed_pending)
    _reject_legacy_claims(
        current, candidate, allowed_sources=allowed_legacy_sources
    )
    target = canonical_name(candidate)
    proposed = Snapshot(target, dict(candidate), raw, byte_digest(raw), 0, 0)
    historical_keys = frozenset(
        (snapshot.name, snapshot.digest) for snapshot in current.historical_ledgers
    )
    _reject_overlaps(
        [*current.ledgers, *current.historical_ledgers, proposed],
        inert_keys=historical_keys,
    )


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


def _release_plan(
    snapshot: Snapshot, request: Mapping[str, Any], *, live_proof: bool = True
) -> tuple[dict[str, Any], bytes, str]:
    document = _release_document(snapshot, request, live_proof=live_proof)
    raw = canonical_bytes(document)
    return document, raw, byte_digest(raw)


@contextmanager
def _release_resource_safety(
    document: Mapping[str, Any], request: Mapping[str, Any] | None
) -> Iterator[_LiveResourceGuard]:
    """Hold topology leases and re-observe stopped lifecycle to commit."""

    topologies = [
        resource for resource in document["resources"]
        if resource["kind"] == "topology" and resource["current"] is not None
    ]
    runtimes = [
        resource for resource in document["resources"]
        if resource["kind"] == "runtime" and resource["current"] is not None
    ]
    if runtimes:
        raise LedgerError("release runtime resource lacks an exact live verifier")
    if not topologies:
        yield _LiveResourceGuard(lambda: None)
        return
    if request is None:
        raise LedgerError("release topology lacks wrapper provenance")
    wrapper_root = request["roots"]["wrapper"]["path"]
    workspace_root = request["roots"]["workspace"]["path"]
    saved = _enter_workspace_environment(workspace_root)
    workspace = None
    try:
        module = _load_workspace_module(wrapper_root)
        workspace = module.Workspace(Path(wrapper_root), backfill_references=False)
        leases = [
            workspace._lease_request(
                "topology", resource["current"]["name"], "shared", "delivery release proof"
            )
            for resource in topologies
        ]
        with workspace._resource_locks(leases, nonblocking=True):
            def recheck() -> None:
                for resource in topologies:
                    current = resource["current"]
                    expected_path = workspace.paths.topologies / current["name"]
                    if current["lifecycle"] != "stopped" or current["path"] != str(expected_path):
                        raise LedgerError("release topology identity is not stopped and exact")
                    status = workspace.topology_status(current["name"])
                    supervisor = status.get("supervisor", {})
                    services = status.get("services", {})
                    if (
                        supervisor.get("liveness") in {"live", "unreachable"}
                        or any(
                            row.get("status") == "running" or row.get("running") is True
                            for row in services.values()
                            if isinstance(row, dict)
                        )
                    ):
                        raise LedgerError("release topology became live or unreachable")
            guard = _LiveResourceGuard(recheck)
            guard.prove()
            yield guard
    except LedgerError:
        raise
    except Exception as error:
        raise LedgerError(f"release resource safety proof failed: {error}") from error
    finally:
        if workspace is not None:
            workspace.close()
        _leave_workspace_environment(saved)


@contextmanager
def _release_live_safety(
    document: Mapping[str, Any]
) -> Iterator[list[_LiveWorktreeGuard | _LiveResourceGuard]]:
    """Hold wrapper coordinate leases and exact live worktree guards to commit."""

    guards: list[_LiveWorktreeGuard | _LiveResourceGuard] = []
    wrapper_request: Mapping[str, Any] | None = None
    with ExitStack() as stack:
        for slot in document["artifacts"]:
            if slot["kind"] != "worktree" or slot["current"] is None:
                continue
            request = slot.get("primitive_request")
            scope_record = None
            allowed: frozenset[str] = frozenset()
            if request is None and slot.get("producer_resource_slot") is not None:
                scopes = [
                    resource for resource in document["resources"]
                    if resource["slot_id"] == slot["producer_resource_slot"]
                    and resource["kind"] == "scope"
                    and resource["current"] is not None
                    and resource["current"]["lifecycle"] == "active"
                ]
                if len(scopes) == 1:
                    scope = scopes[0]
                    retained = _retained_result(
                        scope["current"]["binding"], "release scope binding"
                    )
                    scope_record, _ = _scope_show_record(
                        retained,
                        scope["request"],
                        scope["immutable"]["repository"],
                        "release scope binding",
                    )
                    request = _scope_worktree_request(
                        scope["request"], scope["immutable"]["repository"]
                    )
                    allowed = _scope_owned_references(scope["request"])
            if request is None:
                raise LedgerError("release worktree lacks live wrapper provenance")
            if wrapper_request is None:
                wrapper_request = request
            live_request = _decode(canonical_bytes(request), "release worktree request")
            live_request["expected_head_sha"] = slot["current"]["head_sha"]
            guard = stack.enter_context(
                _pinned_live_worktree(
                    live_request,
                    slot["current"]["path"],
                    f"release worktree {slot['slot_id']}",
                    allowed_references=allowed,
                    scope_record=scope_record,
                )
            )
            guards.append(guard)
        guards.append(
            stack.enter_context(_release_resource_safety(document, wrapper_request))
        )
        for resource in document["resources"]:
            current = resource["current"]
            if resource["state"] == "planned" or current is None:
                continue
            if resource["kind"] == "scope":
                if current["lifecycle"] != "active":
                    raise LedgerError("release scope is not live and active")
                continue
            if current["lifecycle"] in {"running", "active", "ready"}:
                raise LedgerError("release resource became active")
            path = current["path"]
            if path is not None and (not Path(path).exists() or Path(path).is_symlink()):
                raise LedgerError("release resource path changed or disappeared")
        for guard in guards:
            guard.prove()
        yield guards


def release_preview(
    root: Path | str, name: str, request: Mapping[str, Any]
) -> dict[str, Any]:
    """Return a mutation-free plan for one exact post-merge release."""

    target = _direct_name(name)
    with _locked_root(Path(root)) as directory:
        current = _inventory_locked(directory)
        if any(item.target == target for item in current.pending):
            raise LedgerError(f"release is blocked by a pending operation for {target}")
        snapshot = _snapshot(directory, target)
        marker_name = _release_name(target)
        existing = next(
            (row for row in current.releases if row.ledger_name == target), None
        )
        document, raw, plan = _release_plan(
            snapshot, request, live_proof=existing is None
        )
        if existing is not None and existing.raw != raw:
            raise LedgerError(f"ledger already has a different terminal release: {target}")
        return {
            "mode": "preview",
            "plan_sha256": plan,
            "marker": marker_name,
            "ledger": document["ledger"],
            "state": "released" if existing is not None else "eligible",
        }


def release_apply(
    root: Path | str,
    name: str,
    request: Mapping[str, Any],
    *,
    plan_sha256: str,
    failpoint: Failpoint = None,
) -> ReleaseRecord:
    """Durably release active coordinates after an exact hash-bound preview."""

    target = _direct_name(name)
    _string(plan_sha256, "release plan digest", SHA256_RE)
    with _locked_root(Path(root)) as directory:
        current = _inventory_locked(directory)
        snapshot = _snapshot(directory, target)
        marker_name = _release_name(target)
        existing = next(
            (row for row in current.releases if row.ledger_name == target), None
        )
        document, raw, candidate = _release_plan(
            snapshot, request, live_proof=existing is None
        )
        if candidate != plan_sha256:
            raise LedgerError("release plan digest is stale or unrelated")
        stage = f".{target}.release-{candidate}.tmp"
        _require_names_fit(directory, (marker_name, stage))
        allowed_stage = ("release", target, stage)
        unexpected = [
            item for item in current.pending
            if item.target == target and (item.kind, item.target, item.staging) != allowed_stage
        ]
        if unexpected:
            raise LedgerError(f"release is blocked by a pending operation for {target}")
        if existing is not None:
            if existing.raw != raw:
                raise LedgerError(f"ledger already has a different terminal release: {target}")
            if _exists(directory, stage):
                staged_raw, staged_status = _read_regular(
                    directory, stage, managed=True, expected_nlinks={2}
                )
                if staged_raw != raw:
                    raise LedgerError(f"release staging content mismatch: {stage}")
                _unlink_exact(directory, stage, staged_status)
            return existing
        stage_status = _ensure_stage(
            directory, stage, raw, allow_prefix_resume=True, expected_nlinks={1, 2}
        )
        _hit(failpoint, "release:staged")
        # The root lock excludes every helper writer. Re-read the ledger tuple
        # immediately before installation so stale authority cannot win.
        current_snapshot = _snapshot(directory, target)
        if (
            current_snapshot.digest != snapshot.digest
            or current_snapshot.document["generation"] != snapshot.document["generation"]
            or (current_snapshot.device, current_snapshot.inode)
            != (snapshot.device, snapshot.inode)
        ):
            raise LedgerError("release ledger tuple changed before installation")
        # Staging is a deliberate crash boundary. Repeat every live Git proof
        # after it so an external worktree mutation cannot ride an older proof
        # into the terminal marker.
        with _release_live_safety(current_snapshot.document) as guards:
            rechecked_document, rechecked_raw, rechecked_candidate = _release_plan(
                current_snapshot, request, live_proof=True
            )
            if (
                rechecked_document != document
                or rechecked_raw != raw
                or rechecked_candidate != candidate
            ):
                raise LedgerError("release evidence changed before installation")
            for guard in guards:
                guard.prove()
            try:
                os.link(
                    stage,
                    marker_name,
                    src_dir_fd=directory,
                    dst_dir_fd=directory,
                    follow_symlinks=False,
                )
            except FileExistsError as error:
                raise LedgerError(f"release marker appeared: {marker_name}") from error
        _fsync(directory, f"review root after installing {marker_name}")
        _hit(failpoint, "release:installed")
        marker_raw, marker_status = _read_regular(
            directory, marker_name, managed=True, expected_nlinks={2}
        )
        if marker_raw != raw or (marker_status.st_dev, marker_status.st_ino) != (
            stage_status.st_dev,
            stage_status.st_ino,
        ):
            raise LedgerError("installed release marker differs from its staged candidate")
        _unlink_exact(directory, stage, stage_status)
        _hit(failpoint, "release:cleaned")
        final = _inventory_locked(directory)
        matches = [row for row in final.releases if row.ledger_name == target]
        if len(matches) != 1 or matches[0].raw != raw:
            raise LedgerError("terminal release did not become authoritative")
        return matches[0]


def _archive_member_names(directory: int, snapshot: Snapshot) -> list[str]:
    names = {
        snapshot.name,
        _release_name(snapshot.name),
        f".{snapshot.name}.lock",
        snapshot.name.removesuffix(".ledger.json"),
    }
    migration = snapshot.document["migration"]
    if migration is not None:
        names.update(
            {
                migration["source"]["name"],
                migration["snapshot"]["name"],
                migration["canonical_report"],
                migration["marker_name"],
            }
        )
    with os.scandir(directory) as entries:
        for entry in entries:
            if any(
                (match := pattern.fullmatch(entry.name)) is not None
                and match.group("target") == snapshot.name
                for pattern in (
                    _HEAD_CORRECTION_PREDECESSOR_RE,
                    _HEAD_CORRECTION_ERRONEOUS_RE,
                    _HEAD_CORRECTION_RECEIPT_RE,
                )
            ):
                names.add(entry.name)
    return sorted(names, key=str.casefold)


def _archive_plan_locked(
    directory: int,
    snapshot: Snapshot,
    release: ReleaseRecord,
    request: Mapping[str, Any],
) -> tuple[dict[str, Any], bytes, str, str]:
    # The caller holds `_archive_live_safety`, which performs the live scope
    # proof and retains the wrapper coordinate leases through installation.
    prepared = _archive_request(snapshot, release, request, live_scope_proof=False)
    members: list[dict[str, Any]] = []
    for name in _archive_member_names(directory, snapshot):
        try:
            raw, status = _read_regular(directory, name)
        except LedgerError:
            if name in {snapshot.name, _release_name(snapshot.name), f".{snapshot.name}.lock"}:
                raise LedgerError(f"required archive member is missing or unsafe: {name}")
            continue
        members.append(_archive_member(name, raw, status))
    document = _validate_archive_document(
        {
            "transaction": "delivery-ledger-archive-v1",
            "state": "complete",
            "ledger": release.document["ledger"],
            "release_sha256": release.digest,
            **prepared,
            "members": members,
        },
        "archive",
    )
    raw = canonical_bytes(document)
    digest = byte_digest(raw)
    name = f".{snapshot.name}.archive-{snapshot.digest}.json"
    return document, raw, digest, name


def _archive_build_roots(
    scopes: Mapping[str, tuple[Mapping[str, Any], Mapping[str, Any], bytes]],
    resources: list[Mapping[str, Any]],
) -> list[Path]:
    """Return one build root for each canonical profile-build lock."""

    roots: dict[str, Path] = {}

    def add(root: Path) -> None:
        previous = roots.get(root.name)
        if previous is not None and previous != root:
            raise LedgerError("archive build lock coordinate maps to multiple roots")
        roots.setdefault(root.name, root)

    for _slot_id, (_resource, _record, journal_raw) in scopes.items():
        journal = _decode(journal_raw, "archive scope release journal")
        for item in journal["plan"]["items"]:
            if item.get("kind") != "build" or item.get("disposition") != "eligible":
                continue
            add(Path(item["path"]))
    for resource in resources:
        current = resource["current"]
        if (
            current is not None
            and resource["kind"] == "build"
            and current["path"] is not None
        ):
            add(Path(current["path"]))
    return list(roots.values())


@contextmanager
def _archive_live_safety(
    snapshot: Snapshot, request: Mapping[str, Any]
) -> Iterator[Callable[[], None]]:
    """Hold wrapper cleanup coordinates and reprove their terminal state."""

    document = snapshot.document
    scopes: dict[str, tuple[Mapping[str, Any], Mapping[str, Any], bytes]] = {}
    wrapper_request: Mapping[str, Any] | None = None
    worktree_requests: list[tuple[Mapping[str, Any], str]] = []
    for slot in document["artifacts"]:
        if slot["kind"] != "worktree" or slot["current"] is None:
            continue
        primitive = slot.get("primitive_request")
        if primitive is None and slot.get("producer_resource_slot") is not None:
            matches = [
                resource for resource in document["resources"]
                if resource["slot_id"] == slot["producer_resource_slot"]
                and resource["kind"] == "scope"
                and resource["current"] is not None
            ]
            if len(matches) != 1:
                raise LedgerError("archive worktree lacks one scope producer")
            resource = matches[0]
            _prove_scope_release(resource, f"archive resource {resource['slot_id']}")
            retained = _retained_result(
                resource["current"]["binding"], "archive scope binding"
            )
            record, _ = _scope_show_record(
                retained,
                resource["request"],
                resource["immutable"]["repository"],
                "archive scope binding",
            )
            journal_path = record["cleanup"]["release_journal"]
            journal_raw = _read_bytes_input(journal_path)
            scopes[resource["slot_id"]] = (resource, record, journal_raw)
            primitive = _scope_worktree_request(
                resource["request"], resource["immutable"]["repository"]
            )
        if primitive is None:
            raise LedgerError("archive worktree lacks wrapper provenance")
        if wrapper_request is None:
            wrapper_request = primitive
        elif (
            wrapper_request["roots"]["wrapper"] != primitive["roots"]["wrapper"]
            or wrapper_request["roots"]["workspace"] != primitive["roots"]["workspace"]
        ):
            raise LedgerError("archive coordinates span different wrapper roots")
        worktree_requests.append((primitive, slot["current"]["path"]))
    if wrapper_request is None:
        raise LedgerError("archive lacks wrapper provenance for cleanup proof")
    workspace_root = wrapper_request["roots"]["workspace"]["path"]
    wrapper_root = wrapper_request["roots"]["wrapper"]["path"]
    cleanup_journal = _prove_cleanup_journal(request["cleanup"], workspace_root)
    saved = _enter_workspace_environment(workspace_root)
    workspace = None
    build_descriptors: list[int] = []
    try:
        module = _load_workspace_module(wrapper_root)
        workspace = module.Workspace(Path(wrapper_root), backfill_references=False)
        leases: list[Any] = [
            workspace._lease_request(
                "registry", "physical-references", "shared", "delivery archive proof"
            ),
            workspace._lease_request(
                "registry",
                _cleanup_journal_lease_coordinate(cleanup_journal),
                "shared",
                "delivery archive cleanup journal proof",
            ),
        ]
        for primitive, path in worktree_requests:
            wrapper_self = (
                primitive["component"] == "atrinik"
                and primitive["physical_checkout"] == "atrinik"
                and primitive["roots"]["primary"] == primitive["roots"]["wrapper"]
            )
            if wrapper_self:
                admin = workspace._wrapper_git_admin_coordinate()
            else:
                checkout = workspace._resolve_checkout(primitive["component"])
                if checkout.name != primitive["physical_checkout"]:
                    raise LedgerError("archive checkout identity changed")
                admin = workspace._git_admin_coordinate(
                    checkout, Path(primitive["roots"]["primary"]["path"])
                )
            leases.extend(
                (
                    workspace._lease_request(
                        "git-admin", admin, "shared", "delivery archive proof"
                    ),
                    workspace._lease_request(
                        "source",
                        workspace._source_coordinate(
                            primitive["physical_checkout"], Path(path)
                        ),
                        "exclusive",
                        "delivery archive proof",
                    ),
                    workspace._lease_request(
                        "source",
                        workspace._physical_source_coordinate(Path(path)),
                        "exclusive",
                        "delivery archive proof",
                    ),
                )
            )
        for _slot_id, (_resource, record, _journal_raw) in scopes.items():
            leases.extend(
                (
                    workspace._lease_request(
                        "registry", f"scope:{record['name']}", "shared", "delivery archive proof"
                    ),
                    workspace._lease_request(
                        "profile", record["profile"]["name"], "exclusive", "delivery archive proof"
                    ),
                    workspace._lease_request(
                        "topology", record["topology"]["name"], "shared", "delivery archive proof"
                    ),
                )
            )
        cleanup_resources = {
            row["slot_id"]: row for row in request["cleanup"]["resources"]
        }
        for resource in document["resources"]:
            current = resource["current"]
            kind = resource["kind"]
            # Scope coordinates have dedicated leases above, build roots use
            # their canonical profile-build locks below, and the shared
            # physical-reference registry lease covers reference resources.
            if current is None or kind in {"scope", "build", "reference"}:
                continue
            if kind == "runtime":
                raise LedgerError(
                    "archive runtime resource lacks an exact lease mapping"
                )
            if kind not in {"profile", "topology", "scenario", "state"}:
                raise LedgerError(f"archive resource lease kind is unsupported: {kind}")
            leases.append(
                workspace._lease_request(
                    kind,
                    current["name"],
                    "shared",
                    "delivery archive proof",
                )
            )
        with ExitStack() as stack:
            stack.enter_context(workspace._resource_locks(leases, nonblocking=True))
            acquired_build_locks: set[Path] = set()

            def acquire_build_lock(root: Path) -> None:
                lock_path = workspace.paths.builds / "locks" / f"{root.name}.lock"
                if lock_path in acquired_build_locks:
                    return
                lock_path.parent.mkdir(parents=True, exist_ok=True)
                descriptor = os.open(
                    lock_path,
                    os.O_RDWR | os.O_CREAT | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0),
                    0o600,
                )
                try:
                    fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BaseException:
                    os.close(descriptor)
                    raise
                build_descriptors.append(descriptor)
                acquired_build_locks.add(lock_path)

            for root in _archive_build_roots(scopes, document["resources"]):
                acquire_build_lock(root)

            def recheck() -> None:
                _prove_cleanup_journal(request["cleanup"], workspace_root)
                for primitive, path in worktree_requests:
                    if Path(path).exists() or Path(path).is_symlink():
                        raise LedgerError(f"archive worktree still exists: {path}")
                    if workspace._worktree_path_registered(
                        Path(primitive["roots"]["primary"]["path"]), Path(path)
                    ):
                        raise LedgerError(
                            f"archive worktree is still registered: {path}"
                        )
                    if workspace._source_references(Path(path)):
                        raise LedgerError(f"archive worktree is still referenced: {path}")
                for _slot_id, (_resource, record, journal_raw) in scopes.items():
                    if _read_bytes_input(record["cleanup"]["release_journal"]) != journal_raw:
                        raise LedgerError("archive scope release journal changed")
                    journal = _decode(journal_raw, "archive scope release journal")
                    _prove_fresh_scope_release(
                        workspace,
                        record,
                        journal["plan"],
                        "archive scope release",
                    )
                    for item in journal["plan"]["items"]:
                        if item.get("disposition") == "eligible":
                            path = item.get("path")
                            if not isinstance(path, str) or Path(path).exists() or Path(path).is_symlink():
                                raise LedgerError(
                                    f"archive released scope coordinate still exists: {path}"
                                )
                    for row in record["worktrees"]:
                        branch_head = module.git(
                            Path(row["primary_path"]),
                            "for-each-ref",
                            "--format=%(objectname)",
                            f"refs/heads/{row['branch']}",
                            capture=True,
                            trace=False,
                        )
                        if branch_head:
                            raise LedgerError(
                                f"archive released scope branch reappeared: {row['branch']}"
                            )
                for resource in document["resources"]:
                    current = resource["current"]
                    if current is None or resource["kind"] == "scope":
                        continue
                    observed = cleanup_resources[resource["slot_id"]]
                    path = current["path"]
                    if observed["disposition"] == "removed":
                        if path is None or Path(path).exists() or Path(path).is_symlink():
                            raise LedgerError(
                                f"archive removed resource still exists: {resource['slot_id']}"
                            )
                    elif path is not None and (not Path(path).exists() or Path(path).is_symlink()):
                        raise LedgerError(
                            f"archive retained resource changed: {resource['slot_id']}"
                        )
                    if resource["kind"] == "topology" and observed["disposition"] == "retained":
                        status = workspace.topology_status(current["name"])
                        if status.get("supervisor", {}).get("liveness") in {"live", "unreachable"}:
                            raise LedgerError("archive retained topology is live or unreachable")
                _prove_cleanup_journal(request["cleanup"], workspace_root)

            recheck()
            yield recheck
    except LedgerError:
        raise
    except Exception as error:
        raise LedgerError(f"archive live cleanup proof failed: {error}") from error
    finally:
        for descriptor in reversed(build_descriptors):
            try:
                fcntl.flock(descriptor, fcntl.LOCK_UN)
            finally:
                os.close(descriptor)
        if workspace is not None:
            workspace.close()
        _leave_workspace_environment(saved)


def _archive_request_from_document(document: Mapping[str, Any]) -> dict[str, Any]:
    return {
        key: document[key]
        for key in ("authority", "archived_at", "retain_until", "cleanup")
    }


def archive_preview(
    root: Path | str, name: str, request: Mapping[str, Any]
) -> dict[str, Any]:
    """Preview archival after a separately completed cleanup preview/apply."""

    target = _direct_name(name)
    with _locked_root(Path(root)) as directory:
        current = _inventory_locked(directory)
        archived = next(
            (row for row in current.archives if row.ledger_name == target), None
        )
        if archived is not None:
            if _archive_request_from_document(archived.document) != _decode(
                canonical_bytes(request), "archive request"
            ):
                raise LedgerError(f"ledger already has a different archive: {target}")
            return {
                "mode": "preview",
                "plan_sha256": archived.digest,
                "archive": archived.name,
                "ledger": archived.document["ledger"],
                "state": "archived",
            }
        if any(item.target == target for item in current.pending):
            raise LedgerError(f"archive is blocked by a pending operation for {target}")
        snapshot = _snapshot(directory, target)
        release = next(
            (row for row in current.releases if row.ledger_name == target), None
        )
        if release is None:
            raise LedgerError("archive requires an authoritative terminal release")
        _archive_request(snapshot, release, request, live_scope_proof=False)
        with _archive_live_safety(snapshot, request) as recheck:
            document, _raw, plan, archive_name = _archive_plan_locked(
                directory, snapshot, release, request
            )
            recheck()
        return {
            "mode": "preview",
            "plan_sha256": plan,
            "archive": archive_name,
            "ledger": document["ledger"],
            "state": "eligible",
        }


def _finish_archive_members(
    directory: int, archive: ArchiveRecord, *, failpoint: Failpoint = None
) -> None:
    for index, member in enumerate(archive.document["members"]):
        name, raw, mode, device, inode = _archive_member_bytes(
            member, f"archive member[{index}]"
        )
        try:
            current_raw, status = _read_regular(directory, name)
        except LedgerError as error:
            try:
                os.stat(name, dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                continue
            raise error
        if (
            current_raw != raw
            or stat.S_IMODE(status.st_mode) != mode
            or (status.st_dev, status.st_ino) != (device, inode)
        ):
            raise LedgerError(f"archive member changed before removal: {name}")
        _unlink_exact(directory, name, status)
        _hit(failpoint, f"archive:member:{name}")


def _archive_snapshot(archive: ArchiveRecord) -> Snapshot:
    identity = _release_ledger_identity(
        archive.document["ledger"], "installed archive ledger"
    )
    members = [
        _archive_member_bytes(member, f"installed archive member[{index}]")
        for index, member in enumerate(archive.document["members"])
    ]
    ledger_members = [member for member in members if member[0] == identity[0]]
    if len(ledger_members) != 1:
        raise LedgerError("installed archive lost its canonical ledger member")
    name, raw, _mode, device, inode = ledger_members[0]
    document = _decode(raw, "installed archive ledger member")
    validate(document)
    if raw != canonical_bytes(document) or byte_digest(raw) != identity[3]:
        raise LedgerError("installed archive ledger member is mismatched")
    return Snapshot(name, document, raw, identity[3], device, inode)


def _require_archive_members_absent(directory: int, archive: ArchiveRecord) -> None:
    remaining: list[str] = []
    for index, member in enumerate(archive.document["members"]):
        name, _raw, _mode, _device, _inode = _archive_member_bytes(
            member, f"reclaim archive member[{index}]"
        )
        if _exists(directory, name):
            remaining.append(name)
    if remaining:
        raise LedgerError(
            "archive member cleanup is incomplete; retry archive apply before reclaim: "
            + ", ".join(sorted(remaining, key=str.casefold))
        )


def archive_apply(
    root: Path | str,
    name: str,
    request: Mapping[str, Any],
    *,
    plan_sha256: str,
    failpoint: Failpoint = None,
) -> ArchiveRecord:
    """Bundle helper evidence; never remove the worktree or runtime resources."""

    target = _direct_name(name)
    _string(plan_sha256, "archive plan digest", SHA256_RE)
    with _locked_root(Path(root)) as directory:
        current = _inventory_locked(directory)
        archived = next(
            (row for row in current.archives if row.ledger_name == target), None
        )
        stage: str | None = None
        if archived is None:
            snapshot = _snapshot(directory, target)
            release = next(
                (row for row in current.releases if row.ledger_name == target), None
            )
            if release is None:
                raise LedgerError("archive requires an authoritative terminal release")
            _archive_request(snapshot, release, request, live_scope_proof=False)
            with _archive_live_safety(snapshot, request) as recheck:
                _document, raw, candidate, archive_name = _archive_plan_locked(
                    directory, snapshot, release, request
                )
                if candidate != plan_sha256:
                    raise LedgerError("archive plan digest is stale or unrelated")
                stage = (
                    f".{target}.archive-{snapshot.digest}-to-{candidate}.tmp"
                )
                _require_names_fit(directory, (archive_name, stage))
                unexpected = [
                    item for item in current.pending
                    if item.target == target
                    and (item.kind, item.target, item.staging)
                    != ("archive", target, stage)
                ]
                if unexpected:
                    raise LedgerError(f"archive is blocked by a pending operation for {target}")
                stage_status = _ensure_stage(
                    directory,
                    stage,
                    raw,
                    allow_prefix_resume=True,
                    expected_nlinks={1, 2},
                    limit=MAX_ARCHIVE_BYTES,
                )
                _hit(failpoint, "archive:staged")
                recheck()
                try:
                    os.link(
                        stage,
                        archive_name,
                        src_dir_fd=directory,
                        dst_dir_fd=directory,
                        follow_symlinks=False,
                    )
                except FileExistsError as error:
                    raise LedgerError(f"archive appeared: {archive_name}") from error
                _fsync(directory, f"review root after installing {archive_name}")
                _hit(failpoint, "archive:installed")
                recheck()
                archive_raw, archive_status = _read_regular(
                    directory,
                    archive_name,
                    managed=True,
                    expected_nlinks={2},
                    limit=MAX_ARCHIVE_BYTES,
                )
                if archive_raw != raw or (archive_status.st_dev, archive_status.st_ino) != (
                    stage_status.st_dev,
                    stage_status.st_ino,
                ):
                    raise LedgerError("installed archive differs from its staged candidate")
                archived = _archive_record(archive_name, archive_raw, archive_status)
                _finish_archive_members(directory, archived, failpoint=failpoint)
        else:
            if archived.digest != plan_sha256:
                raise LedgerError("archive plan digest is stale or unrelated")
            if _archive_request_from_document(archived.document) != _decode(
                canonical_bytes(request), "archive request"
            ):
                raise LedgerError("archive request differs from the installed archive")
            matching_stages = [
                item.staging for item in current.pending
                if item.kind == "archive" and item.target == target
            ]
            if len(matching_stages) > 1:
                raise LedgerError("multiple archive stages exist")
            stage = matching_stages[0] if matching_stages else None
            snapshot = _archive_snapshot(archived)
            with _archive_live_safety(snapshot, request) as recheck:
                recheck()
                _finish_archive_members(directory, archived, failpoint=failpoint)
        if stage is not None and _exists(directory, stage):
            stage_raw, stage_status = _read_regular(
                directory,
                stage,
                managed=True,
                expected_nlinks={2},
                limit=MAX_ARCHIVE_BYTES,
            )
            if stage_raw != archived.raw:
                raise LedgerError("archive staging content changed")
            _unlink_exact(directory, stage, stage_status)
        _hit(failpoint, "archive:cleaned")
        final = _inventory_locked(directory)
        matches = [row for row in final.archives if row.ledger_name == target]
        if len(matches) != 1 or matches[0].digest != plan_sha256:
            raise LedgerError("archive did not reach its terminal state")
        return matches[0]


def _validate_reclaim_preview(value: Any) -> dict[str, Any]:
    item = _exact(
        value,
        {
            "mode",
            "plan_sha256",
            "archive",
            "archive_sha256",
            "device",
            "inode",
            "observed_at",
            "state",
        },
        "reclaim preview",
    )
    if item["mode"] != "preview" or item["state"] != "eligible":
        raise LedgerError("reclaim apply requires an eligible preview")
    archive = _direct_name(item["archive"], "reclaim archive")
    digest = _string(item["archive_sha256"], "reclaim archive digest", SHA256_RE)
    device = _integer(item["device"], "reclaim archive device", minimum=0)
    inode = _integer(item["inode"], "reclaim archive inode")
    observed = _string(item["observed_at"], "reclaim observed_at", TIMESTAMP_RE)
    _timestamp_key(observed, "reclaim observed_at")
    plan = _string(item["plan_sha256"], "reclaim plan digest", SHA256_RE)
    expected = canonical_object_digest(
        {
            "operation": "reclaim",
            "archive": archive,
            "sha256": digest,
            "device": device,
            "inode": inode,
            "observed_at": observed,
        }
    )
    if plan != expected:
        raise LedgerError("reclaim preview plan is malformed or unrelated")
    return item


def reclaim_preview(root: Path | str, name: str) -> dict[str, Any]:
    archive_name = _direct_name(name, "archive name")
    observed = _utc_now()
    with _locked_root(Path(root)) as directory:
        current = _inventory_locked(directory)
        matches = [row for row in current.archives if row.name == archive_name]
        if len(matches) != 1:
            raise LedgerError("reclaim requires one exact archive")
        archive = matches[0]
        if _timestamp_key(observed, "reclaim observed_at") < _timestamp_key(
            archive.document["retain_until"], "archive retain_until"
        ):
            raise LedgerError("archive retention period has not elapsed")
        _require_archive_members_absent(directory, archive)
        plan = canonical_object_digest(
            {
                "operation": "reclaim",
                "archive": archive.name,
                "sha256": archive.digest,
                "device": archive.device,
                "inode": archive.inode,
                "observed_at": observed,
            }
        )
        return {
            "mode": "preview",
            "plan_sha256": plan,
            "archive": archive.name,
            "archive_sha256": archive.digest,
            "device": archive.device,
            "inode": archive.inode,
            "observed_at": observed,
            "state": "eligible",
        }


def reclaim_apply(
    root: Path | str,
    preview: Mapping[str, Any],
    *,
    plan_sha256: str,
    failpoint: Failpoint = None,
) -> dict[str, Any]:
    plan = _string(plan_sha256, "reclaim plan digest", SHA256_RE)
    item = _validate_reclaim_preview(
        _decode(canonical_bytes(preview), "reclaim preview")
    )
    if item["plan_sha256"] != plan:
        raise LedgerError("reclaim apply requires its exact eligible preview")
    archive_name = _direct_name(item["archive"], "reclaim archive")
    receipt_name = f".{archive_name}.reclaim-{plan}.json"
    complete_name = _RECLAIM_COMPLETE_NAME
    complete_stage = f".delivery-ledger-reclaim-complete-{plan}.tmp"
    preview_raw = canonical_bytes(item)
    with _locked_root(Path(root)) as directory:
        _require_names_fit(
            directory, (archive_name, receipt_name, complete_name, complete_stage)
        )
        current = _inventory_locked(directory)
        matches = [row for row in current.archives if row.name == archive_name]
        completed = [
            row for row in current.reclaims
            if row.archive_name == archive_name and row.plan == plan
        ]
        receipts = [
            row for row in current.pending
            if row.kind == "reclaim" and row.target == archive_name
        ]
        completion_stages = [
            row for row in current.pending if row.kind == "reclaim-complete"
        ]
        if completion_stages and (
            len(completion_stages) != 1
            or completion_stages[0].staging != complete_stage
        ):
            raise LedgerError(
                "a prior reclaim completion must be retried before another reclaim"
            )

        def publish_completion() -> None:
            _ensure_stage(directory, complete_stage, preview_raw)
            if _exists(directory, complete_name):
                complete_raw, complete_status = _read_regular(
                    directory, complete_name, managed=True, expected_nlinks={1}
                )
                if complete_raw == preview_raw:
                    stage_raw, stage_status = _read_regular(
                        directory, complete_stage, managed=True, expected_nlinks={1}
                    )
                    if stage_raw != preview_raw:
                        raise LedgerError("reclaim completion stage changed")
                    _unlink_exact(directory, complete_stage, stage_status)
                    return
                _validate_reclaim_preview(_decode(complete_raw, complete_name))
                _unlink_exact(directory, complete_name, complete_status)
            os.rename(
                complete_stage,
                complete_name,
                src_dir_fd=directory,
                dst_dir_fd=directory,
            )
            _fsync(directory, "review root after publishing reclaim completion")

        def discard_receipt() -> None:
            if not _exists(directory, receipt_name):
                return
            receipt_raw, receipt_status = _read_regular(
                directory, receipt_name, managed=True, expected_nlinks={1}
            )
            if receipt_raw != preview_raw:
                raise LedgerError("reclaim receipt differs from the exact preview")
            _unlink_exact(directory, receipt_name, receipt_status)

        if completed:
            if len(completed) != 1 or completed[0].raw != preview_raw or matches:
                raise LedgerError("reclaim completion identity is ambiguous")
            publish_completion()
            discard_receipt()
            return {
                "mode": "apply",
                "plan_sha256": plan,
                "archive": archive_name,
                "state": "reclaimed",
            }
        if not matches:
            if len(receipts) != 1 or receipts[0].staging != receipt_name:
                raise LedgerError("reclaim has no exact archive or interrupted receipt")
            receipt_raw, receipt_status = _read_regular(
                directory, receipt_name, managed=True, expected_nlinks={1}
            )
            if receipt_raw != preview_raw:
                raise LedgerError("reclaim receipt differs from the exact preview")
            expected_archive = os.stat_result(
                (stat.S_IFREG | 0o600, item["inode"], item["device"], 1, 0, 0, 0, 0, 0, 0)
            )
            try:
                _unlink_exact(directory, archive_name, expected_archive)
            except FileNotFoundError:
                # The exact helper-created receipt proves this is a retry after
                # the archive removal commit, not a never-existing archive.
                pass
            publish_completion()
            discard_receipt()
            _hit(failpoint, "reclaim:committed")
            return {
                "mode": "apply",
                "plan_sha256": plan,
                "archive": archive_name,
                "state": "reclaimed",
            }
        if len(matches) != 1:
            raise LedgerError("reclaim archive identity is ambiguous")
        if receipts:
            if len(receipts) != 1 or receipts[0].staging != receipt_name:
                raise LedgerError("reclaim receipt identity is ambiguous")
            receipt_raw, _receipt_status = _read_regular(
                directory, receipt_name, managed=True, expected_nlinks={1}
            )
            if receipt_raw != preview_raw:
                raise LedgerError("reclaim receipt differs from the exact preview")
        archive = matches[0]
        expected = canonical_object_digest(
            {
                "operation": "reclaim",
                "archive": archive.name,
                "sha256": archive.digest,
                "device": archive.device,
                "inode": archive.inode,
                "observed_at": item["observed_at"],
            }
        )
        if (
            expected != plan
            or item["archive_sha256"] != archive.digest
            or item["device"] != archive.device
            or item["inode"] != archive.inode
        ):
            raise LedgerError("reclaim archive tuple or plan changed")
        now = _utc_now()
        if _timestamp_key(now, "reclaim apply time") < _timestamp_key(
            archive.document["retain_until"], "archive retain_until"
        ):
            raise LedgerError("archive retention period has not elapsed")
        _require_archive_members_absent(directory, archive)
        if not receipts:
            _ensure_stage(directory, receipt_name, preview_raw)
        _hit(failpoint, "reclaim:prepared")
        _unlink_exact(directory, archive.name, archive.status)
        _hit(failpoint, "reclaim:removed")
        receipt_raw, _receipt_status = _read_regular(
            directory, receipt_name, managed=True, expected_nlinks={1}
        )
        if receipt_raw != preview_raw:
            raise LedgerError("reclaim receipt changed before completion")
        publish_completion()
        discard_receipt()
        _hit(failpoint, "reclaim:committed")
        return {
            "mode": "apply",
            "plan_sha256": plan,
            "archive": archive.name,
            "state": "reclaimed",
        }


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


def _target_refresh_change(
    old: Mapping[str, Any], new: Mapping[str, Any], old_digest: str
) -> dict[str, Any]:
    """Validate and describe one target-only coordinate refresh."""

    old_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in old["targets"]
    }
    new_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in new["targets"]
    }
    if set(old_targets) != set(new_targets):
        raise LedgerError("target refresh target set changed")
    changed = [
        (key, old_targets[key], new_targets[key])
        for key in old_targets
        if old_targets[key] != new_targets[key]
    ]
    if len(changed) != 1:
        raise LedgerError("target refresh requires exactly one changed target")
    key, before, after = changed[0]
    if before["repository"] != after["repository"]:
        raise LedgerError("target refresh repository changed")
    tip_changed = False
    for field in ("base", "head"):
        if (
            before[field]["branch"] != after[field]["branch"]
            or before[field]["initial_sha"] != after[field]["initial_sha"]
        ):
            raise LedgerError(f"target refresh {field} anchor changed")
        if before[field]["current_sha"] == after[field]["current_sha"]:
            if before[field]["lineage"] != after[field]["lineage"]:
                raise LedgerError(f"target refresh {field} unchanged tip rewrote lineage")
        else:
            tip_changed = True
            if after[field]["lineage"] != [
                *before[field]["lineage"],
                after[field]["current_sha"],
            ]:
                raise LedgerError(
                    f"target refresh {field} must append exactly its observed tip"
                )
    if not tip_changed:
        raise LedgerError("target refresh cannot change only the merge base")
    if before["merge_base"]["initial_sha"] != after["merge_base"]["initial_sha"]:
        raise LedgerError("target refresh merge-base anchor changed")

    expected = copy.deepcopy(old)
    expected["generation"] = new["generation"]
    expected["previous_byte_digest"] = old_digest
    expected["history"] = [*old["history"], old_digest]
    expected_targets = {
        (row["repository"]["node_id"], row["head"]["branch"]): row
        for row in expected["targets"]
    }
    expected_targets[key].clear()
    expected_targets[key].update(copy.deepcopy(after))

    expected_artifacts = {row["slot_id"]: row for row in expected["artifacts"]}
    matching_worktrees: list[Mapping[str, Any]] = []
    matching_kinds: list[str] = []
    for slot in old["artifacts"]:
        current = slot["current"]
        if (
            current is None
            or current["repository"]["node_id"] != key[0]
            or current["branch"] != key[1]
            or slot["kind"] not in {"branch", "pull_request", "worktree"}
        ):
            continue
        if current["head_sha"] != before["head"]["current_sha"]:
            raise LedgerError("target refresh predecessor artifact head is not mirrored")
        matching_kinds.append(slot["kind"])
        if slot["kind"] == "worktree":
            matching_worktrees.append(slot)
        if before["head"]["current_sha"] != after["head"]["current_sha"]:
            expected_artifacts[slot["slot_id"]]["current"]["head_sha"] = after[
                "head"
            ]["current_sha"]
    if len(matching_worktrees) != 1 or matching_kinds.count("branch") != 1:
        raise LedgerError("target refresh lacks one bound branch and worktree")
    if matching_kinds.count("pull_request") > 1:
        raise LedgerError("target refresh has multiple bound PR mirrors")
    if expected != new:
        raise LedgerError("target refresh changed unrelated ledger state")
    return {
        "key": key,
        "before": before,
        "after": after,
        "worktree_slot": matching_worktrees[0],
    }


def _transition(
    old: Mapping[str, Any],
    new: Mapping[str, Any],
    old_digest: str,
    *,
    _binding_capability: _AtomicBindingCapability | None = None,
    _target_refresh_capability: _TargetRefreshCapability | None = None,
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
    if target_drift:
        if (
            _target_refresh_capability is None
            or _target_refresh_capability.token is not _TARGET_REFRESH_TOKEN
        ):
            raise LedgerError(
                "target coordinate drift requires live target-refresh-cas authority"
            )
        _target_refresh_change(old, new, old_digest)
    elif _target_refresh_capability is not None:
        raise LedgerError("target-refresh-cas candidate has no target coordinate drift")
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


def _target_refresh_worktree_provenance(
    document: Mapping[str, Any], slot: Mapping[str, Any]
) -> tuple[
    dict[str, Any],
    frozenset[str],
    Mapping[str, Any] | None,
    Mapping[str, Any] | None,
]:
    """Resolve exact primitive/scope provenance for one bound target worktree."""

    request = slot.get("primitive_request")
    scope_record = None
    scope_proof = None
    allowed: frozenset[str] = frozenset()
    if request is None:
        scopes = [
            resource
            for resource in document["resources"]
            if resource["slot_id"] == slot["producer_resource_slot"]
        ]
        if len(scopes) != 1 or scopes[0]["kind"] != "scope":
            raise LedgerError("target refresh lacks one exact scope producer")
        scope = scopes[0]
        current = scope["current"]
        if (
            scope["state"] != "created"
            or current is None
            or current["lifecycle"] != "active"
        ):
            raise LedgerError("target refresh scope producer is not active")
        if scope["immutable"]["repository"] != slot["immutable"]["repository"]:
            raise LedgerError("target refresh scope repository differs from its target")
        retained = _retained_result(
            current["binding"], "target refresh scope binding"
        )
        scope_record, scope_row = _scope_show_record(
            retained,
            scope["request"],
            scope["immutable"]["repository"],
            "target refresh scope binding",
        )
        _scope_binding_observation(
            current["observation"],
            scope["request"],
            scope["immutable"]["repository"],
            scope_row,
            byte_digest(retained),
            scope_record,
            "target refresh scope observation",
            live=False,
        )
        request = _scope_worktree_request(
            scope["request"], scope["immutable"]["repository"]
        )
        if slot["current"]["path"] != scope_row["path"]:
            raise LedgerError("target refresh scope worktree differs from its target")
        allowed = _scope_owned_references(scope["request"])
        scope_proof = {
            "request": scope["request"],
            "repository": scope["immutable"]["repository"],
            "row": scope_row,
            "binding_digest": byte_digest(retained),
            "record": scope_record,
            "observation": current["observation"],
        }
    if request is None:
        raise LedgerError("target refresh worktree lacks live wrapper provenance")
    return (
        _decode(canonical_bytes(request), "target refresh worktree request"),
        allowed,
        scope_record,
        scope_proof,
    )


@contextmanager
def _target_refresh_live_safety(
    document: Mapping[str, Any], change: Mapping[str, Any]
) -> Iterator[Callable[[], None]]:
    """Pin and prove one exact target coordinate refresh through CAS install."""

    before = change["before"]
    after = change["after"]
    slot_id = change["worktree_slot"]["slot_id"]
    slot = next(row for row in document["artifacts"] if row["slot_id"] == slot_id)
    request, allowed, scope_record, scope_proof = _target_refresh_worktree_provenance(
        document, slot
    )
    request["expected_head_sha"] = after["head"]["current_sha"]
    path = slot["current"]["path"]
    with _pinned_live_worktree(
        request,
        path,
        "target refresh",
        allowed_references=allowed,
        scope_record=scope_record,
    ) as guard:
        def prove() -> None:
            guard.prove()
            if scope_proof is not None:
                _scope_binding_observation(
                    scope_proof["observation"],
                    scope_proof["request"],
                    scope_proof["repository"],
                    scope_proof["row"],
                    scope_proof["binding_digest"],
                    scope_proof["record"],
                    "target refresh scope observation",
                    live=True,
                    guard=guard,
                    live_request=request,
                )
            worktree = guard.descriptors["worktree"]
            for field in ("base", "head"):
                current = after[field]["current_sha"]
                _, ref_raw = _git(
                    worktree,
                    (
                        "rev-parse",
                        "--verify",
                        f"refs/heads/{after[field]['branch']}^{{commit}}",
                    ),
                    f"target refresh {field} branch",
                )
                if _one_git_line(ref_raw, f"target refresh {field} branch") != current:
                    raise LedgerError(
                        f"target refresh {field} differs from its exact branch ref"
                    )
                previous = before[field]["current_sha"]
                if previous != current:
                    status, _ = _git(
                        worktree,
                        ("merge-base", "--is-ancestor", previous, current),
                        f"target refresh {field} ancestry",
                        accepted={0, 1},
                    )
                    if status != 0:
                        raise LedgerError(
                            f"target refresh {field} is not a descendant"
                        )
            _, merge_base_raw = _git(
                worktree,
                (
                    "merge-base",
                    after["base"]["current_sha"],
                    after["head"]["current_sha"],
                ),
                "target refresh merge base",
            )
            if _one_git_line(merge_base_raw, "target refresh merge base") != after[
                "merge_base"
            ]["current_sha"]:
                raise LedgerError("target refresh merge base differs from live Git")
            guard.prove()

        prove()
        yield prove


def target_refresh_cas(
    root: Path | str,
    name: str,
    document: Mapping[str, Any],
    *,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
    failpoint: Failpoint = None,
) -> Snapshot:
    """Live-prove and atomically install one target-only coordinate refresh."""

    name = _direct_name(name)
    prepared = prepare(document)
    raw = canonical_bytes(prepared)
    current = inspect(root, name)
    expected = (
        expected_generation,
        expected_digest,
        expected_device,
        expected_inode,
    )
    candidate_digest = byte_digest(raw)
    legacy_names = {
        f".{name}.update-g{prepared['generation']}-from-{expected_digest}-"
        f"to-{candidate_digest}.tmp",
        f".{name}.update-proof-g{expected_generation}-from-{expected_digest}-"
        f"d{expected_device}-i{expected_inode}-to-{candidate_digest}.tmp",
    }
    legacy_pending = any(
        item.target == name and item.staging in legacy_names
        for item in inventory(root).pending
    )
    if current.raw == raw and prepared["generation"] == expected_generation + 1:
        if legacy_pending:
            raise LedgerError(
                "installed legacy target update lacks exact predecessor evidence; "
                "explicit recovery is required"
            )
        capability = _TargetRefreshCapability(
            _TARGET_REFRESH_TOKEN,
            name,
            None,
            raw,
            *expected,
        )
        return cas(
            root,
            name,
            prepared,
            expected_generation=expected_generation,
            expected_digest=expected_digest,
            expected_device=expected_device,
            expected_inode=expected_inode,
            failpoint=failpoint,
            _target_refresh_capability=capability,
            _target_refresh_legacy=legacy_pending,
        )
    if (
        current.document["generation"],
        current.digest,
        current.device,
        current.inode,
    ) != expected:
        raise LedgerError("stale target-refresh generation, digest, or inode")
    change = _target_refresh_change(current.document, prepared, current.digest)
    capability = _TargetRefreshCapability(
        _TARGET_REFRESH_TOKEN,
        name,
        current.raw,
        raw,
        *expected,
    )
    with _target_refresh_live_safety(prepared, change) as recheck:
        return cas(
            root,
            name,
            prepared,
            expected_generation=expected_generation,
            expected_digest=expected_digest,
            expected_device=expected_device,
            expected_inode=expected_inode,
            failpoint=failpoint,
            _precommit=recheck,
            _target_refresh_capability=capability,
            _target_refresh_legacy=legacy_pending,
        )


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
    _target_refresh_capability: _TargetRefreshCapability | None = None,
    _target_refresh_legacy: bool = False,
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
    if _target_refresh_capability is not None:
        capability = _target_refresh_capability
        if (
            _binding_capability is not None
            or not isinstance(capability, _TargetRefreshCapability)
            or capability.token is not _TARGET_REFRESH_TOKEN
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
            raise LedgerError("invalid internal target-refresh capability")
        if not isinstance(_target_refresh_legacy, bool):
            raise LedgerError("invalid internal target-refresh recovery mode")
        operation = "" if _target_refresh_legacy else "-refresh-target"
    elif _target_refresh_legacy:
        raise LedgerError("target-refresh recovery mode lacks its capability")
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
        if any(row.ledger_name == name for row in initial_inventory.releases):
            raise LedgerError("terminally released ledger cannot be updated")
        _check_candidate_inventory(
            initial_inventory,
            prepared,
            raw,
            allowed_pending=allowed_pending,
        )
        with _ledger_lock(directory, name):
            current_inventory = _inventory_locked(directory)
            if any(row.ledger_name == name for row in current_inventory.releases):
                raise LedgerError("terminally released ledger cannot be updated")
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
            if _target_refresh_capability is not None:
                if _target_refresh_capability.before_raw is None:
                    raise LedgerError(
                        "target-refresh recovery candidate is not already installed"
                    )
                if _target_refresh_capability.before_raw != current.raw:
                    raise LedgerError("target-refresh predecessor bytes changed")
            _transition(
                current.document,
                prepared,
                current.digest,
                _binding_capability=_binding_capability,
                _target_refresh_capability=_target_refresh_capability,
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


def correct_target_head(
    root: Path | str,
    name: str,
    predecessor_raw: bytes,
    recovery_raw: bytes,
    *,
    expected_generation: int,
    expected_digest: str,
    expected_device: int,
    expected_inode: int,
    bad_head: str,
    actual_head: str,
    actual_merge_base: str | None = None,
    failpoint: Failpoint = None,
) -> Snapshot:
    """Correct one exact nonexistent target-head typo without erasing its audit trail."""

    name = _direct_name(name)
    _integer(expected_generation, "expected_generation", minimum=2)
    _string(expected_digest, "expected_digest", SHA256_RE)
    _integer(expected_device, "expected_device", minimum=0)
    _integer(expected_inode, "expected_inode")
    _string(bad_head, "bad_head", COMMIT_RE)
    _string(actual_head, "actual_head", COMMIT_RE)
    if actual_merge_base is not None:
        _string(actual_merge_base, "actual_merge_base", COMMIT_RE)
    predecessor = validate(_decode(predecessor_raw, "head-correction predecessor"))
    if predecessor_raw != canonical_bytes(predecessor) or canonical_name(predecessor) != name:
        raise LedgerError("head-correction predecessor bytes are not exact canonical input")
    predecessor_digest = byte_digest(predecessor_raw)
    recovery = _head_correction_recovery(
        _decode(recovery_raw, "head-correction recovery authority"),
        "head-correction recovery authority",
    )
    if recovery_raw != canonical_bytes(recovery):
        raise LedgerError("head-correction recovery authority is not canonical")
    if actual_merge_base is not None and recovery["grant"]["reference"] != (
        "recovery:issue-460-stale-merge-base-target-head"
    ):
        raise LedgerError("coordinate correction requires the issue-460 recovery grant")
    prefix = f".{name}.correct-target-head-{expected_digest}"
    predecessor_name = f"{prefix}.predecessor.snapshot"
    erroneous_name = f"{prefix}.erroneous.snapshot"
    stage = f"{prefix}.tmp"
    receipt_name = f"{prefix}.json"
    allowed_pending = {
        ("correct-target-head-predecessor", name, predecessor_name),
        ("correct-target-head-erroneous", name, erroneous_name),
        ("correct-target-head-stage", name, stage),
        ("correct-target-head-receipt", name, receipt_name),
    }

    snapshot = inspect(root, name)
    if snapshot.digest == expected_digest:
        erroneous_raw = snapshot.raw
    else:
        with _locked_root(Path(root)) as directory:
            erroneous_raw, _ = _read_regular(directory, erroneous_name, managed=True)
    erroneous = validate(_decode(erroneous_raw, "head-correction erroneous bytes"))
    if byte_digest(erroneous_raw) != expected_digest:
        raise LedgerError("head-correction erroneous snapshot differs from expected digest")
    corrected, metadata, live_provenance = _head_correction_document(
        predecessor,
        erroneous,
        expected_digest,
        bad_head=bad_head,
        actual_head=actual_head,
        actual_merge_base=actual_merge_base,
    )
    live_request = live_provenance["request"]
    allowed_references = live_provenance["allowed_references"]
    scope_record = live_provenance["scope_record"]
    scope_proof = live_provenance["scope_proof"]
    corrected_raw = canonical_bytes(corrected)
    corrected_digest = byte_digest(corrected_raw)
    source_identity = {
        "generation": expected_generation,
        "sha256": expected_digest,
        "device": expected_device,
        "inode": expected_inode,
    }
    expected_intent = _head_correction_intent(
        erroneous,
        name,
        source_identity,
        predecessor_digest,
        metadata,
        bad_head=bad_head,
        actual_head=actual_head,
        actual_merge_base=actual_merge_base,
    )
    _require_head_correction_recovery(
        recovery,
        erroneous,
        expected_intent,
        "head-correction recovery authority",
    )

    live_context = (
        nullcontext(None)
        if snapshot.digest == corrected_digest
        else _pinned_live_worktree(
            live_request,
            metadata["worktree"],
            "target-head correction",
            allowed_references=allowed_references,
            scope_record=scope_record,
        )
    )
    with live_context as guard:
        def prove_live() -> None:
            if guard is None:
                raise LedgerError("completed coordinate correction has no live guard")
            guard.prove()
            if scope_proof is not None:
                _scope_binding_observation(
                    scope_proof["observation"],
                    scope_proof["request"],
                    scope_proof["repository"],
                    scope_proof["row"],
                    scope_proof["binding_digest"],
                    scope_proof["record"],
                    "target-head correction scope observation",
                    live=True,
                    guard=guard,
                    live_request=live_request,
                )
            worktree = guard.descriptors["worktree"]
            _git(
                worktree,
                ("cat-file", "-e", f"{actual_head}^{{commit}}"),
                "actual correction head commit",
            )
            _, bad_raw = _git(
                worktree,
                ("cat-file", "--batch-check"),
                "bad correction head absence",
                input_bytes=f"{bad_head}\n".encode("ascii"),
            )
            if _one_git_line(bad_raw, "bad correction head absence") != f"{bad_head} missing":
                raise LedgerError("bad correction object is not canonically absent")
            ancestor_status, _ = _git(
                worktree,
                (
                    "merge-base",
                    "--is-ancestor",
                    metadata["predecessor_head"],
                    actual_head,
                ),
                "correction predecessor ancestry",
                accepted={0, 1},
            )
            if ancestor_status != 0:
                raise LedgerError("correction predecessor is not an ancestor of actual head")
            if actual_merge_base is not None:
                _, predecessor_merge_base_raw = _git(
                    worktree,
                    (
                        "merge-base",
                        metadata["base_head"],
                        metadata["predecessor_head"],
                    ),
                    "correction predecessor merge base",
                )
                predecessor_merge_base = _one_git_line(
                    predecessor_merge_base_raw,
                    "correction predecessor merge base",
                )
                if predecessor_merge_base not in {
                    metadata["merge_base"],
                    actual_merge_base,
                }:
                    raise LedgerError(
                        "correction predecessor merge base matches neither authorized coordinate"
                    )
                stale_status, _ = _git(
                    worktree,
                    (
                        "merge-base",
                        "--is-ancestor",
                        metadata["merge_base"],
                        actual_merge_base,
                    ),
                    "correction stale merge-base ancestry",
                    accepted={0, 1},
                )
                if stale_status != 0:
                    raise LedgerError(
                        "recorded correction merge base is not an ancestor of the live merge base"
                    )
            _, merge_base_raw = _git(
                worktree,
                ("merge-base", metadata["base_head"], actual_head),
                "correction merge base",
            )
            expected_merge_base = (
                actual_merge_base
                if actual_merge_base is not None
                else metadata["merge_base"]
            )
            if (
                _one_git_line(merge_base_raw, "correction merge base")
                != expected_merge_base
            ):
                raise LedgerError("corrected target merge base does not match live Git")
            guard.prove()

        if guard is not None:
            prove_live()
        with _locked_root(Path(root)) as directory:
            _require_names_fit(
                directory,
                (name, predecessor_name, erroneous_name, stage, receipt_name, f".{name}.lock"),
            )
            current_inventory = _inventory_locked(directory)
            _require_exact_pending(current_inventory, allowed_pending)
            with _ledger_lock(directory, name):
                current_inventory = _inventory_locked(directory)
                _require_exact_pending(current_inventory, allowed_pending)
                current = _snapshot(directory, name)
                if current.digest == corrected_digest:
                    if _exists(directory, stage):
                        raise LedgerError("completed head correction retained its stage")
                    receipt_raw, _ = _read_regular(
                        directory, receipt_name, managed=True
                    )
                    receipt = _head_correction_receipt(
                        _decode(receipt_raw, receipt_name), receipt_name
                    )
                    if receipt["source"] != source_identity or (
                        receipt["bad_head"], receipt["actual_head"]
                    ) != (bad_head, actual_head) or receipt["predecessor_snapshot"][
                        "sha256"
                    ] != predecessor_digest or receipt["recovery"] != recovery:
                        raise LedgerError("completed head correction has different authority")
                    _fsync(directory, f"review root while resuming correction of {name}")
                    return current
                if (
                    current.document["generation"] != expected_generation
                    or current.digest != expected_digest
                    or current.device != expected_device
                    or current.inode != expected_inode
                    or current.raw != erroneous_raw
                ):
                    raise LedgerError("stale head-correction generation, digest, or inode")
                predecessor_status = _ensure_stage(
                    directory, predecessor_name, predecessor_raw, allow_prefix_resume=True
                )
                _hit(failpoint, "correct-target-head:predecessor-snapshot")
                erroneous_status = _ensure_head_correction_source_link(
                    directory,
                    name,
                    erroneous_name,
                    erroneous_raw,
                    expected_device,
                    expected_inode,
                )
                _hit(failpoint, "correct-target-head:erroneous-snapshot")
                stage_status = _ensure_stage(
                    directory, stage, corrected_raw, allow_prefix_resume=True
                )
                _hit(failpoint, "correct-target-head:staged")
                receipt = {
                    "transaction": (
                        "delivery-ledger-correct-target-coordinates-v1"
                        if actual_merge_base is not None
                        else "delivery-ledger-correct-target-head-v1"
                    ),
                    "target": name,
                    "source": source_identity,
                    "predecessor_snapshot": {
                        "name": predecessor_name,
                        "sha256": predecessor_digest,
                        "device": predecessor_status.st_dev,
                        "inode": predecessor_status.st_ino,
                    },
                    "erroneous_snapshot": {
                        "name": erroneous_name,
                        "sha256": expected_digest,
                        "device": erroneous_status.st_dev,
                        "inode": erroneous_status.st_ino,
                    },
                    "correction": {
                        "generation": corrected["generation"],
                        "sha256": corrected_digest,
                    },
                    **metadata,
                    "actual_head": actual_head,
                    "bad_head": bad_head,
                    "recovery": copy.deepcopy(recovery),
                    "staging": stage,
                }
                receipt_raw = canonical_bytes(receipt)
                _head_correction_receipt(receipt, receipt_name)
                _ensure_stage(directory, receipt_name, receipt_raw, allow_prefix_resume=True)
                _hit(failpoint, "correct-target-head:receipt")
                rechecked = _snapshot(directory, name)
                if (
                    rechecked.digest,
                    rechecked.device,
                    rechecked.inode,
                ) != (expected_digest, expected_device, expected_inode):
                    raise LedgerError("head-correction source changed before replacement")
                staged_raw, staged_visible = _read_regular(
                    directory, stage, managed=True, sync=True
                )
                if staged_raw != corrected_raw or (
                    staged_visible.st_dev,
                    staged_visible.st_ino,
                ) != (stage_status.st_dev, stage_status.st_ino):
                    raise LedgerError("head-correction stage changed before replacement")
                prove_live()
                os.replace(stage, name, src_dir_fd=directory, dst_dir_fd=directory)
                _hit(failpoint, "correct-target-head:renamed")
                installed = _snapshot(directory, name)
                if installed.raw != corrected_raw:
                    raise LedgerError("head-correction installed bytes differ")
                _fsync(directory, f"review root after correcting {name}")
                _hit(failpoint, "correct-target-head:installed")
                final_inventory = _inventory_locked(directory)
                _require_exact_pending(final_inventory, set())
                return _snapshot(directory, name)


def _migration_marker(
    *,
    state: str,
    kind: str,
    candidate_digest: str,
    source: Mapping[str, Any],
    related_sources: Sequence[Mapping[str, Any]] | None,
    historical_heads: Sequence[Mapping[str, Any]] | None,
    snapshot_name: str,
    snapshot: Mapping[str, Any] | None,
    canonical_report: str,
    destination: str,
    destination_digest: str | None,
) -> dict[str, Any]:
    marker = {
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
    if related_sources is not None:
        marker["related_sources"] = [dict(source) for source in related_sources]
    if historical_heads is not None:
        marker["historical_heads"] = [dict(head) for head in historical_heads]
    return marker


def migrate(
    root: Path | str,
    source_name: str,
    document: Mapping[str, Any],
    *,
    kind: str,
    expected_source_digest: str | None = None,
    related_sources: Mapping[str, str] | None = None,
    expected_historical_heads: Mapping[tuple[str, str, str], str] | None = None,
    failpoint: Failpoint = None,
) -> Snapshot:
    if kind not in MIGRATION_KINDS:
        raise LedgerError(
            "migration kind must be legacy, legacy-rebind, or pre-schema"
        )
    if expected_source_digest is None:
        raise LedgerError("migration requires an exact expected source digest")
    _string(expected_source_digest, "expected_source_digest", SHA256_RE)
    source_name = _direct_name(source_name, "migration source")
    requested_related = {} if related_sources is None else dict(related_sources)
    requested_heads = (
        {} if expected_historical_heads is None else dict(expected_historical_heads)
    )
    for related_name, related_digest in requested_related.items():
        _direct_name(related_name, "migration related source")
        _string(related_digest, "migration related source digest", SHA256_RE)
    if len({name.casefold() for name in requested_related}) != len(requested_related):
        raise LedgerError("migration related sources contain a case alias")
    normalized_heads: dict[tuple[str, str, str], str] = {}
    for coordinate, head_sha in requested_heads.items():
        if not isinstance(coordinate, tuple) or len(coordinate) != 3:
            raise LedgerError("historical head coordinate is invalid")
        owner, repository_name, branch = coordinate
        normalized = (
            _string(owner.casefold(), "historical head owner", OWNER_RE),
            _string(
                repository_name.casefold(),
                "historical head repository",
                REPOSITORY_RE,
            ),
            _branch(branch, "historical head branch").casefold(),
        )
        sha = _string(head_sha.casefold(), "historical head SHA", COMMIT_RE)
        if normalized in normalized_heads:
            raise LedgerError("historical head coordinates contain a duplicate")
        normalized_heads[normalized] = sha
    candidate = prepare(document)
    _require_migration_genesis(candidate, kind)
    if candidate["authority"]["kind"] not in {"durable-goal", "explicit-recovery"}:
        raise LedgerError(
            "migration requires durable-goal or explicit-recovery authority"
        )
    if (
        kind == "legacy-rebind"
        and candidate["authority"]["kind"] != "explicit-recovery"
    ):
        raise LedgerError("legacy rebind requires explicit-recovery authority")
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
    if kind == "legacy-rebind":
        _require_legacy_rebind_source(source_name, candidate)
        expected_related = _legacy_source_name(candidate)
        if set(requested_related) != {expected_related}:
            raise LedgerError(
                f"legacy rebind requires exact related source {expected_related}"
            )
        if not normalized_heads:
            raise LedgerError("legacy rebind requires exact historical heads")
    elif requested_related or normalized_heads:
        raise LedgerError("recovery evidence is only valid for legacy rebind")
    historical_head_rows = (
        [
            {
                "owner": owner,
                "repository": repository_name,
                "branch": branch,
                "sha": sha,
            }
            for (owner, repository_name, branch), sha in sorted(
                normalized_heads.items()
            )
        ]
        if kind == "legacy-rebind"
        else None
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
            if {
                source["name"]: source["sha256"]
                for source in preflight_marker.get("related_sources", [])
            } != requested_related:
                raise LedgerError("related source digests differ from planned migration")
            if preflight_marker.get("historical_heads") != historical_head_rows:
                raise LedgerError("historical heads differ from planned migration")
        preflight_raw, preflight_status = _read_regular(directory, source_name)
        preflight_related: list[tuple[str, bytes, os.stat_result]] = []
        for related_name in sorted(requested_related, key=str.casefold):
            related_raw, related_status = _read_regular(directory, related_name)
            if byte_digest(related_raw) != requested_related[related_name]:
                raise LedgerError("migration related source digest does not match expectation")
            preflight_related.append((related_name, related_raw, related_status))
        preflight_digest = byte_digest(preflight_raw)
        mutable_completed_pre_schema = (
            kind == "pre-schema"
            and preflight_marker is not None
            and preflight_marker["state"] == "complete"
            and preflight_marker["source"]["sha256"] == expected_source_digest
        )
        if preflight_digest != expected_source_digest and not mutable_completed_pre_schema:
            raise LedgerError("migration source digest does not match expectation")
        if kind in LEGACY_MIGRATION_KINDS:
            parsed_historical_heads = _require_exact_legacy_claim(
                source_name,
                preflight_raw,
                candidate,
                require_canonical_name=kind == "legacy",
                related_sources=[
                    (name, raw) for name, raw, _status in preflight_related
                ],
            )
            if kind == "legacy-rebind" and parsed_historical_heads != {
                (*coordinate, sha) for coordinate, sha in normalized_heads.items()
            }:
                raise LedgerError(
                    "legacy rebind historical heads do not match exact expectation"
                )
        if preflight_marker is None:
            related_identities = [
                {
                    "name": name,
                    "sha256": byte_digest(raw),
                    "device": status.st_dev,
                    "inode": status.st_ino,
                }
                for name, raw, status in preflight_related
            ]
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
                related_sources=(
                    related_identities if kind == "legacy-rebind" else None
                ),
                historical_heads=(
                    historical_head_rows if kind == "legacy-rebind" else None
                ),
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
            allowed_legacy_sources={source_name, *requested_related},
        )
        with _ledger_lock(directory, target):
            current_inventory = _inventory_locked(directory)
            _check_candidate_inventory(
                current_inventory,
                candidate,
                candidate_raw,
                allowed_pending=allowed_pending,
                allowed_legacy_sources={source_name, *requested_related},
            )
            source_raw, source_status = _read_regular(directory, source_name)
            source_digest = byte_digest(source_raw)
            if (
                source_raw != preflight_raw
                or (source_status.st_dev, source_status.st_ino)
                != (preflight_status.st_dev, preflight_status.st_ino)
            ):
                raise LedgerError("migration source changed after preflight")
            for related_name, related_raw, related_status in preflight_related:
                current_raw, current_status = _read_regular(directory, related_name)
                if current_raw != related_raw or (
                    current_status.st_dev,
                    current_status.st_ino,
                ) != (related_status.st_dev, related_status.st_ino):
                    raise LedgerError("migration related source changed after preflight")
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
                if {
                    source["name"]: source["sha256"]
                    for source in marker.get("related_sources", [])
                } != requested_related:
                    raise LedgerError("related source digests differ from planned migration")
                if marker.get("historical_heads") != historical_head_rows:
                    raise LedgerError("historical heads differ from planned migration")
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
            if kind in LEGACY_MIGRATION_KINDS or marker["state"] != "complete":
                if (
                    source_digest != source["sha256"]
                    or source_status.st_dev != source["device"]
                    or source_status.st_ino != source["inode"]
                ):
                    raise LedgerError("migration source changed before safe completion")
            for related in marker.get("related_sources", []):
                related_raw, related_status = _read_regular(directory, related["name"])
                if (
                    byte_digest(related_raw) != related["sha256"]
                    or related_status.st_dev != related["device"]
                    or related_status.st_ino != related["inode"]
                ):
                    raise LedgerError("migration related source changed before completion")
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
                if kind in LEGACY_MIGRATION_KINDS:
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
                if kind == "legacy-rebind":
                    migration["related_sources"] = marker["related_sources"]
                    migration["historical_heads"] = marker["historical_heads"]
                prepared = prepare({**candidate, "migration": migration})
                raw = canonical_bytes(prepared)
                destination_digest = byte_digest(raw)
                prepared_marker = _migration_marker(
                    state="prepared",
                    kind=kind,
                    candidate_digest=candidate_digest,
                    source=source,
                    related_sources=marker.get("related_sources"),
                    historical_heads=marker.get("historical_heads"),
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
                    kind in LEGACY_MIGRATION_KINDS
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
                if kind == "legacy-rebind":
                    migration["related_sources"] = marker["related_sources"]
                    migration["historical_heads"] = marker["historical_heads"]
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
    release_preview_parser = commands.add_parser(
        "release-preview", help="preview one explicit post-merge terminal release"
    )
    release_preview_parser.add_argument("root", help="initialized review root")
    release_preview_parser.add_argument("name", help="direct canonical ledger filename")
    release_preview_parser.add_argument("input", help="bounded no-follow release evidence JSON")
    release_apply_parser = commands.add_parser(
        "release-apply", help="apply one hash-bound terminal release"
    )
    release_apply_parser.add_argument("root", help="initialized review root")
    release_apply_parser.add_argument("name", help="direct canonical ledger filename")
    release_apply_parser.add_argument("input", help="bounded no-follow release evidence JSON")
    release_apply_parser.add_argument("--plan", required=True, help="exact preview digest")
    archive_preview_parser = commands.add_parser(
        "archive-preview", help="preview bundling one released ledger after cleanup"
    )
    archive_preview_parser.add_argument("root", help="initialized review root")
    archive_preview_parser.add_argument("name", help="direct canonical ledger filename")
    archive_preview_parser.add_argument("input", help="bounded no-follow archive evidence JSON")
    archive_apply_parser = commands.add_parser(
        "archive-apply", help="apply one hash-bound evidence archive"
    )
    archive_apply_parser.add_argument("root", help="initialized review root")
    archive_apply_parser.add_argument("name", help="direct canonical ledger filename")
    archive_apply_parser.add_argument("input", help="bounded no-follow archive evidence JSON")
    archive_apply_parser.add_argument("--plan", required=True, help="exact preview digest")
    reclaim_preview_parser = commands.add_parser(
        "reclaim-preview", help="preview retention-gated archive reclamation"
    )
    reclaim_preview_parser.add_argument("root", help="initialized review root")
    reclaim_preview_parser.add_argument("name", help="direct canonical archive filename")
    reclaim_apply_parser = commands.add_parser(
        "reclaim-apply", help="apply one exact reclaim preview"
    )
    reclaim_apply_parser.add_argument("root", help="initialized review root")
    reclaim_apply_parser.add_argument("input", help="bounded no-follow reclaim preview JSON")
    reclaim_apply_parser.add_argument("--plan", required=True, help="exact preview digest")
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
    refresh_parser = commands.add_parser(
        "target-refresh-cas",
        help="live-prove one target-only coordinate refresh and apply it by CAS",
    )
    refresh_parser.add_argument("root", help="initialized review root")
    refresh_parser.add_argument("name", help="direct canonical ledger filename")
    refresh_parser.add_argument("input", help="bounded no-follow replacement JSON file")
    refresh_parser.add_argument("--expected-generation", required=True, type=int)
    refresh_parser.add_argument("--expected-digest", required=True)
    refresh_parser.add_argument("--expected-device", required=True, type=int)
    refresh_parser.add_argument("--expected-inode", required=True, type=int)
    correction_parser = commands.add_parser(
        "correct-target-head",
        help="live-prove and supersede one exact nonexistent target-head typo",
    )
    correction_parser.add_argument("root", help="initialized review root")
    correction_parser.add_argument("name", help="direct canonical ledger filename")
    correction_parser.add_argument("predecessor", help="exact canonical predecessor JSON")
    correction_parser.add_argument(
        "recovery", help="exact canonical explicit-recovery grant and intent JSON"
    )
    correction_parser.add_argument("--expected-generation", required=True, type=int)
    correction_parser.add_argument("--expected-digest", required=True)
    correction_parser.add_argument("--expected-device", required=True, type=int)
    correction_parser.add_argument("--expected-inode", required=True, type=int)
    correction_parser.add_argument("--bad-head", required=True)
    correction_parser.add_argument("--actual-head", required=True)
    correction_parser.add_argument(
        "--actual-merge-base",
        help="explicit live merge base for an exact stale-coordinate recovery",
    )
    migration_parser = commands.add_parser(
        "migrate", help="recover one exact legacy or pre-schema report"
    )
    migration_parser.add_argument("root", help="initialized review root")
    migration_parser.add_argument("source", help="direct report filename")
    migration_parser.add_argument("input", help="generation-1 migration-null JSON file")
    migration_parser.add_argument("--kind", choices=sorted(MIGRATION_KINDS), required=True)
    migration_parser.add_argument("--expected-source-digest", required=True)
    migration_parser.add_argument(
        "--related-source",
        action="append",
        default=[],
        metavar="NAME=SHA256",
        help="exact related legacy source and digest (legacy-rebind only)",
    )
    migration_parser.add_argument(
        "--historical-head",
        action="append",
        default=[],
        metavar="OWNER/REPOSITORY@BRANCH=SHA",
        help="exact parsed historical head (legacy-rebind only)",
    )
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
        elif arguments.command == "release-preview":
            _print(
                release_preview(
                    arguments.root, arguments.name, _read_input(arguments.input)
                )
            )
        elif arguments.command == "release-apply":
            _print(
                release_apply(
                    arguments.root,
                    arguments.name,
                    _read_input(arguments.input),
                    plan_sha256=arguments.plan,
                ).json()
            )
        elif arguments.command == "archive-preview":
            _print(
                archive_preview(
                    arguments.root, arguments.name, _read_input(arguments.input)
                )
            )
        elif arguments.command == "archive-apply":
            _print(
                archive_apply(
                    arguments.root,
                    arguments.name,
                    _read_input(arguments.input),
                    plan_sha256=arguments.plan,
                ).json()
            )
        elif arguments.command == "reclaim-preview":
            _print(reclaim_preview(arguments.root, arguments.name))
        elif arguments.command == "reclaim-apply":
            _print(
                reclaim_apply(
                    arguments.root,
                    _read_input(arguments.input),
                    plan_sha256=arguments.plan,
                )
            )
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
        elif arguments.command == "target-refresh-cas":
            _print(
                target_refresh_cas(
                    arguments.root,
                    arguments.name,
                    _read_input(arguments.input),
                    expected_generation=arguments.expected_generation,
                    expected_digest=arguments.expected_digest,
                    expected_device=arguments.expected_device,
                    expected_inode=arguments.expected_inode,
                ).json()
            )
        elif arguments.command == "correct-target-head":
            _print(
                correct_target_head(
                    arguments.root,
                    arguments.name,
                    _read_bytes_input(arguments.predecessor),
                    _read_bytes_input(arguments.recovery),
                    expected_generation=arguments.expected_generation,
                    expected_digest=arguments.expected_digest,
                    expected_device=arguments.expected_device,
                    expected_inode=arguments.expected_inode,
                    bad_head=arguments.bad_head,
                    actual_head=arguments.actual_head,
                    actual_merge_base=arguments.actual_merge_base,
                ).json()
            )
        elif arguments.command == "migrate":
            related_sources: dict[str, str] = {}
            for value in arguments.related_source:
                name, separator, digest = value.partition("=")
                if not separator or name in related_sources:
                    raise LedgerError(
                        "related source must be one unique NAME=SHA256 value"
                    )
                related_sources[name] = digest
            historical_heads: dict[tuple[str, str, str], str] = {}
            for value in arguments.historical_head:
                coordinate, equals, sha = value.rpartition("=")
                repository_text, at, branch = coordinate.partition("@")
                owner, slash, repository_name = repository_text.partition("/")
                key = (owner, repository_name, branch)
                if not equals or not at or not slash or key in historical_heads:
                    raise LedgerError(
                        "historical head must be one unique "
                        "OWNER/REPOSITORY@BRANCH=SHA value"
                    )
                historical_heads[key] = sha
            _print(
                migrate(
                    arguments.root,
                    arguments.source,
                    _read_input(arguments.input),
                    kind=arguments.kind,
                    expected_source_digest=arguments.expected_source_digest,
                    related_sources=related_sources,
                    expected_historical_heads=historical_heads,
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
