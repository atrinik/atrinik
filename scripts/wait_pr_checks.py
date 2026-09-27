#!/usr/bin/env python3
"""Wait for named pull-request checks without changing GitHub state."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import math
import subprocess
import sys
import time
from collections.abc import Callable, Sequence
from typing import Any
from urllib.parse import urlsplit


SUCCESS = {"SUCCESS"}
PENDING = {"EXPECTED", "IN_PROGRESS", "PENDING", "QUEUED", "REQUESTED", "WAITING"}
FAILURE = {
    "ACTION_REQUIRED",
    "CANCELLED",
    "ERROR",
    "FAILURE",
    "NEUTRAL",
    "SKIPPED",
    "STALE",
    "STARTUP_FAILURE",
    "TIMED_OUT",
}
GRAPHQL_QUERY = """query($owner:String!,$name:String!,$number:Int!){
  repository(owner:$owner,name:$name){pullRequest(number:$number){
    headRefOid baseRefOid baseRefName
    commits(last:1){nodes{commit{oid statusCheckRollup{contexts(first:100){
      totalCount pageInfo{hasNextPage} nodes{
        __typename
        ... on CheckRun{name status conclusion}
        ... on StatusContext{context state}
      }
    }}}}}
  }}
}"""


class SnapshotUnavailable(Exception):
    """A sanitized failure to obtain or decode a GitHub snapshot."""


class SnapshotOverflow(Exception):
    """The bounded GraphQL snapshot omitted check contexts."""


def validate_timing(timeout: float, interval: float, command_timeout: float) -> None:
    values = {
        "timeout": timeout,
        "interval": interval,
        "command_timeout": command_timeout,
    }
    invalid = [name for name, value in values.items() if not math.isfinite(value) or value <= 0]
    if invalid:
        raise ValueError("timing values must be finite and positive: " + ", ".join(invalid))


@dataclass(frozen=True)
class Target:
    owner: str
    repo: str
    number: int


def parse_target(pr: str, repo: str) -> Target:
    parts = repo.split("/")
    if len(parts) != 2 or any(not part or not part.isascii() or
                              not all(char.isalnum() or char in "-_." for char in part)
                              for part in parts):
        raise ValueError("--repo must be OWNER/REPO")
    owner, name = parts
    if pr.isascii() and pr.isdecimal():
        number_text = pr
    else:
        url = urlsplit(pr)
        path = [part for part in url.path.split("/") if part]
        if (url.scheme != "https" or url.hostname != "github.com" or url.port is not None or
                url.username is not None or url.password is not None or url.query or url.fragment or
                len(path) != 4 or path[2] != "pull" or path[0].casefold() != owner.casefold() or
                path[1].casefold() != name.casefold() or not path[3].isascii() or
                not path[3].isdecimal()):
            raise ValueError("PR must be a positive number or matching canonical GitHub PR URL")
        number_text = path[3]
    number = int(number_text)
    if number <= 0:
        raise ValueError("PR number must be positive")
    return Target(owner, name, number)


@dataclass(frozen=True)
class Snapshot:
    head: str
    base_oid: str
    base_ref: str
    checks: tuple[dict[str, Any], ...]

    @classmethod
    def from_graphql(cls, value: Any) -> "Snapshot":
        if not isinstance(value, dict) or value.get("errors"):
            raise SnapshotUnavailable
        try:
            pull = value["data"]["repository"]["pullRequest"]
            commit_nodes = pull["commits"]["nodes"]
            commit = commit_nodes[0]["commit"]
            rollup = commit["statusCheckRollup"]
            contexts = None if rollup is None else rollup["contexts"]
        except (KeyError, IndexError, TypeError):
            raise SnapshotUnavailable from None
        head = pull.get("headRefOid")
        base_oid = pull.get("baseRefOid")
        base_ref = pull.get("baseRefName")
        if contexts is None:
            checks: Any = []
            total = 0
        else:
            checks = contexts.get("nodes")
            total = contexts.get("totalCount")
            page_info = contexts.get("pageInfo")
            if (not isinstance(total, int) or isinstance(total, bool) or not isinstance(page_info, dict) or
                    not isinstance(page_info.get("hasNextPage"), bool)):
                raise SnapshotUnavailable
            if total > 100 or page_info["hasNextPage"]:
                raise SnapshotOverflow
        if not all(isinstance(item, str) and item for item in (head, base_oid, base_ref)):
            raise SnapshotUnavailable
        if commit.get("oid") != head:
            raise SnapshotUnavailable
        if (not isinstance(checks, list) or total != len(checks) or
                not all(isinstance(item, dict) for item in checks)):
            raise SnapshotUnavailable
        return cls(head, base_oid, base_ref, tuple(checks))


def fetch_snapshot(
    target: Target,
    timeout: float,
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> Snapshot:
    command = [
        "gh", "api", "graphql", "--method", "POST",
        "-f", f"query={GRAPHQL_QUERY}",
        "-f", f"owner={target.owner}",
        "-f", f"name={target.repo}",
        "-F", f"number={target.number}",
    ]
    try:
        result = runner(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.SubprocessError):
        raise SnapshotUnavailable from None
    if result.returncode != 0:
        raise SnapshotUnavailable
    try:
        return Snapshot.from_graphql(json.loads(result.stdout))
    except (json.JSONDecodeError, SnapshotUnavailable):
        raise SnapshotUnavailable from None


def emit_line(message: str) -> None:
    print(message, flush=True)


def check_name(check: dict[str, Any]) -> str | None:
    name = check.get("name") if check.get("__typename") == "CheckRun" else check.get("context")
    return name if isinstance(name, str) and name else None


def check_state(check: dict[str, Any]) -> str:
    if check.get("__typename") == "CheckRun":
        status = check.get("status")
        raw = check.get("conclusion") if status == "COMPLETED" else status
    elif check.get("__typename") == "StatusContext":
        raw = check.get("state")
    else:
        return "unknown"
    if not isinstance(raw, str):
        return "unknown"
    state = raw.upper()
    if state in SUCCESS:
        return "success"
    if state in PENDING:
        return "pending"
    if state in FAILURE:
        return state.lower()
    return "unknown"


def expected_states(snapshot: Snapshot, expected: Sequence[str]) -> dict[str, str]:
    grouped: dict[str, list[str]] = {name: [] for name in expected}
    for check in snapshot.checks:
        name = check_name(check)
        if name in grouped:
            grouped[name].append(check_state(check))

    states: dict[str, str] = {}
    for name, observed in grouped.items():
        if not observed:
            states[name] = "missing"
        elif any(state not in {"success", "pending"} for state in observed):
            states[name] = next(state for state in observed if state not in {"success", "pending"})
        elif "pending" in observed:
            states[name] = "pending"
        else:
            states[name] = "success"
    return states


def wait_for_checks(
    pr: str,
    repo: str,
    expected_head: str,
    expected: Sequence[str],
    *,
    expected_base: str | None = None,
    timeout: float = 900,
    interval: float = 10,
    command_timeout: float = 30,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
    emit: Callable[[str], None] = emit_line,
) -> int:
    """Wait for the explicitly named checks and return zero after two fresh successes."""
    validate_timing(timeout, interval, command_timeout)
    target = parse_target(pr, repo)
    if not expected:
        raise ValueError("at least one expected check is required")
    deadline = monotonic() + timeout
    initial_base: tuple[str, str] | None = None
    last_summary: str | None = None
    last_states: dict[str, str] | None = None
    ready_once = False

    while True:
        remaining = deadline - monotonic()
        if remaining <= 0:
            detail = "snapshot unavailable"
            if last_states is not None:
                incomplete = [f"{name}={state}" for name, state in last_states.items() if state != "success"]
                detail = ", ".join(incomplete) if incomplete else "final confirmation unavailable"
            emit(f"result: failure (timeout; {detail})")
            return 1

        try:
            snapshot = fetch_snapshot(
                target,
                max(0.001, min(command_timeout, remaining)),
                runner=runner,
            )
        except SnapshotOverflow:
            emit("result: failure (more than 100 check contexts; snapshot is incomplete)")
            return 1
        except SnapshotUnavailable:
            summary = "snapshot: unavailable"
            if summary != last_summary:
                emit(summary)
                last_summary = summary
            ready_once = False
            sleep(min(interval, max(0, deadline - monotonic())))
            continue

        if snapshot.head != expected_head:
            emit("result: failure (pull-request head drift)")
            return 1
        if expected_base is not None and initial_base is None and expected_base not in {
            snapshot.base_oid,
            snapshot.base_ref,
        }:
            emit("result: failure (pull-request base does not match --expected-base)")
            return 1
        if initial_base is None:
            initial_base = (snapshot.base_oid, snapshot.base_ref)
        elif initial_base != (snapshot.base_oid, snapshot.base_ref):
            emit("result: failure (pull-request base drift)")
            return 1

        states = expected_states(snapshot, expected)
        last_states = states
        summary = "checks: " + ", ".join(f"{name}={state}" for name, state in states.items())
        if summary != last_summary:
            emit(summary)
            last_summary = summary

        bad = [(name, state) for name, state in states.items() if state not in {"success", "pending", "missing"}]
        if bad:
            emit("result: failure (mandatory check did not succeed: " +
                 ", ".join(f"{name}={state}" for name, state in bad) + ")")
            return 1

        ready = all(state == "success" for state in states.values())
        if ready and ready_once:
            emit("result: success (all mandatory checks passed on the pinned head and base)")
            return 0
        if ready:
            ready_once = True
            continue
        ready_once = False
        sleep(min(interval, max(0, deadline - monotonic())))


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Wait read-only for the explicitly named mandatory PR checks on an exact "
                    "head and stable base. Unnamed reported checks are ignored; skipped and "
                    "neutral named checks are failures.",
    )
    result.add_argument("pr", help="pull request number or URL")
    result.add_argument("--repo", required=True, metavar="OWNER/REPO")
    result.add_argument("--expected-head", required=True, metavar="SHA")
    result.add_argument("--expected-base", help="initial base SHA or ref")
    result.add_argument("--expect", required=True, action="append", metavar="CHECK")
    result.add_argument("--timeout", type=float, default=900, metavar="SECONDS")
    result.add_argument("--interval", type=float, default=10, metavar="SECONDS")
    result.add_argument("--command-timeout", type=float, default=30, metavar="SECONDS")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    argument_parser = parser()
    args = argument_parser.parse_args(argv)
    try:
        validate_timing(args.timeout, args.interval, args.command_timeout)
        parse_target(args.pr, args.repo)
    except ValueError as error:
        argument_parser.error(str(error))
    expected = tuple(dict.fromkeys(args.expect))
    if any(not name.strip() for name in expected):
        argument_parser.error("--expect values must not be empty")
    return wait_for_checks(
        args.pr,
        args.repo,
        args.expected_head,
        expected,
        expected_base=args.expected_base,
        timeout=args.timeout,
        interval=args.interval,
        command_timeout=args.command_timeout,
    )


if __name__ == "__main__":
    raise SystemExit(main())
