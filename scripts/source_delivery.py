#!/usr/bin/env python3
"""Prepare ordinary source worktrees; never admit builds, runtimes or publication.

A small Git-local creation receipt lets an explicit owner resume this helper's
work. Existing wrapper-managed or ledger-bound worktrees keep their own protocol.
No journal inventory or remote authentication is needed for local source edits.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import socket
import stat
import subprocess
import sys


RECEIPT = "source-delivery-creation.json"


class SourceError(Exception):
    """An unsafe or ambiguous source coordinate."""


def git(root: Path, *args: str) -> str:
    # Ambient Git overrides must not redirect a checked repository or index.
    env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
    env["GIT_TERMINAL_PROMPT"] = "0"
    env["GIT_OPTIONAL_LOCKS"] = "0"
    result = subprocess.run(
        ["git", "-c", "core.hooksPath=/dev/null", "-C", str(root), *args],
        env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode:
        raise SourceError(f"Git {args[0]} failed: {result.stderr.strip()}")
    return result.stdout.rstrip("\n")


def checked_path(value: str, *, absent: bool = False, protect: bool = False) -> Path:
    path = Path(os.path.abspath(value))
    # Reject aliases rather than silently following them into another resource.
    private_ancestor = False
    for part in [*reversed(path.parents), path]:
        try:
            info = part.lstat()
        except FileNotFoundError:
            if absent and part == path:
                break
            raise SourceError(f"Missing directory: {part}") from None
        if not stat.S_ISDIR(info.st_mode):
            raise SourceError(f"Directory or ancestor is not a real directory: {part}")
        if protect and not private_ancestor and info.st_mode & 0o022 and not info.st_mode & stat.S_ISVTX:
            raise SourceError(f"Directory permits unrelated writers: {part}")
        if info.st_uid == os.geteuid() and not info.st_mode & 0o077:
            private_ancestor = True
    return path


def owned(path: Path) -> None:
    if path.stat().st_uid != os.geteuid():
        raise SourceError(f"Current user does not own: {path}")


def identity(root: Path) -> dict:
    top = Path(git(root, "rev-parse", "--show-toplevel"))
    if top != root:
        raise SourceError("Worktree/repository must name its exact Git root")
    gitdir = checked_path(git(root, "rev-parse", "--absolute-git-dir"))
    common = checked_path(git(root, "rev-parse", "--path-format=absolute", "--git-common-dir"))
    owned(root)
    owned(gitdir)
    owned(common)
    branch_ref = git(root, "symbolic-ref", "--quiet", "HEAD")
    if not branch_ref.startswith("refs/heads/"):
        raise SourceError("Source HEAD must identify a local branch")
    return {"worktree": str(root), "gitdir": str(gitdir), "common_dir": str(common),
            "repository": registered(root)[0]["worktree"],
            "branch": branch_ref.removeprefix("refs/heads/"),
            "head": git(root, "rev-parse", "--verify", "HEAD")}


def base_commit(root: Path, ref: str) -> str:
    if not ref or ref.startswith("-") or any(char.isspace() for char in ref):
        raise SourceError("Base must be an explicit Git commit or ref")
    return git(root, "rev-parse", "--verify", "--end-of-options", ref + "^{commit}")


def validate_branch(root: Path, branch: str) -> None:
    if not branch or branch.startswith("-") or branch == "HEAD":
        raise SourceError("Branch must be a new named source branch")
    git(root, "check-ref-format", "refs/heads/" + branch)


def registered(root: Path) -> list[dict]:
    records = []
    for block in git(root, "worktree", "list", "--porcelain", "-z").split("\0\0"):
        record = dict(field.split(" ", 1) if " " in field else (field, "")
                      for field in block.split("\0") if field)
        if record:
            records.append(record)
    return records


def assert_registered(root: Path, branch: str) -> None:
    matches = [row for row in registered(root) if row.get("worktree") == str(root)]
    if len(matches) != 1 or matches[0].get("branch") != "refs/heads/" + branch:
        raise SourceError("Git worktree registration or branch identity is ambiguous")
    if "locked" in matches[0] or "prunable" in matches[0]:
        raise SourceError("Locked or prunable worktree requires its existing recovery procedure")


def status(root: Path) -> bool:
    return bool(git(root, "status", "--porcelain=v1", "-z", "--untracked-files=all", "--ignored=matching"))


def start(args: argparse.Namespace) -> dict:
    source = checked_path(args.repository)
    origin = identity(source)
    target = checked_path(args.worktree, absent=True, protect=True)
    owned(target.parent)
    if target.exists():
        raise SourceError("Worktree path already exists; preserve it and choose a fresh path")
    # A disjoint directory outside Git checkouts cannot overlap their managed
    # workspace/build/state trees. This requires no global delivery inventory.
    for ancestor in target.parents:
        if (ancestor / ".git").exists() or (ancestor / ".git").is_symlink():
            try:
                enclosing = git(ancestor, "rev-parse", "--show-toplevel")
            except SourceError:
                continue
            if enclosing == str(ancestor):
                raise SourceError("New source path must be outside existing checkouts and managed resource trees")
    for row in registered(source):
        existing = Path(row["worktree"])
        if target == existing or target.is_relative_to(existing) or existing.is_relative_to(target):
            raise SourceError("New source path overlaps a registered worktree")
    validate_branch(source, args.branch)
    base = base_commit(source, args.base)
    if git(source, "for-each-ref", "--format=%(refname)", "refs/heads/" + args.branch):
        raise SourceError("Branch already exists; choose a fresh source branch")
    # mkdir is an exclusive reservation; Git -b atomically refuses branch races.
    # Never roll back a failed creation by deleting a potentially changed path.
    target.mkdir(mode=0o700)
    try:
        git(source, "worktree", "add", "-b", args.branch, "--", str(target), base)
        result = identity(target)
        assert_registered(target, args.branch)
        if result["common_dir"] != origin["common_dir"] or result["head"] != base:
            raise SourceError("Created Git identity changed during source preparation")
        result.update(base=base, owner=args.owner, uid=os.geteuid(), host=socket.gethostname())
        receipt = Path(result["gitdir"]) / RECEIPT
        descriptor = os.open(receipt, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(descriptor, "w") as stream:
            json.dump(result, stream, sort_keys=True)
            stream.write("\n")
        return {**result, "status": "ready", "dirty": status(target),
                "next_action": "Edit source in this worktree; verify publication identity before pushing."}
    except (SourceError, OSError) as error:
        raise SourceError(f"Source creation did not complete: {error}. Resources were preserved at {target}; "
                          "inspect Git worktree/branch state and use a fresh path for isolated repair.") from error


def resume(args: argparse.Namespace) -> dict:
    target = checked_path(args.worktree, protect=True)
    result = identity(target)
    if result["gitdir"] == result["common_dir"]:
        raise SourceError("Primary checkout cannot be resumed as a dedicated source worktree")
    assert_registered(target, args.branch)
    receipt = Path(result["gitdir"]) / RECEIPT
    try:
        descriptor = os.open(receipt, os.O_RDONLY | os.O_NOFOLLOW)
        with os.fdopen(descriptor) as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or info.st_nlink != 1 or info.st_mode & 0o077:
                raise SourceError("Creation receipt ownership or permissions are ambiguous")
            saved = json.load(stream)
    except FileNotFoundError:
        raise SourceError("No source creation receipt; do not adopt existing bound or unknown work. "
                          "Use its existing recovery procedure or start an isolated fresh worktree.") from None
    if not isinstance(saved, dict):
        raise SourceError("Invalid source creation receipt")
    base = base_commit(target, args.base)
    expected = {key: result[key] for key in ("worktree", "gitdir", "common_dir", "repository", "branch")}
    expected.update(base=base, owner=args.owner, uid=os.geteuid(), host=socket.gethostname())
    if result["branch"] != args.branch or any(saved.get(key) != value for key, value in expected.items()):
        raise SourceError("Source owner, base, branch or Git identity differs from creation receipt")
    git(target, "merge-base", "--is-ancestor", base, result["head"])
    dirty = status(target)
    if dirty and not args.allow_dirty:
        raise SourceError("Owned worktree is dirty; inspect the diff and explicitly pass --allow-dirty to resume it")
    return {**result, **expected, "status": "ready", "dirty": dirty,
            "next_action": "Continue owned source work; runtime and publication checks remain separate."}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("start", "resume"):
        command = commands.add_parser(name)
        if name == "start":
            command.add_argument("--repository", required=True)
        else:
            command.add_argument("--allow-dirty", action="store_true")
        command.add_argument("--worktree", required=True)
        command.add_argument("--branch", required=True)
        command.add_argument("--base", required=True)
        command.add_argument("--owner", required=True, help="Stable caller-supplied owner/session identifier")
    args = parser.parse_args(argv)
    try:
        if not sys.platform.startswith("linux"):
            raise SourceError("Source preparation requires an owned native Linux filesystem")
        if not args.owner.strip():
            raise SourceError("An explicit nonempty owner identifier is required")
        result = start(args) if args.command == "start" else resume(args)
        print(json.dumps(result, sort_keys=True))
        return 0
    except (SourceError, OSError, ValueError) as error:
        print(json.dumps({"status": "refused", "error": str(error),
                          "next_action": "Preserve existing resources; inspect the named coordinate or start a fresh isolated source worktree."}, sort_keys=True))
        return 2


if __name__ == "__main__":
    sys.exit(main())
