from __future__ import annotations

import copy
import base64
import importlib.util
import json
import os
from pathlib import Path
import py_compile
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock

from atrinik_workspace.model import Manifest
from atrinik_workspace.workspace import _parse_worktree_porcelain


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (
    ROOT
    / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
)
SPEC = importlib.util.spec_from_file_location("atrinik_delivery_ledger", SCRIPT)
if SPEC is None or SPEC.loader is None:  # pragma: no cover - import bootstrap guard
    raise RuntimeError(f"cannot import {SCRIPT}")
ledger = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ledger
_DONT_WRITE_BYTECODE = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    SPEC.loader.exec_module(ledger)
finally:
    sys.dont_write_bytecode = _DONT_WRITE_BYTECODE


SHA_A = "a" * 40
SHA_B = "b" * 40
SHA_C = "c" * 40
INITIAL_PR_BODY = b"Initial delivery pull request body\n"
SAFE = {
    "clean": True,
    "detached": False,
    "locked": False,
    "active": False,
    "unowned_reference": False,
    "foreign": False,
    "certain": True,
}


def inline_payload(raw: bytes) -> dict[str, str]:
    return {
        "encoding": "base64",
        "raw_base64": base64.b64encode(raw).decode("ascii"),
        "sha256": ledger.byte_digest(raw),
    }


def repository(name: str = "atrinik", node: str = "R_repo") -> dict[str, object]:
    return {"owner": "atrinik", "name": name, "node_id": node}


def retarget_repository(value: object, target: dict[str, object]) -> None:
    if isinstance(value, dict):
        if value == repository():
            value.update(copy.deepcopy(target))
            return
        for item in value.values():
            retarget_repository(item, target)
    elif isinstance(value, list):
        for index, item in enumerate(value):
            if item == "R_repo":
                value[index] = target["node_id"]
            else:
                retarget_repository(item, target)


def issue(number: int = 419, node: str = "I_issue") -> dict[str, object]:
    return {"repository": repository(), "number": number, "node_id": node}


def identity(
    *,
    branch: str,
    path: str | None = None,
    number: int | None = None,
    node_id: str | None = None,
    body_digest: str | None = None,
    head_sha: str | None = None,
) -> dict[str, object]:
    result: dict[str, object] = {
        "repository": repository(),
        "branch": branch,
        "path": path,
        "number": number,
        "node_id": node_id,
        "body_digest": body_digest,
    }
    if head_sha is not None:
        result["head_sha"] = head_sha
    return result


def artifact(
    slot_id: str,
    kind: str,
    *,
    branch: str,
    path: str | None = None,
    number: int | None = None,
    node_id: str | None = None,
    body_digest: str | None = None,
    state: str = "planned",
    head_sha: str = SHA_A,
    producer_resource_slot: str | None = None,
    initial_body: bytes | None = None,
) -> dict[str, object]:
    if kind == "pull_request" and state in {"planned", "created"}:
        initial_body = INITIAL_PR_BODY if initial_body is None else initial_body
        body_digest = ledger.byte_digest(initial_body)
    immutable = identity(
        branch=branch,
        path=path,
        number=number,
        node_id=node_id,
        body_digest=body_digest,
    )
    current = None
    if state != "planned":
        current = identity(
            branch=branch,
            path=path,
            number=number,
            node_id=node_id,
            body_digest=body_digest,
            head_sha=head_sha,
        )
    return {
        "slot_id": slot_id,
        "kind": kind,
        "state": state,
        "immutable": immutable,
        "current": current,
        "producer_resource_slot": producer_resource_slot,
        **(
            {"primitive_request": None, "primitive_result": None}
            if kind == "worktree"
            else {}
        ),
        **(
            {
                "initial_body_payload": (
                    inline_payload(initial_body)
                    if initial_body is not None
                    else None
                )
            }
            if kind == "pull_request"
            else {}
        ),
        "safety": (
            None
            if state == "planned"
            else {
                "clean": True,
                "detached": False,
                "locked": False,
                "active": False,
                "unowned_reference": False,
                "foreign": False,
                "certain": True,
            }
        ),
    }


def scope_request(
    *,
    name: str = "issue-419-scope",
    component: str = "atrinik",
    profile: str = "default",
    checkout: str = "atrinik",
    label: str = "issue-419",
    branch: str = "docs/issue-419",
    start_sha: str = SHA_A,
    topology: str | None = None,
    roots: dict[str, object] | None = None,
) -> dict[str, object]:
    if roots is None:
        roots = {
            "wrapper": {"path": "/wrapper", "device": 1, "inode": 1},
            "workspace": {"path": "/workspace-data", "device": 1, "inode": 2},
            "primary": {"path": f"/wrapper/{checkout}", "device": 1, "inode": 3},
        }
    return {
        "name": name,
        "component": component,
        "profile": profile,
        "physical_checkout": checkout,
        "label": label,
        "branch": branch,
        "start_sha": start_sha,
        "temporary_state": True,
        "state_policy": {
            "mode": "temporary",
            "name": None,
            "ownership": "topology-generation",
            "lifecycle": "remove-on-clean-stop",
        },
        "topology": topology or f"scope-{name}",
        "roots": copy.deepcopy(roots),
    }


def scope_resource(request: dict[str, object] | None = None) -> dict[str, object]:
    request = scope_request() if request is None else request
    return {
        "slot_id": "scope",
        "kind": "scope",
        "state": "planned",
        "immutable": {
            "repository": repository(),
            "name": request["name"],
            "path": None,
        },
        "current": None,
        "request": copy.deepcopy(request),
    }


def scope_show_bytes(
    request: dict[str, object],
    *,
    generation: str = "1" * 32,
    repository_name: str = "atrinik/atrinik",
) -> bytes:
    scope_name = request["name"]
    profile_name = f"scope-{scope_name}"
    checkout = request["physical_checkout"]
    label = request["label"]
    topology_name = request["topology"]
    workspace_root = request["roots"]["workspace"]["path"]
    worktree_path = f"{workspace_root}/worktrees/{checkout}/{label}"
    profile_path = f"{workspace_root}/profiles/{profile_name}.json"
    topology_path = f"{workspace_root}/topologies/{topology_name}"
    worktree_status = Path(worktree_path).stat()
    profile_file = Path(profile_path)
    profile_file.parent.mkdir(parents=True, exist_ok=True)
    wrapper_manifest = json.loads(
        (Path(request["roots"]["wrapper"]["path"]) / "components.json").read_text(
            encoding="utf-8"
        )
    )
    manifest = Manifest.from_value(wrapper_manifest)
    stack = manifest.stack("default")
    profile_components = {
        component: {
            "kind": "worktree" if component == request["component"] else "primary",
            "value": request["label"] if component == request["component"] else "",
        }
        for component in wrapper_manifest["stacks"]["default"]["components"]
    }
    profile_raw = json_bytes(
        {
            "schema_version": 5,
            "name": profile_name,
            "stack": "default",
            "sound_mode": "source",
            "sound_release": None,
            "components": profile_components,
        }
    )
    profile_file.write_bytes(profile_raw)
    profile_status = profile_file.stat(follow_symlinks=False)
    row = {
        "checkout": checkout,
        "repository": repository_name,
        "logical_components": [request["component"]],
        "label": label,
        "branch": request["branch"],
        "start_point": request["start_sha"],
        "commit": request["start_sha"],
        "tree": git_run(Path(worktree_path), "rev-parse", "HEAD^{tree}").stdout.strip(),
        "path": worktree_path,
        "primary_path": request["roots"]["primary"]["path"],
        "common_git_dir": f"{request['roots']['primary']['path']}/.git",
        "path_device": worktree_status.st_dev,
        "path_inode": worktree_status.st_ino,
        "created_by_scope": True,
    }
    profile = {
        "name": profile_name,
        "path": profile_path,
        "sha256": ledger.byte_digest(profile_raw),
        "path_device": profile_status.st_dev,
        "path_inode": profile_status.st_ino,
        "immutable": True,
    }
    topology = {"name": topology_name, "path": topology_path}
    request_document = {
        "name": scope_name,
        "base_profile": request["profile"],
        "stack": "default",
        "requested_components": [request["component"]],
        "profile": {"name": profile_name, "path": profile_path},
        "topology": topology,
        "state_policy": request["state_policy"],
        "worktrees": [
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
                )
            }
        ],
    }
    record = {
        "schema_version": 1,
        "status": "complete",
        "name": scope_name,
        "generation": generation,
        "request_sha256": ledger.canonical_object_digest(request_document),
        "created_at": "2026-08-14T18:00:00Z",
        "base_profile": request["profile"],
        "stack": "default",
        "requested_components": [request["component"]],
        "worktrees": [row],
        "profile": profile,
        "topology": topology,
        "state_policy": request["state_policy"],
        "commands": {
            "paths": {
                component.name: (
                    f"./atrinik path {component.name} --profile {profile_name}"
                )
                for component in stack.components
            },
            "builds": {
                component.name: (
                    f"./atrinik build {component.name} --profile {profile_name} --test"
                )
                for component in stack.components
                if manifest.effective_build(stack.name, component) != "none"
            },
            "topology_show": f"./atrinik topology show {profile_name} --temporary-state --json",
            "up": f"./atrinik up --name {topology_name} --profile {profile_name} --temporary-state --json",
            "ps": f"./atrinik ps {topology_name} --json",
            "logs": {
                "server": f"./atrinik logs {topology_name} server --tail 100",
                "client": f"./atrinik logs {topology_name} client --tail 100",
            },
            "down": f"./atrinik down {topology_name} --json",
            "release_preview": f"./atrinik scope release {scope_name} --dry-run --json",
            "release_apply": f"./atrinik scope release {scope_name} --apply --plan PLAN_SHA256 --json",
        },
        "cleanup": {
            "policy": "explicit-preview-first",
            "journal": f"{workspace_root}/scopes/{scope_name}/creation-journal.json",
            "release_journal": f"{workspace_root}/scopes/{scope_name}/release-journal.json",
        },
    }
    return (json.dumps(record, indent=2, sort_keys=True) + "\n").encode("utf-8")


def live_roots(base: Path, checkout: str) -> dict[str, object]:
    wrapper = base / "wrapper"
    workspace = base / "alternate-workspace-data"
    wrapper.mkdir(parents=True, exist_ok=True)
    workspace.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "components.json", wrapper / "components.json")
    shutil.copy2(ROOT / "atrinik", wrapper / "atrinik")
    shutil.copytree(ROOT / "atrinik_workspace", wrapper / "atrinik_workspace")
    git_run(wrapper, "init", "--initial-branch=main")
    git_run(wrapper, "config", "user.name", "Delivery Test")
    git_run(wrapper, "config", "user.email", "delivery@example.invalid")
    git_run(wrapper, "remote", "add", "origin", "https://github.com/atrinik/atrinik.git")
    git_run(wrapper, "add", "components.json", "atrinik", "atrinik_workspace")
    git_run(wrapper, "commit", "-m", "test wrapper")
    primary = wrapper if checkout == "atrinik" else wrapper / checkout
    if primary != wrapper:
        primary.mkdir(parents=True)
        git_run(primary, "init", "--initial-branch=main")
        git_run(primary, "config", "user.name", "Delivery Test")
        git_run(primary, "config", "user.email", "delivery@example.invalid")
        (primary / "tracked.txt").write_text("tracked\n", encoding="utf-8")
        git_run(primary, "add", "tracked.txt")
        git_run(primary, "commit", "-m", "test component")
        git_run(
            primary,
            "remote",
            "add",
            "origin",
            f"https://github.com/atrinik/{checkout}.git",
        )
    return {
        name: {
            "path": str(path),
            "device": path.stat().st_dev,
            "inode": path.stat().st_ino,
        }
        for name, path in (
            ("wrapper", wrapper),
            ("workspace", workspace),
            ("primary", primary),
        )
    }


def linked_wrapper_roots(base: Path) -> dict[str, object]:
    upstream = base / "upstream"
    wrapper = base / "linked-wrapper"
    workspace = base / "alternate-workspace-data"
    upstream.mkdir(parents=True)
    workspace.mkdir(parents=True)
    shutil.copy2(ROOT / "components.json", upstream / "components.json")
    shutil.copy2(ROOT / "atrinik", upstream / "atrinik")
    shutil.copy2(ROOT / ".gitignore", upstream / ".gitignore")
    shutil.copytree(ROOT / "atrinik_workspace", upstream / "atrinik_workspace")
    git_run(upstream, "init", "--initial-branch=main")
    git_run(upstream, "config", "user.name", "Delivery Test")
    git_run(upstream, "config", "user.email", "delivery@example.invalid")
    git_run(
        upstream,
        "remote",
        "add",
        "origin",
        "https://github.com/atrinik/atrinik.git",
    )
    git_run(upstream, "add", ".")
    git_run(upstream, "commit", "-m", "test linked wrapper")
    git_run(upstream, "worktree", "add", "-b", "linked-wrapper", str(wrapper))
    return {
        name: {
            "path": str(path),
            "device": path.stat().st_dev,
            "inode": path.stat().st_ino,
        }
        for name, path in (
            ("wrapper", wrapper),
            ("workspace", workspace),
            ("primary", wrapper),
        )
    }


def git_run(path: Path, *arguments: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(path), *arguments],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env={
            **os.environ,
            "LC_ALL": "C",
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": "/dev/null",
            "GIT_CONFIG_SYSTEM": "/dev/null",
        },
    )


def git_head(roots: dict[str, object]) -> str:
    return git_run(Path(roots["primary"]["path"]), "rev-parse", "HEAD").stdout.strip()


def replace_sha(value: object, before: str, after: str) -> object:
    if isinstance(value, dict):
        for key, item in tuple(value.items()):
            value[key] = replace_sha(item, before, after)
    elif isinstance(value, list):
        for index, item in enumerate(value):
            value[index] = replace_sha(item, before, after)
    elif value == before:
        return after
    return value


def live_worktree_path(request: dict[str, object]) -> Path:
    path = (
        Path(request["roots"]["workspace"]["path"])
        / "worktrees"
        / request["physical_checkout"]
        / request["label"]
    )
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        primary = Path(request["roots"]["primary"]["path"])
        branch = request["branch"]
        expected = request.get("expected_head_sha", request.get("start_sha"))
        exists = git_run(
            primary,
            "show-ref",
            "--verify",
            "--quiet",
            f"refs/heads/{branch}",
            check=False,
        )
        if exists.returncode != 0:
            git_run(primary, "branch", branch, expected)
        git_run(primary, "worktree", "add", str(path), branch)
    return path


def worktree_list_bytes(
    request: dict[str, object], *, use_wrapper_command: bool = True
) -> bytes:
    live_worktree_path(request)
    if request["physical_checkout"] == "atrinik" and use_wrapper_command:
        wrapper = Path(request["roots"]["wrapper"]["path"])
        git_executable = shutil.which("git")
        if git_executable is None:  # pragma: no cover - test prerequisite
            raise RuntimeError("git executable is unavailable")
        # Capture post-trust producer behavior from the known-good copied
        # wrapper without inheriting CI secrets or Python/Git selector controls.
        # Trust-before-import adversaries use retained raw evidence below.
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(wrapper / "atrinik"),
                "worktree",
                "list",
                "--wrapper-self",
                "--json",
            ],
            cwd=wrapper,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={
                "ATRINIK_WORKSPACE_DIR": request["roots"]["workspace"]["path"],
                "GIT_CONFIG_NOSYSTEM": "1",
                "GIT_CONFIG_GLOBAL": "/dev/null",
                "GIT_CONFIG_SYSTEM": "/dev/null",
                "LC_ALL": "C",
                "PATH": str(Path(git_executable).absolute().parent),
            },
        )
        return result.stdout
    primary = Path(request["roots"]["primary"]["path"])
    result = subprocess.run(
        ["git", "-C", str(primary), "worktree", "list", "--porcelain", "-z"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env={
            **os.environ,
            "LC_ALL": "C",
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": "/dev/null",
            "GIT_CONFIG_SYSTEM": "/dev/null",
        },
    )
    rows = [
        {"component": request["physical_checkout"], **record}
        for record in _parse_worktree_porcelain(result.stdout)
    ]
    return (json.dumps(rows, sort_keys=True) + "\n").encode()


def safety_observation_bytes(
    request: dict[str, object],
    worktree_list: bytes,
    *,
    producer_kind: str,
    producer_digest: str | None,
    safety: dict[str, bool] | None = None,
    repository_value: dict[str, object] | None = None,
) -> bytes:
    path = live_worktree_path(request)
    value = {
        "schema_version": 1,
        "observed_at": "2026-08-14T18:01:00Z",
        "repository": request.get("repository", repository_value or repository()),
        "component": request["component"],
        "physical_checkout": request["physical_checkout"],
        "roots": request["roots"],
        "path": str(path),
        "path_device": path.stat().st_dev,
        "path_inode": path.stat().st_ino,
        "branch": request["branch"],
        "head_sha": request.get("expected_head_sha", request.get("start_sha")),
        "worktree_list_sha256": ledger.byte_digest(worktree_list),
        "producer": {"kind": producer_kind, "result_sha256": producer_digest},
        "safety": copy.deepcopy(safety or ledger.SAFE_ARTIFACT_STATE),
    }
    return (json.dumps(value, sort_keys=True) + "\n").encode()


def install_scope_references(
    request: dict[str, object], scope_show: bytes
) -> None:
    workspace = Path(request["roots"]["workspace"]["path"])
    path = live_worktree_path(request)
    scope_root = workspace / "scopes" / request["name"]
    scope_root.mkdir(parents=True, exist_ok=True)
    (scope_root / "scope.json").write_bytes(scope_show)
    wrapper = Path(request["roots"]["wrapper"]["path"])
    common = Path(git_run(wrapper, "rev-parse", "--git-common-dir").stdout.strip())
    if not common.is_absolute():
        common = wrapper / common
    registry = common / "atrinik-resource-leases" / "profile-references"
    registry.mkdir(parents=True, mode=0o700, exist_ok=True)
    registry.parent.chmod(0o700)
    registry.chmod(0o700)
    (registry / ("1" * 64 + ".json")).write_text(
        json.dumps(
            {
                "schema_version": 1,
                "kind": "profiles",
                "reference": f"scope-{request['name']}",
                "sources": [str(path)],
            },
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )


def deferred_primitive_pr(
    roots: dict[str, object],
    *,
    number: int = 440,
    node: str = "P_deferred",
    branch: str = "Feature/Deferred",
    label: str = "pr-440",
) -> dict[str, object]:
    live_head = git_head(roots)
    document = pr_ledger(
        number,
        pr_node=node,
        branch=branch,
        worktree=f"/unused/{label}",
    )
    replace_sha(document, SHA_A, live_head)
    branch_slot = next(
        slot for slot in document["artifacts"] if slot["kind"] == "branch"
    )
    branch_slot.update(state="planned", current=None, safety=None)
    worktree = next(
        slot for slot in document["artifacts"] if slot["kind"] == "worktree"
    )
    worktree["immutable"]["path"] = None
    worktree["primitive_request"] = {
        "component": "atrinik",
        "physical_checkout": "atrinik",
        "label": label,
        "repository": repository(),
        "branch": branch,
        "expected_head_sha": live_head,
        "roots": copy.deepcopy(roots),
    }
    return document


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def issue_ledger(
    *,
    number: int = 419,
    issue_node: str = "I_issue",
    branch: str = "docs/issue-419",
    worktree: str = "/workspace/worktrees/issue-419",
    actor: str = "zoeyrose",
    actor_node: str = "U_actor",
) -> dict[str, object]:
    selected_issue = issue(number, issue_node)
    document = {
        "schema_version": 1,
        "ledger_id": f"delivery-v1:issue:{issue_node}",
        "entry_mode": "issue",
        "actor": {
            "login": actor,
            "node_id": actor_node,
            "push_repository_node_ids": ["R_repo"],
        },
        "authority": {
            "kind": "durable-goal",
            "reference": f"goal:issue-{number}",
            "objective_sha256": "d" * 64,
            "issued_at": "2026-08-14T18:00:00Z",
            "actor_node_id": actor_node,
            "allowed": {
                "repositories": ["R_repo"],
                "issues": [issue_node],
                "pull_requests": [],
            },
        },
        "program": None,
        "issues": {"explicit": [selected_issue], "incidental": []},
        "selected_prs": [],
        "targets": [
            {
                "repository": repository(),
                "base": {
                    "branch": "main",
                    "initial_sha": SHA_A,
                    "current_sha": SHA_A,
                    "lineage": [SHA_A],
                },
                "head": {
                    "branch": branch,
                    "initial_sha": SHA_A,
                    "current_sha": SHA_A,
                    "lineage": [SHA_A],
                },
                "merge_base": {"initial_sha": SHA_A, "current_sha": SHA_A},
            }
        ],
        "closing_scope": [selected_issue],
        "artifacts": [
            artifact("branch", "branch", branch=branch),
            artifact(
                "pull-request",
                "pull_request",
                branch=branch,
                body_digest="e" * 64,
            ),
            artifact("worktree", "worktree", branch=branch, path=worktree),
        ],
        "resources": [],
        "generation": 1,
        "previous_byte_digest": None,
        "history": [],
        "migration": None,
    }
    if (
        worktree.startswith("/")
        and worktree != "/"
        and "\x00" not in worktree
        and os.path.normpath(worktree) == worktree
    ):
        label = Path(worktree).name
        worktree_slot = next(
            slot
            for slot in document["artifacts"]
            if slot["kind"] == "worktree"
        )
        worktree_slot["immutable"]["path"] = None
        worktree_slot["primitive_request"] = {
            "component": "atrinik",
            "physical_checkout": "atrinik",
            "label": label,
            "repository": repository(),
            "branch": branch,
            "expected_head_sha": SHA_A,
            "roots": {
                "wrapper": {"path": "/wrapper", "device": 1, "inode": 1},
                "workspace": {"path": "/workspace", "device": 1, "inode": 2},
                "primary": {"path": "/wrapper", "device": 1, "inode": 1},
            },
        }
    return document


def pr_ledger(
    number: int,
    *,
    pr_node: str,
    branch: str,
    worktree: str,
    explicit_issue: dict[str, object] | None = None,
) -> dict[str, object]:
    pull = {
        "repository": repository(),
        "head_repository": repository(),
        "number": number,
        "node_id": pr_node,
        "author_node_id": "U_contributor",
        "base_branch": "main",
        "head_branch": branch,
        "draft": True,
        "draft_intent": None,
        "body": {
            "ownership": "contributor-owned",
            "state": "observed",
            "observed_digest": "e" * 64,
            "intended_digest": None,
            "intended_payload": None,
            "current_digest": "e" * 64,
            "outside_digest": "e" * 64,
            "section_digest": None,
            "updated_at": "2026-08-14T18:00:00Z",
        },
        "comment": {
            "state": "none",
            "marker": None,
            "intended_digest": None,
            "intended_payload": None,
            "node_id": None,
            "current_digest": None,
        },
    }
    explicit = [] if explicit_issue is None else [explicit_issue]
    document = {
        "schema_version": 1,
        "ledger_id": f"delivery-v1:pr:{pr_node}",
        "entry_mode": "pr",
        "actor": {
            "login": "zoeyrose",
            "node_id": "U_actor",
            "push_repository_node_ids": ["R_repo"],
        },
        "authority": {
            "kind": "explicit-invocation",
            "reference": f"turn:pr-{number}",
            "objective_sha256": "f" * 64,
            "issued_at": "2026-08-14T18:00:00Z",
            "actor_node_id": "U_actor",
            "allowed": {
                "repositories": ["R_repo"],
                "issues": [] if explicit_issue is None else [explicit_issue["node_id"]],
                "pull_requests": [pr_node],
            },
        },
        "program": None,
        "issues": {"explicit": explicit, "incidental": []},
        "selected_prs": [pull],
        "targets": [
            {
                "repository": repository(),
                "base": {
                    "branch": "main",
                    "initial_sha": SHA_A,
                    "current_sha": SHA_A,
                    "lineage": [SHA_A],
                },
                "head": {
                    "branch": branch,
                    "initial_sha": SHA_A,
                    "current_sha": SHA_A,
                    "lineage": [SHA_A],
                },
                "merge_base": {"initial_sha": SHA_A, "current_sha": SHA_A},
            }
        ],
        "closing_scope": explicit,
        "artifacts": [
            artifact("branch", "branch", branch=branch, state="adopted"),
            artifact(
                "pull-request",
                "pull_request",
                branch=branch,
                number=number,
                node_id=pr_node,
                body_digest="e" * 64,
                state="adopted",
            ),
            artifact(
                "worktree",
                "worktree",
                branch=branch,
                path=worktree,
            ),
        ],
        "resources": [],
        "generation": 1,
        "previous_byte_digest": None,
        "history": [],
        "migration": None,
    }
    if (
        worktree.startswith("/")
        and worktree != "/"
        and "\x00" not in worktree
        and os.path.normpath(worktree) == worktree
    ):
        worktree_slot = next(
            slot
            for slot in document["artifacts"]
            if slot["kind"] == "worktree"
        )
        worktree_slot["immutable"]["path"] = None
        worktree_slot["primitive_request"] = {
            "component": "atrinik",
            "physical_checkout": "atrinik",
            "label": Path(worktree).name,
            "repository": repository(),
            "branch": branch,
            "expected_head_sha": SHA_A,
            "roots": {
                "wrapper": {"path": "/wrapper", "device": 1, "inode": 1},
                "workspace": {"path": "/workspace", "device": 1, "inode": 2},
                "primary": {"path": "/wrapper", "device": 1, "inode": 1},
            },
        }
    return document


def legacy_report_bytes(document: dict[str, object]) -> bytes:
    lines: list[str] = []
    for mapped in [
        *document["issues"]["explicit"],
        *document["issues"]["incidental"],
    ]:
        repo = mapped["repository"]
        lines.append(
            f"https://github.com/{repo['owner']}/{repo['name']}/issues/{mapped['number']}"
        )
    for pull in document["selected_prs"]:
        repo = pull["repository"]
        lines.append(
            f"https://github.com/{repo['owner']}/{repo['name']}/pull/{pull['number']}"
        )
    worktrees: dict[tuple[str, str], str] = {}
    for slot in document["artifacts"]:
        if slot["kind"] != "worktree":
            continue
        path = slot["immutable"]["path"]
        request = slot["primitive_request"]
        if path is None and request is not None:
            path = str(
                Path(request["roots"]["workspace"]["path"])
                / "worktrees"
                / request["physical_checkout"]
                / request["label"]
            )
        if path is not None:
            worktrees[
                (
                    slot["immutable"]["repository"]["node_id"],
                    slot["immutable"]["branch"],
                )
            ] = path
    for target in document["targets"]:
        repo = target["repository"]
        key = (repo["node_id"], target["head"]["branch"])
        lines.append(
            f"| {repo['owner']}/{repo['name']}@{target['base']['branch']} | base | "
            f"{target['head']['branch']} / {target['head']['current_sha']} | merge | "
            f"{worktrees[key]} | commits |"
        )
    return ("\n".join(lines) + "\n").encode("utf-8")


def legacy_rebind_ledger() -> dict[str, object]:
    """Return the recorded delivery behind atrinik-classic-329.md."""

    document = issue_ledger(
        number=329,
        issue_node="I_atrinik_329",
        branch="fix/legacy-classic-329",
        worktree="/workspace/worktrees/classic/legacy-329",
    )
    selected_issue = copy.deepcopy(document["issues"]["explicit"][0])
    classic = repository("classic", "R_classic")
    retarget_repository(document, classic)
    document["issues"]["explicit"] = [selected_issue]
    document["closing_scope"] = [copy.deepcopy(selected_issue)]
    document["authority"]["kind"] = "explicit-recovery"

    branch = next(
        slot for slot in document["artifacts"] if slot["kind"] == "branch"
    )
    branch["state"] = "adopted"
    branch["current"] = {**copy.deepcopy(branch["immutable"]), "head_sha": SHA_A}
    branch["safety"] = copy.deepcopy(SAFE)
    bind_worktree(document)

    pull = next(
        slot
        for slot in document["artifacts"]
        if slot["kind"] == "pull_request"
    )
    pull["state"] = "adopted"
    pull["immutable"].update(number=92, node_id="P_classic_92")
    pull["current"] = {
        **copy.deepcopy(pull["immutable"]),
        "head_sha": SHA_A,
    }
    pull["initial_body_payload"] = None
    pull["safety"] = copy.deepcopy(SAFE)
    document["selected_prs"] = [
        {
            "repository": copy.deepcopy(classic),
            "head_repository": copy.deepcopy(classic),
            "number": 92,
            "node_id": "P_classic_92",
            "author_node_id": "U_contributor",
            "base_branch": "main",
            "head_branch": "fix/legacy-classic-329",
            "draft": False,
            "draft_intent": None,
            "body": {
                "ownership": "contributor-owned",
                "state": "observed",
                "observed_digest": pull["immutable"]["body_digest"],
                "intended_digest": None,
                "intended_payload": None,
                "current_digest": pull["immutable"]["body_digest"],
                "outside_digest": pull["immutable"]["body_digest"],
                "section_digest": None,
                "updated_at": "2026-08-14T18:00:00Z",
            },
            "comment": {
                "state": "none",
                "marker": None,
                "intended_digest": None,
                "intended_payload": None,
                "node_id": None,
                "current_digest": None,
            },
        }
    ]
    wrapper = repository("repo", "R_repo")
    wrapper["owner"] = "atrinik"
    wrapper["name"] = "atrinik"
    wrapper_target = copy.deepcopy(document["targets"][0])
    wrapper_target["repository"] = copy.deepcopy(wrapper)
    document["targets"] = [wrapper_target, *document["targets"]]

    wrapper_slots = []
    for slot in copy.deepcopy(document["artifacts"]):
        slot["slot_id"] = f"wrapper-{slot['kind']}"
        slot["immutable"]["repository"] = copy.deepcopy(wrapper)
        slot["current"]["repository"] = copy.deepcopy(wrapper)
        if slot["kind"] == "pull_request":
            slot["immutable"].update(
                number=330, node_id="P_atrinik_330", body_digest="e" * 64
            )
            slot["current"].update(
                number=330, node_id="P_atrinik_330", body_digest="e" * 64
            )
        elif slot["kind"] == "worktree":
            path = "/workspace/worktrees/atrinik/legacy-329"
            slot["immutable"]["path"] = path
            slot["current"]["path"] = path
        wrapper_slots.append(slot)
    document["artifacts"] = sorted(
        [*document["artifacts"], *wrapper_slots], key=lambda slot: slot["slot_id"]
    )
    classic_worktree = next(
        slot
        for slot in document["artifacts"]
        if slot["kind"] == "worktree"
        and slot["immutable"]["repository"]["node_id"] == "R_classic"
    )
    classic_path = "/workspace/worktrees/classic/legacy-329"
    classic_worktree["immutable"]["path"] = classic_path
    classic_worktree["current"]["path"] = classic_path

    wrapper_pull = copy.deepcopy(document["selected_prs"][0])
    wrapper_pull.update(
        repository=copy.deepcopy(wrapper),
        head_repository=copy.deepcopy(wrapper),
        number=330,
        node_id="P_atrinik_330",
    )
    wrapper_pull["body"].update(
        observed_digest="e" * 64,
        current_digest="e" * 64,
        outside_digest="e" * 64,
    )
    document["selected_prs"] = [wrapper_pull, *document["selected_prs"]]
    document["actor"]["push_repository_node_ids"] = ["R_classic", "R_repo"]
    document["authority"]["allowed"]["repositories"] = ["R_classic", "R_repo"]
    document["authority"]["allowed"]["pull_requests"] = [
        "P_atrinik_330",
        "P_classic_92",
    ]
    return document


def legacy_bullet_report_bytes(
    document: dict[str, object], repository_name: str, *, reported_sha: str | None = None
) -> bytes:
    target = next(
        target
        for target in document["targets"]
        if target["repository"]["name"] == repository_name
    )
    repository_value = target["repository"]
    worktree = next(
        slot
        for slot in document["artifacts"]
        if slot["kind"] == "worktree"
        and slot["immutable"]["repository"]["node_id"]
        == repository_value["node_id"]
    )
    issue = document["issues"]["explicit"][0]
    pull_urls = [
        f"https://github.com/{pull['repository']['owner']}/"
        f"{pull['repository']['name']}/pull/{pull['number']}"
        for pull in document["selected_prs"]
    ]
    closing = next(
        url for url in pull_urls if "/atrinik/pull/" in url
    )
    lines = [
        "## Coordinates",
        "",
        f"- Issue: https://github.com/{issue['repository']['owner']}/"
        f"{issue['repository']['name']}/issues/{issue['number']}",
        f"- Pull requests: {' and '.join(pull_urls)}",
        f"- Canonical closing PR: {closing}",
        f"- Repository: `{repository_value['owner']}/{repository_value['name']}`",
        f"- Remote head: `{target['head']['branch']}` / "
        f"`{target['head']['current_sha'] if reported_sha is None else reported_sha}`",
        f"- Worktree: `{worktree['immutable']['path']}`",
        "",
        "## Review",
        "",
        "Historical evidence remains preserved.",
    ]
    return ("\n".join(lines) + "\n").encode("utf-8")


def legacy_rebind_historical_heads() -> dict[tuple[str, str, str], str]:
    branch = "fix/legacy-classic-329"
    return {
        ("atrinik", "atrinik", branch): SHA_C,
        ("atrinik", "classic", branch): SHA_B,
    }


def multi_target_issue_ledger() -> dict[str, object]:
    document = issue_ledger()
    content_repository = repository("content", "R_content")
    content_branch = "maps/issue-419"
    document["actor"]["push_repository_node_ids"] = ["R_content", "R_repo"]
    document["authority"]["allowed"]["repositories"] = ["R_content", "R_repo"]
    document["targets"].append(
        {
            "repository": content_repository,
            "base": {
                "branch": "main",
                "initial_sha": SHA_A,
                "current_sha": SHA_A,
                "lineage": [SHA_A],
            },
            "head": {
                "branch": content_branch,
                "initial_sha": SHA_A,
                "current_sha": SHA_A,
                "lineage": [SHA_A],
            },
            "merge_base": {"initial_sha": SHA_A, "current_sha": SHA_A},
        }
    )
    content_slots = [
        artifact("content-branch", "branch", branch=content_branch),
        artifact(
            "content-pr",
            "pull_request",
            branch=content_branch,
            body_digest="f" * 64,
        ),
        artifact(
            "content-worktree",
            "worktree",
            branch=content_branch,
            path="/workspace/worktrees/content-419",
        ),
    ]
    for slot in content_slots:
        slot["immutable"]["repository"] = content_repository
    content_worktree = next(
        slot for slot in content_slots if slot["kind"] == "worktree"
    )
    content_worktree["immutable"]["path"] = None
    content_worktree["primitive_request"] = {
        "component": "content",
        "physical_checkout": "content",
        "label": "content-419",
        "repository": content_repository,
        "branch": content_branch,
        "expected_head_sha": SHA_A,
        "roots": {
            "wrapper": {"path": "/wrapper", "device": 1, "inode": 1},
            "workspace": {"path": "/workspace", "device": 1, "inode": 2},
            "primary": {"path": "/wrapper/content", "device": 1, "inode": 3},
        },
    }
    document["artifacts"] = sorted(
        [*document["artifacts"], *content_slots], key=lambda slot: slot["slot_id"]
    )
    return document


def bind_worktree(document: dict[str, object], index: int = 2) -> None:
    slot = document["artifacts"][index]
    request = slot.get("primitive_request")
    if slot["immutable"]["path"] is None and request is not None:
        slot["immutable"]["path"] = str(
            Path(request["roots"]["workspace"]["path"])
            / "worktrees"
            / request["physical_checkout"]
            / request["label"]
        )
        slot["primitive_request"] = None
        slot["primitive_result"] = None
    slot["state"] = "adopted"
    slot["current"] = {
        **copy.deepcopy(slot["immutable"]),
        "head_sha": SHA_A,
    }
    slot["safety"] = {
        "clean": True,
        "detached": False,
        "locked": False,
        "active": False,
        "unowned_reference": False,
        "foreign": False,
        "certain": True,
    }


def replacement(snapshot: object, *, head: str = SHA_B) -> dict[str, object]:
    assert isinstance(snapshot, ledger.Snapshot)
    result = copy.deepcopy(snapshot.document)
    result["generation"] += 1
    result["previous_byte_digest"] = snapshot.digest
    result["history"].append(snapshot.digest)
    target_head = result["targets"][0]["head"]
    target_head["current_sha"] = head
    target_head["lineage"].append(head)
    return result


def next_generation(snapshot: object) -> dict[str, object]:
    assert isinstance(snapshot, ledger.Snapshot)
    result = copy.deepcopy(snapshot.document)
    result["generation"] += 1
    result["previous_byte_digest"] = snapshot.digest
    result["history"].append(snapshot.digest)
    return result


def cas_arguments(snapshot: object) -> dict[str, object]:
    assert isinstance(snapshot, ledger.Snapshot)
    return {
        "expected_generation": snapshot.document["generation"],
        "expected_digest": snapshot.digest,
        "expected_device": snapshot.device,
        "expected_inode": snapshot.inode,
    }


def head_correction_recovery(
    predecessor: object,
    erroneous: object,
    actual_head: str,
    bad_head: str,
) -> bytes:
    assert isinstance(predecessor, ledger.Snapshot)
    assert isinstance(erroneous, ledger.Snapshot)
    document = erroneous.document
    target = next(
        row for row in document["targets"] if row["head"]["current_sha"] == bad_head
    )
    worktree = next(
        slot
        for slot in document["artifacts"]
        if slot["kind"] == "worktree" and slot["current"]["head_sha"] == bad_head
    )
    repositories = {
        row["repository"]["node_id"]: copy.deepcopy(row["repository"])
        for row in document["targets"]
    }
    issue_rows = [*document["issues"]["explicit"]]
    if document["program"] is not None:
        issue_rows.extend(
            (
                document["program"]["master_issue"],
                document["program"]["leaf_issue"],
            )
        )
    issues = {row["node_id"]: copy.deepcopy(row) for row in issue_rows}
    intent = {
        "transaction": "delivery-ledger-correct-target-head-intent-v1",
        "target": erroneous.name,
        "installed": {
            "generation": document["generation"],
            "sha256": erroneous.digest,
            "device": erroneous.device,
            "inode": erroneous.inode,
        },
        "predecessor_sha256": predecessor.digest,
        "repository": copy.deepcopy(target["repository"]),
        "branch": target["head"]["branch"],
        "worktree": worktree["current"]["path"],
        "bad_head": bad_head,
        "actual_head": actual_head,
        "ledger_scope": {
            "ledger_id": document["ledger_id"],
            "entry_mode": document["entry_mode"],
            "actor": copy.deepcopy(document["actor"]),
            "repositories": sorted(
                repositories.values(),
                key=lambda row: (row["owner"], row["name"], row["node_id"]),
            ),
            "issues": sorted(
                issues.values(),
                key=lambda row: (
                    row["repository"]["owner"],
                    row["repository"]["name"],
                    row["number"],
                    row["node_id"],
                ),
            ),
            "pull_requests": copy.deepcopy(
                document["authority"]["allowed"]["pull_requests"]
            ),
        },
    }
    grant = {
        "kind": "explicit-recovery",
        "reference": "recovery:issue-445-target-head",
        "objective_sha256": ledger.canonical_object_digest(intent),
        "issued_at": "2026-08-15T12:00:00Z",
        "actor_node_id": document["actor"]["node_id"],
        "allowed": copy.deepcopy(document["authority"]["allowed"]),
    }
    return ledger.canonical_bytes({"grant": grant, "intent": intent})


def bind_issue_created_pr(
    snapshot: object, *, number: int = 500, node: str = "P_issue_created"
) -> dict[str, object]:
    """Return the one legal fresh issue-mode planned-PR bind transition."""

    assert isinstance(snapshot, ledger.Snapshot)
    update = next_generation(snapshot)
    slot = next(
        value for value in update["artifacts"] if value["kind"] == "pull_request"
    )
    target = next(
        value
        for value in update["targets"]
        if value["repository"] == slot["immutable"]["repository"]
        and value["head"]["branch"] == slot["immutable"]["branch"]
    )
    digest = slot["immutable"]["body_digest"]
    update["selected_prs"] = [
        {
            "repository": copy.deepcopy(target["repository"]),
            "head_repository": copy.deepcopy(target["repository"]),
            "number": number,
            "node_id": node,
            "author_node_id": update["actor"]["node_id"],
            "base_branch": target["base"]["branch"],
            "head_branch": target["head"]["branch"],
            "draft": True,
            "draft_intent": None,
            "body": {
                "ownership": "delivery-created",
                "state": "written",
                "observed_digest": None,
                "intended_digest": None,
                "intended_payload": None,
                "current_digest": digest,
                "outside_digest": digest,
                "section_digest": None,
                "updated_at": "2026-08-14T18:00:00Z",
            },
            "comment": {
                "state": "none",
                "marker": None,
                "intended_digest": None,
                "intended_payload": None,
                "node_id": None,
                "current_digest": None,
            },
        }
    ]
    slot["state"] = "created"
    slot["current"] = {
        **copy.deepcopy(slot["immutable"]),
        "number": number,
        "node_id": node,
        "head_sha": target["head"]["current_sha"],
    }
    slot["safety"] = {
        "clean": True,
        "detached": False,
        "locked": False,
        "active": False,
        "unowned_reference": False,
        "foreign": False,
        "certain": True,
    }
    return update


def directory_snapshot(root: Path) -> tuple[tuple[object, ...], ...]:
    result: list[tuple[object, ...]] = []
    for path in sorted(root.iterdir(), key=lambda value: value.name):
        status = path.lstat()
        digest = None
        if path.is_file() and not path.is_symlink():
            digest = ledger.byte_digest(path.read_bytes())
        result.append((path.name, status.st_mode, status.st_size, digest))
    return tuple(result)


class DeliveryLedgerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.live_temporary = tempfile.TemporaryDirectory()
        self.live_base = Path(self.live_temporary.name)

    def tearDown(self) -> None:
        self.live_temporary.cleanup()

    @classmethod
    def tearDownClass(cls) -> None:
        if (SCRIPT.parent / "__pycache__").exists():
            raise AssertionError("dynamic helper import created skill-package debris")

    def test_01_strict_schema_and_duplicate_keys_fail_closed(self) -> None:
        valid = issue_ledger()
        self.assertEqual(ledger.prepare(valid), valid)
        mutations = (
            (lambda value: value.update(extra=True), "unexpected extra"),
            (lambda value: value.update(schema_version=True), "unsupported"),
            (lambda value: value.update(entry_mode="PR"), "issue or pr"),
            (
                lambda value: value["actor"].update(login="ZoeyRose"),
                "actor.login",
            ),
            (
                lambda value: value["targets"][0]["head"].update(lineage=[SHA_B]),
                "connect initial_sha",
            ),
            (
                lambda value: value["artifacts"][0].update(current={}),
                "current",
            ),
        )
        for mutate, message in mutations:
            with self.subTest(message=message):
                candidate = copy.deepcopy(valid)
                mutate(candidate)
                with self.assertRaisesRegex(ledger.LedgerError, message):
                    ledger.prepare(candidate)
        with self.assertRaisesRegex(ledger.LedgerError, "duplicate JSON key"):
            ledger._decode(b'{"schema_version":1,"schema_version":1}', "duplicate")

    def test_02_prepare_create_inspect_inventory_and_idempotence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            before_inventory = directory_snapshot(root)
            self.assertEqual(ledger.inventory(root).ledgers, ())
            self.assertEqual(directory_snapshot(root), before_inventory)
            document = issue_ledger()
            created = ledger.create(root, document)
            self.assertEqual(
                created.name, "atrinik-atrinik-issue-419.md.ledger.json"
            )
            self.assertEqual(created.raw, ledger.canonical_bytes(document))
            self.assertEqual(ledger.create(root, document).digest, created.digest)
            inspected = ledger.inspect(root, created.name)
            self.assertEqual((inspected.device, inspected.inode), (created.device, created.inode))
            inventory = ledger.inventory(root)
            self.assertEqual([item.name for item in inventory.ledgers], [created.name])
            self.assertEqual(inventory.pending, ())
            self.assertNotIn(".md", json.dumps(inventory.ledgers[0].document))

    def test_03_create_kill_points_are_resumable(self) -> None:
        for failpoint in (
            "create:staged",
            "create:linked",
            "create:installed",
            "create:cleaned",
        ):
            with self.subTest(failpoint=failpoint), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                document = issue_ledger()
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.create(root, document, failpoint=failpoint)
                resumed = ledger.create(root, document)
                self.assertEqual(resumed.document, document)
                self.assertEqual(ledger.inventory(root).pending, ())

    def test_04_cas_kill_points_stale_writers_and_inode_rechecks(self) -> None:
        for failpoint in (
            "cas:staged",
            "cas:proofed",
            "cas:renamed",
            "cas:installed",
        ):
            with self.subTest(failpoint=failpoint), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                initial = ledger.create(root, issue_ledger())
                update = next_generation(initial)
                arguments = {
                    "expected_generation": initial.document["generation"],
                    "expected_digest": initial.digest,
                    "expected_device": initial.device,
                    "expected_inode": initial.inode,
                }
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.cas(root, initial.name, update, failpoint=failpoint, **arguments)
                if failpoint == "cas:renamed":
                    with self.assertRaisesRegex(ledger.LedgerError, "proof|stale|tuple"):
                        ledger.cas(
                            root,
                            initial.name,
                            update,
                            **{
                                **arguments,
                                "expected_device": initial.device + 1,
                                "expected_inode": initial.inode + 1,
                            },
                        )
                current = ledger.cas(root, initial.name, update, **arguments)
                self.assertEqual(current.document["generation"], 2)
                self.assertEqual(current.document["targets"][0]["head"]["current_sha"], SHA_A)
                with self.assertRaisesRegex(ledger.LedgerError, "stale CAS"):
                    other = next_generation(current)
                    ledger.cas(
                        root,
                        current.name,
                        other,
                        expected_generation=1,
                        expected_digest="0" * 64,
                        expected_device=initial.device,
                        expected_inode=initial.inode,
                    )

    def test_05_same_and_different_coordinate_concurrent_cli_writers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            first = issue_ledger()
            same = copy.deepcopy(first)
            different_bytes = copy.deepcopy(first)
            different_bytes["authority"]["objective_sha256"] = "9" * 64
            second = issue_ledger(
                number=420,
                issue_node="I_other",
                branch="docs/issue-420",
                worktree="/workspace/worktrees/issue-420",
            )

            def invoke(
                review_root: Path, document: dict[str, object], index: int
            ) -> subprocess.Popen[str]:
                path = base / f"input-{index}.json"
                path.write_text(json.dumps(document), encoding="utf-8")
                return subprocess.Popen(
                    [sys.executable, "-B", str(SCRIPT), "create", str(review_root), str(path)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )

            identical_root = base / "identical"
            identical_root.mkdir()
            identical = [
                invoke(identical_root, first, 1),
                invoke(identical_root, same, 2),
            ]
            identical_results = [process.communicate(timeout=20) for process in identical]
            self.assertEqual([process.returncode for process in identical], [0, 0], identical_results)

            same_root = base / "same-coordinate"
            same_root.mkdir()
            takeover = [
                invoke(same_root, first, 3),
                invoke(same_root, different_bytes, 4),
            ]
            takeover_results = [process.communicate(timeout=20) for process in takeover]
            self.assertEqual(sorted(process.returncode for process in takeover), [0, 2])
            self.assertTrue(
                any("different bytes" in stderr for _, stderr in takeover_results),
                takeover_results,
            )

            # A distinct issue/head/worktree can proceed while retaining the
            # root-wide serialization contract.
            different_root = base / "different-coordinates"
            different_root.mkdir()
            third = issue_ledger(
                number=421,
                issue_node="I_third",
                branch="docs/issue-421",
                worktree="/workspace/worktrees/issue-421",
            )
            independent = [
                invoke(different_root, second, 5),
                invoke(different_root, third, 6),
            ]
            independent_results = [process.communicate(timeout=20) for process in independent]
            self.assertEqual(
                [process.returncode for process in independent],
                [0, 0],
                independent_results,
            )
            self.assertEqual(len(ledger.inventory(different_root).ledgers), 2)

    def test_06_sibling_pr_and_cross_mode_ownership_overlaps_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = pr_ledger(
                423,
                pr_node="P_first",
                branch="docs/shared-head",
                worktree="/workspace/worktrees/pr-423",
            )
            ledger.create(root, first)
            sibling = pr_ledger(
                424,
                pr_node="P_second",
                branch="docs/shared-head",
                worktree="/workspace/worktrees/pr-424",
            )
            with self.assertRaisesRegex(ledger.LedgerError, "repository/head ownership overlap"):
                ledger.create(root, sibling)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            selected = issue()
            issue_mode = issue_ledger()
            ledger.create(root, issue_mode)
            pr_mode = pr_ledger(
                423,
                pr_node="P_first",
                branch="docs/pr-423",
                worktree="/workspace/worktrees/pr-423",
                explicit_issue=selected,
            )
            with self.assertRaisesRegex(ledger.LedgerError, "issue ownership overlap"):
                ledger.create(root, pr_mode)

    def test_07_symlink_nonregular_and_case_alias_entries_are_rejected(self) -> None:
        document = issue_ledger()
        name = ledger.canonical_name(document)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            outside = root / "outside"
            outside.write_text("not a ledger", encoding="utf-8")
            (root / name).symlink_to(outside)
            with self.assertRaisesRegex(ledger.LedgerError, "not a regular file"):
                ledger.inventory(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            os.mkfifo(root / name)
            with self.assertRaisesRegex(ledger.LedgerError, "not a regular file"):
                ledger.inventory(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / name.upper()).write_bytes(ledger.canonical_bytes(document))
            with self.assertRaisesRegex(ledger.LedgerError, "case alias"):
                ledger.inventory(root)
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            real = parent / "real"
            real.mkdir()
            alias = parent / "alias"
            alias.symlink_to(real, target_is_directory=True)
            with self.assertRaisesRegex(ledger.LedgerError, "cannot open review root"):
                ledger.inventory(alias)

    def test_08_legacy_and_pre_schema_migrations_resume_and_detect_loss(self) -> None:
        failpoints = (
            "migration:plan-staged",
            "migration:plan-linked",
            "migration:planned",
            "migration:snapshot",
            "migration:report",
            "migration:prepared-staged",
            "migration:prepared-renamed",
            "migration:staged",
            "migration:linked",
            "migration:installed",
            "migration:complete-staged",
            "migration:completed-renamed",
            "migration:complete",
        )
        for kind in ("legacy", "pre-schema"):
            for failpoint in failpoints:
                with (
                    self.subTest(kind=kind, failpoint=failpoint),
                    tempfile.TemporaryDirectory() as temporary,
                ):
                    root = Path(temporary)
                    source = (
                        "atrinik-atrinik-419.md"
                        if kind == "legacy"
                        else "atrinik-atrinik-issue-419.md"
                    )
                    document = issue_ledger()
                    source_bytes = (
                        legacy_report_bytes(document)
                        if kind == "legacy"
                        else b"immutable source bytes\n"
                    )
                    (root / source).write_bytes(source_bytes)
                    source_digest = ledger.byte_digest(source_bytes)
                    with self.assertRaises(ledger.InjectedCrash):
                        ledger.migrate(
                            root,
                            source,
                            document,
                            kind=kind,
                            expected_source_digest=source_digest,
                            failpoint=failpoint,
                        )
                    resumed = ledger.migrate(
                        root,
                        source,
                        document,
                        kind=kind,
                        expected_source_digest=source_digest,
                    )
                    self.assertEqual(resumed.document["migration"]["kind"], kind)
                    self.assertEqual(ledger.inventory(root).pending, ())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "atrinik-atrinik-419.md"
            source_bytes = legacy_report_bytes(issue_ledger())
            source.write_bytes(source_bytes)
            source_digest = ledger.byte_digest(source_bytes)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.migrate(
                    root,
                    source.name,
                    issue_ledger(),
                    kind="legacy",
                    expected_source_digest=source_digest,
                    failpoint="migration:planned",
                )
            source.unlink()
            with self.assertRaisesRegex(ledger.LedgerError, "regular file"):
                ledger.migrate(
                    root,
                    source.name,
                    issue_ledger(),
                    kind="legacy",
                    expected_source_digest=source_digest,
                )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "atrinik-atrinik-419.md"
            source_bytes = legacy_report_bytes(issue_ledger())
            source.write_bytes(source_bytes)
            migrated = ledger.migrate(
                root,
                source.name,
                issue_ledger(),
                kind="legacy",
                expected_source_digest=ledger.byte_digest(source_bytes),
            )
            (root / migrated.name).unlink()
            with self.assertRaisesRegex(ledger.LedgerError, "lost canonical ledger"):
                ledger.inventory(root)

    def test_09_descendant_heads_pass_rewritten_heads_and_cli_errors_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            descendant = replacement(initial)
            with self.assertRaisesRegex(
                ledger.LedgerError, "bound authoritative worktree"
            ):
                ledger.cas(root, initial.name, descendant, **cas_arguments(initial))
            rewritten = copy.deepcopy(descendant)
            rewritten["targets"][0]["head"].update(
                current_sha=SHA_C, lineage=[SHA_C]
            )
            with self.assertRaisesRegex(ledger.LedgerError, "must connect"):
                ledger.cas(
                    root,
                    initial.name,
                    rewritten,
                    **cas_arguments(initial),
                )
            process = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "inventory", str(root)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(process.stdout)["schema_version"], 1)

    def test_10_authority_program_identity_and_resource_contracts(self) -> None:
        invalid = issue_ledger()
        invalid["actor"]["push_repository_node_ids"] = []
        with self.assertRaisesRegex(ledger.LedgerError, "push_repository"):
            ledger.prepare(invalid)
        invalid = issue_ledger()
        invalid["authority"]["kind"] = "retrospective-guess"
        with self.assertRaisesRegex(ledger.LedgerError, "authority.kind"):
            ledger.prepare(invalid)
        invalid = issue_ledger()
        invalid["authority"]["actor_node_id"] = "U_other"
        with self.assertRaisesRegex(ledger.LedgerError, "does not match"):
            ledger.prepare(invalid)

        program = issue_ledger()
        program["program"] = {
            "master_issue": issue(400, "I_master"),
            "leaf_issue": issue(),
            "leaf_position": 0,
        }
        program["authority"]["allowed"]["issues"] = ["I_issue", "I_master"]
        self.assertEqual(ledger.prepare(program)["program"]["leaf_position"], 0)
        invalid_program = pr_ledger(
            423,
            pr_node="P_program",
            branch="Feature/Program",
            worktree="/workspace/worktrees/program",
        )
        invalid_program["program"] = program["program"]
        invalid_program["authority"]["allowed"]["issues"] = ["I_issue", "I_master"]
        with self.assertRaisesRegex(ledger.LedgerError, "program leaf"):
            ledger.prepare(invalid_program)

        resource = {
            "slot_id": "profile",
            "kind": "profile",
            "state": "adopted",
            "immutable": {
                "repository": repository(),
                "name": "issue-419-profile",
                "path": "/workspace/profiles/issue-419",
            },
            "current": {
                "repository": repository(),
                "name": "issue-419-profile",
                "path": "/workspace/profiles/issue-419",
                "generation": 1,
                "external_generation": None,
                "identity_digest": "1" * 64,
                "history": [],
                "lifecycle": "static",
            },
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = issue_ledger()
            planned_resource = copy.deepcopy(resource)
            planned_resource.update(state="planned", current=None)
            document["resources"] = [planned_resource]
            initial = ledger.create(root, document)
            bound = next_generation(initial)
            bound["resources"] = [resource]
            current = ledger.cas(root, initial.name, bound, **cas_arguments(initial))
            ledger.require_reusable_resources(current.document)
            refreshed = next_generation(current)
            refreshed_resource = refreshed["resources"][0]["current"]
            refreshed_resource.update(
                generation=2,
                identity_digest="2" * 64,
                history=["1" * 64],
            )
            refreshed_snapshot = ledger.cas(
                root, current.name, refreshed, **cas_arguments(current)
            )
            self.assertEqual(
                refreshed_snapshot.document["resources"][0]["current"]["generation"], 2
            )

        active = issue_ledger()
        topology = copy.deepcopy(resource)
        topology.update(slot_id="topology", kind="topology")
        topology["immutable"].update(name="issue-419-topology")
        topology["current"].update(
            name="issue-419-topology", lifecycle="running"
        )
        active["resources"] = [topology]
        ledger.prepare(active)
        with self.assertRaisesRegex(ledger.LedgerError, "unsafe"):
            ledger.require_reusable_resources(active)
        active["resources"][0]["current"]["path"] = "/"
        with self.assertRaisesRegex(ledger.LedgerError, "canonical path"):
            ledger.prepare(active)

        alias = issue_ledger()
        alias["issues"]["incidental"] = [issue(419, "I_alias")]
        with self.assertRaisesRegex(ledger.LedgerError, "conflicting node IDs"):
            ledger.prepare(alias)

    def test_11_pr_body_and_ready_intents_are_crash_recoverable(self) -> None:
        body_raw = b"delivery-created current body"
        body_digest = ledger.byte_digest(body_raw)

        def delivery_pr() -> dict[str, object]:
            document = pr_ledger(
                423,
                pr_node="P_body",
                branch="Feature/Body",
                worktree="/workspace/worktrees/body",
            )
            pull = document["selected_prs"][0]
            pull["author_node_id"] = "U_actor"
            pull["body"] = {
                "ownership": "delivery-created",
                "state": "written",
                "observed_digest": None,
                "intended_digest": None,
                "intended_payload": None,
                "current_digest": body_digest,
                "outside_digest": body_digest,
                "section_digest": None,
                "updated_at": "2026-08-14T18:00:00Z",
            }
            slot = next(
                value for value in document["artifacts"] if value["kind"] == "pull_request"
            )
            slot["immutable"]["body_digest"] = body_digest
            slot["current"]["body_digest"] = body_digest
            document["authority"]["kind"] = "explicit-recovery"
            return document

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-pr-423.md"
            report.write_bytes(b"pre-schema owned body\n")
            initial = ledger.migrate(
                root,
                report.name,
                delivery_pr(),
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(report.read_bytes()),
            )
            plan = ledger.describe_body_plan(
                initial.document, "P_body", body_raw, b"next delivery section"
            )
            planned = next_generation(initial)
            planned["selected_prs"][0]["body"] = plan["body"]
            intent = ledger.cas(root, initial.name, planned, **cas_arguments(initial))
            self.assertEqual(
                ledger.body_recovery_action(
                    intent.document,
                    "P_body",
                    body_digest,
                    "2026-08-14T18:00:00Z",
                ),
                "apply-intended",
            )
            self.assertEqual(
                ledger.body_recovery_action(
                    intent.document,
                    "P_body",
                    plan["body_digest"],
                    "2026-08-14T18:01:00Z",
                ),
                "bind-intended",
            )
            with self.assertRaisesRegex(ledger.LedgerError, "neither"):
                ledger.body_recovery_action(
                    intent.document,
                    "P_body",
                    "0" * 64,
                    "2026-08-14T18:00:00Z",
                )
            refreshed = ledger.classify_body_recovery(
                intent.document,
                "P_body",
                body_digest,
                "2026-08-14T18:00:01Z",
            )
            self.assertEqual(refreshed["action"], "refresh-intent-observation")
            applied = next_generation(intent)
            applied["selected_prs"][0]["body"] = ledger.classify_body_recovery(
                intent.document,
                "P_body",
                plan["body_digest"],
                "2026-08-14T18:00:00Z",
            )["cas_body"]
            applied["artifacts"][1]["current"]["body_digest"] = plan["body_digest"]
            written = ledger.cas(root, intent.name, applied, **cas_arguments(intent))
            illegal = next_generation(written)
            illegal["selected_prs"][0]["body"].update(
                current_digest="0" * 64,
                updated_at="2026-08-14T18:02:00Z",
            )
            illegal["artifacts"][1]["current"]["body_digest"] = "0" * 64
            with self.assertRaisesRegex(ledger.LedgerError, "body transition"):
                ledger.cas(root, written.name, illegal, **cas_arguments(written))

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, pr_ledger(
                424,
                pr_node="P_ready",
                branch="Feature/Ready",
                worktree="/workspace/worktrees/ready",
            ))
            ready_intent = next_generation(initial)
            ready_intent["selected_prs"][0]["draft_intent"] = "ready"
            intent = ledger.cas(root, initial.name, ready_intent, **cas_arguments(initial))
            ready = next_generation(intent)
            ready["selected_prs"][0].update(draft=False, draft_intent=None)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.cas(
                    root,
                    intent.name,
                    ready,
                    failpoint="cas:renamed",
                    **cas_arguments(intent),
                )
            completed = ledger.cas(root, intent.name, ready, **cas_arguments(intent))
            self.assertFalse(completed.document["selected_prs"][0]["draft"])
            demoted = next_generation(completed)
            demoted["selected_prs"][0]["draft"] = True
            with self.assertRaisesRegex(ledger.LedgerError, "demoted"):
                ledger.cas(root, completed.name, demoted, **cas_arguments(completed))

    def test_12_current_coordinates_uppercase_branches_and_artifact_heads(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            advanced = next_generation(initial)
            advanced["targets"][0]["base"].update(
                current_sha=SHA_B, lineage=[SHA_A, SHA_B]
            )
            advanced["targets"][0]["head"].update(
                current_sha=SHA_C, lineage=[SHA_A, SHA_C]
            )
            advanced["targets"][0]["merge_base"]["current_sha"] = SHA_B
            with self.assertRaisesRegex(
                ledger.LedgerError, "bound authoritative worktree"
            ):
                ledger.cas(root, initial.name, advanced, **cas_arguments(initial))
            rewritten = next_generation(initial)
            rewritten["targets"][0]["base"].update(
                current_sha="d" * 40, lineage=["d" * 40]
            )
            with self.assertRaisesRegex(ledger.LedgerError, "must connect"):
                ledger.cas(root, initial.name, rewritten, **cas_arguments(initial))

        invalid_head = pr_ledger(
            423,
            pr_node="P_stale",
            branch="Feature/Stale",
            worktree="/workspace/worktrees/stale",
        )
        invalid_head["artifacts"][0]["current"]["head_sha"] = SHA_B
        with self.assertRaisesRegex(ledger.LedgerError, "does not equal target"):
            ledger.prepare(invalid_head)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ledger.create(root, pr_ledger(
                423,
                pr_node="P_upper",
                branch="Feature/Shared",
                worktree="/workspace/worktrees/upper",
            ))
            with self.assertRaisesRegex(ledger.LedgerError, "repository/head ownership overlap"):
                ledger.create(root, pr_ledger(
                    424,
                    pr_node="P_lower",
                    branch="feature/shared",
                    worktree="/workspace/worktrees/lower",
                ))

    def test_13_inventory_bounds_pending_debris_and_legacy_reports(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for index in range(3):
                (root / f"junk-{index}").write_text("x", encoding="utf-8")
            before = directory_snapshot(root)
            with mock.patch.object(ledger, "MAX_INVENTORY_ENTRIES", 2):
                with self.assertRaisesRegex(ledger.LedgerError, "directory entries"):
                    ledger.inventory(root)
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery = legacy_rebind_ledger()
            source = root / "atrinik-classic-329.md"
            source_bytes = legacy_bullet_report_bytes(recovery, "classic")
            duplicate_issue = (
                b"https://github.com/atrinik/atrinik/issues/329\n"
            )
            source.write_bytes(source_bytes + duplicate_issue)
            related = root / "atrinik-atrinik-329.md"
            related.write_bytes(legacy_bullet_report_bytes(recovery, "atrinik"))
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "duplicate or ambiguous"
            ):
                ledger.migrate(
                    root,
                    source.name,
                    recovery,
                    kind="legacy-rebind",
                    expected_source_digest=ledger.byte_digest(source.read_bytes()),
                    related_sources={
                        related.name: ledger.byte_digest(related.read_bytes())
                    },
                    expected_historical_heads=legacy_rebind_historical_heads(),
                )
            self.assertEqual(directory_snapshot(root), before)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "atrinik-atrinik-issue-1.md").write_text("123456", encoding="utf-8")
            (root / "atrinik-atrinik-pr-2.md").write_text("123456", encoding="utf-8")
            with mock.patch.object(ledger, "MAX_INVENTORY_BYTES", 10):
                with self.assertRaisesRegex(ledger.LedgerError, "inventory exceeds"):
                    ledger.inventory(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = issue_ledger()
            with self.assertRaises(ledger.InjectedCrash):
                ledger.create(root, first, failpoint="create:staged")
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "pending"):
                ledger.create(root, issue_ledger(
                    number=420,
                    issue_node="I_pending",
                    branch="docs/pending",
                    worktree="/workspace/worktrees/pending",
                ))
            self.assertEqual(directory_snapshot(root), before)
            ledger.create(root, first)
            (root / ".bad.md.ledger.json.evil").write_text("x", encoding="utf-8")
            with self.assertRaisesRegex(ledger.LedgerError, "unexpected delivery helper"):
                ledger.inventory(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-issue-419.md"
            report.write_text("pre-schema report\n", encoding="utf-8")
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "legacy report ownership"):
                ledger.create(root, issue_ledger())
            self.assertEqual(directory_snapshot(root), before)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            legacy_report = root / "atrinik-atrinik-419.md"
            legacy_report.write_text(
                "Selected pull request: https://github.com/atrinik/atrinik/pull/423\n"
                "| atrinik/atrinik@main | main / " + SHA_A + " | Feature/Legacy / "
                + SHA_A + " | " + SHA_A + " | /workspace/worktrees/legacy | commits |\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ledger.LedgerError, "legacy report ownership"):
                ledger.create(root, pr_ledger(
                    423,
                    pr_node="P_legacy",
                    branch="Feature/Legacy",
                    worktree="/workspace/worktrees/legacy",
                ))

    def test_14_migration_authority_loss_mutability_and_digest_chain(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-issue-419.md"
            original = b"pre-schema\n"
            report.write_bytes(original)
            document = issue_ledger()
            digest = ledger.byte_digest(original)
            migrated = ledger.migrate(
                root,
                report.name,
                document,
                kind="pre-schema",
                expected_source_digest=digest,
            )
            report.write_text("later human evidence\n", encoding="utf-8")
            self.assertEqual(len(ledger.inventory(root).ledgers), 1)
            self.assertEqual(
                ledger.migrate(
                    root,
                    report.name,
                    document,
                    kind="pre-schema",
                    expected_source_digest=digest,
                ).name,
                migrated.name,
            )
            second = ledger.cas(
                root, migrated.name, next_generation(migrated), **cas_arguments(migrated)
            )
            self.assertEqual(len(ledger.inventory(root).ledgers), 1)
            third_document = next_generation(second)
            third = ledger.cas(root, second.name, third_document, **cas_arguments(second))
            self.assertEqual(third.document["history"][0], migrated.digest)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "atrinik-atrinik-419.md"
            source_bytes = legacy_report_bytes(issue_ledger())
            source.write_bytes(source_bytes)
            digest = ledger.byte_digest(source_bytes)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.migrate(
                    root,
                    source.name,
                    issue_ledger(),
                    kind="legacy",
                    expected_source_digest=digest,
                    failpoint="migration:planned",
                )
            different = issue_ledger()
            different["authority"]["objective_sha256"] = "9" * 64
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "different transition"):
                ledger.migrate(
                    root,
                    source.name,
                    different,
                    kind="legacy",
                    expected_source_digest=digest,
                )
            self.assertEqual(directory_snapshot(root), before)

        invalid_authority = issue_ledger()
        invalid_authority["authority"]["kind"] = "explicit-invocation"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "atrinik-atrinik-419.md"
            source.write_bytes(b"legacy\n")
            with self.assertRaisesRegex(ledger.LedgerError, "recovery authority"):
                ledger.migrate(
                    root,
                    source.name,
                    invalid_authority,
                    kind="legacy",
                    expected_source_digest=ledger.byte_digest(b"legacy\n"),
                )

        for lost in ("source", "report", "snapshot", "ledger", "marker"):
            with self.subTest(lost=lost), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source = root / "atrinik-atrinik-419.md"
                source_bytes = legacy_report_bytes(issue_ledger())
                source.write_bytes(source_bytes)
                migrated = ledger.migrate(
                    root,
                    source.name,
                    issue_ledger(),
                    kind="legacy",
                    expected_source_digest=ledger.byte_digest(source_bytes),
                )
                migration = migrated.document["migration"]
                paths = {
                    "source": root / migration["source"]["name"],
                    "report": root / migration["canonical_report"],
                    "snapshot": root / migration["snapshot"]["name"],
                    "ledger": root / migrated.name,
                    "marker": root / migration["marker_name"],
                }
                paths[lost].unlink()
                with self.assertRaises(ledger.LedgerError):
                    ledger.inventory(root)

    def test_15_issue_419_forward_contexts_are_type_explicit_and_write_free_when_blocked(self) -> None:
        # 1. An explicitly selected issue-mode input with no PR starts before
        # any external branch/worktree/PR mutation and reserves planned identities.
        with self.subTest(context="selected issue-mode input"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = issue_ledger()
            original = copy.deepcopy(candidate)
            created = ledger.create(root, candidate)
            self.assertEqual(candidate, original)
            self.assertEqual(created.document["entry_mode"], "issue")
            self.assertEqual(created.document["selected_prs"], [])
            self.assertTrue(
                all(slot["state"] == "planned" for slot in created.document["artifacts"])
            )
            self.assertEqual(
                [snapshot.name for snapshot in ledger.inventory(root).ledgers],
                ["atrinik-atrinik-issue-419.md.ledger.json"],
            )

        # 2. An issue already owned by an active PR-mode ledger routes to that
        # exact PR. A competing issue-mode claim is a zero-write stop.
        with self.subTest(context="issue with active PR"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            active = ledger.create(
                root,
                pr_ledger(
                    423,
                    pr_node="P_active",
                    branch="Feature/Active",
                    worktree="/workspace/worktrees/active",
                    explicit_issue=issue(),
                ),
            )
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "issue ownership overlap"):
                ledger.create(root, issue_ledger())
            self.assertEqual(directory_snapshot(root), before)
            self.assertEqual(
                ledger.inspect(root, active.name).document["selected_prs"][0]["node_id"],
                "P_active",
            )

        # 3. A verified explicit issue may be owned by PR mode without granting
        # closing authority; contributor body ownership remains read-only.
        with self.subTest(context="draft PR with non-closing explicit issue"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = pr_ledger(
                423,
                pr_node="P_linked",
                branch="Feature/Linked",
                worktree="/workspace/worktrees/linked",
                explicit_issue=issue(),
            )
            candidate["closing_scope"] = []
            created = ledger.create(root, candidate)
            selected = created.document["selected_prs"][0]
            self.assertTrue(selected["draft"])
            self.assertEqual(selected["body"]["ownership"], "contributor-owned")
            self.assertEqual(created.document["issues"]["explicit"], [issue()])
            self.assertEqual(
                created.document["authority"]["allowed"]["issues"], ["I_issue"]
            )
            self.assertEqual(created.document["closing_scope"], [])

        # 4. A pre-existing verified closing reference remains explicit closing
        # authority rather than being silently dropped during PR-mode adoption.
        with self.subTest(context="draft PR preserves existing closing issue"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            created = ledger.create(
                root,
                pr_ledger(
                    423,
                    pr_node="P_linked_closing",
                    branch="Feature/LinkedClosing",
                    worktree="/workspace/worktrees/linked-closing",
                    explicit_issue=issue(),
                ),
            )
            self.assertEqual(created.document["closing_scope"], [issue()])
            self.assertEqual(
                next(
                    slot
                    for slot in created.document["artifacts"]
                    if slot["kind"] == "worktree"
                )["state"],
                "planned",
            )

        # 5. An already-ready PR with no issue remains ready and acquires no
        # closing authority or ready-transition intent.
        with self.subTest(context="ready PR without issue"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = pr_ledger(
                424,
                pr_node="P_already_ready",
                branch="Feature/AlreadyReady",
                worktree="/workspace/worktrees/already-ready",
            )
            candidate["selected_prs"][0]["draft"] = False
            created = ledger.create(root, candidate)
            selected = created.document["selected_prs"][0]
            self.assertFalse(selected["draft"])
            self.assertIsNone(selected["draft_intent"])
            self.assertEqual(created.document["issues"]["explicit"], [])
            self.assertEqual(created.document["closing_scope"], [])

        # PR-adoption detail: an absent local branch is created non-forcing at
        # the exact fetched PR head before existing-branch worktree attachment.
        with self.subTest(
            context="absent PR branch starts at verified head"
        ), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            remote = root / "remote.git"
            seed = root / "seed"
            primary = root / "primary"
            worktree = root / "worktree"
            remote.mkdir()
            seed.mkdir()
            git_run(remote, "init", "--bare")
            git_run(seed, "init", "--initial-branch=main")
            git_run(seed, "config", "user.name", "Delivery Test")
            git_run(seed, "config", "user.email", "delivery@example.invalid")
            (seed / "tracked.txt").write_text("base\n", encoding="utf-8")
            git_run(seed, "add", "tracked.txt")
            git_run(seed, "commit", "-m", "base")
            base = git_run(seed, "rev-parse", "HEAD").stdout.strip()
            git_run(seed, "switch", "-c", "Feature/Exact")
            (seed / "tracked.txt").write_text("head\n", encoding="utf-8")
            git_run(seed, "commit", "-am", "head")
            verified_head = git_run(seed, "rev-parse", "HEAD").stdout.strip()
            git_run(seed, "remote", "add", "origin", str(remote))
            git_run(seed, "push", "origin", "main", "Feature/Exact")
            git_run(root, "clone", str(remote), str(primary))
            self.assertNotEqual(base, verified_head)
            self.assertEqual(
                git_run(
                    primary,
                    "show-ref",
                    "--verify",
                    "--quiet",
                    "refs/heads/Feature/Exact",
                    check=False,
                ).returncode,
                1,
            )

            git_run(
                primary,
                "fetch",
                "--no-tags",
                "origin",
                "refs/heads/Feature/Exact",
            )
            fetched = git_run(
                primary,
                "rev-parse",
                "--verify",
                "--end-of-options",
                "FETCH_HEAD^{commit}",
            ).stdout.strip()
            self.assertEqual(fetched, verified_head)
            git_run(
                primary,
                "branch",
                "--no-track",
                "--",
                "Feature/Exact",
                verified_head,
            )
            git_run(primary, "worktree", "add", "--", str(worktree), "Feature/Exact")
            attached_head = git_run(worktree, "rev-parse", "HEAD").stdout.strip()
            self.assertEqual(attached_head, verified_head)
            self.assertNotEqual(attached_head, base)

        # 6. Multiple incidental issue references stay mapped but are never
        # promoted into explicit/closing authority.
        with self.subTest(context="multiple incidental links"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = pr_ledger(
                425,
                pr_node="P_incidental",
                branch="Feature/Incidental",
                worktree="/workspace/worktrees/incidental",
            )
            incidentals = [issue(998, "I_incidental_a"), issue(999, "I_incidental_b")]
            candidate["issues"]["incidental"] = incidentals
            created = ledger.create(root, candidate)
            self.assertEqual(created.document["issues"]["incidental"], incidentals)
            self.assertEqual(created.document["issues"]["explicit"], [])
            self.assertEqual(created.document["closing_scope"], [])
            self.assertEqual(created.document["authority"]["allowed"]["issues"], [])

        # 7. A fork head or missing authenticated push authority is rejected
        # before a ledger, lock, or staging inode appears.
        foreign = pr_ledger(
            426,
            pr_node="P_foreign",
            branch="Feature/Foreign",
            worktree="/workspace/worktrees/foreign",
        )
        foreign["selected_prs"][0]["head_repository"] = repository("fork", "R_fork")
        no_push = pr_ledger(
            427,
            pr_node="P_no_push",
            branch="Feature/NoPush",
            worktree="/workspace/worktrees/no-push",
        )
        no_push["actor"]["push_repository_node_ids"] = ["R_unrelated"]
        for label, candidate, message in (
            ("foreign head", foreign, "foreign"),
            ("no push", no_push, "push authority"),
        ):
            with self.subTest(context=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                before = directory_snapshot(root)
                with self.assertRaisesRegex(ledger.LedgerError, message):
                    ledger.create(root, candidate)
                self.assertEqual(directory_snapshot(root), before)

        # 7. Only the exact safe existing worktree is reusable. Unsafe state
        # and a second ledger claiming the same path both stop without change.
        with self.subTest(context="exact compatible worktree"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            compatible = pr_ledger(
                428,
                pr_node="P_worktree",
                branch="Feature/Worktree",
                worktree="/workspace/worktrees/existing",
            )
            compatible["authority"]["kind"] = "explicit-recovery"
            bind_worktree(compatible)
            report = root / "atrinik-atrinik-pr-428.md"
            report.write_bytes(b"pre-schema exact worktree\n")
            migrated = ledger.migrate(
                root,
                report.name,
                compatible,
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(report.read_bytes()),
            )
            ledger.require_reusable_artifacts(migrated.document)
            unsafe = copy.deepcopy(migrated.document)
            next(
                slot for slot in unsafe["artifacts"] if slot["kind"] == "worktree"
            )["safety"]["locked"] = True
            with self.assertRaisesRegex(ledger.LedgerError, "unsafe"):
                ledger.require_reusable_artifacts(unsafe)
            collision = issue_ledger(
                number=429,
                issue_node="I_worktree_collision",
                branch="docs/worktree-collision",
                worktree="/workspace/worktrees/existing",
            )
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "worktree(?:-request)? ownership overlap"
            ):
                ledger.create(root, collision)
            self.assertEqual(directory_snapshot(root), before)

        # 8. Ambiguous mode and near-miss PR/head coordinates are both
        # classification failures and leave an exact empty directory snapshot.
        ambiguous = issue_ledger()
        ambiguous["entry_mode"] = None
        near_miss = pr_ledger(
            430,
            pr_node="P_near_miss",
            branch="Feature/Exact",
            worktree="/workspace/worktrees/near-miss",
        )
        near_miss["selected_prs"][0]["head_branch"] = "Feature/NearMiss"
        for label, candidate, message in (
            ("ambiguous mode", ambiguous, "issue or pr"),
            ("near-miss head", near_miss, "selected PR does not match exactly"),
        ):
            with self.subTest(context=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                before = directory_snapshot(root)
                with self.assertRaisesRegex(ledger.LedgerError, message):
                    ledger.create(root, candidate)
                self.assertEqual(directory_snapshot(root), before)

        # 9. A delegated program leaf remains issue mode, while both full
        # master/leaf identities and the ordered leaf position stay authoritative.
        with self.subTest(context="delegated program leaf"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            delegated = issue_ledger()
            delegated["program"] = {
                "master_issue": issue(400, "I_master"),
                "leaf_issue": issue(),
                "leaf_position": 1,
            }
            delegated["authority"]["allowed"]["issues"] = ["I_issue", "I_master"]
            created = ledger.create(root, delegated)
            self.assertEqual(created.document["entry_mode"], "issue")
            self.assertEqual(created.document["program"]["master_issue"], issue(400, "I_master"))
            self.assertEqual(created.document["program"]["leaf_issue"], issue())
            self.assertEqual(created.document["program"]["leaf_position"], 1)

    def test_16_genesis_and_migration_preconditions_are_zero_write(self) -> None:
        def rejected_create(document: dict[str, object], message: str) -> None:
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                before = directory_snapshot(root)
                with self.assertRaisesRegex(ledger.LedgerError, message):
                    ledger.create(root, document)
                self.assertEqual(directory_snapshot(root), before)

        generation_two = issue_ledger()
        generation_two.update(
            generation=2,
            previous_byte_digest="0" * 64,
            history=["0" * 64],
        )
        rejected_create(generation_two, "generation 1")

        bound_issue = issue_ledger()
        bound_issue["artifacts"][0] = artifact(
            "branch", "branch", branch="docs/issue-419", state="created"
        )
        rejected_create(bound_issue, "all artifacts planned")

        adopted_worktree = pr_ledger(
            423,
            pr_node="P_genesis",
            branch="Feature/Genesis",
            worktree="/workspace/worktrees/genesis",
        )
        bind_worktree(adopted_worktree)
        with tempfile.TemporaryDirectory() as temporary:
            created = ledger.create(Path(temporary), adopted_worktree)
            self.assertEqual(
                {slot["kind"]: slot["state"] for slot in created.document["artifacts"]},
                {"branch": "adopted", "pull_request": "adopted", "worktree": "adopted"},
            )
        unsafe_adoption = copy.deepcopy(adopted_worktree)
        next(
            slot for slot in unsafe_adoption["artifacts"] if slot["kind"] == "worktree"
        )["safety"]["clean"] = False
        rejected_create(unsafe_adoption, "exact safe")

        delivery_body = pr_ledger(
            424,
            pr_node="P_body_genesis",
            branch="Feature/BodyGenesis",
            worktree="/workspace/worktrees/body-genesis",
        )
        delivery_body["selected_prs"][0]["author_node_id"] = "U_actor"
        delivery_body["selected_prs"][0]["body"].update(
            ownership="delivery-created",
            state="written",
            observed_digest=None,
        )
        rejected_create(delivery_body, "observed contributor body")

        nonexistent = Path(tempfile.gettempdir()) / "delivery-ledger-no-such-root"
        with self.assertRaisesRegex(ledger.LedgerError, "exact expected source digest"):
            ledger.migrate(
                nonexistent,
                "atrinik-atrinik-419.md",
                issue_ledger(),
                kind="legacy",
            )
        with self.assertRaisesRegex(ledger.LedgerError, "issue-mode only"):
            ledger.migrate(
                nonexistent,
                "atrinik-atrinik-423.md",
                pr_ledger(
                    423,
                    pr_node="P_legacy_mode",
                    branch="Feature/LegacyMode",
                    worktree="/workspace/worktrees/legacy-mode",
                ),
                kind="legacy",
                expected_source_digest="0" * 64,
            )
        with self.assertRaisesRegex(ledger.LedgerError, "generation 1"):
            ledger.migrate(
                nonexistent,
                "atrinik-atrinik-issue-419.md",
                generation_two,
                kind="pre-schema",
                expected_source_digest="0" * 64,
            )
        with self.assertRaisesRegex(ledger.LedgerError, "must be exactly"):
            ledger.migrate(
                nonexistent,
                "wrong-419.md",
                issue_ledger(),
                kind="legacy",
                expected_source_digest="0" * 64,
            )

        for corruption in ("issue", "pr", "head"):
            with self.subTest(corruption=corruption), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                document = issue_ledger()
                raw = legacy_report_bytes(document)
                if corruption == "issue":
                    raw = raw.replace(b"/issues/419", b"/issues/420")
                elif corruption == "pr":
                    raw += b"https://github.com/atrinik/atrinik/pull/999\n"
                else:
                    raw = raw.replace(b"docs/issue-419", b"docs/wrong-head")
                source = root / "atrinik-atrinik-419.md"
                source.write_bytes(raw)
                before = directory_snapshot(root)
                with self.assertRaisesRegex(ledger.LedgerError, "does not exactly match"):
                    ledger.migrate(
                        root,
                        source.name,
                        document,
                        kind="legacy",
                        expected_source_digest=ledger.byte_digest(raw),
                    )
                self.assertEqual(directory_snapshot(root), before)

    def test_17_migration_plan_short_write_and_link_crashes_resume_exactly(self) -> None:
        for failpoint in ("migration:plan-staged", "migration:plan-linked"):
            with self.subTest(failpoint=failpoint), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                document = issue_ledger()
                raw = legacy_report_bytes(document)
                source = root / "atrinik-atrinik-419.md"
                source.write_bytes(raw)
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.migrate(
                        root,
                        source.name,
                        document,
                        kind="legacy",
                        expected_source_digest=ledger.byte_digest(raw),
                        failpoint=failpoint,
                    )
                resumed = ledger.migrate(
                    root,
                    source.name,
                    document,
                    kind="legacy",
                    expected_source_digest=ledger.byte_digest(raw),
                )
                self.assertEqual(ledger.inventory(root).pending, ())
                self.assertEqual(os.stat(root / resumed.document["migration"]["marker_name"]).st_nlink, 1)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = issue_ledger()
            raw = legacy_report_bytes(document)
            source = root / "atrinik-atrinik-419.md"
            source.write_bytes(raw)
            real_write = ledger.os.write
            writes = 0

            def interrupted_write(descriptor: int, value: object) -> int:
                nonlocal writes
                writes += 1
                if writes == 1:
                    payload = bytes(value)
                    return real_write(descriptor, payload[: max(1, len(payload) // 4)])
                raise InterruptedError("simulated short write")

            with mock.patch.object(ledger.os, "write", side_effect=interrupted_write):
                with self.assertRaises(InterruptedError):
                    ledger.migrate(
                        root,
                        source.name,
                        document,
                        kind="legacy",
                        expected_source_digest=ledger.byte_digest(raw),
                    )
            resumed = ledger.migrate(
                root,
                source.name,
                document,
                kind="legacy",
                expected_source_digest=ledger.byte_digest(raw),
            )
            self.assertEqual(ledger.inventory(root).pending, ())
            canonical_report = root / resumed.document["migration"]["canonical_report"]
            canonical_report.write_text("mutable human report\n", encoding="utf-8")
            self.assertEqual(len(ledger.inventory(root).ledgers), 1)
            source.unlink()
            with self.assertRaisesRegex(ledger.LedgerError, "cannot open regular file"):
                ledger.inventory(root)

    def test_18_cross_process_cas_race_has_one_winner_and_no_debris(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(
                root,
                pr_ledger(
                    453,
                    pr_node="P_cas_race",
                    branch="fix/cas-race",
                    worktree="/workspace/worktrees/cas-race",
                ),
            )
            unchanged = next_generation(initial)
            ready_intent = next_generation(initial)
            ready_intent["selected_prs"][0]["draft_intent"] = "ready"
            replacements = [unchanged, ready_intent]
            processes: list[subprocess.Popen[str]] = []
            for index, candidate in enumerate(replacements):
                input_path = root / f"cas-{index}.json"
                input_path.write_text(json.dumps(candidate), encoding="utf-8")
                processes.append(
                    subprocess.Popen(
                        [
                            sys.executable,
                            "-B",
                            str(SCRIPT),
                            "cas",
                            str(root),
                            initial.name,
                            str(input_path),
                            "--expected-generation",
                            "1",
                            "--expected-digest",
                            initial.digest,
                            "--expected-device",
                            str(initial.device),
                            "--expected-inode",
                            str(initial.inode),
                        ],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                    )
                )
            results = [process.communicate(timeout=20) for process in processes]
            self.assertEqual(sorted(process.returncode for process in processes), [0, 2], results)
            self.assertTrue(any("stale CAS" in stderr for _, stderr in results), results)
            current = ledger.inspect(root, initial.name)
            self.assertIn(current.raw, [ledger.canonical_bytes(value) for value in replacements])
            self.assertEqual(ledger.inventory(root).pending, ())
            self.assertFalse(any(".update-" in path.name for path in root.iterdir()))

    def test_19_multi_target_scope_program_and_resource_ownership_lifecycle(self) -> None:
        bound = multi_target_issue_ledger()
        for slot in bound["artifacts"]:
            if slot["kind"] not in {"branch", "worktree"}:
                continue
            if slot["kind"] == "worktree":
                request = slot["primitive_request"]
                slot["immutable"]["path"] = str(
                    Path(request["roots"]["workspace"]["path"])
                    / "worktrees"
                    / request["physical_checkout"]
                    / request["label"]
                )
                slot["primitive_request"] = None
                slot["primitive_result"] = None
            slot["state"] = "created"
            slot["current"] = {**copy.deepcopy(slot["immutable"]), "head_sha": SHA_A}
            slot["safety"] = copy.deepcopy(ledger.SAFE_ARTIFACT_STATE)
        ledger.prepare(bound)
        ledger.require_reusable_artifacts(bound)
        missing = copy.deepcopy(bound)
        missing["artifacts"] = missing["artifacts"][1:]
        with self.assertRaisesRegex(ledger.LedgerError, "exactly one matching"):
            ledger.prepare(missing)

        client_repository = repository("client", "R_client")
        scope_roots = live_roots(self.live_base / "scope-19", "client")
        request = scope_request(
            component="client",
            checkout="client",
            start_sha=git_head(scope_roots),
            roots=scope_roots,
        )
        live_worktree_path(request)
        scope = scope_resource(request)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = issue_ledger()
            document["resources"] = [scope]
            worktree = next(slot for slot in document["artifacts"] if slot["kind"] == "worktree")
            worktree["immutable"]["path"] = None
            worktree["primitive_request"] = None
            worktree["producer_resource_slot"] = "scope"
            retarget_repository(document, client_repository)
            replace_sha(document, SHA_A, request["start_sha"])
            initial = ledger.create(root, document)
            scope_show = scope_show_bytes(request, repository_name="atrinik/client")
            install_scope_references(request, scope_show)
            worktree_list = worktree_list_bytes(request)
            safety = safety_observation_bytes(
                request,
                worktree_list,
                producer_kind="scope",
                producer_digest=ledger.byte_digest(scope_show),
                repository_value=client_repository,
            )
            classification = ledger.classify_scope_output(
                initial.document, "scope", scope_show, worktree_list, safety
            )
            result = ledger.bind_scope_cas(
                root,
                initial.name,
                "scope",
                scope_show,
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            self.assertEqual(result["snapshot"]["document"]["generation"], 2)
            current = ledger.inspect(root, initial.name)
            ledger.require_reusable_resources(current.document)
            released = next_generation(current)
            released["resources"][0]["current"].update(
                generation=2,
                identity_digest="3" * 64,
                history=[current.document["resources"][0]["current"]["identity_digest"]],
                lifecycle="released",
            )
            released_snapshot = ledger.cas(
                root, current.name, released, **cas_arguments(current)
            )
            with self.assertRaisesRegex(ledger.LedgerError, "unsafe"):
                ledger.require_reusable_resources(released_snapshot.document)

        planned_profile = {
            "slot_id": "profile",
            "kind": "profile",
            "state": "planned",
            "immutable": {
                "repository": repository(),
                "name": "shared-singleton",
                "path": "/workspace/profiles/shared",
            },
            "current": None,
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = issue_ledger()
            first["resources"] = [planned_profile]
            ledger.create(root, first)
            second = issue_ledger(
                number=420,
                issue_node="I_resource_2",
                branch="docs/resource-2",
                worktree="/workspace/worktrees/resource-2",
            )
            second["resources"] = [copy.deepcopy(planned_profile)]
            with self.assertRaisesRegex(ledger.LedgerError, "resource/name ownership overlap"):
                ledger.create(root, second)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            appended = next_generation(initial)
            appended["resources"] = [planned_profile]
            current = ledger.cas(root, initial.name, appended, **cas_arguments(initial))
            illegal = next_generation(current)
            illegal_resource = copy.deepcopy(planned_profile)
            illegal_resource.update(
                slot_id="runtime",
                kind="runtime",
                state="created",
                immutable={
                    "repository": repository(),
                    "name": "runtime-419",
                    "path": "/workspace/runtime/419",
                },
                current={
                    "repository": repository(),
                    "name": "runtime-419",
                    "path": "/workspace/runtime/419",
                    "generation": 1,
                    "external_generation": None,
                    "identity_digest": "3" * 64,
                    "history": [],
                    "lifecycle": "stopped",
                },
            )
            illegal["resources"] = sorted(
                [*illegal["resources"], illegal_resource], key=lambda value: value["slot_id"]
            )
            with self.assertRaisesRegex(ledger.LedgerError, "new resource slot must be planned"):
                ledger.cas(root, current.name, illegal, **cas_arguments(current))

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = issue_ledger()
            first["program"] = {
                "master_issue": issue(400, "I_master"),
                "leaf_issue": issue(),
                "leaf_position": 0,
            }
            first["authority"]["allowed"]["issues"] = ["I_issue", "I_master"]
            ledger.create(root, first)
            second = issue_ledger(
                number=420,
                issue_node="I_leaf2",
                branch="docs/leaf-2",
                worktree="/workspace/worktrees/leaf-2",
            )
            second["program"] = {
                "master_issue": issue(400, "I_master"),
                "leaf_issue": issue(420, "I_leaf2"),
                "leaf_position": 0,
            }
            second["authority"]["allowed"]["issues"] = ["I_leaf2", "I_master"]
            with self.assertRaisesRegex(ledger.LedgerError, "program/master-position"):
                ledger.create(root, second)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = issue_ledger()
            first["program"] = {
                "master_issue": issue(400, "I_master"),
                "leaf_issue": issue(),
                "leaf_position": 0,
            }
            first["authority"]["allowed"]["issues"] = ["I_issue", "I_master"]
            ledger.create(root, first)
            second = issue_ledger(
                number=420,
                issue_node="I_leaf2",
                branch="docs/leaf-2",
                worktree="/workspace/worktrees/leaf-2",
            )
            second["program"] = {
                "master_issue": issue(400, "I_master"),
                "leaf_issue": issue(420, "I_leaf2"),
                "leaf_position": 1,
            }
            second["authority"].update(
                reference=first["authority"]["reference"],
                objective_sha256=first["authority"]["objective_sha256"],
                issued_at="2026-08-14T18:00:00.0Z",
            )
            second["authority"]["allowed"]["issues"] = ["I_leaf2", "I_master"]
            ledger.create(root, second)

            third = issue_ledger(
                number=421,
                issue_node="I_leaf3",
                branch="docs/leaf-3",
                worktree="/workspace/worktrees/leaf-3",
            )
            third["program"] = {
                "master_issue": issue(400, "I_master"),
                "leaf_issue": issue(421, "I_leaf3"),
                "leaf_position": 2,
            }
            third["authority"].update(
                reference=first["authority"]["reference"],
                objective_sha256=first["authority"]["objective_sha256"],
                issued_at="2026-08-14T18:00:00.1Z",
            )
            third["authority"]["allowed"]["issues"] = ["I_leaf3", "I_master"]
            with self.assertRaisesRegex(ledger.LedgerError, "authority families"):
                ledger.create(root, third)

    def test_20_body_comment_intents_markers_drift_and_binding_cli(self) -> None:
        body_raw = b"Owned introduction\r\nkeep every byte"
        body_digest = ledger.byte_digest(body_raw)

        def delivery_document() -> dict[str, object]:
            document = pr_ledger(
                423,
                pr_node="P_owned",
                branch="Feature/Owned",
                worktree="/workspace/worktrees/owned",
            )
            document["authority"]["kind"] = "explicit-recovery"
            pull = document["selected_prs"][0]
            pull["author_node_id"] = "U_actor"
            pull["body"] = {
                "ownership": "delivery-created",
                "state": "written",
                "observed_digest": None,
                "intended_digest": None,
                "intended_payload": None,
                "current_digest": body_digest,
                "outside_digest": body_digest,
                "section_digest": None,
                "updated_at": "2026-08-14T18:00:00Z",
            }
            pr_slot = next(
                slot for slot in document["artifacts"] if slot["kind"] == "pull_request"
            )
            pr_slot["immutable"]["body_digest"] = body_digest
            pr_slot["current"]["body_digest"] = body_digest
            return document

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-pr-423.md"
            report.write_bytes(b"pre-schema owned PR\n")
            initial = ledger.migrate(
                root,
                report.name,
                delivery_document(),
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(report.read_bytes()),
            )
            section = b"delivery section\nsecond line"
            planned_bytes = ledger.plan_body_section(
                initial.document, "P_owned", body_raw, section
            )
            self.assertTrue(planned_bytes.startswith(body_raw + b"\n"))
            checked = ledger.check_body_section(
                initial.document, "P_owned", planned_bytes
            )
            self.assertEqual(checked["state"], "present")
            self.assertEqual(checked["section_digest"], ledger.byte_digest(section))
            replaced = ledger.plan_body_section(
                initial.document, "P_owned", body_raw, b"replacement"
            )
            marker = ledger.delivery_surface_marker(
                initial.document, "P_owned", "body"
            ).encode("ascii")
            self.assertIn(marker, replaced)
            with self.assertRaisesRegex(ledger.LedgerError, "duplicate"):
                ledger.check_body_section(
                    initial.document, "P_owned", planned_bytes + planned_bytes
                )
            with self.assertRaisesRegex(ledger.LedgerError, "malformed"):
                ledger.check_body_section(
                    initial.document,
                    "P_owned",
                    planned_bytes.replace(b":end -->", b":broken -->"),
                )
            with self.assertRaisesRegex(ledger.LedgerError, "recorded current"):
                ledger.plan_body_section(
                    initial.document, "P_owned", body_raw + b"drift", b"x"
                )

            contributor = pr_ledger(
                424,
                pr_node="P_contributor_marker",
                branch="Feature/ContributorMarker",
                worktree="/workspace/worktrees/contributor-marker",
            )
            contributor_marker = ledger.delivery_surface_marker(
                contributor, "P_contributor_marker", "body"
            ).encode("ascii")
            copied = b"<!-- " + contributor_marker + b":start -->\nx\n<!-- " + contributor_marker + b":end -->"
            with self.assertRaisesRegex(ledger.LedgerError, "unowned"):
                ledger.check_body_section(contributor, "P_contributor_marker", copied)
            contributor_raw = b"not-owned"
            contributor_digest = ledger.byte_digest(contributor_raw)
            contributor["selected_prs"][0]["body"].update(
                observed_digest=contributor_digest,
                current_digest=contributor_digest,
                outside_digest=contributor_digest,
            )
            contributor_slot = next(
                value for value in contributor["artifacts"] if value["kind"] == "pull_request"
            )
            contributor_slot["immutable"]["body_digest"] = contributor_digest
            contributor_slot["current"]["body_digest"] = contributor_digest
            contributor_plan = ledger.describe_body_plan(
                contributor, "P_contributor_marker", contributor_raw, b"x"
            )
            self.assertEqual(contributor_plan["body"]["ownership"], "delivery-section")

            intent = next_generation(initial)
            plan = ledger.describe_body_plan(
                initial.document, "P_owned", body_raw, section
            )
            intent["selected_prs"][0]["body"] = plan["body"]
            intent_snapshot = ledger.cas(
                root, initial.name, intent, **cas_arguments(initial)
            )
            drift = next_generation(intent_snapshot)
            drift["targets"][0]["head"].update(
                current_sha=SHA_B, lineage=[SHA_A, SHA_B]
            )
            for slot in drift["artifacts"]:
                if slot["state"] != "planned":
                    slot["current"]["head_sha"] = SHA_B
            with self.assertRaisesRegex(ledger.LedgerError, "canceling"):
                ledger.cas(
                    root, intent_snapshot.name, drift, **cas_arguments(intent_snapshot)
                )
            cancelled = next_generation(intent_snapshot)
            cancelled["selected_prs"][0]["body"].update(
                state="written",
                observed_digest=body_digest,
                intended_digest=None,
                intended_payload=None,
                current_digest=body_digest,
                updated_at="2026-08-14T18:00:00Z",
            )
            cancelled_snapshot = ledger.cas(
                root,
                intent_snapshot.name,
                cancelled,
                **cas_arguments(intent_snapshot),
            )
            refreshed_document = next_generation(cancelled_snapshot)
            refreshed_document["targets"][0]["head"].update(
                current_sha=SHA_B, lineage=[SHA_A, SHA_B]
            )
            for slot in refreshed_document["artifacts"]:
                if slot["state"] != "planned":
                    slot["current"]["head_sha"] = SHA_B
            with self.assertRaisesRegex(
                ledger.LedgerError, "bound authoritative worktree"
            ):
                ledger.cas(
                    root,
                    cancelled_snapshot.name,
                    refreshed_document,
                    **cas_arguments(cancelled_snapshot),
                )
            refreshed = cancelled_snapshot
            replan = next_generation(refreshed)
            replan["selected_prs"][0]["body"] = plan["body"]
            replanned = ledger.cas(
                root, refreshed.name, replan, **cas_arguments(refreshed)
            )
            cli_ready = next_generation(replanned)
            cli_ready["selected_prs"][0]["body"].update(
                state="written",
                observed_digest=body_digest,
                intended_digest=None,
                intended_payload=None,
                current_digest=body_digest,
                outside_digest=body_digest,
                section_digest=None,
            )
            ledger.cas(
                root, replanned.name, cli_ready, **cas_arguments(replanned)
            )

            body_file = root / "body.bin"
            section_file = root / "section.bin"
            body_file.write_bytes(body_raw)
            section_file.write_bytes(section)
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "body-plan",
                    str(root),
                    initial.name,
                    "P_owned",
                    str(body_file),
                    str(section_file),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(
                base64.b64decode(json.loads(process.stdout)["body_base64"]), planned_bytes
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-pr-423.md"
            report.write_bytes(b"pre-schema comment PR\n")
            initial = ledger.migrate(
                root,
                report.name,
                delivery_document(),
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(report.read_bytes()),
            )
            empty_inventory = {"pagination_complete": True, "comments": []}
            self.assertEqual(
                ledger.classify_comments(initial.document, "P_owned", empty_inventory)["action"],
                "plan-required",
            )
            marker_text = (
                f"<!-- {ledger.delivery_surface_marker(initial.document, 'P_owned', 'comment')} -->"
            )
            comment_body = marker_text + "\nDelivery evidence"
            intended_digest = ledger.byte_digest(comment_body.encode("utf-8"))
            planned = next_generation(initial)
            planned["selected_prs"][0]["comment"] = {
                "state": "planned",
                "marker": marker_text,
                "intended_digest": intended_digest,
                "intended_payload": inline_payload(comment_body.encode("utf-8")),
                "node_id": None,
                "current_digest": None,
            }
            planned_snapshot = ledger.cas(
                root, initial.name, planned, **cas_arguments(initial)
            )
            self.assertEqual(
                ledger.classify_comments(
                    planned_snapshot.document, "P_owned", empty_inventory
                )["action"],
                "mark-in-flight-before-write",
            )
            in_flight = next_generation(planned_snapshot)
            in_flight["selected_prs"][0]["comment"]["state"] = "in-flight"
            in_flight_snapshot = ledger.cas(
                root, planned_snapshot.name, in_flight, **cas_arguments(planned_snapshot)
            )
            with self.assertRaisesRegex(ledger.LedgerError, "no safely retryable"):
                ledger.classify_comments(
                    in_flight_snapshot.document, "P_owned", empty_inventory
                )
            live = {
                "pagination_complete": True,
                "comments": [
                    {
                        "node_id": "C_comment",
                        "author_node_id": "U_actor",
                        "body": comment_body,
                    }
                ],
            }
            classification = ledger.classify_comments(
                in_flight_snapshot.document, "P_owned", live
            )
            self.assertEqual(classification["action"], "bind-observed")
            bound = next_generation(in_flight_snapshot)
            bound["selected_prs"][0]["comment"] = {
                "state": "bound",
                "marker": marker_text,
                "intended_digest": None,
                "intended_payload": None,
                "node_id": "C_comment",
                "current_digest": intended_digest,
            }
            bound_snapshot = ledger.cas(
                root, in_flight_snapshot.name, bound, **cas_arguments(in_flight_snapshot)
            )
            self.assertEqual(
                ledger.classify_comments(bound_snapshot.document, "P_owned", live)["action"],
                "bound-match",
            )
            duplicate = {**live, "comments": [*live["comments"], *live["comments"]]}
            with self.assertRaisesRegex(ledger.LedgerError, "duplicate"):
                ledger.classify_comments(bound_snapshot.document, "P_owned", duplicate)
            wrong_author = copy.deepcopy(live)
            wrong_author["comments"][0]["author_node_id"] = "U_other"
            with self.assertRaisesRegex(ledger.LedgerError, "wrong author"):
                ledger.classify_comments(bound_snapshot.document, "P_owned", wrong_author)
            with self.assertRaisesRegex(ledger.LedgerError, "fully paginated"):
                ledger.classify_comments(
                    bound_snapshot.document,
                    "P_owned",
                    {"pagination_complete": False, "comments": []},
                )

        planned_issue = issue_ledger()
        planned_pr_slot = next(
            slot for slot in planned_issue["artifacts"] if slot["kind"] == "pull_request"
        )
        planned_pr_slot["immutable"].update(number=500, node_id="P_created")
        planned_issue["authority"]["allowed"]["pull_requests"] = ["P_created"]
        binding = {
            "repository": repository(),
            "head_branch": "docs/issue-419",
            "number": 500,
            "node_id": "P_created",
            "head_sha": SHA_A,
            "body_digest": planned_pr_slot["immutable"]["body_digest"],
        }
        self.assertEqual(
            ledger.classify_pr_binding(planned_issue, "pull-request", binding),
            "bind-exact",
        )
        for field, value in (
            ("head_branch", "docs/wrong"),
            ("number", 501),
            ("node_id", "P_wrong"),
            ("head_sha", SHA_B),
            ("body_digest", "0" * 64),
        ):
            with self.subTest(binding_field=field):
                mismatch = copy.deepcopy(binding)
                mismatch[field] = value
                with self.assertRaises(ledger.LedgerError):
                    ledger.classify_pr_binding(planned_issue, "pull-request", mismatch)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            created_pr = pr_ledger(
                500,
                pr_node="P_issue_created",
                branch="docs/issue-419",
                worktree="/workspace/worktrees/unused",
            )["selected_prs"][0]
            created_pr["author_node_id"] = "U_actor"
            created_digest = next(
                value
                for value in initial.document["artifacts"]
                if value["kind"] == "pull_request"
            )["immutable"]["body_digest"]
            created_pr["body"] = {
                "ownership": "delivery-created",
                "state": "written",
                "observed_digest": None,
                "intended_digest": None,
                "intended_payload": None,
                "current_digest": created_digest,
                "outside_digest": created_digest,
                "section_digest": None,
                "updated_at": "2026-08-14T18:00:00Z",
            }
            update = next_generation(initial)
            update["selected_prs"] = [created_pr]
            pr_slot = next(
                slot for slot in update["artifacts"] if slot["kind"] == "pull_request"
            )
            pr_slot.update(
                state="created",
                current={
                    **copy.deepcopy(pr_slot["immutable"]),
                    "number": 500,
                    "node_id": "P_issue_created",
                    "head_sha": SHA_A,
                },
                safety={
                    "clean": True,
                    "detached": False,
                    "locked": False,
                    "active": False,
                    "unowned_reference": False,
                    "foreign": False,
                    "certain": True,
                },
            )
            bound = ledger.cas(root, initial.name, update, **cas_arguments(initial))
            self.assertEqual(bound.document["selected_prs"][0]["node_id"], "P_issue_created")

        invalid_timestamp = pr_ledger(
            430,
            pr_node="P_time",
            branch="Feature/Time",
            worktree="/workspace/worktrees/time",
        )
        invalid_timestamp["selected_prs"][0]["body"]["updated_at"] = "2026-02-30T18:00:00Z"
        with self.assertRaisesRegex(ledger.LedgerError, "real UTC timestamp"):
            ledger.prepare(invalid_timestamp)

    def test_21_filesystem_cli_name_paths_and_all_worktree_safety_states(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            plain = Path(temporary)
            before = directory_snapshot(plain)
            with self.assertRaisesRegex(ledger.LedgerError, "lacks safe"):
                ledger.init_root(plain)
            self.assertEqual(directory_snapshot(plain), before)
        with self.assertRaisesRegex(ledger.LedgerError, "filesystem root"):
            ledger.init_root("/")

        with tempfile.TemporaryDirectory() as temporary:
            wrapper = Path(temporary)
            (wrapper / "components.json").write_text("{}\n", encoding="utf-8")
            (wrapper / "atrinik").write_text("#!/bin/sh\n", encoding="utf-8")
            (wrapper / ".git").mkdir()
            counterfeit_before = directory_snapshot(wrapper)
            with self.assertRaisesRegex(ledger.LedgerError, "recognizable|manifest"):
                ledger.init_root(wrapper)
            self.assertEqual(directory_snapshot(wrapper), counterfeit_before)
            (wrapper / "components.json").write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "cohorts": {"default": {}},
                        "stacks": {"default": {}},
                        "checkouts": [{"name": "atrinik"}],
                        "components": [{"name": "atrinik"}],
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            (wrapper / "atrinik").write_text(
                "#!/usr/bin/env python3\n"
                "from atrinik_workspace.cli import main\n"
                "raise SystemExit(main())\n",
                encoding="utf-8",
            )
            os.chmod(wrapper / "atrinik", 0o755)
            (wrapper / ".git/HEAD").write_text(
                "ref: refs/heads/main\n", encoding="ascii"
            )
            initialized = ledger.init_root(wrapper)
            review_root = wrapper / "build/reviews"
            self.assertEqual(initialized["root"], str(review_root))
            self.assertEqual(ledger.init_root(wrapper)["inode"], initialized["inode"])
            os.chmod(review_root, 0o777)
            with self.assertRaisesRegex(ledger.LedgerError, "writable|trusted"):
                ledger.inventory(review_root)
            os.chmod(review_root, 0o700)
            process = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "init-root", str(wrapper)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(process.stdout)["root"], str(review_root))

            document = issue_ledger()
            input_path = wrapper / "input.json"
            input_path.write_text(json.dumps(document), encoding="utf-8")
            created_process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "create",
                    str(review_root),
                    str(input_path),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(created_process.returncode, 0, created_process.stderr)
            name = json.loads(created_process.stdout)["name"]
            for command in (
                ["inspect", str(review_root), name],
                ["inventory", str(review_root)],
                ["check-reuse", str(review_root), name],
            ):
                roundtrip = subprocess.run(
                    [sys.executable, "-B", str(SCRIPT), *command],
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(roundtrip.returncode, 0, roundtrip.stderr)

            symlink_input = wrapper / "input-link.json"
            symlink_input.symlink_to(input_path)
            rejected = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "prepare", str(symlink_input)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(rejected.returncode, 2)
            self.assertIn("cannot open regular file", rejected.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "Notes").write_text("upper", encoding="utf-8")
            (root / "notes").write_text("lower", encoding="utf-8")
            self.assertEqual(ledger.inventory(root).ledgers, ())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            oversized = issue_ledger(number=int("9" * 220), issue_node="I_huge")
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "NAME_MAX"):
                ledger.create(root, oversized)
            self.assertEqual(directory_snapshot(root), before)

        for bad_path in ("/workspace//double", "/workspace/nu\x00l"):
            with self.subTest(bad_path=bad_path):
                invalid = issue_ledger(worktree=bad_path)
                with self.assertRaisesRegex(ledger.LedgerError, "path"):
                    ledger.prepare(invalid)

        exact = pr_ledger(
            423,
            pr_node="P_safety",
            branch="Feature/Safety",
            worktree="/workspace/worktrees/safety",
        )
        bind_worktree(exact)
        ledger.require_reusable_artifacts(exact)
        unsafe_cases = {
            "clean": False,
            "detached": True,
            "locked": True,
            "active": True,
            "unowned_reference": True,
            "foreign": True,
            "certain": False,
        }
        for field, value in unsafe_cases.items():
            with self.subTest(unsafe_field=field):
                unsafe = copy.deepcopy(exact)
                unsafe["artifacts"][2]["safety"][field] = value
                with self.assertRaisesRegex(ledger.LedgerError, "unsafe"):
                    ledger.require_reusable_artifacts(unsafe)

        collision = multi_target_issue_ledger()
        worktrees = [slot for slot in collision["artifacts"] if slot["kind"] == "worktree"]
        first_request = worktrees[0]["primitive_request"]
        second_request = worktrees[1]["primitive_request"]
        first_request["component"] = second_request["component"]
        first_request["physical_checkout"] = second_request["physical_checkout"]
        first_request["label"] = second_request["label"]
        first_request["roots"]["primary"] = copy.deepcopy(
            second_request["roots"]["primary"]
        )
        second_request["roots"]["workspace"]["path"] = first_request["roots"][
            "workspace"
        ]["path"].upper()
        with self.assertRaisesRegex(ledger.LedgerError, "case alias|duplicate"):
            ledger.prepare(collision)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            snapshot = ledger.create(root, issue_ledger())
            os.chmod(root / snapshot.name, 0o644)
            with self.assertRaisesRegex(ledger.LedgerError, "mode is not 0600"):
                ledger.inventory(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            snapshot = ledger.create(root, issue_ledger())
            os.link(root / snapshot.name, root / "unrelated-hardlink")
            with self.assertRaisesRegex(ledger.LedgerError, "unmodeled hard link"):
                ledger.inventory(root)

    def test_22_every_digest_bound_stage_resumes_prefix_without_takeover(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = issue_ledger()
            raw = ledger.canonical_bytes(document)
            target = ledger.canonical_name(document)
            stage = root / f".{target}.create-{ledger.byte_digest(raw)}.tmp"
            stage.write_bytes(raw[:1])
            os.chmod(stage, 0o600)
            takeover = copy.deepcopy(document)
            takeover["authority"]["objective_sha256"] = "9" * 64
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "pending"):
                ledger.create(root, takeover)
            self.assertEqual(directory_snapshot(root), before)
            created = ledger.create(root, document)
            self.assertEqual(created.raw, raw)
            self.assertEqual(ledger.inventory(root).pending, ())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = pr_ledger(
                453,
                pr_node="P_stage_resume",
                branch="fix/stage-resume",
                worktree="/workspace/worktrees/stage-resume",
            )
            created = ledger.create(root, document)
            self.assertEqual(ledger.inventory(root).pending, ())

            update = next_generation(created)
            update_raw = ledger.canonical_bytes(update)
            update_stage = root / (
                f".{created.name}.update-g2-from-{created.digest}-"
                f"to-{ledger.byte_digest(update_raw)}.tmp"
            )
            update_stage.write_bytes(update_raw[:1])
            os.chmod(update_stage, 0o600)
            other = next_generation(created)
            other["selected_prs"][0]["draft_intent"] = "ready"
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "pending"):
                ledger.cas(root, created.name, other, **cas_arguments(created))
            self.assertEqual(directory_snapshot(root), before)
            updated = ledger.cas(root, created.name, update, **cas_arguments(created))
            self.assertEqual(updated.raw, update_raw)
            self.assertEqual(ledger.inventory(root).pending, ())

        phase_failpoint = {
            "snapshot": "migration:planned",
            "report": "migration:snapshot",
            "prepare": "migration:report",
            "destination": "migration:prepared-renamed",
            "complete": "migration:installed",
        }
        for phase, precursor in phase_failpoint.items():
            with self.subTest(phase=phase), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                document = issue_ledger()
                raw = legacy_report_bytes(document)
                source = root / "atrinik-atrinik-419.md"
                source.write_bytes(raw)
                digest = ledger.byte_digest(raw)
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.migrate(
                        root,
                        source.name,
                        document,
                        kind="legacy",
                        expected_source_digest=digest,
                        failpoint=precursor,
                    )
                real_write = ledger.os.write
                writes = 0

                def interrupt_stage(descriptor: int, value: object) -> int:
                    nonlocal writes
                    writes += 1
                    if writes == 1:
                        return real_write(descriptor, bytes(value)[:1])
                    raise InterruptedError(f"interrupt {phase}")

                with mock.patch.object(ledger.os, "write", side_effect=interrupt_stage):
                    with self.assertRaises(InterruptedError):
                        ledger.migrate(
                            root,
                            source.name,
                            document,
                            kind="legacy",
                            expected_source_digest=digest,
                        )
                resumed = ledger.migrate(
                    root,
                    source.name,
                    document,
                    kind="legacy",
                    expected_source_digest=digest,
                )
                self.assertEqual(resumed.document["migration"]["state"], "complete")
                self.assertEqual(ledger.inventory(root).pending, ())

    def test_23_recovery_binding_and_migration_cli_roundtrips(self) -> None:
        commands = (
            "init-root",
            "prepare",
            "inspect",
            "inventory",
            "create",
            "cas",
            "migrate",
            "check-reuse",
            "body-check",
            "body-plan",
            "body-recovery",
            "pr-create-payload",
            "bind-check",
            "comment-check",
        )
        for command in commands:
            with self.subTest(help=command):
                process = subprocess.run(
                    [sys.executable, "-B", str(SCRIPT), command, "--help"],
                    text=True,
                    capture_output=True,
                    check=False,
                )
                self.assertEqual(process.returncode, 0, process.stderr)
                self.assertIn(command, process.stdout)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            contributor = pr_ledger(
                431,
                pr_node="P_cli",
                branch="Feature/CLI",
                worktree="/workspace/worktrees/cli-pr",
            )
            contributor_snapshot = ledger.create(root, contributor)
            issue_candidate = issue_ledger(
                number=432,
                issue_node="I_cli_issue",
                branch="docs/cli-bind",
                worktree="/workspace/worktrees/cli-bind",
            )
            issue_snapshot = ledger.create(root, issue_candidate)

            prepare_input = root / "prepare-input.json"
            prepare_input.write_text(json.dumps(issue_candidate), encoding="utf-8")
            prepared = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "prepare", str(prepare_input)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(prepared.returncode, 0, prepared.stderr)
            self.assertEqual(json.loads(prepared.stdout), issue_candidate)

            recovery = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "body-recovery",
                    str(root),
                    contributor_snapshot.name,
                    "P_cli",
                    "e" * 64,
                    "2026-08-14T18:00:00Z",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(recovery.returncode, 0, recovery.stderr)
            self.assertEqual(json.loads(recovery.stdout)["action"], "read-only-match")

            comment_input = root / "comment-inventory.json"
            comment_input.write_text(
                json.dumps({"pagination_complete": True, "comments": []}),
                encoding="utf-8",
            )
            comment = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "comment-check",
                    str(root),
                    contributor_snapshot.name,
                    "P_cli",
                    str(comment_input),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(comment.returncode, 0, comment.stderr)
            self.assertEqual(json.loads(comment.stdout)["action"], "plan-required")

            binding_input = root / "binding.json"
            planned_digest = next(
                value
                for value in issue_snapshot.document["artifacts"]
                if value["kind"] == "pull_request"
            )["immutable"]["body_digest"]
            binding_input.write_text(
                json.dumps(
                    {
                        "repository": repository(),
                        "head_branch": "docs/cli-bind",
                        "number": 500,
                        "node_id": "P_cli_created",
                        "head_sha": SHA_A,
                        "body_digest": planned_digest,
                    }
                ),
                encoding="utf-8",
            )
            binding = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "bind-check",
                    str(root),
                    issue_snapshot.name,
                    "pull-request",
                    str(binding_input),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(binding.returncode, 0, binding.stderr)
            self.assertEqual(json.loads(binding.stdout)["classification"], "bind-exact")

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = issue_ledger()
            source = root / "atrinik-atrinik-419.md"
            source_bytes = legacy_report_bytes(candidate)
            source.write_bytes(source_bytes)
            candidate_input = root / "migration-candidate.json"
            candidate_input.write_text(json.dumps(candidate), encoding="utf-8")
            migrated = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "migrate",
                    str(root),
                    source.name,
                    str(candidate_input),
                    "--kind",
                    "legacy",
                    "--expected-source-digest",
                    ledger.byte_digest(source_bytes),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(migrated.returncode, 0, migrated.stderr)
            result = json.loads(migrated.stdout)
            self.assertEqual(result["document"]["migration"]["state"], "complete")
            self.assertEqual(ledger.inventory(root).pending, ())

    def test_24_issue_created_pr_authority_exception_is_one_exact_slot_only(self) -> None:
        def bind_one(snapshot: object) -> dict[str, object]:
            assert isinstance(snapshot, ledger.Snapshot)
            update = next_generation(snapshot)
            slot = next(
                value for value in update["artifacts"] if value["kind"] == "pull_request"
            )
            target = next(
                value
                for value in update["targets"]
                if value["repository"] == slot["immutable"]["repository"]
                and value["head"]["branch"] == slot["immutable"]["branch"]
            )
            number = 500
            node = "P_issue_created"
            digest = slot["immutable"]["body_digest"]
            update["selected_prs"] = [
                {
                    "repository": copy.deepcopy(target["repository"]),
                    "head_repository": copy.deepcopy(target["repository"]),
                    "number": number,
                    "node_id": node,
                    "author_node_id": update["actor"]["node_id"],
                    "base_branch": target["base"]["branch"],
                    "head_branch": target["head"]["branch"],
                    "draft": True,
                    "draft_intent": None,
                    "body": {
                        "ownership": "delivery-created",
                        "state": "written",
                        "observed_digest": None,
                        "intended_digest": None,
                        "intended_payload": None,
                        "current_digest": digest,
                        "outside_digest": digest,
                        "section_digest": None,
                        "updated_at": "2026-08-14T18:00:00Z",
                    },
                    "comment": {
                        "state": "none",
                        "marker": None,
                        "intended_digest": None,
                        "intended_payload": None,
                        "node_id": None,
                        "current_digest": None,
                    },
                }
            ]
            slot["state"] = "created"
            slot["current"] = {
                **copy.deepcopy(slot["immutable"]),
                "number": number,
                "node_id": node,
                "head_sha": target["head"]["current_sha"],
            }
            slot["safety"] = {
                "clean": True,
                "detached": False,
                "locked": False,
                "active": False,
                "unowned_reference": False,
                "foreign": False,
                "certain": True,
            }
            return update

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            bound = ledger.cas(root, initial.name, bind_one(initial), **cas_arguments(initial))
            self.assertEqual(bound.document["authority"]["allowed"]["pull_requests"], [])
            self.assertEqual(bound.document["selected_prs"][0]["node_id"], "P_issue_created")
            self.assertEqual(
                next(
                    slot
                    for slot in bound.document["artifacts"]
                    if slot["kind"] == "pull_request"
                )["state"],
                "created",
            )

        def reject(label: str, mutate: object) -> None:
            with self.subTest(rejected=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                initial = ledger.create(root, issue_ledger())
                candidate = bind_one(initial)
                assert callable(mutate)
                mutate(candidate)
                before = directory_snapshot(root)
                with self.assertRaises(ledger.LedgerError):
                    ledger.cas(root, initial.name, candidate, **cas_arguments(initial))
                self.assertEqual(directory_snapshot(root), before)

        reject(
            "wrong actor",
            lambda value: value["selected_prs"][0].update(author_node_id="U_other"),
        )

        def wrong_body(value: dict[str, object]) -> None:
            value["selected_prs"][0]["body"]["current_digest"] = "f" * 64
            next(
                slot for slot in value["artifacts"] if slot["kind"] == "pull_request"
            )["current"]["body_digest"] = "f" * 64

        reject("wrong body intent", wrong_body)
        reject(
            "foreign repository",
            lambda value: value["selected_prs"][0].update(
                head_repository=repository("content", "R_content")
            ),
        )
        reject(
            "wrong base",
            lambda value: value["selected_prs"][0].update(base_branch="develop"),
        )
        reject(
            "wrong head",
            lambda value: value["selected_prs"][0].update(head_branch="docs/other"),
        )
        reject(
            "adopted rather than created",
            lambda value: next(
                slot for slot in value["artifacts"] if slot["kind"] == "pull_request"
            ).update(state="adopted"),
        )
        reject("changed closing intent", lambda value: value.update(closing_scope=[]))

        def bind_unrelated_branch(value: dict[str, object]) -> None:
            branch = next(
                slot for slot in value["artifacts"] if slot["kind"] == "branch"
            )
            branch["state"] = "created"
            branch["current"] = {
                **copy.deepcopy(branch["immutable"]),
                "head_sha": SHA_A,
            }
            branch["safety"] = {
                "clean": True,
                "detached": False,
                "locked": False,
                "active": False,
                "unowned_reference": False,
                "foreign": False,
                "certain": True,
            }

        reject("unrelated artifact bind", bind_unrelated_branch)

        with self.subTest(rejected="two selected PRs"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, multi_target_issue_ledger())
            candidate = next_generation(initial)
            pulls: list[dict[str, object]] = []
            for number, slot in enumerate(
                (
                    value
                    for value in candidate["artifacts"]
                    if value["kind"] == "pull_request"
                ),
                start=500,
            ):
                target = next(
                    value
                    for value in candidate["targets"]
                    if value["repository"] == slot["immutable"]["repository"]
                    and value["head"]["branch"] == slot["immutable"]["branch"]
                )
                node = f"P_issue_created_{number}"
                digest = slot["immutable"]["body_digest"]
                pulls.append(
                    {
                        "repository": copy.deepcopy(target["repository"]),
                        "head_repository": copy.deepcopy(target["repository"]),
                        "number": number,
                        "node_id": node,
                        "author_node_id": "U_actor",
                        "base_branch": target["base"]["branch"],
                        "head_branch": target["head"]["branch"],
                        "draft": True,
                        "draft_intent": None,
                        "body": {
                            "ownership": "delivery-created",
                            "state": "written",
                            "observed_digest": None,
                            "intended_digest": None,
                            "intended_payload": None,
                            "current_digest": digest,
                            "outside_digest": digest,
                            "section_digest": None,
                            "updated_at": "2026-08-14T18:00:00Z",
                        },
                        "comment": {
                            "state": "none",
                            "marker": None,
                            "intended_digest": None,
                            "intended_payload": None,
                            "node_id": None,
                            "current_digest": None,
                        },
                    }
                )
                slot["state"] = "created"
                slot["current"] = {
                    **copy.deepcopy(slot["immutable"]),
                    "number": number,
                    "node_id": node,
                    "head_sha": target["head"]["current_sha"],
                }
                slot["safety"] = {
                    "clean": True,
                    "detached": False,
                    "locked": False,
                    "active": False,
                    "unowned_reference": False,
                    "foreign": False,
                    "certain": True,
                }
            candidate["selected_prs"] = sorted(
                pulls,
                key=lambda pull: (
                    pull["repository"]["owner"],
                    pull["repository"]["name"],
                    pull["number"],
                ),
            )
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "exactly one selected PR"):
                ledger.cas(root, initial.name, candidate, **cas_arguments(initial))
            self.assertEqual(directory_snapshot(root), before)

    def test_25_strict_authority_genesis_and_git_ref_contracts(self) -> None:
        strict = issue_ledger()
        strict["schema_version"] = 1.0
        with self.assertRaisesRegex(ledger.LedgerError, "schema_version"):
            ledger.prepare(strict)

        impossible_time = issue_ledger()
        impossible_time["authority"]["issued_at"] = "2026-02-30T18:00:00Z"
        with self.assertRaisesRegex(ledger.LedgerError, "real UTC timestamp"):
            ledger.prepare(impossible_time)

        incidental = issue(420, "I_incidental")
        widened = issue_ledger()
        widened["issues"]["incidental"] = [incidental]
        widened["closing_scope"] = [incidental]
        with self.assertRaises(ledger.LedgerError):
            ledger.prepare(widened)

        for allowed_field, value in (
            ("repositories", "R_unrelated"),
            ("issues", "I_unrelated"),
            ("pull_requests", "P_unrelated"),
        ):
            with self.subTest(extra_allowlist=allowed_field):
                extra = issue_ledger()
                extra["authority"]["allowed"][allowed_field].append(value)
                extra["authority"]["allowed"][allowed_field].sort()
                with self.assertRaisesRegex(ledger.LedgerError, "authority|allow"):
                    ledger.prepare(extra)

        valid_unusual = issue_ledger(
            branch="feature/_foo+bar",
            worktree="/workspace/worktrees/unusual-ref",
        )
        self.assertEqual(ledger.prepare(valid_unusual), valid_unusual)

        unsafe_candidates: list[tuple[str, dict[str, object]]] = []
        for branch in ("main", "HEAD"):
            unsafe_candidates.append((f"reserved branch {branch}", issue_ledger(branch=branch)))
        unrelated_head = issue_ledger()
        unrelated_head["targets"][0]["head"].update(
            initial_sha=SHA_B, current_sha=SHA_B, lineage=[SHA_B]
        )
        unsafe_candidates.append(("unrelated head anchor", unrelated_head))
        preauthorized = issue_ledger()
        pr_slot = next(
            slot for slot in preauthorized["artifacts"] if slot["kind"] == "pull_request"
        )
        pr_slot["immutable"].update(number=423, node_id="P_existing")
        preauthorized["authority"]["allowed"]["pull_requests"] = ["P_existing"]
        unsafe_candidates.append(("preauthorized existing PR", preauthorized))
        for label, candidate in unsafe_candidates:
            with self.subTest(genesis=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                before = directory_snapshot(root)
                with self.assertRaises(ledger.LedgerError):
                    ledger.create(root, candidate)
                self.assertEqual(directory_snapshot(root), before)

        ready = pr_ledger(
            430,
            pr_node="P_ready_genesis",
            branch="Feature/ReadyGenesis",
            worktree="/workspace/worktrees/ready-genesis",
        )
        ready["selected_prs"][0]["draft_intent"] = "ready"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "fresh PR-mode|intent"):
                ledger.create(root, ready)
            self.assertEqual(directory_snapshot(root), before)

    def test_26_issue_created_pr_can_finish_body_comment_and_ready_lifecycles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            bound = ledger.cas(
                root,
                initial.name,
                bind_issue_created_pr(initial),
                **cas_arguments(initial),
            )
            old_digest = bound.document["selected_prs"][0]["body"]["current_digest"]
            body_projection = ledger.describe_body_plan(
                bound.document,
                "P_issue_created",
                INITIAL_PR_BODY,
                b"Issue-created delivery section",
            )
            intended_digest = body_projection["body_digest"]
            body_plan = next_generation(bound)
            body_plan["selected_prs"][0]["body"] = body_projection["body"]
            planned = ledger.cas(root, bound.name, body_plan, **cas_arguments(bound))

            body_bound = next_generation(planned)
            body_bound["selected_prs"][0]["body"] = ledger.classify_body_recovery(
                planned.document,
                "P_issue_created",
                intended_digest,
                "2026-08-14T18:00:01Z",
            )["cas_body"]
            next(
                slot
                for slot in body_bound["artifacts"]
                if slot["kind"] == "pull_request"
            )["current"]["body_digest"] = intended_digest
            written = ledger.cas(
                root, planned.name, body_bound, **cas_arguments(planned)
            )

            marker = (
                f"<!-- {ledger.delivery_surface_marker(written.document, 'P_issue_created', 'comment')} -->"
            )
            comment_bytes = (marker + "\nDelivery evidence").encode("utf-8")
            comment_digest = ledger.byte_digest(comment_bytes)
            comment_plan = next_generation(written)
            comment_plan["selected_prs"][0]["comment"] = {
                "state": "planned",
                "marker": marker,
                "intended_digest": comment_digest,
                "intended_payload": inline_payload(comment_bytes),
                "node_id": None,
                "current_digest": None,
            }
            comment_planned = ledger.cas(
                root, written.name, comment_plan, **cas_arguments(written)
            )
            in_flight_document = next_generation(comment_planned)
            in_flight_document["selected_prs"][0]["comment"]["state"] = "in-flight"
            in_flight = ledger.cas(
                root,
                comment_planned.name,
                in_flight_document,
                **cas_arguments(comment_planned),
            )
            comment_bound = next_generation(in_flight)
            comment_bound["selected_prs"][0]["comment"] = {
                "state": "bound",
                "marker": marker,
                "intended_digest": None,
                "intended_payload": None,
                "node_id": "C_delivery",
                "current_digest": comment_digest,
            }
            commented = ledger.cas(
                root, in_flight.name, comment_bound, **cas_arguments(in_flight)
            )

            ready_plan = next_generation(commented)
            ready_plan["selected_prs"][0]["draft_intent"] = "ready"
            ready_planned = ledger.cas(
                root, commented.name, ready_plan, **cas_arguments(commented)
            )
            ready_result = next_generation(ready_planned)
            ready_result["selected_prs"][0].update(draft=False, draft_intent=None)
            ready = ledger.cas(
                root, ready_planned.name, ready_result, **cas_arguments(ready_planned)
            )
            self.assertFalse(ready.document["selected_prs"][0]["draft"])

    def test_27_scope_binding_lifecycle_and_drift_are_atomic(self) -> None:
        client_repository = repository("client", "R_client")
        scope_roots = live_roots(self.live_base / "scope-27", "client")
        request = scope_request(
            component="client",
            checkout="client",
            start_sha=git_head(scope_roots),
            roots=scope_roots,
        )
        live_worktree_path(request)
        scope = scope_resource(request)

        def scoped_document() -> dict[str, object]:
            document = issue_ledger()
            document["resources"] = [copy.deepcopy(scope)]
            worktree = next(
                slot for slot in document["artifacts"] if slot["kind"] == "worktree"
            )
            worktree["immutable"]["path"] = None
            worktree["primitive_request"] = None
            worktree["producer_resource_slot"] = "scope"
            retarget_repository(document, client_repository)
            replace_sha(document, SHA_A, request["start_sha"])
            return document

        invalid_pr = pr_ledger(
            431,
            pr_node="P_scope",
            branch="Feature/Scope",
            worktree="/workspace/worktrees/scope-pr",
        )
        invalid_pr["resources"] = [copy.deepcopy(scope)]
        with self.assertRaisesRegex(ledger.LedgerError, "scope|PR mode"):
            ledger.prepare(invalid_pr)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, scoped_document())
            scope_show = scope_show_bytes(request, repository_name="atrinik/client")
            install_scope_references(request, scope_show)
            worktree_list = worktree_list_bytes(request)
            safety = safety_observation_bytes(
                request,
                worktree_list,
                producer_kind="scope",
                producer_digest=ledger.byte_digest(scope_show),
                repository_value=client_repository,
            )
            exact = ledger.classify_scope_output(
                initial.document, "scope", scope_show, worktree_list, safety
            )
            switched_producer = next_generation(initial)
            next(
                slot
                for slot in switched_producer["artifacts"]
                if slot["kind"] == "worktree"
            )["producer_resource_slot"] = "other-scope"
            with self.assertRaisesRegex(ledger.LedgerError, "producer"):
                ledger._transition(
                    initial.document, switched_producer, initial.digest
                )

            def bind_scope(candidate: dict[str, object], *, resource: bool, artifacts: bool) -> None:
                if resource:
                    candidate["resources"] = [copy.deepcopy(exact["resource"])]
                if artifacts:
                    for slot in candidate["artifacts"]:
                        if slot["kind"] == "branch":
                            slot.update(copy.deepcopy(exact["branch_artifact"]))
                        elif slot["kind"] == "worktree":
                            slot.update(copy.deepcopy(exact["worktree_artifact"]))

            for label, bind_resource, bind_artifacts in (
                ("scope only", True, False),
                ("artifacts only", False, True),
            ):
                with self.subTest(partial=label):
                    partial = next_generation(initial)
                    bind_scope(partial, resource=bind_resource, artifacts=bind_artifacts)
                    with self.assertRaisesRegex(ledger.LedgerError, "scope|atomic|planned"):
                        ledger.cas(root, initial.name, partial, **cas_arguments(initial))

            invalid_generation = next_generation(initial)
            bind_scope(invalid_generation, resource=True, artifacts=True)
            invalid_generation["resources"][0]["current"].update(
                generation=3,
                history=[exact["result_sha256"], exact["result_sha256"]],
            )
            with self.assertRaisesRegex(ledger.LedgerError, "generation"):
                ledger.cas(
                    root,
                    initial.name,
                    invalid_generation,
                    **cas_arguments(initial),
                )

            bound_document = next_generation(initial)
            bind_scope(bound_document, resource=True, artifacts=True)
            with self.assertRaisesRegex(
                ledger.LedgerError, "purpose-specific atomic binder"
            ):
                ledger.cas(
                    root, initial.name, bound_document, **cas_arguments(initial)
                )
            result = ledger.bind_scope_cas(
                root,
                initial.name,
                "scope",
                scope_show,
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            self.assertEqual(result["snapshot"]["document"]["generation"], 2)
            bound = ledger.inspect(root, initial.name)
            released_document = next_generation(bound)
            released_current = released_document["resources"][0]["current"]
            released_current.update(
                generation=2,
                identity_digest="3" * 64,
                history=[exact["result_sha256"]],
                lifecycle="released",
            )
            released = ledger.cas(
                root, bound.name, released_document, **cas_arguments(bound)
            )
            resurrected = next_generation(released)
            resurrected_current = resurrected["resources"][0]["current"]
            resurrected_current.update(
                generation=3,
                identity_digest=exact["result_sha256"],
                history=[exact["result_sha256"], "3" * 64],
                lifecycle="active",
            )
            with self.assertRaisesRegex(ledger.LedgerError, "released|terminal"):
                ledger.cas(
                    root,
                    released.name,
                    resurrected,
                    **cas_arguments(released),
                )

    def test_28_cross_ledger_identity_and_pr_reservations_are_global(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-issue-419.md"
            report.write_bytes(b"pre-schema planned PR ownership\n")
            reserved = issue_ledger()
            slot = next(
                value for value in reserved["artifacts"] if value["kind"] == "pull_request"
            )
            slot["immutable"].update(number=423, node_id="P_reserved")
            reserved["authority"]["allowed"]["pull_requests"] = ["P_reserved"]
            ledger.migrate(
                root,
                report.name,
                reserved,
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(report.read_bytes()),
            )
            contender = pr_ledger(
                423,
                pr_node="P_reserved",
                branch="Feature/Other",
                worktree="/workspace/worktrees/other-pr",
            )
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "pr ownership overlap"):
                ledger.create(root, contender)
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first_report = root / "atrinik-atrinik-issue-419.md"
            first_report.write_bytes(b"first reserved PR\n")
            first = issue_ledger()
            first_slot = next(
                value for value in first["artifacts"] if value["kind"] == "pull_request"
            )
            first_slot["immutable"].update(number=423, node_id="P_reserved")
            first["authority"]["allowed"]["pull_requests"] = ["P_reserved"]
            ledger.migrate(
                root,
                first_report.name,
                first,
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(first_report.read_bytes()),
            )
            second_report = root / "atrinik-atrinik-issue-420.md"
            second_report.write_bytes(b"second reserved PR\n")
            second = issue_ledger(
                number=420,
                issue_node="I_second_reserved",
                branch="docs/second-reserved",
                worktree="/workspace/worktrees/second-reserved",
            )
            second_slot = next(
                value for value in second["artifacts"] if value["kind"] == "pull_request"
            )
            second_slot["immutable"].update(number=423, node_id="P_reserved")
            second["authority"]["allowed"]["pull_requests"] = ["P_reserved"]
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "pr ownership overlap"):
                ledger.migrate(
                    root,
                    second_report.name,
                    second,
                    kind="pre-schema",
                    expected_source_digest=ledger.byte_digest(second_report.read_bytes()),
                )
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ledger.create(root, issue_ledger())
            alias = issue_ledger(
                number=420,
                issue_node="I_alias",
                branch="docs/alias",
                worktree="/workspace/worktrees/alias",
            )

            def replace_repository_node(value: object) -> None:
                if isinstance(value, dict):
                    if set(value) == {"owner", "name", "node_id"} and value["node_id"] == "R_repo":
                        value["node_id"] = "R_alias"
                    for nested in value.values():
                        replace_repository_node(nested)
                elif isinstance(value, list):
                    for nested in value:
                        replace_repository_node(nested)

            replace_repository_node(alias)
            alias["actor"]["push_repository_node_ids"] = ["R_alias"]
            alias["authority"]["allowed"]["repositories"] = ["R_alias"]
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "repository coordinate"):
                ledger.create(root, alias)
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = issue_ledger()
            first["program"] = {
                "master_issue": issue(400, "I_master_a"),
                "leaf_issue": issue(),
                "leaf_position": 0,
            }
            first["authority"]["allowed"]["issues"] = ["I_issue", "I_master_a"]
            ledger.create(root, first)
            second = issue_ledger(
                number=420,
                issue_node="I_program_leaf",
                branch="docs/program-leaf",
                worktree="/workspace/worktrees/program-leaf",
            )
            second["program"] = {
                "master_issue": issue(400, "I_master_b"),
                "leaf_issue": issue(420, "I_program_leaf"),
                "leaf_position": 1,
            }
            second["authority"]["allowed"]["issues"] = [
                "I_master_b",
                "I_program_leaf",
            ]
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "issue coordinate"):
                ledger.create(root, second)
            self.assertEqual(directory_snapshot(root), before)

    def test_29_body_comment_and_remote_intent_recovery_is_exact(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(
                root,
                pr_ledger(
                    432,
                    pr_node="P_observed",
                    branch="Feature/Observed",
                    worktree="/workspace/worktrees/observed",
                ),
            )
            refreshed_document = next_generation(initial)
            refreshed_body = refreshed_document["selected_prs"][0]["body"]
            refreshed_body.update(
                observed_digest="f" * 64,
                current_digest="f" * 64,
                outside_digest="f" * 64,
                updated_at="2026-08-14T18:00:01Z",
            )
            next(
                slot
                for slot in refreshed_document["artifacts"]
                if slot["kind"] == "pull_request"
            )["current"]["body_digest"] = "f" * 64
            refreshed = ledger.cas(
                root,
                initial.name,
                refreshed_document,
                **cas_arguments(initial),
            )
            rollback = next_generation(refreshed)
            rollback["selected_prs"][0]["body"]["updated_at"] = (
                "2026-08-14T17:59:59Z"
            )
            with self.assertRaisesRegex(ledger.LedgerError, "timestamp|backward"):
                ledger.cas(root, refreshed.name, rollback, **cas_arguments(refreshed))

            ready_plan = next_generation(refreshed)
            ready_plan["selected_prs"][0]["draft_intent"] = "ready"
            ready = ledger.cas(
                root, refreshed.name, ready_plan, **cas_arguments(refreshed)
            )
            combined = next_generation(ready)
            combined["selected_prs"][0]["draft_intent"] = None
            combined["targets"][0]["base"].update(
                current_sha=SHA_B, lineage=[SHA_A, SHA_B]
            )
            combined["targets"][0]["merge_base"]["current_sha"] = SHA_B
            with self.assertRaisesRegex(ledger.LedgerError, "separate|cancel"):
                ledger.cas(root, ready.name, combined, **cas_arguments(ready))

        comment_document = pr_ledger(
            433,
            pr_node="P_comment_exact",
            branch="Feature/CommentExact",
            worktree="/workspace/worktrees/comment-exact",
        )
        comment_document["authority"]["kind"] = "explicit-recovery"
        marker = (
            f"<!-- {ledger.delivery_surface_marker(comment_document, 'P_comment_exact', 'comment')} -->"
        )
        body = marker + "\nintended"
        digest = ledger.byte_digest(body.encode("utf-8"))
        comment_document["selected_prs"][0]["comment"] = {
            "state": "in-flight",
            "marker": marker,
            "intended_digest": digest,
            "intended_payload": inline_payload(body.encode("utf-8")),
            "node_id": None,
            "current_digest": None,
        }
        inventory = {
            "pagination_complete": True,
            "comments": [
                {
                    "node_id": "C_exact",
                    "author_node_id": "U_actor",
                    "body": body + "\natrinik-delivery:comment:foreign",
                }
            ],
        }
        with self.assertRaisesRegex(ledger.LedgerError, "foreign|malformed"):
            ledger.classify_comments(comment_document, "P_comment_exact", inventory)

        owned = copy.deepcopy(comment_document)
        owned["selected_prs"][0]["author_node_id"] = "U_actor"
        owned["selected_prs"][0]["body"].update(
            ownership="delivery-created",
            state="written",
            observed_digest=None,
        )
        raw_marker = ledger.delivery_surface_marker(owned, "P_comment_exact", "body")
        rendered = (
            f"<!-- {raw_marker}:start -->\ncurrent\n<!-- {raw_marker}:end -->"
        ).encode("utf-8")
        rendered_digest = ledger.byte_digest(rendered)
        owned["selected_prs"][0]["body"].update(
            current_digest=rendered_digest,
            outside_digest=ledger.byte_digest(b""),
            section_digest=ledger.byte_digest(b"current"),
        )
        next(
            slot for slot in owned["artifacts"] if slot["kind"] == "pull_request"
        )["immutable"]["body_digest"] = rendered_digest
        next(
            slot for slot in owned["artifacts"] if slot["kind"] == "pull_request"
        )["current"]["body_digest"] = rendered_digest
        with self.assertRaisesRegex(ledger.LedgerError, "no-op|unchanged"):
            ledger.plan_body_section(owned, "P_comment_exact", rendered, b"current")
        with self.assertRaisesRegex(ledger.LedgerError, "UTF-8"):
            ledger.plan_body_section(owned, "P_comment_exact", rendered, b"\xff")

    def test_30_migration_plan_is_candidate_bound_and_cli_stdin_is_prepare_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = issue_ledger()
            source = root / "atrinik-atrinik-419.md"
            source.write_bytes(legacy_report_bytes(document))
            source_digest = ledger.byte_digest(source.read_bytes())
            real_write = ledger.os.write
            writes = 0

            def interrupt_first_stage(descriptor: int, value: object) -> int:
                nonlocal writes
                writes += 1
                if writes == 1:
                    return real_write(descriptor, bytes(value)[:1])
                raise InterruptedError("migration plan prefix")

            with mock.patch.object(ledger.os, "write", side_effect=interrupt_first_stage):
                with self.assertRaises(InterruptedError):
                    ledger.migrate(
                        root,
                        source.name,
                        document,
                        kind="legacy",
                        expected_source_digest=source_digest,
                    )
            takeover = copy.deepcopy(document)
            takeover["authority"]["objective_sha256"] = "9" * 64
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "pending|candidate"):
                ledger.migrate(
                    root,
                    source.name,
                    takeover,
                    kind="legacy",
                    expected_source_digest=source_digest,
                )
            self.assertEqual(directory_snapshot(root), before)
            resumed = ledger.migrate(
                root,
                source.name,
                document,
                kind="legacy",
                expected_source_digest=source_digest,
            )
            self.assertEqual(resumed.document["migration"]["state"], "complete")

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            raw = json.dumps(issue_ledger())
            before = directory_snapshot(root)
            rejected = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "create", str(root), "-"],
                input=raw,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(rejected.returncode, 2, rejected.stderr)
            self.assertEqual(directory_snapshot(root), before)
            accepted = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "prepare", "-"],
                input=raw,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(accepted.returncode, 0, accepted.stderr)

    def test_31_deferred_primitive_worktree_request_result_and_pr_genesis(self) -> None:
        roots = live_roots(self.live_base / "primitive", "atrinik")
        live_head = git_head(roots)

        def deferred(
            *,
            number: int = 440,
            node: str = "P_deferred",
            branch: str = "Feature/Deferred",
            label: str = "pr-440",
        ) -> dict[str, object]:
            document = pr_ledger(
                number,
                pr_node=node,
                branch=branch,
                worktree=f"/unused/{label}",
            )
            replace_sha(document, SHA_A, live_head)
            branch_slot = next(
                slot for slot in document["artifacts"] if slot["kind"] == "branch"
            )
            branch_slot.update(state="planned", current=None, safety=None)
            worktree = next(
                slot for slot in document["artifacts"] if slot["kind"] == "worktree"
            )
            worktree["immutable"]["path"] = None
            worktree["primitive_request"] = {
                "component": "atrinik",
                "physical_checkout": "atrinik",
                "label": label,
                "repository": repository(),
                "branch": branch,
                "expected_head_sha": live_head,
                "roots": copy.deepcopy(roots),
            }
            return document

        document = deferred()
        self.assertEqual(ledger.prepare(document), document)
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        output = (str(live_worktree_path(request)) + "\n").encode()
        worktree_list = worktree_list_bytes(request)
        safety = safety_observation_bytes(
            request,
            worktree_list,
            producer_kind="primitive",
            producer_digest=ledger.byte_digest(output),
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            before = directory_snapshot(root)
            first = ledger.classify_worktree_output(
                initial.document, "worktree", worktree_list, safety, output
            )
            second = ledger.classify_worktree_output(
                initial.document, "worktree", worktree_list, safety, output
            )
            self.assertEqual(first, second)
            self.assertEqual(directory_snapshot(root), before)
            self.assertEqual(first["classification"], "bind-exact")
            self.assertEqual(first["path"], output.decode().strip())
            self.assertEqual(first["branch_artifact"]["state"], "created")
            self.assertEqual(first["worktree_artifact"]["state"], "created")
            candidate = next_generation(initial)
            candidate["artifacts"] = [
                first["branch_artifact"]
                if slot["kind"] == "branch"
                else first["worktree_artifact"]
                if slot["kind"] == "worktree"
                else slot
                for slot in candidate["artifacts"]
            ]
            branch_only = next_generation(initial)
            branch_only["artifacts"] = [
                first["branch_artifact"]
                if slot["kind"] == "branch"
                else slot
                for slot in branch_only["artifacts"]
            ]
            for projection in (branch_only, candidate):
                with self.assertRaisesRegex(
                    ledger.LedgerError, "purpose-specific atomic binder"
                ):
                    ledger.cas(
                        root, initial.name, projection, **cas_arguments(initial)
                    )
            self.assertEqual(directory_snapshot(root), before)
            result = ledger.bind_worktree_cas(
                root,
                initial.name,
                "worktree",
                worktree_list,
                safety,
                create_output_raw=output,
                **cas_arguments(initial),
            )
            self.assertEqual(result["snapshot"]["document"]["generation"], 2)
            bound = ledger.inspect(root, initial.name)
            recovered = ledger.classify_worktree_output(
                bound.document, "worktree", worktree_list, safety, output
            )
            self.assertEqual(recovered["classification"], "bound-match")
            for kind in ("branch", "worktree"):
                with self.subTest(unsafe_bound_primitive=kind):
                    unsafe = copy.deepcopy(bound.document)
                    next(
                        slot
                        for slot in unsafe["artifacts"]
                        if slot["kind"] == kind
                    )["safety"]["clean"] = False
                    with self.assertRaisesRegex(ledger.LedgerError, "safely reusable"):
                        ledger.classify_worktree_output(
                            unsafe, "worktree", worktree_list, safety, output
                        )

            immutable_change = next_generation(bound)
            next(
                slot
                for slot in immutable_change["artifacts"]
                if slot["kind"] == "worktree"
            )["primitive_request"]["label"] = "other"
            with self.assertRaisesRegex(ledger.LedgerError, "request|result|path"):
                ledger.cas(root, bound.name, immutable_change, **cas_arguments(bound))

            unsafe_refresh = next_generation(bound)
            next(
                slot
                for slot in unsafe_refresh["artifacts"]
                if slot["kind"] == "branch"
            )["safety"]["clean"] = False
            unsafe_snapshot = ledger.cas(
                root, bound.name, unsafe_refresh, **cas_arguments(bound)
            )
            safe_refresh = next_generation(unsafe_snapshot)
            next(
                slot
                for slot in safe_refresh["artifacts"]
                if slot["kind"] == "branch"
            )["safety"]["clean"] = True
            safe_snapshot = ledger.cas(
                root,
                unsafe_snapshot.name,
                safe_refresh,
                **cas_arguments(unsafe_snapshot),
            )
            self.assertTrue(
                next(
                    slot
                    for slot in safe_snapshot.document["artifacts"]
                    if slot["kind"] == "branch"
                )["safety"]["clean"]
            )

        missing = deferred()
        worktree = next(
            slot for slot in missing["artifacts"] if slot["kind"] == "worktree"
        )
        worktree["primitive_request"] = None
        with self.assertRaisesRegex(ledger.LedgerError, "worktree immutable"):
            ledger.prepare(missing)

        wrong_head = deferred()
        next(
            slot for slot in wrong_head["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]["expected_head_sha"] = SHA_B
        with self.assertRaisesRegex(ledger.LedgerError, "target initial head"):
            ledger.prepare(wrong_head)

        incoherent_adoption = pr_ledger(
            441,
            pr_node="P_incoherent",
            branch="Feature/Incoherent",
            worktree="/wrapper/workspace/worktrees/atrinik/incoherent",
        )
        next(
            slot
            for slot in incoherent_adoption["artifacts"]
            if slot["kind"] == "branch"
        ).update(state="planned", current=None, safety=None)
        bind_worktree(incoherent_adoption)
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ledger.LedgerError, "coherent"):
                ledger.create(Path(temporary), incoherent_adoption)

        for label, raw in (
            ("relative", b"relative/path\n"),
            ("root", b"/\n"),
            ("noncanonical", (str(live_worktree_path(request).parent / ".." / "pr-440") + "\n").encode()),
            ("wrong label", (str(live_worktree_path(request).parent / "other") + "\n").encode()),
            ("multiple lines", output + b"other\n"),
            ("unterminated", output.rstrip(b"\n")),
        ):
            raw_safety = safety_observation_bytes(
                request,
                worktree_list,
                producer_kind="primitive",
                producer_digest=ledger.byte_digest(raw),
            )
            with self.subTest(wrapper_output=label), self.assertRaises(ledger.LedgerError):
                ledger.classify_worktree_output(
                    document, "worktree", worktree_list, raw_safety, raw
                )

        listed = json.loads(worktree_list)
        for label, mutate in (
            ("missing", lambda rows: rows.pop()),
            ("duplicate", lambda rows: rows.append(copy.deepcopy(rows[-1]))),
            ("advanced head", lambda rows: rows[-1].update(HEAD=SHA_B)),
            (
                "detached",
                lambda rows: (rows[-1].pop("branch"), rows[-1].update(detached="")),
            ),
            ("locked", lambda rows: rows[-1].update(locked="delivery")),
            (
                "prunable",
                lambda rows: rows[-1].update(
                    prunable="gitdir file points to non-existent location"
                ),
            ),
        ):
            with self.subTest(worktree_list=label):
                changed = copy.deepcopy(listed)
                mutate(changed)
                changed_raw = (json.dumps(changed, sort_keys=True) + "\n").encode()
                changed_safety = safety_observation_bytes(
                    request,
                    changed_raw,
                    producer_kind="primitive",
                    producer_digest=ledger.byte_digest(output),
                )
                with self.assertRaises(ledger.LedgerError):
                    ledger.classify_worktree_output(
                        document, "worktree", changed_raw, changed_safety, output
                    )

        for field, unsafe_value in (
            ("clean", False),
            ("active", True),
            ("unowned_reference", True),
            ("foreign", True),
            ("certain", False),
        ):
            with self.subTest(unsafe_live_primitive=field):
                unsafe_state = copy.deepcopy(ledger.SAFE_ARTIFACT_STATE)
                unsafe_state[field] = unsafe_value
                unsafe_raw = safety_observation_bytes(
                    request,
                    worktree_list,
                    producer_kind="primitive",
                    producer_digest=ledger.byte_digest(output),
                    safety=unsafe_state,
                )
                with self.assertRaisesRegex(ledger.LedgerError, "safe live"):
                    ledger.classify_worktree_output(
                        document, "worktree", worktree_list, unsafe_raw, output
                    )

        missing_path = live_worktree_path(request)
        hidden_path = missing_path.with_name(missing_path.name + "-hidden")
        missing_path.rename(hidden_path)
        with self.assertRaisesRegex(ledger.LedgerError, "live no-follow directory"):
            ledger.classify_worktree_output(
                document, "worktree", worktree_list, safety, output
            )
        hidden_path.rename(missing_path)
        safety = safety_observation_bytes(
            request,
            worktree_list,
            producer_kind="primitive",
            producer_digest=ledger.byte_digest(output),
        )

        drifted = copy.deepcopy(document)
        drifted_request = next(
            slot for slot in drifted["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        drifted_request["roots"]["workspace"]["inode"] += 1
        drifted_list = worktree_list_bytes(drifted_request)
        drifted_safety = safety_observation_bytes(
            drifted_request,
            drifted_list,
            producer_kind="primitive",
            producer_digest=ledger.byte_digest(output),
        )
        with self.assertRaisesRegex(ledger.LedgerError, "identity drifted"):
            ledger.classify_worktree_output(
                drifted, "worktree", drifted_list, drifted_safety, output
            )

        exact = ledger.classify_worktree_output(
            document, "worktree", worktree_list, safety, output
        )
        crash_recovery = deferred(
            number=443,
            node="P_post_install_crash",
            branch="Feature/PostInstallCrash",
            label="post-install-crash",
        )
        crash_request = next(
            slot
            for slot in crash_recovery["artifacts"]
            if slot["kind"] == "worktree"
        )["primitive_request"]
        crash_list = worktree_list_bytes(crash_request)
        crash_safety = safety_observation_bytes(
            crash_request,
            crash_list,
            producer_kind="primitive",
            producer_digest=None,
        )
        recovered_without_stdout = ledger.classify_worktree_output(
            crash_recovery, "worktree", crash_list, crash_safety
        )
        self.assertEqual(recovered_without_stdout["classification"], "bind-exact")
        self.assertIsNone(
            recovered_without_stdout["worktree_artifact"]["primitive_result"]["create_output"]
        )
        for label, mutate in (
            (
                "hand-authored path",
                lambda slot: slot["current"].update(
                    path="/wrapper/workspace/worktrees/atrinik/other"
                ),
            ),
            (
                "hand-authored digest",
                lambda slot: slot["primitive_result"]["worktree_list"].update(
                    sha256="0" * 64
                ),
            ),
        ):
            with self.subTest(tamper=label):
                tampered = copy.deepcopy(document)
                branch_result = copy.deepcopy(exact["branch_artifact"])
                worktree_result = copy.deepcopy(exact["worktree_artifact"])
                mutate(worktree_result)
                tampered["artifacts"] = [
                    branch_result
                    if slot["kind"] == "branch"
                    else worktree_result
                    if slot["kind"] == "worktree"
                    else slot
                    for slot in tampered["artifacts"]
                ]
                with self.assertRaises(ledger.LedgerError):
                    ledger.prepare(tampered)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ledger.create(root, deferred())
            contender = deferred(
                number=442,
                node="P_deferred_two",
                branch="Feature/DeferredTwo",
            )
            with self.assertRaisesRegex(ledger.LedgerError, "worktree(?:-request)?"):
                ledger.create(root, contender)

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "reviews"
            root.mkdir()
            initial = ledger.create(root, document)
            capture = base / "worktree.out"
            capture.write_bytes(output)
            inventory_capture = base / "worktree-list.json"
            inventory_capture.write_bytes(worktree_list)
            safety_capture = base / "worktree-safety.json"
            safety_capture.write_bytes(safety)
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "worktree-bind",
                    str(root),
                    initial.name,
                    "worktree",
                    str(inventory_capture),
                    str(safety_capture),
                    "--create-output",
                    str(capture),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(process.stdout)["classification"], "bind-exact")

    def test_32_scope_show_request_result_binding_is_strict_and_cli_executable(self) -> None:
        client_repository = repository("client", "R_client")
        scope_roots = live_roots(self.live_base / "scope-32", "client")
        request = scope_request(
            component="client",
            checkout="client",
            start_sha=git_head(scope_roots),
            roots=scope_roots,
        )
        live_worktree_path(request)

        def scoped() -> dict[str, object]:
            document = issue_ledger()
            document["resources"] = [scope_resource(request)]
            worktree = next(
                slot for slot in document["artifacts"] if slot["kind"] == "worktree"
            )
            worktree["immutable"]["path"] = None
            worktree["primitive_request"] = None
            worktree["producer_resource_slot"] = "scope"
            retarget_repository(document, client_repository)
            replace_sha(document, SHA_A, request["start_sha"])
            return document

        def scope_safety(
            worktree_list_value: bytes,
            producer_digest: str,
            safety_value: dict[str, bool] | None = None,
        ) -> bytes:
            return safety_observation_bytes(
                request,
                worktree_list_value,
                producer_kind="scope",
                producer_digest=producer_digest,
                safety=safety_value,
                repository_value=client_repository,
            )

        raw = scope_show_bytes(request, repository_name="atrinik/client")
        install_scope_references(request, raw)
        worktree_list = worktree_list_bytes(request)
        safety = scope_safety(worktree_list, ledger.byte_digest(raw))
        document = scoped()
        exact = ledger.classify_scope_output(
            document, "scope", raw, worktree_list, safety
        )
        self.assertEqual(exact["classification"], "bind-exact")
        self.assertEqual(exact["result_sha256"], ledger.byte_digest(raw))
        self.assertEqual(
            exact["resource"]["current"]["identity_digest"], ledger.byte_digest(raw)
        )
        self.assertEqual(
            exact["resource"]["current"]["external_generation"], "1" * 32
        )

        decoded = json.loads(raw)
        mutations: list[tuple[str, callable]] = [
            ("extra row", lambda value: value["worktrees"].append(copy.deepcopy(value["worktrees"][0]))),
            ("wrong name", lambda value: value.update(name="other-scope")),
            ("bad generation", lambda value: value.update(generation="A" * 32)),
            ("request digest", lambda value: value.update(request_sha256="0" * 64)),
            ("branch", lambda value: value["worktrees"][0].update(branch="docs/other")),
            ("head", lambda value: value["worktrees"][0].update(commit=SHA_B)),
            ("path", lambda value: value["worktrees"][0].update(path="/other/path")),
            ("profile", lambda value: value["profile"].update(path="/other/profile.json")),
            ("topology", lambda value: value["topology"].update(path="/other/topology")),
            ("state", lambda value: value["state_policy"].update(mode="default")),
            ("command", lambda value: value["commands"].update(up="arbitrary")),
            (
                "log control",
                lambda value: value["commands"]["logs"].update(server="bad\ncommand"),
            ),
        ]
        for label, mutate in mutations:
            with self.subTest(scope_show=label):
                candidate = copy.deepcopy(decoded)
                mutate(candidate)
                candidate_raw = (json.dumps(candidate, sort_keys=True) + "\n").encode()
                candidate_safety = scope_safety(
                    worktree_list, ledger.byte_digest(candidate_raw)
                )
                with self.assertRaises(ledger.LedgerError):
                    ledger.classify_scope_output(
                        document,
                        "scope",
                        candidate_raw,
                        worktree_list,
                        candidate_safety,
                    )

        changed_request = copy.deepcopy(document)
        changed_request["resources"][0]["request"]["profile"] = "classic"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            changed = next_generation(initial)
            changed["resources"][0]["request"]["profile"] = "classic"
            with self.assertRaisesRegex(ledger.LedgerError, "resource request"):
                ledger.cas(root, initial.name, changed, **cas_arguments(initial))

            candidate = next_generation(initial)
            candidate["resources"] = [copy.deepcopy(exact["resource"])]
            candidate["artifacts"] = [
                copy.deepcopy(exact["branch_artifact"])
                if slot["kind"] == "branch"
                else copy.deepcopy(exact["worktree_artifact"])
                if slot["kind"] == "worktree"
                else slot
                for slot in candidate["artifacts"]
            ]
            arbitrary = copy.deepcopy(candidate)
            arbitrary["resources"][0]["current"]["identity_digest"] = "0" * 64
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "retained result"):
                ledger.cas(root, initial.name, arbitrary, **cas_arguments(initial))
            self.assertEqual(directory_snapshot(root), before)
            with self.assertRaisesRegex(
                ledger.LedgerError, "purpose-specific atomic binder"
            ):
                ledger.cas(root, initial.name, candidate, **cas_arguments(initial))
            bound_result = ledger.bind_scope_cas(
                root,
                initial.name,
                "scope",
                raw,
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            self.assertEqual(bound_result["snapshot"]["document"]["generation"], 2)
            bound = ledger.inspect(root, initial.name)
            self.assertEqual(
                ledger.classify_scope_output(
                    bound.document, "scope", raw, worktree_list, safety
                )["classification"],
                "bound-match",
            )
            for kind in ("branch", "worktree"):
                with self.subTest(unsafe_bound_scope=kind):
                    unsafe = copy.deepcopy(bound.document)
                    next(
                        slot
                        for slot in unsafe["artifacts"]
                        if slot["kind"] == kind
                    )["safety"]["clean"] = False
                    with self.assertRaisesRegex(ledger.LedgerError, "safely reusable"):
                        ledger.classify_scope_output(
                            unsafe, "scope", raw, worktree_list, safety
                        )
            for field, unsafe_value in (
                ("clean", False),
                ("detached", True),
                ("locked", True),
                ("active", True),
                ("unowned_reference", True),
                ("foreign", True),
                ("certain", False),
            ):
                with self.subTest(unsafe_live_scope=field):
                    unsafe_safety = copy.deepcopy(ledger.SAFE_ARTIFACT_STATE)
                    unsafe_safety[field] = unsafe_value
                    unsafe_raw = scope_safety(
                        worktree_list,
                        ledger.byte_digest(raw),
                        unsafe_safety,
                    )
                    with self.assertRaisesRegex(ledger.LedgerError, "safe live"):
                        ledger.classify_scope_output(
                            document, "scope", raw, worktree_list, unsafe_raw
                        )
            with self.assertRaisesRegex(ledger.LedgerError, "differs"):
                changed_scope = scope_show_bytes(
                    request,
                    generation="2" * 32,
                    repository_name="atrinik/client",
                )
                changed_safety = scope_safety(
                    worktree_list, ledger.byte_digest(changed_scope)
                )
                ledger.classify_scope_output(
                    bound.document,
                    "scope",
                    changed_scope,
                    worktree_list,
                    changed_safety,
                )
            released_document = next_generation(bound)
            released_document["resources"][0]["current"].update(
                generation=2,
                identity_digest="3" * 64,
                history=[exact["result_sha256"]],
                lifecycle="released",
            )
            released = ledger.cas(
                root, bound.name, released_document, **cas_arguments(bound)
            )
            with self.assertRaisesRegex(ledger.LedgerError, "released"):
                ledger.classify_scope_output(
                    released.document, "scope", raw, worktree_list, safety
                )

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "reviews"
            root.mkdir()
            initial = ledger.create(root, document)
            capture = base / "scope.json"
            capture.write_bytes(raw)
            inventory_capture = base / "worktree-list.json"
            inventory_capture.write_bytes(worktree_list)
            safety_capture = base / "scope-safety.json"
            safety_capture.write_bytes(safety)
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "scope-bind",
                    str(root),
                    initial.name,
                    "scope",
                    str(capture),
                    str(inventory_capture),
                    str(safety_capture),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            result = json.loads(process.stdout)
            self.assertEqual(result["classification"], "bind-exact")
            self.assertEqual(result["result_sha256"], ledger.byte_digest(raw))

    def test_33_contributor_section_cli_and_timestamp_matrix(self) -> None:
        contributor_raw = b"Contributor prose ending in LF\n"
        contributor_digest = ledger.byte_digest(contributor_raw)
        document = pr_ledger(
            440,
            pr_node="P_section",
            branch="Feature/Section",
            worktree="/workspace/worktrees/section",
        )
        body = document["selected_prs"][0]["body"]
        body.update(
            observed_digest=contributor_digest,
            current_digest=contributor_digest,
            outside_digest=contributor_digest,
        )
        slot = next(
            value for value in document["artifacts"] if value["kind"] == "pull_request"
        )
        slot["immutable"]["body_digest"] = contributor_digest
        slot["current"]["body_digest"] = contributor_digest

        equivalent = ledger.classify_body_recovery(
            document,
            "P_section",
            contributor_digest,
            "2026-08-14T18:00:00.0Z",
        )
        self.assertEqual(equivalent["action"], "read-only-match")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            spelling = next_generation(initial)
            spelling["selected_prs"][0]["body"]["updated_at"] = (
                "2026-08-14T18:00:00.0Z"
            )
            equivalent_snapshot = ledger.cas(
                root, initial.name, spelling, **cas_arguments(initial)
            )
            rollback = next_generation(equivalent_snapshot)
            rollback["selected_prs"][0]["body"]["updated_at"] = (
                "2026-08-14T17:59:59.999999999Z"
            )
            with self.assertRaisesRegex(ledger.LedgerError, "timestamp|body transition"):
                ledger.cas(
                    root,
                    equivalent_snapshot.name,
                    rollback,
                    **cas_arguments(equivalent_snapshot),
                )
            later_spelling = next_generation(equivalent_snapshot)
            later_spelling["selected_prs"][0]["body"]["updated_at"] = (
                "2026-08-14T18:00:00.1Z"
            )
            advanced = ledger.cas(
                root,
                equivalent_snapshot.name,
                later_spelling,
                **cas_arguments(equivalent_snapshot),
            )
            self.assertEqual(
                advanced.document["selected_prs"][0]["body"]["updated_at"],
                "2026-08-14T18:00:00.1Z",
            )

        changed_digest = ledger.byte_digest(b"contributor changed")
        refresh = ledger.classify_body_recovery(
            document,
            "P_section",
            changed_digest,
            "2026-08-14T18:00:01Z",
        )
        self.assertEqual(refresh["action"], "refresh-observation")
        self.assertEqual(refresh["cas_body"]["outside_digest"], changed_digest)
        ledger._body_transition(
            body, refresh["cas_body"], document, document["selected_prs"][0]
        )
        with self.assertRaisesRegex(ledger.LedgerError, "neither"):
            ledger.classify_body_recovery(
                document,
                "P_section",
                changed_digest,
                "2026-08-14T18:00:00Z",
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            body_file = root / "current-body.bin"
            section_file = root / "section.md"
            body_file.write_bytes(contributor_raw)
            section_file.write_bytes(b"Delivery status")
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "body-plan",
                    str(root),
                    initial.name,
                    "P_section",
                    str(body_file),
                    str(section_file),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            projection = json.loads(process.stdout)
            intended_raw = base64.b64decode(projection["body_base64"])
            self.assertTrue(intended_raw.startswith(contributor_raw + b"\n<!--"))
            self.assertEqual(projection["outside_digest"], contributor_digest)
            self.assertEqual(projection["body"]["ownership"], "delivery-section")
            self.assertEqual(
                projection["body"]["intended_payload"]["raw_base64"],
                projection["body_base64"],
            )

            planned_document = next_generation(initial)
            planned_document["selected_prs"][0]["body"] = projection["body"]
            planned = ledger.cas(
                root, initial.name, planned_document, **cas_arguments(initial)
            )

            later = ledger.classify_body_recovery(
                planned.document,
                "P_section",
                contributor_digest,
                "2026-08-14T18:00:01Z",
            )
            self.assertEqual(later["action"], "refresh-intent-observation")
            ledger._body_transition(
                planned.document["selected_prs"][0]["body"],
                later["cas_body"],
                planned.document,
                planned.document["selected_prs"][0],
            )
            equal_bind = ledger.classify_body_recovery(
                planned.document,
                "P_section",
                projection["body_digest"],
                "2026-08-14T18:00:00Z",
            )
            self.assertEqual(equal_bind["action"], "bind-intended")
            bound_document = next_generation(planned)
            bound_document["selected_prs"][0]["body"] = equal_bind["cas_body"]
            next(
                value
                for value in bound_document["artifacts"]
                if value["kind"] == "pull_request"
            )["current"]["body_digest"] = projection["body_digest"]
            bound = ledger.cas(
                root, planned.name, bound_document, **cas_arguments(planned)
            )
            written_match = ledger.classify_body_recovery(
                bound.document,
                "P_section",
                projection["body_digest"],
                "2026-08-14T18:00:00Z",
            )
            self.assertEqual(written_match["action"], "none")
            written_refresh = ledger.classify_body_recovery(
                bound.document,
                "P_section",
                projection["body_digest"],
                "2026-08-14T18:00:02Z",
            )
            self.assertEqual(written_refresh["action"], "refresh-observation")
            ledger._body_transition(
                bound.document["selected_prs"][0]["body"],
                written_refresh["cas_body"],
                bound.document,
                bound.document["selected_prs"][0],
            )
            with self.assertRaisesRegex(ledger.LedgerError, "neither"):
                ledger.classify_body_recovery(
                    bound.document,
                    "P_section",
                    "0" * 64,
                    "2026-08-14T18:00:03Z",
                )
            with self.assertRaisesRegex(ledger.LedgerError, "terminal"):
                ledger.check_body_section(
                    bound.document, "P_section", intended_raw + b"suffix"
                )

        cancellation = ledger.describe_body_plan(
            document, "P_section", contributor_raw, b"cancel me"
        )
        before_cancel = cancellation["body"]
        after_cancel = copy.deepcopy(body)
        ledger._body_transition(
            before_cancel, after_cancel, document, document["selected_prs"][0]
        )
        invalid_cancel = copy.deepcopy(before_cancel)
        invalid_cancel.update(
            state="written", intended_digest=None, intended_payload=None
        )
        with self.assertRaises(ledger.LedgerError):
            ledger._body_transition(
                before_cancel, invalid_cancel, document, document["selected_prs"][0]
            )

        empty = copy.deepcopy(document)
        empty_digest = ledger.byte_digest(b"")
        empty["selected_prs"][0]["body"].update(
            observed_digest=empty_digest,
            current_digest=empty_digest,
            outside_digest=empty_digest,
        )
        empty_slot = next(
            value for value in empty["artifacts"] if value["kind"] == "pull_request"
        )
        empty_slot["immutable"]["body_digest"] = empty_digest
        empty_slot["current"]["body_digest"] = empty_digest
        empty_plan = ledger.describe_body_plan(empty, "P_section", b"", b"section")
        self.assertTrue(base64.b64decode(empty_plan["body_base64"]).startswith(b"<!--"))

    def test_34_inline_payload_body_and_comment_adversaries(self) -> None:
        raw = b"outside bytes"
        digest = ledger.byte_digest(raw)
        document = pr_ledger(
            441,
            pr_node="P_payload",
            branch="Feature/Payload",
            worktree="/workspace/worktrees/payload",
        )
        document["selected_prs"][0]["body"].update(
            observed_digest=digest, current_digest=digest, outside_digest=digest
        )
        slot = next(
            value for value in document["artifacts"] if value["kind"] == "pull_request"
        )
        slot["immutable"]["body_digest"] = digest
        slot["current"]["body_digest"] = digest
        projection = ledger.describe_body_plan(
            document, "P_payload", raw, b"payload section"
        )
        planned = copy.deepcopy(document)
        planned["selected_prs"][0]["body"] = projection["body"]
        ledger.prepare(planned)

        malformed = copy.deepcopy(planned)
        malformed["selected_prs"][0]["body"]["intended_payload"]["raw_base64"] = "***"
        with self.assertRaisesRegex(ledger.LedgerError, "base64"):
            ledger.prepare(malformed)
        noncanonical = copy.deepcopy(planned)
        noncanonical["selected_prs"][0]["body"]["intended_payload"] = {
            "encoding": "base64",
            "raw_base64": "Zh==",
            "sha256": ledger.byte_digest(b"f"),
        }
        noncanonical["selected_prs"][0]["body"]["intended_digest"] = (
            ledger.byte_digest(b"f")
        )
        with self.assertRaisesRegex(ledger.LedgerError, "noncanonical"):
            ledger.prepare(noncanonical)
        wrong_digest = copy.deepcopy(planned)
        wrong_digest["selected_prs"][0]["body"]["intended_payload"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ledger.LedgerError, "digest"):
            ledger.prepare(wrong_digest)
        invalid_utf8 = copy.deepcopy(planned)
        invalid_utf8["selected_prs"][0]["body"]["intended_payload"] = inline_payload(b"\xff")
        invalid_utf8["selected_prs"][0]["body"]["intended_digest"] = ledger.byte_digest(b"\xff")
        with self.assertRaisesRegex(ledger.LedgerError, "UTF-8"):
            ledger.prepare(invalid_utf8)
        with self.assertRaisesRegex(ledger.LedgerError, "exceeds"):
            ledger._inline_payload(
                inline_payload(b"x" * (ledger.MAX_RETAINED_RESULT_BYTES + 1)),
                "oversized",
            )

        intended_raw = base64.b64decode(projection["body_base64"])
        for label, changed in (
            ("outside", b"X" + intended_raw[1:]),
            ("suffix", intended_raw + b"suffix"),
            ("duplicate", intended_raw + intended_raw),
        ):
            with self.subTest(body_payload=label):
                candidate = copy.deepcopy(planned)
                candidate_body = candidate["selected_prs"][0]["body"]
                candidate_body["intended_payload"] = inline_payload(changed)
                candidate_body["intended_digest"] = ledger.byte_digest(changed)
                with self.assertRaises(ledger.LedgerError):
                    ledger.prepare(candidate)

        marker = (
            f"<!-- {ledger.delivery_surface_marker(document, 'P_payload', 'comment')} -->"
        )
        comment_raw = (marker + "\nstatus").encode("utf-8")
        comment = {
            "state": "planned",
            "marker": marker,
            "intended_digest": ledger.byte_digest(comment_raw),
            "intended_payload": inline_payload(comment_raw),
            "node_id": None,
            "current_digest": None,
        }
        comment_document = copy.deepcopy(document)
        comment_document["selected_prs"][0]["comment"] = comment
        ledger.prepare(comment_document)
        classified = ledger.classify_comments(
            comment_document,
            "P_payload",
            {"pagination_complete": True, "comments": []},
        )
        self.assertEqual(classified["intended_payload"], inline_payload(comment_raw))
        for label, mutate in (
            (
                "digest-only",
                lambda value: value["selected_prs"][0]["comment"].__setitem__(
                    "intended_digest", "0" * 64
                ),
            ),
            (
                "payload-only",
                lambda value: value["selected_prs"][0]["comment"].__setitem__(
                    "intended_payload", inline_payload(comment_raw + b" changed")
                ),
            ),
            (
                "not-first",
                lambda value: value["selected_prs"][0]["comment"].update(
                    intended_payload=inline_payload(b"prefix\n" + comment_raw),
                    intended_digest=ledger.byte_digest(b"prefix\n" + comment_raw),
                ),
            ),
            (
                "extra-marker",
                lambda value: value["selected_prs"][0]["comment"].update(
                    intended_payload=inline_payload(
                        comment_raw + b"\natrinik-delivery:comment:foreign"
                    ),
                    intended_digest=ledger.byte_digest(
                        comment_raw + b"\natrinik-delivery:comment:foreign"
                    ),
                ),
            ),
        ):
            with self.subTest(comment_payload=label):
                candidate = copy.deepcopy(comment_document)
                mutate(candidate)
                with self.assertRaises(ledger.LedgerError):
                    ledger.prepare(candidate)

        bound = {
            "state": "bound",
            "marker": marker,
            "intended_digest": None,
            "intended_payload": None,
            "node_id": "C_payload",
            "current_digest": "1" * 64,
        }
        update = {
            **bound,
            "state": "planned",
            "intended_digest": ledger.byte_digest(comment_raw),
            "intended_payload": inline_payload(comment_raw),
        }
        ledger._comment_transition(bound, update)
        ledger._comment_transition(update, bound)
        uncleared = {**bound, "intended_payload": update["intended_payload"]}
        with self.assertRaises(ledger.LedgerError):
            ledger._comment_transition(update, uncleared)

    def test_35_initial_pr_payload_is_durable_immutable_and_cli_visible(self) -> None:
        document = issue_ledger()
        slot = next(
            value for value in document["artifacts"] if value["kind"] == "pull_request"
        )
        expected = copy.deepcopy(slot["initial_body_payload"])
        self.assertEqual(
            ledger.planned_pr_payload(document, "pull-request"), INITIAL_PR_BODY
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            created = ledger.create(root, document)
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "pr-create-payload",
                    str(root),
                    created.name,
                    "pull-request",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            result = json.loads(process.stdout)
            self.assertEqual(result["body_payload"], expected)
            self.assertEqual(
                base64.b64decode(result["body_payload"]["raw_base64"]),
                INITIAL_PR_BODY,
            )

            changed_raw = b"different initial body"
            changed = next_generation(created)
            changed_slot = next(
                value
                for value in changed["artifacts"]
                if value["kind"] == "pull_request"
            )
            changed_slot["initial_body_payload"] = inline_payload(changed_raw)
            changed_slot["immutable"]["body_digest"] = ledger.byte_digest(changed_raw)
            with self.assertRaisesRegex(ledger.LedgerError, "immutable|initial body"):
                ledger.cas(root, created.name, changed, **cas_arguments(created))

        reserved = issue_ledger()
        reserved_slot = next(
            value for value in reserved["artifacts"] if value["kind"] == "pull_request"
        )
        reserved_raw = b"atrinik-delivery reserved"
        reserved_slot["initial_body_payload"] = inline_payload(reserved_raw)
        reserved_slot["immutable"]["body_digest"] = ledger.byte_digest(reserved_raw)
        with self.assertRaisesRegex(ledger.LedgerError, "reserved"):
            ledger.prepare(reserved)

        adopted = pr_ledger(
            442,
            pr_node="P_adopted_payload",
            branch="Feature/AdoptedPayload",
            worktree="/workspace/worktrees/adopted-payload",
        )
        adopted_slot = next(
            value for value in adopted["artifacts"] if value["kind"] == "pull_request"
        )
        adopted_slot["initial_body_payload"] = inline_payload(INITIAL_PR_BODY)
        adopted_slot["immutable"]["body_digest"] = ledger.byte_digest(INITIAL_PR_BODY)
        adopted_slot["current"]["body_digest"] = ledger.byte_digest(INITIAL_PR_BODY)
        adopted["selected_prs"][0]["body"].update(
            observed_digest=ledger.byte_digest(INITIAL_PR_BODY),
            current_digest=ledger.byte_digest(INITIAL_PR_BODY),
            outside_digest=ledger.byte_digest(INITIAL_PR_BODY),
        )
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ledger.LedgerError, "creation payload"):
                ledger.create(temporary, adopted)

    def test_36_live_observation_atomic_bind_recovery_and_precommit_cleanup(self) -> None:
        roots = live_roots(self.live_base / "atomic-primitive", "atrinik")
        document = deferred_primitive_pr(roots)
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        path = live_worktree_path(request)
        worktree_list = worktree_list_bytes(request)
        listed = json.loads(worktree_list)
        self.assertEqual(listed[-1]["component"], "atrinik")
        self.assertEqual(listed[-1]["worktree"], str(path))
        observation = ledger.observe_primitive_worktree(
            document,
            "worktree",
            worktree_list,
            "2026-08-14T18:02:00Z",
        )
        safety = json_bytes(observation)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            bound_result = ledger.bind_worktree_cas(
                root,
                initial.name,
                "worktree",
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            bound = ledger.inspect(root, initial.name)
            self.assertEqual(bound_result["snapshot"]["document"]["generation"], 2)
            self.assertEqual(bound.document["generation"], 2)
            self.assertEqual(
                next(
                    slot
                    for slot in bound.document["artifacts"]
                    if slot["kind"] == "worktree"
                )["current"]["path"],
                str(path),
            )
            repeated = ledger.bind_worktree_cas(
                root,
                initial.name,
                "worktree",
                worktree_list,
                safety,
                **cas_arguments(bound),
            )
            self.assertEqual(repeated["classification"], "bound-match")
            self.assertEqual(ledger.inspect(root, initial.name).digest, bound.digest)

            advanced_outcome: list[object] = []

            def advance_bound_snapshot(point: str) -> None:
                if point != "worktree-bind:classified":
                    return

                def advance() -> None:
                    try:
                        advanced_outcome.append(
                            ledger.cas(
                                root,
                                bound.name,
                                next_generation(bound),
                                **cas_arguments(bound),
                            )
                        )
                    except BaseException as error:
                        advanced_outcome.append(error)

                worker = threading.Thread(target=advance)
                worker.start()
                worker.join(timeout=10)
                self.assertFalse(worker.is_alive())

            with self.assertRaisesRegex(
                ledger.LedgerError, "bound-match snapshot changed"
            ):
                ledger.bind_worktree_cas(
                    root,
                    bound.name,
                    "worktree",
                    worktree_list,
                    safety,
                    failpoint=advance_bound_snapshot,
                    **cas_arguments(bound),
                )
            self.assertEqual(len(advanced_outcome), 1)
            self.assertIsInstance(advanced_outcome[0], ledger.Snapshot)
            self.assertEqual(
                ledger.inspect(root, bound.name).document["generation"], 3
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.bind_worktree_cas(
                    root,
                    initial.name,
                    "worktree",
                    worktree_list,
                    safety,
                    failpoint="cas:renamed",
                    **cas_arguments(initial),
                )
            installed = ledger.inspect(root, initial.name)
            self.assertEqual(installed.document["generation"], 2)
            with self.assertRaisesRegex(
                ledger.LedgerError, "pending predecessor receipt"
            ):
                ledger.bind_worktree_cas(
                    root,
                    initial.name,
                    "worktree",
                    worktree_list,
                    safety,
                    **cas_arguments(installed),
                )
            self.assertNotEqual(ledger.inventory(root).pending, ())
            recovered = ledger.bind_worktree_cas(
                root,
                initial.name,
                "worktree",
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            self.assertEqual(recovered["snapshot"]["document"]["generation"], 2)
            self.assertEqual(ledger.inventory(root).pending, ())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            barrier = threading.Barrier(2)
            outcomes: list[tuple[str, object]] = []

            def concurrent_failpoint(point: str) -> None:
                if point == "worktree-bind:classified":
                    barrier.wait(timeout=5)

            def bind_concurrently() -> None:
                try:
                    result = ledger.bind_worktree_cas(
                        root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        safety,
                        failpoint=concurrent_failpoint,
                        **cas_arguments(initial),
                    )
                except ledger.LedgerError as error:
                    outcomes.append(("rejected", str(error)))
                else:
                    outcomes.append(("installed", result))

            workers = [threading.Thread(target=bind_concurrently) for _ in range(2)]
            for worker in workers:
                worker.start()
            for worker in workers:
                worker.join(timeout=10)
                self.assertFalse(worker.is_alive())
            self.assertEqual(sorted(outcome[0] for outcome in outcomes), ["installed", "rejected"])
            self.assertEqual(ledger.inspect(root, initial.name).document["generation"], 2)
            self.assertEqual(ledger.inventory(root).pending, ())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            before = directory_snapshot(root)
            hidden = path.with_name(path.name + "-old")

            def replace_path(point: str) -> None:
                if point == "cas:proofed":
                    path.rename(hidden)
                    path.mkdir(mode=0o700)

            try:
                with self.assertRaisesRegex(ledger.LedgerError, "identity drifted"):
                    ledger.bind_worktree_cas(
                        root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        safety,
                        failpoint=replace_path,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
                self.assertEqual(ledger.inspect(root, initial.name).digest, initial.digest)
            finally:
                if path.is_dir():
                    path.rmdir()
                if hidden.exists():
                    hidden.rename(path)

            common = Path(
                git_run(Path(roots["wrapper"]["path"]), "rev-parse", "--git-common-dir")
                .stdout.strip()
            )
            if not common.is_absolute():
                common = Path(roots["wrapper"]["path"]) / common
            registry = common / "atrinik-resource-leases" / "profile-references"
            registry.mkdir(parents=True, mode=0o700, exist_ok=True)
            registry.parent.chmod(0o700)
            registry.chmod(0o700)
            intruder = registry / ("2" * 64 + ".json")

            def add_reference(point: str) -> None:
                if point == "cas:proofed":
                    intruder.write_text(
                        json.dumps(
                            {
                                "schema_version": 1,
                                "kind": "profiles",
                                "reference": "intruder",
                                "sources": [str(path)],
                            },
                            sort_keys=True,
                        )
                        + "\n",
                        encoding="utf-8",
                    )

            try:
                with self.assertRaisesRegex(ledger.LedgerError, "reference set differs"):
                    ledger.bind_worktree_cas(
                        root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        safety,
                        failpoint=add_reference,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
            finally:
                intruder.unlink(missing_ok=True)

            def redirect_push(point: str) -> None:
                if point == "cas:proofed":
                    git_run(
                        Path(roots["primary"]["path"]),
                        "config",
                        "remote.origin.pushurl",
                        "https://github.com/foreign/other.git",
                    )

            try:
                with self.assertRaisesRegex(ledger.LedgerError, "foreign"):
                    ledger.bind_worktree_cas(
                        root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        safety,
                        failpoint=redirect_push,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
            finally:
                git_run(
                    Path(roots["primary"]["path"]),
                    "config",
                    "--unset-all",
                    "remote.origin.pushurl",
                    check=False,
                )

            manifest = Path(roots["wrapper"]["path"]) / "components.json"
            manifest_mode = manifest.stat().st_mode & 0o777

            def weaken_manifest(point: str) -> None:
                if point == "cas:proofed":
                    manifest.chmod(0o666)

            try:
                with self.assertRaisesRegex(
                    ledger.LedgerError, "manifest authority.*group/world writable"
                ):
                    ledger.bind_worktree_cas(
                        root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        safety,
                        failpoint=weaken_manifest,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
            finally:
                manifest.chmod(manifest_mode)

            common = Path(
                git_run(
                    Path(roots["primary"]["path"]),
                    "rev-parse",
                    "--path-format=absolute",
                    "--git-common-dir",
                ).stdout.strip()
            )
            config = common / "config"
            config_mode = config.stat().st_mode & 0o777

            def weaken_git_config(point: str) -> None:
                if point == "cas:proofed":
                    config.chmod(0o666)

            try:
                with self.assertRaisesRegex(
                    ledger.LedgerError, "local config.*group/world writable"
                ):
                    ledger.bind_worktree_cas(
                        root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        safety,
                        failpoint=weaken_git_config,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
            finally:
                config.chmod(config_mode)

    def test_37_scope_observation_and_atomic_bind_use_live_scope_authority(self) -> None:
        client_repository = repository("client", "R_client")
        roots = live_roots(self.live_base / "atomic-scope", "client")
        request = scope_request(
            component="client",
            checkout="client",
            start_sha=git_head(roots),
            roots=roots,
        )
        live_worktree_path(request)
        document = issue_ledger()
        document["resources"] = [scope_resource(request)]
        worktree = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )
        worktree["immutable"]["path"] = None
        worktree["primitive_request"] = None
        worktree["producer_resource_slot"] = "scope"
        retarget_repository(document, client_repository)
        replace_sha(document, SHA_A, request["start_sha"])
        scope_show = scope_show_bytes(request, repository_name="atrinik/client")
        install_scope_references(request, scope_show)
        worktree_list = worktree_list_bytes(request)
        manifest = Manifest.load(Path(roots["wrapper"]["path"]) / "components.json")
        components = {
            component.name: {"kind": "primary", "value": ""}
            for component in manifest.stack("default").components
        }
        components.pop("resources")
        profile_path = (
            Path(roots["workspace"]["path"])
            / "profiles"
            / "stale-scope-unrelated.json"
        )
        profile_path.parent.mkdir(parents=True, exist_ok=True)
        profile_path.write_bytes(
            json_bytes(
                {
                    "schema_version": 5,
                    "name": "stale-scope-unrelated",
                    "stack": "default",
                    "sound_mode": "source",
                    "sound_release": None,
                    "components": components,
                }
            )
        )
        profile_before = profile_path.read_bytes()
        observation = ledger.observe_scope_worktree(
            document,
            "scope",
            scope_show,
            worktree_list,
            "2026-08-14T18:03:00Z",
        )
        self.assertEqual(profile_path.read_bytes(), profile_before)
        safety = json_bytes(observation)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            result = ledger.bind_scope_cas(
                root,
                initial.name,
                "scope",
                scope_show,
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            self.assertEqual(result["snapshot"]["document"]["generation"], 2)
            bound = ledger.inspect(root, initial.name)
            resource = next(
                value for value in bound.document["resources"] if value["kind"] == "scope"
            )
            self.assertEqual(resource["current"]["external_generation"], "1" * 32)
            self.assertEqual(profile_path.read_bytes(), profile_before)

            advanced_outcome: list[object] = []

            def advance_bound_snapshot(point: str) -> None:
                if point != "scope-bind:classified":
                    return

                def advance() -> None:
                    try:
                        advanced_outcome.append(
                            ledger.cas(
                                root,
                                bound.name,
                                next_generation(bound),
                                **cas_arguments(bound),
                            )
                        )
                    except BaseException as error:
                        advanced_outcome.append(error)

                worker = threading.Thread(target=advance)
                worker.start()
                worker.join(timeout=10)
                self.assertFalse(worker.is_alive())

            with self.assertRaisesRegex(
                ledger.LedgerError, "bound-match snapshot changed"
            ):
                ledger.bind_scope_cas(
                    root,
                    bound.name,
                    "scope",
                    scope_show,
                    worktree_list,
                    safety,
                    failpoint=advance_bound_snapshot,
                    **cas_arguments(bound),
                )
            self.assertEqual(len(advanced_outcome), 1)
            self.assertIsInstance(advanced_outcome[0], ledger.Snapshot)
            self.assertEqual(
                ledger.inspect(root, bound.name).document["generation"], 3
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.bind_scope_cas(
                    root,
                    initial.name,
                    "scope",
                    scope_show,
                    worktree_list,
                    safety,
                    failpoint="cas:renamed",
                    **cas_arguments(initial),
                )
            installed = ledger.inspect(root, initial.name)
            with self.assertRaisesRegex(
                ledger.LedgerError, "pending predecessor receipt"
            ):
                ledger.bind_scope_cas(
                    root,
                    initial.name,
                    "scope",
                    scope_show,
                    worktree_list,
                    safety,
                    **cas_arguments(installed),
                )
            self.assertNotEqual(ledger.inventory(root).pending, ())
            recovered = ledger.bind_scope_cas(
                root,
                initial.name,
                "scope",
                scope_show,
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            self.assertEqual(recovered["snapshot"]["document"]["generation"], 2)
            self.assertEqual(ledger.inventory(root).pending, ())

        topology = Path(roots["workspace"]["path"]) / "topologies" / request["topology"]
        topology.mkdir(parents=True)
        (topology / "spec.json").write_text(
            json.dumps(
                {
                    "resolved": {
                        "client": {
                            "checkout_path": str(live_worktree_path(request))
                        }
                    }
                }
            )
            + "\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ledger.LedgerError, "reference set differs"):
            ledger.observe_scope_worktree(
                document,
                "scope",
                scope_show,
                worktree_list,
                "2026-08-14T18:03:01Z",
            )
        (topology / "spec.json").unlink()
        topology.rmdir()

        release = (
            Path(roots["workspace"]["path"])
            / "scopes"
            / request["name"]
            / "release-journal.json"
        )
        release.write_text("{}\n", encoding="utf-8")
        with self.assertRaisesRegex(ledger.LedgerError, "release has started"):
            ledger.observe_scope_worktree(
                document,
                "scope",
                scope_show,
                worktree_list,
                "2026-08-14T18:03:02Z",
            )
        release.unlink()

        scope_root = (
            Path(roots["workspace"]["path"])
            / "scopes"
            / request["name"]
        )
        scope_directory_mode = scope_root.stat().st_mode & 0o777
        scope_root.chmod(0o777)
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "scope directory.*group/world writable"
            ):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:02.1Z",
                )
        finally:
            scope_root.chmod(scope_directory_mode)

        scopes_root = scope_root.parent
        scopes_root_mode = scopes_root.stat().st_mode & 0o777
        scopes_root.chmod(0o777)
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "scopes root.*group/world writable"
            ):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:02.15Z",
                )
        finally:
            scopes_root.chmod(scopes_root_mode)

        scope_file = scope_root / "scope.json"
        scope_file_mode = scope_file.stat().st_mode & 0o777
        scope_file.chmod(0o666)
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "authoritative scope.json.*group/world writable"
            ):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:02.2Z",
                )
        finally:
            scope_file.chmod(scope_file_mode)

        primary = Path(roots["primary"]["path"])
        git_run(
            primary,
            "config",
            "remote.origin.pushurl",
            "https://github.com/foreign/other.git",
        )
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "foreign"):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:03Z",
                )
        finally:
            git_run(
                primary,
                "config",
                "--unset-all",
                "remote.origin.pushurl",
                check=False,
            )

        profile_path = Path(json.loads(scope_show)["profile"]["path"])
        profile_raw = profile_path.read_bytes()
        profiles_root = profile_path.parent
        profiles_root_mode = profiles_root.stat().st_mode & 0o777
        profiles_root.chmod(0o777)
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "profiles root.*group/world writable"
            ):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:03.1Z",
                )
        finally:
            profiles_root.chmod(profiles_root_mode)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            before = directory_snapshot(root)

            def change_profile(point: str) -> None:
                if point == "cas:proofed":
                    profile_path.write_bytes(b"changed profile\n")

            try:
                with self.assertRaisesRegex(ledger.LedgerError, "profile differs"):
                    ledger.bind_scope_cas(
                        root,
                        initial.name,
                        "scope",
                        scope_show,
                        worktree_list,
                        safety,
                        failpoint=change_profile,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
            finally:
                profile_path.write_bytes(profile_raw)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            before = directory_snapshot(root)

            def weaken_scope_record(point: str) -> None:
                if point == "cas:proofed":
                    scope_file.chmod(0o666)

            try:
                with self.assertRaisesRegex(
                    ledger.LedgerError,
                    "authoritative scope.json.*group/world writable",
                ):
                    ledger.bind_scope_cas(
                        root,
                        initial.name,
                        "scope",
                        scope_show,
                        worktree_list,
                        safety,
                        failpoint=weaken_scope_record,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(root), before)
                self.assertEqual(ledger.inventory(root).pending, ())
            finally:
                scope_file.chmod(scope_file_mode)

        profile_path.write_bytes(b"changed profile\n")
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "profile differs"):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:04Z",
                )
        finally:
            profile_path.write_bytes(profile_raw)

        original_mode = profile_path.stat().st_mode & 0o777
        profile_path.chmod(0o777)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "group/world writable"):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:05Z",
                )
        finally:
            profile_path.chmod(original_mode)

        hidden_profile = profile_path.with_name(profile_path.name + ".hidden")
        profile_path.rename(hidden_profile)
        profile_path.symlink_to(hidden_profile)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "regular file|unsafe"):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:06Z",
                )
        finally:
            profile_path.unlink()
            hidden_profile.rename(profile_path)

        profile_path.rename(hidden_profile)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "regular file"):
                ledger.observe_scope_worktree(
                    document,
                    "scope",
                    scope_show,
                    worktree_list,
                    "2026-08-14T18:03:07Z",
                )
        finally:
            hidden_profile.rename(profile_path)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, document)
            ledger.bind_scope_cas(
                root,
                initial.name,
                "scope",
                scope_show,
                worktree_list,
                safety,
                **cas_arguments(initial),
            )
            bound = ledger.inspect(root, initial.name)
            live = live_worktree_path(request)
            (live / "scope-advance.txt").write_text(
                "scope advance\n", encoding="utf-8"
            )
            git_run(live, "add", "scope-advance.txt")
            git_run(live, "commit", "-m", "advance scope target")
            head = git_run(live, "rev-parse", "HEAD").stdout.strip()
            candidate = next_generation(bound)
            target_head = candidate["targets"][0]["head"]
            target_head["current_sha"] = head
            target_head["lineage"].append(head)
            for slot in candidate["artifacts"]:
                if slot["kind"] in {"branch", "worktree"}:
                    slot["current"]["head_sha"] = head
            advanced = ledger.cas(
                root, bound.name, candidate, **cas_arguments(bound)
            )
            self.assertEqual(
                advanced.document["targets"][0]["head"]["current_sha"], head
            )
            self.assertEqual(ledger.inventory(root).pending, ())

    def test_38_cli_fifo_and_unsafe_adopted_pr_are_zero_write(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fifo = root / "input.fifo"
            os.mkfifo(fifo)
            before = directory_snapshot(root)
            process = subprocess.run(
                [sys.executable, "-B", str(SCRIPT), "prepare", str(fifo)],
                text=True,
                capture_output=True,
                check=False,
                timeout=3,
            )
            self.assertEqual(process.returncode, 2, process.stderr)
            self.assertEqual(directory_snapshot(root), before)

        unsafe = pr_ledger(
            449,
            pr_node="P_unsafe_adopted",
            branch="Feature/UnsafeAdopted",
            worktree="/workspace/worktrees/unsafe-adopted",
        )
        pull_request = next(
            slot for slot in unsafe["artifacts"] if slot["kind"] == "pull_request"
        )
        pull_request["safety"]["foreign"] = True
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "adopted pull_request"):
                ledger.create(root, unsafe)
            self.assertEqual(directory_snapshot(root), before)

    def test_39_live_git_proof_scrubs_environment_and_rejects_unsafe_state(self) -> None:
        roots = live_roots(self.live_base / "live-proof", "atrinik")
        document = deferred_primitive_pr(
            roots,
            number=450,
            node="P_live_proof",
            branch="Feature/LiveProof",
            label="live-proof",
        )
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        path = live_worktree_path(request)
        worktree_list = worktree_list_bytes(request)
        hostile = {
            "GIT_DIR": "/definitely/not/the/requested/repository",
            "GIT_WORK_TREE": "/",
            "GIT_INDEX_FILE": "/dev/null",
            "GIT_OBJECT_DIRECTORY": "/dev/null",
            "GIT_ALTERNATE_OBJECT_DIRECTORIES": "/dev/null",
            "GIT_CONFIG_COUNT": "1",
            "GIT_CONFIG_KEY_0": "core.fsmonitor",
            "GIT_CONFIG_VALUE_0": "/bin/false",
            "GIT_NO_LAZY_FETCH": "0",
        }
        original_popen = ledger.subprocess.Popen
        checked_git_environments = 0

        def checked_popen(*arguments: object, **keywords: object) -> object:
            nonlocal checked_git_environments
            command = arguments[0] if arguments else keywords.get("args")
            if isinstance(command, list) and command[:2] == ["git", "--no-pager"]:
                self.assertEqual(keywords["env"]["GIT_NO_LAZY_FETCH"], "1")
                checked_git_environments += 1
            return original_popen(*arguments, **keywords)

        with mock.patch.dict(os.environ, hostile), mock.patch.object(
            ledger.subprocess, "Popen", side_effect=checked_popen
        ):
            observed = ledger.observe_primitive_worktree(
                document,
                "worktree",
                worktree_list,
                "2026-08-14T18:04:00Z",
            )
        self.assertGreater(checked_git_environments, 0)
        self.assertEqual(observed["head_sha"], request["expected_head_sha"])
        with mock.patch.dict(sys.modules, {"atrinik_workspace": mock.Mock()}):
            self.assertEqual(
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:00Z",
                )["path"],
                str(path),
            )

        manifest = Path(roots["wrapper"]["path"]) / "components.json"
        manifest_mode = manifest.stat().st_mode & 0o777
        manifest.chmod(0o666)
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "manifest authority.*group/world writable"
            ):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:00.1Z",
                )
        finally:
            manifest.chmod(manifest_mode)

        common = Path(
            git_run(
                primary := Path(roots["primary"]["path"]),
                "rev-parse",
                "--path-format=absolute",
                "--git-common-dir",
            ).stdout.strip()
        )
        admin = Path(
            git_run(
                path,
                "rev-parse",
                "--path-format=absolute",
                "--absolute-git-dir",
            ).stdout.strip()
        )
        authority_files = (
            (common / "config", "local config"),
            (admin / "HEAD", "admin HEAD"),
            (admin / "index", "worktree index"),
            (common / "refs" / "heads" / request["branch"], "loose branch"),
        )
        for authority_file, label in authority_files:
            with self.subTest(writable_git_authority=label):
                original_mode = authority_file.stat().st_mode & 0o777
                authority_file.chmod(0o666)
                try:
                    with self.assertRaisesRegex(
                        ledger.LedgerError, "group/world writable"
                    ):
                        ledger.observe_primitive_worktree(
                            document,
                            "worktree",
                            worktree_list,
                            "2026-08-14T18:04:00.2Z",
                        )
                finally:
                    authority_file.chmod(original_mode)

        for authority_file, label in (
            (common / "config", "local config"),
            (admin / "HEAD", "admin HEAD"),
            (admin / "index", "worktree index"),
            (common / "refs" / "heads" / request["branch"], "loose branch"),
        ):
            with self.subTest(symlink_git_authority=label):
                hidden = authority_file.with_name(authority_file.name + ".hidden")
                authority_file.rename(hidden)
                authority_file.symlink_to(hidden)
                try:
                    with self.assertRaises(ledger.LedgerError):
                        ledger.observe_primitive_worktree(
                            document,
                            "worktree",
                            worktree_list,
                            "2026-08-14T18:04:00.3Z",
                        )
                finally:
                    authority_file.unlink()
                    hidden.rename(authority_file)

        config = common / "config"
        direct_config = config.read_bytes()
        delegated = common.parent / "delegated-git-config"
        delegated.write_text(
            '[remote "origin"]\n'
            "\turl = https://github.com/atrinik/atrinik.git\n",
            encoding="utf-8",
        )
        git_run(primary, "config", "--unset-all", "remote.origin.url")
        git_run(primary, "config", "include.path", str(delegated))
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "origin URL"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:00.35Z",
                )
        finally:
            config.write_bytes(direct_config)
            delegated.unlink()

        authority_git = ledger._git
        for absent_path, raw in (
            (common / "commondir", b".\n"),
            (admin / "config.worktree", b"[delivery]\n\tlate = true\n"),
        ):
            self.assertFalse(absent_path.exists())
            final_registration_calls = 0

            def appear_late(
                descriptor: int,
                arguments: object,
                context: str,
                *,
                authority_path: Path = absent_path,
                authority_raw: bytes = raw,
                **keywords: object,
            ) -> tuple[int, bytes]:
                nonlocal final_registration_calls
                response = authority_git(descriptor, arguments, context, **keywords)
                if context == "final Git worktree registration":
                    final_registration_calls += 1
                    if final_registration_calls == 2:
                        authority_path.write_bytes(authority_raw)
                return response

            try:
                with mock.patch.object(ledger, "_git", side_effect=appear_late):
                    with self.assertRaisesRegex(
                        ledger.LedgerError,
                        "appeared during live Git proof",
                    ):
                        ledger.observe_primitive_worktree(
                            document,
                            "worktree",
                            worktree_list,
                            "2026-08-14T18:04:00.36Z",
                        )
            finally:
                absent_path.unlink(missing_ok=True)

        refs_heads = common / "refs" / "heads"
        refs_heads_mode = refs_heads.stat().st_mode & 0o777
        refs_heads.chmod(0o777)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "group/world writable"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:00.4Z",
                )
        finally:
            refs_heads.chmod(refs_heads_mode)

        git_run(primary, "config", "extensions.worktreeConfig", "true")
        git_run(primary, "config", "--worktree", "delivery.primary", "true")
        git_run(path, "config", "--worktree", "delivery.child", "true")
        primary_admin = Path(
            git_run(
                primary,
                "rev-parse",
                "--path-format=absolute",
                "--absolute-git-dir",
            ).stdout.strip()
        )
        for worktree_config in (
            primary_admin / "config.worktree",
            admin / "config.worktree",
        ):
            original_mode = worktree_config.stat().st_mode & 0o777
            worktree_config.chmod(0o666)
            try:
                with self.assertRaisesRegex(
                    ledger.LedgerError, "worktree config.*group/world writable"
                ):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:04:00.5Z",
                    )
            finally:
                worktree_config.chmod(original_mode)

        child_config = admin / "config.worktree"
        hidden_child_config = child_config.with_name("config.worktree.hidden")
        child_config.rename(hidden_child_config)
        child_config.symlink_to(hidden_child_config)
        try:
            with self.assertRaises(ledger.LedgerError):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:00.55Z",
                )
        finally:
            child_config.unlink()
            hidden_child_config.rename(child_config)

        worktree_route_sentinel = "worktree-route-credential-sentinel"
        for key in ("remote.origin.pushurl", "remote.origin.url"):
            git_run(
                path,
                "config",
                "--worktree",
                key,
                "https://"
                + worktree_route_sentinel
                + "@github.com/foreign/other.git",
            )
            try:
                with self.assertRaises(ledger.LedgerError) as raised:
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:04:00.56Z",
                    )
                self.assertIn("foreign", str(raised.exception))
                self.assertNotIn(worktree_route_sentinel, str(raised.exception))
            finally:
                git_run(
                    path,
                    "config",
                    "--worktree",
                    "--unset-all",
                    key,
                    check=False,
                )

        worktrees_root = Path(roots["workspace"]["path"]) / "worktrees"
        physical_root = worktrees_root / request["physical_checkout"]
        for ancestor in (worktrees_root, physical_root):
            original_mode = ancestor.stat().st_mode & 0o777
            ancestor.chmod(0o777)
            try:
                with self.assertRaisesRegex(
                    ledger.LedgerError, "group/world writable"
                ):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:04:00.6Z",
                    )
            finally:
                ancestor.chmod(original_mode)

        original_git = ledger._git
        physical_mode = physical_root.stat().st_mode & 0o777

        def weaken_late_path(
            descriptor: int,
            arguments: object,
            context: str,
            **keywords: object,
        ) -> tuple[int, bytes]:
            result = original_git(descriptor, arguments, context, **keywords)
            if context == "Git worktree registration":
                physical_root.chmod(0o777)
            return result

        try:
            with mock.patch.object(ledger, "_git", side_effect=weaken_late_path):
                with self.assertRaisesRegex(
                    ledger.LedgerError, "group/world writable"
                ):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:04:00.7Z",
                    )
        finally:
            physical_root.chmod(physical_mode)

        route_cases = (
            (
                "remote.origin.pushurl",
                "https://github.com/foreign/other.git",
            ),
            (
                "url.https://github.com/foreign/other.git.pushInsteadOf",
                "https://github.com/atrinik/atrinik.git",
            ),
            (
                "remote.origin.pushurl",
                "http://github.com/atrinik/atrinik.git",
            ),
        )
        for key, value in route_cases:
            with self.subTest(foreign_push_route=key):
                git_run(primary, "config", key, value)
                try:
                    with self.assertRaisesRegex(ledger.LedgerError, "foreign"):
                        ledger.observe_primitive_worktree(
                            document,
                            "worktree",
                            worktree_list,
                            "2026-08-14T18:04:00Z",
                        )
                finally:
                    git_run(
                        primary,
                        "config",
                        "--unset-all",
                        key,
                        check=False,
                    )

        global_home = self.live_base / "foreign-global-home"
        global_home.mkdir()
        credential_sentinel = "credential-sentinel"
        (global_home / ".gitconfig").write_text(
            '[url "https://' + credential_sentinel
            + '@github.com/foreign/other.git"]\n'
            "\tpushInsteadOf = https://github.com/atrinik/atrinik.git\n",
            encoding="utf-8",
        )
        with mock.patch.dict(os.environ, {"HOME": str(global_home)}):
            with self.assertRaises(ledger.LedgerError) as raised:
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:00Z",
                )
        self.assertIn("foreign", str(raised.exception))
        self.assertNotIn(credential_sentinel, str(raised.exception))

        original_mode = path.stat().st_mode & 0o777
        path.chmod(0o777)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "group/world writable"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:01Z",
                )
        finally:
            path.chmod(original_mode)

        marker_raw = git_run(path, "rev-parse", "--git-path", "MERGE_HEAD").stdout.strip()
        marker = Path(marker_raw)
        if not marker.is_absolute():
            marker = path / marker
        marker.write_text(request["expected_head_sha"] + "\n", encoding="utf-8")
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "operation in progress"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:02Z",
                )
        finally:
            marker.unlink()

        for late_marker, is_directory in (
            ("MERGE_HEAD", False),
            ("rebase-merge", True),
            ("locked", False),
        ):
            marker_path = admin / late_marker
            final_registration_calls = 0

            def create_marker_after_final_registration(
                descriptor: int,
                arguments: object,
                context: str,
                *,
                target: Path = marker_path,
                directory: bool = is_directory,
                **keywords: object,
            ) -> tuple[int, bytes]:
                nonlocal final_registration_calls
                response = authority_git(descriptor, arguments, context, **keywords)
                if context == "final Git worktree registration":
                    final_registration_calls += 1
                    if final_registration_calls == 2:
                        if directory:
                            target.mkdir()
                        else:
                            target.write_text(
                                request["expected_head_sha"] + "\n", encoding="ascii"
                            )
                return response

            try:
                with mock.patch.object(
                    ledger,
                    "_git",
                    side_effect=create_marker_after_final_registration,
                ):
                    with self.assertRaisesRegex(
                        ledger.LedgerError, "appeared during live Git proof"
                    ):
                        ledger.observe_primitive_worktree(
                            document,
                            "worktree",
                            worktree_list,
                            "2026-08-14T18:04:02.1Z",
                        )
            finally:
                if marker_path.is_dir():
                    marker_path.rmdir()
                else:
                    marker_path.unlink(missing_ok=True)

        late_untracked = path / "late-untracked"

        def dirty_after_final_registration(
            descriptor: int,
            arguments: object,
            context: str,
            **keywords: object,
        ) -> tuple[int, bytes]:
            response = authority_git(descriptor, arguments, context, **keywords)
            if context == "final Git worktree registration":
                late_untracked.write_text("late\n", encoding="ascii")
            return response

        try:
            with mock.patch.object(
                ledger, "_git", side_effect=dirty_after_final_registration
            ):
                with self.assertRaisesRegex(ledger.LedgerError, "dirty"):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:04:02.2Z",
                    )
        finally:
            late_untracked.unlink(missing_ok=True)

        git_run(
            path,
            "update-index",
            "--add",
            "--cacheinfo",
            f"160000,{request['expected_head_sha']},vendor/submodule",
        )
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "submodule/gitlink"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:03Z",
                )
        finally:
            git_run(path, "reset", "--hard", "HEAD")

        sentinel = self.live_base / "fsmonitor-ran"
        hook = self.live_base / "fsmonitor"
        hook.write_text(
            "#!/bin/sh\nprintf invoked > '" + str(sentinel) + "'\nexit 1\n",
            encoding="utf-8",
        )
        hook.chmod(0o700)
        git_run(path, "config", "core.fsmonitor", str(hook))
        try:
            ledger.observe_primitive_worktree(
                document,
                "worktree",
                worktree_list,
                "2026-08-14T18:04:04Z",
            )
            self.assertFalse(sentinel.exists())
        finally:
            git_run(path, "config", "--unset-all", "core.fsmonitor", check=False)

        git_run(path, "update-index", "--split-index")
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "split index"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:04:04.05Z",
                )
        finally:
            git_run(path, "update-index", "--no-split-index")

        git_run(primary, "pack-refs", "--all")
        self.assertFalse(
            (common / "refs" / "heads" / request["branch"]).exists()
        )
        packed = ledger.observe_primitive_worktree(
            document,
            "worktree",
            worktree_list,
            "2026-08-14T18:04:04.1Z",
        )
        self.assertEqual(packed["head_sha"], request["expected_head_sha"])

        loose_ref = common / "refs" / "heads" / request["branch"]
        self.assertFalse(loose_ref.exists())
        final_registration_calls = 0

        def create_loose_ref_late(
            descriptor: int,
            arguments: object,
            context: str,
            **keywords: object,
        ) -> tuple[int, bytes]:
            nonlocal final_registration_calls
            response = authority_git(descriptor, arguments, context, **keywords)
            if context == "final Git worktree registration":
                final_registration_calls += 1
                if final_registration_calls == 2:
                    loose_ref.parent.mkdir(parents=True, exist_ok=True)
                    loose_ref.write_text(
                        request["expected_head_sha"] + "\n", encoding="ascii"
                    )
            return response

        try:
            with mock.patch.object(ledger, "_git", side_effect=create_loose_ref_late):
                with self.assertRaisesRegex(
                    ledger.LedgerError, "appeared during live Git proof"
                ):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:04:04.2Z",
                    )
        finally:
            loose_ref.unlink(missing_ok=True)

        duplicate = path.with_name(path.name + "-duplicate")
        added = git_run(
            Path(roots["primary"]["path"]),
            "worktree",
            "add",
            "--force",
            "--force",
            str(duplicate),
            request["branch"],
            check=False,
        )
        self.assertEqual(added.returncode, 0, added.stderr)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "detached|drifted"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list_bytes(request),
                    "2026-08-14T18:04:05Z",
                )
        finally:
            git_run(
                Path(roots["primary"]["path"]),
                "worktree",
                "remove",
                "--force",
                str(duplicate),
            )

    def test_40_observe_and_atomic_bind_clis_roundtrip(self) -> None:
        roots = live_roots(self.live_base / "cli-primitive", "atrinik")
        document = deferred_primitive_pr(
            roots,
            number=451,
            node="P_cli_primitive",
            branch="Feature/CliPrimitive",
            label="cli-primitive",
        )
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        live_worktree_path(request)
        worktree_list = worktree_list_bytes(request)
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            review_root = base / "reviews"
            review_root.mkdir()
            initial = ledger.create(review_root, document)
            listing = base / "worktree-list.json"
            listing.write_bytes(worktree_list)
            observe = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "worktree-observe",
                    str(review_root),
                    initial.name,
                    "worktree",
                    str(listing),
                    "2026-08-14T18:05:00Z",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(observe.returncode, 0, observe.stderr)
            safety = base / "safety.json"
            safety.write_text(observe.stdout, encoding="utf-8")
            bind = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "worktree-bind-cas",
                    str(review_root),
                    initial.name,
                    "worktree",
                    str(listing),
                    str(safety),
                    "--expected-generation",
                    str(initial.document["generation"]),
                    "--expected-digest",
                    initial.digest,
                    "--expected-device",
                    str(initial.device),
                    "--expected-inode",
                    str(initial.inode),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(bind.returncode, 0, bind.stderr)
            self.assertEqual(
                json.loads(bind.stdout)["snapshot"]["document"]["generation"], 2
            )

    def _stale_profile_worktree_fixture(
        self,
        name: str,
        *,
        claims_candidate: bool,
    ) -> tuple[dict[str, object], dict[str, object], bytes, Path, bytes]:
        roots = live_roots(self.live_base / name, "content")
        document = deferred_primitive_pr(
            roots,
            number=480 if claims_candidate else 479,
            node="P_stale_claim" if claims_candidate else "P_stale_unrelated",
            branch=(
                "Feature/StaleClaim"
                if claims_candidate
                else "Feature/StaleUnrelated"
            ),
            label="stale-profile-candidate",
        )
        retarget_repository(document, repository("content", "R_content"))
        request = next(
            slot
            for slot in document["artifacts"]
            if slot["kind"] == "worktree"
        )["primitive_request"]
        request["component"] = "content"
        request["physical_checkout"] = "content"
        worktree_list = worktree_list_bytes(request, use_wrapper_command=False)

        manifest = Manifest.load(Path(roots["wrapper"]["path"]) / "components.json")
        stack = manifest.stack("default")
        components = {
            component.name: {"kind": "primary", "value": ""}
            for component in stack.components
        }
        components.pop("resources")
        if claims_candidate:
            components["content"] = {
                "kind": "worktree",
                "value": request["label"],
            }
        profile = {
            "schema_version": 5,
            "name": name,
            "stack": "default",
            "sound_mode": "source",
            "sound_release": None,
            "components": components,
        }
        profile_path = (
            Path(roots["workspace"]["path"]) / "profiles" / f"{name}.json"
        )
        profile_path.parent.mkdir(parents=True, exist_ok=True)
        profile_path.write_bytes(json_bytes(profile))
        return document, request, worktree_list, profile_path, profile_path.read_bytes()

    def test_40a_unrelated_incomplete_profile_allows_observe_and_bind(self) -> None:
        document, request, worktree_list, profile_path, profile_before = (
            self._stale_profile_worktree_fixture(
                "stale-unrelated", claims_candidate=False
            )
        )
        observation = ledger.observe_primitive_worktree(
            document,
            "worktree",
            worktree_list,
            "2026-08-15T22:40:00Z",
        )
        self.assertEqual(observation["safety"], ledger.SAFE_ARTIFACT_STATE)
        self.assertEqual(profile_path.read_bytes(), profile_before)

        with tempfile.TemporaryDirectory() as temporary:
            review_root = Path(temporary)
            initial = ledger.create(review_root, document)
            result = ledger.bind_worktree_cas(
                review_root,
                initial.name,
                "worktree",
                worktree_list,
                ledger.canonical_bytes(observation),
                expected_generation=initial.document["generation"],
                expected_digest=initial.digest,
                expected_device=initial.device,
                expected_inode=initial.inode,
            )
        self.assertEqual(result["snapshot"]["document"]["generation"], 2)
        self.assertEqual(profile_path.read_bytes(), profile_before)
        self.assertEqual(
            result["snapshot"]["document"]["artifacts"][2]["current"]["path"],
            str(live_worktree_path(request)),
        )

    def test_40b_incomplete_profile_claiming_candidate_still_blocks(self) -> None:
        document, _request, worktree_list, profile_path, profile_before = (
            self._stale_profile_worktree_fixture(
                "stale-claim", claims_candidate=True
            )
        )
        with self.assertRaisesRegex(ledger.LedgerError, "reference set differs"):
            ledger.observe_primitive_worktree(
                document,
                "worktree",
                worktree_list,
                "2026-08-15T22:41:00Z",
            )
        self.assertEqual(profile_path.read_bytes(), profile_before)

    def test_40c_profile_root_symlink_and_bind_replacement_fail_closed(self) -> None:
        claiming, _request, claiming_list, profile_path, profile_before = (
            self._stale_profile_worktree_fixture(
                "stale-root-symlink", claims_candidate=True
            )
        )
        profiles = profile_path.parent
        hidden = profiles.with_name("profiles-hidden")
        empty = profiles.with_name("profiles-empty")
        profiles.rename(hidden)
        empty.mkdir()
        profiles.symlink_to(empty, target_is_directory=True)
        try:
            with self.assertRaisesRegex(ledger.LedgerError, "profiles root"):
                ledger.observe_primitive_worktree(
                    claiming,
                    "worktree",
                    claiming_list,
                    "2026-08-15T22:42:00Z",
                )
            self.assertEqual((hidden / profile_path.name).read_bytes(), profile_before)
        finally:
            profiles.unlink()
            empty.rmdir()
            hidden.rename(profiles)

        document, _request, worktree_list, profile_path, profile_before = (
            self._stale_profile_worktree_fixture(
                "stale-root-replacement", claims_candidate=False
            )
        )
        observation = ledger.observe_primitive_worktree(
            document,
            "worktree",
            worktree_list,
            "2026-08-15T22:43:00Z",
        )
        profiles = profile_path.parent
        hidden = profiles.with_name("profiles-hidden")

        with tempfile.TemporaryDirectory() as temporary:
            review_root = Path(temporary)
            initial = ledger.create(review_root, document)
            before = directory_snapshot(review_root)

            def replace_profiles(point: str) -> None:
                if point == "cas:proofed":
                    profiles.rename(hidden)
                    profiles.mkdir()

            try:
                with self.assertRaisesRegex(ledger.LedgerError, "profiles root"):
                    ledger.bind_worktree_cas(
                        review_root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        json_bytes(observation),
                        failpoint=replace_profiles,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(review_root), before)
                self.assertEqual((hidden / profile_path.name).read_bytes(), profile_before)
            finally:
                if profiles.is_dir():
                    profiles.rmdir()
                if hidden.exists():
                    hidden.rename(profiles)

        for index in range(4096):
            (profiles / f"ignored-{index:04d}").touch()
        with self.assertRaisesRegex(ledger.LedgerError, "inventory is oversized"):
            ledger.observe_primitive_worktree(
                document,
                "worktree",
                worktree_list,
                "2026-08-15T22:44:00Z",
            )
        self.assertEqual(profile_path.read_bytes(), profile_before)

    def test_40d_manifest_mapping_drift_during_bind_fails_closed(self) -> None:
        document, request, worktree_list, profile_path, _profile_before = (
            self._stale_profile_worktree_fixture(
                "stale-manifest-drift", claims_candidate=False
            )
        )
        profile = json.loads(profile_path.read_bytes())
        profile["components"].pop("content")
        profile["components"]["sound"] = {
            "kind": "worktree",
            "value": request["label"],
        }
        profile_path.write_bytes(json_bytes(profile))
        profile_before = profile_path.read_bytes()
        manifest_path = Path(request["roots"]["wrapper"]["path"]) / "components.json"
        manifest_before = manifest_path.read_bytes()
        changed_manifest = json.loads(manifest_before)
        for component in changed_manifest["components"]:
            if component["name"] == "content":
                component["source"] = "content"
            elif component["name"] == "sound":
                component["checkout"] = "content"
                component["source"] = "sound"
        parsed_manifest = Manifest.from_value(changed_manifest)
        self.assertEqual(parsed_manifest.by_name["sound"].checkout_name, "content")
        self.assertEqual(
            Path(request["roots"]["workspace"]["path"])
            / "worktrees"
            / parsed_manifest.by_name["sound"].checkout_name
            / request["label"],
            live_worktree_path(request),
        )

        original_module_loader = ledger._load_workspace_module
        constructed_workspaces = []

        def load_with_postconstruction_drift(wrapper_root: str):
            module = original_module_loader(wrapper_root)

            def drifting_workspace(*args, **kwargs):
                workspace = module.Workspace(*args, **kwargs)
                constructed_workspaces.append(workspace)
                manifest_path.write_bytes(json_bytes(changed_manifest))
                return workspace

            return type(
                "DriftingWorkspaceModule",
                (),
                {
                    "Manifest": module.Manifest,
                    "Workspace": staticmethod(drifting_workspace),
                },
            )

        try:
            with mock.patch.object(
                ledger,
                "_load_workspace_module",
                side_effect=load_with_postconstruction_drift,
            ), self.assertRaisesRegex(
                ledger.LedgerError, "manifest authority changed"
            ):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-15T22:45:00Z",
                )
        finally:
            manifest_path.write_bytes(manifest_before)
        self.assertEqual(len(constructed_workspaces), 1)
        self.assertIsNone(constructed_workspaces[0]._wrapper_lease)

        def load_with_manifest_aba(wrapper_root: str):
            module = original_module_loader(wrapper_root)

            def racing_workspace(*args, **kwargs):
                manifest_path.write_bytes(json_bytes(changed_manifest))
                try:
                    return module.Workspace(*args, **kwargs)
                finally:
                    manifest_path.write_bytes(manifest_before)

            return type(
                "RacingWorkspaceModule",
                (),
                {
                    "Manifest": module.Manifest,
                    "Workspace": staticmethod(racing_workspace),
                },
            )

        with mock.patch.object(
            ledger,
            "_load_workspace_module",
            side_effect=load_with_manifest_aba,
        ):
            observation = ledger.observe_primitive_worktree(
                document,
                "worktree",
                worktree_list,
                "2026-08-15T22:46:00Z",
            )
        self.assertEqual(observation["safety"], ledger.SAFE_ARTIFACT_STATE)
        self.assertEqual(profile_path.read_bytes(), profile_before)

        with tempfile.TemporaryDirectory() as temporary:
            review_root = Path(temporary)
            initial = ledger.create(review_root, document)
            before = directory_snapshot(review_root)

            def replace_manifest_mapping(point: str) -> None:
                if point == "cas:proofed":
                    manifest_path.write_bytes(json_bytes(changed_manifest))

            try:
                with self.assertRaisesRegex(
                    ledger.LedgerError, "manifest authority changed"
                ):
                    ledger.bind_worktree_cas(
                        review_root,
                        initial.name,
                        "worktree",
                        worktree_list,
                        json_bytes(observation),
                        failpoint=replace_manifest_mapping,
                        **cas_arguments(initial),
                    )
                self.assertEqual(directory_snapshot(review_root), before)
                self.assertEqual(profile_path.read_bytes(), profile_before)
            finally:
                manifest_path.write_bytes(manifest_before)

    def test_41_fresh_known_paths_and_report_coordinate_reservations_stop(self) -> None:
        for mode, candidate in (
            ("issue", issue_ledger()),
            (
                "pr",
                pr_ledger(
                    460,
                    pr_node="P_known_path",
                    branch="Feature/KnownPath",
                    worktree="/workspace/worktrees/known-path",
                ),
            ),
        ):
            with self.subTest(fresh_known_path=mode), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                worktree = next(
                    slot
                    for slot in candidate["artifacts"]
                    if slot["kind"] == "worktree"
                )
                request = worktree["primitive_request"]
                worktree["immutable"]["path"] = str(
                    Path(request["roots"]["workspace"]["path"])
                    / "worktrees"
                    / request["physical_checkout"]
                    / request["label"]
                )
                worktree["primitive_request"] = None
                before = directory_snapshot(root)
                with self.assertRaisesRegex(
                    ledger.LedgerError, "fresh planned worktree requires"
                ):
                    ledger.create(root, candidate)
                self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            migrated = pr_ledger(
                461,
                pr_node="P_migrated_known",
                branch="Feature/MigratedKnown",
                worktree="/workspace/worktrees/migrated-known",
            )
            migrated["authority"]["kind"] = "explicit-recovery"
            bind_worktree(migrated)
            report = root / "atrinik-atrinik-pr-461.md"
            report.write_bytes(b"pre-schema bound worktree\n")
            snapshot = ledger.migrate(
                root,
                report.name,
                migrated,
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(report.read_bytes()),
            )
            self.assertEqual(
                next(
                    slot
                    for slot in snapshot.document["artifacts"]
                    if slot["kind"] == "worktree"
                )["state"],
                "adopted",
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "atrinik-atrinik-issue-419.md"
            report.write_bytes(b"pre-schema planned known path\n")
            planned_known = issue_ledger()
            worktree = next(
                slot
                for slot in planned_known["artifacts"]
                if slot["kind"] == "worktree"
            )
            request = worktree["primitive_request"]
            worktree["immutable"]["path"] = str(
                Path(request["roots"]["workspace"]["path"])
                / "worktrees"
                / request["physical_checkout"]
                / request["label"]
            )
            worktree["primitive_request"] = None
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "migration planned worktree"):
                ledger.migrate(
                    root,
                    report.name,
                    planned_known,
                    kind="pre-schema",
                    expected_source_digest=ledger.byte_digest(report.read_bytes()),
                )
            self.assertEqual(directory_snapshot(root), before)

        cases: list[tuple[str, str, dict[str, object]]] = []
        cases.append(
            (
                "explicit issue from PR mode",
                "atrinik-atrinik-issue-419.md",
                pr_ledger(
                    462,
                    pr_node="P_explicit_issue",
                    branch="Feature/ExplicitIssue",
                    worktree="/workspace/worktrees/explicit-issue",
                    explicit_issue=issue(),
                ),
            )
        )
        program = issue_ledger(
            number=420,
            issue_node="I_program_leaf",
            branch="docs/program-leaf-420",
            worktree="/workspace/worktrees/program-leaf-420",
        )
        program["program"] = {
            "master_issue": issue(419, "I_master"),
            "leaf_issue": issue(420, "I_program_leaf"),
            "leaf_position": 0,
        }
        program["authority"]["allowed"]["issues"] = [
            "I_master",
            "I_program_leaf",
        ]
        cases.append(
            ("program master", "atrinik-atrinik-issue-419.md", program)
        )
        cases.append(
            (
                "mode-less issue filename",
                "atrinik-atrinik-419.md",
                issue_ledger(),
            )
        )
        hyphenated = issue_ledger()
        retarget_repository(
            hyphenated,
            {"owner": "atri-nik", "name": "foo-bar", "node_id": "R_hyphen"},
        )
        cases.append(
            (
                "hyphenated exact coordinate",
                "atri-nik-foo-bar-issue-419.md",
                hyphenated,
            )
        )
        for label, filename, candidate in cases:
            with self.subTest(report_reservation=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                (root / filename).write_bytes(
                    b"unrelated https://github.com/example/unrelated/issues/999\n"
                )
                before = directory_snapshot(root)
                with self.assertRaisesRegex(
                    ledger.LedgerError, "legacy report ownership"
                ):
                    ledger.create(root, candidate)
                self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "atrinik-atrinik-issue-419.md"
            source.write_bytes(b"pre-schema source\n")
            (root / "atrinik-atrinik-pr-463.md").write_bytes(b"")
            candidate = issue_ledger()
            pull_request = next(
                slot
                for slot in candidate["artifacts"]
                if slot["kind"] == "pull_request"
            )
            pull_request["immutable"].update(number=463, node_id="P_reserved")
            candidate["authority"]["allowed"]["pull_requests"] = ["P_reserved"]
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "legacy report ownership"
            ):
                ledger.migrate(
                    root,
                    source.name,
                    candidate,
                    kind="pre-schema",
                    expected_source_digest=ledger.byte_digest(source.read_bytes()),
                )
            self.assertEqual(directory_snapshot(root), before)

        for filename, candidate in (
            ("atrinik-atrinik-pr-419.md", issue_ledger()),
            (
                "atrinik-atrinik-issue-464.md",
                pr_ledger(
                    464,
                    pr_node="P_typed_namespace",
                    branch="Feature/TypedNamespace",
                    worktree="/workspace/worktrees/typed-namespace",
                ),
            ),
        ):
            with self.subTest(unrelated_typed_namespace=filename), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                (root / filename).write_bytes(b"")
                ledger.create(root, candidate)

    def test_42_init_root_gitfile_target_is_live_trusted_and_zero_write(self) -> None:
        def write_wrapper(wrapper: Path, target: str) -> None:
            (wrapper / "components.json").write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "cohorts": {"default": {}},
                        "stacks": {"default": {}},
                        "checkouts": [{"name": "atrinik"}],
                        "components": [{"name": "atrinik"}],
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            (wrapper / "atrinik").write_text(
                "#!/usr/bin/env python3\n"
                "from atrinik_workspace.cli import main\n"
                "raise SystemExit(main())\n",
                encoding="utf-8",
            )
            os.chmod(wrapper / "atrinik", 0o755)
            (wrapper / ".git").write_text(f"gitdir: {target}\n", encoding="ascii")

        for case in ("missing", "symlink", "writable", "invalid-head"):
            with self.subTest(gitfile_target=case), tempfile.TemporaryDirectory() as temporary:
                base = Path(temporary)
                wrapper = base / "wrapper"
                wrapper.mkdir()
                target = base / "git-target"
                if case == "missing":
                    gitfile_target = target
                elif case == "symlink":
                    real = base / "real-git-target"
                    real.mkdir()
                    (real / "HEAD").write_text("ref: refs/heads/main\n", encoding="ascii")
                    target.symlink_to(real, target_is_directory=True)
                    gitfile_target = target
                else:
                    target.mkdir()
                    (target / "HEAD").write_text(
                        "not a ref\n"
                        if case == "invalid-head"
                        else "ref: refs/heads/main\n",
                        encoding="ascii",
                    )
                    if case == "writable":
                        target.chmod(0o777)
                    gitfile_target = target
                write_wrapper(wrapper, str(gitfile_target))
                before = directory_snapshot(wrapper)
                with self.assertRaises(ledger.LedgerError):
                    ledger.init_root(wrapper)
                self.assertEqual(directory_snapshot(wrapper), before)

        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            wrapper = base / "wrapper"
            wrapper.mkdir()
            target = base / "git-target"
            target.mkdir()
            (target / "HEAD").write_text("ref: refs/heads/main\n", encoding="ascii")
            write_wrapper(wrapper, str(target))
            initialized = ledger.init_root(wrapper)
            self.assertEqual(initialized["root"], str(wrapper / "build/reviews"))

    def test_43_scope_atomic_bind_cli_roundtrip(self) -> None:
        client_repository = repository("client", "R_client")
        roots = live_roots(self.live_base / "cli-scope", "client")
        request = scope_request(
            name="issue-419-cli-scope",
            component="client",
            checkout="client",
            label="cli-scope",
            branch="docs/cli-scope",
            start_sha=git_head(roots),
            roots=roots,
        )
        live_worktree_path(request)
        document = issue_ledger(
            number=452,
            issue_node="I_cli_scope",
            branch=request["branch"],
        )
        document["resources"] = [scope_resource(request)]
        worktree = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )
        worktree["immutable"]["path"] = None
        worktree["primitive_request"] = None
        worktree["producer_resource_slot"] = "scope"
        retarget_repository(document, client_repository)
        replace_sha(document, SHA_A, request["start_sha"])
        scope_show = scope_show_bytes(request, repository_name="atrinik/client")
        install_scope_references(request, scope_show)
        worktree_list = worktree_list_bytes(request)
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            review_root = base / "reviews"
            review_root.mkdir()
            initial = ledger.create(review_root, document)
            scope_capture = base / "scope.json"
            scope_capture.write_bytes(scope_show)
            listing = base / "worktree-list.json"
            listing.write_bytes(worktree_list)
            observe = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "scope-observe",
                    str(review_root),
                    initial.name,
                    "scope",
                    str(scope_capture),
                    str(listing),
                    "2026-08-14T18:05:01Z",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(observe.returncode, 0, observe.stderr)
            safety = base / "safety.json"
            safety.write_text(observe.stdout, encoding="utf-8")
            bind = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "scope-bind-cas",
                    str(review_root),
                    initial.name,
                    "scope",
                    str(scope_capture),
                    str(listing),
                    str(safety),
                    "--expected-generation",
                    str(initial.document["generation"]),
                    "--expected-digest",
                    initial.digest,
                    "--expected-device",
                    str(initial.device),
                    "--expected-inode",
                    str(initial.inode),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(bind.returncode, 0, bind.stderr)
            self.assertEqual(
                json.loads(bind.stdout)["snapshot"]["document"]["generation"], 2
            )

    def test_44_linked_wrapper_common_git_directory_is_live_proven(self) -> None:
        base = self.live_base / "linked-wrapper-proof"
        roots = linked_wrapper_roots(base)
        wrapper = Path(roots["wrapper"]["path"])
        self.assertTrue((wrapper / ".git").is_file())
        initialized = ledger.init_root(wrapper)
        review_root = Path(initialized["root"])

        document = deferred_primitive_pr(
            roots,
            number=465,
            node="P_linked_wrapper",
            branch="Feature/LinkedWrapper",
            label="linked-wrapper-delivery",
        )
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        live_worktree_path(request)
        worktree_list = worktree_list_bytes(request)
        observation = ledger.observe_primitive_worktree(
            document,
            "worktree",
            worktree_list,
            "2026-08-14T18:06:00Z",
        )
        expected_common = git_run(
            wrapper,
            "rev-parse",
            "--path-format=absolute",
            "--git-common-dir",
        ).stdout.strip()
        self.assertNotEqual(expected_common, str(wrapper / ".git"))
        safety = json_bytes(observation)
        initial = ledger.create(review_root, document)
        result = ledger.bind_worktree_cas(
            review_root,
            initial.name,
            "worktree",
            worktree_list,
            safety,
            **cas_arguments(initial),
        )
        self.assertEqual(result["snapshot"]["document"]["generation"], 2)

        foreign_common = base / "foreign-common"
        foreign_common.mkdir()
        original_git = ledger._git

        def mismatched_common(
            descriptor: int,
            arguments: object,
            context: str,
            **keywords: object,
        ) -> tuple[int, bytes]:
            if context == "worktree common Git directory":
                return 0, (str(foreign_common) + "\n").encode("utf-8")
            return original_git(descriptor, arguments, context, **keywords)

        with mock.patch.object(ledger, "_git", side_effect=mismatched_common):
            with self.assertRaisesRegex(ledger.LedgerError, "foreign"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:06:01Z",
                )

        gitfile = live_worktree_path(request) / ".git"
        gitfile_raw = gitfile.read_bytes()

        def mutate_gitfile_late(
            descriptor: int,
            arguments: object,
            context: str,
            **keywords: object,
        ) -> tuple[int, bytes]:
            response = original_git(descriptor, arguments, context, **keywords)
            if context == "Git worktree registration":
                gitfile.write_text("gitdir: /definitely/foreign\n", encoding="utf-8")
            return response

        try:
            with mock.patch.object(ledger, "_git", side_effect=mutate_gitfile_late):
                with self.assertRaises(ledger.LedgerError):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        worktree_list,
                        "2026-08-14T18:06:01.1Z",
                    )
        finally:
            gitfile.write_bytes(gitfile_raw)

        registered_admin_text = gitfile_raw.decode("ascii").strip()
        self.assertTrue(registered_admin_text.startswith("gitdir: "))
        registered_admin = Path(registered_admin_text.removeprefix("gitdir: "))
        forged_admin = base / "forged-admin"
        shutil.copytree(registered_admin, forged_admin)
        (forged_admin / "commondir").write_text(
            expected_common + "\n", encoding="ascii"
        )
        (forged_admin / "gitdir").write_text(
            str(gitfile) + "\n", encoding="ascii"
        )
        gitfile.write_text(f"gitdir: {forged_admin}\n", encoding="ascii")
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "direct common worktree registration"
            ):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:06:01.2Z",
                )
        finally:
            gitfile.write_bytes(gitfile_raw)

        common = Path(expected_common)
        hidden_common = common.with_name(common.name + "-exact-hidden")
        common.rename(hidden_common)
        common.symlink_to(hidden_common, target_is_directory=True)
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "common Git directory|safe|symlink"
            ):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    worktree_list,
                    "2026-08-14T18:06:02Z",
                )
        finally:
            common.unlink()
            hidden_common.rename(common)

    def test_45_wrapper_authority_is_trusted_before_any_python_exec(self) -> None:
        cases = (
            "writable-init",
            "writable-workspace",
            "symlink-workspace",
            "writable-package-directory",
            "writable-bytecode",
        )
        for index, case in enumerate(cases, start=1):
            with self.subTest(case=case):
                base = self.live_base / f"preexec-{case}"
                roots = live_roots(base, "atrinik")
                package = Path(roots["wrapper"]["path"]) / "atrinik_workspace"
                marker = base / "untrusted-code-executed"
                payload = (
                    "\nfrom pathlib import Path as _DeliveryMarkerPath\n"
                    f"_DeliveryMarkerPath({str(marker)!r}).write_text("
                    "'executed', encoding='utf-8')\n"
                )
                if case in {"writable-init", "writable-workspace"}:
                    source = package / (
                        "__init__.py" if case == "writable-init" else "workspace.py"
                    )
                    source.write_text(
                        source.read_text(encoding="utf-8") + payload,
                        encoding="utf-8",
                    )
                    source.chmod(0o666)
                elif case == "symlink-workspace":
                    source = package / "workspace.py"
                    hidden = package / "workspace.hidden.py"
                    source.rename(hidden)
                    hidden.write_text(
                        hidden.read_text(encoding="utf-8") + payload,
                        encoding="utf-8",
                    )
                    source.symlink_to(hidden)
                elif case == "writable-package-directory":
                    package.chmod(0o777)
                else:
                    pycache = package / "__pycache__"
                    pycache.mkdir(exist_ok=True)
                    bytecode = pycache / "untrusted.cpython-311.pyc"
                    bytecode.write_bytes(b"untrusted bytecode")
                    bytecode.chmod(0o666)

                document = deferred_primitive_pr(
                    roots,
                    number=470 + index,
                    node=f"P_preexec_{index}",
                    branch=f"Feature/Preexec{index}",
                    label=f"preexec-{index}",
                )
                request = next(
                    slot
                    for slot in document["artifacts"]
                    if slot["kind"] == "worktree"
                )["primitive_request"]
                live_worktree_path(request)
                # This test deliberately poisons the wrapper package before the
                # ledger proves it safe, so invoking that wrapper here would
                # defeat the trust-before-exec property under test.
                listing = worktree_list_bytes(request, use_wrapper_command=False)
                with self.assertRaisesRegex(
                    ledger.LedgerError,
                    "wrapper authority package.*(writable|regular|directory)",
                ):
                    ledger.observe_primitive_worktree(
                        document,
                        "worktree",
                        listing,
                        f"2026-08-14T18:07:0{index}Z",
                    )
                self.assertFalse(marker.exists(), case)

        roots = live_roots(self.live_base / "trusted-package-content-change", "atrinik")
        document = deferred_primitive_pr(
            roots,
            number=476,
            node="P_trusted_content_change",
            branch="Feature/TrustedContentChange",
            label="trusted-content-change",
        )
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        live_worktree_path(request)
        listing = worktree_list_bytes(request)
        workspace_source = (
            Path(roots["wrapper"]["path"])
            / "atrinik_workspace"
            / "workspace.py"
        )
        active_override = (
            "Workspace._source_references = "
            "lambda self, source, **kwargs: {'profile:changed-authority'}"
        )
        inactive_override = "#" + active_override[1:]
        self.assertEqual(len(inactive_override), len(active_override))
        original_source = workspace_source.read_text(encoding="utf-8")
        inactive_source = original_source + "\n" + inactive_override + "\n"
        workspace_source.write_text(inactive_source, encoding="utf-8")
        bytecode_path = Path(importlib.util.cache_from_source(str(workspace_source)))
        py_compile.compile(
            str(workspace_source),
            cfile=str(bytecode_path),
            doraise=True,
        )
        old_status = workspace_source.stat()
        first = ledger.observe_primitive_worktree(
            document,
            "worktree",
            listing,
            "2026-08-14T18:07:10Z",
        )
        self.assertEqual(first["safety"], ledger.SAFE_ARTIFACT_STATE)

        active_source = original_source + "\n" + active_override + "\n"
        self.assertEqual(len(active_source.encode()), len(inactive_source.encode()))
        workspace_source.write_text(active_source, encoding="utf-8")
        os.utime(
            workspace_source,
            ns=(old_status.st_atime_ns, old_status.st_mtime_ns),
        )
        self.assertEqual(workspace_source.stat().st_mode & 0o022, 0)
        self.assertEqual(workspace_source.stat().st_size, old_status.st_size)
        self.assertEqual(workspace_source.stat().st_mtime_ns, old_status.st_mtime_ns)
        with self.assertRaisesRegex(ledger.LedgerError, "reference set differs"):
            ledger.observe_primitive_worktree(
                document,
                "worktree",
                listing,
                "2026-08-14T18:07:11Z",
            )

        roots = live_roots(self.live_base / "trusted-package-aba", "atrinik")
        document = deferred_primitive_pr(
            roots,
            number=477,
            node="P_trusted_package_aba",
            branch="Feature/TrustedPackageAba",
            label="trusted-package-aba",
        )
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        live_worktree_path(request)
        listing = worktree_list_bytes(request)
        workspace_source = (
            Path(roots["wrapper"]["path"])
            / "atrinik_workspace"
            / "workspace.py"
        )
        benign_source = workspace_source.read_bytes()
        aba_marker = self.live_base / "package-aba-executed"
        malicious_source = (
            f"open({str(aba_marker)!r}, 'w').write('executed')\n".encode("utf-8")
            + benign_source
        )
        validated = threading.Event()
        swapped = threading.Event()
        executed = threading.Event()
        restored = threading.Event()
        writer_errors: list[BaseException] = []
        original_prevalidate = ledger._prevalidate_workspace_package
        original_exec_module = ledger._SnapshotSourceLoader.exec_module
        validation_calls = 0

        def pause_after_snapshot(package_root: Path) -> object:
            nonlocal validation_calls
            snapshot = original_prevalidate(package_root)
            validation_calls += 1
            if validation_calls == 1:
                validated.set()
                if not swapped.wait(5):
                    raise AssertionError("ABA writer did not install swapped source")
            return snapshot

        def finish_snapshot_execution(loader: object, module: object) -> None:
            original_exec_module(loader, module)
            if loader.relative_path == "workspace.py":
                executed.set()
                if not restored.wait(5):
                    raise AssertionError("ABA writer did not restore validated source")

        def swap_and_restore() -> None:
            try:
                if not validated.wait(5):
                    raise AssertionError("package prevalidation did not complete")
                workspace_source.write_bytes(malicious_source)
                swapped.set()
                if not executed.wait(5):
                    raise AssertionError("snapshot workspace module did not execute")
                workspace_source.write_bytes(benign_source)
                restored.set()
            except BaseException as error:
                writer_errors.append(error)
                swapped.set()
                restored.set()

        writer = threading.Thread(target=swap_and_restore, daemon=True)
        writer.start()
        try:
            with mock.patch.object(
                ledger,
                "_prevalidate_workspace_package",
                side_effect=pause_after_snapshot,
            ), mock.patch.object(
                ledger._SnapshotSourceLoader,
                "exec_module",
                new=finish_snapshot_execution,
            ):
                observed = ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    listing,
                    "2026-08-14T18:07:12Z",
                )
            self.assertEqual(observed["path"], str(live_worktree_path(request)))
        finally:
            executed.set()
            workspace_source.write_bytes(benign_source)
            restored.set()
            writer.join(5)
        self.assertFalse(writer.is_alive())
        self.assertEqual(writer_errors, [])
        self.assertFalse(aba_marker.exists())

    def test_45a_candidate_bootstrap_requires_clean_wrapper_self_authority(self) -> None:
        original_module_loader = ledger._load_workspace_module

        def legacy_primary_module(wrapper_root: str):
            module = original_module_loader(wrapper_root)

            class LegacyWorkspace(module.Workspace):
                def __init__(
                    self,
                    repository: Path,
                    *,
                    backfill_references: bool = True,
                ) -> None:
                    super().__init__(
                        repository,
                        backfill_references=backfill_references,
                    )

                def _source_references(self, source_root: Path) -> list[str]:
                    return super()._source_references(source_root)

            return type(
                "LegacyPrimaryWorkspaceModule",
                (),
                {"Manifest": module.Manifest, "Workspace": LegacyWorkspace},
            )

        roots = live_roots(self.live_base / "dirty-bootstrap", "atrinik")
        document = deferred_primitive_pr(
            roots,
            number=493,
            node="P_dirty_bootstrap",
            branch="Feature/DirtyBootstrap",
            label="dirty-bootstrap",
        )
        request = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )["primitive_request"]
        live = live_worktree_path(request)
        listing = worktree_list_bytes(request, use_wrapper_command=False)
        marker = self.live_base / "dirty-bootstrap-executed"
        candidate_init = live / "atrinik_workspace" / "__init__.py"
        candidate_init.write_text(
            candidate_init.read_text(encoding="utf-8")
            + "\nfrom pathlib import Path as _DirtyBootstrapMarker\n"
            + f"_DirtyBootstrapMarker({str(marker)!r}).write_text('executed')\n",
            encoding="utf-8",
        )
        with mock.patch.object(
            ledger, "_load_workspace_module", side_effect=legacy_primary_module
        ) as primary_loader:
            with self.assertRaisesRegex(ledger.LedgerError, "worktree is dirty"):
                ledger.observe_primitive_worktree(
                    document,
                    "worktree",
                    listing,
                    "2026-08-16T10:00:00Z",
                )
        primary_loader.assert_not_called()
        self.assertFalse(marker.exists())

        replacement_roots = live_roots(
            self.live_base / "bootstrap-root-replacement", "atrinik"
        )
        replacement_document = deferred_primitive_pr(
            replacement_roots,
            number=495,
            node="P_bootstrap_root_replacement",
            branch="Feature/BootstrapRootReplacement",
            label="bootstrap-root-replacement",
        )
        replacement_request = next(
            slot
            for slot in replacement_document["artifacts"]
            if slot["kind"] == "worktree"
        )["primitive_request"]
        replacement_live = live_worktree_path(replacement_request)
        (replacement_live / "atrinik_workspace" / "bootstrap_identity_probe.py").write_text(
            "PROBE = True\n", encoding="utf-8"
        )
        git_run(replacement_live, "add", "atrinik_workspace/bootstrap_identity_probe.py")
        git_run(replacement_live, "commit", "-m", "test bootstrap identity")
        replacement_head = git_run(
            replacement_live, "rev-parse", "HEAD"
        ).stdout.strip()
        package = replacement_live / "atrinik_workspace"
        hidden_package = replacement_live / "atrinik_workspace-retained"
        replaced = False
        snapshot_calls = 0
        original_git_snapshot = ledger._git_workspace_package_snapshot

        def replace_package_during_load(*args: object, **kwargs: object):
            nonlocal replaced, snapshot_calls
            snapshot_calls += 1
            if snapshot_calls == 2:
                package.rename(hidden_package)
                package.mkdir()
                replaced = True
            return original_git_snapshot(*args, **kwargs)

        replacement_descriptor = os.open(
            replacement_live, os.O_RDONLY | os.O_DIRECTORY
        )
        try:
            with mock.patch.object(
                ledger,
                "_git_workspace_package_snapshot",
                side_effect=replace_package_during_load,
            ):
                with self.assertRaisesRegex(
                    ledger.LedgerError, "Git tree changed during import"
                ):
                    ledger._load_workspace_module_from_git(
                        str(replacement_live),
                        replacement_descriptor,
                        replacement_head,
                    )
            self.assertTrue(replaced)
        finally:
            os.close(replacement_descriptor)
            if package.is_dir():
                package.rmdir()
            if hidden_package.exists():
                hidden_package.rename(package)

        symlink_roots = live_roots(
            self.live_base / "bootstrap-git-symlink", "atrinik"
        )
        symlink_wrapper = Path(symlink_roots["wrapper"]["path"])
        (symlink_wrapper / "atrinik_workspace" / "unsafe.py").symlink_to(
            "workspace.py"
        )
        git_run(symlink_wrapper, "add", "atrinik_workspace/unsafe.py")
        git_run(symlink_wrapper, "commit", "-m", "test package symlink")
        symlink_head = git_run(symlink_wrapper, "rev-parse", "HEAD").stdout.strip()
        symlink_descriptor = os.open(
            symlink_wrapper, os.O_RDONLY | os.O_DIRECTORY
        )
        try:
            with self.assertRaisesRegex(
                ledger.LedgerError, "Git entry is not a regular file"
            ):
                ledger._git_workspace_package_snapshot(
                    symlink_wrapper / "atrinik_workspace",
                    symlink_descriptor,
                    symlink_head,
                )
        finally:
            os.close(symlink_descriptor)

        component_roots = live_roots(
            self.live_base / "component-bootstrap", "content"
        )
        component_document = deferred_primitive_pr(
            component_roots,
            number=494,
            node="P_component_bootstrap",
            branch="Feature/ComponentBootstrap",
            label="component-bootstrap",
        )
        retarget_repository(
            component_document, repository("content", "R_content")
        )
        component_request = next(
            slot
            for slot in component_document["artifacts"]
            if slot["kind"] == "worktree"
        )["primitive_request"]
        component_request["component"] = "content"
        component_request["physical_checkout"] = "content"
        component_listing = worktree_list_bytes(
            component_request, use_wrapper_command=False
        )
        with (
            mock.patch.object(
                ledger,
                "_load_workspace_module",
                side_effect=legacy_primary_module,
            ),
            mock.patch.object(ledger, "_load_workspace_module_from_git") as candidate,
        ):
            with self.assertRaisesRegex(
                ledger.LedgerError,
                "primary wrapper lacks profile inventory authority",
            ):
                ledger.observe_primitive_worktree(
                    component_document,
                    "worktree",
                    component_listing,
                    "2026-08-16T10:01:00Z",
                )
        candidate.assert_not_called()

    def test_46_legacy_paths_reserve_deferred_primitive_and_scope_worktrees(self) -> None:
        primitive = issue_ledger()
        primitive_path = "/workspace/worktrees/atrinik/issue-419"

        roots = live_roots(self.live_base / "legacy-scope-reservation", "client")
        request = scope_request(
            component="client",
            checkout="client",
            start_sha=git_head(roots),
            roots=roots,
        )
        scoped = issue_ledger()
        scoped["resources"] = [scope_resource(request)]
        scoped_worktree = next(
            slot for slot in scoped["artifacts"] if slot["kind"] == "worktree"
        )
        scoped_worktree["immutable"]["path"] = None
        scoped_worktree["primitive_request"] = None
        scoped_worktree["producer_resource_slot"] = "scope"
        client_repository = repository("client", "R_client")
        retarget_repository(scoped, client_repository)
        replace_sha(scoped, SHA_A, request["start_sha"])
        scope_path = str(
            Path(request["roots"]["workspace"]["path"])
            / "worktrees"
            / request["physical_checkout"]
            / request["label"]
        )

        for label, candidate, reserved_path in (
            ("primitive", primitive, primitive_path),
            ("scope", scoped, scope_path),
        ):
            with self.subTest(kind=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                report = root / "example-other-issue-999.md"
                report.write_text(
                    "| example/other@main | base | Feature/Unrelated / "
                    + SHA_A
                    + " | merge | "
                    + reserved_path
                    + " | commits |\n",
                    encoding="utf-8",
                )
                before = directory_snapshot(root)
                with self.assertRaisesRegex(
                    ledger.LedgerError, "legacy report ownership overlaps"
                ):
                    ledger.create(root, candidate)
                self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reserved_branch = "refactor/358-content-main"
            reserved_path = (
                "/workspaces/atrinik/workspace/worktrees/atrinik/"
                "issue-358-content-main"
            )
            report = root / "atrinik-atrinik-358.md"
            report.write_text(
                "Historical issue: "
                "https://github.com/atrinik/atrinik/issues/358#issuecomment-1\n"
                "Historical PR: "
                "https://www.github.com/atrinik/atrinik/pull/359\n"
                "| atrinik/atrinik@main | base | "
                + reserved_branch
                + " / 89bf1ac | merge | "
                + reserved_path
                + " | commits |\n",
                encoding="utf-8",
            )
            claim = ledger.inventory(root).legacy_reports[0]
            self.assertTrue(claim.evidence_invalid)
            self.assertEqual(claim.issues, (("atrinik", "atrinik", 358),))
            self.assertEqual(claim.pull_requests, (("atrinik", "atrinik", 359),))
            self.assertEqual(
                claim.repository_heads,
                (("atrinik", "atrinik", reserved_branch),),
            )
            self.assertEqual(claim.worktrees, (reserved_path,))

            candidate = issue_ledger(
                number=999,
                issue_node="I_unrelated_999",
                branch=reserved_branch,
                worktree=reserved_path,
            )
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "legacy report ownership overlaps"
            ):
                ledger.create(root, candidate)
            self.assertEqual(directory_snapshot(root), before)

            malformed_closing = ledger._legacy_claim(
                "atrinik-classic-59.md",
                (
                    "- Canonical closing PR: `https://github.com/atrinik/classic/"
                    "pull/60` explanatory text\n"
                ).encode(),
                None,
            )
            self.assertTrue(malformed_closing.evidence_invalid)
            self.assertEqual(
                malformed_closing.pull_requests,
                (("atrinik", "classic", 60),),
            )
            uncorroborated_closing = ledger._legacy_claim(
                "atrinik-content-75.md",
                (
                    "- PR: https://github.com/atrinik/content/pull/64\n"
                    "- Canonical closing PR: "
                    "https://github.com/atrinik/classic/pull/77\n"
                ).encode(),
                None,
            )
            self.assertTrue(uncorroborated_closing.evidence_invalid)
            self.assertEqual(
                uncorroborated_closing.pull_requests,
                (("atrinik", "classic", 77), ("atrinik", "content", 64)),
            )

    def test_47_cross_repository_legacy_filename_rebind_is_exact_and_resumable(self) -> None:
        def write_pair(
            root: Path, candidate: dict[str, object]
        ) -> tuple[Path, dict[str, str]]:
            (root / "atrinik-atrinik-304.md").write_text(
                "## Coordinates\n\n"
                "- Issue: https://github.com/atrinik/atrinik/issues/304\n"
                "- Pull request / canonical closing PR: "
                "https://github.com/atrinik/atrinik/pull/306\n"
                "- Repository: `atrinik/atrinik`\n"
                "- Head branch / SHA: `feat/issue-delivery-skill` / `"
                + SHA_B
                + "`\n"
                "- Worktree: `/workspace/worktrees/atrinik/issue-304-delivery`\n\n"
                "Historical validation: https://github.com/atrinik/atrinik/pull/306\n",
                encoding="utf-8",
            )
            source = root / "atrinik-classic-329.md"
            source.write_bytes(
                legacy_bullet_report_bytes(candidate, "classic", reported_sha=SHA_B)
            )
            related = root / "atrinik-atrinik-329.md"
            related.write_bytes(
                legacy_bullet_report_bytes(candidate, "atrinik", reported_sha=SHA_C)
            )
            return source, {
                related.name: ledger.byte_digest(related.read_bytes())
            }

        failpoints = (
            "migration:plan-staged",
            "migration:plan-linked",
            "migration:planned",
            "migration:snapshot",
            "migration:report",
            "migration:prepared-staged",
            "migration:prepared-renamed",
            "migration:staged",
            "migration:linked",
            "migration:installed",
            "migration:complete-staged",
            "migration:completed-renamed",
            "migration:complete",
        )
        for failpoint in failpoints:
            with self.subTest(failpoint=failpoint), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                candidate = legacy_rebind_ledger()
                source, related_sources = write_pair(root, candidate)
                digest = ledger.byte_digest(source.read_bytes())
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.migrate(
                        root,
                        source.name,
                        candidate,
                        kind="legacy-rebind",
                        expected_source_digest=digest,
                        related_sources=related_sources,
                        expected_historical_heads=legacy_rebind_historical_heads(),
                        failpoint=failpoint,
                    )
                migrated = ledger.migrate(
                    root,
                    source.name,
                    candidate,
                    kind="legacy-rebind",
                    expected_source_digest=digest,
                    related_sources=related_sources,
                    expected_historical_heads=legacy_rebind_historical_heads(),
                )
                self.assertEqual(
                    migrated.name, "atrinik-atrinik-issue-329.md.ledger.json"
                )
                self.assertEqual(
                    migrated.document["migration"]["kind"], "legacy-rebind"
                )
                self.assertEqual(ledger.inventory(root).pending, ())
                self.assertTrue(source.is_file())
                self.assertTrue((root / "atrinik-atrinik-issue-329.md").is_file())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = legacy_rebind_ledger()
            source, related_sources = write_pair(root, candidate)
            digest = ledger.byte_digest(source.read_bytes())
            first = ledger.migrate(
                root,
                source.name,
                candidate,
                kind="legacy-rebind",
                expected_source_digest=digest,
                related_sources=related_sources,
                expected_historical_heads=legacy_rebind_historical_heads(),
            )
            second = ledger.migrate(
                root,
                source.name,
                candidate,
                kind="legacy-rebind",
                expected_source_digest=digest,
                related_sources=related_sources,
                expected_historical_heads=legacy_rebind_historical_heads(),
            )
            self.assertEqual(second.digest, first.digest)
            self.assertEqual(
                [source["name"] for source in second.document["migration"]["related_sources"]],
                ["atrinik-atrinik-329.md"],
            )

            fresh = issue_ledger(
                number=329,
                issue_node="I_classic_329",
                branch="fix/fresh-classic-329",
                worktree="/workspace/worktrees/classic/fresh-329",
            )
            retarget_repository(fresh, repository("classic", "R_classic"))
            canonical = root / "atrinik-classic-issue-329.md"
            canonical.write_text("Interrupted fresh delivery report.\n", encoding="utf-8")
            before_fresh_migration = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "legacy report ownership overlaps"
            ):
                ledger.create(root, fresh)
            self.assertEqual(directory_snapshot(root), before_fresh_migration)

            workspace = root / "workspace"
            wrapper = root / "wrapper"
            checkout = wrapper / "classic"
            for path in (workspace, wrapper, checkout):
                path.mkdir()
            worktree = next(
                slot for slot in fresh["artifacts"] if slot["kind"] == "worktree"
            )
            worktree["immutable"]["path"] = None
            worktree["primitive_request"] = {
                "component": "classic",
                "physical_checkout": "classic",
                "label": "fresh-329",
                "repository": repository("classic", "R_classic"),
                "branch": "fix/fresh-classic-329",
                "expected_head_sha": SHA_A,
                "roots": {
                    name: {
                        "path": str(path),
                        "device": path.stat().st_dev,
                        "inode": path.stat().st_ino,
                    }
                    for name, path in (
                        ("wrapper", wrapper),
                        ("workspace", workspace),
                        ("primary", checkout),
                    )
                },
            }
            created = ledger.migrate(
                root,
                canonical.name,
                fresh,
                kind="pre-schema",
                expected_source_digest=ledger.byte_digest(canonical.read_bytes()),
            )
            self.assertEqual(created.name, "atrinik-classic-issue-329.md.ledger.json")
            related = root / "atrinik-atrinik-329.md"
            related.write_bytes(related.read_bytes() + b"drift\n")
            with self.assertRaisesRegex(
                ledger.LedgerError, "migration related source changed"
            ):
                ledger.inventory(root)

        for label, source_name, authority, message in (
            (
                "authority",
                "atrinik-classic-329.md",
                "durable-goal",
                "explicit-recovery authority",
            ),
            (
                "matching issue filename",
                "atrinik-atrinik-329.md",
                "explicit-recovery",
                "ordinary legacy migration",
            ),
            (
                "wrong target repository",
                "atrinik-server-329.md",
                "explicit-recovery",
                "one exact target repository",
            ),
            (
                "wrong issue number",
                "atrinik-classic-330.md",
                "explicit-recovery",
                "selected issue number",
            ),
        ):
            with self.subTest(guard=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                recovery = legacy_rebind_ledger()
                recovery["authority"]["kind"] = authority
                source = root / source_name
                source.write_bytes(legacy_bullet_report_bytes(recovery, "classic"))
                before = directory_snapshot(root)
                with self.assertRaisesRegex(ledger.LedgerError, message):
                    ledger.migrate(
                        root,
                        source.name,
                        recovery,
                        kind="legacy-rebind",
                        expected_source_digest=ledger.byte_digest(source.read_bytes()),
                        related_sources={
                            "atrinik-atrinik-329.md": "a" * 64
                        },
                        expected_historical_heads=legacy_rebind_historical_heads(),
                    )
                self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            conflicting = issue_ledger(
                number=329,
                issue_node="I_classic_329",
                branch="fix/conflicting-classic-329",
                worktree="/workspace/worktrees/classic/conflicting-329",
            )
            retarget_repository(conflicting, repository("classic", "R_classic"))
            source = root / "atrinik-classic-329.md"
            source.write_bytes(legacy_report_bytes(conflicting))
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "legacy report ownership overlaps"
            ):
                ledger.create(root, conflicting)
            self.assertEqual(directory_snapshot(root), before)

            recovery = legacy_rebind_ledger()
            related = root / "atrinik-atrinik-329.md"
            related.write_bytes(legacy_bullet_report_bytes(recovery, "atrinik"))
            migration_before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError,
                "does not exactly prove|claim does not exactly match",
            ):
                ledger.migrate(
                    root,
                    source.name,
                    recovery,
                    kind="legacy-rebind",
                    expected_source_digest=ledger.byte_digest(source.read_bytes()),
                    related_sources={
                        related.name: ledger.byte_digest(related.read_bytes())
                    },
                    expected_historical_heads=legacy_rebind_historical_heads(),
                )
            self.assertEqual(directory_snapshot(root), migration_before)

        malformed_reports = (
            (
                "primary issue URL suffix",
                lambda raw: raw.replace(b"/issues/329", b"/issues/329oops", 1),
            ),
            (
                "primary PR URL suffix",
                lambda raw: raw.replace(b"/pull/92", b"/pull/92oops", 1),
            ),
            (
                "extra malformed issue URL",
                lambda raw: raw + b"https://github.com/atrinik/atrinik/issues/nope\n",
            ),
            (
                "missing issue number",
                lambda raw: raw + b"https://github.com/evil/repo/issues/.\n",
            ),
            (
                "bare missing PR number",
                lambda raw: raw + b"github.com/evil/repo/pull/\n",
            ),
            (
                "issue path without separator",
                lambda raw: raw + b"https://github.com/evil/repo/issues\n",
            ),
            (
                "invalid issue owner",
                lambda raw: raw + b"https://github.com/evil!/repo/issues/999\n",
            ),
            (
                "extra singular issue URL",
                lambda raw: raw + b"https://github.com/atrinik/atrinik/issue/329\n",
            ),
            (
                "unsupported HTTP issue URL",
                lambda raw: raw + b"http://github.com/atrinik/atrinik/issues/329\n",
            ),
            (
                "unsupported www PR URL",
                lambda raw: raw + b"https://www.github.com/atrinik/classic/pull/92\n",
            ),
            (
                "bare GitHub issue coordinate",
                lambda raw: raw + b"github.com/evil/repo/issues/999\n",
            ),
            (
                "protocol-relative GitHub PR coordinate",
                lambda raw: raw + b"//github.com/evil/repo/pull/999\n",
            ),
            (
                "unsupported-scheme GitHub issue coordinate",
                lambda raw: raw + b"ftp://github.com/evil/repo/issues/999\n",
            ),
            (
                "extra malformed PR URL",
                lambda raw: raw + b"https://github.com/atrinik/classic/pull/nope\n",
            ),
            (
                "malformed repository/head row",
                lambda raw: raw
                + b"| atrinik/classic | base | bad..branch / "
                + SHA_A.encode("ascii")
                + b" | merge | /workspace/worktrees/classic/other | commits |\n",
            ),
            (
                "malformed worktree row",
                lambda raw: raw
                + b"| atrinik/classic@main | base | fix/other / "
                + SHA_A.encode("ascii")
                + b" | merge | relative/worktree | commits |\n",
            ),
            (
                "combined malformed repository and worktree row",
                lambda raw: raw
                + b"| atrinik/classic | base | fix/other / "
                + SHA_A.encode("ascii")
                + b" | merge | relative/worktree | commits |\n",
            ),
        )
        for label, mutate in malformed_reports:
            with self.subTest(malformed=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                recovery = legacy_rebind_ledger()
                source, related_sources = write_pair(root, recovery)
                source.write_bytes(mutate(source.read_bytes()))
                before = directory_snapshot(root)
                with self.assertRaisesRegex(
                    ledger.LedgerError, "duplicate or ambiguous"
                ):
                    ledger.migrate(
                        root,
                        source.name,
                        recovery,
                        kind="legacy-rebind",
                        expected_source_digest=ledger.byte_digest(source.read_bytes()),
                        related_sources=related_sources,
                        expected_historical_heads=legacy_rebind_historical_heads(),
                    )
                self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery = legacy_rebind_ledger()
            source, _related_sources = write_pair(root, recovery)
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "requires exact related source"
            ):
                ledger.migrate(
                    root,
                    source.name,
                    recovery,
                    kind="legacy-rebind",
                    expected_source_digest=ledger.byte_digest(source.read_bytes()),
                )
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery = legacy_rebind_ledger()
            source, related_sources = write_pair(root, recovery)
            related = root / next(iter(related_sources))
            related.write_text("Historical narrative only.\n", encoding="utf-8")
            related_sources[related.name] = ledger.byte_digest(related.read_bytes())
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "does not exactly prove its target"
            ):
                ledger.migrate(
                    root,
                    source.name,
                    recovery,
                    kind="legacy-rebind",
                    expected_source_digest=ledger.byte_digest(source.read_bytes()),
                    related_sources=related_sources,
                    expected_historical_heads=legacy_rebind_historical_heads(),
                )
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery = legacy_rebind_ledger()
            source, related_sources = write_pair(root, recovery)
            wrong_heads = legacy_rebind_historical_heads()
            wrong_heads[("atrinik", "classic", "fix/legacy-classic-329")] = SHA_A
            before = directory_snapshot(root)
            with self.assertRaisesRegex(
                ledger.LedgerError, "historical heads do not match"
            ):
                ledger.migrate(
                    root,
                    source.name,
                    recovery,
                    kind="legacy-rebind",
                    expected_source_digest=ledger.byte_digest(source.read_bytes()),
                    related_sources=related_sources,
                    expected_historical_heads=wrong_heads,
                )
            self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recovery = legacy_rebind_ledger()
            source, related_sources = write_pair(root, recovery)
            candidate_input = root / "candidate.json"
            candidate_input.write_text(json.dumps(recovery), encoding="utf-8")
            related_name, related_digest = next(iter(related_sources.items()))
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "migrate",
                    str(root),
                    source.name,
                    str(candidate_input),
                    "--kind",
                    "legacy-rebind",
                    "--expected-source-digest",
                    ledger.byte_digest(source.read_bytes()),
                    "--related-source",
                    f"{related_name}={related_digest}",
                    "--historical-head",
                    f"atrinik/atrinik@fix/legacy-classic-329={SHA_C}",
                    "--historical-head",
                    f"atrinik/classic@fix/legacy-classic-329={SHA_B}",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(
                json.loads(process.stdout)["name"],
                "atrinik-atrinik-issue-329.md.ledger.json",
            )

    def test_48_target_advancement_is_live_proven_and_crash_safe(self) -> None:
        roots = live_roots(self.live_base / "target-advancement", "atrinik")
        initial_head = git_head(roots)
        branch = "fix/target-advancement"
        label = "target-advancement"
        worktree_path = str(
            Path(roots["workspace"]["path"]) / "worktrees" / "atrinik" / label
        )
        document = issue_ledger(
            number=453,
            issue_node="I_target_advancement",
            branch=branch,
            worktree=worktree_path,
        )
        replace_sha(document, SHA_A, initial_head)
        worktree = next(
            slot for slot in document["artifacts"] if slot["kind"] == "worktree"
        )
        worktree["primitive_request"]["roots"] = copy.deepcopy(roots)
        live = live_worktree_path(worktree["primitive_request"])

        def advance_head(snapshot: object, head: str) -> dict[str, object]:
            candidate = next_generation(snapshot)
            target_head = candidate["targets"][0]["head"]
            target_head["current_sha"] = head
            target_head["lineage"].append(head)
            for slot in candidate["artifacts"]:
                if slot["kind"] in {"branch", "worktree"}:
                    slot["current"]["head_sha"] = head
            return candidate

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            created = ledger.create(root, document)
            worktree_list = worktree_list_bytes(worktree["primitive_request"])
            safety = safety_observation_bytes(
                worktree["primitive_request"],
                worktree_list,
                producer_kind="primitive",
                producer_digest=None,
            )
            ledger.bind_worktree_cas(
                root,
                created.name,
                "worktree",
                worktree_list,
                safety,
                **cas_arguments(created),
            )
            current = ledger.inspect(root, created.name)

            (live / "first.txt").write_text("first\n", encoding="utf-8")
            git_run(live, "add", "first.txt")
            git_run(live, "commit", "-m", "first target advancement")
            first_head = git_run(live, "rev-parse", "HEAD").stdout.strip()
            first_candidate = advance_head(current, first_head)
            current = ledger.cas(
                root, current.name, first_candidate, **cas_arguments(current)
            )
            self.assertEqual(
                current.document["targets"][0]["head"]["current_sha"], first_head
            )
            self.assertEqual(ledger.inventory(root).pending, ())

            primary = Path(roots["primary"]["path"])
            (primary / "base.txt").write_text("base\n", encoding="utf-8")
            git_run(primary, "add", "base.txt")
            git_run(primary, "commit", "-m", "advance target base")
            base_head = git_run(primary, "rev-parse", "HEAD").stdout.strip()
            base_candidate = next_generation(current)
            target_base = base_candidate["targets"][0]["base"]
            target_base["current_sha"] = base_head
            target_base["lineage"].append(base_head)
            current = ledger.cas(
                root, current.name, base_candidate, **cas_arguments(current)
            )
            self.assertEqual(
                current.document["targets"][0]["base"]["current_sha"], base_head
            )

            latest_head = first_head
            for index, failpoint in enumerate(("cas:staged", "cas:proofed")):
                filename = f"proof-{index}.txt"
                (live / filename).write_text(f"proof {index}\n", encoding="utf-8")
                git_run(live, "add", filename)
                git_run(live, "commit", "-m", f"target proof {failpoint}")
                latest_head = git_run(live, "rev-parse", "HEAD").stdout.strip()
                candidate = advance_head(current, latest_head)
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.cas(
                        root,
                        current.name,
                        candidate,
                        failpoint=failpoint,
                        **cas_arguments(current),
                    )
                current = ledger.cas(
                    root, current.name, candidate, **cas_arguments(current)
                )
                self.assertEqual(ledger.inventory(root).pending, ())

            missing_base = next_generation(current)
            missing_base["targets"][0]["base"]["current_sha"] = "f" * 40
            missing_base["targets"][0]["base"]["lineage"].append("f" * 40)
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "target base commit"):
                ledger.cas(root, current.name, missing_base, **cas_arguments(current))
            self.assertEqual(directory_snapshot(root), before)

            abbreviated = advance_head(current, latest_head[:12])
            with self.assertRaisesRegex(ledger.LedgerError, "current_sha is invalid"):
                ledger.prepare(abbreviated)

            git_run(live, "reset", "--hard", initial_head)
            (live / "sibling.txt").write_text("sibling\n", encoding="utf-8")
            git_run(live, "add", "sibling.txt")
            git_run(live, "commit", "-m", "non-descendant target")
            sibling_head = git_run(live, "rev-parse", "HEAD").stdout.strip()
            sibling_candidate = advance_head(current, sibling_head)
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "does not descend"):
                ledger.cas(
                    root, current.name, sibling_candidate, **cas_arguments(current)
                )
            self.assertEqual(directory_snapshot(root), before)

            git_run(live, "reset", "--hard", latest_head)
            (live / "second.txt").write_text("second\n", encoding="utf-8")
            git_run(live, "add", "second.txt")
            git_run(live, "commit", "-m", "second target advancement")
            second_head = git_run(live, "rev-parse", "HEAD").stdout.strip()
            wrong_merge_base = advance_head(current, second_head)
            wrong_merge_base["targets"][0]["merge_base"]["current_sha"] = second_head
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "merge-base coordinate"):
                ledger.cas(
                    root, current.name, wrong_merge_base, **cas_arguments(current)
                )
            self.assertEqual(directory_snapshot(root), before)

            second_candidate = advance_head(current, second_head)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.cas(
                    root,
                    current.name,
                    second_candidate,
                    failpoint="cas:renamed",
                    **cas_arguments(current),
                )
            installed = ledger.inspect(root, current.name)
            self.assertEqual(
                installed.document["targets"][0]["head"]["current_sha"], second_head
            )
            (live / "third.txt").write_text("third\n", encoding="utf-8")
            git_run(live, "add", "third.txt")
            git_run(live, "commit", "-m", "post-crash target advancement")
            third_head = git_run(live, "rev-parse", "HEAD").stdout.strip()
            resumed = ledger.cas(
                root, current.name, second_candidate, **cas_arguments(current)
            )
            self.assertEqual(resumed.digest, installed.digest)
            self.assertEqual(ledger.inventory(root).pending, ())
            third_candidate = advance_head(resumed, third_head)
            with self.assertRaises(ledger.InjectedCrash):
                ledger.cas(
                    root,
                    resumed.name,
                    third_candidate,
                    failpoint="cas:installed",
                    **cas_arguments(resumed),
                )
            final = ledger.cas(
                root, resumed.name, third_candidate, **cas_arguments(resumed)
            )
            self.assertEqual(
                final.document["targets"][0]["head"]["current_sha"], third_head
            )

    def test_49_exact_target_head_correction_is_audited_and_resumable(self) -> None:
        bad_head = "f0f8d7493278dc691710056c79d0d63f1d802488"

        def incident(
            root: Path,
            label: str,
            *,
            bad_kind: str = "missing",
            mirror_pull_request: bool = False,
            producer: str = "primitive",
        ) -> tuple[object, object, str, str, Path]:
            checkout = "client" if producer == "scope" else "atrinik"
            roots = live_roots(self.live_base / label, checkout)
            base = git_head(roots)
            branch = f"fix/{label}"
            worktree_path = str(
                Path(roots["workspace"]["path"])
                / "worktrees"
                / checkout
                / label
            )
            document = issue_ledger(
                number=445,
                issue_node=f"I_{label.replace('-', '_')}",
                branch=branch,
                worktree=worktree_path,
            )
            replace_sha(document, SHA_A, base)
            worktree = next(
                slot for slot in document["artifacts"] if slot["kind"] == "worktree"
            )
            request = worktree["primitive_request"]
            request["roots"] = copy.deepcopy(roots)
            if producer == "scope":
                request = scope_request(
                    name=f"{label}-scope",
                    component="client",
                    checkout="client",
                    label=label,
                    branch=branch,
                    start_sha=base,
                    roots=roots,
                )
                document["resources"] = [scope_resource(request)]
                worktree["primitive_request"] = None
                worktree["producer_resource_slot"] = "scope"
                retarget_repository(document, repository("client", "R_client"))
            live = live_worktree_path(request)
            first = ledger.create(root, document)
            worktree_list = worktree_list_bytes(request)
            if producer == "scope":
                scope_show = scope_show_bytes(
                    request, repository_name="atrinik/client"
                )
                install_scope_references(request, scope_show)
                safety = safety_observation_bytes(
                    request,
                    worktree_list,
                    producer_kind="scope",
                    producer_digest=ledger.byte_digest(scope_show),
                    repository_value=repository("client", "R_client"),
                )
                ledger.bind_scope_cas(
                    root,
                    first.name,
                    "scope",
                    scope_show,
                    worktree_list,
                    safety,
                    **cas_arguments(first),
                )
            else:
                safety = safety_observation_bytes(
                    request,
                    worktree_list,
                    producer_kind="primitive",
                    producer_digest=None,
                )
                ledger.bind_worktree_cas(
                    root,
                    first.name,
                    "worktree",
                    worktree_list,
                    safety,
                    **cas_arguments(first),
                )
            predecessor = ledger.inspect(root, first.name)
            if mirror_pull_request:
                predecessor = ledger.cas(
                    root,
                    predecessor.name,
                    bind_issue_created_pr(predecessor),
                    **cas_arguments(predecessor),
                )
            (live / "correction.txt").write_text("actual\n", encoding="utf-8")
            git_run(live, "add", "correction.txt")
            git_run(live, "commit", "-m", "actual correction head")
            actual = git_run(live, "rev-parse", "HEAD").stdout.strip()
            incident_bad_head = bad_head
            if bad_kind == "blob":
                incident_bad_head = git_run(
                    live, "rev-parse", "HEAD:correction.txt"
                ).stdout.strip()
            elif bad_kind == "tree":
                incident_bad_head = git_run(
                    live, "rev-parse", "HEAD^{tree}"
                ).stdout.strip()
            erroneous_document = next_generation(predecessor)
            target_head = erroneous_document["targets"][0]["head"]
            target_head["current_sha"] = incident_bad_head
            target_head["lineage"].append(incident_bad_head)
            mirrored_kinds = {"branch", "worktree"}
            if mirror_pull_request:
                mirrored_kinds.add("pull_request")
            for slot in erroneous_document["artifacts"]:
                if slot["kind"] in mirrored_kinds:
                    slot["current"]["head_sha"] = incident_bad_head
            # Model exact bytes published by the pre-fix generic CAS.  The
            # public CAS must now reject this nonexistent coordinate; recovery
            # still needs a faithful historical incident fixture.
            erroneous = ledger._cas_install(
                root,
                predecessor.name,
                erroneous_document,
                **cas_arguments(predecessor),
            )
            return predecessor, erroneous, actual, incident_bad_head, live

        failpoints = (
            "correct-target-head:predecessor-snapshot",
            "correct-target-head:erroneous-snapshot",
            "correct-target-head:staged",
            "correct-target-head:receipt",
            "correct-target-head:renamed",
            "correct-target-head:installed",
        )
        for producer in ("primitive", "scope"):
            for index, failpoint in enumerate(failpoints):
                with self.subTest(
                    producer=producer, failpoint=failpoint
                ), tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    predecessor, erroneous, actual, incident_bad, _ = incident(
                        root, f"correction-{producer}-{index}", producer=producer
                    )
                    arguments = {
                        **cas_arguments(erroneous),
                        "bad_head": incident_bad,
                        "actual_head": actual,
                    }
                    recovery = head_correction_recovery(
                        predecessor, erroneous, actual, incident_bad
                    )
                    with self.assertRaises(ledger.InjectedCrash):
                        ledger.correct_target_head(
                            root,
                            erroneous.name,
                            predecessor.raw,
                            recovery,
                            failpoint=failpoint,
                            **arguments,
                        )
                    if failpoint == "correct-target-head:renamed":
                        with mock.patch.object(
                            ledger, "_fsync", wraps=ledger._fsync
                        ) as fsync:
                            corrected = ledger.correct_target_head(
                                root,
                                erroneous.name,
                                predecessor.raw,
                                recovery,
                                **arguments,
                            )
                        self.assertTrue(
                            any(
                                "resuming correction" in call.args[1]
                                for call in fsync.call_args_list
                            )
                        )
                    else:
                        corrected = ledger.correct_target_head(
                            root,
                            erroneous.name,
                            predecessor.raw,
                            recovery,
                            **arguments,
                        )
                    self.assertEqual(
                        corrected.document["generation"],
                        erroneous.document["generation"] + 1,
                    )
                    self.assertEqual(
                        corrected.document["history"][-1], erroneous.digest
                    )
                    self.assertEqual(
                        corrected.document["targets"][0]["head"]["current_sha"],
                        actual,
                    )
                    self.assertEqual(
                        corrected.document["targets"][0]["head"]["lineage"],
                        [
                            *predecessor.document["targets"][0]["head"]["lineage"],
                            actual,
                        ],
                    )
                    self.assertEqual(ledger.inventory(root).pending, ())
                    self.assertEqual(
                        ledger.correct_target_head(
                            root,
                            erroneous.name,
                            predecessor.raw,
                            recovery,
                            **arguments,
                        ).digest,
                        corrected.digest,
                    )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            predecessor, erroneous, actual, incident_bad, _ = incident(
                root, "correction-scope-controls", producer="scope"
            )

            def scope_resource_of(document: dict[str, object]) -> dict[str, object]:
                return next(
                    resource
                    for resource in document["resources"]
                    if resource["slot_id"] == "scope"
                )

            def scope_worktree_of(document: dict[str, object]) -> dict[str, object]:
                return next(
                    artifact
                    for artifact in document["artifacts"]
                    if artifact["kind"] == "worktree"
                )

            def alter_both(expected: str, mutation: object) -> None:
                before = copy.deepcopy(predecessor.document)
                after = copy.deepcopy(erroneous.document)
                assert callable(mutation)
                mutation(before)
                mutation(after)
                before_digest = ledger.byte_digest(ledger.canonical_bytes(before))
                after["previous_byte_digest"] = before_digest
                after["history"] = [*before["history"], before_digest]
                after_digest = ledger.byte_digest(ledger.canonical_bytes(after))
                with self.assertRaises(ledger.LedgerError) as rejected:
                    ledger._head_correction_document(
                        before,
                        after,
                        after_digest,
                        bad_head=incident_bad,
                        actual_head=actual,
                    )
                self.assertNotIn(
                    "exact immediate predecessor", str(rejected.exception)
                )
                self.assertIn(expected, str(rejected.exception))

            scope_controls = (
                (
                    "missing producer",
                    "one exact scope producer",
                    lambda document: scope_worktree_of(document).__setitem__(
                        "producer_resource_slot", None
                    ),
                ),
                (
                    "non-scope producer",
                    "one exact scope producer",
                    lambda document: scope_resource_of(document).__setitem__(
                        "kind", "profile"
                    ),
                ),
                (
                    "inactive producer",
                    "scope producer is not active",
                    lambda document: scope_resource_of(document)["current"].__setitem__(
                        "lifecycle", "released"
                    ),
                ),
                (
                    "repository",
                    "scope repository differs from its target",
                    lambda document: scope_resource_of(document)["immutable"][
                        "repository"
                    ].__setitem__("node_id", "R_other"),
                ),
                (
                    "branch",
                    "worktree row differs from exact scope request",
                    lambda document: scope_resource_of(document)["request"].__setitem__(
                        "branch", "fix/other"
                    ),
                ),
                (
                    "head",
                    "worktree row differs from exact scope request",
                    lambda document: scope_resource_of(document)["request"].__setitem__(
                        "start_sha", SHA_C
                    ),
                ),
                (
                    "path",
                    "scope worktree differs from its target",
                    lambda document: scope_worktree_of(document)["current"].__setitem__(
                        "path", "/wrong/worktree"
                    ),
                ),
                (
                    "binding evidence",
                    "digest does not match retained bytes",
                    lambda document: scope_resource_of(document)["current"][
                        "binding"
                    ].__setitem__("sha256", "0" * 64),
                ),
                (
                    "observation evidence",
                    "digest does not match retained bytes",
                    lambda document: scope_resource_of(document)["current"][
                        "observation"
                    ]["safety_observation"].__setitem__("sha256", "0" * 64),
                ),
            )
            for label, expected, mutation in scope_controls:
                with self.subTest(scope_control=label):
                    alter_both(expected, mutation)

            scope = scope_resource_of(erroneous.document)
            scope_file = (
                Path(scope["request"]["roots"]["workspace"]["path"])
                / "scopes"
                / scope["request"]["name"]
                / "scope.json"
            )
            retained_scope = scope_file.read_bytes()
            drifted_scope = json.loads(retained_scope)
            drifted_scope["generation"] = "2" * 32
            scope_file.write_bytes(json_bytes(drifted_scope))
            recovery = head_correction_recovery(
                predecessor, erroneous, actual, incident_bad
            )
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "scope file differs"):
                ledger.correct_target_head(
                    root,
                    erroneous.name,
                    predecessor.raw,
                    recovery,
                    **cas_arguments(erroneous),
                    bad_head=incident_bad,
                    actual_head=actual,
                )
            self.assertEqual(directory_snapshot(root), before)
            scope_file.write_bytes(retained_scope)

        with (
            self.subTest(delivery_created_pull_request=True),
            tempfile.TemporaryDirectory() as temporary,
        ):
            root = Path(temporary)
            predecessor, erroneous, actual, incident_bad, _ = incident(
                root,
                "correction-delivery-created-pr",
                mirror_pull_request=True,
            )
            original_module_loader = ledger._load_workspace_module

            def load_with_legacy_primary_api(wrapper_root: str):
                module = original_module_loader(wrapper_root)

                class LegacyWorkspace(module.Workspace):
                    def __init__(
                        self,
                        repository: Path,
                        *,
                        backfill_references: bool = True,
                    ) -> None:
                        super().__init__(
                            repository,
                            backfill_references=backfill_references,
                        )

                    def _source_references(self, source_root: Path) -> list[str]:
                        return super()._source_references(source_root)

                return type(
                    "LegacyPrimaryWorkspaceModule",
                    (),
                    {
                        "Manifest": module.Manifest,
                        "Workspace": LegacyWorkspace,
                    },
                )

            with mock.patch.object(
                ledger,
                "_load_workspace_module",
                side_effect=load_with_legacy_primary_api,
            ):
                corrected = ledger.correct_target_head(
                    root,
                    erroneous.name,
                    predecessor.raw,
                    head_correction_recovery(
                        predecessor, erroneous, actual, incident_bad
                    ),
                    **cas_arguments(erroneous),
                    bad_head=incident_bad,
                    actual_head=actual,
                )
            self.assertEqual(
                {
                    slot["kind"]: slot["current"]["head_sha"]
                    for slot in corrected.document["artifacts"]
                    if slot["kind"] in {"branch", "pull_request", "worktree"}
                },
                {
                    "branch": actual,
                    "pull_request": actual,
                    "worktree": actual,
                },
            )

            def pull_request_slot(document: dict[str, object]) -> dict[str, object]:
                return next(
                    slot
                    for slot in document["artifacts"]
                    if slot["kind"] == "pull_request"
                )

            def reject_adopted(
                before: dict[str, object], after: dict[str, object]
            ) -> None:
                for document in (before, after):
                    slot = pull_request_slot(document)
                    slot["state"] = "adopted"
                    slot["immutable"]["number"] = slot["current"]["number"]
                    slot["immutable"]["node_id"] = slot["current"]["node_id"]

            def reject_contributor_body(
                before: dict[str, object], after: dict[str, object]
            ) -> None:
                for document in (before, after):
                    document["selected_prs"][0]["body"]["ownership"] = (
                        "contributor-owned"
                    )

            def reject_foreign_head_repository(
                before: dict[str, object], after: dict[str, object]
            ) -> None:
                for document in (before, after):
                    document["selected_prs"][0]["head_repository"] = repository(
                        "fork", "R_fork"
                    )

            def reject_actor_mismatch(
                before: dict[str, object], after: dict[str, object]
            ) -> None:
                for document in (before, after):
                    document["selected_prs"][0]["author_node_id"] = "U_other"

            def reject_additional_current_change(
                _before: dict[str, object], after: dict[str, object]
            ) -> None:
                pull_request_slot(after)["current"]["body_digest"] = "9" * 64

            rejection_cases = (
                ("adopted", reject_adopted),
                ("contributor-owned", reject_contributor_body),
                ("foreign-head", reject_foreign_head_repository),
                ("actor-mismatch", reject_actor_mismatch),
                ("additional-current-change", reject_additional_current_change),
            )
            for label, mutation in rejection_cases:
                with self.subTest(pull_request_rejection=label):
                    rejected_predecessor = copy.deepcopy(predecessor.document)
                    rejected_erroneous = copy.deepcopy(erroneous.document)
                    mutation(rejected_predecessor, rejected_erroneous)
                    before = directory_snapshot(root)
                    with self.assertRaises(ledger.LedgerError):
                        ledger._head_correction_document(
                            rejected_predecessor,
                            rejected_erroneous,
                            ledger.canonical_object_digest(rejected_erroneous),
                            bad_head=incident_bad,
                            actual_head=actual,
                        )
                    self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            predecessor, erroneous, actual, incident_bad, live = incident(
                root, "correction-invalid"
            )
            recovery = head_correction_recovery(predecessor, erroneous, actual, bad_head)
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "differs from expected HEAD"):
                invalid_recovery = head_correction_recovery(
                    predecessor, erroneous, SHA_C, bad_head
                )
                ledger.correct_target_head(
                    root,
                    erroneous.name,
                    predecessor.raw,
                    invalid_recovery,
                    **cas_arguments(erroneous),
                    bad_head=bad_head,
                    actual_head=SHA_C,
                )
            self.assertEqual(directory_snapshot(root), before)

            def altered_recovery(
                mutation: object, *, rebind_objective: bool = True
            ) -> bytes:
                value = json.loads(recovery)
                assert callable(mutation)
                mutation(value)
                if rebind_objective:
                    value["grant"]["objective_sha256"] = ledger.canonical_object_digest(
                        value["intent"]
                    )
                return ledger.canonical_bytes(value)

            authority_controls = (
                (
                    "grant-kind",
                    lambda value: value["grant"].__setitem__("kind", "durable-goal"),
                    True,
                ),
                (
                    "objective",
                    lambda value: value["grant"].__setitem__(
                        "objective_sha256", "0" * 64
                    ),
                    False,
                ),
                (
                    "grant-actor",
                    lambda value: value["grant"].__setitem__(
                        "actor_node_id", "U_other"
                    ),
                    True,
                ),
                (
                    "grant-repository",
                    lambda value: value["grant"]["allowed"].__setitem__(
                        "repositories", ["R_other"]
                    ),
                    True,
                ),
                (
                    "grant-issue",
                    lambda value: value["grant"]["allowed"].__setitem__(
                        "issues", ["I_other"]
                    ),
                    True,
                ),
                (
                    "installed-generation",
                    lambda value: value["intent"]["installed"].__setitem__(
                        "generation", erroneous.document["generation"] + 1
                    ),
                    True,
                ),
                (
                    "installed-digest",
                    lambda value: value["intent"]["installed"].__setitem__(
                        "sha256", "0" * 64
                    ),
                    True,
                ),
                (
                    "installed-device",
                    lambda value: value["intent"]["installed"].__setitem__(
                        "device", erroneous.device + 1
                    ),
                    True,
                ),
                (
                    "installed-inode",
                    lambda value: value["intent"]["installed"].__setitem__(
                        "inode", erroneous.inode + 1
                    ),
                    True,
                ),
                (
                    "ledger-name",
                    lambda value: value["intent"].__setitem__(
                        "target", "atrinik-atrinik-issue-446.md.ledger.json"
                    ),
                    True,
                ),
                (
                    "predecessor",
                    lambda value: value["intent"].__setitem__(
                        "predecessor_sha256", "0" * 64
                    ),
                    True,
                ),
                (
                    "target-repository",
                    lambda value: value["intent"]["repository"].__setitem__(
                        "node_id", "R_other"
                    ),
                    True,
                ),
                (
                    "branch",
                    lambda value: value["intent"].__setitem__(
                        "branch", "fix/other"
                    ),
                    True,
                ),
                (
                    "worktree",
                    lambda value: value["intent"].__setitem__(
                        "worktree", "/wrong/worktree"
                    ),
                    True,
                ),
                (
                    "bad-head",
                    lambda value: value["intent"].__setitem__(
                        "bad_head", "1" * 40
                    ),
                    True,
                ),
                (
                    "actual-head",
                    lambda value: value["intent"].__setitem__(
                        "actual_head", "2" * 40
                    ),
                    True,
                ),
                (
                    "scope-actor",
                    lambda value: value["intent"]["ledger_scope"]["actor"].__setitem__(
                        "node_id", "U_other"
                    ),
                    True,
                ),
                (
                    "scope-repository",
                    lambda value: value["intent"]["ledger_scope"]["repositories"][
                        0
                    ].__setitem__("node_id", "R_other"),
                    True,
                ),
                (
                    "scope-issue",
                    lambda value: value["intent"]["ledger_scope"]["issues"][
                        0
                    ].__setitem__("node_id", "I_other"),
                    True,
                ),
            )
            for label, mutation, rebind_objective in authority_controls:
                with self.subTest(authority_control=label):
                    before = directory_snapshot(root)
                    with self.assertRaises(ledger.LedgerError):
                        ledger.correct_target_head(
                            root,
                            erroneous.name,
                            predecessor.raw,
                            altered_recovery(
                                mutation, rebind_objective=rebind_objective
                            ),
                            **cas_arguments(erroneous),
                            bad_head=bad_head,
                            actual_head=actual,
                        )
                    self.assertEqual(directory_snapshot(root), before)
            noncanonical_recovery = (
                json.dumps(json.loads(recovery), indent=2, sort_keys=True) + "\n"
            ).encode()
            before = directory_snapshot(root)
            with self.assertRaisesRegex(ledger.LedgerError, "not canonical"):
                ledger.correct_target_head(
                    root,
                    erroneous.name,
                    predecessor.raw,
                    noncanonical_recovery,
                    **cas_arguments(erroneous),
                    bad_head=bad_head,
                    actual_head=actual,
                )
            self.assertEqual(directory_snapshot(root), before)

            predecessor_input = root / "predecessor.json"
            predecessor_input.write_bytes(predecessor.raw)
            recovery_input = root / "recovery.json"
            recovery_input.write_bytes(recovery)
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(SCRIPT),
                    "correct-target-head",
                    str(root),
                    erroneous.name,
                    str(predecessor_input),
                    str(recovery_input),
                    "--expected-generation",
                    str(erroneous.document["generation"]),
                    "--expected-digest",
                    erroneous.digest,
                    "--expected-device",
                    str(erroneous.device),
                    "--expected-inode",
                    str(erroneous.inode),
                    "--bad-head",
                    bad_head,
                    "--actual-head",
                    actual,
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertEqual(json.loads(process.stdout)["document"]["generation"], 4)

            receipt_path = root / (
                f".{erroneous.name}.correct-target-head-{erroneous.digest}.json"
            )
            receipt_raw = receipt_path.read_bytes()
            tampered = json.loads(receipt_raw)
            tampered["source"]["device"] += 1
            receipt_path.write_bytes(ledger.canonical_bytes(tampered))
            with self.assertRaises(ledger.LedgerError):
                ledger.inventory(root)
            receipt_path.write_bytes(receipt_raw)
            tampered = json.loads(receipt_raw)
            tampered["source"]["inode"] += 1
            tampered["recovery"]["intent"]["installed"]["inode"] += 1
            receipt_path.write_bytes(ledger.canonical_bytes(tampered))
            with self.assertRaises(ledger.LedgerError):
                ledger.inventory(root)
            receipt_path.write_bytes(receipt_raw)
            tampered = json.loads(receipt_raw)
            tampered["source"]["inode"] += 1
            tampered["erroneous_snapshot"]["inode"] += 1
            tampered["recovery"]["intent"]["installed"]["inode"] += 1
            tampered["recovery"]["grant"]["objective_sha256"] = (
                ledger.canonical_object_digest(tampered["recovery"]["intent"])
            )
            receipt_path.write_bytes(ledger.canonical_bytes(tampered))
            with self.assertRaises(ledger.LedgerError):
                ledger.inventory(root)
            receipt_path.write_bytes(receipt_raw)

            current = ledger.inspect(root, erroneous.name)
            for index in range(2):
                (live / f"later-{index}.txt").write_text(
                    f"later {index}\n", encoding="utf-8"
                )
                git_run(live, "add", f"later-{index}.txt")
                git_run(live, "commit", "-m", f"later correction head {index}")
                later_head = git_run(live, "rev-parse", "HEAD").stdout.strip()
                update = next_generation(current)
                target_head = update["targets"][0]["head"]
                target_head["current_sha"] = later_head
                target_head["lineage"].append(later_head)
                for slot in update["artifacts"]:
                    if slot["kind"] in {"branch", "worktree"}:
                        slot["current"]["head_sha"] = later_head
                current = ledger.cas(
                    root, current.name, update, **cas_arguments(current)
                )
                self.assertEqual(ledger.inventory(root).pending, ())

        for bad_kind in ("blob", "tree"):
            with self.subTest(existing_object=bad_kind), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                predecessor, erroneous, actual, object_oid, _ = incident(
                    root, f"correction-{bad_kind}", bad_kind=bad_kind
                )
                recovery = head_correction_recovery(
                    predecessor, erroneous, actual, object_oid
                )
                before = directory_snapshot(root)
                with self.assertRaisesRegex(ledger.LedgerError, "canonically absent"):
                    ledger.correct_target_head(
                        root,
                        erroneous.name,
                        predecessor.raw,
                        recovery,
                        **cas_arguments(erroneous),
                        bad_head=object_oid,
                        actual_head=actual,
                    )
                self.assertEqual(directory_snapshot(root), before)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            predecessor, erroneous, actual, incident_bad, _ = incident(
                root, "correction-batch-failure"
            )
            recovery = head_correction_recovery(
                predecessor, erroneous, actual, incident_bad
            )
            before = directory_snapshot(root)
            original_git = ledger._git

            def failed_batch_check(
                descriptor: int, arguments: object, context: str, **keywords: object
            ) -> tuple[int, bytes]:
                if tuple(arguments) == ("cat-file", "--batch-check"):
                    self.assertEqual(
                        keywords["input_bytes"], f"{incident_bad}\n".encode("ascii")
                    )
                    raise ledger.LedgerError("simulated batch-check failure")
                return original_git(descriptor, arguments, context, **keywords)

            with mock.patch.object(ledger, "_git", side_effect=failed_batch_check):
                with self.assertRaisesRegex(ledger.LedgerError, "batch-check failure"):
                    ledger.correct_target_head(
                        root,
                        erroneous.name,
                        predecessor.raw,
                        recovery,
                        **cas_arguments(erroneous),
                        bad_head=incident_bad,
                        actual_head=actual,
                    )
            self.assertEqual(directory_snapshot(root), before)

if __name__ == "__main__":
    unittest.main()
