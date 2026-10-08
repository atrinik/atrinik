"""Prepare a pinned Classic walking route without touching scenario state."""
from __future__ import annotations

from contextlib import suppress
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import tempfile
from typing import Any

from .model import WorkspaceError, managed_remove, validate_name
from .path_identity import canonical_path, descriptor_path
from .scenario_benchmark import MAX_ROUTE_BYTES, parse_route, run_bounded


PRODUCER = "brynknot-v1"
PRODUCER_TIMEOUT_SECONDS = 60
BEGIN = b"ATRINIK_WALKING_ROUTE_BEGIN\n"
END = b"\nATRINIK_WALKING_ROUTE_END\n"


def _route_payload(stdout: bytes) -> bytes:
    if len(stdout) > MAX_ROUTE_BYTES + len(BEGIN) + len(END):
        raise WorkspaceError("Classic walking-route producer exceeded its stdout bound")
    if not stdout.startswith(BEGIN) or not stdout.endswith(END):
        raise WorkspaceError("Classic walking-route producer output framing is invalid")
    route = stdout[len(BEGIN):-len(END)]
    if BEGIN in route or END in route:
        raise WorkspaceError("Classic walking-route producer output framing is ambiguous")
    parse_route(route)
    return route


def _source_identities(workspace, snapshot) -> dict[str, dict[str, Any]]:
    profile = snapshot.profile()
    selected = snapshot.paths()
    states = snapshot.checkout_states()
    stack = workspace.manifest.stack(profile["stack"])
    result: dict[str, dict[str, Any]] = {}
    for role, path in sorted(selected.items()):
        provider = stack.providers[role]
        state = states.get(provider.checkout_name)
        if (
            not isinstance(state, dict)
            or state.get("dirty") is not False
            or re.fullmatch(r"[0-9a-f]{40,64}", state.get("head", "")) is None
            or not isinstance(state.get("path"), Path)
        ):
            raise WorkspaceError("walking-route preparation requires clean committed sources")
        result[provider.name] = {
            "path": str(path),
            "checkout_path": str(state["path"]),
            "checkout": provider.checkout_name,
            "repository": provider.repository,
            "branch": provider.branch,
            "source": provider.source,
            "head": state["head"],
            "dirty": False,
        }
    return result


def _file_identity(info: os.stat_result) -> tuple[int, int, int]:
    return info.st_dev, info.st_ino, info.st_mode


def _write_staging(
    directory: int, path: Path, payload: bytes
) -> tuple[str, int, tuple[int, int, int]]:
    descriptor = -1
    name = ""
    identity: tuple[int, int, int] | None = None
    try:
        for _attempt in range(16):
            name = f".{path.name}.{secrets.token_hex(12)}.tmp"
            try:
                descriptor = os.open(
                    name,
                    os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW
                    | os.O_CLOEXEC,
                    0o400,
                    dir_fd=directory,
                )
                break
            except FileExistsError:
                continue
        else:
            raise WorkspaceError("cannot allocate walking-route publication staging")
        view = memoryview(payload)
        while view:
            written = os.write(descriptor, view)
            if written <= 0:
                raise WorkspaceError("cannot write walking-route publication staging")
            view = view[written:]
        os.fsync(descriptor)
        info = os.fstat(descriptor)
        identity = _file_identity(info)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise WorkspaceError("walking-route publication staging is unsafe")
        visible = os.stat(name, dir_fd=directory, follow_symlinks=False)
        if _file_identity(visible) != identity:
            raise WorkspaceError("walking-route publication staging changed")
        return name, descriptor, identity
    except BaseException:
        if descriptor >= 0:
            with suppress(OSError):
                identity = _file_identity(os.fstat(descriptor))
            os.close(descriptor)
        if name and identity is not None:
            _unlink_owned(directory, name, identity)
        raise


def _unlink_owned(
    directory: int, name: str, identity: tuple[int, int, int]
) -> None:
    try:
        info = os.stat(name, dir_fd=directory, follow_symlinks=False)
        if _file_identity(info) == identity:
            os.unlink(name, dir_fd=directory)
    except FileNotFoundError:
        pass


def _publish_pair(output: Path, route: bytes, provenance: bytes) -> None:
    companion = output.with_name(output.name + ".provenance.json")
    try:
        directory = os.open(
            output.parent,
            os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC,
        )
    except OSError as error:
        raise WorkspaceError("walking-route output parent is unavailable") from error
    route_staging = ""
    route_descriptor = -1
    route_identity: tuple[int, int, int] | None = None
    provenance_staging = ""
    provenance_descriptor = -1
    provenance_identity: tuple[int, int, int] | None = None
    published_route_identity: tuple[int, int, int] | None = None
    published_provenance_identity: tuple[int, int, int] | None = None
    route_published = False
    provenance_published = False
    publication_complete = False
    try:
        if descriptor_path(directory) != canonical_path(output.parent):
            raise WorkspaceError("walking-route output parent identity changed")
        route_staging, route_descriptor, route_identity = _write_staging(
            directory, output, route
        )
        provenance_staging, provenance_descriptor, provenance_identity = (
            _write_staging(directory, companion, provenance)
        )
        # The route is the commit point. An interrupted publication can leave
        # an inert companion, never a route that appears user-supplied.
        os.link(
            provenance_staging, companion.name,
            src_dir_fd=directory, dst_dir_fd=directory, follow_symlinks=False
        )
        provenance_published = True
        published_provenance_identity = provenance_identity
        published_provenance = os.stat(
            companion.name, dir_fd=directory, follow_symlinks=False
        )
        observed_provenance_identity = _file_identity(published_provenance)
        if (
            not stat.S_ISREG(published_provenance.st_mode)
            or observed_provenance_identity
            != _file_identity(os.fstat(provenance_descriptor))
            or observed_provenance_identity != provenance_identity
        ):
            raise WorkspaceError("published route provenance differs from its staging")
        os.link(
            route_staging, output.name,
            src_dir_fd=directory, dst_dir_fd=directory, follow_symlinks=False
        )
        route_published = True
        published_route_identity = route_identity
        published_route = os.stat(
            output.name, dir_fd=directory, follow_symlinks=False
        )
        observed_route_identity = _file_identity(published_route)
        if (
            not stat.S_ISREG(published_route.st_mode)
            or observed_route_identity != _file_identity(os.fstat(route_descriptor))
            or observed_route_identity != route_identity
        ):
            raise WorkspaceError("published walking route differs from its staging")
        if (
            descriptor_path(directory) != canonical_path(output.parent)
            or _file_identity(os.stat(
                output.name, dir_fd=directory, follow_symlinks=False
            )) != route_identity
            or _file_identity(os.stat(
                companion.name, dir_fd=directory, follow_symlinks=False
            )) != provenance_identity
        ):
            raise WorkspaceError("walking-route publication identity changed")
        os.fsync(directory)
        publication_complete = True
    except FileExistsError as error:
        raise WorkspaceError("walking-route output or provenance already exists") from error
    except OSError as error:
        raise WorkspaceError("cannot publish walking-route output and provenance") from error
    finally:
        try:
            if not publication_complete:
                # Remove the commit point before its provenance. If removal
                # fails or is interrupted, the surviving route remains pinned.
                if route_published and published_route_identity is not None:
                    _unlink_owned(directory, output.name, published_route_identity)
                if provenance_published and published_provenance_identity is not None:
                    _unlink_owned(
                        directory, companion.name, published_provenance_identity
                    )
        finally:
            try:
                if route_staging and route_identity is not None:
                    _unlink_owned(directory, route_staging, route_identity)
                if provenance_staging and provenance_identity is not None:
                    _unlink_owned(directory, provenance_staging, provenance_identity)
            finally:
                if route_descriptor >= 0:
                    os.close(route_descriptor)
                if provenance_descriptor >= 0:
                    os.close(provenance_descriptor)
                os.close(directory)


def prepare_route(workspace, scenario_name: str, output: Path) -> dict[str, Any]:
    """Generate and publish the fixed route from a fresh disposable server state."""
    validate_name(scenario_name, "scenario name")
    output = Path(output)
    if not output.is_absolute() or output.suffix.lower() != ".xml":
        raise WorkspaceError("walking-route output must be an absolute XML path")
    companion = output.with_name(output.name + ".provenance.json")
    if output.exists() or output.is_symlink() or companion.exists() or companion.is_symlink():
        raise WorkspaceError("walking-route output or provenance already exists")
    try:
        parent = output.parent.stat(follow_symlinks=False)
    except OSError as error:
        raise WorkspaceError("walking-route output parent is unavailable") from error
    if not stat.S_ISDIR(parent.st_mode):
        raise WorkspaceError("walking-route output parent is not a directory")

    initial_scenario = workspace._load_scenario(scenario_name)
    profile_name = initial_scenario["profile"]
    workspace._require_classic_contracts(profile_name, {"server"})
    workspace.paths.ensure()
    operation = f"prepare walking route for scenario {scenario_name}"
    with workspace._resolved_profile_operation(
        profile_name, {"server"}, operation
    ) as snapshot:
        scenario_request = workspace._lease_request(
            "scenario", scenario_name, "shared", operation
        )
        with workspace._resource_locks([scenario_request]):
            scenario = workspace._load_scenario(scenario_name)
            if scenario != initial_scenario:
                raise WorkspaceError("scenario changed during walking-route preparation")
            selected = snapshot.paths()
            sources = _source_identities(workspace, snapshot)
            build_root = workspace._build_resolved(
                "server", profile_name, False, ["server"], selected
            )
            with workspace._profile_build_lock(build_root, profile_name):
                with tempfile.TemporaryDirectory(prefix="atrinik-walking-route-") as temporary:
                    state = Path(temporary) / "state"
                    runtime_path, runtime_purpose = (
                        workspace._server_runtime_coordinate(
                            build_root, state, f"route-{scenario_name}"
                        )
                    )
                    install_data = selected["server"] / "install_data"
                    try:
                        shutil.copytree(install_data, state)
                        workspace._make_tree_owner_writable(state)
                        (state / "tmp").mkdir()
                        workspace._validate_state(state)
                    except OSError as error:
                        raise WorkspaceError("cannot prepare fresh walking-route state") from error
                    try:
                        runtime = workspace._prepare_server_runtime(
                            build_root, selected, state, f"route-{scenario_name}"
                        )
                        if runtime != runtime_path:
                            raise WorkspaceError(
                                "managed walking-route runtime coordinate is uncertain"
                            )
                        executable = runtime / "atrinik-server"
                        stdout = run_bounded(
                            [
                                str(executable),
                                f"--content_benchmark_route={PRODUCER}",
                                f"--assetspath={runtime / 'assets'}",
                            ],
                            timeout=PRODUCER_TIMEOUT_SECONDS,
                            limit=MAX_ROUTE_BYTES + len(BEGIN) + len(END),
                            cwd=runtime,
                        )
                        route = _route_payload(stdout)
                    finally:
                        if runtime_path.exists() or runtime_path.is_symlink():
                            managed_remove(
                                runtime_path, workspace.paths.builds,
                                runtime_purpose
                            )

            route_record = parse_route(route)
            provenance_record = {
                "schema_version": 1,
                "producer": PRODUCER,
                "route_sha256": route_record["sha256"],
                "output": str(output),
                "profile": profile_name,
                "profile_generation": snapshot.generation,
                "scenario": scenario_name,
                "sources": sources,
            }
            provenance = (json.dumps(
                provenance_record, ensure_ascii=True, sort_keys=True,
                separators=(",", ":")
            ) + "\n").encode("utf-8")
            _publish_pair(output, route, provenance)
            return provenance_record
