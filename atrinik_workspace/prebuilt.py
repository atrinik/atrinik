# Copyright 2026 The Atrinik Project
"""Strict completion evidence; callers retain build locks and resource authority."""
from __future__ import annotations

from contextlib import contextmanager
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import secrets
import stat

from .model import WorkspaceError
from .path_identity import descriptor_path

RECEIPT_NAME = ".atrinik-prebuilt.json"
MAX_RECEIPT_BYTES = 16 * 1024 * 1024
MAX_ENTRIES = 1_000_000
MAX_INPUT_BYTES = 1024**4
_HEX = re.compile(r"[0-9a-f]{64}")
_LABEL = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}")
_PLAN_KEYS = set("schema_version target profile tests force_reconfigure use_ccache targets manifest checkout_states source_fingerprints git_observations sources execution_sources wrapper_root workspace_root builds_root build_key build_root plan_sha256".split())


def _fail(message):
    raise WorkspaceError("prebuilt receipt: " + message)


def _canonical(value):
    try:
        return json.dumps(value, sort_keys=True, separators=(",", ":"),
                          ensure_ascii=True, allow_nan=False).encode("ascii")
    except (TypeError, ValueError, RecursionError) as error:
        raise WorkspaceError("prebuilt receipt: invalid JSON value") from error


def _exact(value, keys, label):
    if type(value) is not dict or set(value) != set(keys):
        _fail(f"invalid {label} fields")


def _path(value):
    if (type(value) is not str or not value.startswith("/") or value.startswith("//")
            or len(value) > 4096 or str(PurePosixPath(value)) != value
            or ".." in PurePosixPath(value).parts
            or any(ord(char) < 32 or ord(char) == 127 for char in value)):
        _fail("path must be absolute and canonical")
    return value


def _sha(value):
    if type(value) is not str or _HEX.fullmatch(value) is None:
        _fail("invalid SHA256")
    return value


def _signature(info):
    return (info.st_dev, info.st_ino, info.st_mode, info.st_nlink, info.st_uid,
            info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def _same(before, after):
    if _signature(before) != _signature(after):
        _fail("filesystem input changed during observation")


@contextmanager
def _opened(path, *, directory=False, missing_ok=False, owned=False):
    """Open every component without following links, including ancestors."""
    path = _path(os.fspath(path))
    fd = os.open("/", os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    try:
        parts = PurePosixPath(path).parts[1:]
        for index, part in enumerate(parts):
            flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK
            if directory or index < len(parts) - 1:
                flags |= os.O_DIRECTORY
            try:
                next_fd = os.open(part, flags, dir_fd=fd)
            except FileNotFoundError:
                if not missing_ok:
                    raise
                parent = str(PurePosixPath("/", *parts[:index]))
                _named_descriptor(fd, parent)
                yield None
                _named_descriptor(fd, parent)
                return
            os.close(fd)
            fd = next_fd
        _named_descriptor(fd, path, owned=owned)
        yield fd
        _named_descriptor(fd, path, owned=owned)
    except OSError as error:
        raise WorkspaceError(f"prebuilt receipt: unsafe or unreadable path: {path}") from error
    finally:
        os.close(fd)


def _named_descriptor(fd, path, *, owned=False):
    if descriptor_path(fd) != path:
        _fail("filesystem coordinate changed during observation")
    if owned and os.fstat(fd).st_uid != os.geteuid():
        _fail("build root must be owned by this user")


def _regular(info, *, private=False):
    if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
        _fail("expected a regular file with one link")
    if private and (info.st_uid != os.geteuid() or stat.S_IMODE(info.st_mode) != 0o600):
        _fail("receipt must be owned by this user with mode 0600")


def _root_identity(root):
    with _opened(root, directory=True, owned=True):
        return {"path": _path(os.fspath(root))}


def _selectors(value, *, manifest=False):
    if type(value) is not dict or not value or len(value) > 128:
        _fail("invalid input selection")
    result = {}
    for label, entry in value.items():
        if type(label) is not str or _LABEL.fullmatch(label) is None:
            _fail("invalid input label")
        _exact(entry, {"path", "kind", "exclusions"} | ({"sha256"} if manifest else set()), "input")
        _path(entry["path"])
        if entry["kind"] not in ("tree", "file"):
            _fail("invalid input kind")
        exclusions = entry["exclusions"]
        if (type(exclusions) is not list or len(exclusions) > 128
                or any(type(name) is not str or not name or name in (".", "..")
                       or "/" in name or "\\" in name
                       or any(ord(char) < 32 or ord(char) == 127 for char in name)
                       for name in exclusions)
                or exclusions != sorted(set(exclusions))
                or (entry["kind"] == "file" and exclusions)):
            _fail("invalid immediate-entry exclusions")
        if manifest:
            _sha(entry["sha256"])
        result[label] = {"path": entry["path"], "kind": entry["kind"], "exclusions": list(exclusions)}
    return result


def _observe(selector, budget, *, expected=None):
    """Hash through descriptors; a second metadata walk fences earlier entries."""
    digest = hashlib.sha256()
    observed = {}
    path = selector["path"]

    def walk(fd, relative, depth):
        before = os.fstat(fd)
        budget[0] += 1
        if budget[0] > MAX_ENTRIES or depth > 128:
            _fail("input tree exceeds observation limit")
        signature = _signature(before)
        observed[relative] = signature
        if expected is not None and expected.get(relative) != signature:
            _fail("input changed between observation passes")
        mode = before.st_mode & 0o111
        if stat.S_ISREG(before.st_mode):
            _regular(before)
            budget[1] += before.st_size
            if budget[1] > MAX_INPUT_BYTES:
                _fail("input bytes exceed observation limit")
            if expected is None:
                content = hashlib.sha256()
                total = 0
                while True:
                    chunk = os.read(fd, 1024 * 1024)
                    if not chunk:
                        break
                    total += len(chunk)
                    if total > before.st_size:
                        _fail("input grew during observation")
                    content.update(chunk)
                if total != before.st_size:
                    _fail("input shrank during observation")
                digest.update(_canonical([relative, "file", mode, total, content.hexdigest()]) + b"\n")
        elif stat.S_ISDIR(before.st_mode):
            if expected is None:
                digest.update(_canonical([relative, "tree", mode]) + b"\n")
            names = sorted(os.listdir(fd))
            if len(names) + budget[0] > MAX_ENTRIES:
                _fail("input tree exceeds observation limit")
            for name in names:
                if not relative and name in selector["exclusions"]:
                    continue
                listed = os.stat(name, dir_fd=fd, follow_symlinks=False)
                if not (stat.S_ISREG(listed.st_mode) or stat.S_ISDIR(listed.st_mode)):
                    _fail("links and special filesystem objects are not inputs")
                flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC
                if stat.S_ISDIR(listed.st_mode):
                    flags |= os.O_DIRECTORY
                child = os.open(name, flags, dir_fd=fd)
                try:
                    _same(listed, os.fstat(child))
                    walk(child, f"{relative}/{name}" if relative else name, depth + 1)
                    _same(listed, os.stat(name, dir_fd=fd, follow_symlinks=False))
                finally:
                    os.close(child)
            if names != sorted(os.listdir(fd)):
                _fail("input directory changed during observation")
        else:
            _fail("links and special filesystem objects are not inputs")
        _same(before, os.fstat(fd))

    with _opened(path, directory=selector["kind"] == "tree") as fd:
        before = os.fstat(fd)
        if selector["kind"] == "file":
            _regular(before)
        walk(fd, "", 0)
        with _opened(path, directory=selector["kind"] == "tree") as fresh:
            _same(before, os.fstat(fresh))
    if expected is not None and observed != expected:
        _fail("input tree changed between observation passes")
    return digest.hexdigest(), observed


def capture_inputs(selectors):
    """Return canonical selectors and complete streaming content digests."""
    selected = _selectors(selectors)
    result, observations = {}, {}
    budget = [0, 0]
    for label, selector in sorted(selected.items()):
        sha, observed = _observe(selector, budget)
        result[label] = {**selector, "sha256": sha}
        observations[label] = observed
    budget = [0, 0]
    for label, selector in sorted(selected.items()):
        _observe(selector, budget, expected=observations[label])
    return result


def verify_inputs(selectors, manifest):
    selected = _selectors(selectors)
    if selected != _selectors(manifest, manifest=True):
        _fail("runtime input selection differs from completed build")
    if capture_inputs(selected) != manifest:
        _fail("runtime input bytes or executable modes differ from completed build")


def _validate(value, root):
    _exact(value, {"schema_version", "build_root", "plan", "producer", "inputs"}, "receipt")
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        _fail("unsupported schema")
    identity = value["build_root"]
    if type(identity) is not dict or set(identity) not in (
            {"path"}, {"path", "device", "inode"}):
        _fail("invalid build root keys")
    # Legacy receipts retain their original bytes and digest, but filesystem
    # numbers are observations, not durable identity across storage moves.
    if identity["path"] != _root_identity(root)["path"]:
        _fail("build root identity differs")
    plan = value["plan"]
    _exact(plan, _PLAN_KEYS, "build plan")
    if (type(plan["schema_version"]) is not int or plan["schema_version"] != 1
            or plan["target"] != "topology" or type(plan["profile"]) is not dict
            or plan["profile"].get("stack") != "classic"
            or type(plan["targets"]) is not list
            or any(type(item) is not str for item in plan["targets"])
            or sorted(plan["targets"]) != ["client", "server"]
            or plan["build_root"] != identity["path"]):
        _fail("requires an exact Classic client/server topology plan")
    for key in ("tests", "force_reconfigure", "use_ccache"):
        if type(plan[key]) is not bool:
            _fail("invalid build plan flags")
    for key in ("manifest", "checkout_states", "source_fingerprints", "git_observations", "sources", "execution_sources"):
        if type(plan[key]) is not dict:
            _fail("invalid build plan mappings")
    for key in ("wrapper_root", "workspace_root", "builds_root", "build_root"):
        _path(plan[key])
    if type(plan["build_key"]) is not str or not plan["build_key"]:
        _fail("invalid build key")
    unsigned = {key: item for key, item in plan.items() if key != "plan_sha256"}
    if _sha(plan["plan_sha256"]) != hashlib.sha256(_canonical(unsigned)).hexdigest():
        _fail("embedded build plan digest differs")
    producer = value["producer"]
    _exact(producer, {"generation", "system", "machine", "wrapper_head", "configurations"}, "producer")
    _sha(producer["generation"])
    if (producer["system"] != "Linux" or type(producer["machine"]) is not str
            or not producer["machine"] or len(producer["machine"]) > 128
            or any(ord(char) < 32 for char in producer["machine"])
            or type(producer["wrapper_head"]) is not str
            or re.fullmatch(r"(?:[0-9a-f]{40}|[0-9a-f]{64})", producer["wrapper_head"]) is None):
        _fail("invalid producer platform or wrapper identity")
    configs = producer["configurations"]
    if type(configs) is not dict or not configs or len(configs) > 128:
        _fail("invalid configure identities")
    for name, fingerprint in configs.items():
        if (type(name) is not str or not name.startswith("build/")
                or len(name) > 4096 or str(PurePosixPath(name)) != name
                or ".." in PurePosixPath(name).parts
                or PurePosixPath(name).name != ".atrinik-configure.json"
                or any(ord(char) < 32 for char in name)
                or type(fingerprint) is not dict
                or type(fingerprint.get("schema_version")) is not int
                or fingerprint["schema_version"] < 1
                or fingerprint.get("purpose") != "cmake-configure"):
            _fail("invalid configure identity")
    _selectors(value["inputs"], manifest=True)


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            _fail("duplicate JSON field")
        result[key] = value
    return result


def load(root, expected_digest):
    """Read a private bounded receipt without following or blocking on objects."""
    _sha(expected_digest)
    with _opened(root, directory=True, owned=True) as directory:
        fd = os.open(RECEIPT_NAME, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW | os.O_CLOEXEC,
                     dir_fd=directory)
        try:
            before = os.fstat(fd)
            _regular(before, private=True)
            if before.st_size > MAX_RECEIPT_BYTES:
                _fail("receipt exceeds size limit")
            chunks, size = [], 0
            while True:
                chunk = os.read(fd, min(1024 * 1024, MAX_RECEIPT_BYTES + 1 - size))
                if not chunk:
                    break
                chunks.append(chunk)
                size += len(chunk)
                if size > MAX_RECEIPT_BYTES:
                    _fail("receipt exceeds size limit")
            _same(before, os.fstat(fd))
            _same(before, os.stat(RECEIPT_NAME, dir_fd=directory, follow_symlinks=False))
        finally:
            os.close(fd)
    raw = b"".join(chunks)
    if hashlib.sha256(raw).hexdigest() != expected_digest:
        _fail("receipt SHA256 differs")
    try:
        value = json.loads(raw, object_pairs_hook=_pairs,
                           parse_constant=lambda value: _fail("nonfinite JSON number"))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise WorkspaceError("prebuilt receipt: malformed JSON") from error
    if _canonical(value) + b"\n" != raw:
        _fail("receipt is not canonical JSON")
    _validate(value, root)
    return value


def read_metadata(path, description, limit=4 * 1024 * 1024):
    """Read bounded strict JSON metadata without following or blocking on files."""
    if type(limit) is not int or not 0 < limit <= MAX_RECEIPT_BYTES:
        _fail("invalid metadata size limit")
    try:
        with _opened(path) as fd:
            before = os.fstat(fd)
            _regular(before)
            if before.st_size > limit:
                _fail("metadata exceeds size limit")
            chunks, size = [], 0
            while True:
                chunk = os.read(fd, min(1024 * 1024, limit + 1 - size))
                if not chunk:
                    break
                chunks.append(chunk)
                size += len(chunk)
                if size > limit:
                    _fail("metadata exceeds size limit")
            _same(before, os.fstat(fd))
            with _opened(path) as visible:
                _same(before, os.fstat(visible))
        value = json.loads(b"".join(chunks), object_pairs_hook=_pairs,
                           parse_constant=lambda value: _fail("nonfinite JSON number"))
        if type(value) is not dict:
            _fail("metadata must be a JSON object")
        _canonical(value)  # Reject exponent overflow as well as NaN/Infinity tokens.
        return value
    except (WorkspaceError, ValueError, UnicodeError, RecursionError) as error:
        raise WorkspaceError(f"{description}: unsafe or invalid metadata: {path}") from error


def _rename_noreplace(directory, source, destination):
    """Linux atomic rename without clobbering any destination entry."""
    library = ctypes.CDLL(None, use_errno=True)
    try:
        rename = library.renameat2
    except AttributeError as error:
        raise WorkspaceError("prebuilt receipt requires atomic rename-noreplace") from error
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(directory, os.fsencode(source), directory, os.fsencode(destination), 1):
        number = ctypes.get_errno()
        raise OSError(number, os.strerror(number))


def _inode(info):
    return (info.st_dev, info.st_ino)


def _retire(directory, name, expected):
    """Claim by atomic rename; retain every claimed object, including mismatches.

    There is deliberately no unlink after a stat check. These nonce sidecars are
    evidence only and are never considered by the exact-name receipt loader.
    """
    if os.fstat(directory).st_uid != os.geteuid():
        _fail("build root must be owned by this user")
    for _ in range(8):
        retired = ".atrinik-prebuilt-retired-" + secrets.token_hex(16)
        try:
            _rename_noreplace(directory, name, retired)
            break
        except FileExistsError:
            continue
    else:
        _fail("cannot reserve a receipt quarantine name")
    # Persist the claim even when the moved entry is a concurrent replacement.
    os.fsync(directory)
    observed = os.stat(retired, dir_fd=directory, follow_symlinks=False)
    if _inode(observed) != _inode(expected):
        _fail(f"receipt changed during claim; preserved at {retired}")
    _regular(observed, private=True)
    if os.fstat(directory).st_uid != os.geteuid():
        _fail(f"build root ownership changed; receipt preserved at {retired}")
    try:
        os.stat(name, dir_fd=directory, follow_symlinks=False)
    except FileNotFoundError:
        pass
    else:
        _fail(f"receipt replaced after claim; prior receipt preserved at {retired}")
    return retired


def _invalidate_unsupported(root):
    """Other platforms can build normally only when no Linux receipt exists."""
    root = Path(root)
    if not root.is_absolute():
        _fail("build root must be absolute")
    try:
        for parent in (*reversed(root.parents), root):
            try:
                info = parent.lstat()
            except FileNotFoundError:
                return
            if (not stat.S_ISDIR(info.st_mode)
                    or getattr(info, "st_file_attributes", 0)
                    & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)):
                _fail("unsafe build root on unsupported receipt platform")
        try:
            (root / RECEIPT_NAME).lstat()
        except FileNotFoundError:
            return
    except OSError as error:
        raise WorkspaceError("prebuilt receipt: cannot inspect unsupported-platform build root") from error
    _fail("existing receipt requires retirement on Linux before this build")


def invalidate(root):
    """Atomically retire our receipt under the caller's build lock.

    Retired entries are preserved as evidence. A concurrent replacement claimed
    at the rename boundary is retained and causes failure before build mutation.
    """
    if platform.system() != "Linux":
        _invalidate_unsupported(root)
        return
    with _opened(root, directory=True, missing_ok=True, owned=True) as directory:
        if directory is None:
            return
        try:
            before = os.stat(RECEIPT_NAME, dir_fd=directory, follow_symlinks=False)
        except FileNotFoundError:
            return
        _regular(before, private=True)
        _retire(directory, RECEIPT_NAME, before)


def publish(root, plan, producer, inputs):
    """Publish complete evidence, retracting it on caught post-install failures.

    Rename-noreplace is the atomic visibility point; directory fsync is the
    durability point. A process crash between them may leave a complete receipt:
    all build work and the receipt-file fsync already succeeded. Caught failures
    retire the exact installed object through the same claim protocol as
    invalidation and remove unpublished staging files after claiming their exact
    inode. Concurrent replacements survive in place or as quarantine evidence.
    """
    with _opened(root, directory=True, owned=True) as directory:
        root_observed = os.fstat(directory)
    value = {"schema_version": 1, "build_root": _root_identity(root),
             "plan": plan, "producer": producer, "inputs": inputs}
    _validate(value, root)
    raw = _canonical(value) + b"\n"
    if len(raw) > MAX_RECEIPT_BYTES:
        _fail("receipt exceeds size limit")
    rollback_fd, expected, temporary = None, None, None
    installation_attempted = False
    try:
        with _opened(root, directory=True, owned=True) as directory:
            info = os.fstat(directory)
            if _inode(info) != _inode(root_observed):
                _fail("build root changed before publication")
            rollback_fd = os.dup(directory)
            temporary = ".atrinik-prebuilt-pending-" + secrets.token_hex(16)
            fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_CLOEXEC,
                         0o600, dir_fd=directory)
            with os.fdopen(fd, "wb") as stream:
                expected = os.fstat(stream.fileno())
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
                expected = os.fstat(stream.fileno())
                _regular(expected, private=True)
            installation_attempted = True
            _rename_noreplace(directory, temporary, RECEIPT_NAME)
            installed = os.stat(RECEIPT_NAME, dir_fd=directory, follow_symlinks=False)
            if _inode(installed) != _inode(expected):
                _fail("receipt changed during publication")
            _regular(installed, private=True)
            os.fsync(directory)
    except BaseException as error:
        if expected is not None and rollback_fd is not None:
            try:
                names = ([RECEIPT_NAME] if installation_attempted else []) + [temporary]
                for name in names:
                    try:
                        visible = os.stat(name, dir_fd=rollback_fd, follow_symlinks=False)
                    except FileNotFoundError:
                        continue
                    # Known replacements stay in place; replacements racing the
                    # atomic claim survive as quarantine evidence.
                    if _inode(visible) != _inode(expected):
                        continue
                    retired = _retire(rollback_fd, name, expected)
                    if name == temporary:
                        # Only our unpublished staging inode is disposable.
                        # Claim it under a fresh nonce before removing it, rather
                        # than unlinking the externally known pending name.
                        os.unlink(retired, dir_fd=rollback_fd)
                        os.fsync(rollback_fd)
            except BaseException as rollback_error:
                raise WorkspaceError(
                    "prebuilt receipt publication failed; rollback evidence requires inspection"
                ) from rollback_error
        raise
    finally:
        if rollback_fd is not None:
            os.close(rollback_fd)
    return hashlib.sha256(raw).hexdigest()
