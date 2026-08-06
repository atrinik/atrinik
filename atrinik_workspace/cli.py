from __future__ import annotations

import argparse
import json
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

    status = commands.add_parser(
        "status", help="summarize primary component checkout state"
    )
    status.add_argument("components", nargs="*")
    status.add_argument("--json", action="store_true")

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
    worktree_list.add_argument("--json", action="store_true")

    profile = commands.add_parser("profile", help="manage mixed-component profiles")
    profile_commands = profile.add_subparsers(dest="profile_command", required=True)
    profile_create = profile_commands.add_parser("create")
    profile_create.add_argument("name")
    profile_create.add_argument("--from", dest="source", default="default")
    profile_set = profile_commands.add_parser("set")
    profile_set.add_argument("name")
    profile_set.add_argument("component")
    selector = profile_set.add_mutually_exclusive_group(required=True)
    selector.add_argument("--primary", action="store_true")
    selector.add_argument("--worktree")
    selector.add_argument("--path", type=Path)
    profile_show = profile_commands.add_parser("show")
    profile_show.add_argument("name", nargs="?", default="default")
    profile_show.add_argument("--json", action="store_true")

    path = commands.add_parser(
        "path", help="print a component checkout path for shell or tool use"
    )
    path.add_argument("component")
    path.add_argument("--profile", default="default")

    build = commands.add_parser("build", help="build a component or the playable system")
    build.add_argument("target", help="all or a component name")
    build.add_argument("--profile", default="default")
    build.add_argument("--test", action="store_true")

    topology = commands.add_parser(
        "topology", help="inspect a resolved multi-component topology"
    )
    topology_commands = topology.add_subparsers(
        dest="topology_command", required=True
    )
    topology_show = topology_commands.add_parser("show")
    topology_show.add_argument("profile", nargs="?", default="default")
    topology_show.add_argument("--state", default="default")
    topology_show.add_argument(
        "--service", choices=["server", "client"], action="append"
    )
    topology_show.add_argument("--json", action="store_true")

    up = commands.add_parser("up", help="build and start a supervised topology")
    up.add_argument("--name")
    up.add_argument("--profile", default="default")
    up.add_argument("--state", default="default")
    up.add_argument(
        "--port",
        type=int,
        help="server UDP port (default: choose an available port)",
    )
    up.add_argument("--service", choices=["server", "client"], action="append")
    up.add_argument("--json", action="store_true")

    ps = commands.add_parser("ps", help="show supervised topology processes")
    ps.add_argument("name", nargs="?")
    ps.add_argument("--json", action="store_true")

    logs = commands.add_parser("logs", help="show supervised topology logs")
    logs.add_argument("name", nargs="?", default="default")
    logs.add_argument("service", nargs="?", choices=["server", "client"])
    logs.add_argument("--tail", type=int, default=100)
    logs.add_argument("--follow", "-f", action="store_true")

    down = commands.add_parser("down", help="stop a supervised topology")
    down.add_argument("name", nargs="?", default="default")
    down.add_argument("--json", action="store_true")

    state = commands.add_parser("state", help="register persistent server state")
    state_commands = state.add_subparsers(dest="state_command", required=True)
    state_add = state_commands.add_parser("add")
    state_add.add_argument("name")
    state_add.add_argument("--path", type=Path)
    state_list = state_commands.add_parser("list")
    state_list.add_argument("--json", action="store_true")

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


def _forwarded_arguments(arguments: list[str]) -> list[str]:
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
        elif options.command == "status":
            rows = workspace.repository_status(options.components)
            if options.json:
                print(json.dumps(rows, indent=2, sort_keys=True))
            else:
                for row in rows:
                    if not row["initialized"]:
                        print(f"{row['component']}\tnot-initialized\t{row['path']}")
                        continue
                    cleanliness = "dirty" if row["dirty"] else "clean"
                    comparison = (
                        "ahead=? behind=?"
                        if row["ahead"] is None
                        else f"ahead={row['ahead']} behind={row['behind']}"
                    )
                    print(
                        f"{row['component']}\t{row['branch'] or 'detached'}\t"
                        f"{row['head']}\t{cleanliness}\t{comparison}\t{row['path']}"
                    )
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
                rows = workspace.list_worktrees(options.components)
                if options.json:
                    print(
                        json.dumps(
                            [
                                {"component": component, **record}
                                for component, record in rows
                            ],
                            indent=2,
                            sort_keys=True,
                        )
                    )
                else:
                    for component, record in rows:
                        branch = record.get("branch", "detached").removeprefix(
                            "refs/heads/"
                        )
                        print(f"{component}\t{branch}\t{record['worktree']}")
        elif options.command == "profile":
            if options.profile_command == "create":
                workspace.create_profile(options.name, options.source)
            elif options.profile_command == "set":
                if options.primary:
                    workspace.set_profile(options.name, options.component, "primary")
                elif options.worktree is not None:
                    workspace.set_profile(
                        options.name, options.component, "worktree", options.worktree
                    )
                else:
                    workspace.set_profile(
                        options.name, options.component, "path", str(options.path)
                    )
            else:
                rows = workspace.profile_summary(options.name)
                if options.json:
                    print(
                        json.dumps(
                            [
                                {
                                    "component": component,
                                    "path": str(path),
                                    "head": head,
                                    "dirty": dirty,
                                }
                                for component, path, head, dirty in rows
                            ],
                            indent=2,
                            sort_keys=True,
                        )
                    )
                else:
                    for component, path, head, dirty in rows:
                        status = "dirty" if dirty else "clean"
                        print(f"{component}\t{head}\t{status}\t{path}")
        elif options.command == "path":
            print(workspace.component_path(options.component, options.profile))
        elif options.command == "build":
            print(workspace.build(options.target, options.profile, options.test))
        elif options.command == "topology":
            summary = workspace.topology_summary(
                options.profile, options.state, options.service
            )
            if options.json:
                print(json.dumps(summary, indent=2, sort_keys=True))
            else:
                print(f"profile\t{summary['profile']}")
                print(f"services\t{','.join(summary['services'])}")
                print(f"dependencies\t{','.join(summary['dependencies'])}")
                print(f"state\t{summary['state'] or '-'}")
                print(f"build\t{summary['build_root']}")
                for component, row in summary["components"].items():
                    cleanliness = "dirty" if row["dirty"] else "clean"
                    print(
                        f"{component}\t{row['head'][:12]}\t{cleanliness}\t"
                        f"{row['path']}"
                    )
        elif options.command == "up":
            name = options.name or options.profile
            status = workspace.topology_up(
                name, options.profile, options.state, options.service, options.port
            )
            if options.json:
                print(json.dumps(status, indent=2, sort_keys=True))
            else:
                endpoint = status.get("endpoint")
                suffix = (
                    f" at {endpoint['host']}:{endpoint['port']}"
                    if endpoint
                    else ""
                )
                print(f"topology {name}: started{suffix}")
        elif options.command == "ps":
            statuses = (
                [workspace.topology_status(options.name)]
                if options.name
                else workspace.topology_statuses()
            )
            if options.json:
                value = statuses[0] if options.name else statuses
                print(json.dumps(value, indent=2, sort_keys=True))
            else:
                for index, status in enumerate(statuses):
                    if len(statuses) > 1:
                        if index:
                            print()
                        print(f"==> {status['name']} <==")
                    supervisor = status["supervisor"]
                    supervisor_state = (
                        "running" if supervisor["running"] else "stopped"
                    )
                    print(
                        f"supervisor\t{supervisor_state}\t{supervisor['pid']}\t"
                        f"{status['profile']}"
                    )
                    if status["endpoint"]:
                        endpoint = status["endpoint"]
                        print(
                            f"endpoint\t{endpoint['host']}:{endpoint['port']}\t"
                            f"{endpoint['fingerprint']}"
                        )
                    for service, row in status["services"].items():
                        print(
                            f"{service}\t{row['status']}\t{row['pid']}\t"
                            f"{row['log']}"
                        )
        elif options.command == "logs":
            workspace.topology_logs(
                options.name, options.service, options.tail, options.follow
            )
        elif options.command == "down":
            status = workspace.topology_down(options.name)
            if options.json:
                print(json.dumps(status, indent=2, sort_keys=True))
            else:
                print(f"topology {options.name}: stopped")
        elif options.command == "state":
            if options.state_command == "add":
                workspace.state_add(options.name, options.path)
            else:
                states = workspace.list_states()
                if options.json:
                    print(
                        json.dumps(
                            {name: str(path) for name, path in sorted(states.items())},
                            indent=2,
                            sort_keys=True,
                        )
                    )
                else:
                    for name, path in sorted(states.items()):
                        print(f"{name}\t{path}")
        elif options.command == "run":
            forwarded = _forwarded_arguments(options.arguments)
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
    except OSError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
