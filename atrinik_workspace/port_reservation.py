from __future__ import annotations

import ctypes
import errno
import fcntl
import json
import os
from pathlib import Path
import re
import secrets
import stat
from typing import Any


PORT_RESERVATION_SCHEMA_VERSION = 1
PORT_RESERVATION_DIRECTORY = "port-reservations"
PORT_RESERVATION_KEYS = {
    "schema_version",
    "port",
    "topology",
    "generation",
    "path",
    "directory",
    "lease",
    "token",
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


def _validate_identity(value: Any, description: str) -> dict[str, int]:
    if (
        not isinstance(value, dict)
        or set(value) != {"device", "inode"}
        or not all(
            isinstance(item, int) and not isinstance(item, bool) and item >= 0
            for item in value.values()
        )
    ):
        raise PortReservationError(
            f"topology port reservation {description} identity is invalid"
        )
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
    if reservation_path.name != f"{port}-{generation}.lease":
        raise PortReservationError("topology port reservation path does not match its owner")
    if expected_path is not None and reservation_path != expected_path:
        raise PortReservationError("topology port reservation path is not the expected lease")
    _validate_identity(value.get("directory"), "directory")
    _validate_identity(value.get("lease"), "lease")
    token = value.get("token")
    if not isinstance(token, str) or not GENERATION_PATTERN.fullmatch(token):
        raise PortReservationError("topology port reservation creation token is invalid")
    return value


def _directory_identity(metadata: os.stat_result) -> dict[str, int]:
    return {"device": metadata.st_dev, "inode": metadata.st_ino}


def _validate_directory_path(directory: Path, identity: dict[str, int]) -> None:
    try:
        metadata = directory.lstat()
    except OSError as error:
        raise PortReservationError(
            f"cannot inspect topology port reservation directory {directory}: {error}"
        ) from error
    if (
        not stat.S_ISDIR(metadata.st_mode)
        or _directory_identity(metadata) != identity
    ):
        raise PortReservationError(
            f"topology port reservation directory was replaced: {directory}"
        )


def open_directory(topologies: Path) -> tuple[int, Path, dict[str, int]]:
    directory = topologies / PORT_RESERVATION_DIRECTORY
    try:
        directory.mkdir(mode=0o700)
    except FileExistsError:
        pass
    flags = os.O_RDONLY | os.O_CLOEXEC
    if hasattr(os, "O_DIRECTORY"):
        flags |= os.O_DIRECTORY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(directory, flags)
    except OSError as error:
        raise PortReservationError(
            f"cannot open topology port reservation directory {directory}: {error}"
        ) from error
    try:
        metadata = os.fstat(descriptor)
        path_metadata = directory.lstat()
    except OSError as error:
        os.close(descriptor)
        raise PortReservationError(
            f"cannot open topology port reservation directory {directory}: {error}"
        ) from error
    if (
        not stat.S_ISDIR(metadata.st_mode)
        or stat.S_IMODE(metadata.st_mode) != 0o700
        or metadata.st_uid != os.geteuid()
        or not stat.S_ISDIR(path_metadata.st_mode)
        or (path_metadata.st_dev, path_metadata.st_ino)
        != (metadata.st_dev, metadata.st_ino)
    ):
        os.close(descriptor)
        raise PortReservationError(
            f"topology port reservation directory is invalid: {directory}"
        )
    return descriptor, directory, _directory_identity(metadata)


def _validate_child(
    descriptor: int,
    directory_descriptor: int,
    name: str,
) -> os.stat_result:
    try:
        metadata = os.fstat(descriptor)
        path_metadata = os.stat(
            name, dir_fd=directory_descriptor, follow_symlinks=False
        )
    except OSError as error:
        raise PortReservationError(
            f"cannot inspect topology port reservation lease {name}: {error}"
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
            f"topology port reservation lease identity is invalid: {name}"
        )
    return metadata


def _open_child(
    directory_descriptor: int, name: str, flags: int, *, exclusive: bool = False
) -> int:
    flags |= os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    if exclusive:
        flags |= os.O_CREAT | os.O_EXCL
    try:
        descriptor = os.open(name, flags, 0o600, dir_fd=directory_descriptor)
    except OSError as error:
        raise PortReservationError(
            f"cannot open topology port reservation lease {name}: {error}"
        ) from error
    try:
        _validate_child(descriptor, directory_descriptor, name)
    except BaseException:
        os.close(descriptor)
        raise
    return descriptor


def _rename_no_replace(
    directory_descriptor: int, source: str, destination: str
) -> None:
    try:
        renameat2 = ctypes.CDLL(None, use_errno=True).renameat2
    except AttributeError as error:
        raise PortReservationError(
            "atomic no-replace reservation publication is unsupported"
        ) from error
    renameat2.argtypes = [
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_uint,
    ]
    renameat2.restype = ctypes.c_int
    if (
        renameat2(
            directory_descriptor,
            os.fsencode(source),
            directory_descriptor,
            os.fsencode(destination),
            1,
        )
        == 0
    ):
        return
    error_number = ctypes.get_errno()
    if error_number == errno.EEXIST:
        raise PortReservationError(
            f"topology port reservation generation already exists: {destination}"
        )
    if error_number in {errno.ENOSYS, errno.EINVAL}:
        raise PortReservationError(
            "atomic no-replace reservation publication is unsupported"
        )
    raise PortReservationError(
        f"cannot publish topology port reservation {destination}: "
        f"{os.strerror(error_number)}"
    )


def open_transaction(
    topologies: Path, port: int
) -> tuple[int, int, Path, dict[str, int]]:
    port = _validate_port(port)
    directory_fd, directory, identity = open_directory(topologies)
    try:
        descriptor = _open_child(
            directory_fd, f"{port}.lock", os.O_RDWR | os.O_CREAT
        )
    except BaseException:
        os.close(directory_fd)
        raise
    return descriptor, directory_fd, directory, identity


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


def validate_transaction(
    descriptor: int, directory_descriptor: int, port: int
) -> None:
    _validate_child(descriptor, directory_descriptor, f"{_validate_port(port)}.lock")


def _decode_record(content: bytes, path: Path) -> dict[str, Any]:
    def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        value: dict[str, Any] = {}
        for key, item in pairs:
            if key in value:
                raise ValueError(f"duplicate JSON key: {key}")
            value[key] = item
        return value

    try:
        if len(content) > MAX_RECORD_SIZE:
            raise PortReservationError("topology port reservation record is too large")
        value = json.loads(
            content.decode("utf-8"), object_pairs_hook=reject_duplicate_keys
        )
    except PortReservationError:
        raise
    except (UnicodeError, ValueError) as error:
        raise PortReservationError(
            f"cannot read topology port reservation record {path}: {error}"
        ) from error
    return validate_record(value, expected_path=path)


def read_record(descriptor: int, path: Path) -> dict[str, Any]:
    try:
        os.lseek(descriptor, 0, os.SEEK_SET)
        content = os.read(descriptor, MAX_RECORD_SIZE + 1)
    except OSError as error:
        raise PortReservationError(
            f"cannot read topology port reservation record {path}: {error}"
        ) from error
    return _decode_record(content, path)


def active_owner(
    directory_descriptor: int, directory: Path, port: int
) -> dict[str, Any] | None:
    prefix = f"{_validate_port(port)}-"
    try:
        names = sorted(os.listdir(directory_descriptor))
    except OSError as error:
        raise PortReservationError(
            f"cannot list topology port reservations {directory}: {error}"
        ) from error
    for name in names:
        if not name.startswith(prefix) or not name.endswith(".lease"):
            continue
        descriptor = _open_child(directory_descriptor, name, os.O_RDONLY)
        try:
            record = read_record(descriptor, directory / name)
            try:
                fcntl.flock(descriptor, fcntl.LOCK_SH | fcntl.LOCK_NB)
            except BlockingIOError:
                validate_held(descriptor, record)
                return record
            fcntl.flock(descriptor, fcntl.LOCK_UN)
        finally:
            os.close(descriptor)
    return None


def create_lease(
    directory_descriptor: int,
    directory: Path,
    directory_identity: dict[str, int],
    *,
    port: int,
    topology: str,
    generation: str,
) -> tuple[int, dict[str, Any]]:
    path = directory / f"{port}-{generation}.lease"
    staging_name = f".staging-{port}-{generation}-{secrets.token_hex(16)}"
    descriptor = _open_child(
        directory_descriptor, staging_name, os.O_RDWR, exclusive=True
    )
    try:
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        metadata = _validate_child(descriptor, directory_descriptor, staging_name)
        token = secrets.token_hex(32)
        try:
            os.setxattr(descriptor, "user.atrinik.port-reservation", token.encode())
        except OSError as error:
            raise PortReservationError(
                f"cannot bind topology port reservation creation identity: {error}"
            ) from error
        record = validate_record(
            {
                "schema_version": PORT_RESERVATION_SCHEMA_VERSION,
                "port": port,
                "topology": topology,
                "generation": generation,
                "path": str(path),
                "directory": directory_identity,
                "lease": {"device": metadata.st_dev, "inode": metadata.st_ino},
                "token": token,
            },
            expected_path=path,
        )
        payload = (json.dumps(record, indent=2, sort_keys=True) + "\n").encode()
        view = memoryview(payload)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise OSError("short write")
            view = view[written:]
        os.fsync(descriptor)
        _validate_child(descriptor, directory_descriptor, staging_name)
        _validate_directory_path(directory, directory_identity)
        _rename_no_replace(directory_descriptor, staging_name, path.name)
        _validate_child(descriptor, directory_descriptor, path.name)
        _validate_directory_path(directory, directory_identity)
        return descriptor, record
    except BaseException:
        os.close(descriptor)
        raise


def validate_held(descriptor: int, record: Any) -> dict[str, Any]:
    validated = validate_record(record)
    path = Path(validated["path"])
    directory_fd, directory, directory_identity = open_directory(path.parent.parent)
    try:
        if directory != path.parent or directory_identity != validated["directory"]:
            raise PortReservationError(
                "topology port reservation directory was replaced"
            )
        metadata = _validate_child(descriptor, directory_fd, path.name)
    finally:
        os.close(directory_fd)
    identity = validated["lease"]
    if (metadata.st_dev, metadata.st_ino) != (
        identity["device"],
        identity["inode"],
    ):
        raise PortReservationError("topology port reservation lease was replaced")
    if read_record(descriptor, path) != validated:
        raise PortReservationError("topology port reservation record changed")
    try:
        token = os.getxattr(descriptor, "user.atrinik.port-reservation").decode()
    except (OSError, UnicodeError) as error:
        raise PortReservationError(
            f"cannot validate topology port reservation creation identity: {error}"
        ) from error
    if token != validated["token"]:
        raise PortReservationError(
            "topology port reservation creation identity changed"
        )
    return validated


def reservation_locked(record: Any) -> bool:
    validated = validate_record(record)
    path = Path(validated["path"])
    directory_fd, directory, directory_identity = open_directory(path.parent.parent)
    try:
        if directory != path.parent or directory_identity != validated["directory"]:
            raise PortReservationError(
                "topology port reservation directory was replaced"
            )
        descriptor = _open_child(directory_fd, path.name, os.O_RDONLY)
        try:
            validate_held(descriptor, validated)
            try:
                fcntl.flock(descriptor, fcntl.LOCK_SH | fcntl.LOCK_NB)
            except BlockingIOError:
                validate_held(descriptor, validated)
                return True
            fcntl.flock(descriptor, fcntl.LOCK_UN)
            return False
        finally:
            os.close(descriptor)
    finally:
        os.close(directory_fd)
