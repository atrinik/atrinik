from __future__ import annotations

import base64
import copy
import ctypes
from dataclasses import dataclass
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile
from typing import Any, Iterable

from .model import WorkspaceError, atomic_json, load_json
from .supervisor import process_matches


PLAN_SCHEMA_VERSION = 2
PROFILE_SCHEMA_VERSION = 3
MIGRATION_NAME = "repositories"
MIGRATION_RECORD = "migrations/repositories.json"
MIGRATION_PENDING = "migrations/repositories.pending.json"

# The two source names are deliberately retained.  A workspace may still use
# the pre-rename canonical paths or may already have completed the earlier
# legacy-* relocation.  Both layouts describe the same classic source line.
CLASSIC_SOURCES = (
    ("client", "legacy-client", "classic-client", "client"),
    ("server", "legacy-server", "classic-server", "server"),
    ("editor", "legacy-editor", "classic-editor", "editor"),
    (
        "libatrinik",
        "legacy-libatrinik",
        "classic-libatrinik",
        "libatrinik",
    ),
    ("protocol", "legacy-protocol", "classic-protocol", "protocol"),
)
CLASSIC_HISTORY_ANCHORS = {
    "client": "5be79077ae3b261a98a168e33ffdaa7467ae0e9f",
    "server": "ab28c88fdc2746e7ad4b3168e91eb264a3e52228",
    "editor": "bb0047a06cc07466d2db22a10abc9291d9b491db",
    "libatrinik": "ff5c5054936c159bf2b227502581ca7692ad5d43",
    "protocol": "e2bdedb88381e4d036cefb7ad1e62bf2e39072e0",
}
PROFILE_IDENTITIES = {
    key: logical
    for canonical, legacy, logical, _ in CLASSIC_SOURCES
    for key in (canonical, legacy)
}
PROFILE_IDENTITIES["content"] = "content-1x"
SELECTOR_KINDS = {"primary", "worktree", "path"}
MIGRATED_CONTENT_WORKTREE_KIND = "migrated-worktree"
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
OPERATION_PATHS = (
    "BISECT_LOG",
    "CHERRY_PICK_HEAD",
    "MERGE_HEAD",
    "REVERT_HEAD",
    "rebase-apply",
    "rebase-merge",
    "sequencer",
)


def rename_no_replace(source: Path, destination: Path) -> None:
    """Atomically move *source* without replacing a raced-in destination."""

    try:
        renameat2 = ctypes.CDLL(None, use_errno=True).renameat2
    except AttributeError as error:
        raise WorkspaceError(
            "atomic no-replace path moves are unsupported on this platform"
        ) from error
    renameat2.argtypes = [
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_uint,
    ]
    renameat2.restype = ctypes.c_int
    if (
        renameat2(
            -100,
            os.fsencode(source),
            -100,
            os.fsencode(destination),
            1,
        )
        == 0
    ):
        return
    error_number = ctypes.get_errno()
    if error_number == errno.EEXIST:
        raise WorkspaceError(
            f"destination appeared before atomic install: {destination}"
        )
    if error_number in {errno.ENOSYS, errno.EINVAL}:
        raise WorkspaceError(
            "atomic no-replace path moves are unsupported on this platform"
        )
    raise WorkspaceError(
        f"cannot move path without replacement: {source} -> {destination}: "
        f"{os.strerror(error_number)}"
    )


def exchange_paths(first: Path, second: Path) -> None:
    """Atomically exchange two paths for compare-and-swap profile updates."""

    try:
        renameat2 = ctypes.CDLL(None, use_errno=True).renameat2
    except AttributeError as error:
        raise WorkspaceError("atomic path exchange is unsupported") from error
    renameat2.argtypes = [
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_uint,
    ]
    renameat2.restype = ctypes.c_int
    if renameat2(-100, os.fsencode(first), -100, os.fsencode(second), 2) == 0:
        return
    error_number = ctypes.get_errno()
    if error_number in {errno.ENOSYS, errno.EINVAL}:
        raise WorkspaceError("atomic path exchange is unsupported")
    raise WorkspaceError(
        f"cannot exchange paths atomically: {first} <-> {second}: "
        f"{os.strerror(error_number)}"
    )


def classic_lineage(path: Path, canonical: str) -> bool:
    """Prove that a complete repository contains the registered classic root."""

    anchor = CLASSIC_HISTORY_ANCHORS.get(canonical)
    if anchor is None:
        raise WorkspaceError(f"no classic history anchor is registered for {canonical}")
    environment = os.environ.copy()
    environment["GIT_OPTIONAL_LOCKS"] = "0"
    environment["GIT_NO_REPLACE_OBJECTS"] = "1"

    def inspect(*arguments: str) -> subprocess.CompletedProcess[bytes]:
        try:
            return subprocess.run(
                ["git", "-C", str(path), *arguments],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                env=environment,
            )
        except OSError as error:
            raise WorkspaceError(f"cannot inspect Git history: {error}") from error

    shallow = inspect("rev-parse", "--is-shallow-repository")
    if shallow.returncode or shallow.stdout.strip() != b"false":
        raise WorkspaceError(
            "classic repository history is shallow or cannot be proven complete"
        )
    partial = inspect("config", "--get-regexp", r"^remote\..*\.promisor$")
    if partial.returncode not in {0, 1}:
        raise WorkspaceError("cannot inspect partial-clone configuration")
    extension = inspect("config", "--get", "extensions.partialClone")
    if extension.returncode not in {0, 1}:
        raise WorkspaceError("cannot inspect partial-clone extension")
    if partial.returncode == 0 or extension.returncode == 0:
        raise WorkspaceError(
            "classic repository is a partial clone and history cannot be proven complete"
        )
    if inspect("fsck", "--connectivity-only", "--no-dangling").returncode:
        raise WorkspaceError(
            "repository history is incomplete or corrupt and cannot be proven"
        )
    if inspect("cat-file", "-e", f"{anchor}^{{commit}}").returncode:
        return False
    ancestor = inspect("merge-base", "--is-ancestor", anchor, "HEAD")
    if ancestor.returncode == 0:
        return True
    if ancestor.returncode == 1:
        return False
    raise WorkspaceError("cannot prove classic repository history ancestry")


@dataclass(frozen=True)
class _Worktree:
    canonical: str
    logical: str
    prefix: str
    source_name: str
    path: Path
    head: str
    branch: str | None
    locked: str | None
    status: bytes
    index_sha256: str
    snapshot_sha256: str
    module_sha256: str
    primary: bool
    old_label: str
    new_label: str | None
    destination: Path | None
    branch_ref: str | None
    mapped_parent: str | None

    def public(self, status: str = "planned") -> dict[str, Any]:
        destination_status = bytearray()
        for record in self.status.split(b"\0"):
            if not record:
                continue
            if len(record) < 4 or record[2:3] != b" ":
                raise WorkspaceError("unsupported porcelain status record")
            destination_status.extend(record[:3])
            destination_status.extend(os.fsencode(self.prefix))
            destination_status.extend(b"/")
            destination_status.extend(record[3:])
            destination_status.append(0)
        return {
            "branch": self.branch,
            "branch_ref": self.branch_ref,
            "component": self.logical,
            "destination": str(self.destination) if self.destination else None,
            "destination_status_sha256": (
                hashlib.sha256(bytes(destination_status)).hexdigest()
                if self.destination is not None
                else None
            ),
            "dirty": bool(self.status),
            "head": self.head,
            "index_sha256": self.index_sha256,
            "label": self.new_label,
            "locked": self.locked,
            "mapped_parent": self.mapped_parent,
            "module_sha256": self.module_sha256,
            "path": str(self.path),
            "primary": self.primary,
            "source_component": self.canonical,
            "status": status,
            "status_bytes": len(self.status),
            "status_sha256": hashlib.sha256(self.status).hexdigest(),
            "snapshot_sha256": self.snapshot_sha256,
        }


@dataclass(frozen=True)
class _CompositeWorktree:
    profile: str
    label: str
    destination: Path
    branch_ref: str
    worktrees: tuple[_Worktree, ...]

    def public(self, status: str = "planned") -> dict[str, Any]:
        expected_status = self.expected_status()
        return {
            "branch_ref": self.branch_ref,
            "components": [worktree.logical for worktree in self.worktrees],
            "destination": str(self.destination),
            "dirty": bool(expected_status),
            "label": self.label,
            "module_states": [
                {
                    "component": worktree.logical,
                    "prefix": worktree.prefix,
                    "sha256": worktree.module_sha256,
                }
                for worktree in self.worktrees
            ],
            "profile": self.profile,
            "status": status,
            "status_bytes": len(expected_status),
            "status_sha256": hashlib.sha256(expected_status).hexdigest(),
        }

    def expected_status(self) -> bytes:
        records: list[bytes] = []
        for worktree in self.worktrees:
            for record in worktree.status.split(b"\0"):
                if not record:
                    continue
                if len(record) < 4 or record[2:3] != b" ":
                    raise WorkspaceError("unsupported porcelain status record")
                records.append(
                    record[:3]
                    + os.fsencode(worktree.prefix)
                    + b"/"
                    + record[3:]
                )
        records.sort(key=lambda record: (record[:2] == b"??", record[3:]))
        return b"".join(record + b"\0" for record in records)


@dataclass(frozen=True)
class _Source:
    canonical: str
    legacy: str
    logical: str
    prefix: str
    path: Path
    archive: Path
    repository: str
    remote: str
    remote_url: str
    head: str
    branch: str
    status: bytes
    git_config_sha256: str
    worktrees: tuple[_Worktree, ...]

    def public(self, status: str = "planned") -> dict[str, Any]:
        return {
            "archive": str(self.archive),
            "branch": self.branch,
            "component": self.logical,
            "dirty": bool(self.status),
            "from": str(self.path),
            "git_config_sha256": self.git_config_sha256,
            "head": self.head,
            "layout": self.path.name,
            "remote": self.remote,
            "remote_url": self.remote_url,
            "repository": self.repository,
            "status": status,
            "status_bytes": len(self.status),
            "status_sha256": hashlib.sha256(self.status).hexdigest(),
        }


@dataclass(frozen=True)
class _ProfileRewrite:
    name: str
    path: Path
    before: bytes
    after: bytes

    def public(self, status: str = "planned") -> dict[str, Any]:
        return {
            "after_sha256": hashlib.sha256(self.after).hexdigest(),
            "before_sha256": hashlib.sha256(self.before).hexdigest(),
            "name": self.name,
            "path": str(self.path),
            "status": status,
        }


@dataclass(frozen=True)
class _Classic:
    path: Path
    repository: str
    head: str
    branch: str


@dataclass
class _Inspection:
    plan: dict[str, Any]
    classic: _Classic | None
    sources: list[_Source]
    profiles: list[_ProfileRewrite]
    composites: list[_CompositeWorktree]


class RepositoryMigration:
    """Convert independent classic checkouts into one full classic checkout.

    Source repositories are never deleted.  Their primary directories are
    atomically moved below the workspace archive, and attached original
    worktrees remain in place with Git administrative pointers repaired.  A
    full monorepo worktree is created for every source worktree whose selected
    state differs from the imported classic primary.
    """

    def __init__(self, repository_root: Path, workspace_paths: Any, manifest: Any):
        self.repository_root = Path(repository_root).resolve()
        self.paths = workspace_paths
        self.manifest = manifest
        self.workspace = Path(workspace_paths.workspace).resolve()
        self.record_path = self.workspace / MIGRATION_RECORD
        self.pending_path = self.workspace / MIGRATION_PENDING
        self.archive_root = self.workspace / "archive" / "classic-migration"

    def execute(self, mode: str) -> dict[str, Any]:
        if mode not in {"dry-run", "apply", "audit"}:
            raise WorkspaceError(f"unsupported repository migration mode: {mode}")
        if mode == "audit":
            return self._audit()
        inspection = self._inspect()
        if mode == "dry-run" or inspection.plan["refusals"]:
            return inspection.plan
        if not self.workspace.is_dir() or self.workspace.is_symlink():
            return self._with_refusal(
                inspection.plan,
                "workspace_unavailable",
                f"workspace directory is unavailable: {self.workspace}",
                "initialize the wrapper workspace, then rerun the migration",
            )

        lock_path = self.workspace / "repository-layout.lock"
        flags = os.O_RDWR | os.O_CREAT | os.O_CLOEXEC
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        try:
            descriptor = os.open(lock_path, flags, 0o600)
        except OSError as error:
            raise WorkspaceError(f"cannot open repository migration lock: {error}") from error
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            os.close(descriptor)
            raise WorkspaceError(
                f"repository migration lock is not a regular file: {lock_path}"
            )
        with os.fdopen(descriptor, "a+") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return self._with_refusal(
                    inspection.plan,
                    "repository_layout_busy",
                    "the repository layout is in use by another wrapper operation",
                    "wait for the active wrapper operation to finish, then rerun",
                )
            if self.record_path.is_file() and not self.record_path.is_symlink():
                audited = self._audit()
                if audited["status"] == "complete":
                    audited["status"] = "already-applied"
                    return audited
                return audited
            if self.pending_path.exists() or self.pending_path.is_symlink():
                recovery_errors = self._rollback_pending()
                if recovery_errors:
                    return self._with_refusal(
                        inspection.plan,
                        "pending_migration",
                        "an interrupted repository migration could not be rolled back: "
                        + "; ".join(recovery_errors),
                        "preserve the pending journal and repair only its exact recorded paths",
                    )
            inspection = self._inspect()
            if inspection.plan["refusals"]:
                return inspection.plan
            return self._apply(inspection)

    # ------------------------------------------------------------------
    # Read-only planning

    def _inspect(self) -> _Inspection:
        refusals: list[dict[str, str]] = []
        classic, classic_row, classic_refusals = self._inspect_classic()
        refusals.extend(classic_refusals)
        sources: list[_Source] = []
        source_rows: list[dict[str, Any]] = []
        worktree_rows: list[dict[str, Any]] = []

        for canonical, legacy, logical, prefix in CLASSIC_SOURCES:
            try:
                source, row, source_refusals = self._inspect_source(
                    canonical,
                    legacy,
                    logical,
                    prefix,
                    classic,
                )
            except WorkspaceError as error:
                source = None
                row = {
                    "archive": None,
                    "component": logical,
                    "layouts": [
                        str(self.repository_root / canonical),
                        str(self.repository_root / legacy),
                    ],
                    "source": None,
                    "status": "blocked",
                }
                source_refusals = [
                    self._refusal(
                        "invalid_source",
                        f"cannot inspect classic source {canonical}: {error}",
                        "repair the source checkout without discarding local work",
                    )
                ]
            refusals.extend(source_refusals)
            source_rows.append(row)
            if source is not None:
                sources.append(source)
                worktree_rows.extend(
                    worktree.public() for worktree in source.worktrees
                )

        (
            profiles,
            composites,
            profile_rows,
            profile_refusals,
        ) = self._inspect_profiles(sources, classic)
        refusals.extend(profile_refusals)
        topologies, topology_refusals = self._topology_inventory()
        refusals.extend(topology_refusals)
        inert_paths, inert_refusals = self._inert_inventory()
        refusals.extend(inert_refusals)
        refusals.sort(key=lambda item: (item["code"], item["message"]))
        plan = {
            "classic": classic_row,
            "composite_worktrees": [composite.public() for composite in composites],
            "inert_paths": inert_paths,
            "migration": MIGRATION_NAME,
            "profile_rewrites": profile_rows,
            "refusals": refusals,
            "schema_version": PLAN_SCHEMA_VERSION,
            "sources": source_rows,
            "status": (
                "refused"
                if refusals
                else ("ready" if sources or profiles else "not-needed")
            ),
            "topologies": topologies,
            "worktree_migrations": sorted(
                worktree_rows,
                key=lambda row: (row["component"], row["path"]),
            ),
        }
        return _Inspection(plan, classic, sources, profiles, composites)

    def _inspect_classic(
        self,
    ) -> tuple[_Classic | None, dict[str, Any], list[dict[str, str]]]:
        repository, checkout = self._classic_checkout_contract()
        path = self.repository_root / checkout
        row: dict[str, Any] = {
            "branch": None,
            "head": None,
            "path": str(path),
            "repository": repository,
            "status": "missing",
        }
        refusals: list[dict[str, str]] = []
        if path.parent.resolve() != self.repository_root:
            refusals.append(
                self._refusal(
                    "invalid_manifest_checkout",
                    f"classic checkout escapes the wrapper root: {path}",
                    "set the physical classic checkout to a direct wrapper-root path",
                )
            )
            row["status"] = "blocked"
            return None, row, refusals
        identity, _, _, error = self._repository_identity(path, {repository})
        if identity != "expected":
            refusals.append(
                self._refusal(
                    "classic_checkout_unavailable",
                    f"initialized atrinik/classic checkout is required at {path}: {error}",
                    "initialize the classic cohort before applying the repository migration",
                )
            )
            row["status"] = "blocked"
            return None, row, refusals
        try:
            head = self._git_text(path, "rev-parse", "HEAD")
            branch = self._git_optional_text(
                path, "symbolic-ref", "--quiet", "--short", "HEAD"
            )
            dirty = self._status(path)
            activity = self._git_activity(path)
            valid_primary = self._is_primary_worktree(path)
            shallow = self._git_text(path, "rev-parse", "--is-shallow-repository")
            self._git_bytes(path, "fsck", "--connectivity-only", "--no-dangling")
            invalid_modules = [
                prefix
                for _, _, _, prefix in CLASSIC_SOURCES
                if self._git_optional_text(
                    path,
                    "cat-file",
                    "-t",
                    f"{head}:{prefix}",
                )
                != "tree"
                or (path / prefix).is_symlink()
                or not (path / prefix).is_dir()
            ]
        except WorkspaceError as inspect_error:
            refusals.append(
                self._refusal(
                    "invalid_classic_checkout",
                    f"cannot inspect classic checkout: {inspect_error}",
                    "restore a complete clean main primary checkout",
                )
            )
            row["status"] = "blocked"
            return None, row, refusals
        if (
            branch != "main"
            or dirty
            or activity
            or not valid_primary
            or shallow != "false"
            or invalid_modules
        ):
            detail = []
            if branch != "main":
                detail.append("not on main")
            if dirty:
                detail.append("dirty")
            if activity:
                detail.append("Git operation active")
            if not valid_primary:
                detail.append("not the primary worktree")
            if shallow != "false":
                detail.append("shallow")
            if invalid_modules:
                detail.append(
                    "missing or unsafe module trees: " + ", ".join(invalid_modules)
                )
            refusals.append(
                self._refusal(
                    "invalid_classic_checkout",
                    f"classic checkout is not a clean complete main primary: {', '.join(detail)}",
                    "finish work in the classic checkout and restore clean main",
                )
            )
        row.update(
            {
                "branch": branch,
                "head": head,
                "status": "blocked" if refusals else "verified",
            }
        )
        if refusals:
            return None, row, refusals
        return _Classic(path, repository, head, branch), row, refusals

    def _inspect_source(
        self,
        canonical: str,
        legacy: str,
        logical: str,
        prefix: str,
        classic: _Classic | None,
    ) -> tuple[_Source | None, dict[str, Any], list[dict[str, str]]]:
        candidates = [self.repository_root / canonical, self.repository_root / legacy]
        row: dict[str, Any] = {
            "archive": None,
            "component": logical,
            "layouts": [str(path) for path in candidates],
            "source": None,
            "status": "missing",
        }
        refusals: list[dict[str, str]] = []
        proven: list[tuple[Path, str, str, str]] = []
        allowed = {f"atrinik/{legacy}", f"atrinik/{canonical}"}

        for path in candidates:
            if not path.exists() and not path.is_symlink():
                continue
            if path.is_symlink() or not path.is_dir():
                refusals.append(
                    self._refusal(
                        "invalid_source",
                        f"classic source is not a normal directory: {path}",
                        "preserve and move the conflicting path aside, then rerun",
                    )
                )
                continue
            identity, remote, url, error = self._repository_identity(path, allowed)
            if identity != "expected":
                # A fresh replacement checkout at the pre-split canonical path
                # is intentionally ignored when an already-renamed source is
                # present.  Any other occupant fails closed.
                actual = self._repository_coordinates(path)
                if path.name == canonical and f"atrinik/{canonical}" in actual:
                    try:
                        lineage = self._classic_lineage(path, canonical)
                    except WorkspaceError as lineage_error:
                        refusals.append(
                            self._refusal(
                                "unproven_source",
                                f"cannot classify canonical checkout {path}: {lineage_error}",
                                "restore complete history before rerunning",
                            )
                        )
                        continue
                    if not lineage:
                        continue
                refusals.append(
                    self._refusal(
                        "unproven_source",
                        f"cannot prove classic source identity at {path}: {error}",
                        "restore the source remote and complete classic history",
                    )
                )
                continue
            try:
                if not self._classic_lineage(path, canonical):
                    # A repository coordinate alone is not proof.  This also
                    # distinguishes fresh repositories that inherited an old
                    # redirect URL.
                    if path.name == canonical:
                        continue
                    raise WorkspaceError("registered classic root is absent")
            except WorkspaceError as error_value:
                refusals.append(
                    self._refusal(
                        "unproven_source",
                        f"cannot prove classic history at {path}: {error_value}",
                        "restore a complete source history before rerunning",
                    )
                )
                continue
            assert remote is not None and url is not None
            repository = self._repository_from_url(url)
            assert repository is not None
            proven.append((path, remote, url, repository))

        if len(proven) > 1:
            refusals.append(
                self._refusal(
                    "duplicate_classic_source",
                    f"both classic source layouts exist for {canonical}: "
                    + ", ".join(str(value[0]) for value in proven),
                    "preserve both and choose one authoritative source before rerunning",
                )
            )
            row["status"] = "blocked"
            return None, row, refusals
        if not proven:
            if refusals:
                row["status"] = "blocked"
            return None, row, refusals
        path, remote, remote_url, repository = proven[0]
        archive = self._archive_destination(path)
        row.update({"archive": str(archive), "source": str(path)})
        row["layout"] = path.name
        if archive.exists() or archive.is_symlink():
            refusals.append(
                self._refusal(
                    "archive_conflict",
                    f"classic source archive destination is occupied: {archive}",
                    "preserve and move the conflicting archive path aside, then rerun",
                )
            )
            row["status"] = "blocked"
            return None, row, refusals
        try:
            head = self._git_text(path, "rev-parse", "HEAD")
            branch = self._git_optional_text(
                path, "symbolic-ref", "--quiet", "--short", "HEAD"
            )
            status_bytes = self._status(path)
            git_config_sha256 = self._file_sha256(self._git_path(path, "config"))
            remotes = self._remote_inventory(path)
            activity = self._git_activity(path)
            primary = self._is_primary_worktree(path)
            unmerged = self._git_bytes(path, "ls-files", "-u", "-z")
        except WorkspaceError as error_value:
            refusals.append(
                self._refusal(
                    "invalid_source",
                    f"cannot inspect classic source {path}: {error_value}",
                    "restore a normal complete primary checkout",
                )
            )
            row["status"] = "blocked"
            return None, row, refusals
        if branch != "main" or not primary or activity or unmerged:
            refusals.append(
                self._refusal(
                    "unsafe_source_state",
                    "classic source must be a main primary with no active or "
                    f"unmerged operation: {path}",
                    "finish or abort the Git operation while preserving working files",
                )
            )
        worktrees: list[_Worktree] = []
        if classic is not None and not refusals:
            try:
                worktrees = self._source_worktrees(
                    canonical,
                    logical,
                    prefix,
                    path,
                    classic,
                )
            except WorkspaceError as error_value:
                refusals.append(
                    self._refusal(
                        "invalid_source_worktree",
                        f"cannot migrate worktrees for {canonical}: {error_value}",
                        "repair every attached worktree and unsupported index "
                        "state before rerunning",
                    )
                )
        row.update(
            {
                "branch": branch,
                "dirty": bool(status_bytes),
                "git_config_sha256": git_config_sha256,
                "head": head,
                "repository": repository,
                "remotes": remotes,
                "status": "blocked" if refusals else "planned",
                "status_bytes": len(status_bytes),
                "status_sha256": hashlib.sha256(status_bytes).hexdigest(),
            }
        )
        if refusals:
            return None, row, refusals
        assert branch is not None
        return (
            _Source(
                canonical,
                legacy,
                logical,
                prefix,
                path,
                archive,
                repository,
                remote,
                remote_url,
                head,
                branch,
                status_bytes,
                git_config_sha256,
                tuple(worktrees),
            ),
            row,
            refusals,
        )

    def _source_worktrees(
        self,
        canonical: str,
        logical: str,
        prefix: str,
        primary_path: Path,
        classic: _Classic,
    ) -> list[_Worktree]:
        records = self._parse_worktree_records(
            self._git_bytes(primary_path, "worktree", "list", "--porcelain", "-z")
        )
        if not records:
            raise WorkspaceError(f"Git reports no primary worktree for {primary_path}")
        result: list[_Worktree] = []
        used_labels: set[str] = set()
        primary_common = self._git_common_directory(primary_path)
        for record in records:
            raw_path = record.get("worktree")
            raw_head = record.get("HEAD")
            if raw_path is None or raw_head is None:
                raise WorkspaceError("worktree record lacks path or HEAD")
            path = Path(os.fsdecode(raw_path)).resolve()
            is_primary = path == primary_path.resolve()
            if not is_primary and primary_path.resolve() in path.parents:
                raise WorkspaceError(f"attached worktree is nested in its primary: {path}")
            if "prunable" in record:
                raise WorkspaceError(f"prunable worktree must be repaired first: {path}")
            if self._git_common_directory(path) != primary_common:
                raise WorkspaceError(
                    f"worktree has an unexpected common Git directory: {path}"
                )
            branch_raw = record.get("branch")
            branch = os.fsdecode(branch_raw) if branch_raw is not None else None
            if branch and branch.startswith("refs/heads/"):
                branch = branch[len("refs/heads/") :]
            locked_raw = record.get("locked")
            locked = os.fsdecode(locked_raw) if locked_raw is not None else None
            if self._git_activity(path):
                raise WorkspaceError(f"Git operation is active in {path}")
            if self._git_bytes(path, "ls-files", "-u", "-z"):
                raise WorkspaceError(f"unmerged index entries exist in {path}")
            for tagged in self._git_bytes(path, "ls-files", "-v", "-z").split(b"\0"):
                if not tagged:
                    continue
                tag = tagged[:1]
                if tag == b"S" or tag.islower():
                    raise WorkspaceError(
                        f"skip-worktree/assume-unchanged index flags are unsupported in {path}"
                    )
            for staged in self._git_bytes(
                path,
                "ls-files",
                "--stage",
                "-z",
            ).split(b"\0"):
                if not staged:
                    continue
                metadata, separator, raw_name = staged.partition(b"\t")
                fields = metadata.split()
                if (
                    not separator
                    or len(fields) != 3
                    or fields[2] != b"0"
                    or set(fields[1]) == {ord("0")}
                    or fields[0] == b"160000"
                ):
                    raise WorkspaceError(
                        f"unsupported index entry in {path}: "
                        f"{os.fsdecode(raw_name)}"
                    )
            status_bytes, index_sha256, snapshot_sha256 = self._snapshot_worktree(path)
            module_sha256 = self._stable_module_state_sha256(path)
            head = os.fsdecode(raw_head)
            tree_matches = self._component_tree_matches(
                path, head, classic.path, classic.head, prefix
            )
            requires_worktree = not (is_primary and tree_matches and not status_bytes)
            old_label = self._old_worktree_label(primary_path.name, path, is_primary)
            new_label: str | None = None
            destination: Path | None = None
            branch_ref: str | None = None
            mapped_parent: str | None = None
            if requires_worktree:
                new_label = self._migration_label(canonical, old_label, head, used_labels)
                destination = self.paths.worktrees / "classic" / new_label
                if destination.exists() or destination.is_symlink():
                    raise WorkspaceError(
                        f"migrated classic worktree destination is occupied: {destination}"
                    )
                branch_ref = f"migration/{canonical}/{new_label}-{head[:12]}"
                if self._git_optional_text(
                    classic.path, "show-ref", "--verify", "--hash", f"refs/heads/{branch_ref}"
                ):
                    raise WorkspaceError(
                        f"migrated classic branch already exists: {branch_ref}"
                    )
                archive_ref = self._archive_ref(canonical, new_label, head)
                if self._git_optional_text(
                    classic.path,
                    "show-ref",
                    "--verify",
                    "--hash",
                    archive_ref,
                ):
                    raise WorkspaceError(
                        f"local migration archive ref already exists: {archive_ref}"
                    )
                mapped_parent = self._mapped_parent(
                    classic.path,
                    canonical,
                    head,
                )
            result.append(
                _Worktree(
                    canonical,
                    logical,
                    prefix,
                    primary_path.name,
                    path,
                    head,
                    branch,
                    locked,
                    status_bytes,
                    index_sha256,
                    snapshot_sha256,
                    module_sha256,
                    is_primary,
                    old_label,
                    new_label,
                    destination,
                    branch_ref,
                    mapped_parent,
                )
            )
        return result

    def _mapped_parent(
        self,
        classic: Path,
        canonical: str,
        old_head: str,
    ) -> str | None:
        path = classic / "docs" / "history" / f"{canonical}-commit-map.txt"
        if path.exists() or path.is_symlink():
            if path.is_symlink() or not path.is_file():
                raise WorkspaceError(f"commit map is not a regular file: {path}")
            try:
                lines = path.read_text(encoding="ascii").splitlines()
            except (OSError, UnicodeError) as error:
                raise WorkspaceError(f"cannot read commit map {path}: {error}") from error
            mappings: dict[str, str] = {}
            saw_header = False
            for line_number, line in enumerate(lines, 1):
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                fields = line.split()
                if fields == ["old", "new"]:
                    if saw_header or mappings:
                        raise WorkspaceError(
                            f"invalid commit-map row at {path}:{line_number}"
                        )
                    saw_header = True
                    continue
                if (
                    len(fields) != 2
                    or not re.fullmatch(r"[0-9a-f]{40,64}", fields[0])
                    or not re.fullmatch(r"[0-9a-f]{40,64}", fields[1])
                ):
                    raise WorkspaceError(
                        f"invalid commit-map row at {path}:{line_number}"
                    )
                previous = mappings.setdefault(fields[0], fields[1])
                if previous != fields[1]:
                    raise WorkspaceError(
                        f"conflicting commit-map rows for {fields[0]} in {path}"
                    )
            mapped = mappings.get(old_head)
            if mapped is not None and set(mapped) != {"0"}:
                if self._git_optional_text(
                    classic, "rev-parse", "--verify", f"{mapped}^{{commit}}"
                ):
                    return mapped
        return None

    def _inspect_profiles(
        self, sources: list[_Source], classic: _Classic | None
    ) -> tuple[
        list[_ProfileRewrite],
        list[_CompositeWorktree],
        list[dict[str, Any]],
        list[dict[str, str]],
    ]:
        rewrites: list[_ProfileRewrite] = []
        composites: list[_CompositeWorktree] = []
        rows: list[dict[str, Any]] = []
        refusals: list[dict[str, str]] = []
        if not self.paths.profiles.exists():
            return rewrites, composites, rows, refusals
        if self.paths.profiles.is_symlink() or not self.paths.profiles.is_dir():
            return (
                rewrites,
                composites,
                rows,
                [
                    self._refusal(
                        "invalid_profiles_directory",
                        f"profiles path is not a normal directory: {self.paths.profiles}",
                        "restore the wrapper-managed profiles directory",
                    )
                ],
            )
        try:
            selector_map = self._selector_migration_map(sources)
        except WorkspaceError as error:
            return (
                rewrites,
                composites,
                rows,
                [
                    self._refusal(
                        "ambiguous_profile_selector",
                        str(error),
                        "replace ambiguous worktree labels with exact path selectors",
                    )
                ],
            )
        for path in sorted(self.paths.profiles.glob("*.json")):
            if path.is_symlink() or not path.is_file():
                refusals.append(
                    self._refusal(
                        "invalid_profile",
                        f"profile is not a regular file: {path}",
                        "restore or remove the invalid profile after preserving it",
                    )
                )
                continue
            try:
                before = path.read_bytes()
                profile = load_json(path)
                after_value, composite = self._rewrite_profile(
                    path, profile, selector_map, classic
                )
            except (OSError, WorkspaceError) as error:
                refusals.append(
                    self._refusal(
                        "invalid_profile",
                        f"cannot migrate profile {path}: {error}",
                        "repair the saved profile without discarding its selected worktrees",
                    )
                )
                continue
            if after_value is None:
                continue
            if composite is not None:
                if any(
                    existing.label == composite.label
                    and existing.worktrees != composite.worktrees
                    for existing in composites
                ):
                    refusals.append(
                        self._refusal(
                            "invalid_profile",
                            f"composite migration label collision for {path}: "
                            f"{composite.label}",
                            "rename one of the colliding profiles and rerun",
                        )
                    )
                    continue
                if all(existing.label != composite.label for existing in composites):
                    composites.append(composite)
            after = self._json_bytes(after_value)
            action = _ProfileRewrite(path.stem, path, before, after)
            rewrites.append(action)
            rows.append(action.public())
        return rewrites, composites, rows, refusals

    def _selector_migration_map(
        self, sources: Iterable[_Source]
    ) -> dict[tuple[str, str, str], _Worktree]:
        result: dict[tuple[str, str, str], _Worktree] = {}

        def register(key: tuple[str, str, str], worktree: _Worktree) -> None:
            previous = result.get(key)
            if previous is not None and previous.path != worktree.path:
                raise WorkspaceError(
                    f"profile selector {key[0]}:{key[1]}:{key[2]} matches "
                    f"both {previous.path} and {worktree.path}"
                )
            result[key] = worktree

        for source in sources:
            for worktree in source.worktrees:
                for identity in (source.canonical, source.legacy):
                    if worktree.primary:
                        register((identity, "primary", ""), worktree)
                        register((identity, "path", str(source.path)), worktree)
                        register(
                            (identity, "path", str(source.path.resolve())),
                            worktree,
                        )
                    else:
                        expected = (
                            self.paths.worktrees / identity / worktree.old_label
                        ).resolve()
                        if worktree.path.resolve() == expected:
                            register(
                                (identity, "worktree", worktree.old_label),
                                worktree,
                            )
                        register((identity, "path", str(worktree.path)), worktree)
                        register(
                            (identity, "path", str(worktree.path.resolve())),
                            worktree,
                        )
        return result

    def _rewrite_profile(
        self,
        path: Path,
        profile: Any,
        selector_map: dict[tuple[str, str, str], _Worktree],
        classic: _Classic | None,
    ) -> tuple[dict[str, Any] | None, _CompositeWorktree | None]:
        if not isinstance(profile, dict):
            raise WorkspaceError("profile root must be an object")
        schema = profile.get("schema_version")
        if schema == PROFILE_SCHEMA_VERSION:
            if profile.get("stack") == "classic":
                self._validate_profile_shape(path, profile)
            return None, None
        if schema != 1:
            raise WorkspaceError("unsupported profile schema")
        name = profile.get("name")
        components = profile.get("components")
        if not isinstance(name, str) or path.stem != name or not NAME_PATTERN.fullmatch(name):
            raise WorkspaceError("profile name does not match its file")
        if not isinstance(components, dict):
            raise WorkspaceError("profile components must be an object")
        old_keys = set(PROFILE_IDENTITIES) & set(components)
        if not old_keys:
            return None, None
        output: dict[str, dict[str, str]] = {}
        logical_worktrees: dict[str, _Worktree] = {}
        direct_classic_selectors: list[dict[str, str]] = []
        content_selector: dict[str, str] | None = None
        for component_name, raw_selector in components.items():
            selector = self._validate_selector(component_name, raw_selector)
            if component_name in PROFILE_IDENTITIES:
                logical = PROFILE_IDENTITIES[component_name]
                if logical == "content-1x":
                    converted_content = self._convert_content_selector(selector)
                    if (
                        content_selector is not None
                        and content_selector != converted_content
                    ):
                        raise WorkspaceError("conflicting selectors for content-1x")
                    content_selector = converted_content
                else:
                    converted_worktree = selector_map.get(
                        (component_name, selector["kind"], selector["value"])
                    )
                    if converted_worktree is None:
                        raise WorkspaceError(
                            f"selector for {component_name} cannot be proven "
                            "against a migrated source"
                        )
                    previous = logical_worktrees.get(logical)
                    if previous is not None and previous.path != converted_worktree.path:
                        raise WorkspaceError(f"conflicting selectors for {logical}")
                    logical_worktrees[logical] = converted_worktree
            elif component_name.startswith("classic-"):
                direct_classic_selectors.append(selector)
            else:
                output[component_name] = selector

        expected = self._classic_profile_component_names()
        unexpected = set(output) - expected
        if unexpected:
            raise WorkspaceError(
                "profile contains components outside the classic stack: "
                + ", ".join(sorted(unexpected))
            )
        nonprimary = sorted(
            {
                worktree.path: worktree
                for worktree in logical_worktrees.values()
                if worktree.new_label is not None
            }.values(),
            key=lambda worktree: worktree.logical,
        )
        composite: _CompositeWorktree | None = None
        if direct_classic_selectors:
            unique_direct = {
                (selector["kind"], selector["value"])
                for selector in direct_classic_selectors
            }
            if (
                len(unique_direct) != 1
                or nonprimary
                or logical_worktrees
                and unique_direct != {("primary", "")}
            ):
                raise WorkspaceError(
                    "classic module selectors do not identify one physical checkout root"
                )
            kind, value = next(iter(unique_direct))
            root_selector = {"kind": kind, "value": value}
        elif not nonprimary:
            root_selector = {"kind": "primary", "value": ""}
        elif len(nonprimary) == 1:
            assert nonprimary[0].new_label is not None
            root_selector = {
                "kind": "worktree",
                "value": nonprimary[0].new_label,
            }
        else:
            if classic is None:
                raise WorkspaceError("classic checkout is unavailable for composite profile")
            composite = self._plan_composite(path.stem, nonprimary, classic)
            root_selector = {"kind": "worktree", "value": composite.label}

        for component_name in expected:
            if component_name.startswith("classic-"):
                output[component_name] = dict(root_selector)
            elif component_name == "content-1x":
                output[component_name] = content_selector or {
                    "kind": "primary",
                    "value": "",
                }
            else:
                output.setdefault(component_name, {"kind": "primary", "value": ""})
        output = {key: output[key] for key in sorted(expected)}
        rewritten = {
            "schema_version": PROFILE_SCHEMA_VERSION,
            "name": name,
            "stack": "classic",
            "components": output,
        }
        self._validate_profile_shape(path, rewritten)
        return rewritten, composite

    def _plan_composite(
        self, name: str, worktrees: list[_Worktree], classic: _Classic
    ) -> _CompositeWorktree:
        digest = hashlib.sha256()
        for worktree in worktrees:
            digest.update(worktree.logical.encode())
            digest.update(b"\0")
            digest.update(worktree.head.encode())
            digest.update(b"\0")
            digest.update(hashlib.sha256(worktree.status).digest())
        label = f"migrated-profile-{name}-{digest.hexdigest()[:10]}"
        destination = self.paths.worktrees / "classic" / label
        if destination.exists() or destination.is_symlink():
            raise WorkspaceError(
                f"composite classic worktree destination is occupied: {destination}"
            )
        branch_ref = f"migration/profile/{label}"
        if self._git_optional_text(
            classic.path,
            "show-ref",
            "--verify",
            "--hash",
            f"refs/heads/{branch_ref}",
        ):
            raise WorkspaceError(f"composite classic branch already exists: {branch_ref}")
        return _CompositeWorktree(
            name,
            label,
            destination,
            branch_ref,
            tuple(worktrees),
        )

    def _convert_content_selector(self, selector: dict[str, str]) -> dict[str, str]:
        if selector["kind"] == "primary":
            return {"kind": "primary", "value": ""}
        path: Path | None = None
        if selector["kind"] == "worktree":
            path = self.paths.worktrees / "content" / selector["value"]
        elif selector["kind"] in {"path", MIGRATED_CONTENT_WORKTREE_KIND}:
            path = Path(selector["value"])
        if path is None or not path.is_dir() or path.is_symlink():
            raise WorkspaceError("content selector does not identify a present worktree")
        resolved = path.resolve()
        expected_parent = (self.paths.worktrees / "content").resolve()
        if resolved.parent != expected_parent:
            raise WorkspaceError(
                "content selector is not a wrapper-managed content worktree"
            )
        primary = self.repository_root / "content"
        identity, _, _, _ = self._repository_identity(primary, {"atrinik/content"})
        if (
            identity != "expected"
            or self._git_common_directory(resolved)
            != self._git_common_directory(primary)
        ):
            raise WorkspaceError(
                "content selector cannot be proven against canonical content history"
            )
        return {
            "kind": MIGRATED_CONTENT_WORKTREE_KIND,
            "value": str(resolved),
        }

    def _validate_profile_shape(self, path: Path, profile: dict[str, Any]) -> None:
        if set(profile) != {"schema_version", "name", "stack", "components"}:
            raise WorkspaceError("schema-v3 profile has unexpected fields")
        if profile["schema_version"] != PROFILE_SCHEMA_VERSION:
            raise WorkspaceError("profile is not schema v3")
        if profile["name"] != path.stem or profile["stack"] != "classic":
            raise WorkspaceError("schema-v3 classic profile identity is invalid")
        components = profile["components"]
        if not isinstance(components, dict):
            raise WorkspaceError("schema-v3 components must be an object")
        expected = self._classic_profile_component_names()
        if set(components) != expected:
            raise WorkspaceError("schema-v3 classic profile component closure is invalid")
        for component_name, selector in components.items():
            self._validate_selector(component_name, selector)
        classic_selectors = {
            (components[name]["kind"], components[name]["value"])
            for name in expected
            if name.startswith("classic-")
        }
        if len(classic_selectors) != 1:
            raise WorkspaceError(
                "schema-v3 classic components must select one physical checkout root"
            )

    @staticmethod
    def _validate_selector(component: str, selector: Any) -> dict[str, str]:
        if not isinstance(selector, dict) or set(selector) != {"kind", "value"}:
            raise WorkspaceError(f"selector for {component} is invalid")
        kind = selector["kind"]
        value = selector["value"]
        allowed = SELECTOR_KINDS | {MIGRATED_CONTENT_WORKTREE_KIND}
        if kind not in allowed or not isinstance(value, str):
            raise WorkspaceError(f"selector for {component} is invalid")
        if kind == "primary" and value:
            raise WorkspaceError(f"primary selector for {component} must be empty")
        if kind == "worktree" and not NAME_PATTERN.fullmatch(value):
            raise WorkspaceError(f"worktree selector for {component} is invalid")
        if kind == MIGRATED_CONTENT_WORKTREE_KIND and component not in {
            "content",
            "content-1x",
        }:
            raise WorkspaceError(
                f"migrated content selector is invalid for {component}"
            )
        if kind in {"path", MIGRATED_CONTENT_WORKTREE_KIND} and not Path(value).is_absolute():
            raise WorkspaceError(f"path selector for {component} must be absolute")
        return {"kind": kind, "value": value}

    # ------------------------------------------------------------------
    # Transactional application

    def _apply(self, inspection: _Inspection) -> dict[str, Any]:
        if inspection.classic is None:
            raise WorkspaceError("classic checkout disappeared before apply")
        journal = self._pending_value(inspection)
        self.pending_path.parent.mkdir(parents=True, exist_ok=True)
        self._durable_atomic_json(self.pending_path, journal)
        created_worktrees: list[_Worktree] = []
        created_composites: list[_CompositeWorktree] = []
        archived: list[_Source] = []
        exchanged_profiles: list[tuple[_ProfileRewrite, Path, int]] = []
        temporary_profiles: list[Path] = []
        record_installed = False
        try:
            for source in inspection.sources:
                for worktree in source.worktrees:
                    if worktree.destination is None:
                        continue
                    created_worktrees.append(worktree)
                    self._create_classic_worktree(inspection.classic, worktree)

            for composite in inspection.composites:
                created_composites.append(composite)
                self._create_composite_worktree(inspection.classic, composite)

            for action in inspection.profiles:
                backup, mode = self._exchange_profile(action)
                temporary_profiles.append(backup)
                exchanged_profiles.append((action, backup, mode))

            for source in inspection.sources:
                archived.append(source)
                self._archive_source(source)

            # Re-check immutable/inert roots after every mutation.  They are not
            # part of rollback because this migration never writes them.
            inert_after, inert_refusals = self._inert_inventory()
            if inert_refusals or inert_after != inspection.plan["inert_paths"]:
                raise WorkspaceError(
                    "inert content/state/build/log/scenario paths changed during apply"
                )

            result = copy.deepcopy(inspection.plan)
            result["status"] = "applied"
            for row in result["sources"]:
                if row["status"] == "planned":
                    row["status"] = "archived"
            for row in result["worktree_migrations"]:
                row["status"] = "preserved" if row["destination"] else "primary"
            for row in result["composite_worktrees"]:
                row["status"] = "preserved"
            for row in result["profile_rewrites"]:
                row["status"] = "rewritten"
            for row in result["inert_paths"]:
                row["status"] = "preserved"
            record = copy.deepcopy(result)
            record["journal"] = {
                "profiles": [
                    {
                        "after": self._encode_bytes(action.after),
                        "before": self._encode_bytes(action.before),
                        "path": str(action.path),
                    }
                    for action in inspection.profiles
                ]
            }
            self._durable_atomic_json(self.record_path, record)
            record_installed = True
            # Once the durable record exists, cleanup failures must not trigger
            # rollback of a migration that has already committed successfully.
            try:
                self.pending_path.unlink(missing_ok=True)
                self._fsync_directory(self.pending_path.parent)
            except (OSError, WorkspaceError):
                pass
            for path in temporary_profiles:
                try:
                    path.unlink(missing_ok=True)
                except OSError:
                    pass
            return result
        except BaseException as error:
            rollback_errors: list[str] = []
            if not record_installed and (
                self.record_path.exists() or self.record_path.is_symlink()
            ):
                try:
                    self.record_path.unlink()
                    self._fsync_directory(self.record_path.parent)
                except (OSError, WorkspaceError) as rollback_error:
                    rollback_errors.append(
                        f"partial migration record {self.record_path}: {rollback_error}"
                    )
            for source in reversed(archived):
                try:
                    self._restore_source(source)
                except (OSError, WorkspaceError) as rollback_error:
                    rollback_errors.append(f"source {source.canonical}: {rollback_error}")
            for action, backup, mode in reversed(exchanged_profiles):
                try:
                    self._rollback_profile(action, backup, mode)
                except (OSError, WorkspaceError) as rollback_error:
                    rollback_errors.append(f"profile {action.path}: {rollback_error}")
            for composite in reversed(created_composites):
                try:
                    self._remove_composite_worktree(inspection.classic, composite)
                except WorkspaceError as rollback_error:
                    rollback_errors.append(
                        f"composite worktree {composite.label}: {rollback_error}"
                    )
            for worktree in reversed(created_worktrees):
                try:
                    self._remove_created_worktree(inspection.classic, worktree)
                except WorkspaceError as rollback_error:
                    rollback_errors.append(f"worktree {worktree.new_label}: {rollback_error}")
            if not rollback_errors:
                try:
                    self.pending_path.unlink(missing_ok=True)
                    self._fsync_directory(self.pending_path.parent)
                except (OSError, WorkspaceError) as rollback_error:
                    rollback_errors.append(
                        f"pending journal {self.pending_path}: {rollback_error}"
                    )
            if not rollback_errors:
                for path in temporary_profiles:
                    try:
                        path.unlink(missing_ok=True)
                    except OSError:
                        pass
            if rollback_errors:
                raise WorkspaceError(
                    f"repository migration failed ({error}); rollback also failed: "
                    + "; ".join(rollback_errors)
                ) from error
            if isinstance(error, WorkspaceError):
                raise
            raise WorkspaceError(
                f"repository migration failed and was rolled back: {error}"
            ) from error

    def _create_classic_worktree(self, classic: _Classic, worktree: _Worktree) -> None:
        assert worktree.destination is not None
        assert worktree.branch_ref is not None
        worktree.destination.parent.mkdir(parents=True, exist_ok=True)
        assert worktree.new_label is not None
        archive_ref = self._archive_ref(
            worktree.canonical,
            worktree.new_label,
            worktree.head,
        )
        self._git_bytes(
            classic.path,
            "fetch",
            "--no-write-fetch-head",
            "--no-tags",
            str(worktree.path),
            worktree.head,
        )
        self._git_bytes(
            classic.path,
            "update-ref",
            archive_ref,
            worktree.head,
            "0" * 40,
        )
        parent = worktree.mapped_parent or worktree.head
        if worktree.mapped_parent is not None:
            mapped_tree = self._git_text(
                classic.path, "rev-parse", f"{worktree.mapped_parent}:{worktree.prefix}"
            )
            source_tree = self._git_text(
                classic.path, "rev-parse", f"{worktree.head}^{{tree}}"
            )
            if mapped_tree != source_tree:
                raise WorkspaceError(
                    f"commit map tree mismatch for {worktree.canonical} {worktree.head}"
                )
        tree = self._combined_tree(classic.path, classic.head, worktree)
        base_tree = self._git_text(classic.path, "rev-parse", f"{classic.head}^{{tree}}")
        needs_source_parent = (
            parent != classic.head
            and not self._is_ancestor(classic.path, parent, classic.head)
        )
        if tree == base_tree and not needs_source_parent:
            commit = classic.head
        else:
            arguments = ["commit-tree", tree, "-p", classic.head]
            if needs_source_parent:
                arguments.extend(["-p", parent])
            timestamp = self._git_text(
                classic.path, "show", "-s", "--format=%ct", worktree.head
            )
            environment = os.environ.copy()
            environment.update(
                {
                    "GIT_AUTHOR_NAME": "Atrinik Workspace Migration",
                    "GIT_AUTHOR_EMAIL": "migration@atrinik.invalid",
                    "GIT_AUTHOR_DATE": f"{timestamp} +0000",
                    "GIT_COMMITTER_NAME": "Atrinik Workspace Migration",
                    "GIT_COMMITTER_EMAIL": "migration@atrinik.invalid",
                    "GIT_COMMITTER_DATE": f"{timestamp} +0000",
                }
            )
            message = (
                f"chore({worktree.canonical}): bridge local classic worktree\n\n"
                f"Original-Head: {worktree.head}\n"
                f"Original-Path: {worktree.path}\n"
            ).encode()
            commit = self._git_bytes_env(
                classic.path, environment, *arguments, input_value=message
            ).decode().strip()
        self._git_bytes(
            classic.path,
            "update-ref",
            f"refs/heads/{worktree.branch_ref}",
            commit,
            "0" * 40,
        )
        self._git_bytes(
            classic.path,
            "worktree",
            "add",
            "--",
            str(worktree.destination),
            worktree.branch_ref,
        )
        self._transplant_worktree_state(worktree)

    def _create_composite_worktree(
        self, classic: _Classic, composite: _CompositeWorktree
    ) -> None:
        composite.destination.parent.mkdir(parents=True, exist_ok=True)
        tree = self._combined_tree_many(
            classic.path,
            classic.head,
            composite.worktrees,
        )
        arguments = ["commit-tree", tree, "-p", classic.head]
        parents = {classic.head}
        timestamps: list[int] = []
        for worktree in composite.worktrees:
            assert worktree.branch_ref is not None
            parent = self._git_text(
                classic.path,
                "rev-parse",
                "--verify",
                f"refs/heads/{worktree.branch_ref}^{{commit}}",
            )
            if parent not in parents:
                arguments.extend(["-p", parent])
                parents.add(parent)
            timestamps.append(
                int(
                    self._git_text(
                        classic.path,
                        "show",
                        "-s",
                        "--format=%ct",
                        worktree.head,
                    )
                )
            )
        timestamp = str(max(timestamps))
        environment = os.environ.copy()
        environment.update(
            {
                "GIT_AUTHOR_NAME": "Atrinik Workspace Migration",
                "GIT_AUTHOR_EMAIL": "migration@atrinik.invalid",
                "GIT_AUTHOR_DATE": f"{timestamp} +0000",
                "GIT_COMMITTER_NAME": "Atrinik Workspace Migration",
                "GIT_COMMITTER_EMAIL": "migration@atrinik.invalid",
                "GIT_COMMITTER_DATE": f"{timestamp} +0000",
            }
        )
        component_lines = "".join(
            f"Original-{worktree.canonical.title()}-Head: {worktree.head}\n"
            for worktree in composite.worktrees
        )
        message = (
            f"chore(classic): bridge local profile {composite.profile}\n\n"
            f"Original-Profile: {composite.profile}\n"
            f"{component_lines}"
        ).encode()
        commit = self._git_bytes_env(
            classic.path,
            environment,
            *arguments,
            input_value=message,
        ).decode().strip()
        self._git_bytes(
            classic.path,
            "update-ref",
            f"refs/heads/{composite.branch_ref}",
            commit,
            "0" * 40,
        )
        self._git_bytes(
            classic.path,
            "worktree",
            "add",
            "--",
            str(composite.destination),
            composite.branch_ref,
        )
        for worktree in composite.worktrees:
            self._transplant_worktree_state(
                worktree,
                destination_root=composite.destination,
                verify_destination=False,
            )
        actual_status = self._status(composite.destination)
        expected_status = composite.expected_status()
        if actual_status != expected_status:
            raise WorkspaceError(
                f"combined staged/unstaged/untracked state changed while migrating "
                f"profile {composite.profile} (expected status SHA-256 "
                f"{hashlib.sha256(expected_status).hexdigest()}, found "
                f"{hashlib.sha256(actual_status).hexdigest()})"
            )

    def _combined_tree(self, classic: Path, base: str, worktree: _Worktree) -> str:
        return self._combined_tree_many(classic, base, (worktree,))

    def _combined_tree_many(
        self,
        classic: Path,
        base: str,
        worktrees: Iterable[_Worktree],
    ) -> str:
        descriptor, index_name = tempfile.mkstemp(prefix=".classic-migration-index-")
        os.close(descriptor)
        index = Path(index_name)
        index.unlink()
        environment = os.environ.copy()
        environment["GIT_INDEX_FILE"] = str(index)
        try:
            self._git_bytes_env(classic, environment, "read-tree", base)
            for worktree in worktrees:
                existing = self._git_bytes_env(
                    classic,
                    environment,
                    "ls-files",
                    "-z",
                    "--",
                    worktree.prefix,
                )
                if existing:
                    self._git_bytes_env(
                        classic,
                        environment,
                        "update-index",
                        "--force-remove",
                        "-z",
                        "--stdin",
                        input_value=existing,
                    )
                self._git_bytes_env(
                    classic,
                    environment,
                    "read-tree",
                    f"--prefix={worktree.prefix}/",
                    f"{worktree.head}^{{tree}}",
                )
            return self._git_bytes_env(
                classic, environment, "write-tree"
            ).decode().strip()
        finally:
            index.unlink(missing_ok=True)

    def _transplant_worktree_state(
        self,
        worktree: _Worktree,
        *,
        destination_root: Path | None = None,
        verify_destination: bool = True,
    ) -> None:
        if destination_root is None:
            assert worktree.destination is not None
            destination_root = worktree.destination
        source = worktree.path
        self._verify_worktree_snapshot(worktree, source)
        destination_prefix = destination_root / worktree.prefix
        listed = self._git_bytes(
            source,
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
        ).split(b"\0")
        paths = [self._safe_relative(os.fsdecode(value)) for value in listed if value]
        self._remove_generated_path(destination_prefix)
        destination_prefix.mkdir(parents=True, exist_ok=True)
        for relative in paths:
            source_path = source / relative
            if not source_path.exists() and not source_path.is_symlink():
                continue
            destination = destination_prefix / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if source_path.is_symlink():
                os.symlink(os.readlink(source_path), destination)
            elif source_path.is_file():
                shutil.copy2(source_path, destination, follow_symlinks=False)
            elif source_path.is_dir():
                # Git does not track directories; an entry here would be a
                # submodule or an unsafe special case.
                raise WorkspaceError(f"unsupported directory index entry: {source_path}")
            else:
                raise WorkspaceError(f"unsupported worktree entry: {source_path}")

        existing = self._git_bytes(
            destination_root, "ls-files", "-z", "--", worktree.prefix
        )
        if existing:
            self._git_bytes_input(
                destination_root,
                existing,
                "update-index",
                "--force-remove",
                "-z",
                "--stdin",
            )
        index_rows = self._git_bytes(source, "ls-files", "--stage", "-z").split(b"\0")
        update = bytearray()
        for row in index_rows:
            if not row:
                continue
            metadata, raw_path = row.split(b"\t", 1)
            mode, object_id, stage = metadata.decode("ascii").split()
            if stage != "0" or set(object_id) == {"0"} or mode == "160000":
                raise WorkspaceError(
                    f"unsupported staged index entry in {source}: {os.fsdecode(raw_path)}"
                )
            relative = self._safe_relative(os.fsdecode(raw_path))
            blob = self._git_bytes(source, "cat-file", "blob", object_id)
            written = self._git_bytes_input(
                destination_root, blob, "hash-object", "-w", "--stdin"
            ).decode().strip()
            if written != object_id:
                raise WorkspaceError("staged blob changed while transplanting index")
            target = f"{worktree.prefix}/{relative.as_posix()}"
            update.extend(f"{mode} {object_id} 0\t{target}".encode())
            update.append(0)
        if update:
            self._git_bytes_input(
                destination_root,
                bytes(update),
                "update-index",
                "-z",
                "--index-info",
            )
        if (
            self._stable_module_state_sha256(destination_root, worktree.prefix)
            != worktree.module_sha256
        ):
            raise WorkspaceError(
                f"module file/index state changed while migrating {source}"
            )
        if verify_destination:
            actual = self._status(destination_root)
            expected = self._prefixed_status(worktree.status, worktree.prefix)
            if actual != expected:
                raise WorkspaceError(
                    f"staged/unstaged/untracked state changed while migrating {source}"
                )
        self._verify_worktree_snapshot(worktree, source)

    def _archive_source(self, source: _Source) -> None:
        source.archive.parent.mkdir(parents=True, exist_ok=True)
        self._rename_no_replace(source.path, source.archive)
        self._fsync_directory(source.path.parent)
        if source.archive.parent != source.path.parent:
            self._fsync_directory(source.archive.parent)
        linked = [worktree.path for worktree in source.worktrees if not worktree.primary]
        if linked:
            self._git_bytes(
                source.archive,
                "worktree",
                "repair",
                "--",
                *(str(path) for path in linked),
            )
        self._verify_archived_source(source)

    def _restore_source(self, source: _Source) -> None:
        if source.archive.exists() and not source.path.exists():
            self._rename_no_replace(source.archive, source.path)
            self._fsync_directory(source.archive.parent)
            if source.path.parent != source.archive.parent:
                self._fsync_directory(source.path.parent)
        linked = [worktree.path for worktree in source.worktrees if not worktree.primary]
        if linked:
            self._git_bytes(
                source.path,
                "worktree",
                "repair",
                "--",
                *(str(path) for path in linked),
            )
        for worktree in source.worktrees:
            self._verify_original_worktree(worktree)

    def _verify_archived_source(self, source: _Source) -> None:
        if self._git_text(source.archive, "rev-parse", "HEAD") != source.head:
            raise WorkspaceError(f"archived source HEAD changed for {source.canonical}")
        if (
            self._file_sha256(self._git_path(source.archive, "config"))
            != source.git_config_sha256
        ):
            raise WorkspaceError(
                f"archived source Git configuration changed for {source.canonical}"
            )
        if self._status(source.archive) != source.status:
            raise WorkspaceError(f"archived source status changed for {source.canonical}")
        for worktree in source.worktrees:
            self._verify_original_worktree(
                worktree,
                path=source.archive if worktree.primary else None,
            )

    def _verify_original_worktree(
        self,
        worktree: _Worktree,
        *,
        path: Path | None = None,
    ) -> None:
        actual_path = path or worktree.path
        if self._git_text(actual_path, "rev-parse", "HEAD") != worktree.head:
            raise WorkspaceError(f"original worktree HEAD changed: {actual_path}")
        branch = self._git_optional_text(
            actual_path, "symbolic-ref", "--quiet", "--short", "HEAD"
        )
        if branch != worktree.branch:
            raise WorkspaceError(f"original worktree branch changed: {actual_path}")
        if self._worktree_lock(actual_path) != worktree.locked:
            raise WorkspaceError(f"original worktree lock changed: {actual_path}")
        self._verify_worktree_snapshot(worktree, actual_path)

    def _verify_worktree_snapshot(self, worktree: _Worktree, path: Path) -> None:
        status_value, index_sha256, snapshot_sha256 = self._snapshot_worktree(path)
        if (
            status_value != worktree.status
            or index_sha256 != worktree.index_sha256
            or snapshot_sha256 != worktree.snapshot_sha256
            or self._stable_module_state_sha256(path) != worktree.module_sha256
        ):
            raise WorkspaceError(f"worktree changed during migration: {path}")

    def _exchange_profile(self, action: _ProfileRewrite) -> tuple[Path, int]:
        try:
            current = action.path.read_bytes()
            mode = stat.S_IMODE(action.path.stat().st_mode)
        except OSError as error:
            raise WorkspaceError(f"cannot re-read profile {action.path}: {error}") from error
        if current != action.before:
            raise WorkspaceError(f"profile changed during migration: {action.path}")
        descriptor, name = tempfile.mkstemp(
            prefix=f".{action.path.name}.migration-", dir=action.path.parent
        )
        replacement = Path(name)
        exchanged = False
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(action.after)
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(replacement, mode)
            exchange_paths(replacement, action.path)
            self._fsync_directory(action.path.parent)
            exchanged = True
            if action.path.read_bytes() != action.after:
                raise WorkspaceError(f"profile exchange verification failed: {action.path}")
            return replacement, mode
        except BaseException as error:
            if not exchanged:
                replacement.unlink(missing_ok=True)
                raise
            # After exchange, replacement contains the exact original profile.
            # Put that inode back even if verification or a concurrent writer
            # failed, and preserve the displaced bytes for manual recovery.
            try:
                if replacement.read_bytes() != action.before:
                    raise WorkspaceError(
                        f"original profile backup changed during exchange: {replacement}"
                    )
                exchange_paths(replacement, action.path)
                self._fsync_directory(action.path.parent)
                if action.path.read_bytes() != action.before:
                    raise WorkspaceError(
                        f"cannot restore original profile after exchange: {action.path}"
                    )
            except (OSError, WorkspaceError) as recovery_error:
                raise WorkspaceError(
                    f"profile exchange failed ({error}); preserve {action.path} and "
                    f"{replacement}: {recovery_error}"
                ) from error
            raise

    def _rollback_profile(
        self,
        action: _ProfileRewrite,
        backup: Path,
        mode: int,
    ) -> None:
        if action.path.read_bytes() != action.after or backup.read_bytes() != action.before:
            raise WorkspaceError(
                f"profile changed after migration replacement: {action.path}"
            )
        exchange_paths(backup, action.path)
        os.chmod(action.path, mode)
        self._fsync_directory(action.path.parent)

    def _remove_created_worktree(self, classic: _Classic, worktree: _Worktree) -> None:
        assert worktree.destination is not None
        assert worktree.branch_ref is not None
        if worktree.destination.exists() or worktree.destination.is_symlink():
            self._git_bytes(
                classic.path,
                "worktree",
                "remove",
                "--force",
                "--",
                str(worktree.destination),
            )
        current = self._git_optional_text(
            classic.path,
            "show-ref",
            "--verify",
            "--hash",
            f"refs/heads/{worktree.branch_ref}",
        )
        if current:
            self._git_bytes(
                classic.path, "update-ref", "-d", f"refs/heads/{worktree.branch_ref}"
            )
        assert worktree.new_label is not None
        archive_ref = self._archive_ref(
            worktree.canonical,
            worktree.new_label,
            worktree.head,
        )
        if self._git_optional_text(
            classic.path, "show-ref", "--verify", "--hash", archive_ref
        ):
            self._git_bytes(classic.path, "update-ref", "-d", archive_ref)
        try:
            worktree.destination.parent.rmdir()
        except OSError:
            pass

    def _remove_composite_worktree(
        self,
        classic: _Classic,
        composite: _CompositeWorktree,
    ) -> None:
        if composite.destination.exists() or composite.destination.is_symlink():
            self._git_bytes(
                classic.path,
                "worktree",
                "remove",
                "--force",
                "--",
                str(composite.destination),
            )
        reference = f"refs/heads/{composite.branch_ref}"
        if self._git_optional_text(
            classic.path,
            "show-ref",
            "--verify",
            "--hash",
            reference,
        ):
            self._git_bytes(classic.path, "update-ref", "-d", reference)
        try:
            composite.destination.parent.rmdir()
        except OSError:
            pass

    # ------------------------------------------------------------------
    # Journal, rollback, and audit

    def _pending_value(self, inspection: _Inspection) -> dict[str, Any]:
        return {
            "migration": MIGRATION_NAME,
            "schema_version": PLAN_SCHEMA_VERSION,
            "classic": str(inspection.classic.path) if inspection.classic else None,
            "sources": [
                {
                    "archive": str(source.archive),
                    "path": str(source.path),
                    "linked": [
                        str(worktree.path)
                        for worktree in source.worktrees
                        if not worktree.primary
                    ],
                }
                for source in inspection.sources
            ],
            "worktrees": [
                {
                    "archive_ref": (
                        self._archive_ref(
                            worktree.canonical,
                            str(worktree.new_label),
                            worktree.head,
                        )
                    ),
                    "branch_ref": worktree.branch_ref,
                    "destination": str(worktree.destination),
                }
                for source in inspection.sources
                for worktree in source.worktrees
                if worktree.destination is not None
            ]
            + [
                {
                    "archive_ref": "",
                    "branch_ref": composite.branch_ref,
                    "destination": str(composite.destination),
                }
                for composite in inspection.composites
            ],
            "profiles": [
                {
                    "after": self._encode_bytes(action.after),
                    "before": self._encode_bytes(action.before),
                    "path": str(action.path),
                }
                for action in inspection.profiles
            ],
            "plan": inspection.plan,
        }

    def _rollback_pending(self) -> list[str]:
        if self.pending_path.is_symlink():
            return [f"pending journal is a symlink: {self.pending_path}"]
        try:
            pending = load_json(self.pending_path)
        except WorkspaceError as error:
            return [str(error)]
        if (
            not isinstance(pending, dict)
            or pending.get("migration") != MIGRATION_NAME
            or pending.get("schema_version") != PLAN_SCHEMA_VERSION
        ):
            return ["pending journal has an unsupported shape"]
        try:
            self._validate_pending_journal(pending)
        except WorkspaceError as error:
            return [str(error)]
        errors: list[str] = []
        classic = Path(str(pending.get("classic", "")))
        planned_destinations = self._pending_destination_plans(pending)
        for raw in reversed(pending.get("sources", [])):
            if not isinstance(raw, dict):
                errors.append("invalid source journal entry")
                continue
            source = Path(str(raw.get("path", "")))
            archive = Path(str(raw.get("archive", "")))
            try:
                if archive.exists() and not source.exists():
                    self._rename_no_replace(archive, source)
                    linked = [Path(str(value)) for value in raw.get("linked", [])]
                    if linked:
                        self._git_bytes(
                            source,
                            "worktree",
                            "repair",
                            "--",
                            *(str(path) for path in linked),
                        )
                elif archive.exists() and source.exists():
                    raise WorkspaceError(f"both source and archive exist: {source}")
            except (OSError, WorkspaceError) as error:
                errors.append(str(error))
        for raw in reversed(pending.get("profiles", [])):
            if not isinstance(raw, dict):
                errors.append("invalid profile journal entry")
                continue
            path = Path(str(raw.get("path", "")))
            try:
                before = self._decode_bytes(raw.get("before"))
                after = self._decode_bytes(raw.get("after"))
                current = path.read_bytes()
                if current == after:
                    self._atomic_bytes(path, before)
                    self._fsync_directory(path.parent)
                elif current != before:
                    raise WorkspaceError(f"profile has unrecognized bytes: {path}")
            except (OSError, WorkspaceError) as error:
                errors.append(str(error))
        for raw in reversed(pending.get("worktrees", [])):
            if not isinstance(raw, dict):
                errors.append("invalid worktree journal entry")
                continue
            destination = Path(str(raw.get("destination", "")))
            branch_ref = str(raw.get("branch_ref", ""))
            archive_ref = str(raw.get("archive_ref", ""))
            try:
                if destination.exists() or destination.is_symlink():
                    plan_row = planned_destinations.get(destination)
                    if (
                        plan_row is None
                        or destination.is_symlink()
                        or not destination.is_dir()
                        or self._git_common_directory(destination)
                        != self._git_common_directory(classic)
                        or self._git_optional_text(
                            destination,
                            "symbolic-ref",
                            "--quiet",
                            "--short",
                            "HEAD",
                        )
                        != branch_ref
                        or not self._pending_destination_matches(
                            destination,
                            plan_row,
                        )
                    ):
                        raise WorkspaceError(
                            f"recorded migration worktree identity changed: {destination}"
                        )
                    self._git_bytes(
                        classic,
                        "worktree",
                        "remove",
                        "--force",
                        "--",
                        str(destination),
                    )
                if branch_ref and self._git_optional_text(
                    classic,
                    "show-ref",
                    "--verify",
                    "--hash",
                    f"refs/heads/{branch_ref}",
                ):
                    self._git_bytes(
                        classic, "update-ref", "-d", f"refs/heads/{branch_ref}"
                    )
                if archive_ref and self._git_optional_text(
                    classic, "show-ref", "--verify", "--hash", archive_ref
                ):
                    self._git_bytes(classic, "update-ref", "-d", archive_ref)
            except WorkspaceError as error:
                errors.append(str(error))
        if not errors:
            try:
                self.pending_path.unlink(missing_ok=True)
                self._fsync_directory(self.pending_path.parent)
            except (OSError, WorkspaceError) as error:
                errors.append(f"cannot remove recovered pending journal: {error}")
        return errors

    def _validate_pending_journal(self, pending: dict[str, Any]) -> None:
        if set(pending) != {
            "migration",
            "schema_version",
            "classic",
            "sources",
            "worktrees",
            "profiles",
            "plan",
        }:
            raise WorkspaceError("pending journal has unexpected fields")
        _, checkout = self._classic_checkout_contract()
        expected_classic = self.repository_root / checkout
        classic = Path(str(pending.get("classic", "")))
        if classic != expected_classic or classic.is_symlink() or not classic.is_dir():
            raise WorkspaceError("pending journal classic checkout is invalid")
        identity, _, _, _ = self._repository_identity(classic, {"atrinik/classic"})
        if identity != "expected":
            raise WorkspaceError("pending journal classic checkout identity is invalid")
        sources = pending.get("sources")
        worktrees = pending.get("worktrees")
        profiles = pending.get("profiles")
        if (
            not isinstance(sources, list)
            or not isinstance(worktrees, list)
            or not isinstance(profiles, list)
            or not isinstance(pending.get("plan"), dict)
        ):
            raise WorkspaceError("pending journal collections are invalid")
        allowed_source_names = {
            value
            for canonical, legacy, _, _ in CLASSIC_SOURCES
            for value in (canonical, legacy)
        }
        seen_sources: set[Path] = set()
        for raw in sources:
            if not isinstance(raw, dict) or set(raw) != {"archive", "path", "linked"}:
                raise WorkspaceError("pending journal source entry is invalid")
            source = Path(str(raw["path"]))
            archive = Path(str(raw["archive"]))
            linked_raw = raw["linked"]
            if (
                not source.is_absolute()
                or source.parent != self.repository_root
                or source.name not in allowed_source_names
                or source in seen_sources
                or archive != self._archive_destination(source)
                or not isinstance(linked_raw, list)
                or not all(isinstance(value, str) for value in linked_raw)
            ):
                raise WorkspaceError("pending journal source paths are invalid")
            seen_sources.add(source)
            repository = archive if archive.exists() else source
            if repository.is_symlink() or not repository.is_dir():
                raise WorkspaceError(
                    f"pending source and archive are both unavailable: {source}"
                )
            records = self._parse_worktree_records(
                self._git_bytes(
                    repository,
                    "worktree",
                    "list",
                    "--porcelain",
                    "-z",
                )
            )
            actual_linked = {
                Path(os.fsdecode(record["worktree"])).resolve()
                for record in records
                if "worktree" in record
                and Path(os.fsdecode(record["worktree"])).resolve()
                != repository.resolve()
                and Path(os.fsdecode(record["worktree"])).resolve()
                != source.resolve()
                and Path(os.fsdecode(record["worktree"])).resolve()
                != archive.resolve()
            }
            recorded_linked = {Path(value).resolve() for value in linked_raw}
            if len(recorded_linked) != len(linked_raw) or recorded_linked != actual_linked:
                raise WorkspaceError(
                    f"pending linked-worktree inventory changed for {source}"
                )
        seen_destinations: set[Path] = set()
        seen_branches: set[str] = set()
        for raw in worktrees:
            if (
                not isinstance(raw, dict)
                or set(raw) != {"archive_ref", "branch_ref", "destination"}
            ):
                raise WorkspaceError("pending journal worktree entry is invalid")
            destination = Path(str(raw["destination"]))
            branch_ref = raw["branch_ref"]
            archive_ref = raw["archive_ref"]
            if (
                not destination.is_absolute()
                or destination.parent != self.paths.worktrees / "classic"
                or not NAME_PATTERN.fullmatch(destination.name)
                or destination in seen_destinations
                or not isinstance(branch_ref, str)
                or not re.fullmatch(r"migration/[a-z0-9._/-]+", branch_ref)
                or ".." in branch_ref.split("/")
                or branch_ref in seen_branches
                or not isinstance(archive_ref, str)
                or archive_ref
                and not re.fullmatch(
                    r"refs/heads/archive/local-migration/[a-z0-9._/-]+",
                    archive_ref,
                )
            ):
                raise WorkspaceError("pending journal worktree paths are invalid")
            seen_destinations.add(destination)
            seen_branches.add(branch_ref)
        seen_profiles: set[Path] = set()
        for raw in profiles:
            if not isinstance(raw, dict) or set(raw) != {"after", "before", "path"}:
                raise WorkspaceError("pending journal profile entry is invalid")
            path = Path(str(raw["path"]))
            if (
                not path.is_absolute()
                or path.parent != self.paths.profiles
                or path.suffix != ".json"
                or not NAME_PATTERN.fullmatch(path.stem)
                or path in seen_profiles
                or path.is_symlink()
                or not path.is_file()
            ):
                raise WorkspaceError("pending journal profile path is invalid")
            self._decode_bytes(raw["before"])
            self._decode_bytes(raw["after"])
            seen_profiles.add(path)
        expected_destinations = self._pending_destination_plans(pending)
        recorded_destinations = {
            Path(str(raw["destination"])): raw
            for raw in worktrees
            if isinstance(raw, dict) and "destination" in raw
        }
        if set(recorded_destinations) != set(expected_destinations):
            raise WorkspaceError(
                "pending journal worktrees do not match the recorded migration plan"
            )
        for destination, plan_row in expected_destinations.items():
            raw = recorded_destinations[destination]
            expected_archive_ref = ""
            if "source_component" in plan_row:
                expected_archive_ref = self._archive_ref(
                    str(plan_row["source_component"]),
                    str(plan_row["label"]),
                    str(plan_row["head"]),
                )
            if (
                raw.get("branch_ref") != plan_row.get("branch_ref")
                or raw.get("archive_ref") != expected_archive_ref
            ):
                raise WorkspaceError(
                    "pending journal worktree refs do not match the migration plan"
                )

    def _pending_destination_plans(
        self,
        pending: dict[str, Any],
    ) -> dict[Path, dict[str, Any]]:
        plan = pending.get("plan")
        if not isinstance(plan, dict):
            raise WorkspaceError("pending journal plan is invalid")
        result: dict[Path, dict[str, Any]] = {}
        for key in ("worktree_migrations", "composite_worktrees"):
            rows = plan.get(key)
            if not isinstance(rows, list):
                raise WorkspaceError(f"pending journal plan lacks {key}")
            for row in rows:
                if not isinstance(row, dict):
                    raise WorkspaceError(f"pending journal plan has an invalid {key} row")
                raw_destination = row.get("destination")
                if not raw_destination:
                    continue
                destination = Path(str(raw_destination))
                if destination in result:
                    raise WorkspaceError(
                        "pending journal plan repeats a worktree destination"
                    )
                result[destination] = row
        return result

    def _pending_destination_matches(
        self,
        destination: Path,
        plan_row: dict[str, Any],
    ) -> bool:
        expected_status = (
            plan_row.get("destination_status_sha256")
            if "source_component" in plan_row
            else plan_row.get("status_sha256")
        )
        if hashlib.sha256(self._status(destination)).hexdigest() != expected_status:
            return False
        if "source_component" in plan_row:
            return (
                isinstance(plan_row.get("source_component"), str)
                and self._stable_module_state_sha256(
                    destination,
                    plan_row["source_component"],
                )
                == plan_row.get("module_sha256")
            )
        module_states = plan_row.get("module_states")
        return (
            isinstance(module_states, list)
            and bool(module_states)
            and all(
                isinstance(module, dict)
                and isinstance(module.get("prefix"), str)
                and self._stable_module_state_sha256(
                    destination,
                    module["prefix"],
                )
                == module.get("sha256")
                for module in module_states
            )
        )

    def _audit(self) -> dict[str, Any]:
        if not self.record_path.is_file() or self.record_path.is_symlink():
            if self.pending_path.exists() or self.pending_path.is_symlink():
                try:
                    pending = load_json(self.pending_path)
                    result = copy.deepcopy(pending.get("plan", {}))
                except WorkspaceError as error:
                    return self._empty_audit_refusal(
                        "invalid_pending_migration",
                        f"cannot read pending repository migration: {error}",
                        "restore the exact pending journal before recovery",
                    )
                if not isinstance(result, dict):
                    result = {}
                result.setdefault("refusals", [])
                result["refusals"].append(
                    self._refusal(
                        "pending_migration",
                        "repository migration was interrupted before its final record",
                        "rerun --apply to perform checked rollback and migration",
                    )
                )
                result["status"] = "incomplete"
                return result
            inspection = self._inspect()
            inspection.plan["refusals"].append(
                self._refusal(
                    "migration_record_missing",
                    f"repository migration record is missing: {self.record_path}",
                    "run --dry-run and then --apply",
                )
            )
            inspection.plan["status"] = "incomplete"
            return inspection.plan
        try:
            record = load_json(self.record_path)
        except WorkspaceError as error:
            return self._empty_audit_refusal(
                "invalid_migration_record",
                f"cannot read repository migration record: {error}",
                "restore the exact record written by migration",
            )
        if (
            not isinstance(record, dict)
            or record.get("schema_version") != PLAN_SCHEMA_VERSION
            or record.get("migration") != MIGRATION_NAME
            or not isinstance(record.get("sources"), list)
            or not isinstance(record.get("composite_worktrees"), list)
            or not isinstance(record.get("profile_rewrites"), list)
            or not isinstance(record.get("worktree_migrations"), list)
        ):
            return self._empty_audit_refusal(
                "invalid_migration_record",
                "repository migration record has an unsupported shape",
                "restore the exact record written by migration",
            )
        result = {key: copy.deepcopy(value) for key, value in record.items() if key != "journal"}
        result["refusals"] = []
        classic_path = Path(str(record.get("classic", {}).get("path", "")))
        identity, _, _, _ = self._repository_identity(classic_path, {"atrinik/classic"})
        if identity != "expected":
            result["refusals"].append(
                self._refusal(
                    "classic_audit_failed",
                    f"classic checkout no longer has the recorded identity: {classic_path}",
                    "restore the atrinik/classic checkout",
                )
            )
        try:
            classic_common = self._git_common_directory(classic_path)
        except WorkspaceError:
            classic_common = None
        for row in record["sources"]:
            if isinstance(row, dict) and row.get("status") == "missing":
                continue
            if not isinstance(row, dict) or row.get("status") != "archived":
                result["refusals"].append(
                    self._refusal(
                        "invalid_migration_record",
                        "record contains an invalid source entry",
                        "restore the exact migration record",
                    )
                )
                continue
            source_path = Path(str(row.get("source", "")))
            archive = Path(str(row.get("archive", "")))
            source_vacated = not source_path.exists() and not source_path.is_symlink()
            source_reused = self._replacement_checkout_occupies_source_path(
                source_path, archive
            )
            valid = (
                (source_vacated or source_reused)
                and archive.is_dir()
                and not archive.is_symlink()
            )
            if valid:
                try:
                    valid = (
                        self._git_text(archive, "rev-parse", "HEAD")
                        == row.get("head")
                        and self._file_sha256(self._git_path(archive, "config"))
                        == row.get("git_config_sha256")
                        and hashlib.sha256(self._status(archive)).hexdigest()
                        == row.get("status_sha256")
                    )
                except WorkspaceError:
                    valid = False
            if not valid:
                result["refusals"].append(
                    self._refusal(
                        "source_archive_audit_failed",
                        f"archived source no longer matches its record: {archive}",
                        "restore the recorded source archive without deleting either copy",
                    )
                )
        archives = {
            row.get("component"): Path(str(row.get("archive", "")))
            for row in record["sources"]
            if isinstance(row, dict) and row.get("status") == "archived"
        }
        for row in record["worktree_migrations"]:
            if not isinstance(row, dict):
                result["refusals"].append(
                    self._refusal(
                        "invalid_migration_record",
                        "record contains an invalid worktree migration entry",
                        "restore the exact migration record",
                    )
                )
                continue
            destination_value = row.get("destination")
            if destination_value:
                destination = Path(str(destination_value))
                valid = destination.is_dir() and not destination.is_symlink()
                if valid:
                    try:
                        valid = (
                            classic_common is not None
                            and self._git_common_directory(destination)
                            == classic_common
                            and self._git_optional_text(
                                destination,
                                "symbolic-ref",
                                "--quiet",
                                "--short",
                                "HEAD",
                            )
                            == row.get("branch_ref")
                            and hashlib.sha256(self._status(destination)).hexdigest()
                            == row.get("destination_status_sha256")
                            and self._stable_module_state_sha256(
                                destination,
                                str(row.get("source_component", "")),
                            )
                            == row.get("module_sha256")
                        )
                    except WorkspaceError:
                        valid = False
                if not valid:
                    result["refusals"].append(
                        self._refusal(
                            "migrated_worktree_audit_failed",
                            f"migrated classic worktree no longer matches its record: "
                            f"{destination}",
                            "preserve the worktree and restore its recorded branch and state",
                        )
                    )
            archive = archives.get(row.get("component"))
            original = (
                archive
                if row.get("primary") and archive is not None
                else Path(str(row.get("path", "")))
            )
            valid = original.is_dir() and not original.is_symlink()
            if valid:
                try:
                    status_value, index_sha256, snapshot_sha256 = (
                        self._snapshot_worktree(original)
                    )
                    valid = (
                        self._git_text(original, "rev-parse", "HEAD")
                        == row.get("head")
                        and self._git_optional_text(
                            original,
                            "symbolic-ref",
                            "--quiet",
                            "--short",
                            "HEAD",
                        )
                        == row.get("branch")
                        and self._worktree_lock(original) == row.get("locked")
                        and hashlib.sha256(status_value).hexdigest()
                        == row.get("status_sha256")
                        and index_sha256 == row.get("index_sha256")
                        and snapshot_sha256 == row.get("snapshot_sha256")
                        and self._stable_module_state_sha256(original)
                        == row.get("module_sha256")
                        and (
                            row.get("primary")
                            or archive is not None
                            and self._git_common_directory(original)
                            == self._git_common_directory(archive)
                        )
                    )
                except WorkspaceError:
                    valid = False
            if not valid:
                result["refusals"].append(
                    self._refusal(
                        "original_worktree_audit_failed",
                        f"original worktree no longer matches its record: {original}",
                        "restore the worktree and its archived Git administration",
                    )
                )
        for row in record["composite_worktrees"]:
            if not isinstance(row, dict):
                result["refusals"].append(
                    self._refusal(
                        "invalid_migration_record",
                        "record contains an invalid composite worktree entry",
                        "restore the exact migration record",
                    )
                )
                continue
            destination = Path(str(row.get("destination", "")))
            valid = destination.is_dir() and not destination.is_symlink()
            if valid:
                try:
                    valid = (
                        classic_common is not None
                        and self._git_common_directory(destination) == classic_common
                        and self._git_optional_text(
                            destination,
                            "symbolic-ref",
                            "--quiet",
                            "--short",
                            "HEAD",
                        )
                        == row.get("branch_ref")
                        and hashlib.sha256(self._status(destination)).hexdigest()
                        == row.get("status_sha256")
                        and isinstance(row.get("module_states"), list)
                        and bool(row["module_states"])
                        and all(
                            isinstance(module, dict)
                            and isinstance(module.get("prefix"), str)
                            and self._stable_module_state_sha256(
                                destination,
                                module["prefix"],
                            )
                            == module.get("sha256")
                            for module in row["module_states"]
                        )
                    )
                except WorkspaceError:
                    valid = False
            if not valid:
                result["refusals"].append(
                    self._refusal(
                        "composite_worktree_audit_failed",
                        f"composite classic worktree no longer matches its record: "
                        f"{destination}",
                        "preserve the worktree and restore its recorded branch and state",
                    )
                )
        journal = record.get("journal", {})
        for raw in journal.get("profiles", []) if isinstance(journal, dict) else []:
            path = Path(str(raw.get("path", "")))
            try:
                expected = hashlib.sha256(self._decode_bytes(raw.get("after"))).hexdigest()
                actual = hashlib.sha256(path.read_bytes()).hexdigest()
            except (OSError, WorkspaceError):
                actual = ""
                expected = "missing"
            if actual != expected:
                result["refusals"].append(
                    self._refusal(
                        "profile_audit_failed",
                        f"migrated profile differs from its recorded schema-v3 form: {path}",
                        "review or restore the recorded classic profile",
                    )
                )
        result["refusals"].sort(key=lambda item: (item["code"], item["message"]))
        result["status"] = "incomplete" if result["refusals"] else "complete"
        return result

    # ------------------------------------------------------------------
    # Read-only inventories and helpers

    def _inert_inventory(self) -> tuple[list[dict[str, Any]], list[dict[str, str]]]:
        roots = {
            "build": Path(self.paths.builds),
            "content": self.repository_root / "content",
            "content-1x": self.repository_root / "content-1x",
            "scenarios": Path(self.paths.scenarios),
            "state": Path(self.paths.state),
            "topologies": Path(self.paths.topologies),
        }
        rows: list[dict[str, Any]] = []
        refusals: list[dict[str, str]] = []
        for name, path in sorted(roots.items()):
            if not path.exists() and not path.is_symlink():
                rows.append({"name": name, "path": str(path), "present": False, "status": "inert"})
                continue
            if path.is_symlink():
                refusals.append(
                    self._refusal(
                        "unsafe_inert_path",
                        f"inert ownership root is a symlink: {path}",
                        "restore the wrapper-managed path without moving its contents",
                    )
                )
                continue
            try:
                metadata = path.stat()
            except OSError as error:
                refusals.append(
                    self._refusal(
                        "unsafe_inert_path",
                        f"cannot inspect inert ownership root {path}: {error}",
                        "restore the wrapper-managed path",
                    )
                )
                continue
            rows.append(
                {
                    "identity": f"{metadata.st_dev}:{metadata.st_ino}",
                    "name": name,
                    "path": str(path),
                    "present": True,
                    "status": "inert",
                }
            )
        return rows, refusals

    def _topology_inventory(self) -> tuple[list[dict[str, Any]], list[dict[str, str]]]:
        rows: list[dict[str, Any]] = []
        refusals: list[dict[str, str]] = []
        if not self.paths.topologies.exists():
            return rows, refusals
        if self.paths.topologies.is_symlink() or not self.paths.topologies.is_dir():
            return rows, [
                self._refusal(
                    "invalid_topologies_directory",
                    f"topologies path is not a normal directory: {self.paths.topologies}",
                    "restore the wrapper-managed topologies directory",
                )
            ]
        for directory in sorted(self.paths.topologies.iterdir()):
            if not directory.is_dir() or directory.is_symlink():
                continue
            status_path = directory / "status.json"
            if not status_path.exists() and not status_path.is_symlink():
                continue
            if status_path.is_symlink() or not status_path.is_file():
                refusals.append(
                    self._refusal(
                        "unobservable_topology",
                        f"topology status is not a regular file: {status_path}",
                        "stop or repair the topology before migration",
                    )
                )
                continue
            try:
                status_value = load_json(status_path)
                required = {
                    "schema_version",
                    "name",
                    "profile",
                    "dependencies",
                    "state",
                    "build_root",
                    "resolved",
                    "endpoint",
                    "ready",
                    "started_at",
                    "stopped_at",
                    "supervisor",
                    "services",
                }
                if (
                    not isinstance(status_value, dict)
                    or status_value.get("schema_version") != 1
                    or status_value.get("name") != directory.name
                    or not required <= set(status_value)
                    or not set(status_value)
                    <= required | {"stack", "providers", "error"}
                    or not isinstance(status_value.get("profile"), str)
                    or not isinstance(status_value.get("dependencies"), list)
                    or not isinstance(status_value.get("resolved"), dict)
                    or not isinstance(status_value.get("ready"), bool)
                    or not isinstance(status_value.get("build_root"), str)
                    or not Path(status_value["build_root"]).is_absolute()
                    or status_value.get("state") is not None
                    and (
                        not isinstance(status_value.get("state"), str)
                        or not Path(status_value["state"]).is_absolute()
                    )
                    or not isinstance(status_value.get("services"), dict)
                ):
                    raise WorkspaceError("topology status has an unsupported shape")
                records = [("supervisor", status_value.get("supervisor"))]
                records.extend(
                    (f"service {name}", value)
                    for name, value in status_value["services"].items()
                )
                running_records: list[str] = []
                for label, record in records:
                    if (
                        not isinstance(record, dict)
                        or not isinstance(record.get("pid"), int)
                        or isinstance(record.get("pid"), bool)
                        or record["pid"] <= 0
                        or not isinstance(record.get("start_time"), str)
                        or not record["start_time"].isdigit()
                    ):
                        raise WorkspaceError(
                            f"topology {label} process record is invalid"
                        )
                    if label != "supervisor" and (
                        set(record)
                        != {"pid", "start_time", "status", "exit_code", "log", "cwd"}
                        or record.get("status") not in {"starting", "running", "exited"}
                        or not isinstance(record.get("log"), str)
                        or not Path(record["log"]).is_absolute()
                        or not isinstance(record.get("cwd"), str)
                        or not Path(record["cwd"]).is_absolute()
                    ):
                        raise WorkspaceError(f"topology {label} status is invalid")
                    if label == "supervisor" and set(record) != {"pid", "start_time"}:
                        raise WorkspaceError("topology supervisor status is invalid")
                    if process_matches(record["pid"], record["start_time"]):
                        running_records.append(label)
            except (OSError, WorkspaceError) as error:
                refusals.append(
                    self._refusal(
                        "unobservable_topology",
                        f"cannot inspect topology {directory.name}: {error}",
                        "stop or repair the topology before migration",
                    )
                )
                continue
            running = bool(running_records)
            rows.append(
                {
                    "name": directory.name,
                    "path": str(directory),
                    "processes": running_records,
                    "running": running,
                    "status": "blocked" if running else "inert",
                }
            )
            if running:
                refusals.append(
                    self._refusal(
                        "live_topology",
                        f"topology is active: {directory.name}",
                        f"run ./atrinik down {directory.name} before migration",
                    )
                )
        return rows, refusals

    def _classic_checkout_contract(self) -> tuple[str, str]:
        by_checkout = getattr(self.manifest, "by_checkout", {})
        checkout = by_checkout.get("classic") if isinstance(by_checkout, dict) else None
        if checkout is not None:
            return str(checkout.repository), str(getattr(checkout, "path", "classic"))
        component = self._component("classic-client")
        return str(component.repository), str(getattr(component, "checkout", "classic"))

    def _replacement_checkout_occupies_source_path(
        self, source_path: Path, archive: Path
    ) -> bool:
        """Recognize a manifest-owned replacement at a vacated classic path."""

        if source_path.is_symlink() or not source_path.is_dir():
            return False
        by_checkout = getattr(self.manifest, "by_checkout", {})
        if not isinstance(by_checkout, dict):
            return False
        checkout = next(
            (
                value
                for value in by_checkout.values()
                if getattr(value, "generation", None) == "replacement"
                and self.repository_root / str(getattr(value, "path", ""))
                == source_path
            ),
            None,
        )
        if checkout is None:
            return False
        canonical = str(getattr(checkout, "name", ""))
        if canonical not in {value[0] for value in CLASSIC_SOURCES}:
            return False
        if self.repository_root / canonical != source_path:
            return False
        repository = str(getattr(checkout, "repository", ""))
        if not self._effective_repository_remote_matches(source_path, repository):
            return False
        try:
            top_level = Path(
                self._git_text(source_path, "rev-parse", "--show-toplevel")
            ).resolve()
            if top_level != source_path.resolve():
                return False
            if self._classic_lineage(source_path, canonical):
                return False
            return self._git_common_directory(source_path) != self._git_common_directory(
                archive
            )
        except WorkspaceError:
            return False

    def _archive_destination(self, source: Path) -> Path:
        preferred = self.archive_root / "repositories" / source.name
        try:
            source_device = source.parent.stat().st_dev
            workspace_device = self.workspace.stat().st_dev
        except OSError as error:
            raise WorkspaceError(
                f"cannot determine migration archive filesystem: {error}"
            ) from error
        if source_device == workspace_device:
            return preferred
        # The wrapper-local workspace directory is ignored by repository
        # policy and lives beside every canonical source checkout.  Use it as
        # the preservation root when ATRINIK_WORKSPACE_DIR is on another
        # filesystem so the final no-replace rename remains atomic.
        local = (
            self.repository_root
            / "workspace"
            / "archive"
            / "classic-migration"
            / "repositories"
            / source.name
        )
        try:
            if self.repository_root.stat().st_dev != source_device:
                raise WorkspaceError(
                    f"no same-filesystem archive root is available for {source}"
                )
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect wrapper-local archive filesystem: {error}"
            ) from error
        return local

    def _classic_profile_component_names(self) -> set[str]:
        try:
            stack = self.manifest.stacks["classic"]
            return {component.name for component in stack.components}
        except (AttributeError, KeyError, TypeError):
            return {
                "classic-client",
                "classic-server",
                "classic-editor",
                "classic-libatrinik",
                "classic-protocol",
                "content-1x",
                "playtester",
                "tools",
                "sound",
                "resources",
                "metaserver-worker",
                "devcontainer",
                "github-settings",
            }

    def _component(self, name: str) -> Any:
        try:
            return self.manifest.by_name[name]
        except (AttributeError, KeyError) as error:
            raise WorkspaceError(f"manifest lacks migration component {name}") from error

    def _repository_identity(
        self, path: Path, expected: set[str]
    ) -> tuple[str, str | None, str | None, str | None]:
        if path.is_symlink() or not path.is_dir():
            return "missing", None, None, "path is not a normal directory"
        for remote in ("origin", "upstream"):
            process = self._git_process(path, "remote", "get-url", "--all", remote)
            if process.returncode:
                continue
            for raw_url in process.stdout.decode(errors="replace").splitlines():
                repository = self._repository_from_url(raw_url.strip())
                if repository in expected:
                    return "expected", remote, raw_url.strip(), None
        coordinates = sorted(self._repository_coordinates(path))
        return (
            "wrong",
            None,
            None,
            "no identifying origin/upstream URL"
            + (f" (found {', '.join(coordinates)})" if coordinates else ""),
        )

    def _effective_repository_remote_matches(
        self, path: Path, expected: str
    ) -> bool:
        """Match only the first URL Git fetches for origin or upstream."""

        expected = expected.lower()
        for remote in ("origin", "upstream"):
            process = self._git_process(path, "remote", "get-url", "--all", remote)
            if process.returncode:
                continue
            urls = process.stdout.decode(errors="replace").splitlines()
            if urls and self._repository_from_url(urls[0].strip()) == expected:
                return True
        return False

    def _repository_coordinates(self, path: Path) -> set[str]:
        result: set[str] = set()
        process = self._git_process(path, "remote", "-v")
        if process.returncode:
            return result
        for line in process.stdout.decode(errors="replace").splitlines():
            fields = line.split()
            if len(fields) >= 2:
                repository = self._repository_from_url(fields[1])
                if repository:
                    result.add(repository)
        return result

    def _remote_inventory(self, path: Path) -> list[dict[str, Any]]:
        try:
            names = self._git_bytes(path, "remote").decode("ascii").splitlines()
        except UnicodeDecodeError as error:
            raise WorkspaceError(f"Git remote name is not ASCII in {path}") from error
        rows: list[dict[str, Any]] = []
        for name in sorted(names):
            if not name or "\0" in name or "\n" in name:
                raise WorkspaceError(f"invalid Git remote name in {path}")
            fetch = self._git_bytes(
                path,
                "remote",
                "get-url",
                "--all",
                name,
            ).decode(errors="surrogateescape").splitlines()
            push_process = self._git_process(
                path,
                "remote",
                "get-url",
                "--push",
                "--all",
                name,
            )
            push = (
                push_process.stdout.decode(errors="surrogateescape").splitlines()
                if push_process.returncode == 0
                else []
            )
            rows.append({"name": name, "fetch_urls": fetch, "push_urls": push})
        return rows

    @staticmethod
    def _file_sha256(path: Path) -> str:
        digest = hashlib.sha256()
        try:
            with path.open("rb") as stream:
                while chunk := stream.read(1024 * 1024):
                    digest.update(chunk)
        except OSError as error:
            raise WorkspaceError(f"cannot hash file {path}: {error}") from error
        return digest.hexdigest()

    def _classic_lineage(self, path: Path, canonical: str) -> bool:
        return classic_lineage(path, canonical)

    @staticmethod
    def _repository_from_url(url: str) -> str | None:
        match = re.search(
            r"(?:github\.com[/:])(?P<repository>atrinik/[a-z0-9._-]+?)(?:\.git)?/?$",
            url,
            re.IGNORECASE,
        )
        return match.group("repository").lower() if match else None

    def _component_tree_matches(
        self, source: Path, source_head: str, classic: Path, base: str, prefix: str
    ) -> bool:
        source_tree = self._git_text(source, "rev-parse", f"{source_head}^{{tree}}")
        classic_tree = self._git_optional_text(classic, "rev-parse", f"{base}:{prefix}")
        return classic_tree == source_tree

    @staticmethod
    def _old_worktree_label(source_name: str, path: Path, primary: bool) -> str:
        if primary:
            return "primary"
        return path.name or source_name

    @staticmethod
    def _migration_label(
        canonical: str, old_label: str, head: str, used: set[str]
    ) -> str:
        slug = re.sub(r"[^a-z0-9._-]+", "-", old_label.lower()).strip("-.")
        slug = slug or "worktree"
        base = f"migrated-{canonical}-{slug}-{head[:8]}"[:96].rstrip("-.")
        label = base
        counter = 2
        while label in used:
            label = f"{base}-{counter}"
            counter += 1
        used.add(label)
        return label

    @staticmethod
    def _archive_ref(canonical: str, label: str, head: str) -> str:
        return (
            "refs/heads/archive/local-migration/"
            f"{canonical}/{label}-{head[:12]}"
        )

    def _git_path(self, path: Path, name: str) -> Path:
        raw = self._git_text(path, "rev-parse", "--git-path", name)
        candidate = Path(raw)
        return candidate if candidate.is_absolute() else (path / candidate).resolve()

    def _git_common_directory(self, path: Path) -> Path:
        raw = self._git_text(path, "rev-parse", "--git-common-dir")
        candidate = Path(raw)
        return (
            candidate.resolve()
            if candidate.is_absolute()
            else (path / candidate).resolve()
        )

    def _git_activity(self, source: Path) -> list[str]:
        result: list[str] = []
        for name in OPERATION_PATHS:
            path = self._git_path(source, name)
            if path.exists() or path.is_symlink():
                result.append(name)
        return result

    def _is_primary_worktree(self, path: Path) -> bool:
        git_dir = Path(self._git_text(path, "rev-parse", "--absolute-git-dir"))
        common = Path(self._git_text(path, "rev-parse", "--git-common-dir"))
        if not common.is_absolute():
            common = (path / common).resolve()
        return git_dir.resolve() == common.resolve()

    @staticmethod
    def _parse_worktree_records(output: bytes) -> list[dict[str, bytes]]:
        records: list[dict[str, bytes]] = []
        current: dict[str, bytes] = {}
        for entry in output.split(b"\0"):
            if not entry:
                if current:
                    records.append(current)
                    current = {}
                continue
            key, separator, value = entry.partition(b" ")
            current[key.decode("ascii")] = value if separator else b""
        if current:
            records.append(current)
        return records

    def _worktree_lock(self, path: Path) -> str | None:
        records = self._parse_worktree_records(
            self._git_bytes(path, "worktree", "list", "--porcelain", "-z")
        )
        resolved = path.resolve()
        for record in records:
            raw_path = record.get("worktree")
            if raw_path is None or Path(os.fsdecode(raw_path)).resolve() != resolved:
                continue
            value = record.get("locked")
            return os.fsdecode(value) if value is not None else None
        raise WorkspaceError(f"worktree is absent from its common Git registry: {path}")

    @staticmethod
    def _safe_relative(value: str) -> Path:
        pure = PurePosixPath(value)
        if (
            pure.is_absolute()
            or not pure.parts
            or any(part in {"", ".", ".."} for part in pure.parts)
        ):
            raise WorkspaceError(f"unsafe Git path: {value!r}")
        return Path(*pure.parts)

    @staticmethod
    def _prefixed_status(value: bytes, prefix: str) -> bytes:
        output = bytearray()
        for record in value.split(b"\0"):
            if not record:
                continue
            if len(record) < 4 or record[2:3] != b" ":
                raise WorkspaceError("unsupported porcelain status record")
            output.extend(record[:3])
            output.extend(os.fsencode(prefix))
            output.extend(b"/")
            output.extend(record[3:])
            output.append(0)
        return bytes(output)

    def _snapshot_worktree(self, path: Path) -> tuple[bytes, str, str]:
        first = self._snapshot_worktree_once(path)
        second = self._snapshot_worktree_once(path)
        if first != second:
            raise WorkspaceError(f"worktree changed while it was inspected: {path}")
        return first

    def _stable_module_state_sha256(
        self,
        path: Path,
        prefix: str | None = None,
    ) -> str:
        first = self._module_state_sha256(path, prefix)
        second = self._module_state_sha256(path, prefix)
        if first != second:
            raise WorkspaceError(f"module state changed while it was inspected: {path}")
        return first

    def _module_state_sha256(
        self,
        path: Path,
        prefix: str | None,
    ) -> str:
        pathspec = ("--", prefix) if prefix is not None else ()
        index_rows = self._git_bytes(
            path,
            "ls-files",
            "--stage",
            "-z",
            *pathspec,
        ).split(b"\0")
        listed = self._git_bytes(
            path,
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            *pathspec,
        ).split(b"\0")
        prefix_bytes = os.fsencode(prefix) + b"/" if prefix is not None else b""

        def normalize(raw: bytes) -> bytes:
            if not prefix_bytes:
                return raw
            if not raw.startswith(prefix_bytes) or len(raw) == len(prefix_bytes):
                raise WorkspaceError(
                    f"Git returned a path outside module {prefix}: {os.fsdecode(raw)}"
                )
            return raw[len(prefix_bytes) :]

        digest = hashlib.sha256()

        def include(value: bytes) -> None:
            digest.update(len(value).to_bytes(8, "big"))
            digest.update(value)

        include(b"index")
        for row in index_rows:
            if not row:
                continue
            metadata, separator, raw = row.partition(b"\t")
            if not separator:
                raise WorkspaceError(f"invalid staged index row in {path}")
            include(metadata)
            include(normalize(raw))
        include(b"working")
        for raw in listed:
            if not raw:
                continue
            relative_raw = normalize(raw)
            include(relative_raw)
            candidate = path / self._safe_relative(os.fsdecode(raw))
            try:
                before = candidate.lstat()
            except FileNotFoundError:
                include(b"absent")
                continue
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect module entry {candidate}: {error}"
                ) from error
            include(f"{stat.S_IMODE(before.st_mode):o}".encode("ascii"))
            if stat.S_ISLNK(before.st_mode):
                try:
                    include(b"symlink")
                    include(os.fsencode(os.readlink(candidate)))
                    after = candidate.lstat()
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot snapshot module symlink {candidate}: {error}"
                    ) from error
            elif stat.S_ISREG(before.st_mode):
                include(b"file")
                content = hashlib.sha256()
                try:
                    with candidate.open("rb") as stream:
                        while chunk := stream.read(1024 * 1024):
                            content.update(chunk)
                    after = candidate.lstat()
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot snapshot module file {candidate}: {error}"
                    ) from error
                include(content.digest())
            elif stat.S_ISDIR(before.st_mode):
                raise WorkspaceError(
                    f"unsupported directory or submodule entry: {candidate}"
                )
            else:
                raise WorkspaceError(f"unsupported module entry: {candidate}")
            if any(
                getattr(before, field) != getattr(after, field)
                for field in (
                    "st_dev",
                    "st_ino",
                    "st_mode",
                    "st_size",
                    "st_mtime_ns",
                    "st_ctime_ns",
                )
            ):
                raise WorkspaceError(
                    f"module entry changed while it was inspected: {candidate}"
                )
        return digest.hexdigest()

    def _snapshot_worktree_once(self, path: Path) -> tuple[bytes, str, str]:
        status_before = self._status(path)
        index = self._git_path(path, "index")
        try:
            index_before = index.read_bytes()
        except OSError as error:
            raise WorkspaceError(f"cannot snapshot index for {path}: {error}") from error
        listed = self._git_bytes(
            path,
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
        ).split(b"\0")
        digest = hashlib.sha256()

        def include(value: bytes) -> None:
            digest.update(len(value).to_bytes(8, "big"))
            digest.update(value)

        include(index_before)
        for raw in listed:
            if not raw:
                continue
            relative = self._safe_relative(os.fsdecode(raw))
            include(raw)
            candidate = path / relative
            try:
                before = candidate.lstat()
            except FileNotFoundError:
                include(b"absent")
                continue
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect worktree entry {candidate}: {error}"
                ) from error
            include(f"{stat.S_IMODE(before.st_mode):o}".encode("ascii"))
            if stat.S_ISLNK(before.st_mode):
                try:
                    include(b"symlink")
                    include(os.fsencode(os.readlink(candidate)))
                    after = candidate.lstat()
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot snapshot symlink {candidate}: {error}"
                    ) from error
            elif stat.S_ISREG(before.st_mode):
                include(b"file")
                file_digest = hashlib.sha256()
                try:
                    with candidate.open("rb") as stream:
                        while chunk := stream.read(1024 * 1024):
                            file_digest.update(chunk)
                    after = candidate.lstat()
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot snapshot file {candidate}: {error}"
                    ) from error
                include(file_digest.digest())
            elif stat.S_ISDIR(before.st_mode):
                raise WorkspaceError(
                    f"unsupported directory or submodule entry: {candidate}"
                )
            else:
                raise WorkspaceError(f"unsupported worktree entry: {candidate}")
            stable_fields = (
                "st_dev",
                "st_ino",
                "st_mode",
                "st_size",
                "st_mtime_ns",
                "st_ctime_ns",
            )
            if any(getattr(before, field) != getattr(after, field) for field in stable_fields):
                raise WorkspaceError(
                    f"worktree entry changed while it was inspected: {candidate}"
                )
        status_after = self._status(path)
        try:
            index_after = index.read_bytes()
        except OSError as error:
            raise WorkspaceError(f"cannot recheck index for {path}: {error}") from error
        if status_before != status_after or index_before != index_after:
            raise WorkspaceError(f"worktree changed while it was inspected: {path}")
        return (
            status_before,
            hashlib.sha256(index_before).hexdigest(),
            digest.hexdigest(),
        )

    def _status(self, path: Path) -> bytes:
        return self._git_bytes(
            path,
            "status",
            "--porcelain=v1",
            "-z",
            "--untracked-files=all",
            "--no-renames",
        )

    @staticmethod
    def _remove_generated_path(path: Path) -> None:
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)

    @staticmethod
    def _atomic_bytes(path: Path, value: bytes) -> None:
        descriptor, name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
        temporary = Path(name)
        try:
            mode = stat.S_IMODE(path.stat().st_mode)
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(value)
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(temporary, mode)
            os.replace(temporary, path)
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise

    def _durable_atomic_json(self, path: Path, value: Any) -> None:
        atomic_json(path, value)
        self._fsync_directory(path.parent)

    @staticmethod
    def _fsync_directory(path: Path) -> None:
        flags = os.O_RDONLY | os.O_CLOEXEC
        if hasattr(os, "O_DIRECTORY"):
            flags |= os.O_DIRECTORY
        try:
            descriptor = os.open(path, flags)
        except OSError as error:
            raise WorkspaceError(f"cannot open directory for fsync {path}: {error}") from error
        try:
            os.fsync(descriptor)
        except OSError as error:
            raise WorkspaceError(f"cannot fsync directory {path}: {error}") from error
        finally:
            os.close(descriptor)

    @staticmethod
    def _encode_bytes(value: bytes) -> str:
        return base64.b64encode(value).decode("ascii")

    @staticmethod
    def _decode_bytes(value: Any) -> bytes:
        if not isinstance(value, str):
            raise WorkspaceError("journal byte field is invalid")
        try:
            return base64.b64decode(value, validate=True)
        except (ValueError, base64.binascii.Error) as error:
            raise WorkspaceError("journal byte field is invalid") from error

    @staticmethod
    def _json_bytes(value: Any) -> bytes:
        return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()

    @staticmethod
    def _refusal(code: str, message: str, recovery: str) -> dict[str, str]:
        return {"code": code, "message": message, "recovery": recovery}

    @staticmethod
    def _with_refusal(
        plan: dict[str, Any], code: str, message: str, recovery: str
    ) -> dict[str, Any]:
        result = copy.deepcopy(plan)
        result["refusals"].append(
            {"code": code, "message": message, "recovery": recovery}
        )
        result["refusals"].sort(key=lambda item: (item["code"], item["message"]))
        result["status"] = "refused"
        return result

    def _empty_audit_refusal(
        self, code: str, message: str, recovery: str
    ) -> dict[str, Any]:
        return {
            "classic": {},
            "composite_worktrees": [],
            "inert_paths": [],
            "migration": MIGRATION_NAME,
            "profile_rewrites": [],
            "refusals": [self._refusal(code, message, recovery)],
            "schema_version": PLAN_SCHEMA_VERSION,
            "sources": [],
            "status": "incomplete",
            "topologies": [],
            "worktree_migrations": [],
        }

    @staticmethod
    def _rename_no_replace(source: Path, destination: Path) -> None:
        rename_no_replace(source, destination)

    @staticmethod
    def _is_ancestor(path: Path, ancestor: str, descendant: str) -> bool:
        process = subprocess.run(
            ["git", "-C", str(path), "merge-base", "--is-ancestor", ancestor, descendant],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if process.returncode == 0:
            return True
        if process.returncode == 1:
            return False
        raise WorkspaceError("cannot inspect classic commit ancestry")

    @staticmethod
    def _git_process(path: Path, *arguments: str) -> subprocess.CompletedProcess[bytes]:
        environment = os.environ.copy()
        environment["GIT_OPTIONAL_LOCKS"] = "0"
        environment["GIT_NO_REPLACE_OBJECTS"] = "1"
        try:
            return subprocess.run(
                ["git", "-C", str(path), *arguments],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                env=environment,
            )
        except OSError as error:
            raise WorkspaceError(f"cannot run Git: {error}") from error

    def _git_bytes(self, path: Path, *arguments: str) -> bytes:
        process = self._git_process(path, *arguments)
        if process.returncode:
            raise WorkspaceError(
                f"Git command failed in {path}: git {' '.join(arguments)}: "
                f"{process.stderr.decode(errors='replace').strip()}"
            )
        return process.stdout

    def _git_text(self, path: Path, *arguments: str) -> str:
        return self._git_bytes(path, *arguments).decode().strip()

    def _git_optional_text(self, path: Path, *arguments: str) -> str | None:
        process = self._git_process(path, *arguments)
        return process.stdout.decode().strip() if process.returncode == 0 else None

    def _git_bytes_input(self, path: Path, value: bytes, *arguments: str) -> bytes:
        environment = os.environ.copy()
        environment["GIT_OPTIONAL_LOCKS"] = "0"
        return self._git_bytes_env(path, environment, *arguments, input_value=value)

    @staticmethod
    def _git_bytes_env(
        path: Path,
        environment: dict[str, str],
        *arguments: str,
        input_value: bytes | None = None,
    ) -> bytes:
        environment = environment.copy()
        environment["GIT_NO_REPLACE_OBJECTS"] = "1"
        try:
            process = subprocess.run(
                ["git", "-C", str(path), *arguments],
                input=input_value,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                env=environment,
            )
        except OSError as error:
            raise WorkspaceError(f"cannot run Git: {error}") from error
        if process.returncode:
            raise WorkspaceError(
                f"Git command failed in {path}: git {' '.join(arguments)}: "
                f"{process.stderr.decode(errors='replace').strip()}"
            )
        return process.stdout


def migrate_repositories(
    repository_root: Path, workspace_paths: Any, manifest: Any, mode: str
) -> dict[str, Any]:
    """Convenience entry point for the workspace coordinator."""

    return RepositoryMigration(repository_root, workspace_paths, manifest).execute(mode)
