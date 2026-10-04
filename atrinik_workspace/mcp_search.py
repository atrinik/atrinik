"""Revision-bound, bounded search across manifest-selected source snapshots.

The caller resolves selectors to context ``Snapshot`` objects. Request data
never supplies filesystem paths.  This module uses ripgrep for source matching
and delegates language-aware queries to repository-owned adapters.
"""

from __future__ import annotations

from collections.abc import Callable, Iterable, Mapping, Sequence
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import selectors
import stat
import subprocess
import time
from typing import Protocol
from urllib.parse import quote

from atrinik_workspace.mcp_contract import (
    ContractError,
    Coordinate,
    canonical_json,
    decode_cursor,
    encode_cursor,
    enforce_context_budget,
    enforce_shape_limits,
    guard_request,
    load_json,
    redact,
    snapshot_fingerprint,
    CONTRACT_PATH,
)


SCHEMA_VERSION = "atrinik.mcp.result/v1"
PROVIDER_VERSION = "atrinik-search/1"
SEARCH_MODES = frozenset(
    {
        "exact",
        "regex",
        "path",
        "filename",
        "symbols",
        "references",
        "history",
        "blame",
    }
)
_RG_OUTPUT_BYTES = 4 * 1024 * 1024
_STDERR_BYTES = 4096
_SNIPPET_BYTES = 512
_SEARCH_FILE_BYTES = 256 * 1024
_SEARCH_FILES = 1000
_SEARCH_BATCH_FILES = 64
_SEARCH_FORBIDDEN_SEGMENTS = frozenset(
    {".cache", ".pytest_cache", "__pycache__", "dist", "node_modules", "target"}
)
_ARCHIVE_SUFFIXES = frozenset({".7z", ".bz2", ".gz", ".rar", ".tar", ".xz", ".zip"})


class SearchAdapter(Protocol):
    """Repository-owned language adapter; implementations remain source owners."""

    def search(
        self,
        *,
        mode: str,
        query: str,
        root: Path,
        deadline: float,
        cancelled: Callable[[], bool],
    ) -> Iterable[Mapping[str, object]]: ...


class ResolvedSnapshot(Protocol):
    """Frozen interface supplied by ``mcp_context.ContextService``."""

    coordinate: Coordinate
    root: Path
    identity: Mapping[str, object]
    metadata: Mapping[str, object]

    def assert_current(self) -> None: ...


def _metadata(snapshot: ResolvedSnapshot, key: str) -> object:
    if key not in snapshot.metadata:
        raise ContractError("INTERNAL", "resolved snapshot metadata is incomplete")
    return snapshot.metadata[key]


def _source_root(snapshot: ResolvedSnapshot) -> Path:
    source = _metadata(snapshot, "source")
    if not isinstance(source, str):
        raise ContractError("INTERNAL", "resolved source identity is invalid")
    if source == ".":
        return snapshot.root
    from atrinik_workspace.mcp_context import _source_selector

    contract = load_json(CONTRACT_PATH)
    relative = PurePosixPath(source.replace("\\", "/"))
    forbidden = set(contract["forbidden_path_segments"]) | set(
        _SEARCH_FORBIDDEN_SEGMENTS
    )
    if relative.is_absolute() or any(
        part in {"", ".", ".."} or part.casefold() in forbidden
        for part in relative.parts
    ):
        raise ContractError("INTERNAL", "resolved source identity is invalid")
    # Malformed provider metadata retains its internal-error contract. Apply
    # the shared credential policy before the selected source is inventoried.
    relative = _source_selector(source)
    return snapshot.root.joinpath(*relative.parts)


def _cursor_snapshot_identity(
    snapshot: ResolvedSnapshot, adapter: SearchAdapter | None
) -> dict[str, object]:
    adapter_identity = None
    if adapter is not None:
        adapter_identity = getattr(adapter, "identity", None)
        if not isinstance(adapter_identity, str) or not adapter_identity:
            raise ContractError("INTERNAL", "language adapter identity is required")
    return {
        "context_identity": dict(snapshot.identity),
        "coordinate": snapshot.coordinate.json(),
        "metadata": dict(snapshot.metadata),
        "index_identity": adapter_identity,
        "provider_version": PROVIDER_VERSION,
        "schema_version": SCHEMA_VERSION,
    }


def _cancelled(value: object) -> bool:
    if value is None:
        return False
    if callable(value):
        return bool(value())
    method = getattr(value, "is_set", None)
    if callable(method):
        return bool(method())
    raise ContractError("INVALID_ARGUMENT", "cancellation handle is invalid")


def _check_deadline(deadline: float, cancellation: object) -> None:
    if _cancelled(cancellation):
        raise ContractError("CANCELLED", "request was cancelled")
    if time.monotonic() >= deadline:
        raise ContractError("TIMEOUT", "search deadline expired")


def _verify_snapshot(snapshot: ResolvedSnapshot) -> None:
    try:
        snapshot.assert_current()
    except ContractError:
        raise
    except Exception as error:
        raise ContractError("STALE_COORDINATE", "snapshot identity is unavailable") from error


def _safe_root(snapshot: ResolvedSnapshot) -> tuple[int, int]:
    root = _source_root(snapshot)
    if not isinstance(root, Path) or not root.is_absolute():
        raise ContractError("INTERNAL", "resolved snapshot root is invalid")
    try:
        metadata = root.lstat()
    except OSError as error:
        raise ContractError("STALE_COORDINATE", "resolved snapshot root is unavailable") from error
    if not stat.S_ISDIR(metadata.st_mode) or stat.S_ISLNK(metadata.st_mode):
        raise ContractError("FORBIDDEN", "resolved snapshot root is not a safe directory")
    return metadata.st_dev, metadata.st_ino


def _safe_relative(root: Path, raw: str) -> str | None:
    """Validate an rg-returned path without exposing rejected path text."""

    if (
        not isinstance(raw, str)
        or not raw
        or "\\" in raw
        or redact(raw) != raw
    ):
        return None
    from atrinik_workspace.mcp_context import _source_selector

    try:
        relative = _source_selector(raw)
    except ContractError:
        return None
    contract = load_json(CONTRACT_PATH)
    forbidden = set(contract["forbidden_path_segments"]) | set(
        _SEARCH_FORBIDDEN_SEGMENTS
    )
    if relative.is_absolute() or any(
        part in {"", ".", ".."} or part.casefold() in forbidden
        for part in relative.parts
    ):
        return None
    if relative.suffix.casefold() in _ARCHIVE_SUFFIXES:
        return None
    current = root
    try:
        for index, part in enumerate(relative.parts):
            current = current / part
            metadata = current.lstat()
            if stat.S_ISLNK(metadata.st_mode):
                return None
            if index < len(relative.parts) - 1 and not stat.S_ISDIR(metadata.st_mode):
                return None
        if not stat.S_ISREG(metadata.st_mode):
            return None
    except OSError:
        return None
    return relative.as_posix()


def _terminate(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is None:
        process.kill()
    try:
        process.wait(timeout=1)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=1)


def _capture(
    arguments: Sequence[str],
    *,
    root: Path,
    deadline: float,
    cancellation: object,
    pass_fds: Sequence[int] = (),
    environment: Mapping[str, str] | None = None,
) -> tuple[bytes, bytes, int, bool]:
    """Capture rg without shell evaluation or unbounded pipe buffering."""

    try:
        process_environment = {"PATH": os.environ.get("PATH", ""), "LC_ALL": "C"}
        if environment:
            process_environment.update(environment)
        process = subprocess.Popen(
            list(arguments),
            cwd=root,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=process_environment,
            pass_fds=tuple(pass_fds),
        )
    except (OSError, ValueError) as error:
        raise ContractError("UNSUPPORTED_OPERATION", "search subprocess is unavailable") from error
    assert process.stdout is not None and process.stderr is not None
    output = bytearray()
    errors = bytearray()
    limited = False
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ, (output, _RG_OUTPUT_BYTES))
    selector.register(process.stderr, selectors.EVENT_READ, (errors, _STDERR_BYTES))
    try:
        while selector.get_map():
            _check_deadline(deadline, cancellation)
            wait = min(0.05, max(0.0, deadline - time.monotonic()))
            for key, _ in selector.select(timeout=wait):
                target, limit = key.data
                chunk = os.read(
                    key.fileobj.fileno(), min(64 * 1024, limit + 1 - len(target))
                )
                if not chunk:
                    selector.unregister(key.fileobj)
                    continue
                target.extend(chunk)
                if len(target) > limit:
                    del target[limit:]
                    limited = True
                    _terminate(process)
                    for registered in list(selector.get_map().values()):
                        selector.unregister(registered.fileobj)
                    break
        return_code = process.wait(timeout=max(0.01, deadline - time.monotonic()))
    except (ContractError, subprocess.TimeoutExpired):
        _terminate(process)
        if _cancelled(cancellation):
            raise ContractError("CANCELLED", "request was cancelled")
        raise ContractError("TIMEOUT", "search deadline expired")
    finally:
        selector.close()
        process.stdout.close()
        process.stderr.close()
    return bytes(output), bytes(errors), return_code, limited


def _git_capture(
    snapshot: ResolvedSnapshot,
    arguments: Sequence[str],
    *,
    deadline: float,
    cancellation: object,
) -> tuple[bytes, int, bool]:
    _check_context_git_policy(snapshot, deadline, cancellation)
    directory_flags, _file_flags = _safe_open_flags()
    try:
        checkout_descriptor = os.open(snapshot.root, directory_flags)
    except OSError as error:
        raise ContractError("STALE_COORDINATE", "checkout root is unavailable") from error
    try:
        output, _errors, return_code, limited = _capture(
            [
                "git",
                "--no-replace-objects",
                "--no-lazy-fetch",
                "--literal-pathspecs",
                "-c",
                "core.fsmonitor=false",
                "-c",
                f"core.hooksPath={os.devnull}",
                "-c",
                "diff.external=",
                "-c",
                "pager.log=false",
                "-c",
                "pager.blame=false",
                *arguments,
            ],
            root=Path(f"/proc/self/fd/{checkout_descriptor}"),
            deadline=deadline,
            cancellation=cancellation,
            pass_fds=(checkout_descriptor,),
            environment={
                "GIT_CONFIG_NOSYSTEM": "1",
                "GIT_CONFIG_GLOBAL": os.devnull,
                "GIT_CONFIG_COUNT": "0",
                "GIT_NO_REPLACE_OBJECTS": "1",
                "GIT_NO_LAZY_FETCH": "1",
                "GIT_OPTIONAL_LOCKS": "0",
                "GIT_TERMINAL_PROMPT": "0",
                "GIT_ASKPASS": "/bin/false",
                "GIT_PAGER": "cat",
            },
        )
    finally:
        os.close(checkout_descriptor)
    return output, return_code, limited


def _check_context_git_policy(
    snapshot: ResolvedSnapshot, deadline: float, cancellation: object
) -> None:
    """Reuse the context policy, with a fallback for isolated module tests."""

    try:
        from atrinik_workspace.mcp_context import check_git_read_policy
    except ImportError:
        check_git_read_policy = None
    if check_git_read_policy is not None:
        check_git_read_policy(snapshot.root)
        _check_deadline(deadline, cancellation)
        return

    directory_flags, _file_flags = _safe_open_flags()
    try:
        checkout_descriptor = os.open(snapshot.root, directory_flags)
    except OSError as error:
        raise ContractError("STALE_COORDINATE", "checkout root is unavailable") from error
    try:
        output, _errors, return_code, limited = _capture(
            [
                "git",
                "--no-replace-objects",
                "--no-lazy-fetch",
                "config",
                "--local",
                "--null",
                "--name-only",
                "--get-regexp",
                r"^filter\..*\.(clean|process)$",
            ],
            root=Path(f"/proc/self/fd/{checkout_descriptor}"),
            deadline=deadline,
            cancellation=cancellation,
            pass_fds=(checkout_descriptor,),
            environment={
                "GIT_CONFIG_NOSYSTEM": "1",
                "GIT_CONFIG_GLOBAL": os.devnull,
                "GIT_CONFIG_COUNT": "0",
                "GIT_NO_REPLACE_OBJECTS": "1",
                "GIT_NO_LAZY_FETCH": "1",
                "GIT_OPTIONAL_LOCKS": "0",
                "GIT_TERMINAL_PROMPT": "0",
                "GIT_ASKPASS": "/bin/false",
                "GIT_PAGER": "cat",
            },
        )
    finally:
        os.close(checkout_descriptor)
    if limited or return_code not in {0, 1}:
        raise ContractError("INCOMPLETE", "Git read policy is unavailable")
    if return_code == 0 or output:
        raise ContractError("FORBIDDEN", "configured Git content filters are not allowed")


def _safe_open_flags() -> tuple[int, int]:
    nofollow = getattr(os, "O_NOFOLLOW", 0)
    directory_only = getattr(os, "O_DIRECTORY", 0)
    nonblocking = getattr(os, "O_NONBLOCK", 0)
    if not all((nofollow, directory_only, nonblocking)) or os.open not in os.supports_dir_fd:
        raise ContractError(
            "UNSUPPORTED_OPERATION", "safe descriptor-relative search is unavailable"
        )
    return (
        os.O_RDONLY | os.O_CLOEXEC | directory_only | nofollow,
        os.O_RDONLY | os.O_CLOEXEC | nonblocking | nofollow,
    )


def _open_source_root(snapshot: ResolvedSnapshot) -> int:
    """Open the logical component root without following any directory link."""

    directory_flags, _file_flags = _safe_open_flags()
    descriptors: list[int] = []
    try:
        directory = os.open(snapshot.root, directory_flags)
        descriptors.append(directory)
        source = _metadata(snapshot, "source")
        if source != ".":
            relative = PurePosixPath(str(source).replace("\\", "/"))
            for part in relative.parts:
                directory = os.open(part, directory_flags, dir_fd=directory)
                descriptors.append(directory)
        retained = descriptors.pop()
        return retained
    except OSError as error:
        raise ContractError(
            "FORBIDDEN", "resolved source root is not a safe directory"
        ) from error
    finally:
        for descriptor in reversed(descriptors):
            os.close(descriptor)


def _open_regular(root_descriptor: int, relative: str) -> int:
    """Open one selected-root file descriptor-relative without following links."""

    directory_flags, file_flags = _safe_open_flags()
    directories: list[int] = []
    try:
        directory = os.dup(root_descriptor)
        directories.append(directory)
        parts = PurePosixPath(relative).parts
        for part in parts[:-1]:
            directory = os.open(part, directory_flags, dir_fd=directory)
            directories.append(directory)
        descriptor = os.open(parts[-1], file_flags, dir_fd=directory)
        metadata = os.fstat(descriptor)
        if not stat.S_ISREG(metadata.st_mode):
            os.close(descriptor)
            raise OSError("not a regular file")
        if metadata.st_size > _SEARCH_FILE_BYTES:
            os.close(descriptor)
            raise ContractError(
                "LIMIT_EXCEEDED", "source file exceeds the search byte bound"
            )
        return descriptor
    except ContractError:
        raise
    except OSError as error:
        raise ContractError("FORBIDDEN", "source path is not a safe regular file") from error
    finally:
        for descriptor in reversed(directories):
            os.close(descriptor)


def _checkout_path(snapshot: ResolvedSnapshot, path: str) -> str:
    source = _metadata(snapshot, "source")
    return path if source == "." else f"{source}/{path}"


def _descriptor_git_oid(
    descriptor: int,
    algorithm: str,
    deadline: float,
    cancellation: object,
) -> str:
    before = os.fstat(descriptor)
    if before.st_size > _SEARCH_FILE_BYTES:
        raise ContractError("STALE_COORDINATE", "source grew beyond the search bound")
    digest = hashlib.new(algorithm)
    digest.update(f"blob {before.st_size}\0".encode("ascii"))
    offset = 0
    while offset < before.st_size:
        _check_deadline(deadline, cancellation)
        chunk = os.pread(descriptor, min(64 * 1024, before.st_size - offset), offset)
        if not chunk:
            raise ContractError("STALE_COORDINATE", "source changed during inspection")
        digest.update(chunk)
        offset += len(chunk)
    after = os.fstat(descriptor)
    if (
        before.st_dev,
        before.st_ino,
        before.st_size,
        before.st_mtime_ns,
    ) != (
        after.st_dev,
        after.st_ino,
        after.st_size,
        after.st_mtime_ns,
    ):
        raise ContractError("STALE_COORDINATE", "source changed during inspection")
    return digest.hexdigest()


def _validate_descriptor_batch(
    snapshot: ResolvedSnapshot,
    descriptors: Mapping[int, str],
    deadline: float,
    cancellation: object,
) -> None:
    if not descriptors:
        return
    checkout_paths = [_checkout_path(snapshot, path) for path in descriptors.values()]
    output, return_code, limited = _git_capture(
        snapshot,
        ["ls-files", "-v", "-z", "--", *checkout_paths],
        deadline=deadline,
        cancellation=cancellation,
    )
    if return_code != 0 or limited:
        raise ContractError("STALE_COORDINATE", "source index identity is unavailable")
    index_entries: dict[str, str] = {}
    for raw in output.split(b"\0"):
        if not raw:
            continue
        try:
            entry = raw.decode("utf-8")
        except UnicodeError as error:
            raise ContractError("STALE_COORDINATE", "source index is malformed") from error
        if len(entry) < 3 or entry[1] != " ":
            raise ContractError("STALE_COORDINATE", "source index is malformed")
        index_entries[entry[2:]] = entry[0]
    for checkout_path, tag in index_entries.items():
        if tag == "S" or tag.islower():
            raise ContractError(
                "STALE_COORDINATE", "source index hides selected file changes"
            )

    if set(index_entries) != set(checkout_paths):
        raise ContractError("STALE_COORDINATE", "selected source contains an untracked file")
    if snapshot.coordinate.dirty_fingerprint is not None:
        return
    tree_output, return_code, limited = _git_capture(
        snapshot,
        ["ls-tree", "-r", "-z", snapshot.coordinate.commit, "--", *checkout_paths],
        deadline=deadline,
        cancellation=cancellation,
    )
    if return_code != 0 or limited:
        raise ContractError("STALE_COORDINATE", "pinned source tree is unavailable")
    tree_entries: dict[str, tuple[str, str]] = {}
    for raw in tree_output.split(b"\0"):
        if not raw:
            continue
        try:
            header, path_bytes = raw.split(b"\t", 1)
            mode, kind, object_id = header.decode("ascii").split()
            checkout_path = path_bytes.decode("utf-8")
        except (UnicodeError, ValueError) as error:
            raise ContractError("STALE_COORDINATE", "pinned source tree is malformed") from error
        if mode not in {"100644", "100755"} or kind != "blob":
            raise ContractError("FORBIDDEN", "selected source is not a regular Git file")
        tree_entries[checkout_path] = (object_id, mode)
    if set(tree_entries) != set(checkout_paths):
        raise ContractError("STALE_COORDINATE", "selected source differs from pinned tree")
    object_lengths = {len(value[0]) for value in tree_entries.values()}
    if object_lengths == {40}:
        algorithm = "sha1"
    elif object_lengths == {64}:
        algorithm = "sha256"
    else:
        raise ContractError("STALE_COORDINATE", "pinned object format is unsupported")
    for descriptor, path in descriptors.items():
        expected = tree_entries[_checkout_path(snapshot, path)][0]
        if _descriptor_git_oid(
            descriptor, algorithm, deadline, cancellation
        ) != expected:
            raise ContractError(
                "UNSUPPORTED_OPERATION",
                "clean source byte normalization is not supported safely",
            )


def _inventory(
    snapshot: ResolvedSnapshot, deadline: float, cancellation: object
) -> tuple[list[str], bool]:
    root = _source_root(snapshot)
    root_descriptor = _open_source_root(snapshot)
    try:
        output, _errors, return_code, limited = _capture(
            [
                "rg",
                "--no-config",
                "--files",
                "--null",
                "--sort",
                "path",
                "--hidden",
                "--no-messages",
                ".",
            ],
            root=Path(f"/proc/self/fd/{root_descriptor}"),
            deadline=deadline,
            cancellation=cancellation,
            pass_fds=(root_descriptor,),
        )
    finally:
        os.close(root_descriptor)
    if return_code != 0 and not limited:
        raise ContractError("INCOMPLETE", "source file inventory is unavailable")
    paths: list[str] = []
    for raw in output.split(b"\0"):
        _check_deadline(deadline, cancellation)
        if not raw:
            continue
        try:
            candidate = raw.decode("utf-8").removeprefix("./")
        except UnicodeError:
            limited = True
            continue
        path = _safe_relative(root, candidate)
        if path is None:
            continue
        if len(paths) >= _SEARCH_FILES:
            limited = True
            break
        paths.append(path)
    return paths, limited


def _resource_uri(coordinate: Coordinate, path: str) -> str:
    owner, repository = coordinate.repository.split("/", 1)
    encoded = "/".join(quote(part, safe="._-") for part in PurePosixPath(path).parts)
    return f"atrinik://{owner}/{repository}/{coordinate.commit}/{encoded}"


def _clip_snippet(value: str) -> tuple[str, bool]:
    safe = redact(value.rstrip("\r\n"))
    payload = safe.encode("utf-8", errors="replace")
    if len(payload) <= _SNIPPET_BYTES:
        return safe, False
    clipped = payload[:_SNIPPET_BYTES].decode("utf-8", errors="ignore")
    return clipped, True


def _base_record(snapshot: ResolvedSnapshot, path: str) -> dict[str, object]:
    coordinate = snapshot.coordinate
    source = _metadata(snapshot, "source")
    resource_path = path if source == "." else f"{source}/{path}"
    return {
        "repository": coordinate.repository,
        "branch": coordinate.branch,
        "commit": coordinate.commit,
        "worktree": coordinate.worktree,
        "dirty_fingerprint": coordinate.dirty_fingerprint,
        "component": _metadata(snapshot, "component"),
        "checkout": _metadata(snapshot, "checkout"),
        "profile": _metadata(snapshot, "profile"),
        "stack": _metadata(snapshot, "stack"),
        "roles": _metadata(snapshot, "roles"),
        "generation": _metadata(snapshot, "generation"),
        "owner": _metadata(snapshot, "owner"),
        "license": _metadata(snapshot, "license"),
        "build": _metadata(snapshot, "build"),
        "source": source,
        "path": path,
        "resource_uri": _resource_uri(coordinate, resource_path),
    }


def _content_records(
    snapshot: ResolvedSnapshot,
    mode: str,
    query: str,
    case_sensitive: bool,
    deadline: float,
    cancellation: object,
) -> tuple[list[dict[str, object]], bool]:
    paths, limited = _inventory(snapshot, deadline, cancellation)
    records: list[dict[str, object]] = []
    if not Path("/proc/self/fd").is_dir():
        raise ContractError("UNSUPPORTED_OPERATION", "safe descriptor search is unavailable")
    for batch_start in range(0, len(paths), _SEARCH_BATCH_FILES):
        descriptors: list[int] = []
        descriptor_paths: dict[str, str] = {}
        descriptor_source_paths: dict[int, str] = {}
        descriptor_identities: dict[int, tuple[int, int, int, int]] = {}
        root_descriptor = _open_source_root(snapshot)
        try:
            for path in paths[batch_start : batch_start + _SEARCH_BATCH_FILES]:
                try:
                    descriptor = _open_regular(root_descriptor, path)
                except ContractError as error:
                    if error.code in {"FORBIDDEN", "LIMIT_EXCEEDED"}:
                        limited = True
                        continue
                    raise
                descriptors.append(descriptor)
                descriptor_paths[f"/proc/self/fd/{descriptor}"] = path
                descriptor_source_paths[descriptor] = path
                metadata = os.fstat(descriptor)
                descriptor_identities[descriptor] = (
                    metadata.st_dev,
                    metadata.st_ino,
                    metadata.st_size,
                    metadata.st_mtime_ns,
                )
            if not descriptors:
                continue
            _validate_descriptor_batch(
                snapshot,
                descriptor_source_paths,
                deadline,
                cancellation,
            )
            arguments = [
                "rg",
                "--no-config",
                "--json",
                "--no-messages",
                "--color",
                "never",
                "--sort",
                "path",
            ]
            if mode == "exact":
                arguments.append("--fixed-strings")
            if not case_sensitive:
                arguments.append("--ignore-case")
            arguments.extend(["--", query, *descriptor_paths])
            output, _errors, return_code, batch_limited = _capture(
                arguments,
                root=Path(f"/proc/self/fd/{root_descriptor}"),
                deadline=deadline,
                cancellation=cancellation,
                pass_fds=(*descriptors, root_descriptor),
            )
            limited = limited or batch_limited
            for descriptor in descriptors:
                metadata = os.fstat(descriptor)
                if descriptor_identities[descriptor] != (
                    metadata.st_dev,
                    metadata.st_ino,
                    metadata.st_size,
                    metadata.st_mtime_ns,
                ):
                    raise ContractError(
                        "STALE_COORDINATE", "source changed during search"
                    )
            if return_code not in {0, 1} and not batch_limited:
                # rg uses 2 for malformed regular expressions and operational failures.
                code = "INVALID_ARGUMENT" if mode == "regex" else "INCOMPLETE"
                raise ContractError(code, "search expression or source scan is invalid")
        finally:
            for descriptor in descriptors:
                os.close(descriptor)
            os.close(root_descriptor)
        for line in output.splitlines():
            _check_deadline(deadline, cancellation)
            try:
                event = json.loads(line)
                if event.get("type") != "match":
                    continue
                data = event["data"]
                path = descriptor_paths.get(data["path"]["text"])
                line_number = data["line_number"]
                text = data["lines"]["text"]
                submatches = data["submatches"]
                if path is None or not isinstance(line_number, int) or not submatches:
                    continue
                snippet, snippet_truncated = _clip_snippet(text)
                for match in submatches:
                    start = match.get("start")
                    end = match.get("end")
                    if not all(
                        isinstance(value, int) and not isinstance(value, bool)
                        for value in (start, end)
                    ):
                        continue
                    record = _base_record(snapshot, path)
                    record.update(
                        {
                            "kind": mode,
                            "match_class": "content",
                            "index_source": "ripgrep",
                            "index_revision": PROVIDER_VERSION,
                            "line": line_number,
                            "column": start + 1,
                            "range": {
                                "start_line": line_number,
                                "start_column": start + 1,
                                "end_line": line_number,
                                "end_column": end + 1,
                            },
                            "match_bytes": end - start,
                            "snippet": snippet,
                            "snippet_truncated": snippet_truncated,
                        }
                    )
                    records.append(record)
                    if len(records) > 1000:
                        return records[:1000], True
            except (KeyError, TypeError, ValueError, UnicodeError):
                limited = True
    return records, limited


def _path_records(
    snapshot: ResolvedSnapshot,
    mode: str,
    query: str,
    case_sensitive: bool,
    deadline: float,
    cancellation: object,
) -> tuple[list[dict[str, object]], bool]:
    paths, limited = _inventory(snapshot, deadline, cancellation)
    needle = query if case_sensitive else query.casefold()
    records: list[dict[str, object]] = []
    root_descriptor = _open_source_root(snapshot)
    try:
        for batch_start in range(0, len(paths), _SEARCH_BATCH_FILES):
            opened: dict[int, str] = {}
            try:
                for path in paths[batch_start : batch_start + _SEARCH_BATCH_FILES]:
                    _check_deadline(deadline, cancellation)
                    try:
                        opened[_open_regular(root_descriptor, path)] = path
                    except ContractError as error:
                        if error.code in {"FORBIDDEN", "LIMIT_EXCEEDED"}:
                            limited = True
                            continue
                        raise
                _validate_descriptor_batch(
                    snapshot, opened, deadline, cancellation
                )
                for path in opened.values():
                    haystack = PurePosixPath(path).name if mode == "filename" else path
                    compared = haystack if case_sensitive else haystack.casefold()
                    if needle not in compared:
                        continue
                    record = _base_record(snapshot, path)
                    excerpt, excerpt_truncated = _clip_snippet(path)
                    record.update(
                        {
                            "kind": mode,
                            "match_class": "path",
                            "index_source": "ripgrep-files",
                            "index_revision": PROVIDER_VERSION,
                            "snippet": excerpt,
                            "snippet_truncated": excerpt_truncated,
                        }
                    )
                    records.append(record)
                    if len(records) > 1000:
                        return records[:1000], True
            finally:
                for descriptor in opened:
                    os.close(descriptor)
    finally:
        os.close(root_descriptor)
    return records, limited


def _adapter_records(
    snapshot: ResolvedSnapshot,
    adapter: SearchAdapter | None,
    mode: str,
    query: str,
    deadline: float,
    cancellation: object,
) -> tuple[list[dict[str, object]], bool]:
    if adapter is None:
        raise ContractError(
            "UNSUPPORTED_OPERATION", "selected source has no owned language adapter"
        )
    records: list[dict[str, object]] = []
    try:
        raw_records = adapter.search(
            mode=mode,
            query=query,
            root=_source_root(snapshot),
            deadline=deadline,
            cancelled=lambda: _cancelled(cancellation),
        )
        for raw in raw_records:
            _check_deadline(deadline, cancellation)
            if not isinstance(raw, Mapping):
                raise ContractError(
                    "INCOMPLETE", "language adapter returned an invalid record"
                )
            path_value = raw.get("path")
            path = (
                _safe_relative(_source_root(snapshot), path_value)
                if isinstance(path_value, str)
                else None
            )
            if path is None:
                raise ContractError("INCOMPLETE", "language adapter returned an unsafe path")
            record = _base_record(snapshot, path)
            record.update(
                {
                    "kind": mode,
                    "match_class": "language-index",
                    "index_source": "repository-adapter",
                    "index_revision": getattr(adapter, "identity"),
                }
            )
            for key in ("line", "column", "symbol", "language"):
                value = raw.get(key)
                if value is not None:
                    if key in {"line", "column"} and (
                        not isinstance(value, int) or isinstance(value, bool) or value < 1
                    ):
                        raise ContractError("INCOMPLETE", "language adapter position is invalid")
                    if key in {"symbol", "language"}:
                        if not isinstance(value, str):
                            raise ContractError("INCOMPLETE", "language adapter text is invalid")
                        value = redact(value)[:256]
                    record[key] = value
            records.append(record)
            if len(records) > 1000:
                return records[:1000], True
    except ContractError:
        raise
    except Exception as error:
        raise ContractError("INCOMPLETE", "language adapter search failed") from error
    return records, False


def _provenance_path(
    snapshot: ResolvedSnapshot,
    path_value: object,
    deadline: float,
    cancellation: object,
) -> tuple[str, str]:
    if not isinstance(path_value, str):
        raise ContractError("INVALID_ARGUMENT", "provenance path is required")
    path = _safe_relative(_source_root(snapshot), path_value)
    if path is None:
        raise ContractError("FORBIDDEN", "provenance path is not a safe source file")
    root_descriptor = _open_source_root(snapshot)
    try:
        descriptor = _open_regular(root_descriptor, path)
        try:
            _validate_descriptor_batch(
                snapshot, {descriptor: path}, deadline, cancellation
            )
        finally:
            os.close(descriptor)
    finally:
        os.close(root_descriptor)
    source = _metadata(snapshot, "source")
    checkout_path = path if source == "." else f"{source}/{path}"
    _output, return_code, limited = _git_capture(
        snapshot,
        ["ls-files", "--error-unmatch", "--", checkout_path],
        deadline=deadline,
        cancellation=cancellation,
    )
    if return_code != 0 or limited:
        raise ContractError("FORBIDDEN", "provenance path is not tracked source")
    output, return_code, limited = _git_capture(
        snapshot,
        ["rev-parse", "--is-shallow-repository"],
        deadline=deadline,
        cancellation=cancellation,
    )
    if return_code != 0 or limited or output.strip() != b"false":
        raise ContractError("INCOMPLETE", "Git provenance requires non-shallow history")
    return path, checkout_path


def _historical_component_path(
    snapshot: ResolvedSnapshot, checkout_path: str
) -> tuple[str, str] | None:
    if (
        not checkout_path
        or "\\" in checkout_path
        or any(not character.isprintable() for character in checkout_path)
        or redact(checkout_path) != checkout_path
    ):
        return None
    from atrinik_workspace.mcp_context import _source_selector

    try:
        relative = _source_selector(checkout_path)
    except ContractError:
        return None
    contract = load_json(CONTRACT_PATH)
    forbidden = set(contract["forbidden_path_segments"]) | set(
        _SEARCH_FORBIDDEN_SEGMENTS
    )
    if relative.is_absolute() or any(
        part in {"", ".", ".."} or part.casefold() in forbidden
        for part in relative.parts
    ):
        return None
    if relative.suffix.casefold() in _ARCHIVE_SUFFIXES:
        return None
    source = _metadata(snapshot, "source")
    if source == ".":
        return relative.as_posix(), relative.as_posix()
    prefix = PurePosixPath(str(source))
    try:
        component_path = relative.relative_to(prefix)
    except ValueError:
        return None
    return component_path.as_posix(), relative.as_posix()


def _history_records(
    snapshot: ResolvedSnapshot,
    query: str,
    case_sensitive: bool,
    path_value: object,
    deadline: float,
    cancellation: object,
) -> tuple[list[dict[str, object]], bool]:
    path, checkout_path = _provenance_path(
        snapshot, path_value, deadline, cancellation
    )
    arguments = [
        "log",
        "-z",
        "--no-show-signature",
        "--date-order",
        "--follow",
        "--max-count=1001",
        "--name-only",
        "--format=%H%x00%P%x00%an%x00%aI%x00%s",
    ]
    arguments.extend([snapshot.coordinate.commit, "--", checkout_path])
    output, return_code, limited = _git_capture(
        snapshot, arguments, deadline=deadline, cancellation=cancellation
    )
    if return_code != 0 and not limited:
        raise ContractError("INCOMPLETE", "bounded Git history is unavailable")
    try:
        fields = output.decode("utf-8").split("\0")
    except UnicodeError as error:
        raise ContractError("INCOMPLETE", "bounded Git history is malformed") from error
    if fields and fields[-1] == "":
        fields.pop()
    if len(fields) % 6:
        limited = True
        fields = fields[: len(fields) - (len(fields) % 6)]
    if len(fields) > 6000:
        limited = True
        fields = fields[:6000]
    records: list[dict[str, object]] = []
    needle = query if case_sensitive else query.casefold()
    for offset in range(0, len(fields), 6):
        _check_deadline(deadline, cancellation)
        commit, parents, author, authored_at, subject, raw_historical_path = fields[
            offset : offset + 6
        ]
        historical_path = _historical_component_path(
            snapshot, raw_historical_path.removeprefix("\n")
        )
        if historical_path is None:
            limited = True
            continue
        historical_component_path, historical_checkout_path = historical_path
        compared_subject = subject if case_sensitive else subject.casefold()
        if needle and needle not in compared_subject:
            continue
        if len(commit) != 40 or any(character not in "0123456789abcdef" for character in commit):
            limited = True
            continue
        record = _base_record(snapshot, path)
        historical = Coordinate(
            snapshot.coordinate.repository,
            snapshot.coordinate.branch,
            commit,
            snapshot.coordinate.worktree,
            snapshot.coordinate.dirty_fingerprint,
        )
        snippet, snippet_truncated = _clip_snippet(subject)
        record.update(
            {
                "kind": "history",
                "match_class": "provenance",
                "index_source": "git-log",
                "index_revision": snapshot.coordinate.commit,
                "snapshot_commit": snapshot.coordinate.commit,
                "commit": commit,
                "historical_path": historical_component_path,
                "parents": [value for value in parents.split() if len(value) == 40][
                    :16
                ],
                "author": redact(author)[:256],
                "authored_at": authored_at[:64],
                "snippet": snippet,
                "snippet_truncated": snippet_truncated,
                "resource_uri": _resource_uri(historical, historical_checkout_path),
            }
        )
        records.append(record)
        if len(records) > 1000:
            return records[:1000], True
    return records, limited


def _blame_records(
    snapshot: ResolvedSnapshot,
    path_value: object,
    line_value: object,
    deadline: float,
    cancellation: object,
) -> tuple[list[dict[str, object]], bool]:
    path, checkout_path = _provenance_path(
        snapshot, path_value, deadline, cancellation
    )
    if line_value is not None and (
        not isinstance(line_value, int)
        or isinstance(line_value, bool)
        or line_value < 1
    ):
        raise ContractError("INVALID_ARGUMENT", "blame line is invalid")
    line_range = (
        f"{line_value},{line_value}" if isinstance(line_value, int) else "1,1001"
    )
    output, return_code, limited = _git_capture(
        snapshot,
        [
            "blame",
            "--line-porcelain",
            "--no-textconv",
            "-L",
            line_range,
            snapshot.coordinate.commit,
            "--",
            checkout_path,
        ],
        deadline=deadline,
        cancellation=cancellation,
    )
    if return_code != 0 and not limited:
        raise ContractError("INCOMPLETE", "bounded Git blame is unavailable")
    try:
        lines = output.decode("utf-8").split("\n")
    except UnicodeError as error:
        raise ContractError("INCOMPLETE", "bounded Git blame is malformed") from error
    records: list[dict[str, object]] = []
    current: dict[str, object] | None = None
    for raw in lines:
        _check_deadline(deadline, cancellation)
        header = raw.split()
        if (
            not raw.startswith("\t")
            and len(header) >= 3
            and len(header[0]) == 40
            and all(character in "0123456789abcdef" for character in header[0])
            and header[1].isdigit()
            and header[2].isdigit()
        ):
            current = {
                "commit": header[0],
                "source_line": int(header[1]),
                "line": int(header[2]),
            }
            continue
        if current is None:
            continue
        if raw.startswith("author "):
            current["author"] = redact(raw[7:])[:256]
        elif raw.startswith("author-time "):
            value = raw[12:]
            current["author_time"] = int(value) if value.isdigit() else None
        elif raw.startswith("filename "):
            historical_path = _historical_component_path(snapshot, raw[9:])
            if historical_path is None:
                limited = True
                current = None
            else:
                current["historical_path"] = historical_path
        elif raw.startswith("\t"):
            commit = current["commit"]
            assert isinstance(commit, str)
            historical = current.pop("historical_path", None)
            if not (
                isinstance(historical, tuple)
                and len(historical) == 2
                and all(isinstance(value, str) for value in historical)
            ):
                limited = True
                current = None
                continue
            historical_component_path, historical_checkout_path = historical
            historical = Coordinate(
                snapshot.coordinate.repository,
                snapshot.coordinate.branch,
                commit,
                snapshot.coordinate.worktree,
                snapshot.coordinate.dirty_fingerprint,
            )
            snippet, snippet_truncated = _clip_snippet(raw[1:])
            record = _base_record(snapshot, path)
            record.update(current)
            record.update(
                {
                    "kind": "blame",
                    "match_class": "provenance",
                    "index_source": "git-blame",
                    "index_revision": snapshot.coordinate.commit,
                    "snapshot_commit": snapshot.coordinate.commit,
                    "historical_path": historical_component_path,
                    "snippet": snippet,
                    "snippet_truncated": snippet_truncated,
                    "resource_uri": _resource_uri(
                        historical, historical_checkout_path
                    ),
                }
            )
            records.append(record)
            current = None
            if len(records) > 1000:
                return records[:1000], True
    if current is not None:
        limited = True
    return records, limited


def _request_value(request: Mapping[str, object], key: str, expected: type) -> object:
    value = request.get(key)
    if not isinstance(value, expected) or (expected is int and isinstance(value, bool)):
        raise ContractError("INVALID_ARGUMENT", f"{key} is invalid")
    return value


def search(
    request: Mapping[str, object],
    snapshots: Sequence[ResolvedSnapshot],
    *,
    authorization_identity: str,
    cancellation: object = None,
    language_adapters: Mapping[str, SearchAdapter] | None = None,
    routine_bytes: int = 30720,
) -> dict[str, object]:
    """Search only the canonical snapshots supplied by the trusted resolver."""

    if not isinstance(request, Mapping):
        raise ContractError("INVALID_ARGUMENT", "search request must be an object")
    if type(routine_bytes) is not int or not 1024 <= routine_bytes <= 32768:
        raise ContractError("INVALID_ARGUMENT", "search response budget is invalid")
    allowed = {
        "mode",
        "query",
        "case_sensitive",
        "page_size",
        "cursor",
        "timeout_ms",
        "provenance",
        "path",
        "line",
    }
    if set(request) - allowed:
        raise ContractError("INVALID_ARGUMENT", "search request has unknown fields")
    mode = _request_value(request, "mode", str)
    query = request.get("query", "")
    if not isinstance(query, str):
        raise ContractError("INVALID_ARGUMENT", "query is invalid")
    case_sensitive = _request_value(request, "case_sensitive", bool)
    page_size = _request_value(request, "page_size", int)
    timeout_ms = _request_value(request, "timeout_ms", int)
    cursor = request.get("cursor")
    if cursor is not None and not isinstance(cursor, str):
        raise ContractError("INVALID_ARGUMENT", "cursor is invalid")
    provenance_mode = mode in {"history", "blame"}
    if mode not in SEARCH_MODES or (not provenance_mode and not query):
        raise ContractError("INVALID_ARGUMENT", "search mode or query is invalid")
    provenance = request.get("provenance", False)
    if not isinstance(provenance, bool):
        raise ContractError("INVALID_ARGUMENT", "provenance flag is invalid")
    if provenance_mode:
        if provenance is not True:
            raise ContractError(
                "UNSUPPORTED_OPERATION", "Git provenance requires explicit opt-in"
            )
        if not isinstance(request.get("path"), str):
            raise ContractError("INVALID_ARGUMENT", "provenance path is required")
    elif provenance or "path" in request or "line" in request:
        raise ContractError(
            "INVALID_ARGUMENT", "provenance fields require a provenance mode"
        )
    if not isinstance(authorization_identity, str) or not authorization_identity:
        raise ContractError("UNAUTHORIZED", "authorization identity is required")
    if not snapshots:
        raise ContractError("NOT_FOUND", "selector resolved no source snapshots")
    selection_identities = [
        canonical_json(
            {
                "coordinate": snapshot.coordinate.json(),
                "context_identity": dict(snapshot.identity),
                "metadata": dict(snapshot.metadata),
            }
        )
        for snapshot in snapshots
    ]
    if len(set(selection_identities)) != len(selection_identities):
        raise ContractError("INVALID_ARGUMENT", "duplicate source snapshots are not accepted")

    limits = load_json(CONTRACT_PATH)["limits"]
    if page_size < 1 or page_size > limits["page_records"]:
        raise ContractError("LIMIT_EXCEEDED", "page size is outside the allowed bound")

    input_bytes = len(canonical_json(dict(request)))
    guard_request(
        action="search",
        selector=request.get("path") if provenance_mode else None,
        data_classification="unreleased-source",
        input_bytes=input_bytes,
        requested_records=page_size,
        timeout_ms=timeout_ms,
        cancelled=_cancelled(cancellation),
    )
    enforce_shape_limits(
        query_characters=len(query),
        graph_depth=0,
        graph_edges=0,
        result_bytes=0,
        schema_depth=0,
    )
    deadline = time.monotonic() + timeout_ms / 1000
    authorization_digest = hashlib.sha256(
        authorization_identity.encode("utf-8")
    ).hexdigest()
    adapters = language_adapters or {}
    ordered_snapshots = sorted(
        snapshots,
        key=lambda value: canonical_json(
            {
                "coordinate": value.coordinate.json(),
                "context_identity": dict(value.identity),
                "metadata": dict(value.metadata),
            }
        ),
    )
    adapter_mode = mode in {"symbols", "references"}
    snapshot_identity = {
        "authorization_identity_sha256": authorization_digest,
        "request": {key: value for key, value in request.items() if key != "cursor"},
        "snapshots": [
            _cursor_snapshot_identity(
                snapshot,
                adapters.get(snapshot.coordinate.repository) if adapter_mode else None,
            )
            for snapshot in ordered_snapshots
        ],
    }
    offset = decode_cursor(cursor, snapshot_identity) if cursor else 0

    records: list[dict[str, object]] = []
    incomplete = False
    for snapshot in ordered_snapshots:
        _check_deadline(deadline, cancellation)
        root_identity = _safe_root(snapshot)
        _verify_snapshot(snapshot)
        if mode in {"exact", "regex"}:
            found, limited = _content_records(
                snapshot, mode, query, case_sensitive, deadline, cancellation
            )
        elif mode in {"path", "filename"}:
            found, limited = _path_records(
                snapshot, mode, query, case_sensitive, deadline, cancellation
            )
        elif mode == "history":
            if "line" in request:
                raise ContractError(
                    "INVALID_ARGUMENT", "history does not accept a line selector"
                )
            found, limited = _history_records(
                snapshot,
                query,
                case_sensitive,
                request.get("path"),
                deadline,
                cancellation,
            )
        elif mode == "blame":
            if query:
                raise ContractError(
                    "INVALID_ARGUMENT", "blame does not accept a text query"
                )
            found, limited = _blame_records(
                snapshot,
                request.get("path"),
                request.get("line"),
                deadline,
                cancellation,
            )
        else:
            found, limited = _adapter_records(
                snapshot,
                adapters.get(snapshot.coordinate.repository),
                mode,
                query,
                deadline,
                cancellation,
            )
        _verify_snapshot(snapshot)
        _check_deadline(deadline, cancellation)
        if _safe_root(snapshot) != root_identity:
            raise ContractError("STALE_COORDINATE", "snapshot root changed during search")
        records.extend(found)
        incomplete = incomplete or limited
        if len(records) > 1000:
            records = records[:1000]
            incomplete = True
            break

    ordered = sorted(records, key=canonical_json)
    if offset > len(ordered):
        raise ContractError("STALE_CURSOR", "cursor offset is outside the snapshot")
    page = ordered[offset : offset + page_size]
    next_offset = offset + len(page)
    coordinate = ordered_snapshots[0].coordinate
    observed_at = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    result: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "coordinate": coordinate.json(),
        "freshness": {
            "state": (
                "incomplete"
                if incomplete
                else (
                    "dirty"
                    if any(s.coordinate.dirty_fingerprint for s in ordered_snapshots)
                    else "clean"
                )
            ),
            "observed_at": observed_at,
        },
        "items": page,
        "pagination": {
            "limit": page_size,
            "returned_records": len(page),
            "next_cursor": (
                encode_cursor(next_offset, snapshot_identity)
                if next_offset < len(ordered)
                else None
            ),
            "snapshot": snapshot_fingerprint(snapshot_identity),
        },
        "truncation": {"bytes": False, "graph": False, "records": incomplete},
        "incomplete": incomplete,
        "failures": (
            [
                {
                    "code": "INCOMPLETE",
                    "message": "search scan reached a contract bound",
                    "retryable": False,
                    "record_id": None,
                }
            ]
            if incomplete
            else []
        ),
        "resources": [],
    }

    # Keep routine results under the stricter 32 KiB context budget without
    # skipping records: shorten this page and move its cursor to the true end.
    while len(canonical_json(result)) > routine_bytes and result["items"]:
        result["items"].pop()  # type: ignore[union-attr]
        result["truncation"]["bytes"] = True  # type: ignore[index]
    returned = len(result["items"])
    actual_next = offset + returned
    result["pagination"]["returned_records"] = returned  # type: ignore[index]
    result["pagination"]["next_cursor"] = (  # type: ignore[index]
        encode_cursor(actual_next, snapshot_identity) if actual_next < len(ordered) else None
    )
    result_bytes = len(canonical_json(result))
    enforce_shape_limits(
        query_characters=len(query),
        graph_depth=0,
        graph_edges=0,
        result_bytes=result_bytes,
        schema_depth=0,
    )
    enforce_context_budget(
        visible_tools=0, schema_bytes=0, server_instruction_bytes=0, result_bytes=result_bytes
    )
    _check_deadline(deadline, cancellation)
    for snapshot in ordered_snapshots:
        _verify_snapshot(snapshot)
        _check_deadline(deadline, cancellation)
    return result
