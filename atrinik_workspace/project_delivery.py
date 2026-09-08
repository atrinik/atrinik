"""Operator CLI for project orchestration; worker spawning uses the agent runtime tools."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from .project_coordinator import (ProjectError, attest, digest, new_project, record_worker,
                                  require, replan, reopen, reserve, retry, schedule, terminal_gaps, worker_result)
from .project_coordinator_github import GitHub, apply_operation, cancel_operation, prepare_operation, refresh
from .project_coordinator_store import LIMIT, Store, open_directory, read_input

SOURCE_ROOT = Path(__file__).resolve().parents[1]


def context() -> Path:
    """Do not let a caller-supplied runtime marker authorize this CLI."""
    result = subprocess.run([sys.executable, str(SOURCE_ROOT / "scripts/atrinik_coordinator_context.py"),
                             "--json"], capture_output=True, timeout=30, check=False)
    require(result.returncode == 0, "coordinator probe failed")
    probe = json.loads(result.stdout)
    require(probe.get("authoritative") is True and probe.get("status") == "canonical-linux",
            "project operations require the supported canonical Linux coordinator")
    common = subprocess.run(["git", "-C", str(SOURCE_ROOT), "rev-parse", "--path-format=absolute",
                             "--git-common-dir"], capture_output=True, text=True, timeout=20, check=True)
    root = Path(common.stdout.strip()).parent
    require((root / "components.json").is_file(), "wrapper root unresolved")
    return root


def initialize_root(wrapper: Path, parent: str) -> Path:
    directory = wrapper / "build" / "project-delivery" / digest({"parent": parent})
    ignored = subprocess.run(["git", "-C", str(wrapper), "check-ignore", "--quiet", "--", str(directory)],
                             timeout=20, check=False)
    require(ignored.returncode == 0, "project root must be Git ignored")
    fd = open_directory(wrapper)
    try:
        for component in ("build", "project-delivery", directory.name):
            try:
                os.mkdir(component, 0o700, dir_fd=fd)
            except FileExistsError:
                pass
            next_fd = os.open(component, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = next_fd
    finally:
        os.close(fd)
    return directory


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--root", type=Path, help="exact helper-returned project root")
    sub = p.add_subparsers(dest="command", required=True)
    init = sub.add_parser("init")
    init.add_argument("--plan", required=True, type=Path)
    init.add_argument("--authority", required=True, help="exact user instruction/session or requested goal reference")
    sub.add_parser("inspect")
    change_plan = sub.add_parser("replan")
    change_plan.add_argument("--plan", type=Path, required=True)
    change_plan.add_argument("--expected", type=Path, required=True)
    retry_cmd = sub.add_parser("retry")
    retry_cmd.add_argument("coordinate")
    retry_cmd.add_argument("--attempt", required=True)
    retry_cmd.add_argument("--evidence", required=True)
    retry_cmd.add_argument("--expected", type=Path, required=True)
    reopen_cmd = sub.add_parser("reopen")
    reopen_cmd.add_argument("coordinate")
    reopen_cmd.add_argument("--attempt", required=True)
    reopen_cmd.add_argument("--evidence", required=True)
    reopen_cmd.add_argument("--heavy-limit", type=int, default=1)
    reopen_cmd.add_argument("--expected", type=Path, required=True)
    for name in ("plan", "dispatch"):
        cmd = sub.add_parser(name)
        cmd.add_argument("--capacity", required=True, type=int)
        cmd.add_argument("--open-workers", type=int, required=True)
        cmd.add_argument("--heavy-limit", type=int, default=1)
        if name == "dispatch":
            cmd.add_argument("--expected", required=True, type=Path)
    worker = sub.add_parser("worker")
    worker.add_argument("coordinate")
    worker.add_argument("--attempt", required=True)
    worker.add_argument("--id", required=True, help="actual spawned worker identity")
    worker.add_argument("--expected", required=True, type=Path)
    result = sub.add_parser("result")
    result.add_argument("coordinate")
    result.add_argument("--attempt", required=True)
    result.add_argument("--state", choices=["ready", "blocked"], required=True)
    result.add_argument("--evidence", required=True)
    result.add_argument("--expected", required=True, type=Path)
    refresh_cmd = sub.add_parser("refresh")
    refresh_cmd.add_argument("--expected", required=True, type=Path)
    accept = sub.add_parser("attest")
    accept.add_argument("criterion")
    accept.add_argument("--evidence", required=True)
    accept.add_argument("--expected", required=True, type=Path)
    sub.add_parser("terminal")
    tracking = sub.add_parser("tracking")
    tracking.add_argument("action", choices=["plan", "apply", "reconcile", "cancel"])
    tracking.add_argument("--kind")
    tracking.add_argument("--target")
    tracking.add_argument("--payload", type=Path)
    tracking.add_argument("--operation")
    tracking.add_argument("--expected", required=True, type=Path)
    return p


def run(args, github=None):
    wrapper = context()
    gh = github or GitHub()
    if args.command == "init":
        require(args.root is None, "init derives its canonical root")
        plan = read_input(args.plan)
        document = new_project(plan, gh.actor(), args.authority)
        refresh(document, gh)
        root = initialize_root(wrapper, plan["parent"])
        return {"root": str(root), "snapshot": Store(root).create(document)}
    require(args.root is not None, "--root is required")
    require(args.root.parent == wrapper / "build" / "project-delivery", "noncanonical project root")
    store = Store(args.root)
    snapshot = store.inspect()
    require(args.root.name == digest({"parent": snapshot["document"]["plan"]["parent"]}),
            "project/root mismatch")
    if args.command == "inspect":
        return snapshot
    if args.command == "plan":
        return schedule(snapshot["document"], args.capacity, args.heavy_limit, args.open_workers)
    if args.command == "terminal":
        return {"gaps": terminal_gaps(snapshot["document"]), "note": "refresh before terminal decisions"}
    require(gh.actor() == snapshot["document"]["actor"], "authenticated actor changed")
    expected = read_input(args.expected, 2 * LIMIT)
    if args.command == "tracking" and args.action in {"apply", "reconcile", "cancel"}:
        require(bool(args.operation), "--operation required")
        if args.action == "cancel":
            return cancel_operation(store, expected, args.operation, gh)
        return apply_operation(store, expected, args.operation, gh, reconcile=args.action == "reconcile")
    def change(project):
        if args.command == "replan":
            return replan(project, read_input(args.plan))
        if args.command == "retry":
            return retry(project, args.coordinate, args.attempt, args.evidence)
        if args.command == "reopen":
            return reopen(project, args.coordinate, args.attempt, args.evidence, args.heavy_limit)
        if args.command == "dispatch":
            return reserve(project, args.capacity, args.heavy_limit, args.open_workers)
        if args.command == "worker":
            return record_worker(project, args.coordinate, args.attempt, args.id)
        if args.command == "result":
            node = next(n for n in project["plan"]["nodes"] if n["id"] == args.coordinate)
            require(gh.observe(args.coordinate, node["entry_mode"]) == project["observations"].get(args.coordinate),
                    "worker head/evidence changed; refresh, reopen and obtain a current-attempt result")
            return worker_result(project, args.coordinate, args.attempt, args.state, args.evidence)
        if args.command == "refresh":
            return refresh(project, gh)
        if args.command == "attest":
            return attest(project, args.criterion, args.evidence)
        if args.command == "tracking":
            require(args.kind and args.target and args.payload, "tracking plan needs kind/target/payload")
            return prepare_operation(project, gh, args.kind, args.target, read_input(args.payload))
        raise ProjectError("unsupported command")
    installed, result = store.update(expected, change)
    return {"snapshot": installed, "result": result}


def main(argv=None) -> int:
    args = parser().parse_args(argv)
    try:
        print(json.dumps(run(args), indent=2, sort_keys=True))
        return 0
    except (ProjectError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"project-delivery: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
