from __future__ import annotations

import copy
from contextlib import ExitStack
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
from typing import Any, TYPE_CHECKING

from .filesystem_identity import pair_matches, portable_device
from .locking import LockBusyError, exclusive_lock
from .model import (
    AtomicJsonCommitUncertain,
    MANAGED_MARKER,
    WorkspaceError,
    durable_atomic_json,
    validate_name,
)
from .workspace import load_regular_json

if TYPE_CHECKING:
    from .workspace import Workspace


SCOPE_SCHEMA_VERSION = 1
SCOPE_RESERVATION_SCHEMA_VERSION = 1
SCOPE_JOURNAL_SCHEMA_VERSION = 1
SCOPE_RELEASE_SCHEMA_VERSION = 1
SCOPE_FAILURE_BOUNDARIES_ENV = "ATRINIK_SCOPE_FAIL_AFTER"
_HEX40 = re.compile(r"[0-9a-f]{40}")
_HEX64 = re.compile(r"[0-9a-f]{64}")


def _now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _canonical_sha256(value: Any) -> str:
    return hashlib.sha256(
        json.dumps(value, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def _mapping(values: list[str], context: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw in values:
        key, separator, value = raw.partition("=")
        if not separator or not key or not value:
            raise WorkspaceError(f"{context} must use CHECKOUT=VALUE")
        validate_name(key, f"{context} checkout")
        if key in result:
            raise WorkspaceError(f"{context} repeats checkout: {key}")
        result[key] = value
    return result


class ScopeLifecycle:
    def __init__(self, workspace: Workspace):
        self.workspace = workspace
        self.paths = workspace.paths

    @property
    def root(self) -> Path:
        return self.paths.scopes

    def _scope_root(self, name: str) -> Path:
        validate_name(name, "scope name")
        return self.root / name

    def _record_path(self, name: str) -> Path:
        return self._scope_root(name) / "scope.json"

    def _journal_path(self, name: str) -> Path:
        return self._scope_root(name) / "creation-journal.json"

    def _reservation_path(self, name: str) -> Path:
        return self._scope_root(name) / "reservation.json"

    def _release_path(self, name: str) -> Path:
        return self._scope_root(name) / "release-journal.json"

    @staticmethod
    def _canonical_topology_name(name: str) -> str:
        validate_name(name, "scope name")
        return f"scope-{name}"

    @classmethod
    def _requested_topology_name(cls, name: str, topology: str | None) -> str:
        expected = cls._canonical_topology_name(name)
        if topology is not None and topology != expected:
            raise WorkspaceError(
                f"scope topology must be canonical for {name}: {expected}"
            )
        return expected

    @staticmethod
    def generated_name() -> str:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        return f"agent-{stamp}-{secrets.token_hex(6)}"

    def create(
        self,
        components: list[str],
        *,
        name: str | None = None,
        base_profile: str = "default",
        labels: list[str] | None = None,
        branches: list[str] | None = None,
        start_points: list[str] | None = None,
        topology: str | None = None,
        state_mode: str = "temporary",
        state_name: str | None = None,
    ) -> dict[str, Any]:
        self.paths.ensure()
        scope_name = name or self.generated_name()
        validate_name(scope_name, "scope name")
        if not components:
            raise WorkspaceError("scope creation requires at least one component")
        label_overrides = _mapping(labels or [], "scope label")
        branch_overrides = _mapping(branches or [], "scope branch")
        start_overrides = _mapping(start_points or [], "scope start point")
        if name is not None and (
            self._scope_root(scope_name).exists()
            or self._scope_root(scope_name).is_symlink()
        ):
            return self._retry_existing(
                scope_name,
                components,
                base_profile,
                label_overrides,
                branch_overrides,
                start_overrides,
                topology,
                state_mode,
                state_name,
            )
        request = self._preflight_request(
            scope_name,
            components,
            base_profile,
            label_overrides,
            branch_overrides,
            start_overrides,
            topology,
            state_mode,
            state_name,
        )
        digest = _canonical_sha256(request)
        existing = self._existing_exact(scope_name, digest)
        if existing is not None:
            return existing

        requests = self._creation_leases(request)
        scope_request = requests[0]
        scope_entered = False
        try:
            with self.workspace._resource_locks(
                [scope_request], nonblocking=True, include_wrapper=False
            ):
                scope_entered = True
                with self.workspace._resource_locks(requests[1:]):
                    existing = self._existing_exact(scope_name, digest)
                    if existing is not None:
                        return existing
                    request = self._preflight_request(
                        scope_name,
                        components,
                        base_profile,
                        label_overrides,
                        branch_overrides,
                        start_overrides,
                        topology,
                        state_mode,
                        state_name,
                    )
                    if _canonical_sha256(request) != digest:
                        raise WorkspaceError("scope coordinates changed during preflight; retry")
                    return self._create_locked(request, digest)
        except LockBusyError as error:
            if scope_entered:
                raise WorkspaceError(
                    f"scope resources remained busy while provisioning: {scope_name}; retry after the exact operation finishes"
                ) from error
            raise WorkspaceError(
                f"scope coordinates are already in use: {scope_name}; inspect the exact scope, profile, topology, and worktree coordinates"
            ) from error

    def _retry_existing(
        self,
        name: str,
        components: list[str],
        base_profile: str,
        labels: dict[str, str],
        branches: dict[str, str],
        start_points: dict[str, str],
        topology: str | None,
        state_mode: str,
        state_name: str | None,
    ) -> dict[str, Any]:
        record_path = self._record_path(name)
        if record_path.is_file() and not record_path.is_symlink():
            record = self._load_record(name)
            self._require_unreleased(record)
            expected_topology = self._requested_topology_name(name, topology)
            expected_state_name = (
                None
                if state_mode == "temporary"
                else "default"
                if state_mode == "default"
                else state_name
            )
            rows = {row["checkout"]: row for row in record["worktrees"]}
            expected_rows = {
                checkout: {
                    "label": labels.get(checkout, record["profile"]["name"]),
                    "branch": branches.get(checkout, f"scope/{name}/{checkout}"),
                    "start_point": start_points.get(
                        checkout,
                        f"refs/heads/{self.workspace.manifest.by_checkout[checkout].branch}",
                    ),
                }
                for checkout in rows
            }
            conflicts = (
                record["base_profile"] != base_profile
                or record["requested_components"] != sorted(set(components))
                or record["topology"]["name"] != expected_topology
                or record["state_policy"]["mode"] != state_mode
                or record["state_policy"]["name"] != expected_state_name
                or set(labels) - set(rows)
                or set(branches) - set(rows)
                or set(start_points) - set(rows)
                or any(
                    rows[checkout][coordinate] != expected
                    for checkout, coordinates in expected_rows.items()
                    for coordinate, expected in coordinates.items()
                )
            )
            if conflicts:
                raise WorkspaceError(
                    f"scope name is already bound to different coordinates: {name}"
                )
            return record

        if record_path.is_symlink():
            raise WorkspaceError(f"scope reservation is unsafe: {name}")
        request = self._preflight_request(
            name,
            components,
            base_profile,
            labels,
            branches,
            start_points,
            topology,
            state_mode,
            state_name,
            allow_existing_branches=True,
        )
        digest = _canonical_sha256(request)
        return self._recover_existing(request, digest)

    def _recover_existing(self, request: dict[str, Any], digest: str) -> dict[str, Any]:
        name = request["name"]
        requests = self._creation_leases(request)
        scope_request = requests[0]
        scope_entered = False
        try:
            with self.workspace._resource_locks(
                [scope_request], nonblocking=True, include_wrapper=False
            ):
                scope_entered = True
                with self.workspace._resource_locks(requests[1:]):
                    record_path = self._record_path(name)
                    if record_path.is_file() and not record_path.is_symlink():
                        record = self._load_record(name)
                        self._require_unreleased(record)
                        if record["request_sha256"] != digest:
                            raise WorkspaceError(
                                f"scope name is already bound to different coordinates: {name}"
                            )
                        return record
                    current = self._preflight_request(
                        name,
                        request["requested_components"],
                        request["base_profile"],
                        {
                            row["checkout"]: row["label"]
                            for row in request["worktrees"]
                        },
                        {
                            row["checkout"]: row["branch"]
                            for row in request["worktrees"]
                        },
                        {
                            row["checkout"]: row["start_point"]
                            for row in request["worktrees"]
                        },
                        request["topology"]["name"],
                        request["state_policy"]["mode"],
                        request["state_policy"]["name"]
                        if request["state_policy"]["mode"] == "named"
                        else None,
                        allow_existing_branches=True,
                    )
                    if current != request or _canonical_sha256(current) != digest:
                        raise WorkspaceError(
                            f"scope coordinates changed during recovery: {name}"
                        )
                    reservation, journal = self._load_recovery_inputs(
                        request, digest
                    )
                    return self._recover_locked(request, digest, reservation, journal)
        except LockBusyError as error:
            if scope_entered:
                raise WorkspaceError(
                    f"scope resources remained busy while recovering: {name}; retry after the exact operation finishes"
                ) from error
            raise WorkspaceError(
                f"scope coordinates are already in use: {name}; inspect the exact scope, profile, topology, and worktree coordinates"
            ) from error

    def _preflight_request(
        self,
        name: str,
        components: list[str],
        base_profile: str,
        labels: dict[str, str],
        branches: dict[str, str],
        start_points: dict[str, str],
        topology: str | None,
        state_mode: str,
        state_name: str | None,
        *,
        allow_existing_branches: bool = False,
    ) -> dict[str, Any]:
        from .workspace import git, run

        validate_name(base_profile, "scope base profile")
        base = self.workspace._load_profile_file(base_profile, require_file=False)
        stack = self.workspace.manifest.stack(base["stack"])
        selected: dict[str, set[str]] = {}
        requested: list[str] = []
        for selector in components:
            requested.append(selector)
            resolved = self.workspace._profile_components(base, selector)
            for component in resolved:
                selected.setdefault(component.checkout_name, set()).add(component.name)
        unknown_overrides = (set(labels) | set(branches) | set(start_points)) - set(selected)
        if unknown_overrides:
            raise WorkspaceError(
                "scope override names unselected checkouts: "
                + ", ".join(sorted(unknown_overrides))
            )

        profile_name = self._canonical_topology_name(name)
        topology_name = self._requested_topology_name(name, topology)
        validate_name(profile_name, "scope profile name")
        validate_name(topology_name, "scope topology name")
        if profile_name in self.workspace.manifest.stacks:
            raise WorkspaceError(f"scope profile collides with built-in profile: {profile_name}")
        profile_path = self.paths.profiles / f"{profile_name}.json"
        topology_path = self.paths.topologies / topology_name
        if profile_path.exists() or profile_path.is_symlink():
            raise WorkspaceError(f"scope profile already exists: {profile_name}")
        if any(
            isinstance(reference, dict)
            and reference.get("kind") == "profiles"
            and reference.get("reference") == profile_name
            for reference in self.workspace._physical_reference_records()
        ):
            raise WorkspaceError(f"scope profile reference already exists: {profile_name}")
        if topology_path.exists() or topology_path.is_symlink():
            raise WorkspaceError(f"scope topology namespace already exists: {topology_name}")

        if state_mode not in {"temporary", "named", "default"}:
            raise WorkspaceError(f"invalid scope state mode: {state_mode}")
        if state_mode == "temporary":
            if state_name is not None:
                raise WorkspaceError("temporary scope state does not accept a state name")
            policy = {
                "mode": "temporary",
                "name": None,
                "ownership": "topology-generation",
                "lifecycle": "remove-on-clean-stop",
            }
        elif state_mode == "default":
            if state_name is not None:
                raise WorkspaceError("default scope state does not accept a state name")
            policy = {
                "mode": "default",
                "name": "default",
                "ownership": "persistent-shared",
                "lifecycle": "never-remove-with-scope",
            }
        else:
            if state_name is None:
                raise WorkspaceError("named scope state requires --state NAME")
            validate_name(state_name, "scope state name")
            if state_name == "default":
                raise WorkspaceError("use --default-state to select shared default state")
            states = self.workspace.list_states()
            if state_name not in states:
                raise WorkspaceError(f"registered state does not exist: {state_name}")
            policy = {
                "mode": "named",
                "name": state_name,
                "ownership": "persistent-registered",
                "lifecycle": "never-remove-with-scope",
            }

        rows: list[dict[str, Any]] = []
        for checkout_name in sorted(selected):
            checkout = self.workspace.manifest.by_checkout[checkout_name]
            repository = self.workspace._primary_path(checkout)
            if repository.is_symlink() or not repository.is_dir():
                raise WorkspaceError(
                    f"scope checkout is not initialized: {checkout_name}: {repository}"
                )
            self.workspace._validate_primary_checkout(checkout, repository, trace=False)
            remote = self.workspace._canonical_remote(checkout, repository)
            label = labels.get(checkout_name, profile_name)
            validate_name(label, f"scope worktree label for {checkout_name}")
            branch = branches.get(checkout_name, f"scope/{name}/{checkout_name}")
            run(["git", "check-ref-format", "--branch", branch], capture=True, trace=False)
            point = start_points.get(checkout_name, f"refs/heads/{checkout.branch}")
            if point.startswith("-"):
                raise WorkspaceError("scope start point must not begin with '-'")
            commit = git(
                repository,
                "rev-parse",
                "--verify",
                "--end-of-options",
                f"{point}^{{commit}}",
                capture=True,
                trace=False,
            )
            tree = git(
                repository,
                "rev-parse",
                "--verify",
                "--end-of-options",
                f"{commit}^{{tree}}",
                capture=True,
                trace=False,
            )
            if not _HEX40.fullmatch(commit) or not _HEX40.fullmatch(tree):
                raise WorkspaceError(f"scope start point is not an exact Git commit: {point}")
            destination = self.paths.worktrees / checkout_name / label
            if destination.exists() or destination.is_symlink():
                raise WorkspaceError(f"scope worktree path already exists: {destination}")
            rows.append(
                {
                    "checkout": checkout_name,
                    "repository": checkout.repository,
                    "logical_components": sorted(selected[checkout_name]),
                    "label": label,
                    "branch": branch,
                    "start_point": point,
                    "commit": commit,
                    "tree": tree,
                    "path": str(destination),
                    "primary_path": str(repository),
                }
            )
        # The branch absence check must not use git(), whose non-zero result is
        # exceptional. Keep it last so every other coordinate has been proven.
        import subprocess

        for row in rows:
            completed = subprocess.run(
                [
                    "git",
                    "-C",
                    row["primary_path"],
                    "show-ref",
                    "--verify",
                    "--quiet",
                    f"refs/heads/{row['branch']}",
                ],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if completed.returncode == 0:
                if not allow_existing_branches:
                    raise WorkspaceError(
                        f"scope branch already exists: {row['checkout']}:{row['branch']}"
                    )
                continue
            if completed.returncode != 1:
                raise WorkspaceError(
                    f"cannot preflight scope branch: {row['checkout']}:{row['branch']}"
                )

        return {
            "name": name,
            "base_profile": base_profile,
            "stack": stack.name,
            "requested_components": sorted(set(requested)),
            "profile": {"name": profile_name, "path": str(profile_path)},
            "topology": {"name": topology_name, "path": str(topology_path)},
            "state_policy": policy,
            "worktrees": rows,
        }

    def _creation_leases(self, request: dict[str, Any]) -> list[Any]:
        requests = [
            self.workspace._lease_request(
                "registry", f"scope:{request['name']}", "exclusive", f"provision scope {request['name']}"
            ),
            self.workspace._lease_request(
                "profile", request["profile"]["name"], "exclusive", f"provision scope {request['name']}"
            ),
            self.workspace._lease_request(
                "topology", request["topology"]["name"], "exclusive", f"reserve scope {request['name']} topology"
            ),
        ]
        if request["base_profile"] != request["profile"]["name"]:
            requests.append(
                self.workspace._lease_request(
                    "profile",
                    request["base_profile"],
                    "shared",
                    f"copy base profile for scope {request['name']}",
                )
            )
        policy = request["state_policy"]
        if policy["mode"] != "temporary":
            requests.append(
                self.workspace._lease_request(
                    "state", policy["name"], "shared", f"reserve scope {request['name']} state policy"
                )
            )
        for row in request["worktrees"]:
            checkout = self.workspace.manifest.by_checkout[row["checkout"]]
            primary = Path(row["primary_path"])
            destination = Path(row["path"])
            requests.extend(
                [
                    self.workspace._lease_request(
                        "git-admin",
                        self.workspace._git_admin_coordinate(checkout, primary),
                        "exclusive",
                        f"provision scope {request['name']} worktree",
                    ),
                    self.workspace._lease_request(
                        "source",
                        self.workspace._source_coordinate(checkout.name, destination),
                        "exclusive",
                        f"provision scope {request['name']} worktree",
                    ),
                ]
            )
        return requests

    @staticmethod
    def _directory_identity(path: Path, context: str) -> dict[str, Any]:
        if path.is_symlink() or not path.is_dir():
            raise WorkspaceError(f"{context} is not a regular directory: {path}")
        identity = path.stat(follow_symlinks=False)
        return {
            "path": str(path),
            "device": portable_device(identity),
            "inode": identity.st_ino,
        }

    def _load_recovery_inputs(
        self, request: dict[str, Any], digest: str
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        name = request["name"]
        root = self._scope_root(name)
        reservation_path = self._reservation_path(name)
        journal_path = self._journal_path(name)
        if root.is_symlink() or not root.is_dir():
            raise WorkspaceError(f"scope reservation is unsafe: {name}")
        if not reservation_path.is_file() or reservation_path.is_symlink():
            raise WorkspaceError(
                f"scope creation is incomplete: {name}; preserve and inspect {journal_path}"
            )
        if not journal_path.is_file() or journal_path.is_symlink():
            raise WorkspaceError(
                f"scope creation is incomplete: {name}; preserve and inspect {journal_path}"
            )
        reservation = load_regular_json(reservation_path, f"scope reservation {name}")
        journal = load_regular_json(journal_path, f"scope creation journal {name}")
        if (
            not isinstance(reservation, dict)
            or reservation.get("schema_version") != SCOPE_RESERVATION_SCHEMA_VERSION
            or reservation.get("name") != name
            or not isinstance(reservation.get("generation"), str)
            or not re.fullmatch(r"[0-9a-f]{32}", reservation["generation"])
            or not isinstance(reservation.get("reserved_at"), str)
            or not reservation["reserved_at"]
            or reservation.get("request_sha256") != digest
        ):
            raise WorkspaceError(f"scope reservation identity changed: {name}")
        if (
            not isinstance(journal, dict)
            or journal.get("schema_version") != SCOPE_JOURNAL_SCHEMA_VERSION
            or journal.get("name") != name
            or journal.get("generation") != reservation["generation"]
            or journal.get("request_sha256") != digest
            or journal.get("status") not in {"rolled-back", "recovery-required"}
        ):
            raise WorkspaceError(
                f"scope creation is not safely retryable: {name}; preserve and inspect {journal_path}"
            )
        if (
            not isinstance(journal.get("worktrees"), list)
            or not all(
                isinstance(row, dict)
                and isinstance(row.get("checkout"), str)
                and isinstance(row.get("status"), str)
                for row in journal["worktrees"]
            )
        ):
            raise WorkspaceError(f"scope worktree evidence is invalid: {name}")
        if "request" in journal and journal["request"] != request:
            raise WorkspaceError(f"scope request evidence changed: {name}")
        expected_rows = {row["checkout"]: row for row in request["worktrees"]}
        retained_rows = {
            row.get("checkout"): row
            for row in journal["worktrees"]
            if isinstance(row, dict)
        }
        retained_profile = journal.get("profile")
        row_coordinates = (
            "checkout",
            "repository",
            "logical_components",
            "label",
            "branch",
            "start_point",
            "commit",
            "tree",
            "path",
            "primary_path",
        )
        if (
            len(journal["worktrees"]) != len(expected_rows)
            or len(retained_rows) != len(expected_rows)
            or set(retained_rows) != set(expected_rows)
            or not isinstance(retained_profile, dict)
            or retained_profile.get("name") != request["profile"]["name"]
            or retained_profile.get("path") != request["profile"]["path"]
            or any(
                any(
                    retained_rows[checkout].get(key) != expected_rows[checkout][key]
                    for key in row_coordinates
                )
                for checkout in expected_rows
            )
        ):
            raise WorkspaceError(f"scope request evidence changed: {name}")
        identities = journal.get("identities")
        if "identities" in journal:
            expected_repositories = {
                row["checkout"]: row["primary_path"] for row in request["worktrees"]
            }
            retained_repository_rows = (
                identities.get("repositories", [])
                if isinstance(identities, dict)
                else []
            )
            retained_repositories = {
                item.get("checkout"): item
                for item in retained_repository_rows
                if isinstance(item, dict)
            } if isinstance(identities, dict) else {}
            expected_workspace = self._directory_identity(
                self.paths.workspace, "scope workspace root"
            )
            expected_scope = self._directory_identity(root, "scope reservation root")
            if (
                not isinstance(identities, dict)
                or identities.get("workspace") != expected_workspace
                or identities.get("scope") != expected_scope
                or not isinstance(retained_repository_rows, list)
                or len(retained_repository_rows) != len(expected_repositories)
                or len(retained_repositories) != len(expected_repositories)
                or set(retained_repositories) != set(expected_repositories)
                or any(
                    retained_repositories[checkout].get("path")
                    != expected_repositories[checkout]
                    or {
                        key: retained_repositories[checkout].get(key)
                        for key in ("path", "device", "inode")
                    }
                    != self._directory_identity(
                        Path(expected_repositories[checkout]),
                        f"scope repository {checkout}",
                    )
                    for checkout in expected_repositories
                )
            ):
                raise WorkspaceError(f"scope repository or root identity changed: {name}")
        profile = journal.get("profile")
        if not isinstance(profile, dict) or profile.get("status") not in {
            "planned",
            "rolled-back",
        }:
            raise WorkspaceError(
                f"scope recovery has published or uncertain profile evidence: {name}"
            )
        profile_path = Path(request["profile"]["path"])
        if profile_path.exists() or profile_path.is_symlink():
            raise WorkspaceError(f"scope profile reference changed during recovery: {name}")
        if any(
            isinstance(row, dict)
            and row.get("status") not in {"planned", "rolled-back"}
            for row in journal.get("worktrees", [])
        ):
            raise WorkspaceError(
                f"scope recovery contains changed worktree evidence: {name}"
            )
        return reservation, journal

    def _recover_worktree(self, row: dict[str, Any]) -> None:
        from .workspace import _is_clean, _worktree_records, git

        checkout = self.workspace.manifest.by_checkout[row["checkout"]]
        primary = Path(row["primary_path"])
        destination = Path(row["path"])
        if not _is_clean(primary, trace=False):
            raise WorkspaceError(
                f"scope recovery primary checkout is dirty: {row['checkout']}"
            )
        records = _worktree_records(primary, trace=False)
        destination_key = str(destination.resolve(strict=False))
        target_branch = f"refs/heads/{row['branch']}"
        matching_path = [
            record
            for record in records
            if str(Path(record["worktree"]).resolve(strict=False)) == destination_key
        ]
        matching_branch = [
            record for record in records if record.get("branch") == target_branch
        ]
        if matching_path or matching_branch:
            raise WorkspaceError(
                f"scope recovery found an existing worktree reference: {row['checkout']}:{row['branch']}"
            )
        remote_refs = git(
            primary,
            "for-each-ref",
            "--format=%(refname)",
            "refs/remotes",
            capture=True,
            trace=False,
        ).splitlines()
        if any(
            reference.startswith("refs/remotes/")
            and reference.removeprefix("refs/remotes/").partition("/")[2]
            == row["branch"]
            for reference in remote_refs
        ):
            raise WorkspaceError(
                f"scope recovery found an existing remote branch reference: {row['checkout']}:{row['branch']}"
            )
        branch_head: str | None = None
        try:
            branch_head = git(
                primary,
                "rev-parse",
                "--verify",
                "--end-of-options",
                target_branch,
                capture=True,
                trace=False,
            )
        except WorkspaceError:
            branch_head = None
        if branch_head is not None and branch_head != row["commit"]:
            raise WorkspaceError(
                f"scope branch changed during recovery: {row['checkout']}:{row['branch']}"
            )
        if destination.exists() or destination.is_symlink():
            raise WorkspaceError(
                f"scope worktree path already exists during recovery: {destination}"
            )
        self.workspace._create_worktree(
            checkout.name,
            row["label"],
            row["branch"],
            row["commit"],
            branch_head is not None,
            announce=False,
        )
        self.workspace._validate_checkout(checkout, destination, trace=False)
        if git(destination, "rev-parse", "HEAD", capture=True, trace=False) != row["commit"]:
            raise WorkspaceError(f"scope worktree head changed during recovery: {destination}")
        if git(destination, "branch", "--show-current", capture=True, trace=False) != row["branch"]:
            raise WorkspaceError(f"scope worktree branch changed during recovery: {destination}")
        if not _is_clean(destination, trace=False):
            raise WorkspaceError(f"scope recovery created a dirty worktree: {destination}")
        common = str(self.workspace._git_common_directory(destination, trace=False))
        identity = destination.stat(follow_symlinks=False)
        row.update(
            {
                "status": "created",
                "common_git_dir": common,
                "path_device": portable_device(identity),
                "path_inode": identity.st_ino,
            }
        )

    def _recover_locked(
        self,
        request: dict[str, Any],
        digest: str,
        reservation: dict[str, Any],
        journal: dict[str, Any],
    ) -> dict[str, Any]:
        from .workspace import PROFILE_SCHEMA_VERSION

        name = request["name"]
        journal["recovery"] = {
            "status": "adopting",
            "started_at": _now(),
            "source_status": journal["status"],
            "source_error": journal.get("error"),
            "source_rollback": copy.deepcopy(journal.get("rollback", [])),
            "observed_identities": {
                "workspace": self._directory_identity(
                    self.paths.workspace, "scope workspace root"
                ),
                "scope": self._directory_identity(
                    self._scope_root(name), "scope reservation root"
                ),
                "repositories": [
                    {
                        "checkout": row["checkout"],
                        **self._directory_identity(
                            Path(row["primary_path"]),
                            f"scope repository {row['checkout']}",
                        ),
                    }
                    for row in request["worktrees"]
                ],
            },
        }
        journal["status"] = "recovering"
        journal["updated_at"] = _now()
        self._write_journal(journal)
        final_published = False
        try:
            for row in journal["worktrees"]:
                self._recover_worktree(row)
                self._write_journal(journal)
                self._boundary(journal, f"worktree:{row['checkout']}")

            base = self.workspace._load_profile_file(
                request["base_profile"], require_file=False
            )
            profile = {
                "schema_version": PROFILE_SCHEMA_VERSION,
                "name": request["profile"]["name"],
                "stack": base["stack"],
                "sound_mode": base["sound_mode"],
                "sound_release": copy.deepcopy(base["sound_release"]),
                "components": copy.deepcopy(base["components"]),
            }
            for row in request["worktrees"]:
                for component in self.workspace.manifest.stack(profile["stack"]).components:
                    if component.checkout_name == row["checkout"]:
                        profile["components"][component.name] = {
                            "kind": "worktree",
                            "value": row["label"],
                        }
            profile_path = Path(request["profile"]["path"])
            self.workspace._publish_profile_references(profile["name"], profile)
            journal["profile"]["status"] = "reference-published"
            self._write_journal(journal)
            self._boundary(journal, "profile-reference")
            durable_atomic_json(profile_path, profile)
            profile_identity = profile_path.stat(follow_symlinks=False)
            journal["profile"].update(
                {
                    "status": "created",
                    "sha256": _file_sha256(profile_path),
                    "path_device": portable_device(profile_identity),
                    "path_inode": profile_identity.st_ino,
                }
            )
            self._write_journal(journal)
            self._boundary(journal, "profile")

            worktrees = [
                {
                    key: row[key]
                    for key in (
                        "checkout",
                        "repository",
                        "logical_components",
                        "label",
                        "branch",
                        "start_point",
                        "commit",
                        "tree",
                        "path",
                        "primary_path",
                        "common_git_dir",
                        "path_device",
                        "path_inode",
                    )
                }
                | {"created_by_scope": True}
                for row in journal["worktrees"]
            ]
            record = self._record(
                request,
                digest,
                reservation["generation"],
                reservation["reserved_at"],
                worktrees,
                journal["profile"]["sha256"],
                journal["profile"]["path_device"],
                journal["profile"]["path_inode"],
            )
            self._validate_record(record, name)
            durable_atomic_json(self._record_path(name), record)
            final_published = True
            journal["status"] = "complete"
            journal["updated_at"] = _now()
            journal["boundaries"].append("scope")
            journal["recovery"].update({"status": "complete", "completed_at": journal["updated_at"]})
            self._write_journal(journal)
            self._maybe_fail("scope")
            return record
        except BaseException as error:
            journal["status"] = "complete" if final_published else "recovery-required"
            journal["error"] = f"{type(error).__name__}: {error}"
            journal["updated_at"] = _now()
            journal["recovery"]["status"] = "complete" if final_published else "required"
            self._write_journal(journal)
            if final_published:
                raise
            raise WorkspaceError(
                f"scope recovery failed and recovery inputs were preserved in {self._journal_path(name)}: {error}"
            ) from error

    def _existing_exact(self, name: str, digest: str) -> dict[str, Any] | None:
        root = self._scope_root(name)
        if not root.exists() and not root.is_symlink():
            return None
        if root.is_symlink() or not root.is_dir():
            raise WorkspaceError(f"scope reservation is unsafe: {name}")
        record_path = self._record_path(name)
        if record_path.is_file() and not record_path.is_symlink():
            record = self._load_record(name)
            self._require_unreleased(record)
            if record["request_sha256"] == digest:
                return record
            raise WorkspaceError(f"scope name is already bound to different coordinates: {name}")
        reservation_path = self._reservation_path(name)
        if not reservation_path.is_file() or reservation_path.is_symlink():
            raise WorkspaceError(f"scope reservation is incomplete or unsafe: {name}")
        reservation = load_regular_json(reservation_path, f"scope reservation {name}")
        if reservation.get("request_sha256") != digest:
            raise WorkspaceError(f"scope name is already reserved for different coordinates: {name}")
        raise WorkspaceError(
            f"scope creation is incomplete: {name}; preserve and inspect {self._journal_path(name)}"
        )

    def _create_locked(self, request: dict[str, Any], digest: str) -> dict[str, Any]:
        from .workspace import PROFILE_SCHEMA_VERSION, git

        name = request["name"]
        root = self._scope_root(name)
        try:
            root.mkdir(mode=0o700)
        except FileExistsError as error:
            raise WorkspaceError(f"scope name was concurrently reserved: {name}") from error
        generation = secrets.token_hex(16)
        reservation = {
            "schema_version": SCOPE_RESERVATION_SCHEMA_VERSION,
            "name": name,
            "generation": generation,
            "request_sha256": digest,
            "reserved_at": _now(),
        }
        journal: dict[str, Any] = {
            "schema_version": SCOPE_JOURNAL_SCHEMA_VERSION,
            "name": name,
            "generation": generation,
            "request_sha256": digest,
            "request": copy.deepcopy(request),
            "status": "creating",
            "updated_at": reservation["reserved_at"],
            "identities": {
                "workspace": self._directory_identity(
                    self.paths.workspace, "scope workspace root"
                ),
                "scope": self._directory_identity(root, "scope reservation root"),
                "repositories": [
                    {
                        "checkout": row["checkout"],
                        **self._directory_identity(
                            Path(row["primary_path"]),
                            f"scope repository {row['checkout']}",
                        ),
                    }
                    for row in request["worktrees"]
                ],
            },
            "boundaries": [],
            "worktrees": [
                {
                    **copy.deepcopy(row),
                    "status": "planned",
                    "common_git_dir": None,
                    "path_device": None,
                    "path_inode": None,
                }
                for row in request["worktrees"]
            ],
            "profile": {
                **request["profile"],
                "status": "planned",
                "sha256": None,
                "path_device": None,
                "path_inode": None,
            },
            "rollback": [],
            "error": None,
            "recovery": None,
        }
        durable_atomic_json(self._reservation_path(name), reservation)
        durable_atomic_json(self._journal_path(name), journal)
        profile_created = False
        profile_reference_created = False
        final_published = False
        publication_uncertain = False
        try:
            self._boundary(journal, "reservation")
            for index, row in enumerate(request["worktrees"]):
                path = self.workspace._create_worktree(
                    row["checkout"],
                    row["label"],
                    row["branch"],
                    row["commit"],
                    False,
                    announce=False,
                )
                checkout = self.workspace.manifest.by_checkout[row["checkout"]]
                self.workspace._validate_checkout(checkout, path, trace=False)
                if git(path, "rev-parse", "HEAD", capture=True, trace=False) != row["commit"]:
                    raise WorkspaceError(f"scope worktree head changed during creation: {path}")
                common = str(self.workspace._git_common_directory(path, trace=False))
                identity = path.stat(follow_symlinks=False)
                journal["worktrees"][index].update(
                    {
                        "status": "created",
                        "common_git_dir": common,
                        "path_device": portable_device(identity),
                        "path_inode": identity.st_ino,
                    }
                )
                self._write_journal(journal)
                self._boundary(journal, f"worktree:{row['checkout']}")

            base = self.workspace._load_profile_file(
                request["base_profile"], require_file=False
            )
            profile = {
                "schema_version": PROFILE_SCHEMA_VERSION,
                "name": request["profile"]["name"],
                "stack": base["stack"],
                "sound_mode": base["sound_mode"],
                "sound_release": copy.deepcopy(base["sound_release"]),
                "components": copy.deepcopy(base["components"]),
            }
            for row in request["worktrees"]:
                for component in self.workspace.manifest.stack(profile["stack"]).components:
                    if component.checkout_name == row["checkout"]:
                        profile["components"][component.name] = {
                            "kind": "worktree",
                            "value": row["label"],
                        }
            profile_path = Path(request["profile"]["path"])
            self.workspace._publish_profile_references(profile["name"], profile)
            profile_reference_created = True
            journal["profile"]["status"] = "reference-published"
            self._write_journal(journal)
            self._boundary(journal, "profile-reference")
            try:
                durable_atomic_json(profile_path, profile)
            except AtomicJsonCommitUncertain:
                publication_uncertain = True
                journal["profile"]["status"] = "preserved-uncertain"
                self._write_journal(journal)
                raise
            except BaseException:
                self.workspace._remove_physical_reference(profile_path)
                raise
            profile_created = True
            profile_identity = profile_path.stat(follow_symlinks=False)
            journal["profile"].update(
                {
                    "status": "created",
                    "sha256": _file_sha256(profile_path),
                    "path_device": portable_device(profile_identity),
                    "path_inode": profile_identity.st_ino,
                }
            )
            self._write_journal(journal)
            self._boundary(journal, "profile")

            worktrees = []
            for row in journal["worktrees"]:
                worktrees.append(
                    {
                        key: row[key]
                        for key in (
                            "checkout",
                            "repository",
                            "logical_components",
                            "label",
                            "branch",
                            "start_point",
                            "commit",
                            "tree",
                            "path",
                            "primary_path",
                            "common_git_dir",
                            "path_device",
                            "path_inode",
                        )
                    }
                    | {"created_by_scope": True}
                )
            record = self._record(
                request,
                digest,
                generation,
                reservation["reserved_at"],
                worktrees,
                journal["profile"]["sha256"],
                journal["profile"]["path_device"],
                journal["profile"]["path_inode"],
            )
            self._validate_record(record, name)
            try:
                durable_atomic_json(self._record_path(name), record)
            except AtomicJsonCommitUncertain:
                publication_uncertain = True
                raise
            final_published = True
            journal["status"] = "complete"
            journal["updated_at"] = _now()
            journal["boundaries"].append("scope")
            self._write_journal(journal)
            self._maybe_fail("scope")
            return record
        except BaseException as error:
            if final_published:
                journal["status"] = "complete"
                journal["error"] = f"{type(error).__name__}: {error}"
                journal["updated_at"] = _now()
                self._write_journal(journal)
                raise
            if publication_uncertain:
                journal["status"] = "recovery-required"
                journal["error"] = f"{type(error).__name__}: {error}"
                journal["updated_at"] = _now()
                self._write_journal(journal)
                raise WorkspaceError(
                    f"scope creation publication is uncertain; all inputs were preserved in {self._journal_path(name)}: {error}"
                ) from error
            journal["status"] = "rolling-back"
            journal["error"] = f"{type(error).__name__}: {error}"
            self._write_journal(journal)
            uncertain = self._rollback_creation(
                journal, profile_created, profile_reference_created
            )
            journal["status"] = "recovery-required" if uncertain else "rolled-back"
            journal["updated_at"] = _now()
            self._write_journal(journal)
            if uncertain:
                raise WorkspaceError(
                    f"scope creation failed and recovery inputs were preserved in {self._journal_path(name)}: {error}"
                ) from error
            raise

    def _boundary(self, journal: dict[str, Any], name: str) -> None:
        journal["boundaries"].append(name)
        journal["updated_at"] = _now()
        self._write_journal(journal)
        self._maybe_fail(name)

    @staticmethod
    def _maybe_fail(boundary: str) -> None:
        if os.environ.get(SCOPE_FAILURE_BOUNDARIES_ENV) == boundary:
            raise WorkspaceError(f"injected scope failure after publication boundary: {boundary}")

    def _write_journal(self, journal: dict[str, Any]) -> None:
        durable_atomic_json(self._journal_path(journal["name"]), journal)

    def _rollback_creation(
        self,
        journal: dict[str, Any],
        profile_created: bool,
        profile_reference_created: bool,
    ) -> bool:
        from .workspace import git

        uncertain = False
        profile = journal["profile"]
        profile_path = Path(profile["path"])
        if profile_created:
            try:
                if (
                    profile_path.is_file()
                    and not profile_path.is_symlink()
                    and _file_sha256(profile_path) == profile["sha256"]
                    and pair_matches(
                        {
                            "device": profile["path_device"],
                            "inode": profile["path_inode"],
                        },
                        profile_path.stat(follow_symlinks=False),
                    )
                ):
                    profile_path.unlink()
                    self.workspace._remove_physical_reference(profile_path)
                    journal["rollback"].append("profile")
                    profile["status"] = "rolled-back"
                else:
                    uncertain = True
                    profile["status"] = "preserved-changed"
            except BaseException as rollback_error:
                uncertain = True
                profile["status"] = "preserved-uncertain"
                journal["rollback"].append(f"profile-error:{rollback_error}")
            self._write_journal(journal)
        elif profile_reference_created:
            try:
                self.workspace._remove_physical_reference(profile_path)
                journal["rollback"].append("profile-reference")
                profile["status"] = "rolled-back"
            except BaseException as rollback_error:
                uncertain = True
                profile["status"] = "preserved-uncertain"
                journal["rollback"].append(
                    f"profile-reference-error:{rollback_error}"
                )
            self._write_journal(journal)
        for row in reversed(journal["worktrees"]):
            if row["status"] != "created":
                continue
            path = Path(row["path"])
            try:
                exact = (
                    path.is_dir()
                    and not path.is_symlink()
                    and pair_matches(
                        {
                            "device": row["path_device"],
                            "inode": row["path_inode"],
                        },
                        path.stat(follow_symlinks=False),
                    )
                    and self.workspace._git_common_directory(path, trace=False)
                    == Path(row["common_git_dir"])
                    and git(path, "rev-parse", "HEAD", capture=True, trace=False)
                    == row["commit"]
                    and git(path, "branch", "--show-current", capture=True, trace=False)
                    == row["branch"]
                    and not [
                        reference
                        for reference in self.workspace._source_references(path)
                        if reference != f"scope:{journal['name']}"
                    ]
                )
                from .workspace import _is_clean

                if not exact or not _is_clean(path):
                    uncertain = True
                    row["status"] = "preserved-changed"
                    continue
                checkout = self.workspace.manifest.by_checkout[row["checkout"]]
                primary = Path(row["primary_path"])
                git(primary, "worktree", "remove", str(path), trace=False)
                git(primary, "branch", "-d", row["branch"], trace=False)
                row["status"] = "rolled-back"
                journal["rollback"].append(f"worktree:{row['checkout']}")
            except BaseException as rollback_error:
                uncertain = True
                row["status"] = "preserved-uncertain"
                journal["rollback"].append(
                    f"worktree-error:{row['checkout']}:{rollback_error}"
                )
            self._write_journal(journal)
        return uncertain

    def _record(
        self,
        request: dict[str, Any],
        digest: str,
        generation: str,
        created_at: str,
        worktrees: list[dict[str, Any]],
        profile_sha256: str,
        profile_device: int,
        profile_inode: int,
    ) -> dict[str, Any]:
        profile = request["profile"]["name"]
        topology = request["topology"]["name"]
        state = request["state_policy"]
        state_args = (
            ["--temporary-state"]
            if state["mode"] == "temporary"
            else ["--default-state"]
            if state["mode"] == "default"
            else ["--state", state["name"]]
        )
        quote = lambda values: " ".join(values)
        stack = self.workspace.manifest.stack(request["stack"])
        build_commands = {
            component.name: f"./atrinik build {component.name} --profile {profile} --test"
            for component in stack.components
            if self.workspace.manifest.effective_build(stack.name, component) != "none"
        }
        path_commands = {
            component.name: f"./atrinik path {component.name} --profile {profile}"
            for component in stack.components
        }
        return {
            "schema_version": SCOPE_SCHEMA_VERSION,
            "status": "complete",
            "name": request["name"],
            "generation": generation,
            "request_sha256": digest,
            "created_at": created_at,
            "base_profile": request["base_profile"],
            "stack": request["stack"],
            "requested_components": request["requested_components"],
            "worktrees": worktrees,
            "profile": {
                **request["profile"],
                "sha256": profile_sha256,
                "path_device": profile_device,
                "path_inode": profile_inode,
                "immutable": True,
            },
            "topology": request["topology"],
            "state_policy": state,
            "commands": {
                "paths": path_commands,
                "builds": build_commands,
                "topology_show": quote(
                    ["./atrinik", "topology", "show", profile, *state_args, "--json"]
                ),
                "up": quote(
                    ["./atrinik", "up", "--name", topology, "--profile", profile, *state_args, "--json"]
                ),
                "ps": f"./atrinik ps {topology} --json",
                "logs": {
                    service: f"./atrinik logs {topology} {service} --tail 100"
                    for service in ("server", "client")
                },
                "down": f"./atrinik down {topology} --json",
                "release_preview": f"./atrinik scope release {request['name']} --dry-run --json",
                "release_apply": f"./atrinik scope release {request['name']} --apply --plan PLAN_SHA256 --json",
            },
            "cleanup": {
                "policy": "explicit-preview-first",
                "journal": str(self._journal_path(request["name"])),
                "release_journal": str(self._release_path(request["name"])),
            },
        }

    def show(self, name: str) -> dict[str, Any]:
        return self._load_record(name)

    def list(self) -> list[dict[str, Any]]:
        if not self.root.exists():
            return []
        if self.root.is_symlink() or not self.root.is_dir():
            raise WorkspaceError(f"scope registry is unsafe: {self.root}")
        records = []
        for entry in sorted(self.root.iterdir()):
            if entry.is_dir() and not entry.is_symlink() and (entry / "scope.json").is_file():
                records.append(self._load_record(entry.name))
        return records

    def _load_record(self, name: str) -> dict[str, Any]:
        path = self._record_path(name)
        if path.is_symlink() or not path.is_file():
            raise WorkspaceError(f"completed scope does not exist: {name}")
        value = load_regular_json(path, f"scope {name}")
        self._validate_record(value, name)
        return value

    def _validate_record(self, value: Any, name: str) -> None:
        if not isinstance(value, dict):
            raise WorkspaceError(f"scope record is invalid: {name}")
        expected = {
            "schema_version", "status", "name", "generation", "request_sha256",
            "created_at", "base_profile", "stack", "requested_components",
            "worktrees", "profile", "topology", "state_policy", "commands", "cleanup",
        }
        if set(value) != expected or value.get("schema_version") != SCOPE_SCHEMA_VERSION:
            raise WorkspaceError(f"scope record schema is invalid: {name}")
        if value.get("status") != "complete" or value.get("name") != name:
            raise WorkspaceError(f"scope record identity is invalid: {name}")
        if not isinstance(value.get("generation"), str) or not re.fullmatch(
            r"[0-9a-f]{32}", value["generation"]
        ):
            raise WorkspaceError(f"scope generation is invalid: {name}")
        if not isinstance(value.get("request_sha256"), str) or not _HEX64.fullmatch(
            value["request_sha256"]
        ):
            raise WorkspaceError(f"scope request identity is invalid: {name}")
        if value.get("stack") not in self.workspace.manifest.stacks:
            raise WorkspaceError(f"scope stack is invalid: {name}")
        if not isinstance(value.get("created_at"), str) or not value["created_at"]:
            raise WorkspaceError(f"scope creation time is invalid: {name}")
        if not isinstance(value.get("base_profile"), str):
            raise WorkspaceError(f"scope base profile is invalid: {name}")
        if (
            not isinstance(value.get("requested_components"), list)
            or not value["requested_components"]
            or value["requested_components"] != sorted(set(value["requested_components"]))
            or not all(isinstance(item, str) and item for item in value["requested_components"])
        ):
            raise WorkspaceError(f"scope requested components are invalid: {name}")
        worktrees = value.get("worktrees")
        if not isinstance(worktrees, list) or not worktrees:
            raise WorkspaceError(f"scope worktrees are invalid: {name}")
        seen: set[str] = set()
        for row in worktrees:
            expected_worktree = {
                "checkout", "repository", "logical_components", "label", "branch",
                "start_point", "commit", "tree", "path", "primary_path",
                "common_git_dir", "path_device", "path_inode", "created_by_scope",
            }
            if (
                not isinstance(row, dict)
                or set(row) != expected_worktree
                or row.get("checkout") in seen
                or row.get("created_by_scope") is not True
            ):
                raise WorkspaceError(f"scope worktree record is invalid: {name}")
            checkout = row.get("checkout")
            if checkout not in self.workspace.manifest.by_checkout:
                raise WorkspaceError(f"scope worktree checkout is invalid: {name}")
            seen.add(checkout)
            if row.get("repository") != self.workspace.manifest.by_checkout[checkout].repository:
                raise WorkspaceError(f"scope worktree repository is invalid: {name}")
            if not all(isinstance(row.get(key), str) and row[key] for key in (
                "label", "branch", "start_point", "commit", "tree", "path",
                "primary_path", "common_git_dir",
            )) or not _HEX40.fullmatch(row["commit"]) or not _HEX40.fullmatch(row["tree"]):
                raise WorkspaceError(f"scope worktree coordinates are invalid: {name}")
            if not all(type(row.get(key)) is int and row[key] >= 0 for key in ("path_device", "path_inode")):
                raise WorkspaceError(f"scope worktree path identity is invalid: {name}")
            expected_components = sorted(
                component.name
                for component in self.workspace.manifest.stacks[value["stack"]].components
                if component.checkout_name == checkout
            )
            if row.get("logical_components") != expected_components:
                raise WorkspaceError(f"scope logical checkout coverage is invalid: {name}")
            expected_path = self.paths.worktrees / checkout / row["label"]
            if Path(row["path"]) != expected_path:
                raise WorkspaceError(f"scope worktree path is invalid: {name}")
            if Path(row["primary_path"]) != self.workspace._primary_path(
                self.workspace.manifest.by_checkout[checkout]
            ):
                raise WorkspaceError(f"scope primary checkout path is invalid: {name}")
        profile = value.get("profile")
        topology = value.get("topology")
        policy = value.get("state_policy")
        if not isinstance(profile, dict) or set(profile) != {
            "name", "path", "sha256", "path_device", "path_inode", "immutable",
        }:
            raise WorkspaceError(f"scope profile record is invalid: {name}")
        if profile.get("path") != str(self.paths.profiles / f"{profile.get('name')}.json"):
            raise WorkspaceError(f"scope profile path is invalid: {name}")
        if profile.get("name") != f"scope-{name}":
            raise WorkspaceError(f"scope profile name is invalid: {name}")
        if profile.get("immutable") is not True or not _HEX64.fullmatch(str(profile.get("sha256"))):
            raise WorkspaceError(f"scope profile identity is invalid: {name}")
        if not all(
            type(profile.get(key)) is int and profile[key] >= 0
            for key in ("path_device", "path_inode")
        ):
            raise WorkspaceError(f"scope profile path identity is invalid: {name}")
        if (
            not isinstance(topology, dict)
            or set(topology) != {"name", "path"}
            or not isinstance(topology.get("name"), str)
            or topology.get("path") != str(self.paths.topologies / topology["name"])
        ):
            raise WorkspaceError(f"scope topology record is invalid: {name}")
        if (
            not isinstance(policy, dict)
            or set(policy) != {"mode", "name", "ownership", "lifecycle"}
            or policy.get("mode") not in {"temporary", "named", "default"}
        ):
            raise WorkspaceError(f"scope state policy is invalid: {name}")
        if (
            (policy["mode"] == "temporary" and policy["name"] is not None)
            or (policy["mode"] == "default" and policy["name"] != "default")
            or (policy["mode"] == "named" and not isinstance(policy["name"], str))
        ):
            raise WorkspaceError(f"scope state identity is invalid: {name}")
        commands = value.get("commands")
        if not isinstance(commands, dict) or set(commands) != {
            "paths", "builds", "topology_show", "up", "ps", "logs", "down",
            "release_preview", "release_apply",
        }:
            raise WorkspaceError(f"scope commands are invalid: {name}")
        if not all(
            isinstance(commands.get(key), str) and commands[key]
            for key in (
                "topology_show", "up", "ps", "down", "release_preview", "release_apply"
            )
        ):
            raise WorkspaceError(f"scope command coordinates are invalid: {name}")
        if (
            not isinstance(commands.get("paths"), dict)
            or not isinstance(commands.get("builds"), dict)
            or not isinstance(commands.get("logs"), dict)
            or set(commands["logs"]) != {"client", "server"}
            or not all(isinstance(item, str) and item for item in commands["paths"].values())
            or not all(isinstance(item, str) and item for item in commands["builds"].values())
            or not all(isinstance(item, str) and item for item in commands["logs"].values())
        ):
            raise WorkspaceError(f"scope command maps are invalid: {name}")
        cleanup = value.get("cleanup")
        if (
            not isinstance(cleanup, dict)
            or set(cleanup) != {"policy", "journal", "release_journal"}
            or cleanup.get("policy") != "explicit-preview-first"
            or cleanup.get("journal") != str(self._journal_path(name))
            or cleanup.get("release_journal") != str(self._release_path(name))
        ):
            raise WorkspaceError(f"scope cleanup coordinates are invalid: {name}")
        request = {
            "name": value["name"],
            "base_profile": value["base_profile"],
            "stack": value["stack"],
            "requested_components": value["requested_components"],
            "profile": {"name": profile["name"], "path": profile["path"]},
            "topology": topology,
            "state_policy": policy,
            "worktrees": [
                {
                    key: row[key]
                    for key in (
                        "checkout", "repository", "logical_components", "label", "branch",
                        "start_point", "commit", "tree", "path", "primary_path",
                    )
                }
                for row in worktrees
            ],
        }
        if _canonical_sha256(request) != value["request_sha256"]:
            raise WorkspaceError(f"scope request digest is invalid: {name}")
        canonical = self._record(
            request,
            value["request_sha256"],
            value["generation"],
            value["created_at"],
            worktrees,
            profile["sha256"],
            profile["path_device"],
            profile["path_inode"],
        )
        if value["commands"] != canonical["commands"]:
            raise WorkspaceError(f"scope commands do not match exact coordinates: {name}")
        if value["cleanup"] != canonical["cleanup"]:
            raise WorkspaceError(f"scope cleanup does not match exact coordinates: {name}")

        def forbidden_key(candidate: Any) -> bool:
            if isinstance(candidate, dict):
                return any(
                    re.search(r"password|credential|secret|token", str(key), re.IGNORECASE)
                    or forbidden_key(item)
                    for key, item in candidate.items()
                )
            if isinstance(candidate, list):
                return any(forbidden_key(item) for item in candidate)
            return False

        if forbidden_key(value):
            raise WorkspaceError(f"scope record contains forbidden secret-shaped fields: {name}")

    def profile_owner(self, profile_name: str) -> str | None:
        for record in self.list():
            if record["profile"]["name"] == profile_name:
                return record["name"]
        if not self.root.exists() or self.root.is_symlink() or not self.root.is_dir():
            return None
        for directory in sorted(self.root.iterdir()):
            if directory.is_symlink() or not directory.is_dir():
                continue
            if profile_name == f"scope-{directory.name}":
                return directory.name
        return None

    def topology_owner(self, topology_name: str) -> dict[str, Any] | None:
        for record in self.list():
            if record["topology"]["name"] == topology_name:
                return record
        return None

    def _require_unreleased(self, record: dict[str, Any]) -> None:
        path = self._release_path(record["name"])
        if not path.exists() and not path.is_symlink():
            return
        if path.is_symlink() or not path.is_file():
            raise WorkspaceError(
                f"scope release evidence is unsafe: {record['name']}"
            )
        journal = load_regular_json(path, f"scope release {record['name']}")
        if (
            not isinstance(journal, dict)
            or journal.get("schema_version") != SCOPE_RELEASE_SCHEMA_VERSION
            or journal.get("scope") != record["name"]
            or journal.get("generation") != record["generation"]
        ):
            raise WorkspaceError(
                f"scope release evidence is invalid: {record['name']}"
            )
        raise WorkspaceError(
            f"scope release has started; choose a new scope name: {record['name']}"
        )

    def release(
        self, name: str, *, apply: bool, plan_sha256: str | None = None
    ) -> dict[str, Any]:
        record = self._load_record(name)
        requests = self._release_leases(record)
        try:
            with ExitStack() as locks:
                build_roots = set(
                    self._candidate_build_roots(record["profile"]["name"])
                )
                build_roots.update(self._release_journal_build_roots(record))
                for root in sorted(build_roots):
                    locks.enter_context(
                        exclusive_lock(
                            self.paths.builds / "locks" / f"{root.name}.lock",
                            f"scope release build {root.name}",
                            nonblocking=True,
                        )
                    )
                with self.workspace._resource_locks(requests, nonblocking=True):
                    plan = self._release_plan(record)
                    release_path = self._release_path(record["name"])
                    journal: dict[str, Any] | None = None
                    if release_path.exists() or release_path.is_symlink():
                        if release_path.is_symlink() or not release_path.is_file():
                            raise WorkspaceError("scope release journal is unsafe")
                        candidate = load_regular_json(
                            release_path, f"scope release {record['name']}"
                        )
                        if (
                            not isinstance(candidate, dict)
                            or not isinstance(candidate.get("plan"), dict)
                            or set(candidate["plan"])
                            != {"schema_version", "scope", "generation", "items"}
                            or candidate["plan"].get("schema_version")
                            != SCOPE_RELEASE_SCHEMA_VERSION
                            or candidate["plan"].get("scope") != record["name"]
                            or candidate["plan"].get("generation") != record["generation"]
                            or not isinstance(candidate["plan"].get("items"), list)
                            or _canonical_sha256(candidate["plan"])
                            != candidate.get("plan_sha256")
                        ):
                            raise WorkspaceError("scope release journal plan is invalid")
                        if not self._journal_build_roots(
                            record, candidate
                        ).issubset(build_roots):
                            raise WorkspaceError(
                                "scope release journal build coordinates changed; retry"
                            )
                        journal = candidate
                        pristine = (
                            set(journal)
                            == {
                                "schema_version",
                                "scope",
                                "generation",
                                "plan_sha256",
                                "plan",
                                "status",
                                "completed",
                                "in_flight",
                                "pending_builds",
                                "updated_at",
                            }
                            and journal.get("status") == "applying"
                            and journal.get("completed") == []
                            and journal.get("in_flight") is None
                        )
                        if not pristine:
                            plan = {
                                **journal["plan"],
                                "mode": "dry-run",
                                "plan_sha256": journal["plan_sha256"],
                                "can_apply": True,
                            }
                    if not apply:
                        return plan
                    if plan_sha256 is None or not _HEX64.fullmatch(plan_sha256):
                        raise WorkspaceError("scope release apply requires the exact --plan SHA256 from dry-run")
                    if journal is not None and not pristine:
                        if journal["plan_sha256"] != plan_sha256:
                            raise WorkspaceError("scope release journal plan is invalid")
                    elif plan_sha256 != plan["plan_sha256"]:
                        raise WorkspaceError("scope release coordinates changed since preview; preview again")
                    blockers = [item for item in plan["items"] if item["disposition"] == "protected"]
                    if blockers:
                        raise WorkspaceError(
                            "scope release is protected: "
                            + "; ".join(
                                f"{item['kind']}:{','.join(item['reasons'])}" for item in blockers
                            )
                        )
                    return self._apply_release(record, plan)
        except LockBusyError as error:
            raise WorkspaceError(
                f"scope release refused active resource leases: {name}"
            ) from error

    def _candidate_build_roots(self, profile_name: str) -> list[Path]:
        parent = self.paths.builds / "profiles"
        if not parent.is_dir() or parent.is_symlink():
            return []
        return sorted(parent.glob(f"{profile_name}-*"))

    def _release_build_root(self, record: dict[str, Any], value: Any) -> Path:
        if not isinstance(value, str):
            raise WorkspaceError("scope release journal build path is invalid")
        root = Path(value)
        if (
            root.parent != self.paths.builds / "profiles"
            or not re.fullmatch(
                re.escape(record["profile"]["name"]) + r"-[0-9a-f]{64}",
                root.name,
            )
        ):
            raise WorkspaceError("scope release journal build path is invalid")
        return root

    def _journal_build_roots(
        self, record: dict[str, Any], journal: dict[str, Any]
    ) -> set[Path]:
        plan = journal.get("plan")
        if not isinstance(plan, dict) or not isinstance(plan.get("items"), list):
            raise WorkspaceError("scope release journal plan is invalid")
        roots: set[Path] = set()
        for item in plan["items"]:
            if not isinstance(item, dict):
                raise WorkspaceError("scope release journal plan is invalid")
            if item.get("kind") == "build" and item.get("disposition") == "eligible":
                roots.add(self._release_build_root(record, item.get("path")))
        pending = journal.get("pending_builds", [])
        if not isinstance(pending, list):
            raise WorkspaceError("scope release journal is invalid")
        for item in pending:
            if not isinstance(item, dict):
                raise WorkspaceError("scope release journal is invalid")
            roots.add(self._release_build_root(record, item.get("path")))
        return roots

    def _release_journal_build_roots(self, record: dict[str, Any]) -> set[Path]:
        path = self._release_path(record["name"])
        if not path.exists() and not path.is_symlink():
            return set()
        if path.is_symlink() or not path.is_file():
            raise WorkspaceError("scope release journal is unsafe")
        journal = load_regular_json(path, f"scope release {record['name']}")
        if (
            not isinstance(journal, dict)
            or journal.get("schema_version") != SCOPE_RELEASE_SCHEMA_VERSION
            or journal.get("scope") != record["name"]
            or journal.get("generation") != record["generation"]
            or not isinstance(journal.get("plan"), dict)
            or _canonical_sha256(journal["plan"]) != journal.get("plan_sha256")
        ):
            raise WorkspaceError("scope release journal plan is invalid")
        return self._journal_build_roots(record, journal)

    def _release_leases(self, record: dict[str, Any]) -> list[Any]:
        requests = [
            self.workspace._lease_request(
                "registry", f"scope:{record['name']}", "exclusive", f"release scope {record['name']}"
            ),
            self.workspace._lease_request(
                "registry", "physical-references", "exclusive", f"release scope {record['name']}"
            ),
            self.workspace._lease_request(
                "profile", record["profile"]["name"], "exclusive", f"release scope {record['name']}"
            ),
            self.workspace._lease_request(
                "topology", record["topology"]["name"], "exclusive", f"release scope {record['name']}"
            ),
        ]
        for row in record["worktrees"]:
            checkout = self.workspace.manifest.by_checkout[row["checkout"]]
            requests.extend(
                [
                    self.workspace._lease_request(
                        "git-admin",
                        self.workspace._git_admin_coordinate(checkout, Path(row["primary_path"])),
                        "exclusive",
                        f"release scope {record['name']}",
                    ),
                    self.workspace._lease_request(
                        "source",
                        self.workspace._source_coordinate(checkout.name, Path(row["path"])),
                        "exclusive",
                        f"release scope {record['name']}",
                    ),
                ]
            )
        return requests

    def _release_plan(self, record: dict[str, Any]) -> dict[str, Any]:
        from .workspace import (
            BUILD_METADATA,
            TOPOLOGY_STATUS_SCHEMA_VERSION,
            _is_clean,
            git,
        )

        items: list[dict[str, Any]] = []
        allowed_topology_reference: str | None = None
        topology_evidence: dict[str, int | str] = {}
        topology_path = Path(record["topology"]["path"])
        if topology_path.exists() or topology_path.is_symlink():
            try:
                status = self.workspace.topology_status(record["topology"]["name"])
                supervisor_liveness = status.get("supervisor", {}).get("liveness")
                unreachable = supervisor_liveness == "unreachable"
                running = supervisor_liveness == "live" or any(
                    service.get("status") == "running" or service.get("running") is True
                    for service in status.get("services", {}).values()
                )
                observation = status.get("observation", {})
                retained = any(
                    observation.get(key) == "retained"
                    for key in ("process_tree_lease", "runtime_bundle_lease")
                )
                state_policy = status.get("state_policy", {})
                retained_state = (
                    isinstance(state_policy, dict)
                    and state_policy.get("mode") == "temporary"
                    and state_policy.get("lifecycle") == "retained"
                )
                profile_mismatch = status.get("profile") != record["profile"]["name"]
                if profile_mismatch:
                    reasons = ["unexpected_topology_profile"]
                elif unreachable:
                    reasons = ["unreachable_topology"]
                elif running:
                    reasons = ["live_topology"]
                elif retained or retained_state:
                    reasons = ["retained_generation"]
                elif (
                    status.get("schema_version") != TOPOLOGY_STATUS_SCHEMA_VERSION
                    or status.get("supervisor", {}).get("liveness") != "exited"
                    or any(
                        service.get("liveness") != "exited"
                        for service in status.get("services", {}).values()
                    )
                ):
                    reasons = ["unproven_clean_topology_stop"]
                else:
                    spec_path = topology_path / "spec.json"
                    status_path = topology_path / "status.json"
                    spec_before = spec_path.stat(follow_symlinks=False)
                    status_before = status_path.stat(follow_symlinks=False)
                    spec_sha256 = _file_sha256(spec_path)
                    status_sha256 = _file_sha256(status_path)
                    spec = load_regular_json(spec_path, "scope topology spec")
                    persisted = load_regular_json(
                        status_path, "scope topology status"
                    )
                    spec_after = spec_path.stat(follow_symlinks=False)
                    status_after = status_path.stat(follow_symlinks=False)
                    records_stable = (
                        (spec_before.st_dev, spec_before.st_ino)
                        == (spec_after.st_dev, spec_after.st_ino)
                        and (status_before.st_dev, status_before.st_ino)
                        == (status_after.st_dev, status_after.st_ino)
                        and spec_sha256 == _file_sha256(spec_path)
                        and status_sha256 == _file_sha256(status_path)
                    )
                    common = (
                        "schema_version",
                        "name",
                        "profile",
                        "stack",
                        "providers",
                        "dependencies",
                        "state",
                        "build_root",
                        "resolved",
                        "control",
                        "runtime",
                    )
                    spec_endpoint = (
                        spec.get("endpoint") if isinstance(spec, dict) else None
                    )
                    persisted_endpoint = (
                        persisted.get("endpoint")
                        if isinstance(persisted, dict)
                        else None
                    )
                    endpoint_matches = (
                        spec_endpoint is None
                        and persisted_endpoint is None
                        or isinstance(spec_endpoint, dict)
                        and isinstance(persisted_endpoint, dict)
                        and {
                            key: persisted_endpoint.get(key)
                            for key in ("host", "port")
                        }
                        == {
                            key: spec_endpoint.get(key)
                            for key in ("host", "port")
                        }
                    )
                    spec_policy = (
                        spec.get("state_policy")
                        if isinstance(spec, dict)
                        else None
                    )
                    persisted_policy = (
                        persisted.get("state_policy")
                        if isinstance(persisted, dict)
                        else None
                    )
                    policy_matches = spec_policy == persisted_policy
                    if isinstance(spec_policy, dict) and isinstance(
                        persisted_policy, dict
                    ):
                        policy_matches = all(
                            spec_policy.get(key) == persisted_policy.get(key)
                            for key in set(spec_policy) | set(persisted_policy)
                            if key not in {"lifecycle", "name"}
                        )
                    observation = status.get("observation", {})
                    shutdown = status.get("shutdown")
                    observed_port = observation.get("port_reservation")
                    port_released = observed_port is None or (
                        isinstance(observed_port, dict)
                        and observed_port.get("lease") == "released"
                    )
                    records_match = (
                        records_stable
                        and isinstance(spec, dict)
                        and isinstance(persisted, dict)
                        and all(key in spec and key in persisted for key in common)
                        and all(spec.get(key) == persisted.get(key) for key in common)
                        and spec.get("schema_version")
                        == TOPOLOGY_STATUS_SCHEMA_VERSION
                        and spec.get("name") == record["topology"]["name"]
                        and spec.get("profile") == record["profile"]["name"]
                        and isinstance(spec.get("services"), dict)
                        and isinstance(persisted.get("services"), dict)
                        and set(spec["services"]) == set(persisted["services"])
                        and "endpoint" in spec
                        and "endpoint" in persisted
                        and endpoint_matches
                        and ("state_policy" in spec)
                        == ("state_policy" in persisted)
                        and all(
                            (key in spec) == (key in persisted)
                            and spec.get(key) == persisted.get(key)
                            for key in ("port_reservation", "sound")
                        )
                        and policy_matches
                    )
                    scope_worktrees = {
                        row["checkout"]: row for row in record["worktrees"]
                    }
                    resolved = spec.get("resolved") if isinstance(spec, dict) else None
                    scope_worktrees_by_path = {
                        Path(row["path"]).resolve(strict=False): row
                        for row in record["worktrees"]
                    }
                    scope_coordinates_match = isinstance(resolved, dict)
                    if isinstance(resolved, dict):
                        for coordinate in resolved.values():
                            if not isinstance(coordinate, dict) or not isinstance(
                                coordinate.get("checkout_path"), str
                            ):
                                scope_coordinates_match = False
                                break
                            checkout = coordinate.get("checkout")
                            declared_row = scope_worktrees.get(checkout)
                            path_row = scope_worktrees_by_path.get(
                                Path(coordinate["checkout_path"]).resolve(
                                    strict=False
                                )
                            )
                            rows_to_check: list[dict[str, Any]] = []
                            for row in (declared_row, path_row):
                                if row is not None and row not in rows_to_check:
                                    rows_to_check.append(row)
                            for row in rows_to_check:
                                if (
                                    checkout != row["checkout"]
                                    or Path(coordinate["checkout_path"]).resolve(
                                        strict=False
                                    )
                                    != Path(row["path"]).resolve(strict=False)
                                    or coordinate.get("head") != row["commit"]
                                    or coordinate.get("dirty") is not False
                                ):
                                    scope_coordinates_match = False
                                    break
                            if not scope_coordinates_match:
                                break
                    records_match = records_match and scope_coordinates_match
                    stopped_cleanly = (
                        isinstance(persisted, dict)
                        and isinstance(persisted.get("stopped_at"), str)
                        and bool(persisted["stopped_at"])
                        and persisted.get("error") is None
                        and shutdown
                        == {"control_requested": True, "clean": True}
                        and status.get("shutdown") == shutdown
                        and status.get("error") is None
                        and observation.get("process_tree_lease") == "released"
                        and observation.get("runtime_bundle_lease") == "released"
                        and port_released
                        and all(
                            not service.get("running")
                            and service.get("status") == "exited"
                            for service in status.get("services", {}).values()
                        )
                    )
                    state_disposed = (
                        not isinstance(state_policy, dict)
                        or state_policy.get("mode") != "temporary"
                        or state_policy.get("lifecycle") in {"removed", "promoted"}
                    )
                    clean = records_match and stopped_cleanly and state_disposed
                    if clean:
                        topology_evidence = {
                            "spec_device": portable_device(spec_after),
                            "spec_inode": spec_after.st_ino,
                            "spec_sha256": spec_sha256,
                            "status_device": portable_device(status_after),
                            "status_inode": status_after.st_ino,
                            "status_sha256": status_sha256,
                        }
                    reasons = (
                        ["stopped_topology_history_retained"]
                        if clean
                        else [
                            "unproven_clean_topology_stop",
                            *(
                                []
                                if records_match
                                else ["mismatched_topology_records"]
                            ),
                            *(
                                []
                                if stopped_cleanly
                                else ["unclean_topology_shutdown"]
                            ),
                            *(
                                []
                                if state_disposed
                                else ["retained_temporary_state"]
                            ),
                        ]
                    )
                disposition = (
                    "protected"
                    if reasons != ["stopped_topology_history_retained"]
                    else "retained"
                )
                if disposition == "retained":
                    allowed_topology_reference = (
                        f"topology:{record['topology']['name']}"
                    )
            except (OSError, WorkspaceError) as error:
                reasons = [f"unreachable_topology:{error}"]
                disposition = "protected"
            items.append(
                {
                    "kind": "topology",
                    "path": str(topology_path),
                    "disposition": disposition,
                    "reasons": reasons,
                    **topology_evidence,
                }
            )
        else:
            items.append(
                {"kind": "topology", "path": str(topology_path), "disposition": "absent", "reasons": ["not_created"]}
            )

        policy = record["state_policy"]
        items.append(
            {
                "kind": "state",
                "path": None,
                "disposition": "retained" if policy["mode"] != "temporary" else "absent",
                "reasons": [
                    "persistent_state_never_removed"
                    if policy["mode"] != "temporary"
                    else "generation_owned_state_removed_on_clean_stop"
                ],
            }
        )

        build_roots = self._candidate_build_roots(record["profile"]["name"])
        for root in build_roots:
            reasons: list[str] = []
            marker = root / MANAGED_MARKER
            metadata_path = root / BUILD_METADATA
            marker_sha256: str | None = None
            metadata_sha256: str | None = None
            if root.is_symlink() or not root.is_dir() or not marker.is_file() or marker.is_symlink():
                reasons.append("uncertain_build_ownership")
            else:
                try:
                    metadata = load_regular_json(metadata_path, "scope build metadata")
                    marker_value = load_regular_json(marker, "scope build marker")
                    if (
                        not isinstance(metadata, dict)
                        or metadata.get("profile") != record["profile"]["name"]
                        or not isinstance(metadata.get("key"), str)
                        or root.name != f"{record['profile']['name']}-{metadata['key']}"
                        or marker_value
                        != {
                            "schema_version": 1,
                            "purpose": f"profile:{record['profile']['name']}:{metadata['key']}",
                        }
                    ):
                        reasons.append("uncertain_build_ownership")
                    else:
                        metadata_sha256 = _file_sha256(metadata_path)
                        marker_sha256 = _file_sha256(marker)
                except (OSError, WorkspaceError):
                    reasons.append("uncertain_build_ownership")
            disposition = "protected" if reasons else "eligible"
            identity = root.stat(follow_symlinks=False) if disposition == "eligible" else None
            items.append(
                {
                    "kind": "build",
                    "path": str(root),
                    "device": portable_device(identity) if identity is not None else None,
                    "inode": identity.st_ino if identity is not None else None,
                    "metadata_sha256": metadata_sha256,
                    "marker_sha256": marker_sha256,
                    "disposition": disposition,
                    "reasons": reasons or ["scope_profile_build"],
                }
            )

        profile_path = Path(record["profile"]["path"])
        profile_reasons: list[str] = []
        if profile_path.exists() or profile_path.is_symlink():
            if profile_path.is_symlink() or not profile_path.is_file():
                profile_reasons.append("replaced_profile")
            elif _file_sha256(profile_path) != record["profile"]["sha256"]:
                profile_reasons.append("changed_profile")
            else:
                profile_identity = profile_path.stat(follow_symlinks=False)
                if not pair_matches(
                    {
                        "device": record["profile"]["path_device"],
                        "inode": record["profile"]["path_inode"],
                    },
                    profile_identity,
                ):
                    profile_reasons.append("replaced_profile")
            disposition = "protected" if profile_reasons else "eligible"
        else:
            disposition = "absent"
            profile_reasons.append("already_removed")
        items.append(
            {"kind": "profile", "path": str(profile_path), "disposition": disposition, "reasons": profile_reasons or ["scope_owned"]}
        )

        for row in record["worktrees"]:
            path = Path(row["path"])
            reasons: list[str] = []
            branch_head: str | None = None
            if path.exists() or path.is_symlink():
                if path.is_symlink() or not path.is_dir():
                    reasons.append("replaced_path")
                else:
                    try:
                        identity = path.stat(follow_symlinks=False)
                        if not pair_matches(
                            {
                                "device": row["path_device"],
                                "inode": row["path_inode"],
                            },
                            identity,
                        ):
                            reasons.append("replaced_path")
                        if self.workspace._git_common_directory(path, trace=False) != Path(row["common_git_dir"]):
                            reasons.append("changed_common_git_identity")
                        branch_head = git(
                            path, "rev-parse", "HEAD", capture=True, trace=False
                        )
                        git(
                            path,
                            "merge-base",
                            "--is-ancestor",
                            row["commit"],
                            branch_head,
                            capture=True,
                            trace=False,
                        )
                        branch = git(path, "branch", "--show-current", capture=True, trace=False)
                        if branch != row["branch"]:
                            reasons.append("detached_or_changed_branch")
                        if not _is_clean(path):
                            reasons.append("dirty_worktree")
                        references = self.workspace._source_references(path)
                        unexpected = [
                            reference
                            for reference in references
                            if reference != f"profile:{record['profile']['name']}"
                            and reference != record["profile"]["name"]
                            and reference != f"scope:{record['name']}"
                            and reference != allowed_topology_reference
                        ]
                        if unexpected:
                            reasons.append("unexpected_references:" + ",".join(unexpected))
                    except (OSError, WorkspaceError) as error:
                        reasons.append(f"ambiguous_git_state:{error}")
                disposition = "protected" if reasons else "eligible"
            else:
                try:
                    branch_head = git(
                        Path(row["primary_path"]),
                        "for-each-ref",
                        "--format=%(objectname)",
                        f"refs/heads/{row['branch']}",
                        capture=True,
                        trace=False,
                    )
                    if branch_head and branch_head != row["commit"]:
                        disposition = "protected"
                        reasons.append("changed_scope_branch")
                    else:
                        unexpected = [
                            reference
                            for reference in self.workspace._source_references(path)
                            if reference != f"profile:{record['profile']['name']}"
                            and reference != record["profile"]["name"]
                            and reference != f"scope:{record['name']}"
                            and reference != allowed_topology_reference
                        ]
                        if unexpected:
                            disposition = "protected"
                            reasons.append(
                                "unexpected_references:" + ",".join(unexpected)
                            )
                        else:
                            disposition = "eligible" if branch_head else "absent"
                            reasons.append(
                                "worktree_removed_branch_pending"
                                if branch_head
                                else "already_removed"
                            )
                except (OSError, WorkspaceError) as error:
                    disposition = "protected"
                    reasons.append(f"ambiguous_scope_branch:{error}")
            items.append(
                {
                    "kind": "worktree",
                    "checkout": row["checkout"],
                    "path": str(path),
                    "branch_head": branch_head,
                    "disposition": disposition,
                    "reasons": reasons or ["scope_owned_clean_exact"],
                }
            )

        identity = {
            "schema_version": SCOPE_RELEASE_SCHEMA_VERSION,
            "scope": record["name"],
            "generation": record["generation"],
            "items": items,
        }
        return {
            **identity,
            "mode": "dry-run",
            "plan_sha256": _canonical_sha256(identity),
            "can_apply": not any(item["disposition"] == "protected" for item in items),
        }

    def _apply_release(self, record: dict[str, Any], plan: dict[str, Any]) -> dict[str, Any]:
        from .workspace import (
            BUILD_METADATA,
            _owned_tree_tombstone_path,
            _portable_tombstone_path,
            git,
            remove_owned_tree,
        )

        expected_actions = [
            f"build:{item['path']}"
            for item in plan["items"]
            if item["kind"] == "build" and item["disposition"] == "eligible"
        ]
        if any(
            item["kind"] == "profile" and item["disposition"] == "eligible"
            for item in plan["items"]
        ):
            expected_actions.append("profile")
        expected_actions.extend(
            reversed(
                [
                    f"worktree:{item['checkout']}"
                    for item in plan["items"]
                    if item["kind"] == "worktree"
                    and item["disposition"] == "eligible"
                ]
            )
        )
        topology_item = next(
            item for item in plan["items"] if item["kind"] == "topology"
        )

        def revalidate_topology_evidence() -> None:
            if topology_item["disposition"] != "retained":
                return
            topology_root = Path(topology_item["path"])
            for prefix, filename in (("spec", "spec.json"), ("status", "status.json")):
                path = topology_root / filename
                if path.is_symlink() or not path.is_file():
                    raise WorkspaceError(
                        "scope topology evidence changed during release"
                    )
                identity = path.stat(follow_symlinks=False)
                if (
                    portable_device(identity)
                    != topology_item.get(f"{prefix}_device")
                    or identity.st_ino != topology_item.get(f"{prefix}_inode")
                    or _file_sha256(path) != topology_item.get(f"{prefix}_sha256")
                ):
                    raise WorkspaceError(
                        "scope topology evidence changed during release"
                    )
        completed: list[str] = []
        release_path = self._release_path(record["name"])
        if release_path.exists() or release_path.is_symlink():
            if release_path.is_symlink() or not release_path.is_file():
                raise WorkspaceError("scope release journal is unsafe")
            previous = load_regular_json(release_path, f"scope release {record['name']}")
            pristine = (
                isinstance(previous, dict)
                and previous.get("status") == "applying"
                and previous.get("completed") == []
                and previous.get("in_flight") is None
            )
            if (
                not isinstance(previous, dict)
                or previous.get("schema_version") != SCOPE_RELEASE_SCHEMA_VERSION
                or previous.get("scope") != record["name"]
                or previous.get("generation") != record["generation"]
                or not isinstance(previous.get("completed"), list)
                or not all(isinstance(item, str) for item in previous["completed"])
                or previous.get("in_flight") is not None
                and not (
                    isinstance(previous.get("in_flight"), str)
                    or isinstance(previous.get("in_flight"), dict)
                    and set(previous["in_flight"]) == {"action", "phase"}
                    and isinstance(previous["in_flight"]["action"], str)
                    and previous["in_flight"]["phase"]
                    in {"prepared", "removing"}
                )
                or not isinstance(previous.get("plan"), dict)
                or set(previous["plan"])
                != {"schema_version", "scope", "generation", "items"}
                or _canonical_sha256(previous["plan"])
                != previous.get("plan_sha256")
                or not pristine
                and previous.get("plan") != {
                    key: plan[key]
                    for key in ("schema_version", "scope", "generation", "items")
                }
                or not pristine
                and previous.get("plan_sha256") != plan["plan_sha256"]
                or previous.get("status") not in {"applying", "complete"}
                or previous["completed"]
                != expected_actions[: len(previous["completed"])]
                or (
                    previous.get("in_flight", {}).get("action")
                    if isinstance(previous.get("in_flight"), dict)
                    else previous.get("in_flight")
                )
                not in (
                    None,
                    expected_actions[len(previous["completed"])]
                    if len(previous["completed"]) < len(expected_actions)
                    else None,
                )
                or previous.get("status") == "complete"
                and (
                    previous["completed"] != expected_actions
                    or previous.get("in_flight") is not None
                )
                or "pending_builds" in previous
                and (
                    not isinstance(previous["pending_builds"], list)
                    or any(
                        not isinstance(item, dict)
                        or set(item)
                        != {
                            "path",
                            "device",
                            "inode",
                            "metadata_sha256",
                            "marker_sha256",
                        }
                        or not isinstance(item["path"], str)
                        or Path(item["path"]).parent
                        != self.workspace.paths.builds / "profiles"
                        or not re.fullmatch(
                            re.escape(record["profile"]["name"])
                            + r"-[0-9a-f]{64}",
                            Path(item["path"]).name,
                        )
                        or not all(
                            isinstance(item[key], int)
                            and not isinstance(item[key], bool)
                            and item[key] >= 0
                            for key in ("device", "inode")
                        )
                        or not all(
                            isinstance(item[key], str)
                            and re.fullmatch(r"[0-9a-f]{64}", item[key])
                            for key in ("metadata_sha256", "marker_sha256")
                        )
                        for item in previous["pending_builds"]
                    )
                )
            ):
                raise WorkspaceError("scope release journal is invalid")
            if pristine:
                previous = {}
                completed = []
                in_flight = None
            else:
                completed = list(dict.fromkeys(previous["completed"]))
                in_flight = previous.get("in_flight")
        else:
            previous = {}
            in_flight = None
        pending_by_path = {
            item["path"]: item
            for item in previous.get("pending_builds", [])
        }
        for item in plan["items"]:
            if item["kind"] != "build" or item["disposition"] != "eligible":
                continue
            evidence = {
                key: item[key]
                for key in (
                    "path",
                    "device",
                    "inode",
                    "metadata_sha256",
                    "marker_sha256",
                )
            }
            prior = pending_by_path.setdefault(item["path"], evidence)
            if prior != evidence:
                raise WorkspaceError("scope release build intent changed")
        pending_builds = [pending_by_path[path] for path in sorted(pending_by_path)]
        journal: dict[str, Any] = {
            "schema_version": SCOPE_RELEASE_SCHEMA_VERSION,
            "scope": record["name"],
            "generation": record["generation"],
            "plan_sha256": plan["plan_sha256"],
            "plan": {
                key: plan[key]
                for key in ("schema_version", "scope", "generation", "items")
            },
            "status": "applying",
            "completed": completed,
            "in_flight": in_flight,
            "pending_builds": pending_builds,
            "updated_at": _now(),
        }
        durable_atomic_json(self._release_path(record["name"]), journal)
        self._maybe_fail("release:journal")
        revalidate_topology_evidence()

        def in_flight_action() -> str | None:
            current = journal["in_flight"]
            return current.get("action") if isinstance(current, dict) else current

        def in_flight_phase() -> str | None:
            current = journal["in_flight"]
            return current.get("phase") if isinstance(current, dict) else None

        def begin(action: str) -> None:
            current = journal["in_flight"]
            if current is not None and in_flight_action() != action:
                raise WorkspaceError(
                    f"scope release journal has unfinished action: {current}"
                )
            if current is None:
                journal["in_flight"] = {"action": action, "phase": "prepared"}
                journal["updated_at"] = _now()
                durable_atomic_json(self._release_path(record["name"]), journal)

        def mark_removing(action: str) -> None:
            if in_flight_action() != action:
                raise WorkspaceError(f"scope release action was not prepared: {action}")
            journal["in_flight"] = {"action": action, "phase": "removing"}
            journal["updated_at"] = _now()
            durable_atomic_json(self._release_path(record["name"]), journal)

        def finish(action: str) -> None:
            if in_flight_action() != action:
                raise WorkspaceError(f"scope release action was not prepared: {action}")
            journal["completed"].append(action)
            journal["in_flight"] = None
            journal["updated_at"] = _now()
            durable_atomic_json(self._release_path(record["name"]), journal)
        for item in plan["items"]:
            if item["kind"] != "build" or item["disposition"] != "eligible":
                continue
            root = Path(item["path"])
            action = f"build:{item['path']}"
            if action in journal["completed"]:
                if root.exists() or root.is_symlink():
                    raise WorkspaceError(f"completed scope build reappeared: {root}")
                continue
            resuming_removal = (
                in_flight_action() == action and in_flight_phase() == "removing"
            )
            begin(action)
            if resuming_removal:
                recovery_identity = {
                    "device": item["device"],
                    "inode": item["inode"],
                }
                tombstone = _portable_tombstone_path(root, recovery_identity)
                if tombstone is None and (root.exists() or root.is_symlink()):
                    metadata = root.stat(follow_symlinks=False)
                    tombstone = _owned_tree_tombstone_path(
                        root,
                        {"device": metadata.st_dev, "inode": metadata.st_ino},
                    )
                if (
                    root.exists()
                    or root.is_symlink()
                    or tombstone is not None
                    and (tombstone.exists() or tombstone.is_symlink())
                ):
                    target = root if root.exists() or root.is_symlink() else tombstone
                    if target is None:
                        raise WorkspaceError(
                            f"scope build removal evidence disappeared: {root}"
                        )
                    metadata = target.stat(follow_symlinks=False)
                    if not pair_matches(recovery_identity, metadata):
                        raise WorkspaceError(
                            f"scope build removal identity changed: {root}"
                        )
                    remove_owned_tree(
                        root,
                        expected_identity={
                            "device": metadata.st_dev,
                            "inode": metadata.st_ino,
                        },
                    )
                finish(action)
                continue
            if not root.exists() and not root.is_symlink():
                raise WorkspaceError(
                    f"prepared scope build disappeared before removal: {root}"
                )
            metadata_path = root / BUILD_METADATA
            marker_path = root / MANAGED_MARKER
            if (
                metadata_path.is_symlink()
                or not metadata_path.is_file()
                or marker_path.is_symlink()
                or not marker_path.is_file()
                or _file_sha256(metadata_path) != item["metadata_sha256"]
                or _file_sha256(marker_path) != item["marker_sha256"]
            ):
                raise WorkspaceError(
                    f"scope build ownership changed during release: {root}"
                )
            mark_removing(action)
            root_metadata = root.stat(follow_symlinks=False)
            if not pair_matches(
                {"device": item["device"], "inode": item["inode"]},
                root_metadata,
            ):
                raise WorkspaceError(
                    f"scope build removal identity changed: {root}"
                )
            remove_owned_tree(
                root,
                expected_identity={
                    "device": root_metadata.st_dev,
                    "inode": root_metadata.st_ino,
                },
            )
            self._maybe_fail(f"release:build-tree:{root.name}")
            finish(action)
            self._maybe_fail(f"release:build:{root.name}")

        profile_path = Path(record["profile"]["path"])
        revalidate_topology_evidence()
        if any(item["kind"] == "profile" and item["disposition"] == "eligible" for item in plan["items"]):
            if "profile" in journal["completed"]:
                if profile_path.exists() or profile_path.is_symlink():
                    raise WorkspaceError("completed scope profile reappeared")
            else:
                resuming_removal = (
                    in_flight_action() == "profile"
                    and in_flight_phase() == "removing"
                )
                begin("profile")
                if not profile_path.exists() and not profile_path.is_symlink():
                    if not resuming_removal:
                        raise WorkspaceError(
                            "prepared scope profile disappeared before removal"
                        )
                    # The unlink and physical-reference update are separate
                    # durable mutations. Repeating the reference removal is
                    # the exact recovery for a crash between them.
                    self.workspace._remove_physical_reference(profile_path)
                    finish("profile")
                elif (
                    profile_path.is_symlink()
                    or not profile_path.is_file()
                    or _file_sha256(profile_path) != record["profile"]["sha256"]
                    or not pair_matches(
                        {
                            "device": record["profile"]["path_device"],
                            "inode": record["profile"]["path_inode"],
                        },
                        profile_path.stat(follow_symlinks=False),
                    )
                ):
                    raise WorkspaceError("scope profile changed during release")
                else:
                    mark_removing("profile")
                    profile_path.unlink()
                    self._maybe_fail("release:profile-file")
                    self.workspace._remove_physical_reference(profile_path)
                    finish("profile")
            self._maybe_fail("release:profile")

        by_checkout = {
            item["checkout"]: item
            for item in plan["items"]
            if item["kind"] == "worktree"
        }
        for row in reversed(record["worktrees"]):
            item = by_checkout[row["checkout"]]
            if item["disposition"] != "eligible":
                continue
            path = Path(row["path"])
            action = f"worktree:{row['checkout']}"
            primary = Path(row["primary_path"])
            revalidate_topology_evidence()
            if action in journal["completed"]:
                if path.exists() or path.is_symlink():
                    raise WorkspaceError(f"completed scope worktree reappeared: {path}")
                branch_head = git(
                    primary,
                    "for-each-ref",
                    "--format=%(objectname)",
                    f"refs/heads/{row['branch']}",
                    capture=True,
                    trace=False,
                )
                if branch_head:
                    raise WorkspaceError(
                        f"completed scope branch reappeared: {row['branch']}"
                    )
                continue
            resuming_removal = (
                in_flight_action() == action and in_flight_phase() == "removing"
            )
            begin(action)
            if not path.exists() and not path.is_symlink():
                branch_head = git(
                    primary,
                    "for-each-ref",
                    "--format=%(objectname)",
                    f"refs/heads/{row['branch']}",
                    capture=True,
                    trace=False,
                )
                if not resuming_removal:
                    if not branch_head:
                        raise WorkspaceError(
                            f"prepared scope branch disappeared before removal: "
                            f"{row['branch']}"
                        )
                    if branch_head != item["branch_head"]:
                        raise WorkspaceError(
                            f"scope branch changed during release: {row['branch']}"
                        )
                    mark_removing(action)
                if branch_head:
                    if branch_head != item["branch_head"]:
                        raise WorkspaceError(
                            f"scope branch changed during release: {row['branch']}"
                        )
                    git(
                        primary,
                        "update-ref",
                        "-d",
                        f"refs/heads/{row['branch']}",
                        item["branch_head"],
                        trace=False,
                    )
                    self._maybe_fail(
                        f"release:worktree-branch:{row['checkout']}"
                    )
                finish(action)
                continue
            if path.is_symlink() or not path.is_dir():
                raise WorkspaceError(f"scope worktree changed during release: {path}")
            identity = path.stat(follow_symlinks=False)
            if (
                not pair_matches(
                    {
                        "device": row["path_device"],
                        "inode": row["path_inode"],
                    },
                    identity,
                )
                or self.workspace._git_common_directory(path, trace=False)
                != Path(row["common_git_dir"])
                or git(path, "rev-parse", "HEAD", capture=True, trace=False)
                != item["branch_head"]
                or git(path, "branch", "--show-current", capture=True, trace=False)
                != row["branch"]
            ):
                raise WorkspaceError(f"scope worktree changed during release: {path}")
            from .workspace import _is_clean

            if not _is_clean(path):
                raise WorkspaceError(f"scope worktree became dirty during release: {path}")
            if [
                reference
                for reference in self.workspace._source_references(path)
                if reference != f"scope:{record['name']}"
                and reference != f"topology:{record['topology']['name']}"
            ]:
                raise WorkspaceError(f"scope worktree became referenced during release: {path}")
            mark_removing(action)
            git(primary, "worktree", "remove", str(path), trace=False)
            self._maybe_fail(f"release:worktree-path:{row['checkout']}")
            branch_head = git(
                primary,
                "for-each-ref",
                "--format=%(objectname)",
                f"refs/heads/{row['branch']}",
                capture=True,
                trace=False,
            )
            if branch_head:
                if branch_head != item["branch_head"]:
                    raise WorkspaceError(
                        f"scope branch changed during release: {row['branch']}"
                    )
                git(
                    primary,
                    "update-ref",
                    "-d",
                    f"refs/heads/{row['branch']}",
                    item["branch_head"],
                    trace=False,
                )
            finish(action)
            self._maybe_fail(f"release:worktree:{row['checkout']}")
        if journal["in_flight"] is not None:
            raise WorkspaceError(
                f"scope release journal has unfinished action: {journal['in_flight']}"
            )
        missing_build_evidence = [
            f"build:{intent['path']}"
            for intent in pending_builds
            if f"build:{intent['path']}" not in journal["completed"]
        ]
        if missing_build_evidence:
            raise WorkspaceError(
                "scope release build actions are incomplete: "
                + ", ".join(missing_build_evidence)
            )
        journal["status"] = "complete"
        journal["updated_at"] = _now()
        durable_atomic_json(self._release_path(record["name"]), journal)
        self._maybe_fail("release:complete")
        return {**plan, "mode": "apply", "released": True, "journal": str(self._release_path(record["name"]))}
