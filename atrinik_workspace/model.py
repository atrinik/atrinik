from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
from typing import Any, Iterable


SCHEMA_VERSION = 1
MANAGED_MARKER = ".atrinik-workspace-managed.json"
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
REPOSITORY_PATTERN = re.compile(r"^atrinik/[a-z0-9][a-z0-9._-]*$")
BUILD_KINDS = {
    "client",
    "server",
    "protocol",
    "library",
    "content",
    "assets",
    "worker",
    "none",
}


class WorkspaceError(RuntimeError):
    """A workspace operation cannot be completed safely."""


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise WorkspaceError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> Any:
    try:
        with path.open(encoding="utf-8") as stream:
            return json.load(stream, object_pairs_hook=_reject_duplicate_keys)
    except (OSError, json.JSONDecodeError) as error:
        raise WorkspaceError(f"cannot read {path}: {error}") from error


def atomic_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def require_keys(value: dict[str, Any], expected: set[str], context: str) -> None:
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        detail = []
        if missing:
            detail.append(f"missing {', '.join(missing)}")
        if extra:
            detail.append(f"unexpected {', '.join(extra)}")
        raise WorkspaceError(f"{context}: {'; '.join(detail)}")


def validate_name(value: str, context: str) -> str:
    if not isinstance(value, str) or not NAME_PATTERN.fullmatch(value):
        raise WorkspaceError(f"{context} must use lowercase letters, digits, '.', '_' or '-'")
    return value


@dataclass(frozen=True)
class Component:
    name: str
    repository: str
    branch: str
    build: str


class Manifest:
    def __init__(self, components: list[Component]):
        self.components = components
        self.by_name = {component.name: component for component in components}

    @classmethod
    def load(cls, path: Path) -> "Manifest":
        root = load_json(path)
        if not isinstance(root, dict):
            raise WorkspaceError("component manifest root must be an object")
        require_keys(root, {"schema_version", "components"}, "component manifest")
        if root["schema_version"] != SCHEMA_VERSION:
            raise WorkspaceError("unsupported component manifest schema")
        raw_components = root["components"]
        if not isinstance(raw_components, list) or not raw_components:
            raise WorkspaceError("components must be a non-empty array")
        components: list[Component] = []
        names: set[str] = set()
        repositories: set[str] = set()
        for index, raw in enumerate(raw_components):
            context = f"component {index}"
            if not isinstance(raw, dict):
                raise WorkspaceError(f"{context} must be an object")
            require_keys(raw, {"name", "repository", "branch", "build"}, context)
            name = validate_name(raw["name"], f"{context}.name")
            repository = raw["repository"]
            branch = raw["branch"]
            build = raw["build"]
            if not isinstance(repository, str) or not REPOSITORY_PATTERN.fullmatch(repository):
                raise WorkspaceError(
                    f"{context}.repository must name an atrinik GitHub repository"
                )
            if not isinstance(branch, str) or not branch or branch.startswith("-"):
                raise WorkspaceError(f"{context}.branch is invalid")
            if not isinstance(build, str) or build not in BUILD_KINDS:
                raise WorkspaceError(f"{context}.build is invalid")
            if name in names or repository in repositories:
                raise WorkspaceError(f"duplicate component identity: {name}/{repository}")
            names.add(name)
            repositories.add(repository)
            components.append(Component(name, repository, branch, build))
        required = {"client", "server", "protocol", "libatrinik", "content", "sound", "resources"}
        missing = sorted(required - names)
        if missing:
            raise WorkspaceError(
                f"component manifest lacks required components: {', '.join(missing)}"
            )
        return cls(components)

    def select(self, names: list[str] | None) -> list[Component]:
        if not names:
            return self.components
        unknown = sorted(set(names) - set(self.by_name))
        if unknown:
            raise WorkspaceError(f"unknown components: {', '.join(unknown)}")
        requested = set(names)
        return [component for component in self.components if component.name in requested]


@dataclass(frozen=True)
class Paths:
    repository: Path
    workspace: Path
    repositories: Path
    worktrees: Path
    profiles: Path
    builds: Path
    state: Path
    marker: Path
    states_file: Path

    @classmethod
    def discover(cls, repository: Path) -> "Paths":
        repository = repository.resolve()
        configured = os.environ.get("ATRINIK_WORKSPACE_DIR")
        workspace = Path(configured).expanduser() if configured else repository / "workspace"
        if not workspace.is_absolute():
            raise WorkspaceError("ATRINIK_WORKSPACE_DIR must be an absolute path")
        workspace = workspace.resolve(strict=False)
        if workspace == repository:
            raise WorkspaceError(
                "workspace data directory must not replace the wrapper repository"
            )
        return cls(
            repository=repository,
            workspace=workspace,
            repositories=repository,
            worktrees=workspace / "worktrees",
            profiles=workspace / "profiles",
            builds=workspace / "build",
            state=workspace / "state",
            marker=workspace / ".atrinik-workspace.json",
            states_file=workspace / "states.json",
        )

    def ensure(self) -> None:
        if self.workspace == Path("/") or self.workspace in self.repository.parents:
            raise WorkspaceError(f"refusing unsafe workspace path: {self.workspace}")
        if self.workspace.exists() and not self.workspace.is_dir():
            raise WorkspaceError(f"workspace path is not a directory: {self.workspace}")
        if self.workspace.exists():
            if self.marker.is_file() and not self.marker.is_symlink():
                if load_json(self.marker) != {"schema_version": SCHEMA_VERSION}:
                    raise WorkspaceError(
                        f"workspace ownership marker is invalid: {self.marker}"
                    )
            elif any(self.workspace.iterdir()):
                raise WorkspaceError(
                    f"refusing unmanaged non-empty workspace directory: {self.workspace}"
                )
        self.workspace.mkdir(parents=True, exist_ok=True)
        for directory in (
            self.worktrees,
            self.profiles,
            self.builds,
            self.state,
        ):
            directory.mkdir(parents=True, exist_ok=True)
        if not self.marker.exists():
            atomic_json(self.marker, {"schema_version": SCHEMA_VERSION})


def managed_reset(path: Path, workspace_builds: Path, purpose: str) -> None:
    path = path.resolve(strict=False)
    builds = workspace_builds.resolve()
    if builds not in path.parents:
        raise WorkspaceError(f"refusing to replace path outside workspace builds: {path}")
    marker = path / MANAGED_MARKER
    if path.exists():
        if not path.is_dir() or not marker.is_file() or marker.is_symlink():
            raise WorkspaceError(f"refusing to replace unmanaged build path: {path}")
        metadata = load_json(marker)
        if metadata != {"schema_version": SCHEMA_VERSION, "purpose": purpose}:
            raise WorkspaceError(f"managed build marker does not match {purpose}: {path}")
        shutil.rmtree(path)
    path.mkdir(parents=True)
    atomic_json(marker, {"schema_version": SCHEMA_VERSION, "purpose": purpose})


def managed_directory(path: Path, workspace_builds: Path, purpose: str) -> None:
    path = path.resolve(strict=False)
    builds = workspace_builds.resolve()
    if builds != path and builds not in path.parents:
        raise WorkspaceError(f"refusing build path outside workspace builds: {path}")
    marker = path / MANAGED_MARKER
    if path.exists():
        if not path.is_dir() or not marker.is_file() or marker.is_symlink():
            raise WorkspaceError(f"refusing unmanaged build path: {path}")
        metadata = load_json(marker)
        if metadata != {"schema_version": SCHEMA_VERSION, "purpose": purpose}:
            raise WorkspaceError(f"managed build marker does not match {purpose}: {path}")
        return
    path.mkdir(parents=True)
    atomic_json(marker, {"schema_version": SCHEMA_VERSION, "purpose": purpose})


def profile_key(paths: dict[str, Path]) -> str:
    payload = json.dumps(
        {name: str(path) for name, path in paths.items()},
        sort_keys=True,
        separators=(",", ":"),
    )
    return hashlib.sha256(payload.encode()).hexdigest()[:12]
