#!/usr/bin/env python3
"""Report whether this process is an authoritative delivery coordinator.

The delivery ledger intentionally remains a POSIX-only implementation.  This
probe is therefore deliberately standalone: it performs bounded, read-only
inspection and never imports the ledger, takes a lock, mounts a filesystem, or
creates a directory.  A successful result is the conjunction of the pinned
workspace contract and live facts; an environment marker by itself is never
authorization.  The result also reports whether the live process appears to
be an existing VS Code plugin session, a container bootstrap, or a native host;
that entry-mode field is diagnostic only.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import platform
import re
import stat
from typing import Mapping


SCHEMA_VERSION = 2
CANONICAL_STATUS = "canonical-linux"
NATIVE_WINDOWS_STATUS = "native-windows"
WINDOWS_CROSS_STATUS = "windows-cross"
UNKNOWN_STATUS = "unknown-or-unsafe"

ENTRY_MODE_VSCODE = "inside-vscode-devcontainer"
ENTRY_MODE_CONTAINER = "container-bootstrap"
ENTRY_MODE_NATIVE_HOST = "native-host"
ENTRY_MODE_UNKNOWN = "unknown"

CANONICAL_IMAGE = (
    "ghcr.io/atrinik/linux-build:1.10.0@sha256:"
    "7904a1802054662b0ede5b55de72e4c92b0112a3c211125f994ed6c62e9ec9d8"
)
WINDOWS_CROSS_IMAGE = (
    "ghcr.io/atrinik/windows-build:1.2.1@sha256:"
    "d1f082eb28891600a9cf018a1d4310b9f3e1f985f82139fa48fbd4ac77b623bb"
)
CANONICAL_WORKSPACE_FOLDER = "/workspaces/atrinik"
CANONICAL_REMOTE_USER = "ubuntu"
CANONICAL_HOME = "/home/ubuntu"
CANONICAL_CODEX_HOME = "/home/ubuntu/.codex"
WINDOWS_CROSS_REMOTE_USER = "vscode"
MXE_PATH = "/opt/mxe/usr/bin"

MAX_CONFIG_BYTES = 128 * 1024
MAX_COMPONENTS_BYTES = 4 * 1024 * 1024
MAX_MOUNTINFO_BYTES = 4 * 1024 * 1024
MAX_FAILED_CHECKS = 24
DEFAULT_MOUNTINFO = Path("/proc/1/mountinfo")

# Docker Desktop and other host-filesystem bridges do not provide the Linux
# ownership and descriptor semantics required by the ledger.  ext4, overlay,
# tmpfs, xfs, btrfs, and other native Linux or Docker-volume filesystems remain
# eligible when their live mode/owner checks pass.
UNSAFE_MOUNT_TYPES = frozenset(
    {
        "9p",
        "cifs",
        "drvfs",
        "fuseblk",
        "smb3",
        "vboxsf",
        "virtiofs",
    }
)


class ProbeError(Exception):
    """A stable, secret-free probe failure code."""


@dataclass(frozen=True)
class MountInfo:
    filesystem: str


def _strip_jsonc(text: str) -> str:
    """Remove JSONC comments without changing string contents."""

    result: list[str] = []
    in_string = False
    escaped = False
    line_comment = False
    block_comment = False
    index = 0
    while index < len(text):
        character = text[index]
        next_character = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if character in "\r\n":
                line_comment = False
                result.append(character)
            else:
                result.append(" ")
        elif block_comment:
            if character == "*" and next_character == "/":
                block_comment = False
                result.extend((" ", " "))
                index += 1
            elif character in "\r\n":
                result.append(character)
            else:
                result.append(" ")
        elif in_string:
            result.append(character)
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                in_string = False
        elif character == '"':
            in_string = True
            result.append(character)
        elif character == "/" and next_character == "/":
            line_comment = True
            result.extend((" ", " "))
            index += 1
        elif character == "/" and next_character == "*":
            block_comment = True
            result.extend((" ", " "))
            index += 1
        else:
            result.append(character)
        index += 1
    if block_comment:
        raise ProbeError("invalid-jsonc-comment")
    return "".join(result)


def _absolute(path: Path) -> Path:
    candidate = Path(os.fspath(path))
    if not candidate.is_absolute():
        candidate = Path.cwd() / candidate
    if any(part in {".", ".."} for part in candidate.parts):
        raise ProbeError("non-canonical-path")
    return candidate


def _lstat_no_follow(
    path: Path,
    label: str,
    *,
    allow_missing: bool = False,
) -> os.stat_result | None:
    """Inspect every existing path component without following symlinks."""

    candidate = _absolute(path)
    current = Path(candidate.anchor)
    for part in candidate.parts[1:]:
        current /= part
        try:
            status = current.lstat()
        except FileNotFoundError:
            if allow_missing:
                return None
            raise ProbeError(f"missing-{label}") from None
        except OSError:
            raise ProbeError(f"unreadable-{label}") from None
        if stat.S_ISLNK(status.st_mode):
            raise ProbeError(f"unsafe-{label}-symlink")
    try:
        return candidate.lstat()
    except FileNotFoundError:
        if allow_missing:
            return None
        raise ProbeError(f"missing-{label}") from None
    except OSError:
        raise ProbeError(f"unreadable-{label}") from None


def _safe_directory(
    path: Path,
    label: str,
    uid: int,
    failures: list[str],
    *,
    allow_missing: bool = False,
) -> os.stat_result | None:
    try:
        status = _lstat_no_follow(path, label, allow_missing=allow_missing)
    except ProbeError as error:
        failures.append(str(error))
        return None
    if status is None:
        return None
    if not stat.S_ISDIR(status.st_mode):
        failures.append(f"{label}-not-directory")
    if status.st_mode & 0o022:
        failures.append(f"unsafe-{label}-mode")
    if status.st_uid != uid:
        failures.append(f"unsafe-{label}-owner")
    return status


def _safe_regular(
    path: Path,
    label: str,
    uid: int,
    failures: list[str],
    *,
    allow_missing: bool = False,
    check_owner: bool = True,
) -> os.stat_result | None:
    try:
        status = _lstat_no_follow(path, label, allow_missing=allow_missing)
    except ProbeError as error:
        failures.append(str(error))
        return None
    if status is None:
        return None
    if not stat.S_ISREG(status.st_mode):
        failures.append(f"{label}-not-regular")
    if check_owner and status.st_mode & 0o022:
        failures.append(f"unsafe-{label}-mode")
    if check_owner and status.st_uid != uid:
        failures.append(f"unsafe-{label}-owner")
    return status


def _read_bounded(path: Path, limit: int, label: str) -> str:
    descriptor = -1
    try:
        flags = os.O_RDONLY | getattr(os, "O_CLOEXEC", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        descriptor = os.open(os.fspath(path), flags)
        with os.fdopen(descriptor, "rb") as stream:
            descriptor = -1
            data = stream.read(limit + 1)
    except (OSError, UnicodeError):
        raise ProbeError(f"unreadable-{label}") from None
    finally:
        if descriptor != -1:
            os.close(descriptor)
    if len(data) > limit:
        raise ProbeError(f"oversized-{label}")
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        raise ProbeError(f"invalid-{label}-encoding") from None


def _load_jsonc(
    path: Path,
    label: str,
    uid: int,
    failures: list[str],
    *,
    limit: int,
) -> object | None:
    if _safe_regular(path, label, uid, failures) is None:
        return None
    try:
        text = _read_bounded(path, limit, label)
        return json.loads(_strip_jsonc(text))
    except (ProbeError, json.JSONDecodeError, UnicodeError):
        failures.append(f"invalid-{label}")
        return None


def _unescape_mount_field(value: str) -> str:
    return re.sub(
        r"\\([0-7]{3})",
        lambda match: chr(int(match.group(1), 8)),
        value,
    )


def _read_mountinfo(path: Path) -> dict[str, MountInfo]:
    # /proc/1 avoids the /proc/self symlink while retaining the current mount
    # namespace.  The file is virtual and is intentionally read-only here.
    text = _read_bounded(path, MAX_MOUNTINFO_BYTES, "mountinfo")
    return _parse_mountinfo(text)


def _parse_mountinfo(text: str) -> dict[str, MountInfo]:
    mounts: dict[str, MountInfo] = {}
    for line in text.splitlines():
        prefix, separator, suffix = line.partition(" - ")
        if not separator:
            continue
        fields = prefix.split()
        filesystem_fields = suffix.split()
        if len(fields) < 5 or len(filesystem_fields) < 2:
            continue
        target = _unescape_mount_field(fields[4])
        if not target.startswith("/"):
            continue
        mounts[target] = MountInfo(
            filesystem=filesystem_fields[0].lower(),
        )
    return mounts


def _mount_for_path(
    path: Path, mounts: Mapping[str, MountInfo]
) -> MountInfo | None:
    candidate = _absolute(path)
    for parent in (candidate, *candidate.parents):
        identity = mounts.get(parent.as_posix())
        if identity is not None:
            return identity
    return None


def _check_mount(
    path: Path,
    label: str,
    status: os.stat_result | None,
    mounts: Mapping[str, MountInfo],
    failures: list[str],
    *,
    require_exact: bool = False,
) -> None:
    if status is None:
        return
    try:
        identity = (
            mounts.get(_absolute(path).as_posix())
            if require_exact
            else _mount_for_path(path, mounts)
        )
    except ProbeError:
        identity = None
    if identity is None:
        failures.append(f"missing-{label}-mount")
        return
    if identity.filesystem in UNSAFE_MOUNT_TYPES:
        failures.append(f"unsafe-{label}-mount")


def _marker_present(runtime_root: Path, failures: list[str]) -> bool:
    for relative, label in (
        (Path(".dockerenv"), "dockerenv"),
        (Path("run/.containerenv"), "containerenv"),
    ):
        try:
            status = _lstat_no_follow(
                runtime_root / relative, label, allow_missing=True
            )
        except ProbeError as error:
            failures.append(str(error))
            continue
        if status is not None:
            if stat.S_ISREG(status.st_mode):
                return True
            failures.append(f"{label}-not-regular")
    return False


def _current_user() -> str | None:
    try:
        import pwd
    except ImportError:
        return None
    try:
        return pwd.getpwuid(os.geteuid()).pw_name
    except (KeyError, OSError):
        return None


def _posix_locking_available() -> bool:
    """Check the primitive used by the ledger without locking anything."""

    try:
        locking = __import__("fcntl")
    except (ImportError, OSError):
        return False
    return callable(getattr(locking, "flock", None))


def _mxe_signals(
    environment: Mapping[str, str], user_name: str | None
) -> list[str]:
    signals: list[str] = []
    if user_name == WINDOWS_CROSS_REMOTE_USER:
        signals.append("windows-cross-remote-user")
    if environment.get("HOME") == "/home/vscode":
        signals.append("windows-cross-home")
    if MXE_PATH in environment.get("PATH", "").split(os.pathsep):
        signals.append("mxe-toolchain-present")
    if environment.get("DEVCONTAINER_IMAGE") == WINDOWS_CROSS_IMAGE:
        signals.append("windows-cross-image")
    return signals


def _entry_mode(
    environment: Mapping[str, str],
    *,
    host: str,
    runtime_marker: bool | None = None,
) -> str:
    """Classify the reported entry signal without granting authority.

    The VS Code and Dev Containers variables are process-environment hints.
    They are deliberately not required for a direct Docker attach and are
    never sufficient for a canonical result.  The live container marker is
    only used to distinguish a proven host-side handoff from a container
    signal that failed its runtime checks.
    """

    if host == "Windows":
        return ENTRY_MODE_NATIVE_HOST
    if runtime_marker is False:
        return ENTRY_MODE_NATIVE_HOST
    if (
        environment.get("REMOTE_CONTAINERS") == "true"
        or environment.get("TERM_PROGRAM") == "vscode"
    ):
        return ENTRY_MODE_VSCODE
    if environment.get("DEVCONTAINER") == "true":
        return ENTRY_MODE_CONTAINER
    if runtime_marker is True:
        return ENTRY_MODE_CONTAINER
    return ENTRY_MODE_UNKNOWN


def _result(
    status: str,
    authoritative: bool,
    failures: list[str],
    next_action: str,
    diagnostic: str,
    *,
    entry_mode: str = ENTRY_MODE_UNKNOWN,
) -> dict[str, object]:
    bounded = sorted(set(failures))[:MAX_FAILED_CHECKS]
    return {
        "authoritative": authoritative,
        "diagnostic": diagnostic,
        "entry_mode": entry_mode,
        "failed_checks": bounded,
        "next_action": next_action,
        "schema_version": SCHEMA_VERSION,
        "status": status,
    }


# Native support is a host-user contract, not remote attestation. A trusted
# administrator can construct a container/chroot that resembles a host.
NATIVE_LINUX_STATUS = "native-linux"
NATIVE_DISTRIBUTIONS = {"ubuntu": {"24.04", "26.04"}, "debian": {"12", "13"}}
NATIVE_FILESYSTEMS = frozenset({"ext4", "xfs", "btrfs", "zfs", "tmpfs"})


def _native_open(path: Path, uid: int, *, directory: bool = False) -> int:
    """Pin every ancestor and require trusted ownership/modes without links."""
    candidate = _absolute(path)
    descriptor = os.open(candidate.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        anchor = os.fstat(descriptor)
        if anchor.st_uid not in (0, uid) or anchor.st_mode & 0o022:
            raise ProbeError("native-path-owner-or-mode")
        if not stat.S_ISDIR(anchor.st_mode):
            raise ProbeError("native-path-type")
        for index, part in enumerate(candidate.parts[1:]):
            last = index == len(candidate.parts) - 2
            flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC
            if not last or directory:
                flags |= os.O_DIRECTORY
            child = os.open(part, flags, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
            info = os.fstat(descriptor)
            if info.st_uid not in (0, uid) or info.st_mode & 0o022:
                raise ProbeError("native-path-owner-or-mode")
            if not (stat.S_ISDIR(info.st_mode) if not last or directory else stat.S_ISREG(info.st_mode)):
                raise ProbeError("native-path-type")
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def _native_read(path: Path, uid: int, limit: int = 16384) -> str:
    descriptor = _native_open(path, uid)
    with os.fdopen(descriptor, "rb") as stream:
        before = os.fstat(stream.fileno())
        data = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
        if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            raise ProbeError("native-input-changed")
    if len(data) > limit:
        raise ProbeError("native-input-oversized")
    return data.decode("utf-8")


def _native_probe(
    root: Path, environment: Mapping[str, str], uid: int, failures: list[str],
    *, runtime_root: Path, cwd: Path | None,
) -> dict[str, object]:
    """Recognize an ordinary unprivileged systemd Linux host.

    Kernel/procfs, installed OS and host administrator are trusted. Initial
    user mappings reject ordinary rootless namespace ownership forgery.
    This does not attest PID1 executable bytes or exclude admin-created chroots.
    """
    try:
        import pwd

        account = pwd.getpwuid(uid)
        for relative in ("components.json", ".devcontainer/devcontainer.json",
                         ".devcontainer/windows-cross/devcontainer.json"):
            # Common configuration validation above must also have trusted
            # descriptor-pinned ancestors on a direct native host.
            _native_read(root / relative, uid, MAX_COMPONENTS_BYTES)
        if uid == 0:
            failures.append("native-root-user")
        home = Path(account.pw_dir)
        if environment.get("HOME") != str(home):
            failures.append("native-passwd-home")
        codex = Path(environment.get("CODEX_HOME", str(home / ".codex")))
        if not codex.is_absolute():
            failures.append("native-codex-path")
        if not _absolute(cwd or Path.cwd()).is_relative_to(root):
            failures.append("current-directory-outside-repository")
        if environment.get("ATRINIK_COORDINATOR_NESTED") == "true" or environment.get("ATRINIK_COORDINATOR_DEPTH", "0") != "0":
            failures.append("nested-coordinator")
        if environment.get("DEVCONTAINER_IMAGE") or environment.get("DEVCONTAINER") == "true":
            failures.append("native-container-environment-mismatch")
        if not _posix_locking_available():
            failures.append("posix-locking-unavailable")

        # Read live caller mountinfo, not PID1's potentially different namespace.
        proc = runtime_root / "proc"
        caller = proc / str(os.getpid())
        mount_path = caller / "mountinfo"
        mount_text = _native_read(mount_path, uid, MAX_MOUNTINFO_BYTES)
        mounts = _parse_mountinfo(mount_text)
        proc_identity = _mount_for_path(proc, mounts)
        proc_fd = _native_open(proc, 0, directory=True)
        try:
            proc_info = os.fstat(proc_fd)
        finally:
            os.close(proc_fd)
        if proc_identity is None or proc_identity.filesystem != "proc":
            failures.append("native-procfs")
        else:
            _check_mount(proc, "native-procfs", proc_info, mounts, failures)
        for process in (caller, proc / "1"):
            for field in ("uid_map", "gid_map"):
                rows = _native_read(process / field, uid).splitlines()
                if len(rows) != 1 or rows[0].split() != ["0", "0", "4294967295"]:
                    failures.append("native-initial-user-namespace")
        status = _native_read(proc / "1/status", 0)
        fields = dict(line.split(":", 1) for line in status.splitlines() if ":" in line)
        if fields.get("Name", "").strip() != "systemd" or fields.get("Uid", "").split() != ["0"] * 4:
            failures.append("native-systemd-init")
        # /proc/1/exe and namespace links may be ptrace-denied to normal users.
        # Do not demand elevated permissions merely to read those links.
        kernel = _native_read(proc / "sys/kernel/osrelease", 0).lower()
        if "microsoft" in kernel or "wsl" in kernel:
            failures.append("native-wsl-host-boundary")
        os_release = _native_read(runtime_root / "usr/lib/os-release", 0)
        distribution: dict[str, str] = {}
        for line in os_release.splitlines():
            if line.startswith(("ID=", "VERSION_ID=")):
                key, value = line.split("=", 1)
                if key in distribution:
                    raise ProbeError("native-distribution-duplicate")
                if not re.fullmatch(r'[A-Za-z0-9._-]+|"[A-Za-z0-9._-]+"', value):
                    raise ProbeError("native-distribution-value")
                distribution[key] = value[1:-1] if value.startswith('"') else value
        if distribution.get("VERSION_ID") not in NATIVE_DISTRIBUTIONS.get(distribution.get("ID", ""), set()):
            failures.append("native-distribution-unsupported")
        systemd = _native_open(runtime_root / "usr/lib/systemd/systemd", 0)
        try:
            if not os.fstat(systemd).st_mode & 0o111:
                failures.append("native-systemd-installation")
        finally:
            os.close(systemd)
        systemd_runtime = _native_open(runtime_root / "run/systemd/system", 0, directory=True)
        os.close(systemd_runtime)
        if _lstat_no_follow(runtime_root / "run/systemd/container", "native-container-declaration", allow_missing=True) is not None:
            failures.append("native-container-declaration")
        for path, label in ((runtime_root, "native-root"), (root, "repository-root"),
                            (home, "native-home"), (codex, "codex-home")):
            descriptor = _native_open(path, uid, directory=True)
            try:
                info = os.fstat(descriptor)
                if label in {"repository-root", "native-home", "codex-home"} and info.st_uid != uid:
                    failures.append("native-user-directory-owner")
                if label == "codex-home" and info.st_mode & 0o077:
                    failures.append("unsafe-codex-home-mode")
                identity = _mount_for_path(path, mounts)
                if identity is None or identity.filesystem not in NATIVE_FILESYSTEMS:
                    failures.append("native-filesystem-unsupported")
                _check_mount(path, label, info, mounts, failures)
            finally:
                os.close(descriptor)
        for relative in ("workspace", "build", "build/reviews"):
            path = root / relative
            try:
                descriptor = _native_open(path, uid, directory=True)
            except FileNotFoundError:
                # The helper creates missing descendants only after its own
                # full pinned-parent proof; absence grants no reuse authority.
                continue
            try:
                info = os.fstat(descriptor)
                if info.st_uid != uid:
                    failures.append("native-mutable-directory-owner")
                identity = _mount_for_path(path, mounts)
                if identity is None or identity.filesystem not in NATIVE_FILESYSTEMS:
                    failures.append("native-filesystem-unsupported")
                _check_mount(path, relative.replace("/", "-"), info, mounts, failures)
            finally:
                os.close(descriptor)
    except (OSError, KeyError, ValueError, UnicodeError, ProbeError):
        failures.append("native-host-proof-unavailable-or-unsafe")
    if failures:
        return _result(UNKNOWN_STATUS, False, failures,
                       "Use a supported systemd Ubuntu/Debian host with trusted native filesystems, "
                       "your passwd home and private Codex directory, or attach the pinned Linux container; "
                       "rerun the probe and exact ledger/worktree gates.",
                       "native-linux-contract-failed", entry_mode=ENTRY_MODE_NATIVE_HOST)
    return _result(NATIVE_LINUX_STATUS, True, [],
                   "Native Linux context proved; continue with authenticated ledger, dedicated worktree "
                   "and exact ownership/CAS/lease gates.",
                   "native-linux-coordinator", entry_mode=ENTRY_MODE_NATIVE_HOST)


def probe(
    root: Path,
    *,
    system: str | None = None,
    environment: Mapping[str, str] | None = None,
    user_name: str | None = None,
    cwd: Path | None = None,
    workspace_folder: Path | None = None,
    codex_home: Path | None = None,
    runtime_root: Path = Path("/"),
    mountinfo: Path = DEFAULT_MOUNTINFO,
    effective_uid: int | None = None,
) -> dict[str, object]:
    """Return a stable context record without changing any filesystem state."""

    host = system or platform.system()
    environment = dict(os.environ if environment is None else environment)
    if host == "Windows":
        return _result(
            NATIVE_WINDOWS_STATUS,
            False,
            ["posix-ledger-primitives", "native-host-boundary"],
            "Bootstrap or attach to the pinned Atrinik Linux devcontainer before "
            "any delivery-ledger mutation.",
            "native-windows-boundary",
            entry_mode=ENTRY_MODE_NATIVE_HOST,
        )
    if host != "Linux":
        return _result(
            UNKNOWN_STATUS,
            False,
            ["unsupported-host-platform", "posix-ledger-primitives"],
            "Run this probe from the pinned Atrinik Linux devcontainer; a native "
            "host must bootstrap or attach before delivery.",
            "unknown-or-unsupported-context",
            entry_mode=ENTRY_MODE_UNKNOWN,
        )

    user_name = _current_user() if user_name is None else user_name
    uid = os.geteuid() if effective_uid is None else effective_uid
    failures: list[str] = []
    try:
        repository_root = _absolute(root)
    except ProbeError as error:
        return _result(
            UNKNOWN_STATUS,
            False,
            [str(error)],
            "Run this probe from the pinned Atrinik Linux devcontainer; a native "
            "host must bootstrap or attach before delivery.",
            "unknown-or-unsupported-context",
            entry_mode=_entry_mode(environment, host=host),
        )

    runtime_marker = _marker_present(runtime_root, failures)
    role_signals = _mxe_signals(environment, user_name)
    if not runtime_marker:
        role_signals = [signal for signal in role_signals
                        if signal not in {"windows-cross-remote-user", "windows-cross-home"}]
    if role_signals:
        return _result(
            WINDOWS_CROSS_STATUS,
            False,
            ["windows-cross-context", *role_signals],
            "Use the ordinary pinned Linux devcontainer as the delivery "
            "coordinator; keep windows-cross for host-bound package/build work.",
            "subordinate-windows-cross-context",
            entry_mode=_entry_mode(environment, host=host),
        )

    root_status = _safe_directory(repository_root, "repository-root", uid, failures)
    workspace_status = None
    if root_status is not None:
        try:
            git_status = _lstat_no_follow(repository_root / ".git", "git-metadata")
            if git_status is None or not (
                stat.S_ISREG(git_status.st_mode) or stat.S_ISDIR(git_status.st_mode)
            ):
                failures.append("git-metadata-not-usable")
        except ProbeError as error:
            failures.append(str(error))

    components = _load_jsonc(
        repository_root / "components.json",
        "components-manifest",
        uid,
        failures,
        limit=MAX_COMPONENTS_BYTES,
    )
    if not isinstance(components, dict) or components.get("schema_version") != 3:
        failures.append("components-manifest-schema")

    canonical_config = _load_jsonc(
        repository_root / ".devcontainer/devcontainer.json",
        "canonical-devcontainer-config",
        uid,
        failures,
        limit=MAX_CONFIG_BYTES,
    )
    windows_config = _load_jsonc(
        repository_root / ".devcontainer/windows-cross/devcontainer.json",
        "windows-cross-devcontainer-config",
        uid,
        failures,
        limit=MAX_CONFIG_BYTES,
    )
    if not isinstance(canonical_config, dict):
        canonical_config = {}
    if not isinstance(windows_config, dict):
        windows_config = {}

    if canonical_config.get("image") != CANONICAL_IMAGE:
        failures.append("canonical-image-pin")
    if canonical_config.get("workspaceFolder") != CANONICAL_WORKSPACE_FOLDER:
        failures.append("canonical-workspace-folder")
    if canonical_config.get("remoteUser") != CANONICAL_REMOTE_USER:
        failures.append("canonical-remote-user")
    if canonical_config.get("updateRemoteUserUID") is not True:
        failures.append("canonical-uid-update")
    if canonical_config.get("postCreateCommand") != "./atrinik init":
        failures.append("canonical-init-command")
    container_env = canonical_config.get("containerEnv")
    if not isinstance(container_env, dict) or container_env.get(
        "CODEX_HOME"
    ) != CANONICAL_CODEX_HOME:
        failures.append("canonical-codex-home")
    configured_mounts = canonical_config.get("mounts")
    if not isinstance(configured_mounts, list) or not any(
        isinstance(value, str)
        and re.search(r"(?:^|,)target=/home/ubuntu/\.codex(?:,|$)", value)
        for value in configured_mounts
    ):
        failures.append("canonical-codex-mount")

    if windows_config.get("image") != WINDOWS_CROSS_IMAGE:
        failures.append("windows-cross-image-reference")
    if windows_config.get("remoteUser") != WINDOWS_CROSS_REMOTE_USER:
        failures.append("windows-cross-remote-user-reference")
    windows_remote_env = windows_config.get("remoteEnv")
    if not isinstance(windows_remote_env, dict) or windows_remote_env.get(
        "PATH"
    ) != "/opt/mxe/usr/bin:${containerEnv:PATH}":
        failures.append("windows-cross-mxe-reference")

    if not _marker_present(runtime_root, failures):
        return _native_probe(repository_root, environment, uid, failures,
                             runtime_root=runtime_root, cwd=cwd)

    configured_workspace = canonical_config.get("workspaceFolder")
    try:
        actual_workspace = _absolute(
            workspace_folder
            or Path(
                configured_workspace
                if isinstance(configured_workspace, str)
                else CANONICAL_WORKSPACE_FOLDER
            )
        )
    except ProbeError as error:
        failures.append(str(error))
        actual_workspace = repository_root
    try:
        actual_codex_home = _absolute(codex_home or Path(CANONICAL_CODEX_HOME))
    except ProbeError as error:
        failures.append(str(error))
        actual_codex_home = repository_root
    workspace_status = _safe_directory(
        actual_workspace, "workspace-folder", uid, failures
    )
    workspace_data_status = _safe_directory(
        actual_workspace / "workspace", "workspace-data", uid, failures
    )
    build_status = _safe_directory(
        actual_workspace / "build", "build-root", uid, failures, allow_missing=True
    )
    ledger_path = actual_workspace / "build/reviews"
    ledger_status = _safe_directory(
        ledger_path, "ledger-root", uid, failures, allow_missing=True
    )
    if ledger_status is None:
        ledger_status = build_status or workspace_status
    codex_status = _safe_directory(actual_codex_home, "codex-home", uid, failures)
    if codex_status is not None and stat.S_IMODE(codex_status.st_mode) & 0o077:
        failures.append("unsafe-codex-home-mode")

    try:
        current_directory = _absolute(cwd or Path.cwd())
        if not current_directory.is_relative_to(actual_workspace):
            failures.append("current-directory-outside-workspace")
        if not repository_root.is_relative_to(actual_workspace):
            failures.append("repository-root-outside-workspace")
        if not current_directory.is_relative_to(repository_root):
            failures.append("current-directory-outside-repository")
    except ProbeError as error:
        failures.append(str(error))

    if environment.get("HOME") != CANONICAL_HOME:
        failures.append("runtime-home")
    if environment.get("CODEX_HOME") != CANONICAL_CODEX_HOME:
        failures.append("runtime-codex-home")
    if user_name != CANONICAL_REMOTE_USER:
        failures.append("runtime-user")
    if not _posix_locking_available():
        failures.append("posix-locking-unavailable")
    if environment.get("ATRINIK_COORDINATOR_NESTED") == "true":
        failures.append("nested-coordinator")
    try:
        coordinator_depth = int(environment.get("ATRINIK_COORDINATOR_DEPTH", "0"))
    except (TypeError, ValueError):
        coordinator_depth = 1
    if coordinator_depth != 0:
        failures.append("nested-coordinator")
    configured_runtime_image = environment.get("DEVCONTAINER_IMAGE")
    if configured_runtime_image and configured_runtime_image != CANONICAL_IMAGE:
        failures.append("runtime-image-mismatch")
    runtime_marker = _marker_present(runtime_root, failures)
    if not runtime_marker:
        failures.append("container-runtime-marker")
    entry_mode = _entry_mode(
        environment, host=host, runtime_marker=runtime_marker
    )

    try:
        mounts = _read_mountinfo(mountinfo)
    except ProbeError as error:
        failures.append(str(error))
        mounts = {}
    _check_mount(
        actual_workspace,
        "workspace-folder",
        workspace_status,
        mounts,
        failures,
        require_exact=True,
    )
    _check_mount(
        actual_workspace / "workspace",
        "workspace-data",
        workspace_data_status,
        mounts,
        failures,
    )
    _check_mount(repository_root, "repository-root", root_status, mounts, failures)
    _check_mount(
        actual_codex_home,
        "codex-home",
        codex_status,
        mounts,
        failures,
        require_exact=True,
    )
    _check_mount(
        ledger_path if ledger_status is not None else actual_workspace,
        "ledger-root",
        ledger_status,
        mounts,
        failures,
    )

    if failures:
        next_action = (
            "Use the pinned Atrinik Linux devcontainer or the current proven "
            "in-container session, then rerun this probe before ledger mutation."
        )
        if "runtime-image-mismatch" in failures:
            next_action = (
                f"Rebuild or attach your owned session using {CANONICAL_IMAGE}; "
                "follow README.md's Linux image upgrades instructions, preserve "
                "worktrees and state, and rerun this probe before ledger mutation."
            )
        return _result(
            UNKNOWN_STATUS,
            False,
            failures,
            next_action,
            "unknown-or-unsupported-context",
            entry_mode=entry_mode,
        )
    return _result(
        CANONICAL_STATUS,
        True,
        [],
        "This session is authoritative for the Atrinik delivery ledger; continue "
        "with the ledger and wrapper gates.",
        "canonical-coordinator",
        entry_mode=entry_mode,
    )


def _default_root() -> Path:
    return Path(__file__).absolute().parent.parent


def _render_human(result: Mapping[str, object]) -> str:
    failures = result["failed_checks"]
    failure_text = (
        "none" if not failures else ", ".join(str(value) for value in failures)
    )
    return "\n".join(
        (
            f"atrinik coordinator: {result['status']} "
            f"(authoritative={str(result['authoritative']).lower()})",
            f"entry mode: {result['entry_mode']}",
            f"failed checks: {failure_text}",
            f"next action: {result['next_action']}",
        )
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Probe the read-only Atrinik delivery coordinator context"
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=_default_root(),
        help="repository/worktree root to inspect (defaults to this script's "
        "checkout)",
    )
    parser.add_argument("--json", action="store_true", help="emit stable JSON")
    args = parser.parse_args(argv)

    try:
        result = probe(args.root)
    except (OSError, ProbeError, TypeError, ValueError):
        result = _result(
            UNKNOWN_STATUS,
            False,
            ["probe-failed-closed"],
            "Run this probe from the pinned Atrinik Linux devcontainer before "
            "ledger mutation.",
            "unknown-or-unsupported-context",
            entry_mode=ENTRY_MODE_UNKNOWN,
        )
    if args.json:
        print(json.dumps(result, sort_keys=True))
    else:
        print(_render_human(result))
    return 0 if result["authoritative"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
