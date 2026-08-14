from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import ExitStack, contextmanager
from contextvars import copy_context
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import stat
import subprocess
import sys
from typing import Any, Iterable, Iterator

from .locking import LockBusyError, active_lock_fds
from .content_migration import CONTENT_MIGRATION_PENDING, CONTENT_MIGRATION_RECORD
from .migration import MIGRATION_PENDING, MIGRATION_RECORD, OPERATION_PATHS
from .model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
    WorkspaceError,
    atomic_json,
    durable_atomic_json,
    load_json,
    managed_remove,
)
from .process_tree import bound_lease_locked, control_socket_path, lease_locked
from .supervisor import process_matches
from .sound import PLAYTEST_MARKER
from .workspace import (
    BUILD_METADATA,
    BUILD_METADATA_SCHEMA_VERSION,
    CACHE_METADATA,
    WORKER_DEPENDENCY_METADATA,
    WORKER_DEPENDENCY_SCHEMA_VERSION,
    COMPILER_CACHE_MAX_SIZE,
    COMPILER_CACHE_PURPOSE,
    TEMPORARY_STATE_METADATA,
    TEMPORARY_STATE_SCHEMA_VERSION,
    _descriptor_mount_id,
    _owned_tree_tombstone_path,
    _remote_matches,
    exclusive_lock,
    remove_owned_tree,
)


CLEANUP_SCHEMA_VERSION = 1
WORKER_DEPENDENCY_CLEANUP_SCHEMA_VERSIONS = frozenset(
    {1, 2, 3, WORKER_DEPENDENCY_SCHEMA_VERSION}
)
DEFAULT_SCOPES = ("worktrees", "builds")
ALL_SCOPES = (
    *DEFAULT_SCOPES,
    "temporary-states",
    "npm-cache",
    "compiler-cache",
    "sound-cache",
)
SUPPORTED_SCOPES = (*ALL_SCOPES, "topologies")
BUILD_RETENTION_RECORD = "retention.json"
LEGACY_BUILD_METADATA_SCHEMA_VERSION = 1
LEGACY_BUILD_METADATA_KEYS = {
    "schema_version",
    "profile",
    "key",
    "purpose",
    "coordinates",
    "last_used_at",
}
BUILD_METADATA_KEYS = {*LEGACY_BUILD_METADATA_KEYS, "sound"}
BUILD_COORDINATE_KEYS = {
    "component",
    "checkout",
    "repository",
    "branch",
    "source",
    "checkout_path",
    "source_path",
    "head",
}
PROFILE_PURPOSE = re.compile(
    r"^profile:(?P<profile>[a-z0-9][a-z0-9._-]*):(?P<key>[0-9a-f]{12})$"
)
HEAD_PATTERN = re.compile(r"^[0-9a-f]{40,64}$")
SOUND_PRODUCER_LOCK = "atrinik-playtest-builds.lock"
SOUND_PRODUCER_LOCK_MARKER = "atrinik-sound-playtest-builds-v1\n"
HISTORICAL_PULL_BASE_BOUNDARIES = {
    (
        "atrinik/atrinik",
        "main",
        "master",
    ): "ee5ba2096c94bce0161629423d4962a966bc61d8",
}


def _command(path: Path, *arguments: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(path), *arguments],
            check=True,
            capture_output=True,
            text=True,
            pass_fds=active_lock_fds(),
        )
    except FileNotFoundError as error:
        raise WorkspaceError("required command not found: git") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.strip()
        suffix = f": {detail}" if detail else ""
        raise WorkspaceError(
            f"git command failed ({error.returncode}) at {path}{suffix}"
        ) from error
    return result.stdout.strip()


def _worktree_records(repository: Path) -> list[dict[str, str]]:
    output = _command(repository, "worktree", "list", "--porcelain", "-z")
    records: list[dict[str, str]] = []
    current: dict[str, str] = {}
    for field in output.split("\0"):
        if not field:
            if current:
                records.append(current)
                current = {}
            continue
        key, _, value = field.partition(" ")
        current[key] = value
    if current:
        records.append(current)
    return records


def _git_common_directory(path: Path) -> Path:
    return _git_directories(path)[0]


def _git_directory(path: Path) -> Path:
    return _git_directories(path)[1]


def _git_directories(path: Path) -> tuple[Path, Path]:
    values = _command(
        path,
        "rev-parse",
        "--path-format=absolute",
        "--git-common-dir",
        "--git-dir",
    ).splitlines()
    if len(values) != 2 or not all(values):
        raise WorkspaceError(f"git returned invalid directory metadata at {path}")
    return Path(values[0]).resolve(), Path(values[1]).resolve()


def _regular_text_identity(path: Path) -> tuple[str, tuple[int, int, int, int]]:
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags)
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1:
            raise WorkspaceError(f"Git worktree pointer is not a safe regular file: {path}")
        with os.fdopen(os.dup(descriptor), "r", encoding="utf-8") as stream:
            value = stream.read(4097)
        if len(value) > 4096:
            raise WorkspaceError(f"Git worktree pointer is oversized: {path}")
        return value, (
            metadata.st_dev,
            metadata.st_ino,
            metadata.st_ctime_ns,
            metadata.st_size,
        )
    finally:
        os.close(descriptor)


def _sound_worktree_identity(worktree: Path, common: Path) -> tuple[Any, ...]:
    """Bind a sound path to its exact primary or linked Git admin record."""
    common = common.resolve()
    reported_common, git_directory = _git_directories(worktree)
    if reported_common != common:
        raise WorkspaceError("sound worktree has an unexpected Git common directory")
    worktree_metadata = worktree.lstat()
    if not stat.S_ISDIR(worktree_metadata.st_mode) or stat.S_ISLNK(worktree_metadata.st_mode):
        raise WorkspaceError("sound worktree path is not a safe directory")
    pointer = worktree / ".git"
    if git_directory == common:
        pointer_metadata = pointer.lstat()
        if (
            stat.S_ISLNK(pointer_metadata.st_mode)
            or not stat.S_ISDIR(pointer_metadata.st_mode)
            or pointer.resolve() != common
        ):
            raise WorkspaceError("sound primary Git directory is invalid")
        return (
            "primary",
            worktree_metadata.st_dev,
            worktree_metadata.st_ino,
            pointer_metadata.st_dev,
            pointer_metadata.st_ino,
            str(common),
        )
    pointer_value, pointer_identity = _regular_text_identity(pointer)
    prefix = "gitdir: "
    if not pointer_value.endswith("\n") or not pointer_value.startswith(prefix):
        raise WorkspaceError("sound linked-worktree pointer is invalid")
    raw_git_directory = pointer_value.removeprefix(prefix).strip()
    pointer_git_directory = Path(raw_git_directory)
    if not pointer_git_directory.is_absolute():
        pointer_git_directory = pointer.parent / pointer_git_directory
    if pointer_git_directory.resolve() != git_directory:
        raise WorkspaceError("sound linked-worktree pointer changed identity")
    admin_metadata = git_directory.lstat()
    if (
        stat.S_ISLNK(admin_metadata.st_mode)
        or not stat.S_ISDIR(admin_metadata.st_mode)
        or git_directory.parent.resolve() != (common / "worktrees").resolve()
    ):
        raise WorkspaceError("sound linked-worktree Git directory is invalid")
    backlink_value, backlink_identity = _regular_text_identity(git_directory / "gitdir")
    backlink = Path(backlink_value.strip())
    if not backlink.is_absolute():
        backlink = git_directory / backlink
    if backlink.resolve() != pointer.resolve():
        raise WorkspaceError("sound linked-worktree Git backlink does not match its path")
    return (
        "linked",
        worktree_metadata.st_dev,
        worktree_metadata.st_ino,
        *pointer_identity,
        admin_metadata.st_dev,
        admin_metadata.st_ino,
        *backlink_identity,
        str(common),
        str(git_directory),
    )


def _sound_producer_lock_snapshot(
    worktree: Path,
) -> tuple[Path, tuple[int, int, int, int]]:
    path = _git_directory(worktree) / SOUND_PRODUCER_LOCK
    marker, identity = _regular_text_identity(path)
    if marker != SOUND_PRODUCER_LOCK_MARKER or path.lstat().st_uid != os.geteuid():
        raise WorkspaceError("sound producer cleanup lease marker is invalid")
    return path, identity


@contextmanager
def _exclusive_sound_producer_lease(
    worktree: Path,
    expected_identity: tuple[int, int, int, int],
) -> Iterator[None]:
    path = _git_directory(worktree) / SOUND_PRODUCER_LOCK
    flags = os.O_RDWR | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags)
    try:
        metadata = os.fstat(descriptor)
        identity = (
            metadata.st_dev,
            metadata.st_ino,
            metadata.st_ctime_ns,
            metadata.st_size,
        )
        if (
            not stat.S_ISREG(metadata.st_mode)
            or metadata.st_nlink != 1
            or metadata.st_uid != os.geteuid()
            or identity != expected_identity
        ):
            raise WorkspaceError("sound producer cleanup lease changed identity")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise WorkspaceError("sound playtest producer is already in use") from error
        os.lseek(descriptor, 0, os.SEEK_SET)
        marker = os.read(descriptor, len(SOUND_PRODUCER_LOCK_MARKER.encode()) + 1)
        try:
            path_metadata = path.lstat()
        except OSError as error:
            raise WorkspaceError("sound producer cleanup lease path changed") from error
        if (
            marker != SOUND_PRODUCER_LOCK_MARKER.encode()
            or (path_metadata.st_dev, path_metadata.st_ino)
            != (metadata.st_dev, metadata.st_ino)
        ):
            raise WorkspaceError("sound producer cleanup lease changed while locking")
        yield
    finally:
        os.close(descriptor)


def _parse_time(value: Any, context: str) -> datetime:
    if not isinstance(value, str) or not value:
        raise WorkspaceError(f"{context} must be a timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as error:
        raise WorkspaceError(f"{context} is not a valid timestamp") from error
    if parsed.tzinfo is None:
        raise WorkspaceError(f"{context} must include a UTC offset")
    return parsed.astimezone(timezone.utc)


def _workspace_owned(paths: Any) -> bool:
    marker = paths.marker
    if (
        not paths.workspace.is_dir()
        or paths.workspace.is_symlink()
        or not marker.is_file()
        or marker.is_symlink()
    ):
        return False
    try:
        return load_json(marker) == {"schema_version": SCHEMA_VERSION}
    except WorkspaceError:
        return False


def _path_relation(root: Path, path: Path) -> bool:
    try:
        root = root.resolve(strict=False)
        path = path.resolve(strict=False)
    except (OSError, RuntimeError):
        return True
    return root == path or root in path.parents


def _tree_usage(
    root: Path, excluded: Iterable[Path] = ()
) -> tuple[dict[tuple[int, int], int], datetime | None, str | None]:
    sizes: dict[tuple[int, int], int] = {}
    maximum: float | None = None
    try:
        root_value = os.fspath(root)
        excluded_values = tuple(excluded)
        if not excluded_values:
            stack = [root_value]
            while stack:
                raw_path = stack.pop()
                metadata = os.lstat(raw_path)
                key = (metadata.st_dev, metadata.st_ino)
                sizes.setdefault(key, metadata.st_blocks * 512)
                maximum = (
                    metadata.st_mtime
                    if maximum is None
                    else max(maximum, metadata.st_mtime)
                )
                if stat.S_ISDIR(metadata.st_mode) and not stat.S_ISLNK(
                    metadata.st_mode
                ):
                    with os.scandir(raw_path) as entries:
                        stack.extend(entry.path for entry in entries)
            observed = (
                datetime.fromtimestamp(maximum, timezone.utc)
                if maximum is not None
                else None
            )
            return sizes, observed, None
        resolved_root = root.resolve(strict=False)
        excluded_paths = {
            path.resolve(strict=False) for path in excluded_values
        }
        stack = [(root_value, resolved_root)]
        while stack:
            raw_path, normalized = stack.pop()
            if raw_path != root_value and normalized in excluded_paths:
                continue
            metadata = os.lstat(raw_path)
            key = (metadata.st_dev, metadata.st_ino)
            sizes.setdefault(key, metadata.st_blocks * 512)
            maximum = metadata.st_mtime if maximum is None else max(maximum, metadata.st_mtime)
            if stat.S_ISDIR(metadata.st_mode) and not stat.S_ISLNK(metadata.st_mode):
                with os.scandir(raw_path) as entries:
                    stack.extend(
                        (entry.path, normalized / entry.name) for entry in entries
                    )
    except (OSError, RuntimeError) as error:
        return {}, None, str(error)
    observed = (
        datetime.fromtimestamp(maximum, timezone.utc) if maximum is not None else None
    )
    return sizes, observed, None


def _temporary_tree_usage(
    root: Path,
) -> tuple[dict[tuple[int, int], int], datetime | None, str | None]:
    """Measure temporary state without following links or crossing mounts."""

    sizes: dict[tuple[int, int], int] = {}
    maximum: float | None = None
    root_fd: int | None = None
    try:
        root_fd = os.open(
            root,
            os.O_RDONLY | os.O_CLOEXEC | os.O_DIRECTORY | os.O_NOFOLLOW,
        )
        root_mount = _descriptor_mount_id(root_fd)
        visited: set[tuple[int, int, int | tuple[int, int]]] = set()

        def walk(descriptor: int, display: Path) -> None:
            nonlocal maximum
            directory = os.fstat(descriptor)
            coordinate = (directory.st_dev, directory.st_ino, root_mount)
            if coordinate in visited:
                raise WorkspaceError(
                    f"temporary state traversal encountered a cycle: {display}"
                )
            visited.add(coordinate)
            sizes.setdefault(
                (directory.st_dev, directory.st_ino), directory.st_blocks * 512
            )
            maximum = (
                directory.st_mtime
                if maximum is None
                else max(maximum, directory.st_mtime)
            )
            for name in os.listdir(descriptor):
                child = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
                sizes.setdefault(
                    (child.st_dev, child.st_ino), child.st_blocks * 512
                )
                maximum = (
                    child.st_mtime
                    if maximum is None
                    else max(maximum, child.st_mtime)
                )
                if stat.S_ISREG(child.st_mode):
                    flags = os.O_NOFOLLOW
                    if sys.platform == "linux":
                        flags |= os.O_PATH
                    else:
                        flags |= os.O_RDONLY | os.O_NONBLOCK
                    file_fd = os.open(name, flags, dir_fd=descriptor)
                    try:
                        opened = os.fstat(file_fd)
                        if (
                            (opened.st_dev, opened.st_ino)
                            != (child.st_dev, child.st_ino)
                            or _descriptor_mount_id(file_fd) != root_mount
                        ):
                            raise WorkspaceError(
                                "temporary state traversal encountered a mount: "
                                f"{display / name}"
                            )
                    finally:
                        os.close(file_fd)
                    continue
                if not stat.S_ISDIR(child.st_mode):
                    continue
                child_fd = os.open(
                    name,
                    os.O_RDONLY
                    | os.O_CLOEXEC
                    | os.O_DIRECTORY
                    | os.O_NOFOLLOW,
                    dir_fd=descriptor,
                )
                try:
                    opened = os.fstat(child_fd)
                    if (
                        (opened.st_dev, opened.st_ino)
                        != (child.st_dev, child.st_ino)
                        or _descriptor_mount_id(child_fd) != root_mount
                    ):
                        raise WorkspaceError(
                            f"temporary state traversal encountered a mount: "
                            f"{display / name}"
                        )
                    walk(child_fd, display / name)
                finally:
                    os.close(child_fd)

        walk(root_fd, root)
        observed = (
            datetime.fromtimestamp(maximum, timezone.utc)
            if maximum is not None
            else None
        )
        return sizes, observed, None
    except (OSError, RuntimeError, WorkspaceError) as error:
        observed = (
            datetime.fromtimestamp(maximum, timezone.utc)
            if maximum is not None
            else None
        )
        return sizes, observed, str(error)
    finally:
        if root_fd is not None:
            os.close(root_fd)


def _topology_tree_snapshot(
    root: Path,
) -> tuple[
    str | None,
    list[str],
    datetime | None,
    dict[tuple[int, int], int],
    str | None,
]:
    """Snapshot one topology tree without following links or special files."""

    rows: list[tuple[Any, ...]] = []
    paths: list[str] = []
    sizes: dict[tuple[int, int], int] = {}
    maximum: float | None = None
    parent_descriptor: int | None = None
    root_descriptor: int | None = None

    def record(
        metadata: os.stat_result,
        relative: str,
        display: Path,
        *,
        allow_runtime_state_link: bool = False,
    ) -> None:
        nonlocal maximum
        if metadata.st_dev != root_device:
            raise WorkspaceError(f"topology tree contains a mount: {display}")
        if not (
            stat.S_ISDIR(metadata.st_mode)
            or stat.S_ISREG(metadata.st_mode)
            or allow_runtime_state_link and stat.S_ISLNK(metadata.st_mode)
        ):
            raise WorkspaceError(f"topology tree contains a special file: {display}")
        if stat.S_ISREG(metadata.st_mode) and metadata.st_nlink != 1:
            raise WorkspaceError(f"topology tree contains a linked file: {display}")
        rows.append(
            (
                relative,
                metadata.st_dev,
                metadata.st_ino,
                stat.S_IFMT(metadata.st_mode),
                stat.S_IMODE(metadata.st_mode),
                metadata.st_nlink,
                metadata.st_size,
                metadata.st_blocks,
                metadata.st_mtime_ns,
                metadata.st_ctime_ns,
            )
        )
        paths.append(str(display))
        sizes.setdefault(
            (metadata.st_dev, metadata.st_ino), metadata.st_blocks * 512
        )
        maximum = (
            metadata.st_mtime
            if maximum is None
            else max(maximum, metadata.st_mtime)
        )

    def walk(descriptor: int, relative: str, display: Path) -> None:
        record(os.fstat(descriptor), relative, display)
        for name in sorted(os.listdir(descriptor)):
            child_display = display / name
            child_relative = name if relative == "." else f"{relative}/{name}"
            metadata = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
            if stat.S_ISLNK(metadata.st_mode):
                if not re.fullmatch(
                    r"generations/[0-9a-f]{64}/server/data", child_relative
                ):
                    raise WorkspaceError(
                        f"topology tree contains a symbolic link: {child_display}"
                    )
                record(
                    metadata,
                    child_relative,
                    child_display,
                    allow_runtime_state_link=True,
                )
                continue
            if stat.S_ISDIR(metadata.st_mode):
                child_descriptor = os.open(
                    name,
                    os.O_RDONLY
                    | os.O_DIRECTORY
                    | os.O_CLOEXEC
                    | getattr(os, "O_NOFOLLOW", 0),
                    dir_fd=descriptor,
                )
                try:
                    opened = os.fstat(child_descriptor)
                    if (opened.st_dev, opened.st_ino) != (
                        metadata.st_dev,
                        metadata.st_ino,
                    ):
                        raise WorkspaceError(
                            f"topology tree changed during inventory: {child_display}"
                        )
                    walk(child_descriptor, child_relative, child_display)
                finally:
                    os.close(child_descriptor)
            else:
                record(metadata, child_relative, child_display)

    try:
        parent_descriptor = os.open(
            root.parent,
            os.O_RDONLY
            | os.O_DIRECTORY
            | os.O_CLOEXEC
            | getattr(os, "O_NOFOLLOW", 0),
        )
        root_metadata = os.stat(
            root.name, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if not stat.S_ISDIR(root_metadata.st_mode) or stat.S_ISLNK(
            root_metadata.st_mode
        ):
            raise WorkspaceError("topology root is not a regular directory")
        root_device = root_metadata.st_dev
        root_descriptor = os.open(
            root.name,
            os.O_RDONLY
            | os.O_DIRECTORY
            | os.O_CLOEXEC
            | getattr(os, "O_NOFOLLOW", 0),
            dir_fd=parent_descriptor,
        )
        opened = os.fstat(root_descriptor)
        if (opened.st_dev, opened.st_ino) != (
            root_metadata.st_dev,
            root_metadata.st_ino,
        ):
            raise WorkspaceError(f"topology root changed during inventory: {root}")
        walk(root_descriptor, ".", root)
        retained = os.stat(
            root.name, dir_fd=parent_descriptor, follow_symlinks=False
        )
        if (retained.st_dev, retained.st_ino) != (
            opened.st_dev,
            opened.st_ino,
        ):
            raise WorkspaceError(f"topology root changed during inventory: {root}")
        payload = json.dumps(rows, separators=(",", ":"), ensure_ascii=True)
        observed = (
            datetime.fromtimestamp(maximum, timezone.utc)
            if maximum is not None
            else None
        )
        return (
            hashlib.sha256(payload.encode()).hexdigest(),
            sorted(paths),
            observed,
            sizes,
            None,
        )
    except (OSError, RuntimeError, WorkspaceError) as error:
        return None, sorted(paths), None, {}, str(error)
    finally:
        if root_descriptor is not None:
            os.close(root_descriptor)
        if parent_descriptor is not None:
            os.close(parent_descriptor)


def _listed_usage(root: Path, relative_paths: Iterable[str]) -> dict[tuple[int, int], int]:
    """Account for Git-listed paths without resolving and rewalking every file."""

    sizes: dict[tuple[int, int], int] = {}
    root_value = os.fspath(root)
    for relative in relative_paths:
        parts = relative.split("/")
        if (
            not relative
            or relative.startswith("/")
            or ".." in parts
            or "" in parts
        ):
            continue
        candidate = os.path.join(root_value, *parts)
        try:
            metadata = os.lstat(candidate)
        except OSError:
            continue
        if stat.S_ISDIR(metadata.st_mode) and not stat.S_ISLNK(metadata.st_mode):
            nested, _, error = _tree_usage(Path(candidate))
            if error is None:
                sizes.update(nested)
            continue
        sizes.setdefault((metadata.st_dev, metadata.st_ino), metadata.st_blocks * 512)
    return sizes


def _base_item(kind: str, owner: str, repository: str, path: Path) -> dict[str, Any]:
    return {
        "kind": kind,
        "owner": owner,
        "repository": repository,
        "path": str(path),
        "allocated_bytes": 0,
        "age_seconds": None,
        "age_basis": None,
        "disposition": "protected",
        "reasons": [],
        "references": {
            "profiles": [],
            "scenarios": [],
            "topologies": [],
            "migration": [],
            "retention": [],
        },
    }


class Cleanup:
    """Build and optionally execute a fail-closed workspace cleanup plan."""

    def __init__(self, workspace: Any):
        self.workspace = workspace
        self.paths = workspace.paths
        self.manifest = workspace.manifest
        self.now = datetime.now(timezone.utc)
        self._repositories: dict[str, Path] = {}
        self._wrapper_primary = self.paths.repository
        self._repository_cache: dict[
            tuple[str, str],
            tuple[Path, list[dict[str, str]], Path] | str,
        ] = {}
        self._github_cache: dict[
            tuple[str, str], tuple[list[dict[str, Any]] | None, str | None]
        ] = {}

    def execute(
        self,
        scopes: list[str],
        older_than_days: int,
        names: list[str],
        apply: bool,
    ) -> dict[str, Any]:
        selected_scopes = self._normalize_scopes(scopes)
        selected_names = self._normalize_names(names)
        if older_than_days < 0:
            raise WorkspaceError("--older-than must be zero or greater")
        if not apply:
            return self._plan(selected_scopes, older_than_days, selected_names, "dry-run")
        if not _workspace_owned(self.paths):
            raise WorkspaceError(
                f"workspace ownership marker is invalid: {self.paths.marker}"
            )
        report = self._plan(selected_scopes, older_than_days, selected_names, "apply")
        if report["summary"]["error_count"]:
            report["aborted"] = True
            return report
        targets = [
            item for item in report["items"] if item["disposition"] == "eligible"
        ]
        targets.sort(key=self._apply_order)
        mutated = False
        completed: set[tuple[str, str]] = set()
        report["completed_actions"] = []
        journal_path = (
            self.paths.workspace
            / "cleanup-journals"
            / f"{self.now.strftime('%Y%m%dT%H%M%S%fZ')}-{secrets.token_hex(6)}.json"
        )
        journal: dict[str, Any] = {
            "schema_version": 1,
            "started_at": self.now.isoformat(),
            "status": "in-progress",
            "targets": [
                {"kind": target["kind"], "path": target["path"]}
                for target in targets
            ],
            "completed": [],
        }
        durable_atomic_json(journal_path, journal)
        report["journal"] = str(journal_path)
        for target in targets:
            with ExitStack() as target_stack:
                if target["kind"] in {"worktree", "prunable-metadata"}:
                    owner = target["owner"]
                    checkout = self.manifest.by_checkout.get(owner)
                    primary = (
                        self.paths.repository
                        if owner == "atrinik"
                        else self.paths.repositories / checkout.path
                        if checkout is not None
                        else None
                    )
                    if primary is None:
                        target["disposition"] = "error"
                        target["reasons"] = ["unknown_worktree_owner"]
                        report["aborted"] = True
                        break
                    requests = [
                        self.workspace._lease_request(
                            "git-admin",
                            (
                                self.workspace._git_admin_coordinate(
                                    checkout, primary
                                )
                                if checkout is not None
                                else self.workspace._wrapper_git_admin_coordinate()
                            ),
                            "exclusive",
                            f"cleanup {target['kind']} {target['path']}",
                        )
                    ]
                    if target["kind"] == "worktree":
                        requests.extend(
                            [
                                self.workspace._lease_request(
                                    "registry",
                                    "physical-references",
                                    "exclusive",
                                    f"cleanup worktree {target['path']}",
                                ),
                                self.workspace._lease_request(
                                    "source",
                                    self.workspace._source_coordinate(
                                        owner, Path(target["path"])
                                    ),
                                    "exclusive",
                                    f"cleanup worktree {target['path']}",
                                ),
                                self.workspace._lease_request(
                                    "source",
                                    self.workspace._physical_source_coordinate(
                                        Path(target["path"])
                                    ),
                                    "exclusive",
                                    f"cleanup worktree {target['path']}",
                                ),
                            ]
                        )
                    try:
                        target_stack.enter_context(
                            self.workspace._resource_locks(
                                requests,
                                nonblocking=True,
                            )
                        )
                    except LockBusyError as error:
                        target["disposition"] = "skipped"
                        target["reasons"] = ["resource_busy"]
                        target["error"] = str(error)
                        continue
                elif target["kind"] == "topology":
                    try:
                        target_stack.enter_context(
                            self.workspace._resource_locks(
                                [
                                    self.workspace._lease_request(
                                        "topology",
                                        target["name"],
                                        "exclusive",
                                        f"cleanup topology {target['name']}",
                                    )
                                ],
                                nonblocking=True,
                            )
                        )
                    except LockBusyError as error:
                        target["disposition"] = "skipped"
                        target["reasons"] = ["resource_busy"]
                        target["error"] = str(error)
                        continue
                identity = (target["kind"], target["path"])
                if identity in completed:
                    continue
                try:
                    match = self._revalidate_target(
                        target,
                        older_than_days,
                        selected_scopes,
                        selected_names,
                    )
                except (OSError, RuntimeError, WorkspaceError) as error:
                    target["disposition"] = "error"
                    target["reasons"] = ["revalidation_error"]
                    target["error"] = str(error)
                    report["aborted"] = True
                    break
                if match is None or match["disposition"] != "eligible":
                    target["disposition"] = "error"
                    target["reasons"] = ["revalidation_failed"]
                    if match is not None:
                        target["revalidation"] = {
                            "disposition": match["disposition"],
                            "reasons": match["reasons"],
                        }
                        if "error" in match:
                            target["revalidation"]["error"] = match["error"]
                    report["aborted"] = True
                    break
                credited = {
                    key: target[key]
                    for key in ("allocated_bytes", "ignored_bytes", "ignored_paths")
                    if key in target
                }
                for key, value in match.items():
                    if not key.startswith("_"):
                        target[key] = value
                target.update(credited)
                report["mutation_attempted"] = True
                try:
                    self._remove(match, older_than_days)
                except LockBusyError as error:
                    target["disposition"] = "skipped"
                    target["reasons"] = ["resource_busy"]
                    target["error"] = str(error)
                    continue
                except (OSError, RuntimeError, WorkspaceError) as error:
                    target["disposition"] = "error"
                    target["reasons"] = ["removal_failed"]
                    target["error"] = str(error)
                    report["aborted"] = True
                    break
                target["disposition"] = "removed"
                target["reasons"] = ["removed"]
                mutated = True
                completed.add(identity)
                report["completed_actions"].append(
                    {"kind": target["kind"], "path": target["path"]}
                )
                journal["completed"] = list(report["completed_actions"])
                try:
                    durable_atomic_json(journal_path, journal)
                except (OSError, RuntimeError, WorkspaceError) as error:
                    report["aborted"] = True
                    report["journal_error"] = (
                        "cleanup removed the reported target but could not durably "
                        f"refresh its progress journal: {error}"
                    )
                    break
                if target["kind"] == "prunable-metadata":
                    for related in targets:
                        if (
                            related["kind"] == "prunable-metadata"
                            and related["owner"] == target["owner"]
                        ):
                            related["disposition"] = "removed"
                            related["reasons"] = ["removed"]
                            completed.add((related["kind"], related["path"]))
        report["mutated"] = mutated
        report.setdefault("mutation_attempted", False)
        report["summary"] = self._summary(report["items"])
        journal["status"] = "aborted" if report.get("aborted") else "complete"
        journal["finished_at"] = datetime.now(timezone.utc).isoformat()
        try:
            durable_atomic_json(journal_path, journal)
        except (OSError, RuntimeError, WorkspaceError) as error:
            report["aborted"] = True
            report.setdefault(
                "journal_error",
                "cleanup completed the reported actions but could not durably "
                f"finalize its journal: {error}",
            )
        return report

    @staticmethod
    def _normalize_scopes(scopes: list[str]) -> list[str]:
        requested = scopes or list(DEFAULT_SCOPES)
        if "all" in requested:
            requested = [*requested, *ALL_SCOPES]
        return [scope for scope in SUPPORTED_SCOPES if scope in set(requested)]

    def _normalize_names(self, names: list[str]) -> set[str] | None:
        if not names:
            return None
        selected: set[str] = set()
        unknown: list[str] = []
        for name in names:
            if name == "atrinik":
                selected.add(name)
            elif name in self.manifest.by_checkout:
                selected.add(name)
            elif name in self.manifest.by_name:
                selected.add(self.manifest.by_name[name].checkout_name)
            else:
                unknown.append(name)
        if unknown:
            raise WorkspaceError(
                f"unknown components or checkouts: {', '.join(sorted(set(unknown)))}"
            )
        return selected

    def _plan(
        self,
        scopes: list[str],
        older_than_days: int,
        names: set[str] | None,
        mode: str,
    ) -> dict[str, Any]:
        self._reset_inventory()
        runtime_only = bool(scopes) and set(scopes) <= {
            "topologies",
            "temporary-states",
        }
        if runtime_only:
            references = {
                "profiles": {},
                "scenarios": {},
                "topologies": {},
                "live_builds": {},
                "migration": {},
                "retention": {},
            }
            reference_errors: set[str] = set()
        else:
            references, reference_errors = self._references()
        items: list[dict[str, Any]] = []
        registered: set[Path] = set()
        if not runtime_only:
            registered, registered_error = self._registered_worktree_paths()
            if registered_error:
                reference_errors.add("worktree_inventory_error")
        if "worktrees" in scopes:
            worktrees, _ = self._worktrees(names, references, reference_errors)
            items.extend(worktrees)
            self._resolve_github(worktrees, older_than_days)
            self._protect_shared_prune_scope(worktrees)
        removable_worktrees: set[Path] = set()
        try:
            removable_worktrees = {
                Path(item["path"]).resolve(strict=False)
                for item in items
                if item["kind"] == "worktree" and item["disposition"] == "eligible"
            }
        except (OSError, RuntimeError):
            reference_errors.add("worktree_inventory_error")
        if "builds" in scopes:
            items.extend(
                self._builds(
                    older_than_days,
                    registered,
                    removable_worktrees,
                    references,
                    reference_errors,
                )
            )
            items.extend(self._unmanaged_builds(registered))
        if "temporary-states" in scopes and names is None:
            items.extend(self._temporary_states(older_than_days))
        if "npm-cache" in scopes:
            cache = self._npm_cache(older_than_days, references, reference_errors)
            if cache is not None:
                items.append(cache)
        if "compiler-cache" in scopes:
            cache = self._compiler_cache(
                older_than_days, references, reference_errors
            )
            if cache is not None:
                items.append(cache)
        if "sound-cache" in scopes and (names is None or "sound" in names):
            items.extend(
                self._sound_caches(older_than_days, reference_errors)
            )
        if "topologies" in scopes:
            items.extend(self._topologies(older_than_days))
        items.sort(key=lambda item: (item["kind"], item["owner"], item["path"]))
        self._credit_sizes(items)
        for item in items:
            self._strip_internal(item)
        summary = self._summary(items)
        summary["error_count"] += len(reference_errors)
        return {
            "schema_version": CLEANUP_SCHEMA_VERSION,
            "mode": mode,
            "scopes": scopes,
            "older_than_days": older_than_days,
            "filters": sorted(names or []),
            "inventory_errors": sorted(reference_errors),
            "items": items,
            "summary": summary,
        }

    def _reset_inventory(self) -> None:
        self.now = datetime.now(timezone.utc)
        self._repositories = {}
        self._repository_cache = {}
        self._github_cache = {}

    @staticmethod
    def _strip_internal(item: dict[str, Any]) -> None:
        item.pop("_inodes", None)
        item.pop("_primary", None)
        item.pop("_purpose", None)
        item.pop("_older_than_days", None)
        item.pop("_identity", None)
        item.pop("_worktree_identity", None)
        item.pop("_git_common", None)
        item.pop("_sound_worktree_identity", None)
        item.pop("_sound_producer_identity", None)
        item.pop("_physical_path", None)
        item.pop("_lease_only", None)

    def _revalidate_target(
        self,
        target: dict[str, Any],
        older_than_days: int,
        scopes: list[str],
        names: set[str] | None,
    ) -> dict[str, Any] | None:
        """Refresh one target's safety predicates without rebuilding report payloads."""

        self._reset_inventory()
        path = Path(target["path"])
        kind = target["kind"]
        if kind == "topology":
            item = self._topology_item(
                path,
                older_than_days,
            )
            if not self._same_topology_snapshot(target, item):
                raise WorkspaceError(
                    f"topology changed during apply revalidation: {target['name']}"
                )
            return item
        if kind == "temporary-state":
            item = next(
                (
                    candidate
                    for candidate in self._temporary_states(older_than_days)
                    if candidate["path"] == str(path)
                ),
                self._temporary_state_item(path, older_than_days),
            )
            filesystem_identity = item.get("_identity")
            physical = item.get("_physical_path")
            lease_only = item.get("_lease_only")
            self._strip_internal(item)
            item["_identity"] = filesystem_identity
            item["_physical_path"] = physical
            item["_lease_only"] = lease_only
            return item
        references, reference_errors = self._references()
        registered, registered_error = self._registered_worktree_paths()
        if registered_error:
            reference_errors.add("worktree_inventory_error")
        if kind == "profile-build":
            removable_worktrees = (
                self._revalidate_build_sources(
                    path,
                    older_than_days,
                    references,
                    reference_errors,
                    names,
                )
                if target.get("source_worktree_removal")
                and "worktrees" in scopes
                else set()
            )
            item = self._build_item(
                path,
                older_than_days,
                registered,
                removable_worktrees,
                references,
                reference_errors,
            )
        elif kind == "worker-dependencies":
            item = self._worker_dependency_item(
                path,
                older_than_days,
                registered,
                references,
                reference_errors,
            )
        elif kind == "worker-dependency-transaction":
            item = self._worker_dependency_transaction_item(path, older_than_days)
        elif kind in {"worktree", "prunable-metadata"}:
            item = self._revalidate_worktree(
                target["owner"],
                path,
                kind,
                older_than_days,
                references,
                reference_errors,
            )
        elif kind in {"npm-cache", "compiler-cache"}:
            cache_method = (
                self._npm_cache if kind == "npm-cache" else self._compiler_cache
            )
            item = cache_method(
                older_than_days,
                references,
                reference_errors,
            )
            if item is not None and item["path"] != target["path"]:
                item = None
        elif kind == "sound-cache":
            item = next(
                (
                    candidate
                    for candidate in self._sound_caches(
                        older_than_days, reference_errors
                    )
                    if candidate["path"] == target["path"]
                ),
                None,
            )
        else:
            raise WorkspaceError(f"unsupported cleanup target: {kind}")
        if item is not None:
            filesystem_identity = item.get("_identity")
            worktree_identity = item.get("_worktree_identity")
            git_common = item.get("_git_common")
            sound_worktree_identity = item.get("_sound_worktree_identity")
            sound_producer_identity = item.get("_sound_producer_identity")
            self._strip_internal(item)
            if kind in {
                "worker-dependency-transaction",
                "sound-cache",
                "temporary-state",
            }:
                item["_identity"] = filesystem_identity
            if kind == "sound-cache":
                item["_worktree_identity"] = worktree_identity
                item["_git_common"] = git_common
                item["_sound_producer_identity"] = sound_producer_identity
            if kind == "worktree" and target["owner"] == "sound":
                item["_sound_worktree_identity"] = sound_worktree_identity
                item["_sound_producer_identity"] = sound_producer_identity
        return item

    def _topologies(self, older_than_days: int) -> list[dict[str, Any]]:
        root = self.paths.topologies
        if not root.exists() and not root.is_symlink():
            return []
        if root.is_symlink() or not root.is_dir():
            item = self._base_topology_item(root)
            item["reasons"] = ["invalid_topology_container"]
            return [item]
        infrastructure = {"port-reservations", "ports.lock"}
        return [
            self._topology_item(path, older_than_days)
            for path in sorted(root.iterdir())
            if path.name not in infrastructure
        ]

    @staticmethod
    def _base_topology_item(path: Path) -> dict[str, Any]:
        item = _base_item("topology", "atrinik", "atrinik/atrinik", path)
        item.update(
            {
                "name": path.name,
                "liveness": "unverifiable",
                "control_observation": "unverifiable",
                "generation": None,
                "process_tree_lease": "unverifiable",
                "runtime_bundle_lease": "unverifiable",
                "port_reservation_lease": "unverifiable",
                "repository_layout_lease": "unverifiable",
                "age_observed_at": None,
                "deletion_paths": [],
                "tree_identity": None,
            }
        )
        return item

    def _topology_item(
        self,
        path: Path,
        older_than_days: int,
        *,
        check_operation: bool = True,
    ) -> dict[str, Any]:
        item = self._base_topology_item(path)
        reasons: list[str] = []
        try:
            if not self._owned_direct_child(path, self.paths.topologies):
                raise WorkspaceError("topology is not a direct managed child")
            marker = path / MANAGED_MARKER
            if (
                path.is_symlink()
                or not path.is_dir()
                or marker.is_symlink()
                or not marker.is_file()
                or load_json(marker)
                != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": f"topology:{path.name}",
                }
            ):
                raise WorkspaceError("topology ownership marker is invalid")
        except (OSError, RuntimeError, WorkspaceError) as error:
            item["reasons"] = ["invalid_topology_ownership"]
            item["error"] = str(error)
            return item

        (
            identity,
            deletion_paths,
            tree_time,
            inodes,
            tree_error,
        ) = _topology_tree_snapshot(path)
        item["tree_identity"] = identity
        item["deletion_paths"] = deletion_paths
        item["_inodes"] = inodes
        if tree_error is not None:
            reasons.append("invalid_topology_tree")
            item["error"] = tree_error

        temporary_container = path / "temporary-states"
        if temporary_container.exists() or temporary_container.is_symlink():
            try:
                if temporary_container.is_symlink() or not temporary_container.is_dir():
                    raise WorkspaceError("temporary state container is invalid")
                if any(
                    child.name != MANAGED_MARKER
                    for child in temporary_container.iterdir()
                ):
                    reasons.append("temporary_states_present")
            except (OSError, WorkspaceError) as error:
                reasons.append("temporary_state_inventory_unverifiable")
                item["error"] = str(error)

        if check_operation:
            operation_lock = path / "operation.lock"
            if not operation_lock.exists() and not operation_lock.is_symlink():
                reasons.append("topology_operation_lock_unavailable")
            else:
                busy, lock_error = self._lock_busy(operation_lock)
                if busy:
                    reasons.append("active_topology_operation")
                if lock_error:
                    reasons.append("invalid_topology_operation_lock")
                    item["error"] = lock_error

        try:
            status = self.workspace.topology_status(path.name)
            supervisor = status["supervisor"]
            services = status["services"].values()
            observed = [supervisor["liveness"], *(row["liveness"] for row in services)]
            if "live" in observed:
                liveness = "live"
            elif "unreachable" in observed:
                liveness = "unreachable"
            elif "stale" in observed:
                liveness = "stale"
            else:
                liveness = "exited"
            observation = status["observation"]
            item["liveness"] = liveness
            item["control_observation"] = observation["control"]
            item["generation"] = observation["generation"]
            item["process_tree_lease"] = observation["process_tree_lease"]
            item["runtime_bundle_lease"] = observation.get(
                "runtime_bundle_lease", "unverifiable"
            )
            if liveness == "live":
                reasons.append("live_topology")
            elif liveness == "unreachable":
                reasons.append("unreachable_topology")
            if observation["control"] == "reachable":
                reasons.append("reachable_topology_control")
            if observation["process_tree_lease"] == "retained":
                reasons.append("process_tree_lease_retained")
            elif observation["process_tree_lease"] != "released":
                reasons.append("process_tree_lease_unverifiable")
            if item["runtime_bundle_lease"] == "retained":
                reasons.append("runtime_bundle_lease_retained")
            elif item["runtime_bundle_lease"] not in {"released", "historical"}:
                reasons.append("runtime_bundle_lease_unverifiable")
            port = observation.get("port_reservation")
            if port is None:
                item["port_reservation_lease"] = "released"
            elif isinstance(port, dict) and port.get("lease") in {
                "released",
                "retained",
            }:
                item["port_reservation_lease"] = port["lease"]
            if item["port_reservation_lease"] == "retained":
                reasons.append("port_reservation_lease_retained")
            elif item["port_reservation_lease"] != "released":
                reasons.append("port_reservation_lease_unverifiable")
            if observation.get("repository_layout_lease_owner") is not None:
                reasons.append("repository_layout_lease_retained")
                item["repository_layout_lease"] = "retained"
            else:
                item["repository_layout_lease"] = "released"

            stopped_at = status.get("stopped_at")
            if stopped_at is not None:
                age_time = _parse_time(stopped_at, "topology stopped_at")
                item["age_basis"] = "stopped-at"
            elif liveness == "stale":
                age_time = tree_time
                item["age_basis"] = "legacy-tree-mtime" if tree_time else None
            else:
                age_time = None
                item["age_basis"] = None
                reasons.append("topology_stopped_at_unavailable")
            if age_time is None:
                reasons.append("topology_age_unavailable")
            else:
                item["age_observed_at"] = age_time.isoformat()
                age_seconds = int((self.now - age_time).total_seconds())
                item["age_seconds"] = max(0, age_seconds)
                if age_time > self.now:
                    reasons.append("future_topology_timestamp")
                elif age_seconds < older_than_days * 86400:
                    reasons.append("topology_younger_than_grace_period")
        except (OSError, RuntimeError, KeyError, WorkspaceError) as error:
            reasons.append("topology_status_unverifiable")
            item["error"] = str(error)

        if tree_error is None:
            (
                rechecked_identity,
                rechecked_paths,
                _rechecked_time,
                _rechecked_inodes,
                rechecked_error,
            ) = _topology_tree_snapshot(path)
            if (
                rechecked_error is not None
                or rechecked_identity != identity
                or rechecked_paths != deletion_paths
            ):
                reasons.append("topology_changed_during_inventory")
                item["error"] = rechecked_error or (
                    f"topology tree changed during inventory: {path.name}"
                )
                item["tree_identity"] = None

        item["reasons"] = sorted(set(reasons)) or ["inactive_topology"]
        item["disposition"] = "eligible" if not reasons else "protected"
        return item

    @staticmethod
    def _same_topology_snapshot(
        expected: dict[str, Any], current: dict[str, Any]
    ) -> bool:
        keys = (
            "disposition",
            "liveness",
            "control_observation",
            "generation",
            "process_tree_lease",
            "runtime_bundle_lease",
            "port_reservation_lease",
            "repository_layout_lease",
            "age_basis",
            "age_observed_at",
            "tree_identity",
            "deletion_paths",
        )
        return all(current.get(key) == expected.get(key) for key in keys)

    def _revalidate_build_sources(
        self,
        path: Path,
        older_than_days: int,
        references: dict[str, Any],
        reference_errors: set[str],
        names: set[str] | None,
    ) -> set[Path]:
        try:
            purpose, profile, key = self._profile_marker(path)
            probe = _base_item("profile-build", "atrinik", "atrinik/atrinik", path)
            probe.update(
                {"profile": profile, "key": key, "_purpose": purpose}
            )
            metadata = self._load_build_metadata(path / BUILD_METADATA, probe)
        except (OSError, RuntimeError, WorkspaceError):
            return set()
        removable: set[Path] = set()
        candidates = {
            (row["checkout"], Path(row["checkout_path"]).resolve(strict=False))
            for row in metadata["coordinates"].values()
            if names is None or row["checkout"] in names
        }
        for owner, candidate in sorted(candidates, key=lambda value: str(value[1])):
            item = self._revalidate_worktree(
                owner,
                candidate,
                "worktree",
                older_than_days,
                references,
                reference_errors,
            )
            if item is not None and item["disposition"] == "eligible":
                removable.add(candidate)
        return removable

    def _revalidate_worktree(
        self,
        owner: str,
        target: Path,
        kind: str,
        older_than_days: int,
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> dict[str, Any] | None:
        if owner == "atrinik":
            repository = "atrinik/atrinik"
            base = "main"
            invocation = self.paths.repository
            wrapper = True
        else:
            checkout = self.manifest.by_checkout.get(owner)
            if checkout is None:
                raise WorkspaceError(f"unknown cleanup worktree owner: {owner}")
            repository = checkout.repository
            base = checkout.branch
            invocation = self.paths.repositories / checkout.path
            wrapper = False
        self._repository_cache.pop((repository, str(invocation)), None)
        common, records, primary = self._repository_inventory(repository, invocation)
        self._repositories[owner] = primary
        allowed = (
            [
                self.paths.worktrees / "atrinik",
                primary / "build" / "worktrees",
            ]
            if wrapper
            else [self.paths.worktrees / owner]
        )
        normalized_target = target.resolve(strict=False)
        selected: list[dict[str, Any]] = []
        for row in records:
            raw = row.get("worktree")
            if raw is None:
                continue
            candidate = Path(raw)
            if kind == "prunable-metadata":
                if "prunable" not in row or candidate.exists():
                    continue
            elif candidate.resolve(strict=False) != normalized_target:
                continue
            selected.append(
                self._worktree_item(
                    owner,
                    repository,
                    base,
                    primary,
                    common,
                    allowed,
                    row,
                    references,
                    reference_errors,
                )
            )
        self._resolve_github(selected, older_than_days)
        if kind == "prunable-metadata":
            self._protect_shared_prune_scope(selected)
        return next(
            (
                item
                for item in selected
                if item["kind"] == kind
                and Path(item["path"]).resolve(strict=False) == normalized_target
            ),
            None,
        )

    def _references(self) -> tuple[dict[str, Any], set[str]]:
        references: dict[str, Any] = {
            "profiles": {},
            "scenarios": {},
            "topologies": {},
            "live_builds": {},
            "migration": {},
            "retention": {},
        }
        errors: set[str] = set()
        collectors = (
            (self._profile_references, "profile_inventory_error"),
            (self._scenario_references, "scenario_inventory_error"),
            (self._topology_references, "topology_inventory_error"),
            (self._migration_references, "migration_inventory_error"),
            (self._retention_references, "retention_inventory_error"),
        )
        for collector, reason in collectors:
            try:
                collector(references, errors)
            except (OSError, RuntimeError, WorkspaceError):
                errors.add(reason)
        return references, errors

    def _registered_worktree_paths(self) -> tuple[set[Path], bool]:
        registered: set[Path] = set()
        failed = False
        repositories: list[tuple[str, Path]] = [
            ("atrinik/atrinik", self.paths.repository)
        ]
        for checkout in self.manifest.checkouts:
            candidate = self.paths.repositories / checkout.path
            if candidate.exists() or candidate.is_symlink():
                repositories.append((checkout.repository, candidate))
        for repository, invocation in repositories:
            try:
                _, records, primary = self._repository_inventory(
                    repository, invocation
                )
                if repository == "atrinik/atrinik":
                    self._wrapper_primary = primary.resolve()
                registered.update(
                    Path(row["worktree"]).resolve(strict=False)
                    for row in records
                    if "worktree" in row
                )
            except (OSError, RuntimeError, WorkspaceError):
                failed = True
        return registered, failed

    def _repository_inventory(
        self, repository: str, invocation: Path
    ) -> tuple[Path, list[dict[str, str]], Path]:
        key = (repository, str(invocation))
        cached = self._repository_cache.get(key)
        if isinstance(cached, str):
            raise WorkspaceError(cached)
        if cached is not None:
            return cached
        try:
            common = _git_common_directory(invocation)
            records = _worktree_records(invocation)
            primary = None
            for row in records:
                candidate = Path(row.get("worktree", ""))
                if not candidate.is_dir() or candidate.is_symlink():
                    continue
                try:
                    if _git_directory(candidate) == common:
                        primary = candidate.resolve()
                        break
                except WorkspaceError:
                    continue
            if primary is None or not self._remote_identity(primary, repository):
                raise WorkspaceError("repository identity is unproven")
        except (OSError, RuntimeError, WorkspaceError) as error:
            detail = str(error)
            self._repository_cache[key] = detail
            raise WorkspaceError(detail) from error
        result = (common, records, primary)
        self._repository_cache[key] = result
        return result

    @staticmethod
    def _add_reference(container: dict[Path, list[str]], path: Path, name: str) -> None:
        try:
            normalized = path.resolve(strict=False)
        except (OSError, RuntimeError) as error:
            raise WorkspaceError(f"cannot resolve retained path {path}: {error}") from error
        container.setdefault(normalized, []).append(name)

    @staticmethod
    def _owned_direct_child(path: Path, namespace: Path) -> bool:
        """Prove a path is directly below one fixed non-symlink namespace."""

        try:
            if (
                namespace.is_symlink()
                or not namespace.is_dir()
                or namespace.parent.is_symlink()
                or not namespace.parent.is_dir()
            ):
                return False
            return path.resolve(strict=False).parent == namespace.resolve()
        except (OSError, RuntimeError):
            return False

    def _profile_references(self, references: dict[str, Any], errors: set[str]) -> None:
        try:
            for record in self.workspace._physical_reference_records():
                if (
                    not isinstance(record, dict)
                    or set(record)
                    != {"schema_version", "kind", "reference", "sources"}
                    or record["schema_version"] != 1
                    or record["kind"] not in {"profiles", "scenarios"}
                    or not isinstance(record["reference"], str)
                    or not isinstance(record["sources"], list)
                    or not all(
                        isinstance(source, str) and Path(source).is_absolute()
                        for source in record["sources"]
                    )
                ):
                    raise WorkspaceError("physical profile reference is invalid")
                for source in record["sources"]:
                    self._add_reference(
                        references[record["kind"]],
                        Path(source),
                        record["reference"],
                    )
        except (OSError, WorkspaceError):
            errors.add("profile_inventory_error")
        if not self.paths.profiles.is_dir() or self.paths.profiles.is_symlink():
            if self.paths.profiles.exists() or self.paths.profiles.is_symlink():
                errors.add("profile_inventory_error")
            return
        for path in sorted(self.paths.profiles.glob("*.json")):
            try:
                if path.is_symlink():
                    raise WorkspaceError("profile is a symlink")
                try:
                    profile = self.workspace._load_profile(
                        path.stem, require_file=True
                    )
                except WorkspaceError:
                    profile = load_json(path)
                    if (
                        not isinstance(profile, dict)
                        or profile.get("name") != path.stem
                        or profile.get("stack") != "classic"
                        or not isinstance(profile.get("components"), dict)
                        or "content-1x" not in profile["components"]
                        or "content" in profile["components"]
                        or set(profile["components"])
                        - set(self.manifest.by_name)
                        != {"content-1x"}
                    ):
                        raise
                for component_name, selector in profile["components"].items():
                    if (
                        component_name not in self.manifest.by_name
                        and component_name != "content-1x"
                    ) or not isinstance(selector, dict):
                        raise WorkspaceError("profile selector is invalid")
                    if set(selector) != {"kind", "value"} or not isinstance(
                        selector.get("value"), str
                    ):
                        raise WorkspaceError("profile selector is invalid")
                    checkout = (
                        "content-1x"
                        if component_name == "content-1x"
                        else self.manifest.by_name[component_name].checkout_name
                    )
                    kind = selector.get("kind")
                    if kind == "worktree":
                        selected = self.paths.worktrees / checkout / selector["value"]
                    elif kind == "path":
                        selected = Path(selector["value"])
                        if not selected.is_absolute():
                            raise WorkspaceError("profile path is relative")
                    elif kind == "migrated-worktree":
                        selected = Path(selector["value"])
                        if not selected.is_absolute():
                            raise WorkspaceError("migrated worktree path is relative")
                    elif kind == "primary":
                        continue
                    else:
                        raise WorkspaceError("profile selector kind is invalid")
                    self._add_reference(references["profiles"], selected, path.stem)
            except (OSError, WorkspaceError):
                errors.add("profile_inventory_error")

    def _scenario_references(self, references: dict[str, Any], errors: set[str]) -> None:
        if not self.paths.scenarios.is_dir() or self.paths.scenarios.is_symlink():
            if self.paths.scenarios.exists() or self.paths.scenarios.is_symlink():
                errors.add("scenario_inventory_error")
            return
        for root in sorted(self.paths.scenarios.iterdir()):
            if root.name.startswith("."):
                continue
            if root.is_symlink():
                errors.add("scenario_inventory_error")
                continue
            if not root.is_dir():
                continue
            try:
                marker = root / MANAGED_MARKER
                metadata_path = root / "scenario.json"
                if (
                    marker.is_symlink()
                    or load_json(marker)
                    != {"schema_version": SCHEMA_VERSION, "purpose": "test-scenario"}
                    or metadata_path.is_symlink()
                ):
                    raise WorkspaceError("scenario metadata is a symlink")
                metadata = load_json(metadata_path)
                if not isinstance(metadata, dict):
                    raise WorkspaceError("scenario metadata is invalid")
                resolved = metadata.get("resolved")
                if metadata.get("name") != root.name or not isinstance(resolved, dict):
                    raise WorkspaceError("scenario resolution is invalid")
                for row in resolved.values():
                    if (
                        not isinstance(row, dict)
                        or not isinstance(row.get("checkout_path"), str)
                        or not isinstance(row.get("checkout"), str)
                        or not isinstance(row.get("repository"), str)
                        or not isinstance(row.get("branch"), str)
                        or not isinstance(row.get("source"), str)
                        or not isinstance(row.get("head"), str)
                        or not HEAD_PATTERN.fullmatch(row["head"])
                    ):
                        raise WorkspaceError("scenario checkout path is invalid")
                    selected = Path(row["checkout_path"])
                    if not selected.is_absolute():
                        raise WorkspaceError("scenario checkout path is relative")
                    self._add_reference(references["scenarios"], selected, root.name)
            except (OSError, WorkspaceError):
                errors.add("scenario_inventory_error")

    def _topology_references(self, references: dict[str, Any], errors: set[str]) -> None:
        if not self.paths.topologies.is_dir() or self.paths.topologies.is_symlink():
            if self.paths.topologies.exists() or self.paths.topologies.is_symlink():
                errors.add("topology_inventory_error")
            return
        for root in sorted(self.paths.topologies.iterdir()):
            status_path = root / "status.json"
            if not status_path.exists() and not status_path.is_symlink():
                continue
            try:
                marker = root / MANAGED_MARKER
                if (
                    root.is_symlink()
                    or not root.is_dir()
                    or marker.is_symlink()
                    or load_json(marker)
                    != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"topology:{root.name}",
                    }
                    or status_path.is_symlink()
                ):
                    raise WorkspaceError("topology path is invalid")
                status_value = load_json(status_path)
                if not isinstance(status_value, dict):
                    raise WorkspaceError("topology status is invalid")
                process_records: list[Any] = [status_value.get("supervisor")]
                services = status_value.get("services")
                if not isinstance(services, dict):
                    raise WorkspaceError("topology service status is invalid")
                process_records.extend(services.values())
                lease_path = root / "process-tree.lease"
                if lease_path.is_symlink():
                    raise WorkspaceError("topology process-tree lease is invalid")
                control = status_value.get("control")
                if control is not None:
                    if (
                        not isinstance(control, dict)
                        or set(control) != {"socket", "generation", "lease"}
                        or not isinstance(control.get("generation"), str)
                        or re.fullmatch(r"[0-9a-f]{64}", control["generation"])
                        is None
                        or control.get("socket")
                        != str(control_socket_path(root, control["generation"]))
                        or not isinstance(control.get("lease"), dict)
                    ):
                        raise WorkspaceError("topology control identity is invalid")
                    live = bound_lease_locked(
                        lease_path, control["generation"], control["lease"]
                    )
                else:
                    live = lease_path.is_file() and lease_locked(lease_path)
                for record in process_records:
                    if not isinstance(record, dict):
                        raise WorkspaceError("topology process status is invalid")
                    pid, start_time = record.get("pid"), record.get("start_time")
                    if (
                        not isinstance(pid, int)
                        or isinstance(pid, bool)
                        or not isinstance(start_time, str)
                    ):
                        raise WorkspaceError("topology process identity is invalid")
                    if control is None:
                        live = live or process_matches(pid, start_time)
                if not live:
                    continue
                build_root = status_value.get("build_root")
                resolved = status_value.get("resolved")
                if (
                    status_value.get("schema_version") not in {SCHEMA_VERSION, 2, 3}
                    or status_value.get("name") != root.name
                    or not isinstance(build_root, str)
                    or not Path(build_root).is_absolute()
                ):
                    raise WorkspaceError("topology build root is invalid")
                self._add_reference(references["live_builds"], Path(build_root), root.name)
                if not isinstance(resolved, dict):
                    raise WorkspaceError("topology resolution is invalid")
                stack_name = status_value.get("stack")
                providers = status_value.get("providers")
                dependencies = status_value.get("dependencies")
                if (
                    not isinstance(stack_name, str)
                    or stack_name not in self.manifest.stacks
                    or not isinstance(providers, dict)
                    or not isinstance(dependencies, list)
                    or not all(isinstance(value, str) for value in dependencies)
                    or not all(
                        isinstance(role, str) and isinstance(component, str)
                        for role, component in providers.items()
                    )
                    or set(providers) != set(dependencies)
                    or set(resolved) != set(providers.values())
                ):
                    raise WorkspaceError("topology coordinates are historical or invalid")
                for role, component_name in providers.items():
                    if (
                        not isinstance(role, str)
                        or not isinstance(component_name, str)
                        or component_name not in self.manifest.by_name
                        or self.manifest.provider(stack_name, role).name
                        != component_name
                    ):
                        raise WorkspaceError("topology provider identity is invalid")
                coordinate_keys = {
                    "path",
                    "checkout_path",
                    "checkout",
                    "repository",
                    "branch",
                    "source",
                    "head",
                    "dirty",
                }
                for component_name, row in resolved.items():
                    component = self.manifest.by_name[component_name]
                    if (
                        not isinstance(row, dict)
                        or set(row) != coordinate_keys
                        or not isinstance(row.get("checkout_path"), str)
                        or not isinstance(row.get("path"), str)
                        or not isinstance(row.get("head"), str)
                        or not HEAD_PATTERN.fullmatch(row["head"])
                        or not isinstance(row.get("dirty"), bool)
                        or row.get("checkout") != component.checkout_name
                        or row.get("repository") != component.repository
                        or row.get("branch") != component.branch
                        or row.get("source") != component.source
                    ):
                        raise WorkspaceError("topology checkout identity is invalid")
                    selected = Path(row["checkout_path"])
                    source_path = Path(row["path"])
                    if not selected.is_absolute() or not source_path.is_absolute():
                        raise WorkspaceError("topology checkout path is relative")
                    source = PurePosixPath(component.source)
                    expected_source = (
                        selected
                        if component.source == "."
                        else selected.joinpath(*source.parts)
                    )
                    if source_path.resolve(strict=False) != expected_source.resolve(
                        strict=False
                    ):
                        raise WorkspaceError("topology source path is invalid")
                    self._add_reference(references["topologies"], selected, root.name)
            except (OSError, RuntimeError, KeyError, WorkspaceError):
                errors.add("topology_inventory_error")

    def _migration_references(self, references: dict[str, Any], errors: set[str]) -> None:
        for relative in (
            MIGRATION_RECORD,
            MIGRATION_PENDING,
            CONTENT_MIGRATION_RECORD,
            CONTENT_MIGRATION_PENDING,
        ):
            path = self.paths.workspace / relative
            if not path.exists() and not path.is_symlink():
                continue
            try:
                if path.is_symlink():
                    raise WorkspaceError("migration record is a symlink")
                value = load_json(path)
                if not isinstance(value, dict):
                    raise WorkspaceError("migration record is invalid")
                found = False
                for label, candidate in self._migration_paths(value):
                    found = True
                    self._add_reference(references["migration"], candidate, label)
                if not found:
                    raise WorkspaceError("migration record contains no paths")
            except (OSError, WorkspaceError):
                errors.add("migration_inventory_error")

    @staticmethod
    def _migration_paths(value: dict[str, Any]) -> Iterable[tuple[str, Path]]:
        sections = (
            ("sources", ("source", "archive", "path")),
            ("worktree_migrations", ("path", "destination")),
            ("worktree_moves", ("source", "destination")),
            ("composite_worktrees", ("destination",)),
            ("worktrees", ("destination",)),
            ("profiles", ("path",)),
        )
        for section, keys in sections:
            rows = value.get(section, [])
            if not isinstance(rows, list):
                raise WorkspaceError(f"migration {section} is not a list")
            for index, row in enumerate(rows):
                if not isinstance(row, dict):
                    raise WorkspaceError(f"migration {section}[{index}] is invalid")
                for key in keys:
                    raw = row.get(key)
                    if raw is None:
                        continue
                    if not isinstance(raw, str) or not Path(raw).is_absolute():
                        raise WorkspaceError(
                            f"migration {section}[{index}].{key} is invalid"
                        )
                    if raw:
                        yield f"{section}[{index}].{key}", Path(raw)
        classic = value.get("classic")
        if isinstance(classic, str) and Path(classic).is_absolute():
            yield "classic", Path(classic)
        elif isinstance(classic, dict):
            raw = classic.get("path")
            if isinstance(raw, str) and Path(raw).is_absolute():
                yield "classic.path", Path(raw)
            elif raw is not None:
                raise WorkspaceError("migration classic.path is invalid")
        elif classic is not None:
            raise WorkspaceError("migration classic path is invalid")
        for section in ("canonical", "legacy"):
            row = value.get(section)
            if row is None:
                continue
            if not isinstance(row, dict):
                raise WorkspaceError(f"migration {section} is invalid")
            raw = row.get("path")
            if not isinstance(raw, str) or not Path(raw).is_absolute():
                raise WorkspaceError(f"migration {section}.path is invalid")
            yield f"{section}.path", Path(raw)
        resources = value.get("resources")
        if resources is not None:
            if not isinstance(resources, dict):
                raise WorkspaceError("migration resources are invalid")
            for category, rows in resources.items():
                if not isinstance(category, str) or not isinstance(rows, list):
                    raise WorkspaceError("migration resource category is invalid")
                for index, row in enumerate(rows):
                    if not isinstance(row, dict):
                        raise WorkspaceError(
                            f"migration resources.{category}[{index}] is invalid"
                        )
                    raw = row.get("path")
                    if raw is None:
                        continue
                    if not isinstance(raw, str) or not Path(raw).is_absolute():
                        raise WorkspaceError(
                            f"migration resources.{category}[{index}].path is invalid"
                        )
                    yield f"resources.{category}[{index}].path", Path(raw)

    def _retention_references(self, references: dict[str, Any], errors: set[str]) -> None:
        path = self.paths.builds / BUILD_RETENTION_RECORD
        if self.paths.builds.is_symlink() or (
            self.paths.builds.exists() and not self.paths.builds.is_dir()
        ):
            if path.exists() or path.is_symlink():
                errors.add("retention_inventory_error")
            return
        if not path.exists() and not path.is_symlink():
            return
        try:
            if path.is_symlink():
                raise WorkspaceError("build retention record is a symlink")
            value = load_json(path)
            if not isinstance(value, dict) or set(value) != {
                "schema_version", "build_roots"
            } or value.get("schema_version") != CLEANUP_SCHEMA_VERSION or not isinstance(
                value.get("build_roots"), list
            ):
                raise WorkspaceError("build retention record is invalid")
            for index, raw in enumerate(value["build_roots"]):
                if not isinstance(raw, str) or not Path(raw).is_absolute():
                    raise WorkspaceError("retained build root is invalid")
                self._add_reference(references["retention"], Path(raw), str(index))
            if len(value["build_roots"]) != len(set(value["build_roots"])):
                raise WorkspaceError("retained build roots must be unique")
        except (OSError, WorkspaceError):
            errors.add("retention_inventory_error")

    def _worktrees(
        self,
        names: set[str] | None,
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> tuple[list[dict[str, Any]], set[Path]]:
        items: list[dict[str, Any]] = []
        registered: set[Path] = set()
        repositories: list[tuple[str, str, str, Path, bool]] = []
        if names is None or "atrinik" in names:
            repositories.append(
                ("atrinik", "atrinik/atrinik", "main", self.paths.repository, True)
            )
        for checkout in self.manifest.checkouts:
            if names is not None and checkout.name not in names:
                continue
            primary = self.paths.repositories / checkout.path
            if primary.exists() or primary.is_symlink():
                repositories.append(
                    (checkout.name, checkout.repository, checkout.branch, primary, False)
                )
        for owner, repository, base, invocation, wrapper in repositories:
            try:
                common, records, primary = self._repository_inventory(
                    repository, invocation
                )
                self._repositories[owner] = primary
                allowed = (
                    [
                        self.paths.worktrees / "atrinik",
                        primary / "build" / "worktrees",
                    ]
                    if wrapper
                    else [self.paths.worktrees / owner]
                )
                for row in records:
                    if "worktree" not in row:
                        continue
                    path = Path(row["worktree"])
                    normalized = path.resolve(strict=False)
                    registered.add(normalized)
                    items.append(
                        self._worktree_item(
                            owner,
                            repository,
                            base,
                            primary,
                            common,
                            allowed,
                            row,
                            references,
                            reference_errors,
                        )
                    )
            except (OSError, RuntimeError, WorkspaceError) as error:
                item = _base_item("worktree", owner, repository, invocation)
                item["disposition"] = "error"
                item["reasons"] = ["repository_inventory_error"]
                item["error"] = str(error)
                items.append(item)
        return items, registered

    @staticmethod
    def _remote_identity(path: Path, repository: str) -> bool:
        for remote in ("origin", "upstream"):
            try:
                urls = _command(path, "remote", "get-url", "--all", remote).splitlines()
            except WorkspaceError:
                continue
            if urls and _remote_matches(urls[0], repository):
                return True
        return False

    def _worktree_item(
        self,
        owner: str,
        repository: str,
        base: str,
        primary: Path,
        common: Path,
        allowed: list[Path],
        record: dict[str, str],
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> dict[str, Any]:
        path = Path(record["worktree"])
        normalized = path.resolve(strict=False)
        prunable = "prunable" in record and not path.exists()
        item = _base_item(
            "prunable-metadata" if prunable else "worktree",
            owner,
            repository,
            path,
        )
        item.update(
            {
                "branch": record.get("branch", "").removeprefix("refs/heads/") or None,
                "head": record.get("HEAD"),
                "base_branch": base,
                "ignored_bytes": 0,
                "ignored_paths": 0,
                "merged_pr": None,
                "_primary": str(primary),
            }
        )
        if not isinstance(item["head"], str) or not HEAD_PATTERN.fullmatch(item["head"]):
            item["reasons"].append("invalid_worktree_head")
        if "branch" not in record:
            item["reasons"].append("detached_head")
        if "locked" in record:
            item["reasons"].append("locked_worktree")
        owned = any(self._owned_direct_child(path, root) for root in allowed)
        if not owned:
            item["reasons"].append("external_path")
        primary_item = normalized == primary.resolve()
        if primary_item:
            item["reasons"].append("primary_checkout")
        if owner == "atrinik" and normalized == self.paths.repository.resolve():
            item["reasons"].append("active_wrapper_view")
        reference_reasons = {
            "profiles": "profile_reference",
            "scenarios": "scenario_reference",
            "topologies": "topology_reference",
            "migration": "migration_reference",
        }
        for category, reference_reason in reference_reasons.items():
            values = references[category].get(normalized, [])
            if values:
                item["references"][category] = sorted(set(values))
                item["reasons"].append(reference_reason)
        item["reasons"].extend(sorted(reference_errors))
        if prunable:
            try:
                if self._prunable_metadata_has_submodules(common, path):
                    item["reasons"].append("populated_submodules")
            except WorkspaceError as error:
                item["reasons"].append("git_inspection_error")
                item["error"] = str(error)
            item["reasons"].append("prunable_metadata")
            if item["reasons"] == ["prunable_metadata"]:
                item["disposition"] = "skipped"
                item["reasons"] = ["github_pending"]
            return item
        if primary_item:
            return item
        inodes, _, walk_error = _tree_usage(path)
        item["_inodes"] = inodes
        if walk_error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = walk_error
        if path.is_symlink():
            item["reasons"].append("symlinked_worktree")
        if not path.is_dir():
            item["reasons"].append("missing_worktree")
            return item
        if owner == "sound":
            try:
                item["_sound_worktree_identity"] = _sound_worktree_identity(
                    path, common
                )
            except (OSError, RuntimeError, WorkspaceError) as error:
                item["reasons"].append("unexpected_git_worktree_identity")
                item["error"] = str(error)
                item["_sound_worktree_identity"] = None
            try:
                _producer_path, producer_identity = (
                    _sound_producer_lock_snapshot(path)
                )
                item["_sound_producer_identity"] = producer_identity
            except FileNotFoundError:
                item["_sound_producer_identity"] = None
            except (OSError, RuntimeError, WorkspaceError):
                item["_sound_producer_identity"] = None
            item["reasons"].extend(self._sound_worktree_lock_reasons(path))
        try:
            worktree_common, worktree_git = _git_directories(path)
            if worktree_common != common:
                item["reasons"].append("unexpected_git_common_directory")
            if worktree_git == common and normalized != primary.resolve():
                item["reasons"].append("unexpected_primary_identity")
            if self._populated_submodules(path, worktree_git):
                item["reasons"].append("populated_submodules")
            if self._operation_in_progress(path):
                item["reasons"].append("git_operation_in_progress")
            status_value = _command(
                path,
                "status",
                "--porcelain=v1",
                "--untracked-files=all",
                "--ignore-submodules=none",
            )
            if status_value:
                item["reasons"].append("dirty_worktree")
            ignored = _command(
                path,
                "ls-files",
                "--others",
                "--ignored",
                "--exclude-standard",
                "-z",
            )
            ignored_paths = [value for value in ignored.split("\0") if value]
            ignored_sizes = _listed_usage(path, ignored_paths)
            item["ignored_paths"] = len(ignored_paths)
            item["ignored_bytes"] = sum(ignored_sizes.values())
        except WorkspaceError as error:
            item["reasons"].append("git_inspection_error")
            item["error"] = str(error)
        if not item["reasons"]:
            item["disposition"] = "skipped"
            item["reasons"] = ["github_pending"]
        return item

    def _sound_worktree_lock_reasons(self, worktree: Path) -> list[str]:
        reasons: list[str] = []
        try:
            producer_lock, _producer_identity = _sound_producer_lock_snapshot(
                worktree
            )
            producer_busy, producer_error = self._lock_busy(producer_lock)
        except FileNotFoundError:
            reasons.append("sound_cleanup_lease_unavailable")
            producer_busy, producer_error = False, None
        except (OSError, RuntimeError, WorkspaceError):
            reasons.append("sound_cache_lock_uncertain")
            producer_busy, producer_error = False, None
        if producer_error:
            reasons.append("sound_cache_lock_uncertain")
        elif producer_busy:
            reasons.append("active_sound_build")
        build_root = worktree / "build"
        cache_root = build_root / "atrinik-workspace"
        if not cache_root.exists() and not cache_root.is_symlink():
            return reasons
        if (
            build_root.is_symlink()
            or cache_root.is_symlink()
            or not cache_root.is_dir()
        ):
            return sorted(set([*reasons, "sound_cache_lock_uncertain"]))
        try:
            children = sorted(cache_root.iterdir())
        except OSError:
            return sorted(set([*reasons, "sound_cache_lock_uncertain"]))
        for child in children:
            if not re.fullmatch(r"\.[0-9a-f]{20}\.build\.lock", child.name):
                reasons.append("sound_cache_present")
                continue
            busy, error = self._lock_busy(child)
            if error:
                reasons.append("sound_cache_lock_uncertain")
            elif busy:
                reasons.append("active_sound_build")
        return sorted(set(reasons))

    @staticmethod
    def _operation_in_progress(path: Path) -> bool:
        arguments = tuple(
            argument
            for name in OPERATION_PATHS
            for argument in ("--git-path", name)
        )
        values = _command(path, "rev-parse", *arguments).splitlines()
        if len(values) != len(OPERATION_PATHS):
            raise WorkspaceError(
                f"git returned invalid operation metadata at {path}"
            )
        for value in values:
            operation_path = Path(value)
            if not operation_path.is_absolute():
                operation_path = path / operation_path
            if operation_path.exists() or operation_path.is_symlink():
                return True
        return False

    @staticmethod
    def _populated_submodules(path: Path, git_directory: Path) -> bool:
        """Match Git's conservative refusal to remove populated submodules."""

        try:
            modules = git_directory / "modules"
            if modules.exists() or modules.is_symlink():
                return True
            gitmodules = path / ".gitmodules"
            if not gitmodules.exists() and not gitmodules.is_symlink():
                return False
            if gitmodules.is_symlink() or not gitmodules.is_file():
                raise WorkspaceError(
                    f"worktree has unsafe .gitmodules metadata: {path}"
                )
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect worktree submodule metadata at {path}: {error}"
            ) from error
        output = _command(path, "submodule", "status", "--recursive")
        for line in output.splitlines():
            if not line or line[0] not in {" ", "-", "+", "U"}:
                raise WorkspaceError(f"git returned invalid submodule status at {path}")
            if line[0] != "-":
                return True
        return False

    @staticmethod
    def _prunable_metadata_has_submodules(common: Path, path: Path) -> bool:
        """Inspect the exact missing worktree's retained administrative directory."""

        worktrees = common / "worktrees"
        try:
            if worktrees.is_symlink() or not worktrees.is_dir():
                raise WorkspaceError("worktree administrative directory is unsafe")
            expected = (path / ".git").resolve(strict=False)
            matches: list[Path] = []
            for admin in worktrees.iterdir():
                if admin.is_symlink() or not admin.is_dir():
                    raise WorkspaceError(
                        "worktree administrative entry is unsafe"
                    )
                gitdir = admin / "gitdir"
                if gitdir.is_symlink() or not gitdir.is_file():
                    raise WorkspaceError("worktree gitdir metadata is unsafe")
                value = gitdir.read_text(encoding="utf-8")
                if (
                    not value.endswith("\n")
                    or "\n" in value[:-1]
                    or "\0" in value
                    or not Path(value[:-1]).is_absolute()
                ):
                    raise WorkspaceError("worktree gitdir metadata is invalid")
                if Path(value[:-1]).resolve(strict=False) == expected:
                    matches.append(admin)
            if len(matches) != 1:
                raise WorkspaceError(
                    "cannot map prunable worktree to administrative metadata"
                )
            modules = matches[0] / "modules"
            return modules.exists() or modules.is_symlink()
        except WorkspaceError:
            raise
        except (OSError, RuntimeError, UnicodeError) as error:
            raise WorkspaceError(
                f"cannot inspect prunable worktree metadata for {path}: {error}"
            ) from error

    def _resolve_github(self, items: list[dict[str, Any]], older_than_days: int) -> None:
        pending = [
            item
            for item in items
            if item["kind"] in {"worktree", "prunable-metadata"}
            and item["disposition"] == "skipped"
            and item["reasons"] == ["github_pending"]
        ]
        keys = sorted({(item["repository"], item["head"]) for item in pending})
        with ThreadPoolExecutor(max_workers=min(8, max(1, len(keys)))) as executor:
            futures = {
                executor.submit(
                    copy_context().run, self._github_pulls, repository, head
                ): (repository, head)
                for repository, head in keys
            }
            for future in as_completed(futures):
                key = futures[future]
                try:
                    self._github_cache[key] = (future.result(), None)
                except WorkspaceError as error:
                    self._github_cache[key] = (None, str(error))
        cutoff = older_than_days * 86400
        for item in pending:
            pulls, error = self._github_cache[(item["repository"], item["head"])]
            item["disposition"] = "protected"
            item["reasons"] = []
            if error is not None or pulls is None:
                item["reasons"] = ["github_unavailable"]
                item["github_error"] = error
                continue
            reason, evidence, merged_at = self._pull_evidence(
                pulls, item["head"], item["base_branch"]
            )
            historical_base = False
            if reason == "wrong_base_branch":
                row = pulls[0]
                boundary = self._historical_pull_boundary(item, row)
                if boundary is not None:
                    historical_branch = row["base"]["ref"]
                    reason, evidence, merged_at = self._pull_evidence(
                        pulls, item["head"], historical_branch
                    )
                    if reason is None and not self._historical_merge_proven(
                        item, row, boundary
                    ):
                        reason = "historical_base_unverified"
                        evidence = None
                        merged_at = None
                    historical_base = reason is None
            if reason is not None:
                item["reasons"] = [reason]
                continue
            assert evidence is not None and merged_at is not None
            age = max(0, int((self.now - merged_at).total_seconds()))
            item["age_seconds"] = age
            item["age_basis"] = "pr-merge-time"
            item["merged_pr"] = evidence
            if merged_at > self.now:
                item["reasons"] = ["future_merge_time"]
            elif age < cutoff:
                item["reasons"] = ["younger_than_grace_period"]
            else:
                item["disposition"] = "eligible"
                item["reasons"] = [
                    "merged_pr_head_historical_base"
                    if historical_base
                    else "merged_pr_head"
                ]
                if item["kind"] == "prunable-metadata":
                    item["reasons"].append("prunable_metadata")

    def _historical_pull_boundary(
        self, item: dict[str, Any], row: dict[str, Any]
    ) -> str | None:
        """Return the frozen pre-rewrite boundary for one legacy wrapper path."""

        base = row.get("base")
        primary = item.get("_primary")
        if (
            item.get("owner") != "atrinik"
            or item.get("repository") != "atrinik/atrinik"
            or item.get("base_branch") != "main"
            or not isinstance(primary, str)
            or not isinstance(base, dict)
            or not isinstance(base.get("ref"), str)
            or not self._owned_direct_child(
                Path(item["path"]), Path(primary) / "build" / "worktrees"
            )
        ):
            return None
        return HISTORICAL_PULL_BASE_BOUNDARIES.get(
            (item["repository"], item["base_branch"], base["ref"])
        )

    @staticmethod
    def _historical_merge_proven(
        item: dict[str, Any], row: dict[str, Any], boundary: str
    ) -> bool:
        """Prove GitHub's merge commit belongs to the frozen historical line."""

        primary = item.get("_primary")
        base = row.get("base")
        merge_commit = row.get("merge_commit_sha")
        if (
            not isinstance(primary, str)
            or not isinstance(base, dict)
            or not isinstance(base.get("sha"), str)
            or not isinstance(merge_commit, str)
            or not HEAD_PATTERN.fullmatch(base["sha"])
            or not HEAD_PATTERN.fullmatch(merge_commit)
            or not HEAD_PATTERN.fullmatch(boundary)
        ):
            return False
        try:
            common = _git_common_directory(Path(primary))
            grafts = common / "info" / "grafts"
            try:
                grafts.lstat()
            except FileNotFoundError:
                pass
            else:
                return False
            commit = _command(
                Path(primary),
                "--no-replace-objects",
                "rev-list",
                "--parents",
                "-n",
                "1",
                merge_commit,
            ).split()
            if (
                len(commit) < 2
                or commit[0] != merge_commit
                or commit[1] != base["sha"]
            ):
                return False
            _command(
                Path(primary),
                "--no-replace-objects",
                "merge-base",
                "--is-ancestor",
                merge_commit,
                boundary,
            )
        except (OSError, RuntimeError, WorkspaceError):
            return False
        return True

    @staticmethod
    def _protect_shared_prune_scope(items: list[dict[str, Any]]) -> None:
        owners = {
            item["owner"]
            for item in items
            if item["kind"] == "prunable-metadata"
            and item["disposition"] != "eligible"
        }
        for item in items:
            if (
                item["kind"] == "prunable-metadata"
                and item["owner"] in owners
                and item["disposition"] == "eligible"
            ):
                item["disposition"] = "protected"
                item["reasons"] = ["shared_prune_scope_protected"]

    @staticmethod
    def _github_pulls(repository: str, head: str) -> list[dict[str, Any]]:
        try:
            result = subprocess.run(
                [
                    "gh",
                    "api",
                    f"repos/{repository}/commits/{head}/pulls?per_page=100",
                    "--header",
                    "Accept: application/vnd.github+json",
                    "--paginate",
                    "--jq",
                    ".[] | {number,state,merged_at,merge_commit_sha,"
                    "head:{sha:.head.sha},base:{ref:.base.ref,sha:.base.sha},html_url}",
                ],
                check=True,
                capture_output=True,
                text=True,
                pass_fds=active_lock_fds(),
                timeout=30,
            )
        except FileNotFoundError as error:
            raise WorkspaceError("required command not found: gh") from error
        except subprocess.CalledProcessError as error:
            detail = error.stderr.strip()
            raise WorkspaceError(detail or "GitHub pull request query failed") from error
        except subprocess.TimeoutExpired as error:
            raise WorkspaceError("GitHub pull request query timed out") from error
        pulls: list[dict[str, Any]] = []
        try:
            for line in result.stdout.splitlines():
                row = json.loads(line)
                if not isinstance(row, dict):
                    raise WorkspaceError(
                        "GitHub pull request response has an invalid shape"
                    )
                pulls.append(row)
        except json.JSONDecodeError as error:
            raise WorkspaceError("GitHub pull request response is not JSON") from error
        return pulls

    @staticmethod
    def _pull_evidence(
        pulls: list[dict[str, Any]], head: str, base: str
    ) -> tuple[str | None, dict[str, Any] | None, datetime | None]:
        if not pulls:
            return "no_associated_pr", None, None
        if not all(Cleanup._valid_pull_record(row) for row in pulls):
            return "invalid_pull_request_evidence", None, None
        if any(row.get("state") == "open" for row in pulls):
            return "open_pull_request", None, None
        if any(not row.get("merged_at") for row in pulls):
            return "closed_unmerged_pr", None, None
        if len(pulls) != 1:
            return "ambiguous_pull_requests", None, None
        merged = pulls
        exact_head = [
            row
            for row in merged
            if isinstance(row.get("head"), dict) and row["head"].get("sha") == head
        ]
        if not exact_head:
            return "pr_head_mismatch", None, None
        exact_base = [
            row
            for row in exact_head
            if isinstance(row.get("base"), dict) and row["base"].get("ref") == base
        ]
        if not exact_base:
            return "wrong_base_branch", None, None
        if len(exact_base) != 1:
            return "ambiguous_pull_requests", None, None
        row = exact_base[0]
        try:
            merged_at = _parse_time(row.get("merged_at"), "PR merge time")
        except WorkspaceError:
            return "invalid_pull_request_evidence", None, None
        number = row.get("number")
        url = row.get("html_url")
        if not isinstance(number, int) or isinstance(number, bool) or not isinstance(url, str):
            return "invalid_pull_request_evidence", None, None
        return (
            None,
            {
                "number": number,
                "url": url,
                "base": base,
                "head": head,
                "merged_at": merged_at.isoformat(),
            },
            merged_at,
        )

    @staticmethod
    def _valid_pull_record(row: dict[str, Any]) -> bool:
        head = row.get("head")
        base = row.get("base")
        merged_at = row.get("merged_at")
        merge_commit = row.get("merge_commit_sha")
        state_value = row.get("state")
        return (
            set(row)
            == {
                "number",
                "state",
                "html_url",
                "merged_at",
                "merge_commit_sha",
                "head",
                "base",
            }
            and isinstance(row.get("number"), int)
            and not isinstance(row.get("number"), bool)
            and isinstance(row.get("html_url"), str)
            and bool(row["html_url"])
            and state_value in {"open", "closed"}
            and (merged_at is None or isinstance(merged_at, str) and bool(merged_at))
            and not (state_value == "open" and merged_at is not None)
            and isinstance(head, dict)
            and set(head) == {"sha"}
            and isinstance(head.get("sha"), str)
            and bool(HEAD_PATTERN.fullmatch(head["sha"]))
            and isinstance(base, dict)
            and set(base) == {"ref", "sha"}
            and isinstance(base.get("ref"), str)
            and bool(base["ref"])
            and isinstance(base.get("sha"), str)
            and bool(HEAD_PATTERN.fullmatch(base["sha"]))
            and (
                merge_commit is None and merged_at is None
                or isinstance(merge_commit, str)
                and bool(HEAD_PATTERN.fullmatch(merge_commit))
            )
        )

    def _builds(
        self,
        older_than_days: int,
        registered: set[Path],
        removable_worktrees: set[Path],
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> list[dict[str, Any]]:
        if self.paths.builds.is_symlink() or (
            self.paths.builds.exists() and not self.paths.builds.is_dir()
        ):
            item = _base_item(
                "unmanaged-build", "atrinik", "atrinik/atrinik", self.paths.builds
            )
            item["reasons"] = ["invalid_build_container"]
            inodes, _, error = _tree_usage(self.paths.builds)
            item["_inodes"] = inodes
            if error:
                item["reasons"].append("filesystem_traversal_error")
                item["error"] = error
            return [item]
        profiles = self.paths.builds / "profiles"
        items: list[dict[str, Any]] = []
        if not profiles.is_dir() or profiles.is_symlink():
            if profiles.exists() or profiles.is_symlink():
                item = _base_item(
                    "unmanaged-build", "atrinik", "atrinik/atrinik", profiles
                )
                item["reasons"] = ["invalid_profiles_container"]
                inodes, _, error = _tree_usage(profiles)
                item["_inodes"] = inodes
                if error:
                    item["error"] = error
                items.append(item)
        else:
            try:
                profile_roots = sorted(profiles.iterdir())
            except OSError as error:
                item = _base_item(
                    "unmanaged-build", "atrinik", "atrinik/atrinik", profiles
                )
                item["reasons"] = ["profiles_inventory_error"]
                item["error"] = str(error)
                items.append(item)
            else:
                items.extend(
                    self._build_item(
                        path,
                        older_than_days,
                        registered,
                        removable_worktrees,
                        references,
                        reference_errors,
                    )
                    for path in profile_roots
                )
        items.extend(
            self._worker_dependency_caches(
                older_than_days, registered, references, reference_errors
            )
        )
        return items

    def _worker_dependency_caches(
        self,
        older_than_days: int,
        registered: set[Path],
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> list[dict[str, Any]]:
        root = self.paths.builds / "worker-dependencies"
        if not root.exists() and not root.is_symlink():
            return []
        try:
            marker = root / MANAGED_MARKER
            if (
                root.is_symlink()
                or not root.is_dir()
                or marker.is_symlink()
                or load_json(marker)
                != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "worker-dependency-cache",
                }
            ):
                raise WorkspaceError(
                    "Worker dependency cache root is not marker-owned"
                )
            paths = sorted(
                path
                for path in root.iterdir()
                if path.name not in {MANAGED_MARKER, ".transactions"}
            )
        except (OSError, WorkspaceError) as error:
            item = _base_item(
                "unmanaged-build", "atrinik", "atrinik/atrinik", root
            )
            item["reasons"] = ["invalid_worker_dependency_cache"]
            item["error"] = str(error)
            inodes, _, walk_error = _tree_usage(root)
            item["_inodes"] = inodes
            if walk_error:
                item["reasons"].append("filesystem_traversal_error")
            return [item]
        items = [
            self._worker_dependency_item(
                path,
                older_than_days,
                registered,
                references,
                reference_errors,
            )
            for path in paths
        ]
        transactions = root / ".transactions"
        if transactions.exists() or transactions.is_symlink():
            try:
                marker = transactions / MANAGED_MARKER
                if (
                    transactions.is_symlink()
                    or not transactions.is_dir()
                    or marker.is_symlink()
                    or load_json(marker)
                    != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": "worker-dependency-transactions",
                    }
                ):
                    raise WorkspaceError(
                        "Worker dependency transaction root is not marker-owned"
                    )
                transaction_paths = sorted(
                    path
                    for path in transactions.iterdir()
                    if path.name != MANAGED_MARKER
                )
            except (OSError, WorkspaceError) as error:
                item = _base_item(
                    "unmanaged-build",
                    "atrinik",
                    "atrinik/atrinik",
                    transactions,
                )
                item["reasons"] = ["invalid_worker_dependency_transactions"]
                item["error"] = str(error)
                inodes, _, walk_error = _tree_usage(transactions)
                item["_inodes"] = inodes
                if walk_error:
                    item["reasons"].append("filesystem_traversal_error")
                items.append(item)
            else:
                items.extend(
                    self._worker_dependency_transaction_item(
                        path, older_than_days
                    )
                    for path in transaction_paths
                )
        return items

    @staticmethod
    def _worker_dependency_transaction_created_at(path: Path) -> datetime:
        return datetime.fromtimestamp(path.lstat().st_ctime, timezone.utc)

    def _worker_dependency_transaction_item(
        self,
        path: Path,
        older_than_days: int,
        *,
        check_lock: bool = True,
    ) -> dict[str, Any]:
        item = _base_item(
            "worker-dependency-transaction", "atrinik", "atrinik/atrinik", path
        )
        inodes, observed, walk_error = _tree_usage(path)
        item["_inodes"] = inodes
        try:
            path_status = path.lstat()
            item["_identity"] = (
                path_status.st_dev,
                path_status.st_ino,
                path_status.st_ctime_ns,
                stat.S_IFMT(path_status.st_mode),
                stat.S_IMODE(path_status.st_mode),
            )
            created = self._worker_dependency_transaction_created_at(path)
            observed = created if observed is None else max(observed, created)
        except OSError as error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = str(error)
        match = re.fullmatch(
            r"([0-9a-f]{64})-(staging|backup)-([a-z0-9_]+)", path.name
        )
        if walk_error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = walk_error
        if match is None or path.is_symlink() or not path.is_dir():
            item["reasons"].append("invalid_worker_dependency_transaction")
        else:
            key, transaction_type, _suffix = match.groups()
            item["key"] = key
            marker = path / MANAGED_MARKER
            allowed_purposes = {f"worker-dependencies:{key}"}
            if transaction_type == "staging":
                allowed_purposes.add(f"worker-dependency-transaction:{key}")
            try:
                if marker.exists() or marker.is_symlink():
                    metadata = load_json(marker)
                    if marker.is_symlink() or metadata not in (
                        {
                            "schema_version": SCHEMA_VERSION,
                            "purpose": purpose,
                        }
                        for purpose in allowed_purposes
                    ):
                        raise WorkspaceError(
                            "Worker dependency transaction marker is invalid"
                        )
            except (OSError, WorkspaceError) as error:
                item["reasons"].append("invalid_worker_dependency_transaction")
                item["error"] = str(error)
            if check_lock:
                lock = (
                    self.paths.builds / "locks" / f"worker-dependencies-{key}.lock"
                )
                busy, lock_error = self._lock_busy(lock)
                if lock_error:
                    item["reasons"].append("build_lock_error")
                    item["error"] = lock_error
                elif busy:
                    item["reasons"].append("build_lock_busy")
        item["age_basis"] = "tree-mtime-or-root-ctime" if observed else None
        if observed is None:
            item["reasons"].append("build_age_unavailable")
        else:
            age = max(0, int((self.now - observed).total_seconds()))
            item["age_seconds"] = age
            if observed > self.now:
                item["reasons"].append("future_tree_mtime")
            elif age < older_than_days * 86400:
                item["reasons"].append("younger_than_grace_period")
        item["reasons"] = sorted(set(item["reasons"]))
        if not item["reasons"]:
            item["disposition"] = "eligible"
            item["reasons"] = ["stale_worker_dependency_transaction"]
        return item

    def _worker_dependency_item(
        self,
        path: Path,
        older_than_days: int,
        registered: set[Path],
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> dict[str, Any]:
        item = _base_item(
            "worker-dependencies", "atrinik", "atrinik/atrinik", path
        )
        inodes, _, walk_error = _tree_usage(path)
        item["_inodes"] = inodes
        if walk_error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = walk_error
        key = path.name
        purpose = f"worker-dependencies:{key}"
        item["key"] = key
        item["_purpose"] = purpose
        item["_older_than_days"] = older_than_days
        try:
            if not re.fullmatch(r"[0-9a-f]{64}", key):
                raise WorkspaceError("Worker dependency cache key is invalid")
            marker = path / MANAGED_MARKER
            if (
                path.is_symlink()
                or not path.is_dir()
                or marker.is_symlink()
                or load_json(marker)
                != {"schema_version": SCHEMA_VERSION, "purpose": purpose}
            ):
                raise WorkspaceError("Worker dependency cache marker is invalid")
            metadata_path = path / WORKER_DEPENDENCY_METADATA
            if metadata_path.is_symlink() or not metadata_path.is_file():
                raise WorkspaceError(
                    "Worker dependency metadata is not a regular file"
                )
            metadata = load_json(metadata_path)
            schema_version = (
                metadata.get("schema_version")
                if isinstance(metadata, dict)
                else None
            )
            metadata_keys = {
                "schema_version",
                "purpose",
                "key",
                "inputs",
                "node_modules_lock_sha256",
                "last_used_at",
            }
            if schema_version != 1:
                metadata_keys.add("node_modules_sha256")
            if schema_version == WORKER_DEPENDENCY_SCHEMA_VERSION:
                metadata_keys.add("node_modules_view_sha256")
            if (
                not isinstance(metadata, dict)
                or set(metadata) != metadata_keys
                or not isinstance(schema_version, int)
                or isinstance(schema_version, bool)
                or schema_version
                not in WORKER_DEPENDENCY_CLEANUP_SCHEMA_VERSIONS
                or metadata.get("purpose") != "worker-dependencies"
                or metadata.get("key") != key
                or not isinstance(metadata.get("inputs"), dict)
                or not isinstance(metadata.get("node_modules_lock_sha256"), str)
                or not re.fullmatch(
                    r"[0-9a-f]{64}", metadata["node_modules_lock_sha256"]
                )
                or schema_version != 1
                and (
                    not isinstance(metadata.get("node_modules_sha256"), str)
                    or not re.fullmatch(
                        r"[0-9a-f]{64}", metadata["node_modules_sha256"]
                    )
                )
                or schema_version == WORKER_DEPENDENCY_SCHEMA_VERSION
                and (
                    not isinstance(metadata.get("node_modules_view_sha256"), str)
                    or not re.fullmatch(
                        r"[0-9a-f]{64}", metadata["node_modules_view_sha256"]
                    )
                )
            ):
                raise WorkspaceError(
                    "Worker dependency metadata fields are invalid"
                )
            used_at = _parse_time(
                metadata.get("last_used_at"),
                "Worker dependencies last_used_at",
            )
            item["age_basis"] = "last-used-at"
            item["last_used_at"] = metadata["last_used_at"]
        except (OSError, WorkspaceError) as error:
            item["reasons"].append("invalid_worker_dependency_cache")
            item["error"] = str(error)
            used_at = None
        normalized = path.resolve(strict=False)
        if any(_path_relation(normalized, worktree) for worktree in registered):
            item["reasons"].append("contains_registered_worktree")
        for category, reason in (
            ("live_builds", "live_topology"),
            ("retention", "retention_reference"),
        ):
            values = references[category].get(normalized, [])
            if values:
                target = (
                    "topologies" if category == "live_builds" else "retention"
                )
                item["references"][target] = sorted(set(values))
                item["reasons"].append(reason)
        item["reasons"].extend(sorted(reference_errors))
        if re.fullmatch(r"[0-9a-f]{64}", key):
            lock = (
                self.paths.builds / "locks" / f"worker-dependencies-{key}.lock"
            )
            busy, lock_error = self._lock_busy(lock)
            if lock_error:
                item["reasons"].append("build_lock_error")
                item["error"] = lock_error
            elif busy:
                item["reasons"].append("build_lock_busy")
        if used_at is None:
            item["reasons"].append("build_age_unavailable")
        else:
            age = max(0, int((self.now - used_at).total_seconds()))
            item["age_seconds"] = age
            if used_at > self.now:
                item["reasons"].append("future_last_used")
            elif age < older_than_days * 86400:
                item["reasons"].append("younger_than_grace_period")
        item["reasons"] = sorted(set(item["reasons"]))
        if not item["reasons"]:
            item["disposition"] = "eligible"
            item["reasons"] = ["stale_worker_dependencies"]
        return item

    def _build_item(
        self,
        path: Path,
        older_than_days: int,
        registered: set[Path],
        removable_worktrees: set[Path],
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> dict[str, Any]:
        item = _base_item("profile-build", "atrinik", "atrinik/atrinik", path)
        metadata_path = path / BUILD_METADATA
        inodes, observed, walk_error = _tree_usage(path)
        item["_inodes"] = inodes
        if walk_error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = walk_error
        try:
            purpose, profile, key = self._profile_marker(path)
            item["profile"] = profile
            item["key"] = key
            item["_purpose"] = purpose
        except (OSError, WorkspaceError) as error:
            item["kind"] = "unmanaged-build"
            item["reasons"].append("invalid_managed_marker")
            item["error"] = str(error)
            return item
        normalized = path.resolve(strict=False)
        for category, reason in (
            ("live_builds", "live_topology"),
            ("retention", "retention_reference"),
        ):
            values = references[category].get(normalized, [])
            if values:
                target = "topologies" if category == "live_builds" else "retention"
                item["references"][target] = sorted(set(values))
                item["reasons"].append(reason)
        item["reasons"].extend(sorted(reference_errors))
        if any(_path_relation(normalized, worktree) for worktree in registered):
            item["reasons"].append("contains_registered_worktree")
        busy, lock_error = self._build_lock_busy(item["profile"], item["key"])
        if lock_error:
            item["reasons"].append("build_lock_error")
            item["error"] = lock_error
        elif busy:
            item["reasons"].append("build_lock_busy")
        source_removal = False
        if metadata_path.exists() or metadata_path.is_symlink():
            try:
                metadata = self._load_build_metadata(metadata_path, item)
                used_at = _parse_time(metadata["last_used_at"], "build last_used_at")
                item["age_basis"] = "last-used-at"
                try:
                    source_removal = any(
                        Path(row["checkout_path"]).resolve(strict=False)
                        in removable_worktrees
                        for row in metadata["coordinates"].values()
                    )
                except RuntimeError as error:
                    raise WorkspaceError(
                        "build source worktree path cannot be resolved"
                    ) from error
                item["source_worktree_removal"] = source_removal
            except (OSError, WorkspaceError) as error:
                item["reasons"].append("invalid_build_metadata")
                item["error"] = str(error)
                used_at = None
        else:
            used_at = observed
            item["age_basis"] = "legacy-tree-mtime"
            item["source_worktree_removal"] = False
        if used_at is None:
            item["reasons"].append("build_age_unavailable")
        else:
            age = max(0, int((self.now - used_at).total_seconds()))
            item["age_seconds"] = age
            if used_at > self.now:
                item["reasons"].append("future_last_used")
            elif not source_removal and age < older_than_days * 86400:
                item["reasons"].append("younger_than_grace_period")
        item["reasons"] = sorted(set(item["reasons"]))
        if not item["reasons"]:
            item["disposition"] = "eligible"
            item["reasons"] = [
                "source_worktree_removal" if source_removal else "stale_profile_build"
            ]
        return item

    @staticmethod
    def _profile_marker(path: Path) -> tuple[str, str, str]:
        if path.is_symlink() or not path.is_dir():
            raise WorkspaceError("profile build is not a regular directory")
        marker = path / MANAGED_MARKER
        if not marker.is_file() or marker.is_symlink():
            raise WorkspaceError("profile build marker is missing or invalid")
        value = load_json(marker)
        if not isinstance(value, dict) or set(value) != {"schema_version", "purpose"}:
            raise WorkspaceError("profile build marker shape is invalid")
        purpose = value.get("purpose")
        match = PROFILE_PURPOSE.fullmatch(purpose) if isinstance(purpose, str) else None
        if value.get("schema_version") != SCHEMA_VERSION or match is None:
            raise WorkspaceError("profile build marker purpose is invalid")
        profile, key = match.group("profile"), match.group("key")
        if path.name != f"{profile}-{key}":
            raise WorkspaceError("profile build marker does not match its path")
        return purpose, profile, key

    def _load_build_metadata(
        self, path: Path, item: dict[str, Any]
    ) -> dict[str, Any]:
        if path.is_symlink() or not path.is_file():
            raise WorkspaceError("build metadata is not a regular file")
        value = load_json(path)
        if not isinstance(value, dict):
            raise WorkspaceError("build metadata fields are invalid")
        schema_version = value.get("schema_version")
        expected_keys = (
            LEGACY_BUILD_METADATA_KEYS
            if schema_version == LEGACY_BUILD_METADATA_SCHEMA_VERSION
            else BUILD_METADATA_KEYS
        )
        if set(value) != expected_keys:
            raise WorkspaceError("build metadata fields are invalid")
        if (
            schema_version
            not in {
                LEGACY_BUILD_METADATA_SCHEMA_VERSION,
                BUILD_METADATA_SCHEMA_VERSION,
            }
            or value.get("profile") != item["profile"]
            or value.get("key") != item["key"]
            or value.get("purpose") != item["_purpose"]
            or not isinstance(value.get("coordinates"), dict)
            or not value["coordinates"]
        ):
            raise WorkspaceError("build metadata identity is invalid")
        for role, row in value["coordinates"].items():
            if (
                not isinstance(role, str)
                or not isinstance(row, dict)
                or set(row) != BUILD_COORDINATE_KEYS
                or not all(isinstance(raw, str) and raw for raw in row.values())
                or not Path(row["checkout_path"]).is_absolute()
                or not Path(row["source_path"]).is_absolute()
                or not HEAD_PATTERN.fullmatch(row["head"])
            ):
                raise WorkspaceError("build coordinate metadata is invalid")
            component = self.manifest.by_name.get(row["component"])
            if (
                component is None
                or role not in component.provides
                or row["checkout"] != component.checkout_name
                or row["repository"] != component.repository
                or row["branch"] != component.branch
                or row["source"] != component.source
            ):
                raise WorkspaceError("build coordinate manifest identity is invalid")
            source = PurePosixPath(row["source"])
            try:
                checkout_path = Path(row["checkout_path"]).resolve(strict=False)
                expected_source = (
                    checkout_path
                    if row["source"] == "."
                    else checkout_path.joinpath(*source.parts).resolve(strict=False)
                )
                if Path(row["source_path"]).resolve(strict=False) != expected_source:
                    raise WorkspaceError("build coordinate source path is invalid")
            except RuntimeError as error:
                raise WorkspaceError("build coordinate path cannot be resolved") from error
        if schema_version == BUILD_METADATA_SCHEMA_VERSION:
            sound = value.get("sound")
            if sound is not None:
                from .sound import validate_sound_record
                validate_sound_record(sound)
        return value

    def _build_lock_busy(self, profile: str, key: str) -> tuple[bool, str | None]:
        return self._lock_busy(self.paths.builds / "locks" / f"{profile}-{key}.lock")

    @staticmethod
    def _lock_busy(path: Path) -> tuple[bool, str | None]:
        if not path.exists() and not path.is_symlink():
            return False, None
        flags = os.O_RDWR | os.O_CLOEXEC
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        try:
            descriptor = os.open(path, flags)
            if not stat.S_ISREG(os.fstat(descriptor).st_mode):
                os.close(descriptor)
                return False, f"lock is not a regular file: {path}"
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                os.close(descriptor)
                return True, None
            fcntl.flock(descriptor, fcntl.LOCK_UN)
            os.close(descriptor)
            return False, None
        except OSError as error:
            return False, str(error)

    @staticmethod
    def _state_lock_observation(
        path: Path,
    ) -> tuple[bool, str | None, dict[str, int] | None]:
        flags = os.O_RDWR | os.O_CLOEXEC
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        try:
            descriptor = os.open(path, flags)
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1:
                os.close(descriptor)
                return False, f"state lock identity is invalid: {path}", None
            identity = {"device": metadata.st_dev, "inode": metadata.st_ino}
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                os.close(descriptor)
                return True, None, identity
            fcntl.flock(descriptor, fcntl.LOCK_UN)
            os.close(descriptor)
            return False, None, identity
        except OSError as error:
            return False, str(error), None

    def _unmanaged_builds(self, registered: set[Path]) -> list[dict[str, Any]]:
        items: list[dict[str, Any]] = []
        roots: list[tuple[Path, list[Path]]] = []
        top = self._wrapper_primary / "build"
        if top.is_dir() and not top.is_symlink():
            try:
                for path in sorted(top.iterdir()):
                    try:
                        if path.resolve(strict=False) in registered:
                            continue
                    except (OSError, RuntimeError):
                        roots.append((path, []))
                        continue
                    roots.append(
                        (
                            path,
                            [
                                candidate
                                for candidate in registered
                                if _path_relation(path, candidate)
                            ],
                        )
                    )
            except OSError:
                roots.append((top, []))
        elif top.exists() or top.is_symlink():
            roots.append((top, []))
        if self.paths.builds.is_dir() and not self.paths.builds.is_symlink():
            try:
                for path in sorted(self.paths.builds.iterdir()):
                    if path.name in {
                        "profiles",
                        "npm-cache",
                        "worker-dependencies",
                        "compiler-cache",
                    }:
                        continue
                    roots.append((path, []))
            except OSError:
                roots.append((self.paths.builds, []))
        for path, excluded in roots:
            item = _base_item("unmanaged-build", "atrinik", "atrinik/atrinik", path)
            inodes, observed, error = _tree_usage(path, excluded)
            item["_inodes"] = inodes
            item["age_basis"] = "tree-mtime" if observed else None
            item["age_seconds"] = (
                max(0, int((self.now - observed).total_seconds())) if observed else None
            )
            item["reasons"] = ["unmanaged_build"]
            if error:
                item["reasons"].append("filesystem_traversal_error")
                item["error"] = error
            items.append(item)
        return items

    def _npm_cache(
        self,
        older_than_days: int,
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> dict[str, Any] | None:
        return self._shared_cache(
            "npm-cache",
            "npm-cache",
            older_than_days,
            reference_errors,
            legacy_allowed=True,
        )

    def _compiler_cache(
        self,
        older_than_days: int,
        references: dict[str, Any],
        reference_errors: set[str],
    ) -> dict[str, Any] | None:
        return self._shared_cache(
            "compiler-cache",
            COMPILER_CACHE_PURPOSE,
            older_than_days,
            reference_errors,
            legacy_allowed=False,
        )

    def _sound_caches(
        self, older_than_days: int, reference_errors: set[str]
    ) -> list[dict[str, Any]]:
        checkout = self.manifest.by_checkout.get("sound")
        if checkout is None:
            return []
        invocation = self.paths.repositories / checkout.path
        if not invocation.exists() and not invocation.is_symlink():
            return []
        try:
            common, records, _primary = self._repository_inventory(
                checkout.repository, invocation
            )
        except (OSError, RuntimeError, WorkspaceError):
            reference_errors.add("sound_cache_inventory_error")
            return []
        items: list[dict[str, Any]] = []
        seen: set[Path] = set()
        for row in records:
            raw = row.get("worktree")
            if raw is None:
                continue
            worktree = Path(raw)
            build_root = worktree / "build"
            cache_root = build_root / "atrinik-workspace"
            try:
                worktree_identity = _sound_worktree_identity(worktree, common)
                if (
                    worktree.is_symlink()
                    or not worktree.is_dir()
                    or build_root.is_symlink()
                    or not build_root.is_dir()
                    or cache_root.is_symlink()
                    or not cache_root.is_dir()
                ):
                    continue
                children = sorted(cache_root.iterdir())
            except (OSError, RuntimeError, WorkspaceError):
                reference_errors.add("sound_cache_inventory_error")
                continue
            try:
                _producer_path, producer_identity = (
                    _sound_producer_lock_snapshot(worktree)
                )
            except (OSError, RuntimeError, WorkspaceError):
                producer_identity = None
            for path in children:
                if path.name.startswith("."):
                    continue
                try:
                    if path.is_symlink():
                        raise WorkspaceError("sound cache child is a symlink")
                    normalized = path.resolve(strict=False)
                except (OSError, RuntimeError, WorkspaceError):
                    reference_errors.add("sound_cache_inventory_error")
                    continue
                if normalized in seen:
                    continue
                seen.add(normalized)
                item = self._sound_cache_item(path, worktree, older_than_days)
                item["_worktree_identity"] = worktree_identity
                item["_git_common"] = str(common)
                item["_sound_producer_identity"] = producer_identity
                if producer_identity is None:
                    item["disposition"] = "skipped"
                    if item["reasons"] == ["stale_sound_cache"]:
                        item["reasons"] = []
                    item["reasons"].append("sound_cleanup_lease_unavailable")
                items.append(item)
        return items

    def _sound_cache_item(
        self,
        path: Path,
        worktree: Path,
        older_than_days: int,
        *,
        check_lock: bool = True,
    ) -> dict[str, Any]:
        item = _base_item("sound-cache", "sound", "atrinik/sound", path)
        item["checkout_path"] = str(worktree.resolve(strict=False))
        inodes, observed, error = _tree_usage(path, [])
        item["_inodes"] = inodes
        try:
            metadata = path.lstat()
            item["_identity"] = (
                metadata.st_dev,
                metadata.st_ino,
                metadata.st_ctime_ns,
                stat.S_IFMT(metadata.st_mode),
                stat.S_IMODE(metadata.st_mode),
            )
        except OSError:
            item["_identity"] = None
        item["age_basis"] = "tree-mtime" if observed else None
        item["age_seconds"] = (
            max(0, int((self.now - observed).total_seconds()))
            if observed
            else None
        )
        reasons: list[str] = []
        expected_marker = {
            "format": "atrinik-sound-playtest-tree",
            "playtest_only": True,
            "publishable": False,
            "schema_version": 1,
        }
        try:
            build_root = worktree / "build"
            cache_root = build_root / "atrinik-workspace"
            expected_parent = (
                cache_root.resolve(strict=False)
            )
            marker = path / PLAYTEST_MARKER
            if (
                not re.fullmatch(r"[0-9a-f]{20}", path.name)
                or worktree.is_symlink()
                or build_root.is_symlink()
                or cache_root.is_symlink()
                or path.parent.resolve(strict=False) != expected_parent
                or path.is_symlink()
                or not path.is_dir()
                or marker.is_symlink()
                or not marker.is_file()
                or load_json(marker) != expected_marker
            ):
                reasons.append("invalid_sound_cache_ownership")
        except (OSError, RuntimeError, WorkspaceError):
            reasons.append("invalid_sound_cache_ownership")
        if check_lock:
            lock = path.parent / f".{path.name}.build.lock"
            busy, lock_error = self._lock_busy(lock)
            if busy:
                reasons.append("active_build_lock")
            if lock_error:
                reasons.append("invalid_build_lock")
                item["error"] = lock_error
        if error:
            reasons.append("filesystem_traversal_error")
            item["error"] = error
        if observed is None:
            reasons.append("age_unknown")
        elif observed > self.now:
            reasons.append("future_timestamp")
        elif (self.now - observed).total_seconds() < older_than_days * 86400:
            reasons.append("retained_by_age")
        item["reasons"] = reasons or ["stale_sound_cache"]
        item["disposition"] = "eligible" if not reasons else "skipped"
        return item

    def _shared_cache(
        self,
        kind: str,
        purpose: str,
        older_than_days: int,
        reference_errors: set[str],
        *,
        legacy_allowed: bool,
    ) -> dict[str, Any] | None:
        path = self.paths.builds / kind
        if not path.exists() and not path.is_symlink():
            return None
        item = _base_item(kind, "atrinik", "atrinik/atrinik", path)
        item["_purpose"] = purpose
        if self.paths.builds.is_symlink() or not self.paths.builds.is_dir():
            item["reasons"] = ["invalid_cache_path"]
            item["legacy_known_cache"] = False
            return item
        metadata_path = path / CACHE_METADATA
        inodes, observed, walk_error = _tree_usage(path)
        item["_inodes"] = inodes
        if walk_error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = walk_error
        if not _workspace_owned(self.paths):
            item["reasons"].append("invalid_workspace_marker")
        try:
            valid_path = (
                not self.paths.builds.is_symlink()
                and self.paths.builds.is_dir()
                and not path.is_symlink()
                and path.is_dir()
                and path.resolve(strict=False)
                == self.paths.builds.resolve(strict=False) / kind
            )
        except (OSError, RuntimeError):
            valid_path = False
        if not valid_path:
            item["reasons"].append("invalid_cache_path")
        marker = path / MANAGED_MARKER
        legacy = legacy_allowed and not marker.exists() and not marker.is_symlink()
        if not legacy:
            try:
                if marker.is_symlink() or load_json(marker) != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": purpose,
                }:
                    raise WorkspaceError(f"{kind} marker is invalid")
            except WorkspaceError as error:
                item["reasons"].append("invalid_managed_marker")
                item["error"] = str(error)
        item["legacy_known_cache"] = legacy
        busy, lock_error = self._any_build_lock_busy()
        if lock_error:
            item["reasons"].append("build_lock_error")
            item["error"] = lock_error
        elif busy:
            item["reasons"].append("active_build")
        item["reasons"].extend(sorted(reference_errors))
        if metadata_path.exists() or metadata_path.is_symlink():
            try:
                if metadata_path.is_symlink() or not metadata_path.is_file():
                    raise WorkspaceError("cache metadata is not a regular file")
                metadata = load_json(metadata_path)
                expected_fields = {"schema_version", "purpose", "last_used_at"}
                if kind == "compiler-cache":
                    expected_fields.add("max_size")
                if (
                    not isinstance(metadata, dict)
                    or set(metadata) != expected_fields
                    or metadata.get("schema_version")
                    not in {
                        LEGACY_BUILD_METADATA_SCHEMA_VERSION,
                        BUILD_METADATA_SCHEMA_VERSION,
                    }
                    or metadata.get("purpose") != purpose
                    or (
                        kind == "compiler-cache"
                        and metadata.get("max_size") != COMPILER_CACHE_MAX_SIZE
                    )
                ):
                    raise WorkspaceError("cache metadata fields are invalid")
                used_at = _parse_time(metadata["last_used_at"], "cache last_used_at")
                item["age_basis"] = "last-used-at"
            except (OSError, WorkspaceError) as error:
                item["reasons"].append("invalid_cache_metadata")
                item["error"] = str(error)
                used_at = None
        else:
            if legacy_allowed:
                used_at = observed
                item["age_basis"] = "legacy-tree-mtime"
            else:
                used_at = None
                item["reasons"].append("invalid_cache_metadata")
                item["error"] = f"cache metadata is missing: {metadata_path}"
        if used_at is None:
            item["reasons"].append("cache_age_unavailable")
        else:
            age = max(0, int((self.now - used_at).total_seconds()))
            item["age_seconds"] = age
            if used_at > self.now:
                item["reasons"].append("future_last_used")
            elif age < older_than_days * 86400:
                item["reasons"].append("younger_than_grace_period")
        item["reasons"] = sorted(set(item["reasons"]))
        if not item["reasons"]:
            item["disposition"] = "eligible"
            item["reasons"] = [f"stale_{kind.replace('-', '_')}"]
        return item

    def _temporary_states(self, older_than_days: int) -> list[dict[str, Any]]:
        if self.paths.topologies.is_symlink() or not self.paths.topologies.is_dir():
            return []
        items: list[dict[str, Any]] = []
        try:
            topologies = sorted(self.paths.topologies.iterdir())
        except OSError:
            return []
        for topology in topologies:
            container = topology / "temporary-states"
            if not container.exists() and not container.is_symlink():
                continue
            if container.is_symlink() or not container.is_dir():
                item = _base_item(
                    "temporary-state", "atrinik", "atrinik/atrinik", container
                )
                item["reasons"] = ["invalid_temporary_state_container"]
                items.append(item)
                continue
            try:
                children = sorted(container.iterdir())
            except OSError as error:
                item = _base_item(
                    "temporary-state", "atrinik", "atrinik/atrinik", container
                )
                item["reasons"] = ["temporary_state_inventory_error"]
                item["error"] = str(error)
                items.append(item)
                continue
            for path in children:
                if path.name == MANAGED_MARKER or path.name.endswith(".lock"):
                    continue
                pending = re.fullmatch(
                    r"\.([0-9a-f]{64})\.removal-pending", path.name
                )
                if pending:
                    logical = path.parent / pending.group(1)
                    items.append(
                        self._temporary_state_item(
                            logical, older_than_days, physical_path=path
                        )
                    )
                elif re.fullmatch(
                    r"\.remove-[0-9a-f]{16}-[0-9a-f]+-[0-9a-f]+", path.name
                ):
                    try:
                        try:
                            creation = load_json(path / TEMPORARY_STATE_METADATA)
                            policy = creation["state_policy"]
                        except (OSError, WorkspaceError):
                            status = self.workspace.topology_status(topology.name)
                            policy = status["state_policy"]
                            if policy.get("lifecycle") not in {
                                "removal-pending",
                                "removed",
                            }:
                                raise WorkspaceError(
                                    "temporary state removal status is invalid"
                                )
                        logical = Path(policy["path"])
                        identity = policy["identity"]
                        pending_path = logical.parent / (
                            f".{logical.name}.removal-pending"
                        )
                        if path not in {
                            _owned_tree_tombstone_path(logical, identity),
                            _owned_tree_tombstone_path(pending_path, identity),
                        }:
                            raise WorkspaceError(
                                "temporary state removal tombstone is invalid"
                            )
                        items.append(
                            self._temporary_state_item(
                                logical, older_than_days, physical_path=path
                            )
                        )
                    except (KeyError, OSError, TypeError, WorkspaceError):
                        items.append(
                            self._temporary_state_item(path, older_than_days)
                        )
                else:
                    items.append(self._temporary_state_item(path, older_than_days))
            represented = {item["path"] for item in items}
            try:
                status = self.workspace.topology_status(topology.name)
                policy = status.get("state_policy")
                if (
                    isinstance(policy, dict)
                    and policy.get("mode") == "temporary"
                    and policy.get("lifecycle")
                    in {"removal-pending", "removed"}
                    and policy.get("path") not in represented
                ):
                    logical = Path(policy["path"])
                    lock = Path(f"{logical}.lock")
                    lease_identity = policy["lease_identity"]
                    lock_tombstone = lock.parent / (
                        f".{lock.name}.remove-{lease_identity['device']:x}-"
                        f"{lease_identity['inode']:x}"
                    )
                    if policy["lifecycle"] == "removal-pending" or (
                        lock.exists()
                        or lock.is_symlink()
                        or lock_tombstone.exists()
                        or lock_tombstone.is_symlink()
                    ):
                        items.append(
                            self._detached_temporary_state_item(
                                topology.name, status, older_than_days
                            )
                        )
            except (KeyError, OSError, TypeError, WorkspaceError):
                pass
        return items

    def _detached_temporary_state_item(
        self,
        topology: str,
        status: dict[str, Any],
        older_than_days: int,
    ) -> dict[str, Any]:
        policy = status["state_policy"]
        path = Path(policy["path"])
        item = _base_item(
            "temporary-state", "atrinik", "atrinik/atrinik", path
        )
        item["topology"] = topology
        item["generation"] = policy["owner"]["generation"]
        item["state_policy"] = policy
        lifecycle = policy["lifecycle"]
        item["_lease_only"] = lifecycle == "removed"
        item["_identity"] = None
        item["_physical_path"] = None
        if path.exists() or path.is_symlink():
            item["reasons"].append("temporary_state_reappeared")
        if lifecycle == "removal-pending":
            item["reasons"].append(
                "temporary_state_ownership_evidence_missing"
            )
        lock = Path(f"{path}.lock")
        lease_identity = policy["lease_identity"]
        lock_tombstone = lock.parent / (
            f".{lock.name}.remove-{lease_identity['device']:x}-"
            f"{lease_identity['inode']:x}"
        )
        if lock.exists() or lock.is_symlink():
            busy, lock_error, observed_identity = self._state_lock_observation(lock)
            if lock_error:
                item["reasons"].append("state_lease_error")
                item["error"] = lock_error
            elif busy:
                item["reasons"].append("active_state_lease")
            elif observed_identity != lease_identity:
                item["reasons"].append("state_lease_identity_mismatch")
        elif lock_tombstone.exists() or lock_tombstone.is_symlink():
            metadata = lock_tombstone.stat(follow_symlinks=False)
            if (
                not stat.S_ISREG(metadata.st_mode)
                or metadata.st_nlink != 1
                or {"device": metadata.st_dev, "inode": metadata.st_ino}
                != lease_identity
            ):
                item["reasons"].append("state_lease_identity_mismatch")
        else:
            item["reasons"].append("state_lease_unverifiable")
        observation = status.get("observation")
        liveness = [
            status["supervisor"].get("liveness"),
            *(service.get("liveness") for service in status["services"].values()),
        ]
        if any(value in {"live", "unreachable"} for value in liveness):
            item["reasons"].append("topology_liveness_unverifiable")
        if not isinstance(observation, dict):
            item["reasons"].append("topology_observation_unverifiable")
        else:
            if observation.get("control") == "reachable":
                item["reasons"].append("reachable_topology_control")
            if observation.get("process_tree_lease") != "released":
                item["reasons"].append("process_tree_lease_unverifiable")
            if observation.get("runtime_bundle_lease") not in {
                "released",
                "historical",
            }:
                item["reasons"].append("runtime_bundle_lease_unverifiable")
            endpoint = status.get("endpoint")
            port = observation.get("port_reservation")
            if (
                not isinstance(endpoint, dict)
                or not isinstance(port, dict)
                or port.get("port") != endpoint.get("port")
                or port.get("owner") != topology
                or port.get("generation") != item["generation"]
                or port.get("lease") != "released"
            ):
                item["reasons"].append(
                    "port_reservation_lease_unverifiable"
                )
        created = _parse_time(policy.get("created_at"), "temporary state created_at")
        age = max(0, int((self.now - created).total_seconds()))
        item["age_seconds"] = age
        item["age_basis"] = "created-at"
        if created > self.now:
            item["reasons"].append("future_creation_time")
        elif age < older_than_days * 86400:
            item["reasons"].append("younger_than_grace_period")
        item["reasons"] = sorted(set(item["reasons"]))
        if not item["reasons"]:
            item["disposition"] = "eligible"
            item["reasons"] = ["stale_removed_temporary_state_lease"]
        return item

    def _temporary_state_item(
        self,
        path: Path,
        older_than_days: int,
        *,
        check_lock: bool = True,
        held_lease_identity: dict[str, int] | None = None,
        physical_path: Path | None = None,
    ) -> dict[str, Any]:
        item = _base_item(
            "temporary-state", "atrinik", "atrinik/atrinik", path
        )
        pending_path = path.parent / f".{path.name}.removal-pending"
        physical = physical_path or (
            pending_path
            if not (path.exists() or path.is_symlink())
            and (pending_path.exists() or pending_path.is_symlink())
            else path
        )
        item["_physical_path"] = str(physical)
        inodes, observed, walk_error = _temporary_tree_usage(physical)
        item["_inodes"] = inodes
        if walk_error:
            item["reasons"].append("filesystem_traversal_error")
            item["error"] = walk_error
        try:
            metadata = physical.lstat()
            item["_identity"] = (
                metadata.st_dev,
                metadata.st_ino,
                metadata.st_ctime_ns,
                stat.S_IFMT(metadata.st_mode),
            )
            if physical.is_symlink() or not stat.S_ISDIR(metadata.st_mode):
                raise WorkspaceError("temporary state is not a normal directory")
            container = path.parent
            topology = container.parent
            topology_name = topology.name
            generation = path.name
            if (
                not re.fullmatch(r"[0-9a-f]{64}", generation)
                or container.name != "temporary-states"
                or topology.parent.resolve(strict=False)
                != self.paths.topologies.resolve(strict=False)
            ):
                raise WorkspaceError("temporary state path identity is invalid")
            if load_json(container / MANAGED_MARKER) != {
                "schema_version": SCHEMA_VERSION,
                "purpose": "topology-temporary-states",
            }:
                raise WorkspaceError("temporary state container marker is invalid")
            if load_json(topology / MANAGED_MARKER) != {
                "schema_version": SCHEMA_VERSION,
                "purpose": f"topology:{topology_name}",
            }:
                raise WorkspaceError("temporary state topology marker is invalid")
            empty_removal_tombstone = physical != path and not any(
                physical.iterdir()
            )
            if empty_removal_tombstone:
                status = self.workspace.topology_status(topology_name)
                status_policy = status.get("state_policy")
                if (
                    not isinstance(status_policy, dict)
                    or status_policy.get("lifecycle")
                    not in {"removal-pending", "removed"}
                    or status_policy.get("identity")
                    != {"device": metadata.st_dev, "inode": metadata.st_ino}
                ):
                    raise WorkspaceError(
                        "temporary state removal status is invalid"
                    )
                creation_policy = {
                    key: status_policy[key]
                    for key in (
                        "mode",
                        "path",
                        "owner",
                        "created_at",
                        "identity",
                        "implementation",
                        "profile",
                        "server",
                    )
                }
                creation_policy.update(
                    {"name": None, "lifecycle": "disposable"}
                )
                record: object = {
                    "schema_version": TEMPORARY_STATE_SCHEMA_VERSION,
                    "state_policy": creation_policy,
                }
            else:
                if load_json(physical / MANAGED_MARKER) != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "temporary-topology-state",
                    "topology": topology_name,
                    "generation": generation,
                }:
                    raise WorkspaceError("temporary state ownership marker is invalid")
                record = load_json(physical / TEMPORARY_STATE_METADATA)
                creation_policy = (
                    record.get("state_policy") if isinstance(record, dict) else None
                )
            if (
                not isinstance(record, dict)
                or record.get("schema_version") != TEMPORARY_STATE_SCHEMA_VERSION
                or not isinstance(creation_policy, dict)
                or creation_policy.get("mode") != "temporary"
                or creation_policy.get("name") is not None
                or creation_policy.get("lifecycle") != "disposable"
                or creation_policy.get("path") != str(path)
                or creation_policy.get("owner")
                != {
                    "kind": "topology-generation",
                    "topology": topology_name,
                    "generation": generation,
                }
                or creation_policy.get("identity")
                != {"device": metadata.st_dev, "inode": metadata.st_ino}
            ):
                raise WorkspaceError("temporary state metadata is invalid")
            item["topology"] = topology_name
            item["generation"] = generation
            policy = creation_policy
            observed_lease_identity = held_lease_identity
            registered_state = False
            for value in self.workspace._load_states().values():
                registered = self.workspace._canonical_state_path(Path(value))
                if registered == path:
                    registered_state = True
                    break
                if registered.exists() and not registered.is_symlink() and (
                    self.workspace._state_identity(registered)
                    == {"device": metadata.st_dev, "inode": metadata.st_ino}
                ):
                    registered_state = True
                    break
            if registered_state:
                item["reasons"].append("registered_state")
            if not empty_removal_tombstone:
                state_fd = os.open(
                    physical,
                    os.O_RDONLY
                    | os.O_CLOEXEC
                    | os.O_DIRECTORY
                    | os.O_NOFOLLOW,
                )
                try:
                    opened = os.fstat(state_fd)
                    if (opened.st_dev, opened.st_ino) != (
                        metadata.st_dev,
                        metadata.st_ino,
                    ):
                        raise WorkspaceError(
                            "temporary state changed during integrity validation"
                        )
                    try:
                        self.workspace._validate_temporary_state_integrity(
                            state_fd, physical
                        )
                    except WorkspaceError as error:
                        message = str(error)
                        item["reasons"].append(
                            "linked_state"
                            if "link" in message or "mounted" in message
                            else "malformed_state"
                        )
                        item["error"] = message
                finally:
                    os.close(state_fd)
            if check_lock:
                state_lock = Path(f"{path}.lock")
                if not state_lock.exists() and not state_lock.is_symlink():
                    item["reasons"].append("state_lease_unverifiable")
                else:
                    busy, lock_error, observed_lease_identity = (
                        self._state_lock_observation(state_lock)
                    )
                    if lock_error:
                        item["reasons"].append("state_lease_error")
                        item["error"] = lock_error
                    elif busy:
                        item["reasons"].append("active_state_lease")
            status_path = topology / "status.json"
            if not status_path.exists() and not status_path.is_symlink():
                item["reasons"].append("topology_status_uncertain")
            else:
                try:
                    status = self.workspace.topology_status(topology_name)
                    control = status.get("control")
                    observation = status.get("observation")
                    current_generation = (
                        control.get("generation")
                        if isinstance(control, dict)
                        else None
                    )
                    if current_generation != generation:
                        item["reasons"].append("topology_generation_mismatch")
                    else:
                        status_policy = status.get("state_policy")
                        if not isinstance(status_policy, dict) or not (
                            self.workspace._temporary_state_metadata_matches(
                                status_policy, creation_policy
                            )
                        ):
                            item["reasons"].append(
                                "topology_state_record_mismatch"
                            )
                        else:
                            policy = status_policy
                            if (
                                observed_lease_identity is None
                                or status_policy.get("lease_identity")
                                != observed_lease_identity
                            ):
                                item["reasons"].append(
                                    "state_lease_identity_mismatch"
                                )
                        liveness = [
                            status["supervisor"].get("liveness"),
                            *(
                                service.get("liveness")
                                for service in status["services"].values()
                            ),
                        ]
                        if any(value == "live" for value in liveness):
                            item["reasons"].append("live_topology")
                        if any(value == "unreachable" for value in liveness):
                            item["reasons"].append("unreachable_topology")
                        if not isinstance(observation, dict):
                            item["reasons"].append(
                                "topology_observation_unverifiable"
                            )
                        else:
                            if observation.get("control") == "reachable":
                                item["reasons"].append(
                                    "reachable_topology_control"
                                )
                            if observation.get("process_tree_lease") != "released":
                                item["reasons"].append(
                                    "process_tree_lease_unverifiable"
                                )
                            if observation.get("runtime_bundle_lease") not in {
                                "released",
                                "historical",
                            }:
                                item["reasons"].append(
                                    "runtime_bundle_lease_unverifiable"
                                )
                            endpoint = status.get("endpoint")
                            port = observation.get("port_reservation")
                            if (
                                not isinstance(endpoint, dict)
                                or not isinstance(port, dict)
                                or port.get("port") != endpoint.get("port")
                                or port.get("owner") != topology_name
                                or port.get("generation") != generation
                                or port.get("lease") != "released"
                            ):
                                item["reasons"].append(
                                    "port_reservation_lease_unverifiable"
                                )
                except (OSError, RuntimeError, WorkspaceError) as error:
                    item["reasons"].append("topology_status_uncertain")
                    item["error"] = str(error)
            item["state_policy"] = policy
            lifecycle = policy.get("lifecycle")
            if lifecycle in {"retained", "promotion-pending", "promoted"}:
                item["reasons"].append(f"temporary_state_{lifecycle.replace('-', '_')}")
            elif lifecycle in {"removal-pending", "removed"} and physical != path:
                pass
            elif lifecycle != "disposable":
                item["reasons"].append("invalid_temporary_state_lifecycle")
            created = _parse_time(policy.get("created_at"), "temporary state created_at")
            age = max(0, int((self.now - created).total_seconds()))
            item["age_seconds"] = age
            item["age_basis"] = "created-at"
            if created > self.now:
                item["reasons"].append("future_creation_time")
            elif age < older_than_days * 86400:
                item["reasons"].append("younger_than_grace_period")
        except (OSError, RuntimeError, WorkspaceError) as error:
            item["reasons"].append("invalid_temporary_state")
            item["error"] = str(error)
            if item["age_seconds"] is None and observed is not None:
                item["age_seconds"] = max(
                    0, int((self.now - observed).total_seconds())
                )
                item["age_basis"] = "tree-mtime"
        item["reasons"] = sorted(set(item["reasons"]))
        if not item["reasons"]:
            item["disposition"] = "eligible"
            item["reasons"] = ["stale_abandoned_temporary_state"]
        return item

    def _remove_temporary_state(
        self, item: dict[str, Any], older_than_days: int
    ) -> None:
        path = Path(item["path"])
        topology = item.get("topology")
        if not isinstance(topology, str):
            raise WorkspaceError("temporary state topology identity is missing")
        root = self.workspace._topology_directory(topology)
        if item.get("_lease_only"):
            with exclusive_lock(
                root / "operation.lock",
                f"topology {topology} operation",
                nonblocking=True,
            ):
                status = self.workspace.topology_status(topology)
                policy = status.get("state_policy")
                if (
                    not isinstance(policy, dict)
                    or policy.get("mode") != "temporary"
                    or policy.get("lifecycle") != "removed"
                    or policy.get("path") != str(path)
                ):
                    raise WorkspaceError(
                        "removed temporary state lease status changed before cleanup"
                    )
                lock = Path(f"{path}.lock")
                if lock.exists() or lock.is_symlink():
                    with exclusive_lock(
                        lock,
                        f"temporary topology state {path}",
                        nonblocking=True,
                    ) as state_lease:
                        self.workspace._unlink_temporary_state_lock(
                            path, state_lease, policy["lease_identity"]
                        )
                elif not self.workspace._finish_temporary_state_lock_tombstone(
                    path, policy["lease_identity"]
                ):
                    raise WorkspaceError(
                        f"removed temporary state lease is missing: {lock}"
                    )
            return
        with exclusive_lock(
            root / "operation.lock",
            f"topology {topology} operation",
            nonblocking=True,
        ):
            with exclusive_lock(
                Path(f"{path}.lock"),
                f"temporary topology state {path}",
                nonblocking=True,
            ) as state_lease:
                lease_metadata = os.fstat(state_lease.fileno())
                if (
                    not stat.S_ISREG(lease_metadata.st_mode)
                    or lease_metadata.st_nlink != 1
                ):
                    raise WorkspaceError(
                        f"temporary state lease identity is invalid: {path}.lock"
                    )
                current = self._temporary_state_item(
                    path,
                    older_than_days,
                    check_lock=False,
                    held_lease_identity={
                        "device": lease_metadata.st_dev,
                        "inode": lease_metadata.st_ino,
                    },
                    physical_path=(
                        Path(item["_physical_path"])
                        if not path.exists()
                        else None
                    ),
                )
                if (
                    current.get("_identity") != item.get("_identity")
                    or current["disposition"] != "eligible"
                ):
                    raise WorkspaceError(
                        f"temporary state changed before removal: {path}: "
                        f"{current.get('error', current['reasons'])}"
                    )
                status = self.workspace.topology_status(topology)
                policy = status["state_policy"]
                pending = self.workspace._temporary_state_removal_path(policy)
                removal_tombstone = _owned_tree_tombstone_path(
                    pending, policy["identity"]
                )
                mutation_path = next(
                    (
                        candidate
                        for candidate in (path, pending, removal_tombstone)
                        if candidate.exists() or candidate.is_symlink()
                    ),
                    None,
                )
                mutation_fd = (
                    self.workspace._lock_state_directory_mutation(
                        mutation_path, policy["identity"]
                    )
                    if mutation_path is not None
                    else None
                )
                try:
                    self.workspace._commit_temporary_state_removal(
                        topology, status, state_lease, mutation_fd
                    )
                finally:
                    if mutation_fd is not None:
                        os.close(mutation_fd)

    def _any_build_lock_busy(self) -> tuple[bool, str | None]:
        locks = self.paths.builds / "locks"
        if not locks.exists() and not locks.is_symlink():
            return False, None
        if locks.is_symlink() or not locks.is_dir():
            return False, f"build lock directory is invalid: {locks}"
        try:
            for path in sorted(locks.iterdir()):
                busy, error = self._lock_busy(path)
                if error or busy:
                    return busy, error
        except OSError as error:
            return False, str(error)
        return False, None

    @staticmethod
    def _credit_sizes(items: list[dict[str, Any]]) -> None:
        claimed: set[tuple[int, int]] = set()
        for item in items:
            inodes = item.get("_inodes", {})
            item["allocated_bytes"] = sum(
                size for inode, size in inodes.items() if inode not in claimed
            )
            claimed.update(inodes)

    @staticmethod
    def _summary(items: list[dict[str, Any]]) -> dict[str, int]:
        summary = {
            "item_count": len(items),
            "candidate_count": 0,
            "candidate_bytes": 0,
            "protected_count": 0,
            "protected_bytes": 0,
            "skipped_count": 0,
            "skipped_bytes": 0,
            "removed_count": 0,
            "removed_bytes": 0,
            "error_count": 0,
            "error_bytes": 0,
        }
        for item in items:
            disposition = item["disposition"]
            prefix = "candidate" if disposition == "eligible" else disposition
            summary[f"{prefix}_count"] += 1
            summary[f"{prefix}_bytes"] += item["allocated_bytes"]
        return summary

    @staticmethod
    def _apply_order(item: dict[str, Any]) -> tuple[int, str]:
        order = {
            "topology": -1,
            "profile-build": 0,
            "worker-dependencies": 0,
            "worker-dependency-transaction": 0,
            "worktree": 1,
            "npm-cache": 2,
            "compiler-cache": 2,
            "sound-cache": 0,
            "temporary-state": 0,
            "prunable-metadata": 3,
        }
        return order.get(item["kind"], 99), item["path"]

    def _remove(self, item: dict[str, Any], older_than_days: int = 0) -> None:
        path = Path(item["path"])
        if item["kind"] == "profile-build":
            lock = self.paths.builds / "locks" / f"{item['profile']}-{item['key']}.lock"
            with exclusive_lock(
                lock,
                f"profile build {item['profile']}",
                nonblocking=True,
            ):
                managed_remove(
                    path,
                    self.paths.builds,
                    f"profile:{item['profile']}:{item['key']}",
                )
        elif item["kind"] == "topology":
            with exclusive_lock(
                path / "operation.lock",
                f"topology {item['name']} operation",
                nonblocking=True,
            ):
                current = self._topology_item(
                    path,
                    older_than_days,
                    check_operation=False,
                )
                if not self._same_topology_snapshot(item, current):
                    raise WorkspaceError(
                        f"topology changed before removal: {item['name']}"
                    )
                marker = path / MANAGED_MARKER
                if load_json(marker) != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": f"topology:{item['name']}",
                }:
                    raise WorkspaceError(
                        f"topology ownership changed before removal: {item['name']}"
                    )
                remove_owned_tree(path)
        elif item["kind"] == "worker-dependencies":
            lock = (
                self.paths.builds
                / "locks"
                / f"worker-dependencies-{item['key']}.lock"
            )
            with exclusive_lock(
                lock,
                f"Worker dependencies {item['key']}",
                nonblocking=True,
            ):
                expected = (
                    self.paths.builds / "worker-dependencies" / item["key"]
                ).resolve(strict=False)
                if path.resolve(strict=False) != expected:
                    raise WorkspaceError(
                        f"Worker dependency cache path changed before removal: {path}"
                    )
                metadata = load_json(path / WORKER_DEPENDENCY_METADATA)
                if (
                    not isinstance(metadata, dict)
                    or metadata.get("last_used_at") != item.get("last_used_at")
                ):
                    raise WorkspaceError(
                        "Worker dependency cache was refreshed before removal"
                    )
                used_at = _parse_time(
                    metadata["last_used_at"], "Worker dependencies last_used_at"
                )
                age = max(0, int((self.now - used_at).total_seconds()))
                if used_at > self.now or age < older_than_days * 86400:
                    raise WorkspaceError(
                        "Worker dependency cache is no longer old enough to remove"
                    )
                marker = path / MANAGED_MARKER
                if (
                    path.is_symlink()
                    or not path.is_dir()
                    or marker.is_symlink()
                    or not marker.is_file()
                    or load_json(marker)
                    != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"worker-dependencies:{item['key']}",
                    }
                ):
                    raise WorkspaceError(
                        "Worker dependency cache ownership changed before removal"
                    )
                remove_owned_tree(path)
        elif item["kind"] == "worker-dependency-transaction":
            key = item["key"]
            lock = (
                self.paths.builds / "locks" / f"worker-dependencies-{key}.lock"
            )
            with exclusive_lock(
                lock,
                f"Worker dependencies {key}",
                nonblocking=True,
            ):
                transaction_root = (
                    self.paths.builds / "worker-dependencies" / ".transactions"
                )
                marker = transaction_root / MANAGED_MARKER
                if (
                    transaction_root.is_symlink()
                    or not transaction_root.is_dir()
                    or marker.is_symlink()
                    or load_json(marker)
                    != {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": "worker-dependency-transactions",
                    }
                    or path.parent.resolve(strict=False)
                    != transaction_root.resolve(strict=False)
                    or not re.fullmatch(
                        rf"{key}-(?:staging|backup)-[a-z0-9_]+", path.name
                    )
                    or path.is_symlink()
                    or not path.is_dir()
                ):
                    raise WorkspaceError(
                        f"Worker dependency transaction changed before removal: {path}"
                    )
                current = self._worker_dependency_transaction_item(
                    path,
                    older_than_days,
                    check_lock=False,
                )
                if (
                    current.get("_identity") != item.get("_identity")
                    or current["disposition"] != "eligible"
                ):
                    raise WorkspaceError(
                        f"Worker dependency transaction changed before removal: {path}"
                    )
                remove_owned_tree(path)
        elif item["kind"] == "worktree":
            primary = self._repositories[item["owner"]]
            if item["owner"] != "sound":
                _command(primary, "worktree", "remove", "--", str(path))
            else:
                common = _git_common_directory(primary)
                expected_worktree_identity = item.get(
                    "_sound_worktree_identity"
                )
                expected_producer_identity = item.get(
                    "_sound_producer_identity"
                )
                if (
                    expected_worktree_identity is None
                    or expected_producer_identity is None
                    or _sound_worktree_identity(path, common)
                    != expected_worktree_identity
                ):
                    raise WorkspaceError(
                        "sound worktree identity changed before removal"
                    )
                with _exclusive_sound_producer_lease(
                    path, expected_producer_identity
                ):
                    if (
                        _sound_worktree_identity(path, common)
                        != expected_worktree_identity
                    ):
                        raise WorkspaceError(
                            "sound worktree identity changed while locking"
                        )
                    build_root = path / "build"
                    cache_root = build_root / "atrinik-workspace"
                    if cache_root.exists() or cache_root.is_symlink():
                        if (
                            build_root.is_symlink()
                            or cache_root.is_symlink()
                            or not cache_root.is_dir()
                        ):
                            raise WorkspaceError(
                                "sound cache root changed before worktree removal"
                            )
                        for child in sorted(cache_root.iterdir()):
                            if not re.fullmatch(
                                r"\.[0-9a-f]{20}\.build\.lock", child.name
                            ):
                                raise WorkspaceError(
                                    "sound cache remains before worktree removal"
                                )
                            busy, error = self._lock_busy(child)
                            if busy or error:
                                raise WorkspaceError(
                                    "sound cache lock changed before worktree removal"
                                )
                    _command(primary, "worktree", "remove", "--", str(path))
        elif item["kind"] in {"npm-cache", "compiler-cache"}:
            purpose = item["kind"]
            if item.get("legacy_known_cache"):
                marker = path / MANAGED_MARKER
                if marker.exists() or marker.is_symlink():
                    raise WorkspaceError("legacy npm cache marker appeared before removal")
                atomic_json(
                    marker,
                    {"schema_version": SCHEMA_VERSION, "purpose": purpose},
                )
            managed_remove(path, self.paths.builds, purpose)
        elif item["kind"] == "sound-cache":
            checkout_path = Path(item["checkout_path"])
            git_common = item.get("_git_common")
            producer_identity = item.get("_sound_producer_identity")
            if not isinstance(git_common, str) or (
                _sound_worktree_identity(checkout_path, Path(git_common))
                != item.get("_worktree_identity")
            ) or producer_identity is None:
                raise WorkspaceError("sound worktree identity changed before removal")
            expected_parent = (
                checkout_path / "build" / "atrinik-workspace"
            ).resolve(strict=False)
            if path.parent.resolve(strict=False) != expected_parent:
                raise WorkspaceError("sound cache path changed before removal")
            lock = path.parent / f".{path.name}.build.lock"
            with _exclusive_sound_producer_lease(
                checkout_path, producer_identity
            ):
                with exclusive_lock(
                    lock, f"sound cache {path.name}", nonblocking=True
                ):
                    current = self._sound_cache_item(
                        path, checkout_path, older_than_days, check_lock=False
                    )
                    if (
                        current["disposition"] != "eligible"
                        or current.get("_identity") != item.get("_identity")
                    ):
                        raise WorkspaceError("sound cache changed before removal")
                    remove_owned_tree(path)
        elif item["kind"] == "temporary-state":
            self._remove_temporary_state(item, older_than_days)
        elif item["kind"] == "prunable-metadata":
            primary = self._repositories[item["owner"]]
            _command(primary, "worktree", "prune", "--expire", "now")
        else:
            raise WorkspaceError(f"unsupported cleanup target: {item['kind']}")
