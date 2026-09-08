"""Bounded GitHub observation and journaled project tracking (never merge/deploy)."""
from __future__ import annotations

import copy
import json
import os
import re
import subprocess

from .project_coordinator import ProjectError, coordinate, digest, require, terminal_gaps
from . import project_coordinator_projects as projects

MAX_PAGES = 20
API_VERSION = "2026-03-10"


class GitHub:
    """Use the authenticated installed CLI without shell interpolation."""

    def request(self, path: str, method: str = "GET", payload: dict | None = None):
        require(not path.startswith(("/", "http")) and ".." not in path, "unsafe API route")
        command = ["/usr/local/bin/gh" if os.path.exists("/usr/local/bin/gh") else "/usr/bin/gh",
                   "api", "--hostname", "github.com", "--method", method,
                   "-H", f"X-GitHub-Api-Version: {API_VERSION}", path]
        if payload is not None:
            command += ["--input", "-"]
        try:
            result = subprocess.run(command, input=None if payload is None else json.dumps(payload).encode(),
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=40, check=False)
        except subprocess.TimeoutExpired as error:
            raise ProjectError("GitHub timeout; reconcile any in-flight operation, do not repost") from error
        require(result.returncode == 0, "GitHub request failed; inspect access/rate limits; never blind-retry a write")
        require(len(result.stdout) <= 2 * 1024 * 1024, "GitHub response exceeds bound")
        try:
            value = json.loads(result.stdout) if result.stdout else None
        except ValueError as error:
            raise ProjectError("invalid GitHub response") from error
        require(not isinstance(value, dict) or not value.get("errors"), "GitHub GraphQL result incomplete")
        return value

    def pages(self, path: str, collection: str | None = None) -> list:
        rows = []
        seen = set()
        for page in range(1, MAX_PAGES + 1):
            data = self.request(f"{path}{'&' if '?' in path else '?'}per_page=100&page={page}")
            total = None
            if collection is not None:
                require(isinstance(data, dict) and type(data.get("total_count")) is int,
                        "incomplete GitHub collection")
                total, data = data["total_count"], data.get(collection)
            require(isinstance(data, list), "incomplete GitHub list")
            for item in data:
                # Cross-reference timeline entries have no REST numeric ID.
                # Hash the complete event when an ID is absent; never drop an event.
                require(isinstance(item, dict), "invalid GitHub page item")
                identity = ("id", item["id"]) if item.get("id") is not None else ("event", digest(item))
                require(identity not in seen, "duplicate GitHub page item")
                seen.add(identity)
            rows.extend(data)
            if len(data) < 100:
                require(total is None or total == len(rows), "GitHub collection changed or incomplete")
                return rows
        raise ProjectError("incomplete pagination; no mutation permitted")

    def actor(self) -> str:
        return self.request("user")["login"]

    def issue(self, ident: str) -> dict:
        return self.request(route(ident))

    def checks(self, repository: str, sha: str) -> dict:
        require(re.fullmatch(r"[0-9a-f]{40}", sha) is not None, "invalid PR check head")
        root = f"repos/{repository}/commits/{sha}"
        runs = self.pages(root + "/check-runs?filter=latest", "check_runs")
        statuses = self.pages(root + "/statuses")
        latest = {}
        for status in statuses:  # GitHub returns newest status first.
            latest.setdefault(status["context"], status["state"])
        return {"complete": True,
                "passing": all(r["status"] == "completed" and r["conclusion"] in
                               {"success", "neutral", "skipped"} for r in runs)
                and all(value == "success" for value in latest.values()),
                "runs": [{"id": r["id"], "status": r["status"], "conclusion": r["conclusion"]} for r in runs],
                "statuses": latest}

    def observe(self, ident: str, mode: str) -> dict:
        value = self.request(route(ident, "pulls" if mode == "PR" else "issues"))
        require(value["number"] == int(ident.split("#")[1]), "remote coordinate drift")
        if mode == "PR":
            checks = self.checks(ident.split("#")[0], value["head"]["sha"])
            return {"node_id": value["node_id"], "complete": True,
                    "terminal": bool(value.get("merged_at")) and checks["passing"], "state": value["state"],
                    "checks": checks,
                    "head": value["head"]["sha"], "base": value["base"]["sha"],
                    "body": digest(value.get("body")), "merged": value.get("merge_commit_sha")
                    if value.get("merged_at") else None}
        require("pull_request" not in value, "issue coordinate is a PR")
        timeline = self.pages(route(ident) + "/timeline")
        references = {}
        for event in timeline:
            source = event.get("source", {}).get("issue", {})
            if source.get("pull_request"):
                url = source.get("html_url", "")
                match = re.fullmatch(r"https://github.com/([^/]+/[^/]+)/pull/([1-9][0-9]*)", url)
                require(match is not None, "unrecognized referenced PR")
                pr = self.request(f"repos/{match[1]}/pulls/{match[2]}")
                checks = self.checks(match[1], pr["head"]["sha"])
                references[url] = {"head": pr["head"]["sha"], "base": pr["base"]["sha"],
                                   "merged": bool(pr.get("merged_at")), "state": pr["state"], "checks": checks}
        children = self.pages(route(ident) + "/sub_issues")
        dependencies = self.pages(route(ident) + "/dependencies/blocked_by")
        resolved = (all(p["merged"] and p["checks"]["passing"] for p in references.values())
                    and all(c["state"] == "closed" and c.get("state_reason") == "completed" for c in children)
                    and all(d["state"] == "closed" and d.get("state_reason") == "completed" for d in dependencies))
        return {"node_id": value["node_id"], "complete": True,
                "work_resolved": resolved,
                "terminal": value["state"] == "closed" and value.get("state_reason") == "completed" and resolved,
                "state": value["state"], "body": digest(value.get("body")), "references": references,
                "children": [c["node_id"] for c in children], "dependencies": [d["node_id"] for d in dependencies]}


def route(ident: str, surface: str = "issues") -> str:
    repo, number = coordinate(ident).split("#")
    return f"repos/{repo}/{surface}/{number}"


def refresh(project: dict, github: GitHub) -> dict:
    """Observe first, then replace evidence together; partial scans change nothing."""
    observations = {n["id"]: github.observe(n["id"], n["entry_mode"]) for n in project["plan"]["nodes"]}
    parent = github.observe(project["plan"]["parent"], "issue")
    observations[project["plan"]["parent"]] = parent
    changed = [ident for ident, value in observations.items()
               if project["observations"].get(ident) != value]
    project["observations"] = observations
    for ident in changed:
        if ident not in project["nodes"]:
            continue
        state = project["nodes"][ident]
        if state["state"] in {"running", "reserved"}:
            state.update(state="blocked", detail="remote evidence changed; reprove same worker and leaf before reopening")
        elif observations[ident]["terminal"]:
            state.update(state="merged", worker=None, attempt=None)
        elif state["state"] in {"ready", "merged", "accepted"}:
            state.update(state="blocked", detail="remote evidence changed; refresh leaf delivery")
    # Invalidate descendants even when their own PR head has not changed.
    stale = set(changed)
    for _ in project["plan"]["nodes"]:
        for node in project["plan"]["nodes"]:
            if any(d["id"] in stale for d in node["dependencies"]):
                stale.add(node["id"])
                state = project["nodes"][node["id"]]
                if state["state"] in {"ready", "accepted", "running", "reserved"}:
                    state.update(state="blocked", detail="dependency changed; integration revalidation required")
    return {"changed": changed, "invalidated": sorted(stale)}


def authorize(project: dict, kind: str, target: str) -> None:
    require(kind in project["plan"]["permissions"], "tracking operation outside explicit authority")
    known = {project["plan"]["parent"]} | {n["id"] for n in project["plan"]["nodes"]
                                               if n["entry_mode"] == "issue"}
    require(target in known, "tracking target not an explicit issue")
    require(not any(n["id"] == target and n["external"] for n in project["plan"]["nodes"]),
            "external leaf tracking is read-only")


def validate_operation(project: dict, op: dict) -> None:
    """A saved intent cannot turn into an arbitrary API request on resume."""
    kind, target, payload = op["kind"], op["target"], op["payload"]
    authorize(project, kind, target)
    request = op["request"]
    if kind == "project-status":
        require(request == projects.validate(payload), "Project mutation request drift")
    elif kind == "assign":
        require(payload == {"login": project["actor"]} and request == {"assignees": [project["actor"]]},
                "assignment request drift")
    elif kind in {"comment", "create-child"}:
        expected_keys = {"body"} if kind == "comment" else {"repository", "title", "body"}
        require(set(payload) == expected_keys and isinstance(payload["body"], str)
                and 0 < len(payload["body"]) <= 16384 and "<!-- atrinik-" not in payload["body"],
                "invalid tracking text")
        body = payload["body"].rstrip()
        if kind == "create-child":
            require(target == project["plan"]["parent"] and isinstance(payload["title"], str)
                    and 0 < len(payload["title"]) <= 256, "invalid child target/title")
            body += f"\n\nParent: https://github.com/{target.replace('#', '/issues/')}"
        expected = {"body": body + "\n\n" + op["marker"] + "\n"}
        if kind == "create-child":
            expected["title"] = payload["title"]
        require(request == expected, "tracking text request drift")
    elif kind in {"link", "dependency"}:
        require(set(payload) == {"issue"} and payload["issue"] != target, "invalid related issue")
        authorize(project, kind, payload["issue"])
        key = "sub_issue_id" if kind == "link" else "issue_id"
        require(set(request) == {key} and type(request[key]) is int and request[key] > 0,
                "relationship request drift")
    else:
        require(kind == "close-parent" and target == project["plan"]["parent"] and payload == {}
                and request == {"state": "closed", "state_reason": "completed"}, "invalid closure request")


def relationship_guard(operation: dict, github: GitHub) -> None:
    """Reprove related identity and the complete bounded reachable graph before write."""
    kind, target = operation["kind"], operation["target"]
    if kind not in {"link", "dependency"}:
        return
    related = github.issue(operation["payload"]["issue"])
    require("pull_request" not in related and related["id"] == next(iter(operation["request"].values())),
            "related issue identity changed")
    wanted = github.issue(target)["node_id"]
    if kind == "link":
        parent = github.request("graphql", "POST", {"query":
            "query($id:ID!) { node(id:$id) { ... on Issue { parent { id } } } }",
            "variables": {"id": related["node_id"]}})
        require(not parent.get("errors"), "parent lookup incomplete")
        current = parent["data"]["node"]["parent"]
        require(current is None or current["id"] == wanted, "foreign parent")
        return  # GitHub also rejects hierarchy cycles; never force reparenting.
    pending, seen = [operation["payload"]["issue"]], set()
    while pending:
        ident = pending.pop()
        if ident in seen:
            continue
        seen.add(ident)
        require(len(seen) <= 256, "dependency graph exceeds bound")
        require(github.issue(ident)["node_id"] != wanted, "dependency cycle")
        for dependency in github.pages(route(ident) + "/dependencies/blocked_by"):
            match = re.fullmatch(r"https://github.com/(atrinik/[^/]+)/issues/([1-9][0-9]*)",
                                 dependency.get("html_url", ""))
            require(match is not None, "dependency outside inspectable project graph")
            pending.append(coordinate(f"{match[1]}#{match[2]}"))


def prepare_operation(project: dict, github: GitHub, kind: str, target: str, payload: dict) -> dict:
    """Return a durable intent; caller persists it before apply."""
    authorize(project, kind, target)
    require(github.actor() == project["actor"], "authenticated actor changed")
    require(isinstance(payload, dict), "tracking payload must be an object")
    require(not any(op["target"] == target and op["phase"] not in {"bound", "cancelled"}
                    for op in project["operations"].values()), "target has unresolved tracking intent")
    ident = digest({"authority": project["authority"], "generation": project["generation"],
                    "kind": kind, "target": target, "payload": payload})
    marker = f"<!-- atrinik-project:{ident} -->"
    request, method, path = {}, "POST", route(target)
    if kind == "project-status":
        request, path = projects.validate(payload), "graphql"
        projects.gate(project, {"payload": payload, "target": target}, github)
    elif kind == "assign":
        require(set(payload) == {"login"} and payload["login"] == project["actor"], "assign only authenticated actor")
        request = {"assignees": [payload["login"]]}
        path += "/assignees"
    elif kind == "comment":
        require(set(payload) == {"body"} and isinstance(payload["body"], str)
                and 0 < len(payload["body"]) <= 16384 and "<!-- atrinik-" not in payload["body"],
                "invalid coordinator comment")
        request = {"body": payload["body"].rstrip() + "\n\n" + marker + "\n"}
        path += "/comments"
    elif kind in {"link", "dependency"}:
        require(set(payload) == {"issue"}, "exact related issue required")
        authorize(project, kind, payload["issue"])
        require(payload["issue"] != target, "self relationship")
        related = github.issue(payload["issue"])
        require("pull_request" not in related, "relationship target must be issue")
        if kind == "link":
            # Query the native graph; never force reparenting.
            parent = github.request("graphql", "POST", {"query":
                "query($id:ID!) { node(id:$id) { ... on Issue { parent { id } } } }",
                "variables": {"id": related["node_id"]}})
            require("errors" not in parent, "parent lookup incomplete")
            current = parent["data"]["node"]["parent"]
            wanted = github.issue(target)["node_id"]
            require(current is None or current["id"] == wanted, "foreign parent; reparenting requires authority")
            request = {"sub_issue_id": related["id"]}
            path += "/sub_issues"
        else:
            require(not any(d["node_id"] == github.issue(target)["node_id"]
                            for d in github.pages(route(payload["issue"]) + "/dependencies/blocked_by")),
                    "reverse dependency")
            request = {"issue_id": related["id"]}
            path += "/dependencies/blocked_by"
    elif kind == "create-child":
        require(target == project["plan"]["parent"] and set(payload) == {"repository", "title", "body"},
                "child requires explicit parent/repository/title/body")
        require(payload["repository"] in project["plan"]["repositories"], "child outside repository scope")
        require(all(isinstance(payload[k], str) and 0 < len(payload[k]) <= 16384 for k in ("title", "body"))
                and "<!-- atrinik-" not in payload["body"], "invalid child text")
        path = f"repos/{payload['repository']}/issues"
        rows = github.pages(path + "?state=all")
        require(not any("pull_request" not in row and row["title"].casefold() == payload["title"].casefold()
                        for row in rows), "matching issue exists; reconcile rather than create")
        request = {"title": payload["title"], "body": payload["body"].rstrip()
                   + f"\n\nParent: https://github.com/{target.replace('#', '/issues/')}\n\n{marker}\n"}
    elif kind == "close-parent":
        require(target == project["plan"]["parent"] and payload == {}, "only terminal parent closure")
        candidate = copy.deepcopy(project)
        refresh(candidate, github)
        require(candidate["observations"] == project["observations"], "refresh and re-attest before closure")
        require(not terminal_gaps(project), "terminal acceptance incomplete: " + "; ".join(terminal_gaps(project)))
        # Native children outside the declared graph must not disappear from acceptance.
        parent = github.issue(target)
        children = github.pages(route(target) + "/sub_issues")
        known = {o["node_id"] for key, o in project["observations"].items() if key != target}
        require(all(c["node_id"] in known for c in children), "untracked native child")
        require(parent["state"] == "open", "parent already closed; reconcile")
        method, request = "PATCH", {"state": "closed", "state_reason": "completed"}
    else:
        raise ProjectError("unsupported tracking operation")
    operation = {"id": ident, "kind": kind, "target": target, "payload": payload, "path": path,
                 "method": method, "request": request, "marker": marker, "phase": "planned",
                 "before": None, "result": None}
    validate_operation(project, operation)
    relationship_guard(operation, github)
    operation["before"] = observe_operation(operation, github)
    require(operation["before"].get("match") is None, "operation already satisfied; no write needed")
    # Stable consecutive complete scans before planning a creation.
    require(observe_operation(operation, github) == operation["before"], "remote state changed during planning")
    project["operations"][ident] = operation
    return operation


def observe_operation(operation: dict, github: GitHub) -> dict:
    kind, target = operation["kind"], operation["target"]
    if kind == "project-status":
        return projects.observe(operation, github)
    issue = github.issue(target)
    if kind == "assign":
        found = any(a["login"] == operation["payload"]["login"] for a in issue["assignees"])
        return {"identity": issue["node_id"], "match": issue["node_id"] if found else None}
    if kind in {"comment", "create-child"}:
        path = operation["path"] + ("?state=all" if kind == "create-child" else "")
        rows = github.pages(path)
        matches = [r for r in rows if operation["marker"] in (r.get("body") or "")]
        require(len(matches) <= 1, "duplicate coordinator marker")
        match = matches[0] if matches else None
        if kind == "create-child":
            require(not any("pull_request" not in row and
                            row.get("title", "").casefold() == operation["payload"]["title"].casefold()
                            and row not in matches for row in rows), "matching issue exists; creation blocked")
        if match:
            require(match["user"]["login"] == github.actor() and match["body"] == operation["request"]["body"],
                    "foreign or drifted coordinator text")
            if kind == "create-child":
                require(match["title"] == operation["request"]["title"], "child title drift")
        return {"identity": issue["node_id"], "match": match["node_id"] if match else None,
                "inventory": digest([(r["node_id"], r.get("title"), r.get("body")) for r in rows])}
    if kind in {"link", "dependency"}:
        rows = github.pages(operation["path"])
        wanted = next(iter(operation["request"].values()))
        matches = [r for r in rows if r["id"] == wanted]
        require(len(matches) <= 1, "duplicate relationship")
        return {"identity": issue["node_id"], "match": matches[0]["node_id"] if matches else None,
                "inventory": digest([r["node_id"] for r in rows])}
    return {"identity": issue["node_id"], "match": issue["node_id"] if issue["state"] == "closed"
            and issue.get("state_reason") == "completed" else None, "body": digest(issue.get("body"))}


def cancel_operation(store, expected: dict, ident: str, github: GitHub) -> dict:
    """Cancel only an intent whose durable state proves no remote call started."""
    def change(project):
        require(github.actor() == project["actor"], "authenticated actor changed")
        op = project["operations"][ident]
        require(op["phase"] == "planned", "cannot cancel started or completed operation")
        live = observe_operation(op, github)
        safe_satisfaction = op["kind"] in {"assign", "project-status", "link", "dependency"}
        require(live["identity"] == op["before"]["identity"] and (live["match"] is None or safe_satisfaction),
                "cannot prove planned operation unapplied")
        op["phase"] = "cancelled"
    snapshot, _ = store.update(expected, change)
    return snapshot


def apply_operation(store, expected: dict, ident: str, github: GitHub, reconcile: bool = False) -> dict:
    """Persist in-flight before one remote write. Unknown outcomes are never replayed."""
    snapshot = store.inspect()
    require(all(snapshot[k] == expected[k] for k in ("generation", "digest", "device", "inode")),
            "stale tracking CAS")
    project = snapshot["document"]
    require(github.actor() == project["actor"], "authenticated actor changed")
    require(ident in project["operations"], "unknown operation")
    operation = project["operations"][ident]
    validate_operation(project, operation)
    require(operation["phase"] != "cancelled", "cancelled operation cannot apply")
    if operation["phase"] == "bound":
        return snapshot
    live = observe_operation(operation, github)
    if live["match"] is not None:
        require(operation["phase"] == "in-flight", "unowned result appeared before call")
    else:
        require(not reconcile and operation["phase"] == "planned",
                "uncertain remote result; observation-only reconciliation, never repost")
        require(live == operation["before"], "remote target changed; replan with explicit reconciliation")
        relationship_guard(operation, github)
        if operation["kind"] == "project-status":
            projects.gate(project, operation, github)
        if operation["kind"] == "close-parent":
            candidate = copy.deepcopy(project)
            candidate["operations"].pop(ident)
            refresh(candidate, github)
            require(candidate["observations"] == project["observations"] and not terminal_gaps(candidate),
                    "closure evidence changed")
        snapshot, _ = store.update(expected, lambda p: p["operations"][ident].update(phase="in-flight"))
        github.request(operation["path"], operation["method"], operation["request"])
        live = observe_operation(operation, github)
        require(live["match"] is not None, "write result uncertain; do not retry")
    snapshot, _ = store.update(snapshot, lambda p: p["operations"][ident].update(phase="bound", result=live))
    return snapshot
