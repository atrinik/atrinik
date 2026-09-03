"""Concurrency-safe updates for the ignored local agent ledgers.

The two Markdown ledgers are shared workspace state, not repository evidence.
This module deliberately owns their complete read/validate/merge/publish
transaction so callers do not have to reproduce the locking and atomic-write
protocol.
"""

from __future__ import annotations

from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import re
import secrets
import stat
import subprocess
from typing import Iterator, Mapping

from .guidance_inventory import (
    PROCESS_IMPROVEMENT_LEDGER,
    PROCESS_KEY,
    PROCESS_STATUSES,
    PROCESS_TABLE_HEADER,
    PROCESS_TIMESTAMP,
    TOOLING_LEDGER_COLUMNS,
    TOOLING_LEDGER_MAX_BYTES,
    TOOLING_LEDGER_RELATIVE,
    _TOOLING_KEY,
    _TOOLING_STATUSES,
    _valid_process_timestamp,
    validate_process_improvement_ledger_text,
    validate_tooling_ledger_text,
)
from .locking import LockBusyError, exclusive_lock
from .model import WorkspaceError, _open_directory_nofollow
from .platform_compat import (
    IS_WINDOWS,
    O_BINARY,
    O_CLOEXEC,
    assert_no_symlink_components,
    flush_file,
)


AGENT_LEDGER_LOCK = Path("build/.agent-ledgers.lock")
AGENT_LEDGER_MAX_BYTES = TOOLING_LEDGER_MAX_BYTES
ABSENT_DIGEST = "absent"
_DIGEST = re.compile(r"^[0-9a-f]{64}$")


class AgentLedgerError(WorkspaceError):
    """A requested local agent-ledger transaction cannot be accepted."""


class AgentLedgerCommitUncertain(AgentLedgerError):
    """Publication happened but a durability proof failed afterward."""


@dataclass(frozen=True)
class _LedgerSpec:
    name: str
    relative: Path
    title: str
    header: str
    columns: tuple[str, ...]


@dataclass(frozen=True)
class _Snapshot:
    data: bytes | None


@dataclass(frozen=True)
class _Table:
    lines: list[str]
    row_indexes: dict[str, int]
    insert_at: int


def _spec(ledger: str) -> _LedgerSpec:
    if ledger == "process-improvements":
        return _LedgerSpec(
            name=ledger,
            relative=PROCESS_IMPROVEMENT_LEDGER,
            title="# Agent process improvements",
            header=PROCESS_TABLE_HEADER,
            columns=(
                "key",
                "status",
                "observation",
                "expected benefit / proposed action",
                "related issue / pr",
                "last observed (utc)",
            ),
        )
    if ledger == "tooling-issues":
        return _LedgerSpec(
            name=ledger,
            relative=TOOLING_LEDGER_RELATIVE,
            title="# Agent tooling issues",
            header="| Stable key | Status | Observation | Impact | Recommended action |",
            columns=TOOLING_LEDGER_COLUMNS,
        )
    raise AgentLedgerError(
        "agent-ledger: ledger must be process-improvements or tooling-issues"
    )


def ledger_path(root: Path, ledger: str) -> Path:
    """Return the only supported target path for one local ledger."""

    return Path(root) / _spec(ledger).relative


def ledger_lock_path(root: Path) -> Path:
    """Return the stable lock path shared by both replaceable ledger files."""

    return Path(root) / AGENT_LEDGER_LOCK


def _git_root_output(repository: Path, argument: str) -> Path:
    try:
        result = subprocess.run(
            ["git", "-C", str(repository), "rev-parse", argument],
            capture_output=True,
            check=False,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise AgentLedgerError(
            "agent-ledger: cannot discover the canonical shared workspace root"
        ) from error
    if result.returncode or not result.stdout.strip():
        raise AgentLedgerError(
            "agent-ledger: canonical shared workspace root is not a Git checkout"
        )
    return Path(result.stdout.strip())


def resolve_shared_root(repository: Path) -> Path:
    """Resolve a worktree or primary checkout to its shared wrapper root.

    Linked Git worktrees have a different source top-level but share the
    repository's common Git directory. Its parent is the only root accepted by
    the CLI, which keeps both ignored ledgers and the stable lock common to all
    worktrees of the wrapper repository.
    """

    candidate = Path(repository)
    try:
        assert_no_symlink_components(candidate, "agent-ledger repository")
        candidate = candidate.resolve(strict=True)
    except OSError as error:
        raise AgentLedgerError(
            f"agent-ledger: cannot inspect repository {repository}: {error}"
        ) from error
    if not candidate.is_dir():
        raise AgentLedgerError(
            f"agent-ledger: repository is not a directory: {candidate}"
        )

    top = _git_root_output(candidate, "--show-toplevel")
    common = _git_root_output(candidate, "--git-common-dir")
    if not common.is_absolute():
        common = top / common
    try:
        top = top.resolve(strict=True)
        common = common.resolve(strict=True)
    except OSError as error:
        raise AgentLedgerError(
            "agent-ledger: Git checkout identity could not be resolved"
        ) from error
    if not common.is_dir():
        raise AgentLedgerError(
            f"agent-ledger: Git common directory is not a directory: {common}"
        )
    root = common.parent
    try:
        assert_no_symlink_components(root, "agent-ledger shared root")
    except OSError as error:
        raise AgentLedgerError(
            f"agent-ledger: shared root is unsafe: {error}"
        ) from error
    wrapper = root / "atrinik"
    if not (root / "components.json").is_file() or not wrapper.is_file():
        raise AgentLedgerError(
            "agent-ledger: Git common root is not the canonical Atrinik wrapper"
        )
    if not os.access(wrapper, os.X_OK):
        raise AgentLedgerError(
            f"agent-ledger: canonical wrapper is not executable: {wrapper}"
        )
    if top == root and not (root / ".git").exists():
        raise AgentLedgerError(
            "agent-ledger: canonical wrapper checkout has no Git metadata"
        )
    return root


def _canonical_root(root: Path) -> Path:
    candidate = Path(root)
    try:
        assert_no_symlink_components(candidate, "agent-ledger root")
        candidate = candidate.resolve(strict=True)
    except OSError as error:
        raise AgentLedgerError(
            f"agent-ledger: cannot inspect shared root {root}: {error}"
        ) from error
    if not candidate.is_dir():
        raise AgentLedgerError(
            f"agent-ledger: shared root is not a directory: {candidate}"
        )
    return candidate


def _verify_local_only(root: Path, spec: _LedgerSpec) -> None:
    relative = spec.relative.as_posix()
    try:
        ignored = subprocess.run(
            ["git", "check-ignore", "--no-index", "--quiet", "--", relative],
            cwd=root,
            capture_output=True,
            check=False,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise AgentLedgerError(
            f"agent-ledger: cannot verify that {relative} is ignored"
        ) from error
    if ignored.returncode != 0:
        raise AgentLedgerError(
            f"agent-ledger: refusing non-ignored ledger path {relative}"
        )

    try:
        tracked = subprocess.run(
            ["git", "ls-files", "--error-unmatch", "--", relative],
            cwd=root,
            capture_output=True,
            check=False,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise AgentLedgerError(
            f"agent-ledger: cannot verify tracking state for {relative}"
        ) from error
    if tracked.returncode == 0:
        raise AgentLedgerError(
            f"agent-ledger: refusing tracked ledger path {relative}"
        )
    if tracked.returncode != 1:
        raise AgentLedgerError(
            f"agent-ledger: cannot verify tracking state for {relative}"
        )


def _ensure_build_directory(root: Path) -> Path:
    build = root / "build"
    try:
        assert_no_symlink_components(build, "agent-ledger build")
        build.mkdir(parents=True, exist_ok=True)
        assert_no_symlink_components(build, "agent-ledger build")
        metadata = build.stat(follow_symlinks=False)
    except OSError as error:
        raise AgentLedgerError(
            f"agent-ledger: cannot establish the shared build directory: {error}"
        ) from error
    if not stat.S_ISDIR(metadata.st_mode):
        raise AgentLedgerError(
            f"agent-ledger: shared build path is not a directory: {build}"
        )
    return build


@contextmanager
def _opened_build_directory(build: Path) -> Iterator[int | None]:
    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        yield None
        return
    flags = os.O_RDONLY | O_CLOEXEC | getattr(os, "O_DIRECTORY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    descriptor: int | None = None
    try:
        descriptor = _open_directory_nofollow(build, flags)
        _assert_directory_identity(build, descriptor)
    except (OSError, WorkspaceError) as error:
        if descriptor is not None:
            os.close(descriptor)
        raise AgentLedgerError(
            f"agent-ledger: shared build directory is unsafe: {build}: {error}"
        ) from error
    try:
        yield descriptor
    finally:
        os.close(descriptor)


def _assert_directory_identity(build: Path, directory: int) -> None:
    opened = os.fstat(directory)
    visible = build.stat(follow_symlinks=False)
    if (
        not stat.S_ISDIR(opened.st_mode)
        or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
    ):
        raise AgentLedgerError(
            f"agent-ledger: shared build directory changed during publication: {build}"
        )


def _read_limited(stream: object) -> bytes:
    chunks: list[bytes] = []
    total = 0
    while total <= AGENT_LEDGER_MAX_BYTES:
        if isinstance(stream, int):
            piece = os.read(stream, AGENT_LEDGER_MAX_BYTES + 1 - total)
        else:
            piece = stream.read(AGENT_LEDGER_MAX_BYTES + 1 - total)  # type: ignore[union-attr]
        if not piece:
            break
        chunks.append(piece)
        total += len(piece)
        if total > AGENT_LEDGER_MAX_BYTES:
            raise AgentLedgerError(
                "agent-ledger: ledger exceeds the 128 KiB size limit"
            )
    return b"".join(chunks)


def _read_snapshot(path: Path, directory: int | None) -> _Snapshot:
    if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
        opened_file = False
        try:
            assert_no_symlink_components(path, "agent-ledger")
            opened_parent = path.parent.stat(follow_symlinks=False)
            with path.open("rb") as stream:
                opened_file = True
                opened = os.fstat(stream.fileno())
                if not stat.S_ISREG(opened.st_mode):
                    raise AgentLedgerError(
                        f"agent-ledger: ledger is not a regular file: {path}"
                    )
                data = _read_limited(stream)
            visible = path.stat(follow_symlinks=False)
            visible_parent = path.parent.stat(follow_symlinks=False)
            identity = (opened.st_dev, opened.st_ino)
            if (
                not stat.S_ISREG(visible.st_mode)
                or identity != (visible.st_dev, visible.st_ino)
                or (opened_parent.st_dev, opened_parent.st_ino)
                != (visible_parent.st_dev, visible_parent.st_ino)
                or opened.st_size != len(data)
            ):
                raise AgentLedgerError(
                    f"agent-ledger: ledger changed while being read: {path}"
                )
            return _Snapshot(data)
        except FileNotFoundError:
            if not opened_file:
                return _Snapshot(None)
            raise AgentLedgerError(
                f"agent-ledger: ledger disappeared while being read: {path}"
            )
        except (AgentLedgerError, OSError) as error:
            if isinstance(error, AgentLedgerError):
                raise
            raise AgentLedgerError(
                f"agent-ledger: cannot read ledger {path}: {error}"
            ) from error

    if directory is None:  # pragma: no cover - protected by the context manager
        raise AgentLedgerError("agent-ledger: missing shared directory descriptor")
    flags = os.O_RDONLY | O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
    descriptor: int | None = None
    try:
        try:
            descriptor = os.open(path.name, flags, dir_fd=directory)
        except FileNotFoundError:
            return _Snapshot(None)
        opened = os.fstat(descriptor)
        if not stat.S_ISREG(opened.st_mode):
            raise AgentLedgerError(
                f"agent-ledger: ledger is not a regular file: {path}"
            )
        data = _read_limited(descriptor)
        visible = os.stat(path.name, dir_fd=directory, follow_symlinks=False)
        after = os.fstat(descriptor)
        identity = (opened.st_dev, opened.st_ino)
        if (
            not stat.S_ISREG(visible.st_mode)
            or identity != (visible.st_dev, visible.st_ino)
            or identity != (after.st_dev, after.st_ino)
            or after.st_size != len(data)
        ):
            raise AgentLedgerError(
                f"agent-ledger: ledger changed while being read: {path}"
            )
        return _Snapshot(data)
    except AgentLedgerError:
        raise
    except OSError as error:
        if getattr(error, "errno", None) == 2 and descriptor is None:
            return _Snapshot(None)
        raise AgentLedgerError(
            f"agent-ledger: cannot read ledger {path}: {error}"
        ) from error
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _digest(data: bytes | None) -> str | None:
    return None if data is None else hashlib.sha256(data).hexdigest()


def _validate_expected_digest(value: str | None) -> None:
    if value is not None and value != ABSENT_DIGEST and not _DIGEST.fullmatch(value):
        raise AgentLedgerError(
            "agent-ledger: expected digest must be a lowercase SHA-256 digest or absent"
        )


def _field(label: str, value: str, *, key: bool = False) -> str:
    if not isinstance(value, str):
        raise AgentLedgerError(f"agent-ledger: {label} must be text")
    cleaned = value.strip()
    if not cleaned:
        raise AgentLedgerError(f"agent-ledger: {label} must not be empty")
    if "\n" in cleaned or "\r" in cleaned or "|" in cleaned:
        raise AgentLedgerError(
            f"agent-ledger: {label} must be one Markdown-table line without pipes"
        )
    if any((ord(char) < 32 and char != "\t") or ord(char) == 127 for char in cleaned):
        raise AgentLedgerError(
            f"agent-ledger: {label} contains a control character"
        )
    if key and "`" in cleaned:
        raise AgentLedgerError(f"agent-ledger: {label} must not contain backticks")
    return cleaned


def _timestamp(value: str | None) -> str:
    if value is None:
        return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
            "+00:00", "Z"
        )
    cleaned = _field("observed-at", value)
    if not PROCESS_TIMESTAMP.fullmatch(cleaned) or not _valid_process_timestamp(cleaned):
        raise AgentLedgerError(
            "agent-ledger: observed-at must be a valid UTC timestamp ending in Z"
        )
    return cleaned


def _row_values(
    ledger: str,
    *,
    key: str,
    status: str,
    observation: str,
    expected_benefit: str | None,
    related: str,
    observed_at: str | None,
    impact: str | None,
    recommended_action: str | None,
) -> tuple[str, ...]:
    clean_key = _field("stable key", key, key=True)
    clean_status = _field("status", status)
    clean_observation = _field("observation", observation)
    if ledger == "process-improvements":
        if not PROCESS_KEY.fullmatch(clean_key):
            raise AgentLedgerError("agent-ledger: invalid process-improvement stable key")
        if clean_status.lower() not in PROCESS_STATUSES:
            raise AgentLedgerError("agent-ledger: invalid process-improvement status")
        benefit = _field("expected-benefit", expected_benefit or "")
        relation = _field("related", related)
        return (
            clean_key,
            clean_status.lower(),
            clean_observation,
            benefit,
            relation,
            _timestamp(observed_at),
        )

    if not _TOOLING_KEY.fullmatch(clean_key):
        raise AgentLedgerError("agent-ledger: invalid tooling-issue stable key")
    if clean_status not in _TOOLING_STATUSES:
        raise AgentLedgerError("agent-ledger: invalid tooling-issue status")
    return (
        clean_key,
        clean_status,
        clean_observation,
        _field("impact", impact or ""),
        _field("recommended-action", recommended_action or ""),
    )


def _line_cells(line: str) -> list[str] | None:
    stripped = line.strip()
    if not (stripped.startswith("|") and stripped.endswith("|")):
        return None
    return [cell.strip() for cell in stripped[1:-1].split("|")]


def _unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] == "`":
        return value[1:-1].strip()
    return value


def _table_for(spec: _LedgerSpec, text: str) -> _Table:
    lines = text.splitlines(keepends=True)
    logical = [line.rstrip("\r\n") for line in lines]
    if spec.name == "process-improvements":
        header_index = next(
            (index for index, line in enumerate(logical) if line.strip() == spec.header),
            None,
        )
    else:
        header_index = next(
            (
                index
                for index, line in enumerate(logical)
                if (cells := _line_cells(line))
                and [_unquote(cell).casefold() for cell in cells]
                == list(spec.columns)
            ),
            None,
        )
    if header_index is None or header_index + 1 >= len(lines):
        raise AgentLedgerError("agent-ledger: current ledger table is not canonical")
    separator = _line_cells(logical[header_index + 1])
    if separator is None or not all(re.fullmatch(r":?-{3,}:?", cell) for cell in separator):
        raise AgentLedgerError("agent-ledger: current ledger table separator is invalid")

    row_indexes: dict[str, int] = {}
    row_seen = False
    insert_at = len(lines)
    for index in range(header_index + 2, len(lines)):
        stripped = logical[index].strip()
        if not stripped or not stripped.startswith("|"):
            if row_seen:
                insert_at = index
                break
            continue
        cells = _line_cells(logical[index])
        if cells is None or len(cells) != len(spec.columns):
            raise AgentLedgerError("agent-ledger: current ledger contains a malformed row")
        key = _unquote(cells[0])
        if key in row_indexes:
            raise AgentLedgerError("agent-ledger: current ledger contains duplicate stable keys")
        row_indexes[key] = index
        row_seen = True
    if not row_seen:
        raise AgentLedgerError("agent-ledger: current ledger contains no rows")
    return _Table(lines, row_indexes, insert_at)


def _row_text(ledger: str, values: tuple[str, ...]) -> str:
    cells = [f"`{values[0]}`", *values[1:]]
    return "| " + " | ".join(cells) + " |"


def _merge(spec: _LedgerSpec, current: bytes | None, values: tuple[str, ...]) -> tuple[bytes, str]:
    if current is None:
        newline = "\n"
        if spec.name == "process-improvements":
            separator = "| --- | --- | --- | --- | --- | --- |"
        else:
            separator = "| --- | --- | --- | --- | --- |"
        text = (
            f"{spec.title}{newline}{newline}"
            f"{spec.header}{newline}{separator}{newline}"
            f"{_row_text(spec.name, values)}{newline}"
        )
        return text.encode("utf-8"), "added"

    try:
        text = current.decode("utf-8")
    except UnicodeDecodeError as error:
        raise AgentLedgerError("agent-ledger: current ledger is not UTF-8") from error
    table = _table_for(spec, text)
    newline = "\r\n" if "\r\n" in text else "\n"
    rendered = _row_text(spec.name, values)
    key = values[0]
    operation = "updated" if key in table.row_indexes else "added"
    if key in table.row_indexes:
        index = table.row_indexes[key]
        ending = (
            "\r\n"
            if table.lines[index].endswith("\r\n")
            else "\n"
            if table.lines[index].endswith("\n")
            else ""
        )
        table.lines[index] = rendered + ending
    else:
        index = table.insert_at
        if index == len(table.lines) and table.lines and not table.lines[-1].endswith(("\n", "\r")):
            table.lines[-1] += newline
        table.lines.insert(index, rendered + newline)
    result = "".join(table.lines).encode("utf-8")
    return result, operation


def _validate_candidate(spec: _LedgerSpec, data: bytes) -> None:
    if len(data) > AGENT_LEDGER_MAX_BYTES:
        raise AgentLedgerError("agent-ledger: candidate ledger exceeds the 128 KiB size limit")
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise AgentLedgerError("agent-ledger: candidate ledger is not UTF-8") from error
    failures = (
        validate_process_improvement_ledger_text(text)
        if spec.name == "process-improvements"
        else validate_tooling_ledger_text(text)
    )
    if failures:
        raise AgentLedgerError(
            "agent-ledger: candidate rejected: " + "; ".join(failures)
        )


def _atomic_publish(path: Path, directory: int | None, data: bytes) -> None:
    temporary_name = f".{path.name}.{secrets.token_hex(12)}.tmp"
    replaced = False
    descriptor: int | None = None
    try:
        if IS_WINDOWS:  # pragma: no cover - exercised by native Windows CI
            assert_no_symlink_components(path.parent, "agent-ledger")
            assert_no_symlink_components(path, "agent-ledger")
            opened_parent = path.parent.stat(follow_symlinks=False)
            descriptor = os.open(
                path.with_name(temporary_name),
                os.O_WRONLY | os.O_CREAT | os.O_EXCL | O_CLOEXEC | O_BINARY,
                0o600,
            )
            with os.fdopen(descriptor, "wb") as stream:
                descriptor = None
                stream.write(data)
                stream.flush()
                flush_file(stream.fileno())
            visible_parent = path.parent.stat(follow_symlinks=False)
            if (opened_parent.st_dev, opened_parent.st_ino) != (
                visible_parent.st_dev,
                visible_parent.st_ino,
            ):
                raise AgentLedgerError(
                    "agent-ledger: shared build directory changed during publication"
                )
            assert_no_symlink_components(path, "agent-ledger")
            os.replace(path.with_name(temporary_name), path)
            replaced = True
            assert_no_symlink_components(path, "agent-ledger")
            with path.open("r+b") as stream:
                opened = os.fstat(stream.fileno())
                visible = path.stat(follow_symlinks=False)
                if (
                    not stat.S_ISREG(opened.st_mode)
                    or (opened.st_dev, opened.st_ino) != (visible.st_dev, visible.st_ino)
                ):
                    raise OSError("published ledger identity changed")
                flush_file(stream.fileno())
            return

        if directory is None:  # pragma: no cover - protected by the context manager
            raise AgentLedgerError("agent-ledger: missing shared directory descriptor")
        flags = (
            os.O_WRONLY
            | os.O_CREAT
            | os.O_EXCL
            | O_CLOEXEC
            | getattr(os, "O_NOFOLLOW", 0)
        )
        descriptor = os.open(temporary_name, flags, 0o600, dir_fd=directory)
        with os.fdopen(descriptor, "wb") as stream:
            descriptor = None
            stream.write(data)
            stream.flush()
            flush_file(stream.fileno())
        _assert_directory_identity(path.parent, directory)
        os.replace(
            temporary_name,
            path.name,
            src_dir_fd=directory,
            dst_dir_fd=directory,
        )
        replaced = True
        try:
            _assert_directory_identity(path.parent, directory)
            os.fsync(directory)
        except OSError as error:
            raise AgentLedgerCommitUncertain(
                "agent-ledger: publication is visible but directory durability is uncertain; "
                f"inspect the latest helper output and retry: {path}: {error}"
            ) from error
    except AgentLedgerCommitUncertain:
        raise
    except BaseException as error:
        if replaced:
            raise AgentLedgerCommitUncertain(
                "agent-ledger: publication is visible but verification is uncertain; "
                f"inspect the latest helper output and retry: {path}: {error}"
            ) from error
        raise
    finally:
        if descriptor is not None:
            os.close(descriptor)
        if not replaced:
            try:
                if IS_WINDOWS:
                    path.with_name(temporary_name).unlink(missing_ok=True)
                elif directory is not None:
                    os.unlink(temporary_name, dir_fd=directory)
            except FileNotFoundError:
                pass
            except OSError:
                pass


def update_agent_ledger(
    root: Path,
    ledger: str,
    *,
    key: str,
    status: str,
    observation: str,
    expected_benefit: str | None = None,
    related: str = "none",
    observed_at: str | None = None,
    impact: str | None = None,
    recommended_action: str | None = None,
    expected_digest: str | None = None,
    nonblocking: bool = False,
) -> dict[str, object]:
    """Add or replace one stable-key row through one serialized transaction.

    With no ``expected_digest`` the operation merges the row into the latest
    bytes read under the shared lock. Passing a SHA-256 digest (or ``absent``
    for a missing target) turns the same operation into a stale-writer
    compare-and-swap that fails closed.
    """

    spec = _spec(ledger)
    _validate_expected_digest(expected_digest)
    values = _row_values(
        ledger,
        key=key,
        status=status,
        observation=observation,
        expected_benefit=expected_benefit,
        related=related,
        observed_at=observed_at,
        impact=impact,
        recommended_action=recommended_action,
    )
    canonical_root = _canonical_root(root)
    _verify_local_only(canonical_root, spec)
    build = _ensure_build_directory(canonical_root)
    path = canonical_root / spec.relative
    lock = ledger_lock_path(canonical_root)
    if lock == path:
        raise AgentLedgerError("agent-ledger: stable lock cannot be the replaceable ledger")

    try:
        with exclusive_lock(
            lock,
            "agent ledgers",
            nonblocking=nonblocking,
            inherit=False,
        ):
            _verify_local_only(canonical_root, spec)
            with _opened_build_directory(build) as directory:
                snapshot = _read_snapshot(path, directory)
                actual_digest = _digest(snapshot.data)
                actual_display = actual_digest or ABSENT_DIGEST
                if expected_digest is not None and expected_digest != actual_display:
                    raise AgentLedgerError(
                        "agent-ledger: stale digest; expected "
                        f"{expected_digest}, actual {actual_display}; reread the latest "
                        "helper output and retry"
                    )
                if snapshot.data is not None:
                    if len(snapshot.data) > AGENT_LEDGER_MAX_BYTES:
                        raise AgentLedgerError(
                            "agent-ledger: current ledger exceeds the 128 KiB size limit"
                        )
                    try:
                        current_text = snapshot.data.decode("utf-8")
                    except UnicodeDecodeError as error:
                        raise AgentLedgerError(
                            "agent-ledger: current ledger is not UTF-8; refusing to replace it"
                        ) from error
                    failures = (
                        validate_process_improvement_ledger_text(current_text)
                        if spec.name == "process-improvements"
                        else validate_tooling_ledger_text(current_text)
                    )
                    if failures:
                        raise AgentLedgerError(
                            "agent-ledger: current ledger is malformed; refusing to replace it: "
                            + "; ".join(failures)
                        )

                candidate, operation = _merge(spec, snapshot.data, values)
                _validate_candidate(spec, candidate)
                candidate_digest = hashlib.sha256(candidate).hexdigest()
                if candidate == snapshot.data:
                    return {
                        "schema_version": 1,
                        "ledger": spec.name,
                        "path": spec.relative.as_posix(),
                        "lock": AGENT_LEDGER_LOCK.as_posix(),
                        "key": values[0],
                        "operation": "unchanged",
                        "previous_digest": actual_digest,
                        "digest": candidate_digest,
                        "bytes": len(candidate),
                    }

                latest = _read_snapshot(path, directory)
                if _digest(latest.data) != actual_digest:
                    raise AgentLedgerError(
                        "agent-ledger: ledger changed while preparing the publication; "
                        "reread the latest helper output and retry"
                    )
                try:
                    _atomic_publish(path, directory, candidate)
                except AgentLedgerError:
                    raise
                except OSError as error:
                    raise AgentLedgerError(
                        "agent-ledger: atomic publication failed; the previous "
                        "ledger bytes were preserved when possible; inspect the "
                        "latest helper output and retry"
                    ) from error
                return {
                    "schema_version": 1,
                    "ledger": spec.name,
                    "path": spec.relative.as_posix(),
                    "lock": AGENT_LEDGER_LOCK.as_posix(),
                    "key": values[0],
                    "operation": operation,
                    "previous_digest": actual_digest,
                    "digest": candidate_digest,
                    "bytes": len(candidate),
                }
    except LockBusyError as error:
        raise AgentLedgerError(
            "agent-ledger: shared lock is busy; retry after reading the latest helper output"
        ) from error


def update_from_mapping(
    root: Path,
    ledger: str,
    row: Mapping[str, str],
    *,
    expected_digest: str | None = None,
    nonblocking: bool = False,
) -> dict[str, object]:
    """Convenience adapter for callers that already have a named row mapping."""

    allowed = {
        "key",
        "status",
        "observation",
        "expected_benefit",
        "related",
        "observed_at",
        "impact",
        "recommended_action",
    }
    unknown = set(row) - allowed
    if unknown:
        raise AgentLedgerError(
            "agent-ledger: unsupported row fields: " + ", ".join(sorted(unknown))
        )
    required = {"key", "status", "observation"} - set(row)
    if required:
        raise AgentLedgerError(
            "agent-ledger: row is missing required fields: "
            + ", ".join(sorted(required))
        )
    return update_agent_ledger(
        root,
        ledger,
        key=row["key"],
        status=row["status"],
        observation=row["observation"],
        expected_benefit=row.get("expected_benefit"),
        related=row.get("related", "none"),
        observed_at=row.get("observed_at"),
        impact=row.get("impact"),
        recommended_action=row.get("recommended_action"),
        expected_digest=expected_digest,
        nonblocking=nonblocking,
    )
