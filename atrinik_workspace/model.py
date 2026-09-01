from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import stat
import time
from typing import Any, Callable, Iterable

from .platform_compat import (
    IS_WINDOWS,
    O_BINARY,
    O_CLOEXEC,
    assert_no_symlink_components,
    fcntl,
    flush_file,
)


SCHEMA_VERSION = 1
MANIFEST_SCHEMA_VERSION = 3
MANAGED_MARKER = ".atrinik-workspace-managed.json"
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
REPOSITORY_PATTERN = re.compile(r"^atrinik/[a-z0-9][a-z0-9._-]*$")
BUILD_KINDS = {
    "classic-client",
    "classic-server",
    "classic-protocol",
    "classic-library",
    "classic-content",
    "assets",
    "worker",
    "none",
}
V1_BUILD_KINDS = {
    "client": "classic-client",
    "server": "classic-server",
    "protocol": "classic-protocol",
    "library": "classic-library",
    "content": "classic-content",
    "assets": "assets",
    "worker": "worker",
    "none": "none",
}
GENERATIONS = {"replacement", "classic", "shared"}
STACK_GENERATIONS = {"replacement", "classic"}
COHORT_NAMES = {"default", "classic"}
STACK_NAMES = {"default", "classic"}
REQUIRED_COHORT_CHECKOUTS = {
    "default": {
        "client",
        "server",
        "protocol",
        "editor",
        "renderer",
        "content-toolkit",
        "website",
        "content",
        "sound",
        "resources",
        "metaserver-worker",
        "devcontainer",
        "github-settings",
        "observatory",
        "deploy-control",
    },
    "classic": {
        "classic",
        "playtester",
        "tools",
    },
}
REQUIRED_STACK_PROVIDERS = {
    "default": {
        "client": "client",
        "server": "server",
        "protocol": "protocol",
        "editor": "editor",
        "renderer": "renderer",
        "content-toolkit": "content-toolkit",
        "website": "website",
        "content": "content",
        "sound": "sound",
        "resources": "resources",
        "metaserver-worker": "metaserver-worker",
        "devcontainer": "devcontainer",
        "github-settings": "github-settings",
        "observatory": "observatory",
        "deploy-control": "deploy-control",
    },
    "classic": {
        "client": "classic-client",
        "server": "classic-server",
        "protocol": "classic-protocol",
        "libatrinik": "classic-libatrinik",
        "editor": "classic-editor",
        "content": "content",
        "playtester": "playtester",
        "tools": "tools",
        "sound": "sound",
        "resources": "resources",
        "metaserver-worker": "metaserver-worker",
        "devcontainer": "devcontainer",
        "github-settings": "github-settings",
    },
}
LOGICAL_ROLES = {
    "client",
    "server",
    "protocol",
    "libatrinik",
    "editor",
    "renderer",
    "content-toolkit",
    "website",
    "content",
    "playtester",
    "tools",
    "sound",
    "resources",
    "metaserver-worker",
    "devcontainer",
    "github-settings",
    "observatory",
    "deploy-control",
}
IMPLEMENTATION_ROLES = {
    "client",
    "server",
    "protocol",
    "libatrinik",
    "editor",
    "renderer",
    "content-toolkit",
    "website",
    "content",
    "tools",
}
GENERATION_BUILD_KINDS = {
    "replacement": {"none"},
    "classic": {
        "classic-client",
        "classic-server",
        "classic-protocol",
        "classic-library",
        "classic-content",
        "none",
    },
    "shared": {"assets", "worker", "none"},
}
CLASSIC_BUILD_ROLES = {
    "classic-client": "client",
    "classic-server": "server",
    "classic-protocol": "protocol",
    "classic-library": "libatrinik",
    "classic-content": "content",
}


class WorkspaceError(RuntimeError):
    """A workspace operation cannot be completed safely."""


class AtomicJsonCommitUncertain(WorkspaceError):
    """The JSON replacement is visible but directory durability was not proven."""


class JsonUnlinkCommitUncertain(WorkspaceError):
    """The JSON record is absent but directory durability was not proven."""


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
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise WorkspaceError(f"cannot read {path}: {error}") from error


def unlink_validated_json(path: Path, validate: Callable[[Any], None]) -> None:
    """Validate and unlink the same no-follow JSON inode through a parent dirfd."""

    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        raise WorkspaceError(
            "validated JSON removal is unavailable on native Windows: "
            "the wrapper cannot prove durable parent-directory unlink semantics"
        )

    directory_flags = os.O_RDONLY | O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    directory_flags |= getattr(os, "O_NOFOLLOW", 0)
    file_flags = os.O_RDONLY | O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
    directory: int | None = None
    descriptor: int | None = None
    try:
        directory = _open_directory_nofollow(path.parent, directory_flags)
        parent_opened = os.fstat(directory)
        parent_visible = path.parent.stat(follow_symlinks=False)
        if (
            not stat.S_ISDIR(parent_opened.st_mode)
            or (parent_opened.st_dev, parent_opened.st_ino)
            != (parent_visible.st_dev, parent_visible.st_ino)
        ):
            raise WorkspaceError(f"JSON parent directory was replaced: {path.parent}")
        descriptor = os.open(path.name, file_flags, dir_fd=directory)
        opened = os.fstat(descriptor)
        visible = os.stat(path.name, dir_fd=directory, follow_symlinks=False)
        if (
            not stat.S_ISREG(opened.st_mode)
            or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
        ):
            raise WorkspaceError(f"JSON record identity is unsafe: {path}")
        with os.fdopen(descriptor, encoding="utf-8", closefd=False) as stream:
            value = json.load(stream, object_pairs_hook=_reject_duplicate_keys)
        validate(value)
        parent_visible = path.parent.stat(follow_symlinks=False)
        if (parent_opened.st_dev, parent_opened.st_ino) != (
            parent_visible.st_dev,
            parent_visible.st_ino,
        ):
            raise WorkspaceError(f"JSON parent directory was replaced: {path.parent}")
        visible = os.stat(path.name, dir_fd=directory, follow_symlinks=False)
        if (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino):
            raise WorkspaceError(f"JSON record was replaced: {path}")
        os.unlink(path.name, dir_fd=directory)
        try:
            os.fsync(directory)
        except OSError as error:
            raise JsonUnlinkCommitUncertain(
                "JSON record is absent but its directory durability is uncertain: "
                f"{path}: {error}"
            ) from error
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise WorkspaceError(f"cannot consume {path}: {error}") from error
    finally:
        if descriptor is not None:
            os.close(descriptor)
        if directory is not None:
            os.close(directory)


def atomic_json(path: Path, value: Any) -> None:
    _atomic_json(path, value, durable=False)


def durable_atomic_json(path: Path, value: Any) -> None:
    """Replace JSON and prove the containing directory persisted the rename."""

    _atomic_json(path, value, durable=True)


def _atomic_json(path: Path, value: Any, *, durable: bool) -> None:
    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        _atomic_json_windows(path, value, durable=durable)
        return

    directory_flags = os.O_RDONLY | O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    directory_flags |= getattr(os, "O_NOFOLLOW", 0)
    directory = _open_directory_nofollow(
        path.parent, directory_flags, create=True
    )
    temporary = f".{path.name}.{secrets.token_hex(12)}.tmp"
    descriptor: int | None = None
    try:
        opened_parent = os.fstat(directory)
        visible_parent = path.parent.stat(follow_symlinks=False)
        if (
            not stat.S_ISDIR(opened_parent.st_mode)
            or (opened_parent.st_dev, opened_parent.st_ino)
            != (visible_parent.st_dev, visible_parent.st_ino)
        ):
            raise WorkspaceError(f"JSON parent directory was replaced: {path.parent}")
        descriptor = os.open(
            temporary,
            os.O_WRONLY
            | os.O_CREAT
            | os.O_EXCL
            | O_CLOEXEC
            | getattr(os, "O_NOFOLLOW", 0),
            0o600,
            dir_fd=directory,
        )
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            descriptor = None
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        visible_parent = path.parent.stat(follow_symlinks=False)
        if (opened_parent.st_dev, opened_parent.st_ino) != (
            visible_parent.st_dev,
            visible_parent.st_ino,
        ):
            raise WorkspaceError(f"JSON parent directory was replaced: {path.parent}")
        os.rename(
            temporary,
            path.name,
            src_dir_fd=directory,
            dst_dir_fd=directory,
        )
        if durable:
            try:
                os.fsync(directory)
            except OSError as error:
                raise AtomicJsonCommitUncertain(
                    "JSON replacement is visible but its directory durability is "
                    f"uncertain: {path}: {error}"
                ) from error
    except BaseException:
        try:
            os.unlink(temporary, dir_fd=directory)
        except FileNotFoundError:
            pass
        raise
    finally:
        if descriptor is not None:
            os.close(descriptor)
        os.close(directory)


def _atomic_json_windows(  # pragma: no cover - exercised by native Windows CI
    path: Path, value: Any, *, durable: bool
) -> None:
    """Atomically publish JSON using Windows' replace and flush primitives.

    Windows has no portable descriptor-relative directory API equivalent to
    the POSIX implementation above.  Existing path components are therefore
    checked for links/junctions before the uniquely named temporary file is
    written, and the file contents are flushed before and after replacement.
    Operations that require durable parent-directory unlink proof remain
    explicitly unavailable instead of using an unsafe fallback.
    """

    try:
        assert_no_symlink_components(path.parent, "JSON")
        path.parent.mkdir(parents=True, exist_ok=True)
        assert_no_symlink_components(path, "JSON")
        opened_parent = path.parent.stat(follow_symlinks=False)
        if not stat.S_ISDIR(opened_parent.st_mode):
            raise OSError(f"JSON parent is not a directory: {path.parent}")
    except OSError as error:
        raise WorkspaceError(str(error)) from error
    temporary = path.with_name(f".{path.name}.{secrets.token_hex(12)}.tmp")
    descriptor: int | None = None
    replaced = False
    try:
        descriptor = os.open(
            temporary,
            os.O_WRONLY | os.O_CREAT | os.O_EXCL | O_BINARY | O_CLOEXEC,
            0o600,
        )
        with os.fdopen(
            descriptor, "w", encoding="utf-8", newline="\n"
        ) as stream:
            descriptor = None
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            flush_file(stream.fileno())
        assert_no_symlink_components(path, "JSON")
        visible_parent = path.parent.stat(follow_symlinks=False)
        if (opened_parent.st_dev, opened_parent.st_ino) != (
            visible_parent.st_dev,
            visible_parent.st_ino,
        ):
            raise WorkspaceError(f"JSON parent directory was replaced: {path.parent}")
        os.replace(temporary, path)
        replaced = True
        assert_no_symlink_components(path, "JSON")
        if durable:
            with path.open("r+b") as stream:
                opened = os.fstat(stream.fileno())
                visible = path.stat(follow_symlinks=False)
                if (
                    not stat.S_ISREG(opened.st_mode)
                    or not stat.S_ISREG(visible.st_mode)
                    or (opened.st_dev, opened.st_ino)
                    != (visible.st_dev, visible.st_ino)
                ):
                    raise WorkspaceError(
                        f"JSON file identity changed after replacement: {path}"
                    )
                flush_file(stream.fileno())
    except BaseException as error:
        if replaced:
            raise AtomicJsonCommitUncertain(
                "JSON replacement is visible but its Windows post-replacement "
                f"file verification is uncertain: {path}: {error}"
            ) from error
        raise
    finally:
        if descriptor is not None:
            os.close(descriptor)
        temporary.unlink(missing_ok=True)


def _open_directory_nofollow(
    path: Path, flags: int, *, create: bool = False
) -> int:
    """Open every component of a directory path without following symlinks."""

    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        raise WorkspaceError(
            "descriptor-relative directory operations are unavailable on native Windows"
        )

    absolute = Path(os.path.abspath(path))
    descriptor = os.open("/", flags)
    try:
        for part in absolute.parts[1:]:
            try:
                next_descriptor = os.open(part, flags, dir_fd=descriptor)
            except FileNotFoundError:
                if not create:
                    raise
                try:
                    os.mkdir(part, 0o777, dir_fd=descriptor)
                except FileExistsError:
                    # Another publisher may have created the same safe
                    # directory after our failed open. Reopen it no-follow
                    # below and prove its parent directory entry durable.
                    pass
                try:
                    os.fsync(descriptor)
                except OSError as error:
                    raise AtomicJsonCommitUncertain(
                        "JSON ancestor directory is visible but its parent "
                        f"durability is uncertain: {absolute}: {error}"
                    ) from error
                next_descriptor = os.open(part, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = next_descriptor
        return descriptor
    except BaseException:
        os.close(descriptor)
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
class Checkout:
    name: str
    repository: str
    branch: str
    path: str
    generation: str
    license: str


@dataclass(frozen=True)
class Component:
    name: str
    checkout_name: str
    source: str
    source_includes: tuple[str, ...]
    repository: str
    branch: str
    checkout: str
    build: str
    build_by_stack: tuple[tuple[str, str], ...]
    generation: str
    provides: tuple[str, ...]
    requires: tuple[str, ...]
    license: str


@dataclass(frozen=True)
class Stack:
    name: str
    generation: str
    components: tuple[Component, ...]
    providers: dict[str, Component]


@dataclass(frozen=True)
class _StackSpec:
    name: str
    generation: str
    components: tuple[str, ...]
    providers: dict[str, str]


class Manifest:
    def __init__(
        self,
        checkouts: list[Checkout],
        components: list[Component],
        cohorts: dict[str, tuple[str, ...]],
        stack_specs: dict[str, _StackSpec],
    ):
        self.checkouts = checkouts
        self.by_checkout = {checkout.name: checkout for checkout in checkouts}
        self.components = components
        self.by_name = {component.name: component for component in components}
        self.cohorts = cohorts
        self.stacks = {
            name: Stack(
                name=spec.name,
                generation=spec.generation,
                components=tuple(self.by_name[component] for component in spec.components),
                providers={
                    role: self.by_name[component]
                    for role, component in spec.providers.items()
                },
            )
            for name, spec in stack_specs.items()
        }
        self._component_cohorts = {
            component.name: self.checkout_cohorts(component.checkout_name)
            for component in components
        }
        self._component_stacks = {
            component.name: tuple(
                name
                for name, stack in stack_specs.items()
                if component.name in stack.components
            )
            for component in components
        }
        self._checkout_stacks = {
            checkout.name: tuple(
                name
                for name, spec in stack_specs.items()
                if any(
                    self.by_name[component].checkout_name == checkout.name
                    for component in spec.components
                )
            )
            for checkout in checkouts
        }

    @classmethod
    def load(cls, path: Path) -> "Manifest":
        return cls.from_value(load_json(path))

    @classmethod
    def from_value(cls, root: Any) -> "Manifest":
        """Load a manifest from an already bounded and decoded JSON value."""

        if not isinstance(root, dict):
            raise WorkspaceError("component manifest root must be an object")
        schema_version = root.get("schema_version")
        if type(schema_version) is not int:
            raise WorkspaceError("unsupported component manifest schema")
        if schema_version == 1:
            return cls._load_v1(root)
        if schema_version != MANIFEST_SCHEMA_VERSION:
            raise WorkspaceError("unsupported component manifest schema")
        return cls._load_v3(root)

    @classmethod
    def _load_v1(cls, root: dict[str, Any]) -> "Manifest":
        require_keys(root, {"schema_version", "components"}, "component manifest")
        raw_components = root["components"]
        if not isinstance(raw_components, list) or not raw_components:
            raise WorkspaceError("components must be a non-empty array")
        checkouts: list[Checkout] = []
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
            if not isinstance(build, str) or build not in V1_BUILD_KINDS:
                raise WorkspaceError(f"{context}.build is invalid")
            if name in names or repository in repositories:
                raise WorkspaceError(f"duplicate component identity: {name}/{repository}")
            names.add(name)
            repositories.add(repository)
            components.append(
                Component(
                    name=name,
                    checkout_name=name,
                    source=".",
                    source_includes=(),
                    repository=repository,
                    branch=branch,
                    checkout=name,
                    build=V1_BUILD_KINDS[build],
                    build_by_stack=(),
                    generation="shared",
                    provides=(name,),
                    requires=(),
                    license="NOASSERTION",
                )
            )
            checkouts.append(
                Checkout(
                    name=name,
                    repository=repository,
                    branch=branch,
                    path=name,
                    generation="shared",
                    license="NOASSERTION",
                )
            )
        required = {"client", "server", "protocol", "libatrinik", "content", "sound", "resources"}
        missing = sorted(required - names)
        if missing:
            raise WorkspaceError(
                f"component manifest lacks required components: {', '.join(missing)}"
            )
        members = tuple(component.name for component in components)
        providers = {component.name: component.name for component in components}
        return cls(
            checkouts,
            components,
            {"default": members, "classic": ()},
            {
                "default": _StackSpec("default", "replacement", members, providers),
                "classic": _StackSpec("classic", "classic", (), {}),
            },
        )

    @classmethod
    def _load_v3(cls, root: dict[str, Any]) -> "Manifest":
        require_keys(
            root,
            {"schema_version", "cohorts", "stacks", "checkouts", "components"},
            "component manifest",
        )
        checkouts = cls._load_v3_checkouts(root["checkouts"])
        by_checkout = {checkout.name: checkout for checkout in checkouts}
        components = cls._load_v3_components(root["components"], by_checkout)
        by_name = {component.name: component for component in components}
        cohorts = cls._load_cohorts(root["cohorts"], by_checkout)
        stacks = cls._load_stacks(root["stacks"], by_name)
        cls._validate_stack_contracts(stacks, by_name, by_checkout, cohorts)
        manifest = cls(checkouts, components, cohorts, stacks)
        manifest._validate_repository_variant_membership()
        manifest._validate_required_contract()
        return manifest

    def _validate_repository_variant_membership(self) -> None:
        by_repository: dict[str, list[Checkout]] = {}
        for checkout in self.checkouts:
            by_repository.setdefault(checkout.repository, []).append(checkout)
        for repository, variants in by_repository.items():
            for index, first in enumerate(variants):
                for second in variants[index + 1 :]:
                    if self.checkout_cohorts(first.name) == self.checkout_cohorts(
                        second.name
                    ) or set(self.checkout_stacks(first.name)) & set(
                        self.checkout_stacks(second.name)
                    ):
                        raise WorkspaceError(
                            "repository variants must have distinct cohort and stack "
                            f"membership: {repository}"
                        )

    def _validate_required_contract(self) -> None:
        for cohort_name, required in REQUIRED_COHORT_CHECKOUTS.items():
            missing = sorted(required - set(self.cohorts[cohort_name]))
            if missing:
                raise WorkspaceError(
                    f"cohorts.{cohort_name} lacks required checkouts: "
                    f"{', '.join(missing)}"
                )
        for stack_name, required in REQUIRED_STACK_PROVIDERS.items():
            providers = {
                role: component.name
                for role, component in self.stacks[stack_name].providers.items()
            }
            missing = sorted(set(required) - set(providers))
            wrong = sorted(
                role
                for role in set(required) & set(providers)
                if providers[role] != required[role]
            )
            if missing or wrong:
                detail: list[str] = []
                if missing:
                    detail.append(f"missing roles {', '.join(missing)}")
                if wrong:
                    detail.append(f"wrong providers for {', '.join(wrong)}")
                raise WorkspaceError(
                    f"stacks.{stack_name} lacks required provider contract: "
                    + "; ".join(detail)
                )

    @staticmethod
    def _load_v3_checkouts(raw_checkouts: Any) -> list[Checkout]:
        if not isinstance(raw_checkouts, list) or not raw_checkouts:
            raise WorkspaceError("checkouts must be a non-empty array")
        expected = {
            "name",
            "repository",
            "branch",
            "path",
            "generation",
            "license",
        }
        checkouts: list[Checkout] = []
        names: set[str] = set()
        paths: set[str] = set()
        coordinates: set[tuple[str, str]] = set()
        for index, raw in enumerate(raw_checkouts):
            context = f"checkout {index}"
            if not isinstance(raw, dict):
                raise WorkspaceError(f"{context} must be an object")
            require_keys(raw, expected, context)
            name = validate_name(raw["name"], f"{context}.name")
            repository = raw["repository"]
            branch = raw["branch"]
            path = validate_name(raw["path"], f"{context}.path")
            generation = raw["generation"]
            license_name = _validate_license(raw["license"], f"{context}.license")
            if not isinstance(repository, str) or not REPOSITORY_PATTERN.fullmatch(repository):
                raise WorkspaceError(
                    f"{context}.repository must name an atrinik GitHub repository"
                )
            _validate_branch(branch, f"{context}.branch")
            if not isinstance(generation, str) or generation not in GENERATIONS:
                raise WorkspaceError(f"{context}.generation is invalid")
            if name in names:
                raise WorkspaceError(f"duplicate checkout name: {name}")
            if path in paths:
                raise WorkspaceError(f"duplicate checkout path: {path}")
            coordinate = (repository, branch)
            if coordinate in coordinates:
                raise WorkspaceError(
                    f"duplicate checkout repository and branch: {repository}@{branch}"
                )
            names.add(name)
            paths.add(path)
            coordinates.add(coordinate)
            checkouts.append(
                Checkout(name, repository, branch, path, generation, license_name)
            )
        return checkouts

    @staticmethod
    def _load_v3_components(
        raw_components: Any, by_checkout: dict[str, Checkout]
    ) -> list[Component]:
        if not isinstance(raw_components, list) or not raw_components:
            raise WorkspaceError("components must be a non-empty array")
        components: list[Component] = []
        names: set[str] = set()
        sources: dict[str, list[tuple[str, PurePosixPath]]] = {}
        source_includes_by_checkout: dict[
            str, list[tuple[str, PurePosixPath]]
        ] = {}
        required = {
            "name",
            "checkout",
            "source",
            "build",
            "generation",
            "provides",
            "requires",
            "license",
        }
        allowed = required | {"build_by_stack", "source_includes"}
        for index, raw in enumerate(raw_components):
            context = f"component {index}"
            if not isinstance(raw, dict):
                raise WorkspaceError(f"{context} must be an object")
            actual_keys = set(raw)
            if not required <= actual_keys or not actual_keys <= allowed:
                missing = sorted(required - actual_keys)
                extra = sorted(actual_keys - allowed)
                detail: list[str] = []
                if missing:
                    detail.append(f"missing {', '.join(missing)}")
                if extra:
                    detail.append(f"unexpected {', '.join(extra)}")
                raise WorkspaceError(f"{context}: {'; '.join(detail)}")
            name = validate_name(raw["name"], f"{context}.name")
            checkout_name = validate_name(raw["checkout"], f"{context}.checkout")
            if checkout_name not in by_checkout:
                raise WorkspaceError(f"{context}.checkout names an unknown checkout")
            checkout = by_checkout[checkout_name]
            source = _validate_source(raw["source"], f"{context}.source")
            raw_source_includes = raw.get("source_includes", [])
            if not isinstance(raw_source_includes, list):
                raise WorkspaceError(f"{context}.source_includes must be an array")
            source_includes = tuple(
                _validate_source(value, f"{context}.source_includes {include_index}")
                for include_index, value in enumerate(raw_source_includes)
            )
            if len(set(source_includes)) != len(source_includes):
                raise WorkspaceError(
                    f"{context}.source_includes contains duplicate directories"
                )
            reserved_generation_paths = {
                PurePosixPath("source"),
                PurePosixPath(MANAGED_MARKER),
                PurePosixPath(".atrinik-source-generation.json"),
            }
            if any(
                include == reserved or reserved in include.parents
                for include in map(PurePosixPath, source_includes)
                for reserved in reserved_generation_paths
            ):
                raise WorkspaceError(
                    f"{context}.source_includes overlaps a reserved generation path"
                )
            closure_paths = [PurePosixPath(source), *map(PurePosixPath, source_includes)]
            for path_index, path in enumerate(closure_paths):
                for other in closure_paths[:path_index]:
                    if path == other or path in other.parents or other in path.parents:
                        raise WorkspaceError(
                            f"{context} source closure paths overlap: {other}, {path}"
                        )
            build = raw["build"]
            raw_build_by_stack = raw.get("build_by_stack", {})
            generation = raw["generation"]
            license_name = _validate_license(raw["license"], f"{context}.license")
            if not isinstance(build, str) or build not in BUILD_KINDS:
                raise WorkspaceError(f"{context}.build is invalid")
            if not isinstance(raw_build_by_stack, dict):
                raise WorkspaceError(f"{context}.build_by_stack must be an object")
            build_by_stack: dict[str, str] = {}
            for raw_stack, raw_adapter in raw_build_by_stack.items():
                stack_name = validate_name(
                    raw_stack, f"{context}.build_by_stack stack"
                )
                if stack_name not in STACK_NAMES:
                    raise WorkspaceError(
                        f"{context}.build_by_stack names unknown stack {stack_name}"
                    )
                if not isinstance(raw_adapter, str) or raw_adapter not in BUILD_KINDS:
                    raise WorkspaceError(
                        f"{context}.build_by_stack.{stack_name} is invalid"
                    )
                if raw_adapter == build:
                    raise WorkspaceError(
                        f"{context}.build_by_stack.{stack_name} redundantly repeats build"
                    )
                build_by_stack[stack_name] = raw_adapter
            if not isinstance(generation, str) or generation not in GENERATIONS:
                raise WorkspaceError(f"{context}.generation is invalid")
            if generation != checkout.generation:
                raise WorkspaceError(
                    f"{context}.generation does not match checkout {checkout.name}"
                )
            if build not in GENERATION_BUILD_KINDS[generation]:
                raise WorkspaceError(
                    f"{context}.build {build} is incompatible with {generation} generation"
                )
            provides = _validate_roles(raw["provides"], f"{context}.provides", nonempty=True)
            requires = _validate_roles(raw["requires"], f"{context}.requires")
            unknown_roles = sorted((set(provides) | set(requires)) - LOGICAL_ROLES)
            if unknown_roles:
                raise WorkspaceError(
                    f"{context} names unknown logical roles: "
                    f"{', '.join(unknown_roles)}"
                )
            claimed_implementations = IMPLEMENTATION_ROLES & set(provides)
            if len(claimed_implementations) > 1:
                raise WorkspaceError(
                    f"{context} claims incompatible implementation roles: "
                    f"{', '.join(sorted(claimed_implementations))}"
                )
            shared_content = (
                generation == "shared"
                and name == "content"
                and checkout_name == "content"
                and claimed_implementations == {"content"}
            )
            if generation == "shared" and claimed_implementations and not shared_content:
                raise WorkspaceError(
                    f"{context} shared component claims implementation role: "
                    f"{', '.join(sorted(claimed_implementations))}"
                )
            adapter_role = CLASSIC_BUILD_ROLES.get(build)
            if adapter_role is not None and adapter_role not in provides:
                raise WorkspaceError(
                    f"{context}.build {build} requires provided role {adapter_role}"
                )
            if name in names:
                raise WorkspaceError(f"duplicate component name: {name}")
            source_path = PurePosixPath(source)
            for other_name, other_include in source_includes_by_checkout.get(
                checkout_name, []
            ):
                if (
                    source_path == other_include
                    or source_path in other_include.parents
                    or other_include in source_path.parents
                ):
                    raise WorkspaceError(
                        f"component source overlaps declared source include in checkout "
                        f"{checkout_name}: {other_name}, {name}"
                    )
            for other_name, other_source in sources.get(checkout_name, []):
                if (
                    source_path == other_source
                    or source_path in other_source.parents
                    or other_source in source_path.parents
                ):
                    raise WorkspaceError(
                        f"component sources overlap in checkout {checkout_name}: "
                        f"{other_name}, {name}"
                    )
            for include in map(PurePosixPath, source_includes):
                for other_name, other_source in sources.get(checkout_name, []):
                    if (
                        include == other_source
                        or include in other_source.parents
                        or other_source in include.parents
                    ):
                        raise WorkspaceError(
                            f"component source include overlaps a component source in "
                            f"checkout {checkout_name}: {name}, {other_name}"
                        )
            names.add(name)
            sources.setdefault(checkout_name, []).append((name, source_path))
            source_includes_by_checkout.setdefault(checkout_name, []).extend(
                (name, PurePosixPath(include)) for include in source_includes
            )
            components.append(
                Component(
                    name=name,
                    checkout_name=checkout_name,
                    source=source,
                    source_includes=source_includes,
                    repository=checkout.repository,
                    branch=checkout.branch,
                    checkout=checkout.path,
                    build=build,
                    build_by_stack=tuple(sorted(build_by_stack.items())),
                    generation=generation,
                    provides=provides,
                    requires=requires,
                    license=license_name,
                )
            )
        return components

    @staticmethod
    def _load_cohorts(
        raw_cohorts: Any, by_checkout: dict[str, Checkout]
    ) -> dict[str, tuple[str, ...]]:
        if not isinstance(raw_cohorts, dict):
            raise WorkspaceError("cohorts must be an object")
        require_keys(raw_cohorts, COHORT_NAMES, "cohorts")
        cohorts: dict[str, tuple[str, ...]] = {}
        memberships: dict[str, str] = {}
        for cohort_name in ("default", "classic"):
            members = _validate_checkout_names(
                raw_cohorts[cohort_name], f"cohorts.{cohort_name}", by_checkout
            )
            for checkout_name in members:
                if checkout_name in memberships:
                    raise WorkspaceError(
                        f"checkout {checkout_name} belongs to multiple cohorts"
                    )
                memberships[checkout_name] = cohort_name
                generation = by_checkout[checkout_name].generation
                if cohort_name == "default" and generation == "classic":
                    raise WorkspaceError(
                        f"classic checkout {checkout_name} cannot belong to default cohort"
                    )
                if cohort_name == "classic" and generation != "classic":
                    raise WorkspaceError(
                        f"classic cohort checkout {checkout_name} must be classic"
                    )
            cohorts[cohort_name] = members
        missing = sorted(set(by_checkout) - set(memberships))
        if missing:
            raise WorkspaceError(
                f"checkouts lack cohort membership: {', '.join(missing)}"
            )
        return cohorts

    @staticmethod
    def _load_stacks(
        raw_stacks: Any, by_name: dict[str, Component]
    ) -> dict[str, _StackSpec]:
        if not isinstance(raw_stacks, dict):
            raise WorkspaceError("stacks must be an object")
        require_keys(raw_stacks, STACK_NAMES, "stacks")
        stacks: dict[str, _StackSpec] = {}
        for stack_name in ("default", "classic"):
            raw = raw_stacks[stack_name]
            context = f"stacks.{stack_name}"
            if not isinstance(raw, dict):
                raise WorkspaceError(f"{context} must be an object")
            require_keys(raw, {"generation", "components", "providers"}, context)
            generation = raw["generation"]
            if not isinstance(generation, str) or generation not in STACK_GENERATIONS:
                raise WorkspaceError(f"{context}.generation is invalid")
            expected_generation = "replacement" if stack_name == "default" else "classic"
            if generation != expected_generation:
                raise WorkspaceError(
                    f"{context}.generation must be {expected_generation}"
                )
            members = _validate_component_names(
                raw["components"], f"{context}.components", by_name
            )
            raw_providers = raw["providers"]
            if not isinstance(raw_providers, dict):
                raise WorkspaceError(f"{context}.providers must be an object")
            providers: dict[str, str] = {}
            for raw_role, component_name in raw_providers.items():
                role = validate_name(raw_role, f"{context}.providers role")
                if not isinstance(component_name, str) or component_name not in by_name:
                    raise WorkspaceError(
                        f"{context}.providers.{role} names an unknown component"
                    )
                if component_name not in members:
                    raise WorkspaceError(
                        f"{context}.providers.{role} is outside the stack"
                    )
                providers[role] = component_name
            stacks[stack_name] = _StackSpec(
                stack_name, generation, members, providers
            )
        return stacks

    @staticmethod
    def _validate_stack_contracts(
        stacks: dict[str, _StackSpec],
        by_name: dict[str, Component],
        by_checkout: dict[str, Checkout],
        cohorts: dict[str, tuple[str, ...]],
    ) -> None:
        checkout_cohort = {
            checkout_name: cohort_name
            for cohort_name, checkout_names in cohorts.items()
            for checkout_name in checkout_names
        }
        for stack_name, stack in stacks.items():
            context = f"stacks.{stack_name}"
            actual_providers: dict[str, str] = {}
            for component_name in stack.components:
                component = by_name[component_name]
                build_by_stack = dict(component.build_by_stack)
                effective_build = build_by_stack.get(stack_name, component.build)
                unused_overrides = sorted(set(build_by_stack) - {
                    candidate
                    for candidate, candidate_stack in stacks.items()
                    if component_name in candidate_stack.components
                })
                if unused_overrides:
                    raise WorkspaceError(
                        f"component {component_name}.build_by_stack targets stacks where "
                        f"the component is absent: {', '.join(unused_overrides)}"
                    )
                effective_generation = (
                    "replacement" if stack_name == "default" else "classic"
                )
                if (
                    stack_name in build_by_stack
                    and effective_build
                    not in GENERATION_BUILD_KINDS[effective_generation]
                ):
                    raise WorkspaceError(
                        f"component {component_name} effective build {effective_build} "
                        f"is incompatible with {stack_name} stack"
                    )
                adapter_role = CLASSIC_BUILD_ROLES.get(effective_build)
                if adapter_role is not None and adapter_role not in component.provides:
                    raise WorkspaceError(
                        f"component {component_name} effective build {effective_build} "
                        f"requires provided role {adapter_role}"
                    )
                if component.generation not in {stack.generation, "shared"}:
                    raise WorkspaceError(
                        f"{context} mixes {component.generation} component {component_name} "
                        f"into the {stack.generation} stack"
                    )
                allowed_cohorts = {"default"}
                if stack_name == "classic":
                    allowed_cohorts.add("classic")
                if checkout_cohort[component.checkout_name] not in allowed_cohorts:
                    raise WorkspaceError(
                        f"{context} uses checkout {component.checkout_name} outside "
                        "its initialization closure"
                    )
                for role in component.provides:
                    if role in actual_providers:
                        raise WorkspaceError(
                            f"{context} has multiple providers for role {role}: "
                            f"{actual_providers[role]}, {component_name}"
                        )
                    actual_providers[role] = component_name
            if stack.providers != actual_providers:
                missing = sorted(set(actual_providers) - set(stack.providers))
                extra = sorted(set(stack.providers) - set(actual_providers))
                wrong = sorted(
                    role
                    for role in set(stack.providers) & set(actual_providers)
                    if stack.providers[role] != actual_providers[role]
                )
                detail = []
                if missing:
                    detail.append(f"missing roles {', '.join(missing)}")
                if extra:
                    detail.append(f"unexpected roles {', '.join(extra)}")
                if wrong:
                    detail.append(f"incorrect providers for {', '.join(wrong)}")
                raise WorkspaceError(
                    f"{context}.providers does not match component contracts: "
                    + "; ".join(detail)
                )
            dependencies: dict[str, set[str]] = {
                component_name: set() for component_name in stack.components
            }
            for component_name in stack.components:
                component = by_name[component_name]
                for role in component.requires:
                    provider_name = stack.providers.get(role)
                    if provider_name is None:
                        raise WorkspaceError(
                            f"{context} cannot satisfy required role {role} for {component_name}"
                        )
                    dependencies[component_name].add(provider_name)
            _validate_dependency_cycles(dependencies, context)

    def cohort(self, name: str) -> list[Checkout]:
        if name not in self.cohorts:
            raise WorkspaceError(f"unknown cohort: {name}")
        return [self.by_checkout[checkout] for checkout in self.cohorts[name]]

    def stack(self, name: str) -> Stack:
        if name not in self.stacks:
            raise WorkspaceError(f"unknown stack: {name}")
        return self.stacks[name]

    def provider(self, stack: str, role: str) -> Component:
        selected = self.stack(stack)
        if role not in selected.providers:
            raise WorkspaceError(f"stack {stack} has no provider for role {role}")
        return selected.providers[role]

    def effective_build(self, stack: str, component: Component | str) -> str:
        """Return the one validated build adapter for a component in a stack."""

        selected = self.stack(stack)
        value = self.by_name[component] if isinstance(component, str) else component
        if value.name not in {candidate.name for candidate in selected.components}:
            raise WorkspaceError(
                f"component {value.name} is not part of {selected.name} stack"
            )
        return dict(value.build_by_stack).get(selected.name, value.build)

    def component_cohorts(self, name: str) -> tuple[str, ...]:
        if name not in self.by_name:
            raise WorkspaceError(f"unknown component: {name}")
        return self._component_cohorts[name]

    def checkout_cohorts(self, name: str) -> tuple[str, ...]:
        if name not in self.by_checkout:
            raise WorkspaceError(f"unknown checkout: {name}")
        return tuple(
            cohort_name
            for cohort_name, members in self.cohorts.items()
            if name in members
        )

    def checkout_stacks(self, name: str) -> tuple[str, ...]:
        if name not in self.by_checkout:
            raise WorkspaceError(f"unknown checkout: {name}")
        return self._checkout_stacks[name]

    def checkout_for(self, component: Component | str) -> Checkout:
        selected = self.by_name[component] if isinstance(component, str) else component
        return self.by_checkout[selected.checkout_name]

    def component_stacks(self, name: str) -> tuple[str, ...]:
        if name not in self.by_name:
            raise WorkspaceError(f"unknown component: {name}")
        return self._component_stacks[name]

    def select(self, names: list[str] | None) -> list[Component]:
        if not names:
            return self.components
        unknown = sorted(set(names) - set(self.by_name))
        if unknown:
            raise WorkspaceError(f"unknown components: {', '.join(unknown)}")
        requested = set(names)
        return [component for component in self.components if component.name in requested]

    def select_checkouts(self, names: list[str] | None) -> list[Checkout]:
        if not names:
            return self.checkouts
        unknown = sorted(set(names) - set(self.by_checkout))
        if unknown:
            raise WorkspaceError(f"unknown checkouts: {', '.join(unknown)}")
        requested = set(names)
        return [checkout for checkout in self.checkouts if checkout.name in requested]


def _validate_roles(value: Any, context: str, nonempty: bool = False) -> tuple[str, ...]:
    if not isinstance(value, list) or (nonempty and not value):
        qualifier = "non-empty " if nonempty else ""
        raise WorkspaceError(f"{context} must be a {qualifier}array")
    roles: list[str] = []
    seen: set[str] = set()
    for index, raw_role in enumerate(value):
        role = validate_name(raw_role, f"{context}[{index}]")
        if role in seen:
            raise WorkspaceError(f"{context} contains duplicate role {role}")
        seen.add(role)
        roles.append(role)
    return tuple(roles)


def _validate_branch(value: Any, context: str) -> str:
    if (
        not isinstance(value, str)
        or not value
        or value.startswith("-")
        or any(character.isspace() or ord(character) < 32 for character in value)
        or ".." in value
        or "@{" in value
        or value.endswith(("/", ".", ".lock"))
        or any(character in value for character in "~^:?*[\\")
    ):
        raise WorkspaceError(f"{context} is invalid")
    return value


def _validate_license(value: Any, context: str) -> str:
    if (
        not isinstance(value, str)
        or not value.strip()
        or value != value.strip()
        or any(ord(character) < 32 for character in value)
    ):
        raise WorkspaceError(f"{context} is invalid")
    return value


def _validate_source(value: Any, context: str) -> str:
    if not isinstance(value, str) or not value:
        raise WorkspaceError(f"{context} must be a safe checkout-relative directory")
    path = PurePosixPath(value)
    if (
        path.is_absolute()
        or value != path.as_posix()
        or any(part in {"", ".."} for part in path.parts)
        or (value != "." and "." in path.parts)
    ):
        raise WorkspaceError(f"{context} must be a safe checkout-relative directory")
    return value


def _validate_component_names(
    value: Any, context: str, by_name: dict[str, Component]
) -> tuple[str, ...]:
    if not isinstance(value, list):
        raise WorkspaceError(f"{context} must be an array")
    names: list[str] = []
    seen: set[str] = set()
    for index, raw_name in enumerate(value):
        name = validate_name(raw_name, f"{context}[{index}]")
        if name not in by_name:
            raise WorkspaceError(f"{context} names unknown component {name}")
        if name in seen:
            raise WorkspaceError(f"{context} contains duplicate component {name}")
        seen.add(name)
        names.append(name)
    return tuple(names)


def _validate_checkout_names(
    value: Any, context: str, by_name: dict[str, Checkout]
) -> tuple[str, ...]:
    if not isinstance(value, list):
        raise WorkspaceError(f"{context} must be an array")
    names: list[str] = []
    seen: set[str] = set()
    for index, raw_name in enumerate(value):
        name = validate_name(raw_name, f"{context}[{index}]")
        if name not in by_name:
            raise WorkspaceError(f"{context} names unknown checkout {name}")
        if name in seen:
            raise WorkspaceError(f"{context} contains duplicate checkout {name}")
        seen.add(name)
        names.append(name)
    return tuple(names)


def _validate_dependency_cycles(
    dependencies: dict[str, set[str]], context: str
) -> None:
    visiting: list[str] = []
    visited: set[str] = set()

    def visit(component_name: str) -> None:
        if component_name in visiting:
            start = visiting.index(component_name)
            cycle = visiting[start:] + [component_name]
            raise WorkspaceError(
                f"{context} contains dependency cycle: {' -> '.join(cycle)}"
            )
        if component_name in visited:
            return
        visiting.append(component_name)
        for dependency in sorted(dependencies[component_name]):
            visit(dependency)
        visiting.pop()
        visited.add(component_name)

    for component_name in dependencies:
        visit(component_name)


@dataclass(frozen=True)
class Paths:
    repository: Path
    workspace: Path
    repositories: Path
    worktrees: Path
    scopes: Path
    profiles: Path
    builds: Path
    topologies: Path
    scenarios: Path
    state: Path
    marker: Path
    states_file: Path

    @classmethod
    def discover(cls, repository: Path) -> "Paths":
        repository = Path(repository).expanduser()
        if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
            try:
                assert_no_symlink_components(repository, "repository")
            except OSError as error:
                raise WorkspaceError(str(error)) from error
        repository = repository.resolve()
        configured = os.environ.get("ATRINIK_WORKSPACE_DIR")
        workspace = Path(configured).expanduser() if configured else repository / "workspace"
        if not workspace.is_absolute():
            raise WorkspaceError("ATRINIK_WORKSPACE_DIR must be an absolute path")
        if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
            try:
                assert_no_symlink_components(workspace, "workspace")
            except OSError as error:
                raise WorkspaceError(str(error)) from error
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
            scopes=workspace / "scopes",
            profiles=workspace / "profiles",
            builds=workspace / "build",
            topologies=workspace / "topologies",
            scenarios=workspace / "scenarios",
            state=workspace / "state",
            marker=workspace / ".atrinik-workspace.json",
            states_file=workspace / "states.json",
        )

    def ensure(self) -> None:
        if self.workspace == Path("/") or self.workspace in self.repository.parents:
            raise WorkspaceError(f"refusing unsafe workspace path: {self.workspace}")
        if self.workspace.exists() and not self.workspace.is_dir():
            raise WorkspaceError(f"workspace path is not a directory: {self.workspace}")
        if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
            try:
                assert_no_symlink_components(self.workspace, "workspace")
            except OSError as error:
                raise WorkspaceError(str(error)) from error
        self.workspace.mkdir(parents=True, exist_ok=True)
        if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
            try:
                assert_no_symlink_components(self.workspace, "workspace")
            except OSError as error:
                raise WorkspaceError(str(error)) from error
        expected = {"schema_version": SCHEMA_VERSION}
        created = False
        descriptor: int | None = None
        empty_marker_retries = 0
        flags = os.O_RDWR | O_BINARY | O_CLOEXEC
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW

        while descriptor is None:
            if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
                try:
                    assert_no_symlink_components(self.marker, "workspace marker")
                except OSError as error:
                    raise WorkspaceError(str(error)) from error
            if self.marker.is_symlink():
                raise WorkspaceError(
                    f"refusing unmanaged non-empty workspace directory: {self.workspace}"
                )
            if not self.marker.exists() and not self.marker.is_symlink():
                entries = list(self.workspace.iterdir())
                if entries:
                    # A concurrent initializer may have installed the marker
                    # between the existence check and directory scan. Retry so
                    # both callers serialize on that marker instead of
                    # misclassifying its managed subdirectories as foreign.
                    if self.marker.exists() or self.marker.is_symlink():
                        continue
                    raise WorkspaceError(
                        f"refusing unmanaged non-empty workspace directory: {self.workspace}"
                    )
                try:
                    descriptor = os.open(
                        self.marker,
                        flags | os.O_CREAT | os.O_EXCL,
                        0o600,
                    )
                    created = True
                    continue
                except FileExistsError:
                    continue
                except OSError as error:
                    raise WorkspaceError(
                        f"cannot create workspace ownership marker {self.marker}: {error}"
                    ) from error
            try:
                descriptor = os.open(self.marker, flags)
            except OSError as error:
                raise WorkspaceError(
                    f"workspace ownership marker is invalid: {self.marker}: {error}"
                ) from error
            opened = os.fstat(descriptor)
            if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
                try:
                    assert_no_symlink_components(self.marker, "workspace marker")
                    visible = self.marker.stat(follow_symlinks=False)
                except OSError as error:
                    os.close(descriptor)
                    descriptor = None
                    raise WorkspaceError(str(error)) from error
                if (
                    not stat.S_ISREG(opened.st_mode)
                    or not stat.S_ISREG(visible.st_mode)
                    or (opened.st_dev, opened.st_ino)
                    != (visible.st_dev, visible.st_ino)
                ):
                    os.close(descriptor)
                    descriptor = None
                    raise WorkspaceError(
                        f"workspace ownership marker changed during open: {self.marker}"
                    )
            if opened.st_size == 0:
                # The process that won O_EXCL has not published the complete
                # marker yet. Do not take its lock first and mistake that
                # transient empty file for corrupt ownership metadata.
                os.close(descriptor)
                descriptor = None
                empty_marker_retries += 1
                if empty_marker_retries >= 100:
                    raise WorkspaceError(
                        f"workspace ownership marker is invalid: {self.marker}"
                    )
                time.sleep(0.01)

        try:
            opened = os.fstat(descriptor)
            if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
                try:
                    assert_no_symlink_components(self.marker, "workspace marker")
                    visible = self.marker.stat(follow_symlinks=False)
                except OSError as error:
                    raise WorkspaceError(str(error)) from error
                if (
                    not stat.S_ISREG(opened.st_mode)
                    or not stat.S_ISREG(visible.st_mode)
                    or (opened.st_dev, opened.st_ino)
                    != (visible.st_dev, visible.st_ino)
                ):
                    raise WorkspaceError(
                        f"workspace ownership marker changed during open: {self.marker}"
                    )
            if not stat.S_ISREG(opened.st_mode):
                raise WorkspaceError(
                    f"workspace ownership marker is invalid: {self.marker}"
                )
            with os.fdopen(descriptor, "r+", encoding="utf-8") as stream:
                descriptor = None
                fcntl.flock(stream, fcntl.LOCK_EX)
                if created:
                    unexpected = [
                        entry for entry in self.workspace.iterdir() if entry != self.marker
                    ]
                    if unexpected:
                        self.marker.unlink(missing_ok=True)
                        raise WorkspaceError(
                            "refusing concurrently modified unmanaged workspace "
                            f"directory: {self.workspace}"
                        )
                    json.dump(expected, stream, indent=2, sort_keys=True)
                    stream.write("\n")
                    stream.flush()
                    flush_file(stream.fileno())
                else:
                    stream.seek(0)
                    try:
                        actual = json.load(
                            stream, object_pairs_hook=_reject_duplicate_keys
                        )
                    except (OSError, json.JSONDecodeError) as error:
                        raise WorkspaceError(
                            f"cannot read {self.marker}: {error}"
                        ) from error
                    if actual != expected:
                        raise WorkspaceError(
                            f"workspace ownership marker is invalid: {self.marker}"
                        )
                for directory in (
                    self.worktrees,
                    self.scopes,
                    self.profiles,
                    self.builds,
                    self.topologies,
                    self.scenarios,
                    self.state,
                ):
                    directory.mkdir(parents=True, exist_ok=True)
        finally:
            if descriptor is not None:
                os.close(descriptor)


def _managed_path_no_symlinks(path: Path, workspace_builds: Path) -> Path:
    if workspace_builds.exists() or workspace_builds.is_symlink():
        if workspace_builds.is_symlink() or not workspace_builds.is_dir():
            raise WorkspaceError(
                f"workspace builds path is not a regular directory: {workspace_builds}"
            )
    builds = Path(os.path.abspath(workspace_builds))
    candidate = Path(os.path.abspath(path))
    try:
        relative = candidate.relative_to(builds)
    except ValueError as error:
        raise WorkspaceError(
            f"managed build path is outside workspace builds: {candidate}"
        ) from error
    current = builds
    for part in relative.parts:
        current /= part
        try:
            mode = current.lstat().st_mode
        except FileNotFoundError:
            break
        except OSError as error:
            raise WorkspaceError(
                f"cannot inspect managed build path {current}: {error}"
            ) from error
        if stat.S_ISLNK(mode):
            raise WorkspaceError(f"refusing symlinked managed build path: {current}")
        if current != candidate and not stat.S_ISDIR(mode):
            raise WorkspaceError(
                f"managed build parent is not a directory: {current}"
            )
    return candidate


def managed_reset(path: Path, workspace_builds: Path, purpose: str) -> None:
    path = _managed_path_no_symlinks(path, workspace_builds)
    marker = path / MANAGED_MARKER
    if path.exists():
        if not path.is_dir() or not marker.is_file() or marker.is_symlink():
            raise WorkspaceError(f"refusing to replace unmanaged build path: {path}")
        metadata = load_json(marker)
        if metadata != {"schema_version": SCHEMA_VERSION, "purpose": purpose}:
            raise WorkspaceError(f"managed build marker does not match {purpose}: {path}")
        # Import lazily to preserve the model/CLI boundary: workspace imports
        # this module, while completion must not import the heavyweight
        # workspace implementation. The owned-tree remover safely makes
        # immutable generated directories writable before deleting them.
        from .workspace import remove_owned_tree

        remove_owned_tree(path)
    path.mkdir(parents=True)
    atomic_json(marker, {"schema_version": SCHEMA_VERSION, "purpose": purpose})


def managed_directory(path: Path, workspace_builds: Path, purpose: str) -> None:
    path = _managed_path_no_symlinks(path, workspace_builds)
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


def managed_remove(path: Path, workspace_builds: Path, purpose: str) -> None:
    """Remove one marker-owned build root after repeating every ownership check."""

    if workspace_builds.is_symlink() or not workspace_builds.is_dir():
        raise WorkspaceError(
            f"workspace builds path is not a regular directory: {workspace_builds}"
        )
    path = _managed_path_no_symlinks(path, workspace_builds)
    marker = path / MANAGED_MARKER
    if not path.is_dir() or not marker.is_file() or marker.is_symlink():
        raise WorkspaceError(f"refusing to remove unmanaged build path: {path}")
    metadata = load_json(marker)
    if metadata != {"schema_version": SCHEMA_VERSION, "purpose": purpose}:
        raise WorkspaceError(f"managed build marker does not match {purpose}: {path}")
    from .workspace import remove_owned_tree

    remove_owned_tree(path)


def profile_key(paths: dict[str, Path], *, namespace: str = "") -> str:
    coordinates = {name: str(path) for name, path in paths.items()}
    value: Any = (
        {"namespace": namespace, "paths": coordinates}
        if namespace
        else coordinates
    )
    payload = json.dumps(value, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(payload.encode()).hexdigest()[:12]
