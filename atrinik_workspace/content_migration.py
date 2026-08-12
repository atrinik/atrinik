from __future__ import annotations

import base64
import binascii
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile
from typing import Any, TextIO

from .locking import inherit_lock_fds
from .model import WorkspaceError, atomic_json, load_json
from .process_tree import holders_exist
from .supervisor import process_matches


CONTENT_MIGRATION_NAME = "content"
CONTENT_MIGRATION_RECORD = "migrations/content.json"
CONTENT_MIGRATION_PENDING = "migrations/content.pending.json"
CONTENT_MIGRATION_SCHEMA_VERSION = 1
CERTIFIED_MAIN_COMMIT = "7dde0c0afe8840fc95dd26f404310e77d9c82621"
CERTIFIED_1X_COMMIT = "566bd25f78b80b08d5f75f4b02017ab2429204db"
PROFILE_MAX_BYTES = 1024 * 1024
RESOURCE_RECORD_MAX_BYTES = 4 * 1024 * 1024
MIGRATION_RECORD_MAX_BYTES = 64 * 1024 * 1024
PROFILE_KEYS = {"schema_version", "name", "stack", "components", "sound_mode"}
LEGACY_PROFILE_KEYS = {"schema_version", "name", "stack", "components"}
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
OPERATION_PATHS = (
    "BISECT_LOG",
    "CHERRY_PICK_HEAD",
    "MERGE_HEAD",
    "REVERT_HEAD",
    "rebase-apply",
    "rebase-merge",
    "sequencer",
)


def _refusal(code: str, message: str, recovery: str) -> dict[str, str]:
    return {"code": code, "message": message, "recovery": recovery}


def _certified_parity() -> dict[str, str]:
    return {
        "issue": "https://github.com/atrinik/content/issues/166",
        "main_commit": CERTIFIED_MAIN_COMMIT,
        "main_release": "v2.14.0",
        "final_1x_commit": CERTIFIED_1X_COMMIT,
        "final_1x_release": "v1.8.19",
        "status": "certified",
    }


def _git(path: Path, *arguments: str, check: bool = True) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(path), *arguments],
            check=check,
            capture_output=True,
            text=True,
            pass_fds=(),
        )
    except FileNotFoundError as error:
        raise WorkspaceError("required command not found: git") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.strip() or error.stdout.strip()
        suffix = f": {detail}" if detail else ""
        raise WorkspaceError(
            f"git {' '.join(arguments)} failed in {path}{suffix}"
        ) from error
    return result.stdout.strip()


def _git_succeeds(path: Path, *arguments: str) -> bool:
    try:
        return subprocess.run(
            ["git", "-C", str(path), *arguments],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            pass_fds=(),
        ).returncode == 0
    except FileNotFoundError as error:
        raise WorkspaceError("required command not found: git") from error


def _remote_matches(url: str) -> bool:
    normalized = url.strip().removesuffix(".git")
    if normalized.startswith("git@github.com:"):
        normalized = "github.com/" + normalized.removeprefix("git@github.com:")
    elif normalized.startswith("ssh://git@github.com/"):
        normalized = "github.com/" + normalized.removeprefix(
            "ssh://git@github.com/"
        )
    elif normalized.startswith("https://"):
        normalized = normalized.removeprefix("https://")
    return normalized == "github.com/atrinik/content"


def _digest(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise WorkspaceError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _atomic_bytes(path: Path, value: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.content-migration-", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(value)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(
            path.parent, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
        )
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def _open_layout_lock(path: Path) -> TextIO:
    flags = os.O_RDWR | os.O_CREAT | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(path, flags, 0o600)
    except OSError as error:
        raise WorkspaceError(f"cannot open repository layout lock {path}: {error}") from error
    if not stat.S_ISREG(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise WorkspaceError(f"repository layout lock is not a regular file: {path}")
    return os.fdopen(descriptor, "a+")


class ContentMigration:
    """Retire active ``content-1x`` selectors without rewriting history."""

    def __init__(self, repository_root: Path, workspace_paths: Any, manifest: Any):
        self.repository_root = Path(repository_root).resolve()
        self.paths = workspace_paths
        self.manifest = manifest
        self.workspace = Path(workspace_paths.workspace).resolve()
        self.record_path = self.workspace / CONTENT_MIGRATION_RECORD
        self.pending_path = self.workspace / CONTENT_MIGRATION_PENDING
        self.canonical = self.repository_root / "content"
        self.legacy = self.repository_root / "content-1x"

    def execute(self, mode: str) -> dict[str, Any]:
        if mode not in {"dry-run", "apply", "audit", "restore"}:
            raise WorkspaceError(f"unsupported content migration mode: {mode}")
        if mode == "audit":
            return self._audit()
        if mode == "restore":
            return self._locked_restore()
        inspection = self._inspect()
        if mode == "dry-run" or inspection["refusals"]:
            return inspection
        if inspection["status"] == "not-needed":
            return inspection
        return self._locked_apply()

    def _locked_apply(self) -> dict[str, Any]:
        lock_path = self.workspace / "repository-layout.lock"
        with _open_layout_lock(lock_path) as lock, inherit_lock_fds(lock):
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                plan = self._inspect()
                plan["refusals"].append(
                    _refusal(
                        "repository_layout_busy",
                        "the repository layout is in use by another wrapper operation",
                        "wait for that operation to finish and rerun the migration",
                    )
                )
                plan["status"] = "refused"
                return plan
            if self.record_path.is_file() and not self.record_path.is_symlink():
                result = self._audit()
                if result["status"] == "complete":
                    result["status"] = "already-applied"
                elif result["status"] == "restored":
                    result["status"] = "refused"
                    result["refusals"] = [
                        _refusal(
                            "migration_already_restored",
                            "the recorded content migration was explicitly restored",
                            "preserve the record; a new apply requires a separately "
                            "reviewed migration transaction",
                        )
                    ]
                return result
            if self.record_path.exists() or self.record_path.is_symlink():
                return {
                    "migration": CONTENT_MIGRATION_NAME,
                    "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
                    "status": "refused",
                    "refusals": [
                        _refusal(
                            "invalid_migration_record",
                            f"content migration record is unsafe: {self.record_path}",
                            "preserve the path and restore the exact regular migration record",
                        )
                    ],
                }
            if self.pending_path.exists() or self.pending_path.is_symlink():
                return {
                    "migration": CONTENT_MIGRATION_NAME,
                    "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
                    "status": "refused",
                    "refusals": [
                        _refusal(
                            "pending_migration",
                            f"an interrupted migration journal exists: {self.pending_path}",
                            "preserve it and restore only its exact recorded "
                            "profile bytes and worktree paths",
                        )
                    ],
                }
            inspection = self._inspect()
            if inspection["refusals"] or inspection["status"] == "not-needed":
                return inspection
            return self._apply(inspection)

    def _locked_restore(self) -> dict[str, Any]:
        lock_path = self.workspace / "repository-layout.lock"
        with _open_layout_lock(lock_path) as lock, inherit_lock_fds(lock):
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return {
                    "migration": CONTENT_MIGRATION_NAME,
                    "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
                    "status": "refused",
                    "refusals": [
                        _refusal(
                            "repository_layout_busy",
                            "the repository layout is in use by another wrapper operation",
                            "wait for that operation to finish and rerun the restore",
                        )
                    ],
                }
            return self._restore()

    def _inspect(self) -> dict[str, Any]:
        refusals: list[dict[str, str]] = []
        canonical = self._inspect_checkout(
            self.canonical,
            "main",
            CERTIFIED_MAIN_COMMIT,
            required=True,
            require_clean=True,
            refusals=refusals,
        )
        legacy = self._inspect_checkout(
            self.legacy,
            "1.x",
            CERTIFIED_1X_COMMIT,
            required=False,
            require_clean=False,
            refusals=refusals,
        )
        profiles, moves, profile_refusals = self._inspect_profiles(
            canonical, legacy
        )
        refusals.extend(profile_refusals)
        changed = [row for row in profiles if row["status"] == "rewrite"]
        resources, resource_refusals = self._resource_inventory(
            {row["name"] for row in changed}
        )
        refusals.extend(resource_refusals)
        refusals.sort(key=lambda row: (row["code"], row["message"]))
        return {
            "migration": CONTENT_MIGRATION_NAME,
            "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
            "status": "refused" if refusals else ("ready" if changed else "not-needed"),
            "certified": {
                "main": CERTIFIED_MAIN_COMMIT,
                "1.x": CERTIFIED_1X_COMMIT,
            },
            "parity_proof": _certified_parity(),
            "canonical": canonical,
            "legacy": legacy,
            "profiles": profiles,
            "worktree_moves": moves,
            "resources": resources,
            "refusals": refusals,
        }

    def _inspect_checkout(
        self,
        path: Path,
        branch: str,
        anchor: str,
        *,
        required: bool,
        require_clean: bool,
        refusals: list[dict[str, str]],
    ) -> dict[str, Any]:
        row: dict[str, Any] = {
            "path": str(path),
            "repository": "atrinik/content",
            "expected_branch": branch,
            "anchor": anchor,
            "present": path.exists() or path.is_symlink(),
            "head": None,
            "tree": None,
            "branch": None,
            "clean": None,
            "common_directory": None,
            "proven": False,
        }
        if not row["present"]:
            if required:
                refusals.append(
                    _refusal(
                        "canonical_content_missing",
                        f"canonical content checkout is missing: {path}",
                        "initialize content@main and rerun the dry run",
                    )
                )
            return row
        try:
            if path.is_symlink() or not path.is_dir():
                raise WorkspaceError("path is not a normal directory")
            if _git(path, "rev-parse", "--is-inside-work-tree") != "true":
                raise WorkspaceError("path is not a Git worktree")
            if Path(_git(path, "rev-parse", "--show-toplevel")).resolve() != path.resolve():
                raise WorkspaceError("path is not the Git worktree root")
            remote_found = False
            for remote in ("origin", "upstream"):
                output = _git(path, "remote", "get-url", "--all", remote, check=False)
                urls = output.splitlines()
                if urls and _remote_matches(urls[0]):
                    remote_found = True
                    break
            if not remote_found:
                raise WorkspaceError("no canonical atrinik/content origin or upstream")
            actual_branch = _git(path, "branch", "--show-current")
            head = _git(path, "rev-parse", "HEAD")
            tree = _git(path, "rev-parse", "HEAD^{tree}")
            common = Path(_git(path, "rev-parse", "--git-common-dir"))
            if not common.is_absolute():
                common = path / common
            clean = not _git(
                path, "status", "--porcelain", "--untracked-files=normal"
            )
            if actual_branch != branch:
                raise WorkspaceError(
                    f"expected branch {branch}, found {actual_branch or 'detached HEAD'}"
                )
            if not _git_succeeds(path, "merge-base", "--is-ancestor", anchor, head):
                raise WorkspaceError(f"HEAD does not descend from certified commit {anchor}")
            if require_clean and not clean:
                raise WorkspaceError("checkout is dirty")
            row.update(
                {
                    "head": head,
                    "tree": tree,
                    "branch": actual_branch,
                    "clean": clean,
                    "common_directory": str(common.resolve()),
                    "proven": True,
                }
            )
        except (OSError, WorkspaceError) as error:
            code = "canonical_content_unproven" if required else "legacy_content_unproven"
            row["error"] = str(error)
            if required:
                refusals.append(
                    _refusal(
                        code,
                        f"cannot prove {path}: {error}",
                        "preserve the checkout and repair its exact repository, "
                        "branch, ancestry, and cleanliness",
                    )
                )
        return row

    def _inspect_profiles(
        self, canonical: dict[str, Any], legacy: dict[str, Any]
    ) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, str]]]:
        rows: list[dict[str, Any]] = []
        moves: list[dict[str, Any]] = []
        refusals: list[dict[str, str]] = []
        profiles_root = Path(self.paths.profiles)
        if not profiles_root.exists():
            return rows, moves, refusals
        if profiles_root.is_symlink() or not profiles_root.is_dir():
            return rows, moves, [
                _refusal(
                    "invalid_profiles_directory",
                    f"profiles path is not a normal directory: {profiles_root}",
                    "restore the wrapper-owned profiles directory",
                )
            ]
        new_components = {
            component.name for component in self.manifest.stack("classic").components
        }
        old_components = (new_components - {"content"}) | {"content-1x"}
        for path in sorted(profiles_root.glob("*.json")):
            try:
                if path.is_symlink() or not path.is_file():
                    raise WorkspaceError("profile is not a regular file")
                if path.stat().st_size > PROFILE_MAX_BYTES:
                    raise WorkspaceError("profile exceeds the bounded migration size")
                original = path.read_bytes()
                if len(original) > PROFILE_MAX_BYTES:
                    raise WorkspaceError("profile exceeds the bounded migration size")
                value = json.loads(original, object_pairs_hook=_reject_duplicate_keys)
                if not isinstance(value, dict) or value.get("name") != path.stem:
                    raise WorkspaceError("profile identity is invalid")
                schema_version = value.get("schema_version")
                expected_keys = (
                    LEGACY_PROFILE_KEYS if schema_version == 3 else PROFILE_KEYS
                )
                if schema_version not in {3, 4} or set(value) != expected_keys:
                    raise WorkspaceError("profile schema is unsupported")
                if schema_version == 4 and value.get("sound_mode") not in {
                    "source",
                    "local-playtest",
                }:
                    raise WorkspaceError("profile sound mode is invalid")
                components = value.get("components")
                if value.get("stack") != "classic":
                    rows.append(
                        {
                            "name": path.stem,
                            "path": str(path),
                            "status": "inert",
                            "reason": "non-classic profile",
                        }
                    )
                    continue
                if not isinstance(components, dict):
                    raise WorkspaceError("profile components are invalid")
                if set(components) == new_components:
                    self._validate_current_profile_components(components)
                    rows.append(
                        {
                            "name": path.stem,
                            "path": str(path),
                            "status": "current",
                            "reason": "already uses content@main",
                        }
                    )
                    continue
                if set(components) != old_components:
                    raise WorkspaceError(
                        "classic profile component set is neither legacy nor current"
                    )
                selector = components["content-1x"]
                replacement, move = self._migrate_selector(
                    path.stem, selector, canonical, legacy
                )
                rewritten = dict(value)
                rewritten_components = dict(components)
                rewritten_components.pop("content-1x")
                rewritten_components["content"] = replacement
                self._validate_current_profile_components(rewritten_components)
                rewritten["components"] = rewritten_components
                replacement_bytes = (
                    json.dumps(rewritten, indent=2, sort_keys=True).encode("utf-8")
                    + b"\n"
                )
                row = {
                    "name": path.stem,
                    "path": str(path),
                    "status": "rewrite",
                    "selector": replacement,
                    "original_sha256": _digest(original),
                    "original_base64": base64.b64encode(original).decode("ascii"),
                    "replacement_sha256": _digest(replacement_bytes),
                    "replacement_base64": base64.b64encode(replacement_bytes).decode(
                        "ascii"
                    ),
                }
                rows.append(row)
                if move is not None:
                    move["profile"] = path.stem
                    moves.append(move)
            except (
                OSError,
                UnicodeError,
                json.JSONDecodeError,
                RecursionError,
                WorkspaceError,
            ) as error:
                rows.append(
                    {
                        "name": path.stem,
                        "path": str(path),
                        "status": "blocked",
                        "reason": str(error),
                    }
                )
                refusals.append(
                    _refusal(
                        "profile_unproven",
                        f"cannot migrate profile {path.stem}: {error}",
                        "preserve the profile and repair only its exact selector evidence",
                    )
                )
        destinations: dict[str, str] = {}
        for move in moves:
            previous = destinations.setdefault(move["destination"], move["profile"])
            if previous != move["profile"]:
                refusals.append(
                    _refusal(
                        "worktree_destination_collision",
                        f"profiles {previous} and {move['profile']} select the "
                        f"same migration destination {move['destination']}",
                        "give each proven worktree a distinct managed label",
                    )
                )
        return rows, moves, refusals

    def _validate_current_profile_components(
        self, components: dict[str, Any]
    ) -> None:
        checkout_selectors: dict[str, dict[str, str]] = {}
        for component_name, selector in components.items():
            if (
                component_name not in self.manifest.by_name
                or not isinstance(selector, dict)
                or set(selector) != {"kind", "value"}
                or not isinstance(selector.get("kind"), str)
                or not isinstance(selector.get("value"), str)
            ):
                raise WorkspaceError("profile selector is invalid")
            kind = selector["kind"]
            value = selector["value"]
            if kind == "primary":
                if value:
                    raise WorkspaceError("primary selector has a value")
            elif kind == "worktree":
                if not NAME_PATTERN.fullmatch(value):
                    raise WorkspaceError("managed worktree selector is invalid")
            elif kind == "path":
                if not Path(value).is_absolute():
                    raise WorkspaceError("external path selector is not absolute")
            else:
                raise WorkspaceError(
                    f"selector kind is not valid after content migration: {kind}"
                )
            checkout = self.manifest.by_name[component_name].checkout_name
            previous = checkout_selectors.setdefault(checkout, selector)
            if previous != selector:
                raise WorkspaceError(
                    f"profile selectors disagree for shared checkout {checkout}"
                )

    def _migrate_selector(
        self,
        profile: str,
        selector: Any,
        canonical: dict[str, Any],
        legacy: dict[str, Any],
    ) -> tuple[dict[str, str], dict[str, Any] | None]:
        if not isinstance(selector, dict) or set(selector) != {"kind", "value"}:
            raise WorkspaceError("content-1x selector is invalid")
        kind, value = selector["kind"], selector["value"]
        if not isinstance(kind, str) or not isinstance(value, str):
            raise WorkspaceError("content-1x selector is invalid")
        if not canonical.get("proven"):
            raise WorkspaceError("canonical content@main is not proven")
        if kind == "primary":
            if value:
                raise WorkspaceError("primary selector has a value")
            if (
                not legacy.get("proven")
                or not legacy.get("clean")
                or legacy.get("head") != CERTIFIED_1X_COMMIT
            ):
                raise WorkspaceError("legacy primary is not clean and certified")
            return {"kind": "primary", "value": ""}, None
        if kind == "worktree":
            if not NAME_PATTERN.fullmatch(value):
                raise WorkspaceError("managed worktree label is invalid")
            source, destination = self._managed_worktree_paths(value)
            proof = self._prove_main_worktree(source, canonical)
            if destination.exists() or destination.is_symlink():
                raise WorkspaceError(
                    f"managed worktree destination already exists: {destination}"
                )
            return {"kind": "worktree", "value": value}, {
                "source": str(source),
                "destination": str(destination),
                **proof,
            }
        if kind in {"path", "migrated-worktree"}:
            selected = Path(value)
            if not selected.is_absolute():
                raise WorkspaceError(f"{kind} selector is not absolute")
            if selected.resolve(strict=False) == self.canonical.resolve(strict=False):
                return {"kind": "primary", "value": ""}, None
            self._prove_main_worktree(selected, canonical)
            return {"kind": "path", "value": str(selected.resolve())}, None
        raise WorkspaceError(f"unsupported selector kind {kind}")

    def _managed_worktree_paths(self, label: str) -> tuple[Path, Path]:
        if not NAME_PATTERN.fullmatch(label):
            raise WorkspaceError("managed worktree label is invalid")
        root = Path(self.paths.worktrees)
        if root.is_symlink() or not root.is_dir():
            raise WorkspaceError(
                f"managed worktree root is not a normal directory: {root}"
            )
        source_parent = root / "content-1x"
        destination_parent = root / "content"
        for parent in (source_parent, destination_parent):
            if parent.is_symlink() or parent.exists() and not parent.is_dir():
                raise WorkspaceError(
                    f"managed content worktree namespace is unsafe: {parent}"
                )
        return source_parent / label, destination_parent / label

    def _prove_main_worktree(
        self, path: Path, canonical: dict[str, Any]
    ) -> dict[str, str]:
        if path.is_symlink() or not path.is_dir():
            raise WorkspaceError(f"selected worktree is not a normal directory: {path}")
        if Path(_git(path, "rev-parse", "--show-toplevel")).resolve() != path.resolve():
            raise WorkspaceError(f"selected path is not a Git worktree root: {path}")
        branch = _git(path, "branch", "--show-current")
        if not branch:
            raise WorkspaceError(f"selected worktree is detached: {path}")
        head = _git(path, "rev-parse", "HEAD")
        tree = _git(path, "rev-parse", "HEAD^{tree}")
        common = Path(_git(path, "rev-parse", "--git-common-dir"))
        if not common.is_absolute():
            common = path / common
        if str(common.resolve()) != canonical["common_directory"]:
            raise WorkspaceError(
                f"selected worktree is not attached to canonical content: {path}"
            )
        if _git(
            path,
            "status",
            "--porcelain",
            "--untracked-files=normal",
        ):
            raise WorkspaceError(f"selected worktree is dirty: {path}")
        if not _git_succeeds(
            path,
            "merge-base",
            "--is-ancestor",
            CERTIFIED_MAIN_COMMIT,
            head,
        ):
            raise WorkspaceError(
                f"selected worktree does not descend from certified content@main: {path}"
            )
        for operation in OPERATION_PATHS:
            git_path = Path(_git(path, "rev-parse", "--git-path", operation))
            if not git_path.is_absolute():
                git_path = path / git_path
            if git_path.exists() or git_path.is_symlink():
                raise WorkspaceError(
                    f"selected worktree has in-progress Git operation {operation}: {path}"
                )
        records = _git(self.canonical, "worktree", "list", "--porcelain").splitlines()
        current: str | None = None
        for line in records:
            if line.startswith("worktree "):
                current = str(Path(line.removeprefix("worktree ")).resolve())
            elif line.startswith("locked") and current == str(path.resolve()):
                raise WorkspaceError(f"selected worktree is locked: {path}")
        return {"head": head, "tree": tree, "branch": branch}

    def _resource_inventory(
        self, changed_profiles: set[str]
    ) -> tuple[dict[str, list[dict[str, Any]]], list[dict[str, str]]]:
        resources: dict[str, list[dict[str, Any]]] = {
            "builds": [],
            "topologies": [],
            "scenarios": [],
            "locks": [],
            "historical_paths": [],
        }
        refusals: list[dict[str, str]] = []
        resources["historical_paths"].append(
            {
                "path": str(self.legacy),
                "coordinate": "atrinik/content@1.x",
                "present": self.legacy.exists() or self.legacy.is_symlink(),
                "disposition": "preserve",
            }
        )
        for root_name, root, pattern in (
            ("builds", Path(self.paths.builds), ".atrinik-build.json"),
            ("scenarios", Path(self.paths.scenarios), "scenario.json"),
        ):
            if not root.exists():
                continue
            if root.is_symlink() or not root.is_dir():
                refusals.append(
                    _refusal(
                        f"invalid_{root_name}_directory",
                        f"{root_name} path is not a normal directory: {root}",
                        f"restore the wrapper-owned {root_name} directory",
                    )
                )
                continue
            candidates = root.rglob(pattern)
            for path in sorted(candidates):
                if path.is_symlink() or not path.is_file():
                    refusals.append(
                        _refusal(
                            f"unobservable_{root_name}_record",
                            f"cannot safely inspect {root_name} record: {path}",
                            "preserve the path and restore a regular wrapper-owned record",
                        )
                    )
                    continue
                try:
                    if path.stat().st_size > RESOURCE_RECORD_MAX_BYTES:
                        raise WorkspaceError("record exceeds the bounded inventory size")
                    raw = path.read_bytes()
                except (OSError, RecursionError, WorkspaceError) as error:
                    refusals.append(
                        _refusal(
                            f"unobservable_{root_name}_record",
                            f"cannot inspect {root_name} record {path}: {error}",
                            "preserve the path and restore readable wrapper-owned metadata",
                        )
                    )
                    continue
                if b"content-1x" in raw or b'"branch": "1.x"' in raw:
                    resources[root_name].append(
                        {
                            "path": str(path),
                            "status": "historical-inert",
                            "sha256": _digest(raw),
                        }
                    )
        topologies = Path(self.paths.topologies)
        if topologies.exists() and (topologies.is_symlink() or not topologies.is_dir()):
            refusals.append(
                _refusal(
                    "invalid_topologies_directory",
                    f"topologies path is not a normal directory: {topologies}",
                    "restore or stop the topology before migration",
                )
            )
        elif topologies.is_dir():
            for directory in sorted(topologies.iterdir()):
                status_path = directory / "status.json"
                if directory.is_symlink():
                    refusals.append(
                        _refusal(
                            "unobservable_topology",
                            f"topology path is a link: {directory}",
                            "preserve the path and restore a normal wrapper-owned "
                            "topology directory",
                        )
                    )
                    continue
                if not directory.is_dir() or not status_path.exists():
                    continue
                try:
                    if status_path.is_symlink() or not status_path.is_file():
                        raise WorkspaceError("status is not a regular file")
                    if status_path.stat().st_size > RESOURCE_RECORD_MAX_BYTES:
                        raise WorkspaceError("status exceeds the bounded inventory size")
                    value = load_json(status_path)
                    if not isinstance(value, dict) or not isinstance(value.get("services"), dict):
                        raise WorkspaceError("unsupported topology status")
                    records = [value.get("supervisor"), *value["services"].values()]
                    running = []
                    for record in records:
                        if not isinstance(record, dict):
                            raise WorkspaceError("invalid topology process record")
                        pid, start = record.get("pid"), record.get("start_time")
                        if (
                            not isinstance(pid, int)
                            or isinstance(pid, bool)
                            or pid <= 0
                            or not isinstance(start, str)
                            or not start.isdigit()
                        ):
                            raise WorkspaceError("invalid topology process identity")
                        if process_matches(pid, start):
                            running.append(pid)
                    affected = (
                        value.get("profile") in changed_profiles
                        or "content-1x" in json.dumps(value, sort_keys=True)
                    )
                    resources["topologies"].append(
                        {
                            "name": directory.name,
                            "path": str(directory),
                            "profile": value.get("profile"),
                            "affected": affected,
                            "running_pids": running,
                            "status": "blocked" if affected and running else "historical-inert",
                        }
                    )
                    if affected and running:
                        refusals.append(
                            _refusal(
                                "live_topology",
                                f"affected topology is active: {directory.name}",
                                f"run ./atrinik down {directory.name} before migration",
                            )
                        )
                except (OSError, RecursionError, WorkspaceError) as error:
                    refusals.append(
                        _refusal(
                            "unobservable_topology",
                            f"cannot inspect topology {directory.name}: {error}",
                            "stop or repair the topology before migration",
                        )
                    )
        lock_roots = [
            self.workspace / "repository-layout.lock",
            Path(self.paths.builds) / "locks",
        ]
        for root in lock_roots:
            if root.is_symlink() or root.exists() and not (
                root.is_file() or root.is_dir()
            ):
                resources["locks"].append(
                    {"path": str(root), "active": True, "status": "unsafe"}
                )
                refusals.append(
                    _refusal(
                        "invalid_lock_path",
                        f"lock inventory path is unsafe: {root}",
                        "preserve the path and restore a regular lock file or directory",
                    )
                )
                continue
            paths = (
                [root]
                if root.is_file()
                else sorted(root.glob("*"))
                if root.is_dir()
                else []
            )
            for path in paths:
                if path.is_symlink() or not path.is_file():
                    resources["locks"].append(
                        {"path": str(path), "active": True, "status": "unsafe"}
                    )
                    refusals.append(
                        _refusal(
                            "invalid_lock_path",
                            f"lock inventory entry is unsafe: {path}",
                            "preserve the path and restore a regular lock file",
                        )
                    )
                    continue
                descriptor: int | None = None
                try:
                    descriptor = os.open(path, os.O_RDONLY | os.O_CLOEXEC)
                    active = holders_exist(descriptor, exclude={os.getpid()})
                except (OSError, WorkspaceError):
                    active = True
                finally:
                    if descriptor is not None:
                        os.close(descriptor)
                resources["locks"].append(
                    {"path": str(path), "active": active, "status": "observed"}
                )
        return resources, refusals

    def _apply(self, inspection: dict[str, Any]) -> dict[str, Any]:
        journal = {
            **inspection,
            "status": "pending",
            "started_at": datetime.now(timezone.utc).isoformat(),
        }
        atomic_json(self.pending_path, journal)
        applied_profiles: list[dict[str, Any]] = []
        applied_moves: list[dict[str, Any]] = []
        published = False
        try:
            for move in inspection["worktree_moves"]:
                source, destination = self._managed_worktree_paths(
                    Path(move["source"]).name
                )
                if str(source) != move["source"] or str(destination) != move["destination"]:
                    raise WorkspaceError("worktree migration paths changed after preflight")
                destination.parent.mkdir(parents=True, exist_ok=True)
                self._prove_main_worktree(source, inspection["canonical"])
                if destination.exists() or destination.is_symlink():
                    raise WorkspaceError(
                        f"worktree destination appeared during migration: {destination}"
                    )
                _git(self.canonical, "worktree", "move", str(source), str(destination))
                applied_moves.append(move)
            for row in inspection["profiles"]:
                if row["status"] != "rewrite":
                    continue
                path = Path(row["path"])
                current = path.read_bytes()
                if _digest(current) != row["original_sha256"]:
                    raise WorkspaceError(f"profile changed during migration: {path}")
                replacement = base64.b64decode(row["replacement_base64"], validate=True)
                _atomic_bytes(path, replacement)
                applied_profiles.append(row)
            record = {
                **inspection,
                "status": "complete",
                "applied_at": datetime.now(timezone.utc).isoformat(),
                "restore": {
                    "available": True,
                    "command": "./atrinik migrate content --restore --json",
                    "precondition": (
                        "all replacement profile bytes and worktree destinations "
                        "remain exact"
                    ),
                },
            }
            atomic_json(self.record_path, record)
            published = True
            try:
                self.pending_path.unlink()
            except OSError:
                return {
                    **record,
                    "pending_journal_retained": str(self.pending_path),
                }
            return record
        except BaseException as error:
            if published:
                raise WorkspaceError(
                    "content migration completed and its durable record was published, "
                    f"but final journal cleanup failed: {error}; preserve {self.record_path} "
                    f"and {self.pending_path} and run --audit"
                ) from error
            rollback_errors = self._rollback(applied_profiles, applied_moves)
            if not rollback_errors:
                self.pending_path.unlink(missing_ok=True)
            detail = f"content migration failed: {error}"
            if rollback_errors:
                detail += "; rollback failed: " + "; ".join(rollback_errors)
            raise WorkspaceError(detail) from error

    def _rollback(
        self,
        profiles: list[dict[str, Any]],
        moves: list[dict[str, Any]],
    ) -> list[str]:
        errors: list[str] = []
        for row in reversed(profiles):
            path = Path(row["path"])
            try:
                if _digest(path.read_bytes()) != row["replacement_sha256"]:
                    raise WorkspaceError(f"replacement profile changed: {path}")
                original = base64.b64decode(row["original_base64"], validate=True)
                _atomic_bytes(path, original)
            except BaseException as error:
                errors.append(str(error))
        for move in reversed(moves):
            try:
                source, destination = self._managed_worktree_paths(
                    Path(move["source"]).name
                )
                if source.exists() or source.is_symlink():
                    raise WorkspaceError(f"original worktree path is occupied: {source}")
                source.parent.mkdir(parents=True, exist_ok=True)
                _git(self.canonical, "worktree", "move", str(destination), str(source))
            except BaseException as error:
                errors.append(str(error))
        return errors

    def _load_record(self) -> dict[str, Any]:
        try:
            if self.record_path.stat().st_size > MIGRATION_RECORD_MAX_BYTES:
                raise WorkspaceError("record exceeds the bounded migration size")
            raw = self.record_path.read_bytes()
            if len(raw) > MIGRATION_RECORD_MAX_BYTES:
                raise WorkspaceError("record exceeds the bounded migration size")
            record = json.loads(raw, object_pairs_hook=_reject_duplicate_keys)
        except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
            raise WorkspaceError(f"cannot parse migration record: {error}") from error
        except OSError as error:
            raise WorkspaceError(f"cannot inspect migration record: {error}") from error
        if (
            not isinstance(record, dict)
            or record.get("migration") != CONTENT_MIGRATION_NAME
            or record.get("schema_version") != CONTENT_MIGRATION_SCHEMA_VERSION
            or record.get("status") not in {"complete", "restored"}
            or record.get("certified")
            != {"main": CERTIFIED_MAIN_COMMIT, "1.x": CERTIFIED_1X_COMMIT}
            or record.get("parity_proof") != _certified_parity()
            or not isinstance(record.get("canonical"), dict)
            or record["canonical"].get("path") != str(self.canonical)
            or record["canonical"].get("repository") != "atrinik/content"
            or record["canonical"].get("expected_branch") != "main"
            or record["canonical"].get("anchor") != CERTIFIED_MAIN_COMMIT
            or not isinstance(record["canonical"].get("common_directory"), str)
            or not Path(record["canonical"]["common_directory"]).is_absolute()
            or not isinstance(record.get("legacy"), dict)
            or record["legacy"].get("path") != str(self.legacy)
            or not isinstance(record.get("profiles"), list)
            or not isinstance(record.get("worktree_moves"), list)
        ):
            raise WorkspaceError("record has an unsupported shape")
        rewrite_names: set[str] = set()
        for row in record["profiles"]:
            if not isinstance(row, dict) or not isinstance(row.get("status"), str):
                raise WorkspaceError("record profile row is invalid")
            if row["status"] != "rewrite":
                continue
            name = row.get("name")
            if not isinstance(name, str) or not NAME_PATTERN.fullmatch(name):
                raise WorkspaceError("record profile name is invalid")
            expected_path = (Path(self.paths.profiles) / f"{name}.json").resolve(
                strict=False
            )
            raw_path = row.get("path")
            if (
                name in rewrite_names
                or not isinstance(raw_path, str)
                or raw_path != str(expected_path)
            ):
                raise WorkspaceError("record profile path is invalid")
            rewrite_names.add(name)
            for prefix in ("original", "replacement"):
                encoded = row.get(f"{prefix}_base64")
                digest = row.get(f"{prefix}_sha256")
                if (
                    not isinstance(encoded, str)
                    or not isinstance(digest, str)
                    or re.fullmatch(r"[0-9a-f]{64}", digest) is None
                ):
                    raise WorkspaceError("record profile bytes are invalid")
                try:
                    decoded = base64.b64decode(encoded, validate=True)
                except (ValueError, binascii.Error) as error:
                    raise WorkspaceError("record profile bytes are invalid") from error
                if _digest(decoded) != digest:
                    raise WorkspaceError("record profile digest is invalid")
        move_profiles: set[str] = set()
        for move in record["worktree_moves"]:
            if not isinstance(move, dict):
                raise WorkspaceError("record worktree move is invalid")
            profile = move.get("profile")
            source = move.get("source")
            destination = move.get("destination")
            if (
                not isinstance(profile, str)
                or profile not in rewrite_names
                or profile in move_profiles
                or not isinstance(source, str)
                or not isinstance(destination, str)
            ):
                raise WorkspaceError("record worktree move identity is invalid")
            source_path = Path(source)
            destination_path = Path(destination)
            source_parent = Path(self.paths.worktrees) / "content-1x"
            destination_parent = Path(self.paths.worktrees) / "content"
            if (
                source_path.parent != source_parent
                or destination_path.parent != destination_parent
                or source_path.name != destination_path.name
                or not NAME_PATTERN.fullmatch(source_path.name)
                or any(
                    not isinstance(move.get(key), str)
                    or re.fullmatch(r"[0-9a-f]{40,64}", move[key]) is None
                    for key in ("head", "tree")
                )
                or not isinstance(move.get("branch"), str)
                or not move["branch"]
            ):
                raise WorkspaceError("record worktree move path is invalid")
            move_profiles.add(profile)
        return record

    def _audit(self) -> dict[str, Any]:
        refusals: list[dict[str, str]] = []
        if not self.record_path.exists() and not self.record_path.is_symlink():
            if self.pending_path.exists() or self.pending_path.is_symlink():
                return {
                    "migration": CONTENT_MIGRATION_NAME,
                    "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
                    "status": "incomplete",
                    "refusals": [
                        _refusal(
                            "pending_migration",
                            f"an interrupted migration journal exists: {self.pending_path}",
                            "preserve it and recover only its exact recorded bytes and paths",
                        )
                    ],
                }
            inspection = self._inspect()
            if inspection["status"] == "not-needed":
                return inspection
            return {
                **inspection,
                "status": "incomplete",
                "refusals": [
                    *inspection["refusals"],
                    _refusal(
                        "migration_record_missing",
                        f"content migration record is missing: {self.record_path}",
                        "run the dry run and apply before auditing",
                    ),
                ],
            }
        if self.record_path.is_symlink() or not self.record_path.is_file():
            return {
                "migration": CONTENT_MIGRATION_NAME,
                "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
                "status": "incomplete",
                "refusals": [
                    _refusal(
                        "invalid_migration_record",
                        f"content migration record is unsafe: {self.record_path}",
                        "preserve the path and restore the exact regular migration record",
                    )
                ],
            }
        try:
            record = self._load_record()
        except (WorkspaceError, RecursionError) as error:
            return {
                "migration": CONTENT_MIGRATION_NAME,
                "schema_version": CONTENT_MIGRATION_SCHEMA_VERSION,
                "status": "incomplete",
                "refusals": [
                    _refusal(
                        "invalid_migration_record",
                        f"cannot validate {self.record_path}: {error}",
                        "restore the exact migration record",
                    )
                ],
            }
        expected_profile_field = (
            "original_sha256" if record["status"] == "restored" else "replacement_sha256"
        )
        for row in record["profiles"]:
            if not isinstance(row, dict) or row.get("status") != "rewrite":
                continue
            try:
                path = Path(row["path"])
                if _digest(path.read_bytes()) != row[expected_profile_field]:
                    raise WorkspaceError("profile bytes differ from the journal")
            except (OSError, KeyError, WorkspaceError) as error:
                refusals.append(
                    _refusal(
                        "profile_audit_failed",
                        f"profile audit failed for {row.get('path')}: {error}",
                        "restore the exact journaled bytes or record a new reviewed migration",
                    )
                )
        canonical_refusals: list[dict[str, str]] = []
        canonical = self._inspect_checkout(
            self.canonical,
            "main",
            CERTIFIED_MAIN_COMMIT,
            required=True,
            require_clean=False,
            refusals=canonical_refusals,
        )
        refusals.extend(canonical_refusals)
        if canonical.get("proven") and any(
            canonical.get(key) != record["canonical"].get(key)
            for key in ("common_directory", "repository")
        ):
            refusals.append(
                _refusal(
                    "canonical_audit_failed",
                    "canonical content identity differs from the migration journal",
                    "preserve all paths and restore the exact certified checkout",
                )
            )
        if record["status"] == "complete":
            for move in record["worktree_moves"]:
                try:
                    source, destination = self._managed_worktree_paths(
                        Path(move["source"]).name
                    )
                    if (
                        str(source) != move["source"]
                        or str(destination) != move["destination"]
                    ):
                        raise WorkspaceError("journaled worktree paths changed")
                    if source.exists() or source.is_symlink():
                        raise WorkspaceError("original worktree path was recreated")
                    proof = self._prove_main_worktree(destination, record["canonical"])
                    if any(proof[key] != move[key] for key in ("head", "tree", "branch")):
                        raise WorkspaceError("worktree identity changed")
                except (OSError, KeyError, WorkspaceError) as error:
                    refusals.append(
                        _refusal(
                            "worktree_audit_failed",
                            f"worktree audit failed for {move.get('destination')}: {error}",
                            "preserve both paths and restore only the exact journaled worktree",
                        )
                    )
        else:
            for move in record["worktree_moves"]:
                try:
                    source, destination = self._managed_worktree_paths(
                        Path(move["source"]).name
                    )
                    if (
                        str(source) != move["source"]
                        or str(destination) != move["destination"]
                    ):
                        raise WorkspaceError("journaled worktree paths changed")
                    if destination.exists() or destination.is_symlink():
                        raise WorkspaceError("migration destination was recreated")
                    proof = self._prove_main_worktree(source, record["canonical"])
                    if any(proof[key] != move[key] for key in ("head", "tree", "branch")):
                        raise WorkspaceError("restored worktree identity changed")
                except (OSError, KeyError, WorkspaceError) as error:
                    refusals.append(
                        _refusal(
                            "worktree_audit_failed",
                            f"restored worktree audit failed for {move.get('source')}: {error}",
                            "preserve both paths and repair only the exact journaled worktree",
                        )
                    )
        return {
            **record,
            "status": "incomplete" if refusals else record["status"],
            "refusals": refusals,
        }

    def _restore(self) -> dict[str, Any]:
        audit = self._audit()
        if audit.get("status") != "complete":
            return audit
        resources, refusals = self._resource_inventory(
            {row["name"] for row in audit["profiles"] if row.get("status") == "rewrite"}
        )
        if refusals:
            return {**audit, "status": "refused", "resources": resources, "refusals": refusals}
        restored_profiles: list[dict[str, Any]] = []
        restored_moves: list[dict[str, Any]] = []
        try:
            for row in audit["profiles"]:
                if row.get("status") != "rewrite":
                    continue
                path = Path(row["path"])
                if _digest(path.read_bytes()) != row["replacement_sha256"]:
                    raise WorkspaceError(f"replacement profile changed: {path}")
                _atomic_bytes(
                    path,
                    base64.b64decode(row["original_base64"], validate=True),
                )
                restored_profiles.append(row)
            for move in reversed(audit["worktree_moves"]):
                source, destination = self._managed_worktree_paths(
                    Path(move["source"]).name
                )
                if str(source) != move["source"] or str(destination) != move["destination"]:
                    raise WorkspaceError("worktree restore paths changed after audit")
                if source.exists() or source.is_symlink():
                    raise WorkspaceError(f"original worktree path is occupied: {source}")
                source.parent.mkdir(parents=True, exist_ok=True)
                _git(self.canonical, "worktree", "move", str(destination), str(source))
                restored_moves.append(move)
            restored = {
                **audit,
                "status": "restored",
                "restored_at": datetime.now(timezone.utc).isoformat(),
                "refusals": [],
            }
            atomic_json(self.record_path, restored)
            return restored
        except BaseException as error:
            rollback_errors = self._rollback_restore(
                restored_profiles, restored_moves, audit["canonical"]
            )
            detail = (
                "content migration restore stopped after a changed precondition: "
                f"{error}; preserve the journal and all paths"
            )
            if rollback_errors:
                detail += "; restore rollback failed: " + "; ".join(rollback_errors)
            raise WorkspaceError(detail) from error

    def _rollback_restore(
        self,
        profiles: list[dict[str, Any]],
        moves: list[dict[str, Any]],
        canonical: dict[str, Any],
    ) -> list[str]:
        errors: list[str] = []
        for move in reversed(moves):
            try:
                source, destination = self._managed_worktree_paths(
                    Path(move["source"]).name
                )
                self._prove_main_worktree(source, canonical)
                if destination.exists() or destination.is_symlink():
                    raise WorkspaceError(
                        f"migration destination is occupied: {destination}"
                    )
                _git(self.canonical, "worktree", "move", str(source), str(destination))
            except BaseException as error:
                errors.append(str(error))
        for row in reversed(profiles):
            path = Path(row["path"])
            try:
                if _digest(path.read_bytes()) != row["original_sha256"]:
                    raise WorkspaceError(f"original profile changed: {path}")
                replacement = base64.b64decode(
                    row["replacement_base64"], validate=True
                )
                _atomic_bytes(path, replacement)
            except BaseException as error:
                errors.append(str(error))
        return errors
