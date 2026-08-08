from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
from typing import Any, Iterable

from .migration import MIGRATION_PENDING, MIGRATION_RECORD, OPERATION_PATHS
from .model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
    WorkspaceError,
    atomic_json,
    load_json,
    managed_remove,
)
from .supervisor import process_matches
from .workspace import (
    BUILD_METADATA,
    BUILD_METADATA_SCHEMA_VERSION,
    CACHE_METADATA,
    _remote_matches,
    exclusive_lock,
)


CLEANUP_SCHEMA_VERSION = 1
DEFAULT_SCOPES = ("worktrees", "builds")
ALL_SCOPES = (*DEFAULT_SCOPES, "npm-cache")
BUILD_RETENTION_RECORD = "retention.json"
BUILD_METADATA_KEYS = {
    "schema_version",
    "profile",
    "key",
    "purpose",
    "coordinates",
    "last_used_at",
}
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


def _command(path: Path, *arguments: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(path), *arguments],
            check=True,
            capture_output=True,
            text=True,
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
    value = _command(path, "rev-parse", "--path-format=absolute", "--git-common-dir")
    return Path(value).resolve()


def _git_directory(path: Path) -> Path:
    value = _command(path, "rev-parse", "--path-format=absolute", "--git-dir")
    return Path(value).resolve()


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
    stack = [root]
    try:
        excluded_paths = {path.resolve(strict=False) for path in excluded}
        while stack:
            path = stack.pop()
            normalized = path.resolve(strict=False)
            if path != root and normalized in excluded_paths:
                continue
            metadata = path.lstat()
            key = (metadata.st_dev, metadata.st_ino)
            sizes.setdefault(key, metadata.st_blocks * 512)
            maximum = metadata.st_mtime if maximum is None else max(maximum, metadata.st_mtime)
            if stat.S_ISDIR(metadata.st_mode) and not stat.S_ISLNK(metadata.st_mode):
                with os.scandir(path) as entries:
                    stack.extend(Path(entry.path) for entry in entries)
    except (OSError, RuntimeError) as error:
        return {}, None, str(error)
    observed = (
        datetime.fromtimestamp(maximum, timezone.utc) if maximum is not None else None
    )
    return sizes, observed, None


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
        with exclusive_lock(
            self.paths.workspace / "repository-layout.lock",
            "repository layout",
        ):
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
            for target in targets:
                identity = (target["kind"], target["path"])
                if identity in completed:
                    continue
                try:
                    fresh = self._plan(
                        selected_scopes, older_than_days, selected_names, "apply"
                    )
                except (OSError, RuntimeError, WorkspaceError) as error:
                    target["disposition"] = "error"
                    target["reasons"] = ["revalidation_error"]
                    target["error"] = str(error)
                    report["aborted"] = True
                    break
                match = next(
                    (
                        item
                        for item in fresh["items"]
                        if item["kind"] == target["kind"]
                        and item["path"] == target["path"]
                    ),
                    None,
                )
                if match is None or match["disposition"] != "eligible":
                    target["disposition"] = "error"
                    target["reasons"] = ["revalidation_failed"]
                    if match is not None:
                        target["revalidation"] = {
                            "disposition": match["disposition"],
                            "reasons": match["reasons"],
                        }
                    report["aborted"] = True
                    break
                for key, value in match.items():
                    if not key.startswith("_"):
                        target[key] = value
                report["mutation_attempted"] = True
                try:
                    self._remove(match)
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
            return report

    @staticmethod
    def _normalize_scopes(scopes: list[str]) -> list[str]:
        requested = scopes or list(DEFAULT_SCOPES)
        if "all" in requested:
            requested = list(ALL_SCOPES)
        return [scope for scope in ALL_SCOPES if scope in set(requested)]

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
        self.now = datetime.now(timezone.utc)
        self._repositories = {}
        self._github_cache = {}
        references, reference_errors = self._references()
        items: list[dict[str, Any]] = []
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
        if "npm-cache" in scopes:
            cache = self._npm_cache(older_than_days, references, reference_errors)
            if cache is not None:
                items.append(cache)
        items.sort(key=lambda item: (item["kind"], item["owner"], item["path"]))
        self._credit_sizes(items)
        for item in items:
            item.pop("_inodes", None)
            item.pop("_primary", None)
            item.pop("_purpose", None)
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
                records = _worktree_records(invocation)
                common = _git_common_directory(invocation)
                primary = None
                for row in records:
                    candidate = Path(row.get("worktree", ""))
                    if not candidate.is_dir() or candidate.is_symlink():
                        continue
                    try:
                        if _git_directory(candidate) == common:
                            primary = candidate
                            break
                    except WorkspaceError:
                        continue
                if primary is None or not self._remote_identity(primary, repository):
                    raise WorkspaceError("repository identity is unproven")
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
        if not self.paths.profiles.is_dir() or self.paths.profiles.is_symlink():
            if self.paths.profiles.exists() or self.paths.profiles.is_symlink():
                errors.add("profile_inventory_error")
            return
        for path in sorted(self.paths.profiles.glob("*.json")):
            try:
                if path.is_symlink():
                    raise WorkspaceError("profile is a symlink")
                profile = self.workspace._load_profile(path.stem, require_file=True)
                for component_name, selector in profile["components"].items():
                    if component_name not in self.manifest.by_name or not isinstance(
                        selector, dict
                    ):
                        raise WorkspaceError("profile selector is invalid")
                    if set(selector) != {"kind", "value"} or not isinstance(
                        selector.get("value"), str
                    ):
                        raise WorkspaceError("profile selector is invalid")
                    checkout = self.manifest.by_name[component_name].checkout_name
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
                live = False
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
                    live = live or process_matches(pid, start_time)
                if not live:
                    continue
                build_root = status_value.get("build_root")
                resolved = status_value.get("resolved")
                if (
                    status_value.get("schema_version") != SCHEMA_VERSION
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
        for relative in (MIGRATION_RECORD, MIGRATION_PENDING):
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
            ("composite_worktrees", ("destination",)),
            ("worktrees", ("destination",)),
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
                    item = self._worktree_item(
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
                    items.append(item)
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
        try:
            if _git_common_directory(path) != common:
                item["reasons"].append("unexpected_git_common_directory")
            if _git_directory(path) == common and normalized != primary.resolve():
                item["reasons"].append("unexpected_primary_identity")
            if self._operation_in_progress(path):
                item["reasons"].append("git_operation_in_progress")
            status_value = _command(
                path, "status", "--porcelain=v1", "--untracked-files=all"
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
            ignored_sizes: dict[tuple[int, int], int] = {}
            for raw in ignored_paths:
                candidate = path / raw
                sizes, _, _ = _tree_usage(candidate)
                ignored_sizes.update(sizes)
            item["ignored_paths"] = len(ignored_paths)
            item["ignored_bytes"] = sum(ignored_sizes.values())
        except WorkspaceError as error:
            item["reasons"].append("git_inspection_error")
            item["error"] = str(error)
        if not item["reasons"]:
            item["disposition"] = "skipped"
            item["reasons"] = ["github_pending"]
        return item

    @staticmethod
    def _operation_in_progress(path: Path) -> bool:
        for name in OPERATION_PATHS:
            operation_path = Path(_command(path, "rev-parse", "--git-path", name))
            if not operation_path.is_absolute():
                operation_path = path / operation_path
            if operation_path.exists() or operation_path.is_symlink():
                return True
        return False

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
                executor.submit(self._github_pulls, repository, head): (repository, head)
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
                item["reasons"] = ["merged_pr_head"]
                if item["kind"] == "prunable-metadata":
                    item["reasons"].append("prunable_metadata")

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
                    ".[] | {number,state,merged_at,head:{sha:.head.sha},"
                    "base:{ref:.base.ref},html_url}",
                ],
                check=True,
                capture_output=True,
                text=True,
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
        state_value = row.get("state")
        return (
            set(row) == {"number", "state", "html_url", "merged_at", "head", "base"}
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
            and set(base) == {"ref"}
            and isinstance(base.get("ref"), str)
            and bool(base["ref"])
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
                return [item]
            return []
        try:
            profile_roots = sorted(profiles.iterdir())
        except OSError as error:
            item = _base_item(
                "unmanaged-build", "atrinik", "atrinik/atrinik", profiles
            )
            item["reasons"] = ["profiles_inventory_error"]
            item["error"] = str(error)
            return [item]
        return [
            self._build_item(
                path,
                older_than_days,
                registered,
                removable_worktrees,
                references,
                reference_errors,
            )
            for path in profile_roots
        ]

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
        metadata_path = path / BUILD_METADATA
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
        if not isinstance(value, dict) or set(value) != BUILD_METADATA_KEYS:
            raise WorkspaceError("build metadata fields are invalid")
        if (
            value.get("schema_version") != BUILD_METADATA_SCHEMA_VERSION
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
                    if path.name in {"profiles", "npm-cache"}:
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
        path = self.paths.builds / "npm-cache"
        if not path.exists() and not path.is_symlink():
            return None
        item = _base_item("npm-cache", "atrinik", "atrinik/atrinik", path)
        item["_purpose"] = "npm-cache"
        if self.paths.builds.is_symlink() or not self.paths.builds.is_dir():
            item["reasons"] = ["invalid_cache_path"]
            item["legacy_known_cache"] = False
            return item
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
                == self.paths.builds.resolve(strict=False) / "npm-cache"
            )
        except (OSError, RuntimeError):
            valid_path = False
        if not valid_path:
            item["reasons"].append("invalid_cache_path")
        marker = path / MANAGED_MARKER
        legacy = not marker.exists() and not marker.is_symlink()
        if not legacy:
            try:
                if marker.is_symlink() or load_json(marker) != {
                    "schema_version": SCHEMA_VERSION,
                    "purpose": "npm-cache",
                }:
                    raise WorkspaceError("npm cache marker is invalid")
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
        metadata_path = path / CACHE_METADATA
        if metadata_path.exists() or metadata_path.is_symlink():
            try:
                if metadata_path.is_symlink() or not metadata_path.is_file():
                    raise WorkspaceError("cache metadata is not a regular file")
                metadata = load_json(metadata_path)
                if (
                    not isinstance(metadata, dict)
                    or set(metadata) != {"schema_version", "purpose", "last_used_at"}
                    or metadata.get("schema_version") != BUILD_METADATA_SCHEMA_VERSION
                    or metadata.get("purpose") != "npm-cache"
                ):
                    raise WorkspaceError("cache metadata fields are invalid")
                used_at = _parse_time(metadata["last_used_at"], "cache last_used_at")
                item["age_basis"] = "last-used-at"
            except (OSError, WorkspaceError) as error:
                item["reasons"].append("invalid_cache_metadata")
                item["error"] = str(error)
                used_at = None
        else:
            used_at = observed
            item["age_basis"] = "legacy-tree-mtime"
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
            item["reasons"] = ["stale_npm_cache"]
        return item

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
            "profile-build": 0,
            "worktree": 1,
            "npm-cache": 2,
            "prunable-metadata": 3,
        }
        return order.get(item["kind"], 99), item["path"]

    def _remove(self, item: dict[str, Any]) -> None:
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
        elif item["kind"] == "worktree":
            primary = self._repositories[item["owner"]]
            _command(primary, "worktree", "remove", "--", str(path))
        elif item["kind"] == "npm-cache":
            if item.get("legacy_known_cache"):
                marker = path / MANAGED_MARKER
                if marker.exists() or marker.is_symlink():
                    raise WorkspaceError("legacy npm cache marker appeared before removal")
                atomic_json(
                    marker,
                    {"schema_version": SCHEMA_VERSION, "purpose": "npm-cache"},
                )
            managed_remove(path, self.paths.builds, "npm-cache")
        elif item["kind"] == "prunable-metadata":
            primary = self._repositories[item["owner"]]
            _command(primary, "worktree", "prune", "--expire", "now")
        else:
            raise WorkspaceError(f"unsupported cleanup target: {item['kind']}")
