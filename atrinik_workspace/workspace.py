from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
import fcntl
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Iterable

from .model import (
    MANAGED_MARKER,
    SCHEMA_VERSION,
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


PROFILE_KEYS = {"schema_version", "name", "components"}
SELECTOR_KEYS = {"kind", "value"}
EXPECTED_SERVER_DATA = {
    "files": ("bans", "motd"),
    "directories": ("keys", "unique-items"),
}
SENSITIVE_ARGUMENTS = {"--join_password", "--join-password"}
SENSITIVE_PREFIXES = ("--join_password=", "--join-password=")


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
) -> str:
    print(f"+ {display_arguments(arguments)}", file=sys.stderr)
    try:
        result = subprocess.run(
            arguments,
            cwd=cwd,
            check=True,
            text=True,
            capture_output=capture,
            env=env,
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


def git(path: Path, *arguments: str, capture: bool = False) -> str:
    return run(["git", "-C", str(path), *arguments], capture=capture)


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


def _is_clean(path: Path) -> bool:
    return not git(path, "status", "--porcelain=v1", "--untracked-files=all", capture=True)


def _worktree_records(repository: Path) -> list[dict[str, str]]:
    output = git(repository, "worktree", "list", "--porcelain", capture=True)
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


class Workspace:
    def __init__(self, repository: Path):
        self.paths = Paths.discover(repository)
        self.manifest = Manifest.load(self.paths.repository / "components.json")

    def initialize(self, names: list[str] | None = None, jobs: int = 4) -> None:
        self.paths.ensure()
        components = self.manifest.select(names)
        failures: list[str] = []
        with ThreadPoolExecutor(max_workers=max(1, min(jobs, len(components)))) as executor:
            futures = {executor.submit(self._ensure_repository, component): component for component in components}
            for future in as_completed(futures):
                component = futures[future]
                try:
                    future.result()
                    print(f"{component.name}: ready")
                except Exception as error:
                    failures.append(f"{component.name}: {error}")
        if failures:
            raise WorkspaceError("repository initialization failed:\n" + "\n".join(sorted(failures)))

    def _ensure_repository(self, component: Component) -> Path:
        destination = self.paths.repositories / component.name
        if not destination.exists():
            run(["gh", "repo", "clone", component.repository, str(destination)])
        self._validate_checkout(component, destination)
        return destination

    def _validate_checkout(self, component: Component, path: Path) -> None:
        if not path.is_dir():
            raise WorkspaceError(f"component checkout is not a directory: {path}")
        try:
            inside = git(path, "rev-parse", "--is-inside-work-tree", capture=True)
        except WorkspaceError as error:
            raise WorkspaceError(f"component is not a Git checkout: {path}") from error
        if inside != "true":
            raise WorkspaceError(f"component is not a Git worktree: {path}")
        remotes = git(path, "remote", "get-url", "--all", "origin", capture=True).splitlines()
        if not any(_remote_matches(url, component.repository) for url in remotes):
            upstream_urls: list[str] = []
            try:
                upstream_urls = git(
                    path, "remote", "get-url", "--all", "upstream", capture=True
                ).splitlines()
            except WorkspaceError:
                pass
            if not any(_remote_matches(url, component.repository) for url in upstream_urls):
                raise WorkspaceError(
                    f"checkout has no origin/upstream for {component.repository}: {path}"
                )

    def sync(self, names: list[str] | None, worktree_strategy: str) -> None:
        self.paths.ensure()
        components = self.manifest.select(names)
        for component in components:
            repository = self._ensure_repository(component)
            if not _is_clean(repository):
                raise WorkspaceError(f"refusing to update dirty primary checkout: {repository}")
            branch = git(repository, "branch", "--show-current", capture=True)
            if branch != component.branch:
                raise WorkspaceError(
                    f"primary checkout must be on {component.branch}, found {branch or 'detached'}: {repository}"
                )
            git(repository, "fetch", "--prune", "--tags", "origin")
            git(repository, "merge", "--ff-only", f"origin/{component.branch}")
            print(f"{component.name}: primary synchronized")
            if worktree_strategy != "none":
                self._sync_component_worktrees(component, repository, worktree_strategy)

    def _sync_component_worktrees(
        self, component: Component, repository: Path, strategy: str
    ) -> None:
        primary = repository.resolve()
        candidates: list[Path] = []
        for record in _worktree_records(repository):
            path = Path(record["worktree"]).resolve()
            if path == primary or "branch" not in record:
                continue
            if not _is_clean(path):
                raise WorkspaceError(f"refusing to update dirty worktree: {path}")
            candidates.append(path)
        for path in candidates:
            if strategy == "merge":
                git(path, "merge", "--no-edit", component.branch)
            elif strategy == "rebase":
                git(path, "rebase", component.branch)
            else:
                raise WorkspaceError(f"unknown worktree strategy: {strategy}")
            print(f"{component.name}: updated {path}")

    def create_worktree(
        self,
        component_name: str,
        label: str,
        branch: str,
        start_point: str | None,
        existing: bool,
    ) -> Path:
        self.paths.ensure()
        validate_name(label, "worktree label")
        component = self._component(component_name)
        repository = self._ensure_repository(component)
        run(["git", "check-ref-format", "--branch", branch], capture=True)
        destination = self.paths.worktrees / component.name / label
        if destination.exists():
            raise WorkspaceError(f"worktree destination already exists: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if existing:
            git(repository, "worktree", "add", str(destination), branch)
        else:
            point = start_point or f"origin/{component.branch}"
            git(repository, "fetch", "--prune", "origin")
            git(repository, "worktree", "add", "-b", branch, str(destination), point)
        self._validate_checkout(component, destination)
        print(destination)
        return destination

    def remove_worktree(self, component_name: str, label: str) -> None:
        self.paths.ensure()
        validate_name(label, "worktree label")
        component = self._component(component_name)
        repository = self._ensure_repository(component)
        destination = (self.paths.worktrees / component.name / label).resolve()
        expected_parent = (self.paths.worktrees / component.name).resolve()
        if destination.parent != expected_parent:
            raise WorkspaceError(f"invalid managed worktree path: {destination}")
        if not destination.is_dir():
            raise WorkspaceError(f"worktree does not exist: {destination}")
        if not _is_clean(destination):
            raise WorkspaceError(f"refusing to remove dirty worktree: {destination}")
        git(repository, "worktree", "remove", str(destination))

    def list_worktrees(self, names: list[str] | None = None) -> list[tuple[str, dict[str, str]]]:
        self.paths.ensure()
        result: list[tuple[str, dict[str, str]]] = []
        for component in self.manifest.select(names):
            repository = self.paths.repositories / component.name
            if not repository.is_dir():
                continue
            self._validate_checkout(component, repository)
            result.extend((component.name, record) for record in _worktree_records(repository))
        return result

    def create_profile(self, name: str) -> Path:
        self.paths.ensure()
        validate_name(name, "profile name")
        if name == "default":
            raise WorkspaceError("default is a built-in profile")
        path = self.paths.profiles / f"{name}.json"
        if path.exists():
            raise WorkspaceError(f"profile already exists: {name}")
        value = {
            "schema_version": SCHEMA_VERSION,
            "name": name,
            "components": {
                component.name: {"kind": "primary", "value": ""}
                for component in self.manifest.components
            },
        }
        atomic_json(path, value)
        print(path)
        return path

    def set_profile(
        self, name: str, component_name: str, kind: str, value: str = ""
    ) -> None:
        self.paths.ensure()
        component = self._component(component_name)
        profile = self._load_profile(name, require_file=True)
        if kind == "primary":
            value = ""
        elif kind == "worktree":
            validate_name(value, "worktree label")
            path = self.paths.worktrees / component.name / value
            self._validate_checkout(component, path)
        elif kind == "path":
            path = Path(value).expanduser()
            if not path.is_absolute():
                raise WorkspaceError("profile checkout path must be absolute")
            path = path.resolve()
            self._validate_checkout(component, path)
            value = str(path)
        else:
            raise WorkspaceError(f"invalid profile selector kind: {kind}")
        profile["components"][component.name] = {"kind": kind, "value": value}
        atomic_json(self.paths.profiles / f"{name}.json", profile)

    def resolve_profile(self, name: str) -> dict[str, Path]:
        self.paths.ensure()
        profile = self._load_profile(name, require_file=False)
        result: dict[str, Path] = {}
        for component in self.manifest.components:
            selector = profile["components"][component.name]
            kind = selector["kind"]
            value = selector["value"]
            if kind == "primary":
                path = self.paths.repositories / component.name
            elif kind == "worktree":
                path = self.paths.worktrees / component.name / value
            else:
                path = Path(value)
            path = path.resolve()
            self._validate_checkout(component, path)
            result[component.name] = path
        return result

    def _load_profile(self, name: str, require_file: bool) -> dict[str, Any]:
        validate_name(name, "profile name")
        if name == "default" and not require_file:
            return {
                "schema_version": SCHEMA_VERSION,
                "name": "default",
                "components": {
                    component.name: {"kind": "primary", "value": ""}
                    for component in self.manifest.components
                },
            }
        path = self.paths.profiles / f"{name}.json"
        if not path.is_file():
            raise WorkspaceError(f"profile does not exist: {name}")
        profile = load_json(path)
        if not isinstance(profile, dict):
            raise WorkspaceError(f"profile must be an object: {name}")
        require_keys(profile, PROFILE_KEYS, f"profile {name}")
        if profile["schema_version"] != SCHEMA_VERSION or profile["name"] != name:
            raise WorkspaceError(f"profile identity/schema mismatch: {name}")
        selectors = profile["components"]
        if not isinstance(selectors, dict) or set(selectors) != set(self.manifest.by_name):
            raise WorkspaceError(f"profile component set does not match manifest: {name}")
        for component_name, selector in selectors.items():
            if not isinstance(selector, dict):
                raise WorkspaceError(f"profile selector must be an object: {component_name}")
            require_keys(selector, SELECTOR_KEYS, f"profile selector {component_name}")
            kind = selector["kind"]
            value = selector["value"]
            if kind not in {"primary", "worktree", "path"} or not isinstance(value, str):
                raise WorkspaceError(f"invalid profile selector: {component_name}")
            if kind == "primary" and value:
                raise WorkspaceError(f"primary selector must not have a value: {component_name}")
            if kind == "worktree":
                validate_name(value, f"profile selector {component_name}")
            if kind == "path" and not Path(value).is_absolute():
                raise WorkspaceError(f"profile path must be absolute: {component_name}")
        return profile

    def profile_summary(self, name: str) -> list[tuple[str, Path, str, bool]]:
        resolved = self.resolve_profile(name)
        rows = []
        for component in self.manifest.components:
            path = resolved[component.name]
            head = git(path, "rev-parse", "--short=12", "HEAD", capture=True)
            rows.append((component.name, path, head, not _is_clean(path)))
        return rows

    def build(self, target: str, profile_name: str, tests: bool) -> Path:
        selected = self.resolve_profile(profile_name)
        key = profile_key(selected)
        root = self.paths.builds / "profiles" / f"{profile_name}-{key}"
        managed_directory(root, self.paths.builds, f"profile:{profile_name}:{key}")
        targets = self._expand_build_target(target)
        if "content" in targets or "server" in targets:
            self._collect_content(root, selected)
        if "server" in targets:
            self._stage_resources(root, selected)
        if "protocol" in targets:
            self._build_protocol(root, selected, tests)
        if "libatrinik" in targets:
            self._build_library(root, selected, tests)
        if "client" in targets:
            self._build_client(root, selected, tests)
        if "server" in targets:
            self._build_server(root, selected, tests)
        if "metaserver-worker" in targets:
            self._build_worker(root, selected)
        if target in {"sound", "resources"}:
            print(f"{target}: selected {selected[target]}")
        return root

    def _expand_build_target(self, target: str) -> list[str]:
        if target == "all":
            return [
                "content",
                "protocol",
                "libatrinik",
                "client",
                "server",
                "metaserver-worker",
            ]
        component = self._component(target)
        if component.build == "none":
            raise WorkspaceError(f"component has no wrapper build contract: {target}")
        return [target]

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
        managed_reset(output, self.paths.builds, "collected-content")
        shutil.rmtree(output)
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
                    str(output),
                    "--source-commit",
                    commit,
                ]
            )
            atomic_json(
                output / MANAGED_MARKER,
                {"schema_version": SCHEMA_VERSION, "purpose": "collected-content"},
            )
            atomic_json(
                output / ".atrinik-dependency.json",
                {"schema_version": SCHEMA_VERSION, "workspace_source": str(source), "commit": commit},
            )
        except Exception:
            if output.exists():
                shutil.rmtree(output)
            raise
        if not (output / "lib").is_dir() or not (output / "maps").is_dir():
            raise WorkspaceError("content collection did not produce lib and maps")
        return output

    def _stage_resources(self, root: Path, selected: dict[str, Path]) -> Path:
        output = root / "runtime" / "resources"
        source = selected["resources"]
        managed_reset(output, self.paths.builds, "resource-view")
        for entry in source.iterdir():
            if entry.name == ".git":
                continue
            (output / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        atomic_json(
            output / ".atrinik-dependency.json",
            {
                "schema_version": SCHEMA_VERSION,
                "workspace_source": str(source),
                "commit": git(source, "rev-parse", "HEAD", capture=True),
            },
        )
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
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                *arguments,
            ]
        )
        run(["cmake", "--build", str(binary), "--parallel"])
        if tests:
            run(["ctest", "--test-dir", str(binary), "--output-on-failure"])

    def _build_protocol(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        self._cmake(selected["protocol"], root / "build" / "protocol", [], tests)

    def _build_library(self, root: Path, selected: dict[str, Path], tests: bool) -> None:
        self._cmake(
            selected["libatrinik"],
            root / "build" / "libatrinik",
            [f"-DATRINIK_PROTOCOL_SOURCE_DIR={selected['protocol']}"],
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
                f"-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL={selected['protocol']}",
                f"-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK={selected['libatrinik']}",
            ],
            tests,
        )

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
                f"-DFETCHCONTENT_SOURCE_DIR_ATRINIK_PROTOCOL={selected['protocol']}",
                f"-DFETCHCONTENT_SOURCE_DIR_LIBATRINIK={selected['libatrinik']}",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            tests,
        )

    def _build_worker(self, root: Path, selected: dict[str, Path]) -> None:
        view = self._profile_source_view(
            root,
            "metaserver-worker",
            selected["metaserver-worker"],
            {"build", "dist", "node_modules", ".wrangler"},
            copy_all=True,
        )
        environment = os.environ.copy()
        npm_cache = self.paths.builds / "npm-cache"
        npm_cache.mkdir(parents=True, exist_ok=True)
        environment["npm_config_cache"] = str(npm_cache)
        run(["npm", "ci"], cwd=view, env=environment)
        run(["npm", "run", "check"], cwd=view, env=environment)

    def state_add(self, name: str, path: Path | None) -> Path:
        self.paths.ensure()
        validate_name(name, "state name")
        states = self._load_states()
        if name in states:
            raise WorkspaceError(f"state already exists: {name}")
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
        states[name] = str(resolved)
        atomic_json(self.paths.states_file, {"schema_version": SCHEMA_VERSION, "states": states})
        print(resolved)
        return resolved

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

    def state_path(self, name: str, server_source: Path) -> Path:
        validate_name(name, "state name")
        states = self._load_states()
        if name not in states:
            if name != "default":
                raise WorkspaceError(f"state does not exist: {name}")
            path = (self.paths.state / "server" / "default").resolve(strict=False)
        else:
            path = Path(states[name]).resolve(strict=False)
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
            except Exception:
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

    def run_client(
        self, profile_name: str, arguments: list[str], dry_run: bool
    ) -> Path:
        root = self.build("client", profile_name, tests=False)
        executable = root / "build" / "client" / "atrinik"
        working = root / "sources" / "client"
        if not executable.is_file():
            raise WorkspaceError(f"client executable is missing: {executable}")
        command = [str(executable), *arguments]
        print(f"cwd: {working}")
        print(f"command: {display_arguments(command)}")
        if not dry_run:
            run(command, cwd=working)
        return executable

    def run_server(
        self,
        profile_name: str,
        state_name: str,
        arguments: list[str],
        dry_run: bool,
    ) -> Path:
        selected = self.resolve_profile(profile_name)
        root = self.build("server", profile_name, tests=False)
        state = self.state_path(state_name, selected["server"])
        runtime = self._prepare_server_runtime(root, selected, state)
        executable = runtime / "atrinik-server"
        server_arguments = arguments or ["--port_mapping=off", "--stun_server=off"]
        command = [str(executable), *server_arguments]
        print(f"state: {state}")
        print(f"cwd: {runtime}")
        print(f"command: {display_arguments(command)}")
        if dry_run:
            return executable
        lock_path = Path(f"{state}.lock")
        with lock_path.open("a+") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise WorkspaceError(f"server state is already in use: {state}") from error
            run(command, cwd=runtime)
        return executable

    def _prepare_server_runtime(
        self, root: Path, selected: dict[str, Path], state: Path
    ) -> Path:
        runtime = root / "run" / "server"
        managed_reset(runtime, self.paths.builds, "server-runtime")
        source = selected["server"]
        binary = root / "build" / "server"
        links = {
            "atrinik-server": binary / "atrinik-server",
            "libplugin_arena.so": binary / "libplugin_arena.so",
            "libplugin_python.so": binary / "libplugin_python.so",
            "lib": root / "runtime" / "content" / "lib",
            "maps": root / "runtime" / "content" / "maps",
            "resources": root / "runtime" / "resources",
            "data": state,
            "tools": source / "tools",
        }
        for name, target in links.items():
            if not target.exists():
                raise WorkspaceError(f"server runtime input is missing: {target}")
            (runtime / name).symlink_to(target, target_is_directory=target.is_dir())
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            target = source / name
            if not target.is_file():
                raise WorkspaceError(f"server runtime input is missing: {target}")
            (runtime / name).symlink_to(target)
        custom = source / "server-custom.cfg"
        if custom.is_file():
            (runtime / "server-custom.cfg").symlink_to(custom)
        return runtime

    def _component(self, name: str) -> Component:
        try:
            return self.manifest.by_name[name]
        except KeyError as error:
            raise WorkspaceError(f"unknown component: {name}") from error
