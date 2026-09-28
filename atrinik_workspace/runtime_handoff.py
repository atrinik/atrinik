"""Bounded public runtime declarations; declarations alone grant no authority.

Only the coordinator publisher projects these fields from authenticated delivery
proof. Consumers additionally require a live lease and reprove local inputs.
Private ledger documents and arbitrary producer payloads never cross this API.
"""
from __future__ import annotations

import contextlib
import hashlib
import json
import os
import re
import socket
import stat
import struct
import subprocess
import time
from pathlib import Path, PurePosixPath
from typing import Any

from .model import WorkspaceError
from .path_identity import descriptor_path
from .platform_compat import fcntl

MAX_BYTES = 64 * 1024
MAX_LIFETIME = 900
HEX40 = re.compile(r"[0-9a-f]{40}")
HEX64 = re.compile(r"[0-9a-f]{64}")
NAME = re.compile(r"[a-z0-9][a-z0-9._-]{0,127}")
NODE = re.compile(r"[A-Za-z0-9_=-]{2,256}")
REPOSITORY = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")


def canonical(value: Any) -> bytes:
    try:
        return json.dumps(value, ensure_ascii=True, sort_keys=True,
                          separators=(",", ":"), allow_nan=False).encode("ascii") + b"\n"
    except (TypeError, ValueError, RecursionError) as error:
        raise WorkspaceError("runtime handoff is not bounded JSON") from error


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def _exact(value, keys, label):
    if not isinstance(value, dict) or set(value) != set(keys.split()):
        raise WorkspaceError(f"runtime handoff {label} has an unsupported shape")
    return value


def _text(value, pattern, label):
    if not isinstance(value, str) or pattern.fullmatch(value) is None:
        raise WorkspaceError(f"runtime handoff {label} is invalid")
    return value


def _integer(value, minimum, maximum, label):
    if type(value) is not int or not minimum <= value <= maximum:
        raise WorkspaceError(f"runtime handoff {label} is invalid")
    return value


def _path(value):
    if (not isinstance(value, str) or len(value) > 4096 or not value.startswith("/")
            or value.startswith("//") or str(PurePosixPath(value)) != value or ".." in PurePosixPath(value).parts
            or any(ord(char) < 32 or ord(char) == 127 for char in value)):
        raise WorkspaceError("runtime handoff path is not canonical")
    return value


def commands(value):
    """Generate argv, never executable caller-authored shell text."""
    common = ["--state", value["state"], "--service", "server",
              "--retained-build-plan", value["plan"]["sha256"],
              "--runtime-handoff", value["lease_id"],
              "--handoff-issue", value["issue"]["repository"] + "#" + str(value["issue"]["number"]),
              "--handoff-attempt", value["attempt_sha256"],
              "--handoff-publisher", publisher_fingerprint(value["publisher_key"]),
              "--handoff-endpoint", "TRUSTED_COORDINATOR_ENDPOINT"]
    return {"inspect": ["./atrinik", "topology", "show", value["profile"], *common, "--json"],
            "start": ["./atrinik", "up", "--name", value["topology"],
                      "--profile", value["profile"], *common, "--json"]}


def validate(value):
    _exact(value, "schema_version issue attempt_sha256 actor_node_id generation ledger_sha256 "
           "wrapper workspace profile topology state plan sources artifacts content_sha256 "
           "issued_at expires_at lease_id publisher_key commands", "envelope")
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        raise WorkspaceError("runtime handoff schema version is unsupported")
    if len(canonical(value)) > MAX_BYTES:
        raise WorkspaceError("runtime handoff exceeds size bound")
    issue = _exact(value["issue"], "repository number node_id", "issue")
    _text(issue["repository"], REPOSITORY, "issue repository")
    _integer(issue["number"], 1, 2**31 - 1, "issue number")
    _text(issue["node_id"], NODE, "issue identity")
    _text(value["actor_node_id"], NODE, "actor identity")
    for field in ("attempt_sha256", "ledger_sha256", "content_sha256", "lease_id"):
        _text(value[field], HEX64, field)
    _text(value["publisher_key"], HEX64, "publisher public key")
    _integer(value["generation"], 1, 2**53 - 1, "CAS generation")
    for field in ("profile", "topology", "state"):
        _text(value[field], NAME, field)
    for field in ("wrapper", "workspace"):
        _path(value[field])
    for field in ("issued_at", "expires_at"):
        _integer(value[field], 1, 2**53 - 1, field)
    if not 1 <= value["expires_at"] - value["issued_at"] <= MAX_LIFETIME:
        raise WorkspaceError("runtime handoff lease duration is invalid")
    plan = _exact(value["plan"], "sha256 force_reconfigure use_ccache retained_content_input", "plan")
    _text(plan["sha256"], HEX64, "plan digest")
    for field in ("force_reconfigure", "use_ccache"):
        if type(plan[field]) is not bool:
            raise WorkspaceError("runtime handoff build option is invalid")
    if plan["retained_content_input"] is not None:
        _text(plan["retained_content_input"], HEX40, "content commit")
    sources = value["sources"]
    if not isinstance(sources, list) or not 1 <= len(sources) <= 32:
        raise WorkspaceError("runtime handoff sources exceed bound")
    names = []
    for source in sources:
        _exact(source, "checkout repository branch path commit tree sha256", "source")
        names.append(_text(source["checkout"], NAME, "checkout"))
        _text(source["repository"], REPOSITORY, "source repository")
        branch = source["branch"]
        if (not isinstance(branch, str) or not 1 <= len(branch) <= 256
                or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._/-]*", branch) is None
                or any(part in {"", ".", ".."} for part in branch.split("/"))):
            raise WorkspaceError("runtime handoff source branch is invalid")
        _path(source["path"])
        for field in ("commit", "tree"):
            _text(source[field], HEX40, field)
        _text(source["sha256"], HEX64, "source fingerprint")
    if names != sorted(set(names)):
        raise WorkspaceError("runtime handoff sources are ambiguous")
    artifacts = value["artifacts"]
    if not isinstance(artifacts, list) or not 1 <= len(artifacts) <= 128:
        raise WorkspaceError("runtime handoff artifacts exceed bound")
    paths = []
    for artifact in artifacts:
        _exact(artifact, "path sha256", "artifact")
        paths.append(_path(artifact["path"]))
        _text(artifact["sha256"], HEX64, "artifact digest")
    if paths != sorted(set(paths)):
        raise WorkspaceError("runtime handoff artifacts are ambiguous")
    if value["commands"] != commands(value):
        raise WorkspaceError("runtime handoff wrapper commands differ from its coordinates")
    return value


def decode(raw):
    if not isinstance(raw, bytes) or len(raw) > MAX_BYTES:
        raise WorkspaceError("runtime handoff exceeds size bound")
    def pairs(items):
        result = {}
        for key, val in items:
            if key in result:
                raise WorkspaceError("runtime handoff repeats a JSON key")
            result[key] = val
        return result
    try:
        value = json.loads(raw.decode("ascii"), object_pairs_hook=pairs)
    except (ValueError, UnicodeError, RecursionError) as error:
        raise WorkspaceError("runtime handoff is invalid JSON") from error
    validate(value)
    if canonical(value) != raw:
        raise WorkspaceError("runtime handoff bytes are not canonical")
    return value


def require_binding(value, *, issue, attempt, wrapper, workspace, profile,
                    topology, state, plan, publisher, endpoint, now):
    _text(endpoint, HEX64, "trusted endpoint fingerprint")
    validate(value)
    if (issue != value["issue"]["repository"] + "#" + str(value["issue"]["number"])
            or attempt != value["attempt_sha256"] or wrapper != value["wrapper"]
            or workspace != value["workspace"] or profile != value["profile"]
            or topology not in (None, value["topology"]) or state != value["state"]
            or plan != value["plan"]["sha256"]
            or publisher != publisher_fingerprint(value["publisher_key"])):
        raise WorkspaceError("runtime handoff exact issue/attempt/source/profile/topology binding differs")
    if not value["issued_at"] <= now < value["expires_at"]:
        raise WorkspaceError("runtime handoff lease is stale or not yet valid")

# The endpoint lives in the existing shared build mount. /proc/self/fd keeps
# AF_UNIX's pathname limit independent of the length of the owned worktree.
def _directory(path):
    path = Path(_path(str(path)))
    descriptor = os.open("/", os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC,
                            dir_fd=descriptor)
            os.close(descriptor)
            descriptor = child
        return descriptor
    except BaseException:
        os.close(descriptor)
        raise


def _trusted(metadata, kind):
    if (not kind(metadata.st_mode) or metadata.st_uid != os.geteuid()
            or stat.S_IMODE(metadata.st_mode) & 0o077):
        raise WorkspaceError("runtime handoff path has unsafe ownership, type or permissions")


@contextlib.contextmanager
def public_directory(builds, lease_id, *, create=False):
    _text(lease_id, HEX64, "lease identity")
    root = _directory(builds)
    descriptors = [root]
    try:
        for name in ("runtime-handoffs", lease_id):
            if create:
                try:
                    os.mkdir(name, 0o700, dir_fd=root)
                    os.fsync(root)
                except FileExistsError:
                    pass
            child = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC,
                            dir_fd=root)
            descriptors.append(child)
            _trusted(os.fstat(child), stat.S_ISDIR)
            root = child
        expected_path = str(Path(builds) / "runtime-handoffs" / lease_id)
        if descriptor_path(root) != expected_path:
            raise WorkspaceError("runtime handoff namespace path changed")
        yield root
        if descriptor_path(root) != expected_path:
            raise WorkspaceError("runtime handoff namespace path changed")
    except OSError as error:
        raise WorkspaceError("runtime handoff public namespace is missing or unsafe") from error
    finally:
        for descriptor in reversed(descriptors):
            os.close(descriptor)


def _read_at(directory, name):
    descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=directory)
    try:
        before = os.fstat(descriptor)
        _trusted(before, stat.S_ISREG)
        if before.st_nlink != 1 or before.st_size > MAX_BYTES:
            raise WorkspaceError("runtime handoff public file exceeds bounds or is hard-linked")
        with os.fdopen(os.dup(descriptor), "rb") as stream:
            raw = stream.read(MAX_BYTES + 1)
        after = os.fstat(descriptor)
        visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
        if _stable(before) != _stable(after) or _stable(after) != _stable(visible):
            raise WorkspaceError("runtime handoff public file changed during read")
        return raw
    finally:
        os.close(descriptor)


def _peer(connection):
    _pid, uid, _gid = struct.unpack("3i", connection.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
    if uid != os.geteuid():
        raise WorkspaceError("runtime handoff endpoint has a foreign peer")


def _send(connection, value):
    raw = canonical(value)
    if len(raw) > 4096:
        raise WorkspaceError("runtime handoff protocol response exceeds bound")
    connection.sendall(raw)


def _receive(connection, *, deadline=None):
    # recv timeouts are inactivity bounds, so refresh the remaining absolute
    # budget for every fragment. A slow peer cannot extend a lease by dripping.
    if deadline is None:
        deadline = time.monotonic() + 30
    raw = bytearray()
    while len(raw) < 4096:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise WorkspaceError("runtime handoff protocol deadline expired")
        connection.settimeout(remaining)
        byte = connection.recv(1)
        if time.monotonic() >= deadline:
            raise WorkspaceError("runtime handoff protocol deadline expired")
        if not byte:
            raise WorkspaceError("runtime handoff publisher or executor disconnected")
        raw.extend(byte)
        if byte == b"\n":
            try:
                value = json.loads(raw)
            except (ValueError, RecursionError) as error:
                raise WorkspaceError("runtime handoff protocol is malformed") from error
            if canonical(value) != raw:
                raise WorkspaceError("runtime handoff protocol is not canonical")
            return value
    raise WorkspaceError("runtime handoff protocol exceeds bound")


def publish(value, guard, *, signer, ready=None):
    """Serve a bounded lease; guard holds private CAS authority per connection.

    The caller has already proved the complete retained producer under resource
    leases. guard() acquires the helper's operation-scoped private ledger guard
    and yields a recheck callable. No private descriptor crosses the socket.
    """
    validate(value)
    if not isinstance(signer, PublisherSigner) or signer.public_key != value["publisher_key"]:
        raise WorkspaceError("runtime handoff publisher signing capability differs")
    raw = canonical(value)
    checksum = hashlib.sha256(raw).hexdigest()
    lease_deadline = time.monotonic() + max(0, value["expires_at"] - time.time())
    with public_directory(Path(value["workspace"]) / "build", value["lease_id"], create=True) as directory:
        lock = os.open("lease.lock", os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW | os.O_CLOEXEC,
                       0o600, dir_fd=directory)
        listener = None
        endpoint = f"/proc/self/fd/{directory}/endpoint.sock"
        try:
            _trusted(os.fstat(lock), stat.S_ISREG)
            if os.fstat(lock).st_nlink != 1:
                raise WorkspaceError("runtime handoff lease is hard-linked")
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise WorkspaceError("runtime handoff publisher lease is already active") from error
            try:
                os.stat("revoked.json", dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                raise WorkspaceError("runtime handoff lease was permanently revoked")
            payload = (value["lease_id"] + "\n").encode("ascii")
            os.lseek(lock, 0, os.SEEK_SET)
            previous_payload = os.read(lock, 66)
            if previous_payload not in (b"", payload):
                raise WorkspaceError("runtime handoff lease generation changed")
            if not previous_payload:
                os.write(lock, payload)
                os.fsync(lock)
            try:
                existing = _read_at(directory, "envelope.json")
            except FileNotFoundError:
                descriptor = os.open("envelope.json", os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                     0o600, dir_fd=directory)
                with os.fdopen(descriptor, "wb") as stream:
                    stream.write(raw); stream.flush(); os.fsync(stream.fileno())
                os.fsync(directory)
            else:
                if existing != raw:
                    raise WorkspaceError("runtime handoff retry changed immutable historical envelope")
            # Only the exclusive publisher lease permits removal of its own
            # dead endpoint; an active same-coordinate publisher cannot race it.
            try:
                previous = os.stat("endpoint.sock", dir_fd=directory, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                _trusted(previous, stat.S_ISSOCK)
                os.unlink("endpoint.sock", dir_fd=directory)
            listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            listener.bind(endpoint)
            os.chmod("endpoint.sock", 0o600, dir_fd=directory, follow_symlinks=False)
            listener.listen(1)
            listener.settimeout(0.25)
            endpoint_pin = endpoint_fingerprint(directory, Path(value["workspace"]) / "build", value["lease_id"])
            if ready:
                ready({"lease_id": value["lease_id"], "envelope_sha256": checksum,
                       "publisher_fingerprint": publisher_fingerprint(value["publisher_key"]),
                       "endpoint_fingerprint": endpoint_pin,
                       "path": str(Path(value["workspace"]) / "build/runtime-handoffs" / value["lease_id"] / "envelope.json"),
                       "commands": {operation: [endpoint_pin if argument == "TRUSTED_COORDINATOR_ENDPOINT" else argument
                                                for argument in arguments]
                                    for operation, arguments in value["commands"].items()}})
            revoked = False
            while time.time() < value["expires_at"] and time.monotonic() < lease_deadline and not revoked:
                try:
                    connection, _ = listener.accept()
                except socket.timeout:
                    continue
                with connection:
                    connection.settimeout(max(0.1, value["expires_at"] - time.time()))
                    try:
                        _peer(connection)
                        request = _receive(connection, deadline=lease_deadline)
                        nonce = request.get("nonce") if isinstance(request, dict) else None
                        _text(nonce, HEX64, "challenge nonce")
                        expected = {"operation": "begin", "nonce": nonce, "endpoint_fingerprint": endpoint_pin, "envelope_sha256": checksum,
                                    "lease_id": value["lease_id"], "generation": value["generation"],
                                    "ledger_sha256": value["ledger_sha256"]}
                        if request == {**expected, "operation": "revoke"}:
                            descriptor = os.open("revoked.json", os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                                 0o600, dir_fd=directory)
                            with os.fdopen(descriptor, "wb") as stream:
                                stream.write(canonical({"lease_id": value["lease_id"], "status": "revoked"}))
                                stream.flush(); os.fsync(stream.fileno())
                            os.fsync(directory)
                            revoked = True
                            response = {"status": "revoked", "nonce": nonce, "endpoint_fingerprint": endpoint_pin, "envelope_sha256": checksum}
                            _send(connection, {"response": response, "signature": signer.sign(canonical(response))})
                            continue
                        if request != expected:
                            raise WorkspaceError("runtime handoff request has foreign or stale coordinates")
                        with guard() as recheck:
                            while True:
                                if time.time() >= value["expires_at"]:
                                    raise WorkspaceError("runtime handoff lease expired")
                                recheck()
                                if _read_at(directory, "envelope.json") != raw:
                                    raise WorkspaceError("runtime handoff declaration changed")
                                _namespace_current(directory, Path(value["workspace"]) / "build", value["lease_id"])
                                if endpoint_fingerprint(directory, Path(value["workspace"]) / "build", value["lease_id"]) != endpoint_pin:
                                    raise WorkspaceError("runtime handoff publisher endpoint changed")
                                response = {"status": "verified", **expected}
                                _send(connection, {"response": response, "signature": signer.sign(canonical(response))})
                                connection.settimeout(max(0.1, value["expires_at"] - time.time()))
                                request = _receive(connection, deadline=lease_deadline)
                                _exact(request, "operation nonce", "session request")
                                expected["nonce"] = _text(request["nonce"], HEX64, "challenge nonce")
                                if request["operation"] == "finish":
                                    if time.time() >= value["expires_at"]:
                                        raise WorkspaceError("runtime handoff lease expired before completion")
                                    recheck()
                                    if endpoint_fingerprint(directory, Path(value["workspace"]) / "build", value["lease_id"]) != endpoint_pin:
                                        raise WorkspaceError("runtime handoff publisher endpoint changed")
                                    response = {"status": "finished", **expected}
                                    _send(connection, {"response": response, "signature": signer.sign(canonical(response))})
                                    break
                                if request["operation"] != "recheck":
                                    raise WorkspaceError("runtime handoff session request is invalid")
                    except (OSError, WorkspaceError):
                        # Never send private helper exception text to an executor.
                        try:
                            _send(connection, {"status": "rejected"})
                        except OSError:
                            pass
        finally:
            if listener is not None:
                listener.close()
            os.close(lock)


@contextlib.contextmanager
def consume(builds, lease_id, binding):
    """Connect using only public build files; retain verification through use."""
    with public_directory(builds, lease_id) as directory:
        try:
            raw = _read_at(directory, "envelope.json")
            value = decode(raw)
            if value["lease_id"] != lease_id:
                raise WorkspaceError("runtime handoff lease path differs")
            require_binding(value, **binding, now=time.time())
            lease_deadline = time.monotonic() + max(0, value["expires_at"] - time.time())
            _lease_live(directory, lease_id)
            if endpoint_fingerprint(directory, builds, lease_id) != binding["endpoint"]:
                raise WorkspaceError("runtime handoff trusted endpoint differs")
            before = os.stat("endpoint.sock", dir_fd=directory, follow_symlinks=False)
            _trusted(before, stat.S_ISSOCK)
            connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            with connection:
                connection.settimeout(max(0.1, value["expires_at"] - time.time()))
                connection.connect(f"/proc/self/fd/{directory}/endpoint.sock")
                _peer(connection)
                after = os.stat("endpoint.sock", dir_fd=directory, follow_symlinks=False)
                if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino):
                    raise WorkspaceError("runtime handoff endpoint changed during connection")
                expected = {"operation": "begin", "nonce": os.urandom(32).hex(), "endpoint_fingerprint": binding["endpoint"], "envelope_sha256": hashlib.sha256(raw).hexdigest(),
                            "lease_id": lease_id, "generation": value["generation"],
                            "ledger_sha256": value["ledger_sha256"]}
                _send(connection, expected)
                def reply(status="verified"):
                    require_binding(value, **binding, now=time.time())
                    _namespace_current(directory, builds, lease_id)
                    if endpoint_fingerprint(directory, builds, lease_id) != binding["endpoint"]:
                        raise WorkspaceError("runtime handoff trusted endpoint differs")
                    _lease_live(directory, lease_id)
                    visible = os.stat("endpoint.sock", dir_fd=directory, follow_symlinks=False)
                    if (before.st_dev, before.st_ino) != (visible.st_dev, visible.st_ino):
                        raise WorkspaceError("runtime handoff endpoint incarnation changed")
                    connection.settimeout(max(0.1, value["expires_at"] - time.time()))
                    packet = _receive(connection, deadline=lease_deadline)
                    if (not verify_response(packet, value["publisher_key"], {"status": status, **expected})
                            or _read_at(directory, "envelope.json") != raw):
                        raise WorkspaceError("runtime handoff live authority was rejected or changed")
                    # Verification can block on a live coordinator proof. Fence
                    # namespace, endpoint and expiry again after its response.
                    require_binding(value, **binding, now=time.time())
                    _namespace_current(directory, builds, lease_id)
                    if endpoint_fingerprint(directory, builds, lease_id) != binding["endpoint"]:
                        raise WorkspaceError("runtime handoff trusted endpoint differs")
                    _lease_live(directory, lease_id)
                    visible = os.stat("endpoint.sock", dir_fd=directory, follow_symlinks=False)
                    _trusted(visible, stat.S_ISSOCK)
                    if (before.st_dev, before.st_ino) != (visible.st_dev, visible.st_ino):
                        raise WorkspaceError("runtime handoff endpoint incarnation changed")
                reply()
                def recheck():
                    expected["nonce"] = os.urandom(32).hex()
                    _send(connection, {"operation": "recheck", "nonce": expected["nonce"]})
                    reply()
                yield value, recheck
                expected["nonce"] = os.urandom(32).hex()
                _send(connection, {"operation": "finish", "nonce": expected["nonce"]})
                reply("finished")
        except (OSError, ValueError) as error:
            raise WorkspaceError("runtime handoff publisher is missing, revoked, or unavailable") from error


def _lease_live(directory, lease_id):
    try:
        os.stat("revoked.json", dir_fd=directory, follow_symlinks=False)
    except FileNotFoundError:
        pass
    else:
        raise WorkspaceError("runtime handoff lease was revoked")
    descriptor = os.open("lease.lock", os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=directory)
    try:
        _trusted(os.fstat(descriptor), stat.S_ISREG)
        if (os.fstat(descriptor).st_nlink != 1
                or os.read(descriptor, 66) != (lease_id + "\n").encode("ascii")):
            raise WorkspaceError("runtime handoff lease coordinate changed")
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        raise WorkspaceError("runtime handoff publisher lease is no longer active")
    finally:
        os.close(descriptor)


def revoke(builds, lease_id, *, issue, attempt, publisher, endpoint):
    """Revoke an owned publisher; keep its immutable envelope and lease file."""
    with public_directory(builds, lease_id) as directory:
        value = decode(_read_at(directory, "envelope.json"))
        if (value["lease_id"] != lease_id or value["attempt_sha256"] != attempt
                or publisher != publisher_fingerprint(value["publisher_key"])
                or value["issue"]["repository"] + "#" + str(value["issue"]["number"]) != issue):
            raise WorkspaceError("runtime handoff revoke has foreign issue or attempt")
        lease_deadline = time.monotonic() + max(0, value["expires_at"] - time.time())
        _lease_live(directory, lease_id)
        if endpoint_fingerprint(directory, builds, lease_id) != endpoint:
            raise WorkspaceError("runtime handoff trusted endpoint differs")
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
            connection.settimeout(max(0.1, value["expires_at"] - time.time()))
            connection.connect(f"/proc/self/fd/{directory}/endpoint.sock")
            _peer(connection)
            nonce = os.urandom(32).hex()
            _send(connection, {"operation": "revoke", "nonce": nonce, "endpoint_fingerprint": endpoint, "envelope_sha256": digest(value),
                               "lease_id": lease_id, "generation": value["generation"],
                               "ledger_sha256": value["ledger_sha256"]})
            if not verify_response(_receive(connection, deadline=lease_deadline), value["publisher_key"],
                                   {"status": "revoked", "nonce": nonce, "endpoint_fingerprint": endpoint, "envelope_sha256": digest(value)}):
                raise WorkspaceError("runtime handoff revocation was rejected")
    return {"lease_id": lease_id, "status": "revoked"}


def endpoint_fingerprint(directory, builds, lease_id):
    """Pin the socket and its shared namespace, independently of envelope bytes."""
    _namespace_current(directory, builds, lease_id)
    root = _directory(builds)
    descriptors = [root]
    try:
        identities = []
        for name in (None, "runtime-handoffs", lease_id):
            if name is not None:
                root = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC,
                               dir_fd=root)
                descriptors.append(root)
                _trusted(os.fstat(root), stat.S_ISDIR)
            metadata = os.fstat(root)
            identities.append([metadata.st_dev, metadata.st_ino])
        if identities[-1] != [os.fstat(directory).st_dev, os.fstat(directory).st_ino]:
            raise WorkspaceError("runtime handoff namespace incarnation changed")
        endpoint = os.stat("endpoint.sock", dir_fd=root, follow_symlinks=False)
        _trusted(endpoint, stat.S_ISSOCK)
        identities.append([endpoint.st_dev, endpoint.st_ino, endpoint.st_ctime_ns])
        return digest(identities)
    finally:
        for descriptor in reversed(descriptors):
            os.close(descriptor)


def _stable(metadata):
    # Access-time updates are observations, not content/authority mutations.
    return (metadata.st_dev, metadata.st_ino, metadata.st_mode, metadata.st_uid,
            metadata.st_nlink, metadata.st_size, metadata.st_mtime_ns, metadata.st_ctime_ns)


def _namespace_current(directory, builds, lease_id):
    expected = Path(builds) / "runtime-handoffs" / lease_id
    if descriptor_path(directory) != str(expected):
        raise WorkspaceError("runtime handoff namespace path changed")
    with public_directory(builds, lease_id) as visible:
        before, after = os.fstat(directory), os.fstat(visible)
        if (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino):
            raise WorkspaceError("runtime handoff namespace incarnation changed")


_ED25519_PUBLIC_DER = bytes.fromhex("302a300506032b6570032100")


def publisher_fingerprint(public_key):
    _text(public_key, HEX64, "publisher public key")
    return hashlib.sha256(bytes.fromhex(public_key)).hexdigest()


@contextlib.contextmanager
def _anonymous_bytes(raw):
    descriptor = os.memfd_create("atrinik-handoff", os.MFD_CLOEXEC)
    try:
        os.write(descriptor, raw)
        os.lseek(descriptor, 0, os.SEEK_SET)
        yield descriptor
    finally:
        os.close(descriptor)


def _openssl(arguments, *, input_bytes=None, descriptors=(), verify=False):
    environment = {key: val for key, val in os.environ.items()
                   if not key.startswith(("OPENSSL_", "LD_"))}
    environment["OPENSSL_CONF"] = "/dev/null"
    try:
        result = subprocess.run(["/usr/bin/openssl", *arguments], input=input_bytes,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, pass_fds=descriptors,
            timeout=10, check=False, env=environment)
    except (OSError, subprocess.SubprocessError) as error:
        raise WorkspaceError("runtime handoff Ed25519 provider is unavailable") from error
    if verify:
        return result.returncode == 0
    if result.returncode or len(result.stdout) > 4096:
        raise WorkspaceError("runtime handoff Ed25519 operation failed")
    return result.stdout


class PublisherSigner:
    """Ephemeral helper-process key; only its public fingerprint leaves memory."""
    def __init__(self):
        self._private = _openssl(["genpkey", "-algorithm", "ED25519", "-outform", "DER"])
        public = _openssl(["pkey", "-inform", "DER", "-pubout", "-outform", "DER"], input_bytes=self._private)
        if len(public) != 44 or not public.startswith(_ED25519_PUBLIC_DER):
            raise WorkspaceError("runtime handoff Ed25519 public key is invalid")
        self.public_key = public[-32:].hex()

    def sign(self, raw):
        with _anonymous_bytes(self._private) as key, _anonymous_bytes(raw) as message:
            signature = _openssl(["pkeyutl", "-sign", "-rawin", "-keyform", "DER",
                "-inkey", f"/proc/self/fd/{key}", "-in", f"/proc/self/fd/{message}"],
                descriptors=(key, message))
        if len(signature) != 64:
            raise WorkspaceError("runtime handoff signature has invalid size")
        return signature.hex()


def verify_response(packet, public_key, expected):
    if (not isinstance(packet, dict) or set(packet) != {"response", "signature"}
            or packet["response"] != expected or not isinstance(packet["signature"], str)
            or re.fullmatch(r"[0-9a-f]{128}", packet["signature"]) is None):
        return False
    with _anonymous_bytes(_ED25519_PUBLIC_DER + bytes.fromhex(public_key)) as key, \
            _anonymous_bytes(canonical(expected)) as message, \
            _anonymous_bytes(bytes.fromhex(packet["signature"])) as signature:
        return _openssl(["pkeyutl", "-verify", "-rawin", "-pubin", "-keyform", "DER",
            "-inkey", f"/proc/self/fd/{key}", "-in", f"/proc/self/fd/{message}",
            "-sigfile", f"/proc/self/fd/{signature}"], descriptors=(key, message, signature), verify=True)
