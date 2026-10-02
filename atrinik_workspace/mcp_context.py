# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Bounded, read-only workspace inspection shared by CLI and MCP consumers."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from contextvars import ContextVar
from dataclasses import dataclass, field
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import subprocess
import sys
import threading
import time
from types import SimpleNamespace
from typing import Any

from .mcp_contract import (
    ContractError, Coordinate, canonical_json, dirty_fingerprint, paginate,
    read_regular, snapshot_fingerprint, validate_selector, _unique_object,
)
from .model import Manifest, Paths, WorkspaceError, validate_name
from .workspace import Workspace, _parse_worktree_porcelain

SCHEMA_VERSION = "atrinik.context/v1"
PROVIDER_VERSION = "1.0.0"
MAX_BYTES = 262144
_EXCLUDED = {".git", ".env", "workspace", "build", "credentials", "password", "secrets"}
_REQUEST: ContextVar[tuple[float, threading.Event] | None] = ContextVar("context_request", default=None)


@contextmanager
def request_scope(cancelled: threading.Event | None = None, timeout_ms: int = 5000):
    if not 0 < timeout_ms <= 5000:
        raise ContractError("TIMEOUT", "invalid request deadline")
    token = _REQUEST.set((time.monotonic() + timeout_ms / 1000, cancelled or threading.Event()))
    try:
        yield
    finally:
        _REQUEST.reset(token)


def check_request() -> None:
    request = _REQUEST.get()
    if request:
        if request[1].is_set():
            raise ContractError("CANCELLED", "request cancelled")
        if time.monotonic() >= request[0]:
            raise ContractError("TIMEOUT", "request deadline exceeded")


@contextmanager
def directory(path: Path):
    """Pin every ancestor without following symlinks; never create directories."""
    if not sys.platform.startswith("linux"):
        raise ContractError("UNSUPPORTED_OPERATION", "descriptor-pinned Git inspection requires Linux")
    if not path.is_absolute() or ".." in path.parts:
        raise ContractError("FORBIDDEN", "configured directory must be absolute")
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC
    fd = os.open("/", flags)
    try:
        for part in path.parts[1:]:
            check_request()
            next_fd = os.open(part, flags, dir_fd=fd)
            os.close(fd)
            fd = next_fd
        yield fd
    except OSError as error:
        raise ContractError("NOT_FOUND", "configured directory unavailable") from error
    finally:
        os.close(fd)


def _digest(value: object) -> str:
    return hashlib.sha256(canonical_json(value)).hexdigest()


def _git(root: Path, *arguments: str, maximum: int = MAX_BYTES) -> bytes:
    """Fixed internal Git reads, bounded while running, pinned checkout descriptor."""
    check_request()
    with directory(root) as fd:
        environment = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
        environment.update(GIT_OPTIONAL_LOCKS="0", GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL="/dev/null", LC_ALL="C")
        command = ["git", "--no-optional-locks", "-c", "core.fsmonitor=false", "-c", "core.untrackedCache=false",
                   "-c", "diff.external=", "-C", f"/proc/self/fd/{fd}", *arguments]
        try:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                       env=environment, pass_fds=(fd,))
        except OSError as error:
            raise ContractError("OFFLINE", "Git is unavailable") from error
        assert process.stdout is not None
        chunks = bytearray()
        deadline = time.monotonic() + 5
        try:
            with selectors.DefaultSelector() as poller:
                poller.register(process.stdout, selectors.EVENT_READ)
                while True:
                    check_request()
                    if time.monotonic() >= deadline:
                        raise ContractError("TIMEOUT", "Git observation exceeded deadline")
                    if not poller.select(0.02):
                        continue
                    data = os.read(process.stdout.fileno(), min(65536, maximum + 1 - len(chunks)))
                    if not data:
                        break
                    chunks.extend(data)
                    if len(chunks) > maximum:
                        raise ContractError("LIMIT_EXCEEDED", "Git observation exceeds limit")
            if process.wait(timeout=max(0.01, deadline - time.monotonic())):
                raise ContractError("NOT_FOUND", "Git coordinate unavailable")
            return bytes(chunks)
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            process.stdout.close()


def _text(data: bytes) -> str:
    try:
        value = data.decode("utf-8").strip()
    except UnicodeError as error:
        raise ContractError("INCOMPLETE", "invalid text encoding") from error
    if any(not char.isprintable() for char in value):
        raise ContractError("INCOMPLETE", "invalid text characters")
    return value


def _name(value: str) -> str:
    try:
        return validate_name(value, "selector")
    except WorkspaceError as error:
        raise ContractError("INVALID_ARGUMENT", "invalid selector") from error


def _safe_read(root: Path, path: str, maximum: int = MAX_BYTES) -> bytes:
    # read_regular pins relative components; the outer walk also pins all root ancestors.
    with directory(root) as fd:
        # /proc is only an internal descriptor bridge, never caller-selected.
        return read_regular(f"/proc/self/fd/{fd}/.", path, maximum)


def _source_selector(path: str):
    relative = validate_selector(path, _EXCLUDED)
    if any(part.casefold().startswith((".env.", "credentials.", "secrets.", "passwords."))
           or part.casefold().endswith((".pem", ".key", ".p12")) for part in relative.parts):
        raise ContractError("FORBIDDEN", "credential-like source excluded")
    return relative


def _dirty_content(root: Path, status: bytes) -> bytes:
    """Fingerprint bounded permitted dirty source, never generic Git diff payloads."""
    tokens = status.split(b"\0")
    fingerprints = []
    total = 0
    index = 0
    while index < len(tokens):
        record = tokens[index]
        index += 1
        if not record:
            continue
        if len(record) < 4 or record[2:3] != b" ":
            raise ContractError("INCOMPLETE", "invalid tracked status record")
        if record[:1] in (b"R", b"C") or record[1:2] in (b"R", b"C"):
            index += 1  # The old rename path is metadata, not another source read.
        if len(fingerprints) >= 100:
            raise ContractError("LIMIT_EXCEEDED", "dirty source observation exceeds limit")
        try:
            path = record[3:].decode("utf-8")
            _source_selector(path)
        except (UnicodeError, ContractError):
            fingerprints.append([hashlib.sha256(record).hexdigest(), "excluded"])
            continue
        stage = _git(root, "ls-files", "--stage", "-z", "--", ":(literal)" + path)
        if b"D" in record[:2]:
            content = b""
        else:
            content = _safe_read(root, path)
        total += len(content)
        if total > MAX_BYTES:
            raise ContractError("LIMIT_EXCEEDED", "dirty source observation exceeds byte limit")
        fingerprints.append([path, hashlib.sha256(stage).hexdigest(), hashlib.sha256(content).hexdigest()])
    return canonical_json(fingerprints)


@dataclass(frozen=True)
class Snapshot:
    root: Path
    coordinate: Coordinate
    identity: dict[str, Any]
    metadata: dict[str, Any]
    _service: ContextService = field(repr=False, compare=False)
    _selection: dict[str, Any] = field(repr=False, compare=False)

    def assert_current(self) -> None:
        check_request()
        if self._service.resolve(**self._selection).identity != self.identity:
            raise ContractError("STALE_COORDINATE", "selected coordinate changed")

    def read(self, path: str, max_bytes: int = MAX_BYTES) -> bytes:
        _source_selector(path)
        self.assert_current()
        tracked = _git(self.root, "ls-files", "-z", "--", ":(literal)" + path).split(b"\0")
        if path.encode() not in tracked:
            raise ContractError("FORBIDDEN", "resource is not tracked source")
        payload = _safe_read(self.root, path, max_bytes)
        self.assert_current()
        return payload

    def json(self) -> dict[str, Any]:
        return {"coordinate": self.coordinate.json(), **self.metadata,
                "snapshot": snapshot_fingerprint(self.identity), "freshness": "observed-uncached",
                "schema_version": SCHEMA_VERSION, "provider_version": PROVIDER_VERSION}


class ContextService:
    """No constructor leases, writes, implicit cwd, eager worktree scans, or cache."""

    def __init__(self, root: Path, authorization_identity: str = "local"):
        self.root = Path(root)
        if not authorization_identity or len(authorization_identity) > 1024:
            raise ContractError("UNAUTHORIZED", "authorization scope required")
        self.authorization_identity = authorization_identity
        with directory(self.root):
            pass

    def _model(self):
        raw = _safe_read(self.root, "components.json")
        try:
            manifest = Manifest.from_value(json.loads(raw, object_pairs_hook=_unique_object))
        except (ValueError, WorkspaceError, RecursionError) as error:
            raise ContractError("INCOMPLETE", "manifest is invalid") from error
        # Canonical profile validation is pure when retained_raw is supplied.
        # Do not call Workspace.__init__, Paths.ensure, or resolve_profile here.
        paths = Paths.discover(self.root)
        if paths.workspace != self.root / "workspace":
            raise ContractError("FORBIDDEN", "external workspace configuration requires explicit approval")
        return manifest, paths, hashlib.sha256(raw).hexdigest()

    def manifest(self) -> Manifest:
        """Return a fresh canonical manifest without constructor or lease effects."""
        return self._model()[0]

    def manifest_identity(self) -> str:
        return self._model()[2]

    def _profile(self, manifest: Manifest, paths: Paths, name: str):
        _name(name)
        raw = None if name in manifest.stacks else _safe_read(paths.profiles, name + ".json")
        if raw is not None:
            try:
                original = json.loads(raw, object_pairs_hook=_unique_object)
                for selector in original.get("components", {}).values():
                    if isinstance(selector, dict) and selector.get("kind") == "path":
                        with directory(Path(selector["value"])):
                            pass
            except (ValueError, TypeError, AttributeError, KeyError) as error:
                raise ContractError("INCOMPLETE", "invalid profile source selection") from error
        subject = SimpleNamespace(manifest=manifest, paths=paths)
        try:
            return Workspace._load_profile_file(subject, name, False, retained_raw=raw)
        except (WorkspaceError, KeyError, TypeError, ValueError) as error:
            raise ContractError("INCOMPLETE", "profile is invalid or unavailable") from error

    def _registry(self, root: Path):
        raw = _git(root, "worktree", "list", "--porcelain", "-z")
        records = []
        # Isolate corrupt historical entries without dropping healthy coordinates.
        for block in raw.split(b"\0\0"):
            if not block:
                continue
            try:
                records.extend(_parse_worktree_porcelain(block + b"\0\0"))
            except (ValueError, WorkspaceError):
                records.append({"incomplete": "true", "record_identity": hashlib.sha256(block).hexdigest()})
        seen = set()
        duplicates = set()
        for record in records:
            path = record.get("worktree")
            if path and path in seen:
                duplicates.add(path)
            seen.add(path)
        for record in records:
            if record.get("worktree") in duplicates:
                identity = _digest(record)
                record.clear()
                record.update(incomplete="true", record_identity=identity)
        if len(records) > 1000:
            raise ContractError("LIMIT_EXCEEDED", "worktree registry exceeds limit")
        return records, hashlib.sha256(raw).hexdigest()

    def resolve(self, profile: str = "default", component: str | None = None,
                role: str | None = None, worktree: str | None = None) -> Snapshot:
        check_request()
        manifest, paths, manifest_digest = self._model()
        selected_profile = self._profile(manifest, paths, profile)
        stack = manifest.stack(selected_profile["stack"])
        if component and role:
            raise ContractError("INVALID_ARGUMENT", "select component or role")
        selected = None
        if role:
            selected = stack.providers.get(_name(role))
        elif component:
            selected = next((item for item in stack.components if item.name == _name(component)), None)
        if (component or role) and selected is None:
            raise ContractError("NOT_FOUND", "provider unavailable in selected stack")
        if selected:
            checkout = manifest.checkout_for(selected)
            primary = paths.repositories / checkout.path
            selector = selected_profile["components"][selected.name]
            if selector["kind"] == "primary":
                root = primary
            elif selector["kind"] == "worktree":
                root = paths.worktrees / checkout.name / selector["value"]
            elif selector["kind"] == "path":
                root = Path(selector["value"])
            else:
                raise ContractError("FORBIDDEN", "historical source selector unavailable")
            repository = checkout.repository
            metadata = {"profile": profile, "stack": stack.name, "component": selected.name,
                        "checkout": checkout.name, "source": selected.source, "owner": repository,
                        "generation": selected.generation, "license": selected.license,
                        "roles": list(selected.provides), "build": manifest.effective_build(stack.name, selected),
                        "requires": list(selected.requires)}
        else:
            primary = root = self.root
            repository = "atrinik/atrinik"
            metadata = {"profile": profile, "stack": stack.name, "component": "atrinik", "checkout": "atrinik",
                        "source": ".", "owner": repository, "generation": "shared", "license": "MIT",
                        "roles": [], "build": "none", "requires": []}
        records, registry_digest = self._registry(primary)
        if worktree:
            if not re.fullmatch(r"[0-9a-f]{64}", worktree):
                raise ContractError("INVALID_ARGUMENT", "invalid registered worktree identity")
            matches = [record for record in records if _digest([repository, record.get("worktree")]) == worktree]
            if len(matches) != 1:
                raise ContractError("NOT_FOUND", "registered worktree unavailable")
            root = Path(matches[0]["worktree"])
        if str(root) not in {record.get("worktree") for record in records}:
            raise ContractError("FORBIDDEN", "selected checkout is not registered")
        with directory(root) as fd:
            inode = [os.fstat(fd).st_dev, os.fstat(fd).st_ino]
        if selected and selected.source != ".":
            with directory(root / selected.source):
                pass
        # Prove the selected root belongs to the expected physical checkout.
        actual_top = _text(_git(root, "rev-parse", "--show-toplevel"))
        if actual_top != str(root):
            raise ContractError("FORBIDDEN", "selected root is not a checkout root")
        origin = _text(_git(root, "config", "--get", "remote.origin.url"))
        if origin not in {f"https://github.com/{repository}.git", f"https://github.com/{repository}",
                          f"git@github.com:{repository}.git", f"ssh://git@github.com/{repository}.git"}:
            raise ContractError("FORBIDDEN", "repository origin does not match manifest")
        head = _text(_git(root, "rev-parse", "--verify", "HEAD"))
        branch_raw = _text(_git(root, "rev-parse", "--abbrev-ref", "HEAD"))
        branch = branch_raw if branch_raw != "HEAD" else "detached"
        status = _git(root, "status", "--porcelain=v1", "-z", "--untracked-files=no")
        diff = _dirty_content(root, status) if status else b""
        if selected and selected.checkout_name == "content":
            main = _text(_git(root, "rev-parse", "--verify", "refs/heads/main"))
            ancestor = _text(_git(root, "merge-base", main, head))
            if branch != "main" and (root == primary or ancestor != main):
                raise ContractError("FORBIDDEN", "content review must be based on main")
            metadata["main_base_commit"] = main if branch != "main" else head
        coordinate = Coordinate.from_mapping({"repository": repository, "branch": branch, "commit": head,
            "worktree": _digest([repository, str(root)]), "dirty_fingerprint": dirty_fingerprint(status, diff)})
        identity = {**coordinate.json(), "manifest": manifest_digest, "profile": _digest(selected_profile),
                    "registry": registry_digest, "authorization": _digest(self.authorization_identity),
                    "schema_version": SCHEMA_VERSION, "provider_version": PROVIDER_VERSION,
                    "root_identity": inode, "selection": metadata}
        if (_text(_git(root, "rev-parse", "--verify", "HEAD")) != head
                or _git(root, "status", "--porcelain=v1", "-z", "--untracked-files=no") != status
                or (status and _dirty_content(root, status) != diff)):
            raise ContractError("STALE_COORDINATE", "checkout changed during observation")
        return Snapshot(root, coordinate, identity, metadata, self,
                        {"profile": profile, "component": component, "role": role, "worktree": worktree})

    def _page(self, records, identity, page_size=20, cursor=None):
        wrapper = self.resolve()
        result = paginate(records, page_size=page_size, cursor=cursor,
                          snapshot_identity={**identity, "wrapper": wrapper.identity, "authorization": _digest(self.authorization_identity),
                                             "schema": SCHEMA_VERSION, "provider": PROVIDER_VERSION,
                                             "page_size": page_size})
        wrapper.assert_current()
        result.update(wrapper.json())
        result.update(incomplete=any(item.get("incomplete", False) for item in result["items"]))
        if len(canonical_json(result)) > 32768:
            raise ContractError("CONTEXT_BUDGET_EXCEEDED", "page exceeds routine result budget")
        return result

    def describe(self, profile="default", page_size=20, cursor=None):
        manifest, paths, digest = self._model()
        chosen = self._profile(manifest, paths, profile)
        stack = manifest.stack(chosen["stack"])
        def closure(component):
            seen = set()
            def visit(item, depth):
                if depth > 8 or len(seen) > 1000:
                    raise ContractError("LIMIT_EXCEEDED", "dependency graph exceeds limit")
                for role in item.requires:
                    provider = stack.providers[role]
                    if provider.name not in seen:
                        seen.add(provider.name)
                        visit(provider, depth + 1)
            visit(component, 0)
            return sorted(seen)
        rows = [{"component": item.name, "checkout": item.checkout_name, "repository": item.repository,
                 "source": item.source, "generation": item.generation, "license": item.license,
                 "roles": list(item.provides), "requires": list(item.requires),
                 "cohorts": list(manifest.component_cohorts(item.name)),
                 "providers": {role: stack.providers[role].name for role in item.requires},
                 "dependency_closure": closure(item),
                 "build": manifest.effective_build(stack.name, item), "profile": profile,
                 "stack": stack.name} for item in stack.components]
        return self._page(rows, {"manifest": digest, "profile": chosen}, page_size, cursor)

    def list_profiles(self, page_size=20, cursor=None, kind="profiles", name_prefix=None):
        if name_prefix is not None:
            _name(name_prefix)
        if kind != "profiles" and name_prefix is not None:
            raise ContractError("INVALID_ARGUMENT", "name filter applies to profiles")
        if kind != "profiles":
            return self.registrations(kind, page_size, cursor)
        manifest, paths, digest = self._model()
        names = set(manifest.stacks)
        try:
            with directory(paths.profiles) as fd:
                for entry in os.scandir(fd):
                    check_request()
                    if entry.name.endswith(".json"):
                        names.add(entry.name[:-5])
                    if len(names) > 1000:
                        raise ContractError("LIMIT_EXCEEDED", "profile registry exceeds limit")
        except ContractError as error:
            if error.code != "NOT_FOUND":
                raise
        rows = []
        for name in sorted(names):
            if name_prefix is not None and not name.startswith(name_prefix):
                continue
            try:
                profile = self._profile(manifest, paths, name)
                rows.append({"name": name, "stack": profile["stack"], "identity": _digest(profile), "incomplete": False})
            except ContractError:
                rows.append({"name": _digest(name), "incomplete": True, "error": "INCOMPLETE"})
        return self._page(rows, {"manifest": digest, "profiles": rows, "filter": name_prefix}, page_size, cursor)

    def list_worktrees(self, profile="default", component=None, role=None, page_size=20, cursor=None):
        snapshot = self.resolve(profile, component, role)
        records, digest = self._registry(snapshot.root)
        rows = []
        for record in records:
            path = record.get("worktree", "")
            head = record.get("HEAD", "")
            valid = bool(re.fullmatch(r"[0-9a-f]{40}", head)) and bool(path)
            rows.append({"worktree": _digest([snapshot.coordinate.repository, path, record.get("record_identity")]) if not valid else _digest([snapshot.coordinate.repository, path]),
                         "repository": snapshot.coordinate.repository, "commit": head if valid else None,
                         "branch": record.get("branch", "detached").removeprefix("refs/heads/"),
                         "locked": "locked" in record, "prunable": "prunable" in record,
                         "incomplete": not valid})
        return self._page(rows, {"coordinate": snapshot.identity, "registry": digest}, page_size, cursor)

    def registrations(self, kind, page_size=20, cursor=None):
        """Only registered directory names; never read mutable state payloads."""
        if kind not in {"topologies", "states", "scenarios"}:
            raise ContractError("INVALID_ARGUMENT", "unknown registry")
        snapshot = self.resolve()
        _, paths, _ = self._model()
        root = {"topologies": paths.topologies, "states": paths.state, "scenarios": paths.scenarios}[kind]
        rows = []
        if kind == "states":
            try:
                raw = _safe_read(paths.workspace, "states.json")
                registry = json.loads(raw)
                if not isinstance(registry, dict) or registry.get("schema_version") != 1 or not isinstance(registry.get("states"), dict):
                    raise ContractError("INCOMPLETE", "invalid states registry")
                if len(registry["states"]) > 1000:
                    raise ContractError("LIMIT_EXCEEDED", "registry exceeds limit")
                for name, destination in registry["states"].items():
                    try:
                        _name(name)
                        valid = isinstance(destination, str) and Path(destination).is_absolute()
                        rows.append({"name": name, "kind": kind, "incomplete": not valid,
                                     "health": "uninspected", "identity": _digest([kind, name, destination])})
                    except ContractError:
                        rows.append({"identity": _digest(name), "kind": kind, "incomplete": True})
            except (ValueError, ContractError) as error:
                if isinstance(error, ContractError) and error.code == "LIMIT_EXCEEDED":
                    raise
                rows = [{"kind": kind, "incomplete": True, "error": "INCOMPLETE"}]
            return self._page(rows, {"coordinate": snapshot.identity, "kind": kind, "records": rows}, page_size, cursor)
        try:
            with directory(root) as fd:
                with os.scandir(fd) as entries:
                    for entry in entries:
                        check_request()
                        if len(rows) >= 1000:
                            raise ContractError("LIMIT_EXCEEDED", "registry exceeds limit")
                        try:
                            _name(entry.name)
                            valid = entry.is_dir(follow_symlinks=False)
                            rows.append({"name": entry.name, "kind": kind, "incomplete": not valid,
                                         "health": "uninspected", "identity": _digest([kind, entry.name])})
                        except ContractError:
                            rows.append({"identity": _digest([kind, entry.name]), "kind": kind, "incomplete": True})
        except ContractError as error:
            if error.code != "NOT_FOUND":
                raise
        snapshot.assert_current()
        return self._page(rows, {"coordinate": snapshot.identity, "kind": kind, "records": rows}, page_size, cursor)

    def guidance(self, **selection):
        snapshot = self.resolve(**selection)
        source = snapshot.metadata["source"]
        parts = [] if source == "." else source.split("/")
        candidates = ["AGENTS.md"] + ["/".join(parts[:n] + ["AGENTS.md"]) for n in range(1, len(parts) + 1)]
        tracked = set(_git(snapshot.root, "ls-files", "-z").decode().split("\0"))
        guides = [path for path in candidates if path in tracked]
        # Names and exact resource identities only; never automatically attach guidance text.
        skills = sorted(path for path in tracked if path.startswith(".agents/skills/") and path.endswith("/SKILL.md"))
        snapshot.assert_current()
        return {**snapshot.json(), "guidance": guides, "skills": skills[:50], "truncated": len(skills) > 50,
                "validation_guides": [path for path in ("CONTRIBUTING.md", "AGENTS.md") if path in tracked]}

    def changes(self, profile="default", component=None, role=None, worktree=None, page_size=20, cursor=None):
        snapshot = self.resolve(profile, component, role, worktree)
        paths = _git(snapshot.root, "diff", "--name-only", "-z", "HEAD", "--").split(b"\0")
        rows = []
        for raw in paths:
            if not raw:
                continue
            try:
                path = raw.decode()
                validate_selector(path, _EXCLUDED)
                rows.append({"path": path, "manifest_impact": path == "components.json"})
            except (UnicodeError, ContractError):
                rows.append({"incomplete": True, "error": "FORBIDDEN"})
        snapshot.assert_current()
        return {**self._page(rows, snapshot.identity, page_size, cursor), **snapshot.json()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("operation", choices=("describe", "resolve", "profiles", "worktrees", "guidance", "changes"))
    parser.add_argument("--profile", default="default")
    parser.add_argument("--component")
    args = parser.parse_args(argv)
    try:
        service = ContextService(args.root)
        with request_scope():
            if args.operation == "profiles":
                result = service.list_profiles()
            elif args.operation == "describe":
                result = service.describe(args.profile)
            else:
                operation = "list_worktrees" if args.operation == "worktrees" else args.operation
                result = getattr(service, operation)(profile=args.profile, component=args.component)
                if isinstance(result, Snapshot):
                    result = result.json()
        print(canonical_json(result).decode())
        return 0
    except ContractError as error:
        print(canonical_json({"error": error.code}).decode())
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
