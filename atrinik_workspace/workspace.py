from __future__ import annotations

import binascii
from collections import deque
from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import ExitStack, contextmanager
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
import platform
from pathlib import Path, PurePosixPath
import re
import secrets
import shlex
import shutil
import signal
import socket
import ssl
import stat
import subprocess
import sys
import tempfile
import time
from typing import Any, Callable, Iterator, TextIO

from .launch_identity import CLIENT_LAUNCH_LABEL_ENV, client_launch_label

from .model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
    Checkout,
    Component,
    Manifest,
    Paths,
    WorkspaceError,
    atomic_json,
    load_json,
    managed_directory,
    managed_reset,
    profile_key,
    require_keys,
    validate_name,
)
from .migration import (
    MIGRATED_CONTENT_WORKTREE_KIND,
    RepositoryMigration,
    classic_lineage,
    rename_no_replace,
)
from .supervisor import process_matches


PROFILE_SCHEMA_VERSION = 3
PROFILE_KEYS = {"schema_version", "name", "stack", "components"}
SELECTOR_KEYS = {"kind", "value"}
EXPECTED_SERVER_DATA = {
    "files": ("bans", "motd"),
    "directories": ("keys", "unique-items"),
}
SENSITIVE_ARGUMENTS = {"--join_password", "--join-password"}
SENSITIVE_PREFIXES = ("--join_password=", "--join-password=")
ALL_BUILD_TARGETS = (
    "content",
    "protocol",
    "libatrinik",
    "client",
    "server",
    "metaserver-worker",
)
TOPOLOGY_SERVICES = ("server", "client")
PRE_MONOREPO_REPOSITORIES = {
    "client": "legacy-client",
    "server": "legacy-server",
    "editor": "legacy-editor",
    "libatrinik": "legacy-libatrinik",
    "protocol": "legacy-protocol",
}
RESOURCE_PATHS_MANIFEST = "runtime-paths.txt"
SERVER_IDENTITY_MAX_SIZE = 64 * 1024
SCENARIO_KEYS = {
    "schema_version",
    "name",
    "profile",
    "stack",
    "providers",
    "preset",
    "state",
    "account",
    "character",
    "archetype",
    "resolved",
    "provisioned_at",
}
SCENARIO_SCHEMA_VERSION = 4
SCENARIO_PRESETS = {"basic-player": {"archetype": "human_male"}}
SCENARIO_PASSWORD_MAX_SIZE = 128
BUILD_METADATA = ".atrinik-build.json"
BUILD_METADATA_SCHEMA_VERSION = 1
CACHE_METADATA = ".atrinik-cache.json"
WORKER_DEPENDENCY_METADATA = ".atrinik-worker-dependencies.json"
WORKER_VIEW_METADATA = ".atrinik-worker-view.json"
WORKER_DEPENDENCY_SCHEMA_VERSION = 3
WORKER_VIEW_SCHEMA_VERSION = 1
WORKER_DEPENDENCY_FILES = ("package.json", "package-lock.json")
WORKER_SOURCE_EXCLUSIONS = {
    ".git",
    MANAGED_MARKER,
    WORKER_VIEW_METADATA,
    "build",
    "dist",
    "node_modules",
    ".wrangler",
}
WORKER_VIEW_NODE_MODULES_EXCLUSIONS = {".vite", ".vite-temp"}
WORKER_NPM_FILE_CONFIG_KEYS = {
    "cafile",
    "certfile",
    "globalconfig",
    "keyfile",
    "userconfig",
}
REGION_MAP_METADATA = ".atrinik-region-maps.json"
REGION_MAP_SCHEMA_VERSION = 1
EXPECTED_REGION_MAP = "incuna_-1"


def display_arguments(arguments: list[str]) -> str:
    displayed: list[str] = []
    redact_next = False
    for argument in arguments:
        if redact_next:
            displayed.append("<redacted>")
            redact_next = False
        elif argument in SENSITIVE_ARGUMENTS:
            displayed.append(argument)
            redact_next = True
        elif argument.startswith(SENSITIVE_PREFIXES):
            displayed.append(argument.split("=", 1)[0] + "=<redacted>")
        else:
            displayed.append(argument)
    return shlex.join(displayed)


def run(
    arguments: list[str],
    *,
    cwd: Path | None = None,
    capture: bool = False,
    env: dict[str, str] | None = None,
    trace: bool = True,
    diagnostics_to_stderr: bool = True,
) -> str:
    if trace:
        print(f"+ {display_arguments(arguments)}", file=sys.stderr)
    try:
        result = subprocess.run(
            arguments,
            cwd=cwd,
            check=True,
            text=True,
            capture_output=capture,
            env=env,
            stdout=sys.stderr if diagnostics_to_stderr and not capture else None,
        )
    except FileNotFoundError as error:
        raise WorkspaceError(f"required command not found: {arguments[0]}") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.strip() if capture and error.stderr else ""
        suffix = f": {detail}" if detail else ""
        raise WorkspaceError(
            f"command failed ({error.returncode}): {display_arguments(arguments)}{suffix}"
        ) from error
    return result.stdout.strip() if capture else ""


def git(
    path: Path, *arguments: str, capture: bool = False, trace: bool = True
) -> str:
    return run(["git", "-C", str(path), *arguments], capture=capture, trace=trace)


def replace_directory(
    output: Path,
    staging: Path,
    backup_prefix: str,
    backup_parent: Path | None = None,
) -> None:
    if output.exists():
        backup = Path(
            tempfile.mkdtemp(prefix=backup_prefix, dir=backup_parent or output.parent)
        )
        backup.rmdir()
        output.replace(backup)
        try:
            staging.replace(output)
        except BaseException:
            backup.replace(output)
            raise
        shutil.rmtree(backup)
    else:
        staging.replace(output)


def _file_digest(path: Path, description: str) -> str:
    descriptor: int | None = None
    try:
        descriptor = open_regular_file(path, os.O_RDONLY, description)
        digest = hashlib.sha256()
        with os.fdopen(descriptor, "rb") as stream:
            descriptor = None
            while chunk := stream.read(1024 * 1024):
                digest.update(chunk)
        return digest.hexdigest()
    except WorkspaceError:
        raise
    except OSError as error:
        raise WorkspaceError(f"cannot read {description} {path}: {error}") from error
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _tree_digest(
    root: Path,
    exclusions: set[str],
    *,
    bounded_symlinks: bool = False,
    reject_symlinks: bool = False,
    copied_metadata: bool = False,
) -> str:
    """Hash a tree as framed records without following links."""

    digest = hashlib.sha256()
    resolved_root = root.resolve()

    def record(*fields: object) -> None:
        encoded = json.dumps(
            fields, ensure_ascii=True, separators=(",", ":")
        ).encode()
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)

    def extended_attributes(path: Path) -> tuple[tuple[str, int, str], ...]:
        if not hasattr(os, "listxattr"):
            return ()
        try:
            values = []
            for name in sorted(os.listxattr(path, follow_symlinks=False)):
                value = os.getxattr(path, name, follow_symlinks=False)
                values.append(
                    (name, len(value), hashlib.sha256(value).hexdigest())
                )
            return tuple(values)
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect Worker source metadata {path}: {error}"
            ) from error

    def visit(directory: Path, relative: PurePosixPath) -> None:
        try:
            entries = sorted(directory.iterdir(), key=lambda entry: entry.name)
        except OSError as error:
            raise WorkspaceError(
                f"cannot inventory Worker source {directory}: {error}"
            ) from error
        for entry in entries:
            if not relative.parts and entry.name in exclusions:
                continue
            child = relative / entry.name
            try:
                status = entry.lstat()
                mode = stat.S_IMODE(status.st_mode)
                metadata: tuple[object, ...] = (
                    (
                        status.st_mtime_ns,
                        getattr(status, "st_flags", 0),
                        extended_attributes(entry),
                    )
                    if copied_metadata
                    else ()
                )
                if stat.S_ISDIR(status.st_mode):
                    record(
                        "directory",
                        child.as_posix(),
                        mode,
                        *metadata,
                    )
                    visit(entry, child)
                elif stat.S_ISREG(status.st_mode):
                    record(
                        "file",
                        child.as_posix(),
                        mode,
                        status.st_size,
                        _file_digest(entry, "Worker tree file"),
                        *metadata,
                    )
                elif stat.S_ISLNK(status.st_mode):
                    target = os.readlink(entry)
                    if reject_symlinks:
                        raise WorkspaceError(
                            f"Worker source contains a symbolic link: {entry}"
                        )
                    if bounded_symlinks:
                        if Path(target).is_absolute():
                            raise WorkspaceError(
                                f"Worker dependencies contain an absolute link: {entry}"
                            )
                        resolved = entry.parent.joinpath(target).resolve(strict=False)
                        try:
                            resolved.relative_to(resolved_root)
                        except ValueError as error:
                            raise WorkspaceError(
                                f"Worker dependencies contain an escaping link: {entry}"
                            ) from error
                        if not resolved.exists():
                            raise WorkspaceError(
                                f"Worker dependencies contain a dangling link: {entry}"
                            )
                    record(
                        "symlink",
                        child.as_posix(),
                        mode,
                        target,
                        *metadata,
                    )
                else:
                    raise WorkspaceError(
                        f"Worker source contains an unsupported file type: {entry}"
                    )
            except WorkspaceError:
                raise
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect Worker source {entry}: {error}"
                ) from error

    if root.is_symlink() or not root.is_dir():
        raise WorkspaceError(f"Worker source is not a regular directory: {root}")
    root_status = root.lstat()
    root_metadata: tuple[object, ...] = (
        (
            root_status.st_mtime_ns,
            getattr(root_status, "st_flags", 0),
            extended_attributes(root),
        )
        if copied_metadata
        else ()
    )
    record("root", stat.S_IMODE(root_status.st_mode), *root_metadata)
    visit(root, PurePosixPath())
    return digest.hexdigest()


def _copy_worker_source(
    source: Path,
    destination: Path,
    *,
    include_npmrc: bool = True,
) -> None:
    """Copy Worker source while excluding generated names only at its root."""

    def ignore(directory: str, names: list[str]) -> list[str]:
        if Path(directory) != source:
            return []
        excluded = set(names) & WORKER_SOURCE_EXCLUSIONS
        if not include_npmrc and Path(directory) == source and ".npmrc" in names:
            excluded.add(".npmrc")
        return sorted(excluded)

    shutil.copytree(
        source,
        destination,
        dirs_exist_ok=True,
        symlinks=True,
        ignore=ignore,
    )


def _copy_regular_file(source: Path, destination: Path, description: str) -> None:
    """Copy one no-follow regular file without inheriting extended metadata."""

    source_descriptor = open_regular_file(source, os.O_RDONLY, description)
    destination_descriptor: int | None = None
    try:
        source_status = os.fstat(source_descriptor)
        mode = stat.S_IMODE(source_status.st_mode)
        destination_descriptor = os.open(
            destination,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL,
            mode,
        )
        os.fchmod(destination_descriptor, mode)
        with os.fdopen(source_descriptor, "rb") as source_stream:
            source_descriptor = -1
            with os.fdopen(destination_descriptor, "wb") as destination_stream:
                destination_descriptor = None
                shutil.copyfileobj(source_stream, destination_stream)
        os.utime(
            destination,
            ns=(source_status.st_atime_ns, source_status.st_mtime_ns),
            follow_symlinks=False,
        )
    except OSError as error:
        raise WorkspaceError(f"cannot stage {description} {source}: {error}") from error
    finally:
        if source_descriptor >= 0:
            os.close(source_descriptor)
        if destination_descriptor is not None:
            os.close(destination_descriptor)


def _normalize_worker_atime(root: Path) -> None:
    """Give lifecycle staging a deterministic initial access time."""

    paths = [root]
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        paths.extend(parent / name for name in (*directories, *files))
    for path in reversed(paths):
        try:
            status = path.lstat()
            os.utime(
                path,
                ns=(0, status.st_mtime_ns),
                follow_symlinks=False,
            )
        except OSError as error:
            raise WorkspaceError(
                f"cannot normalize Worker lifecycle access time {path}: {error}"
            ) from error


def _tree_references_path(root: Path, referenced: Path) -> bool:
    """Return whether a no-follow tree stores one absolute path literally."""

    needle = str(referenced).encode()
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        for name in (*directories, *files):
            path = parent / name
            status = path.lstat()
            if stat.S_ISLNK(status.st_mode):
                if needle in os.readlink(path).encode(
                    "utf-8", "surrogateescape"
                ):
                    return True
            elif stat.S_ISREG(status.st_mode):
                descriptor = open_regular_file(
                    path, os.O_RDONLY, "Worker installed file"
                )
                with os.fdopen(descriptor, "rb") as stream:
                    previous = b""
                    while chunk := stream.read(1024 * 1024):
                        combined = previous + chunk
                        if needle in combined:
                            return True
                        previous = combined[-max(0, len(needle) - 1) :]
    return False


def _remote_matches(url: str, repository: str) -> bool:
    normalized = url.strip().removesuffix(".git")
    expected = f"github.com/{repository}"
    if normalized.startswith("git@github.com:"):
        normalized = "github.com/" + normalized.removeprefix("git@github.com:")
    elif normalized.startswith("ssh://git@github.com/"):
        normalized = "github.com/" + normalized.removeprefix("ssh://git@github.com/")
    elif normalized.startswith("https://"):
        normalized = normalized.removeprefix("https://")
    return normalized == expected


def _github_clone_url(template: str, repository: str) -> str | None:
    template = template.strip()
    if template.startswith("git@github.com:"):
        return f"git@github.com:{repository}.git"
    if template.startswith("ssh://git@github.com/"):
        return f"ssh://git@github.com/{repository}.git"
    if template.startswith("https://github.com/"):
        return f"https://github.com/{repository}.git"
    return None


def _is_clean(path: Path, *, trace: bool = True) -> bool:
    return not git(
        path,
        "status",
        "--porcelain=v1",
        "--untracked-files=all",
        capture=True,
        trace=trace,
    )


def _worktree_records(
    repository: Path, *, trace: bool = True
) -> list[dict[str, str]]:
    output = git(
        repository, "worktree", "list", "--porcelain", capture=True, trace=trace
    )
    records: list[dict[str, str]] = []
    current: dict[str, str] = {}
    for line in [*output.splitlines(), ""]:
        if not line:
            if current:
                records.append(current)
                current = {}
            continue
        key, _, value = line.partition(" ")
        current[key] = value
    return records


def open_regular_file(
    path: Path, flags: int, description: str, mode: int = 0o600
) -> int:
    flags |= os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags, mode)
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):
            os.close(descriptor)
            raise WorkspaceError(f"{description} is not a regular file: {path}")
        return descriptor
    except OSError as error:
        raise WorkspaceError(f"cannot open {description} {path}: {error}") from error


@contextmanager
def exclusive_lock(
    path: Path, description: str, nonblocking: bool = False
) -> Iterator[TextIO]:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor = open_regular_file(
        path, os.O_RDWR | os.O_CREAT, f"{description} lock"
    )
    with os.fdopen(descriptor, "a+") as lock:
        operation = fcntl.LOCK_EX | (fcntl.LOCK_NB if nonblocking else 0)
        try:
            fcntl.flock(lock, operation)
        except BlockingIOError as error:
            raise WorkspaceError(f"{description} is already in use") from error
        yield lock


class Workspace:
    def __init__(self, repository: Path):
        self.paths = Paths.discover(repository)
        self.manifest = Manifest.load(self.paths.repository / "components.json")

    def migrate_repositories(self, mode: str) -> dict[str, Any]:
        if mode == "apply":
            self.paths.ensure()
        return RepositoryMigration(
            self.paths.repository, self.paths, self.manifest
        ).execute(mode)

    def cleanup(
        self,
        scopes: list[str],
        older_than_days: int,
        names: list[str],
        apply: bool,
    ) -> dict[str, Any]:
        # Import lazily so the planner can reuse the workspace lock and metadata
        # helpers without creating a module import cycle.
        from .cleanup import Cleanup

        return Cleanup(self).execute(scopes, older_than_days, names, apply)

    def _checkout_identity(self, value: Checkout | Component) -> Checkout:
        if isinstance(value, Checkout):
            return value
        return self.manifest.checkout_for(value)

    def _primary_path(self, value: Checkout | Component) -> Path:
        checkout = self._checkout_identity(value)
        return self.paths.repositories / checkout.path

    def _operation_checkouts(
        self,
        names: list[str] | None,
        include_classic: bool,
    ) -> list[Checkout]:
        requested: set[str] = set()
        if names:
            unknown: list[str] = []
            for name in names:
                if name in self.manifest.by_checkout:
                    requested.add(name)
                elif name in self.manifest.by_name:
                    requested.add(self.manifest.by_name[name].checkout_name)
                else:
                    unknown.append(name)
            if unknown:
                raise WorkspaceError(
                    f"unknown components or checkouts: {', '.join(sorted(set(unknown)))}"
                )
        else:
            requested.update(self.manifest.cohorts["default"])
        if include_classic:
            requested.update(self.manifest.cohorts["default"])
            requested.update(self.manifest.cohorts["classic"])
        return [
            checkout
            for checkout in self.manifest.checkouts
            if checkout.name in requested
        ]

    def initialize(
        self,
        names: list[str] | None = None,
        jobs: int = 4,
        *,
        include_classic: bool = False,
    ) -> None:
        self.paths.ensure()
        checkouts = self._operation_checkouts(names, include_classic)
        failures: list[str] = []
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            # Validate every occupied destination before starting any clone.
            # A pre-split classic checkout at a canonical replacement path
            # must stop the entire operation without leaving a partially
            # initialized replacement cohort behind.
            for checkout in checkouts:
                destination = self._primary_path(checkout)
                if destination.exists() or destination.is_symlink():
                    self._validate_primary_checkout(checkout, destination)
            with ThreadPoolExecutor(
                max_workers=max(1, min(jobs, len(checkouts)))
            ) as executor:
                futures = {
                    executor.submit(self._ensure_repository, checkout): checkout
                    for checkout in checkouts
                }
                for future in as_completed(futures):
                    checkout = futures[future]
                    try:
                        future.result()
                        print(f"{checkout.name}: ready")
                    except Exception as error:
                        failures.append(f"{checkout.name}: {error}")
        if failures:
            raise WorkspaceError(
                "repository initialization failed:\n" + "\n".join(sorted(failures))
            )

    def _ensure_repository(self, value: Checkout | Component) -> Path:
        checkout = self._checkout_identity(value)
        destination = self._primary_path(checkout)
        if not destination.exists() and not destination.is_symlink():
            temporary = Path(
                tempfile.mkdtemp(
                    prefix=f".atrinik-clone-{checkout.name}-",
                    dir=self.paths.repositories,
                )
            )
            try:
                run(
                    [
                        "git",
                        "clone",
                        "--branch",
                        checkout.branch,
                        "--single-branch",
                        "--",
                        self._component_clone_url(checkout),
                        str(temporary),
                    ]
                )
                self._validate_primary_checkout(checkout, temporary)
                if destination.exists() or destination.is_symlink():
                    raise WorkspaceError(
                        f"component destination appeared during clone: {destination}"
                    )
                rename_no_replace(temporary, destination)
            except BaseException:
                shutil.rmtree(temporary, ignore_errors=True)
                raise
        self._validate_primary_checkout(checkout, destination)
        return destination

    def _validate_primary_checkout(
        self, value: Checkout | Component, path: Path, *, trace: bool = True
    ) -> str:
        checkout = self._checkout_identity(value)
        remote = self._validate_checkout(checkout, path, trace=trace)
        branch = git(
            path, "branch", "--show-current", capture=True, trace=trace
        )
        if branch != checkout.branch:
            raise WorkspaceError(
                f"primary checkout must be on {checkout.branch}, found "
                f"{branch or 'detached'}: {path}"
            )
        return remote

    def _component_clone_url(self, value: Checkout | Component) -> str:
        checkout = self._checkout_identity(value)
        for remote in ("origin", "upstream"):
            try:
                urls = git(
                    self.paths.repository,
                    "remote",
                    "get-url",
                    "--all",
                    remote,
                    capture=True,
                    trace=False,
                ).splitlines()
            except WorkspaceError:
                continue
            for url in urls:
                clone_url = _github_clone_url(url, checkout.repository)
                if clone_url is not None:
                    return clone_url
        return f"https://github.com/{checkout.repository}.git"

    def _canonical_remote(
        self, value: Checkout | Component, path: Path, *, trace: bool = True
    ) -> str:
        checkout = self._checkout_identity(value)
        for remote in ("origin", "upstream"):
            try:
                urls = git(
                    path,
                    "remote",
                    "get-url",
                    "--all",
                    remote,
                    capture=True,
                    trace=trace,
                ).splitlines()
            except WorkspaceError:
                continue
            # Git fetches the first URL when a remote has multiple fetch URLs.
            # Do not accept a later canonical-looking URL while fetching a fork
            # or unrelated repository from the effective first URL.
            if urls and _remote_matches(urls[0], checkout.repository):
                return remote
        raise WorkspaceError(
            f"checkout has no origin/upstream for {checkout.repository}: {path}"
        )

    def _validate_checkout(
        self, value: Checkout | Component, path: Path, *, trace: bool = True
    ) -> str:
        checkout = self._checkout_identity(value)
        if path.is_symlink() or not path.is_dir():
            raise WorkspaceError(f"component checkout is not a directory: {path}")
        try:
            inside = git(
                path,
                "rev-parse",
                "--is-inside-work-tree",
                capture=True,
                trace=trace,
            )
        except WorkspaceError as error:
            raise WorkspaceError(f"component is not a Git checkout: {path}") from error
        if inside != "true":
            raise WorkspaceError(f"component is not a Git worktree: {path}")
        top_level = Path(
            git(
                path,
                "rev-parse",
                "--show-toplevel",
                capture=True,
                trace=trace,
            )
        ).resolve()
        if top_level != path.resolve():
            raise WorkspaceError(f"component path must be the Git worktree root: {path}")
        self._validate_checkout_lineage(checkout, path)
        try:
            return self._canonical_remote(checkout, path, trace=trace)
        except WorkspaceError as error:
            historical_name = PRE_MONOREPO_REPOSITORIES.get(checkout.name)
            if historical_name is not None:
                historical_repository = f"atrinik/{historical_name}"
                for remote in ("origin", "upstream"):
                    try:
                        urls = git(
                            path,
                            "remote",
                            "get-url",
                            "--all",
                            remote,
                            capture=True,
                            trace=trace,
                        ).splitlines()
                    except WorkspaceError:
                        continue
                    if urls and _remote_matches(urls[0], historical_repository):
                        raise WorkspaceError(
                            f"replacement path contains pre-monorepo classic history: "
                            f"{path}; run ./atrinik migrate repositories --dry-run"
                        ) from error
            raise

    def _validate_checkout_lineage(self, checkout: Checkout, path: Path) -> None:
        old_name = checkout.name if checkout.name in PRE_MONOREPO_REPOSITORIES else None
        if old_name is None:
            return
        try:
            actual_classic = classic_lineage(path, old_name)
        except WorkspaceError as error:
            raise WorkspaceError(
                f"cannot prove repository history for {checkout.name}: {path}: {error}"
            ) from error
        if actual_classic:
            raise WorkspaceError(
                f"replacement path contains pre-monorepo classic history: {path}; "
                "run ./atrinik migrate repositories --dry-run"
            )

    def repository_status(self, names: list[str] | None = None) -> list[dict[str, Any]]:
        """Return quiet, machine-readable primary-checkout status."""
        self.paths.ensure()
        rows: list[dict[str, Any]] = []
        checkouts = (
            self._operation_checkouts(names, False)
            if names
            else self.manifest.checkouts
        )
        for checkout in checkouts:
            modules = [
                component
                for component in self.manifest.components
                if component.checkout_name == checkout.name
            ]
            path = self._primary_path(checkout)
            row: dict[str, Any] = {
                "component": checkout.name,
                "checkout": checkout.name,
                "repository": checkout.repository,
                "default_branch": checkout.branch,
                "destination": checkout.path,
                "cohorts": sorted(self.manifest.checkout_cohorts(checkout.name)),
                "stacks": sorted(self.manifest.checkout_stacks(checkout.name)),
                "modules": [component.name for component in modules],
                "roles": sorted(
                    {role for component in modules for role in component.provides}
                ),
                "license": checkout.license,
                "optional": checkout.name not in self.manifest.cohorts["default"],
                "path": str(path),
                "initialized": False,
                "branch": None,
                "head": None,
                "dirty": None,
                "remote": None,
                "ahead": None,
                "behind": None,
            }
            if not path.exists() and not path.is_symlink():
                rows.append(row)
                continue
            remote = self._validate_primary_checkout(checkout, path, trace=False)
            row.update(
                {
                    "initialized": True,
                    "branch": git(
                        path, "branch", "--show-current", capture=True, trace=False
                    )
                    or None,
                    "head": git(
                        path,
                        "rev-parse",
                        "--short=12",
                        "HEAD",
                        capture=True,
                        trace=False,
                    ),
                    "dirty": not _is_clean(path, trace=False),
                    "remote": remote,
                }
            )
            try:
                counts = git(
                    path,
                    "rev-list",
                    "--left-right",
                    "--count",
                    f"HEAD...{remote}/{checkout.branch}",
                    capture=True,
                    trace=False,
                ).split()
                if len(counts) == 2:
                    row["ahead"], row["behind"] = (int(value) for value in counts)
            except (ValueError, WorkspaceError):
                # A newly created or deliberately minimal checkout may not yet
                # have a cached remote default-branch ref.
                pass
            rows.append(row)
        return rows

    def sync(
        self,
        names: list[str] | None,
        worktree_strategy: str,
        *,
        include_classic: bool = False,
    ) -> None:
        self.paths.ensure()
        if worktree_strategy not in {"none", "merge", "rebase"}:
            raise WorkspaceError(f"unknown worktree strategy: {worktree_strategy}")
        checkouts = self._operation_checkouts(names, include_classic)
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            self._sync_components(checkouts, names, worktree_strategy)

    def _sync_components(
        self,
        checkouts: list[Checkout],
        explicit_names: list[str] | None,
        worktree_strategy: str,
    ) -> None:
        migrated_content_worktrees = (
            self._migrated_content_worktree_paths()
            if worktree_strategy != "none"
            and any(checkout.name == "content" for checkout in checkouts)
            else set()
        )
        prepared: list[
            tuple[Checkout, Path, str, list[Path], list[Path]]
        ] = []
        explicitly_requested_checkouts = {
            checkout.name
            for checkout in self._operation_checkouts(explicit_names, False)
        } if explicit_names else set()
        for checkout in checkouts:
            repository = self._primary_path(checkout)
            if not repository.exists() and not repository.is_symlink():
                if checkout.name in explicitly_requested_checkouts:
                    raise WorkspaceError(
                        f"component is not initialized; run ./atrinik init "
                        f"{checkout.name}: {repository}"
                    )
                continue
            self._validate_primary_checkout(checkout, repository)
            if not _is_clean(repository):
                raise WorkspaceError(f"refusing to update dirty primary checkout: {repository}")
            remote = self._canonical_remote(checkout, repository)
            candidates: list[Path] = []
            skipped: list[Path] = []
            if worktree_strategy != "none":
                excluded = (
                    migrated_content_worktrees
                    if checkout.name == "content"
                    else set()
                )
                candidates, skipped = self._component_worktrees(
                    repository, excluded
                )
            prepared.append((checkout, repository, remote, candidates, skipped))
        for checkout, repository, remote, candidates, skipped in prepared:
            git(repository, "fetch", "--prune", "--tags", remote)
            git(repository, "merge", "--ff-only", f"{remote}/{checkout.branch}")
            print(f"{checkout.name}: primary synchronized")
            for path in skipped:
                print(
                    f"{checkout.name}: skipped migration-only classic worktree "
                    f"{path}"
                )
            if worktree_strategy != "none":
                self._sync_component_worktrees(
                    checkout, candidates, worktree_strategy
                )

    def _migrated_content_worktree_paths(self) -> set[Path]:
        """Return profile-owned classic worktrees that main must never update."""

        if not self.paths.profiles.is_dir():
            return set()
        component = self.manifest.by_name.get("content-1x")
        if component is None:
            return set()
        protected: set[Path] = set()
        for path in sorted(self.paths.profiles.glob("*.json")):
            profile = self._load_profile(path.stem, require_file=True)
            selector = profile["components"].get("content-1x")
            if selector is None or selector["kind"] != MIGRATED_CONTENT_WORKTREE_KIND:
                continue
            selected = Path(selector["value"]).resolve()
            self._validate_selected_checkout(
                component,
                selected,
                MIGRATED_CONTENT_WORKTREE_KIND,
                trace=False,
            )
            protected.add(selected)
        return protected

    def _component_worktrees(
        self, repository: Path, excluded: set[Path] | None = None
    ) -> tuple[list[Path], list[Path]]:
        primary = repository.resolve()
        excluded = excluded or set()
        candidates: list[Path] = []
        skipped: list[Path] = []
        for record in _worktree_records(repository):
            path = Path(record["worktree"]).resolve()
            if path == primary or "branch" not in record:
                continue
            if path in excluded:
                skipped.append(path)
                continue
            if not _is_clean(path):
                raise WorkspaceError(f"refusing to update dirty worktree: {path}")
            candidates.append(path)
        return candidates, skipped

    def _sync_component_worktrees(
        self, checkout: Checkout, candidates: list[Path], strategy: str
    ) -> None:
        for path in candidates:
            if strategy == "merge":
                git(path, "merge", "--no-edit", checkout.branch)
            elif strategy == "rebase":
                git(path, "rebase", checkout.branch)
            print(f"{checkout.name}: updated {path}")

    def create_worktree(
        self,
        component_name: str,
        label: str,
        branch: str,
        start_point: str | None,
        existing: bool,
    ) -> Path:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._create_worktree(
                component_name, label, branch, start_point, existing
            )

    def _create_worktree(
        self,
        component_name: str,
        label: str,
        branch: str,
        start_point: str | None,
        existing: bool,
    ) -> Path:
        self.paths.ensure()
        validate_name(label, "worktree label")
        checkout = self._resolve_checkout(component_name)
        repository = self._ensure_repository(checkout)
        remote = self._canonical_remote(checkout, repository)
        run(["git", "check-ref-format", "--branch", branch], capture=True)
        destination = self.paths.worktrees / checkout.name / label
        if destination.exists():
            raise WorkspaceError(f"worktree destination already exists: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if existing:
            git(repository, "worktree", "add", "--", str(destination), branch)
        else:
            if start_point is not None and start_point.startswith("-"):
                raise WorkspaceError("worktree start point must not begin with '-'")
            point = start_point or f"{remote}/{checkout.branch}"
            git(repository, "fetch", "--prune", remote)
            commit = git(
                repository,
                "rev-parse",
                "--verify",
                "--end-of-options",
                f"{point}^{{commit}}",
                capture=True,
            )
            git(
                repository,
                "worktree",
                "add",
                "-b",
                branch,
                "--",
                str(destination),
                commit,
            )
        self._validate_checkout(checkout, destination)
        print(destination)
        return destination

    def remove_worktree(self, component_name: str, label: str) -> None:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            self._remove_worktree(component_name, label)

    def _remove_worktree(self, component_name: str, label: str) -> None:
        self.paths.ensure()
        validate_name(label, "worktree label")
        checkout = self._resolve_checkout(component_name)
        repository = self._ensure_repository(checkout)
        candidates = [self.paths.worktrees / checkout.name / label]
        existing = [candidate.resolve() for candidate in candidates if candidate.is_dir()]
        if len(existing) != 1:
            rendered = ", ".join(str(candidate) for candidate in candidates)
            raise WorkspaceError(f"worktree does not exist unambiguously: {rendered}")
        destination = existing[0]
        expected_parents = {
            (self.paths.worktrees / checkout.name).resolve(),
        }
        if destination.parent not in expected_parents:
            raise WorkspaceError(f"invalid managed worktree path: {destination}")
        if not _is_clean(destination):
            raise WorkspaceError(f"refusing to remove dirty worktree: {destination}")
        git(repository, "worktree", "remove", str(destination))

    def list_worktrees(self, names: list[str] | None = None) -> list[tuple[str, dict[str, str]]]:
        self.paths.ensure()
        result: list[tuple[str, dict[str, str]]] = []
        checkouts = (
            self._operation_checkouts(names, False)
            if names
            else self.manifest.checkouts
        )
        for checkout in checkouts:
            repository = self._primary_path(checkout)
            if not repository.is_dir():
                continue
            self._validate_checkout(checkout, repository, trace=False)
            result.extend(
                (checkout.name, record)
                for record in _worktree_records(repository, trace=False)
            )
        return result

    def create_profile(self, name: str, source: str = "default") -> Path:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._create_profile(name, source)

    def _create_profile(self, name: str, source: str = "default") -> Path:
        self.paths.ensure()
        validate_name(name, "profile name")
        if name in self.manifest.stacks:
            raise WorkspaceError(f"{name} is a built-in profile")
        path = self.paths.profiles / f"{name}.json"
        if path.exists():
            raise WorkspaceError(f"profile already exists: {name}")
        source_profile = self._load_profile(source, require_file=False)
        value = {
            "schema_version": PROFILE_SCHEMA_VERSION,
            "name": name,
            "stack": source_profile["stack"],
            "components": {
                component_name: dict(selector)
                for component_name, selector in source_profile["components"].items()
            },
        }
        atomic_json(path, value)
        print(path)
        return path

    def set_profile(
        self, name: str, component_name: str, kind: str, value: str = ""
    ) -> None:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            self._set_profile(name, component_name, kind, value)

    def _set_profile(
        self, name: str, component_name: str, kind: str, value: str = ""
    ) -> None:
        self.paths.ensure()
        profile = self._load_profile(name, require_file=True)
        components = self._profile_components(profile, component_name)
        checkout_names = {component.checkout_name for component in components}
        if len(checkout_names) != 1:
            raise WorkspaceError(
                f"profile selection spans multiple checkouts: {component_name}"
            )
        checkout = self.manifest.by_checkout[checkout_names.pop()]
        if kind == "primary":
            value = ""
        elif kind == "worktree":
            validate_name(value, "worktree label")
            path = self.paths.worktrees / checkout.name / value
            self._validate_selected_checkout(components[0], path, kind)
        elif kind == "path":
            path = Path(value).expanduser()
            if not path.is_absolute():
                raise WorkspaceError("profile checkout path must be absolute")
            self._validate_selected_checkout(components[0], path, kind)
            value = str(path.resolve())
        else:
            raise WorkspaceError(f"invalid profile selector kind: {kind}")
        for component in components:
            profile["components"][component.name] = {"kind": kind, "value": value}
        atomic_json(self.paths.profiles / f"{name}.json", profile)

    def _profile_components(
        self, profile: dict[str, Any], component_checkout_or_role: str
    ) -> list[Component]:
        stack = self.manifest.stack(profile["stack"])
        if component_checkout_or_role in self.manifest.by_checkout:
            selected = [
                component
                for component in stack.components
                if component.checkout_name == component_checkout_or_role
            ]
            if selected:
                return selected
        component = self._profile_component(profile, component_checkout_or_role)
        return [
            candidate
            for candidate in stack.components
            if candidate.checkout_name == component.checkout_name
        ]

    def _profile_component(
        self, profile: dict[str, Any], component_or_role: str
    ) -> Component:
        stack = self.manifest.stack(profile["stack"])
        components = {component.name: component for component in stack.components}
        if component_or_role in components:
            return components[component_or_role]
        provider = stack.providers.get(component_or_role)
        if provider is not None:
            return provider
        raise WorkspaceError(
            f"component or role is not part of {stack.name} stack: "
            f"{component_or_role}"
        )

    def _selector_root(
        self, profile: dict[str, Any], component: Component
    ) -> Path:
        selector = profile["components"][component.name]
        if selector["kind"] == "primary":
            return self._primary_path(component)
        if selector["kind"] == "worktree":
            return self.paths.worktrees / component.checkout_name / selector["value"]
        return Path(selector["value"])

    def _component_source(self, component: Component, checkout_root: Path) -> Path:
        if checkout_root.is_symlink() or not checkout_root.is_dir():
            raise WorkspaceError(
                f"component checkout is not a directory: {checkout_root}"
            )
        root = checkout_root.resolve()
        parts = (
            ()
            if component.source == "."
            else PurePosixPath(component.source).parts
        )
        source = checkout_root
        for part in parts:
            source = source / part
            if source.is_symlink():
                raise WorkspaceError(
                    f"component source is not a normal directory: "
                    f"{component.name}: {source}"
                )
        if not source.is_dir():
            raise WorkspaceError(
                f"component source is not a normal directory: {component.name}: {source}"
            )
        resolved = source.resolve()
        if resolved != root and root not in resolved.parents:
            raise WorkspaceError(
                f"component source escapes checkout: {component.name}: {source}"
            )
        return resolved

    def resolve_profile(
        self,
        name: str,
        component_names: set[str] | None = None,
        *,
        trace: bool = True,
    ) -> dict[str, Path]:
        self.paths.ensure()
        profile = self._load_profile(name, require_file=False)
        result: dict[str, Path] = {}
        stack = self.manifest.stack(profile["stack"])
        stack_components = {component.name: component for component in stack.components}
        if component_names is None:
            components = list(stack.components)
        else:
            unknown = sorted(component_names - set(stack_components))
            if unknown:
                raise WorkspaceError(
                    f"components are not part of {stack.name} stack: "
                    f"{', '.join(unknown)}"
                )
            components = [
                component
                for component in stack.components
                if component.name in component_names
            ]
        validated_checkouts: set[str] = set()
        for component in components:
            root = self._selector_root(profile, component)
            if component.checkout_name not in validated_checkouts:
                selector = profile["components"][component.name]
                self._validate_selected_checkout(
                    component, root, selector["kind"], trace=trace
                )
                validated_checkouts.add(component.checkout_name)
            result[component.name] = self._component_source(component, root)
        return result

    def _validate_selected_checkout(
        self,
        component: Component,
        path: Path,
        selector_kind: str,
        *,
        trace: bool = True,
    ) -> str:
        checkout = self.manifest.checkout_for(component)
        if selector_kind == "primary":
            return self._validate_primary_checkout(checkout, path, trace=trace)
        remote = self._validate_checkout(checkout, path, trace=trace)
        if selector_kind == MIGRATED_CONTENT_WORKTREE_KIND:
            if component.name != "content-1x":
                raise WorkspaceError(
                    "migration-only worktree selector is valid only for content-1x"
                )
            expected_parent = (self.paths.worktrees / "content").resolve()
            if path.resolve().parent != expected_parent:
                raise WorkspaceError(
                    "migrated content worktree must remain directly below "
                    f"{expected_parent}: {path}"
                )
            content_checkout = self.manifest.by_checkout.get("content")
            if content_checkout is None:
                raise WorkspaceError(
                    "manifest has no canonical content provider for migrated worktree"
                )
            primary = self._primary_path(content_checkout)
            if not primary.is_dir() or primary.is_symlink():
                raise WorkspaceError(
                    "cannot prove migrated content worktree lineage; initialize "
                    f"canonical content first: {primary}"
                )
            self._validate_primary_checkout(content_checkout, primary, trace=trace)
            selected_common = self._git_common_directory(path, trace=trace)
            content_common = self._git_common_directory(primary, trace=trace)
            if selected_common != content_common:
                raise WorkspaceError(
                    "migrated content worktree is no longer attached to canonical "
                    f"content: {path}"
                )
            return remote
        variants = [
            candidate
            for candidate in self.manifest.checkouts
            if candidate.repository == checkout.repository
        ]
        if len(variants) < 2:
            return remote

        branch = git(
            path, "branch", "--show-current", capture=True, trace=trace
        )
        if branch == checkout.branch:
            return remote

        primary = self._primary_path(checkout)
        if not primary.is_dir() or primary.is_symlink():
            raise WorkspaceError(
                f"cannot prove {checkout.name}@{checkout.branch} lineage for {path}; "
                f"initialize its primary checkout first: {primary}"
            )
        self._validate_primary_checkout(checkout, primary, trace=trace)
        selected_common = self._git_common_directory(path, trace=trace)
        primary_common = self._git_common_directory(primary, trace=trace)
        if selected_common != primary_common:
            raise WorkspaceError(
                f"checkout cannot be proven to belong to {checkout.name}@"
                f"{checkout.branch}: {path}; use that exact branch or a worktree "
                f"attached to {primary}"
            )
        return remote

    @staticmethod
    def _git_common_directory(path: Path, *, trace: bool = True) -> Path:
        value = Path(
            git(
                path,
                "rev-parse",
                "--git-common-dir",
                capture=True,
                trace=trace,
            )
        )
        return value.resolve() if value.is_absolute() else (path / value).resolve()

    def _load_profile(self, name: str, require_file: bool) -> dict[str, Any]:
        validate_name(name, "profile name")
        if name in self.manifest.stacks and not require_file:
            stack = self.manifest.stack(name)
            return {
                "schema_version": PROFILE_SCHEMA_VERSION,
                "name": name,
                "stack": name,
                "components": {
                    component.name: {"kind": "primary", "value": ""}
                    for component in stack.components
                },
            }
        path = self.paths.profiles / f"{name}.json"
        if not path.is_file():
            raise WorkspaceError(f"profile does not exist: {name}")
        profile = load_json(path)
        if not isinstance(profile, dict):
            raise WorkspaceError(f"profile must be an object: {name}")
        require_keys(profile, PROFILE_KEYS, f"profile {name}")
        if (
            profile["schema_version"] != PROFILE_SCHEMA_VERSION
            or profile["name"] != name
        ):
            raise WorkspaceError(f"profile identity/schema mismatch: {name}")
        stack_name = profile["stack"]
        if not isinstance(stack_name, str) or stack_name not in self.manifest.stacks:
            raise WorkspaceError(f"profile stack is invalid: {name}")
        stack = self.manifest.stack(stack_name)
        selectors = profile["components"]
        expected = {component.name for component in stack.components}
        if not isinstance(selectors, dict) or set(selectors) != expected:
            raise WorkspaceError(f"profile component set does not match manifest: {name}")
        checkout_selectors: dict[str, dict[str, Any]] = {}
        for component_name, selector in selectors.items():
            if not isinstance(selector, dict):
                raise WorkspaceError(f"profile selector must be an object: {component_name}")
            require_keys(selector, SELECTOR_KEYS, f"profile selector {component_name}")
            kind = selector["kind"]
            value = selector["value"]
            if kind not in {
                "primary",
                "worktree",
                "path",
                MIGRATED_CONTENT_WORKTREE_KIND,
            } or not isinstance(value, str):
                raise WorkspaceError(f"invalid profile selector: {component_name}")
            if kind == "primary" and value:
                raise WorkspaceError(f"primary selector must not have a value: {component_name}")
            if kind == "worktree":
                validate_name(value, f"profile selector {component_name}")
            if kind == "path" and not Path(value).is_absolute():
                raise WorkspaceError(f"profile path must be absolute: {component_name}")
            if kind == MIGRATED_CONTENT_WORKTREE_KIND:
                migrated = Path(value)
                expected_parent = (self.paths.worktrees / "content").resolve()
                if (
                    component_name != "content-1x"
                    or not migrated.is_absolute()
                    or migrated.resolve(strict=False).parent != expected_parent
                ):
                    raise WorkspaceError(
                        "invalid migrated content worktree selector: "
                        f"{component_name}"
                    )
            checkout_name = self.manifest.by_name[component_name].checkout_name
            previous = checkout_selectors.setdefault(checkout_name, selector)
            if selector != previous:
                raise WorkspaceError(
                    "profile selectors for components in one checkout must match: "
                    f"{name}/{checkout_name}"
                )
        return profile

    def profile_summary(self, name: str) -> dict[str, Any]:
        profile = self._load_profile(name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        rows: list[dict[str, Any]] = []
        for component in stack.components:
            selector_root = self._selector_root(profile, component)
            checkout_root = selector_root.resolve()
            path = (
                checkout_root
                if component.source == "."
                else checkout_root.joinpath(*PurePosixPath(component.source).parts)
            )
            row: dict[str, Any] = {
                "component": component.name,
                "checkout": component.checkout_name,
                "repository": component.repository,
                "branch": component.branch,
                "source": component.source,
                "roles": sorted(component.provides),
                "path": str(path),
                "checkout_path": str(checkout_root),
                "initialized": False,
                "head": None,
                "dirty": None,
            }
            if selector_root.exists() or selector_root.is_symlink():
                selector = profile["components"][component.name]
                self._validate_selected_checkout(
                    component, selector_root, selector["kind"], trace=False
                )
                path = self._component_source(component, selector_root)
                row["path"] = str(path)
                row.update(
                    {
                        "initialized": True,
                        "head": git(
                            checkout_root,
                            "rev-parse",
                            "--short=12",
                            "HEAD",
                            capture=True,
                            trace=False,
                        ),
                        "dirty": not _is_clean(checkout_root, trace=False),
                    }
                )
            rows.append(row)
        return {"name": name, "stack": stack.name, "components": rows}

    def component_path(self, component_name: str, profile_name: str) -> Path:
        profile = self._load_profile(profile_name, require_file=False)
        component = self._profile_component(profile, component_name)
        return self.resolve_profile(
            profile_name, {component.name}, trace=False
        )[component.name]

    @staticmethod
    def _classic_requires(component: Component) -> tuple[str, ...]:
        if component.requires:
            return component.requires
        return {
            "classic-client": ("sound", "libatrinik", "protocol"),
            "classic-server": ("content", "resources", "libatrinik", "protocol"),
            "classic-library": ("protocol",),
        }.get(component.build, ())

    def _dependency_roles(
        self, profile: dict[str, Any], requested: set[str]
    ) -> set[str]:
        stack = self.manifest.stack(profile["stack"])
        unknown = sorted(requested - set(stack.providers))
        if unknown:
            raise WorkspaceError(
                f"{stack.name} stack has no provider for roles: {', '.join(unknown)}"
            )
        resolved: set[str] = set()
        pending = deque(sorted(requested))
        while pending:
            role = pending.popleft()
            if role in resolved:
                continue
            resolved.add(role)
            component = stack.providers[role]
            for requirement in self._classic_requires(component):
                if requirement not in stack.providers:
                    raise WorkspaceError(
                        f"{component.name} requires role {requirement}, which has no "
                        f"provider in {stack.name} stack"
                    )
                if requirement not in resolved:
                    pending.append(requirement)
        return resolved

    def _require_classic_contracts(
        self, profile_name: str, requested: set[str]
    ) -> None:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        expected = {
            "client": "classic-client",
            "server": "classic-server",
        }
        for role in sorted(requested):
            component = stack.providers.get(role)
            adapter = expected.get(role)
            if component is None:
                raise WorkspaceError(
                    f"{stack.name} stack has no provider for runtime role {role}"
                )
            if adapter is not None and component.build != adapter:
                raise WorkspaceError(
                    f"{component.name} has no wrapper build/runtime contract yet "
                    f"for the {stack.name} stack"
                )

    def _resolve_build_profile(
        self, profile_name: str, required: set[str]
    ) -> dict[str, Path]:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        requested_roles = self._dependency_roles(profile, required)
        build_targets = {
            role
            for role, component in stack.providers.items()
            if component.build != "none"
        }
        common_roles = self._dependency_roles(profile, build_targets)
        common_components = {
            stack.providers[role].name for role in common_roles
        }

        # Missing preferred checkouts identify a deliberately partial workspace.
        # Any preferred checkout that is present must still pass full profile
        # validation, so a malformed path cannot silently shrink the build key.
        complete = True
        present_components: set[str] = set()
        for component in stack.components:
            if component.name not in common_components:
                continue
            root = self._selector_root(profile, component)
            if root.exists() or root.is_symlink():
                present_components.add(component.name)
            else:
                complete = False
        roles = (
            common_roles | requested_roles if complete else requested_roles
        )
        component_names = {stack.providers[role].name for role in roles}
        # Resolve present preferred components and the required selection in one
        # pass. This both fails closed for malformed optional checkouts and keeps
        # validation deduplicated if initialization races the existence snapshot.
        present_paths = self.resolve_profile(
            profile_name, present_components | component_names
        )
        paths = {
            component_name: present_paths[component_name]
            for component_name in component_names
        }
        return {role: paths[stack.providers[role].name] for role in roles}

    def build(self, target: str, profile_name: str, tests: bool) -> Path:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._build(target, profile_name, tests)

    def _build(self, target: str, profile_name: str, tests: bool) -> Path:
        targets = self._expand_build_target(target, profile_name)
        required = set(targets)
        selected = self._resolve_build_profile(profile_name, required)
        return self._build_resolved(target, profile_name, tests, targets, selected)

    def _build_resolved(
        self,
        target: str,
        profile_name: str,
        tests: bool,
        targets: list[str],
        selected: dict[str, Path],
    ) -> Path:
        key = self._profile_build_key(profile_name, selected)
        root = self.paths.builds / "profiles" / f"{profile_name}-{key}"
        lock = self.paths.builds / "locks" / f"{profile_name}-{key}.lock"
        with exclusive_lock(lock, f"profile build {profile_name}"):
            managed_directory(root, self.paths.builds, f"profile:{profile_name}:{key}")
            self._refresh_build_metadata(root, profile_name, key, selected)
            if "content" in targets or "server" in targets:
                self._collect_content(root, selected)
            if "server" in targets:
                self._stage_resources(root, selected)
            integrated_classic = self._uses_integrated_classic_build(
                targets, selected
            )
            if integrated_classic:
                self._build_integrated_classic(root, selected, tests)
            else:
                if "protocol" in targets:
                    self._build_protocol(root, selected, tests)
                if "libatrinik" in targets:
                    self._build_library(root, selected, tests)
                if "client" in targets:
                    self._build_client(root, selected, tests)
                if "server" in targets:
                    self._build_server(root, selected, tests)
            if "server" in targets:
                self._generate_region_maps(root, profile_name, selected)
            if "metaserver-worker" in targets:
                self._build_worker(root, selected)
            if target in {"sound", "resources"}:
                print(f"{target}: selected {selected[target]}")
        return root

    def _refresh_build_metadata(
        self,
        root: Path,
        profile_name: str,
        key: str,
        selected: dict[str, Path],
    ) -> None:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        checkout_states = self._selected_checkout_states(
            profile, selected, include_dirty=False
        )
        coordinates: dict[str, dict[str, str]] = {}
        for role in sorted(selected):
            component = stack.providers[role]
            checkout_state = checkout_states[component.checkout_name]
            checkout_path = checkout_state["path"]
            coordinates[role] = {
                "component": component.name,
                "checkout": component.checkout_name,
                "repository": component.repository,
                "branch": component.branch,
                "source": component.source,
                "checkout_path": str(checkout_path),
                "source_path": str(selected[role].resolve()),
                "head": checkout_state["head"],
            }
        atomic_json(
            root / BUILD_METADATA,
            {
                "schema_version": BUILD_METADATA_SCHEMA_VERSION,
                "profile": profile_name,
                "key": key,
                "purpose": f"profile:{profile_name}:{key}",
                "coordinates": coordinates,
                "last_used_at": datetime.now(timezone.utc).isoformat(),
            },
        )

    def _selected_checkout_states(
        self,
        profile: dict[str, Any],
        selected: dict[str, Path],
        *,
        include_dirty: bool,
    ) -> dict[str, dict[str, Any]]:
        stack = self.manifest.stack(profile["stack"])
        states: dict[str, dict[str, Any]] = {}
        for role in sorted(selected):
            component = stack.providers[role]
            if component.checkout_name in states:
                continue
            checkout = self._selector_root(profile, component).resolve()
            state: dict[str, Any] = {
                "path": checkout,
                "head": git(
                    checkout,
                    "rev-parse",
                    "HEAD",
                    capture=True,
                    trace=False,
                ),
            }
            if include_dirty:
                state["dirty"] = not _is_clean(checkout, trace=False)
            states[component.checkout_name] = state
        return states

    def _profile_build_key(
        self, profile_name: str, selected: dict[str, Path]
    ) -> str:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        providers = ",".join(
            f"{role}={stack.providers[role].name}@"
            f"{stack.providers[role].repository}@"
            f"{stack.providers[role].branch}@"
            f"{stack.providers[role].checkout_name}:"
            f"{stack.providers[role].source}"
            for role in sorted(selected)
        )
        namespace = (
            f"profile-schema:{PROFILE_SCHEMA_VERSION};stack:{stack.name};"
            f"generation:{stack.generation};providers:{providers}"
        )
        return profile_key(selected, namespace=namespace)

    def _expand_build_target(self, target: str, profile_name: str) -> list[str]:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        if target == "all":
            targets = [role for role in ALL_BUILD_TARGETS if role in stack.providers]
        elif target in stack.providers:
            targets = [target]
        else:
            component = self._profile_component(profile, target)
            targets = [
                role
                for role, provider in stack.providers.items()
                if provider.name == component.name
            ]
        for role in targets:
            component = stack.providers[role]
            if component.build == "none":
                raise WorkspaceError(
                    f"{component.name} has no wrapper build/runtime contract yet "
                    f"for the {stack.name} stack"
                )
        return targets

    def _profile_source_view(
        self,
        root: Path,
        component: str,
        source: Path,
        exclusions: set[str],
        copied_directories: set[str] | None = None,
        copy_all: bool = False,
    ) -> Path:
        view = root / "sources" / component
        managed_reset(view, self.paths.builds, f"source-view:{component}")
        exclusions = {*exclusions, MANAGED_MARKER}
        if copy_all:
            shutil.copytree(
                source,
                view,
                dirs_exist_ok=True,
                symlinks=True,
                ignore=shutil.ignore_patterns(".git", *exclusions),
            )
            return view
        copied_directories = copied_directories or set()
        for entry in source.iterdir():
            if entry.name in exclusions or entry.name == ".git":
                continue
            destination = view / entry.name
            if entry.name in copied_directories:
                if not entry.is_dir():
                    raise WorkspaceError(
                        f"source-view copy input is not a directory: {entry}"
                    )
                shutil.copytree(entry, destination, symlinks=True)
            else:
                destination.symlink_to(entry, target_is_directory=entry.is_dir())
        return view

    def _collect_content(self, root: Path, selected: dict[str, Path]) -> Path:
        output = root / "runtime" / "content"
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists():
            managed_directory(output, self.paths.builds, "collected-content")
        staging = Path(tempfile.mkdtemp(prefix=".content-", dir=output.parent))
        staging.rmdir()
        source = selected["content"]
        commit = git(source, "rev-parse", "HEAD", capture=True)
        try:
            run(
                [
                    sys.executable,
                    str(source / "tools" / "build_runtime.py"),
                    "--source",
                    str(source),
                    "--output",
                    str(staging),
                    "--source-commit",
                    commit,
                ]
            )
            atomic_json(
                staging / MANAGED_MARKER,
                {"schema_version": SCHEMA_VERSION, "purpose": "collected-content"},
            )
            atomic_json(
                staging / ".atrinik-dependency.json",
                {
                    "schema_version": SCHEMA_VERSION,
                    "workspace_source": str(source),
                    "commit": commit,
                },
            )
            if not (staging / "lib").is_dir() or not (staging / "maps").is_dir():
                raise WorkspaceError("content collection did not produce lib and maps")
        except BaseException:
            if staging.exists():
                shutil.rmtree(staging)
            raise
        replace_directory(output, staging, ".content-previous-")
        return output

    def _stage_resources(self, root: Path, selected: dict[str, Path]) -> Path:
        output = root / "runtime" / "resources"
        source = selected["resources"]
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists() or output.is_symlink():
            managed_directory(output, self.paths.builds, "resource-view")
        manifest = source / RESOURCE_PATHS_MANIFEST
        try:
            lines = manifest.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeError) as error:
            raise WorkspaceError(
                f"cannot read resource runtime manifest: {manifest}: {error}"
            ) from error
        if not lines:
            raise WorkspaceError(f"resource runtime manifest is empty: {manifest}")
        runtime_paths: list[str] = []
        for line in lines:
            path = PurePosixPath(line)
            if (
                not line
                or path.is_absolute()
                or line != path.as_posix()
                or any(part in {"", ".", ".."} for part in path.parts)
                or line in runtime_paths
            ):
                raise WorkspaceError(
                    f"invalid resource runtime path in {manifest}: {line!r}"
                )
            runtime_paths.append(line)

        try:
            result = subprocess.run(
                [
                    "git",
                    "-C",
                    str(source),
                    "ls-files",
                    "-z",
                    "--cached",
                    "--",
                    *runtime_paths,
                ],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except FileNotFoundError as error:
            raise WorkspaceError("required command not found: git") from error
        except subprocess.CalledProcessError as error:
            detail = error.stderr.decode("utf-8", errors="replace").strip()
            suffix = f": {detail}" if detail else ""
            raise WorkspaceError(f"cannot list tracked runtime resources{suffix}") from error

        try:
            tracked = [
                item.decode("utf-8")
                for item in result.stdout.split(b"\0")
                if item
            ]
        except UnicodeDecodeError as error:
            raise WorkspaceError("runtime resource paths must use UTF-8 names") from error
        if not tracked:
            raise WorkspaceError(
                f"resource runtime manifest selects no tracked files: {manifest}"
            )
        staging = Path(tempfile.mkdtemp(prefix=".resources-", dir=output.parent))
        selected_roots = tuple(PurePosixPath(path) for path in runtime_paths)
        try:
            atomic_json(
                staging / MANAGED_MARKER,
                {"schema_version": SCHEMA_VERSION, "purpose": "resource-view"},
            )
            for relative in tracked:
                path = PurePosixPath(relative)
                if not any(
                    path == root_path or root_path in path.parents
                    for root_path in selected_roots
                ):
                    raise WorkspaceError(
                        f"Git returned an unexpected resource path: {relative}"
                    )
                source_path = source.joinpath(*path.parts)
                try:
                    mode = source_path.lstat().st_mode
                except FileNotFoundError:
                    continue
                if not stat.S_ISREG(mode):
                    raise WorkspaceError(
                        f"tracked runtime resource is not a regular file: {source_path}"
                    )
                destination = staging.joinpath(*path.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_path, destination, follow_symlinks=False)
            atomic_json(
                staging / ".atrinik-dependency.json",
                {
                    "schema_version": SCHEMA_VERSION,
                    "workspace_source": str(source),
                    "commit": git(source, "rev-parse", "HEAD", capture=True),
                },
            )
        except BaseException:
            shutil.rmtree(staging)
            raise
        replace_directory(output, staging, ".resources-previous-")
        return output

    def _cmake(
        self,
        source: Path,
        binary: Path,
        arguments: list[str],
        tests: bool,
    ) -> None:
        binary.mkdir(parents=True, exist_ok=True)
        run(
            [
                "cmake",
                "-S",
                str(source),
                "-B",
                str(binary),
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Debug",
                f"-DBUILD_TESTING={'ON' if tests else 'OFF'}",
                *arguments,
            ]
        )
        run(["cmake", "--build", str(binary), "--parallel"])
        if tests:
            run(["ctest", "--test-dir", str(binary), "--output-on-failure"])

    def _build_protocol(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        self._cmake(selected["protocol"], root / "build" / "protocol", [], tests)

    @staticmethod
    def _uses_integrated_classic_build(
        targets: list[str], selected: dict[str, Path]
    ) -> bool:
        shared_roles = {"client", "server", "protocol", "libatrinik"}
        if not {"client", "server"}.issubset(targets) or not shared_roles.issubset(
            selected
        ):
            return False
        checkout = selected["client"].parent.resolve()
        return (checkout / "CMakeLists.txt").is_file() and all(
            selected[role].resolve() == checkout / role for role in shared_roles
        )

    @staticmethod
    def _record_classic_graph(root: Path, roles: set[str], graph: str) -> None:
        for role in roles:
            atomic_json(
                root / "build" / f".{role}-graph.json",
                {
                    "schema_version": 1,
                    "purpose": "classic-build-graph",
                    "graph": graph,
                },
            )

    @staticmethod
    def _classic_binary_directory(root: Path, role: str) -> Path:
        marker = root / "build" / f".{role}-graph.json"
        try:
            record = load_json(marker)
        except (OSError, ValueError, WorkspaceError):
            record = {}
        if (
            record.get("schema_version") == 1
            and record.get("purpose") == "classic-build-graph"
            and record.get("graph") == "integrated"
        ):
            return root / "build" / "integrated" / role
        return root / "build" / role

    def _build_integrated_classic(
        self, root: Path, selected: dict[str, Path], tests: bool
    ) -> None:
        checkout = selected["client"].parent.resolve()
        view = self._profile_source_view(
            root, "integrated", checkout, {"build", "client", "server"}
        )
        client = self._profile_source_view(
            root,
            "integrated/client",
            selected["client"],
            {"build", "sound"},
        )
        (client / "sound").symlink_to(selected["sound"], target_is_directory=True)
        server = self._profile_source_view(
            root,
            "integrated/server",
            selected["server"],
            {
                "atrinik-server",
                "build",
                "data",
                "lib",
                "maps",
                "resources",
                "runtime",
                "libplugin_arena.so",
                "libplugin_python.so",
            },
            {"install_data"},
        )
        runtime = server / "runtime"
        runtime.mkdir()
        (runtime / "content").symlink_to(
            root / "runtime" / "content", target_is_directory=True
        )
        (server / "resources").symlink_to(
            root / "runtime" / "resources", target_is_directory=True
        )
        self._cmake(
            view,
            root / "build" / "integrated",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            tests,
        )
        self._record_classic_graph(root, {"client", "server"}, "integrated")

    def _build_library(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        self._cmake(
            selected["libatrinik"],
            root / "build" / "libatrinik",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                f"-DATRINIK_PROTOCOL_SOURCE_DIR={selected['protocol']}",
            ],
            tests,
        )

    def _build_client(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        view = self._profile_source_view(
            root, "client", selected["client"], {"build", "sound"}
        )
        (view / "sound").symlink_to(selected["sound"], target_is_directory=True)
        self._cmake(
            view,
            root / "build" / "client",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                f"-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL={selected['protocol']}",
                f"-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK={selected['libatrinik']}",
            ],
            tests,
        )
        self._record_classic_graph(root, {"client"}, "standalone")

    def _build_server(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        view = self._profile_source_view(
            root,
            "server",
            selected["server"],
            {
                "atrinik-server",
                "build",
                "data",
                "lib",
                "maps",
                "resources",
                "runtime",
                "libplugin_arena.so",
                "libplugin_python.so",
            },
            # The server CTest setup copies this tree with CMake's file(COPY).
            # CMake treats a top-level directory symlink as the object to copy,
            # which conflicts with the destination directory it just created.
            {"install_data"},
        )
        runtime = view / "runtime"
        runtime.mkdir()
        (runtime / "content").symlink_to(
            root / "runtime" / "content", target_is_directory=True
        )
        (view / "resources").symlink_to(
            root / "runtime" / "resources", target_is_directory=True
        )
        self._cmake(
            view,
            root / "build" / "server",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                f"-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL={selected['protocol']}",
                f"-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK={selected['libatrinik']}",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            tests,
        )
        self._record_classic_graph(root, {"server"}, "standalone")

    def _region_map_inputs(
        self, profile_name: str, selected: dict[str, Path]
    ) -> tuple[dict[str, Any], bool]:
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        required = self._dependency_roles(profile, {"server"})
        coordinates: dict[str, dict[str, str]] = {}
        cacheable = True
        checkout_states: dict[Path, tuple[bool, str]] = {}
        for role in sorted(required & set(selected)):
            source = selected[role]
            component = stack.providers[role]
            checkout = self._selector_root(profile, component).resolve()
            if checkout not in checkout_states:
                checkout_states[checkout] = (
                    _is_clean(checkout, trace=False),
                    git(
                        checkout,
                        "rev-parse",
                        "HEAD",
                        capture=True,
                        trace=False,
                    ),
                )
            clean, head = checkout_states[checkout]
            cacheable = cacheable and clean
            coordinates[role] = {
                "component": component.name,
                "repository": component.repository,
                "branch": component.branch,
                "checkout": component.checkout_name,
                "source": component.source,
                "checkout_path": str(checkout),
                "source_path": str(source.resolve()),
                "head": head,
            }
        return (
            {
                "schema_version": REGION_MAP_SCHEMA_VERSION,
                "cacheable": cacheable,
                "coordinates": coordinates,
            },
            cacheable,
        )

    @staticmethod
    def _validate_region_maps(path: Path) -> None:
        if not path.is_dir() or path.is_symlink():
            raise WorkspaceError(f"region-map output is not a directory: {path}")
        png_names: set[str] = set()
        definition_names: set[str] = set()
        for entry in path.iterdir():
            if entry.name in {MANAGED_MARKER, REGION_MAP_METADATA}:
                continue
            try:
                mode = entry.lstat().st_mode
            except OSError as error:
                raise WorkspaceError(
                    f"cannot inspect generated region map {entry}: {error}"
                ) from error
            if not stat.S_ISREG(mode) or entry.suffix not in {".png", ".def"}:
                raise WorkspaceError(
                    f"generated region-map output is invalid: {entry}"
                )
            descriptor = open_regular_file(
                entry, os.O_RDONLY, "generated region map"
            )
            try:
                with os.fdopen(descriptor, "rb") as stream:
                    if os.fstat(stream.fileno()).st_size == 0:
                        raise WorkspaceError(
                            f"generated region map is empty: {entry}"
                        )
                    if entry.suffix == ".png":
                        signature = stream.read(8)
                        if signature != b"\x89PNG\r\n\x1a\n":
                            raise WorkspaceError(
                                f"generated region map is not a PNG file: {entry}"
                            )
                        png_names.add(entry.stem)
                    else:
                        stream.read().decode("utf-8")
                        definition_names.add(entry.stem)
            except UnicodeError as error:
                raise WorkspaceError(
                    f"generated region-map definition is not UTF-8: {entry}"
                ) from error
        if png_names != definition_names:
            missing_png = sorted(definition_names - png_names)
            missing_definition = sorted(png_names - definition_names)
            details = []
            if missing_png:
                details.append(f"missing PNG: {', '.join(missing_png)}")
            if missing_definition:
                details.append(
                    f"missing definition: {', '.join(missing_definition)}"
                )
            raise WorkspaceError(
                f"generated region-map pairs are incomplete ({'; '.join(details)})"
            )
        if EXPECTED_REGION_MAP not in png_names:
            raise WorkspaceError(
                f"generated region maps lack required {EXPECTED_REGION_MAP} pair"
            )

    def _region_map_cache_matches(
        self, output: Path, inputs: dict[str, Any], cacheable: bool
    ) -> bool:
        if not cacheable or not output.is_dir() or output.is_symlink():
            return False
        marker = output / MANAGED_MARKER
        metadata = output / REGION_MAP_METADATA
        expected_marker = {
            "schema_version": SCHEMA_VERSION,
            "purpose": "region-map-cache",
        }
        if (
            not marker.is_file()
            or marker.is_symlink()
            or not metadata.is_file()
            or metadata.is_symlink()
        ):
            return False
        try:
            if load_json(marker) != expected_marker or load_json(metadata) != inputs:
                return False
            self._validate_region_maps(output)
        except WorkspaceError:
            return False
        return True

    def _generate_region_maps(
        self, root: Path, profile_name: str, selected: dict[str, Path]
    ) -> Path:
        output = root / "runtime" / "client-maps"
        output.parent.mkdir(parents=True, exist_ok=True)
        inputs, cacheable = self._region_map_inputs(profile_name, selected)
        if self._region_map_cache_matches(output, inputs, cacheable):
            print(f"region maps: cached {output}")
            return output
        if output.exists() or output.is_symlink():
            managed_directory(output, self.paths.builds, "region-map-cache")

        staging_root = Path(
            tempfile.mkdtemp(prefix=".region-maps-", dir=output.parent)
        )
        try:
            working = staging_root / "server"
            working.mkdir()
            data = staging_root / "data"
            shutil.copytree(selected["server"] / "install_data", data)
            (data / "tmp").mkdir(exist_ok=True)
            assets = staging_root / "assets"
            assets.mkdir()
            generated = assets / "client-maps"
            content = root / "runtime" / "content"
            resources = root / "runtime" / "resources"
            self._link_server_runtime_inputs(
                working, root, selected, data, content, resources
            )
            executable = working / "atrinik-server"
            run(
                [
                    str(executable),
                    "--worldmaker",
                    f"--datapath={data}",
                    f"--assetspath={assets}",
                    f"--libpath={working / 'lib'}",
                    f"--mapspath={working / 'maps'}",
                    f"--resourcespath={working / 'resources'}",
                ],
                cwd=working,
            )
            self._validate_region_maps(generated)
            final_inputs, final_cacheable = self._region_map_inputs(
                profile_name, selected
            )
            if final_inputs != inputs or final_cacheable != cacheable:
                raise WorkspaceError(
                    "selected region-map inputs changed during generation"
                )
            atomic_json(
                generated / MANAGED_MARKER,
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "region-map-cache",
                },
            )
            atomic_json(generated / REGION_MAP_METADATA, inputs)
            replace_directory(output, generated, ".client-maps-previous-")
        finally:
            if staging_root.exists():
                shutil.rmtree(staging_root)
        print(f"region maps: generated {output}")
        return output

    @staticmethod
    def _worker_required_packages(source: Path) -> tuple[str, ...]:
        try:
            package = load_json(source / "package.json")
        except WorkspaceError:
            raise
        if not isinstance(package, dict):
            raise WorkspaceError("Worker package.json root is not an object")
        names: set[str] = set()
        for field in ("dependencies", "devDependencies"):
            value = package.get(field, {})
            if not isinstance(value, dict) or not all(
                isinstance(name, str) and isinstance(version, str)
                for name, version in value.items()
            ):
                raise WorkspaceError(f"Worker package.json {field} is invalid")
            names.update(value)
        for name in names:
            if not re.fullmatch(
                r"(?:@[a-z0-9][a-z0-9._-]*/)?[a-z0-9][a-z0-9._-]*",
                name,
            ):
                raise WorkspaceError(f"Worker package name is unsafe: {name}")
        return tuple(sorted(names))

    @staticmethod
    def _worker_environment_digest(environment: dict[str, str]) -> str:
        # Lifecycle scripts can observe arbitrary environment variables. Hash the
        # complete install environment, without persisting names or values, so an
        # unrepresented input fails closed as a cache miss.
        payload = json.dumps(
            sorted(environment.items()),
            ensure_ascii=True,
            separators=(",", ":"),
        )
        return hashlib.sha256(payload.encode()).hexdigest()

    @staticmethod
    def _worker_npm_file_config_digest(
        source: Path,
        environment: dict[str, str],
        config: dict[str, Any],
    ) -> str:
        """Hash file-backed effective npm configuration without storing secrets."""

        records: list[tuple[str, str, str | None]] = []
        file_values = {
            name: value
            for name, value in config.items()
            if name in WORKER_NPM_FILE_CONFIG_KEYS
            or name.rsplit(":", 1)[-1] in {"cafile", "certfile", "keyfile"}
        }
        for name, value in sorted(file_values.items()):
            if value is None or value == "":
                continue
            if not isinstance(value, str) or "\0" in value:
                raise WorkspaceError(f"npm configuration {name} path is invalid")
            if value == "~" or value.startswith("~/"):
                home = environment.get("HOME")
                if not home:
                    raise WorkspaceError(
                        f"npm configuration {name} requires HOME to resolve"
                    )
                path = Path(home) / value.removeprefix("~/")
            else:
                path = Path(value)
            if not path.is_absolute():
                path = source / path
            path = Path(os.path.abspath(path))
            path_digest = hashlib.sha256(str(path).encode()).hexdigest()
            if path.exists() or path.is_symlink():
                _file_digest(path, f"npm {name}")
                raise WorkspaceError(
                    "external file-backed npm configuration is unsupported; "
                    "use the project .npmrc"
                )
            else:
                content_digest = None
            records.append((name, path_digest, content_digest))
        payload = json.dumps(records, separators=(",", ":"))
        return hashlib.sha256(payload.encode()).hexdigest()

    def _worker_dependency_inputs(
        self, source: Path, environment: dict[str, str]
    ) -> dict[str, Any]:
        files = {
            name: _file_digest(source / name, f"Worker {name}")
            for name in WORKER_DEPENDENCY_FILES
        }
        npmrc = source / ".npmrc"
        files[".npmrc"] = (
            _file_digest(npmrc, "Worker .npmrc")
            if npmrc.exists() or npmrc.is_symlink()
            else None
        )
        versions: dict[str, str] = {}
        for command in ("node", "npm"):
            value = run(
                [command, "--version"],
                cwd=source,
                capture=True,
                env=environment,
                trace=False,
            )
            if not value or "\n" in value or len(value) > 256:
                raise WorkspaceError(f"{command} returned an invalid version")
            versions[command] = value
        raw_config = run(
            ["npm", "config", "list", "--json"],
            cwd=source,
            capture=True,
            env=environment,
            trace=False,
        )
        try:
            config = json.loads(raw_config)
        except json.JSONDecodeError as error:
            raise WorkspaceError("npm configuration is not valid JSON") from error
        if not isinstance(config, dict):
            raise WorkspaceError("npm configuration root is not an object")
        config_digest = hashlib.sha256(
            json.dumps(config, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        package = load_json(source / "package.json")
        scripts = package.get("scripts", {}) if isinstance(package, dict) else None
        if not isinstance(scripts, dict) or not all(
            isinstance(name, str) and isinstance(value, str)
            for name, value in scripts.items()
        ):
            raise WorkspaceError("Worker package.json scripts are invalid")
        lifecycle_names = (
            "preinstall",
            "install",
            "postinstall",
            "prepublish",
            "preprepare",
            "prepare",
            "postprepare",
        )
        root_lifecycle = tuple(name for name in lifecycle_names if name in scripts)
        return {
            "schema_version": WORKER_DEPENDENCY_SCHEMA_VERSION,
            "files": files,
            "node_version": versions["node"],
            "npm_version": versions["npm"],
            "os": platform.system(),
            "architecture": platform.machine(),
            "python_platform": sys.platform,
            "npm_config_sha256": config_digest,
            "npm_file_config_sha256": self._worker_npm_file_config_digest(
                source, environment, config
            ),
            "environment_sha256": self._worker_environment_digest(environment),
            "install_command": ["npm", "ci"],
            "lifecycle_scripts": "enabled; complete environment participates in key",
            "root_lifecycle_scripts": list(root_lifecycle),
            # Dependency lifecycle scripts can observe INIT_CWD and arbitrary
            # root files even when package.json has no root hook. Preserve npm's
            # enabled-script semantics by staging and keying the complete source.
            "lifecycle_source_sha256": _tree_digest(
                source,
                WORKER_SOURCE_EXCLUSIONS,
                reject_symlinks=True,
                copied_metadata=True,
            ),
            "lifecycle_source_without_npmrc_sha256": _tree_digest(
                source,
                WORKER_SOURCE_EXCLUSIONS | {".npmrc"},
                reject_symlinks=True,
                copied_metadata=True,
            ),
            "install_root_sha256": hashlib.sha256(
                str(
                    self.paths.builds
                    / "worker-dependencies"
                    / ".transactions"
                ).encode()
            ).hexdigest(),
        }

    @staticmethod
    def _worker_dependency_key(inputs: dict[str, Any]) -> str:
        payload = json.dumps(inputs, sort_keys=True, separators=(",", ":"))
        return hashlib.sha256(payload.encode()).hexdigest()

    @staticmethod
    def _validate_worker_node_modules(
        node_modules: Path,
        hidden_lock_digest: str,
        tree_digest: str,
        required: tuple[str, ...],
        generated_exclusions: set[str] | None = None,
    ) -> None:
        if node_modules.is_symlink() or not node_modules.is_dir():
            raise WorkspaceError("Worker node_modules is not a regular directory")
        hidden_lock = node_modules / ".package-lock.json"
        if _file_digest(hidden_lock, "Worker installed lockfile") != hidden_lock_digest:
            raise WorkspaceError(
                "Worker installed lockfile does not match cache metadata"
            )
        installed = load_json(hidden_lock)
        packages = installed.get("packages") if isinstance(installed, dict) else None
        if not isinstance(packages, dict):
            raise WorkspaceError("Worker installed lockfile packages are invalid")
        for relative in packages:
            if not isinstance(relative, str) or not relative.startswith(
                "node_modules/"
            ):
                raise WorkspaceError("Worker installed package path is invalid")
            package_path = PurePosixPath(relative)
            if (
                package_path.is_absolute()
                or ".." in package_path.parts
                or "\\" in relative
                or len(package_path.parts) < 2
            ):
                raise WorkspaceError("Worker installed package path is unsafe")
            dependency = node_modules.joinpath(*package_path.parts[1:])
            if dependency.is_symlink() or not dependency.is_dir():
                raise WorkspaceError(
                    f"Worker installed package is missing or unsafe: {relative}"
                )
        for name in required:
            dependency = node_modules.joinpath(*name.split("/"))
            if dependency.is_symlink() or not dependency.is_dir():
                raise WorkspaceError(f"Worker dependency is missing or unsafe: {name}")
        if (
            _tree_digest(
                node_modules,
                generated_exclusions or set(),
                bounded_symlinks=True,
            )
            != tree_digest
        ):
            raise WorkspaceError("Worker node_modules does not match cache metadata")

    def _worker_dependency_cache_matches(
        self,
        entry: Path,
        key: str,
        inputs: dict[str, Any],
        required: tuple[str, ...],
    ) -> dict[str, Any] | None:
        marker = entry / MANAGED_MARKER
        metadata_path = entry / WORKER_DEPENDENCY_METADATA
        try:
            if (
                entry.is_symlink()
                or not entry.is_dir()
                or marker.is_symlink()
                or load_json(marker)
                != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": f"worker-dependencies:{key}",
                }
                or metadata_path.is_symlink()
                or not metadata_path.is_file()
            ):
                return None
            metadata = load_json(metadata_path)
            if (
                not isinstance(metadata, dict)
                or set(metadata)
                != {
                    "schema_version",
                    "purpose",
                    "key",
                    "inputs",
                    "node_modules_lock_sha256",
                    "node_modules_sha256",
                    "last_used_at",
                }
                or metadata.get("schema_version") != WORKER_DEPENDENCY_SCHEMA_VERSION
                or metadata.get("purpose") != "worker-dependencies"
                or metadata.get("key") != key
                or metadata.get("inputs") != inputs
                or not isinstance(metadata.get("node_modules_lock_sha256"), str)
                or not re.fullmatch(
                    r"[0-9a-f]{64}", metadata["node_modules_lock_sha256"]
                )
                or not isinstance(metadata.get("node_modules_sha256"), str)
                or not re.fullmatch(r"[0-9a-f]{64}", metadata["node_modules_sha256"])
                or not isinstance(metadata.get("last_used_at"), str)
            ):
                return None
            for name, expected in inputs["files"].items():
                path = entry / name
                if name == ".npmrc":
                    if path.exists() or path.is_symlink():
                        return None
                    continue
                if expected is None:
                    if path.exists() or path.is_symlink():
                        return None
                elif _file_digest(path, f"cached Worker {name}") != expected:
                    return None
            self._validate_worker_node_modules(
                entry / "node_modules",
                metadata["node_modules_lock_sha256"],
                metadata["node_modules_sha256"],
                required,
            )
            return metadata
        except (OSError, WorkspaceError):
            return None

    def _worker_dependencies(
        self,
        source: Path,
        environment: dict[str, str],
        consume: Callable[[Path, str, dict[str, Any]], Any] | None = None,
    ) -> tuple[Path, str, dict[str, Any], bool, float, Any]:
        npm_cache = self.paths.builds / "npm-cache"
        npm_cache_lock = self.paths.builds / "locks" / "npm-cache.lock"
        with exclusive_lock(npm_cache_lock, "npm download cache"):
            marker = npm_cache / MANAGED_MARKER
            if npm_cache.exists() and not marker.exists() and not marker.is_symlink():
                if npm_cache.is_symlink() or not npm_cache.is_dir():
                    raise WorkspaceError(f"npm cache path is invalid: {npm_cache}")
                atomic_json(
                    marker,
                    {"schema_version": SCHEMA_VERSION, "purpose": "npm-cache"},
                )
            managed_directory(npm_cache, self.paths.builds, "npm-cache")
            atomic_json(
                npm_cache / CACHE_METADATA,
                {
                    "schema_version": BUILD_METADATA_SCHEMA_VERSION,
                    "purpose": "npm-cache",
                    "last_used_at": datetime.now(timezone.utc).isoformat(),
                },
            )
        environment["npm_config_cache"] = str(npm_cache)
        inputs = self._worker_dependency_inputs(source, environment)
        key = self._worker_dependency_key(inputs)
        required = self._worker_required_packages(source)
        cache_root = self.paths.builds / "worker-dependencies"
        with exclusive_lock(
            self.paths.builds / "locks" / "worker-dependency-cache.lock",
            "Worker dependency cache",
        ):
            managed_directory(
                cache_root, self.paths.builds, "worker-dependency-cache"
            )
            transactions = cache_root / ".transactions"
            managed_directory(
                transactions,
                self.paths.builds,
                "worker-dependency-transactions",
            )
        entry = cache_root / key
        lock = self.paths.builds / "locks" / f"worker-dependencies-{key}.lock"
        started = time.monotonic()
        with exclusive_lock(lock, f"Worker dependencies {key}"):
            metadata = self._worker_dependency_cache_matches(
                entry, key, inputs, required
            )
            recoverable: list[Path] = []
            if metadata is None:
                for candidate in transactions.iterdir():
                    if not re.fullmatch(
                        rf"{key}-(?:staging|backup)-[a-z0-9_]+",
                        candidate.name,
                    ):
                        continue
                    if self._worker_dependency_cache_matches(
                        candidate, key, inputs, required
                    ) is not None:
                        recoverable.append(candidate)
            if recoverable:
                if entry.exists() or entry.is_symlink():
                    managed_directory(
                        entry,
                        self.paths.builds,
                        f"worker-dependencies:{key}",
                    )
                recovered = max(
                    recoverable,
                    key=lambda candidate: (
                        candidate.stat().st_mtime_ns,
                        candidate.name,
                    ),
                )
                replace_directory(
                    entry,
                    recovered,
                    f"{key}-backup-",
                    backup_parent=transactions,
                )
                metadata = self._worker_dependency_cache_matches(
                    entry, key, inputs, required
                )
            if metadata is not None:
                metadata["last_used_at"] = datetime.now(timezone.utc).isoformat()
                atomic_json(entry / WORKER_DEPENDENCY_METADATA, metadata)
                elapsed = time.monotonic() - started
                consumed = (
                    consume(entry / "node_modules", key, metadata)
                    if consume is not None
                    else None
                )
                return entry / "node_modules", key, metadata, True, elapsed, consumed
            if entry.is_symlink() or entry.exists() and not entry.is_dir():
                raise WorkspaceError(f"Worker dependency cache path is unsafe: {entry}")
            if entry.exists():
                managed_directory(
                    entry,
                    self.paths.builds,
                    f"worker-dependencies:{key}",
                )
            staging = transactions / f"{key}-staging-install"
            if staging.exists() or staging.is_symlink():
                try:
                    managed_directory(
                        staging,
                        self.paths.builds,
                        f"worker-dependency-transaction:{key}",
                    )
                except WorkspaceError:
                    managed_directory(
                        staging,
                        self.paths.builds,
                        f"worker-dependencies:{key}",
                    )
                shutil.rmtree(staging)
            managed_directory(
                staging,
                self.paths.builds,
                f"worker-dependency-transaction:{key}",
            )
            try:
                # Ownership is proven by the marker-owned transaction parent,
                # exact per-key name, and held key lock. Keep wrapper metadata
                # outside the lifecycle-visible input tree while npm runs.
                (staging / MANAGED_MARKER).unlink()
                _copy_worker_source(
                    source,
                    staging,
                    include_npmrc=False,
                )
                if inputs["files"][".npmrc"] is not None:
                    _copy_regular_file(
                        source / ".npmrc",
                        staging / ".npmrc",
                        "Worker .npmrc",
                    )
                shutil.copystat(source, staging, follow_symlinks=False)
                if (
                    _tree_digest(
                        staging,
                        WORKER_SOURCE_EXCLUSIONS | {".npmrc"},
                        reject_symlinks=True,
                        copied_metadata=True,
                    )
                    != inputs["lifecycle_source_without_npmrc_sha256"]
                ):
                    raise WorkspaceError(
                        "staged Worker lifecycle source does not match its cache key"
                    )
                if (staging / ".npmrc").exists():
                    (staging / ".npmrc").chmod(0o600)
                    if (
                        _file_digest(staging / ".npmrc", "staged Worker .npmrc")
                        != inputs["files"][".npmrc"]
                    ):
                        raise WorkspaceError(
                            "staged Worker .npmrc does not match its cache key"
                        )
                _normalize_worker_atime(staging)
                run(["npm", "ci"], cwd=staging, env=environment)
                if (staging / MANAGED_MARKER).exists() or (
                    staging / MANAGED_MARKER
                ).is_symlink():
                    raise WorkspaceError(
                        "Worker lifecycle created reserved workspace metadata"
                    )
                if _tree_references_path(staging / "node_modules", staging):
                    raise WorkspaceError(
                        "Worker dependency output embeds its install path"
                    )
                final_inputs = self._worker_dependency_inputs(source, environment)
                if final_inputs != inputs:
                    raise WorkspaceError(
                        "Worker dependency inputs changed during installation"
                    )
                for child in staging.iterdir():
                    if child.name == "node_modules":
                        continue
                    if child.is_symlink() or not child.is_dir():
                        child.unlink()
                    else:
                        shutil.rmtree(child)
                for name, expected in inputs["files"].items():
                    if expected is not None and name != ".npmrc":
                        shutil.copy2(source / name, staging / name)
                hidden_digest = _file_digest(
                    staging / "node_modules" / ".package-lock.json",
                    "Worker installed lockfile",
                )
                modules_digest = _tree_digest(
                    staging / "node_modules", set(), bounded_symlinks=True
                )
                self._validate_worker_node_modules(
                    staging / "node_modules",
                    hidden_digest,
                    modules_digest,
                    required,
                )
                metadata = {
                    "schema_version": WORKER_DEPENDENCY_SCHEMA_VERSION,
                    "purpose": "worker-dependencies",
                    "key": key,
                    "inputs": inputs,
                    "node_modules_lock_sha256": hidden_digest,
                    "node_modules_sha256": modules_digest,
                    "last_used_at": datetime.now(timezone.utc).isoformat(),
                }
                atomic_json(
                    staging / MANAGED_MARKER,
                    {
                        "schema_version": SCHEMA_VERSION,
                        "purpose": f"worker-dependencies:{key}",
                    },
                )
                atomic_json(staging / WORKER_DEPENDENCY_METADATA, metadata)
                replace_directory(
                    entry,
                    staging,
                    f"{key}-backup-",
                    backup_parent=transactions,
                )
            finally:
                if staging.exists():
                    shutil.rmtree(staging)
            elapsed = time.monotonic() - started
            consumed = (
                consume(entry / "node_modules", key, metadata)
                if consume is not None
                else None
            )
        return (
            entry / "node_modules",
            key,
            metadata,
            False,
            elapsed,
            consumed,
        )

    def _worker_view(
        self,
        root: Path,
        source: Path,
        dependencies: Path,
        dependency_key: str,
        dependency_metadata: dict[str, Any],
    ) -> tuple[Path, bool, float]:
        started = time.monotonic()
        view = root / "sources" / "metaserver-worker"
        lifecycle_source_digest = _tree_digest(
            source,
            WORKER_SOURCE_EXCLUSIONS,
            reject_symlinks=True,
            copied_metadata=True,
        )
        source_digest = _tree_digest(
            source, WORKER_SOURCE_EXCLUSIONS, reject_symlinks=True
        )
        dependency_source_digest = dependency_metadata.get("inputs", {}).get(
            "lifecycle_source_sha256"
        )
        if lifecycle_source_digest != dependency_source_digest:
            raise WorkspaceError(
                "Worker source does not match dependency lifecycle inputs"
            )
        expected = {
            "schema_version": WORKER_VIEW_SCHEMA_VERSION,
            "purpose": "worker-view",
            "source_sha256": source_digest,
            "dependency_key": dependency_key,
            "node_modules_lock_sha256": dependency_metadata[
                "node_modules_lock_sha256"
            ],
        }
        try:
            if (
                not view.is_symlink()
                and view.is_dir()
                and load_json(view / MANAGED_MARKER)
                == {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "source-view:metaserver-worker",
                }
                and load_json(view / WORKER_VIEW_METADATA) == expected
                and _tree_digest(view, WORKER_SOURCE_EXCLUSIONS) == source_digest
            ):
                self._validate_worker_node_modules(
                    view / "node_modules",
                    dependency_metadata["node_modules_lock_sha256"],
                    dependency_metadata["node_modules_sha256"],
                    self._worker_required_packages(source),
                    WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
                )
                return view, True, time.monotonic() - started
        except (OSError, WorkspaceError):
            pass
        if view.exists() or view.is_symlink():
            managed_directory(
                view, self.paths.builds, "source-view:metaserver-worker"
            )
        view.parent.mkdir(parents=True, exist_ok=True)
        staging = Path(
            tempfile.mkdtemp(prefix=".metaserver-worker-", dir=view.parent)
        )
        try:
            _copy_worker_source(source, staging)
            if (
                _tree_digest(
                    staging,
                    WORKER_SOURCE_EXCLUSIONS,
                    reject_symlinks=True,
                    copied_metadata=True,
                )
                != lifecycle_source_digest
            ):
                raise WorkspaceError(
                    "staged Worker view source does not match its fingerprint"
                )
            shutil.copytree(dependencies, staging / "node_modules", symlinks=True)
            if (
                _tree_digest(
                    source,
                    WORKER_SOURCE_EXCLUSIONS,
                    reject_symlinks=True,
                    copied_metadata=True,
                )
                != lifecycle_source_digest
            ):
                raise WorkspaceError("Worker source changed during view preparation")
            self._validate_worker_node_modules(
                staging / "node_modules",
                dependency_metadata["node_modules_lock_sha256"],
                dependency_metadata["node_modules_sha256"],
                self._worker_required_packages(source),
                WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
            )
            atomic_json(
                staging / MANAGED_MARKER,
                {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "source-view:metaserver-worker",
                },
            )
            atomic_json(staging / WORKER_VIEW_METADATA, expected)
            replace_directory(view, staging, ".metaserver-worker-previous-")
        finally:
            if staging.exists():
                shutil.rmtree(staging)
        return view, False, time.monotonic() - started

    def _build_worker(self, root: Path, selected: dict[str, Path]) -> None:
        source = selected["metaserver-worker"]
        environment = os.environ.copy()
        dependencies, key, metadata, cache_hit, install_seconds, view_result = (
            self._worker_dependencies(
                source,
                environment,
                lambda dependencies, key, metadata: self._worker_view(
                    root, source, dependencies, key, metadata
                ),
            )
        )
        view, view_hit, view_seconds = view_result
        print(
            f"worker dependencies: {'cached' if cache_hit else 'installed'} "
            f"{key} ({install_seconds:.2f}s)"
        )
        print(
            f"worker view: {'reused' if view_hit else 'prepared'} {view} "
            f"({view_seconds:.2f}s)"
        )
        run(["npm", "run", "check"], cwd=view, env=environment)

    def state_add(self, name: str, path: Path | None) -> Path:
        self.paths.ensure()
        validate_name(name, "state name")
        if path is None:
            resolved = (self.paths.state / "server" / name).resolve(strict=False)
        else:
            if not path.is_absolute():
                raise WorkspaceError("state path must be absolute")
            resolved = path.expanduser().resolve(strict=False)
        if resolved == Path("/") or resolved == self.paths.repository:
            raise WorkspaceError(f"refusing unsafe state path: {resolved}")
        if resolved.exists() and not resolved.is_dir():
            raise WorkspaceError(f"state path is not a directory: {resolved}")
        if resolved.exists():
            self._validate_state(resolved)
        self._register_state(name, resolved)
        print(resolved)
        return resolved

    def _register_state(self, name: str, resolved: Path) -> None:
        with exclusive_lock(self.paths.workspace / "states.lock", "states registry"):
            states = self._load_states()
            if name in states:
                raise WorkspaceError(f"state already exists: {name}")
            states[name] = str(resolved)
            atomic_json(
                self.paths.states_file,
                {"schema_version": SCHEMA_VERSION, "states": states},
            )

    def _scenario_directory(self, name: str) -> Path:
        validate_name(name, "scenario name")
        directory = (self.paths.scenarios / name).resolve(strict=False)
        if directory.parent != self.paths.scenarios.resolve():
            raise WorkspaceError(f"invalid scenario path: {directory}")
        return directory

    @staticmethod
    def _write_scenario_password(path: Path, password: str) -> None:
        descriptor = open_regular_file(
            path,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL,
            "scenario password",
            0o600,
        )
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
                stream.write(password)
                stream.flush()
                os.fsync(stream.fileno())
        except BaseException:
            path.unlink(missing_ok=True)
            raise

    @staticmethod
    def _open_scenario_password(path: Path) -> int:
        descriptor = open_regular_file(path, os.O_RDONLY, "scenario password")
        metadata = os.fstat(descriptor)
        mode = stat.S_IMODE(metadata.st_mode)
        wrong_owner = hasattr(os, "geteuid") and metadata.st_uid != os.geteuid()
        if wrong_owner or mode != 0o600:
            os.close(descriptor)
            raise WorkspaceError(
                f"scenario password must be owned by this user and mode 0600: {path}"
            )
        if not 0 < metadata.st_size <= SCENARIO_PASSWORD_MAX_SIZE:
            os.close(descriptor)
            raise WorkspaceError(f"scenario password file is invalid: {path}")
        return descriptor

    @classmethod
    def _read_scenario_password(cls, path: Path) -> str:
        descriptor = cls._open_scenario_password(path)
        try:
            with os.fdopen(descriptor, encoding="utf-8") as stream:
                password = stream.read(SCENARIO_PASSWORD_MAX_SIZE + 1)
                descriptor = -1
        except UnicodeError as error:
            raise WorkspaceError(
                f"scenario password file is not UTF-8: {path}"
            ) from error
        finally:
            if descriptor >= 0:
                os.close(descriptor)
        if (
            not password
            or len(password) > SCENARIO_PASSWORD_MAX_SIZE
            or "\n" in password
            or "\r" in password
        ):
            raise WorkspaceError(f"scenario password file is invalid: {path}")
        return password

    def _load_scenario(self, name: str) -> dict[str, Any]:
        self.paths.ensure()
        root = self._scenario_directory(name)
        if not root.is_dir() or root.is_symlink():
            raise WorkspaceError(f"scenario does not exist: {name}")
        marker = root / MANAGED_MARKER
        if marker.is_symlink() or load_json(marker) != {
            "schema_version": SCHEMA_VERSION,
            "purpose": "test-scenario",
        }:
            raise WorkspaceError(f"scenario ownership marker is invalid: {name}")
        metadata_path = root / "scenario.json"
        if metadata_path.is_symlink():
            raise WorkspaceError(f"scenario metadata is invalid: {name}")
        metadata = load_json(metadata_path)
        if not isinstance(metadata, dict):
            raise WorkspaceError(f"scenario metadata must be an object: {name}")
        actual_keys = set(metadata)
        historical_keys = SCENARIO_KEYS - {"stack", "providers"}
        if (
            actual_keys == historical_keys
            and metadata.get("schema_version") == SCHEMA_VERSION
        ):
            raise WorkspaceError(
                "historical scenario lacks immutable stack/provider identity and is "
                f"inert; recreate it explicitly: {name}"
            )
        if actual_keys != SCENARIO_KEYS:
            raise WorkspaceError(f"scenario fields are invalid: {name}")
        if metadata.get("schema_version") != SCENARIO_SCHEMA_VERSION:
            raise WorkspaceError(
                "historical scenario lacks immutable repository/branch identity and "
                f"is inert; recreate it explicitly: {name}"
            )
        resolved = metadata.get("resolved")
        profile = self._load_profile(metadata.get("profile", ""), require_file=False)
        stack = self.manifest.stack(profile["stack"])
        required = self._dependency_roles(profile, {"server"})
        expected_providers = {
            role: stack.providers[role].name for role in sorted(required)
        }
        if (
            metadata.get("name") != name
            or not isinstance(metadata.get("profile"), str)
            or metadata.get("preset") not in SCENARIO_PRESETS
            or metadata.get("state") != f"scenario-{name}"
            or not isinstance(metadata.get("account"), str)
            or not isinstance(metadata.get("character"), str)
            or not isinstance(metadata.get("archetype"), str)
            or not isinstance(metadata.get("provisioned_at"), str)
            or not metadata["provisioned_at"]
            or not isinstance(resolved, dict)
            or set(resolved) != required
            or metadata.get("stack") != stack.name
            or metadata.get("providers") != expected_providers
        ):
            raise WorkspaceError(f"scenario metadata is invalid: {name}")
        for component, record in resolved.items():
            if (
                not isinstance(record, dict)
                or set(record)
                != {
                    "path",
                    "checkout_path",
                    "checkout",
                    "repository",
                    "branch",
                    "source",
                    "head",
                    "dirty",
                }
                or not isinstance(record.get("path"), str)
                or not Path(record["path"]).is_absolute()
                or not isinstance(record.get("checkout_path"), str)
                or not Path(record["checkout_path"]).is_absolute()
                or not isinstance(record.get("checkout"), str)
                or not isinstance(record.get("repository"), str)
                or not isinstance(record.get("branch"), str)
                or not isinstance(record.get("source"), str)
                or not isinstance(record.get("head"), str)
                or not re.fullmatch(r"[0-9a-f]{40,64}", record["head"])
                or not isinstance(record.get("dirty"), bool)
            ):
                raise WorkspaceError(
                    f"scenario component metadata is invalid: {name}/{component}"
                )
            provider = stack.providers[component]
            checkout_path = Path(record["checkout_path"]).resolve(strict=False)
            expected_path = (
                checkout_path
                if provider.source == "."
                else checkout_path.joinpath(*PurePosixPath(provider.source).parts)
            ).resolve(strict=False)
            if (
                record["checkout"] != provider.checkout_name
                or record["repository"] != provider.repository
                or record["branch"] != provider.branch
                or record["source"] != provider.source
                or Path(record["path"]).resolve(strict=False) != expected_path
            ):
                raise WorkspaceError(
                    f"scenario component identity is invalid: {name}/{component}"
                )
        state = root / "state"
        if state.is_symlink():
            raise WorkspaceError(f"scenario state is invalid: {name}")
        self._validate_state(state)
        registered = self._load_states().get(metadata["state"])
        if registered is None or Path(registered).resolve(strict=False) != state.resolve():
            raise WorkspaceError(f"scenario state registration is invalid: {name}")
        os.close(self._open_scenario_password(root / "password"))
        return metadata

    def _scenario_provision_state(
        self,
        metadata: dict[str, Any],
        state: Path,
        password_file: Path,
    ) -> dict[str, dict[str, Any]]:
        profile = self._load_profile(metadata["profile"], require_file=False)
        required = self._dependency_roles(profile, {"server"})
        selected = self._resolve_build_profile(metadata["profile"], required)
        root = self._build_resolved(
            "server", metadata["profile"], False, ["server"], selected
        )
        runtime = self._prepare_server_runtime(
            root, selected, state, metadata["state"]
        )
        executable = runtime / "atrinik-server"
        run(
            [
                str(executable),
                "--provision_scenario",
                f"--provision_account={metadata['account']}",
                f"--provision_character={metadata['character']}",
                f"--provision_archetype={metadata['archetype']}",
                f"--provision_password_file={password_file}",
                f"--assetspath={runtime / 'assets'}",
            ],
            cwd=runtime,
        )
        profile = self._load_profile(metadata["profile"], require_file=False)
        stack = self.manifest.stack(profile["stack"])
        audited = {role: selected[role] for role in required}
        checkout_states = self._selected_checkout_states(
            profile, audited, include_dirty=True
        )
        return {
            role: {
                "path": str(path),
                "checkout_path": str(
                    checkout_states[stack.providers[role].checkout_name]["path"]
                ),
                "checkout": stack.providers[role].checkout_name,
                "repository": stack.providers[role].repository,
                "branch": stack.providers[role].branch,
                "source": stack.providers[role].source,
                "head": checkout_states[stack.providers[role].checkout_name]["head"],
                "dirty": checkout_states[stack.providers[role].checkout_name]["dirty"],
            }
            for role, path in sorted(
                (role, selected[role]) for role in required
            )
        }

    def scenario_create(
        self, name: str, profile: str, preset: str = "basic-player"
    ) -> dict[str, Any]:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._scenario_create(name, profile, preset)

    def _scenario_create(
        self, name: str, profile: str, preset: str = "basic-player"
    ) -> dict[str, Any]:
        self.paths.ensure()
        validate_name(name, "scenario name")
        if preset not in SCENARIO_PRESETS:
            raise WorkspaceError(f"unknown scenario preset: {preset}")
        root = self._scenario_directory(name)
        state_name = f"scenario-{name}"
        digest = hashlib.sha256(name.encode()).hexdigest()[:8]
        metadata: dict[str, Any] = {
            "schema_version": SCENARIO_SCHEMA_VERSION,
            "name": name,
            "profile": profile,
            "stack": self._load_profile(profile, require_file=False)["stack"],
            "preset": preset,
            "state": state_name,
            "account": f"scenario{digest}",
            "character": f"Scenario {digest}",
            "archetype": SCENARIO_PRESETS[preset]["archetype"],
            "resolved": {},
            "provisioned_at": "pending",
        }
        selected_profile = self._load_profile(profile, require_file=False)
        selected_stack = self.manifest.stack(selected_profile["stack"])
        required = self._dependency_roles(selected_profile, {"server"})
        self._require_classic_contracts(profile, {"server"})
        metadata["providers"] = {
            role: selected_stack.providers[role].name for role in sorted(required)
        }
        operation_lock = self.paths.scenarios / "operation.lock"
        with exclusive_lock(operation_lock, "scenario operation"):
            if root.exists() or root.is_symlink():
                raise WorkspaceError(f"scenario already exists: {name}")
            if state_name in self._load_states():
                raise WorkspaceError(f"state already exists: {state_name}")
            staging = Path(
                tempfile.mkdtemp(prefix=f".{name}-", dir=self.paths.scenarios)
            )
            try:
                atomic_json(
                    staging / MANAGED_MARKER,
                    {"schema_version": SCHEMA_VERSION, "purpose": "test-scenario"},
                )
                password = secrets.token_urlsafe(12)
                password_file = staging / "password"
                self._write_scenario_password(password_file, password)
                server_source = self.component_path("server", profile)
                state = self.state_path(
                    state_name, server_source, resolved_path=staging / "state"
                )
                metadata["resolved"] = self._scenario_provision_state(
                    metadata, state, password_file
                )
                metadata["provisioned_at"] = datetime.now(timezone.utc).isoformat()
                atomic_json(staging / "scenario.json", metadata)
                staging.replace(root)
                try:
                    self._register_state(state_name, (root / "state").resolve())
                except BaseException:
                    shutil.rmtree(root)
                    raise
            except BaseException:
                if staging.exists():
                    shutil.rmtree(staging)
                raise
        return self.scenario_show(name)

    def scenario_show(self, name: str) -> dict[str, Any]:
        metadata = self._load_scenario(name)
        return {**metadata, "path": str(self._scenario_directory(name))}

    def scenario_list(self) -> list[dict[str, Any]]:
        self.paths.ensure()
        scenarios: list[dict[str, Any]] = []
        for path in sorted(self.paths.scenarios.iterdir()):
            if path.is_dir() and not path.name.startswith("."):
                scenarios.append(self.scenario_show(path.name))
        return scenarios

    def scenario_credentials(self, name: str) -> dict[str, str]:
        metadata = self._load_scenario(name)
        return {
            "account": metadata["account"],
            "character": metadata["character"],
            "password": self._read_scenario_password(
                self._scenario_directory(name) / "password"
            ),
        }

    def scenario_reset(self, name: str) -> dict[str, Any]:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._scenario_reset(name)

    def _scenario_reset(self, name: str) -> dict[str, Any]:
        self.paths.ensure()
        root = self._scenario_directory(name)
        operation_lock = self.paths.scenarios / "operation.lock"
        with exclusive_lock(operation_lock, "scenario operation"):
            metadata = self._load_scenario(name)
            state = root / "state"
            with exclusive_lock(
                Path(f"{state}.lock"),
                f"server state {state}",
                nonblocking=True,
            ):
                staging_root = Path(
                    tempfile.mkdtemp(prefix=f".{name}-reset-", dir=self.paths.scenarios)
                )
                try:
                    server_source = self.component_path("server", metadata["profile"])
                    staging_state = self.state_path(
                        metadata["state"],
                        server_source,
                        resolved_path=staging_root / "state",
                    )
                    metadata["resolved"] = self._scenario_provision_state(
                        metadata, staging_state, root / "password"
                    )
                    metadata["provisioned_at"] = datetime.now(timezone.utc).isoformat()
                    replace_directory(state, staging_state, f".{name}-state-previous-")
                    atomic_json(root / "scenario.json", metadata)
                finally:
                    if staging_root.exists():
                        shutil.rmtree(staging_root)
        return self.scenario_show(name)

    def _load_states(self) -> dict[str, str]:
        if not self.paths.states_file.exists():
            return {}
        value = load_json(self.paths.states_file)
        if not isinstance(value, dict):
            raise WorkspaceError("states registry must be an object")
        require_keys(value, {"schema_version", "states"}, "states registry")
        if value["schema_version"] != SCHEMA_VERSION or not isinstance(value["states"], dict):
            raise WorkspaceError("states registry schema is invalid")
        states: dict[str, str] = {}
        for name, path in value["states"].items():
            validate_name(name, "state name")
            if not isinstance(path, str) or not Path(path).is_absolute():
                raise WorkspaceError(f"state path must be absolute: {name}")
            states[name] = path
        return states

    def list_states(self) -> dict[str, str]:
        self.paths.ensure()
        states = self._load_states()
        if "default" not in states:
            states = {
                "default": str(self.paths.state / "server" / "default"),
                **states,
            }
        return states

    def _state_location(self, name: str) -> Path:
        validate_name(name, "state name")
        states = self._load_states()
        if name in states:
            return Path(states[name]).resolve(strict=False)
        if name == "default":
            return (self.paths.state / "server" / "default").resolve(strict=False)
        raise WorkspaceError(f"state does not exist: {name}")

    def state_path(
        self, name: str, server_source: Path, resolved_path: Path | None = None
    ) -> Path:
        validate_name(name, "state name")
        path = resolved_path or self._state_location(name)
        path = path.resolve(strict=False)
        server_source = server_source.resolve()
        if server_source == path or server_source in path.parents:
            raise WorkspaceError(f"server state must be outside its source worktree: {path}")
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            staging = Path(tempfile.mkdtemp(prefix=f".{path.name}.", dir=path.parent))
            try:
                shutil.copytree(server_source / "install_data", staging, dirs_exist_ok=True)
                (staging / "tmp").mkdir()
                if path.exists():
                    raise WorkspaceError(f"state appeared during initialization: {path}")
                staging.replace(path)
            except BaseException:
                shutil.rmtree(staging, ignore_errors=True)
                raise
        self._validate_state(path)
        (path / "tmp").mkdir(exist_ok=True)
        return path.resolve()

    def _validate_state(self, path: Path) -> None:
        if not path.is_dir():
            raise WorkspaceError(f"server state is not a directory: {path}")
        for name in EXPECTED_SERVER_DATA["files"]:
            if not (path / name).is_file():
                raise WorkspaceError(f"server state lacks required file {name}: {path}")
        for name in EXPECTED_SERVER_DATA["directories"]:
            if not (path / name).is_dir():
                raise WorkspaceError(f"server state lacks required directory {name}: {path}")

    def _topology_services(self, services: list[str] | None) -> list[str]:
        requested = set(services or TOPOLOGY_SERVICES)
        unknown = sorted(requested - set(TOPOLOGY_SERVICES))
        if unknown:
            raise WorkspaceError(f"unknown topology services: {', '.join(unknown)}")
        if not requested:
            raise WorkspaceError("a topology must contain at least one service")
        return [service for service in TOPOLOGY_SERVICES if service in requested]

    def topology_summary(
        self,
        profile_name: str,
        state_name: str,
        services: list[str] | None = None,
    ) -> dict[str, Any]:
        selected_services = self._topology_services(services)
        requested = set(selected_services)
        profile = self._load_profile(profile_name, require_file=False)
        stack = self.manifest.stack(profile["stack"])
        resolved = self._resolve_build_profile(profile_name, requested)
        required = set(resolved)
        key = self._profile_build_key(profile_name, resolved)
        checkout_states = self._selected_checkout_states(
            profile, resolved, include_dirty=True
        )
        state = (
            str(self._state_location(state_name))
            if "server" in selected_services
            else None
        )
        return {
            "profile": profile_name,
            "stack": stack.name,
            "services": selected_services,
            "dependencies": sorted(required),
            "providers": {
                role: stack.providers[role].name for role in sorted(required)
            },
            "state": state,
            "build_root": str(
                self.paths.builds / "profiles" / f"{profile_name}-{key}"
            ),
            "components": {
                stack.providers[role].name: {
                    "checkout": stack.providers[role].checkout_name,
                    "repository": stack.providers[role].repository,
                    "branch": stack.providers[role].branch,
                    "source": stack.providers[role].source,
                    "roles": sorted(stack.providers[role].provides),
                    "path": str(path),
                    "checkout_path": str(
                        checkout_states[stack.providers[role].checkout_name]["path"]
                    ),
                    "head": checkout_states[stack.providers[role].checkout_name]["head"],
                    "dirty": checkout_states[stack.providers[role].checkout_name]["dirty"],
                }
                for role, path in sorted(resolved.items())
            },
        }

    def _topology_directory(self, name: str, create: bool = False) -> Path:
        validate_name(name, "topology name")
        self.paths.ensure()
        path = self.paths.topologies / name
        marker = path / MANAGED_MARKER
        metadata = {"schema_version": SCHEMA_VERSION, "purpose": f"topology:{name}"}
        if path.exists() or path.is_symlink():
            if not path.is_dir() or path.is_symlink():
                raise WorkspaceError(f"topology path is not a managed directory: {path}")
            if not marker.is_file() or marker.is_symlink() or load_json(marker) != metadata:
                raise WorkspaceError(f"topology ownership marker is invalid: {path}")
        elif create:
            path.mkdir()
            atomic_json(marker, metadata)
        else:
            raise WorkspaceError(f"topology does not exist: {name}")
        return path

    def _reset_topology_subdirectory(
        self, topology_root: Path, name: str, purpose: str
    ) -> Path:
        path = topology_root / name
        metadata = {"schema_version": SCHEMA_VERSION, "purpose": purpose}
        if path.exists() or path.is_symlink():
            marker = path / MANAGED_MARKER
            if (
                not path.is_dir()
                or path.is_symlink()
                or not marker.is_file()
                or marker.is_symlink()
                or load_json(marker) != metadata
            ):
                raise WorkspaceError(
                    f"topology runtime path is not managed for {purpose}: {path}"
                )
            shutil.rmtree(path)
        path.mkdir()
        atomic_json(path / MANAGED_MARKER, metadata)
        return path

    def _take_topology_runtime_input(
        self,
        topology_root: Path,
        source: Path,
        name: str,
        purpose: str,
        preserve_source: bool = False,
    ) -> Path:
        expected = {"schema_version": SCHEMA_VERSION, "purpose": purpose}
        marker = source / MANAGED_MARKER
        if (
            not source.is_dir()
            or source.is_symlink()
            or not marker.is_file()
            or marker.is_symlink()
            or load_json(marker) != expected
        ):
            raise WorkspaceError(
                f"topology runtime input is not managed for {purpose}: {source}"
            )
        container = topology_root / "runtime"
        if container.exists() or container.is_symlink():
            if not container.is_dir() or container.is_symlink():
                raise WorkspaceError(
                    f"topology runtime container is invalid: {container}"
                )
        else:
            container.mkdir()
        destination = container / name
        if destination.exists() or destination.is_symlink():
            destination_marker = destination / MANAGED_MARKER
            if (
                not destination.is_dir()
                or destination.is_symlink()
                or not destination_marker.is_file()
                or destination_marker.is_symlink()
                or load_json(destination_marker) != expected
            ):
                raise WorkspaceError(
                    f"topology runtime destination is not managed: {destination}"
                )
            shutil.rmtree(destination)
        if preserve_source:
            shutil.copytree(source, destination)
        else:
            source.replace(destination)
        return destination

    def _prepare_topology_client_runtime(
        self, topology_root: Path, selected: dict[str, Path]
    ) -> Path:
        runtime = self._reset_topology_subdirectory(
            topology_root, "client-runtime", "topology-client-runtime"
        )
        source = selected["client"]
        for entry in source.iterdir():
            if entry.name in {".git", "build", "sound", MANAGED_MARKER}:
                continue
            (runtime / entry.name).symlink_to(
                entry, target_is_directory=entry.is_dir()
            )
        (runtime / "sound").symlink_to(
            selected["sound"], target_is_directory=True
        )
        return runtime

    @staticmethod
    def _recorded_process_running(record: Any) -> bool:
        if not isinstance(record, dict):
            return False
        pid = record.get("pid")
        start_time = record.get("start_time")
        return (
            isinstance(pid, int)
            and not isinstance(pid, bool)
            and isinstance(start_time, str)
            and process_matches(pid, start_time)
        )

    def topology_status(self, name: str) -> dict[str, Any]:
        root = self._topology_directory(name)
        status_path = root / "status.json"
        if not status_path.is_file() or status_path.is_symlink():
            raise WorkspaceError(f"topology has not been started: {name}")
        status = load_json(status_path)
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
        optional = {"stack", "providers"}
        historical_record = isinstance(status, dict) and not (
            {"stack", "providers"} & set(status)
        )
        historical_coordinate_keys = {
            "path",
            "checkout_path",
            "checkout",
            "source",
            "head",
            "dirty",
        }
        coordinate_historical_record = (
            isinstance(status, dict)
            and not historical_record
            and isinstance(status.get("resolved"), dict)
            and bool(status["resolved"])
            and all(
                isinstance(record, dict)
                and set(record) == historical_coordinate_keys
                for record in status["resolved"].values()
            )
        )
        if (
            not isinstance(status, dict)
            or status.get("schema_version") != SCHEMA_VERSION
            or status.get("name") != name
            or not required <= set(status) <= required | optional | {"error"}
            or not isinstance(status.get("dependencies"), list)
            or not isinstance(status.get("resolved"), dict)
            or not isinstance(status.get("ready"), bool)
            or not isinstance(status.get("profile"), str)
            or not status["profile"]
            or not all(
                isinstance(dependency, str)
                and validate_name(dependency, "topology dependency")
                for dependency in status["dependencies"]
            )
            or len(status["dependencies"]) != len(set(status["dependencies"]))
            or status.get("state") is not None
            and (
                not isinstance(status.get("state"), str)
                or not Path(status["state"]).is_absolute()
            )
            or not isinstance(status.get("build_root"), str)
            or not Path(status["build_root"]).is_absolute()
            or not isinstance(status.get("started_at"), str)
            or not status["started_at"]
            or status.get("stopped_at") is not None
            and not isinstance(status.get("stopped_at"), str)
            or "error" in status
            and not isinstance(status.get("error"), str)
        ):
            raise WorkspaceError(f"topology status is invalid: {name}")
        if historical_record:
            status["stack"] = "classic"
            status["providers"] = {
                role: {
                    "client": "classic-client",
                    "server": "classic-server",
                    "protocol": "classic-protocol",
                    "libatrinik": "classic-libatrinik",
                    "content": "content-1x",
                }.get(role, role)
                for role in status["dependencies"]
            }
            status["inert_historical_record"] = True
        elif coordinate_historical_record:
            stack_name = status.get("stack")
            providers = status.get("providers")
            if (
                not isinstance(stack_name, str)
                or not isinstance(providers, dict)
                or set(providers) != set(status["dependencies"])
                or any(
                    not isinstance(role, str)
                    or not isinstance(component, str)
                    for role, component in providers.items()
                )
                or set(status["resolved"]) != set(providers.values())
            ):
                raise WorkspaceError(
                    f"historical topology identity status is invalid: {name}"
                )
            status["inert_historical_record"] = True
        else:
            stack_name = status.get("stack")
            providers = status.get("providers")
            if (
                not isinstance(stack_name, str)
                or stack_name not in self.manifest.stacks
                or not isinstance(providers, dict)
                or set(providers) != set(status["dependencies"])
                or any(
                    not isinstance(component, str)
                    or component
                    != self.manifest.provider(stack_name, role).name
                    for role, component in providers.items()
                )
            ):
                raise WorkspaceError(f"topology stack/provider status is invalid: {name}")
            if set(status["resolved"]) != set(providers.values()):
                raise WorkspaceError(
                    f"topology resolution/provider set is invalid: {name}"
                )
        for component, resolved in status["resolved"].items():
            expected_resolved = (
                {"path", "head", "dirty"}
                if historical_record
                else historical_coordinate_keys
                if coordinate_historical_record
                else {
                    "path",
                    "checkout_path",
                    "checkout",
                    "repository",
                    "branch",
                    "source",
                    "head",
                    "dirty",
                }
            )
            if (
                (
                    not historical_record
                    and component not in status["providers"].values()
                )
                or not isinstance(resolved, dict)
                or set(resolved) != expected_resolved
                or not isinstance(resolved.get("path"), str)
                or not Path(resolved["path"]).is_absolute()
                or not historical_record
                and (
                    not isinstance(resolved.get("checkout_path"), str)
                    or not Path(resolved["checkout_path"]).is_absolute()
                    or not isinstance(resolved.get("checkout"), str)
                    or (
                        not coordinate_historical_record
                        and (
                            not isinstance(resolved.get("repository"), str)
                            or not isinstance(resolved.get("branch"), str)
                        )
                    )
                    or not isinstance(resolved.get("source"), str)
                )
                or not isinstance(resolved.get("head"), str)
                or not re.fullmatch(r"[0-9a-f]{40,64}", resolved["head"])
                or not isinstance(resolved.get("dirty"), bool)
            ):
                raise WorkspaceError(f"topology resolution status is invalid: {name}")
            if not historical_record and not coordinate_historical_record:
                provider = self.manifest.by_name[component]
                checkout_path = Path(resolved["checkout_path"]).resolve(
                    strict=False
                )
                expected_path = (
                    checkout_path
                    if provider.source == "."
                    else checkout_path.joinpath(
                        *PurePosixPath(provider.source).parts
                    )
                ).resolve(strict=False)
                if (
                    resolved["checkout"] != provider.checkout_name
                    or resolved["repository"] != provider.repository
                    or resolved["branch"] != provider.branch
                    or resolved["source"] != provider.source
                    or Path(resolved["path"]).resolve(strict=False)
                    != expected_path
                ):
                    raise WorkspaceError(
                        f"topology component identity is invalid: {name}/{component}"
                    )
        supervisor = status.get("supervisor")
        if (
            not isinstance(supervisor, dict)
            or set(supervisor) != {"pid", "start_time"}
            or not isinstance(supervisor.get("pid"), int)
            or isinstance(supervisor.get("pid"), bool)
            or supervisor["pid"] <= 0
            or not isinstance(supervisor.get("start_time"), str)
            or not supervisor["start_time"].isdigit()
        ):
            raise WorkspaceError(f"topology supervisor status is invalid: {name}")
        supervisor_running = self._recorded_process_running(supervisor)
        supervisor["running"] = supervisor_running
        endpoint = status.get("endpoint")
        if endpoint is not None:
            fingerprint = endpoint.get("fingerprint") if isinstance(endpoint, dict) else None
            if (
                not isinstance(endpoint, dict)
                or set(endpoint) != {"host", "port", "fingerprint"}
                or endpoint.get("host") != "127.0.0.1"
                or not isinstance(endpoint.get("port"), int)
                or isinstance(endpoint.get("port"), bool)
                or not 1 <= endpoint["port"] <= 65535
                or (
                    fingerprint is not None
                    and (
                        not isinstance(fingerprint, str)
                        or len(fingerprint) != 64
                        or any(character not in "0123456789abcdef" for character in fingerprint)
                    )
                )
            ):
                raise WorkspaceError(f"topology endpoint status is invalid: {name}")
        services = status.get("services")
        if (
            not isinstance(services, dict)
            or (not services and not status.get("error"))
            or not set(services) <= set(TOPOLOGY_SERVICES)
        ):
            raise WorkspaceError(f"topology service status is invalid: {name}")
        for service in services.values():
            if (
                not isinstance(service, dict)
                or set(service)
                != {"pid", "start_time", "status", "exit_code", "log", "cwd"}
                or not isinstance(service.get("pid"), int)
                or isinstance(service.get("pid"), bool)
                or service["pid"] <= 0
                or not isinstance(service.get("start_time"), str)
                or not service["start_time"].isdigit()
                or service.get("status") not in {"starting", "running", "exited"}
                or (
                    service.get("exit_code") is not None
                    and (
                        not isinstance(service.get("exit_code"), int)
                        or isinstance(service.get("exit_code"), bool)
                    )
                )
                or not isinstance(service.get("log"), str)
                or not Path(service["log"]).is_absolute()
                or not isinstance(service.get("cwd"), str)
                or not Path(service["cwd"]).is_absolute()
            ):
                raise WorkspaceError(f"topology service status is invalid: {name}")
            service["running"] = self._recorded_process_running(service)
            if (
                service.get("status") in {"starting", "running"}
                and not service["running"]
            ):
                service["status"] = "stale"
        if (
            ("server" in services) != (endpoint is not None)
            or (
                status["ready"]
                and endpoint is not None
                and endpoint["fingerprint"] is None
            )
        ):
            raise WorkspaceError(f"topology endpoint status is invalid: {name}")
        if not supervisor_running:
            status["ready"] = False
        return status

    def topology_statuses(self) -> list[dict[str, Any]]:
        self.paths.ensure()
        statuses: list[dict[str, Any]] = []
        for path in sorted(self.paths.topologies.iterdir()):
            if path.is_dir() and not path.is_symlink() and (path / "status.json").is_file():
                statuses.append(self.topology_status(path.name))
        return statuses

    @staticmethod
    def _select_topology_port(requested: int | None) -> int:
        if requested is not None and (
            not isinstance(requested, int)
            or isinstance(requested, bool)
            or not 0 <= requested <= 65535
        ):
            raise WorkspaceError("topology port must be between 0 and 65535")
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as candidate:
                candidate.bind(("0.0.0.0", requested or 0))
                return int(candidate.getsockname()[1])
        except OSError as error:
            if requested:
                raise WorkspaceError(
                    f"topology UDP port {requested} is unavailable: {error}"
                ) from error
            raise WorkspaceError(f"cannot allocate a topology UDP port: {error}") from error

    @staticmethod
    def _require_client_display() -> None:
        wayland = os.environ.get("WAYLAND_DISPLAY")
        runtime = os.environ.get("XDG_RUNTIME_DIR")
        if wayland and runtime and (Path(runtime) / wayland).is_socket():
            return
        display = os.environ.get("DISPLAY", "")
        if display.startswith(":"):
            display_number = display[1:].split(".", 1)[0]
            if display_number.isdigit() and Path(
                f"/tmp/.X11-unix/X{display_number}"
            ).is_socket():
                return
        raise WorkspaceError(
            "client display forwarding is unavailable; reload or reopen the "
            "devcontainer before starting the client"
        )

    def topology_up(
        self,
        name: str,
        profile_name: str,
        state_name: str,
        services: list[str] | None = None,
        port: int | None = None,
    ) -> dict[str, Any]:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._topology_up(
                name, profile_name, state_name, services, port
            )

    def _topology_up(
        self,
        name: str,
        profile_name: str,
        state_name: str,
        services: list[str] | None = None,
        port: int | None = None,
    ) -> dict[str, Any]:
        selected_services = self._topology_services(services)
        if "client" in selected_services:
            client_launch_label(profile_name, name)
        if "server" not in selected_services and port is not None:
            raise WorkspaceError("--port requires the server service")
        self._require_classic_contracts(profile_name, set(selected_services))
        topology_root = self._topology_directory(name, create=True)
        operation_lock = topology_root / "operation.lock"
        with exclusive_lock(
            operation_lock, f"topology {name} operation", nonblocking=True
        ):
            status_path = topology_root / "status.json"
            startup_error_path = topology_root / "startup-error.json"
            if status_path.is_file():
                if status_path.is_symlink():
                    raise WorkspaceError(f"topology status is invalid: {name}")
                previous = self.topology_status(name)
                if previous.get("inert_historical_record"):
                    raise WorkspaceError(
                        f"topology {name} is an inert pre-migration record; "
                        "choose a new topology name"
                    )
                if previous["supervisor"]["running"] or any(
                    service["running"] for service in previous["services"].values()
                ):
                    raise WorkspaceError(f"topology is already running: {name}")

            if "client" in selected_services:
                self._require_client_display()

            selected = self._resolve_build_profile(
                profile_name, set(selected_services)
            )
            required = set(selected)
            targets = [
                service
                for service in ("client", "server")
                if service in selected_services
            ]

            with ExitStack() as stack:
                state_location: Path | None = None
                state_lock: TextIO | None = None
                if "server" in selected_services:
                    state_location = self._state_location(state_name)
                    state_lock = stack.enter_context(
                        exclusive_lock(
                            Path(f"{state_location}.lock"),
                            f"server state {state_location}",
                            nonblocking=True,
                        )
                    )

                root = self._build_resolved(
                    "topology", profile_name, False, targets, selected
                )
                endpoint: dict[str, Any] | None = None
                if "server" in selected_services:
                    stack.enter_context(
                        exclusive_lock(
                            self.paths.topologies / "ports.lock",
                            "topology port allocation",
                        )
                    )
                    endpoint = {
                        "host": "127.0.0.1",
                        "port": self._select_topology_port(port),
                    }
                service_specs: dict[str, dict[str, Any]] = {}
                if "server" in selected_services:
                    assert state_location is not None
                    state = self.state_path(
                        state_name,
                        selected["server"],
                        resolved_path=state_location,
                    )
                    content = self._take_topology_runtime_input(
                        topology_root,
                        root / "runtime" / "content",
                        "content",
                        "collected-content",
                    )
                    resources = self._take_topology_runtime_input(
                        topology_root,
                        root / "runtime" / "resources",
                        "resources",
                        "resource-view",
                    )
                    client_maps = self._take_topology_runtime_input(
                        topology_root,
                        root / "runtime" / "client-maps",
                        "client-maps",
                        "region-map-cache",
                        preserve_source=True,
                    )
                    runtime = self._prepare_server_runtime(
                        root,
                        selected,
                        state,
                        state_name,
                        content,
                        resources,
                        client_maps,
                    )
                    executable = runtime / "atrinik-server"
                    service_specs["server"] = {
                        "command": [
                            str(executable),
                            f"--port_quic={endpoint['port']}",
                            "--port_mapping=off",
                            "--stun_server=off",
                            f"--assetspath={runtime / 'assets'}",
                            "--no_console",
                        ],
                        "cwd": str(runtime),
                        "log": str(topology_root / "server.log"),
                    }
                if "client" in selected_services:
                    executable = (
                        self._classic_binary_directory(root, "client") / "atrinik"
                    )
                    working = self._prepare_topology_client_runtime(
                        topology_root, selected
                    )
                    if not executable.is_file():
                        raise WorkspaceError(f"client executable is missing: {executable}")
                    client_config = topology_root / "client-config"
                    if client_config.exists() or client_config.is_symlink():
                        if not client_config.is_dir() or client_config.is_symlink():
                            raise WorkspaceError(
                                f"client configuration path is invalid: {client_config}"
                            )
                    else:
                        client_config.mkdir()
                    service_specs["client"] = {
                        "command": [str(executable)],
                        "cwd": str(working),
                        "log": str(topology_root / "client.log"),
                        "environment": {
                            "ATRINIK_CONFIG_DIR": str(client_config.resolve())
                        },
                    }

                profile = self._load_profile(profile_name, require_file=False)
                selected_stack = self.manifest.stack(profile["stack"])
                checkout_states = self._selected_checkout_states(
                    profile, selected, include_dirty=True
                )
                resolved_status = {
                    selected_stack.providers[role].name: {
                        "path": str(path),
                        "checkout_path": str(
                            checkout_states[
                                selected_stack.providers[role].checkout_name
                            ]["path"]
                        ),
                        "checkout": selected_stack.providers[role].checkout_name,
                        "repository": selected_stack.providers[role].repository,
                        "branch": selected_stack.providers[role].branch,
                        "source": selected_stack.providers[role].source,
                        "head": checkout_states[
                            selected_stack.providers[role].checkout_name
                        ]["head"],
                        "dirty": checkout_states[
                            selected_stack.providers[role].checkout_name
                        ]["dirty"],
                    }
                    for role, path in sorted(selected.items())
                }
                spec = {
                    "schema_version": SCHEMA_VERSION,
                    "name": name,
                    "profile": profile_name,
                    "stack": selected_stack.name,
                    "providers": {
                        role: selected_stack.providers[role].name
                        for role in sorted(required)
                    },
                    "dependencies": sorted(required),
                    "state": str(state_location) if state_location else None,
                    "build_root": str(root),
                    "resolved": resolved_status,
                    "endpoint": endpoint,
                    "services": service_specs,
                }
                spec_path = topology_root / "spec.json"
                atomic_json(spec_path, spec)
                status_path.unlink(missing_ok=True)
                startup_error_path.unlink(missing_ok=True)

                supervisor_log_path = topology_root / "supervisor.log"
                supervisor_log = os.fdopen(
                    open_regular_file(
                        supervisor_log_path,
                        os.O_WRONLY | os.O_CREAT | os.O_APPEND,
                        "topology supervisor log",
                    ),
                    "ab",
                    buffering=0,
                )
                try:
                    command = [
                        sys.executable,
                        "-m",
                        "atrinik_workspace.supervisor",
                        "--daemonize",
                        "--spec",
                        str(spec_path),
                    ]
                    pass_fds: tuple[int, ...] = ()
                    if state_lock is not None:
                        command.extend(["--lock-fd", str(state_lock.fileno())])
                        pass_fds = (state_lock.fileno(),)
                    environment = os.environ.copy()
                    source_root = str(Path(__file__).resolve().parents[1])
                    python_path = environment.get("PYTHONPATH")
                    environment["PYTHONPATH"] = (
                        source_root
                        if not python_path
                        else source_root + os.pathsep + python_path
                    )
                    process = subprocess.Popen(
                        command,
                        cwd=self.paths.repository,
                        env=environment,
                        stdin=subprocess.DEVNULL,
                        stdout=supervisor_log,
                        stderr=subprocess.STDOUT,
                        start_new_session=True,
                        pass_fds=pass_fds,
                    )
                except OSError as error:
                    raise WorkspaceError(f"cannot start topology supervisor: {error}") from error
                finally:
                    supervisor_log.close()

                deadline = time.monotonic() + 45
                while time.monotonic() < deadline:
                    if startup_error_path.is_file():
                        if startup_error_path.is_symlink():
                            raise WorkspaceError(
                                f"topology startup error is invalid: {name}"
                            )
                        failure = load_json(startup_error_path)
                        if (
                            not isinstance(failure, dict)
                            or set(failure) != {"error"}
                            or not isinstance(failure.get("error"), str)
                        ):
                            raise WorkspaceError(
                                f"topology startup error is invalid: {name}"
                            )
                        raise WorkspaceError(
                            f"topology supervisor failed: {failure['error']}"
                        )
                    if status_path.is_file():
                        status = self.topology_status(name)
                        if status.get("error"):
                            raise WorkspaceError(
                                f"topology supervisor failed: {status['error']}"
                            )
                        if status["supervisor"]["running"] and status["ready"]:
                            process.wait(timeout=2)
                            return status
                    if process.poll() not in (None, 0):
                        break
                    time.sleep(0.1)
                raise WorkspaceError(
                    f"topology supervisor failed to start; inspect "
                    f"{topology_root / 'supervisor.log'}"
                )

    def topology_down(self, name: str, timeout: float = 15) -> dict[str, Any]:
        root = self._topology_directory(name)
        with exclusive_lock(
            root / "operation.lock", f"topology {name} operation", nonblocking=True
        ):
            status = self.topology_status(name)
            supervisor = status["supervisor"]
            orphaned = [
                service
                for service in status["services"].values()
                if service["running"]
            ]
            targets = [supervisor] if supervisor["running"] else orphaned
            if not targets:
                return status
            for record in targets:
                pid = record["pid"]
                start_time = record["start_time"]
                try:
                    pidfd = os.pidfd_open(pid)
                except ProcessLookupError:
                    continue
                try:
                    if process_matches(pid, start_time):
                        signal.pidfd_send_signal(pidfd, signal.SIGTERM)
                finally:
                    os.close(pidfd)
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if not any(self._recorded_process_running(record) for record in targets):
                    status = self.topology_status(name)
                    return status
                time.sleep(0.1)
            raise WorkspaceError(
                f"topology did not stop within {timeout:g} seconds: {name}"
            )

    def topology_logs(
        self,
        name: str,
        service: str | None,
        tail: int,
        follow: bool,
    ) -> None:
        if tail < 0:
            raise WorkspaceError("log tail must not be negative")
        root = self._topology_directory(name)
        services = [service] if service else list(TOPOLOGY_SERVICES)
        unknown = sorted(set(services) - set(TOPOLOGY_SERVICES))
        if unknown:
            raise WorkspaceError(f"unknown topology services: {', '.join(unknown)}")
        paths = [(item, root / f"{item}.log") for item in services]
        paths = [(item, path) for item, path in paths if path.is_file()]
        if not paths:
            raise WorkspaceError(f"topology has no matching logs: {name}")
        positions: dict[Path, tuple[int, int, int]] = {}
        for item, path in paths:
            with os.fdopen(
                open_regular_file(path, os.O_RDONLY, "topology log"),
                encoding="utf-8",
                errors="replace",
            ) as stream:
                lines = deque(stream, maxlen=tail) if tail else ()
                if len(paths) > 1:
                    print(f"==> {item} <==")
                for line in lines:
                    print(line, end="")
                metadata = os.fstat(stream.fileno())
                positions[path] = (metadata.st_dev, metadata.st_ino, stream.tell())
        while follow:
            changed = False
            for item, path in paths:
                try:
                    descriptor = open_regular_file(
                        path, os.O_RDONLY, "topology log"
                    )
                except WorkspaceError:
                    continue
                with os.fdopen(
                    descriptor, encoding="utf-8", errors="replace"
                ) as stream:
                    metadata = os.fstat(stream.fileno())
                    device, inode, offset = positions[path]
                    if (
                        (metadata.st_dev, metadata.st_ino) != (device, inode)
                        or metadata.st_size < offset
                    ):
                        offset = 0
                    stream.seek(offset)
                    content = stream.read()
                    positions[path] = (
                        metadata.st_dev,
                        metadata.st_ino,
                        stream.tell(),
                    )
                if content:
                    if len(paths) > 1:
                        print(f"==> {item} <==")
                    print(content, end="", flush=True)
                    changed = True
            try:
                status = self.topology_status(name)
                running = status["supervisor"]["running"]
            except WorkspaceError:
                running = False
            if not running and not changed:
                break
            time.sleep(0.25)

    def run_client(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
    ) -> Path:
        self._validate_run_port(port)
        launch_label = client_launch_label(profile_name)
        self._require_classic_contracts(profile_name, {"client"})
        state = self._state_location(state_name)
        self._validate_state(state)
        fingerprint = self._server_identity_fingerprint(state)
        root = self.build("client", profile_name, tests=False)
        executable = self._classic_binary_directory(root, "client") / "atrinik"
        working = root / "sources" / "client"
        if not executable.is_file():
            raise WorkspaceError(f"client executable is missing: {executable}")
        command = [
            str(executable),
            f"--server=127.0.0.1 {port} {fingerprint}",
            "--stun_server=off",
            "--nometa",
            *arguments,
        ]
        print(f"state: {state}")
        print(f"cwd: {working}")
        print(f"launch label: {launch_label}")
        print(f"command: {display_arguments(command)}")
        if not dry_run:
            self._require_client_display()
            environment = os.environ.copy()
            environment[CLIENT_LAUNCH_LABEL_ENV] = launch_label
            run(
                command,
                cwd=working,
                env=environment,
                diagnostics_to_stderr=False,
            )
        return executable

    def run_server(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
    ) -> Path:
        self.paths.ensure()
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
            return self._run_server(
                profile_name, state_name, port, arguments, dry_run
            )

    def _run_server(
        self,
        profile_name: str,
        state_name: str,
        port: int,
        arguments: list[str],
        dry_run: bool,
    ) -> Path:
        self._validate_run_port(port)
        self._require_classic_contracts(profile_name, {"server"})
        targets = self._expand_build_target("server", profile_name)
        selected = self._resolve_build_profile(profile_name, {"server"})
        state_location = self._state_location(state_name)
        lock_path = Path(f"{state_location}.lock")
        with exclusive_lock(lock_path, f"server state {state_location}", nonblocking=True):
            root = self._build_resolved(
                "server", profile_name, False, targets, selected
            )
            state = self.state_path(
                state_name, selected["server"], resolved_path=state_location
            )
            runtime = self._prepare_server_runtime(root, selected, state, state_name)
            executable = runtime / "atrinik-server"
            command = [
                str(executable),
                f"--port_quic={port}",
                "--port_mapping=off",
                "--stun_server=off",
                *arguments,
                f"--assetspath={runtime / 'assets'}",
            ]
            print(f"state: {state}")
            print(f"cwd: {runtime}")
            print(f"command: {display_arguments(command)}")
            if not dry_run:
                run(command, cwd=runtime, diagnostics_to_stderr=False)
            return executable

    @staticmethod
    def _validate_run_port(port: int) -> None:
        if (
            not isinstance(port, int)
            or isinstance(port, bool)
            or not 1 <= port <= 65535
        ):
            raise WorkspaceError("server UDP port must be between 1 and 65535")

    @staticmethod
    def _server_identity_fingerprint(state: Path) -> str:
        identity = state / "quic-identity.pem"
        if not identity.exists():
            raise WorkspaceError(
                f"server QUIC identity does not exist: {identity}; start the "
                "matching server before launching the foreground client"
            )
        try:
            descriptor = open_regular_file(
                identity, os.O_RDONLY, "server QUIC identity"
            )
            with os.fdopen(descriptor, encoding="ascii") as stream:
                contents = stream.read(SERVER_IDENTITY_MAX_SIZE + 1)
        except UnicodeError as error:
            raise WorkspaceError(
                f"server QUIC identity is not ASCII PEM: {identity}"
            ) from error
        if len(contents) > SERVER_IDENTITY_MAX_SIZE:
            raise WorkspaceError(f"server QUIC identity is too large: {identity}")

        begin = "-----BEGIN CERTIFICATE-----"
        end = "-----END CERTIFICATE-----"
        start = contents.find(begin)
        finish = contents.find(end, start + len(begin)) if start >= 0 else -1
        if start < 0 or finish < 0:
            raise WorkspaceError(f"server QUIC identity lacks a certificate: {identity}")
        certificate = contents[start : finish + len(end)]
        try:
            encoded = ssl.PEM_cert_to_DER_cert(certificate)
        except (binascii.Error, ValueError) as error:
            raise WorkspaceError(
                f"server QUIC identity contains an invalid certificate: {identity}"
            ) from error
        return hashlib.sha256(encoded).hexdigest()

    def _link_server_runtime_inputs(
        self,
        runtime: Path,
        root: Path,
        selected: dict[str, Path],
        state: Path,
        content: Path,
        resources: Path,
    ) -> None:
        source = selected["server"]
        binary = self._classic_binary_directory(root, "server")
        links = {
            "atrinik-server": binary / "atrinik-server",
            "libplugin_arena.so": binary / "libplugin_arena.so",
            "libplugin_python.so": binary / "libplugin_python.so",
            "lib": content / "lib",
            "maps": content / "maps",
            "resources": resources,
            "data": state,
            "tools": source / "tools",
        }
        for name, target in links.items():
            if not target.exists():
                raise WorkspaceError(f"server runtime input is missing: {target}")
            (runtime / name).symlink_to(
                target, target_is_directory=target.is_dir()
            )
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            target = source / name
            if not target.is_file():
                raise WorkspaceError(f"server runtime input is missing: {target}")
            (runtime / name).symlink_to(target)
        custom = source / "server-custom.cfg"
        if custom.is_file():
            (runtime / "server-custom.cfg").symlink_to(custom)

    def _prepare_server_runtime(
        self,
        root: Path,
        selected: dict[str, Path],
        state: Path,
        state_name: str,
        content: Path | None = None,
        resources: Path | None = None,
        client_maps: Path | None = None,
    ) -> Path:
        state_key = profile_key({"state": state})
        runtime = root / "run" / "server" / f"{state_name}-{state_key}"
        managed_reset(runtime, self.paths.builds, f"server-runtime:{state_key}")
        content = content or root / "runtime" / "content"
        resources = resources or root / "runtime" / "resources"
        client_maps = client_maps or root / "runtime" / "client-maps"
        self._validate_region_maps(client_maps)
        self._link_server_runtime_inputs(
            runtime, root, selected, state, content, resources
        )
        assets = runtime / "assets"
        self._prepare_asset_staging_directory(assets)
        self._prepare_asset_staging_directory(assets / "data")
        shutil.copytree(client_maps, assets / "client-maps")
        return runtime

    @staticmethod
    def _prepare_asset_staging_directory(path: Path) -> None:
        if path.is_symlink():
            raise WorkspaceError(f"server asset staging path is invalid: {path}")
        try:
            mode = path.lstat().st_mode
        except FileNotFoundError:
            path.mkdir()
            return
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect server asset staging path {path}: {error}"
            ) from error
        if not stat.S_ISDIR(mode):
            raise WorkspaceError(f"server asset staging path is invalid: {path}")

    def _component(self, name: str) -> Component:
        try:
            return self.manifest.by_name[name]
        except KeyError as error:
            raise WorkspaceError(f"unknown component: {name}") from error

    def _resolve_checkout(self, name: str) -> Checkout:
        if name in self.manifest.by_checkout:
            return self.manifest.by_checkout[name]
        if name in self.manifest.by_name:
            return self.manifest.checkout_for(name)
        raise WorkspaceError(f"unknown component or checkout: {name}")
