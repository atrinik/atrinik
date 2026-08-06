from __future__ import annotations

import argparse
from pathlib import Path
import sys

from .model import Manifest, WorkspaceError
from .workspace import Workspace


ROOT = Path(__file__).resolve().parents[1]


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(
        prog="atrinik", description="Atrinik multi-repository development workspace"
    )
    commands = root.add_subparsers(dest="command", required=True)

    manifest = commands.add_parser("manifest", help="validate the component manifest")
    manifest.add_argument("action", choices=["validate"])

    initialize = commands.add_parser("init", help="clone missing component repositories")
    initialize.add_argument("components", nargs="*")
    initialize.add_argument("--jobs", type=int, default=4)

    sync = commands.add_parser("sync", help="fetch and fast-forward component repositories")
    sync.add_argument("components", nargs="*")
    sync.add_argument(
        "--worktrees",
        choices=["none", "merge", "rebase"],
        default="none",
        help="also merge/rebase each component's clean feature worktrees",
    )

    worktree = commands.add_parser("worktree", help="manage component worktrees")
    worktree_commands = worktree.add_subparsers(dest="worktree_command", required=True)
    worktree_create = worktree_commands.add_parser("create")
    worktree_create.add_argument("component")
    worktree_create.add_argument("label")
    worktree_create.add_argument("--branch", required=True)
    worktree_create.add_argument("--from", dest="start_point")
    worktree_create.add_argument("--existing", action="store_true")
    worktree_remove = worktree_commands.add_parser("remove")
    worktree_remove.add_argument("component")
    worktree_remove.add_argument("label")
    worktree_list = worktree_commands.add_parser("list")
    worktree_list.add_argument("components", nargs="*")

    profile = commands.add_parser("profile", help="manage mixed-component profiles")
    profile_commands = profile.add_subparsers(dest="profile_command", required=True)
    profile_create = profile_commands.add_parser("create")
    profile_create.add_argument("name")
    profile_set = profile_commands.add_parser("set")
    profile_set.add_argument("name")
    profile_set.add_argument("component")
    selector = profile_set.add_mutually_exclusive_group(required=True)
    selector.add_argument("--primary", action="store_true")
    selector.add_argument("--worktree")
    selector.add_argument("--path", type=Path)
    profile_show = profile_commands.add_parser("show")
    profile_show.add_argument("name", nargs="?", default="default")

    build = commands.add_parser("build", help="build a component or the playable system")
    build.add_argument("target", help="all or a component name")
    build.add_argument("--profile", default="default")
    build.add_argument("--test", action="store_true")

    state = commands.add_parser("state", help="register persistent server state")
    state_commands = state.add_subparsers(dest="state_command", required=True)
    state_add = state_commands.add_parser("add")
    state_add.add_argument("name")
    state_add.add_argument("--path", type=Path)
    state_commands.add_parser("list")

    launch = commands.add_parser("run", help="build and run client or server")
    launch_commands = launch.add_subparsers(dest="target", required=True)
    for target in ("client", "server"):
        target_parser = launch_commands.add_parser(target)
        target_parser.add_argument("--profile", default="default")
        if target == "server":
            target_parser.add_argument("--state", default="default")
        target_parser.add_argument("--dry-run", action="store_true")
        target_parser.add_argument("arguments", nargs=argparse.REMAINDER)
    return root


def _server_arguments(arguments: list[str]) -> list[str]:
    return arguments[1:] if arguments and arguments[0] == "--" else arguments


def main(arguments: list[str] | None = None) -> int:
    options = parser().parse_args(arguments)
    try:
        if options.command == "manifest":
            manifest = Manifest.load(ROOT / "components.json")
            print(f"components.json: valid ({len(manifest.components)} components)")
            return 0

        workspace = Workspace(ROOT)
        if options.command == "init":
            workspace.initialize(options.components, options.jobs)
        elif options.command == "sync":
            workspace.sync(options.components, options.worktrees)
        elif options.command == "worktree":
            if options.worktree_command == "create":
                if options.existing and options.start_point:
                    raise WorkspaceError("--existing and --from cannot be combined")
                workspace.create_worktree(
                    options.component,
                    options.label,
                    options.branch,
                    options.start_point,
                    options.existing,
                )
            elif options.worktree_command == "remove":
                workspace.remove_worktree(options.component, options.label)
            else:
                for component, record in workspace.list_worktrees(options.components):
                    branch = record.get("branch", "detached").removeprefix("refs/heads/")
                    print(f"{component}\t{branch}\t{record['worktree']}")
        elif options.command == "profile":
            if options.profile_command == "create":
                workspace.create_profile(options.name)
            elif options.profile_command == "set":
                if options.primary:
                    workspace.set_profile(options.name, options.component, "primary")
                elif options.worktree is not None:
                    workspace.set_profile(
                        options.name, options.component, "worktree", options.worktree
                    )
                else:
                    workspace.set_profile(
                        options.name, options.component, "path", str(options.path.resolve())
                    )
            else:
                for component, path, head, dirty in workspace.profile_summary(options.name):
                    status = "dirty" if dirty else "clean"
                    print(f"{component}\t{head}\t{status}\t{path}")
        elif options.command == "build":
            print(workspace.build(options.target, options.profile, options.test))
        elif options.command == "state":
            if options.state_command == "add":
                workspace.state_add(options.name, options.path)
            else:
                workspace.paths.ensure()
                states = workspace._load_states()
                if "default" not in states:
                    states = {"default": str(workspace.paths.state / "server" / "default"), **states}
                for name, path in sorted(states.items()):
                    print(f"{name}\t{path}")
        elif options.command == "run":
            forwarded = _server_arguments(options.arguments)
            if options.target == "client":
                workspace.run_client(options.profile, forwarded, options.dry_run)
            else:
                workspace.run_server(
                    options.profile, options.state, forwarded, options.dry_run
                )
        return 0
    except WorkspaceError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
