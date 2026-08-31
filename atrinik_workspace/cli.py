from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
from typing import Any

from .completion import mark, protocol, protocol_command, shell_script
from .model import Manifest, WorkspaceError


# Keep completion startup independent from the heavyweight workspace/runtime module.
# Tests replace this hook directly; normal dispatch resolves it lazily below.
Workspace: Any = None


class Inventory:
    @staticmethod
    def load(*arguments: object, **keywords: object) -> object:
        from .supply_chain import Inventory as implementation

        return implementation.load(*arguments, **keywords)


def repository_roots(*arguments: object, **keywords: object) -> object:
    from .supply_chain import repository_roots as implementation

    return implementation(*arguments, **keywords)


def report_component_commits(*arguments: object, **keywords: object) -> object:
    from .supply_chain import report_component_commits as implementation

    return implementation(*arguments, **keywords)


def version_report(*arguments: object, **keywords: object) -> object:
    from .supply_chain import version_report as implementation

    return implementation(*arguments, **keywords)


def write_generated(*arguments: object, **keywords: object) -> object:
    from .supply_chain import write_generated as implementation

    return implementation(*arguments, **keywords)


def validate_provenance_identity(*arguments: object, **keywords: object) -> object:
    from .provenance_identity import validate_paths as implementation

    return implementation(*arguments, **keywords)


def preflight_provenance_revisions(*arguments: object, **keywords: object) -> object:
    from .provenance_identity import preflight_provenance_revisions as implementation

    return implementation(*arguments, **keywords)


ROOT = Path(__file__).resolve().parents[1]


def _human_bytes(value: int) -> str:
    """Render an exact byte count compactly for human-facing output."""

    if value < 0:
        raise ValueError("byte count cannot be negative")
    units = ("B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB")
    unit = 0
    amount = float(value)
    while amount >= 1024 and unit < len(units) - 1:
        amount /= 1024
        unit += 1
    rounded = round(amount, 1)
    if rounded >= 1024 and unit < len(units) - 1:
        rounded /= 1024
        unit += 1
    if unit == 0:
        return f"{value}{units[unit]}"
    rendered = f"{rounded:.1f}".removesuffix(".0")
    return f"{rendered}{units[unit]}"


class _ExactArgumentParser(argparse.ArgumentParser):
    def __init__(self, *args: object, **kwargs: object):
        kwargs.setdefault("allow_abbrev", False)
        super().__init__(*args, **kwargs)


def parser() -> argparse.ArgumentParser:
    root = _ExactArgumentParser(
        prog="atrinik", description="Atrinik multi-repository development workspace"
    )
    commands = root.add_subparsers(dest="command", required=True)

    manifest = commands.add_parser("manifest", help="validate the component manifest")
    manifest.add_argument("action", choices=["validate"])

    initialize = commands.add_parser("init", help="clone missing physical checkouts")
    mark(initialize.add_argument("components", nargs="*"), "component")
    initialize.add_argument(
        "--with",
        dest="additional_sets",
        action="append",
        choices=["classic"],
        default=[],
        help="add the complete classic cohort to the default checkout cohort",
    )
    mark(initialize.add_argument("--jobs", type=int, default=4), "none")

    sync = commands.add_parser(
        "sync", help="fetch and fast-forward physical checkouts"
    )
    mark(sync.add_argument("components", nargs="*"), "component")
    sync.add_argument(
        "--with",
        dest="additional_sets",
        action="append",
        choices=["classic"],
        default=[],
        help="also synchronize initialized classic checkouts",
    )
    sync.add_argument(
        "--worktrees",
        choices=["none", "merge", "rebase"],
        default="none",
        help="also merge/rebase each physical checkout's clean feature worktrees",
    )

    status = commands.add_parser(
        "status", help="summarize primary physical-checkout state"
    )
    mark(status.add_argument("components", nargs="*"), "component")
    status.add_argument("--json", action="store_true")

    migrate = commands.add_parser(
        "migrate", help="safely migrate an existing workspace layout"
    )
    migrate_commands = migrate.add_subparsers(
        dest="migrate_command", required=True
    )
    migrate_repositories = migrate_commands.add_parser(
        "repositories", help="relocate classic primary checkouts"
    )
    migrate_mode = migrate_repositories.add_mutually_exclusive_group(required=True)
    migrate_mode.add_argument("--dry-run", action="store_true")
    migrate_mode.add_argument("--apply", action="store_true")
    migrate_mode.add_argument("--audit", action="store_true")
    migrate_repositories.add_argument("--json", action="store_true")
    migrate_content = migrate_commands.add_parser(
        "content", help="cut persisted Classic profiles over to content@main"
    )
    migrate_content_mode = migrate_content.add_mutually_exclusive_group(required=True)
    migrate_content_mode.add_argument("--dry-run", action="store_true")
    migrate_content_mode.add_argument("--apply", action="store_true")
    migrate_content_mode.add_argument("--audit", action="store_true")
    migrate_content_mode.add_argument("--restore", action="store_true")
    migrate_content.add_argument("--json", action="store_true")
    migrate_filesystem = migrate_commands.add_parser(
        "filesystem", help="convert legacy filesystem identities after a remount"
    )
    migrate_filesystem_mode = migrate_filesystem.add_mutually_exclusive_group(
        required=True
    )
    migrate_filesystem_mode.add_argument("--dry-run", action="store_true")
    migrate_filesystem_mode.add_argument("--apply", action="store_true")
    migrate_filesystem_mode.add_argument("--audit", action="store_true")
    migrate_filesystem.add_argument("--confirm-remount", action="store_true")
    migrate_filesystem.add_argument("--json", action="store_true")

    worktree = commands.add_parser(
        "worktree", help="manage physical-checkout worktrees"
    )
    worktree_commands = worktree.add_subparsers(dest="worktree_command", required=True)
    worktree_create = worktree_commands.add_parser("create")
    mark(worktree_create.add_argument("component"), "component")
    mark(worktree_create.add_argument("label"), "none")
    mark(worktree_create.add_argument("--branch", required=True), "none")
    mark(worktree_create.add_argument("--from", dest="start_point"), "none")
    worktree_create.add_argument("--existing", action="store_true")
    worktree_remove = worktree_commands.add_parser("remove")
    mark(worktree_remove.add_argument("component"), "component")
    mark(worktree_remove.add_argument("label"), "worktree")
    worktree_list = worktree_commands.add_parser("list")
    mark(worktree_list.add_argument("components", nargs="*"), "component")
    worktree_list.add_argument(
        "--wrapper-self",
        action="store_true",
        help="list the wrapper repository's complete Git worktree inventory",
    )
    worktree_list.add_argument("--json", action="store_true")

    scope = commands.add_parser(
        "scope", help="provision and release isolated agent development scopes"
    )
    scope_commands = scope.add_subparsers(dest="scope_command", required=True)
    scope_create = scope_commands.add_parser(
        "create", help="atomically provision one complete development scope"
    )
    mark(scope_create.add_argument("components", nargs="+"), "component")
    mark(scope_create.add_argument("--name"), "none")
    mark(scope_create.add_argument("--from", dest="base_profile", default="default"), "profile")
    mark(
        scope_create.add_argument(
            "--label", action="append", default=[], metavar="CHECKOUT=LABEL"
        ),
        "none",
    )
    mark(
        scope_create.add_argument(
            "--branch", action="append", default=[], metavar="CHECKOUT=BRANCH"
        ),
        "none",
    )
    mark(
        scope_create.add_argument(
            "--start-point", action="append", default=[], metavar="CHECKOUT=REF"
        ),
        "none",
    )
    mark(scope_create.add_argument("--topology"), "none")
    scope_state = scope_create.add_mutually_exclusive_group()
    scope_state.add_argument(
        "--temporary-state",
        dest="state_mode",
        action="store_const",
        const="temporary",
        help="use generation-owned temporary state (default)",
    )
    mark(scope_state.add_argument("--state", dest="state_name"), "state")
    scope_state.add_argument(
        "--default-state",
        dest="state_mode",
        action="store_const",
        const="default",
        help="deliberately select shared persistent default state",
    )
    scope_create.set_defaults(state_mode="temporary")
    scope_create.add_argument("--json", action="store_true")
    scope_show = scope_commands.add_parser("show")
    mark(scope_show.add_argument("name"), "scope")
    scope_show.add_argument("--json", action="store_true")
    scope_list = scope_commands.add_parser("list")
    scope_list.add_argument("--json", action="store_true")
    scope_release = scope_commands.add_parser(
        "release", help="preview or apply exact scope release"
    )
    mark(scope_release.add_argument("name"), "scope")
    scope_release_mode = scope_release.add_mutually_exclusive_group(required=True)
    scope_release_mode.add_argument("--dry-run", action="store_true")
    scope_release_mode.add_argument("--apply", action="store_true")
    mark(scope_release.add_argument("--plan"), "none")
    scope_release.add_argument("--json", action="store_true")

    cleanup = commands.add_parser(
        "cleanup", help="preview or reclaim stale workspace data"
    )
    mark(cleanup.add_argument(
        "components",
        nargs="*",
        help=(
            "limit worktrees to checkout/component identities, or select exact "
            "topology or cleanup-journal names"
        ),
    ), "component")
    cleanup.add_argument(
        "--scope",
        action="append",
        choices=[
            "worktrees", "builds", "temporary-states", "npm-cache",
            "compiler-cache", "sound-cache", "topologies", "cleanup-journals",
            "all",
        ],
        default=[],
    )
    mark(cleanup.add_argument("--older-than", type=int, default=7, metavar="DAYS"), "none")
    cleanup_mode = cleanup.add_mutually_exclusive_group()
    cleanup_mode.add_argument("--dry-run", action="store_true")
    cleanup_mode.add_argument("--apply", action="store_true")
    cleanup.add_argument("--json", action="store_true")

    profile = commands.add_parser("profile", help="manage coherent source profiles")
    profile_commands = profile.add_subparsers(dest="profile_command", required=True)
    profile_create = profile_commands.add_parser("create")
    mark(profile_create.add_argument("name"), "none")
    mark(profile_create.add_argument("--from", dest="source", default="default"), "profile")
    profile_set = profile_commands.add_parser("set")
    mark(profile_set.add_argument("name"), "saved_profile")
    mark(profile_set.add_argument("component"), "profile_selection")
    selector = profile_set.add_mutually_exclusive_group(required=True)
    selector.add_argument("--primary", action="store_true")
    mark(selector.add_argument("--worktree"), "worktree")
    mark(selector.add_argument("--path", type=Path), "path")
    profile_show = profile_commands.add_parser("show")
    mark(profile_show.add_argument("name", nargs="?", default="default"), "profile")
    profile_show.add_argument("--json", action="store_true")
    profile_sound = profile_commands.add_parser(
        "sound-mode", help="select source, local-playtest, or released Classic sound"
    )
    mark(profile_sound.add_argument("name"), "saved_profile")
    profile_sound.add_argument(
        "mode", choices=["source", "local-playtest", "released"]
    )
    for flag, destination in (
        ("--release-repository", "release_repository"),
        ("--release-tag", "release_tag"),
        ("--release-product-version", "release_product_version"),
        ("--release-source-commit", "release_source_commit"),
        ("--release-source-tree", "release_source_tree"),
        ("--release-asset-url", "release_asset_url"),
        ("--release-archive-sha256", "release_archive_sha256"),
        ("--release-manifest-sha256", "release_manifest_sha256"),
        ("--release-source-manifest-sha256", "release_source_manifest_sha256"),
        ("--release-schema-sha256", "release_schema_sha256"),
        ("--release-toolchain-sha256", "release_toolchain_sha256"),
        ("--release-tree-sha256", "release_tree_sha256"),
    ):
        mark(profile_sound.add_argument(flag, dest=destination), "none")

    path = commands.add_parser(
        "path", help="print a resolved logical-component source path"
    )
    mark(path.add_argument("component"), "profile_component")
    mark(path.add_argument("--profile", default="default"), "profile")

    build = commands.add_parser("build", help="build a component or the playable system")
    mark(build.add_argument("target", help="all or a component name"), "build_target")
    mark(build.add_argument("--profile", default="default"), "profile")
    build.add_argument("--test", action="store_true")
    build.add_argument(
        "--force-reconfigure",
        action="store_true",
        help="run CMake configure even when its managed fingerprint is unchanged",
    )
    build.add_argument(
        "--no-ccache",
        action="store_true",
        help="disable automatic C/C++ compiler caching",
    )

    package = commands.add_parser(
        "package", help="package a resolved profile for another host"
    )
    package_commands = package.add_subparsers(
        dest="package_command", required=True
    )
    package_windows = package_commands.add_parser(
        "windows", help="build one portable Windows review topology ZIP"
    )
    mark(package_windows.add_argument("--profile", default="classic"), "profile")
    mark(package_windows.add_argument("--state", default="default"), "state")
    mark(
        package_windows.add_argument(
            "--port", type=int, default=1730,
            help="local Windows server UDP port (default: 1730)",
        ),
        "none",
    )
    mark(package_windows.add_argument("--output", type=Path), "path")
    package_windows.add_argument("--json", action="store_true")

    topology = commands.add_parser(
        "topology", help="inspect a resolved multi-component topology"
    )
    topology_commands = topology.add_subparsers(
        dest="topology_command", required=True
    )
    topology_show = topology_commands.add_parser("show")
    mark(topology_show.add_argument("profile", nargs="?", default="default"), "profile")
    topology_state = topology_show.add_mutually_exclusive_group()
    mark(
        topology_state.add_argument("--state", default="default"),
        "state",
    )
    topology_state.add_argument(
        "--temporary-state",
        dest="state_mode",
        action="store_const",
        const="temporary",
        help="use a fresh disposable state owned by the topology generation",
    )
    topology_state.add_argument(
        "--default-state",
        dest="state_mode",
        action="store_const",
        const="default",
        help="use the legacy managed persistent default state explicitly",
    )
    topology_show.add_argument(
        "--service", choices=["server", "client"], action="append"
    )
    topology_show.add_argument("--json", action="store_true")

    up = commands.add_parser("up", help="build and start a supervised topology")
    mark(up.add_argument("--name"), "none")
    mark(up.add_argument("--profile", default="default"), "profile")
    up_state = up.add_mutually_exclusive_group()
    mark(up_state.add_argument("--state", default="default"), "state")
    up_state.add_argument(
        "--temporary-state",
        dest="state_mode",
        action="store_const",
        const="temporary",
        help="use a fresh disposable state owned by this topology generation",
    )
    up_state.add_argument(
        "--default-state",
        dest="state_mode",
        action="store_const",
        const="default",
        help="use the legacy managed persistent default state explicitly",
    )
    mark(up.add_argument(
        "--port",
        type=int,
        help="server UDP port (default: choose an available port)",
    ), "none")
    up.add_argument("--service", choices=["server", "client"], action="append")
    up.add_argument("--json", action="store_true")

    ps = commands.add_parser("ps", help="show supervised topology processes")
    mark(ps.add_argument("name", nargs="?"), "topology")
    ps.add_argument("--json", action="store_true")

    logs = commands.add_parser("logs", help="show supervised topology logs")
    mark(logs.add_argument("name", nargs="?", default="default"), "topology")
    logs.add_argument("service", nargs="?", choices=["server", "client"])
    mark(logs.add_argument("--tail", type=int, default=100), "none")
    logs.add_argument("--follow", "-f", action="store_true")

    down = commands.add_parser("down", help="stop a supervised topology")
    mark(down.add_argument("name", nargs="?", default="default"), "topology")
    down.add_argument(
        "--retain-state",
        action="store_true",
        help="retain a cleanly stopped temporary state for later promotion",
    )
    down.add_argument("--json", action="store_true")

    state = commands.add_parser("state", help="register persistent server state")
    state_commands = state.add_subparsers(dest="state_command", required=True)
    state_add = state_commands.add_parser("add")
    mark(state_add.add_argument("name"), "none")
    mark(state_add.add_argument("--path", type=Path), "path")
    state_list = state_commands.add_parser("list")
    state_list.add_argument("--json", action="store_true")
    state_promote = state_commands.add_parser(
        "promote", help="promote a stopped retained temporary topology state"
    )
    mark(state_promote.add_argument("topology"), "topology")
    mark(state_promote.add_argument("name"), "none")
    state_promote.add_argument("--json", action="store_true")

    scenario = commands.add_parser(
        "scenario", help="manage deterministic local test scenarios"
    )
    scenario_commands = scenario.add_subparsers(
        dest="scenario_command", required=True
    )
    scenario_create = scenario_commands.add_parser("create")
    mark(scenario_create.add_argument("name"), "none")
    mark(scenario_create.add_argument("--profile", default="default"), "profile")
    mark(scenario_create.add_argument("--preset", default="basic-player"), "none")
    scenario_create.add_argument("--json", action="store_true")
    scenario_list = scenario_commands.add_parser("list")
    scenario_list.add_argument("--json", action="store_true")
    scenario_show = scenario_commands.add_parser("show")
    mark(scenario_show.add_argument("name"), "scenario")
    scenario_show.add_argument("--json", action="store_true")
    scenario_credentials = scenario_commands.add_parser("credentials")
    mark(scenario_credentials.add_argument("name"), "scenario")
    scenario_reset = scenario_commands.add_parser("reset")
    mark(scenario_reset.add_argument("name"), "scenario")
    scenario_reset.add_argument("--json", action="store_true")

    supply_chain = commands.add_parser(
        "supply-chain", help="validate and report dependency ownership"
    )
    supply_chain_commands = supply_chain.add_subparsers(
        dest="supply_chain_command", required=True
    )
    supply_chain_commands.add_parser("validate")
    supply_chain_audit = supply_chain_commands.add_parser("audit")
    mark(supply_chain_audit.add_argument("--profile", default="default"), "profile")
    mark(supply_chain_audit.add_argument(
        "--repository",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="override one component checkout or source root for a read-only audit",
    ), "none")
    supply_chain_report = supply_chain_commands.add_parser("report")
    supply_chain_report.add_argument(
        "--format", choices=["cyclonedx", "licenses", "spdx"], required=True
    )
    mark(supply_chain_report.add_argument(
        "--profile",
        default="default",
        help="profile whose initialized checkouts supply first-party commits",
    ), "profile")
    mark(supply_chain_report.add_argument("--output", type=Path), "path")
    supply_chain_versions = supply_chain_commands.add_parser("versions")
    mark(supply_chain_versions.add_argument("--output", type=Path), "path")

    provenance = commands.add_parser(
        "provenance", help="validate the canonical public identity registry"
    )
    provenance_commands = provenance.add_subparsers(
        dest="provenance_command", required=True
    )
    provenance_validate = provenance_commands.add_parser("validate")
    mark(
        provenance_validate.add_argument(
            "--registry",
            type=Path,
            default=ROOT / "governance/provenance-identities/registry.json",
        ),
        "path",
    )
    mark(
        provenance_validate.add_argument(
            "--schema",
            type=Path,
            default=ROOT / "governance/provenance-identities/schema-v1.json",
        ),
        "path",
    )
    mark(
        provenance_validate.add_argument(
            "--reviewers",
            type=Path,
            default=ROOT / "governance/provenance-identities/reviewers.json",
        ),
        "path",
    )
    mark(
        provenance_validate.add_argument(
            "--reference", type=Path, action="append", default=[]
        ),
        "path",
    )
    mark(
        provenance_validate.add_argument(
            "--non-authorizing-audit-ref",
            metavar="REF",
            help="audit a pre-merge ref; output is not approval for production reuse",
        ),
        "none",
    )
    provenance_preflight = provenance_commands.add_parser(
        "preflight", help="resolve pinned provenance revisions and Git objects"
    )
    mark(
        provenance_preflight.add_argument(
            "--migration",
            type=Path,
            default=ROOT / "governance/provenance-revision-migration.json",
        ),
        "path",
    )

    launch = commands.add_parser("run", help="build and run client or server")
    launch_commands = launch.add_subparsers(dest="target", required=True)
    for target in ("client", "server"):
        target_parser = launch_commands.add_parser(target)
        mark(target_parser.add_argument("--profile", default="default"), "profile")
        mark(target_parser.add_argument("--state", default="default"), "state")
        mark(target_parser.add_argument(
            "--port", type=int, default=1730, help="server UDP port (default: 1730)"
        ), "none")
        target_parser.add_argument("--dry-run", action="store_true")
        mark(target_parser.add_argument("arguments", nargs=argparse.REMAINDER), "none")

    completion = commands.add_parser(
        "completion", help="emit a native shell completion activation script"
    )
    completion.add_argument("shell", choices=["bash", "zsh", "fish"])
    return root


def _forwarded_arguments(arguments: list[str]) -> list[str]:
    return arguments[1:] if arguments and arguments[0] == "--" else arguments


def _print_scenario(summary: dict[str, object]) -> None:
    print(f"scenario\t{summary['name']}")
    print(f"profile\t{summary['profile']}")
    print(f"preset\t{summary['preset']}")
    print(f"state\t{summary['state']}")
    print(f"account\t{summary['account']}")
    print(f"character\t{summary['character']}")
    print(f"path\t{summary['path']}")


def _print_scenario_handoff(summary: dict[str, object]) -> None:
    name = summary["name"]
    profile = summary["profile"]
    state = summary["state"]
    print("manual verification:")
    print(f"  ./atrinik profile show {profile}")
    print(f"  ./atrinik build server --profile {profile} --test")
    print(f"  ./atrinik topology show {profile} --state {state} --json")
    print(f"  ./atrinik up --name {name} --profile {profile} --state {state}")
    print("  # the supervised client logs in with the scenario automatically")
    print(f"  ./atrinik ps {name} --json")
    print(f"  ./atrinik logs {name} server --follow")
    print(f"  ./atrinik logs {name} client --follow")
    print(f"  ./atrinik down {name}")


def main(arguments: list[str] | None = None) -> int:
    raw_arguments = list(sys.argv[1:] if arguments is None else arguments)
    root_parser = parser()
    if raw_arguments and raw_arguments[0] == protocol_command():
        return protocol(root_parser, ROOT, raw_arguments[1:])
    options = root_parser.parse_args(raw_arguments)
    workspace: Any = None
    command_maintenance: Any = None
    try:
        if options.command == "completion":
            print(shell_script(options.shell), end="")
            return 0
        if options.command == "manifest":
            manifest = Manifest.load(ROOT / "components.json")
            print(f"components.json: valid ({len(manifest.components)} components)")
            return 0
        if options.command == "provenance":
            if options.provenance_command == "preflight":
                revisions, objects = preflight_provenance_revisions(
                    ROOT, migration_path=options.migration
                )
                migration_display = options.migration
                try:
                    migration_display = options.migration.resolve().relative_to(ROOT)
                except ValueError:
                    pass
                print(
                    f"{migration_display}: valid "
                    f"({revisions} coordinator revisions, {objects} Git objects)"
                )
                return 0
            count = validate_provenance_identity(
                ROOT,
                registry_path=options.registry,
                schema_path=options.schema,
                reviewers_path=options.reviewers,
                reference_paths=options.reference,
                as_of=datetime.now(timezone.utc).date(),
                trusted_ref=options.non_authorizing_audit_ref or "origin/main",
            )
            prefix = (
                "NON-AUTHORIZING AUDIT: "
                if options.non_authorizing_audit_ref
                else ""
            )
            print(
                prefix + "governance/provenance-identities/registry.json: valid "
                f"({count} records, {len(options.reference)} references)"
            )
            return 0

        if (
            options.command == "migrate"
            and options.migrate_command == "filesystem"
        ):
            from .filesystem_migration import migrate_filesystem_records

            mode = (
                "apply"
                if options.apply
                else "audit"
                if options.audit
                else "dry-run"
            )
            result = migrate_filesystem_records(
                ROOT,
                mode,
                confirm_remount=options.confirm_remount,
            )
            if options.json:
                print(json.dumps(result, indent=2, sort_keys=True))
            else:
                print(f"migration\t{result['migration']}")
                print(f"status\t{result['status']}")
                for record in result.get("records", []):
                    print(
                        f"record\t{record.get('status', 'planned')}\t"
                        f"{record['path']}"
                    )
                if result.get("requires_confirm_remount"):
                    print(
                        "recovery\tlegacy identities require "
                        "--apply --confirm-remount"
                    )
            return 0

        workspace_type = Workspace
        if workspace_type is None:
            from .workspace import Workspace as workspace_type
        read_only_dry_run = (
            options.command == "cleanup" and not options.apply
        ) or (
            options.command == "migrate"
            and options.migrate_command in {"repositories", "content"}
            and (options.dry_run or options.audit)
        )
        workspace = workspace_type(
            ROOT, backfill_references=not read_only_dry_run
        )
        # Foreground runs acquire the maintenance barrier only while resolving,
        # building, and publishing their sealed runtime generation. Keeping the
        # CLI-wide reader after publication would needlessly block migration for
        # the lifetime of an immutable client/server process.
        if options.command not in {"migrate", "run"}:
            command_maintenance = workspace.command_maintenance()
            command_maintenance.__enter__()
        if options.command == "supply-chain":
            inventory = Inventory.load(
                ROOT / "supply-chain" / "inventory.json", ROOT / "components.json"
            )
            inventory.validate_schema(ROOT / "supply-chain" / "schema.json")
            if options.supply_chain_command == "validate":
                print(
                    "supply-chain/inventory.json: valid "
                    f"({len(inventory.dependencies)} dependencies)"
                )
            elif options.supply_chain_command == "audit":
                for message in inventory.audit(
                    repository_roots(
                        ROOT, workspace, options.profile, options.repository
                    )
                ):
                    print(message)
                summary = workspace.profile_summary(options.profile)
                release = summary.get("sound_release")
                if summary.get("sound_mode") == "released" and isinstance(
                    release, dict
                ):
                    print(
                        "sound-release: "
                        f"{release['repository']}@{release['tag']} "
                        f"commit={release['source_commit']} "
                        f"tree={release['source_tree']} "
                        f"archive=sha256:{release['archive_sha256']} "
                        f"logical-tree=sha256:{release['output_tree_sha256']}"
                    )
            elif options.supply_chain_command == "report":
                stack, commits = report_component_commits(
                    ROOT, workspace, options.profile
                )
                summary = workspace.profile_summary(options.profile)
                release = (
                    summary.get("sound_release")
                    if summary.get("sound_mode") == "released"
                    else None
                )
                report_arguments = [options.format, commits, stack]
                if isinstance(release, dict):
                    report_arguments.append(release)
                write_generated(
                    ROOT,
                    options.output,
                    inventory.report(*report_arguments),
                )
            else:
                write_generated(ROOT, options.output, version_report(inventory))
        elif options.command == "init":
            workspace.initialize(
                options.components,
                options.jobs,
                include_classic="classic" in options.additional_sets,
            )
        elif options.command == "sync":
            workspace.sync(
                options.components,
                options.worktrees,
                include_classic="classic" in options.additional_sets,
            )
        elif options.command == "status":
            rows = workspace.repository_status(options.components)
            if options.json:
                print(json.dumps(rows, indent=2, sort_keys=True))
            else:
                for row in rows:
                    if not row["initialized"]:
                        membership = ",".join(row["cohorts"])
                        print(
                            f"{row['component']}\tnot-initialized\t"
                            f"cohorts={membership}\toptional={str(row['optional']).lower()}\t"
                            f"{row['path']}"
                        )
                        continue
                    cleanliness = "dirty" if row["dirty"] else "clean"
                    comparison = (
                        "ahead=? behind=?"
                        if row["ahead"] is None
                        else f"ahead={row['ahead']} behind={row['behind']}"
                    )
                    print(
                        f"{row['component']}\t{row['branch'] or 'detached'}\t"
                        f"{row['head']}\t{cleanliness}\t{comparison}\t"
                        f"cohorts={','.join(row['cohorts'])}\t{row['path']}"
                    )
        elif options.command == "migrate":
            mode = (
                "apply"
                if options.apply
                else "audit"
                if options.audit
                else "restore"
                if getattr(options, "restore", False)
                else "dry-run"
            )
            result = (
                workspace.migrate_content(mode)
                if options.migrate_command == "content"
                else workspace.migrate_repositories(mode)
            )
            if options.json:
                print(json.dumps(result, indent=2, sort_keys=True))
            else:
                print(f"migration\t{result['migration']}")
                print(f"status\t{result['status']}")
                classic = result.get("classic", {})
                if classic:
                    print(
                        f"classic\t{classic.get('status', '-')}\t"
                        f"{classic.get('path', '-')}"
                    )
                for source in result.get("sources", []):
                    print(
                        f"source\t{source['status']}\t{source['component']}\t"
                        f"{source.get('source') or '-'}\t"
                        f"{source.get('archive') or '-'}"
                    )
                for worktree in result.get("worktree_migrations", []):
                    print(
                        f"worktree\t{worktree['status']}\t"
                        f"{worktree['component']}\t{worktree['path']}\t"
                        f"{worktree.get('destination') or '-'}"
                    )
                for composite in result.get("composite_worktrees", []):
                    print(
                        f"composite\t{composite['status']}\t"
                        f"{composite['profile']}\t{composite['destination']}"
                    )
                for profile in result.get("profile_rewrites", []):
                    print(
                        f"profile\t{profile['status']}\t{profile['name']}\t"
                        f"{profile['path']}"
                    )
                for profile in result.get("profiles", []):
                    print(
                        f"profile\t{profile['status']}\t{profile['name']}\t"
                        f"{profile['path']}"
                    )
                for worktree in result.get("worktree_moves", []):
                    print(
                        f"worktree\tmove\t{worktree['profile']}\t"
                        f"{worktree['source']}\t{worktree['destination']}"
                    )
                for topology in result.get("topologies", []):
                    print(
                        f"topology\t{topology['status']}\t{topology['name']}\t"
                        f"{topology['path']}"
                    )
                for inert in result.get("inert_paths", []):
                    print(
                        f"inert\t{inert['status']}\t{inert['name']}\t"
                        f"{inert['path']}"
                    )
                for refusal in result["refusals"]:
                    print(
                        f"refusal\t{refusal['code']}\t{refusal['message']}\n"
                        f"recovery\t{refusal['recovery']}"
                    )
            if result["refusals"]:
                return 1
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
                if options.wrapper_self and options.components:
                    raise WorkspaceError(
                        "--wrapper-self cannot be combined with component selectors"
                    )
                rows = (
                    workspace.list_wrapper_worktrees()
                    if options.wrapper_self
                    else workspace.list_worktrees(options.components)
                )
                if options.json:
                    rendered = json.dumps(
                        [
                            {"component": component, **record}
                            for component, record in rows
                        ],
                        indent=2,
                        sort_keys=True,
                    )
                    if (
                        options.wrapper_self
                        and len(rendered.encode("utf-8")) + 1 > 512 * 1024
                    ):
                        raise WorkspaceError(
                            "wrapper worktree inventory JSON exceeds the retained-evidence limit"
                        )
                    print(rendered)
                else:
                    for component, record in rows:
                        branch = record.get("branch", "detached").removeprefix(
                            "refs/heads/"
                        )
                        print(f"{component}\t{branch}\t{record['worktree']}")
        elif options.command == "scope":
            if options.scope_command == "create":
                state_mode = "named" if options.state_name is not None else options.state_mode
                result = workspace.scope_create(
                    options.components,
                    name=options.name,
                    base_profile=options.base_profile,
                    labels=options.label,
                    branches=options.branch,
                    start_points=options.start_point,
                    topology=options.topology,
                    state_mode=state_mode,
                    state_name=options.state_name,
                )
            elif options.scope_command == "show":
                result = workspace.scope_show(options.name)
            elif options.scope_command == "list":
                result = workspace.scope_list()
            else:
                if options.dry_run and options.plan is not None:
                    raise WorkspaceError("--plan is accepted only with --apply")
                result = workspace.scope_release(
                    options.name,
                    apply=options.apply,
                    plan_sha256=options.plan,
                )
            if options.json:
                print(json.dumps(result, indent=2, sort_keys=True))
            elif isinstance(result, list):
                for record in result:
                    print(
                        f"{record['name']}\t{record['status']}\t"
                        f"{record['profile']['name']}\t{record['topology']['name']}"
                    )
            elif options.scope_command == "release":
                print(f"scope\t{result['scope']}\t{result['mode']}")
                print(f"plan\t{result['plan_sha256']}")
                for item in result["items"]:
                    print(
                        f"{item['disposition']}\t{item['kind']}\t"
                        f"{item.get('path') or '-'}\t{','.join(item['reasons'])}"
                    )
            else:
                print(f"scope\t{result['name']}\t{result['generation']}")
                print(f"profile\t{result['profile']['name']}\t{result['profile']['path']}")
                print(f"topology\t{result['topology']['name']}\t{result['topology']['path']}")
                for row in result["worktrees"]:
                    print(
                        f"worktree\t{row['checkout']}\t{row['branch']}\t{row['path']}"
                    )
        elif options.command == "cleanup":
            report = workspace.cleanup(
                options.scope,
                options.older_than,
                options.components,
                options.apply,
            )
            if options.json:
                print(json.dumps(report, indent=2, sort_keys=True))
            else:
                for item in report["items"]:
                    age = (
                        "-"
                        if item["age_seconds"] is None
                        else f"{item['age_seconds'] // 86400}d"
                    )
                    reasons = ",".join(item["reasons"])
                    sizes = [
                        f"allocated={_human_bytes(item['allocated_bytes'])}"
                    ]
                    if "ignored_bytes" in item:
                        sizes.append(
                            f"ignored={_human_bytes(item['ignored_bytes'])}"
                        )
                    size_fields = ",".join(sizes)
                    print(
                        f"{item['disposition']}\t{item['kind']}\t"
                        f"{size_fields}\t{age}\t{item['path']}\t"
                        f"{reasons}"
                    )
                    if item["kind"] == "topology":
                        generation = item["generation"] or "-"
                        print(
                            f"topology-observation\t{item['name']}\t"
                            f"liveness={item['liveness']}\t"
                            f"control={item['control_observation']}\t"
                            f"generation={generation}\t"
                            f"process-tree={item['process_tree_lease']}\t"
                            f"runtime-bundle={item['runtime_bundle_lease']}\t"
                            f"port-reservation={item['port_reservation_lease']}\t"
                            f"repository-layout={item['repository_layout_lease']}\t"
                            f"age-basis={item['age_basis'] or '-'}\t"
                            f"age-observed-at={item['age_observed_at'] or '-'}"
                        )
                        for deletion_path in item["deletion_paths"]:
                            print(f"delete\t{item['name']}\t{deletion_path}")
                summary = report["summary"]
                print(
                    "summary\t"
                    f"candidates={summary['candidate_count']} "
                    f"candidate_bytes={_human_bytes(summary['candidate_bytes'])} "
                    f"protected={summary['protected_count']} "
                    f"protected_bytes={_human_bytes(summary['protected_bytes'])} "
                    f"removed={summary['removed_count']} "
                    f"removed_bytes={_human_bytes(summary['removed_bytes'])} "
                    f"errors={summary['error_count']}"
                )
            retry_required = bool(
                options.apply
                and any(
                    item.get("disposition") == "skipped"
                    and "resource_busy" in item.get("reasons", [])
                    for item in report.get("items", [])
                    if isinstance(item, dict)
                )
            )
            failed = bool(
                report["summary"]["error_count"]
                or report.get("aborted")
                or retry_required
            )
            if options.apply and not failed:
                sys.stdout.flush()
                workspace.cleanup_acknowledge(report)
            if retry_required:
                print(
                    "cleanup apply is incomplete; retry the identical request",
                    file=sys.stderr,
                )
            if failed:
                return 1
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
            elif options.profile_command == "sound-mode":
                release_values = {
                    "repository": options.release_repository,
                    "tag": options.release_tag,
                    "product_version": options.release_product_version,
                    "source_commit": options.release_source_commit,
                    "source_tree": options.release_source_tree,
                    "asset_url": options.release_asset_url,
                    "archive_sha256": options.release_archive_sha256,
                    "release_manifest_sha256": options.release_manifest_sha256,
                    "source_manifest_sha256": options.release_source_manifest_sha256,
                    "schema_sha256": options.release_schema_sha256,
                    "toolchain_sha256": options.release_toolchain_sha256,
                    "output_tree_sha256": options.release_tree_sha256,
                    "product": "atrinik-sound-classic-runtime",
                    "manifest_schema_version": 1,
                }
                coordinates = (
                    release_values
                    if options.mode == "released"
                    or any(
                        release_values[key] is not None
                        for key in release_values
                        if key not in {"product", "manifest_schema_version"}
                    )
                    else None
                )
                if coordinates is None:
                    workspace.set_profile_sound_mode(options.name, options.mode)
                else:
                    workspace.set_profile_sound_mode(
                        options.name, options.mode, coordinates
                    )
            else:
                summary = workspace.profile_summary(options.name)
                if options.json:
                    print(json.dumps(summary, indent=2, sort_keys=True))
                else:
                    print(f"profile\t{summary['name']}")
                    print(f"stack\t{summary['stack']}")
                    print(f"sound-mode\t{summary['sound_mode']}")
                    if summary["sound_release"] is not None:
                        print(
                            "sound-release\t"
                            f"{summary['sound_release']['repository']}@"
                            f"{summary['sound_release']['tag']}\t"
                            f"{summary['sound_release']['archive_sha256']}"
                        )
                    for row in summary["components"]:
                        if not row["initialized"]:
                            print(
                                f"{row['component']}\tnot-initialized\t{row['path']}"
                            )
                            continue
                        status = "dirty" if row["dirty"] else "clean"
                        print(
                            f"{row['component']}\t{row['head']}\t{status}\t"
                            f"{row['path']}"
                        )
        elif options.command == "path":
            print(workspace.component_path(options.component, options.profile))
        elif options.command == "build":
            print(
                workspace.build(
                    options.target,
                    options.profile,
                    options.test,
                    force_reconfigure=options.force_reconfigure,
                    use_ccache=not options.no_ccache,
                )
            )
        elif options.command == "package":
            summary = workspace.package_windows_profile(
                options.profile,
                options.state,
                options.output,
                port=options.port,
            )
            if options.json:
                print(json.dumps(summary, indent=2, sort_keys=True))
            else:
                print(f"package\t{summary['path']}")
                print(f"sha256\t{summary['sha256']}")
                print(
                    "sensitive\tcontains private server state, player data, "
                    "and credentials"
                )
        elif options.command == "topology":
            state = None if options.state_mode == "temporary" else options.state
            summary = workspace.topology_summary(
                options.profile,
                state,
                options.service,
                state_mode=options.state_mode,
            )
            if options.json:
                print(json.dumps(summary, indent=2, sort_keys=True))
            else:
                print(f"profile\t{summary['profile']}")
                print(f"stack\t{summary['stack']}")
                sound = summary.get("sound", {"mode": "source"})
                print(f"sound-mode\t{sound['mode']}")
                print(f"services\t{','.join(summary['services'])}")
                print(f"dependencies\t{','.join(summary['dependencies'])}")
                for role, provider in sorted(summary["providers"].items()):
                    print(f"provider\t{role}\t{provider}")
                print(f"state\t{summary['state'] or '-'}")
                state_policy = summary.get("state_policy")
                if isinstance(state_policy, dict):
                    print(
                        "state-policy\t"
                        f"{state_policy['mode']}\t"
                        f"{json.dumps(state_policy['owner'], sort_keys=True)}\t"
                        f"{state_policy['lifecycle']}\t"
                        f"{state_policy.get('path') or 'allocated-on-start'}"
                    )
                print(f"build\t{summary['build_root']}")
                for component, row in summary["components"].items():
                    cleanliness = "dirty" if row["dirty"] else "clean"
                    print(
                        f"{component}\t{row['head'][:12]}\t{cleanliness}\t"
                        f"{row['path']}"
                    )
        elif options.command == "up":
            name = options.name or options.profile
            state = None if options.state_mode == "temporary" else options.state
            status = workspace.topology_up(
                name,
                options.profile,
                state,
                options.service,
                options.port,
                state_mode=options.state_mode,
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
                state_policy = status.get("state_policy")
                if isinstance(state_policy, dict):
                    print(
                        "state-policy\t"
                        f"{state_policy['mode']}\t"
                        f"{json.dumps(state_policy['owner'], sort_keys=True)}\t"
                        f"{state_policy['lifecycle']}\t"
                        f"{state_policy['path']}"
                    )
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
                    supervisor_state = supervisor.get(
                        "liveness",
                        "running" if supervisor["running"] else "stopped",
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
                    state_policy = status.get("state_policy")
                    if isinstance(state_policy, dict):
                        owner = state_policy.get("owner", {})
                        owner_kind = (
                            owner.get("kind", "unknown")
                            if isinstance(owner, dict)
                            else "unknown"
                        )
                        print(
                            "state-policy\t"
                            f"{state_policy['mode']}\t{owner_kind}\t"
                            f"{state_policy['lifecycle']}\t{state_policy['path']}"
                        )
                    for service, row in status["services"].items():
                        print(
                            f"{service}\t{row.get('liveness', row['status'])}\t"
                            f"{row['pid']}\t"
                            f"{row['log']}"
                        )
                    observation = status.get("observation")
                    if (
                        isinstance(observation, dict)
                        and observation.get("process_tree_lease") == "retained"
                    ):
                        if observation.get("runtime_generation") is not None:
                            print(
                                "runtime-generation\t"
                                f"{observation['runtime_bundle_lease']}\t"
                                f"{observation['runtime_generation']}\t"
                                f"{observation['safe_action']}"
                            )
                        else:
                            print(
                                "repository-layout-lease\tretained\t"
                                f"{observation['repository_layout_lease_owner']}\t"
                                f"{observation['safe_action']}"
                            )
        elif options.command == "logs":
            workspace.topology_logs(
                options.name, options.service, options.tail, options.follow
            )
        elif options.command == "down":
            status = (
                workspace.topology_down(options.name, retain_state=True)
                if options.retain_state
                else workspace.topology_down(options.name)
            )
            if options.json:
                print(json.dumps(status, indent=2, sort_keys=True))
            else:
                print(f"topology {options.name}: stopped")
        elif options.command == "state":
            if options.state_command == "add":
                workspace.state_add(options.name, options.path)
            elif options.state_command == "list":
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
            else:
                promoted = workspace.state_promote(
                    options.topology, options.name
                )
                if options.json:
                    print(json.dumps(promoted, indent=2, sort_keys=True))
                else:
                    print(
                        f"state {options.name}: promoted from topology "
                        f"{options.topology} at {promoted['path']}"
                    )
        elif options.command == "scenario":
            if options.scenario_command == "create":
                summary = workspace.scenario_create(
                    options.name, options.profile, options.preset
                )
                if options.json:
                    print(json.dumps(summary, indent=2, sort_keys=True))
                else:
                    _print_scenario(summary)
                    _print_scenario_handoff(summary)
            elif options.scenario_command == "list":
                summaries = workspace.scenario_list()
                if options.json:
                    print(json.dumps(summaries, indent=2, sort_keys=True))
                else:
                    for summary in summaries:
                        if summary.get("inert"):
                            name = json.dumps(str(summary["name"]))[1:-1]
                            path = json.dumps(str(summary["path"]))[1:-1]
                            print(
                                f"{name}\tinert\t"
                                f"{summary['inert_reason']}\t{path}"
                            )
                        else:
                            print(
                                f"{summary['name']}\t{summary['profile']}\t"
                                f"{summary['preset']}\t{summary['state']}"
                            )
            elif options.scenario_command == "show":
                summary = workspace.scenario_show(options.name)
                if options.json:
                    print(json.dumps(summary, indent=2, sort_keys=True))
                else:
                    _print_scenario(summary)
                    _print_scenario_handoff(summary)
            elif options.scenario_command == "credentials":
                credentials = workspace.scenario_credentials(options.name)
                for key in ("account", "character", "password"):
                    print(f"{key}\t{credentials[key]}")
            else:
                summary = workspace.scenario_reset(options.name)
                if options.json:
                    print(json.dumps(summary, indent=2, sort_keys=True))
                else:
                    print(f"scenario {options.name}: reset")
                    _print_scenario(summary)
                    _print_scenario_handoff(summary)
        elif options.command == "run":
            forwarded = _forwarded_arguments(options.arguments)
            if options.target == "client":
                workspace.run_client(
                    options.profile,
                    options.state,
                    options.port,
                    forwarded,
                    options.dry_run,
                )
            else:
                workspace.run_server(
                    options.profile,
                    options.state,
                    options.port,
                    forwarded,
                    options.dry_run,
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
    finally:
        if command_maintenance is not None:
            command_maintenance.__exit__(None, None, None)
        if workspace is not None:
            workspace.close()


if __name__ == "__main__":
    raise SystemExit(main())
