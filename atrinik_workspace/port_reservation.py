from __future__ import annotations

import fcntl
import json
import os
from pathlib import Path
import re
import stat
import time
from typing import Any


PORT_RESERVATION_SCHEMA_VERSION = 1
PORT_RESERVATION_DIRECTORY = "port-reservations"
PORT_RESERVATION_KEYS = {
    "schema_version",
    "port",
    "topology",
    "generation",
    "path",
    "lease",
}
GENERATION_PATTERN = re.compile(r"[0-9a-f]{64}")
TOPOLOGY_PATTERN = re.compile(r"[a-z0-9][a-z0-9._-]*")
MAX_RECORD_SIZE = 4096


class PortReservationError(RuntimeError):
    """A topology UDP-port reservation cannot be trusted or acquired."""


def _validate_port(port: Any) -> int:
    if (
        not isinstance(port, int)
        or isinstance(port, bool)
        or not 1 <= port <= 65535
    ):
        raise PortReservationError("reserved topology port must be between 1 and 65535")
    return port


def _validate_lease_identity(value: Any) -> dict[str, int]:
    if (
        not isinstance(value, dict)
        or set(value) != {"device", "inode"}
        or not all(
            isinstance(item, int) and not isinstance(item, bool) and item >= 0
            for item in value.values()
        )
    ):
        raise PortReservationError("topology port reservation lease identity is invalid")
    return value


def validate_record(value: Any, *, expected_path: Path | None = None) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != PORT_RESERVATION_KEYS:
        raise PortReservationError("topology port reservation record is invalid")
    if (
        not isinstance(value.get("schema_version"), int)
        or isinstance(value.get("schema_version"), bool)
        or value["schema_version"] != PORT_RESERVATION_SCHEMA_VERSION
    ):
        raise PortReservationError("topology port reservation schema is unsupported")
    port = _validate_port(value.get("port"))
    topology = value.get("topology")
    generation = value.get("generation")
    path = value.get("path")
    if not isinstance(topology, str) or not TOPOLOGY_PATTERN.fullmatch(topology):
        raise PortReservationError("topology port reservation owner is invalid")
    if not isinstance(generation, str) or not GENERATION_PATTERN.fullmatch(generation):
        raise PortReservationError("topology port reservation generation is invalid")
    if not isinstance(path, str) or not Path(path).is_absolute():
        raise PortReservationError("topology port reservation path is invalid")
    reservation_path = Path(path)
    if reservation_path.name != f"{port}.lease":
        raise PortReservationError("topology port reservation path does not match its port")
    if expected_path is not None and reservation_path != expected_path:
        raise PortReservationError("topology port reservation path is not the expected lease")
    _validate_lease_identity(value.get("lease"))
    return value


def _secure_directory(topologies: Path) -> Path:
    directory = topologies / PORT_RESERVATION_DIRECTORY
    try:
        directory.mkdir(mode=0o700)
    except FileExistsError:
        pass
    try:
        metadata = directory.lstat()
    except OSError as error:
        raise PortReservationError(
            f"cannot inspect topology port reservation directory {directory}: {error}"
        ) from error
    if (
        not stat.S_ISDIR(metadata.st_mode)
        or stat.S_IMODE(metadata.st_mode) != 0o700
        or metadata.st_uid != os.geteuid()
    ):
        raise PortReservationError(
            f"topology port reservation directory is invalid: {directory}"
        )
    return directory


def _validate_descriptor(descriptor: int, path: Path) -> os.stat_result:
    try:
        metadata = os.fstat(descriptor)
        path_metadata = path.lstat()
    except OSError as error:
        raise PortReservationError(
            f"cannot inspect topology port reservation lease {path}: {error}"
        ) from error
    if (
        not stat.S_ISREG(metadata.st_mode)
        or stat.S_IMODE(metadata.st_mode) != 0o600
        or metadata.st_uid != os.geteuid()
        or metadata.st_nlink != 1
        or not stat.S_ISREG(path_metadata.st_mode)
        or (path_metadata.st_dev, path_metadata.st_ino)
        != (metadata.st_dev, metadata.st_ino)
    ):
        raise PortReservationError(
            f"topology port reservation lease identity is invalid: {path}"
        )
    return metadata


def open_lease(topologies: Path, port: int) -> tuple[int, Path]:
    port = _validate_port(port)
    path = _secure_directory(topologies) / f"{port}.lease"
    flags = os.O_RDWR | os.O_CREAT | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags, 0o600)
    except OSError as error:
        raise PortReservationError(
            f"cannot open topology port reservation lease {path}: {error}"
        ) from error
    try:
        _validate_descriptor(descriptor, path)
    except BaseException:
        os.close(descriptor)
        raise
    return descriptor, path


def try_lock(descriptor: int) -> bool:
    try:
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except BlockingIOError:
        return False
    except OSError as error:
        raise PortReservationError(
            f"cannot lock topology port reservation lease: {error}"
        ) from error


def read_record(descriptor: int, path: Path) -> dict[str, Any]:
    def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        value: dict[str, Any] = {}
        for key, item in pairs:
            if key in value:
                raise ValueError(f"duplicate JSON key: {key}")
            value[key] = item
        return value

    try:
        os.lseek(descriptor, 0, os.SEEK_SET)
        content = os.read(descriptor, MAX_RECORD_SIZE + 1)
        if len(content) > MAX_RECORD_SIZE:
            raise PortReservationError("topology port reservation record is too large")
        value = json.loads(
            content.decode("utf-8"), object_pairs_hook=reject_duplicate_keys
        )
    except PortReservationError:
        raise
    except (OSError, UnicodeError, ValueError) as error:
        raise PortReservationError(
            f"cannot read topology port reservation record {path}: {error}"
        ) from error
    return validate_record(value, expected_path=path)


def bind_record(
    descriptor: int,
    path: Path,
    *,
    port: int,
    topology: str,
    generation: str,
) -> dict[str, Any]:
    metadata = _validate_descriptor(descriptor, path)
    record = validate_record(
        {
            "schema_version": PORT_RESERVATION_SCHEMA_VERSION,
            "port": port,
            "topology": topology,
            "generation": generation,
            "path": str(path),
            "lease": {"device": metadata.st_dev, "inode": metadata.st_ino},
        },
        expected_path=path,
    )
    payload = (json.dumps(record, indent=2, sort_keys=True) + "\n").encode("utf-8")
    try:
        os.ftruncate(descriptor, 0)
        os.lseek(descriptor, 0, os.SEEK_SET)
        view = memoryview(payload)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise OSError("short write")
            view = view[written:]
        os.fsync(descriptor)
    except OSError as error:
        raise PortReservationError(
            f"cannot publish topology port reservation {path}: {error}"
        ) from error
    _validate_descriptor(descriptor, path)
    return record


def validate_held(descriptor: int, record: Any) -> dict[str, Any]:
    validated = validate_record(record)
    path = Path(validated["path"])
    metadata = _validate_descriptor(descriptor, path)
    identity = validated["lease"]
    if (metadata.st_dev, metadata.st_ino) != (
        identity["device"],
        identity["inode"],
    ):
        raise PortReservationError("topology port reservation lease was replaced")
    return validated


def reservation_locked(record: Any) -> bool:
    validated = validate_record(record)
    path = Path(validated["path"])
    flags = os.O_RDWR | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise PortReservationError(
            f"cannot inspect topology port reservation lease {path}: {error}"
        ) from error
    try:
        validate_held(descriptor, validated)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            # A normal later owner rewrites the same lease inode. Attribute a
            # conflicting lock only when the record currently protected by it
            # still names this exact topology generation.
            deadline = time.monotonic() + 1
            while True:
                try:
                    current = read_record(descriptor, path)
                    validate_held(descriptor, current)
                    return current == validated
                except PortReservationError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.01)
        except OSError as error:
            raise PortReservationError(
                f"cannot inspect topology port reservation lock {path}: {error}"
            ) from error
        fcntl.flock(descriptor, fcntl.LOCK_UN)
        return False
    finally:
        os.close(descriptor)
