from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import stat
from typing import Any, Iterable

from .model import MANAGED_MARKER, Manifest, Paths, WorkspaceError, validate_name


_PROTOCOL_COMMAND = "__complete"
_MAX_WORDS = 256
_MAX_WORD_LENGTH = 4096
_MAX_DIRECTORY_ENTRIES = 256
_MAX_METADATA_BYTES = 256 * 1024
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
            return "path", []
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
    if kind == "profile_component":
        if manifest is None:
            return []
        profile = _last(values, "name") or "default"
        stack_name = _profile_stack(manifest, paths, profile)
        roles = manifest.stacks[stack_name].providers if stack_name else {}
        return [
            *(checkout.name for checkout in manifest.checkouts),
            *(component.name for component in manifest.components),
            *roles,
        ]
    if kind == "build_target":
        if manifest is None:
            return ["all"]
        return [
            "all",
            *(
                role
                for component in manifest.components
                if component.build != "none"
                for role in component.provides
            ),
        ]
    if kind == "profile":
        builtins = list(manifest.stacks) if manifest is not None else ["default", "classic"]
        return [*builtins, *_profile_names(manifest, paths)]
    if kind == "worktree":
        checkout = _selected_checkout(manifest, paths, values)
        return _directory_names(paths.worktrees / checkout) if paths and checkout else []
    if kind == "state":
        return ["default", *_state_names(paths)]
    if kind == "scenario":
        return _record_names(paths.scenarios, "scenario.json", "name") if paths else []
    if kind == "topology":
        return _topology_names(paths)
    return []


def _manifest(repository: Path) -> Manifest | None:
    path = repository / "components.json"
    try:
        if not _regular_file(path) or path.stat().st_size > _MAX_METADATA_BYTES:
            return None
        return Manifest.load(path)
    except (OSError, WorkspaceError):
        return None


def _paths(repository: Path) -> Paths | None:
    try:
        return Paths.discover(repository)
    except (OSError, WorkspaceError):
        return None


def _profile_names(manifest: Manifest | None, paths: Paths | None) -> list[str]:
    if manifest is None or paths is None:
        return []
    names: list[str] = []
    for path in _entries(paths.profiles):
        if path.suffix != ".json" or not _regular_file(path):
            continue
        name = path.stem
        if _profile_record(manifest, paths, name) is not None:
            names.append(name)
    return names


def _profile_stack(
    manifest: Manifest, paths: Paths | None, profile: str
) -> str | None:
    if profile in manifest.stacks:
        return profile
    if paths is None or not _valid_name(profile):
        return None
    value = _profile_record(manifest, paths, profile)
    if value is None:
        return None
    stack = value.get("stack")
    return stack if isinstance(stack, str) and stack in manifest.stacks else None


def _profile_record(
    manifest: Manifest, paths: Paths, name: str
) -> dict[str, Any] | None:
    if not _valid_name(name):
        return None
    value = _json(paths.profiles / f"{name}.json")
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
    for selector in value["components"].values():
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
        if kind in {"path", "migrated-worktree"} and not Path(selected).is_absolute():
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
    if paths is None:
        return []
    value = _json(paths.states_file)
    if (
        not isinstance(value, dict)
        or set(value) != {"schema_version", "states"}
        or value.get("schema_version") != 1
    ):
        return []
    states = value.get("states")
    if not isinstance(states, dict):
        return []
    if any(
        not _valid_name(name)
        or not isinstance(path, str)
        or not Path(path).is_absolute()
        for name, path in states.items()
    ):
        return []
    return list(states)


def _record_names(directory: Path, metadata: str, key: str) -> list[str]:
    names: list[str] = []
    for path in _entries(directory):
        if not _normal_directory(path) or not _valid_name(path.name):
            continue
        value = _json(path / metadata)
        if (
            isinstance(value, dict)
            and set(value) == _SCENARIO_KEYS
            and value.get("schema_version") == 4
            and value.get(key) == path.name
            and all(
                isinstance(value.get(field), str)
                for field in (
                    "profile",
                    "stack",
                    "preset",
                    "state",
                    "account",
                    "character",
                    "archetype",
                    "provisioned_at",
                )
            )
            and isinstance(value.get("providers"), dict)
            and isinstance(value.get("resolved"), dict)
        ):
            names.append(path.name)
    return names


def _topology_names(paths: Paths | None) -> list[str]:
    if paths is None:
        return []
    names: list[str] = []
    for path in _entries(paths.topologies):
        if not _normal_directory(path) or not _valid_name(path.name):
            continue
        marker = _json(path / MANAGED_MARKER)
        if marker == {"schema_version": 1, "purpose": f"topology:{path.name}"}:
            names.append(path.name)
    return names


def _directory_names(directory: Path) -> list[str]:
    return [
        path.name
        for path in _entries(directory)
        if _normal_directory(path) and _valid_name(path.name)
    ]


def _entries(directory: Path) -> list[Path]:
    try:
        if not _normal_directory(directory):
            return []
        with os.scandir(directory) as stream:
            names: list[str] = []
            for entry in stream:
                names.append(entry.name)
                if len(names) > _MAX_DIRECTORY_ENTRIES:
                    return []
        return [directory / name for name in sorted(names)]
    except OSError:
        return []


def _normal_directory(path: Path) -> bool:
    try:
        mode = path.lstat().st_mode
    except OSError:
        return False
    return stat.S_ISDIR(mode)


def _regular_file(path: Path) -> bool:
    try:
        return stat.S_ISREG(path.lstat().st_mode)
    except OSError:
        return False


def _json(path: Path) -> Any:
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags)
        with os.fdopen(descriptor, encoding="utf-8") as stream:
            metadata = os.fstat(stream.fileno())
            if (
                not stat.S_ISREG(metadata.st_mode)
                or metadata.st_size > _MAX_METADATA_BYTES
            ):
                return None
            return json.load(stream, object_pairs_hook=_unique_object)
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError):
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
            mapfile -t COMPREPLY < <(compgen -f -- "$current")
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


_ZSH_SCRIPT = r'''# Atrinik Zsh completion; generated by ./atrinik completion zsh.
_atrinik_completion() {
    local output
    local -a response
    output=$("${words[1]}" __complete "$((CURRENT - 1))" -- "${words[@]}" 2>/dev/null)
    response=("${(@f)output}")
    case ${response[1]-none} in
        path)
            _files
            ;;
        candidates)
            shift response
            compadd -- "${response[@]}"
            ;;
    esac
}
compdef _atrinik_completion atrinik ./atrinik
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
            __fish_complete_path "$current"
        case candidates
            for candidate in $response[2..-1]
                string escape -- "$candidate"
            end
    end
end
complete -c atrinik -f -a '(__atrinik_completion)'
'''
