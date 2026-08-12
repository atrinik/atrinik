from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import stat
from typing import Any, Iterable

from .model import MANAGED_MARKER, Manifest, Paths, WorkspaceError, validate_name


_PROTOCOL_COMMAND = "__complete"
_MAX_WORDS = 256
_MAX_WORD_LENGTH = 4096
_MAX_DIRECTORY_ENTRIES = 256
_MAX_METADATA_BYTES = 256 * 1024
_MAX_RECORD_ENTRIES = 64
_MAX_RECORD_BYTES = 64 * 1024
_MAX_MARKER_BYTES = 1024
_PROFILE_KEYS = {"schema_version", "name", "stack", "components"}
_SCENARIO_KEYS = {
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


def mark(action: argparse.Action, kind: str) -> argparse.Action:
    """Classify an argparse value for parser-driven completion."""

    setattr(action, "completion_kind", kind)
    return action


def shell_script(shell: str) -> str:
    scripts = {
        "bash": _BASH_SCRIPT,
        "zsh": _ZSH_SCRIPT,
        "fish": _FISH_SCRIPT,
    }
    return scripts[shell]


def protocol(
    root_parser: argparse.ArgumentParser, repository: Path, arguments: list[str]
) -> int:
    """Emit a bounded, line-oriented completion response without side effects."""

    try:
        cursor, words = _protocol_arguments(arguments)
        mode, candidates = complete(root_parser, repository, words, cursor)
    except (OSError, ValueError, WorkspaceError):
        mode, candidates = "none", []
    print(mode)
    for candidate in candidates:
        print(candidate)
    return 0


def _protocol_arguments(arguments: list[str]) -> tuple[int, list[str]]:
    if len(arguments) < 2 or arguments[1] != "--":
        raise ValueError("invalid completion protocol arguments")
    cursor = int(arguments[0])
    words = arguments[2:]
    if not 0 <= cursor <= len(words) or len(words) > _MAX_WORDS:
        raise ValueError("invalid completion cursor")
    if any(len(word) > _MAX_WORD_LENGTH for word in words):
        raise ValueError("completion word is too long")
    return cursor, words


def complete(
    root_parser: argparse.ArgumentParser,
    repository: Path,
    words: list[str],
    cursor: int,
) -> tuple[str, list[str]]:
    current = words[cursor] if cursor < len(words) else ""
    completed = words[1:cursor] if words else []
    selected = _select_action(root_parser, completed)
    if selected.forwarded:
        return "none", []
    if selected.pending is not None:
        return _value_candidates(
            selected.pending,
            repository,
            selected.values,
            current,
        )
    if current.startswith("--") and "=" in current:
        option, prefix = current.split("=", 1)
        action = selected.options.get(option)
        if action is not None and _takes_value(action):
            mode, values = _value_candidates(
                action, repository, selected.values, prefix
            )
            if mode == "path":
                return mode, values
            return mode, [f"{option}={value}" for value in values]
    if current.startswith("-"):
        return "candidates", _option_candidates(selected, current)
    action = selected.positional
    if action is None:
        return "candidates", []
    if action.nargs == argparse.REMAINDER:
        return "none", []
    return _value_candidates(action, repository, selected.values, current)


class _Selection:
    def __init__(self, parser: argparse.ArgumentParser):
        self.parser = parser
        self.positionals = _positionals(parser)
        self.positional_index = 0
        self.pending: argparse.Action | None = None
        self.consumed: set[int] = set()
        self.values: dict[str, list[str]] = {}
        self.forwarded = False
        self.options = _options(parser)

    @property
    def positional(self) -> argparse.Action | None:
        if self.positional_index >= len(self.positionals):
            return None
        return self.positionals[self.positional_index]

    def descend(self, parser: argparse.ArgumentParser) -> None:
        self.parser = parser
        self.positionals = _positionals(parser)
        self.positional_index = 0
        self.pending = None
        self.options = _options(parser)


def _select_action(
    root_parser: argparse.ArgumentParser, completed: list[str]
) -> _Selection:
    selection = _Selection(root_parser)
    index = 0
    while index < len(completed):
        word = completed[index]
        if word == "--":
            selection.forwarded = True
            return selection
        if selection.pending is not None:
            _record(selection, selection.pending, word)
            selection.pending = None
            index += 1
            continue
        option_name, separator, option_value = word.partition("=")
        option = selection.options.get(option_name)
        if word.startswith("-") and option is not None:
            selection.consumed.add(id(option))
            if _takes_value(option):
                if separator:
                    _record(selection, option, option_value)
                else:
                    selection.pending = option
            index += 1
            continue
        positional = selection.positional
        if positional is None:
            index += 1
            continue
        if isinstance(positional, argparse._SubParsersAction):
            child = positional.choices.get(word)
            if child is None:
                index += 1
                continue
            _record(selection, positional, word)
            selection.descend(child)
            index += 1
            continue
        if positional.nargs == argparse.REMAINDER:
            selection.forwarded = True
            return selection
        _record(selection, positional, word)
        if positional.nargs not in ("*", "+"):
            selection.positional_index += 1
        index += 1
    return selection


def _record(selection: _Selection, action: argparse.Action, value: str) -> None:
    selection.values.setdefault(action.dest, []).append(value)
    selection.consumed.add(id(action))


def _positionals(parser: argparse.ArgumentParser) -> list[argparse.Action]:
    return [action for action in parser._actions if not action.option_strings]


def _options(parser: argparse.ArgumentParser) -> dict[str, argparse.Action]:
    return {
        option: action
        for action in parser._actions
        for option in action.option_strings
    }


def _takes_value(action: argparse.Action) -> bool:
    return action.nargs != 0


def _option_candidates(selection: _Selection, prefix: str) -> list[str]:
    excluded = set(selection.consumed)
    for group in selection.parser._mutually_exclusive_groups:
        if any(id(action) in selection.consumed for action in group._group_actions):
            excluded.update(id(action) for action in group._group_actions)
    candidates: list[str] = []
    for action in selection.parser._actions:
        repeatable = isinstance(action, argparse._AppendAction)
        if id(action) in excluded and not repeatable:
            continue
        candidates.extend(
            option
            for option in action.option_strings
            if option.startswith(prefix)
        )
    return sorted(set(candidates))


def _value_candidates(
    action: argparse.Action,
    repository: Path,
    values: dict[str, list[str]],
    prefix: str,
) -> tuple[str, list[str]]:
    if action.choices is not None:
        candidates = [str(choice) for choice in action.choices]
    else:
        kind = getattr(action, "completion_kind", "none")
        if kind == "path":
            return "path", [prefix]
        candidates = _dynamic_candidates(kind, repository, values)
    return "candidates", [
        candidate
        for candidate in sorted(set(candidates))
        if candidate.startswith(prefix) and _safe_candidate(candidate)
    ]


def _dynamic_candidates(
    kind: str, repository: Path, values: dict[str, list[str]]
) -> list[str]:
    manifest = _manifest(repository)
    paths = _paths(repository)
    if kind == "component":
        if manifest is None:
            return []
        return [
            *(checkout.name for checkout in manifest.checkouts),
            *(component.name for component in manifest.components),
        ]
    if kind in {"profile_component", "profile_selection"}:
        if manifest is None:
            return []
        profile = _last(values, "name") or _last(values, "profile") or "default"
        stack_name = _profile_stack(manifest, paths, profile)
        if stack_name is None:
            return []
        stack = manifest.stacks[stack_name]
        candidates = [
            *(component.name for component in stack.components),
            *stack.providers,
        ]
        if kind == "profile_selection":
            candidates.extend(component.checkout_name for component in stack.components)
        return candidates
    if kind == "build_target":
        if manifest is None:
            return ["all"]
        profile = _last(values, "profile") or "default"
        stack_name = _profile_stack(manifest, paths, profile)
        if stack_name is None:
            return []
        stack = manifest.stacks[stack_name]
        all_roles = [
            role for role in (
                "content", "protocol", "libatrinik", "client", "server",
                "metaserver-worker",
            )
            if role in stack.providers
        ]
        return [
            *(
                ["all"]
                if all(
                    manifest.effective_build(stack_name, stack.providers[role])
                    != "none"
                    for role in all_roles
                )
                else []
            ),
            *(
                role
                for role, component in stack.providers.items()
                if manifest.effective_build(stack_name, component) != "none"
            ),
            *(
                component.name
                for component in stack.components
                if manifest.effective_build(stack_name, component) != "none"
            ),
        ]
    if kind == "profile":
        builtins = list(manifest.stacks) if manifest is not None else ["default", "classic"]
        return [*builtins, *_profile_names(manifest, paths)]
    if kind == "saved_profile":
        return _profile_names(manifest, paths)
    if kind == "worktree":
        checkout = _selected_checkout(manifest, paths, values)
        return _worktree_names(paths, checkout) if paths and checkout else []
    if kind == "state":
        return ["default", *_state_names(paths)]
    if kind == "scenario":
        return _scenario_names(manifest, paths)
    if kind == "topology":
        return _topology_names(manifest, paths)
    return []


def _manifest(repository: Path) -> Manifest | None:
    try:
        value = _json(repository / "components.json")
        if value is None:
            return None
        return Manifest.from_value(value)
    except (OSError, RecursionError, WorkspaceError):
        return None


def _paths(repository: Path) -> Paths | None:
    try:
        paths = Paths.discover(repository)
        descriptor = _workspace_descriptor(paths)
        if descriptor is None:
            return None
        os.close(descriptor)
        return paths
    except (OSError, RuntimeError, WorkspaceError):
        return None


def _profile_names(manifest: Manifest | None, paths: Paths | None) -> list[str]:
    if manifest is None or paths is None:
        return []
    root = _workspace_descriptor(paths)
    if root is None:
        return {}
    profiles = _directory_descriptor("profiles", dir_fd=root)
    try:
        if profiles is None:
            return []
        names: list[str] = []
        for filename in _entry_names(profiles, file_type="file"):
            path = Path(filename)
            if path.suffix != ".json":
                continue
            name = path.stem
            if _profile_value(manifest, paths, profiles, name) is not None:
                names.append(name)
        return names
    finally:
        if profiles is not None:
            os.close(profiles)
        os.close(root)


def _profile_stack(
    manifest: Manifest, paths: Paths | None, profile: str
) -> str | None:
    if profile in manifest.stacks:
        return profile
    if paths is None or not _valid_name(profile):
        return None
    root = _workspace_descriptor(paths)
    if root is None:
        return None
    profiles = _directory_descriptor("profiles", dir_fd=root)
    try:
        value = (
            _profile_value(manifest, paths, profiles, profile)
            if profiles is not None
            else None
        )
        if value is None:
            return None
        stack = value.get("stack")
        return stack if isinstance(stack, str) and stack in manifest.stacks else None
    finally:
        if profiles is not None:
            os.close(profiles)
        os.close(root)


def _profile_value(
    manifest: Manifest, paths: Paths, profiles: int, name: str
) -> dict[str, Any] | None:
    if not _valid_name(name):
        return None
    value = _json_at(profiles, f"{name}.json", _MAX_RECORD_BYTES)
    if not isinstance(value, dict) or set(value) != _PROFILE_KEYS:
        return None
    stack_name = value.get("stack")
    if (
        value.get("schema_version") != 3
        or value.get("name") != name
        or not isinstance(stack_name, str)
        or stack_name not in manifest.stacks
        or not isinstance(value.get("components"), dict)
    ):
        return None
    expected = {component.name for component in manifest.stacks[stack_name].components}
    if set(value["components"]) != expected:
        return None
    checkout_selectors: dict[str, dict[str, Any]] = {}
    for component_name, selector in value["components"].items():
        if (
            not isinstance(selector, dict)
            or set(selector) != {"kind", "value"}
            or not isinstance(selector.get("kind"), str)
            or not isinstance(selector.get("value"), str)
        ):
            return None
        kind = selector["kind"]
        selected = selector["value"]
        if kind not in {"primary", "worktree", "path", "migrated-worktree"}:
            return None
        if kind == "primary" and selected:
            return None
        if kind == "worktree" and not _valid_name(selected):
            return None
        if kind == "path" and not Path(selected).is_absolute():
            return None
        if kind == "migrated-worktree":
            migrated = Path(selected)
            if (
                component_name != "content-1x"
                or not migrated.is_absolute()
                or paths is None
                or migrated.parent != paths.worktrees / "content"
            ):
                return None
        checkout = manifest.by_name[component_name].checkout_name
        previous = checkout_selectors.setdefault(checkout, selector)
        if selector != previous:
            return None
    return value


def _selected_checkout(
    manifest: Manifest | None, paths: Paths | None, values: dict[str, list[str]]
) -> str | None:
    if manifest is None:
        return None
    selected = _last(values, "component")
    profile = _last(values, "name")
    if profile is None:
        if selected in manifest.by_checkout:
            return selected
        if selected in manifest.by_name:
            return manifest.by_name[selected].checkout_name
        return None
    stack_name = _profile_stack(manifest, paths, profile)
    if stack_name:
        stack = manifest.stacks[stack_name]
        if selected in manifest.by_checkout and any(
            component.checkout_name == selected for component in stack.components
        ):
            return selected
        if selected in manifest.by_name and manifest.by_name[selected] in stack.components:
            return manifest.by_name[selected].checkout_name
        if selected in stack.providers:
            return stack.providers[selected].checkout_name
    return None


def _state_names(paths: Paths | None) -> list[str]:
    return list(_registered_states(paths))


def _registered_states(paths: Paths | None) -> dict[str, str]:
    if paths is None:
        return {}
    root = _workspace_descriptor(paths)
    if root is None:
        return {}
    try:
        return _registered_states_at(root)
    finally:
        os.close(root)


def _registered_states_at(root: int) -> dict[str, str]:
    value = _json_at(root, "states.json", _MAX_RECORD_BYTES)
    if (
        not isinstance(value, dict)
        or set(value) != {"schema_version", "states"}
        or value.get("schema_version") != 1
    ):
        return {}
    states = value.get("states")
    if not isinstance(states, dict):
        return {}
    if any(
        not _valid_name(name)
        or not isinstance(path, str)
        or not Path(path).is_absolute()
        for name, path in states.items()
    ):
        return {}
    return states


def _scenario_names(manifest: Manifest | None, paths: Paths | None) -> list[str]:
    if manifest is None or paths is None:
        return []
    opened = _workspace_child_descriptor(paths, "scenarios")
    if opened is None:
        return []
    root, records = opened
    states = _registered_states_at(root)
    try:
        names: list[str] = []
        for name in _entry_names(
            records, file_type="directory", limit=_MAX_RECORD_ENTRIES
        ):
            if not _valid_name(name):
                continue
            record = _directory_descriptor(name, dir_fd=records)
            if record is None:
                continue
            try:
                marker = _json_at(record, MANAGED_MARKER, _MAX_MARKER_BYTES)
                value = _json_at(record, "scenario.json", _MAX_RECORD_BYTES)
                if (
                    marker == {"schema_version": 1, "purpose": "test-scenario"}
                    and _valid_scenario(manifest, paths, states, name, value)
                ):
                    names.append(name)
            finally:
                os.close(record)
        return names
    finally:
        os.close(records)
        os.close(root)


def _valid_scenario(
    manifest: Manifest,
    paths: Paths,
    states: dict[str, str],
    name: str,
    value: Any,
) -> bool:
    if not isinstance(value, dict) or set(value) != _SCENARIO_KEYS:
        return False
    profile = value.get("profile")
    stack_name = (
        _profile_stack(manifest, paths, profile)
        if isinstance(profile, str)
        else None
    )
    if stack_name is None:
        return False
    stack = manifest.stacks[stack_name]
    providers = value.get("providers")
    resolved = value.get("resolved")
    required = _required_roles(stack, "server")
    expected_providers = {
        role: stack.providers[role].name for role in sorted(required)
    }
    state_name = f"scenario-{name}"
    if (
        value.get("schema_version") != 4
        or not required
        or value.get("name") != name
        or value.get("stack") != stack_name
        or value.get("state") != state_name
        or value.get("preset") != "basic-player"
        or any(
            not isinstance(value.get(field), str) or not value[field]
            for field in ("account", "character", "archetype", "provisioned_at")
        )
        or not isinstance(providers, dict)
        or providers != expected_providers
        or not isinstance(resolved, dict)
        or set(resolved) != required
        or states.get(state_name) != str(paths.scenarios / name / "state")
    ):
        return False
    for role, component_name in providers.items():
        provider = stack.providers.get(role)
        record = resolved.get(role)
        if (
            provider is None
            or component_name != provider.name
            or not isinstance(record, dict)
            or set(record)
            != {
                "path", "checkout_path", "checkout", "repository", "branch",
                "source", "head", "dirty",
            }
            or record.get("checkout") != provider.checkout_name
            or record.get("repository") != provider.repository
            or record.get("branch") != provider.branch
            or record.get("source") != provider.source
            or not isinstance(record.get("path"), str)
            or not Path(record["path"]).is_absolute()
            or not isinstance(record.get("checkout_path"), str)
            or not Path(record["checkout_path"]).is_absolute()
            or not isinstance(record.get("head"), str)
            or re.fullmatch(r"[0-9a-f]{40,64}", record["head"]) is None
            or not isinstance(record.get("dirty"), bool)
        ):
            return False
        expected = Path(record["checkout_path"])
        if provider.source != ".":
            expected = expected.joinpath(*provider.source.split("/"))
        if Path(record["path"]) != expected:
            return False
    return True


def _required_roles(stack: Any, role: str) -> set[str]:
    required: set[str] = set()
    pending = [role]
    while pending:
        current = pending.pop()
        if current in required:
            continue
        provider = stack.providers.get(current)
        if provider is None:
            return set()
        required.add(current)
        pending.extend(provider.requires)
    return required


def _topology_names(manifest: Manifest | None, paths: Paths | None) -> list[str]:
    if manifest is None or paths is None:
        return []
    opened = _workspace_child_descriptor(paths, "topologies")
    if opened is None:
        return []
    root, topologies = opened
    try:
        names: list[str] = []
        for name in _entry_names(
            topologies, file_type="directory", limit=_MAX_RECORD_ENTRIES
        ):
            if not _valid_name(name):
                continue
            record = _directory_descriptor(name, dir_fd=topologies)
            if record is None:
                continue
            try:
                marker = _json_at(record, MANAGED_MARKER, _MAX_MARKER_BYTES)
                status = _json_at(record, "status.json", _MAX_RECORD_BYTES)
                if (
                    marker == {"schema_version": 1, "purpose": f"topology:{name}"}
                    and _valid_topology(manifest, name, status)
                ):
                    names.append(name)
            finally:
                os.close(record)
        return names
    finally:
        os.close(topologies)
        os.close(root)


def _valid_topology(
    manifest: Manifest, name: str, status: Any
) -> bool:
    required = {
        "schema_version", "name", "profile", "stack", "providers",
        "dependencies", "state", "build_root", "resolved", "endpoint", "ready",
        "started_at", "stopped_at", "supervisor", "services",
    }
    if (
        not isinstance(status, dict)
        or not required <= set(status) <= required | {"error"}
        or "error" in status
        and not isinstance(status["error"], str)
    ):
        return False
    profile = status.get("profile")
    stack_name = status.get("stack")
    dependencies = status.get("dependencies")
    providers = status.get("providers")
    resolved = status.get("resolved")
    if (
        status.get("schema_version") != 1
        or status.get("name") != name
        or not isinstance(profile, str)
        or not profile
        or not isinstance(stack_name, str)
        or stack_name not in manifest.stacks
        or not isinstance(dependencies, list)
        or not dependencies
        or not all(isinstance(role, str) and _valid_name(role) for role in dependencies)
        or len(dependencies) != len(set(dependencies))
        or not isinstance(providers, dict)
        or not all(
            isinstance(role, str) and isinstance(component, str)
            for role, component in providers.items()
        )
        or set(providers) != set(dependencies)
        or not isinstance(resolved, dict)
        or set(resolved) != set(providers.values())
        or status.get("state") is not None
        and (
            not isinstance(status["state"], str)
            or not Path(status["state"]).is_absolute()
        )
        or not isinstance(status.get("build_root"), str)
        or not Path(status["build_root"]).is_absolute()
        or not isinstance(status.get("ready"), bool)
        or not isinstance(status.get("started_at"), str)
        or not status["started_at"]
        or status.get("stopped_at") is not None
        and not isinstance(status["stopped_at"], str)
    ):
        return False
    stack = manifest.stacks[stack_name]
    for role, component_name in providers.items():
        provider = stack.providers.get(role)
        if provider is None or component_name != provider.name:
            return False
        if not _valid_resolution(provider, resolved.get(component_name)):
            return False
    supervisor = status.get("supervisor")
    if (
        not _valid_process(supervisor)
        or set(supervisor) != {"pid", "start_time"}
    ):
        return False
    endpoint = status.get("endpoint")
    if endpoint is not None and (
        not isinstance(endpoint, dict)
        or set(endpoint) != {"host", "port", "fingerprint"}
        or endpoint.get("host") != "127.0.0.1"
        or not isinstance(endpoint.get("port"), int)
        or isinstance(endpoint.get("port"), bool)
        or not 1 <= endpoint["port"] <= 65535
        or endpoint.get("fingerprint") is not None
        and (
            not isinstance(endpoint["fingerprint"], str)
            or re.fullmatch(r"[0-9a-f]{64}", endpoint["fingerprint"]) is None
        )
    ):
        return False
    services = status.get("services")
    if (
        not isinstance(services, dict)
        or not services and not status.get("error")
        or not set(services) <= {"client", "server"}
    ):
        return False
    for service in services.values():
        if (
            not _valid_process(service)
            or set(service)
            != {"pid", "start_time", "status", "exit_code", "log", "cwd"}
            or service.get("status") not in {"starting", "running", "exited"}
            or service.get("exit_code") is not None
            and (
                not isinstance(service["exit_code"], int)
                or isinstance(service["exit_code"], bool)
            )
            or not isinstance(service.get("log"), str)
            or not Path(service["log"]).is_absolute()
            or not isinstance(service.get("cwd"), str)
            or not Path(service["cwd"]).is_absolute()
        ):
            return False
    return (
        ("server" in services) == (endpoint is not None)
        and not (
            status["ready"]
            and endpoint is not None
            and endpoint["fingerprint"] is None
        )
    )


def _valid_process(record: Any) -> bool:
    return (
        isinstance(record, dict)
        and isinstance(record.get("pid"), int)
        and not isinstance(record.get("pid"), bool)
        and record["pid"] > 0
        and isinstance(record.get("start_time"), str)
        and record["start_time"].isdigit()
    )


def _valid_resolution(provider: Any, record: Any) -> bool:
    if (
        not isinstance(record, dict)
        or set(record)
        != {
            "path", "checkout_path", "checkout", "repository", "branch", "source",
            "head", "dirty",
        }
        or record.get("checkout") != provider.checkout_name
        or record.get("repository") != provider.repository
        or record.get("branch") != provider.branch
        or record.get("source") != provider.source
        or not isinstance(record.get("path"), str)
        or not Path(record["path"]).is_absolute()
        or not isinstance(record.get("checkout_path"), str)
        or not Path(record["checkout_path"]).is_absolute()
        or not isinstance(record.get("head"), str)
        or re.fullmatch(r"[0-9a-f]{40,64}", record["head"]) is None
        or not isinstance(record.get("dirty"), bool)
    ):
        return False
    expected = Path(record["checkout_path"])
    if provider.source != ".":
        expected = expected.joinpath(*provider.source.split("/"))
    return Path(record["path"]) == expected


def _worktree_names(paths: Paths, checkout: str) -> list[str]:
    opened = _workspace_child_descriptor(paths, "worktrees")
    if opened is None:
        return []
    root, worktrees = opened
    selected = _directory_descriptor(checkout, dir_fd=worktrees)
    try:
        if selected is None:
            return []
        names: list[str] = []
        for name in _entry_names(selected, file_type="directory"):
            candidate = _directory_descriptor(name, dir_fd=selected)
            if candidate is None:
                continue
            try:
                if _valid_name(name) and _regular_at(candidate, ".git"):
                    names.append(name)
            finally:
                os.close(candidate)
        return names
    finally:
        if selected is not None:
            os.close(selected)
        os.close(worktrees)
        os.close(root)


def _directory_descriptor(path: str | Path, *, dir_fd: int | None = None) -> int | None:
    flags = os.O_RDONLY | os.O_CLOEXEC
    flags |= getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        return os.open(path, flags, dir_fd=dir_fd)
    except OSError:
        return None


def _workspace_descriptor(paths: Paths) -> int | None:
    root = _directory_descriptor(paths.workspace)
    if root is None:
        return None
    if _json_at(root, paths.marker.name, _MAX_MARKER_BYTES) != {"schema_version": 1}:
        os.close(root)
        return None
    return root


def _workspace_child_descriptor(paths: Paths, name: str) -> tuple[int, int] | None:
    root = _workspace_descriptor(paths)
    if root is None:
        return None
    child = _directory_descriptor(name, dir_fd=root)
    if child is None:
        os.close(root)
        return None
    return root, child


def _entry_names(
    directory: int, *, file_type: str, limit: int = _MAX_DIRECTORY_ENTRIES
) -> list[str]:
    try:
        names: list[str] = []
        with os.scandir(directory) as stream:
            for index, entry in enumerate(stream):
                if index >= limit:
                    return []
                matches = (
                    entry.is_dir(follow_symlinks=False)
                    if file_type == "directory"
                    else entry.is_file(follow_symlinks=False)
                )
                if matches:
                    names.append(entry.name)
        return sorted(names)
    except OSError:
        return []


def _regular_at(directory: int, name: str) -> bool:
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_NONBLOCK", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(name, flags, dir_fd=directory)
    except OSError:
        return False
    try:
        return stat.S_ISREG(os.fstat(descriptor).st_mode)
    finally:
        os.close(descriptor)


def _json(path: Path) -> Any:
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_NONBLOCK", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError:
        return None
    try:
        return _json_descriptor(descriptor)
    finally:
        os.close(descriptor)


def _json_at(
    directory: int, name: str, max_bytes: int = _MAX_METADATA_BYTES
) -> Any:
    flags = os.O_RDONLY | os.O_CLOEXEC | getattr(os, "O_NONBLOCK", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(name, flags, dir_fd=directory)
    except OSError:
        return None
    try:
        return _json_descriptor(descriptor, max_bytes)
    finally:
        os.close(descriptor)


def _json_descriptor(
    descriptor: int, max_bytes: int = _MAX_METADATA_BYTES
) -> Any:
    try:
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > max_bytes:
            return None
        chunks: list[bytes] = []
        remaining = max_bytes + 1
        while remaining:
            chunk = os.read(descriptor, min(64 * 1024, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        data = b"".join(chunks)
        if len(data) > max_bytes:
            return None
        return json.loads(data.decode("utf-8"), object_pairs_hook=_unique_object)
    except (OSError, UnicodeError, ValueError, RecursionError, json.JSONDecodeError):
        return None


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("duplicate metadata key")
        value[key] = item
    return value


def _valid_name(value: object) -> bool:
    if not isinstance(value, str):
        return False
    try:
        validate_name(value, "completion candidate")
    except WorkspaceError:
        return False
    return True


def _safe_candidate(value: str) -> bool:
    return bool(value) and all(character.isprintable() for character in value)


def _last(values: dict[str, list[str]], name: str) -> str | None:
    candidates = values.get(name, [])
    return candidates[-1] if candidates else None


def protocol_command() -> str:
    return _PROTOCOL_COMMAND


def classified_actions(root_parser: argparse.ArgumentParser) -> Iterable[argparse.Action]:
    """Expose parser actions for the parser/completion drift regression test."""

    pending = [root_parser]
    while pending:
        current = pending.pop()
        for action in current._actions:
            yield action
            if isinstance(action, argparse._SubParsersAction):
                pending.extend(action.choices.values())


_BASH_SCRIPT = r'''# Atrinik Bash completion; generated by ./atrinik completion bash.
_atrinik_completion() {
    local current=${COMP_WORDS[COMP_CWORD]}
    local -a response
    mapfile -t response < <(
        "${COMP_WORDS[0]}" __complete "$COMP_CWORD" -- "${COMP_WORDS[@]}" 2>/dev/null
    )
    COMPREPLY=()
    case ${response[0]-none} in
        path)
            local fragment=${response[1]-$current}
            local option_prefix=${current%"$fragment"}
            local path
            while IFS= read -r path; do
                COMPREPLY+=("${option_prefix}${path}")
            done < <(compgen -f -- "$fragment")
            ;;
        candidates)
            local candidate
            for candidate in "${response[@]:1}"; do
                [[ $candidate == "$current"* ]] && COMPREPLY+=("$candidate")
            done
            ;;
    esac
}
complete -o filenames -F _atrinik_completion atrinik ./atrinik
'''


_ZSH_SCRIPT = r'''#compdef atrinik
# Atrinik Zsh completion; generated by ./atrinik completion zsh.
_atrinik() {
    local output
    local -a response
    output=$("${words[1]}" __complete "$((CURRENT - 1))" -- "${words[@]}" 2>/dev/null)
    response=("${(@f)output}")
    case ${response[1]-none} in
        path)
            local fragment=${response[2]-${words[CURRENT]}}
            local option_prefix=${words[CURRENT]%$fragment}
            PREFIX=$fragment
            _files -P "$option_prefix"
            ;;
        candidates)
            shift response
            compadd -- "${response[@]}"
            ;;
    esac
}
if [[ ${funcstack[1]-} == _atrinik ]]; then
    _atrinik "$@"
else
    compdef _atrinik atrinik ./atrinik
fi
'''


_FISH_SCRIPT = r'''# Atrinik Fish completion; generated by ./atrinik completion fish.
function __atrinik_completion
    set -l words (commandline -opc)
    set -l current (commandline -ct)
    set -a words "$current"
    set -l cursor (math (count $words) - 1)
    set -l response (command $words[1] __complete $cursor -- $words 2>/dev/null)
    switch "$response[1]"
        case path
            set -l fragment "$response[2]"
            set -l option_prefix (string replace -r -- (string escape --style=regex "$fragment")'$' '' "$current")
            for candidate in (__fish_complete_path "$fragment")
                printf '%s%s\n' "$option_prefix" "$candidate"
            end
        case candidates
            for candidate in $response[2..-1]
                string escape -- "$candidate"
            end
    end
end
complete -c atrinik -f -a '(__atrinik_completion)'
'''
