"""Dependency and resource scheduling; leaf delivery remains the ownership authority."""
from __future__ import annotations

import copy
from datetime import datetime, timezone
import hashlib
import json
import re
from pathlib import PurePosixPath
from typing import Any

MAX_NODES = 256
COORDINATE = re.compile(r"atrinik/[a-z0-9][a-z0-9._-]*#[1-9][0-9]*")
SHA = re.compile(r"[0-9a-f]{40}")
STATES = {"pending", "reserved", "running", "blocked", "ready", "merged", "accepted", "external"}
CONDITIONS = {"ready", "merged", "accepted"}


class ProjectError(ValueError):
    """A project cannot safely advance from the observed state."""


def canonical(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
                       allow_nan=False) + "\n").encode()


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ProjectError(message)


def keys(value: Any, expected: set[str], context: str) -> None:
    require(isinstance(value, dict) and set(value) == expected, f"invalid {context} fields")


def coordinate(value: Any) -> str:
    require(isinstance(value, str) and COORDINATE.fullmatch(value) is not None,
            "expected exact atrinik/repository#number")
    return value


def boundary(value: Any) -> str:
    require(isinstance(value, str) and bool(value), "empty file boundary")
    path = PurePosixPath(value)
    require(not path.is_absolute() and ".." not in path.parts and "\\" not in value
            and str(path) == value and not any(ord(c) < 32 for c in value),
            "file boundary must be a normalized repository-relative path")
    return value


def validate_plan(plan: dict) -> None:
    keys(plan, {"parent", "repositories", "nodes", "acceptance", "permissions"}, "plan")
    coordinate(plan["parent"])
    repos = plan["repositories"]
    require(isinstance(repos, list) and 0 < len(repos) <= MAX_NODES
            and len(set(repos)) == len(repos), "invalid repository allowlist")
    for repo in repos:
        coordinate(f"{repo}#1")
    require(plan["parent"].split("#")[0] in repos, "parent outside repository scope")
    permissions = plan["permissions"]
    require(isinstance(permissions, list) and len(permissions) == len(set(permissions))
            and set(permissions) <= {"assign", "comment", "link", "dependency",
                                     "create-child", "close-parent", "project-status"},
            "unsupported permission; merge/deploy are never coordinator operations")
    nodes = plan["nodes"]
    require(isinstance(nodes, list) and 0 < len(nodes) <= MAX_NODES, "invalid node count")
    seen = set()
    for node in nodes:
        keys(node, {"id", "entry_mode", "dependencies", "reads", "writes", "resources",
                    "heavy", "external"}, "node")
        ident = coordinate(node["id"])
        require(ident not in seen and ident != plan["parent"], "duplicate or parent leaf")
        seen.add(ident)
        require(ident.split("#")[0] in repos, "leaf outside repository scope")
        require(node["entry_mode"] in {"issue", "PR"}, "type-explicit issue or PR required")
        require(type(node["heavy"]) is bool and type(node["external"]) is bool, "invalid flags")
        for kind in ("reads", "writes", "resources"):
            values = node[kind]
            require(isinstance(values, list) and len(values) <= 128
                    and len(set(values)) == len(values), "invalid boundary/resource list")
            for value in values:
                boundary(value)
        require(isinstance(node["dependencies"], list), "invalid dependencies")
        dep_ids = set()
        for dep in node["dependencies"]:
            keys(dep, {"id", "condition"}, "dependency")
            coordinate(dep["id"])
            require(dep["id"] not in dep_ids and dep["condition"] in CONDITIONS,
                    "duplicate or invalid dependency")
            dep_ids.add(dep["id"])
    visiting, visited = set(), set()
    by_id = {n["id"]: n for n in nodes}

    def visit(ident: str) -> None:
        require(ident not in visiting, "dependency cycle")
        if ident in visited:
            return
        visiting.add(ident)
        for dep in by_id[ident]["dependencies"]:
            require(dep["id"] in by_id, "missing dependency owner")
            visit(dep["id"])
        visiting.remove(ident)
        visited.add(ident)

    for ident in by_id:
        visit(ident)
    require(isinstance(plan["acceptance"], list) and 0 < len(plan["acceptance"]) <= 256,
            "explicit terminal acceptance required")
    acceptance_ids = set()
    for item in plan["acceptance"]:
        keys(item, {"id", "description", "owners"}, "acceptance")
        require(isinstance(item["id"], str) and re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", item["id"])
                and item["id"] not in acceptance_ids, "invalid acceptance identity")
        acceptance_ids.add(item["id"])
        require(isinstance(item["description"], str) and 0 < len(item["description"]) <= 4096,
                "invalid acceptance description")
        require(isinstance(item["owners"], list) and bool(item["owners"])
                and set(item["owners"]) <= seen, "acceptance lacks declared owners")


def new_project(plan: dict, actor: str, authority: str) -> dict:
    validate_plan(plan)
    require(bool(re.fullmatch(r"[a-z0-9][a-z0-9-]{0,38}", actor)), "invalid actor")
    require(isinstance(authority, str) and 0 < len(authority) <= 4096, "explicit authority required")
    return {"schema_version": 1, "generation": 1, "previous": None, "actor": actor,
            "authority": authority, "plan": copy.deepcopy(plan), "observations": {},
            "nodes": {n["id"]: {"state": "external" if n["external"] else "pending",
                                "worker": None, "attempt": None, "detail": ""}
                      for n in plan["nodes"]},
            "attestations": {}, "operations": {}}


def validate_project(project: dict) -> None:
    keys(project, {"schema_version", "generation", "previous", "actor", "authority", "plan",
                   "observations", "nodes", "attestations", "operations"}, "project")
    require(type(project["schema_version"]) is int and project["schema_version"] == 1,
            "unsupported project schema")
    require(type(project["generation"]) is int and project["generation"] > 0, "invalid generation")
    require(project["previous"] is None or isinstance(project["previous"], str)
            and re.fullmatch(r"[0-9a-f]{64}", project["previous"]) is not None, "invalid history digest")
    validate_plan(project["plan"])
    require(isinstance(project["actor"], str) and re.fullmatch(r"[a-z0-9][a-z0-9-]{0,38}", project["actor"])
            and isinstance(project["authority"], str) and 0 < len(project["authority"]) <= 4096,
            "invalid project authority")
    require(set(project["nodes"]) == {n["id"] for n in project["plan"]["nodes"]}, "node set drift")
    for state in project["nodes"].values():
        keys(state, {"state", "worker", "attempt", "detail"}, "node state")
        require(state["state"] in STATES, "invalid node state")
        require(state["worker"] is None or isinstance(state["worker"], str), "invalid worker")
        require(state["attempt"] is None or isinstance(state["attempt"], str), "invalid attempt")
        require(isinstance(state["detail"], str) and len(state["detail"]) <= 8192, "invalid detail")
    require(isinstance(project["observations"], dict) and isinstance(project["attestations"], dict)
            and isinstance(project["operations"], dict), "invalid evidence maps")
    require(len(project["operations"]) <= 512, "operation journal bound exceeded")
    for ident, op in project["operations"].items():
        keys(op, {"id", "kind", "target", "payload", "path", "method", "request", "marker",
                  "phase", "before", "result"}, "tracking operation")
        require(ident == op["id"] and re.fullmatch(r"[0-9a-f]{64}", ident) is not None,
                "invalid operation identity")
        require(op["kind"] in project["plan"]["permissions"]
                and op["phase"] in {"planned", "in-flight", "bound", "cancelled"}, "invalid tracking state")
        coordinate(op["target"])
        require(op["marker"] == f"<!-- atrinik-project:{ident} -->", "operation marker mismatch")
        require(isinstance(op["payload"], dict) and isinstance(op["request"], dict)
                and isinstance(op["before"], dict), "invalid operation payload")
        require((op["phase"] == "bound") == (op["result"] is not None), "invalid operation result")
        require(op["method"] == ("PATCH" if op["kind"] == "close-parent" else "POST"),
                "unsupported tracking method")
        repo, number = op["target"].split("#")
        suffixes = {"assign": "/assignees", "comment": "/comments", "link": "/sub_issues",
                    "dependency": "/dependencies/blocked_by", "close-parent": ""}
        path = ("graphql" if op["kind"] == "project-status" else
                f"repos/{op['payload'].get('repository')}/issues" if op["kind"] == "create-child"
                else f"repos/{repo}/issues/{number}" + suffixes[op["kind"]])
        require(op["path"] == path, "tracking route mismatch")
        if op["kind"] == "create-child":
            require(op["payload"].get("repository") in project["plan"]["repositories"],
                    "child repository outside scope")
        from .project_coordinator_github import validate_operation
        validate_operation(project, op)


def overlaps(left: str, right: str) -> bool:
    return left == "." or right == "." or left == right or left.startswith(right + "/") or right.startswith(left + "/")


def conflict(a: dict, b: dict) -> bool:
    if set(a["resources"]) & set(b["resources"]):
        return True
    if a["id"].split("#")[0] != b["id"].split("#")[0]:
        return False
    return any(overlaps(x, y) for x in a["writes"] for y in b["writes"] + b["reads"]) or any(
        overlaps(x, y) for x in b["writes"] for y in a["reads"])


def condition_met(project: dict, ident: str, condition: str) -> bool:
    state = project["nodes"][ident]["state"]
    return state in {"ready", "merged", "accepted"} if condition == "ready" else (
        state in {"merged", "accepted"} if condition == "merged" else state == "accepted" and
        all(acceptance_valid(project, a) for a in project["plan"]["acceptance"] if ident in a["owners"]))


def occupied_attempt(state: dict) -> bool:
    return state["state"] in {"reserved", "running"} or state["state"] == "blocked" and state["attempt"] is not None


def occupies_resources(project: dict, node: dict) -> bool:
    return occupied_attempt(project["nodes"][node["id"]]) or (
        node["external"] and not project["observations"].get(node["id"], {}).get("terminal"))


def schedule(project: dict, capacity: int, heavy_limit: int, open_workers: int = 0) -> dict:
    validate_project(project)
    require(all(type(n) is int and 0 <= n <= 256 for n in (capacity, heavy_limit, open_workers)),
            "invalid observed capacity")
    nodes = project["plan"]["nodes"]
    active = [n for n in nodes if occupied_attempt(project["nodes"][n["id"]])]
    occupied = [n for n in nodes if occupies_resources(project, n)]
    # Observed capacity includes all open subagents, including completed but unclosed ones.
    unbound = sum(project["nodes"][n["id"]]["worker"] is None for n in active)
    bound = len(active) - unbound
    free = max(0, capacity - max(open_workers, bound) - unbound)
    heavy = sum(n["heavy"] for n in occupied)
    selected, blocked = [], {}
    for node in nodes:
        ident = node["id"]
        state = project["nodes"][ident]["state"]
        if node["external"]:
            blocked[ident] = "external ownership"
            continue
        if state != "pending":
            if state in {"external", "blocked", "reserved"}:
                blocked[ident] = state
            continue
        if any(not condition_met(project, d["id"], d["condition"]) for d in node["dependencies"]):
            blocked[ident] = "dependency"
        elif any(conflict(node, other) for other in occupied + selected):
            blocked[ident] = "file-or-resource-conflict"
        elif len(selected) >= free or node["heavy"] and heavy >= heavy_limit:
            blocked[ident] = "capacity"
        else:
            selected.append(node)
            heavy += node["heavy"]
    return {"ready": [n["id"] for n in selected], "blocked": blocked}


def reserve(project: dict, capacity: int, heavy_limit: int, open_workers: int = 0) -> list[dict]:
    plan = schedule(project, capacity, heavy_limit, open_workers)
    requests = []
    by_id = {n["id"]: n for n in project["plan"]["nodes"]}
    for ident in plan["ready"]:
        state = project["nodes"][ident]
        attempt = digest({"authority": project["authority"], "generation": project["generation"],
                          "entry": by_id[ident]})
        state.update(state="reserved", attempt=attempt, worker=None, detail="")
        requests.append({"entry_mode": by_id[ident]["entry_mode"], "coordinate": ident,
                         "attempt": attempt, "reads": by_id[ident]["reads"],
                         "writes": by_id[ident]["writes"],
                         "instruction": "Use $atrinik-issue-delivery with the exact entry mode and coordinate. "
                         "Complete live ownership/ledger/worktree gates. Do not create a nested goal."})
    return requests


def reserve_existing(project: dict, ident: str, worker: str, observation: dict,
                     expected: dict, heavy_limit: int, *, now: datetime | None = None) -> dict:
    """Reserve a retained idle handle using a fresh coordinator attestation.

    The runtime cannot authenticate this JSON. The trusted single dispatcher
    supplies the complete live inventory and correlates its own worker's retired
    attempt; leaf authority remains with the issue-delivery workflow.
    """
    validate_project(project)
    require(ident in project["nodes"], "unknown leaf")
    current = project["nodes"][ident]
    node = next(n for n in project["plan"]["nodes"] if n["id"] == ident)
    require(not node["external"] and current["state"] == "pending"
            and current["worker"] is None, "existing worker needs an unbound pending leaf")
    require(isinstance(current["attempt"], str)
            and re.fullmatch(r"[0-9a-f]{64}", current["attempt"]) is not None,
            "existing worker needs an exact retired attempt")
    require(type(heavy_limit) is int and 0 <= heavy_limit <= MAX_NODES, "invalid heavy limit")
    keys(observation, {"schema_version", "observed_at", "snapshot", "project", "runtime", "selection"},
         "runtime observation")
    require(type(observation["schema_version"]) is int and observation["schema_version"] == 1,
            "unsupported runtime observation schema")
    keys(observation["snapshot"], {"generation", "digest", "path"}, "runtime snapshot")
    require(type(observation["snapshot"]["generation"]) is int
            and observation["snapshot"] == {k: expected[k] for k in ("generation", "digest", "path")}
            and expected["generation"] == project["generation"], "stale runtime snapshot")
    keys(observation["project"], {"parent", "authority", "actor"}, "runtime project")
    require(observation["project"] == {"parent": project["plan"]["parent"],
            "authority": project["authority"], "actor": project["actor"]}, "foreign runtime project")
    timestamp = observation["observed_at"]
    require(isinstance(timestamp, str) and re.fullmatch(
        r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]{1,6})?Z", timestamp) is not None,
        "invalid runtime observation time")
    try:
        observed = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
    except ValueError as error:
        raise ProjectError("invalid runtime observation time") from error
    age = ((now or datetime.now(timezone.utc)) - observed).total_seconds()
    require(0 <= age <= 60, "runtime observation is stale or from the future")

    runtime = observation["runtime"]
    keys(runtime, {"namespace", "capacity_domain", "capacity", "complete", "agents"}, "runtime inventory")
    # This adapter uses the collaboration runtime's whole-thread tree, including
    # the root coordinator in both inventory and capacity. Nested workers belong
    # to their own dispatcher and cannot be reassigned by this operation.
    require(runtime["namespace"] == "/root" and runtime["capacity_domain"] == "whole-thread-tree"
            and runtime["complete"] is True, "incomplete or foreign runtime namespace")
    capacity = runtime["capacity"]
    require(type(capacity) is int and 1 <= capacity <= MAX_NODES, "invalid runtime capacity")
    require(isinstance(runtime["agents"], list) and 1 <= len(runtime["agents"]) <= capacity,
            "runtime inventory exceeds capacity or is empty")
    agents = {}
    for agent in runtime["agents"]:
        keys(agent, {"agent_name", "agent_status"}, "runtime agent")
        name, status = agent["agent_name"], agent["agent_status"]
        require(isinstance(name, str) and len(name) <= 256
                and re.fullmatch(r"/root(?:/[a-z0-9_]+)*", name) is not None,
                "invalid or foreign runtime worker identity")
        require(name not in agents, "duplicate runtime worker identity")
        if isinstance(status, dict):
            keys(status, {"completed"}, "completed runtime status")
            require(isinstance(status["completed"], str) and len(status["completed"]) <= 128 * 1024,
                    "invalid completed runtime result")
            # Preserve the actual tagged runtime row in the observation digest;
            # never copy its potentially private result text into project state.
            status = "completed"
        else:
            require(isinstance(status, str) and status in {"running", "idle"},
                    "unknown runtime worker state")
        agents[name] = status
    require(agents.get("/root") == "running", "runtime coordinator is missing or inactive")
    require(isinstance(worker, str) and worker in agents and PurePosixPath(worker).parent.as_posix() == "/root",
            "selected worker is absent or not owned by this dispatcher")
    require(agents[worker] in {"idle", "completed"}, "selected worker is active")
    selection = observation["selection"]
    keys(selection, {"worker", "coordinate", "entry_mode", "retired_attempt", "evidence"}, "runtime selection")
    require(all(selection[k] == v for k, v in {
        "worker": worker, "coordinate": ident, "entry_mode": node["entry_mode"],
        "retired_attempt": current["attempt"]}.items()), "runtime selection does not match the retired delivery")
    require(isinstance(selection["evidence"], str) and 0 < len(selection["evidence"].strip()) <= 4096,
            "runtime history and exact leaf ownership evidence required")
    require(not any(s["worker"] == worker for key, s in project["nodes"].items() if key != ident),
            "worker already owns another node")

    active = [s for s in project["nodes"].values() if occupied_attempt(s)]
    bound = {s["worker"] for s in active if s["worker"] is not None}
    require(bound <= agents.keys(), "occupied worker missing from runtime inventory")
    for state in project["nodes"].values():
        require(agents.get(state["worker"]) != "running" or occupied_attempt(state),
                "running runtime worker has an inactive project reservation")
    unbound = sum(s["worker"] is None for s in active)
    # Open handles do not increase, but prior unbound spawn reservations still
    # own future handles. Retained idle handles are not active workers.
    require(len(agents) + unbound <= capacity, "runtime capacity already reserved for spawning")
    running = {name for name, status in agents.items() if status == "running"}
    require(len(running | bound | {worker}) + unbound <= capacity, "runtime active capacity exhausted")
    occupied = [n for n in project["plan"]["nodes"] if n["id"] != ident and occupies_resources(project, n)]
    require(all(condition_met(project, d["id"], d["condition"]) for d in node["dependencies"]),
            "existing worker dependency gate")
    require(not any(conflict(node, other) for other in occupied), "existing worker resource conflict")
    require(not node["heavy"] or sum(n["heavy"] for n in occupied) < heavy_limit,
            "existing worker heavy capacity")
    proof = digest(observation)
    previous = current["attempt"]
    attempt = digest({"authority": project["authority"], "generation": project["generation"],
                      "coordinate": ident, "entry_mode": node["entry_mode"], "worker": worker,
                      "previous_attempt": previous, "runtime_observation_sha256": proof})
    require(attempt != previous, "existing worker requires a fresh attempt")
    current.update(state="reserved", worker=worker, attempt=attempt,
                   detail=f"Existing worker observation {proof}: {selection['evidence']}")
    return {"entry_mode": node["entry_mode"], "coordinate": ident, "worker": worker,
            "attempt": attempt, "previous_attempt": previous, "runtime_observation_sha256": proof,
            "reads": node["reads"], "writes": node["writes"], "resources": node["resources"],
            "instruction": "Recheck the actual runtime worker is idle, then follow up this exact worker and attempt. "
            "Use $atrinik-issue-delivery for this same delivery and reprove leaf ownership/ledger/worktree gates. "
            "Record worker only after accepted start; preserve an uncertain reservation and do not spawn."}


def record_worker(project: dict, ident: str, attempt: str, worker: str) -> None:
    require(ident in project["nodes"], "unknown leaf")
    state = project["nodes"][ident]
    require(state["state"] in {"reserved", "running"} and state["attempt"] == attempt,
            "stale or unreserved dispatch")
    require(isinstance(worker, str) and 0 < len(worker) <= 256, "invalid worker identity")
    require(state["worker"] in {None, worker}, "worker takeover denied")
    require(not any(s["worker"] == worker for key, s in project["nodes"].items() if key != ident),
            "worker already owns another node")
    state.update(state="running", worker=worker)


def worker_result(project: dict, ident: str, attempt: str, state: str, detail: str) -> None:
    require(state in {"blocked", "ready"}, "worker cannot assert merged/accepted")
    current = project["nodes"][ident]
    require(current["state"] == "running" and current["attempt"] == attempt, "stale worker result")
    require(isinstance(detail, str) and 0 < len(detail) <= 8192, "exact delivery evidence required")
    current.update(state=state, detail=detail)


def reopen(project: dict, ident: str, attempt: str, evidence: str, heavy_limit: int) -> dict:
    """Resume the same live owner for fresh findings without spawning a duplicate."""
    current = project["nodes"][ident]
    require(current["state"] in {"ready", "blocked"} and current["attempt"] == attempt
            and current["worker"] is not None, "reopen requires the exact retained worker")
    require(isinstance(evidence, str) and 0 < len(evidence) <= 8192, "live worker/leaf proof required")
    require(type(heavy_limit) is int and 0 <= heavy_limit <= 256, "invalid heavy limit")
    node = next(n for n in project["plan"]["nodes"] if n["id"] == ident)
    others = [n for n in project["plan"]["nodes"] if n["id"] != ident and occupies_resources(project, n)]
    require(not any(conflict(node, n) for n in others), "reopen resource conflict")
    require(not node["heavy"] or sum(n["heavy"] for n in others) < heavy_limit, "reopen heavy capacity")
    require(all(condition_met(project, d["id"], d["condition"]) for d in node["dependencies"]),
            "reopen dependency gate")
    renewed = digest({"previous_attempt": attempt, "generation": project["generation"],
                      "observations": project["observations"], "evidence": evidence})
    current.update(state="running", detail=evidence, attempt=renewed)
    return {"attempt": renewed, "worker": current["worker"]}


def requirements_identity(project: dict) -> str:
    parent = project["observations"].get(project["plan"]["parent"], {})
    implementation = {key: parent.get(key) for key in ("node_id", "body", "references", "children", "dependencies")}
    return digest({"parent_requirements": implementation, "acceptance": project["plan"]["acceptance"]})


def acceptance_observations(project: dict, owners: list[str]) -> dict:
    """Bind transitive declared and native requirements, not only direct owners."""
    by_id = {n["id"]: n for n in project["plan"]["nodes"]}
    by_node = {value.get("node_id"): ident for ident, value in project["observations"].items()
               if ident in by_id and value.get("node_id")}
    pending, proofs = list(owners), {}
    while pending:
        ident = pending.pop()
        if ident in proofs:
            continue
        observation = project["observations"].get(ident)
        require(observation and observation.get("complete") is True, "acceptance needs complete required observations")
        proofs[ident] = digest(observation)
        pending.extend(d["id"] for d in by_id[ident]["dependencies"])
        for required in observation.get("children", []) + observation.get("dependencies", []):
            require(required in by_node, "acceptance needs every native required node declared")
            pending.append(by_node[required])
    return proofs


def acceptance_valid(project: dict, item: dict) -> bool:
    proof = project["attestations"].get(item["id"])
    try:
        current = acceptance_observations(project, item["owners"])
    except ProjectError:
        return False
    return bool(proof and proof.get("requirements") == requirements_identity(project)
                and proof["observations"] == current)


def attest(project: dict, ident: str, evidence: str) -> None:
    require(ident in {a["id"] for a in project["plan"]["acceptance"]}, "unknown acceptance item")
    require(isinstance(evidence, str) and 0 < len(evidence) <= 8192, "acceptance evidence required")
    owners = next(a["owners"] for a in project["plan"]["acceptance"] if a["id"] == ident)
    require(all(o in project["observations"] for o in owners), "owners need fresh observations")
    project["attestations"][ident] = {"evidence": evidence, "requirements": requirements_identity(project),
        "observations": acceptance_observations(project, owners)}
    for owner in owners:
        requirements = [a for a in project["plan"]["acceptance"] if owner in a["owners"]]
        state = project["nodes"][owner]
        revalidated = (state["state"] == "blocked" and state["worker"] is None and not occupied_attempt(state)
                       and project["observations"][owner].get("terminal") is True)
        if (state["state"] == "merged" or revalidated) and all(acceptance_valid(project, a) for a in requirements):
            project["nodes"][owner]["state"] = "accepted"


def replan(project: dict, plan: dict) -> None:
    validate_plan(plan)
    old = project["plan"]
    require(all(plan[k] == old[k] for k in ("parent", "repositories", "permissions")),
            "replan cannot expand authority")
    current = {n["id"]: n for n in old["nodes"]}
    updated = {n["id"]: n for n in plan["nodes"]}
    require(set(current) <= set(updated), "replan cannot drop known work")
    for ident in current:
        if current[ident] != updated[ident]:
            require(not current[ident]["external"], "external declaration cannot be adopted or changed")
            require(not occupied_attempt(project["nodes"][ident]), "retire occupied attempt before replanning boundaries")
            require(project["nodes"][ident]["state"] not in {"reserved", "running", "external"},
                    "cannot change active/foreign task boundaries")
            require(current[ident]["entry_mode"] == updated[ident]["entry_mode"],
                    "entry-mode change requires explicit separate delivery")
            project["nodes"][ident].update(state="pending", worker=None)
    for ident in set(updated) - set(current):
        project["nodes"][ident] = {"state": "external" if updated[ident]["external"] else "pending",
                                  "worker": None, "attempt": None, "detail": ""}
    project["plan"] = copy.deepcopy(plan)
    project["attestations"] = {}
    for state in project["nodes"].values():
        if state["state"] == "accepted":
            state["state"] = "merged"


def retry(project: dict, ident: str, attempt: str, evidence: str) -> None:
    current = project["nodes"][ident]
    require(current["attempt"] == attempt and current["state"] in {"reserved", "blocked", "ready"},
            "only a proven failed/blocked attempt can retry")
    require(isinstance(evidence, str) and 0 < len(evidence) <= 8192,
            "runtime stop/non-start and exact leaf ownership proof required")
    current.update(state="pending", worker=None, detail=evidence)


def terminal_gaps(project: dict) -> list[str]:
    gaps = []
    parent = project["observations"].get(project["plan"]["parent"], {})
    if parent.get("complete") is not True or parent.get("work_resolved") is not True:
        gaps.append("parent: incomplete observation or unresolved PRs/checks/children/dependencies")
    known_nodes = {value.get("node_id") for ident, value in project["observations"].items()
                   if ident in project["nodes"] and value.get("node_id")}
    for ident, observation in project["observations"].items():
        for required in observation.get("children", []) + observation.get("dependencies", []):
            if required not in known_nodes:
                gaps.append(f"{ident}: undeclared native required node {required}")
    for node in project["plan"]["nodes"]:
        ident = node["id"]
        observation = project["observations"].get(ident)
        if not observation or not observation.get("complete"):
            gaps.append(f"{ident}: incomplete observation")
        elif not observation.get("terminal"):
            gaps.append(f"{ident}: not terminal")
        if project["nodes"][ident]["state"] in {"running", "reserved", "blocked", "external"}:
            gaps.append(f"{ident}: unresolved ownership/work")
    for item in project["plan"]["acceptance"]:
        if not acceptance_valid(project, item):
            gaps.append(f"{item['id']}: missing or stale acceptance evidence")
    for op in project["operations"].values():
        if op["kind"] == "create-child" and op["phase"] == "bound":
            child = op["result"]["match"]
            known = {project["observations"].get(n["id"], {}).get("node_id") for n in project["plan"]["nodes"]}
            if child not in known or child not in parent.get("children", []):
                gaps.append("created child needs tracked graph and verified native parent link")
    if any(op["phase"] not in {"bound", "cancelled"} for op in project["operations"].values()):
        gaps.append("unresolved tracking operation")
    return gaps
