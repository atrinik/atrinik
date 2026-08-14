from __future__ import annotations

import copy
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager, ExitStack, redirect_stderr
import ctypes
import errno
import fcntl
import hashlib
import io
import json
import multiprocessing
import os
from pathlib import Path
import queue
import re
import shutil
import signal
import socket
import stat
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import unittest
from dataclasses import replace
from unittest import mock

from atrinik_workspace import workspace as workspace_module
from atrinik_workspace import cleanup as cleanup_module
from atrinik_workspace import locking as locking_module
from atrinik_workspace.cleanup import Cleanup
from atrinik_workspace.locking import (
    LeaseRequest,
    active_lock_fds,
    resource_lock_path,
    resource_locks,
)
from atrinik_workspace.migration import rename_no_replace as real_rename_no_replace
from atrinik_workspace.launch_identity import client_launch_label
from atrinik_workspace.model import (
    MANAGED_MARKER,
    WorkspaceError,
    atomic_json,
    load_json,
    managed_directory,
    managed_reset,
    profile_key,
)
from atrinik_workspace.workspace import (
    WORKER_SOURCE_EXCLUSIONS,
    WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
    CONFIGURE_METADATA,
    RUNTIME_INPUT_METADATA,
    SOURCE_VIEW_METADATA,
    Workspace,
    _copy_regular_file as real_copy_regular_file,
    _copy_worker_source as real_copy_worker_source,
    _tree_digest,
    _remote_matches as real_remote_matches,
    display_arguments,
    exclusive_lock,
    exclusive_layout_lock,
    shared_lock,
    shared_layout_lock,
    remove_owned_tree,
    replace_directory as worker_replace_directory,
    replace_runtime_directory as workspace_replace_directory,
    run as workspace_run,
)


def synthetic_checkout_states(root: Path) -> dict[str, dict[str, object]]:
    identity = root.stat()
    return {
        "client": {
            "path": root,
            "head": "a" * 40,
            "dirty": False,
            "device": identity.st_dev,
            "inode": identity.st_ino,
            "git_common": str(root / ".git"),
        }
    }


def synthetic_build_process(
    wrapper: str,
    workspace_directory: str,
    profile: str,
    attempting: object,
    entered: object,
    release: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            real_flock = workspace_module.fcntl.flock

            def observe_flock(lock: object, operation: int) -> None:
                if operation & workspace_module.fcntl.LOCK_EX:
                    attempting.set()
                real_flock(lock, operation)

            def pause_build(*_arguments: object) -> None:
                entered.put(profile)
                if not release.wait(10):
                    raise TimeoutError("test build was not released")

            with (
                mock.patch.object(
                    workspace, "_expand_build_target", return_value=["client"]
                ),
                mock.patch.object(
                    workspace,
                    "_resolve_build_profile",
                    return_value={"client": Path(wrapper)},
                ),
                mock.patch.object(
                    workspace,
                    "_selected_checkout_states",
                    return_value=synthetic_checkout_states(Path(wrapper)),
                ),
                mock.patch.object(
                    workspace,
                    "_materialize_clean_primary_sources",
                    side_effect=lambda _profile, selected, _states: (
                        selected,
                        set(),
                        {},
                    ),
                ),
                mock.patch.object(
                    workspace, "_profile_build_key", return_value="a" * 12
                ),
                mock.patch.object(
                    workspace, "_refresh_build_metadata", side_effect=pause_build
                ),
                mock.patch.object(
                    workspace, "_uses_integrated_classic_build", return_value=False
                ),
                mock.patch.object(workspace, "_build_client"),
                mock.patch.object(
                    workspace_module.fcntl, "flock", side_effect=observe_flock
                ),
            ):
                workspace.build("client", profile, False)
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def layout_writer_process(
    wrapper: str,
    workspace_directory: str,
    operation: str,
    attempting: object,
    entered: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            real_flock = workspace_module.fcntl.flock

            def observe_flock(lock: object, lock_operation: int) -> None:
                if lock_operation & workspace_module.fcntl.LOCK_EX:
                    attempting.set()
                real_flock(lock, lock_operation)

            with mock.patch.object(
                workspace_module.fcntl, "flock", side_effect=observe_flock
            ):
                if operation == "sync":
                    with mock.patch.object(
                        workspace,
                        "_sync_components",
                        side_effect=lambda *_: entered.set(),
                    ):
                        workspace.sync([], "none")
                else:
                    with mock.patch.object(
                        workspace,
                        "_remove_worktree",
                        side_effect=lambda *_: entered.set(),
                    ):
                        workspace.remove_worktree("client", "review")
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def synthetic_client_process(
    wrapper: str,
    workspace_directory: str,
    attempting: object,
    entered: object,
    release: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            real_flock = workspace_module.fcntl.flock

            def observe_flock(lock: object, operation: int) -> None:
                if operation & workspace_module.fcntl.LOCK_SH:
                    attempting.set()
                real_flock(lock, operation)

            def pause_client(
                *_arguments: object, **_keywords: object
            ) -> Path:
                entered.set()
                if not release.wait(10):
                    raise TimeoutError("test client was not released")
                return Path(wrapper) / "client"

            with (
                mock.patch.object(
                    workspace, "_run_client", side_effect=pause_client
                ),
                mock.patch.object(
                    workspace_module.fcntl, "flock", side_effect=observe_flock
                ),
            ):
                workspace.run_client("default", "default", 13327, [], True)
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def timed_public_build_process(
    wrapper: str,
    workspace_directory: str,
    profile: str,
    mode: str,
    ready: object,
    start: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            ready.put(profile)
            if not start.wait(5):
                raise TimeoutError("timed build test did not start")
            with (
                mock.patch.object(
                    workspace, "_expand_build_target", return_value=["client"]
                ),
                mock.patch.object(
                    workspace,
                    "_resolve_build_profile",
                    return_value={"client": Path(wrapper)},
                ),
                mock.patch.object(
                    workspace,
                    "_selected_checkout_states",
                    return_value=synthetic_checkout_states(Path(wrapper)),
                ),
                mock.patch.object(
                    workspace,
                    "_materialize_clean_primary_sources",
                    side_effect=lambda _profile, selected, _states: (
                        selected,
                        set(),
                        {},
                    ),
                ),
                mock.patch.object(
                    workspace, "_profile_build_key", return_value="a" * 12
                ),
                mock.patch.object(
                    workspace,
                    "_refresh_build_metadata",
                    side_effect=lambda *_: time.sleep(0.4),
                ),
                mock.patch.object(
                    workspace, "_uses_integrated_classic_build", return_value=False
                ),
                mock.patch.object(workspace, "_build_client"),
            ):
                if mode == "shared":
                    workspace.build("client", profile, False)
                else:
                    with exclusive_lock(
                        workspace.paths.workspace / "repository-layout.lock",
                        "legacy repository layout",
                    ):
                        workspace._build("client", profile, False)
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def fair_layout_reader_process(
    layout_path: str,
    name: str,
    attempting: object | None,
    entered: object,
    release: object | None,
    results: object,
) -> None:
    try:
        if attempting is not None:
            attempting.set()
        with shared_layout_lock(Path(layout_path), "repository layout"):
            entered.put(name)
            if release is not None and not release.wait(10):
                raise TimeoutError(f"{name} reader was not released")
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def fair_layout_writer_process(
    layout_path: str,
    entered: object,
    release: object,
    results: object,
) -> None:
    try:
        with exclusive_layout_lock(Path(layout_path), "repository layout"):
            entered.put("writer")
            if not release.wait(10):
                raise TimeoutError("writer was not released")
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def resource_lease_process(
    workspace_directory: str,
    kind: str,
    coordinate: str,
    mode: str,
    name: str,
    entered: object,
    release: object | None,
    results: object,
    attempting: object | None = None,
    entered_event: object | None = None,
) -> None:
    try:
        request = LeaseRequest(
            kind,
            coordinate,
            mode,
            name,
            "wait for the exact test operation",
        )
        if attempting is not None:
            attempting.set()
        with resource_locks(Path(workspace_directory), [request]):
            if entered_event is not None:
                entered_event.set()
            entered.put(name)
            if release is not None and not release.wait(60):
                raise TimeoutError(f"{name} was not released")
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def public_profile_mutation_process(
    wrapper: str,
    workspace_directory: str,
    attempting: object,
    entered: object,
    release: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            original = workspace._set_profile

            def paused_mutation(
                name: str, component_name: str, kind: str, value: str = ""
            ) -> None:
                entered.set()
                if not release.wait(60):
                    raise TimeoutError("profile mutation was not released")
                original(name, component_name, kind, value)

            with mock.patch.object(
                workspace, "_set_profile", side_effect=paused_mutation
            ):
                attempting.set()
                workspace.set_profile(
                    "profile-a", "client", "worktree", "source-a"
                )
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def public_profile_reader_process(
    wrapper: str,
    workspace_directory: str,
    build_root: str,
    admission_attempting: object,
    entered: object,
    release: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            profile_intent = locking_module.layout_writer_intent_path(
                resource_lock_path(
                    workspace._lease_root(
                        LeaseRequest(
                            "profile", "profile-a", "shared", "build", "wait"
                        )
                    ),
                    "profile",
                    "profile-a",
                )
            )
            original_advisory_lock = locking_module._advisory_lock

            @contextmanager
            def observed_advisory_lock(*args: object, **kwargs: object):
                if Path(args[0]) == profile_intent:
                    admission_attempting.set()
                with original_advisory_lock(*args, **kwargs) as lock:
                    yield lock

            def paused_build(*args: object, **kwargs: object) -> Path:
                entered.set()
                if not release.wait(60):
                    raise TimeoutError("profile reader was not released")
                return Path(build_root)

            with (
                mock.patch.object(
                    locking_module, "_advisory_lock", observed_advisory_lock
                ),
                mock.patch.object(
                    workspace, "_build_resolved", side_effect=paused_build
                ),
            ):
                workspace.build("client", "profile-a", False)
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def public_lifecycle_reader_process(
    wrapper: str,
    workspace_directory: str,
    operation: str,
    build_root: str,
    blocked: object,
    entered: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            blocked.set()
            if operation == "build-c":
                workspace.build("sound", "profile-c", False)
            else:
                with (
                    mock.patch.object(workspace, "_require_client_display"),
                    mock.patch.object(
                        workspace,
                        "_build_resolved",
                        return_value=Path(build_root),
                    ),
                ):
                    try:
                        workspace.topology_up(
                            "topology-c",
                            "profile-c",
                            "default",
                            ["client"],
                        )
                    finally:
                        status_path = (
                            workspace.paths.topologies
                            / "topology-c"
                            / "status.json"
                        )
                        if status_path.is_file():
                            workspace.topology_down("topology-c", timeout=5)
            entered.set()
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def synthetic_server_start_process(
    wrapper: str,
    workspace_directory: str,
    name: str,
    state_path: str,
    build_root: str,
    port: int,
    reserved_port: socket.socket,
    reservation_received: object,
    release_reservation: object,
    port_blocked: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            ports_path = (
                workspace._lease_namespace / "ports" / f"{port}.lock"
            ).resolve()
            state = Path(state_path)
            root = Path(build_root)
            real_flock = locking_module.fcntl.flock

            def observe_flock(lock: object, operation: int) -> None:
                descriptor = lock if isinstance(lock, int) else lock.fileno()
                descriptor_path = Path(
                    os.readlink(f"/proc/self/fd/{descriptor}")
                )
                if (
                    operation & fcntl.LOCK_EX
                    and not operation & fcntl.LOCK_NB
                    and descriptor_path == ports_path
                ):
                    with ports_path.open("a+", encoding="utf-8") as probe:
                        try:
                            real_flock(probe, fcntl.LOCK_EX | fcntl.LOCK_NB)
                        except BlockingIOError:
                            port_blocked.set()
                        else:
                            real_flock(probe, fcntl.LOCK_UN)
                real_flock(lock, operation)
                if (
                    operation & fcntl.LOCK_EX
                    and descriptor_path == ports_path
                ):
                    reserved_port.close()

            state.mkdir(parents=True, exist_ok=True)

            reservation_received.set()
            if not release_reservation.wait(10):
                raise TimeoutError("port reservation was not released")
            # The parent reserves an exact free port across process spawn. The
            # child closes its inherited copy immediately before the wrapper's
            # kernel availability check.
            reserved_port.close()

            def prepared_state_path(
                *_args: object, **kwargs: object
            ) -> Path | tuple[Path, int]:
                if kwargs.get("keep_descriptor"):
                    return state, os.open(
                        state, os.O_PATH | os.O_DIRECTORY | os.O_NOFOLLOW
                    )
                return state

            with (
                mock.patch.object(
                    locking_module.fcntl, "flock", side_effect=observe_flock
                ),
                mock.patch.object(workspace, "_require_classic_contracts"),
                mock.patch.object(
                    workspace, "_state_location", return_value=state
                ),
                mock.patch.object(
                    workspace, "state_path", side_effect=prepared_state_path
                ),
                mock.patch.object(
                    workspace, "_build_resolved", return_value=root
                ),
            ):
                try:
                    status = workspace.topology_up(
                        name, name, name, ["server"], port
                    )
                    if status["endpoint"]["port"] != port:
                        raise AssertionError("topology published the wrong port")
                finally:
                    status_path = workspace.paths.topologies / name / "status.json"
                    if status_path.is_file():
                        workspace.topology_down(name, timeout=5)
        results.put(None)
    except BaseException as error:
        reserved_port.close()
        results.put(f"{type(error).__name__}: {error}")
        raise

def inherited_leases_wrapper_process(
    layout_path: str,
    build_path: str,
    state_path: str,
    child_script: str,
    child_pid_path: str,
) -> None:
    with (
        shared_lock(Path(layout_path), "repository layout"),
        exclusive_lock(Path(build_path), "profile build orphan"),
        exclusive_lock(Path(state_path), "server state orphan"),
    ):
        workspace_run(
            [sys.executable, child_script, child_pid_path],
            diagnostics_to_stderr=False,
        )


def cleanup_writer_wrapper_process(
    layout_path: str,
    repository: str,
    executable_directory: str,
    child_pid_path: str,
) -> None:
    from atrinik_workspace.cleanup import _command as cleanup_command

    with mock.patch.dict(
        os.environ,
        {
            "PATH": executable_directory + os.pathsep + os.environ["PATH"],
            "ATRINIK_TEST_CHILD_PID": child_pid_path,
        },
    ):
        with exclusive_lock(Path(layout_path), "repository layout"):
            cleanup_command(Path(repository), "worktree", "prune")


def compiler_cache_first_use_process(
    wrapper: str,
    workspace_directory: str,
    source_path: str,
    binary_path: str,
    ready: object,
    start: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            ready.put(binary_path)
            if not start.wait(5):
                raise TimeoutError("compiler-cache test did not start")
            with (
                mock.patch(
                    "atrinik_workspace.workspace.shutil.which",
                    side_effect=lambda tool: (
                        "/usr/bin/ccache" if tool == "ccache" else None
                    ),
                ),
                mock.patch.object(
                    workspace,
                    "_add_debug_prefix_environment",
                    return_value={"c": False, "cxx": False},
                ),
                mock.patch("atrinik_workspace.workspace.run"),
            ):
                workspace._cmake(
                    Path(source_path), Path(binary_path), [], False
                )
        results.put(None)
    except BaseException as error:
        results.put(f"{type(error).__name__}: {error}")
        raise


def mixed_layout_operation_process(
    wrapper: str,
    workspace_directory: str,
    operation: str,
    status: dict[str, object],
    ready: object,
    start: object,
    entered: object,
    release: object,
    results: object,
) -> None:
    try:
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": workspace_directory}
        ):
            workspace = Workspace(Path(wrapper))
            topology_root = workspace.paths.topologies / "stress"
            first_entry = True
            topology_generation = 0

            def rendezvous() -> None:
                nonlocal first_entry
                if not first_entry:
                    return
                first_entry = False
                entered.put(operation)
                if not release.wait(20):
                    raise TimeoutError("mixed operation was not released")

            def compile_client(*_arguments: object, **_keywords: object) -> None:
                rendezvous()

            def refresh_topology(
                *_arguments: object, **_keywords: object
            ) -> dict[str, object]:
                nonlocal topology_generation
                selected = workspace._resolve_build_profile("default", {"server"})
                rendezvous()
                current = copy.deepcopy(status)
                current["resolved"] = workspace._topology_resolved_status(
                    "default", selected
                )
                current["dependencies"] = sorted(selected)
                current["error"] = f"stress generation {topology_generation}"
                topology_generation += 1
                atomic_json(topology_root / "status.json", current)
                return current

            ready.put(operation)
            if not start.wait(5):
                raise TimeoutError("mixed operation test did not start")
            if operation == "build":
                with (
                    mock.patch.object(
                        workspace, "_expand_build_target", return_value=["client"]
                    ),
                    mock.patch.object(
                        workspace,
                        "_materialize_clean_primary_sources",
                        side_effect=lambda _profile, selected, _states: (
                            selected,
                            set(),
                            {},
                        ),
                    ),
                    mock.patch.object(
                        workspace, "_build_client", side_effect=compile_client
                    ),
                ):
                    for _ in range(20):
                        workspace.build("client", "stress", False)
            elif operation == "topology":
                with mock.patch.object(
                    workspace, "_topology_up", side_effect=refresh_topology
                ):
                    for _ in range(20):
                        workspace.topology_up(
                            "stress", "default", "default", ["server"], None
                        )
            elif operation == "status":
                with mock.patch(
                    "atrinik_workspace.workspace.process_matches",
                    return_value=False,
                ):
                    for index in range(20):
                        current = workspace.topology_status("stress")
                        for coordinate in current["resolved"].values():
                            checkout = Path(coordinate["checkout_path"])
                            if coordinate["head"] != command(
                                "git", "rev-parse", "HEAD", cwd=checkout
                            ):
                                raise AssertionError("topology coordinates changed")
                        if index == 0:
                            rendezvous()
            else:
                with mock.patch(
                    "atrinik_workspace.cleanup.Cleanup._registered_worktree_paths",
                    return_value=(set(), None),
                ):
                    for index in range(20):
                        report = workspace.cleanup(["builds"], 7, [], False)
                        if report["inventory_errors"]:
                            raise AssertionError(report["inventory_errors"])
                        if index == 0:
                            rendezvous()
        results.put((operation, None))
    except BaseException as error:
        results.put((operation, f"{type(error).__name__}: {error}"))
        raise


def join_or_stop_processes(
    processes: list[multiprocessing.Process], timeout: float
) -> None:
    deadline = time.monotonic() + timeout
    for process in processes:
        process.join(timeout=max(0.0, deadline - time.monotonic()))
    for process in processes:
        if process.is_alive():
            process.terminate()
    for process in processes:
        process.join(timeout=2)
    for process in processes:
        if process.is_alive():
            process.kill()
            process.join(timeout=2)


def wait_for_process_event(
    event: object,
    description: str,
    results: object,
    timeout: float = 5,
) -> None:
    if event.wait(timeout):
        return
    child_results = []
    while True:
        try:
            child_results.append(results.get_nowait())
        except queue.Empty:
            break
    raise AssertionError(
        f"{description} was not reached within {timeout:g}s; "
        f"child results: {child_results or 'none'}"
    )


COMPONENTS = (
    ("client", "client"),
    ("server", "server"),
    ("protocol", "protocol"),
    ("libatrinik", "library"),
    ("content", "content"),
    ("sound", "assets"),
    ("resources", "assets"),
)


def command(*arguments: str, cwd: Path) -> str:
    result = subprocess.run(
        list(arguments), cwd=cwd, check=True, capture_output=True, text=True
    )
    return result.stdout.strip()


class WorkspaceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        manifest = {
            "schema_version": 1,
            "components": [
                {
                    "name": name,
                    "repository": f"atrinik/{name}",
                    "branch": "main",
                    "build": build,
                }
                for name, build in COMPONENTS
            ],
        }
        (self.wrapper / "components.json").write_text(
            json.dumps(manifest), encoding="utf-8"
        )
        self.workspace_directory = self.root / "workspace"
        self.environment = mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(self.workspace_directory)}
        )
        self.environment.start()
        self.workspace = Workspace(self.wrapper)
        self.workspace.paths.ensure()
        self.seeds: dict[str, Path] = {}
        self.origins: dict[str, Path] = {}
        for name, _ in COMPONENTS:
            self.make_component(name)
        self.remote_matcher = mock.patch(
            "atrinik_workspace.workspace._remote_matches",
            side_effect=lambda url, repository: real_remote_matches(url, repository)
            or url == str(self.origins[repository.split("/", 1)[1]]),
        )
        self.remote_matcher.start()

    def tearDown(self) -> None:
        # Test-owned process cleanup must run before its temporary lease paths
        # are removed; unittest's default ordering runs cleanups after tearDown.
        self.doCleanups()
        self.remote_matcher.stop()
        self.environment.stop()
        self.temporary.cleanup()

    def make_component(self, name: str) -> None:
        origin = self.root / "origins" / f"{name}.git"
        origin.parent.mkdir(exist_ok=True)
        command("git", "init", "--bare", str(origin), cwd=self.root)
        seed = self.root / "seeds" / name
        seed.mkdir(parents=True)
        command("git", "init", "-b", "main", cwd=seed)
        command("git", "config", "user.name", "Tests", cwd=seed)
        command("git", "config", "user.email", "tests@example.invalid", cwd=seed)
        (seed / "README").write_text(f"{name}\n", encoding="utf-8")
        if name == "resources":
            (seed / "runtime-paths.txt").write_text("paintings\n", encoding="utf-8")
            (seed / "paintings").mkdir()
            (seed / "paintings" / "scene.jpg").write_text(
                "resource\n", encoding="utf-8"
            )
        if name == "server":
            (seed / "install_data" / "keys").mkdir(parents=True)
            (seed / "install_data" / "unique-items").mkdir()
            (seed / "install_data" / "keys" / "test.pub").write_text(
                "key\n", encoding="utf-8"
            )
            (seed / "install_data" / "unique-items" / ".keep").write_text(
                "\n", encoding="utf-8"
            )
            (seed / "install_data" / "bans").write_text("", encoding="utf-8")
            (seed / "install_data" / "motd").write_text("Welcome\n", encoding="utf-8")
        command("git", "add", ".", cwd=seed)
        command("git", "commit", "-m", "feat: seed", cwd=seed)
        command("git", "remote", "add", "origin", str(origin), cwd=seed)
        command("git", "push", "-u", "origin", "main", cwd=seed)
        command("git", "symbolic-ref", "HEAD", "refs/heads/main", cwd=origin)
        checkout = self.workspace.paths.repositories / name
        checkout.parent.mkdir(parents=True, exist_ok=True)
        command("git", "clone", str(origin), str(checkout), cwd=self.root)
        command(
            "git",
            "remote",
            "add",
            "upstream",
            f"https://github.com/atrinik/{name}.git",
            cwd=checkout,
        )
        command("git", "config", "user.name", "Tests", cwd=checkout)
        command("git", "config", "user.email", "tests@example.invalid", cwd=checkout)
        self.seeds[name] = seed
        self.origins[name] = origin

    def scenario_resolved_fixture(self) -> dict[str, dict[str, object]]:
        resolved: dict[str, dict[str, object]] = {}
        for component in ("server", "content", "resources", "libatrinik", "protocol"):
            path = self.workspace.paths.repositories / component
            provider = self.workspace.manifest.by_name[component]
            resolved[component] = {
                "path": str(path),
                "checkout_path": str(path),
                "checkout": component,
                "repository": provider.repository,
                "branch": provider.branch,
                "source": ".",
                "head": command("git", "rev-parse", "HEAD", cwd=path),
                "dirty": False,
            }
        return resolved

    @staticmethod
    def make_region_map_cache(root: Path) -> Path:
        output = root / "runtime" / "client-maps"
        output.mkdir(parents=True)
        (output / "incuna_-1.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        (output / "incuna_-1.def").write_text(
            "pixel_size 4\n", encoding="utf-8"
        )
        atomic_json(
            output / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "region-map-cache"},
        )
        return output

    def make_rendezvous_server_build(
        self,
        root: Path,
        rendezvous: Path,
        marker: str,
        *,
        peers: int = 2,
        bind_after_gate: bool = False,
    ) -> None:
        binary = root / "build" / "server"
        binary.mkdir(parents=True)
        executable = binary / "atrinik-server"
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import os, pathlib, socket, sys, time\n"
            "port = int(next(value.split('=', 1)[1] for value in sys.argv "
            "if value.startswith('--port_quic=')))\n"
            "datapath = pathlib.Path(next(value.split('=', 1)[1] for value in "
            "sys.argv if value.startswith('--datapath=')))\n"
            "(datapath / 'rendezvous-state-proof').write_text('pinned\\n', "
            "encoding='utf-8')\n"
            f"rendezvous = pathlib.Path({str(rendezvous)!r})\n"
            f"marker = rendezvous / {marker!r}\n"
            "udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)\n"
            + ("marker.write_text('entered\\n', encoding='utf-8')\n" if bind_after_gate else "udp.bind(('0.0.0.0', port))\nmarker.write_text('bound\\n', encoding='utf-8')\n")
            + "deadline = time.monotonic() + 10\n"
            f"while len(list(rendezvous.glob('*.entered'))) + len(list(rendezvous.glob('*.bound'))) < {peers}:\n"
            "    if time.monotonic() >= deadline:\n"
            "        raise RuntimeError('rendezvous timed out')\n"
            "    time.sleep(0.01)\n"
            + ("udp.bind(('0.0.0.0', port))\n" if bind_after_gate else "")
            + "for descriptor in os.listdir('/proc/self/fd'):\n"
            "    try:\n"
            "        target = os.readlink('/proc/self/fd/' + descriptor)\n"
            "    except OSError:\n"
            "        continue\n"
            "    assert '/port-reservations/' not in target, target\n"
            f"print('QUIC certificate SHA-256: {'e' * 64}', flush=True)\n"
            "print('Server ready. Waiting for connections...', flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            root / "runtime" / "content" / "lib",
            root / "runtime" / "content" / "maps",
            root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        atomic_json(
            root / "runtime" / "content" / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        atomic_json(
            root / "runtime" / "resources" / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "resource-view"},
        )
        self.make_region_map_cache(root)
        atomic_json(root / workspace_module.BUILD_METADATA, {})

    @staticmethod
    def make_content_candidate(output: Path, commit: str, payload: str) -> None:
        (output / "lib").mkdir(parents=True)
        (output / "maps").mkdir()
        compatibility = output / "compatibility.json"
        atomic_json(
            compatibility,
            {
                "schema_version": 1,
                "target": "classic",
                "component": "content",
                "repository": "atrinik/content",
                "branch": "main",
                "content_format": "classic-ads-v1",
                "artifact_format": "atrinik-classic-runtime-content-v1",
                "compatible_classic_releases": ">=5.10.1 <6.0.0",
                "consumers": [
                    "classic/client",
                    "classic/editor",
                    "classic/server",
                ],
                "replacement_ready": False,
                "replacement_toolkit_package": False,
            },
        )
        payload_file = output / "maps" / "payload"
        payload_file.write_text(payload, encoding="utf-8")
        library_file = output / "lib" / "payload"
        library_file.write_text(payload, encoding="utf-8")
        license_file = output / "attribution" / "maps" / "COPYING"
        license_file.parent.mkdir(parents=True)
        license_file.write_text("fixture license\n", encoding="utf-8")
        files = []
        for candidate in (compatibility, license_file, library_file, payload_file):
            files.append(
                {
                    "path": candidate.relative_to(output).as_posix(),
                    "sha256": hashlib.sha256(candidate.read_bytes()).hexdigest(),
                    "size": candidate.stat().st_size,
                }
            )
        atomic_json(
            output / "manifest.json",
            {
                "schema_version": 2,
                "target": "classic",
                "source": {
                    "repository": "atrinik/content",
                    "branch": "main",
                    "commit": commit,
                },
                "release_version": "unreleased",
                "content_format": "classic-ads-v1",
                "artifact_format": "atrinik-classic-runtime-content-v1",
                "compatible_classic_releases": ">=5.10.1 <6.0.0",
                "consumers": [
                    "classic/client",
                    "classic/editor",
                    "classic/server",
                ],
                "replacement_ready": False,
                "replacement_toolkit_package": False,
                "license_files": [files[1]],
                "files": files,
            },
        )

    def advance_origin(self, name: str, filename: str) -> str:
        seed = self.seeds[name]
        command("git", "pull", "--ff-only", cwd=seed)
        (seed / filename).write_text("change\n", encoding="utf-8")
        command("git", "add", filename, cwd=seed)
        command("git", "commit", "-m", "fix: advance", cwd=seed)
        command("git", "push", cwd=seed)
        return command("git", "rev-parse", "HEAD", cwd=seed)

    def test_initialize_accepts_existing_real_repositories(self) -> None:
        self.workspace.initialize(None, jobs=3)
        self.assertTrue((self.workspace.paths.repositories / "server" / ".git").exists())

    def test_initialize_is_idempotent_and_preserves_existing_heads(self) -> None:
        before = {
            name: command(
                "git",
                "rev-parse",
                "HEAD",
                cwd=self.workspace.paths.repositories / name,
            )
            for name, _ in COMPONENTS
        }

        self.workspace.initialize(None, jobs=2)
        self.workspace.initialize(None, jobs=4)

        self.assertEqual(
            before,
            {
                name: command(
                    "git",
                    "rev-parse",
                    "HEAD",
                    cwd=self.workspace.paths.repositories / name,
                )
                for name, _ in COMPONENTS
            },
        )

    def test_initialize_serializes_concurrent_invocations(self) -> None:
        other = Workspace(self.wrapper)

        with ThreadPoolExecutor(max_workers=2) as executor:
            futures = [
                executor.submit(workspace.initialize, None, 2)
                for workspace in (self.workspace, other)
            ]
            for future in futures:
                future.result(timeout=10)

        self.assertTrue((self.workspace.paths.repositories / "client" / ".git").exists())

    def test_initialize_workers_inherit_exact_checkout_leases(self) -> None:
        observed: list[tuple[int, ...]] = []
        observed_lock = threading.Lock()

        def run_in_worker(*_arguments: object, **keywords: object) -> object:
            descriptors = keywords["pass_fds"]
            if _arguments and _arguments[0] == ["tool"] and descriptors:
                with observed_lock:
                    observed.append(descriptors)
            return mock.MagicMock(stdout="")

        def ensure(_checkout: object) -> None:
            workspace_run(["tool"])

        with (
            mock.patch.object(self.workspace, "_validate_primary_checkout"),
            mock.patch.object(
                self.workspace, "_ensure_repository", side_effect=ensure
            ),
            mock.patch(
                "atrinik_workspace.workspace.subprocess.run",
                side_effect=run_in_worker,
            ),
        ):
            self.workspace.initialize(["client", "server"], jobs=2)

        workers = [descriptors for descriptors in observed if len(descriptors) > 1]
        self.assertEqual(len(workers), 2)
        self.assertTrue(
            all(len(descriptors) == 11 for descriptors in workers), workers
        )

    def test_sync_distinct_checkouts_overlap(self) -> None:
        barrier = threading.Barrier(2)
        observed: set[str] = set()
        observed_lock = threading.Lock()

        def synchronize(checkouts: list[object], *_arguments: object) -> None:
            with observed_lock:
                observed.add(checkouts[0].name)
            barrier.wait(timeout=5)

        with mock.patch.object(
            self.workspace, "_sync_components", side_effect=synchronize
        ):
            self.workspace.sync(["client", "server"], "none")
        self.assertEqual(observed, {"client", "server"})

    def test_same_checkout_sync_serializes(self) -> None:
        entered = threading.Event()
        release = threading.Event()
        calls = 0
        calls_lock = threading.Lock()

        def synchronize(*_arguments: object) -> None:
            nonlocal calls
            with calls_lock:
                calls += 1
                current = calls
            if current == 1:
                entered.set()
                self.assertTrue(release.wait(5))

        other = Workspace(self.wrapper)
        with (
            mock.patch.object(
                self.workspace, "_sync_components", side_effect=synchronize
            ),
            mock.patch.object(other, "_sync_components", side_effect=synchronize),
            ThreadPoolExecutor(max_workers=2) as executor,
        ):
            first = executor.submit(self.workspace.sync, ["client"], "none")
            self.assertTrue(entered.wait(2))
            second = executor.submit(other.sync, ["client"], "none")
            time.sleep(0.05)
            self.assertEqual(calls, 1)
            release.set()
            first.result(timeout=5)
            second.result(timeout=5)
        self.assertEqual(calls, 2)

    def test_sync_waiting_on_source_does_not_block_disjoint_worktree_create(
        self,
    ) -> None:
        source_a = self.workspace.create_worktree(
            "client", "source-a", "test/source-a", None, False
        )
        coordinate = self.workspace._source_coordinate("client", source_a)
        pending = locking_module.layout_writer_pending_path(
            resource_lock_path(
                self.workspace._lease_namespace, "source", coordinate
            )
        )
        reader = LeaseRequest(
            "source",
            coordinate,
            "shared",
            "build source A",
            "wait for build A",
        )
        sync_workspace = Workspace(self.wrapper)
        sync_entered = threading.Event()

        with ThreadPoolExecutor(max_workers=2) as executor:
            with mock.patch.object(
                sync_workspace,
                "_sync_components",
                side_effect=lambda *_: sync_entered.set(),
            ):
                with resource_locks(
                    self.workspace._lease_root, [reader]
                ):
                    syncing = executor.submit(
                        sync_workspace.sync, ["client"], "merge"
                    )
                    deadline = time.monotonic() + 5
                    while time.monotonic() < deadline:
                        try:
                            with exclusive_lock(
                                pending,
                                "source A writer pending",
                                nonblocking=True,
                            ):
                                pass
                        except WorkspaceError:
                            break
                        time.sleep(0.01)
                    else:
                        self.fail("sync did not queue for source A")

                    created = executor.submit(
                        self.workspace.create_worktree,
                        "client",
                        "source-b",
                        "test/source-b",
                        None,
                        False,
                    ).result(timeout=5)
                    self.assertTrue(created.is_dir())
                    self.assertFalse(sync_entered.is_set())

                syncing.result(timeout=5)
                self.assertTrue(sync_entered.is_set())

    def test_remove_waiting_on_source_does_not_block_disjoint_worktree_create(
        self,
    ) -> None:
        source_a = self.workspace.create_worktree(
            "client", "remove-a", "test/remove-a", None, False
        )
        request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("client", source_a),
            "shared",
            "run source A",
        )
        with ThreadPoolExecutor(max_workers=2) as executor:
            with resource_locks(self.workspace._lease_root, [request]):
                removing = executor.submit(
                    self.workspace.remove_worktree, "client", "remove-a"
                )
                time.sleep(0.05)
                self.assertFalse(removing.done())
                created = executor.submit(
                    self.workspace.create_worktree,
                    "client",
                    "remove-b",
                    "test/remove-b",
                    None,
                    False,
                ).result(timeout=5)
                self.assertTrue(created.is_dir())
            removing.result(timeout=5)
        self.assertFalse(source_a.exists())

    def test_worktree_creation_waits_for_missing_primary_source_reader(self) -> None:
        primary = self.workspace.paths.repositories / "client"
        shutil.rmtree(primary)
        request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("client", primary),
            "shared",
            "inspect missing primary",
        )
        with ThreadPoolExecutor(max_workers=1) as executor:
            with mock.patch.object(
                self.workspace,
                "_component_clone_url",
                return_value=str(self.origins["client"]),
            ):
                with resource_locks(self.workspace._lease_root, [request]):
                    creating = executor.submit(
                        self.workspace.create_worktree,
                        "client",
                        "initialized",
                        "test/initialized",
                        None,
                        False,
                    )
                    time.sleep(0.05)
                    self.assertFalse(creating.done())
                    self.assertFalse(primary.exists())
                created = creating.result(timeout=10)
        self.assertTrue(primary.is_dir())
        self.assertTrue(created.is_dir())

    def test_failed_clone_does_not_strand_destination(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)

        def fail_clone(arguments: list[str], **kwargs: object) -> str:
            if arguments[:2] == ["git", "clone"]:
                temporary = Path(arguments[-1])
                (temporary / "partial").write_text("incomplete\n", encoding="utf-8")
                raise WorkspaceError("clone failed")
            return workspace_run(arguments, **kwargs)

        with mock.patch("atrinik_workspace.workspace.run", side_effect=fail_clone):
            with self.assertRaisesRegex(WorkspaceError, "clone failed"):
                self.workspace._ensure_repository(self.workspace._component("client"))

        self.assertFalse(destination.exists())
        self.assertEqual(
            list(self.workspace.paths.repositories.glob(".atrinik-clone-client-*")), []
        )

    def test_clone_destination_race_never_replaces_raced_in_path(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)

        def race(temporary: Path, target: Path) -> None:
            target.mkdir()
            (target / "sentinel").write_text("preserve\n", encoding="utf-8")
            real_rename_no_replace(temporary, target)

        with mock.patch.object(
            self.workspace,
            "_component_clone_url",
            return_value=str(self.origins["client"]),
        ), mock.patch(
            "atrinik_workspace.workspace.rename_no_replace", side_effect=race
        ):
            with self.assertRaisesRegex(WorkspaceError, "destination appeared"):
                self.workspace._ensure_repository(
                    self.workspace._component("client")
                )

        self.assertEqual(
            (destination / "sentinel").read_text(encoding="utf-8"),
            "preserve\n",
        )
        self.assertEqual(
            list(self.workspace.paths.repositories.glob(".atrinik-clone-client-*")), []
        )

    def test_clone_transport_follows_wrapper_remote(self) -> None:
        command("git", "init", cwd=self.wrapper)
        command(
            "git",
            "remote",
            "add",
            "origin",
            "git@github.com:atrinik/atrinik.git",
            cwd=self.wrapper,
        )

        url = self.workspace._component_clone_url(self.workspace._component("client"))

        self.assertEqual(url, "git@github.com:atrinik/client.git")

    def test_clone_transport_defaults_to_public_https(self) -> None:
        url = self.workspace._component_clone_url(self.workspace._component("client"))

        self.assertEqual(url, "https://github.com/atrinik/client.git")

    def test_initialize_preserves_broken_symlink_at_component_path(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        shutil.rmtree(destination)
        destination.symlink_to(self.root / "missing-checkout", target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._ensure_repository(self.workspace._component("client"))

        self.assertTrue(destination.is_symlink())

    def test_sync_rejects_checkout_symlink_to_external_git_root(self) -> None:
        destination = self.workspace.paths.repositories / "client"
        external = self.root / "external-client"
        destination.rename(external)
        destination.symlink_to(external, target_is_directory=True)
        before = command("git", "rev-parse", "HEAD", cwd=external)

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace.sync(["client"], "none")

        self.assertTrue(destination.is_symlink())
        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=external), before)

    def test_sync_fast_forwards_primary_checkout(self) -> None:
        expected = self.advance_origin("client", "new-file")
        self.workspace.sync(["client"], "none")
        actual = command(
            "git", "rev-parse", "HEAD", cwd=self.workspace.paths.repositories / "client"
        )
        self.assertEqual(actual, expected)

    def test_sync_refuses_dirty_primary_checkout(self) -> None:
        checkout = self.workspace.paths.repositories / "client"
        (checkout / "dirty").write_text("keep\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "dirty primary"):
            self.workspace.sync(["client"], "none")
        self.assertTrue((checkout / "dirty").is_file())

    def test_worktree_sync_excludes_protected_paths_before_dirty_check(self) -> None:
        repository = self.workspace.paths.repositories / "content"
        protected = self.workspace.paths.worktrees / "content" / "classic-maps"
        ordinary = self.workspace.paths.worktrees / "content" / "main-maps"
        records = [
            {"worktree": str(repository), "branch": "refs/heads/main"},
            {"worktree": str(protected), "branch": "refs/heads/classic-maps"},
            {"worktree": str(ordinary), "branch": "refs/heads/main-maps"},
        ]

        with (
            mock.patch(
                "atrinik_workspace.workspace._worktree_records",
                return_value=records,
            ),
            mock.patch(
                "atrinik_workspace.workspace._is_clean",
                side_effect=lambda path: path.resolve() != protected.resolve(),
            ) as clean,
        ):
            candidates, skipped = self.workspace._component_worktrees(
                repository, {protected.resolve()}
            )

        self.assertEqual(candidates, [ordinary.resolve()])
        self.assertEqual(skipped, [protected.resolve()])
        clean.assert_called_once_with(ordinary.resolve())

    def test_sync_preflights_every_checkout_before_updating(self) -> None:
        client = self.workspace.paths.repositories / "client"
        before = command("git", "rev-parse", "HEAD", cwd=client)
        self.advance_origin("client", "new-file")
        server = self.workspace.paths.repositories / "server"
        (server / "dirty").write_text("keep\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "dirty primary"):
            self.workspace.sync(["client", "server"], "none")

        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=client), before)

    def test_sync_uses_canonical_upstream_when_origin_is_absent(self) -> None:
        checkout = self.workspace.paths.repositories / "client"
        command("git", "remote", "remove", "origin", cwd=checkout)
        command(
            "git", "remote", "set-url", "upstream", str(self.origins["client"]), cwd=checkout
        )
        command(
            "git",
            "remote",
            "set-url",
            "--add",
            "upstream",
            "https://github.com/atrinik/client.git",
            cwd=checkout,
        )
        expected = self.advance_origin("client", "upstream-file")

        self.workspace.sync(["client"], "none")

        self.assertEqual(command("git", "rev-parse", "HEAD", cwd=checkout), expected)

    def test_repository_status_reports_dirty_and_cached_divergence(self) -> None:
        client = self.workspace.paths.repositories / "client"
        self.advance_origin("client", "remote-change")
        command("git", "fetch", "origin", cwd=client)
        (client / "untracked").write_text("keep\n", encoding="utf-8")

        rows = {
            row["component"]: row
            for row in self.workspace.repository_status(["client", "server"])
        }

        self.assertTrue(rows["client"]["initialized"])
        self.assertTrue(rows["client"]["dirty"])
        self.assertEqual(rows["client"]["remote"], "origin")
        self.assertEqual(rows["client"]["ahead"], 0)
        self.assertEqual(rows["client"]["behind"], 1)
        self.assertFalse(rows["server"]["dirty"])

    def test_repository_status_reports_uninitialized_component(self) -> None:
        shutil.rmtree(self.workspace.paths.repositories / "client")

        row = self.workspace.repository_status(["client"])[0]

        self.assertFalse(row["initialized"])
        self.assertIsNone(row["head"])
        self.assertIsNone(row["dirty"])

    def test_repository_status_rejects_non_directory_component_path(self) -> None:
        client = self.workspace.paths.repositories / "client"
        shutil.rmtree(client)
        client.write_text("not a checkout\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace.repository_status(["client"])

    def test_worktree_profile_and_safe_removal(self) -> None:
        path = self.workspace.create_worktree(
            "content", "map-review", "feat/map-review", None, False
        )
        self.workspace.create_profile("review")
        self.workspace.set_profile("review", "content", "worktree", "map-review")
        resolved = self.workspace.resolve_profile("review")
        self.assertEqual(resolved["content"], path.resolve())

        (path / "untracked").write_text("keep\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "dirty worktree"):
            self.workspace.remove_worktree("content", "map-review")
        self.assertTrue((path / "untracked").is_file())

    def test_remove_worktree_rejects_symlinked_managed_parent(self) -> None:
        external = self.root / "external-worktrees"
        external.mkdir()
        worktrees = self.workspace.paths.worktrees
        shutil.rmtree(worktrees)
        worktrees.symlink_to(external, target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "cannot open managed worktree"):
            self.workspace.remove_worktree("content", "redirected")

    def test_create_worktree_rejects_symlinked_managed_parent(self) -> None:
        external = self.root / "external-worktrees"
        external.mkdir()
        parent = self.workspace.paths.worktrees / "content"
        parent.symlink_to(external, target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "cannot open managed worktree"):
            self.workspace.create_worktree(
                "content", "redirected", "feat/redirected", None, False
            )

        self.assertEqual(list(external.iterdir()), [])

    def test_create_worktree_rolls_back_after_visibility_race(self) -> None:
        repository = self.workspace.paths.repositories / "content"
        destination = self.workspace.paths.worktrees / "content" / "rolled-back"

        with (
            mock.patch.object(
                self.workspace,
                "_require_visible_worktree_identity",
                side_effect=WorkspaceError("simulated parent replacement"),
            ),
            self.assertRaisesRegex(WorkspaceError, "parent replacement"),
        ):
            self.workspace.create_worktree(
                "content", "rolled-back", "feat/rolled-back", None, False
            )

        records = command(
            "git", "worktree", "list", "--porcelain", cwd=repository
        )
        self.assertNotIn(str(destination), records)
        branch = subprocess.run(
            ["git", "show-ref", "--verify", "--quiet", "refs/heads/feat/rolled-back"],
            cwd=repository,
            check=False,
        )
        self.assertNotEqual(branch.returncode, 0)
        self.assertFalse(destination.exists())

    def test_descriptor_path_uses_dev_fd_off_linux(self) -> None:
        descriptor = os.open(self.workspace.paths.worktrees, os.O_RDONLY)
        try:
            with mock.patch.object(workspace_module.sys, "platform", "darwin"):
                path = workspace_module._descriptor_path(descriptor)
            self.assertEqual(path, Path("/dev/fd") / str(descriptor))
            self.assertTrue(path.samefile(self.workspace.paths.worktrees))
        finally:
            os.close(descriptor)

    def test_open_managed_worktree_pins_target_across_parent_replacement(self) -> None:
        path = self.workspace.create_worktree(
            "content", "pinned", "feat/pinned", None, False
        )
        parent = path.parent
        detached = parent.with_name("detached-content-worktrees")
        external = self.root / "external-worktrees"
        external.mkdir()
        (external / "pinned").mkdir()

        with self.workspace._open_managed_worktree(path) as (stable, physical):
            self.assertEqual(physical, path.resolve())
            parent.rename(detached)
            parent.symlink_to(external, target_is_directory=True)
            try:
                self.assertTrue(stable.samefile(detached / "pinned"))
                self.assertFalse(stable.samefile(external / "pinned"))
            finally:
                parent.unlink()
                detached.rename(parent)

    def test_remove_worktree_refuses_parent_replacement_before_git(self) -> None:
        path = self.workspace.create_worktree(
            "content", "replace-race", "feat/replace-race", None, False
        )
        parent = path.parent
        detached = parent.with_name("detached-content-worktrees")
        external = self.root / "external-worktrees"
        external.mkdir()
        (external / "replace-race").mkdir()

        def replace_parent(_path: Path) -> bool:
            parent.rename(detached)
            parent.symlink_to(external, target_is_directory=True)
            return True

        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace._is_clean",
                    side_effect=replace_parent,
                ),
                self.assertRaisesRegex(WorkspaceError, "path was replaced"),
            ):
                self.workspace.remove_worktree("content", "replace-race")
            self.assertTrue((detached / "replace-race").is_dir())
            self.assertTrue((external / "replace-race").is_dir())
        finally:
            parent.unlink(missing_ok=True)
            detached.rename(parent)

    def test_profile_can_clone_an_existing_selection(self) -> None:
        path = self.workspace.create_worktree(
            "content", "map-review", "feat/map-review", None, False
        )
        self.workspace.create_profile("review")
        self.workspace.set_profile("review", "content", "worktree", "map-review")

        self.workspace.create_profile("review-copy", "review")

        self.assertEqual(
            self.workspace.component_path("content", "review-copy"), path.resolve()
        )
        self.workspace.set_profile("review-copy", "content", "primary")
        self.assertEqual(
            self.workspace.component_path("content", "review"), path.resolve()
        )

    def test_profile_resolution_snapshot_is_old_or_new_never_mixed(self) -> None:
        path = self.workspace.create_worktree(
            "client", "snapshot-review", "feat/snapshot-review", None, False
        )
        self.workspace.create_profile("snapshot")
        updated = threading.Event()

        def update_profile() -> None:
            self.workspace.set_profile(
                "snapshot", "client", "worktree", "snapshot-review"
            )
            updated.set()

        with self.workspace._resolved_profile_operation(
            "snapshot", {"client"}, "snapshot test"
        ) as snapshot:
            updater = threading.Thread(target=update_profile)
            updater.start()
            time.sleep(0.05)
            self.assertFalse(updated.is_set())
            self.assertNotIn(path.resolve(), snapshot.paths().values())
            self.assertEqual(
                self.workspace._load_profile("snapshot", require_file=False),
                snapshot.profile(),
            )
        updater.join(2)
        self.assertFalse(updater.is_alive())
        self.assertTrue(updated.is_set())
        self.assertEqual(
            self.workspace.component_path("client", "snapshot"), path.resolve()
        )

    def test_profile_resolution_waits_before_inspecting_locked_source(self) -> None:
        source = self.workspace.paths.repositories / "client"
        displaced = self.root / "client-displaced"
        request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("client", source),
            "exclusive",
            "replace client source",
        )

        def resolve() -> object:
            with self.workspace._resolved_profile_operation(
                "default", {"client"}, "build default"
            ) as snapshot:
                return snapshot

        with ThreadPoolExecutor(max_workers=1) as executor:
            with resource_locks(self.workspace._lease_root, [request]):
                source.rename(displaced)
                resolution = executor.submit(resolve)
                time.sleep(0.05)
                self.assertFalse(resolution.done())
                displaced.rename(source)
            snapshot = resolution.result(timeout=5)
            self.assertEqual(snapshot.paths()["client"], source.resolve())

    def test_profile_resolution_wait_does_not_retain_earlier_source(self) -> None:
        client = self.workspace.paths.repositories / "client"
        protocol = self.workspace.paths.repositories / "protocol"
        client_writer = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("client", client),
            "exclusive",
            "synchronize client",
        )
        protocol_writer = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("protocol", protocol),
            "exclusive",
            "synchronize protocol",
        )
        entered = threading.Event()
        protocol_attempted = threading.Event()
        real_resource_locks = self.workspace._resource_locks

        def observe_requests(requests, **kwargs):
            if any(
                request.coordinate == protocol_writer.coordinate
                for request in requests
            ) and not kwargs.get("nonblocking", False):
                protocol_attempted.set()
            return real_resource_locks(requests, **kwargs)

        def resolve() -> None:
            with self.workspace._resolved_profile_operation(
                "default", {"client"}, "build client"
            ):
                entered.set()

        with ThreadPoolExecutor(max_workers=1) as executor:
            with real_resource_locks([protocol_writer]):
                with mock.patch.object(
                    self.workspace,
                    "_resource_locks",
                    side_effect=observe_requests,
                ):
                    resolution = executor.submit(resolve)
                    self.assertTrue(protocol_attempted.wait(5))
                    client_lock = resource_lock_path(
                        self.workspace._lease_root(client_writer),
                        client_writer.kind,
                        client_writer.coordinate,
                    )
                    with exclusive_lock(
                        client_lock,
                        "released client source",
                        nonblocking=True,
                    ):
                        self.assertFalse(entered.is_set())
            resolution.result(timeout=5)
        self.assertTrue(entered.is_set())

    def test_clean_primary_build_snapshot_releases_source_and_stays_immutable(self) -> None:
        primary = self.workspace.paths.repositories / "client"
        original = (primary / "README").read_bytes()
        coordinate = self.workspace._source_coordinate("client", primary)
        request = self.workspace._lease_request(
            "source", coordinate, "exclusive", "advance clean client primary"
        )

        with self.workspace._resolved_profile_operation(
            "default",
            {"client"},
            "build client",
            materialize_clean_primaries=True,
        ) as snapshot:
            generated = snapshot.paths()["client"]
            self.assertNotEqual(generated, primary.resolve())
            self.assertEqual((generated / "README").read_bytes(), original)
            self.assertFalse(generated.lstat().st_mode & 0o222)
            with self.workspace._resource_locks([request], nonblocking=True):
                (primary / "README").write_text("advanced\n", encoding="utf-8")
                command("git", "add", "README", cwd=primary)
                command("git", "commit", "-m", "test: advance primary", cwd=primary)
            self.assertEqual((generated / "README").read_bytes(), original)
            self.assertEqual(
                snapshot.checkout_states()["client"]["head"],
                command("git", "rev-parse", "HEAD^", cwd=primary),
            )

    def test_dirty_primary_build_retains_exact_source_lease(self) -> None:
        primary = self.workspace.paths.repositories / "client"
        (primary / "README").write_text("dirty\n", encoding="utf-8")
        coordinate = self.workspace._source_coordinate("client", primary)
        request = self.workspace._lease_request(
            "source", coordinate, "exclusive", "synchronize dirty client"
        )
        entered = threading.Event()
        attempting = threading.Event()

        def acquire_writer() -> None:
            attempting.set()
            with self.workspace._resource_locks([request]):
                entered.set()

        with self.workspace._resolved_profile_operation(
            "default",
            {"client"},
            "build client",
            materialize_clean_primaries=True,
        ) as snapshot:
            self.assertEqual(snapshot.paths()["client"], primary.resolve())
            writer = threading.Thread(target=acquire_writer)
            writer.start()
            self.assertTrue(attempting.wait(5))
            self.assertFalse(entered.is_set())
        writer.join(2)
        self.assertFalse(writer.is_alive())
        self.assertTrue(entered.is_set())

    def test_worktree_build_retains_exact_source_lease(self) -> None:
        path = self.workspace.create_worktree(
            "client", "build-client", "perf/build-client", None, False
        )
        (path / "dirty-build-input").write_text("dirty\n", encoding="utf-8")
        self.workspace.create_profile("worktree-build")
        self.workspace.set_profile(
            "worktree-build", "client", "worktree", "build-client"
        )
        coordinate = self.workspace._source_coordinate("client", path)
        request = self.workspace._lease_request(
            "source", coordinate, "exclusive", "synchronize client worktree"
        )
        entered = threading.Event()
        attempting = threading.Event()

        def acquire_writer() -> None:
            attempting.set()
            with self.workspace._resource_locks([request]):
                entered.set()

        with self.workspace._resolved_profile_operation(
            "worktree-build",
            {"client"},
            "build client",
            materialize_clean_primaries=True,
        ) as snapshot:
            self.assertEqual(snapshot.paths()["client"], path.resolve())
            writer = threading.Thread(target=acquire_writer)
            writer.start()
            self.assertTrue(attempting.wait(5))
            self.assertFalse(entered.is_set())
        writer.join(2)
        self.assertFalse(writer.is_alive())
        self.assertTrue(entered.is_set())

    def test_worktree_build_allows_every_clean_primary_to_synchronize(self) -> None:
        path = self.workspace.create_worktree(
            "client", "sync-client", "perf/sync-client", None, False
        )
        self.workspace.create_profile("sync-build")
        self.workspace.set_profile(
            "sync-build", "client", "worktree", "sync-client"
        )
        prior_heads = {
            name: command(
                "git",
                "rev-parse",
                "HEAD",
                cwd=self.workspace.paths.repositories / name,
            )
            for name, _build in COMPONENTS
        }
        for name, _build in COMPONENTS:
            seed = self.seeds[name]
            (seed / "sync-generation").write_text("advanced\n", encoding="utf-8")
            command("git", "add", "sync-generation", cwd=seed)
            command("git", "commit", "-m", "test: advance primary", cwd=seed)
            command("git", "push", "origin", "main", cwd=seed)

        prepared = threading.Event()
        release = threading.Event()
        observations: dict[str, object] = {}

        def hold_build() -> None:
            with self.workspace._resolved_profile_operation(
                "sync-build",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ) as snapshot:
                selected = snapshot.paths()
                observations["selected"] = selected
                observations["bytes"] = {
                    role: (source / "README").read_bytes()
                    for role, source in selected.items()
                }
                observations["states"] = snapshot.checkout_states()
                prepared.set()
                self.assertTrue(release.wait(10))
                self.assertEqual(
                    {
                        role: (source / "README").read_bytes()
                        for role, source in selected.items()
                    },
                    observations["bytes"],
                )

        with ThreadPoolExecutor(max_workers=1) as executor:
            build = executor.submit(hold_build)
            self.assertTrue(prepared.wait(10))
            try:
                self.workspace.sync([name for name, _build in COMPONENTS], "none")
            finally:
                release.set()
            build.result(timeout=10)
        selected = observations["selected"]
        self.assertEqual(selected["client"], path.resolve())
        for role in ("sound", "libatrinik", "protocol"):
            self.assertNotEqual(
                selected[role], self.workspace.paths.repositories / role
            )
        states = observations["states"]
        for name, _build in COMPONENTS:
            primary = self.workspace.paths.repositories / name
            self.assertNotEqual(
                command("git", "rev-parse", "HEAD", cwd=primary),
                prior_heads[name],
            )
        for checkout in ("sound", "libatrinik", "protocol"):
            self.assertEqual(states[checkout]["head"], prior_heads[checkout])

    def test_clean_primary_source_generation_reuses_and_rejects_corruption(self) -> None:
        def resolve() -> Path:
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ) as snapshot:
                return snapshot.paths()["client"]

        first = resolve()
        self.assertEqual(resolve(), first)
        metadata = first.parent / workspace_module.SOURCE_GENERATION_METADATA
        first.parent.chmod(0o700)
        metadata.chmod(0o600)
        record = load_json(metadata)
        record["source_tree_sha256"] = "0" * 64
        atomic_json(metadata, record)
        first.parent.chmod(0o500)

        with self.assertRaisesRegex(WorkspaceError, "source generation is corrupt"):
            resolve()

        forged = self.root / "forged" / "source"
        forged.mkdir(parents=True)
        shutil.copy2(metadata, forged.parent / metadata.name)
        with self.assertRaisesRegex(WorkspaceError, "ownership is invalid"):
            self.workspace._source_generation_record(forged)

    def test_scoped_classic_sources_include_shared_cmake_inputs(self) -> None:
        for role in ("client", "server"):
            with self.subTest(role=role):
                checkout = self.workspace.paths.repositories / role
                (checkout / role).mkdir()
                if role == "server":
                    (checkout / role / "install_data" / "unique-items").mkdir(
                        parents=True
                    )
                    (checkout / role / "install_data" / "bans").write_text(
                        "", encoding="utf-8"
                    )
                (checkout / "cmake").mkdir()
                (checkout / "cmake" / "AtrinikVersion.cmake").write_text(
                    "function(atrinik_resolve_version output)\n"
                    "  set(${output} test-version PARENT_SCOPE)\n"
                    "endfunction()\n",
                    encoding="utf-8",
                )
                (checkout / "LICENSE.md").write_text(
                    "test license\n", encoding="utf-8"
                )
                (checkout / "ATTRIBUTIONS.md").write_text(
                    "test attributions\n", encoding="utf-8"
                )
                (checkout / role / "CMakeLists.txt").write_text(
                    "cmake_minimum_required(VERSION 3.16)\n"
                    "include(../cmake/AtrinikVersion.cmake)\n"
                    "atrinik_resolve_version(ATRINIK_VERSION)\n"
                    f"project(scoped-{role} VERSION 1.0 LANGUAGES NONE)\n"
                    "if(NOT ATRINIK_VERSION STREQUAL test-version)\n"
                    '  message(FATAL_ERROR "shared version module was not used")\n'
                    "endif()\n"
                    "foreach(document LICENSE.md ATTRIBUTIONS.md)\n"
                    "  if(NOT EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/../${document}\")\n"
                    '    message(FATAL_ERROR "missing root document: ${document}")\n'
                    "  endif()\n"
                    "endforeach()\n"
                    "enable_testing()\n"
                    "add_test(NAME shared-version COMMAND "
                    "${CMAKE_COMMAND} -E true)\n",
                    encoding="utf-8",
                )
                command(
                    "git",
                    "add",
                    role,
                    "cmake",
                    "LICENSE.md",
                    "ATTRIBUTIONS.md",
                    cwd=checkout,
                )
                command(
                    "git",
                    "commit",
                    "-m",
                    f"test: seed scoped {role}",
                    cwd=checkout,
                )

                original = self.workspace.manifest.by_name[role]
                component = replace(
                    original,
                    source=role,
                    source_includes=(
                        "cmake",
                        "LICENSE.md",
                        "ATTRIBUTIONS.md",
                    ),
                )
                self.workspace.manifest.by_name[role] = component
                self.workspace.manifest.components = [
                    component if item.name == role else item
                    for item in self.workspace.manifest.components
                ]
                stack = self.workspace.manifest.stacks["default"]
                stack.providers[role] = component
                object.__setattr__(
                    stack,
                    "components",
                    tuple(
                        component if item.name == role else item
                        for item in stack.components
                    ),
                )
                profile = self.workspace._load_profile("default", require_file=False)
                selected = {role: checkout / role}
                state = self.workspace._selected_checkout_states(
                    profile,
                    selected,
                    include_dirty=True,
                    include_identity=True,
                )[role]
                generated = self.workspace._materialize_primary_source(
                    component,
                    checkout,
                    checkout / role,
                    state,
                )

                self.assertTrue(
                    (generated.parent / "cmake" / "AtrinikVersion.cmake").is_file()
                )
                self.assertTrue((generated.parent / "LICENSE.md").is_file())
                self.assertTrue((generated.parent / "ATTRIBUTIONS.md").is_file())
                build_root = self.workspace.paths.builds / f"scoped-{role}"
                view = self.workspace._profile_source_view(
                    build_root,
                    role,
                    generated,
                    set(),
                    {"install_data"} if role == "server" else set(),
                    preserved_entries={
                        workspace_module.SOURCE_INCLUDE_VIEW_METADATA
                    },
                )
                if role == "server":
                    self.assertTrue(
                        (view / "install_data").stat().st_mode & stat.S_IWUSR
                    )
                    self.assertTrue(
                        (view / "install_data" / "bans").stat().st_mode
                        & stat.S_IWUSR
                    )
                self.workspace._prepare_component_source_includes(
                    build_root, component, generated, view
                )
                self.assertTrue(
                    (build_root / "sources" / "cmake" / "AtrinikVersion.cmake").is_file()
                )
                self.assertTrue(
                    (build_root / "sources" / "LICENSE.md").is_file()
                )
                self.assertTrue(
                    (build_root / "sources" / "ATTRIBUTIONS.md").is_file()
                )
                view = self.workspace._profile_source_view(
                    build_root,
                    role,
                    generated,
                    set(),
                    {"install_data"} if role == "server" else set(),
                    preserved_entries={
                        workspace_module.SOURCE_INCLUDE_VIEW_METADATA
                    },
                )
                self.workspace._prepare_component_source_includes(
                    build_root, component, generated, view
                )
                view_key = str(view.resolve())
                self.assertTrue(self.workspace._source_view_unchanged[view_key])
                (build_root / "sources" / "cmake" / "AtrinikVersion.cmake").unlink()
                view = self.workspace._profile_source_view(
                    build_root,
                    role,
                    generated,
                    set(),
                    {"install_data"} if role == "server" else set(),
                    preserved_entries={
                        workspace_module.SOURCE_INCLUDE_VIEW_METADATA
                    },
                )
                self.workspace._prepare_component_source_includes(
                    build_root, component, generated, view
                )
                self.assertFalse(self.workspace._source_view_unchanged[view_key])
                self.workspace._cmake(
                    view,
                    build_root / "build" / role,
                    [],
                    True,
                )
                self.assertEqual(
                    self.workspace._materialize_primary_source(
                        component,
                        checkout,
                        checkout / role,
                        state,
                    ),
                    generated,
                )

                generation_mode = stat.S_IMODE(generated.parent.stat().st_mode)
                include_mode = stat.S_IMODE((generated.parent / "cmake").stat().st_mode)
                module = generated.parent / "cmake" / "AtrinikVersion.cmake"
                module_mode = stat.S_IMODE(module.stat().st_mode)
                generated.parent.chmod(0o700)
                (generated.parent / "cmake").chmod(0o700)
                module.chmod(0o600)
                module.write_text("corrupt\n", encoding="utf-8")
                module.chmod(module_mode)
                (generated.parent / "cmake").chmod(include_mode)
                generated.parent.chmod(generation_mode)
                with self.assertRaisesRegex(
                    WorkspaceError, "source generation is corrupt"
                ):
                    self.workspace._materialize_primary_source(
                        component,
                        checkout,
                        checkout / role,
                        state,
                    )

    def test_source_generation_archive_extraction_rejects_unsafe_entries(self) -> None:
        def archive(name: str, entries: list[tuple[tarfile.TarInfo, bytes]]) -> Path:
            path = self.root / f"{name}.tar"
            with tarfile.open(path, "w") as output:
                for member, payload in entries:
                    member.size = len(payload)
                    output.addfile(member, io.BytesIO(payload) if payload else None)
            return path

        directory = tarfile.TarInfo("dir")
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o755
        duplicate_directory = tarfile.TarInfo("dir")
        duplicate_directory.type = tarfile.DIRTYPE
        duplicate_directory.mode = 0o755
        regular = tarfile.TarInfo("dir/file")
        regular.mode = 0o644
        link = tarfile.TarInfo("link")
        link.type = tarfile.SYMTYPE
        link.linkname = "dir/file"
        valid = self.root / "valid-archive"
        self.workspace._extract_git_source_archive(
            archive(
                "valid",
                [
                    (directory, b""),
                    (duplicate_directory, b""),
                    (regular, b"payload"),
                    (link, b""),
                ],
            ),
            valid,
        )
        self.assertEqual((valid / "dir" / "file").read_bytes(), b"payload")
        self.assertEqual((valid / "link").readlink(), Path("dir/file"))

        cases: list[tuple[str, list[tuple[tarfile.TarInfo, bytes]], str]] = []
        for name in ("/absolute", "../escape"):
            cases.append(
                (
                    name.strip("/.") or "unsafe",
                    [(tarfile.TarInfo(name), b"x")],
                    "unsafe path",
                )
            )
        duplicate_a = tarfile.TarInfo("same")
        duplicate_b = tarfile.TarInfo("same")
        cases.append(
            (
                "duplicate",
                [(duplicate_a, b"a"), (duplicate_b, b"b")],
                "repeats a path",
            )
        )
        absolute_link = tarfile.TarInfo("absolute-link")
        absolute_link.type = tarfile.SYMTYPE
        absolute_link.linkname = "/outside"
        cases.append(("absolute-link", [(absolute_link, b"")], "unsafe link"))
        escaping_link = tarfile.TarInfo("escaping-link")
        escaping_link.type = tarfile.SYMTYPE
        escaping_link.linkname = "../outside"
        cases.append(
            (
                "escaping-link",
                [(escaping_link, b"")],
                "escapes its generation",
            )
        )
        hard_link = tarfile.TarInfo("hard-link")
        hard_link.type = tarfile.LNKTYPE
        hard_link.linkname = "target"
        cases.append(("hard-link", [(hard_link, b"")], "unsupported entry"))
        linked_directory = tarfile.TarInfo("linked-dir")
        linked_directory.type = tarfile.SYMTYPE
        linked_directory.linkname = "dir"
        linked_target = tarfile.TarInfo("dir/")
        linked_target.type = tarfile.DIRTYPE
        linked_target.mode = 0o755
        traversed_file = tarfile.TarInfo("linked-dir/file")
        cases.append(
            (
                "linked-parent",
                [
                    (linked_target, b""),
                    (linked_directory, b""),
                    (traversed_file, b"payload"),
                ],
                "traverses a symbolic link",
            )
        )
        for index, (name, entries, message) in enumerate(cases):
            with self.subTest(name=name), self.assertRaisesRegex(
                WorkspaceError, message
            ):
                self.workspace._extract_git_source_archive(
                    archive(f"invalid-{index}", entries),
                    self.root / f"invalid-output-{index}",
                )

    def test_source_generation_archive_command_failures_are_bounded(self) -> None:
        profile = self.workspace._load_profile("default", require_file=False)
        selected = self.workspace._resolve_build_profile(
            "default", {"client"}, trace=False, profile=profile
        )
        states = self.workspace._selected_checkout_states(
            profile, selected, include_dirty=True, include_identity=True
        )
        component = self.workspace.manifest.stack(profile["stack"]).providers[
            "client"
        ]
        checkout = self.workspace.paths.repositories / "client"
        failures = (
            (FileNotFoundError("git"), "required command not found"),
            (
                subprocess.CalledProcessError(1, ["git"], stderr=b"archive failed"),
                "cannot export immutable source generation: archive failed",
            ),
        )
        real_subprocess_run = subprocess.run
        for failure, message in failures:
            def fail_archive(arguments, *args, injected=failure, **kwargs):
                if "archive" in arguments:
                    raise injected
                return real_subprocess_run(arguments, *args, **kwargs)

            with (
                self.subTest(message=message),
                mock.patch(
                    "atrinik_workspace.workspace.subprocess.run",
                    side_effect=fail_archive,
                ),
                self.assertRaisesRegex(WorkspaceError, message),
            ):
                self.workspace._materialize_primary_source(
                    component,
                    checkout,
                    selected["client"],
                    states["client"],
                )

    def test_source_generation_rejects_external_hard_link(self) -> None:
        def resolve() -> Path:
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ) as snapshot:
                return snapshot.paths()["client"]

        source = resolve()
        target = source / "README"
        external = self.root / "external-hard-link"
        external.write_bytes(target.read_bytes())
        external.chmod(stat.S_IMODE(target.stat().st_mode))
        generation_mode = stat.S_IMODE(source.parent.stat().st_mode)
        source_mode = stat.S_IMODE(source.stat().st_mode)
        source.parent.chmod(0o700)
        source.chmod(0o700)
        target.unlink()
        os.link(external, target)
        source.chmod(source_mode)
        source.parent.chmod(generation_mode)

        with self.assertRaisesRegex(WorkspaceError, "hard-linked file"):
            resolve()

        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(
            row
            for row in report["items"]
            if row["kind"] == "source-generation"
        )
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("invalid_source_generation", item["reasons"])

    def test_source_generation_publication_failure_leaves_no_partial_generation(self) -> None:
        with (
            mock.patch(
                "atrinik_workspace.workspace.rename_no_replace",
                side_effect=WorkspaceError("injected publication failure"),
            ),
            self.assertRaisesRegex(WorkspaceError, "injected publication failure"),
        ):
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ):
                self.fail("failed source generation was yielded")

        container = self.workspace.paths.builds / "source-generations" / "client"
        self.assertEqual(
            [
                path.name
                for path in container.iterdir()
                if path.name != MANAGED_MARKER
            ],
            [],
        )

    def test_source_generation_is_sealed_before_atomic_publication(self) -> None:
        def publish_then_interrupt(source: Path, destination: Path) -> None:
            real_rename_no_replace(source, destination)
            raise WorkspaceError("injected post-publication interruption")

        with (
            mock.patch(
                "atrinik_workspace.workspace.rename_no_replace",
                side_effect=publish_then_interrupt,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "post-publication interruption"
            ),
        ):
            with self.workspace._resolved_profile_operation(
                "default",
                {"resources"},
                "build resources",
                materialize_clean_primaries=True,
            ):
                self.fail("interrupted publication was yielded")

        with self.workspace._resolved_profile_operation(
            "default",
            {"resources"},
            "build resources",
            materialize_clean_primaries=True,
        ) as snapshot:
            generation = snapshot.paths()["resources"].parent
            self.assertFalse(stat.S_IMODE(generation.stat().st_mode) & 0o222)

    def test_source_generation_rejects_checkout_change_during_staging(self) -> None:
        primary = self.workspace.paths.repositories / "client"
        extract = self.workspace._extract_git_source_archive

        def advance(archive: Path, output: Path) -> None:
            extract(archive, output)
            (primary / "advanced-during-export").write_text(
                "changed\n", encoding="utf-8"
            )
            command("git", "add", "advanced-during-export", cwd=primary)
            command("git", "commit", "-m", "test: race generation", cwd=primary)

        with (
            mock.patch.object(
                self.workspace,
                "_extract_git_source_archive",
                side_effect=advance,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "changed during materialization"
            ),
        ):
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ):
                self.fail("changed source generation was yielded")

    def test_source_generation_cleanup_cannot_cross_lease_handoff(self) -> None:
        real_shared_lock = shared_lock
        cleanup_ran = False

        def cleanup_before_pin(path: Path, description: str):
            nonlocal cleanup_ran
            if "source-generation-" in path.name and not cleanup_ran:
                cleanup_ran = True
                with mock.patch(
                    "atrinik_workspace.cleanup.Cleanup._registered_worktree_paths",
                    return_value=(set(), False),
                ):
                    self.workspace.cleanup(["builds"], 0, [], True)
            return real_shared_lock(path, description)

        with (
            mock.patch(
                "atrinik_workspace.workspace.shared_lock",
                side_effect=cleanup_before_pin,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "changed before lease handoff"
            ),
        ):
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ):
                self.fail("removed source generation was yielded")
        self.assertTrue(cleanup_ran)

    def test_source_generation_cleanup_before_record_collection_fails_closed(
        self,
    ) -> None:
        materialize = self.workspace._materialize_primary_source
        cleanup_ran = False

        def cleanup_after_materialize(*args, **kwargs) -> Path:
            nonlocal cleanup_ran
            generated = materialize(*args, **kwargs)
            if not cleanup_ran:
                cleanup_ran = True
                with mock.patch(
                    "atrinik_workspace.cleanup.Cleanup._registered_worktree_paths",
                    return_value=(set(), False),
                ):
                    self.workspace.cleanup(["builds"], 0, [], True)
            return generated

        with (
            mock.patch.object(
                self.workspace,
                "_materialize_primary_source",
                side_effect=cleanup_after_materialize,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "changed before lease handoff"
            ),
        ):
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ):
                self.fail("removed source generation was yielded")
        self.assertTrue(cleanup_ran)

    def test_source_generation_container_creation_is_serialized(self) -> None:
        real_managed_directory = managed_directory
        active = 0
        maximum = 0
        probes = 0
        guard = threading.Lock()
        real_exclusive_lock = exclusive_lock

        def observe(path: Path, builds: Path, purpose: str) -> None:
            nonlocal active, maximum, probes
            if purpose == "source-generations:client":
                container_lock = (
                    self.workspace.paths.builds
                    / "locks"
                    / "source-generation-container-client.lock"
                )
                with self.assertRaises(locking_module.LockBusyError):
                    with real_exclusive_lock(
                        container_lock,
                        "probe source generation container",
                        nonblocking=True,
                    ):
                        self.fail("container initialization was not locked")
                with guard:
                    probes += 1
                    active += 1
                    maximum = max(maximum, active)
                try:
                    real_managed_directory(path, builds, purpose)
                finally:
                    with guard:
                        active -= 1
                return
            real_managed_directory(path, builds, purpose)

        def resolve() -> Path:
            with self.workspace._resolved_profile_operation(
                "default",
                {"client"},
                "build client",
                materialize_clean_primaries=True,
            ) as snapshot:
                return snapshot.paths()["client"]

        with mock.patch(
            "atrinik_workspace.workspace.managed_directory",
            side_effect=observe,
        ):
            with ThreadPoolExecutor(max_workers=2) as executor:
                paths = list(executor.map(lambda _unused: resolve(), range(2)))
        self.assertEqual(paths[0], paths[1])
        self.assertEqual(probes, 2)
        self.assertEqual(maximum, 1)

    def test_source_generation_reuse_revalidates_captured_identity(self) -> None:
        profile = self.workspace._load_profile("default", require_file=False)
        selected = self.workspace._resolve_build_profile(
            "default", {"client"}, trace=False, profile=profile
        )
        states = self.workspace._selected_checkout_states(
            profile, selected, include_dirty=True, include_identity=True
        )
        component = self.workspace.manifest.stack(profile["stack"]).providers[
            "client"
        ]
        checkout = self.workspace.paths.repositories / "client"
        generated = self.workspace._materialize_primary_source(
            component, checkout, selected["client"], states["client"]
        )
        self.assertTrue(generated.is_dir())
        (checkout / "reuse-race").write_text("advanced\n", encoding="utf-8")
        command("git", "add", "reuse-race", cwd=checkout)
        command("git", "commit", "-m", "test: advance reuse source", cwd=checkout)
        with self.assertRaisesRegex(WorkspaceError, "changed before generation reuse"):
            self.workspace._materialize_primary_source(
                component, checkout, selected["client"], states["client"]
            )

    def test_source_generation_interruption_residue_is_reclaimable(self) -> None:
        container = self.workspace.paths.builds / "source-generations" / "client"
        managed_directory(
            container,
            self.workspace.paths.builds,
            "source-generations:client",
        )
        key = "a" * 64
        residue = container / f"{key}-staging-interrupted"
        residue.mkdir()
        (residue / "source.tar").write_bytes(b"partial archive")

        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(
            row
            for row in report["items"]
            if row["kind"] == "source-generation-transaction"
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(
            item["reasons"], ["stale_source_generation_transaction"]
        )
        with mock.patch(
            "atrinik_workspace.cleanup.Cleanup._registered_worktree_paths",
            return_value=(set(), False),
        ):
            applied = self.workspace.cleanup(["builds"], 0, [], True)
        removed = next(
            row
            for row in applied["items"]
            if row["kind"] == "source-generation-transaction"
        )
        self.assertEqual(removed["disposition"], "removed")
        self.assertFalse(residue.exists())

    def test_source_generation_transaction_uncertainty_is_protected(self) -> None:
        container = self.workspace.paths.builds / "source-generations" / "client"
        managed_directory(
            container,
            self.workspace.paths.builds,
            "source-generations:client",
        )
        cleaner = Cleanup(self.workspace)
        key = "b" * 64
        missing = container / f"{key}-staging-missing"
        missing_item = cleaner._source_generation_transaction_item(
            missing, "client", 7
        )
        self.assertIn("filesystem_traversal_error", missing_item["reasons"])
        self.assertIn("build_age_unavailable", missing_item["reasons"])

        wrong_marker = container / f"{key}-staging-marker"
        wrong_marker.mkdir()
        atomic_json(wrong_marker / MANAGED_MARKER, {"purpose": "wrong"})
        old_timestamp = cleaner.now.timestamp() - 8 * 86400
        os.utime(wrong_marker / MANAGED_MARKER, (old_timestamp, old_timestamp))
        os.utime(wrong_marker, (old_timestamp, old_timestamp))
        marker_item = cleaner._source_generation_transaction_item(
            wrong_marker, "client", 7, check_lock=False
        )
        self.assertIn(
            "invalid_source_generation_transaction", marker_item["reasons"]
        )

        busy = container / f"{key}-staging-busy"
        busy.mkdir()
        os.utime(busy, (old_timestamp, old_timestamp))
        with mock.patch.object(cleaner, "_lock_busy", return_value=(True, None)):
            busy_item = cleaner._source_generation_transaction_item(
                busy, "client", 7
            )
        self.assertIn("build_lock_busy", busy_item["reasons"])

        lock_error = container / f"{key}-staging-lock_error"
        lock_error.mkdir()
        os.utime(lock_error, (old_timestamp, old_timestamp))
        with mock.patch.object(
            cleaner, "_lock_busy", return_value=(False, "cannot inspect lock")
        ):
            error_item = cleaner._source_generation_transaction_item(
                lock_error, "client", 7
            )
        self.assertIn("build_lock_error", error_item["reasons"])

        young = container / f"{key}-staging-young"
        young.mkdir()
        young_cleaner = Cleanup(self.workspace)
        young_item = young_cleaner._source_generation_transaction_item(
            young, "client", 7, check_lock=False
        )
        self.assertIn("younger_than_grace_period", young_item["reasons"])

        future = container / f"{key}-staging-future"
        future.mkdir()
        future_file = future / "partial"
        future_file.write_text("partial\n", encoding="utf-8")
        future_timestamp = cleaner.now.timestamp() + 86400
        os.utime(future_file, (future_timestamp, future_timestamp))
        future_item = cleaner._source_generation_transaction_item(
            future, "client", 7, check_lock=False
        )
        self.assertIn("future_tree_mtime", future_item["reasons"])

        external = self.root / "transaction-external"
        external.mkdir()
        linked = container / f"{key}-staging-linked"
        linked.symlink_to(external, target_is_directory=True)
        linked_item = cleaner._source_generation_transaction_item(
            linked, "client", 7, check_lock=False
        )
        self.assertIn(
            "invalid_source_generation_transaction", linked_item["reasons"]
        )

    def test_source_generation_metadata_and_cleanup_follow_generation_lease(
        self,
    ) -> None:
        with self.workspace._resolved_profile_operation(
            "default",
            {"resources"},
            "build resources",
            materialize_clean_primaries=True,
        ) as snapshot:
            selected = snapshot.paths()
            key = "a" * 12
            root = self.workspace.paths.builds / "profiles" / f"default-{key}"
            managed_directory(root, self.workspace.paths.builds, f"profile:default:{key}")
            self.workspace._refresh_build_metadata(
                root, "default", key, selected
            )
            coordinate = load_json(root / workspace_module.BUILD_METADATA)[
                "coordinates"
            ]["resources"]
            generation = coordinate["source_generation"]
            self.assertEqual(coordinate["source_path"], generation["path"])
            self.assertEqual(generation["commit"], coordinate["head"])
            report = self.workspace.cleanup(["builds"], 0, [], False)
            active = next(
                item
                for item in report["items"]
                if item["kind"] == "source-generation"
            )
            self.assertEqual(active["disposition"], "protected")
            self.assertIn("build_lock_busy", active["reasons"])

        report = self.workspace.cleanup(["builds"], 0, [], False)
        stale = next(
            item
            for item in report["items"]
            if item["kind"] == "source-generation"
        )
        self.assertEqual(stale["disposition"], "eligible")
        self.assertEqual(stale["reasons"], ["stale_source_generation"])
        generation_path = Path(stale["path"])
        with mock.patch(
            "atrinik_workspace.cleanup.Cleanup._registered_worktree_paths",
            return_value=(set(), False),
        ):
            applied = self.workspace.cleanup(["builds"], 0, [], True)
        removed = next(
            item
            for item in applied["items"]
            if item["kind"] == "source-generation"
        )
        self.assertEqual(removed["disposition"], "removed")
        self.assertFalse(generation_path.exists())

    def test_source_generation_cleanup_recognizes_schema_one_metadata(self) -> None:
        with self.workspace._resolved_profile_operation(
            "default",
            {"resources"},
            "build resources",
            materialize_clean_primaries=True,
        ) as snapshot:
            generation = snapshot.paths()["resources"].parent
        metadata_path = generation / workspace_module.SOURCE_GENERATION_METADATA
        metadata = load_json(metadata_path)
        metadata.pop("source_includes")
        metadata.pop("closure_tree_sha256")
        metadata["schema_version"] = 1
        identity = {
            key: value
            for key, value in metadata.items()
            if key != "source_tree_sha256"
        }
        legacy_key = hashlib.sha256(
            json.dumps(identity, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        legacy_generation = generation.parent / legacy_key
        generation.chmod(0o700)
        metadata_path.chmod(0o600)
        atomic_json(metadata_path, metadata)
        marker = generation / MANAGED_MARKER
        marker.chmod(0o600)
        atomic_json(
            marker,
            {
                "schema_version": 1,
                "purpose": f"source-generation:{legacy_key}",
            },
        )
        generation.rename(legacy_generation)
        legacy_generation.chmod(0o500)

        report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(
            row
            for row in report["items"]
            if row["path"] == str(legacy_generation)
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertEqual(item["reasons"], ["stale_source_generation"])

    def test_source_generation_cleanup_does_not_follow_metadata_symlink(
        self,
    ) -> None:
        with self.workspace._resolved_profile_operation(
            "default",
            {"resources"},
            "build resources",
            materialize_clean_primaries=True,
        ) as snapshot:
            generation = snapshot.paths()["resources"].parent
        external = self.root / "external-generation.json"
        external.write_text('{"outside": true}\n', encoding="utf-8")
        metadata = generation / workspace_module.SOURCE_GENERATION_METADATA
        generation.chmod(0o700)
        metadata.unlink()
        metadata.symlink_to(external)
        generation.chmod(0o500)
        real_load_json = load_json

        def reject_external_read(path: Path):
            if path == metadata or path == external:
                raise AssertionError("cleanup followed external metadata")
            return real_load_json(path)

        with mock.patch(
            "atrinik_workspace.cleanup.load_json",
            side_effect=reject_external_read,
        ):
            report = self.workspace.cleanup(["builds"], 0, [], False)
        item = next(
            row
            for row in report["items"]
            if row["kind"] == "source-generation"
        )
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("invalid_source_generation", item["reasons"])

    def test_clean_referenced_worktree_cannot_be_removed(self) -> None:
        path = self.workspace.create_worktree(
            "content", "referenced", "feat/referenced", None, False
        )
        self.workspace.create_profile("referenced")
        self.workspace.set_profile(
            "referenced", "content", "worktree", "referenced"
        )
        with self.assertRaisesRegex(WorkspaceError, "profile:referenced"):
            self.workspace.remove_worktree("content", "referenced")
        self.assertTrue(path.is_dir())

    def test_profile_publication_cannot_enter_target_removal_window(self) -> None:
        path = self.workspace.create_worktree(
            "content", "publication-race", "feat/publication-race", None, False
        )
        self.workspace.create_profile("publication-race")
        replacement = path.with_name("publication-race.removed")
        published = threading.Event()
        errors: list[BaseException] = []

        def publish_reference() -> None:
            try:
                self.workspace.set_profile(
                    "publication-race",
                    "content",
                    "worktree",
                    "publication-race",
                )
            except BaseException as error:
                errors.append(error)
            finally:
                published.set()

        request = LeaseRequest(
            "source",
            self.workspace._source_coordinate("content", path),
            "exclusive",
            "cleanup publication-race candidate",
            "wait for candidate cleanup to finish and retry",
        )
        try:
            with resource_locks(self.workspace._lease_root, [request]):
                publisher = threading.Thread(target=publish_reference)
                publisher.start()
                time.sleep(0.05)
                self.assertFalse(published.is_set())
                path.rename(replacement)
            publisher.join(2)
            self.assertFalse(publisher.is_alive())
            self.assertTrue(published.is_set())
            self.assertEqual(len(errors), 1)
            self.assertIsInstance(errors[0], WorkspaceError)
            self.assertEqual(
                self.workspace._load_profile_file(
                    "publication-race", require_file=True
                )["components"]["content"],
                {"kind": "primary", "value": ""},
            )
        finally:
            if replacement.exists() and not path.exists():
                replacement.rename(path)

    def test_path_profile_alias_uses_canonical_source_lease(self) -> None:
        path = self.workspace.create_worktree(
            "content", "path-alias", "feat/path-alias", None, False
        )
        self.workspace.create_profile("path-alias")
        alias_parent = self.root / "worktree-alias"
        alias_parent.symlink_to(path.parent, target_is_directory=True)
        alias = alias_parent / path.name
        request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("content", path),
            "exclusive",
            "remove canonical source",
        )
        with ThreadPoolExecutor(max_workers=1) as executor:
            with resource_locks(self.workspace._lease_root, [request]):
                publishing = executor.submit(
                    self.workspace.set_profile,
                    "path-alias",
                    "content",
                    "path",
                    str(alias),
                )
                time.sleep(0.05)
                self.assertFalse(publishing.done())
            publishing.result(timeout=5)
        self.assertEqual(
            self.workspace._load_profile_file("path-alias", True)["components"][
                "content"
            ]["value"],
            str(path.resolve()),
        )

    def test_loaded_path_profile_alias_uses_canonical_source_lease(self) -> None:
        path = self.workspace.create_worktree(
            "content", "loaded-alias", "feat/loaded-alias", None, False
        )
        alias_parent = self.root / "loaded-worktree-alias"
        alias_parent.symlink_to(path.parent, target_is_directory=True)
        profile = self.workspace._load_profile_file("default", False)
        profile["name"] = "loaded-alias"
        profile["components"]["content"] = {
            "kind": "path",
            "value": str(alias_parent / path.name),
        }
        atomic_json(self.workspace.paths.profiles / "loaded-alias.json", profile)
        request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("content", path),
            "exclusive",
            "remove canonical source",
        )
        with ThreadPoolExecutor(max_workers=1) as executor:
            with resource_locks(self.workspace._lease_root, [request]):
                resolving = executor.submit(
                    lambda: self._resolved_path("loaded-alias", "content")
                )
                time.sleep(0.05)
                self.assertFalse(resolving.done())
            self.assertEqual(resolving.result(timeout=5), path.resolve())

    def test_resolved_profile_uses_confirmed_profile_without_rereading(self) -> None:
        load_profile = self.workspace._load_profile_file
        calls = 0

        def load_once_confirmed(name: str, require_file: bool) -> dict[str, object]:
            nonlocal calls
            calls += 1
            if calls > 2:
                raise AssertionError("profile reread after exact source leases")
            return load_profile(name, require_file)

        with mock.patch.object(
            self.workspace, "_load_profile_file", side_effect=load_once_confirmed
        ):
            self.assertEqual(
                self._resolved_path("default", "content"),
                (self.workspace.paths.repositories / "content").resolve(),
            )
        self.assertEqual(calls, 2)

    def test_create_profile_copies_confirmed_profile_without_rereading(self) -> None:
        load_profile = self.workspace._load_profile_file
        calls = 0

        def load_once_confirmed(name: str, require_file: bool) -> dict[str, object]:
            nonlocal calls
            calls += 1
            if calls > 2:
                raise AssertionError("source profile reread after exact source leases")
            return load_profile(name, require_file)

        with mock.patch.object(
            self.workspace, "_load_profile_file", side_effect=load_once_confirmed
        ):
            self.workspace.create_profile("confirmed-copy")
        self.assertEqual(calls, 2)
        self.assertEqual(
            self.workspace._load_profile_file("confirmed-copy", True)["stack"],
            "default",
        )

    def _resolved_path(self, profile: str, role: str) -> Path:
        with self.workspace._resolved_profile_operation(
            profile, {role}, f"resolve {profile}"
        ) as snapshot:
            return snapshot.paths()[role]

    def test_component_path_only_requires_selected_component(self) -> None:
        for name, _ in COMPONENTS:
            if name != "content":
                shutil.rmtree(self.workspace.paths.repositories / name)

        self.assertEqual(
            self.workspace.component_path("content", "default"),
            (self.workspace.paths.repositories / "content").resolve(),
        )

    def test_profile_rejects_checkout_for_another_component(self) -> None:
        self.workspace.create_profile("review")
        with self.assertRaisesRegex(WorkspaceError, "no origin/upstream"):
            self.workspace.set_profile(
                "review",
                "content",
                "path",
                str(self.workspace.paths.repositories / "client"),
            )

    def test_profile_rejects_nested_checkout_path(self) -> None:
        checkout = self.workspace.paths.repositories / "content"
        nested = checkout / "nested"
        nested.mkdir()
        self.workspace.create_profile("review")
        with self.assertRaisesRegex(WorkspaceError, "worktree root"):
            self.workspace.set_profile("review", "content", "path", str(nested))

    def test_profile_rejects_symlinked_checkout_path(self) -> None:
        checkout = self.workspace.paths.repositories / "content"
        link = self.root / "content-link"
        link.symlink_to(checkout, target_is_directory=True)
        self.workspace.create_profile("review")

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace.set_profile("review", "content", "path", str(link))

    def test_component_build_resolves_only_its_dependencies(self) -> None:
        for name, _ in COMPONENTS:
            if name != "content":
                shutil.rmtree(self.workspace.paths.repositories / name)
        expected = self.workspace.paths.builds / "result"
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=expected
        ) as build_resolved:
            actual = self.workspace.build("content", "default", tests=False)

        self.assertEqual(actual, expected)
        selected = build_resolved.call_args.args[4]
        self.assertEqual(set(selected), {"content"})

    def test_integrated_classic_build_requires_one_complete_monorepo(self) -> None:
        checkout = self.root / "classic"
        checkout.mkdir()
        (checkout / "CMakeLists.txt").write_text("project(classic)\n", encoding="utf-8")
        selected = {}
        for role in ("client", "server", "protocol", "libatrinik"):
            selected[role] = checkout / role
            selected[role].mkdir()

        self.assertTrue(
            self.workspace._uses_integrated_classic_build(
                ["client", "server"], selected
            )
        )
        self.assertFalse(
            self.workspace._uses_integrated_classic_build(
                ["protocol", "libatrinik", "client"], selected
            )
        )
        selected["server"] = self.root / "other" / "server"
        self.assertFalse(
            self.workspace._uses_integrated_classic_build(
                ["protocol", "libatrinik", "client", "server"], selected
            )
        )

    def test_integrated_classic_build_creates_one_nested_source_graph(self) -> None:
        checkout = self.root / "classic"
        for role in ("client", "server", "protocol", "libatrinik"):
            (checkout / role).mkdir(parents=True)
            (checkout / role / "README").write_text(role + "\n", encoding="utf-8")
        (checkout / "CMakeLists.txt").write_text(
            "project(classic)\n", encoding="utf-8"
        )
        (checkout / "server" / "install_data").mkdir()
        sound = self.root / "sound"
        sound.mkdir()
        selected = {
            role: checkout / role
            for role in ("client", "server", "protocol", "libatrinik")
        }
        selected["sound"] = sound
        root = self.workspace.paths.builds / "profiles" / "classic-test"
        root.mkdir(parents=True)
        (root / "runtime" / "content").mkdir(parents=True)
        (root / "runtime" / "resources").mkdir()

        with mock.patch.object(self.workspace, "_cmake") as cmake:
            self.workspace._build_integrated_classic(root, selected, tests=True)

        cmake.assert_called_once_with(
            root / "sources" / "integrated",
            root / "build" / "integrated",
            [
                "-DENABLE_WARNING_ERRORS=ON",
                "-DPACKAGE_TYPE=none",
                "-DENABLE_PYTHON_PLUGIN=ON",
            ],
            True,
        )
        self.assertEqual(
            (root / "sources" / "integrated" / "client" / "sound").resolve(),
            sound,
        )
        self.assertEqual(
            (
                root
                / "sources"
                / "integrated"
                / "server"
                / "runtime"
                / "content"
            ).resolve(),
            root / "runtime" / "content",
        )
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "server"),
            root / "build" / "integrated" / "server",
        )

    def test_paired_classic_build_falls_back_without_shared_role_builds(self) -> None:
        selected = {
            role: self.workspace.paths.repositories / role
            for role in ("client", "server", "protocol", "libatrinik")
        }
        with (
            mock.patch.object(
                self.workspace, "_profile_build_key", return_value="fallback"
            ),
            mock.patch.object(self.workspace, "_refresh_build_metadata"),
            mock.patch.object(self.workspace, "_collect_content"),
            mock.patch.object(self.workspace, "_stage_resources"),
            mock.patch.object(self.workspace, "_build_protocol") as build_protocol,
            mock.patch.object(self.workspace, "_build_library") as build_library,
            mock.patch.object(self.workspace, "_build_client") as build_client,
            mock.patch.object(self.workspace, "_build_server") as build_server,
            mock.patch.object(self.workspace, "_generate_region_maps"),
        ):
            self.workspace._build_resolved(
                "topology", "default", False, ["client", "server"], selected
            )

        build_protocol.assert_not_called()
        build_library.assert_not_called()
        build_client.assert_called_once()
        build_server.assert_called_once()

    def test_classic_binary_directory_tracks_last_successful_graph(self) -> None:
        root = self.workspace.paths.builds / "profiles" / "classic-test"
        (root / "build").mkdir(parents=True)

        self.assertEqual(
            self.workspace._classic_binary_directory(root, "client"),
            root / "build" / "client",
        )
        self.workspace._record_classic_graph(
            root, {"client", "server"}, "integrated"
        )
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "client"),
            root / "build" / "integrated" / "client",
        )
        self.workspace._record_classic_graph(root, {"client"}, "standalone")
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "client"),
            root / "build" / "client",
        )
        self.assertEqual(
            self.workspace._classic_binary_directory(root, "server"),
            root / "build" / "integrated" / "server",
        )

    def test_profile_schema_namespace_leaves_old_partial_build_inert(self) -> None:
        selected = {
            "resources": self.workspace.paths.repositories / "resources"
        }
        old_key = profile_key(selected)
        old_root = (
            self.workspace.paths.builds / "profiles" / f"default-{old_key}"
        )
        managed_directory(
            old_root,
            self.workspace.paths.builds,
            f"profile:default:{old_key}",
        )
        sentinel = old_root / "historical-output.bin"
        sentinel.write_bytes(b"historical build output\x00\n")

        new_root = self.workspace._build_resolved(
            "resources", "default", False, ["resources"], selected
        )

        self.assertNotEqual(new_root, old_root)
        self.assertEqual(sentinel.read_bytes(), b"historical build output\x00\n")
        self.assertTrue(new_root.is_dir())

    def test_profile_build_key_names_repository_and_branch_coordinates(self) -> None:
        selected = {"server": self.workspace.paths.repositories / "server"}
        with mock.patch(
            "atrinik_workspace.workspace.profile_key", return_value="key"
        ) as make_key:
            self.assertEqual(
                self.workspace._profile_build_key("default", selected), "key"
            )

        namespace = make_key.call_args.kwargs["namespace"]
        providers = json.loads(namespace.split("providers:", 1)[1])
        self.assertEqual(
            providers,
            {
                "server": {
                    "name": "server",
                    "repository": "atrinik/server",
                    "branch": "main",
                    "checkout": "server",
                    "source": ".",
                    "source_includes": [],
                }
            },
        )

    def test_start_point_cannot_be_an_option(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "must not begin"):
            self.workspace.create_worktree(
                "content", "bad-start", "feat/bad-start", "--help", False
            )
        self.assertFalse(
            (self.workspace.paths.worktrees / "content" / "bad-start").exists()
        )

    def test_source_view_reserves_ownership_marker_and_copies_worker(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / MANAGED_MARKER).write_text("component data\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        linked = self.workspace._profile_source_view(root, "linked", source, set())
        self.assertEqual(load_json(linked / MANAGED_MARKER)["purpose"], "source-view:linked")

        copied = self.workspace._profile_source_view(
            root, "copied", source, set(), copy_all=True
        )
        (copied / "README").write_text("changed in view\n", encoding="utf-8")
        self.assertEqual((source / "README").read_text(encoding="utf-8"), "content\n")

    def make_worker_source(self) -> Path:
        source = self.root / "worker-source"
        source.mkdir()
        (source / "package.json").write_text(
            json.dumps(
                {
                    "name": "worker-test",
                    "version": "1.0.0",
                    "scripts": {"check": "node check.js"},
                    "dependencies": {"alpha": "1.0.0"},
                    "devDependencies": {"@scope/beta": "2.0.0"},
                }
            ),
            encoding="utf-8",
        )
        (source / "package-lock.json").write_text(
            json.dumps(
                {
                    "name": "worker-test",
                    "version": "1.0.0",
                    "lockfileVersion": 3,
                    "packages": {},
                }
            ),
            encoding="utf-8",
        )
        (source / "worker.ts").write_text("export const value = 1;\n", encoding="utf-8")
        (source / "src" / "build").mkdir(parents=True)
        (source / "src" / "build" / "nested.ts").write_text(
            "export const nested = true;\n", encoding="utf-8"
        )
        return source

    @staticmethod
    def fake_worker_run(
        installs: list[Path], versions: dict[str, str], install_lock: threading.Lock
    ):
        def invoke(
            arguments: list[str],
            *,
            cwd: Path | None = None,
            capture: bool = False,
            env: dict[str, str] | None = None,
            **_kwargs: object,
        ) -> str:
            if arguments == ["node", "--version"]:
                return versions["node"]
            if arguments == ["npm", "--version"]:
                return versions["npm"]
            if arguments == [
                "node",
                "-p",
                "JSON.stringify({platform:process.platform,arch:process.arch,"
                "versions:process.versions})",
            ]:
                return json.dumps(
                    {
                        "platform": versions.get("node_platform", "linux"),
                        "arch": versions.get("node_architecture", "x64"),
                        "versions": {"modules": "127", "napi": "10"},
                    }
                )
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps(
                    {
                        "cache": (env or {}).get("npm_config_cache"),
                        "ignore-scripts": False,
                    }
                )
            if arguments == ["npm", "ci"]:
                assert cwd is not None
                if (cwd / MANAGED_MARKER).exists() or (
                    cwd / MANAGED_MARKER
                ).is_symlink():
                    raise AssertionError("workspace metadata was exposed to npm")
                if not (cwd / "worker.ts").is_file():
                    raise AssertionError("npm lifecycle source was not staged")
                if (cwd / "worker.ts").stat().st_atime_ns != 0:
                    raise AssertionError(
                        "lifecycle source access time was not normalized"
                    )
                npmrc = cwd / ".npmrc"
                if npmrc.exists():
                    if (
                        npmrc.is_symlink()
                        or stat.S_IMODE(npmrc.stat().st_mode) != 0o600
                    ):
                        raise AssertionError("project npm configuration is not isolated")
                if not (cwd / "src" / "build" / "nested.ts").is_file():
                    raise AssertionError("nested generated-name source was omitted")
                with install_lock:
                    installs.append(cwd)
                modules = cwd / "node_modules"
                (modules / "alpha").mkdir(parents=True)
                (modules / "alpha" / "bin.js").write_text(
                    "console.log('alpha');\n", encoding="utf-8"
                )
                (modules / "@scope" / "beta").mkdir(parents=True)
                (modules / ".bin").mkdir()
                (modules / ".bin" / "alpha").symlink_to("../alpha/bin.js")
                (modules / ".package-lock.json").write_text(
                    json.dumps(
                        {
                            "lockfileVersion": 3,
                            "packages": {
                                "node_modules/alpha": {},
                                "node_modules/@scope/beta": {},
                            },
                        }
                    )
                    + "\n",
                    encoding="utf-8",
                )
                return ""
            raise AssertionError(f"unexpected command: {arguments}")

        return invoke

    def test_worker_dependency_publication_revalidates_after_rename(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}

        def mutate_before_verification(
            output: Path,
            staging: Path,
            backup_prefix: str,
            backup_parent: Path | None = None,
            verify_after_install: object = None,
        ) -> None:
            assert callable(verify_after_install)

            def corrupt_then_verify() -> None:
                (output / "node_modules" / "alpha" / "bin.js").write_text(
                    "post-rename corruption\n", encoding="utf-8"
                )
                verify_after_install()

            worker_replace_directory(
                output,
                staging,
                backup_prefix,
                backup_parent,
                corrupt_then_verify,
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run(
                    installs, versions, threading.Lock()
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace.replace_directory",
                side_effect=mutate_before_verification,
            ),
            self.assertRaisesRegex(WorkspaceError, "published Worker dependencies"),
        ):
            self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(len(installs), 1)
        entries = [
            path
            for path in (self.workspace.paths.builds / "worker-dependencies").iterdir()
            if path.name not in {".transactions", MANAGED_MARKER}
        ]
        self.assertEqual(entries, [])

    def test_worker_dependencies_reuse_exact_inputs_and_rebuild_corruption(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(installs, versions, threading.Lock()),
        ):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            second = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(first[3])
            self.assertTrue(second[3])
            self.assertEqual(first[1], second[1])
            self.assertEqual(len(installs), 1)
            self.assertFalse((first[0].parent / "worker.ts").exists())

            (source / "worker.ts").write_text(
                "export const value = 2;\n", encoding="utf-8"
            )
            application_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(application_changed[3])
            self.assertNotEqual(application_changed[1], first[1])
            self.assertEqual(len(installs), 2)

            (application_changed[0] / "alpha").rename(
                application_changed[0] / "alpha-corrupt"
            )
            rebuilt = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(rebuilt[3])
            self.assertEqual(rebuilt[1], application_changed[1])
            self.assertEqual(len(installs), 3)

            versions["npm"] = "11.1.0"
            invalidated = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(invalidated[1], first[1])
            self.assertEqual(len(installs), 4)

            (source / ".npmrc").write_text("strict-peer-deps=true\n", encoding="utf-8")
            config_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(config_changed[1], invalidated[1])
            self.assertEqual(len(installs), 5)
            self.assertFalse((config_changed[0].parent / ".npmrc").exists())

            (source / "package-lock.json").write_text(
                '{"lockfileVersion":3,"changed":true}\n', encoding="utf-8"
            )
            lock_changed = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertNotEqual(lock_changed[1], config_changed[1])
            self.assertEqual(len(installs), 6)

    def test_worker_dependency_keys_node_runtime_architecture(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {
            "node": "v22.0.0",
            "npm": "11.0.0",
            "node_architecture": "x64",
        }
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(
                installs, versions, threading.Lock()
            ),
        ):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            versions["node_architecture"] = "arm64"
            changed = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertNotEqual(first[1], changed[1])
        self.assertEqual(len(installs), 2)

    def test_worker_dependency_cache_preserves_unowned_entries(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            entry = first[0].parent
            for marker in (None, {"schema_version": 1, "purpose": "unrelated"}):
                with self.subTest(marker=marker):
                    shutil.rmtree(entry)
                    entry.mkdir()
                    valuable = entry / "valuable.txt"
                    valuable.write_text("preserve\n", encoding="utf-8")
                    if marker is not None:
                        atomic_json(entry / MANAGED_MARKER, marker)
                    with self.assertRaisesRegex(
                        WorkspaceError, "unmanaged|marker does not match"
                    ):
                        self.workspace._worker_dependencies(source, {"PATH": "/bin"})
                    self.assertEqual(valuable.read_text(encoding="utf-8"), "preserve\n")

    def test_worker_dependency_cache_authenticates_complete_tree(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            modules = first[0]
            (modules / "alpha" / "bin.js").write_text(
                "corrupt\n", encoding="utf-8"
            )
            rebuilt_content = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_content[3])
            self.assertEqual(len(installs), 2)

            entry_metadata_path = (
                rebuilt_content[0].parent / ".atrinik-worker-dependencies.json"
            )
            entry_metadata = load_json(entry_metadata_path)
            entry_metadata["node_modules_view_sha256"] = "0" * 64
            atomic_json(entry_metadata_path, entry_metadata)
            rebuilt_view_digest = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_view_digest[3])
            self.assertEqual(len(installs), 3)

            (modules / "unexpected.txt").write_text("unexpected\n", encoding="utf-8")
            rebuilt_addition = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_addition[3])
            self.assertEqual(len(installs), 4)

            (modules / "escape").symlink_to("../../outside")
            rebuilt_link = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
            self.assertFalse(rebuilt_link[3])
            self.assertEqual(len(installs), 5)

    def test_worker_dependency_cache_authenticates_copied_metadata(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            installed = first[0] / "alpha" / "bin.js"
            status = installed.stat()
            os.utime(
                installed,
                ns=(status.st_atime_ns, status.st_mtime_ns + 1),
            )
            rebuilt = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            self.assertFalse(rebuilt[3])
            self.assertEqual(len(installs), 2)

            installed = rebuilt[0] / "alpha" / "bin.js"
            if hasattr(os, "setxattr"):
                try:
                    os.setxattr(installed, "user.atrinik-test", b"changed")
                except OSError:
                    return
                rebuilt_xattr = self.workspace._worker_dependencies(
                    source, {"PATH": "/bin"}
                )
                self.assertFalse(rebuilt_xattr[3])
                self.assertEqual(len(installs), 3)

    def test_worker_dependency_failed_rebuild_preserves_owned_cache(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        damaged = first[0] / "alpha" / "bin.js"
        damaged.write_text("valuable-corrupt-state\n", encoding="utf-8")

        def failing_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "ci"]:
                raise WorkspaceError("simulated install failure")
            return runner(arguments, **kwargs)

        with mock.patch("atrinik_workspace.workspace.run", side_effect=failing_run):
            with self.assertRaisesRegex(WorkspaceError, "simulated install failure"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(
            damaged.read_text(encoding="utf-8"), "valuable-corrupt-state\n"
        )

    def test_worker_dependency_recovers_interrupted_atomic_backup(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            entry = first[0].parent
            transaction = (
                entry.parent
                / ".transactions"
                / f"{first[1]}-backup-_interrupted"
            )
            entry.rename(transaction)
            recovered = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertTrue(recovered[3])
        self.assertEqual(len(installs), 1)
        self.assertTrue(entry.is_dir())
        self.assertFalse(transaction.exists())

    def test_worker_dependency_recovers_unmarked_install_staging(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run(installs, versions, threading.Lock())
        with mock.patch("atrinik_workspace.workspace.run", side_effect=runner):
            first = self.workspace._worker_dependencies(source, {"PATH": "/bin"})
            entry = first[0].parent
            staging = (
                entry.parent
                / ".transactions"
                / f"{first[1]}-staging-install"
            )
            shutil.rmtree(entry)
            staging.mkdir()
            (staging / "partial-install").write_text(
                "interrupted\n", encoding="utf-8"
            )
            recovered = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}
            )
        self.assertFalse(recovered[3])
        self.assertEqual(len(installs), 2)
        self.assertTrue(entry.is_dir())
        self.assertFalse(staging.exists())

    def test_worker_tree_digest_is_canonical_and_lifecycle_rejects_links(self) -> None:
        first = self.root / "tree-first"
        second = self.root / "tree-second"
        first.mkdir()
        second.mkdir()
        (first / "a").write_bytes(b"Xf\0b\0Y")
        (second / "a").write_bytes(b"X")
        (second / "b").write_bytes(b"Y")
        self.assertNotEqual(_tree_digest(first, set()), _tree_digest(second, set()))
        before = _tree_digest(first, set())
        (first / "a").chmod(0o755)
        self.assertNotEqual(before, _tree_digest(first, set()))
        before = _tree_digest(first, set(), copied_metadata=True)
        status = (first / "a").stat()
        os.utime(first / "a", ns=(status.st_atime_ns, status.st_mtime_ns + 1))
        self.assertNotEqual(
            before, _tree_digest(first, set(), copied_metadata=True)
        )
        source = self.make_worker_source()
        (source / "linked.ts").symlink_to("worker.ts")
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run([], versions, threading.Lock()),
        ):
            with self.assertRaisesRegex(WorkspaceError, "symbolic link"):
                self.workspace._worker_dependency_inputs(
                    source, {"PATH": "/bin", "npm_config_cache": "/cache"}
                )

    def test_worker_dependency_rejects_external_npm_configuration(self) -> None:
        source = self.make_worker_source()
        userconfig = self.root / "user.npmrc"
        userconfig.write_text(
            "//registry.example/:_authToken=first\n", encoding="utf-8"
        )
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run([], versions, threading.Lock())

        def configured_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps({"userconfig": str(userconfig)})
            return runner(arguments, **kwargs)

        environment = {"PATH": "/bin", "npm_config_cache": "/cache"}
        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=configured_run
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "external file-backed npm configuration"
            ):
                self.workspace._worker_dependency_inputs(source, environment)

    def test_worker_dependency_rejects_custom_npm_script_shell(self) -> None:
        source = self.make_worker_source()
        script_shell = self.root / "npm-script-shell"
        script_shell.write_text(
            "#!/bin/sh\nexec /bin/sh \"$@\"\n", encoding="utf-8"
        )
        script_shell.chmod(0o755)
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []
        runner = self.fake_worker_run(installs, versions, threading.Lock())

        def configured_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps({"script-shell": str(script_shell)})
            return runner(arguments, **kwargs)

        environment = {"PATH": "/bin", "npm_config_cache": "/cache"}
        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=configured_run
        ):
            for contents in (
                "#!/bin/sh\nexec /bin/sh \"$@\"\n",
                "#!/bin/sh\nexit 99\n",
            ):
                script_shell.write_text(contents, encoding="utf-8")
                with self.assertRaisesRegex(
                    WorkspaceError, "custom npm script-shell"
                ):
                    self.workspace._worker_dependencies(source, environment)
        self.assertEqual(installs, [])

    def test_worker_dependency_rejects_external_node_preload_options(self) -> None:
        source = self.make_worker_source()
        hook = self.root / "node-hook.cjs"
        environment = {
            "PATH": "/bin",
            "NODE_OPTIONS": f"--require={hook}",
        }
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=AssertionError("Node must not run with external preload code"),
        ):
            for contents in ("module.exports = 1;\n", "module.exports = 2;\n"):
                hook.write_text(contents, encoding="utf-8")
                with self.assertRaisesRegex(
                    WorkspaceError, "custom Node execution options"
                ):
                    self.workspace._worker_dependencies(source, environment)

        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []
        runner = self.fake_worker_run(installs, versions, threading.Lock())

        def configured_run(arguments: list[str], **kwargs: object) -> str:
            if arguments == ["npm", "config", "list", "--json"]:
                return json.dumps({"node-options": f"--require={hook}"})
            return runner(arguments, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=configured_run
        ):
            with self.assertRaisesRegex(WorkspaceError, "custom npm node-options"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(installs, [])

    def test_worker_dependency_authenticates_staged_project_npmrc(self) -> None:
        source = self.make_worker_source()
        (source / ".npmrc").write_text("strict-peer-deps=true\n", encoding="utf-8")
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []

        def corrupt_copy(*args: object, **kwargs: object) -> None:
            real_copy_regular_file(*args, **kwargs)
            destination = args[1]
            assert isinstance(destination, Path)
            self.assertEqual(stat.S_IMODE(destination.stat().st_mode), 0o600)
            destination.write_text("strict-peer-deps=false\n", encoding="utf-8")

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run(
                    installs, versions, threading.Lock()
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace._copy_regular_file",
                side_effect=corrupt_copy,
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "does not match its cache key"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(installs, [])

    def test_worker_dependency_authenticates_staged_source_snapshot(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        installs: list[Path] = []

        def corrupt_copy(*args: object, **kwargs: object) -> None:
            real_copy_worker_source(*args, **kwargs)
            destination = args[1]
            assert isinstance(destination, Path)
            (destination / "worker.ts").write_text(
                "mixed snapshot\n", encoding="utf-8"
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run(
                    installs, versions, threading.Lock()
                ),
            ),
            mock.patch(
                "atrinik_workspace.workspace._copy_worker_source",
                side_effect=corrupt_copy,
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "does not match its cache key"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})
        self.assertEqual(installs, [])

    def test_worker_dependency_consumer_holds_key_lock(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}

        def consume(_modules: Path, key: str, _metadata: dict[str, object]) -> str:
            lock = (
                self.workspace.paths.builds
                / "locks"
                / f"worker-dependencies-{key}.lock"
            )
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(lock, "competing cleanup", nonblocking=True):
                    self.fail("dependency lease was not held")
            return "consumed"

        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run([], versions, threading.Lock()),
        ):
            result = self.workspace._worker_dependencies(
                source, {"PATH": "/bin"}, consume
            )
        self.assertEqual(result[5], "consumed")

    def test_worker_dependency_timing_excludes_view_consumption(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        consumed: list[Path] = []
        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=self.fake_worker_run([], versions, threading.Lock()),
            ),
            mock.patch(
                "atrinik_workspace.workspace.time.monotonic",
                side_effect=(10.0, 12.5),
            ),
        ):
            result = self.workspace._worker_dependencies(
                source,
                {"PATH": "/bin"},
                lambda modules, _key, _metadata: consumed.append(modules),
            )
        self.assertEqual(result[4], 2.5)
        self.assertEqual(consumed, [result[0]])

    def test_worker_dependency_rejects_embedded_install_path(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run([], versions, threading.Lock())

        def path_embedding_run(arguments: list[str], **kwargs: object) -> str:
            result = runner(arguments, **kwargs)
            if arguments == ["npm", "ci"]:
                cwd = kwargs["cwd"]
                assert isinstance(cwd, Path)
                (cwd / "node_modules" / "alpha" / "embedded").write_text(
                    str(cwd), encoding="utf-8"
                )
            return result

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=path_embedding_run
        ):
            with self.assertRaisesRegex(WorkspaceError, "embeds its install path"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})

    def test_worker_dependency_hides_reserved_metadata_from_lifecycle(self) -> None:
        source = self.make_worker_source()
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        runner = self.fake_worker_run([], versions, threading.Lock())

        def marker_creating_run(arguments: list[str], **kwargs: object) -> str:
            result = runner(arguments, **kwargs)
            if arguments == ["npm", "ci"]:
                cwd = kwargs["cwd"]
                assert isinstance(cwd, Path)
                atomic_json(cwd / MANAGED_MARKER, {"lifecycle": "unexpected"})
            return result

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=marker_creating_run
        ):
            with self.assertRaisesRegex(WorkspaceError, "reserved workspace metadata"):
                self.workspace._worker_dependencies(source, {"PATH": "/bin"})

    def test_worker_package_and_tool_metadata_fail_closed(self) -> None:
        source = self.make_worker_source()
        package_path = source / "package.json"
        cases = (
            ([], "root is not an object"),
            ({"dependencies": []}, "dependencies is invalid"),
            ({"dependencies": {"../escape": "1"}}, "package name is unsafe"),
        )
        for package, message in cases:
            with self.subTest(message=message):
                package_path.write_text(json.dumps(package), encoding="utf-8")
                with self.assertRaisesRegex(WorkspaceError, message):
                    self.workspace._worker_required_packages(source)

        package_path.write_text(
            json.dumps({"scripts": {"check": "node check.js"}}), encoding="utf-8"
        )
        environment = {"PATH": "/bin", "npm_config_cache": "/cache"}

        def invalid_version(arguments: list[str], **_kwargs: object) -> str:
            if arguments == ["node", "--version"]:
                return "v22\ninvalid"
            return "11.0.0"

        with mock.patch("atrinik_workspace.workspace.run", side_effect=invalid_version):
            with self.assertRaisesRegex(WorkspaceError, "invalid version"):
                self.workspace._worker_dependency_inputs(source, environment)

        def valid_node_runtime() -> str:
            return json.dumps(
                {
                    "platform": "linux",
                    "arch": "x64",
                    "versions": {"modules": "127", "napi": "10"},
                }
            )

        def invalid_runtime(arguments: list[str], **_kwargs: object) -> str:
            if arguments in (["node", "--version"], ["npm", "--version"]):
                return "v22.0.0"
            return "not-json"

        with mock.patch("atrinik_workspace.workspace.run", side_effect=invalid_runtime):
            with self.assertRaisesRegex(WorkspaceError, "runtime identity"):
                self.workspace._worker_dependency_inputs(source, environment)

        def invalid_config(arguments: list[str], **_kwargs: object) -> str:
            if arguments in (["node", "--version"], ["npm", "--version"]):
                return "v22.0.0"
            if arguments[:2] == ["node", "-p"]:
                return valid_node_runtime()
            return "not-json"

        with mock.patch("atrinik_workspace.workspace.run", side_effect=invalid_config):
            with self.assertRaisesRegex(WorkspaceError, "not valid JSON"):
                self.workspace._worker_dependency_inputs(source, environment)

        package_path.write_text(json.dumps({"scripts": []}), encoding="utf-8")
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=lambda arguments, **_kwargs: (
                "{}"
                if arguments == ["npm", "config", "list", "--json"]
                else valid_node_runtime()
                if arguments[:2] == ["node", "-p"]
                else "v22.0.0"
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "scripts are invalid"):
                self.workspace._worker_dependency_inputs(source, environment)

    def test_worker_installed_tree_validation_rejects_unsafe_shapes(self) -> None:
        modules = self.root / "validation-node-modules"
        with self.assertRaisesRegex(WorkspaceError, "not a regular directory"):
            self.workspace._validate_worker_node_modules(
                modules, "0" * 64, "0" * 64, ()
            )
        modules.mkdir()
        hidden = modules / ".package-lock.json"

        def write_hidden(packages: object) -> str:
            hidden.write_text(json.dumps({"packages": packages}), encoding="utf-8")
            return hashlib.sha256(hidden.read_bytes()).hexdigest()

        digest = write_hidden({})
        with self.assertRaisesRegex(WorkspaceError, "installed lockfile does not match"):
            self.workspace._validate_worker_node_modules(
                modules, "0" * 64, _tree_digest(modules, set()), ()
            )
        digest = write_hidden([])
        with self.assertRaisesRegex(WorkspaceError, "packages are invalid"):
            self.workspace._validate_worker_node_modules(
                modules, digest, _tree_digest(modules, set()), ()
            )
        for packages, message in (
            ({"invalid": {}}, "package path is invalid"),
            ({"node_modules/../escape": {}}, "package path is unsafe"),
            ({"node_modules/missing": {}}, "package is missing or unsafe"),
        ):
            with self.subTest(message=message):
                digest = write_hidden(packages)
                with self.assertRaisesRegex(WorkspaceError, message):
                    self.workspace._validate_worker_node_modules(
                        modules, digest, _tree_digest(modules, set()), ()
                    )
        digest = write_hidden({})
        with self.assertRaisesRegex(WorkspaceError, "dependency is missing"):
            self.workspace._validate_worker_node_modules(
                modules, digest, _tree_digest(modules, set()), ("required",)
            )
        with self.assertRaisesRegex(WorkspaceError, "does not match cache metadata"):
            self.workspace._validate_worker_node_modules(
                modules, digest, "0" * 64, ()
            )

    def test_worker_root_lifecycle_scripts_key_complete_source(self) -> None:
        source = self.make_worker_source()
        package = json.loads((source / "package.json").read_text(encoding="utf-8"))
        package["scripts"]["postinstall"] = "node worker.ts"
        (source / "package.json").write_text(json.dumps(package), encoding="utf-8")
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(installs, versions, threading.Lock()),
        ):
            environment = {"PATH": "/bin", "BUILD_MODE": "one"}
            environment["npm_config_cache"] = "/cache"
            first = self.workspace._worker_dependency_inputs(source, environment)
            (source / "worker.ts").write_text(
                "export const value = 2;\n", encoding="utf-8"
            )
            second = self.workspace._worker_dependency_inputs(source, environment)
            changed_environment = dict(environment, BUILD_MODE="two")
            third = self.workspace._worker_dependency_inputs(
                source, changed_environment
            )
        self.assertEqual(first["root_lifecycle_scripts"], ["postinstall"])
        self.assertNotEqual(
            first["lifecycle_source_sha256"],
            second["lifecycle_source_sha256"],
        )
        self.assertNotEqual(
            second["environment_sha256"], third["environment_sha256"]
        )

    def test_worker_dependency_concurrency_installs_once(self) -> None:
        source = self.make_worker_source()
        installs: list[Path] = []
        versions = {"node": "v22.0.0", "npm": "11.0.0"}
        install_lock = threading.Lock()
        with mock.patch(
            "atrinik_workspace.workspace.run",
            side_effect=self.fake_worker_run(installs, versions, install_lock),
        ):
            with ThreadPoolExecutor(max_workers=2) as executor:
                results = list(
                    executor.map(
                        lambda _value: self.workspace._worker_dependencies(
                            source, {"PATH": "/bin"}
                        ),
                        range(2),
                    )
                )
        self.assertEqual(len(installs), 1)
        self.assertEqual(results[0][1], results[1][1])
        self.assertEqual(sorted(result[3] for result in results), [False, True])

    def test_worker_view_reuses_application_and_isolates_dependencies(self) -> None:
        source = self.make_worker_source()
        dependencies = self.root / "cached-node-modules"
        (dependencies / "alpha").mkdir(parents=True)
        (dependencies / "@scope" / "beta").mkdir(parents=True)
        hidden = dependencies / ".package-lock.json"
        hidden.write_text(
            json.dumps(
                {
                    "lockfileVersion": 3,
                    "packages": {
                        "node_modules/alpha": {},
                        "node_modules/@scope/beta": {},
                    },
                }
            )
            + "\n",
            encoding="utf-8",
        )
        hidden_digest = hashlib.sha256(hidden.read_bytes()).hexdigest()
        metadata = {
            "node_modules_lock_sha256": hidden_digest,
            "node_modules_sha256": _tree_digest(
                dependencies,
                set(),
                bounded_symlinks=True,
                copied_metadata=True,
            ),
            "node_modules_view_sha256": _tree_digest(
                dependencies,
                WORKER_VIEW_NODE_MODULES_EXCLUSIONS,
                bounded_symlinks=True,
                copied_metadata=True,
                ignore_root_mtime=True,
            ),
            "inputs": {
                "lifecycle_source_sha256": _tree_digest(
                    source,
                    WORKER_SOURCE_EXCLUSIONS,
                    reject_symlinks=True,
                    copied_metadata=True,
                )
            },
        }
        root = self.workspace.paths.builds / "profiles" / "worker-test"
        managed_directory(root, self.workspace.paths.builds, "worker-test")

        def corrupt_copy(*args: object, **kwargs: object) -> None:
            real_copy_worker_source(*args, **kwargs)
            destination = args[1]
            assert isinstance(destination, Path)
            (destination / "worker.ts").write_text(
                "mixed view snapshot\n", encoding="utf-8"
            )

        with mock.patch(
            "atrinik_workspace.workspace._copy_worker_source",
            side_effect=corrupt_copy,
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "does not match its fingerprint"
            ):
                self.workspace._worker_view(
                    root, source, dependencies, "a" * 64, metadata
                )

        first = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        (first[0] / "node_modules" / ".vite").mkdir()
        (first[0] / "node_modules" / ".vite" / "cache").write_text(
            "profile generated\n", encoding="utf-8"
        )
        (first[0] / "node_modules" / ".mf").mkdir()
        (first[0] / "node_modules" / ".mf" / "cache").write_text(
            "profile generated\n", encoding="utf-8"
        )
        second = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(first[1])
        self.assertTrue(second[1])
        self.assertTrue((second[0] / "src" / "build" / "nested.ts").is_file())
        generated_type_declarations = (
            "publisher-worker-configuration.d.ts",
            "rendezvous-worker-configuration.d.ts",
            "worker-configuration.d.ts",
            "worker-runtime.d.ts",
        )
        for name in generated_type_declarations:
            (second[0] / name).write_text("generated\n", encoding="utf-8")
        self.workspace._reconcile_worker_view_after_checks(
            source, second[0], "a" * 64, metadata
        )
        for name in generated_type_declarations:
            self.assertTrue((second[0] / name).is_file())
        nested_generated_type = second[0] / "src" / "worker-runtime.d.ts"
        nested_generated_type.write_text("unexpected\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "source changed"):
            self.workspace._reconcile_worker_view_after_checks(
                source, second[0], "a" * 64, metadata
            )
        nested_generated_type.unlink()
        unexpected_dependency_output = second[0] / "node_modules" / "alpha" / "changed"
        unexpected_dependency_output.write_text("changed\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "does not match cache metadata"):
            self.workspace._reconcile_worker_view_after_checks(
                source, second[0], "a" * 64, metadata
            )
        unexpected_dependency_output.unlink()

        copied_source = second[0] / "worker.ts"
        copied_status = copied_source.stat()
        os.utime(
            copied_source,
            ns=(copied_status.st_atime_ns, copied_status.st_mtime_ns + 1),
        )
        reconciled_metadata = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(reconciled_metadata[1])
        if hasattr(os, "setxattr"):
            try:
                os.setxattr(
                    reconciled_metadata[0] / "worker.ts",
                    "user.atrinik-view-test",
                    b"changed",
                )
            except OSError:
                pass
            else:
                reconciled_metadata = self.workspace._worker_view(
                    root, source, dependencies, "a" * 64, metadata
                )
                self.assertFalse(reconciled_metadata[1])

        external_metadata = self.root / "matching-worker-view.json"
        view_metadata = reconciled_metadata[0] / ".atrinik-worker-view.json"
        shutil.copy2(view_metadata, external_metadata)
        view_metadata.unlink()
        view_metadata.symlink_to(external_metadata)
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_metadata[0], "a" * 64, metadata
            )
        reconciled_control = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertFalse(reconciled_control[1])
        self.assertFalse(
            (reconciled_control[0] / ".atrinik-worker-view.json").is_symlink()
        )

        copied_source = reconciled_control[0] / "worker.ts"
        copied_status = copied_source.stat()
        os.utime(
            copied_source,
            ns=(copied_status.st_atime_ns, copied_status.st_mtime_ns + 1),
        )
        with mock.patch(
            "atrinik_workspace.workspace._copy_worker_source",
            side_effect=AssertionError("post-check reconciliation copied source bytes"),
        ):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_control[0], "a" * 64, metadata
            )
        reconciled_after_check = self.workspace._worker_view(
            root, source, dependencies, "a" * 64, metadata
        )
        self.assertTrue(reconciled_after_check[1])

        external_marker = self.root / "matching-worker-marker.json"
        marker = reconciled_after_check[0] / MANAGED_MARKER
        shutil.copy2(marker, external_marker)
        view_metadata = reconciled_after_check[0] / ".atrinik-worker-view.json"

        def fail_after_corrupting_controls(*args: object, **kwargs: object) -> None:
            check_environment = kwargs.get("env")
            assert isinstance(check_environment, dict)
            self.assertEqual(check_environment.get("PYTHONDONTWRITEBYTECODE"), "1")
            marker.unlink()
            view_metadata.unlink()
            view_metadata.symlink_to(external_metadata)
            raise subprocess.CalledProcessError(1, ["npm", "run", "check"])

        worker_environment: dict[str, str] = {}
        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=fail_after_corrupting_controls,
            ),
            self.assertRaises(subprocess.CalledProcessError),
        ):
            self.workspace._run_worker_checks(
                reconciled_after_check[0], worker_environment, "a" * 64, metadata
            )
        self.assertNotIn("PYTHONDONTWRITEBYTECODE", worker_environment)
        self.assertEqual(
            load_json(marker),
            {
                "schema_version": 1,
                "purpose": "source-view:metaserver-worker",
            },
        )
        self.assertFalse(view_metadata.is_symlink())

        def fail_after_replacing_controls_with_directories(
            *args: object, **kwargs: object
        ) -> None:
            marker.unlink()
            marker.mkdir()
            (marker / "nested").write_text("corrupt\n", encoding="utf-8")
            view_metadata.unlink()
            view_metadata.mkdir()
            (view_metadata / "nested").write_text("corrupt\n", encoding="utf-8")
            raise subprocess.CalledProcessError(1, ["npm", "run", "check"])

        with (
            mock.patch(
                "atrinik_workspace.workspace.run",
                side_effect=fail_after_replacing_controls_with_directories,
            ),
            self.assertRaises(subprocess.CalledProcessError),
        ):
            self.workspace._run_worker_checks(
                reconciled_after_check[0], {}, "a" * 64, metadata
            )
        self.assertTrue(marker.is_file())
        self.assertTrue(view_metadata.is_file())

        atomic_json(marker, {"schema_version": 1, "purpose": "wrong"})
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_after_check[0], "a" * 64, metadata
            )
        shutil.copy2(external_marker, marker)

        view_metadata.unlink()
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_after_check[0], "a" * 64, metadata
            )
        shutil.copy2(external_metadata, view_metadata)

        marker.unlink()
        marker.symlink_to(external_marker)
        with self.assertRaisesRegex(WorkspaceError, "control metadata"):
            self.workspace._reconcile_worker_view_source(
                source, reconciled_after_check[0], "a" * 64, metadata
            )
        with self.assertRaises(WorkspaceError):
            self.workspace._worker_view(
                root, source, dependencies, "a" * 64, metadata
            )
        self.assertTrue(marker.is_symlink())
        marker.unlink()
        shutil.copy2(external_marker, marker)

        (second[0] / "node_modules" / "alpha" / "local").write_text(
            "profile only\n", encoding="utf-8"
        )
        self.assertFalse((dependencies / "alpha" / "local").exists())

        (source / "worker.ts").write_text(
            "export const value = 2;\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(WorkspaceError, "lifecycle inputs"):
            self.workspace._worker_view(
                root, source, dependencies, "a" * 64, metadata
            )
        metadata["inputs"]["lifecycle_source_sha256"] = _tree_digest(
            source,
            WORKER_SOURCE_EXCLUSIONS,
            reject_symlinks=True,
            copied_metadata=True,
        )

        def corrupt_view_before_verification(
            output: Path,
            staging: Path,
            backup_prefix: str,
            backup_parent: Path | None = None,
            verify_after_install: object = None,
        ) -> None:
            assert callable(verify_after_install)

            def corrupt_then_verify() -> None:
                (output / "worker.ts").write_text(
                    "post-rename corruption\n", encoding="utf-8"
                )
                verify_after_install()

            worker_replace_directory(
                output,
                staging,
                backup_prefix,
                backup_parent,
                corrupt_then_verify,
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.replace_directory",
                side_effect=corrupt_view_before_verification,
            ),
            self.assertRaisesRegex(WorkspaceError, "published Worker view"),
        ):
            self.workspace._worker_view(
                root, source, dependencies, "b" * 64, metadata
            )
        self.assertEqual(
            (first[0] / "worker.ts").read_text(encoding="utf-8"),
            "export const value = 1;\n",
        )
        changed = self.workspace._worker_view(
            root, source, dependencies, "b" * 64, metadata
        )
        self.assertFalse(changed[1])
        self.assertEqual(
            (changed[0] / "worker.ts").read_text(encoding="utf-8"),
            "export const value = 2;\n",
        )
        self.assertFalse((changed[0] / "node_modules" / "alpha" / "local").exists())
    def test_source_view_reconciles_links_copies_and_stale_entries_in_place(self) -> None:
        source = self.workspace.paths.repositories / "server"
        copied_source = source / "install_data"
        copied_file = copied_source / "keys" / "test.pub"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        source_permissions = stat.S_IMODE(copied_source.stat().st_mode)
        copied_source.chmod(0o555)
        try:
            view = self.workspace._profile_source_view(
                root, "server", source, set(), {"install_data"}
            )
        finally:
            copied_source.chmod(source_permissions)
        self.assertEqual(
            stat.S_IMODE((view / "install_data").stat().st_mode), 0o555
        )
        readme = view / "README"
        copied = view / "install_data" / "keys" / "test.pub"
        (view / "stale").write_text("stale\n", encoding="utf-8")
        readme.unlink()
        readme.symlink_to(source / "install_data", target_is_directory=True)
        copied_file.write_text("changed\n", encoding="utf-8")
        (copied_source / "unique-items" / "new").write_text("new\n", encoding="utf-8")

        reconciled = self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )

        self.assertEqual(reconciled, view)
        self.assertFalse((view / "stale").exists())
        self.assertTrue(readme.is_symlink())
        self.assertEqual(os.readlink(readme), str(source / "README"))
        self.assertEqual(readme.resolve(), source / "README")
        self.assertEqual(copied.read_text(encoding="utf-8"), "changed\n")
        self.assertTrue((view / "install_data" / "unique-items" / "new").is_file())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        metadata = load_json(view / SOURCE_VIEW_METADATA)
        self.assertEqual(metadata["purpose"], "source-view:server")

        copied_file.chmod(0o700)
        self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )
        self.assertEqual(copied.stat().st_mode & 0o777, 0o700)
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])

        (source / "README").unlink()
        self.workspace._profile_source_view(
            root, "server", source, set(), {"install_data"}
        )
        self.assertFalse(readme.exists())

    def test_mutable_source_copy_restores_owner_permissions(self) -> None:
        root = self.root / "mutable-copy"
        directory = root / "unique-items"
        directory.mkdir(parents=True)
        regular = directory / "state"
        regular.write_text("test\n", encoding="utf-8")
        executable = root / "tool"
        executable.write_text("test\n", encoding="utf-8")
        regular.chmod(0o444)
        executable.chmod(0o555)
        directory.chmod(0o555)
        root.chmod(0o555)

        self.workspace._make_tree_owner_writable(root)

        self.assertEqual(stat.S_IMODE(root.stat().st_mode), 0o755)
        self.assertEqual(stat.S_IMODE(directory.stat().st_mode), 0o755)
        self.assertEqual(stat.S_IMODE(regular.stat().st_mode), 0o644)
        self.assertEqual(stat.S_IMODE(executable.stat().st_mode), 0o755)

    def test_source_view_link_rejects_symlinked_nested_parent(self) -> None:
        view = self.workspace.paths.builds / "profiles" / "test" / "sources"
        managed_directory(view, self.workspace.paths.builds, "test-sources")
        outside = self.root / "outside-source-view"
        outside.mkdir()
        sentinel = outside / "LICENSE.md"
        sentinel.write_text("preserved\n", encoding="utf-8")
        (view / "docs").symlink_to(outside, target_is_directory=True)

        with self.assertRaisesRegex(WorkspaceError, "link parent is unsafe"):
            self.workspace._source_view_link(
                view,
                "docs/LICENSE.md",
                self.root / "target-license",
                target_is_directory=False,
            )

        self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserved\n")
        self.assertTrue((view / "docs").is_symlink())

    def test_source_view_retains_unchanged_entries_and_rejects_escaping_symlink(
        self,
    ) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        view = self.workspace._profile_source_view(root, "content", source, set())
        inode = (view / "README").lstat().st_ino

        self.workspace._profile_source_view(root, "content", source, set())
        self.assertEqual((view / "README").lstat().st_ino, inode)
        self.assertTrue(self.workspace._source_view_unchanged[str(view.resolve())])

        internal_target = source / "internal-target"
        internal_target.write_text("one\n", encoding="utf-8")
        source_link = source / "internal-link"
        source_link.symlink_to(internal_target.name)
        self.workspace._profile_source_view(root, "content", source, set())
        self.workspace._profile_source_view(root, "content", source, set())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        first_metadata = load_json(view / SOURCE_VIEW_METADATA)
        second_target = source / "second-target"
        second_target.write_text("two\n", encoding="utf-8")
        source_link.unlink()
        source_link.symlink_to(second_target.name)
        self.workspace._profile_source_view(root, "content", source, set())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        self.assertNotEqual(first_metadata, load_json(view / SOURCE_VIEW_METADATA))

        outside_directory = self.root / "outside-directory"
        outside_directory.mkdir()
        sentinel = outside_directory / "sentinel"
        sentinel.write_text("preserved\n", encoding="utf-8")
        (view / "README").unlink()
        (view / "README").symlink_to(outside_directory, target_is_directory=True)
        self.workspace._profile_source_view(root, "content", source, set())
        self.assertTrue(sentinel.is_file())
        self.assertEqual((view / "README").resolve(), source / "README")
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])

        outside = self.root / "outside"
        outside.write_text("outside\n", encoding="utf-8")
        (source / "escape").symlink_to(outside)
        with self.assertRaisesRegex(WorkspaceError, "escapes its source root"):
            self.workspace._profile_source_view(root, "content", source, set())

        (source / "escape").unlink()
        nested = source / "nested-linked-directory"
        nested.mkdir()
        (nested / "escape").symlink_to(outside)
        with self.assertRaisesRegex(WorkspaceError, "escapes its source root"):
            self.workspace._profile_source_view(root, "content", source, set())

    def test_copied_source_view_recursively_excludes_generated_entries(self) -> None:
        source = self.workspace.paths.repositories / "content"
        nested = source / "nested"
        (nested / "kept").mkdir(parents=True)
        (nested / "kept" / "input").write_text("kept\n", encoding="utf-8")
        for excluded in (".git", "build", "dist", "node_modules", ".wrangler"):
            (nested / excluded).mkdir()
            (nested / excluded / "stale").write_text("excluded\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        view = self.workspace._profile_source_view(
            root,
            "worker",
            source,
            {"build", "dist", "node_modules", ".wrangler"},
            copy_all=True,
        )

        self.assertTrue((view / "nested" / "kept" / "input").is_file())
        for excluded in (".git", "build", "dist", "node_modules", ".wrangler"):
            self.assertFalse((view / "nested" / excluded).exists())
        (view / "nested" / "node_modules").mkdir()
        self.workspace._profile_source_view(
            root,
            "worker",
            source,
            {"build", "dist", "node_modules", ".wrangler"},
            copy_all=True,
        )
        self.assertFalse((view / "nested" / "node_modules").exists())

    def test_copied_source_view_allows_only_repository_internal_symlinks(self) -> None:
        source = self.workspace.paths.repositories / "content"
        left = source / "left"
        right = source / "right"
        left.mkdir()
        right.mkdir()
        (right / "value").write_text("safe\n", encoding="utf-8")
        (left / "value-link").symlink_to(Path("../right/value"))
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        view = self.workspace._profile_source_view(
            root, "worker", source, set(), copy_all=True
        )

        self.assertTrue((view / "left" / "value-link").is_symlink())
        self.assertEqual(
            (view / "left" / "value-link").resolve(), view / "right" / "value"
        )
        top_level_link = source / "top-level-link"
        top_level_link.symlink_to(Path("../content/README"))
        conflicting = root / "sources" / source.name
        conflicting.mkdir()
        (conflicting / "README").write_text("wrong\n", encoding="utf-8")
        self.workspace._profile_source_view(
            root, "worker", source, set(), copy_all=True
        )
        self.assertEqual((view / "top-level-link").resolve(), view / "README")
        self.assertNotEqual(
            (view / "top-level-link").resolve(), conflicting / "README"
        )

        (source / "build").mkdir()
        (source / "build" / "generated").write_text("excluded\n", encoding="utf-8")
        (source / "excluded-link").symlink_to(Path("build/generated"))
        with self.assertRaisesRegex(WorkspaceError, "targets an excluded entry"):
            self.workspace._profile_source_view(
                root, "worker", source, {"build"}, copy_all=True
            )
        (source / "excluded-link").unlink()
        outside = self.root / "outside-copy-root"
        outside.write_text("unsafe\n", encoding="utf-8")
        (left / "escape-link").symlink_to(outside)
        with self.assertRaisesRegex(WorkspaceError, "escapes its source root"):
            self.workspace._profile_source_view(
                root, "worker", source, set(), copy_all=True
            )

    def test_profile_views_and_cmake_reject_intermediate_symlink_aliases(self) -> None:
        builds = self.workspace.paths.builds
        profile_a = builds / "profiles" / "a"
        profile_z = builds / "profiles" / "z"
        managed_directory(profile_a, builds, "profile:a")
        managed_directory(profile_z, builds, "profile:z")
        source = self.workspace.paths.repositories / "content"
        target_view = self.workspace._profile_source_view(
            profile_z, "content", source, set()
        )
        (profile_a / "sources").symlink_to(
            profile_z / "sources", target_is_directory=True
        )

        with self.assertRaisesRegex(WorkspaceError, "symlinked managed build path"):
            self.workspace._profile_source_view(profile_a, "content", source, set())
        self.assertEqual((target_view / "README").resolve(), source / "README")

        target_binary = profile_z / "build" / "sample"
        managed_directory(target_binary, builds, "cmake-binary")
        (profile_a / "build").symlink_to(
            profile_z / "build", target_is_directory=True
        )
        with self.assertRaisesRegex(WorkspaceError, "symlinked managed build path"):
            self.workspace._prepare_cmake_binary(profile_a / "build" / "sample")
        self.assertTrue(target_binary.is_dir())

    def test_preserved_source_view_directory_removes_unexpected_children(self) -> None:
        source = self.workspace.paths.repositories / "server"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        view = self.workspace._profile_source_view(
            root, "server", source, {"runtime"}, preserved_entries={"runtime"}
        )
        runtime = view / "runtime"
        runtime.mkdir()
        (runtime / "content").symlink_to(source, target_is_directory=True)
        (runtime / "stale").write_text("stale\n", encoding="utf-8")

        self.workspace._source_view_directory(view, "runtime", {"content"})

        self.assertTrue((runtime / "content").is_symlink())
        self.assertFalse((runtime / "stale").exists())

    def test_cmake_skips_only_unchanged_fingerprint_and_honors_force(self) -> None:
        source_root = self.workspace.paths.repositories / "content"
        (source_root / "CMakeLists.txt").write_text(
            "project(test C)\n", encoding="utf-8"
        )
        command("git", "add", "CMakeLists.txt", cwd=source_root)
        command("git", "commit", "-m", "test: add CMake project", cwd=source_root)
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        source = self.workspace._profile_source_view(root, "content", source_root, set())
        binary = root / "build" / "content"

        def configured(command: list[str], **kwargs: object) -> str | None:
            if command[0] == "git":
                return workspace_run(command, **kwargs)
            if command[:2] == ["ninja", "-C"]:
                return "build.ninja:\n  input: RERUN_CMAKE\n"
            if command[:2] != ["cmake", "-S"]:
                return
            (binary / "CMakeCache.txt").write_text(
                f"CMAKE_HOME_DIRECTORY:INTERNAL={source.resolve()}\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                "CMAKE_BUILD_TYPE:STRING=Debug\n"
                "BUILD_TESTING:UNINITIALIZED=OFF\n"
                "CMAKE_C_COMPILER_LAUNCHER:UNINITIALIZED=\n"
                "CMAKE_CXX_COMPILER_LAUNCHER:UNINITIALIZED=\n",
                encoding="utf-8",
            )
            (binary / "build.ninja").write_text("# generated\n", encoding="utf-8")

        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value=None),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch("atrinik_workspace.workspace.run", side_effect=configured) as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            first_count = run.call_count
            self.workspace._profile_source_view(root, "content", source_root, set())
            self.workspace._cmake(source, binary, [], tests=False)
            second_commands = [
                call.args[0]
                for call in run.call_args_list[first_count:]
                if call.args[0][0] != "git"
            ]
            self.assertEqual(
                second_commands,
                [
                    ["ninja", "-C", str(binary), "-t", "query", "build.ninja"],
                    ["cmake", "--build", str(binary), "--parallel"],
                ],
            )

            (binary / "build.ninja").unlink()
            repair_start = run.call_count
            self.workspace._cmake(source, binary, [], tests=False)
            repair_commands = [
                call.args[0] for call in run.call_args_list[repair_start:]
            ]
            self.assertTrue(
                any(command[:2] == ["cmake", "-S"] for command in repair_commands)
            )

            self.workspace._force_reconfigure = True
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertTrue(any(command[:2] == ["cmake", "-S"] for command in [
                call.args[0] for call in run.call_args_list[first_count + 1 :]
            ]))

        self.assertTrue((binary / CONFIGURE_METADATA).is_file())

    def test_cmake_fingerprint_invalidates_for_tests_environment_and_toolchain(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / "CMakeLists.txt").write_text("project(test C)\n", encoding="utf-8")
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        identities = {
            "cmake": {"command": "cmake", "path": "/cmake", "version": "cmake 1"},
            "ninja": {"command": "ninja", "path": "/ninja", "version": "ninja 1"},
            "cc": {"command": "cc", "path": "/cc", "version": "cc 1"},
            "c++": {"command": "c++", "path": "/c++", "version": "c++ 1"},
        }
        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value=None),
            mock.patch.object(
                self.workspace, "_tool_identity", side_effect=lambda tool: identities[tool]
            ),
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            self.workspace._cmake(source, binary, [], tests=True)
            with mock.patch.dict(os.environ, {"CFLAGS": "-DCHANGED"}):
                self.workspace._cmake(source, binary, [], tests=True)
            with mock.patch.dict(
                os.environ,
                {
                    "ATRINIK_PACKAGE_VERSION": "review",
                    "PKG_CONFIG_PATH": "/opt/review/pkgconfig",
                },
            ):
                self.workspace._cmake(source, binary, [], tests=True)
            identities["cc"] = {"command": "cc", "path": "/cc", "version": "cc 2"}
            self.workspace._cmake(source, binary, [], tests=True)

        configure_calls = [
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        ]
        self.assertEqual(len(configure_calls), 5)

    def test_tool_identity_handles_wrappers_and_empty_version_output(self) -> None:
        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value="/tool"),
            mock.patch("atrinik_workspace.workspace.run", return_value="") as run,
        ):
            identity = self.workspace._tool_identity("wrapper --compiler cc")

        run.assert_called_once_with(
            ["/tool", "--compiler", "cc", "--version"],
            capture=True,
            trace=False,
        )
        self.assertEqual(identity["version"], "unavailable: empty --version output")

    def test_direct_source_does_not_trust_unowned_source_view_metadata(self) -> None:
        source = self.workspace.paths.repositories / "content"
        cmakelists = source / "CMakeLists.txt"
        cmakelists.write_text("project(test C)\n", encoding="utf-8")
        atomic_json(
            source / SOURCE_VIEW_METADATA,
            {
                "schema_version": 1,
                "purpose": "source-view:forged",
                "source": "/forged",
                "entries": {},
            },
        )

        identity = self.workspace._cmake_source_identity(source)

        self.assertEqual(identity["path"], str(source.resolve()))
        self.assertEqual(
            identity["cmakelists"], hashlib.sha256(cmakelists.read_bytes()).hexdigest()
        )
        self.assertFalse(identity["configure_skip_safe"])

    def test_direct_source_skip_requires_clean_git_identity(self) -> None:
        source = self.workspace.paths.repositories / "protocol"
        cmakelists = source / "CMakeLists.txt"
        cmakelists.write_text("project(test C)\n", encoding="utf-8")
        command("git", "add", "CMakeLists.txt", cwd=source)
        command("git", "commit", "-m", "test: add CMake project", cwd=source)

        clean = self.workspace._cmake_source_identity(source)
        self.assertTrue(clean["configure_skip_safe"])
        self.assertEqual(clean["git"]["root"], str(source.resolve()))
        self.assertEqual(len(clean["git"]["head"]), 40)

        (source / "new-input.c").write_text("int added;\n", encoding="utf-8")
        dirty = self.workspace._cmake_source_identity(source)
        self.assertFalse(dirty["configure_skip_safe"])
        self.assertEqual(dirty["git"]["head"], clean["git"]["head"])

        non_git = self.root / "non-git-cmake-source"
        non_git.mkdir()
        (non_git / "CMakeLists.txt").write_text(
            "project(non_git NONE)\n", encoding="utf-8"
        )
        fallback = self.workspace._cmake_source_identity(non_git)
        self.assertIsNone(fallback["git"])
        self.assertFalse(fallback["configure_skip_safe"])

    def test_dirty_direct_cmake_source_never_skips_explicit_configure(self) -> None:
        source = self.workspace.paths.repositories / "protocol"
        (source / "CMakeLists.txt").write_text("project(test C)\n", encoding="utf-8")
        binary = (
            self.workspace.paths.builds / "profiles" / "test" / "build" / "protocol"
        )
        with (
            mock.patch("atrinik_workspace.workspace.shutil.which", return_value=None),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch.object(self.workspace, "_cmake_state_valid", return_value=True),
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            self.workspace._cmake(source, binary, [], tests=False)

        configure_calls = [
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        ]
        self.assertEqual(len(configure_calls), 2)

    def test_cmake_enables_bounded_marker_owned_ccache_with_normalized_paths(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / "CMakeLists.txt").write_text("project(test CXX)\n", encoding="utf-8")
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        with (
            mock.patch(
                "atrinik_workspace.workspace.shutil.which",
                side_effect=lambda tool: "/usr/bin/ccache" if tool == "ccache" else f"/usr/bin/{tool}",
            ),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch.object(
                self.workspace, "_compiler_supports_prefix_maps", return_value=True
            ),
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(source, binary, [], tests=False)

        configure = next(
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        )
        arguments = configure.args[0]
        environment = configure.kwargs["env"]
        self.assertIn("-DCMAKE_C_COMPILER_LAUNCHER=/usr/bin/ccache", arguments)
        self.assertIn("-fdebug-prefix-map=", environment["CFLAGS"])
        self.assertIn("-ffile-prefix-map=", environment["CXXFLAGS"])
        self.assertEqual(environment["CCACHE_MAXSIZE"], "5G")
        self.assertEqual(environment["CCACHE_BASEDIR"], str(binary.parent.parent.resolve()))
        self.assertEqual(environment["CCACHE_NOHASHDIR"], "true")
        self.assertNotIn("CCACHE_HASHDIR", environment)
        cache = self.workspace.paths.builds / "compiler-cache"
        self.assertEqual(load_json(cache / MANAGED_MARKER)["purpose"], "compiler-cache")
        self.assertEqual(load_json(cache / ".atrinik-cache.json")["max_size"], "5G")

    def test_compiler_cache_first_use_is_serialized_across_processes(self) -> None:
        context = multiprocessing.get_context("spawn")
        ready = context.Queue()
        start = context.Event()
        results = context.Queue()
        source = self.root / "cache-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "project(cache_first_use NONE)\n", encoding="utf-8"
        )
        processes = [
            context.Process(
                target=compiler_cache_first_use_process,
                args=(
                    str(self.wrapper),
                    str(self.workspace_directory),
                    str(source),
                    str(
                        self.workspace.paths.builds
                        / "profiles"
                        / f"cache-first-{index}"
                        / "build"
                        / "sample"
                    ),
                    ready,
                    start,
                    results,
                ),
            )
            for index in range(2)
        ]
        try:
            for process in processes:
                process.start()
            self.assertEqual(len({ready.get(timeout=5) for _ in processes}), 2)
            start.set()
        finally:
            join_or_stop_processes(processes, 10)
        self.assertEqual([process.exitcode for process in processes], [0, 0])
        self.assertEqual(
            [results.get(timeout=2), results.get(timeout=2)], [None, None]
        )
        cache = (
            self.workspace.paths.builds / workspace_module.COMPILER_CACHE_PURPOSE
        )
        self.assertEqual(
            load_json(cache / MANAGED_MARKER)["purpose"],
            workspace_module.COMPILER_CACHE_PURPOSE,
        )
        self.assertEqual(
            load_json(cache / workspace_module.CACHE_METADATA)["max_size"],
            workspace_module.COMPILER_CACHE_MAX_SIZE,
        )

    def test_debug_prefix_flags_require_supported_non_toolchain_compilers(self) -> None:
        source = self.workspace.paths.repositories / "content"
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        environment = {"CFLAGS": "/existing", "CXXFLAGS": "/existing-cxx"}
        with mock.patch.object(
            self.workspace, "_compiler_supports_prefix_maps", return_value=False
        ):
            support = self.workspace._add_debug_prefix_environment(
                source, binary, environment, []
            )
        self.assertEqual(support, {"c": False, "cxx": False})
        self.assertEqual(environment["CFLAGS"], "/existing")
        self.assertEqual(environment["CXXFLAGS"], "/existing-cxx")

        partial_environment = {"CFLAGS": "/c", "CXXFLAGS": "/cxx"}
        with mock.patch.object(
            self.workspace,
            "_compiler_supports_prefix_maps",
            side_effect=[True, False],
        ):
            support = self.workspace._add_debug_prefix_environment(
                source, binary, partial_environment, []
            )
        self.assertEqual(support, {"c": True, "cxx": False})
        self.assertIn("-fdebug-prefix-map=", partial_environment["CFLAGS"])
        self.assertEqual(partial_environment["CXXFLAGS"], "/cxx")

        with mock.patch.object(
            self.workspace, "_compiler_supports_prefix_maps"
        ) as supported:
            support = self.workspace._add_debug_prefix_environment(
                source,
                binary,
                environment,
                ["-DCMAKE_TOOLCHAIN_FILE=/tmp/windows-toolchain.cmake"],
            )
        self.assertEqual(support, {"c": False, "cxx": False})
        supported.assert_not_called()

    def test_cmake_keeps_hash_directory_for_unproven_toolchain_compilers(self) -> None:
        source = self.workspace.paths.repositories / "content"
        (source / "CMakeLists.txt").write_text("project(test C)\n", encoding="utf-8")
        binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "content"
        with (
            mock.patch.dict(os.environ, {"CCACHE_NOHASHDIR": "true"}),
            mock.patch(
                "atrinik_workspace.workspace.shutil.which",
                side_effect=lambda tool: f"/usr/bin/{tool}",
            ),
            mock.patch.object(
                self.workspace,
                "_tool_identity",
                return_value={"command": "tool", "path": "/tool", "version": "1"},
            ),
            mock.patch.object(
                self.workspace, "_compiler_supports_prefix_maps"
            ) as supported,
            mock.patch("atrinik_workspace.workspace.run") as run,
        ):
            self.workspace._cmake(
                source,
                binary,
                ["-DCMAKE_TOOLCHAIN_FILE=/missing/toolchain.cmake"],
                tests=False,
            )

        supported.assert_not_called()
        configure = next(
            call for call in run.call_args_list if call.args[0][:2] == ["cmake", "-S"]
        )
        environment = configure.kwargs["env"]
        self.assertEqual(environment["CCACHE_HASHDIR"], "true")
        self.assertNotIn("CCACHE_NOHASHDIR", environment)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "cmake", "ninja")),
        "real CMake toolchain is unavailable",
    )
    def test_real_cmake_reinitializes_changed_toolchain_and_preserves_init_flags(
        self,
    ) -> None:
        source = self.root / "cmake-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(wrapper_toolchain C)\n"
            "add_executable(sample main.c)\n",
            encoding="utf-8",
        )
        (source / "main.c").write_text(
            "#ifndef TOOLCHAIN_VALUE\n#error missing toolchain flag\n#endif\n"
            "int main(void) { return 0; }\n",
            encoding="utf-8",
        )
        compiler = self.root / "compiler-wrapper"
        compiler.write_text("#!/bin/sh\nexec /usr/bin/cc \"$@\"\n", encoding="utf-8")
        compiler.chmod(0o755)
        fragment = self.root / "toolchain-flags.cmake"
        fragment.write_text(
            'set(CMAKE_C_FLAGS_INIT "-DTOOLCHAIN_VALUE=1")\n', encoding="utf-8"
        )
        toolchain_target = self.root / "toolchain-real.cmake"
        toolchain_target.write_text(
            f'include("{fragment}")\nset(CMAKE_C_COMPILER "{compiler}")\n',
            encoding="utf-8",
        )
        toolchain = self.root / "toolchain.cmake"
        toolchain.symlink_to(toolchain_target)
        binary = self.workspace.paths.builds / "profiles" / "real" / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        cache = (binary / "CMakeCache.txt").read_text(encoding="utf-8")
        self.assertIn("-DTOOLCHAIN_VALUE=1", cache)
        preserved = binary / "preserved-on-unchanged-toolchain"
        preserved.write_text("current\n", encoding="utf-8")
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        self.assertTrue(preserved.is_file())
        sentinel = binary / "removed-on-toolchain-change"
        sentinel.write_text("stale\n", encoding="utf-8")

        fragment.write_text(
            'set(CMAKE_C_FLAGS_INIT "-DTOOLCHAIN_VALUE=2")\n', encoding="utf-8"
        )
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        self.assertFalse(sentinel.exists())
        self.assertIn(
            "-DTOOLCHAIN_VALUE=2",
            (binary / "CMakeCache.txt").read_text(encoding="utf-8"),
        )
        compiler_sentinel = binary / "removed-on-compiler-change"
        compiler_sentinel.write_text("stale\n", encoding="utf-8")
        compiler.write_text(
            "#!/bin/sh\n# updated wrapper\nexec /usr/bin/cc \"$@\"\n",
            encoding="utf-8",
        )
        compiler.chmod(0o755)

        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        self.assertFalse(compiler_sentinel.exists())
        link_sentinel = binary / "removed-on-toolchain-link-change"
        link_sentinel.write_text("stale\n", encoding="utf-8")
        second_target = self.root / "toolchain-second.cmake"
        second_target.write_text(
            f"include([[{fragment}]])\nset(CMAKE_C_COMPILER \"{compiler}\")\n"
            "# second toolchain target\n",
            encoding="utf-8",
        )
        toolchain.unlink()
        toolchain.symlink_to(second_target)

        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        self.assertFalse(link_sentinel.exists())
        (binary / "build.ninja").write_text("corrupt graph\n", encoding="utf-8")
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        self.assertIn(
            "RERUN_CMAKE",
            subprocess.run(
                ["ninja", "-C", str(binary), "-t", "query", "build.ninja"],
                check=True,
                capture_output=True,
                text=True,
            ).stdout,
        )

        compiler_path = self.root / "compiler-path.txt"
        compiler_path.write_text(str(compiler), encoding="utf-8")
        dynamic_target = self.root / "toolchain-dynamic.cmake"
        dynamic_target.write_text(
            f'include("{fragment}")\n'
            f'file(READ "{compiler_path}" SELECTED_COMPILER)\n'
            'set(CMAKE_C_COMPILER "${SELECTED_COMPILER}")\n',
            encoding="utf-8",
        )
        toolchain.unlink()
        toolchain.symlink_to(dynamic_target)
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        incomplete_sentinel = binary / "removed-for-unproven-toolchain-inputs"
        incomplete_sentinel.write_text("stale\n", encoding="utf-8")
        self.workspace._cmake(
            source,
            binary,
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        self.assertFalse(incomplete_sentinel.exists())

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "cmake", "ninja")),
        "real CMake toolchain is unavailable",
    )
    def test_real_cmake_repairs_cache_and_rebuilds_for_implicit_environment(
        self,
    ) -> None:
        source = self.root / "environment-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(environment_rebuild C)\n"
            "find_program(SELECTED_TOOL selected-tool REQUIRED)\n"
            'file(WRITE "${CMAKE_BINARY_DIR}/selected-tool.txt" "${SELECTED_TOOL}")\n'
            "add_executable(environment main.c)\n",
            encoding="utf-8",
        )
        (source / "main.c").write_text(
            '#include <stdio.h>\n#include "selected-value.h"\n'
            "int main(void) { return puts(SELECTED_VALUE); }\n",
            encoding="utf-8",
        )
        include_one = self.root / "include-one"
        include_two = self.root / "include-two"
        include_one.mkdir()
        include_two.mkdir()
        tool_one = self.root / "tool-one"
        tool_two = self.root / "tool-two"
        tool_one.mkdir()
        tool_two.mkdir()
        for directory in (tool_one, tool_two):
            tool = directory / "selected-tool"
            tool.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            tool.chmod(0o755)
        (include_one / "selected-value.h").write_text(
            '#define SELECTED_VALUE "atrinik-environment-one"\n', encoding="utf-8"
        )
        (include_two / "selected-value.h").write_text(
            '#define SELECTED_VALUE "atrinik-environment-two"\n', encoding="utf-8"
        )
        binary = self.workspace.paths.builds / "profiles" / "environment" / "build" / "sample"
        self.workspace._use_ccache = False

        base_path = os.environ.get("PATH", "")
        with mock.patch.dict(
            os.environ,
            {"CPATH": str(include_one), "PATH": f"{tool_one}{os.pathsep}{base_path}"},
        ):
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertEqual(
                (binary / "selected-tool.txt").read_text(encoding="utf-8"),
                str(tool_one / "selected-tool"),
            )
            preserved = binary / "preserved-on-unchanged-environment"
            preserved.write_text("current\n", encoding="utf-8")
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertTrue(preserved.is_file())
            cache = binary / "CMakeCache.txt"
            cache.write_text(
                cache.read_text(encoding="utf-8").replace(
                    "CMAKE_BUILD_TYPE:STRING=Debug",
                    "CMAKE_BUILD_TYPE:STRING=Release",
                ),
                encoding="utf-8",
            )
            self.workspace._cmake(source, binary, [], tests=False)
            self.assertFalse(preserved.exists())
            self.assertIn(
                "CMAKE_BUILD_TYPE:STRING=Debug", cache.read_text(encoding="utf-8")
            )

        environment_sentinel = binary / "removed-on-implicit-environment-change"
        environment_sentinel.write_text("stale\n", encoding="utf-8")
        with mock.patch.dict(
            os.environ,
            {"CPATH": str(include_two), "PATH": f"{tool_two}{os.pathsep}{base_path}"},
        ):
            self.workspace._cmake(source, binary, [], tests=False)
        self.assertFalse(environment_sentinel.exists())
        self.assertIn(
            b"atrinik-environment-two", (binary / "environment").read_bytes()
        )
        discovery_sentinel = binary / "removed-on-discovery-environment-change"
        discovery_sentinel.write_text("stale\n", encoding="utf-8")
        with mock.patch.dict(
            os.environ,
            {"CPATH": str(include_two), "PATH": f"{tool_one}{os.pathsep}{base_path}"},
        ):
            self.workspace._cmake(source, binary, [], tests=False)
        self.assertFalse(discovery_sentinel.exists())
        self.assertEqual(
            (binary / "selected-tool.txt").read_text(encoding="utf-8"),
            str(tool_one / "selected-tool"),
        )

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cmake", "ninja")),
        "real CMake/Ninja toolchain is unavailable",
    )
    def test_real_cmake_reconfigures_after_source_symlink_retarget(self) -> None:
        source = self.root / "symlinked-cmake-source"
        source.mkdir()
        nested = source / "src"
        nested.mkdir()
        (nested / "a.c").write_text("int a;\n", encoding="utf-8")
        first = source / "first.cmake"
        second = source / "second.cmake"
        first.write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(first NONE)\n"
            'file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "first:${SOURCES}")\n',
            encoding="utf-8",
        )
        second.write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(second NONE)\n"
            'file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "second:${SOURCES}")\n',
            encoding="utf-8",
        )
        older = first.stat().st_mtime - 60
        os.utime(second, (older, older))
        cmakelists = source / "CMakeLists.txt"
        cmakelists.symlink_to(first.name)
        root = self.workspace.paths.builds / "profiles" / "symlink-configure"
        managed_directory(root, self.workspace.paths.builds, "profile:symlink")
        view = self.workspace._profile_source_view(
            root, "sample", source, set()
        )
        binary = root / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(view, binary, [], tests=False)
        self.assertIn(
            "first:", (binary / "selected.txt").read_text(encoding="utf-8")
        )
        cmakelists.unlink()
        cmakelists.symlink_to(second.name)
        self.workspace._profile_source_view(root, "sample", source, set())
        self.workspace._cmake(view, binary, [], tests=False)
        selected = (binary / "selected.txt").read_text(encoding="utf-8")
        self.assertIn("second:", selected)
        self.assertNotIn("b.c", selected)

        added = nested / "b.c"
        added.write_text("int b;\n", encoding="utf-8")
        os.utime(added, (older, older))
        self.workspace._profile_source_view(root, "sample", source, set())
        self.workspace._cmake(view, binary, [], tests=False)
        self.assertIn(
            "b.c", (binary / "selected.txt").read_text(encoding="utf-8")
        )

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("git", "cmake", "ninja")),
        "real Git/CMake toolchain is unavailable",
    )
    def test_real_dirty_direct_source_reconfigures_plain_glob(self) -> None:
        source = self.root / "direct-cmake-source"
        (source / "src").mkdir(parents=True)
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(direct_dirty NONE)\n"
            'file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${SOURCES}")\n',
            encoding="utf-8",
        )
        (source / "src" / "a.c").write_text("int a;\n", encoding="utf-8")
        command("git", "init", "-b", "main", cwd=source)
        command("git", "config", "user.name", "Tests", cwd=source)
        command("git", "config", "user.email", "tests@example.invalid", cwd=source)
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: seed direct source", cwd=source)
        root = self.workspace.paths.builds / "profiles" / "direct-dirty"
        managed_directory(root, self.workspace.paths.builds, "profile:direct-dirty")
        binary = root / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(source, binary, [], tests=False)
        self.assertNotIn("b.c", (binary / "selected.txt").read_text(encoding="utf-8"))
        (source / "src" / "b.c").write_text("int b;\n", encoding="utf-8")
        self.workspace._cmake(source, binary, [], tests=False)
        self.assertIn("b.c", (binary / "selected.txt").read_text(encoding="utf-8"))

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("git", "cmake", "ninja")),
        "real Git/CMake toolchain is unavailable",
    )
    def test_real_clean_source_commit_reconfigures_despite_older_mtime(self) -> None:
        source = self.root / "clean-commit-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(clean_commit NONE)\n"
            "configure_file(value.in generated.txt COPYONLY)\n",
            encoding="utf-8",
        )
        configured_input = source / "value.in"
        configured_input.write_text("one\n", encoding="utf-8")
        command("git", "init", "-b", "main", cwd=source)
        command("git", "config", "user.name", "Tests", cwd=source)
        command("git", "config", "user.email", "tests@example.invalid", cwd=source)
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: seed clean source", cwd=source)
        root = self.workspace.paths.builds / "profiles" / "clean-commit"
        managed_directory(root, self.workspace.paths.builds, "profile:clean-commit")
        view = self.workspace._profile_source_view(root, "renamed", source, set())
        binary = root / "build" / "sample"
        self.workspace._use_ccache = False

        self.workspace._cmake(view, binary, [], tests=False)
        self.assertEqual((binary / "generated.txt").read_text(), "one\n")
        sentinel = binary / "preserve-me"
        sentinel.write_text("keep\n", encoding="utf-8")

        configured_input.write_text("two\n", encoding="utf-8")
        command("git", "add", "value.in", cwd=source)
        command("git", "commit", "-m", "test: change configured input", cwd=source)
        older = (binary / "build.ninja").stat().st_mtime - 60
        os.utime(configured_input, (older, older))
        self.workspace._profile_source_view(root, "renamed", source, set())
        self.assertFalse(self.workspace._source_view_unchanged[str(view.resolve())])
        self.workspace._cmake(view, binary, [], tests=False)

        self.assertEqual((binary / "generated.txt").read_text(), "two\n")
        self.assertTrue(sentinel.is_file())

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "ccache", "cmake", "ninja")),
        "real ccache/CMake toolchain is unavailable",
    )
    def test_real_cmake_reuses_ccache_across_equivalent_profile_views(self) -> None:
        with mock.patch.dict(
            os.environ,
            {"ATRINIK_WORKSPACE_DIR": str(self.root / "workspace with spaces")},
        ):
            workspace = Workspace(self.wrapper)
            workspace.paths.ensure()
        checkout = self.root / "shared cmake source"
        checkout.mkdir()
        (checkout / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(wrapper_ccache C)\n"
            "add_executable(sample main.c)\n",
            encoding="utf-8",
        )
        (checkout / "main.c").write_text(
            "const char *source_file = __FILE__;\n"
            "int main(void) { return source_file[0] == 0; }\n",
            encoding="utf-8",
        )
        roots = [
            workspace.paths.builds / "profiles" / name
            for name in ("cache-a", "cache-b")
        ]
        views: list[Path] = []
        for root in roots:
            managed_directory(root, workspace.paths.builds, f"profile:{root.name}")
            views.append(
                workspace._profile_source_view(
                    root, "sample", checkout, set()
                )
            )

        workspace._cmake(
            views[0], roots[0] / "build" / "sample", [], tests=False
        )
        cache = workspace.paths.builds / "compiler-cache"
        statistics_environment = os.environ.copy()
        statistics_environment["CCACHE_DIR"] = str(cache)
        subprocess.run(
            ["ccache", "--zero-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        )
        workspace._cmake(
            views[1], roots[1] / "build" / "sample", [], tests=False
        )

        statistics = subprocess.run(
            ["ccache", "--print-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        ).stdout
        values = {
            name: int(value)
            for name, value in (
                line.split("\t", 1) for line in statistics.splitlines() if "\t" in line
            )
            if value.isdigit()
        }
        self.assertGreater(
            values.get("direct_cache_hit", 0)
            + values.get("preprocessed_cache_hit", 0),
            0,
        )
        object_file = next((roots[1] / "build" / "sample").rglob("main.c.o"))
        object_data = object_file.read_bytes()
        self.assertIn(b"/atrinik/source/main.c", object_data)
        self.assertNotIn(str(roots[1]).encode(), object_data)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cc", "ccache", "cmake", "ninja")),
        "real ccache/CMake toolchain is unavailable",
    )
    def test_real_cmake_toolchain_keeps_profile_paths_out_of_shared_hits(self) -> None:
        source = self.root / "opaque-toolchain-source"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(wrapper_opaque_ccache C)\n"
            "add_executable(opaque_sample opaque_unique_main.c)\n",
            encoding="utf-8",
        )
        (source / "opaque_unique_main.c").write_text(
            "const char *opaque_source_file = __FILE__;\n"
            "int main(void) { return opaque_source_file[0] == 0; }\n",
            encoding="utf-8",
        )
        toolchain = self.root / "opaque-toolchain.cmake"
        toolchain.write_text(
            f'set(CMAKE_C_COMPILER "{shutil.which("cc")}")\n', encoding="utf-8"
        )
        roots = [
            self.workspace.paths.builds / "profiles" / name
            for name in ("opaque-cache-a", "opaque-cache-b")
        ]
        views: list[Path] = []
        for root in roots:
            managed_directory(root, self.workspace.paths.builds, f"profile:{root.name}")
            views.append(
                self.workspace._profile_source_view(root, "opaque", source, set())
            )

        self.workspace._cmake(
            views[0],
            roots[0] / "build" / "opaque",
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )
        cache = self.workspace.paths.builds / "compiler-cache"
        statistics_environment = os.environ.copy()
        statistics_environment["CCACHE_DIR"] = str(cache)
        subprocess.run(
            ["ccache", "--zero-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        )
        self.workspace._cmake(
            views[1],
            roots[1] / "build" / "opaque",
            [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"],
            tests=False,
        )

        statistics = subprocess.run(
            ["ccache", "--print-stats"],
            check=True,
            capture_output=True,
            text=True,
            env=statistics_environment,
        ).stdout
        values = {
            name: int(value)
            for name, value in (
                line.split("\t", 1) for line in statistics.splitlines() if "\t" in line
            )
            if value.isdigit()
        }
        self.assertEqual(
            values.get("direct_cache_hit", 0)
            + values.get("preprocessed_cache_hit", 0),
            0,
        )
        self.assertGreater(values.get("cache_miss", 0), 0)
        object_file = next(
            (roots[1] / "build" / "opaque").rglob("opaque_unique_main.c.o")
        )
        object_data = object_file.read_bytes()
        self.assertIn(str(roots[1]).encode(), object_data)
        self.assertNotIn(str(roots[0]).encode(), object_data)

    def test_resource_view_reserves_generated_metadata_names(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / MANAGED_MARKER).write_text("component marker\n", encoding="utf-8")
        (source / ".atrinik-dependency.json").write_text(
            "component metadata\n", encoding="utf-8"
        )
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        output = self.workspace._stage_resources(root, {"resources": source})

        self.assertEqual(load_json(output / MANAGED_MARKER)["purpose"], "resource-view")
        self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())
        self.assertEqual(
            (source / ".atrinik-dependency.json").read_text(encoding="utf-8"),
            "component metadata\n",
        )
        self.assertTrue((output / "paintings" / "scene.jpg").is_file())
        self.assertFalse((output / "paintings" / "scene.jpg").is_symlink())
        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            "resource\n",
        )
        self.assertFalse((output / "README").exists())

    def test_resource_view_ignores_untracked_files(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / "paintings" / "private.txt").write_text(
            "do not serve\n", encoding="utf-8"
        )
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        output = self.workspace._stage_resources(root, {"resources": source})

        self.assertFalse((output / "paintings" / "private.txt").exists())

    def test_resource_view_rejects_tracked_generated_metadata_names(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / MANAGED_MARKER).write_text("payload\n", encoding="utf-8")
        (source / "runtime-paths.txt").write_text(
            f"{MANAGED_MARKER}\n", encoding="utf-8"
        )
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: select reserved resource", cwd=source)
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        with self.assertRaisesRegex(WorkspaceError, "reserved generated paths"):
            self.workspace._stage_resources(root, {"resources": source})

    def test_resource_view_rejects_unsafe_manifest_path(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        (source / "runtime-paths.txt").write_text("../outside\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        with self.assertRaisesRegex(WorkspaceError, "invalid resource runtime path"):
            self.workspace._stage_resources(root, {"resources": source})

    def test_resource_view_failure_preserves_previous_output(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        (source / "runtime-paths.txt").write_text("../outside\n", encoding="utf-8")

        with self.assertRaisesRegex(WorkspaceError, "invalid resource runtime path"):
            self.workspace._stage_resources(root, {"resources": source})

        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            "resource\n",
        )

    def test_resource_view_reuses_only_exact_clean_valid_inputs(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        copied = 0
        real_copy = shutil.copy2

        def counting_copy(source_path: Path, destination: Path, **kwargs: object) -> None:
            nonlocal copied
            copied += 1
            real_copy(source_path, destination, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.shutil.copy2", side_effect=counting_copy
        ):
            output = self.workspace._stage_resources(root, {"resources": source})
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 1)

            (source / "paintings" / "scene.jpg").write_text(
                "new commit\n", encoding="utf-8"
            )
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: change resource", cwd=source)
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 2)

            dirty = source / "local-only"
            dirty.write_text("dirty\n", encoding="utf-8")
            self.workspace._stage_resources(root, {"resources": source})
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 4)
            self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())
            dirty.unlink()

            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 5)
            (output / RUNTIME_INPUT_METADATA).write_text("{", encoding="utf-8")
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 6)

            (output / MANAGED_MARKER).write_text("{", encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, "cannot read"):
                self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 6)
            atomic_json(
                output / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "resource-view"},
            )

            (output / "unexpected").write_text("corrupt\n", encoding="utf-8")
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 7)
            self.assertFalse((output / "unexpected").exists())

            (output / "paintings" / "scene.jpg").write_text(
                "bad cache!\n", encoding="utf-8"
            )
            self.workspace._stage_resources(root, {"resources": source})
            self.assertEqual(copied, 8)
            self.assertEqual(
                (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
                "new commit\n",
            )

    def test_resource_view_race_preserves_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        previous = (output / "paintings" / "scene.jpg").read_text(encoding="utf-8")

        (source / "paintings" / "scene.jpg").write_text(
            "next commit\n", encoding="utf-8"
        )
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: advance resource", cwd=source)
        real_copy = shutil.copy2
        mutated = False

        def mutate_after_copy(source_path: Path, destination: Path, **kwargs: object) -> None:
            nonlocal mutated
            real_copy(source_path, destination, **kwargs)
            if not mutated:
                mutated = True
                (source / "README").write_text("changed during staging\n", encoding="utf-8")

        try:
            with mock.patch(
                "atrinik_workspace.workspace.shutil.copy2",
                side_effect=mutate_after_copy,
            ):
                with self.assertRaisesRegex(WorkspaceError, "changed during staging"):
                    self.workspace._stage_resources(root, {"resources": source})
        finally:
            command("git", "checkout", "--", "README", cwd=source)

        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            previous,
        )

    def test_resource_install_race_rolls_back_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        previous = (output / "paintings" / "scene.jpg").read_text(encoding="utf-8")
        (source / "paintings" / "scene.jpg").write_text(
            "next commit\n", encoding="utf-8"
        )
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: advance resource", cwd=source)
        advanced = False

        def replace_then_advance(
            destination: Path,
            staging: Path,
            backup_prefix: str,
            verify_after_install: object = None,
        ) -> None:
            nonlocal advanced

            def advance_and_verify() -> None:
                nonlocal advanced
                if not advanced:
                    advanced = True
                    (source / "paintings" / "scene.jpg").write_text(
                        "commit after install\n", encoding="utf-8"
                    )
                    command("git", "add", ".", cwd=source)
                    command(
                        "git",
                        "commit",
                        "-m",
                        "test: race after install",
                        cwd=source,
                    )
                assert callable(verify_after_install)
                verify_after_install()

            workspace_replace_directory(
                destination,
                staging,
                backup_prefix,
                advance_and_verify,
            )

        with mock.patch(
            "atrinik_workspace.workspace.replace_runtime_directory",
            side_effect=replace_then_advance,
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed during staging"):
                self.workspace._stage_resources(root, {"resources": source})

        self.assertEqual(
            (output / "paintings" / "scene.jpg").read_text(encoding="utf-8"),
            previous,
        )

    def test_resource_view_resamples_coordinates_before_staging(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        real_files = self.workspace._resource_runtime_files
        sampled = False

        def advance_after_first_sample(path: Path) -> tuple[list[str], list[str]]:
            nonlocal sampled
            result = real_files(path)
            if not sampled:
                sampled = True
                (source / "catalog").mkdir()
                (source / "catalog" / "resources.json").write_text(
                    "new resource\n", encoding="utf-8"
                )
                (source / "runtime-paths.txt").write_text(
                    "catalog\n", encoding="utf-8"
                )
                command("git", "add", ".", cwd=source)
                command(
                    "git", "commit", "-m", "test: change resource allowlist", cwd=source
                )
            return result

        with mock.patch.object(
            self.workspace,
            "_resource_runtime_files",
            side_effect=advance_after_first_sample,
        ):
            output = self.workspace._stage_resources(root, {"resources": source})

        self.assertTrue((output / "catalog" / "resources.json").is_file())
        self.assertFalse((output / "paintings").exists())
        self.assertEqual(
            load_json(output / RUNTIME_INPUT_METADATA)["coordinate"]["head"],
            command("git", "rev-parse", "HEAD", cwd=source),
        )

    def test_resource_cache_hit_rechecks_source_after_validation(self) -> None:
        source = self.workspace.paths.repositories / "resources"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = self.workspace._stage_resources(root, {"resources": source})
        real_validate = self.workspace._validate_resource_view
        mutated = False

        def mutate_after_validation(
            path: Path,
            selected_source: Path,
            tracked: list[str],
            *,
            require_metadata: bool = True,
        ) -> None:
            nonlocal mutated
            real_validate(
                path,
                selected_source,
                tracked,
                require_metadata=require_metadata,
            )
            if not mutated:
                mutated = True
                (source / "local-race").write_text("dirty\n", encoding="utf-8")

        with mock.patch.object(
            self.workspace,
            "_validate_resource_view",
            side_effect=mutate_after_validation,
        ):
            self.workspace._stage_resources(root, {"resources": source})

        self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())

    def test_content_collection_failure_preserves_previous_output(self) -> None:
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        output = root / "runtime" / "content"
        managed_reset(output, self.workspace.paths.builds, "collected-content")
        (output / "sentinel").write_text("last good\n", encoding="utf-8")

        def fail_collector(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] == os.sys.executable:
                raise WorkspaceError("collector failed")
            return workspace_run(arguments, **kwargs)

        with mock.patch("atrinik_workspace.workspace.run", side_effect=fail_collector):
            with self.assertRaisesRegex(WorkspaceError, "collector failed"):
                self.workspace._collect_content(
                    root,
                    {"content": self.workspace.paths.repositories / "content"},
                )

        self.assertEqual((output / "sentinel").read_text(encoding="utf-8"), "last good\n")

    def test_content_collection_reuses_only_exact_clean_valid_inputs(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        collections = 0

        def collect(arguments: list[str], **kwargs: object) -> str:
            nonlocal collections
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            collections += 1
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                f"collection {collections}\n",
            )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 1)

            (source / "README").write_text("new commit\n", encoding="utf-8")
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: change content", cwd=source)
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 2)

            dirty = source / "local-only"
            dirty.write_text("dirty\n", encoding="utf-8")
            self.workspace._collect_content(root, {"content": source})
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 4)
            self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())
            dirty.unlink()

            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 5)
            (output / RUNTIME_INPUT_METADATA).write_text("{", encoding="utf-8")
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 6)
            compatibility = output / "compatibility.json"
            corrupted = compatibility.read_bytes()
            compatibility.write_bytes(b"X" + corrupted[1:])
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 7)
            (output / MANAGED_MARKER).write_text("{", encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, "cannot read"):
                self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 7)
            atomic_json(
                output / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "collected-content"},
            )
            (output / "manifest.json").unlink()
            self.workspace._collect_content(root, {"content": source})
            self.assertEqual(collections, 8)

    def test_content_collection_race_preserves_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        mutate = False

        def collect(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                "generated\n",
            )
            if mutate:
                (source / "README").write_text(
                    "changed during collection\n", encoding="utf-8"
                )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            previous = (output / "manifest.json").read_text(encoding="utf-8")
            (source / "README").write_text("next commit\n", encoding="utf-8")
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: advance content", cwd=source)
            mutate = True
            try:
                with self.assertRaisesRegex(
                    WorkspaceError, "changed during collection"
                ):
                    self.workspace._collect_content(root, {"content": source})
            finally:
                command("git", "checkout", "--", "README", cwd=source)

        self.assertEqual(
            (output / "manifest.json").read_text(encoding="utf-8"), previous
        )

    def test_content_cache_hit_rechecks_source_after_validation(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        def collect(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                "content\n",
            )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            real_validate = self.workspace._validate_collected_content
            mutated = False

            def mutate_after_validation(
                path: Path,
                coordinate: dict[str, str],
                adapter: str,
                *,
                require_metadata: bool = True,
            ) -> None:
                nonlocal mutated
                real_validate(
                    path,
                    coordinate,
                    adapter,
                    require_metadata=require_metadata,
                )
                if not mutated:
                    mutated = True
                    (source / "local-race").write_text("dirty\n", encoding="utf-8")

            with mock.patch.object(
                self.workspace,
                "_validate_collected_content",
                side_effect=mutate_after_validation,
            ):
                self.workspace._collect_content(root, {"content": source})

        self.assertFalse((output / RUNTIME_INPUT_METADATA).exists())

    def test_content_install_race_rolls_back_previous_cache(self) -> None:
        source = self.workspace.paths.repositories / "content"
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")

        def collect(arguments: list[str], **kwargs: object) -> str:
            if arguments[0] != os.sys.executable:
                return workspace_run(arguments, **kwargs)
            output = Path(arguments[arguments.index("--output") + 1])
            self.make_content_candidate(
                output,
                arguments[arguments.index("--source-commit") + 1],
                "content\n",
            )
            return ""

        with mock.patch("atrinik_workspace.workspace.run", side_effect=collect):
            output = self.workspace._collect_content(root, {"content": source})
            previous = (output / "manifest.json").read_text(encoding="utf-8")
            (source / "README").write_text("next commit\n", encoding="utf-8")
            command("git", "add", ".", cwd=source)
            command("git", "commit", "-m", "test: advance content", cwd=source)
            advanced = False

            def advance_after_metadata(path: Path, value: object) -> None:
                nonlocal advanced
                atomic_json(path, value)
                if path.name == RUNTIME_INPUT_METADATA and not advanced:
                    advanced = True
                    (source / "README").write_text(
                        "commit after metadata\n", encoding="utf-8"
                    )
                    command("git", "add", ".", cwd=source)
                    command(
                        "git",
                        "commit",
                        "-m",
                        "test: race after metadata",
                        cwd=source,
                    )

            with mock.patch(
                "atrinik_workspace.workspace.atomic_json",
                side_effect=advance_after_metadata,
            ):
                with self.assertRaisesRegex(
                    WorkspaceError, "changed during collection"
                ):
                    self.workspace._collect_content(root, {"content": source})

        self.assertEqual(
            (output / "manifest.json").read_text(encoding="utf-8"), previous
        )

    def test_topology_runtime_copies_are_independent_from_shared_cache(self) -> None:
        sources: dict[str, Path] = {}
        for name, purpose in (
            ("content", "collected-content"),
            ("resources", "resource-view"),
            ("client-maps", "region-map-cache"),
        ):
            source = self.root / f"shared-{name}-cache"
            source.mkdir()
            (source / "payload").write_text("shared\n", encoding="utf-8")
            atomic_json(
                source / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
            sources[name] = source
        first_root = self.root / "first-topology"
        second_root = self.root / "second-topology"
        first_root.mkdir()
        second_root.mkdir()
        specifications = tuple(
            (name, sources[name], purpose)
            for name, purpose in (
                ("content", "collected-content"),
                ("resources", "resource-view"),
                ("client-maps", "region-map-cache"),
            )
        )
        first_inputs = self.workspace._copy_topology_runtime_inputs(
            first_root, specifications
        )
        second_inputs = self.workspace._copy_topology_runtime_inputs(
            second_root, specifications
        )

        for name in sources:
            first = first_inputs[name]
            second = second_inputs[name]
            (first / "payload").write_text("first changed\n", encoding="utf-8")
            shutil.rmtree(first)

            self.assertEqual(
                (sources[name] / "payload").read_text(encoding="utf-8"),
                "shared\n",
            )
            self.assertEqual(
                (second / "payload").read_text(encoding="utf-8"), "shared\n"
            )

    def test_runtime_generation_staging_change_publishes_nothing(self) -> None:
        owner = self.root / "runtime-owner"
        owner.mkdir()
        client = self.root / "runtime-client"
        sound = self.root / "runtime-sound"
        binary = self.root / "runtime-client-binary"
        for path in (client, sound, binary):
            path.mkdir()
        (client / "data").write_text("client\n", encoding="utf-8")
        (sound / "sound").write_text("sound\n", encoding="utf-8")
        executable = binary / "atrinik"
        executable.write_text("client\n", encoding="utf-8")
        executable.chmod(0o755)

        with (
            mock.patch.object(
                self.workspace, "_classic_binary_directory", return_value=binary
            ),
            mock.patch.object(
                self.workspace,
                "_runtime_publication_input_digests",
                side_effect=({"client": "before"}, {"client": "after"}),
            ),
            self.assertRaisesRegex(
                WorkspaceError, "inputs changed during staging"
            ),
        ):
            self.workspace._publish_runtime_generation(
                owner,
                "a" * 64,
                "default",
                self.root / "build",
                {"client": client, "sound": sound},
                {},
                ["client"],
                identity={"kind": "test"},
                sound_root=sound,
            )

        self.assertEqual(list((owner / "generations").iterdir()), [])

    def test_runtime_generation_ignores_client_build_headers(self) -> None:
        owner = self.root / "runtime-owner"
        owner.mkdir()
        client = self.root / "runtime-client"
        sound = self.root / "runtime-sound"
        binary = self.root / "runtime-client-binary"
        (client / "src").mkdir(parents=True)
        (sound / "sound").mkdir(parents=True)
        (binary / "src" / "include").mkdir(parents=True)
        (client / "src" / "main.c").write_text("source\n", encoding="utf-8")
        (binary / "src" / "include" / "version.h").write_text(
            "generated\n", encoding="utf-8"
        )
        executable = binary / "atrinik"
        executable.write_text("client\n", encoding="utf-8")
        executable.chmod(0o755)

        with mock.patch.object(
            self.workspace, "_classic_binary_directory", return_value=binary
        ):
            published, lease_fd, _record, state_output_fd = (
                self.workspace._publish_runtime_generation(
                    owner,
                    "a" * 64,
                    "default",
                    self.root / "build",
                    {"client": client, "sound": sound},
                    {},
                    ["client"],
                    identity={"kind": "test"},
                    sound_root=sound,
                )
            )

        try:
            self.assertIsNone(state_output_fd)
            self.assertEqual(
                (published / "client" / "src" / "main.c").read_text(
                    encoding="utf-8"
                ),
                "source\n",
            )
            self.assertFalse(
                (published / "client" / "src" / "include" / "version.h").exists()
            )
            self.assertTrue((published / "client" / "atrinik").is_file())
        finally:
            os.close(lease_fd)

    def test_topology_runtime_set_copy_failure_preserves_all_snapshots(self) -> None:
        topology = self.root / "topology"
        runtime = topology / "runtime"
        runtime.mkdir(parents=True)
        sources: list[tuple[str, Path, str]] = []
        for name, purpose in (
            ("content", "collected-content"),
            ("resources", "resource-view"),
            ("client-maps", "region-map-cache"),
        ):
            source = self.root / f"shared-{name}"
            source.mkdir()
            (source / "payload").write_text("new\n", encoding="utf-8")
            atomic_json(
                source / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
            destination = runtime / name
            destination.mkdir()
            (destination / "payload").write_text("previous\n", encoding="utf-8")
            atomic_json(
                destination / MANAGED_MARKER,
                {"schema_version": 1, "purpose": purpose},
            )
            sources.append((name, source, purpose))
        status = topology / "status.json"
        status.write_text("previous status\n", encoding="utf-8")
        real_copy = self.workspace._copy_topology_runtime_tree
        copied = 0

        def fail_second_copy(
            source_path: Path, destination: Path
        ) -> int:
            nonlocal copied
            copied += 1
            if copied == 2:
                raise OSError("disk full")
            return real_copy(source_path, destination)

        with mock.patch.object(
            self.workspace,
            "_copy_topology_runtime_tree",
            side_effect=fail_second_copy,
        ):
            with self.assertRaisesRegex(OSError, "disk full"):
                self.workspace._copy_topology_runtime_inputs(
                    topology, tuple(sources)
                )

        for name, _source, _purpose in sources:
            self.assertEqual(
                (runtime / name / "payload").read_text(encoding="utf-8"),
                "previous\n",
            )
        self.assertEqual(status.read_text(encoding="utf-8"), "previous status\n")
        self.assertEqual(
            sorted(entry.name for entry in topology.iterdir()),
            ["runtime", "status.json"],
        )

    def test_topology_runtime_set_rejects_internal_links(self) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        source.mkdir()
        external = self.root / "external"
        external.write_text("private\n", encoding="utf-8")
        (source / "payload").symlink_to(external)
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )

        with self.assertRaisesRegex(WorkspaceError, "contains a"):
            self.workspace._copy_topology_runtime_inputs(
                topology,
                (("content", source, "collected-content"),),
            )

        self.assertEqual(list(topology.iterdir()), [])

    def test_runtime_directory_copy_reports_unopenable_source(self) -> None:
        destination = self.root / "runtime-destination"
        destination.mkdir()

        with self.assertRaisesRegex(
            WorkspaceError, "cannot open runtime publication directory"
        ):
            self.workspace._copy_runtime_directory_contents(
                self.root / "missing-runtime-source", destination
            )

        source = self.root / "runtime-source"
        source.mkdir()
        with (
            mock.patch("atrinik_workspace.workspace.os.close") as close,
            self.assertRaisesRegex(
                WorkspaceError, "cannot open runtime publication directory"
            ),
        ):
            self.workspace._copy_runtime_directory_contents(
                source, self.root / "missing-runtime-destination"
            )
        close.assert_called_once()

    def test_topology_runtime_set_rejects_file_changed_to_link_during_copy(
        self,
    ) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        source.mkdir()
        payload = source / "payload"
        payload.write_text("shared\n", encoding="utf-8")
        external = self.root / "external"
        external.write_text("private\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        real_stat = os.stat
        changed = False

        def stat_then_change(
            path: object, *args: object, **kwargs: object
        ) -> os.stat_result:
            nonlocal changed
            result = real_stat(path, *args, **kwargs)
            if path == "payload" and kwargs.get("dir_fd") is not None and not changed:
                changed = True
                payload.unlink()
                payload.symlink_to(external)
            return result

        with mock.patch(
            "atrinik_workspace.workspace.os.stat", side_effect=stat_then_change
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed or contains a link"):
                self.workspace._copy_topology_runtime_inputs(
                    topology,
                    (("content", source, "collected-content"),),
                )

        self.assertEqual(list(topology.iterdir()), [])

    def test_topology_runtime_set_rejects_destination_directory_link_race(
        self,
    ) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        nested = source / "nested"
        nested.mkdir(parents=True)
        (nested / "payload").write_text("shared\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        external = self.root / "external"
        external.mkdir()
        (external / "sentinel").write_text("private\n", encoding="utf-8")
        real_open = os.open
        changed = False

        def open_after_change(
            path: object, flags: int, *args: object, **kwargs: object
        ) -> int:
            nonlocal changed
            directory_fd = kwargs.get("dir_fd")
            if (
                path == "nested"
                and isinstance(directory_fd, int)
                and flags & os.O_DIRECTORY
                and not changed
            ):
                parent = Path(f"/proc/self/fd/{directory_fd}").resolve()
                if ".runtime-" in str(parent):
                    changed = True
                    (parent / "nested").rmdir()
                    (parent / "nested").symlink_to(external, target_is_directory=True)
            return real_open(path, flags, *args, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.os.open", side_effect=open_after_change
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "staging destination changed"
            ):
                self.workspace._copy_topology_runtime_inputs(
                    topology,
                    (("content", source, "collected-content"),),
                )

        self.assertTrue(changed)
        self.assertEqual(
            (external / "sentinel").read_text(encoding="utf-8"), "private\n"
        )
        self.assertEqual(sorted(path.name for path in external.iterdir()), ["sentinel"])
        self.assertEqual(list(topology.iterdir()), [])

    def test_runtime_state_output_cleanup_remains_bound_to_pinned_state(
        self,
    ) -> None:
        state = self.root / "external-state"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "a" * 32
        relocated = self.root / "relocated-state"
        try:
            output, output_fd, output_identity = self.workspace._prepare_runtime_state_output(
                state, generation, state_fd
            )
            os.close(output_fd)
            state.rename(relocated)
            state.mkdir()
            sentinel = state / "sentinel"
            sentinel.write_text("replacement\n", encoding="utf-8")

            self.workspace._remove_runtime_state_output(
                output, generation, state_fd, output_identity
            )

            self.assertFalse(
                (relocated / "tmp" / "runtime-assets" / generation).exists()
            )
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "replacement\n")
            self.assertEqual(sorted(path.name for path in state.iterdir()), ["sentinel"])
        finally:
            os.close(state_fd)

    def test_runtime_state_output_cleanup_rejects_generation_swap(self) -> None:
        state = self.root / "state-output-race"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "b" * 32
        output, output_fd, output_identity = self.workspace._prepare_runtime_state_output(
            state, generation, state_fd
        )
        os.close(output_fd)
        replacement = self.root / "replacement-output"
        replacement.mkdir()
        atomic_json(
            replacement / MANAGED_MARKER,
            {
                "schema_version": 1,
                "purpose": f"runtime-state-output:{generation}",
            },
        )
        sentinel = replacement / "sentinel"
        sentinel.write_text("replacement\n", encoding="utf-8")
        displaced = self.root / "displaced-output"
        real_open = os.open
        swapped = False

        def open_after_swap(
            path: object, flags: int, *args: object, **kwargs: object
        ) -> int:
            nonlocal swapped
            if path == generation and not swapped:
                swapped = True
                output.rename(displaced)
                replacement.rename(output)
            return real_open(path, flags, *args, **kwargs)

        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.os.open",
                    side_effect=open_after_swap,
                ),
                self.assertRaisesRegex(WorkspaceError, "output path changed"),
            ):
                self.workspace._remove_runtime_state_output(
                    output, generation, state_fd, output_identity
                )
            self.assertTrue(swapped)
            self.assertEqual(
                (output / "sentinel").read_text(encoding="utf-8"),
                "replacement\n",
            )
            self.assertTrue((displaced / MANAGED_MARKER).is_file())
        finally:
            os.close(state_fd)

    def test_runtime_state_output_cleanup_rejects_completed_replacement(self) -> None:
        state = self.root / "state-output-replaced"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "c" * 32
        output, output_fd, output_identity = self.workspace._prepare_runtime_state_output(
            state, generation, state_fd
        )
        displaced = self.root / "displaced-runtime-output"
        output.rename(displaced)
        output.mkdir()
        atomic_json(
            output / MANAGED_MARKER,
            {
                "schema_version": 1,
                "purpose": f"runtime-state-output:{generation}",
            },
        )
        sentinel = output / "sentinel"
        sentinel.write_text("replacement\n", encoding="utf-8")
        try:
            pinned = Path(f"/proc/self/fd/{output_fd}") / "pinned-sentinel"
            pinned.write_text("original\n", encoding="utf-8")
            self.assertEqual(
                (displaced / "pinned-sentinel").read_text(encoding="utf-8"),
                "original\n",
            )
            self.assertFalse((output / "pinned-sentinel").exists())
            with self.assertRaisesRegex(WorkspaceError, "identity changed"):
                self.workspace._remove_runtime_state_output(
                    output, generation, state_fd, output_identity
                )
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "replacement\n")
            self.assertTrue((displaced / MANAGED_MARKER).is_file())
        finally:
            os.close(output_fd)
            os.close(state_fd)

    def test_runtime_state_output_cleanup_rejects_fifo_marker_and_mount(self) -> None:
        state = self.root / "state-output-invalid"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "d" * 32
        output, output_fd, output_identity = self.workspace._prepare_runtime_state_output(
            state, generation, state_fd
        )
        os.close(output_fd)
        marker = output / MANAGED_MARKER
        marker.unlink()
        os.mkfifo(marker)
        try:
            with self.assertRaisesRegex(WorkspaceError, "ownership.*invalid"):
                self.workspace._remove_runtime_state_output(
                    output, generation, state_fd, output_identity
                )
            marker.unlink()
            atomic_json(
                marker,
                {
                    "schema_version": 1,
                    "purpose": f"runtime-state-output:{generation}",
                },
            )
            with (
                mock.patch(
                    "atrinik_workspace.workspace._descriptor_mount_id",
                    side_effect=[1, 1, 2],
                ),
                self.assertRaisesRegex(WorkspaceError, "crosses a mount"),
            ):
                self.workspace._remove_runtime_state_output(
                    output, generation, state_fd, output_identity
                )
            self.assertTrue(marker.is_file())
        finally:
            os.close(state_fd)

    def test_runtime_state_output_cleanup_tracks_pending_renames(self) -> None:
        topology = self.workspace._topology_directory(
            "runtime-output-recovery", create=True
        )
        state = self.root / "runtime-output-recovery-state"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "e" * 64
        output, output_fd, output_identity = (
            self.workspace._prepare_runtime_state_output(
                state, generation, state_fd
            )
        )
        os.close(output_fd)
        os.close(state_fd)
        quarantine = state / "quarantine"
        quarantine.mkdir()
        displaced = quarantine / "operator-renamed-output"
        output.rename(displaced)
        status = {
            "name": "runtime-output-recovery",
            "state_policy": {
                "mode": "named",
                "path": str(state),
                "identity": self.workspace._state_identity(state),
            },
            "control": {"generation": generation},
            "runtime": {
                "mutable_state_outputs": [str(output)],
                "mutable_state_output_identities": [output_identity],
            },
        }
        status_path = topology / "status.json"
        atomic_json(status_path, status)
        with mock.patch.object(
            self.workspace,
            "topology_status",
            side_effect=lambda _name: load_json(status_path),
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "ownership evidence is missing"
            ):
                self.workspace._cleanup_topology_mutable_state_outputs(status)
            pending = load_json(status_path)
            self.assertEqual(
                pending["mutable_state_cleanup"]["entries"][0]["status"],
                "pending",
            )
            displaced.rename(output)
            completed = self.workspace._cleanup_topology_mutable_state_outputs(
                pending
            )
        self.assertEqual(
            completed["mutable_state_cleanup"]["entries"][0]["status"],
            "complete",
        )
        self.assertFalse(output.exists())
        output.mkdir()
        with (
            mock.patch.object(
                self.workspace,
                "topology_status",
                side_effect=lambda _name: load_json(status_path),
            ),
            self.assertRaisesRegex(WorkspaceError, "output reappeared"),
        ):
            self.workspace._cleanup_topology_mutable_state_outputs(completed)

    def test_interrupted_runtime_output_publication_is_recovered(self) -> None:
        topology = self.workspace._topology_directory(
            "runtime-output-transaction", create=True
        )
        state = self.root / "runtime-output-transaction-state"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "f" * 64
        output, output_fd, output_identity = (
            self.workspace._prepare_runtime_state_output(
                state, generation, state_fd
            )
        )
        os.close(output_fd)
        os.close(state_fd)
        transaction = topology / workspace_module.RUNTIME_STATE_OUTPUT_TRANSACTION
        atomic_json(
            transaction,
            {
                "schema_version": 1,
                "generation": generation,
                "state": str(state),
                "state_identity": self.workspace._state_identity(state),
                "phase": "prepared",
                "output_identity": output_identity,
            },
        )
        preview = self.workspace.cleanup(["topologies"], 0, [], False)
        topology_item = next(
            item
            for item in preview["items"]
            if item["path"] == str(topology)
        )
        self.assertEqual(topology_item["disposition"], "protected")
        self.assertIn(
            "runtime_state_output_transaction_pending",
            topology_item["reasons"],
        )

        self.workspace._recover_runtime_state_output_transaction(
            topology, "runtime-output-transaction"
        )

        self.assertFalse(output.exists())
        self.assertFalse(transaction.exists())

        atomic_json(
            transaction,
            {
                "schema_version": 1,
                "generation": "2" * 64,
                "state": str(state),
                "state_identity": self.workspace._state_identity(state),
                "phase": "creating",
                "output_identity": None,
            },
        )
        with self.assertRaisesRegex(
            WorkspaceError, "before exact ownership was recorded"
        ):
            self.workspace._recover_runtime_state_output_transaction(
                topology, "runtime-output-transaction"
            )
        self.assertTrue(transaction.is_file())

    def test_runtime_output_creation_marks_uncertainty_before_mkdir(self) -> None:
        state = self.root / "runtime-output-mkdir-interruption"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "9" * 64
        proof = [True]
        real_mkdir = os.mkdir

        def mkdir_then_interrupt(
            name: str | bytes,
            mode: int = 0o777,
            *,
            dir_fd: int | None = None,
        ) -> None:
            real_mkdir(name, mode, dir_fd=dir_fd)
            if name == generation:
                raise KeyboardInterrupt("simulated interruption after mkdir")

        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.os.mkdir",
                    side_effect=mkdir_then_interrupt,
                ),
                self.assertRaisesRegex(KeyboardInterrupt, "after mkdir"),
            ):
                self.workspace._prepare_runtime_state_output(
                    state,
                    generation,
                    state_fd,
                    cleanup_proof=proof,
                )
            self.assertFalse(proof[0])
            self.assertTrue(
                (state / "tmp" / "runtime-assets" / generation).is_dir()
            )
        finally:
            os.close(state_fd)

    def test_runtime_output_preparation_failure_removes_created_generation(self) -> None:
        state = self.root / "runtime-output-preparation-failure"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "4" * 64
        proof = [False]
        real_open = os.open

        def fail_marker_open(
            path: str | bytes,
            flags: int,
            mode: int = 0o777,
            *,
            dir_fd: int | None = None,
        ) -> int:
            if path == MANAGED_MARKER and dir_fd is not None:
                raise OSError("simulated marker publication failure")
            return real_open(path, flags, mode, dir_fd=dir_fd)

        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.os.open",
                    side_effect=fail_marker_open,
                ),
                self.assertRaisesRegex(WorkspaceError, "marker publication failure"),
            ):
                self.workspace._prepare_runtime_state_output(
                    state,
                    generation,
                    state_fd,
                    cleanup_proof=proof,
                )
            self.assertTrue(proof[0])
            self.assertFalse(
                (state / "tmp" / "runtime-assets" / generation).exists()
            )
        finally:
            os.close(state_fd)

    def test_runtime_output_transaction_clear_restores_replacement(self) -> None:
        topology = self.workspace._topology_directory(
            "runtime-output-clear-race", create=True
        )
        transaction = topology / workspace_module.RUNTIME_STATE_OUTPUT_TRANSACTION
        atomic_json(transaction, {"schema_version": 1, "value": "original"})
        displaced = topology / "displaced-transaction"
        real_rename_at = workspace_module.rename_no_replace_at
        raced = False

        def replace_before_rename(
            source_fd: int,
            source: str,
            destination_fd: int,
            destination: str,
        ) -> None:
            nonlocal raced
            if source == workspace_module.RUNTIME_STATE_OUTPUT_TRANSACTION and not raced:
                raced = True
                os.rename(source, displaced.name, src_dir_fd=source_fd, dst_dir_fd=source_fd)
                replacement_fd = os.open(
                    source,
                    os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                    0o600,
                    dir_fd=source_fd,
                )
                try:
                    os.write(replacement_fd, b'{"replacement": true}\n')
                finally:
                    os.close(replacement_fd)
            real_rename_at(source_fd, source, destination_fd, destination)

        with (
            mock.patch(
                "atrinik_workspace.workspace.rename_no_replace_at",
                side_effect=replace_before_rename,
            ),
            self.assertRaisesRegex(WorkspaceError, "changed before removal"),
        ):
            self.workspace._clear_runtime_state_output_transaction(topology)
        self.assertEqual(load_json(transaction), {"replacement": True})
        self.assertEqual(
            load_json(displaced), {"schema_version": 1, "value": "original"}
        )

    def test_pre_spawn_runtime_output_rollback_completes_transaction(self) -> None:
        topology = self.workspace._topology_directory(
            "runtime-output-pre-spawn", create=True
        )
        state = self.root / "runtime-output-pre-spawn-state"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "3" * 64
        output, output_fd, output_identity = (
            self.workspace._prepare_runtime_state_output(
                state, generation, state_fd
            )
        )
        os.close(output_fd)
        transaction = topology / workspace_module.RUNTIME_STATE_OUTPUT_TRANSACTION
        atomic_json(
            transaction,
            {
                "schema_version": 1,
                "generation": generation,
                "state": str(state),
                "state_identity": self.workspace._state_identity(state),
                "phase": "prepared",
                "output_identity": output_identity,
            },
        )
        try:
            self.workspace._rollback_runtime_state_output_transaction(
                topology, output, generation, state_fd, output_identity
            )
            self.assertFalse(output.exists())
            self.assertFalse(transaction.exists())
            self.workspace._recover_runtime_state_output_transaction(
                topology, "runtime-output-pre-spawn"
            )
        finally:
            os.close(state_fd)

    def test_runtime_output_removal_retries_from_empty_tombstone(self) -> None:
        state = self.root / "runtime-output-removal-retry"
        state.mkdir()
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        generation = "1" * 64
        output, output_fd, output_identity = (
            self.workspace._prepare_runtime_state_output(
                state, generation, state_fd
            )
        )
        os.close(output_fd)
        original_remove = workspace_module._remove_owned_tree_contents

        def remove_then_interrupt(*args: object, **kwargs: object) -> None:
            original_remove(*args, **kwargs)
            raise WorkspaceError("simulated output removal interruption")

        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace._remove_owned_tree_contents",
                    side_effect=remove_then_interrupt,
                ),
                self.assertRaisesRegex(
                    WorkspaceError, "simulated output removal interruption"
                ),
            ):
                self.workspace._remove_runtime_state_output(
                    output, generation, state_fd, output_identity
                )
            self.workspace._remove_runtime_state_output(
                output, generation, state_fd, output_identity
            )
            self.assertFalse(output.exists())
        finally:
            os.close(state_fd)

    def test_schema_two_restart_preserves_unowned_mutable_output(self) -> None:
        topology = self.workspace._topology_directory(
            "legacy-output", create=True
        )
        atomic_json(topology / "status.json", {})
        output = self.root / "legacy-runtime-output"
        output.mkdir()
        previous = {
            "schema_version": workspace_module.LEGACY_RUNTIME_TOPOLOGY_STATUS_SCHEMA_VERSION,
            "supervisor": {"running": False},
            "services": {"server": {"running": False}},
            "runtime": {"mutable_state_outputs": [str(output)]},
        }
        with (
            mock.patch.object(
                self.workspace, "topology_status", return_value=previous
            ),
            self.assertRaisesRegex(
                WorkspaceError, "legacy mutable state output"
            ),
        ):
            self.workspace.topology_up(
                "legacy-output", "default", "default", ["server"], 0
            )
        output.rename(output.parent / "operator-renamed-legacy-output")
        with (
            mock.patch.object(
                self.workspace, "topology_status", return_value=previous
            ),
            self.assertRaisesRegex(
                WorkspaceError, "legacy mutable state output"
            ),
        ):
            self.workspace.topology_up(
                "legacy-output", "default", "default", ["server"], 0
            )

    def test_topology_runtime_set_copies_read_only_directories(self) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        nested = source / "nested"
        nested.mkdir(parents=True)
        (nested / "payload").write_text("shared\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        nested.chmod(0o555)
        source.chmod(0o555)

        copied = self.workspace._copy_topology_runtime_inputs(
            topology,
            (("content", source, "collected-content"),),
        )["content"]
        copied = self.workspace._copy_topology_runtime_inputs(
            topology,
            (("content", source, "collected-content"),),
        )["content"]

        self.assertEqual(
            (copied / "nested" / "payload").read_text(encoding="utf-8"),
            "shared\n",
        )
        self.assertEqual(stat.S_IMODE(copied.stat().st_mode), 0o555)
        self.assertEqual(stat.S_IMODE((copied / "nested").stat().st_mode), 0o555)

    def test_replace_directory_cleanup_failure_keeps_committed_output(self) -> None:
        output = self.root / "output"
        output.mkdir()
        (output / "first").write_text("delete first\n", encoding="utf-8")
        (output / "payload").write_text("previous\n", encoding="utf-8")
        staging = self.root / "staging"
        staging.mkdir()
        (staging / "payload").write_text("new\n", encoding="utf-8")
        real_unlink = os.unlink

        def fail_backup_cleanup(
            path: object, *args: object, **kwargs: object
        ) -> None:
            directory_fd = kwargs.get("dir_fd")
            parent = (
                Path(f"/proc/self/fd/{directory_fd}").resolve()
                if isinstance(directory_fd, int)
                else None
            )
            if (
                isinstance(path, str)
                and path.startswith(".remove-")
                and parent is not None
                and parent.name == "previous"
            ):
                raise PermissionError("read only")
            real_unlink(path, *args, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.os.unlink",
            side_effect=fail_backup_cleanup,
        ):
            workspace_replace_directory(output, staging, ".previous-")

            next_staging = self.root / "next-staging"
            next_staging.mkdir()
            (next_staging / "payload").write_text("next\n", encoding="utf-8")
            with self.assertRaisesRegex(
                WorkspaceError, "cannot recover replaced-directory transaction"
            ):
                workspace_replace_directory(output, next_staging, ".previous-")

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "new\n"
        )
        self.assertEqual(
            len(
                [
                    path
                    for path in self.root.iterdir()
                    if path.name.startswith(".previous-")
                ]
            ),
            2,
        )
        self.assertTrue((self.root / ".previous-pending.json").is_file())
        workspace_replace_directory(output, next_staging, ".previous-")
        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "next\n"
        )
        self.assertEqual(
            [
                path
                for path in self.root.iterdir()
                if path.name.startswith(".previous-")
            ],
            [],
        )

    def test_replace_directory_restores_interrupted_pending_output(self) -> None:
        output = self.root / "output"
        pending = self.root / ".previous-pending"
        previous = pending / "previous"
        previous.mkdir(parents=True)
        (previous / "payload").write_text("previous\n", encoding="utf-8")
        atomic_json(
            self.root / ".previous-pending.json",
            {
                "schema_version": 1,
                "purpose": "replaced-directory-backup",
                "output": "output",
                "phase": "prepared",
            },
        )
        missing_staging = self.root / "missing-staging"

        with self.assertRaises(FileNotFoundError):
            workspace_replace_directory(output, missing_staging, ".previous-")

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "previous\n"
        )
        self.assertFalse(pending.exists())
        self.assertFalse((self.root / ".previous-pending.json").exists())

    def test_replace_directory_restores_unverified_installed_output(self) -> None:
        output = self.root / "output"
        output.mkdir()
        (output / "payload").write_text("unverified\n", encoding="utf-8")
        pending = self.root / ".previous-pending"
        previous = pending / "previous"
        previous.mkdir(parents=True)
        (previous / "payload").write_text("previous\n", encoding="utf-8")
        atomic_json(
            self.root / ".previous-pending.json",
            {
                "schema_version": 1,
                "purpose": "replaced-directory-backup",
                "output": "output",
                "phase": "prepared",
            },
        )

        with self.assertRaises(FileNotFoundError):
            workspace_replace_directory(
                output, self.root / "missing-staging", ".previous-"
            )

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "previous\n"
        )
        self.assertFalse(pending.exists())
        self.assertFalse((self.root / ".previous-pending.json").exists())

    def test_owned_tree_removal_refuses_nested_mount_before_deletion(self) -> None:
        owned = self.root / "owned"
        first = owned / "a-first"
        first.mkdir(parents=True)
        first_payload = first / "payload"
        first_payload.write_text("preserve first\n", encoding="utf-8")
        nested = owned / "nested"
        nested.mkdir()
        nested.chmod(0o755)
        payload = nested / "payload"
        payload.write_text("preserve\n", encoding="utf-8")
        first.chmod(0o555)
        owned.chmod(0o555)
        owned_mode = stat.S_IMODE(owned.stat().st_mode)
        first_mode = stat.S_IMODE(first.stat().st_mode)
        original_mode = stat.S_IMODE(nested.stat().st_mode)

        with mock.patch(
            "atrinik_workspace.workspace._descriptor_mount_id",
            side_effect=[1] * 9 + [2],
        ):
            with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                remove_owned_tree(owned)

        self.assertEqual(payload.read_text(encoding="utf-8"), "preserve\n")
        self.assertEqual(
            first_payload.read_text(encoding="utf-8"), "preserve first\n"
        )
        self.assertEqual(stat.S_IMODE(owned.stat().st_mode), owned_mode)
        self.assertEqual(stat.S_IMODE(first.stat().st_mode), first_mode)
        self.assertEqual(stat.S_IMODE(nested.stat().st_mode), original_mode)

    def test_owned_tree_removal_uses_portable_mount_fallback(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        (owned / "payload").write_text("remove\n", encoding="utf-8")

        with (
            mock.patch("atrinik_workspace.workspace.sys.platform", "darwin"),
            mock.patch(
                "atrinik_workspace.workspace._darwin_descriptor_mount_id",
                return_value=(1, 2),
            ),
        ):
            remove_owned_tree(owned)

        self.assertFalse(owned.exists())

    def test_owned_tree_removal_checks_file_mounts_before_deletion(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        first = owned / "a-first"
        mounted = owned / "z-mounted"
        first.write_text("preserve first\n", encoding="utf-8")
        mounted.write_text("preserve mounted\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.workspace._descriptor_mount_id",
            side_effect=[1] * 6 + [2],
        ):
            with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                remove_owned_tree(owned)

        self.assertEqual(first.read_text(encoding="utf-8"), "preserve first\n")
        self.assertEqual(
            mounted.read_text(encoding="utf-8"), "preserve mounted\n"
        )

    def test_owned_tree_removal_does_not_require_procfs(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        (owned / "payload").write_text("remove\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.workspace.Path.read_text",
            side_effect=FileNotFoundError("no procfs"),
        ):
            remove_owned_tree(owned)

        self.assertFalse(owned.exists())

    @unittest.skipUnless(sys.platform == "linux", "requires Linux O_PATH")
    def test_owned_tree_removal_handles_unreadable_directories(self) -> None:
        real_open = os.open

        def deny_initial_read(
            path: object, flags: int, *args: object, **kwargs: object
        ) -> int:
            if path in {"owned", "nested", "unsupported"} and not (
                flags & os.O_PATH
            ):
                raise PermissionError(errno.EACCES, "permission denied")
            return real_open(path, flags, *args, **kwargs)

        readable = self.root / "readable"
        readable.mkdir()
        (readable / "payload").write_text("remove\n", encoding="utf-8")
        with mock.patch(
            "atrinik_workspace.workspace._linux_fchmod_path_descriptor",
            side_effect=WorkspaceError("fchmodat2 unavailable"),
        ) as fallback:
            remove_owned_tree(readable)
        fallback.assert_not_called()
        self.assertFalse(readable.exists())

        owned = self.root / "owned"
        nested = owned / "nested"
        nested.mkdir(parents=True)
        (nested / "payload").write_text("remove\n", encoding="utf-8")
        nested.chmod(0)
        owned.chmod(0)

        with mock.patch(
            "atrinik_workspace.workspace.os.open", side_effect=deny_initial_read
        ):
            remove_owned_tree(owned)

        self.assertFalse(owned.exists())

        unsupported = self.root / "unsupported"
        unsupported.mkdir()
        payload = unsupported / "payload"
        payload.write_text("preserve\n", encoding="utf-8")
        unsupported.chmod(0)
        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.os.open",
                    side_effect=deny_initial_read,
                ),
                mock.patch(
                    "atrinik_workspace.workspace._linux_fchmod_path_descriptor",
                    side_effect=WorkspaceError("fchmodat2 unavailable"),
                ),
            ):
                with self.assertRaisesRegex(WorkspaceError, "unavailable"):
                    remove_owned_tree(unsupported)
            self.assertEqual(stat.S_IMODE(unsupported.stat().st_mode), 0)
            unsupported.chmod(0o700)
            self.assertEqual(payload.read_text(encoding="utf-8"), "preserve\n")
        finally:
            unsupported.chmod(0o700)

    def test_owned_tree_removal_rejects_special_nodes_without_opening(self) -> None:
        owned = self.root / "owned"
        owned.mkdir()
        fifo = owned / "fifo"
        os.mkfifo(fifo)

        with (
            mock.patch("atrinik_workspace.workspace.sys.platform", "darwin"),
            mock.patch(
                "atrinik_workspace.workspace._darwin_descriptor_mount_id",
                return_value=(1, 2),
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "entry is unsupported"):
                remove_owned_tree(owned)

        self.assertTrue(fifo.exists())

    def test_mount_identity_probes_fail_closed(self) -> None:
        class FakeFunction:
            def __init__(self, result: int, values: tuple[int, int] | None = None):
                self.result = result
                self.values = values

            def __call__(self, *arguments: object) -> int:
                if self.values is not None:
                    buffer = arguments[-1]._obj  # type: ignore[attr-defined]
                    ctypes.c_int32.from_buffer(buffer, 48).value = self.values[0]
                    ctypes.c_int32.from_buffer(buffer, 52).value = self.values[1]
                return self.result

        class FakeLibrary:
            def __init__(self, function: FakeFunction):
                self.fstatfs = function
                self.statx = function

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(0, (7, 9))),
        ):
            self.assertEqual(workspace_module._darwin_descriptor_mount_id(1), (7, 9))

        ctypes.set_errno(errno.EIO)
        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(-1)),
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect filesystem"):
                workspace_module._darwin_descriptor_mount_id(1)

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL", return_value=object()
        ):
            with self.assertRaisesRegex(WorkspaceError, "statx mount identity"):
                workspace_module._linux_descriptor_mount_id(1)

        ctypes.set_errno(errno.EIO)
        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(-1)),
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect filesystem"):
                workspace_module._linux_descriptor_mount_id(1)

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(FakeFunction(0)),
        ):
            with self.assertRaisesRegex(WorkspaceError, "did not return"):
                workspace_module._linux_descriptor_mount_id(1)

        class SuccessfulStatx(FakeFunction):
            def __call__(self, *arguments: object) -> int:
                buffer = arguments[-1]._obj  # type: ignore[attr-defined]
                ctypes.c_uint32.from_buffer(buffer, 0).value = 0x1000
                ctypes.c_uint64.from_buffer(buffer, 144).value = 8675309
                return 0

        with mock.patch(
            "atrinik_workspace.workspace.ctypes.CDLL",
            return_value=FakeLibrary(SuccessfulStatx(0)),
        ):
            self.assertEqual(
                workspace_module._linux_descriptor_mount_id(1), 8675309
            )

        with mock.patch("atrinik_workspace.workspace.sys.platform", "freebsd"):
            with self.assertRaisesRegex(WorkspaceError, "unavailable on freebsd"):
                workspace_module._descriptor_mount_id(1)

    def test_owned_tree_removal_detects_descriptor_races(self) -> None:
        invalid = self.root / "invalid-removal-root"
        invalid.write_text("file\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "root is invalid"):
            remove_owned_tree(invalid)

        mounted = self.root / "mounted-removal-root"
        mounted.mkdir()
        mounted_payload = mounted / "payload"
        mounted_payload.write_text("preserve\n", encoding="utf-8")
        mounted_identity = mounted_payload.lstat()
        with mock.patch(
            "atrinik_workspace.workspace._descriptor_mount_id",
            side_effect=[1, 2],
        ):
            with self.assertRaisesRegex(WorkspaceError, "root changed or is mounted"):
                remove_owned_tree(mounted)
        self.assertEqual(
            (mounted_payload.read_text(encoding="utf-8"), mounted_payload.lstat().st_ino),
            ("preserve\n", mounted_identity.st_ino),
        )

        probe_root = self.root / "probe-open-race"
        probe_root.mkdir()
        probe_file = probe_root / "payload"
        probe_file.write_text("data\n", encoding="utf-8")
        probe_identity = probe_file.lstat()
        probe_descriptor = os.open(probe_root, os.O_RDONLY | os.O_DIRECTORY)
        try:
            with mock.patch(
                "atrinik_workspace.workspace.os.open",
                side_effect=OSError("changed"),
            ):
                with self.assertRaisesRegex(WorkspaceError, "entry changed"):
                    workspace_module._probe_owned_tree_entry_mount(
                        probe_descriptor,
                        "payload",
                        probe_file.stat(),
                        1,
                        probe_file,
                    )
        finally:
            os.close(probe_descriptor)
        self.assertEqual(
            (probe_file.read_text(encoding="utf-8"), probe_file.lstat().st_ino),
            ("data\n", probe_identity.st_ino),
        )

        boundary = self.root / "prepare-boundary"
        boundary.mkdir()
        boundary_payload = boundary / "payload"
        boundary_payload.write_text("preserve\n", encoding="utf-8")
        boundary.chmod(0o555)
        boundary_mode = stat.S_IMODE(boundary.stat().st_mode)
        boundary_identity = boundary_payload.lstat()
        boundary_descriptor = os.open(boundary, os.O_RDONLY | os.O_DIRECTORY)
        try:
            boundary_stat = os.fstat(boundary_descriptor)
            with self.assertRaisesRegex(WorkspaceError, "crossed a filesystem"):
                workspace_module._prepare_owned_tree_removal(
                    boundary_descriptor,
                    boundary_stat.st_dev + 1,
                    1,
                    boundary,
                )
        finally:
            os.close(boundary_descriptor)
        self.assertEqual(
            (
                boundary_payload.read_text(encoding="utf-8"),
                boundary_payload.lstat().st_ino,
            ),
            ("preserve\n", boundary_identity.st_ino),
        )
        self.assertEqual(stat.S_IMODE(boundary.stat().st_mode), boundary_mode)

        def changed_device(result: os.stat_result) -> os.stat_result:
            fields = list(result)
            fields[2] += 1
            return os.stat_result(fields)

        for operation in (
            workspace_module._prepare_owned_tree_removal,
            workspace_module._remove_owned_tree_contents,
        ):
            root = self.root / f"device-race-{operation.__name__}"
            root.mkdir()
            child = root / "payload"
            child.write_text("data\n", encoding="utf-8")
            root.chmod(0o555)
            root_mode = stat.S_IMODE(root.stat().st_mode)
            descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            try:
                root_stat = os.fstat(descriptor)
                with (
                    mock.patch(
                        "atrinik_workspace.workspace._descriptor_mount_id",
                        return_value=1,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace._probe_owned_tree_entry_mount"
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace.os.stat",
                        return_value=changed_device(child.stat()),
                    ),
                ):
                    with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                        operation(descriptor, root_stat.st_dev, 1, root)
            finally:
                os.close(descriptor)
            self.assertEqual(child.read_text(encoding="utf-8"), "data\n")
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), root_mode)

        real_open = os.open
        for operation in (
            workspace_module._prepare_owned_tree_removal,
            workspace_module._remove_owned_tree_contents,
        ):
            root = self.root / f"open-race-{operation.__name__}"
            nested = root / "nested"
            nested.mkdir(parents=True)
            nested_payload = nested / "payload"
            nested_payload.write_text("preserve\n", encoding="utf-8")
            nested.chmod(0o555)
            root.chmod(0o555)
            root_mode = stat.S_IMODE(root.stat().st_mode)
            nested_mode = stat.S_IMODE(nested.stat().st_mode)
            nested_identity = nested_payload.lstat()
            descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            root_stat = os.fstat(descriptor)

            def fail_nested_open(
                path: object, flags: int, *args: object, **kwargs: object
            ) -> int:
                if path == "nested" and kwargs.get("dir_fd") == descriptor:
                    raise OSError("changed")
                return real_open(path, flags, *args, **kwargs)

            try:
                with (
                    mock.patch(
                        "atrinik_workspace.workspace._descriptor_mount_id",
                        return_value=1,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace._probe_owned_tree_entry_mount"
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace.os.open",
                        side_effect=fail_nested_open,
                    ),
                ):
                    with self.assertRaisesRegex(WorkspaceError, "directory changed"):
                        operation(descriptor, root_stat.st_dev, 1, root)
            finally:
                os.close(descriptor)
            self.assertEqual(
                (
                    nested_payload.read_text(encoding="utf-8"),
                    nested_payload.lstat().st_ino,
                ),
                ("preserve\n", nested_identity.st_ino),
            )
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), root_mode)
            self.assertEqual(stat.S_IMODE(nested.stat().st_mode), nested_mode)

        for operation in (
            workspace_module._prepare_owned_tree_removal,
            workspace_module._remove_owned_tree_contents,
        ):
            root = self.root / f"identity-race-{operation.__name__}"
            nested = root / "nested"
            nested.mkdir(parents=True)
            nested_payload = nested / "payload"
            nested_payload.write_text("preserve\n", encoding="utf-8")
            nested.chmod(0o555)
            root.chmod(0o555)
            root_mode = stat.S_IMODE(root.stat().st_mode)
            nested_mode = stat.S_IMODE(nested.stat().st_mode)
            nested_identity = nested_payload.lstat()
            descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
            root_stat = os.fstat(descriptor)
            mount_ids = (
                [1, 2]
                if operation is workspace_module._prepare_owned_tree_removal
                else [2]
            )
            try:
                with (
                    mock.patch(
                        "atrinik_workspace.workspace._descriptor_mount_id",
                        side_effect=mount_ids,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace._probe_owned_tree_entry_mount"
                    ),
                ):
                    with self.assertRaisesRegex(WorkspaceError, "encountered a mount"):
                        operation(descriptor, root_stat.st_dev, 1, root)
            finally:
                os.close(descriptor)
            self.assertEqual(
                (
                    nested_payload.read_text(encoding="utf-8"),
                    nested_payload.lstat().st_ino,
                ),
                ("preserve\n", nested_identity.st_ino),
            )
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), root_mode)
            self.assertEqual(stat.S_IMODE(nested.stat().st_mode), nested_mode)

    def test_owned_tree_removal_rejects_root_rebinding(self) -> None:
        for phase in ("before-contents", "before-rmdir"):
            with self.subTest(phase=phase):
                parent = self.root / phase
                parent.mkdir()
                owned = parent / "owned"
                owned.mkdir()
                (owned / "payload").write_text("owned\n", encoding="utf-8")
                identity = self.workspace._state_identity(owned)
                moved = parent / "moved"
                real_prepare = workspace_module._prepare_owned_tree_removal
                real_remove = workspace_module._remove_owned_tree_contents

                def replace_before_contents(*args: object, **kwargs: object) -> None:
                    real_prepare(*args, **kwargs)
                    owned.rename(moved)
                    owned.mkdir()
                    (owned / "replacement").write_text(
                        "preserve\n", encoding="utf-8"
                    )

                def replace_before_rmdir(*args: object, **kwargs: object) -> None:
                    real_remove(*args, **kwargs)
                    owned.rename(moved)
                    owned.mkdir()
                    (owned / "replacement").write_text(
                        "preserve\n", encoding="utf-8"
                    )

                target = (
                    "atrinik_workspace.workspace._prepare_owned_tree_removal"
                    if phase == "before-contents"
                    else "atrinik_workspace.workspace._remove_owned_tree_contents"
                )
                replacement = (
                    replace_before_contents
                    if phase == "before-contents"
                    else replace_before_rmdir
                )
                with (
                    mock.patch(target, side_effect=replacement),
                    self.assertRaisesRegex(
                        WorkspaceError, "owned removal root identity changed"
                    ),
                ):
                    remove_owned_tree(owned, expected_identity=identity)
                self.assertEqual(
                    (owned / "replacement").read_text(encoding="utf-8"),
                    "preserve\n",
                )
                if phase == "before-contents":
                    self.assertEqual(
                        (moved / "payload").read_text(encoding="utf-8"),
                        "owned\n",
                    )

    def test_owned_tree_removal_recovers_identity_named_tombstones(self) -> None:
        root = self.root / "recover-root-tombstone"
        root.mkdir()
        (root / "payload").write_text("owned\n", encoding="utf-8")
        identity = self.workspace._state_identity(root)
        root_tombstone = workspace_module._owned_tree_tombstone_path(
            root, identity
        )
        root.rename(root_tombstone)
        remove_owned_tree(root, expected_identity=identity)
        self.assertFalse(root_tombstone.exists())

        child_root = self.root / "recover-child-tombstone"
        child_root.mkdir()
        child = child_root / "payload"
        child.write_text("owned\n", encoding="utf-8")
        child_metadata = child.stat()
        child_tombstone = child_root / workspace_module._owned_tree_tombstone_name(
            child.name, child_metadata.st_dev, child_metadata.st_ino
        )
        child.rename(child_tombstone)
        remove_owned_tree(
            child_root,
            expected_identity=self.workspace._state_identity(child_root),
        )
        self.assertFalse(child_root.exists())

    def test_owned_tree_removal_rejects_unverified_tombstone(self) -> None:
        root = self.root / "uncertain-child-tombstone"
        root.mkdir()
        original = root / "payload"
        original.write_text("owned\n", encoding="utf-8")
        metadata = original.stat()
        tombstone = root / workspace_module._owned_tree_tombstone_name(
            original.name, metadata.st_dev, metadata.st_ino
        )
        original.rename(self.root / "preserved-original")
        tombstone.write_text("must survive\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "uncertain tombstone"):
            remove_owned_tree(
                root, expected_identity=self.workspace._state_identity(root)
            )
        self.assertEqual(tombstone.read_text(encoding="utf-8"), "must survive\n")

    def test_owned_tree_removal_rechecks_links_after_tombstoning(self) -> None:
        root = self.root / "linked-after-tombstone"
        root.mkdir()
        payload = root / "payload"
        payload.write_text("preserve\n", encoding="utf-8")
        external = self.root / "external-hard-link"
        real_rename = workspace_module.rename_no_replace_at

        def link_after_rename(
            source_fd: int,
            source: str,
            destination_fd: int,
            destination: str,
        ) -> None:
            real_rename(source_fd, source, destination_fd, destination)
            if source == "payload":
                os.link(
                    destination,
                    external,
                    src_dir_fd=destination_fd,
                    follow_symlinks=False,
                )

        with (
            mock.patch(
                "atrinik_workspace.workspace.rename_no_replace_at",
                side_effect=link_after_rename,
            ),
            self.assertRaisesRegex(WorkspaceError, "linked state"),
        ):
            remove_owned_tree(
                root,
                expected_identity=self.workspace._state_identity(root),
                reject_links=True,
            )
        self.assertEqual(external.read_text(encoding="utf-8"), "preserve\n")

    def test_owned_tree_removal_supports_name_max_entries(self) -> None:
        root = self.root / "name-max-removal"
        root.mkdir()
        (root / ("f" * 255)).write_text("owned\n", encoding="utf-8")
        (root / ("d" * 255)).mkdir()
        remove_owned_tree(root, expected_identity=self.workspace._state_identity(root))
        self.assertFalse(root.exists())

    def test_replaced_directory_recovery_rejects_invalid_states(self) -> None:
        def snapshot(parent: Path) -> dict[str, tuple[object, ...]]:
            result: dict[str, tuple[object, ...]] = {}
            for directory, dirnames, filenames in os.walk(
                parent, followlinks=False
            ):
                directory_path = Path(directory)
                for name in sorted([*dirnames, *filenames]):
                    entry = directory_path / name
                    relative = entry.relative_to(parent).as_posix()
                    metadata = entry.lstat()
                    if entry.is_symlink():
                        value: object = os.readlink(entry)
                    elif entry.is_file():
                        value = entry.read_bytes()
                    else:
                        value = None
                    result[relative] = (
                        stat.S_IFMT(metadata.st_mode),
                        stat.S_IMODE(metadata.st_mode),
                        metadata.st_ino,
                        value,
                    )
            return result

        def prepare(
            name: str,
            phase: str,
            *,
            output: str = "absent",
            backup: str = "absent",
        ) -> tuple[Path, Path, Path]:
            parent = self.root / name
            parent.mkdir()
            target = parent / "output"
            pending = parent / ".previous-pending"
            if output == "directory":
                target.mkdir()
            elif output == "symlink":
                external = parent / "external"
                external.mkdir()
                target.symlink_to(external, target_is_directory=True)
            if backup != "absent":
                pending.mkdir()
                if backup == "previous":
                    (pending / "previous").mkdir()
                elif backup == "previous-symlink":
                    external = parent / "backup-external"
                    external.mkdir()
                    (pending / "previous").symlink_to(
                        external, target_is_directory=True
                    )
                elif backup == "other":
                    (pending / "other").write_text("unexpected\n", encoding="utf-8")
            atomic_json(
                parent / ".previous-pending.json",
                {
                    "schema_version": 1,
                    "purpose": "replaced-directory-backup",
                    "output": "output",
                    "phase": phase,
                },
            )
            return target, pending, parent / ".previous-pending.json"

        unmanaged = self.root / "unmanaged"
        unmanaged.mkdir()
        (unmanaged / ".previous-pending").mkdir()
        unmanaged_before = snapshot(unmanaged)
        with self.assertRaisesRegex(WorkspaceError, "is not managed"):
            workspace_module.recover_replaced_directory(
                unmanaged / "output", ".previous-"
            )
        self.assertEqual(snapshot(unmanaged), unmanaged_before)

        invalid_link = self.root / "invalid-link"
        invalid_link.mkdir()
        link_target = invalid_link / "journal-target"
        link_target.write_text("{}", encoding="utf-8")
        (invalid_link / ".previous-pending.json").symlink_to(link_target)
        invalid_link_before = snapshot(invalid_link)
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            workspace_module.recover_replaced_directory(
                invalid_link / "output", ".previous-"
            )
        self.assertEqual(snapshot(invalid_link), invalid_link_before)

        invalid_json = self.root / "invalid-json"
        invalid_json.mkdir()
        atomic_json(invalid_json / ".previous-pending.json", [])
        invalid_json_before = snapshot(invalid_json)
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            workspace_module.recover_replaced_directory(
                invalid_json / "output", ".previous-"
            )
        self.assertEqual(snapshot(invalid_json), invalid_json_before)

        invalid_phase = self.root / "invalid-phase"
        invalid_phase.mkdir()
        atomic_json(
            invalid_phase / ".previous-pending.json",
            {
                "schema_version": 1,
                "purpose": "replaced-directory-backup",
                "output": "output",
                "phase": "unknown",
            },
        )
        invalid_phase_before = snapshot(invalid_phase)
        with self.assertRaisesRegex(WorkspaceError, "journal is invalid"):
            workspace_module.recover_replaced_directory(
                invalid_phase / "output", ".previous-"
            )
        self.assertEqual(snapshot(invalid_phase), invalid_phase_before)

        cases = (
            (
                "initializing-nonempty",
                "initializing",
                "absent",
                "other",
                "backup is invalid",
            ),
            ("prepared-no-backup", "prepared", "absent", "absent", "backup is invalid"),
            (
                "previous-link",
                "prepared",
                "absent",
                "previous-symlink",
                "payload is invalid",
            ),
            (
                "prepared-output-link",
                "prepared",
                "symlink",
                "previous",
                "replacement is invalid",
            ),
            (
                "committed-no-output",
                "committed",
                "absent",
                "previous",
                "replacement is invalid",
            ),
            (
                "committed-empty",
                "committed",
                "absent",
                "empty",
                "replacement is invalid",
            ),
            ("prepared-empty", "prepared", "absent", "empty", "replacement is invalid"),
            ("unexpected-payload", "prepared", "directory", "other", "not empty"),
        )
        for name, phase, output, backup, message in cases:
            with self.subTest(name=name):
                target, _pending, _journal = prepare(
                    name, phase, output=output, backup=backup
                )
                before = snapshot(target.parent)
                with self.assertRaisesRegex(WorkspaceError, message):
                    workspace_module.recover_replaced_directory(
                        target, ".previous-"
                    )
                self.assertEqual(snapshot(target.parent), before)

        target, pending, journal = prepare(
            "committed-clean", "committed", output="directory"
        )
        workspace_module.recover_replaced_directory(target, ".previous-")
        self.assertFalse(pending.exists())
        self.assertFalse(journal.exists())

    def test_content_and_resource_validators_reject_malformed_trees(self) -> None:
        coordinate = {
            "repository": "atrinik/content",
            "branch": "main",
            "head": "a" * 40,
        }

        def content(name: str) -> Path:
            path = self.root / name
            path.mkdir()
            self.make_content_candidate(path, coordinate["head"], "content\n")
            atomic_json(
                path / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "collected-content"},
            )
            return path

        root_file = self.root / "content-file"
        root_file.write_text("bad\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_collected_content(
                root_file, coordinate, "classic-content", require_metadata=False
            )

        missing_directory = content("content-missing-directory")
        shutil.rmtree(missing_directory / "lib")
        with self.assertRaisesRegex(WorkspaceError, "required directory"):
            self.workspace._validate_collected_content(
                missing_directory, coordinate, "classic-content", require_metadata=False
            )

        invalid_manifest = content("content-invalid-manifest")
        atomic_json(invalid_manifest / "manifest.json", [])
        with self.assertRaisesRegex(WorkspaceError, "manifest is invalid"):
            self.workspace._validate_collected_content(
                invalid_manifest, coordinate, "classic-content", require_metadata=False
            )

        invalid_compatibility = content("content-invalid-compatibility")
        compatibility = load_json(invalid_compatibility / "compatibility.json")
        compatibility["branch"] = "1.x"
        atomic_json(invalid_compatibility / "compatibility.json", compatibility)
        with self.assertRaisesRegex(WorkspaceError, "compatibility contract is invalid"):
            self.workspace._validate_collected_content(
                invalid_compatibility,
                coordinate,
                "classic-content",
                require_metadata=False,
            )

        invalid_classic_manifest = content("content-invalid-classic-manifest")
        manifest = load_json(invalid_classic_manifest / "manifest.json")
        manifest["source"]["branch"] = "1.x"
        atomic_json(invalid_classic_manifest / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "Classic content manifest is invalid"):
            self.workspace._validate_collected_content(
                invalid_classic_manifest,
                coordinate,
                "classic-content",
                require_metadata=False,
            )

        with self.assertRaisesRegex(WorkspaceError, "unsupported content build adapter"):
            self.workspace._validate_collected_content(
                content("content-unsupported-adapter"),
                coordinate,
                "future-content",
                require_metadata=False,
            )

        invalid_entry = content("content-invalid-entry")
        manifest = load_json(invalid_entry / "manifest.json")
        manifest["files"][0]["path"] = "../escape"
        atomic_json(invalid_entry / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "file entry is invalid"):
            self.workspace._validate_collected_content(
                invalid_entry, coordinate, "classic-content", require_metadata=False
            )

        linked = content("content-link")
        (linked / "linked").symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "contains a link"):
            self.workspace._validate_collected_content(
                linked, coordinate, "classic-content", require_metadata=False
            )

        extra = content("content-extra")
        (extra / "extra").write_text("extra\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "does not match"):
            self.workspace._validate_collected_content(
                extra, coordinate, "classic-content", require_metadata=False
            )

        wrong_size = content("content-size")
        manifest = load_json(wrong_size / "manifest.json")
        manifest["files"][0]["size"] += 1
        atomic_json(wrong_size / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "size does not match"):
            self.workspace._validate_collected_content(
                wrong_size, coordinate, "classic-content", require_metadata=False
            )

        unreadable = content("content-unreadable")
        real_lstat = Path.lstat
        walking = False

        def fail_content_lstat(candidate: Path) -> os.stat_result:
            if walking and candidate == unreadable / "compatibility.json":
                raise OSError("changed")
            return real_lstat(candidate)

        real_walk = os.walk

        def activate_content_walk(
            *arguments: object, **kwargs: object
        ) -> object:
            nonlocal walking
            walking = True
            yield from real_walk(*arguments, **kwargs)

        with (
            mock.patch(
                "atrinik_workspace.workspace.Path.lstat",
                autospec=True,
                side_effect=fail_content_lstat,
            ),
            mock.patch(
                "atrinik_workspace.workspace.os.walk",
                side_effect=activate_content_walk,
            ),
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect collected"):
                self.workspace._validate_collected_content(
                    unreadable, coordinate, "classic-content", require_metadata=False
                )

        special_content = content("content-special")
        fifo = special_content / "fifo"
        os.mkfifo(fifo)
        manifest = load_json(special_content / "manifest.json")
        manifest["files"].append(
            {"path": "fifo", "sha256": "0" * 64, "size": 0}
        )
        atomic_json(special_content / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "non-regular file"):
            self.workspace._validate_collected_content(
                special_content, coordinate, "classic-content", require_metadata=False
            )

        missing_license = content("content-missing-license-binding")
        manifest = load_json(missing_license / "manifest.json")
        manifest["license_files"] = []
        atomic_json(missing_license / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "license inventory"):
            self.workspace._validate_collected_content(
                missing_license,
                coordinate,
                "classic-content",
                require_metadata=False,
            )

        invalid_license = content("content-invalid-license-binding")
        manifest = load_json(invalid_license / "manifest.json")
        manifest["license_files"][0]["path"] = "maps/COPYING"
        atomic_json(invalid_license / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "license entry is invalid"):
            self.workspace._validate_collected_content(
                invalid_license,
                coordinate,
                "classic-content",
                require_metadata=False,
            )

        incomplete_payload = content("content-incomplete-payload")
        (incomplete_payload / "lib" / "payload").unlink()
        manifest = load_json(incomplete_payload / "manifest.json")
        manifest["files"] = [
            entry for entry in manifest["files"] if entry["path"] != "lib/payload"
        ]
        atomic_json(incomplete_payload / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "payload is incomplete"):
            self.workspace._validate_collected_content(
                incomplete_payload,
                coordinate,
                "classic-content",
                require_metadata=False,
            )

        source = self.workspace.paths.repositories / "resources"

        def resource(name: str) -> Path:
            path = self.root / name
            (path / "paintings").mkdir(parents=True)
            shutil.copy2(
                source / "paintings" / "scene.jpg",
                path / "paintings" / "scene.jpg",
            )
            atomic_json(
                path / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "resource-view"},
            )
            return path

        resource_file = self.root / "resource-file"
        resource_file.write_text("bad\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_resource_view(
                resource_file,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        resource_link = resource("resource-link")
        (resource_link / "linked").symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "contains a link"):
            self.workspace._validate_resource_view(
                resource_link,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        resource_extra = resource("resource-extra")
        (resource_extra / "extra").write_text("extra\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "tracked file set"):
            self.workspace._validate_resource_view(
                resource_extra,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        resource_directory = resource("resource-directory")
        (resource_directory / "extra").mkdir()
        with self.assertRaisesRegex(WorkspaceError, "tracked directories"):
            self.workspace._validate_resource_view(
                resource_directory,
                source,
                ["paintings/scene.jpg"],
                require_metadata=False,
            )

        unreadable_resource = resource("resource-unreadable")

        def fail_resource_lstat(candidate: Path) -> os.stat_result:
            if candidate == unreadable_resource / "paintings" / "scene.jpg":
                raise OSError("changed")
            return real_lstat(candidate)

        with mock.patch(
            "atrinik_workspace.workspace.Path.lstat",
            autospec=True,
            side_effect=fail_resource_lstat,
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect staged"):
                self.workspace._validate_resource_view(
                    unreadable_resource,
                    source,
                    ["paintings/scene.jpg"],
                    require_metadata=False,
                )

        special_resource = self.root / "resource-special"
        (special_resource / "paintings").mkdir(parents=True)
        os.mkfifo(special_resource / "paintings" / "fifo")
        atomic_json(
            special_resource / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "resource-view"},
        )
        with self.assertRaisesRegex(WorkspaceError, "non-regular file"):
            self.workspace._validate_resource_view(
                special_resource,
                source,
                ["paintings/fifo"],
                require_metadata=False,
            )

    def test_default_content_validator_accepts_only_schema_one_source_commit(self) -> None:
        coordinate = {
            "repository": "atrinik/content",
            "branch": "main",
            "head": "a" * 40,
        }
        candidate = self.root / "default-content"
        (candidate / "lib").mkdir(parents=True)
        (candidate / "maps").mkdir()
        atomic_json(
            candidate / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        atomic_json(
            candidate / "manifest.json",
            {
                "schema_version": 1,
                "source_commit": coordinate["head"],
                "files": [],
            },
        )

        self.workspace._validate_collected_content(
            candidate, coordinate, "none", require_metadata=False
        )
        manifest = load_json(candidate / "manifest.json")
        manifest["source_commit"] = "b" * 40
        atomic_json(candidate / "manifest.json", manifest)
        with self.assertRaisesRegex(WorkspaceError, "default content manifest"):
            self.workspace._validate_collected_content(
                candidate, coordinate, "none", require_metadata=False
            )

    def test_stack_selects_only_its_content_publisher_target(self) -> None:
        wrapper = self.root / "actual-wrapper"
        wrapper.mkdir()
        shutil.copy2(
            Path(__file__).resolve().parents[1] / "components.json",
            wrapper / "components.json",
        )
        workspace = Workspace(wrapper)
        workspace.paths.ensure()
        source = self.workspace.paths.repositories / "content"
        commit = command("git", "rev-parse", "HEAD", cwd=source)
        inputs = {
            "schema_version": 1,
            "cacheable": False,
            "coordinate": {
                "component": "content",
                "repository": "atrinik/content",
                "branch": "main",
                "checkout": "content",
                "source": ".",
                "checkout_path": str(source),
                "source_path": str(source),
                "head": commit,
            },
        }
        for stack_name, expected_target in (("default", False), ("classic", True)):
            with self.subTest(stack=stack_name):
                root = workspace.paths.builds / "profiles" / f"target-{stack_name}"
                managed_directory(root, workspace.paths.builds, "test-profile")

                def collect(arguments: list[str], **kwargs: object) -> str:
                    self.assertEqual("--target" in arguments, expected_target)
                    if expected_target:
                        self.assertEqual(arguments[-2:], ["--target", "classic"])
                    output = Path(arguments[arguments.index("--output") + 1])
                    if expected_target:
                        self.make_content_candidate(output, commit, "classic\n")
                    else:
                        (output / "lib").mkdir(parents=True)
                        (output / "maps").mkdir()
                        atomic_json(
                            output / "manifest.json",
                            {
                                "schema_version": 1,
                                "source_commit": commit,
                                "files": [],
                            },
                        )
                    return ""

                with (
                    mock.patch.object(
                        workspace,
                        "_runtime_input_coordinates",
                        return_value=(inputs, False),
                    ),
                    mock.patch.object(
                        workspace,
                        "_load_profile",
                        return_value={"stack": stack_name},
                    ),
                    mock.patch("atrinik_workspace.workspace.run", side_effect=collect),
                ):
                    output = workspace._collect_content(
                        root, {"content": source}, stack_name
                    )

                self.assertTrue((output / "manifest.json").is_file())

    def test_replace_directory_interrupted_journal_publish_is_retryable(
        self,
    ) -> None:
        output = self.root / "output"
        output.mkdir()
        (output / "payload").write_text("previous\n", encoding="utf-8")
        staging = self.root / "staging"
        staging.mkdir()
        (staging / "payload").write_text("new\n", encoding="utf-8")

        with mock.patch(
            "atrinik_workspace.workspace.atomic_json",
            side_effect=KeyboardInterrupt("interrupted"),
        ):
            with self.assertRaises(KeyboardInterrupt):
                workspace_replace_directory(output, staging, ".previous-")

        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "previous\n"
        )
        self.assertFalse((self.root / ".previous-pending").exists())
        workspace_replace_directory(output, staging, ".previous-")
        self.assertEqual(
            (output / "payload").read_text(encoding="utf-8"), "new\n"
        )

    def test_topology_runtime_install_rejects_post_copy_replacement(self) -> None:
        topology = self.root / "topology"
        topology.mkdir()
        source = self.root / "shared-content"
        source.mkdir()
        (source / "payload").write_text("shared\n", encoding="utf-8")
        atomic_json(
            source / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        external = self.root / "external"
        external.write_text("private\n", encoding="utf-8")
        real_copy = self.workspace._copy_topology_runtime_tree

        def copy_then_replace(source_path: Path, destination: Path) -> int:
            descriptor = real_copy(source_path, destination)
            destination.replace(destination.with_name("copied-original"))
            destination.mkdir()
            (destination / "payload").symlink_to(external)
            atomic_json(
                destination / MANAGED_MARKER,
                {"schema_version": 1, "purpose": "collected-content"},
            )
            return descriptor

        with mock.patch.object(
            self.workspace,
            "_copy_topology_runtime_tree",
            side_effect=copy_then_replace,
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "installed topology runtime input changed"
            ):
                self.workspace._copy_topology_runtime_inputs(
                    topology,
                    (("content", source, "collected-content"),),
                )

        self.assertEqual(list(topology.iterdir()), [])
        self.assertEqual(external.read_text(encoding="utf-8"), "private\n")

    def test_region_maps_are_atomic_cached_and_keyed_by_clean_inputs(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        command("git", "add", ".", cwd=source)
        command("git", "commit", "-m", "test: add runtime inputs", cwd=source)

        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        binary = root / "build" / "server"
        binary.mkdir(parents=True)
        executable = binary / "atrinik-server"
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "from pathlib import Path\n"
            "import os\n"
            "import sys\n"
            "binary = Path(__file__).resolve()\n"
            "counter = binary.with_name('worldmaker-count')\n"
            "count = int(counter.read_text()) + 1 if counter.exists() else 1\n"
            "counter.write_text(str(count))\n"
            "binary.with_name('worldmaker-bytecode').write_text("
            "os.environ.get('PYTHONDONTWRITEBYTECODE', ''))\n"
            "assets = Path(next(arg.split('=', 1)[1] for arg in sys.argv "
            "if arg.startswith('--assetspath=')))\n"
            "output = assets / 'client-maps'\n"
            "output.mkdir(parents=True)\n"
            "(output / 'incuna_-1.png').write_bytes(b'\\x89PNG\\r\\n\\x1a\\n')\n"
            "(output / 'incuna_-1.def').write_text('pixel_size 4\\n')\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            root / "runtime" / "content" / "lib",
            root / "runtime" / "content" / "maps",
            root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        selected = {
            role: self.workspace.paths.repositories / role
            for role in ("server", "content", "resources")
        }

        output = self.workspace._generate_region_maps(root, "default", selected)
        self.workspace._generate_region_maps(root, "default", selected)

        self.assertEqual((binary / "worldmaker-count").read_text(), "1")
        self.assertEqual((binary / "worldmaker-bytecode").read_text(), "1")
        self.assertTrue((output / "incuna_-1.png").is_file())
        previous = (output / "incuna_-1.def").read_text(encoding="utf-8")
        atomic_json(output / ".atrinik-region-maps.json", {"stale": True})

        def mutate_after_generation(
            arguments: list[str], **kwargs: object
        ) -> str:
            result = workspace_run(arguments, **kwargs)
            if Path(arguments[0]).resolve() == executable:
                (source / "README").write_text(
                    "dirty input\n", encoding="utf-8"
                )
            return result

        with mock.patch(
            "atrinik_workspace.workspace.run", side_effect=mutate_after_generation
        ):
            with self.assertRaisesRegex(WorkspaceError, "changed during generation"):
                self.workspace._generate_region_maps(root, "default", selected)
        self.assertEqual(
            (output / "incuna_-1.def").read_text(encoding="utf-8"), previous
        )

        executable.write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "command failed"):
            self.workspace._generate_region_maps(root, "default", selected)
        self.assertEqual(
            (output / "incuna_-1.def").read_text(encoding="utf-8"), previous
        )

    def test_region_map_inputs_ignore_unrelated_common_build_roles(self) -> None:
        profile = self.workspace._load_profile("default", require_file=False)
        required = self.workspace._dependency_roles(profile, {"server"})
        selected = {
            role: self.workspace.paths.repositories / role
            for role in required | {"client", "sound"}
        }

        inputs, cacheable = self.workspace._region_map_inputs("default", selected)

        self.assertTrue(cacheable)
        self.assertEqual(set(inputs["coordinates"]), required)

    def test_server_resource_inputs_keep_pre_sync_generation_identity(self) -> None:
        profile = self.workspace._load_profile("default", require_file=False)
        required = self.workspace._dependency_roles(profile, {"server"})
        stack = self.workspace.manifest.stack(profile["stack"])
        generated_checkouts = sorted(
            {
                stack.providers[role].checkout_name
                for role in required
                if role != "content"
            }
        )
        for checkout in generated_checkouts:
            seed = self.seeds[checkout]
            (seed / "sync-during-server-build").write_text(
                f"advanced {checkout}\n", encoding="utf-8"
            )
            command("git", "add", "sync-during-server-build", cwd=seed)
            command("git", "commit", "-m", "test: advance server input", cwd=seed)
            command("git", "push", "origin", "main", cwd=seed)

        with self.workspace._resolved_profile_operation(
            "default",
            {"server"},
            "build server",
            materialize_clean_primaries=True,
        ) as snapshot:
            selected = snapshot.paths()
            captured = snapshot.checkout_states()
            self.workspace.sync(generated_checkouts, "none")
            key = self.workspace._profile_build_key("default", selected)
            root = self.workspace.paths.builds / "profiles" / f"default-{key}"
            managed_directory(
                root,
                self.workspace.paths.builds,
                f"profile:default:{key}",
            )
            self.workspace._stage_resources(root, selected, "default")
            resource_inputs, resource_cacheable = (
                self.workspace._runtime_input_coordinates(
                    "default", selected, "resources"
                )
            )
            region_inputs, region_cacheable = self.workspace._region_map_inputs(
                "default", selected
            )

        self.assertTrue(resource_cacheable)
        self.assertEqual(
            resource_inputs["coordinate"]["head"],
            captured["resources"]["head"],
        )
        self.assertTrue(region_cacheable)
        for role, coordinate in region_inputs["coordinates"].items():
            checkout = stack.providers[role].checkout_name
            self.assertEqual(coordinate["head"], captured[checkout]["head"])

    def test_region_map_validation_rejects_malformed_outputs(self) -> None:
        output = self.root / "client-maps"

        def reset() -> None:
            if output.exists():
                shutil.rmtree(output)
            output.mkdir()

        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_region_maps(output)

        target = self.root / "map-target"
        target.mkdir()
        output.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "not a directory"):
            self.workspace._validate_region_maps(output)
        output.unlink()

        reset()
        with self.assertRaisesRegex(WorkspaceError, "lack required"):
            self.workspace._validate_region_maps(output)

        (output / "incuna_-1.def").write_text("pixel_size 4\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "pairs are incomplete"):
            self.workspace._validate_region_maps(output)

        reset()
        (output / "incuna_-1.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        with self.assertRaisesRegex(WorkspaceError, "missing definition"):
            self.workspace._validate_region_maps(output)

        (output / "incuna_-1.png").write_bytes(b"not a png")
        with self.assertRaisesRegex(WorkspaceError, "not a PNG"):
            self.workspace._validate_region_maps(output)

        (output / "incuna_-1.png").write_bytes(b"\x89PNG\r\n\x1a\n")
        (output / "incuna_-1.def").write_bytes(b"\xff")
        with self.assertRaisesRegex(WorkspaceError, "not UTF-8"):
            self.workspace._validate_region_maps(output)

        reset()
        (output / "unexpected").mkdir()
        with self.assertRaisesRegex(WorkspaceError, "output is invalid"):
            self.workspace._validate_region_maps(output)

        reset()
        (output / "incuna_-1.png").write_bytes(b"")
        (output / "incuna_-1.def").write_text("pixel_size 4\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "is empty"):
            self.workspace._validate_region_maps(output)

        reset()
        entry = mock.Mock()
        entry.name = "vanished.png"
        entry.lstat.side_effect = OSError("vanished")
        with mock.patch("pathlib.Path.iterdir", return_value=iter([entry])):
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect"):
                self.workspace._validate_region_maps(output)

    def test_region_map_cache_rejects_incomplete_or_invalid_metadata(self) -> None:
        output = self.make_region_map_cache(self.root)
        marker = output / MANAGED_MARKER
        metadata = output / ".atrinik-region-maps.json"
        inputs = {"schema_version": 1, "cacheable": True, "coordinates": {}}

        marker.unlink()
        self.assertFalse(
            self.workspace._region_map_cache_matches(output, inputs, True)
        )

        linked_output = self.root / "linked-maps"
        linked_output.symlink_to(output, target_is_directory=True)
        self.assertFalse(
            self.workspace._region_map_cache_matches(linked_output, inputs, True)
        )

        atomic_json(
            marker,
            {"schema_version": 1, "purpose": "region-map-cache"},
        )
        atomic_json(metadata, inputs)
        marker.write_text("{", encoding="utf-8")
        self.assertFalse(
            self.workspace._region_map_cache_matches(output, inputs, True)
        )

    def test_exclusive_lock_rejects_concurrent_nonblocking_user(self) -> None:
        lock = self.workspace.paths.builds / "locks" / "test.lock"
        with exclusive_lock(lock, "test resource"):
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(lock, "test resource", nonblocking=True):
                    self.fail("concurrent lock unexpectedly succeeded")

    def test_shared_lock_fails_closed_without_platform_primitive(self) -> None:
        lock = self.workspace.paths.builds / "locks" / "test.lock"
        with mock.patch.object(workspace_module.fcntl, "LOCK_SH", None):
            with self.assertRaisesRegex(
                WorkspaceError, "shared locking is unavailable"
            ):
                with shared_lock(lock, "test resource"):
                    self.fail("shared lock unexpectedly succeeded")

        with mock.patch.object(
            workspace_module.fcntl,
            "flock",
            side_effect=OSError(errno.ENOTSUP, "operation not supported"),
        ):
            with self.assertRaisesRegex(
                WorkspaceError, "cannot acquire test resource lock"
            ):
                with shared_lock(lock, "test resource"):
                    self.fail("unsupported shared lock unexpectedly succeeded")

        with mock.patch.object(locking_module.fcntl, "LOCK_SH", None):
            with self.assertRaisesRegex(
                WorkspaceError, "shared locking is unavailable"
            ):
                with shared_layout_lock(lock, "repository layout"):
                    self.fail("unsupported shared layout lock unexpectedly succeeded")

    def test_layout_writer_precedes_continuing_reader_arrivals(self) -> None:
        context = multiprocessing.get_context("spawn")
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        intent = workspace_module._layout_writer_intent_path(layout)
        pending = locking_module.layout_writer_pending_path(layout)
        entered = context.Queue()
        results = context.Queue()
        release_initial = context.Event()
        release_writer = context.Event()
        initial = context.Process(
            target=fair_layout_reader_process,
            args=(
                str(layout),
                "initial",
                None,
                entered,
                release_initial,
                results,
            )
        )
        writer = context.Process(
            target=fair_layout_writer_process,
            args=(str(layout), entered, release_writer, results),
        )
        arrival_attempts = [context.Event() for _ in range(8)]
        arrivals = [
            context.Process(
                target=fair_layout_reader_process,
                args=(
                    str(layout),
                    f"reader-{index}",
                    arrival_attempts[index],
                    entered,
                    None,
                    results,
                ),
            )
            for index in range(8)
        ]
        processes = [initial, writer, *arrivals]
        started: list[multiprocessing.Process] = []
        try:
            initial.start()
            started.append(initial)
            self.assertEqual(entered.get(timeout=5), "initial")
            with exclusive_lock(intent, "held reader admission"):
                writer.start()
                started.append(writer)
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline:
                    try:
                        with exclusive_lock(
                            pending,
                            "repository layout writer pending",
                            nonblocking=True,
                        ):
                            pass
                    except WorkspaceError:
                        break
                    time.sleep(0.005)
                else:
                    self.fail("writer did not publish its pending state")

                for arrival in arrivals:
                    arrival.start()
                    started.append(arrival)
                self.assertTrue(
                    all(attempt.wait(5) for attempt in arrival_attempts)
                )
                with self.assertRaises(queue.Empty):
                    entered.get(timeout=0.1)

            release_initial.set()
            self.assertEqual(entered.get(timeout=5), "writer")
            with self.assertRaises(queue.Empty):
                entered.get(timeout=0.1)
            release_writer.set()
            self.assertEqual(
                {entered.get(timeout=5) for _ in arrivals},
                {f"reader-{index}" for index in range(8)},
            )
        finally:
            release_initial.set()
            release_writer.set()
            join_or_stop_processes(started, 10)
        self.assertEqual(started, processes)
        self.assertEqual([process.exitcode for process in processes], [0] * 10)
        self.assertEqual([results.get(timeout=2) for _ in processes], [None] * 10)

    def test_exact_resource_matrix_scopes_conflicts_to_coordinates(self) -> None:
        context = multiprocessing.get_context("spawn")
        for kind in sorted(
            locking_module.RESOURCE_KIND_ORDER,
            key=locking_module.RESOURCE_KIND_ORDER.__getitem__,
        ):
            with self.subTest(kind=kind):
                entered = context.Queue()
                results = context.Queue()
                release_reader = context.Event()
                release_writers = context.Event()
                writer_attempting = context.Event()
                writer_a_entered = context.Event()
                writer_b_entered = context.Event()
                coordinate_a = f"{kind}:a"
                coordinate_b = f"{kind}:b"
                lease_root = self.workspace._lease_root(
                    LeaseRequest(kind, coordinate_a, "shared", "matrix", "wait")
                )
                reader = context.Process(
                    target=resource_lease_process,
                    args=(
                        str(lease_root),
                        kind,
                        coordinate_a,
                        "shared",
                        "reader-a",
                        entered,
                        release_reader,
                        results,
                    ),
                )
                writer_a = context.Process(
                    target=resource_lease_process,
                    args=(
                        str(lease_root),
                        kind,
                        coordinate_a,
                        "exclusive",
                        "writer-a",
                        entered,
                        release_writers,
                        results,
                        writer_attempting,
                        writer_a_entered,
                    ),
                )
                writer_b = context.Process(
                    target=resource_lease_process,
                    args=(
                        str(lease_root),
                        kind,
                        coordinate_b,
                        "exclusive",
                        "writer-b",
                        entered,
                        release_writers,
                        results,
                        None,
                        writer_b_entered,
                    ),
                )
                processes = [reader, writer_a, writer_b]
                started: list[multiprocessing.Process] = []
                try:
                    reader.start()
                    started.append(reader)
                    self.assertEqual(entered.get(timeout=5), "reader-a")
                    writer_a.start()
                    started.append(writer_a)
                    self.assertTrue(writer_attempting.wait(5))
                    pending = locking_module.layout_writer_pending_path(
                        resource_lock_path(lease_root, kind, coordinate_a)
                    )
                    deadline = time.monotonic() + 5
                    while time.monotonic() < deadline:
                        try:
                            with exclusive_lock(
                                pending,
                                f"{kind} writer pending observation",
                                nonblocking=True,
                            ):
                                pass
                        except locking_module.LockBusyError:
                            break
                        time.sleep(0.005)
                    else:
                        self.fail(f"{kind} writer did not publish pending state")

                    writer_b.start()
                    started.append(writer_b)
                    self.assertTrue(writer_b_entered.wait(5))
                    self.assertFalse(writer_a_entered.is_set())
                    release_reader.set()
                    self.assertTrue(writer_a_entered.wait(5))
                finally:
                    release_reader.set()
                    release_writers.set()
                    join_or_stop_processes(started, 10)
                self.assertEqual(started, processes)
                self.assertEqual(
                    [process.exitcode for process in processes], [0, 0, 0]
                )
                self.assertEqual(
                    [results.get(timeout=2) for _ in processes], [None] * 3
                )

    def test_incremental_harness_isolates_live_topology_conflicts(self) -> None:
        context = multiprocessing.get_context("spawn")
        source_a = self.workspace.create_worktree(
            "client", "source-a", "test/source-a", None, False
        )
        self.workspace.create_worktree(
            "client", "source-c", "test/source-c", None, False
        )
        self.workspace.create_worktree(
            "sound", "source-c", "test/sound-source-c", None, False
        )
        for profile in ("profile-a", "profile-c"):
            self.workspace.create_profile(profile)
        self.workspace.set_profile(
            "profile-a", "client", "worktree", "source-a"
        )
        self.workspace.set_profile(
            "profile-c", "client", "worktree", "source-c"
        )
        self.workspace.set_profile(
            "profile-c", "sound", "worktree", "source-c"
        )

        def client_build_root(profile: str) -> Path:
            root = self.workspace.paths.builds / "profiles" / profile
            executable = root / "build" / "client" / "atrinik"
            executable.parent.mkdir(parents=True)
            (root / "sources" / "client").mkdir(parents=True)
            executable.write_text(
                "#!/usr/bin/env python3\n"
                "import time\n"
                "print('client ready', flush=True)\n"
                "while True:\n"
                "    time.sleep(0.1)\n",
                encoding="utf-8",
            )
            executable.chmod(0o755)
            selected_sound = self.workspace.resolve_profile(
                profile, {"sound"}
            )["sound"]
            atomic_json(
                root / workspace_module.BUILD_METADATA,
                {"sound": workspace_module.sound_source_record(selected_sound)},
            )
            return root

        topology_a_root = client_build_root("profile-a")
        topology_c_root = client_build_root("profile-c")

        def stop_test_topologies() -> None:
            failures = []
            for name in ("topology-a", "topology-c"):
                root = self.workspace.paths.topologies / name
                status_path = root / "status.json"
                process_tree_path = (
                    root / workspace_module.TOPOLOGY_PROCESS_TREE_LEASE
                )
                down_error = None
                if status_path.is_file():
                    try:
                        self.workspace.topology_down(name, timeout=5)
                    except WorkspaceError as error:
                        down_error = error
                if process_tree_path.is_file() and not process_tree_path.is_symlink():
                    descriptor = workspace_module.open_regular_file(
                        process_tree_path,
                        os.O_PATH,
                        "test topology process-tree lease",
                    )
                    try:
                        if workspace_module.holders_exist(
                            descriptor, exclude=(os.getpid(),)
                        ):
                            workspace_module.signal_holders(
                                descriptor, signal.SIGTERM, exclude=(os.getpid(),)
                            )
                            deadline = time.monotonic() + 5
                            while (
                                time.monotonic() < deadline
                                and workspace_module.holders_exist(
                                    descriptor, exclude=(os.getpid(),)
                                )
                            ):
                                time.sleep(0.05)
                        if workspace_module.holders_exist(
                            descriptor, exclude=(os.getpid(),)
                        ):
                            workspace_module.signal_holders(
                                descriptor, signal.SIGKILL, exclude=(os.getpid(),)
                            )
                            deadline = time.monotonic() + 2
                            while (
                                time.monotonic() < deadline
                                and workspace_module.holders_exist(
                                    descriptor, exclude=(os.getpid(),)
                                )
                            ):
                                time.sleep(0.05)
                        if workspace_module.holders_exist(
                            descriptor, exclude=(os.getpid(),)
                        ):
                            failures.append(f"{name}: process tree remains active")
                    finally:
                        os.close(descriptor)
                elif down_error is not None:
                    failures.append(f"{name}: {down_error}")
            if failures:
                self.fail("cannot stop test topologies: " + "; ".join(failures))

        self.addCleanup(stop_test_topologies)
        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=topology_a_root
            ),
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            topology_a = self.workspace.topology_up(
                "topology-a", "profile-a", "default", ["client"]
            )
        self.assertTrue(topology_a["ready"])
        generation_root = Path(topology_a["runtime"]["path"])
        generation_digest = _tree_digest(
            generation_root, frozenset(), reject_symlinks=True
        )
        (source_a / "post-publication.txt").write_text(
            "changed source\n", encoding="utf-8"
        )
        (topology_a_root / "build" / "client" / "atrinik").write_text(
            "#!/bin/sh\nexit 1\n", encoding="utf-8"
        )
        self.assertEqual(
            _tree_digest(generation_root, frozenset(), reject_symlinks=True),
            generation_digest,
        )
        self.assertTrue(self.workspace.topology_status("topology-a")["ready"])

        coordinate_a = "profile-a"
        lease_root = self.workspace._lease_root(
            LeaseRequest("profile", coordinate_a, "shared", "matrix", "wait")
        )
        exact_entered = context.Queue()
        release_initial = context.Event()
        release_writer = context.Event()
        release_late_reader = context.Event()
        writer_attempting = context.Event()
        late_reader_attempting = context.Event()
        writer_entered = context.Event()
        late_reader_entered = context.Event()
        readers_blocked = [context.Event(), context.Event()]
        readers_entered = [context.Event(), context.Event()]
        results = context.Queue()
        initial_reader = context.Process(
            target=resource_lease_process,
            args=(
                str(lease_root),
                "profile",
                coordinate_a,
                "shared",
                "initial-reader-a",
                exact_entered,
                release_initial,
                results,
            ),
        )
        writer = context.Process(
            target=public_profile_mutation_process,
            args=(
                str(self.wrapper),
                str(self.workspace_directory),
                writer_attempting,
                writer_entered,
                release_writer,
                results,
            ),
        )
        late_reader = context.Process(
            target=public_profile_reader_process,
            args=(
                str(self.wrapper),
                str(self.workspace_directory),
                str(topology_a_root),
                late_reader_attempting,
                late_reader_entered,
                release_late_reader,
                results,
            ),
        )
        readers = [
            context.Process(
                target=public_lifecycle_reader_process,
                args=(
                    str(self.wrapper),
                    str(self.workspace_directory),
                    operation,
                    str(topology_c_root),
                    readers_blocked[index],
                    readers_entered[index],
                    results,
                ),
            )
            for index, operation in enumerate(("build-c", "topology-c"))
        ]
        processes = [initial_reader, writer, late_reader, *readers]
        started: list[multiprocessing.Process] = []
        try:
            initial_reader.start()
            started.append(initial_reader)
            self.assertEqual(exact_entered.get(timeout=5), "initial-reader-a")
            writer.start()
            started.append(writer)
            self.assertTrue(writer_attempting.wait(5))
            pending = locking_module.layout_writer_pending_path(
                resource_lock_path(lease_root, "profile", coordinate_a)
            )
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                try:
                    with exclusive_lock(
                        pending,
                        "profile A writer pending observation",
                        nonblocking=True,
                    ):
                        pass
                except locking_module.LockBusyError:
                    break
                time.sleep(0.005)
            else:
                self.fail("profile A writer did not publish pending state")

            late_reader.start()
            started.append(late_reader)
            self.assertTrue(late_reader_attempting.wait(5))
            self.assertFalse(writer_entered.is_set())
            self.assertFalse(late_reader_entered.is_set())

            for reader in readers:
                reader.start()
                started.append(reader)
            for index, entered in enumerate(readers_entered):
                wait_for_process_event(
                    entered, f"disjoint C reader {index} completion", results, 10
                )
            self.assertTrue(self.workspace.topology_status("topology-a")["ready"])
            release_initial.set()
            self.assertTrue(writer_entered.wait(5))
            self.assertFalse(late_reader_entered.is_set())
            release_writer.set()
            self.assertTrue(late_reader_entered.wait(5))
            release_late_reader.set()

            # The immutable topology remains live while a real profile A
            # mutation gates only A and disjoint build/topology C completes.
            self.workspace.topology_down("topology-a", timeout=5)
        finally:
            release_initial.set()
            release_writer.set()
            release_late_reader.set()
            join_or_stop_processes(started, 10)
            stop_test_topologies()
        self.assertEqual(started, processes)
        self.assertEqual([process.exitcode for process in processes], [0] * 5)
        self.assertEqual([results.get(timeout=2) for _ in processes], [None] * 5)

    def test_distinct_server_ports_reach_pre_ready_concurrently(
        self,
    ) -> None:
        context = multiprocessing.get_context("spawn")
        results = context.Queue()
        reservation_received = [context.Event(), context.Event()]
        release_reservation = [context.Event(), context.Event()]
        port_blocked = [context.Event(), context.Event()]
        pre_ready = [self.root / "pre-ready-a", self.root / "pre-ready-b"]
        releases = [self.root / "release-a", self.root / "release-b"]

        def reserved_port() -> socket.socket:
            candidate = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            candidate.bind(("0.0.0.0", 0))
            return candidate

        reservations = [reserved_port(), reserved_port()]
        coordinates = (
            ("server-a", "state-a", "build-a", reservations[0].getsockname()[1]),
            ("server-b", "state-b", "build-b", reservations[1].getsockname()[1]),
        )
        self.assertNotEqual(coordinates[0][3], coordinates[1][3])
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        for index, (name, _state, build, _port) in enumerate(coordinates):
            self.workspace.create_profile(name)
            root = self.workspace.paths.builds / "profiles" / build
            root.mkdir(parents=True)
            atomic_json(root / workspace_module.BUILD_METADATA, {})
            binary = root / "build" / "server"
            binary.mkdir(parents=True)
            executable = binary / "atrinik-server"
            executable.write_text(
                "#!/usr/bin/env python3\n"
                "from pathlib import Path\n"
                "import socket, sys, time\n"
                "port = int(next(value.split('=', 1)[1] for value in sys.argv "
                "if value.startswith('--port_quic=')))\n"
                "server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)\n"
                "server.bind(('127.0.0.1', port))\n"
                f"Path({str(pre_ready[index])!r}).write_text('ready\\n')\n"
                f"release = Path({str(releases[index])!r})\n"
                "while not release.exists():\n"
                "    time.sleep(0.01)\n"
                "print('QUIC certificate SHA-256: ' + 'a' * 64, flush=True)\n"
                "print('Server ready. Waiting for connections...', flush=True)\n"
                "while True:\n"
                "    time.sleep(0.1)\n",
                encoding="utf-8",
            )
            executable.chmod(0o755)
            for library in ("libplugin_arena.so", "libplugin_python.so"):
                (binary / library).write_text("test\n", encoding="utf-8")
            for path in (
                root / "runtime" / "content" / "lib",
                root / "runtime" / "content" / "maps",
                root / "runtime" / "resources",
            ):
                path.mkdir(parents=True, exist_ok=True)
            self.make_region_map_cache(root)
        processes = [
            context.Process(
                target=synthetic_server_start_process,
                args=(
                    str(self.wrapper),
                    str(self.workspace_directory),
                    name,
                    str(self.workspace.paths.state / state),
                    str(self.workspace.paths.builds / "profiles" / build),
                    port,
                    reservations[index],
                    reservation_received[index],
                    release_reservation[index],
                    port_blocked[index],
                    results,
                ),
            )
            for index, (name, state, build, port) in enumerate(coordinates)
        ]
        started: list[multiprocessing.Process] = []

        def wait_for_path(path: Path, description: str) -> None:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if path.is_file():
                    return
                time.sleep(0.01)
            child_errors = []
            while True:
                try:
                    child_errors.append(results.get_nowait())
                except queue.Empty:
                    break
            self.fail(f"{description} was not reached; children={child_errors}")

        try:
            for process in processes:
                process.start()
                started.append(process)
            for index, received in enumerate(reservation_received):
                wait_for_process_event(
                    received,
                    f"server {index} port reservation transfer",
                    results,
                )
                reservations[index].close()
            for event in release_reservation:
                event.set()
            wait_for_path(pre_ready[0], "server A pre-ready barrier")
            wait_for_path(pre_ready[1], "server B pre-ready barrier")
            self.assertTrue(all(not blocked.is_set() for blocked in port_blocked))

            # Distinct explicit ports, build roots, states, and topology names
            # reach the server pre-ready barrier concurrently. Neither waits on
            # the automatic allocator or the other generation's owner lease.
            for release in releases:
                release.write_text("release\n", encoding="utf-8")
        finally:
            for reservation in reservations:
                reservation.close()
            for event in release_reservation:
                event.set()
            for release in releases:
                release.write_text("release\n", encoding="utf-8")
            join_or_stop_processes(started, 10)
        self.assertEqual(started, processes)
        self.assertEqual([process.exitcode for process in processes], [0, 0])
        self.assertEqual([results.get(timeout=2) for _ in processes], [None, None])

    def test_profile_reference_registry_rejects_symlink(self) -> None:
        target = self.root / "redirected-references"
        target.mkdir()
        registry = self.workspace._lease_namespace / "profile-references"
        shutil.rmtree(registry)
        registry.symlink_to(target, target_is_directory=True)

        with self.assertRaisesRegex(
            WorkspaceError, "cannot publish physical reference"
        ):
            self.workspace.create_profile("unsafe-registry")

        self.assertEqual(list(target.iterdir()), [])

    def test_physical_lease_namespace_replacement_fails_closed(self) -> None:
        namespace = self.workspace._lease_namespace
        detached = namespace.with_name("detached-atrinik-resource-leases")
        namespace.rename(detached)
        namespace.mkdir(mode=0o700)
        try:
            with self.assertRaisesRegex(
                WorkspaceError, "physical lease namespace identity changed"
            ):
                self.workspace.create_profile("split-brain")
        finally:
            shutil.rmtree(namespace)
            detached.rename(namespace)

    def test_physical_reference_rollback_rejects_namespace_replacement(self) -> None:
        profile = self.workspace.create_profile("rollback-namespace")
        namespace = self.workspace._lease_namespace
        detached = namespace.with_name("detached-rollback-namespace")
        real_open = os.open
        replaced = False

        def replace_before_open(path: object, *args: object, **kwargs: object) -> int:
            nonlocal replaced
            if Path(path) == namespace and not replaced:
                replaced = True
                namespace.rename(detached)
                namespace.mkdir(mode=0o700)
            return real_open(path, *args, **kwargs)

        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.os.open",
                    side_effect=replace_before_open,
                ),
                self.assertRaisesRegex(
                    WorkspaceError, "physical lease namespace identity changed"
                ),
            ):
                self.workspace._remove_physical_reference(profile)
        finally:
            shutil.rmtree(namespace)
            detached.rename(namespace)

    def test_profile_reference_publication_rejects_registry_replacement(self) -> None:
        registry = self.workspace._lease_namespace / "profile-references"
        detached = registry.with_name("detached-profile-references")
        real_rename = os.rename

        def replace_after_publish(*arguments: object, **keywords: object) -> None:
            real_rename(*arguments, **keywords)
            real_rename(registry, detached)
            registry.mkdir(mode=0o700)

        with (
            mock.patch("atrinik_workspace.workspace.os.rename", side_effect=replace_after_publish),
            self.assertRaisesRegex(WorkspaceError, "registry was replaced"),
        ):
            self.workspace.create_profile("replaced-registry")

        self.assertFalse(
            (self.workspace.paths.profiles / "replaced-registry.json").exists()
        )

    def test_first_physical_reference_persists_registry_before_record(self) -> None:
        namespace = self.workspace._lease_namespace
        registry = namespace / "profile-references"
        shutil.rmtree(registry)
        namespace_identity = namespace.stat()
        namespace_key = (namespace_identity.st_dev, namespace_identity.st_ino)
        fsynced: list[tuple[int, int]] = []
        real_fsync = os.fsync

        def observe(descriptor: int) -> None:
            metadata = os.fstat(descriptor)
            if stat.S_ISDIR(metadata.st_mode):
                fsynced.append((metadata.st_dev, metadata.st_ino))
            real_fsync(descriptor)

        with mock.patch(
            "atrinik_workspace.workspace.os.fsync", side_effect=observe
        ):
            self.workspace.create_profile("durable-registry")

        registry_identity = registry.stat()
        registry_key = (registry_identity.st_dev, registry_identity.st_ino)
        self.assertIn(namespace_key, fsynced)
        self.assertIn(registry_key, fsynced)
        self.assertLess(fsynced.index(namespace_key), fsynced.index(registry_key))

    def test_physical_reference_retry_repersists_existing_registry(self) -> None:
        namespace = self.workspace._lease_namespace
        registry = namespace / "profile-references"
        shutil.rmtree(registry)
        namespace_identity = namespace.stat()
        namespace_key = (namespace_identity.st_dev, namespace_identity.st_ino)
        real_fsync = os.fsync
        namespace_fsyncs = 0

        def fail_first_namespace_fsync(descriptor: int) -> None:
            nonlocal namespace_fsyncs
            metadata = os.fstat(descriptor)
            if (metadata.st_dev, metadata.st_ino) == namespace_key:
                namespace_fsyncs += 1
                if namespace_fsyncs == 1:
                    raise OSError("simulated namespace fsync failure")
            real_fsync(descriptor)

        with mock.patch(
            "atrinik_workspace.workspace.os.fsync",
            side_effect=fail_first_namespace_fsync,
        ):
            with self.assertRaisesRegex(WorkspaceError, "cannot publish"):
                self.workspace.create_profile("retry-registry")
            self.workspace.create_profile("retry-registry")

        self.assertGreaterEqual(namespace_fsyncs, 2)
        self.assertTrue(
            (self.workspace.paths.profiles / "retry-registry.json").is_file()
        )

    def test_profile_creation_failure_removes_prepublished_reference(self) -> None:
        path = self.workspace.paths.profiles / "failed-create.json"
        identity = hashlib.sha256(str(path.resolve()).encode()).hexdigest()
        with (
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json",
                side_effect=WorkspaceError("simulated profile write failure"),
            ),
            self.assertRaisesRegex(WorkspaceError, "simulated"),
        ):
            self.workspace.create_profile("failed-create")
        self.assertFalse(path.exists())
        self.assertFalse(
            (
                self.workspace._lease_namespace
                / "profile-references"
                / f"{identity}.json"
            ).exists()
        )

    def test_profile_creation_keeps_reference_when_directory_fsync_is_uncertain(self) -> None:
        path = self.workspace.paths.profiles / "uncertain-create.json"
        identity = hashlib.sha256(str(path.resolve()).encode()).hexdigest()
        def uncertain_write(target: Path, value: object) -> None:
            atomic_json(target, value)
            raise workspace_module.AtomicJsonCommitUncertain(
                "simulated durability is uncertain"
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json",
                side_effect=uncertain_write,
            ),
            self.assertRaisesRegex(WorkspaceError, "durability is uncertain"),
        ):
            self.workspace.create_profile("uncertain-create")

        self.assertTrue(path.is_file())
        self.assertTrue(
            (
                self.workspace._lease_namespace
                / "profile-references"
                / f"{identity}.json"
            ).is_file()
        )

    def test_backfill_retries_without_publishing_when_source_is_busy(self) -> None:
        source = self.workspace.create_worktree(
            "content", "backfill-busy", "feat/backfill-busy", None, False
        )
        profile = self.workspace.create_profile("backfill-busy")
        self.workspace.set_profile("backfill-busy", "content", "path", str(source))
        registry = self.workspace._lease_namespace / "profile-references"
        profile_identity = hashlib.sha256(str(profile.resolve()).encode()).hexdigest()
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        (registry / f"{profile_identity}.json").unlink()
        (registry / f"{state_identity}.json").unlink()
        source_request = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("content", source),
            "exclusive",
            "remove selected source",
        )
        with resource_locks(self.workspace._lease_root, [source_request]):
            with self.assertRaisesRegex(
                WorkspaceError,
                rf"source content:{re.escape(str(source))} is already in use by "
                r"exclusive remove selected source by .*; "
                r"inspect `\./atrinik worktree list --json` and retry",
            ):
                self.workspace._backfill_physical_references()
        self.assertFalse((registry / f"{profile_identity}.json").exists())
        self.assertFalse((registry / f"{state_identity}.json").exists())

        self.workspace._backfill_physical_references()
        self.assertTrue((registry / f"{profile_identity}.json").is_file())
        self.assertTrue((registry / f"{state_identity}.json").is_file())

    def test_backfill_preserves_missing_profile_as_historical_reference(self) -> None:
        source = self.workspace.create_worktree(
            "content", "missing-profile", "feat/missing-profile", None, False
        )
        profile = self.workspace.create_profile("missing-profile")
        self.workspace.set_profile("missing-profile", "content", "path", str(source))
        authored = profile.read_bytes()
        registry = self.workspace._lease_namespace / "profile-references"
        profile_identity = hashlib.sha256(str(profile.resolve()).encode()).hexdigest()
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        (registry / f"{profile_identity}.json").unlink()
        (registry / f"{state_identity}.json").unlink()
        shutil.rmtree(source)

        fresh = Workspace(self.wrapper)
        try:
            self.assertTrue(fresh.repository_status(["content"]))
            fresh.sync(["content"], "none")
            self.assertEqual(profile.read_bytes(), authored)
            record = load_json(registry / f"{profile_identity}.json")
            self.assertEqual(record["sources"], [str(source.resolve())])
            self.assertTrue((registry / f"{state_identity}.json").is_file())
        finally:
            fresh.close()

    def test_relocated_backfill_and_removal_share_missing_source_coordinate(
        self,
    ) -> None:
        source = self.workspace.create_worktree(
            "content", "relocated-missing", "feat/relocated-missing", None, False
        )
        alternate_root = self.root / "alternate-workspace"
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(alternate_root)}
        ):
            alternate = Workspace(self.wrapper)
            alternate.paths.ensure()
            profile = alternate.create_profile("relocated-missing")
            alternate.set_profile(
                "relocated-missing", "content", "path", str(source)
            )
            alternate.close()

        registry = self.workspace._lease_namespace / "profile-references"
        profile_identity = hashlib.sha256(str(profile.resolve()).encode()).hexdigest()
        state_identity = hashlib.sha256(str(alternate_root.resolve()).encode()).hexdigest()
        (registry / f"{profile_identity}.json").unlink()
        (registry / f"{state_identity}.json").unlink()
        shutil.rmtree(source)
        removal = self.workspace._lease_request(
            "source",
            self.workspace._source_coordinate("content", source),
            "exclusive",
            "remove selected source",
        )
        with resource_locks(self.workspace._lease_root, [removal]):
            with (
                mock.patch.dict(
                    os.environ, {"ATRINIK_WORKSPACE_DIR": str(alternate_root)}
                ),
                self.assertRaisesRegex(
                    WorkspaceError,
                    rf"source content:{re.escape(str(source))} is already in use by "
                    r"exclusive remove selected source by",
                ),
            ):
                Workspace(self.wrapper)

        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(alternate_root)}
        ):
            fresh = Workspace(self.wrapper)
            fresh.close()
        source.mkdir(parents=True)
        with resource_locks(self.workspace._lease_root, [removal]):
            self.assertIn(
                "profile:relocated-missing",
                self.workspace._source_references(source),
            )

    def test_relocated_scenario_backfill_blocks_removal_until_complete(self) -> None:
        sources = [
            self.workspace.create_worktree(
                "content",
                f"retired-scenario-{index}",
                f"feat/retired-scenario-{index}",
                None,
                False,
            )
            for index in range(2)
        ]
        alternate_root = self.root / "alternate-scenario-workspace"
        scenario = alternate_root / "scenarios" / "retired-scenario"
        scenario.mkdir(parents=True)
        record = scenario / "scenario.json"
        atomic_json(
            record,
            {
                "resolved": {
                    f"server-{index}": {
                        "checkout": "retired-server-owner",
                        "checkout_path": str(source),
                    }
                    for index, source in enumerate(sources)
                }
            },
        )
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(alternate_root)}
        ):
            fresh = Workspace(self.wrapper, backfill_references=False)
        entered = threading.Event()
        release = threading.Event()
        removal_reached_fence = threading.Event()
        publish = fresh._publish_scenario_references
        lock_resources = self.workspace._resource_locks

        def pause_publication(name: str, metadata: dict[str, object]) -> None:
            entered.set()
            self.assertTrue(release.wait(5))
            publish(name, metadata)

        def observe_removal_fence(
            requests: list[LeaseRequest], *args: object, **kwargs: object
        ) -> object:
            if any(
                request.kind == "registry"
                and request.coordinate == "physical-references"
                and request.mode == "exclusive"
                for request in requests
            ):
                removal_reached_fence.set()
            return lock_resources(requests, *args, **kwargs)

        try:
            with (
                mock.patch.object(
                    fresh,
                    "_publish_scenario_references",
                    side_effect=pause_publication,
                ),
                mock.patch.object(
                    self.workspace,
                    "_resource_locks",
                    side_effect=observe_removal_fence,
                ),
                ThreadPoolExecutor(max_workers=2) as executor,
            ):
                backfill = executor.submit(fresh._backfill_physical_references)
                self.assertTrue(entered.wait(5))
                removing = executor.submit(
                    self.workspace.remove_worktree,
                    "content",
                    "retired-scenario-1",
                )
                self.assertTrue(removal_reached_fence.wait(5))
                self.assertFalse(removing.done())
                release.set()
                backfill.result(timeout=5)
                with self.assertRaisesRegex(
                    WorkspaceError,
                    r"refusing to remove referenced worktree .*: "
                    r"scenario:retired-scenario$",
                ):
                    removing.result(timeout=5)
            self.assertTrue(sources[1].is_dir())
            reference = load_json(
                fresh._lease_namespace
                / "profile-references"
                / f"{hashlib.sha256(str(record.resolve()).encode()).hexdigest()}.json"
            )
            self.assertEqual(reference["sources"], sorted(map(str, sources)))
        finally:
            release.set()
            fresh.close()

    def test_scenario_backfill_does_not_cross_product_historical_owners(self) -> None:
        root = self.workspace.paths.scenarios / "bounded-historical"
        root.mkdir()
        historical_count = 341
        atomic_json(
            root / "scenario.json",
            {
                "resolved": {
                    f"role-{index}": {
                        "checkout": f"retired-owner-{index}",
                        "checkout_path": str(self.root / f"missing-{index}"),
                    }
                    for index in range(historical_count)
                }
            },
        )
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        registry = self.workspace._lease_namespace / "profile-references"
        (registry / f"{state_identity}.json").unlink()
        observed: list[int] = []
        real_locks = workspace_module.resource_locks

        def record_requests(*args: object, **kwargs: object) -> object:
            requests = args[1]
            if any(request.kind == "scenario" for request in requests):
                observed.append(len(requests))
            return real_locks(*args, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.resource_locks",
            side_effect=record_requests,
        ):
            self.workspace._backfill_physical_references()

        self.assertEqual(
            observed,
            [1],
        )

    def test_backfill_preserves_missing_scenario_as_historical_reference(self) -> None:
        root = self.workspace.paths.scenarios / "missing-scenario"
        root.mkdir()
        missing = self.workspace.paths.worktrees / "server" / "missing-scenario"
        record = root / "scenario.json"
        atomic_json(
            record,
            {
                "resolved": {
                    "server": {
                        "checkout": "server",
                        "checkout_path": str(missing),
                    }
                }
            },
        )
        authored = record.read_bytes()
        registry = self.workspace._lease_namespace / "profile-references"
        scenario_identity = hashlib.sha256(str(record.resolve()).encode()).hexdigest()
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        (registry / f"{state_identity}.json").unlink()

        fresh = Workspace(self.wrapper)
        try:
            self.assertEqual(record.read_bytes(), authored)
            reference = load_json(registry / f"{scenario_identity}.json")
            self.assertEqual(reference["sources"], [str(missing.resolve())])
            self.assertTrue((registry / f"{state_identity}.json").is_file())
        finally:
            fresh.close()

    def test_backfill_rejects_malformed_scenario_without_marker(self) -> None:
        root = self.workspace.paths.scenarios / "malformed-backfill"
        root.mkdir()
        atomic_json(
            root / "scenario.json",
            {
                "resolved": {
                    "server": {
                        "checkout": "server",
                        "checkout_path": str(self.root / "retained-server"),
                    },
                    "invalid": {"checkout": "retired-owner"},
                }
            },
        )
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        registry = self.workspace._lease_namespace / "profile-references"
        (registry / f"{state_identity}.json").unlink()

        with self.assertRaisesRegex(
            WorkspaceError,
            "scenario resolved references are invalid: malformed-backfill",
        ):
            self.workspace._backfill_physical_references()

        self.assertFalse((registry / f"{state_identity}.json").exists())

    def test_backfill_reports_authored_record_change_separately(self) -> None:
        profile = self.workspace.create_profile("changing-backfill")
        registry = self.workspace._lease_namespace / "profile-references"
        profile_identity = hashlib.sha256(str(profile.resolve()).encode()).hexdigest()
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        (registry / f"{profile_identity}.json").unlink()
        (registry / f"{state_identity}.json").unlink()
        real_load = workspace_module.load_regular_json
        reads = 0

        def changed_on_confirmation(path: Path, description: str) -> object:
            nonlocal reads
            value = real_load(path, description)
            if path == profile:
                reads += 1
                if reads == 2:
                    return {**value, "name": "changed-directly"}
            return value

        with (
            mock.patch(
                "atrinik_workspace.workspace.load_regular_json",
                side_effect=changed_on_confirmation,
            ),
            self.assertRaisesRegex(
                WorkspaceError,
                "profile changed during physical reference backfill: "
                "changing-backfill; stop editing that profile and retry",
            ),
        ):
            self.workspace._backfill_physical_references()
        self.assertFalse((registry / f"{profile_identity}.json").exists())
        self.assertFalse((registry / f"{state_identity}.json").exists())

    def test_backfill_rejects_inexact_marker_schema(self) -> None:
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        marker = (
            self.workspace._lease_namespace
            / "profile-references"
            / f"{state_identity}.json"
        )
        atomic_json(
            marker,
            {
                "kind": "profiles",
                "reference": f"__backfill__:{state_identity}",
                "sources": [],
                "unexpected": True,
            },
        )

        with self.assertRaisesRegex(WorkspaceError, "backfill marker is invalid"):
            self.workspace._backfill_physical_references()

    def test_concurrent_backfill_rechecks_marker_after_serialization(self) -> None:
        registry = self.workspace._lease_namespace / "profile-references"
        state_identity = hashlib.sha256(
            str(self.workspace.paths.workspace.resolve()).encode()
        ).hexdigest()
        (registry / f"{state_identity}.json").unlink()
        first = Workspace(self.wrapper, backfill_references=False)
        second = Workspace(self.wrapper, backfill_references=False)
        entered = threading.Event()
        release = threading.Event()
        second_started_work = threading.Event()
        first_backfill = first._backfill_scenario_references

        def pause_first_backfill() -> None:
            entered.set()
            self.assertTrue(release.wait(5))
            first_backfill()

        def observe_second_work() -> None:
            second_started_work.set()

        try:
            with (
                mock.patch.object(
                    first,
                    "_backfill_scenario_references",
                    side_effect=pause_first_backfill,
                ),
                mock.patch.object(
                    second,
                    "_backfill_profile_references",
                    side_effect=observe_second_work,
                ),
                ThreadPoolExecutor(max_workers=2) as executor,
            ):
                first_result = executor.submit(first._backfill_physical_references)
                self.assertTrue(entered.wait(5))
                second_result = executor.submit(second._backfill_physical_references)
                time.sleep(0.05)
                self.assertFalse(second_result.done())
                self.assertFalse(second_started_work.is_set())
                release.set()
                first_result.result(timeout=5)
                second_result.result(timeout=5)
            self.assertFalse(second_started_work.is_set())
            self.assertTrue((registry / f"{state_identity}.json").is_file())
        finally:
            release.set()
            first.close()
            second.close()

    def test_profile_json_rejects_duplicate_keys_without_following_links(self) -> None:
        path = self.workspace.paths.profiles / "duplicate.json"
        path.write_text(
            '{"schema_version":4,"name":"duplicate","name":"other"}\n',
            encoding="utf-8",
        )
        with self.assertRaisesRegex(WorkspaceError, "duplicate JSON key"):
            self.workspace._load_profile_file("duplicate", require_file=True)

    def test_migration_reference_publisher_accepts_legacy_profile_shapes(self) -> None:
        content_source = self.workspace.paths.worktrees / "content-1x" / "maps"
        content_source.mkdir(parents=True)
        current = self.workspace._load_profile("classic", require_file=False)
        legacy_content = copy.deepcopy(current)
        legacy_content["name"] = "legacy-content"
        legacy_content["components"]["content-1x"] = {
            "kind": "worktree",
            "value": "maps",
        }
        legacy_content["components"].pop("content", None)
        content_path = self.workspace.paths.profiles / "legacy-content.json"
        atomic_json(content_path, legacy_content)

        old_source = self.workspace.paths.worktrees / "client" / "review"
        old_source.mkdir(parents=True)
        legacy_repository = {
            "schema_version": 1,
            "name": "legacy-repository",
            "components": {
                "client": {"kind": "worktree", "value": "review"}
            },
        }
        repository_path = self.workspace.paths.profiles / "legacy-repository.json"
        atomic_json(repository_path, legacy_repository)

        self.workspace._publish_migration_profile_references(
            {
                "legacy-content": (
                    json.dumps(legacy_content).encode(),
                    json.dumps(current | {"name": "legacy-content"}).encode(),
                ),
                "legacy-repository": (
                    json.dumps(legacy_repository).encode(),
                    json.dumps(legacy_repository).encode(),
                ),
            }
        )

        records = {
            record["reference"]: record
            for record in self.workspace._physical_reference_records()
        }
        self.assertIn(str(content_source.resolve()), records["legacy-content"]["sources"])
        self.assertIn(str(old_source.resolve()), records["legacy-repository"]["sources"])

    def test_profile_write_failure_retains_old_physical_reference(self) -> None:
        path = self.workspace.create_worktree(
            "content", "old-reference", "feat/old-reference", None, False
        )
        self.workspace.create_profile("write-failure")
        self.workspace.set_profile(
            "write-failure", "content", "path", str(path)
        )
        with mock.patch(
            "atrinik_workspace.workspace.durable_atomic_json",
            side_effect=WorkspaceError("simulated profile write failure"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "simulated"):
                self.workspace.set_profile(
                    "write-failure", "content", "primary", ""
                )
        identity = hashlib.sha256(
            str(
                (self.workspace.paths.profiles / "write-failure.json").resolve()
            ).encode()
        ).hexdigest()
        record = load_json(
            self.workspace._lease_namespace
            / "profile-references"
            / f"{identity}.json"
        )
        self.assertIn(str(path.resolve()), record["sources"])

    def test_profile_fsync_uncertainty_retains_conservative_references(self) -> None:
        path = self.workspace.create_worktree(
            "content", "uncertain-reference", "feat/uncertain-reference", None, False
        )
        self.workspace.create_profile("uncertain-update")
        self.workspace.set_profile(
            "uncertain-update", "content", "path", str(path)
        )
        profile_path = self.workspace.paths.profiles / "uncertain-update.json"
        real_publish = workspace_module.durable_atomic_json

        def uncertain_write(target: Path, value: object) -> None:
            real_publish(target, value)
            if target == profile_path:
                raise workspace_module.AtomicJsonCommitUncertain(
                    "simulated profile durability uncertainty"
                )

        with (
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json",
                side_effect=uncertain_write,
            ),
            self.assertRaisesRegex(WorkspaceError, "durability uncertainty"),
        ):
            self.workspace.set_profile(
                "uncertain-update", "content", "primary", ""
            )

        identity = hashlib.sha256(str(profile_path.resolve()).encode()).hexdigest()
        record = load_json(
            self.workspace._lease_namespace
            / "profile-references"
            / f"{identity}.json"
        )
        self.assertIn(str(path.resolve()), record["sources"])

    def test_layout_lock_reports_one_actionable_prolonged_wait(self) -> None:
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        intent = workspace_module._layout_writer_intent_path(layout)
        waiter_entered = threading.Event()

        def wait_for_reader_lease() -> None:
            with shared_layout_lock(layout, "repository layout"):
                waiter_entered.set()

        output = io.StringIO()
        with (
            mock.patch.object(
                locking_module, "LOCK_WAIT_DIAGNOSTIC_SECONDS", 0.05
            ),
            redirect_stderr(output),
            exclusive_lock(layout, "held repository layout"),
        ):
            with exclusive_lock(intent, "held reader admission"):
                waiter = threading.Thread(target=wait_for_reader_lease)
                waiter.start()
                time.sleep(0.08)
                self.assertFalse(waiter_entered.is_set())
            time.sleep(0.08)
            self.assertFalse(waiter_entered.is_set())
        waiter.join(2)
        self.assertFalse(waiter.is_alive())
        self.assertTrue(waiter_entered.is_set())
        diagnostic = output.getvalue()
        self.assertEqual(diagnostic.count("waiting more than"), 1)
        self.assertIn("./atrinik ps --json", diagnostic)
        self.assertIn("./atrinik worktree list --json", diagnostic)
        self.assertIn("do not bypass the wrapper", diagnostic)

    def test_resource_wait_names_coordinate_owner_and_recovery(self) -> None:
        coordinate = "client:/worktrees/review"
        holder = LeaseRequest(
            "source",
            coordinate,
            "exclusive",
            "advance review worktree",
            "wait for the exact advance to finish",
        )
        waiter_entered = threading.Event()

        def wait_for_source() -> None:
            request = LeaseRequest(
                "source",
                coordinate,
                "shared",
                "build review",
                "inspect `./atrinik worktree list --json` and retry",
            )
            with resource_locks(self.workspace.paths.workspace, [request]):
                waiter_entered.set()

        output = io.StringIO()
        with (
            mock.patch.object(locking_module, "LOCK_WAIT_DIAGNOSTIC_SECONDS", 0.05),
            redirect_stderr(output),
            resource_locks(self.workspace.paths.workspace, [holder]),
        ):
            waiter = threading.Thread(target=wait_for_source)
            waiter.start()
            time.sleep(0.08)
            self.assertFalse(waiter_entered.is_set())
        waiter.join(2)
        self.assertFalse(waiter.is_alive())
        diagnostic = output.getvalue()
        self.assertIn("resource source coordinate", diagnostic)
        self.assertIn(coordinate, diagnostic)
        self.assertIn("advance review worktree", diagnostic)
        self.assertIn("worktree list --json", diagnostic)
        self.assertNotIn(f"pid={os.getpid()}", diagnostic)

    def test_resource_lease_rejects_symlinked_namespace(self) -> None:
        external = self.root / "external-leases"
        external.mkdir()
        leases = self.workspace.paths.workspace / "leases"
        leases.symlink_to(external, target_is_directory=True)
        request = LeaseRequest(
            "source",
            "client:/worktrees/review",
            "shared",
            "inspect review",
            "remove the unsafe lease namespace",
        )

        with self.assertRaisesRegex(
            WorkspaceError, "directory is unsafe|cannot open.*directory"
        ):
            with resource_locks(self.workspace.paths.workspace, [request]):
                self.fail("symlinked lease namespace unexpectedly acquired")

        self.assertEqual(list(external.iterdir()), [])

    def test_resource_lease_rejects_symlinked_kind_and_owner_directories(
        self,
    ) -> None:
        request = LeaseRequest(
            "source",
            "client:/worktrees/review",
            "shared",
            "inspect review",
            "remove the unsafe lease namespace",
        )
        for level in ("kind", "owners"):
            with self.subTest(level=level):
                root = self.root / f"lease-{level}"
                kind = root / "leases" / "source"
                kind.parent.mkdir(parents=True)
                external = self.root / f"external-{level}"
                external.mkdir()
                if level == "kind":
                    kind.symlink_to(external, target_is_directory=True)
                else:
                    kind.mkdir()
                    lock = resource_lock_path(
                        root, request.kind, request.coordinate
                    )
                    lock.with_name(f"{lock.name}.owners").symlink_to(
                        external, target_is_directory=True
                    )

                with self.assertRaisesRegex(
                    WorkspaceError, "directory is unsafe|cannot open.*directory"
                ):
                    with resource_locks(root, [request]):
                        self.fail("symlinked lease directory unexpectedly acquired")
                self.assertEqual(list(external.iterdir()), [])

    def test_resource_owner_metadata_is_reclaimed_after_normal_release(self) -> None:
        request = LeaseRequest(
            "source",
            "client:/worktrees/review",
            "shared",
            "inspect review",
            "wait for review",
        )
        for _ in range(20):
            with resource_locks(self.workspace.paths.workspace, [request]):
                pass

        lock = resource_lock_path(
            self.workspace.paths.workspace, request.kind, request.coordinate
        )
        owners = lock.with_name(f"{lock.name}.owners")
        self.assertEqual(list(owners.iterdir()), [])

    def test_resource_owner_metadata_survives_inherited_child(self) -> None:
        request = LeaseRequest(
            "source",
            "client:/worktrees/review",
            "shared",
            "inspect review",
            "wait for review",
        )
        lock = resource_lock_path(
            self.workspace.paths.workspace, request.kind, request.coordinate
        )
        child: subprocess.Popen[bytes]
        with resource_locks(self.workspace.paths.workspace, [request]):
            child = subprocess.Popen(
                [sys.executable, "-c", "import time; time.sleep(30)"],
                pass_fds=active_lock_fds(),
            )
        owners = lock.with_name(f"{lock.name}.owners")
        self.assertEqual(len(list(owners.iterdir())), 1)
        child.terminate()
        child.wait(timeout=5)
        locking_module._lease_owner_summary(lock)
        self.assertEqual(list(owners.iterdir()), [])

    def test_relocated_workspace_roots_share_physical_lease_namespace(self) -> None:
        alternate_root = self.root / "alternate-workspace"
        with mock.patch.dict(
            os.environ, {"ATRINIK_WORKSPACE_DIR": str(alternate_root)}
        ):
            alternate = Workspace(self.wrapper)
            alternate.paths.ensure()

        self.assertEqual(
            self.workspace._lease_namespace, alternate._lease_namespace
        )
        primary = self.workspace.paths.repositories / "client"
        coordinate = self.workspace._source_coordinate("client", primary)
        request = self.workspace._lease_request(
            "source", coordinate, "exclusive", "replace client source"
        )
        competing = alternate._lease_request(
            "source", coordinate, "exclusive", "sync client source"
        )
        with resource_locks(self.workspace._lease_root, [request]):
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with resource_locks(
                    alternate._lease_root, [competing], nonblocking=True
                ):
                    self.fail("relocated workspace acquired duplicate source lease")

    def test_wrapper_worktrees_share_common_git_lease_namespace(self) -> None:
        self.workspace.close()
        command("git", "init", "-b", "main", cwd=self.wrapper)
        command("git", "config", "user.name", "Tests", cwd=self.wrapper)
        command(
            "git", "config", "user.email", "tests@example.invalid", cwd=self.wrapper
        )
        command("git", "add", "components.json", cwd=self.wrapper)
        command("git", "commit", "-m", "feat: seed wrapper", cwd=self.wrapper)
        self.workspace = Workspace(self.wrapper)
        linked_root = self.root / "linked-wrapper"
        command(
            "git",
            "worktree",
            "add",
            "-b",
            "test/linked-wrapper",
            str(linked_root),
            cwd=self.wrapper,
        )
        linked = Workspace(linked_root)
        primary_status = command("git", "status", "--porcelain", cwd=self.wrapper)
        linked_status = command("git", "status", "--porcelain", cwd=linked_root)

        self.assertEqual(self.workspace._lease_namespace, linked._lease_namespace)
        request = LeaseRequest(
            "git-admin",
            self.workspace._wrapper_git_admin_coordinate(),
            "exclusive",
            "clean wrapper worktree",
            "wait for wrapper administration",
        )
        with resource_locks(self.workspace._lease_root, [request]):
            competing = LeaseRequest(
                "git-admin",
                linked._wrapper_git_admin_coordinate(),
                "exclusive",
                "clean linked wrapper",
                "wait for wrapper administration",
            )
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with resource_locks(
                    linked._lease_root, [competing], nonblocking=True
                ):
                    self.fail("linked wrapper acquired duplicate Git-admin lease")
        self.assertEqual(
            command("git", "status", "--porcelain", cwd=self.wrapper),
            primary_status,
        )
        self.assertEqual(
            command("git", "status", "--porcelain", cwd=linked_root),
            linked_status,
        )
        linked.close()
        namespace = self.workspace._lease_namespace
        detached = namespace.with_name("detached-atrinik-resource-leases")
        namespace.rename(detached)
        namespace.mkdir(mode=0o700)
        try:
            with self.assertRaisesRegex(
                WorkspaceError, "physical lease namespace identity changed"
            ):
                Workspace(linked_root)
        finally:
            shutil.rmtree(namespace)
            detached.rename(namespace)

    def test_git_checkout_common_namespace_resolution_fails_closed(self) -> None:
        (self.wrapper / ".git").mkdir()
        with mock.patch.object(
            Workspace,
            "_git_common_directory",
            side_effect=WorkspaceError("untrusted Git administration"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "untrusted"):
                Workspace(self.wrapper)

    def test_shared_layout_lock_registers_only_live_layout_lease(self) -> None:
        layout = self.workspace.paths.workspace / "repository-layout.lock"

        self.assertEqual(active_lock_fds(), ())
        with shared_layout_lock(layout, "repository layout") as lease:
            self.assertEqual(active_lock_fds(), (lease.fileno(),))
        self.assertEqual(active_lock_fds(), ())

    def test_layout_writer_partial_failures_release_earlier_leases(self) -> None:
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        intent = workspace_module._layout_writer_intent_path(layout)
        pending = locking_module.layout_writer_pending_path(layout)

        for blocker in (intent, layout):
            with exclusive_lock(blocker, "staged acquisition blocker"):
                with self.assertRaisesRegex(WorkspaceError, "already in use"):
                    with exclusive_layout_lock(
                        layout, "repository layout", nonblocking=True
                    ):
                        self.fail("partially blocked writer unexpectedly entered")
            self.assertEqual(active_lock_fds(), ())
            for path in (pending, intent, layout):
                with exclusive_lock(path, "released writer stage", nonblocking=True):
                    pass

    def test_delayed_layout_readers_drain_after_writer(self) -> None:
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        entered = 0
        entered_lock = threading.Lock()
        attempts = [threading.Event() for _ in range(16)]

        def read_layout(attempt: threading.Event) -> None:
            nonlocal entered
            attempt.set()
            with shared_layout_lock(layout, "repository layout"):
                with entered_lock:
                    entered += 1

        with exclusive_layout_lock(layout, "repository layout"):
            readers = [
                threading.Thread(target=read_layout, args=(attempt,))
                for attempt in attempts
            ]
            for reader in readers:
                reader.start()
            self.assertTrue(all(attempt.wait(2) for attempt in attempts))
            self.assertEqual(entered, 0)
        for reader in readers:
            reader.join(2)
        self.assertTrue(all(not reader.is_alive() for reader in readers))
        self.assertEqual(entered, len(readers))

    def test_layout_writer_reports_once_across_sequential_waits(self) -> None:
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        intent = workspace_module._layout_writer_intent_path(layout)
        pending = locking_module.layout_writer_pending_path(layout)
        writer_entered = threading.Event()

        def wait_for_writer_lease() -> None:
            with exclusive_layout_lock(layout, "repository layout"):
                writer_entered.set()

        output = io.StringIO()
        with (
            mock.patch.object(
                locking_module, "LOCK_WAIT_DIAGNOSTIC_SECONDS", 0.05
            ),
            redirect_stderr(output),
            exclusive_lock(intent, "held writer admission"),
        ):
            with exclusive_lock(pending, "held writer announcement"):
                writer = threading.Thread(target=wait_for_writer_lease)
                writer.start()
                time.sleep(0.08)
                self.assertFalse(writer_entered.is_set())
            time.sleep(0.08)
            self.assertFalse(writer_entered.is_set())
        writer.join(2)
        self.assertFalse(writer.is_alive())
        self.assertTrue(writer_entered.is_set())
        self.assertEqual(output.getvalue().count("waiting more than"), 1)

    def test_independent_build_roots_overlap_across_processes(self) -> None:
        context = multiprocessing.get_context("spawn")
        self.workspace.create_profile("independent-a")
        self.workspace.create_profile("independent-b")
        entered = context.Queue()
        release = context.Event()
        results = context.Queue()
        attempting = [context.Event(), context.Event()]
        processes = [
            context.Process(
                target=synthetic_build_process,
                args=(
                    str(self.wrapper),
                    str(self.workspace_directory),
                    profile,
                    attempting[index],
                    entered,
                    release,
                    results,
                ),
            )
            for index, profile in enumerate(("independent-a", "independent-b"))
        ]
        try:
            for process in processes:
                process.start()
            self.assertEqual(
                {entered.get(timeout=5), entered.get(timeout=5)},
                {"independent-a", "independent-b"},
            )
        finally:
            release.set()
            join_or_stop_processes(processes, 10)
        self.assertEqual(
            [process.exitcode for process in processes], [0, 0]
        )
        self.assertEqual(
            [results.get(timeout=2), results.get(timeout=2)], [None, None]
        )

    def test_independent_build_options_are_thread_local(self) -> None:
        self.workspace.create_profile("thread-a")
        self.workspace.create_profile("thread-b")
        selected = {"client": self.workspace.paths.repositories / "client"}
        barrier = threading.Barrier(2)
        observed: dict[str, tuple[bool, bool, set[str]]] = {}

        def inspect_build(
            root: Path, *_arguments: object, **_keywords: object
        ) -> None:
            self.workspace._source_view_unchanged[str(root)] = True
            barrier.wait(timeout=5)
            observed[root.name] = (
                self.workspace._force_reconfigure,
                self.workspace._use_ccache,
                set(self.workspace._source_view_unchanged),
            )

        def build(profile: str, force: bool, ccache: bool) -> Path:
            return self.workspace._build_resolved(
                "client",
                profile,
                False,
                ["client"],
                selected,
                force_reconfigure=force,
                use_ccache=ccache,
            )

        with (
            mock.patch.object(self.workspace, "_refresh_build_metadata"),
            mock.patch.object(
                self.workspace, "_build_client", side_effect=inspect_build
            ),
            ThreadPoolExecutor(max_workers=2) as executor,
        ):
            futures = (
                executor.submit(build, "thread-a", True, False),
                executor.submit(build, "thread-b", False, True),
            )
            roots = [future.result(timeout=10) for future in futures]

        self.assertEqual(
            observed[roots[0].name], (True, False, {str(roots[0])})
        )
        self.assertEqual(
            observed[roots[1].name], (False, True, {str(roots[1])})
        )

    def test_shared_layout_lock_improves_independent_elapsed_time(self) -> None:
        context = multiprocessing.get_context("spawn")

        def measure(mode: str) -> float:
            ready = context.Queue()
            start = context.Event()
            results = context.Queue()
            processes = [
                context.Process(
                    target=timed_public_build_process,
                    args=(
                        str(self.wrapper),
                        str(self.workspace_directory),
                        f"timed-{mode}-{index}",
                        mode,
                        ready,
                        start,
                        results,
                    ),
                )
                for index in range(2)
            ]
            for index in range(2):
                self.workspace.create_profile(f"timed-{mode}-{index}")
            try:
                for process in processes:
                    process.start()
                self.assertEqual(
                    {ready.get(timeout=5), ready.get(timeout=5)},
                    {f"timed-{mode}-0", f"timed-{mode}-1"},
                )
                began = time.monotonic()
                start.set()
            finally:
                join_or_stop_processes(processes, 5)
            elapsed = time.monotonic() - began
            self.assertEqual([process.exitcode for process in processes], [0, 0])
            self.assertEqual(
                [results.get(timeout=2), results.get(timeout=2)], [None, None]
            )
            return elapsed

        exclusive_elapsed = measure("exclusive")
        shared_elapsed = measure("shared")
        self.assertLess(shared_elapsed, exclusive_elapsed * 0.8)

    def test_same_build_root_serializes_across_processes(self) -> None:
        context = multiprocessing.get_context("spawn")
        self.workspace.create_profile("same-root")
        entered = context.Queue()
        release = context.Event()
        results = context.Queue()
        attempting = [context.Event(), context.Event()]
        processes = [
            context.Process(
                target=synthetic_build_process,
                args=(
                    str(self.wrapper),
                    str(self.workspace_directory),
                    "same-root",
                    attempting[index],
                    entered,
                    release,
                    results,
                ),
            )
            for index in range(2)
        ]
        try:
            processes[0].start()
            self.assertEqual(entered.get(timeout=5), "same-root")
            processes[1].start()
            self.assertTrue(attempting[1].wait(timeout=5))
            build_lock = (
                self.workspace.paths.builds
                / "locks"
                / f"same-root-{'a' * 12}.lock"
            )
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    build_lock, "profile build same-root", nonblocking=True
                ):
                    self.fail("held build-root lock unexpectedly available")
            with self.assertRaises(queue.Empty):
                entered.get(timeout=0.25)
        finally:
            release.set()
            join_or_stop_processes(processes, 10)
        self.assertEqual(entered.get(timeout=2), "same-root")
        self.assertEqual(
            [process.exitcode for process in processes], [0, 0]
        )
        self.assertEqual(
            [results.get(timeout=2), results.get(timeout=2)], [None, None]
        )

    def test_source_reader_blocks_only_same_coordinate_writer(self) -> None:
        context = multiprocessing.get_context("spawn")
        entered = context.Queue()
        results = context.Queue()
        release_reader = context.Event()
        release_writers = context.Event()
        coordinate_a = "client:/worktrees/a"
        coordinate_b = "client:/worktrees/b"
        reader = context.Process(
            target=resource_lease_process,
            args=(
                str(self.workspace_directory),
                "source",
                coordinate_a,
                "shared",
                "reader-a",
                entered,
                release_reader,
                results,
            ),
        )
        writer_a = context.Process(
            target=resource_lease_process,
            args=(
                str(self.workspace_directory),
                "source",
                coordinate_a,
                "exclusive",
                "writer-a",
                entered,
                release_writers,
                results,
            ),
        )
        writer_b = context.Process(
            target=resource_lease_process,
            args=(
                str(self.workspace_directory),
                "source",
                coordinate_b,
                "exclusive",
                "writer-b",
                entered,
                release_writers,
                results,
            ),
        )
        processes = [reader, writer_a, writer_b]
        try:
            reader.start()
            self.assertEqual(entered.get(timeout=5), "reader-a")
            writer_a.start()
            writer_b.start()
            self.assertEqual(entered.get(timeout=5), "writer-b")
            with self.assertRaises(queue.Empty):
                entered.get(timeout=0.25)
            release_reader.set()
            self.assertEqual(entered.get(timeout=5), "writer-a")
        finally:
            release_reader.set()
            release_writers.set()
            join_or_stop_processes(processes, 10)
        self.assertEqual([process.exitcode for process in processes], [0, 0, 0])
        self.assertEqual([results.get(timeout=2) for _ in processes], [None] * 3)

    def test_multi_source_wait_does_not_retain_earlier_coordinate(self) -> None:
        earlier = self.workspace._lease_request(
            "source", "client:/a", "exclusive", "synchronize client"
        )
        later = self.workspace._lease_request(
            "source", "client:/z", "exclusive", "synchronize client"
        )
        later_reader = self.workspace._lease_request(
            "source", "client:/z", "shared", "build later"
        )
        earlier_reader = self.workspace._lease_request(
            "source", "client:/a", "shared", "build earlier"
        )
        entered = threading.Event()
        release = threading.Event()

        def acquire_both() -> None:
            with self.workspace._resource_locks_all_or_none([earlier, later]):
                entered.set()
                self.assertTrue(release.wait(5))

        with ThreadPoolExecutor(max_workers=1) as executor:
            with self.workspace._resource_locks([later_reader]):
                writer = executor.submit(acquire_both)
                time.sleep(0.05)
                with self.workspace._resource_locks(
                    [earlier_reader], nonblocking=True
                ):
                    self.assertFalse(entered.is_set())
            self.assertTrue(entered.wait(2))
            release.set()
            writer.result(timeout=5)

    def test_exclusive_lock_refuses_symlink(self) -> None:
        target = self.root / "valuable"
        target.write_text("preserve\n", encoding="utf-8")
        lock = self.workspace.paths.builds / "locks" / "test.lock"
        lock.parent.mkdir(parents=True)
        lock.symlink_to(target)

        with self.assertRaisesRegex(WorkspaceError, "cannot open test resource lock"):
            with exclusive_lock(lock, "test resource"):
                self.fail("symlinked lock unexpectedly opened")

        self.assertEqual(target.read_text(encoding="utf-8"), "preserve\n")

    def test_operational_profile_mutation_waits_for_maintenance_barrier(self) -> None:
        lock = self.workspace._lease_namespace / "repository-layout.lock"
        completed = threading.Event()

        def create() -> None:
            self.workspace.create_profile("review")
            completed.set()

        with ThreadPoolExecutor(max_workers=1) as executor:
            with exclusive_layout_lock(lock, "repository maintenance"):
                future = executor.submit(create)
                time.sleep(0.05)
                self.assertFalse(completed.is_set())
            future.result(timeout=5)
        self.assertEqual(self.workspace.profile_summary("review")["name"], "review")

    def test_state_and_cleanup_mutations_wait_for_maintenance_barrier(self) -> None:
        lock = self.workspace._lease_namespace / "repository-layout.lock"
        with ThreadPoolExecutor(max_workers=2) as executor:
            with exclusive_layout_lock(lock, "repository maintenance"):
                state = executor.submit(self.workspace.state_add, "held", None)
                cleanup = executor.submit(
                    self.workspace.cleanup, ["builds"], 7, [], True
                )
                time.sleep(0.05)
                self.assertFalse(state.done())
                self.assertFalse(cleanup.done())
            self.assertEqual(
                state.result(timeout=5),
                (self.workspace.paths.state / "server" / "held").resolve(),
            )
            self.assertIsInstance(cleanup.result(timeout=5), dict)

    def test_distinct_profile_writers_overlap(self) -> None:
        context = multiprocessing.get_context("spawn")
        entered = context.Queue()
        results = context.Queue()
        release = context.Event()
        processes = [
            context.Process(
                target=resource_lease_process,
                args=(
                    str(self.workspace_directory),
                    "profile",
                    name,
                    "exclusive",
                    name,
                    entered,
                    release,
                    results,
                ),
            )
            for name in ("profile-a", "profile-b")
        ]
        try:
            for process in processes:
                process.start()
            self.assertEqual(
                {entered.get(timeout=5), entered.get(timeout=5)},
                {"profile-a", "profile-b"},
            )
        finally:
            release.set()
            join_or_stop_processes(processes, 10)
        self.assertEqual([process.exitcode for process in processes], [0, 0])
        self.assertEqual([results.get(timeout=2) for _ in processes], [None, None])

    def test_mixed_layout_readers_preserve_markers_and_coordinates(self) -> None:
        self.workspace.create_profile("stress")
        selected_build = self.workspace._resolve_build_profile(
            "stress", {"client"}
        )
        build_key = self.workspace._profile_build_key("stress", selected_build)
        build_root = (
            self.workspace.paths.builds / "profiles" / f"stress-{build_key}"
        )
        managed_directory(
            build_root,
            self.workspace.paths.builds,
            f"profile:stress:{build_key}",
        )
        marker = load_json(build_root / MANAGED_MARKER)
        topology_root = self.workspace._topology_directory("stress", create=True)
        selected_topology = self.workspace._resolve_build_profile(
            "default", {"server"}
        )
        resolved = self.workspace._topology_resolved_status(
            "default", selected_topology
        )
        status = {
            "schema_version": 1,
            "name": "stress",
            "profile": "default",
            "stack": "default",
            "providers": {
                role: self.workspace.manifest.stack("default").providers[role].name
                for role in sorted(selected_topology)
            },
            "dependencies": sorted(selected_topology),
            "state": None,
            "build_root": str(build_root),
            "resolved": resolved,
            "endpoint": None,
            "ready": False,
            "started_at": "2026-08-11T00:00:00+00:00",
            "stopped_at": None,
            "supervisor": {"pid": 999, "start_time": "1"},
            "services": {},
            "error": "stress fixture",
            "sound": workspace_module.sound_source_record(self.wrapper / "sound"),
        }
        atomic_json(topology_root / "status.json", status)
        context = multiprocessing.get_context("spawn")
        ready = context.Queue()
        start = context.Event()
        entered = context.Queue()
        release = context.Event()
        results = context.Queue()
        operations = ("build", "topology", "status", "cleanup")
        processes = [
            context.Process(
                target=mixed_layout_operation_process,
                args=(
                    str(self.wrapper),
                    str(self.workspace_directory),
                    operation,
                    status,
                    ready,
                    start,
                    entered,
                    release,
                    results,
                ),
            )
            for operation in operations
        ]
        try:
            for process in processes:
                process.start()
            self.assertEqual(
                {ready.get(timeout=5) for _ in processes}, set(operations)
            )
            start.set()
            self.assertEqual(
                {entered.get(timeout=20) for _ in processes}, set(operations)
            )
        finally:
            release.set()
            join_or_stop_processes(processes, 20)
        self.assertEqual([process.exitcode for process in processes], [0] * 4)
        outcomes = dict(results.get(timeout=2) for _ in processes)
        self.assertEqual(outcomes, {operation: None for operation in operations})
        self.assertEqual(load_json(build_root / MANAGED_MARKER), marker)
        metadata = load_json(build_root / workspace_module.BUILD_METADATA)
        self.assertEqual(metadata["profile"], "stress")
        self.assertEqual(metadata["key"], build_key)
        for role, coordinate in metadata["coordinates"].items():
            self.assertEqual(
                Path(coordinate["source_path"]), selected_build[role].resolve()
            )
            checkout_path = Path(coordinate["checkout_path"])
            self.assertEqual(
                coordinate["head"],
                command("git", "rev-parse", "HEAD", cwd=checkout_path),
            )
        expected_status = copy.deepcopy(status)
        expected_status["error"] = "stress generation 19"
        self.assertEqual(load_json(topology_root / "status.json"), expected_status)

    def test_topology_releases_profile_and_source_leases_after_publication(self) -> None:
        observed: tuple[int, ...] = ()

        def inspect(*_arguments: object, **keywords: object) -> dict[str, object]:
            nonlocal observed
            self.assertNotIn("resource_lock_fds", keywords)
            observed = active_lock_fds()
            self.assertGreaterEqual(len(observed), 2)
            self.assertTrue(all(os.fstat(descriptor) for descriptor in observed))
            return {"name": "review"}

        with mock.patch.object(self.workspace, "_topology_up", side_effect=inspect):
            result = self.workspace.topology_up(
                "review", "default", "default", ["server"]
            )
        self.assertEqual(result, {"name": "review"})
        self.assertTrue(observed)
        for descriptor in observed:
            with self.assertRaises(OSError):
                os.fstat(descriptor)

    def test_server_runtime_paths_are_isolated_by_state(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        root = self.workspace.paths.builds / "profiles" / "test"
        managed_directory(root, self.workspace.paths.builds, "test-profile")
        binary = root / "build" / "server"
        binary.mkdir(parents=True)
        for name in ("atrinik-server", "libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            root / "runtime" / "content" / "lib",
            root / "runtime" / "content" / "maps",
            root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        self.make_region_map_cache(root)
        state_one = self.root / "state-one"
        state_two = self.root / "state-two"
        state_one.mkdir()
        state_two.mkdir()

        first = self.workspace._prepare_server_runtime(
            root, {"server": source}, state_one, "one"
        )
        second = self.workspace._prepare_server_runtime(
            root, {"server": source}, state_two, "two"
        )

        self.assertNotEqual(first, second)
        self.assertTrue((first / "data").is_symlink())
        self.assertEqual((second / "data").resolve(), state_two)
        self.assertTrue((first / "assets" / "data").is_dir())
        self.assertFalse((first / "assets" / "data").is_symlink())
        self.assertNotEqual(first / "assets", second / "assets")
        staged_maps = first / "assets" / "client-maps"
        self.assertTrue((staged_maps / "incuna_-1.png").is_file())
        self.assertFalse(staged_maps.is_symlink())

        generated = first / "assets" / "data" / "listing.txt"
        generated.write_text("generated\n", encoding="utf-8")
        repeated = self.workspace._prepare_server_runtime(
            root, {"server": source}, state_one, "one"
        )
        self.assertEqual(repeated, first)
        self.assertFalse(generated.exists())

    def test_asset_staging_directory_rejects_invalid_nodes(self) -> None:
        missing = self.root / "missing-assets"
        self.workspace._prepare_asset_staging_directory(missing)
        self.assertTrue(missing.is_dir())

        invalid_file = self.root / "asset-file"
        invalid_file.write_text("invalid\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "asset staging path is invalid"):
            self.workspace._prepare_asset_staging_directory(invalid_file)

        target = self.root / "asset-target"
        target.mkdir()
        invalid_link = self.root / "asset-link"
        invalid_link.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "asset staging path is invalid"):
            self.workspace._prepare_asset_staging_directory(invalid_link)

    def test_topology_summary_uses_target_specific_build_roles(self) -> None:
        summary = self.workspace.topology_summary(
            "default", "default", ["client"]
        )

        self.assertEqual(summary["services"], ["client"])
        self.assertIsNone(summary["state"])
        self.assertEqual(
            set(summary["dependencies"]),
            {
                "client",
                "sound",
                "libatrinik",
                "protocol",
            },
        )
        self.assertEqual(
            set(summary["components"]),
            {
                "client",
                "sound",
                "libatrinik",
                "protocol",
            },
        )
        self.assertIn("default-", summary["build_root"])
        self.assertEqual(
            summary["sound"],
            {
                "mode": "source",
                "release": None,
                "source_path": str(self.wrapper / "sound"),
            },
        )

    def test_supervised_topology_lifecycle_and_logs(self) -> None:
        build_root = self.workspace.paths.builds / "fake-topology"
        executable = build_root / "build" / "client" / "atrinik"
        executable.parent.mkdir(parents=True)
        (build_root / "sources" / "client").mkdir(parents=True)
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import os, sys, time\n"
            "print('client ready', flush=True)\n"
            "print('arguments=' + repr(sys.argv[1:]), flush=True)\n"
            "print('config=' + os.environ['ATRINIK_CONFIG_DIR'], flush=True)\n"
            "print('launch=' + os.environ['ATRINIK_LAUNCH_LABEL'], flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        atomic_json(
            build_root / workspace_module.BUILD_METADATA,
            {"sound": workspace_module.sound_source_record(self.wrapper / "sound")},
        )
        second_build_root = self.workspace.paths.builds / "fake-topology-second"
        shutil.copytree(build_root, second_build_root, symlinks=True)

        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ) as build_resolved,
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            status = self.workspace.topology_up(
                "review", "default", "default", ["client"]
            )
        self.assertEqual(build_resolved.call_args.args[3], ["client"])
        try:
            self.assertTrue(status["supervisor"]["running"])
            self.assertTrue(status["services"]["client"]["running"])
            self.assertEqual(
                status["sound"],
                workspace_module.sound_source_record(self.wrapper / "sound"),
            )
            self.assertEqual(
                Path(status["services"]["client"]["cwd"]),
                Path(status["runtime"]["path"]) / "client",
            )
            with (
                mock.patch.object(
                    self.workspace,
                    "_build_resolved",
                    return_value=second_build_root,
                ),
                mock.patch.object(self.workspace, "_require_client_display"),
            ):
                second = self.workspace.topology_up(
                    "review-two", "default", "default", ["client"]
                )
            self.assertNotEqual(
                status["services"]["client"]["cwd"],
                second["services"]["client"]["cwd"],
            )
            with self.assertRaisesRegex(WorkspaceError, "already running"):
                with (
                    mock.patch.object(
                        self.workspace, "_build_resolved", return_value=build_root
                    ),
                    mock.patch.object(self.workspace, "_require_client_display"),
                ):
                    self.workspace.topology_up(
                        "review", "default", "default", ["client"]
                    )

            deadline = time.monotonic() + 5
            log = self.workspace.paths.topologies / "review" / "client.log"
            while time.monotonic() < deadline and (
                not log.is_file() or "client ready" not in log.read_text()
            ):
                time.sleep(0.05)
            self.assertIn("client ready", log.read_text())
            self.assertIn(
                str(self.workspace.paths.topologies / "review" / "client-config"),
                log.read_text(),
            )
            self.assertIn("launch=topology review - profile default", log.read_text())
            persisted_spec = (
                self.workspace.paths.topologies / "review" / "spec.json"
            ).read_text()
            self.assertNotIn("ATRINIK_LAUNCH_LABEL", persisted_spec)
            self.assertNotIn("topology review - profile default", persisted_spec)

            second_log = self.workspace.paths.topologies / "review-two" / "client.log"
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and (
                not second_log.is_file() or "launch=" not in second_log.read_text()
            ):
                time.sleep(0.05)
            self.assertIn(
                "launch=topology review-two - profile default",
                second_log.read_text(),
            )

            with mock.patch("builtins.print") as output:
                self.workspace.topology_logs("review", "client", 10, False)
            self.assertIn("client ready", "".join(call.args[0] for call in output.call_args_list))
            with mock.patch(
                "atrinik_workspace.workspace.process_matches", return_value=False
            ):
                cross_namespace = self.workspace.topology_status("review")
            self.assertEqual(
                cross_namespace["supervisor"]["liveness"], "live"
            )
            self.assertEqual(
                cross_namespace["services"]["client"]["liveness"], "live"
            )
            self.assertEqual(
                cross_namespace["observation"]["control"], "reachable"
            )
            self.assertNotIn(
                "source_lease_owner", cross_namespace["observation"]
            )
            self.assertEqual(
                cross_namespace["observation"]["runtime_bundle_lease"],
                "retained",
            )
            status_path = (
                self.workspace.paths.topologies / "review" / "status.json"
            )
            persisted_status = load_json(status_path)
            reused = copy.deepcopy(persisted_status)
            reused["control"]["generation"] = "b" * 64
            reused["control"]["socket"] = str(
                workspace_module.control_socket_path(
                    self.workspace.paths.topologies / "review", "b" * 64
                )
            )
            reused["supervisor"]["generation"] = "b" * 64
            for service in reused["services"].values():
                service["generation"] = "b" * 64
            atomic_json(status_path, reused)
            try:
                with (
                    mock.patch(
                        "atrinik_workspace.workspace.process_matches",
                        return_value=False,
                    ),
                    mock.patch(
                        "atrinik_workspace.workspace.signal_holders"
                    ) as signaled,
                    self.assertRaisesRegex(
                        WorkspaceError, "lease generation changed"
                    ),
                ):
                    self.workspace.topology_down("review", timeout=0.1)
                signaled.assert_not_called()
            finally:
                atomic_json(status_path, persisted_status)
        finally:
            try:
                if self.workspace.topology_status("review-two")["supervisor"][
                    "running"
                ]:
                    self.workspace.topology_down("review-two", timeout=5)
            except WorkspaceError:
                pass
            if self.workspace.topology_status("review")["supervisor"]["running"]:
                self.workspace.topology_down("review", timeout=5)

        stopped = self.workspace.topology_status("review")
        self.assertFalse(stopped["supervisor"]["running"])
        self.assertFalse(stopped["services"]["client"]["running"])
        status_path = self.workspace.paths.topologies / "review" / "status.json"
        missing_sound = load_json(status_path)
        sound = missing_sound.pop("sound")
        atomic_json(status_path, missing_sound)
        with self.assertRaisesRegex(WorkspaceError, "topology status is invalid"):
            self.workspace.topology_status("review")
        missing_sound["sound"] = sound
        atomic_json(status_path, missing_sound)
        with mock.patch(
            "atrinik_workspace.workspace.process_matches", return_value=True
        ):
            reused_local_pid = self.workspace.topology_status("review")
        self.assertEqual(reused_local_pid["supervisor"]["liveness"], "exited")
        self.assertFalse(reused_local_pid["services"]["client"]["running"])

        invalid_root = self.workspace._topology_directory(
            "invalid-config", create=True
        )
        (invalid_root / "client-config").write_text(
            "not a directory\n", encoding="utf-8"
        )
        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(self.workspace, "_require_client_display"),
            self.assertRaisesRegex(
                WorkspaceError, "client configuration path is invalid"
            ),
        ):
            self.workspace.topology_up(
                "invalid-config", "default", "default", ["client"]
            )
        self.assertEqual(list((invalid_root / "generations").iterdir()), [])

    def test_topology_status_inventory_probes_with_bounded_concurrency(self) -> None:
        for index in range(24):
            root = self.workspace.paths.topologies / f"probe-{index}"
            root.mkdir()
            (root / "status.json").write_text("{}\n", encoding="utf-8")

        def delayed(name: str) -> dict[str, str]:
            time.sleep(0.05)
            return {"name": name}

        started = time.monotonic()
        with mock.patch.object(
            self.workspace, "topology_status", side_effect=delayed
        ):
            statuses = self.workspace.topology_statuses()

        self.assertLess(time.monotonic() - started, 0.5)
        self.assertEqual(
            [status["name"] for status in statuses],
            [f"probe-{index}" for index in sorted(range(24), key=str)],
        )

    def test_supervised_local_playtest_client_uses_recorded_verified_root(self) -> None:
        self.workspace.create_profile("classic-audio", "classic")
        self.workspace.set_profile_sound_mode("classic-audio", "local-playtest")
        build_root = self.workspace.paths.builds / "fake-playtest-topology"
        executable = build_root / "build" / "client" / "atrinik"
        executable.parent.mkdir(parents=True)
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import time\n"
            "print('local playtest client ready', flush=True)\n"
            "time.sleep(5)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        sound_root = build_root / "runtime" / "sound-local-playtest"
        sound_root.mkdir(parents=True)
        sound_record = {
            "mode": "local-playtest",
            "root": str(sound_root),
            "playtest_manifest_sha256": "1" * 64,
            "playtest_schema_version": 1,
            "source_commit": "2" * 40,
            "source_tree": "3" * 40,
            "source_clean": True,
            "source_manifest_sha256": "4" * 64,
            "toolchain_sha256": "5" * 64,
            "toolchain_schema": "../schemas/playtest-audio-toolchain-v1.schema.json",
            "toolchain_schema_version": 1,
            "schema_sha256": "6" * 64,
            "marker_sha256": "7" * 64,
            "blocker_report_sha256": "8" * 64,
            "output_tree_sha256": "9" * 64,
            "logical_path_count": 339,
            "copied_vorbis_count": 196,
            "converted_opus_count": 143,
        }
        atomic_json(build_root / workspace_module.BUILD_METADATA, {"sound": sound_record})
        selected = {
            "client": self.wrapper / "client",
            "sound": self.wrapper / "sound",
        }
        resolved = {
            "client": {
                "path": str(self.wrapper / "client"),
                "checkout_path": str(self.wrapper / "client"),
                "checkout": "client",
                "repository": "atrinik/client",
                "branch": "main",
                "source": ".",
                "head": "a" * 40,
                "dirty": False,
            },
            "sound": {
                "path": str(self.wrapper / "sound"),
                "checkout_path": str(self.wrapper / "sound"),
                "checkout": "sound",
                "repository": "atrinik/sound",
                "branch": "main",
                "source": ".",
                "head": "b" * 40,
                "dirty": False,
            },
        }
        inputs = {
            "source_commit": sound_record["source_commit"],
            "source_tree": sound_record["source_tree"],
        }
        snapshot_states = {
            role: {
                "path": Path(record["checkout_path"]),
                "head": record["head"],
                "dirty": record["dirty"],
                "device": Path(record["checkout_path"]).stat().st_dev,
                "inode": Path(record["checkout_path"]).stat().st_ino,
                "git_common": str(Path(record["checkout_path"]) / ".git"),
            }
            for role, record in resolved.items()
        }
        classic_stack = mock.Mock()
        classic_stack.name = "classic"
        classic_stack.components = ()
        classic_stack.providers = {
            "client": self.workspace.manifest.by_name["client"],
            "sound": self.workspace.manifest.by_name["sound"],
        }
        with (
            mock.patch.object(
                self.workspace.manifest, "stack", return_value=classic_stack
            ),
            mock.patch.object(self.workspace, "_require_classic_contracts"),
            mock.patch.object(self.workspace, "_require_client_display"),
            mock.patch.object(
                self.workspace,
                "_dependency_roles",
                return_value=set(selected),
            ),
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace,
                "_selector_root",
                side_effect=lambda _profile, component: selected[component.name],
            ),
            mock.patch.object(
                self.workspace,
                "_selected_checkout_states",
                return_value=snapshot_states,
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(
                self.workspace, "_topology_resolved_status", return_value=resolved
            ),
            mock.patch(
                "atrinik_workspace.workspace.clean_source_inputs",
                return_value=inputs,
            ),
            mock.patch(
                "atrinik_workspace.workspace.verify_playtest_tree",
                return_value=sound_record,
            ),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="c" * 40
            ),
        ):
            status = self.workspace.topology_up(
                "playtest-client", "classic-audio", "default", ["client"]
            )
        try:
            self.assertTrue(status["services"]["client"]["running"])
            self.assertEqual(status["sound"], sound_record)
            runtime = Path(status["services"]["client"]["cwd"])
            self.assertFalse((runtime / "sound").is_symlink())
            self.assertNotEqual((runtime / "sound").resolve(), sound_root.resolve())
            log = self.workspace.paths.topologies / "playtest-client" / "client.log"
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and (
                not log.is_file()
                or "local playtest client ready" not in log.read_text()
            ):
                time.sleep(0.05)
            with mock.patch("builtins.print") as output:
                self.workspace.topology_logs(
                    "playtest-client", "client", tail=20, follow=False
                )
            self.assertIn(
                "local playtest client ready",
                "".join(call.args[0] for call in output.call_args_list),
            )
        finally:
            with mock.patch.object(
                self.workspace.manifest, "stack", return_value=classic_stack
            ):
                stopped = self.workspace.topology_down(
                    "playtest-client", timeout=5
                )
        self.assertFalse(stopped["services"]["client"]["running"])

    def test_concurrent_server_topologies_overlap_through_readiness(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")

        def free_port() -> int:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as candidate:
                candidate.bind(("0.0.0.0", 0))
                return int(candidate.getsockname()[1])

        for mode in ("explicit", "automatic", "temporary", "mixed-policy"):
            with self.subTest(mode=mode):
                rendezvous = self.root / f"{mode}-port-rendezvous"
                rendezvous.mkdir()
                roots = [
                    self.workspace.paths.builds / f"{mode}-server-{index}"
                    for index in range(2)
                ]
                for index, root in enumerate(roots):
                    self.make_rendezvous_server_build(
                        root, rendezvous, f"{index}.bound"
                    )
                states: list[str | None]
                if mode == "temporary":
                    states = [None, None]
                elif mode == "mixed-policy":
                    states = [None, "mixed-policy-state"]
                else:
                    states = [f"{mode}-state-{index}" for index in range(2)]
                for state in states:
                    if state is not None:
                        self.workspace.state_add(state, None)
                names = [f"{mode}-topology-{index}" for index in range(2)]
                if mode == "explicit":
                    ports: list[int | None] = [free_port(), free_port()]
                    while ports[0] == ports[1]:
                        ports[1] = free_port()
                else:
                    ports = [None, None]
                sessions = [Workspace(self.wrapper), Workspace(self.wrapper)]
                try:
                    with (
                        mock.patch.object(
                            sessions[0], "_build_resolved", return_value=roots[0]
                        ),
                        mock.patch.object(
                            sessions[1], "_build_resolved", return_value=roots[1]
                        ),
                        ThreadPoolExecutor(max_workers=2) as executor,
                    ):
                        futures = [
                            executor.submit(
                                sessions[index].topology_up,
                                names[index],
                                "default",
                                states[index],
                                ["server"],
                                ports[index],
                            )
                            for index in range(2)
                        ]
                        statuses = [future.result(timeout=20) for future in futures]
                    self.assertTrue(all(status["ready"] for status in statuses))
                    self.assertEqual(
                        len({status["endpoint"]["port"] for status in statuses}), 2
                    )
                    self.assertEqual(
                        {path.name for path in rendezvous.glob("*.bound")},
                        {"0.bound", "1.bound"},
                    )
                    if mode == "temporary":
                        state_paths = {
                            status["state_policy"]["path"] for status in statuses
                        }
                        self.assertEqual(len(state_paths), 2)
                        self.assertTrue(
                            all(Path(path).is_dir() for path in state_paths)
                        )
                        registered = set(self.workspace._load_states().values())
                        self.assertTrue(state_paths.isdisjoint(registered))
                    elif mode == "mixed-policy":
                        self.assertEqual(
                            [
                                status["state_policy"]["mode"]
                                for status in statuses
                            ],
                            ["temporary", "named"],
                        )
                        registered = set(self.workspace._load_states().values())
                        self.assertNotIn(
                            statuses[0]["state_policy"]["path"], registered
                        )
                        self.assertIn(
                            statuses[1]["state_policy"]["path"], registered
                        )
                    observer = Workspace(self.wrapper)
                    for index, name in enumerate(names):
                        observed = observer.topology_status(name)
                        self.assertTrue(observed["ready"])
                        self.assertEqual(
                            observed["observation"]["port_reservation"]["lease"],
                            "retained",
                        )
                        if mode == "explicit":
                            self.assertEqual(observed["endpoint"]["port"], ports[index])

                    supervisor = statuses[0]["supervisor"]
                    pidfd = os.pidfd_open(supervisor["pid"])
                    try:
                        signal.pidfd_send_signal(pidfd, signal.SIGKILL)
                    finally:
                        os.close(pidfd)
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        crashed = observer.topology_status(names[0])
                        if (
                            not crashed["supervisor"]["running"]
                            and crashed["observation"]["port_reservation"]["lease"]
                            == "released"
                        ):
                            break
                        time.sleep(0.05)
                    self.assertFalse(crashed["supervisor"]["running"])
                    self.assertEqual(
                        crashed["observation"]["port_reservation"]["lease"],
                        "released",
                    )
                    reassigned_fd, reassigned = observer._reserve_topology_port(
                        crashed["endpoint"]["port"],
                        f"{mode}-replacement",
                        "f" * 64,
                    )
                    try:
                        self.assertTrue(
                            workspace_module.port_reservation_locked(reassigned)
                        )
                        self.assertEqual(
                            observer.topology_status(names[0])["observation"]
                            ["port_reservation"]["lease"],
                            "released",
                        )
                    finally:
                        os.close(reassigned_fd)
                finally:
                    observer = Workspace(self.wrapper)
                    for name in names:
                        remaining = observer.topology_status(name)
                        if remaining["supervisor"]["running"] or any(
                            service["running"]
                            for service in remaining["services"].values()
                        ):
                            observer.topology_down(name, timeout=5)

    def test_supervisor_loss_before_server_bind_releases_reservation(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        rendezvous = self.root / "pre-bind-rendezvous"
        rendezvous.mkdir()
        build_root = self.workspace.paths.builds / "pre-bind-server"
        self.make_rendezvous_server_build(
            build_root,
            rendezvous,
            "server.entered",
            peers=2,
            bind_after_gate=True,
        )
        state = "pre-bind-state"
        self.workspace.state_add(state, None)
        name = "pre-bind-topology"
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as candidate:
            candidate.bind(("0.0.0.0", 0))
            port = int(candidate.getsockname()[1])
        session = Workspace(self.wrapper)
        observer = Workspace(self.wrapper)
        try:
            with (
                mock.patch.object(
                    session, "_build_resolved", return_value=build_root
                ),
                ThreadPoolExecutor(max_workers=1) as executor,
            ):
                startup = executor.submit(
                    session.topology_up,
                    name,
                    "default",
                    state,
                    ["server"],
                    port,
                )
                marker = rendezvous / "server.entered"
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline and not marker.is_file():
                    time.sleep(0.02)
                self.assertTrue(marker.is_file())
                pending = observer.topology_status(name)
                self.assertEqual(
                    pending["observation"]["port_reservation"]["lease"],
                    "retained",
                )
                supervisor = pending["supervisor"]
                pidfd = os.pidfd_open(supervisor["pid"])
                try:
                    signal.pidfd_send_signal(pidfd, signal.SIGKILL)
                finally:
                    os.close(pidfd)
                with self.assertRaisesRegex(
                    WorkspaceError, "supervisor exited during startup"
                ):
                    startup.result(timeout=10)

            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                failed = observer.topology_status(name)
                if (
                    not failed["supervisor"]["running"]
                    and failed["observation"]["port_reservation"]["lease"]
                    == "released"
                ):
                    break
                time.sleep(0.05)
            self.assertEqual(
                failed["observation"]["port_reservation"]["lease"], "released"
            )
            replacement_fd, replacement = observer._reserve_topology_port(
                port, "post-crash", "f" * 64
            )
            try:
                self.assertEqual(replacement["port"], port)
            finally:
                os.close(replacement_fd)
        finally:
            remaining = observer.topology_status(name)
            if remaining["supervisor"]["running"] or any(
                service["running"] for service in remaining["services"].values()
            ):
                observer.topology_down(name, timeout=5)

    def test_supervised_pair_pins_client_and_holds_state_lock_until_down(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")
        client_source = self.workspace.paths.repositories / "client" / "src"
        client_source.mkdir()
        (client_source / "authored.c").write_text("source\n", encoding="utf-8")
        build_root = self.workspace.paths.builds / "fake-server-topology"
        binary = build_root / "build" / "server"
        binary.mkdir(parents=True)
        executable = binary / "atrinik-server"
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import os, pathlib, sys, time\n"
            "for descriptor in os.listdir('/proc/self/fd'):\n"
            "    try:\n"
            "        target = os.readlink('/proc/self/fd/' + descriptor)\n"
            "    except OSError:\n"
            "        continue\n"
            "    assert not target.endswith('/ports.lock'), target\n"
            "    assert '/port-reservations/' not in target, target\n"
            "assetspath = next(\n"
            "    value.split('=', 1)[1]\n"
            "    for value in sys.argv[1:]\n"
            "    if value.startswith('--assetspath=')\n"
            ")\n"
            "data = pathlib.Path(assetspath) / 'data'\n"
            "data.mkdir(exist_ok=True)\n"
            "(data / 'listing.txt').write_text('generated\\n')\n"
            f"print('QUIC certificate SHA-256: {'a' * 64}', flush=True)\n"
            "print('Server ready. Waiting for connections...', flush=True)\n"
            "print(repr(sys.argv[1:]), flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            build_root / "runtime" / "content" / "lib",
            build_root / "runtime" / "content" / "maps",
            build_root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        (build_root / "runtime" / "content" / "maps" / "map").write_text(
            "shared content\n", encoding="utf-8"
        )
        (build_root / "runtime" / "resources" / "resource").write_text(
            "shared resource\n", encoding="utf-8"
        )
        atomic_json(
            build_root / "runtime" / "content" / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "collected-content"},
        )
        atomic_json(
            build_root / "runtime" / "resources" / MANAGED_MARKER,
            {"schema_version": 1, "purpose": "resource-view"},
        )
        self.make_region_map_cache(build_root)
        client = build_root / "build" / "client" / "atrinik"
        client.parent.mkdir(parents=True)
        build_only_source = client.parent / "src"
        build_only_source.mkdir()
        (build_only_source / "generated.c").write_text(
            "build-only\n", encoding="utf-8"
        )
        (build_root / "sources" / "client").mkdir(parents=True)
        client.write_text(
            "#!/usr/bin/env python3\n"
            "import os, sys, time\n"
            "print(repr(sys.argv[1:]), flush=True)\n"
            "print('config=' + os.environ['ATRINIK_CONFIG_DIR'], flush=True)\n"
            "print('launch=' + os.environ['ATRINIK_LAUNCH_LABEL'], flush=True)\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        client.chmod(0o755)
        atomic_json(
            build_root / workspace_module.BUILD_METADATA,
            {"sound": workspace_module.sound_source_record(self.wrapper / "sound")},
        )
        second_build_root = (
            self.workspace.paths.builds / "profiles" / "fake-server-topology-second"
        )
        second_build_root.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(build_root, second_build_root, symlinks=True)
        atomic_json(second_build_root / workspace_module.BUILD_METADATA, {})

        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ) as build_resolved,
            mock.patch.object(
                self.workspace, "_select_topology_port", return_value=17300
            ),
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            status = self.workspace.topology_up(
                "server-review", "default", "default", None, 17300
            )
        self.assertEqual(
            build_resolved.call_args.args[3],
            ["client", "server"],
        )
        self.assertTrue(status["ready"])
        self.assertEqual(status["state_policy"]["mode"], "default")
        self.assertEqual(status["state_policy"]["lifecycle"], "persistent")
        self.assertEqual(status["endpoint"]["port"], 17300)
        self.assertEqual(status["port_reservation"]["port"], 17300)
        self.assertEqual(status["port_reservation"]["topology"], "server-review")
        self.assertEqual(
            status["observation"]["port_reservation"]["lease"], "retained"
        )
        self.assertEqual(status["endpoint"]["fingerprint"], "a" * 64)
        server_runtime = Path(status["services"]["server"]["cwd"])
        generation_root = Path(status["runtime"]["path"])
        mutable_asset_output = Path(
            status["runtime"]["mutable_state_outputs"][0]
        )
        self.assertFalse((server_runtime / "assets").exists())
        self.assertTrue((mutable_asset_output / "data").is_dir())
        self.assertFalse((mutable_asset_output / "data").is_symlink())
        self.assertEqual(
            (mutable_asset_output / "data" / "listing.txt").read_text(),
            "generated\n",
        )
        self.assertTrue(
            (mutable_asset_output / "client-maps" / "incuna_-1.png").is_file()
        )
        self.assertTrue(
            mutable_asset_output.is_relative_to(
                self.workspace._state_location("default")
            )
        )
        manifest_path = generation_root / workspace_module.RUNTIME_GENERATION_MANIFEST
        status_path = (
            self.workspace.paths.topologies / "server-review" / "status.json"
        )
        manifest_record = load_json(manifest_path)
        status_record = load_json(status_path)
        generation_mode = stat.S_IMODE(generation_root.stat().st_mode)
        manifest_mode = stat.S_IMODE(manifest_path.stat().st_mode)
        try:
            generation_root.chmod(0o700)
            manifest_path.chmod(0o600)
            invalid_manifest = dict(manifest_record)
            invalid_manifest["identity"] = {
                **invalid_manifest["identity"],
                "name": "different-topology",
            }
            atomic_json(manifest_path, invalid_manifest)
            invalid_status = dict(status_record)
            invalid_status["runtime"] = dict(invalid_status["runtime"])
            invalid_status["runtime"]["manifest_sha256"] = (
                workspace_module._file_digest(
                    manifest_path, "runtime generation manifest"
                )
            )
            atomic_json(status_path, invalid_status)
            with self.assertRaisesRegex(
                WorkspaceError, "runtime manifest identity is invalid"
            ):
                self.workspace.topology_status("server-review")
        finally:
            atomic_json(manifest_path, manifest_record)
            manifest_path.chmod(manifest_mode)
            generation_root.chmod(generation_mode)
            atomic_json(status_path, status_record)
        self.assertEqual(server_runtime, generation_root / "server")
        self.assertFalse((server_runtime / "maps").is_symlink())
        self.assertEqual(
            (server_runtime / "maps" / "map").read_text(), "shared content\n"
        )
        staged_maps = mutable_asset_output / "client-maps"
        self.assertTrue((staged_maps / "incuna_-1.png").is_file())
        self.assertFalse(staged_maps.is_symlink())
        self.assertTrue(
            (build_root / "runtime" / "client-maps" / "incuna_-1.png").is_file()
        )
        self.assertTrue(
            (build_root / "runtime" / "content" / "maps" / "map").is_file()
        )
        self.assertTrue(
            (build_root / "runtime" / "resources" / "resource").is_file()
        )
        self.assertEqual(
            Path(status["services"]["client"]["cwd"]),
            generation_root / "client",
        )
        self.assertFalse(
            (generation_root / "client" / "src" / "generated.c").exists()
        )
        self.assertEqual(
            (generation_root / "client" / "src" / "authored.c").read_text(),
            "source\n",
        )
        client_log = self.workspace.paths.topologies / "server-review" / "client.log"
        server_log = self.workspace.paths.topologies / "server-review" / "server.log"
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and (
            not client_log.is_file() or "--server=" not in client_log.read_text()
        ):
            time.sleep(0.05)
        self.assertIn("'--port_quic=17300'", server_log.read_text())
        self.assertIn("'--port_mapping=off'", server_log.read_text())
        self.assertIn("'--stun_server=off'", server_log.read_text())
        self.assertRegex(server_log.read_text(), r"'--datapath=/proc/self/fd/\d+'")
        self.assertRegex(
            server_log.read_text(),
            r"'--assetspath=/proc/self/fd/\d+'",
        )
        self.assertIn(
            f"'--server=127.0.0.1 17300 {'a' * 64}'", client_log.read_text()
        )
        self.assertIn("'--connect=127.0.0.1'", client_log.read_text())
        self.assertIn("'--stun_server=off'", client_log.read_text())
        self.assertIn("'--nometa'", client_log.read_text())
        self.assertIn(
            "launch=topology server-review - profile default",
            client_log.read_text(),
        )
        self.assertIn(
            str(
                self.workspace.paths.topologies
                / "server-review"
                / "client-config"
            ),
            client_log.read_text(),
        )
        state = self.workspace._state_location("default")
        identity = self.workspace._state_identity(state)
        with self.assertRaisesRegex(WorkspaceError, "already in use"):
            with exclusive_lock(
                self.workspace._lease_namespace
                / f"state-identity-{identity['device']}-{identity['inode']}.lock",
                "live physical state",
                nonblocking=True,
            ):
                self.fail("supervisor released the physical state lease")
        second_state = self.workspace.state_add("second", None)
        source_lock = resource_lock_path(
            self.workspace._lease_namespace,
            "source",
            self.workspace._source_coordinate(
                "client", self.workspace.paths.repositories / "client"
            ),
        )
        server_source_lock = resource_lock_path(
            self.workspace._lease_namespace,
            "source",
            self.workspace._source_coordinate(
                "server", self.workspace.paths.repositories / "server"
            ),
        )
        profile_lock = (
            self.workspace.paths.builds / "locks" / f"{build_root.name}.lock"
        )
        try:
            with (
                mock.patch.object(
                    self.workspace,
                    "_build_resolved",
                    return_value=second_build_root,
                ),
                mock.patch.object(
                    self.workspace, "_select_topology_port", return_value=17301
                ),
            ):
                second = self.workspace.topology_up(
                    "server-review-two", "default", "second", ["server"], 17301
                )
            self.assertTrue(second["ready"])
            self.assertEqual(second["state_policy"]["mode"], "named")
            self.assertEqual(second["endpoint"]["port"], 17301)
            self.assertNotIn("client", second["dependencies"])
            self.assertNotIn("client", second["services"])
            self.assertNotIn("sound", second)
            for topology_status in (status, second):
                snapshot = Path(topology_status["runtime"]["path"]) / "server"
                self.assertEqual(
                    (snapshot / "maps" / "map").read_text(),
                    "shared content\n",
                )
                self.assertEqual(
                    (snapshot / "resources" / "resource").read_text(),
                    "shared resource\n",
                )
            self.workspace.topology_down("server-review-two", timeout=5)
            (build_root / "runtime" / "content" / "maps" / "map").write_text(
                "rebuilt content\n", encoding="utf-8"
            )
            self.assertEqual(
                (generation_root / "server" / "maps" / "map").read_text(),
                "shared content\n",
            )
            self.assertEqual(
                (build_root / "runtime" / "content" / "maps" / "map").read_text(),
                "rebuilt content\n",
            )
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    Path(f"{state}.lock"), "server state", nonblocking=True
                ):
                    self.fail("supervised state lock unexpectedly became available")
            with exclusive_lock(
                profile_lock, "profile build default", nonblocking=True
            ):
                pass
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    generation_root / workspace_module.RUNTIME_GENERATION_LEASE,
                    "runtime generation",
                    nonblocking=True,
                ):
                    self.fail("live topology released its runtime generation lease")
            self.assertIsNone(
                self.workspace.topology_status("server-review")["observation"][
                    "repository_layout_lease_owner"
                ]
            )

            supervisor = status["supervisor"]
            pidfd = os.pidfd_open(supervisor["pid"])
            try:
                signal.pidfd_send_signal(pidfd, signal.SIGKILL)
            finally:
                os.close(pidfd)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and (
                self.workspace.topology_status("server-review")["observation"][
                    "process_tree_lease"
                ]
                == "retained"
            ):
                time.sleep(0.05)
            orphaned = self.workspace.topology_status("server-review")
            self.assertFalse(orphaned["supervisor"]["running"])
            self.assertFalse(orphaned["ready"])
            self.assertFalse(orphaned["services"]["server"]["running"])
            self.assertEqual(
                orphaned["observation"]["process_tree_lease"], "released"
            )
            recovered = self.workspace.topology_down("server-review", timeout=5)
            self.assertFalse(
                any(service["running"] for service in recovered["services"].values())
            )
            with (
                mock.patch.object(
                    self.workspace, "_build_resolved", return_value=build_root
                ),
                mock.patch.object(
                    self.workspace, "_select_topology_port", return_value=17300
                ),
                mock.patch.object(self.workspace, "_require_client_display"),
            ):
                restarted = self.workspace.topology_up(
                    "server-review", "default", "default", None, 17300
                )
            self.assertNotEqual(
                restarted["control"]["generation"],
                recovered["control"]["generation"],
            )
            self.workspace.topology_down("server-review", timeout=5)
        finally:
            second_remaining = self.workspace.topology_status("server-review-two")
            if second_remaining["supervisor"]["running"] or any(
                service["running"]
                for service in second_remaining["services"].values()
            ):
                self.workspace.topology_down("server-review-two", timeout=5)
            remaining = self.workspace.topology_status("server-review")
            if remaining["supervisor"]["running"] or any(
                service["running"] for service in remaining["services"].values()
            ):
                self.workspace.topology_down("server-review", timeout=5)

        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(
                self.workspace, "_select_topology_port", return_value=17302
            ),
        ):
            executable.write_text(
                "#!/usr/bin/env python3\n"
                "import os, signal, time\n"
                "child = os.fork()\n"
                "if child == 0:\n"
                "    for fd in os.listdir('/proc/self/fd'):\n"
                "        try:\n"
                "            target = os.readlink('/proc/self/fd/' + fd)\n"
                "            if target.endswith('/process-tree.lease'):\n"
                "                os.close(int(fd))\n"
                "        except OSError:\n"
                "            pass\n"
                "    signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
                "    while True:\n"
                "        time.sleep(0.1)\n"
                "open('data/tmp/descendant.pid', 'w', encoding='utf-8').write(str(child))\n"
                f"print('QUIC certificate SHA-256: {'a' * 64}', flush=True)\n"
                "print('Server ready. Waiting for connections...', flush=True)\n"
                "while True:\n"
                "    time.sleep(0.1)\n",
                encoding="utf-8",
            )
            server_only = self.workspace.topology_up(
                "server-lease", "default", "default", ["server"], 17302
            )
        try:
            self.assertTrue(server_only["ready"])
            for path, description in (
                (server_source_lock, "server source"),
                (profile_lock, "profile build default"),
            ):
                with exclusive_lock(path, description, nonblocking=True):
                    pass
            descendant_path = (
                self.workspace._state_location("default")
                / "tmp"
                / "descendant.pid"
            )
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and not descendant_path.is_file():
                time.sleep(0.05)
            descendant_pid = int(descendant_path.read_text(encoding="utf-8"))
            service = server_only["services"]["server"]
            service_pidfd = os.pidfd_open(service["pid"])
            try:
                signal.pidfd_send_signal(service_pidfd, signal.SIGTERM)
            finally:
                os.close(service_pidfd)
            deadline = time.monotonic() + 5
            while (
                time.monotonic() < deadline
                and self.workspace.topology_status("server-lease")["services"][
                    "server"
                ]["running"]
            ):
                time.sleep(0.05)
            self.assertTrue(
                self.workspace.topology_status("server-lease")["supervisor"][
                    "running"
                ]
            )
            supervisor = server_only["supervisor"]
            pidfd = os.pidfd_open(supervisor["pid"])
            try:
                signal.pidfd_send_signal(pidfd, signal.SIGKILL)
            finally:
                os.close(pidfd)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and (
                self.workspace.topology_status("server-lease")["observation"][
                    "process_tree_lease"
                ]
                == "retained"
            ):
                time.sleep(0.05)
            orphaned = self.workspace.topology_status("server-lease")
            self.assertFalse(orphaned["supervisor"]["running"])
            self.assertFalse(orphaned["services"]["server"]["running"])
            self.assertTrue(Path(f"/proc/{descendant_pid}").exists())
            self.assertEqual(
                orphaned["observation"]["process_tree_lease"], "released"
            )
            for path, description in (
                (server_source_lock, "server source"),
                (profile_lock, "profile build default"),
            ):
                with exclusive_lock(path, description, nonblocking=True):
                    pass
            self.workspace.topology_down("server-lease", timeout=0.5)
        finally:
            if "descendant_pid" in locals() and Path(
                f"/proc/{descendant_pid}"
            ).exists():
                try:
                    os.kill(descendant_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            self.workspace.topology_down("server-lease", timeout=5)

        with exclusive_lock(Path(f"{state}.lock"), "server state", nonblocking=True):
            pass
        with exclusive_lock(
            Path(f"{second_state}.lock"), "server state", nonblocking=True
        ):
            pass
        with exclusive_lock(
            server_source_lock, "server source", nonblocking=True
        ):
            pass
        with exclusive_lock(
            profile_lock, "profile build default", nonblocking=True
        ):
            pass

    def test_temporary_topology_state_clean_retain_and_promote_lifecycle(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for filename in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / filename).write_text("test\n", encoding="utf-8")
        rendezvous = self.root / "temporary-state-rendezvous"
        rendezvous.mkdir()
        build_root = self.workspace.paths.builds / "temporary-state-server"
        self.make_rendezvous_server_build(
            build_root, rendezvous, "temporary.bound", peers=1
        )

        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            first = self.workspace.topology_up(
                "temporary-clean", "default", None, ["server"], 0
            )
        first_path = Path(first["state_policy"]["path"])
        self.assertEqual(first["state_policy"]["mode"], "temporary")
        self.assertEqual(first["state_policy"]["lifecycle"], "disposable")
        self.assertEqual(first["state"], str(first_path))
        self.assertTrue(first_path.is_dir())
        self.assertEqual(
            (first_path / "rendezvous-state-proof").read_text(encoding="utf-8"),
            "pinned\n",
        )
        self.assertNotIn(str(first_path), self.workspace._load_states().values())
        tombstone = first_path.parent / f".{first_path.name}.removal-pending"
        real_rename_no_replace_at = workspace_module.rename_no_replace_at

        def interrupt_before_removal_rename(
            source_parent: int,
            source: str,
            target_parent: int,
            target: str,
        ) -> None:
            if source == first_path.name and target == tombstone.name:
                raise WorkspaceError("simulated pre-rename interruption")
            real_rename_no_replace_at(
                source_parent, source, target_parent, target
            )

        with (
            mock.patch(
                "atrinik_workspace.workspace.rename_no_replace_at",
                side_effect=interrupt_before_removal_rename,
            ),
            self.assertRaisesRegex(WorkspaceError, "pre-rename interruption"),
        ):
            self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertEqual(
            self.workspace.topology_status("temporary-clean")["state_policy"][
                "lifecycle"
            ],
            "removal-pending",
        )
        with self.assertRaisesRegex(
            WorkspaceError, "removal has already begun"
        ):
            self.workspace.topology_down(
                "temporary-clean", timeout=5, retain_state=True
            )
        topology_preview = self.workspace.cleanup(
            ["topologies"], 0, [], False
        )
        pending_topology_item = next(
            item
            for item in topology_preview["items"]
            if item.get("name") == "temporary-clean"
        )
        self.assertEqual(pending_topology_item["disposition"], "protected")
        self.assertIn(
            "temporary_state_recovery_pending",
            pending_topology_item["reasons"],
        )
        pending_link_target = self.root / "pending-link-target"
        pending_link_target.write_text("preserve\n", encoding="utf-8")
        pending_link = first_path / "pending-link"
        pending_link.symlink_to(pending_link_target)
        with self.assertRaisesRegex(WorkspaceError, "symbolic link"):
            self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertTrue(first_path.is_dir())
        self.assertTrue(pending_link.is_symlink())
        pending_link.unlink()
        displaced = self.root / "displaced-temporary-state"
        first_path.rename(displaced)
        with self.assertRaisesRegex(WorkspaceError, "ownership evidence is missing"):
            self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertTrue(displaced.is_dir())
        missing_evidence_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        missing_evidence_item = next(
            item
            for item in missing_evidence_preview["items"]
            if item["path"] == str(first_path)
        )
        self.assertEqual(missing_evidence_item["disposition"], "protected")
        self.assertIn(
            "temporary_state_ownership_evidence_missing",
            missing_evidence_item["reasons"],
        )
        missing_evidence_apply = self.workspace.cleanup(
            ["temporary-states"], 0, [], True
        )
        missing_evidence_applied_item = next(
            item
            for item in missing_evidence_apply["items"]
            if item["path"] == str(first_path)
        )
        self.assertEqual(
            missing_evidence_applied_item["disposition"], "protected"
        )
        self.assertTrue(displaced.is_dir())
        state_lock = Path(f"{first_path}.lock")
        displaced_lock = self.root / "displaced-temporary-state.lock"
        state_lock.rename(displaced_lock)
        missing_all_evidence = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        missing_all_item = next(
            item
            for item in missing_all_evidence["items"]
            if item["path"] == str(first_path)
        )
        self.assertEqual(missing_all_item["disposition"], "protected")
        self.assertIn(
            "temporary_state_ownership_evidence_missing",
            missing_all_item["reasons"],
        )
        self.assertIn("state_lease_unverifiable", missing_all_item["reasons"])
        pending_status_path = (
            self.workspace.paths.topologies / "temporary-clean" / "status.json"
        )
        pending_status = load_json(pending_status_path)
        malformed_created_at = copy.deepcopy(pending_status)
        malformed_created_at["state_policy"]["created_at"] = "not-a-timestamp"
        atomic_json(pending_status_path, malformed_created_at)
        malformed_pending = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        malformed_pending_item = next(
            item
            for item in malformed_pending["items"]
            if item["path"] == str(first_path)
        )
        self.assertEqual(malformed_pending_item["disposition"], "protected")
        self.assertIn(
            "invalid_temporary_state", malformed_pending_item["reasons"]
        )
        atomic_json(pending_status_path, pending_status)
        container = first_path.parent
        displaced_container = container.with_name("temporary-states.displaced")
        container.rename(displaced_container)
        missing_container = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        missing_container_item = next(
            item
            for item in missing_container["items"]
            if item["path"] == str(first_path)
        )
        self.assertEqual(missing_container_item["disposition"], "protected")
        self.assertIn(
            "temporary_state_ownership_evidence_missing",
            missing_container_item["reasons"],
        )
        displaced_container.rename(container)
        displaced_lock.rename(state_lock)
        displaced.rename(first_path)
        with (
            mock.patch(
                "atrinik_workspace.workspace.remove_owned_tree",
                side_effect=WorkspaceError("simulated removal interruption"),
            ),
            self.assertRaisesRegex(WorkspaceError, "simulated removal interruption"),
        ):
            self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertEqual(
            self.workspace.topology_status("temporary-clean")["state_policy"][
                "lifecycle"
            ],
            "removal-pending",
        )
        self.assertFalse(first_path.exists())
        self.assertTrue(tombstone.is_dir())
        tombstone_link = tombstone / "late-link"
        tombstone_link.symlink_to(pending_link_target)
        with self.assertRaisesRegex(WorkspaceError, "linked state"):
            self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertTrue(tombstone.is_dir())
        self.assertTrue(tombstone_link.is_symlink())
        tombstone_link.unlink()
        tombstone_hardlink = self.root / "pending-hardlink"
        os.link(tombstone / "motd", tombstone_hardlink)
        with self.assertRaisesRegex(WorkspaceError, "linked state"):
            self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertTrue(tombstone.is_dir())
        tombstone_hardlink.unlink()
        stopped = self.workspace.topology_down("temporary-clean", timeout=5)
        self.assertEqual(stopped["state_policy"]["lifecycle"], "removed")
        self.assertFalse(first_path.exists())
        self.assertFalse(tombstone.exists())
        self.assertFalse(Path(f"{first_path}.lock").exists())
        clean_status_path = (
            self.workspace.paths.topologies / "temporary-clean" / "status.json"
        )
        clean_status_record = load_json(clean_status_path)
        missing_server_state = copy.deepcopy(clean_status_record)
        missing_server_state["state"] = None
        missing_server_state["state_policy"] = None
        with self.assertRaisesRegex(WorkspaceError, "state policy is invalid"):
            self.workspace._validate_topology_state_policy(
                "temporary-clean",
                clean_status_path.parent,
                missing_server_state,
                missing_server_state["control"],
            )
        topology_preview = self.workspace.cleanup(["topologies"], 0, [], False)
        temporary_clean_item = next(
            item
            for item in topology_preview["items"]
            if item.get("name") == "temporary-clean"
        )
        self.assertEqual(
            temporary_clean_item["disposition"], "eligible", temporary_clean_item
        )
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            restarted_clean = self.workspace.topology_up(
                "temporary-clean", "default", None, ["server"], 0
            )
        self.assertNotEqual(
            restarted_clean["control"]["generation"],
            first["control"]["generation"],
        )
        self.assertNotEqual(
            restarted_clean["state_policy"]["path"],
            first["state_policy"]["path"],
        )
        restarted_path = Path(restarted_clean["state_policy"]["path"])
        unsafe_target = self.root / "temporary-clean-symlink-target"
        unsafe_target.write_text("preserve\n", encoding="utf-8")
        (restarted_path / "unsafe-link").symlink_to(unsafe_target)
        with self.assertRaisesRegex(
            WorkspaceError, "retained because its integrity could not be proved"
        ):
            self.workspace.topology_down("temporary-clean", timeout=5)
        linked_down = self.workspace.topology_status("temporary-clean")
        self.assertEqual(linked_down["state_policy"]["lifecycle"], "retained")
        self.assertTrue(restarted_path.is_dir())
        self.assertEqual(unsafe_target.read_text(encoding="utf-8"), "preserve\n")

        (rendezvous / "temporary.bound").unlink()
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            hardlinked = self.workspace.topology_up(
                "temporary-hardlinked", "default", None, ["server"], 0
            )
        hardlinked_path = Path(hardlinked["state_policy"]["path"])
        hardlink_target = self.root / "temporary-clean-hardlink"
        os.link(hardlinked_path / "motd", hardlink_target)
        with self.assertRaisesRegex(
            WorkspaceError, "retained because its integrity could not be proved"
        ):
            self.workspace.topology_down("temporary-hardlinked", timeout=5)
        self.assertEqual(
            self.workspace.topology_status("temporary-hardlinked")["state_policy"][
                "lifecycle"
            ],
            "retained",
        )
        self.assertTrue(hardlinked_path.is_dir())
        hardlink_target.unlink()

        (rendezvous / "temporary.bound").unlink()
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            malformed = self.workspace.topology_up(
                "temporary-malformed", "default", None, ["server"], 0
            )
        malformed_path = Path(malformed["state_policy"]["path"])
        (malformed_path / "motd").unlink()
        with self.assertRaisesRegex(
            WorkspaceError, "retained because its integrity could not be proved"
        ):
            self.workspace.topology_down("temporary-malformed", timeout=5)
        self.assertEqual(
            self.workspace.topology_status("temporary-malformed")["state_policy"][
                "lifecycle"
            ],
            "retained",
        )
        self.assertTrue(malformed_path.is_dir())

        external_state = self.root / "external-supervised-state"
        shutil.copytree(source / "install_data", external_state)
        (external_state / "external-sentinel").write_text(
            "preserve\n", encoding="utf-8"
        )
        self.workspace.state_add("external-supervised", external_state)
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            external_status = self.workspace.topology_up(
                "external-supervised", "default", "external-supervised", ["server"], 0
            )
        self.assertEqual(
            external_status["state_policy"]["owner"], {"kind": "external"}
        )
        self.assertEqual(
            external_status["state_policy"]["lifecycle"],
            "persistent-external",
        )
        external_runtime_output = Path(
            external_status["runtime"]["mutable_state_outputs"][0]
        )
        self.assertTrue(external_runtime_output.is_dir())
        self.workspace.topology_down("external-supervised", timeout=5)
        self.assertFalse(external_runtime_output.exists())
        self.assertEqual(
            (external_state / "external-sentinel").read_text(encoding="utf-8"),
            "preserve\n",
        )

        (rendezvous / "temporary.bound").unlink()
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            retained = self.workspace.topology_up(
                "temporary-retained", "default", None, ["server"], 0
            )
        retained_path = Path(retained["state_policy"]["path"])
        self.assertEqual(retained["state_policy"]["profile"], "default")
        retained_status_path = (
            self.workspace.paths.topologies / "temporary-retained" / "status.json"
        )
        implementation_marker = (
            retained_path / workspace_module.STATE_IMPLEMENTATION_MARKER
        )
        implementation_payload = implementation_marker.read_bytes()
        implementation_marker.unlink()
        with self.assertRaisesRegex(
            WorkspaceError, "retained because its integrity could not be proved"
        ):
            self.workspace.topology_down("temporary-retained", timeout=5)
        self.assertTrue(retained_path.is_dir())
        self.assertEqual(
            load_json(retained_status_path)["state_policy"]["lifecycle"],
            "retained",
        )
        implementation_marker.write_bytes(implementation_payload)
        stopped = self.workspace.topology_down(
            "temporary-retained", timeout=5, retain_state=True
        )
        self.assertEqual(stopped["state_policy"]["lifecycle"], "retained")
        self.assertTrue(retained_path.is_dir())
        with self.assertRaisesRegex(
            WorkspaceError, "only be registered through state promote"
        ):
            self.workspace.state_add("unsafe-temporary-alias", retained_path)
        retained_status_path = (
            self.workspace.paths.topologies / "temporary-retained" / "status.json"
        )
        retained_record = load_json(retained_status_path)
        invalid_removed = copy.deepcopy(retained_record)
        invalid_removed["state_policy"]["lifecycle"] = "removed"
        atomic_json(retained_status_path, invalid_removed)
        with self.assertRaisesRegex(
            WorkspaceError, "removed temporary topology state still exists"
        ):
            self.workspace.topology_status("temporary-retained")
        atomic_json(retained_status_path, retained_record)
        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            self.assertRaisesRegex(
                WorkspaceError, "retains temporary generation state"
            ),
        ):
            self.workspace.topology_up(
                "temporary-retained", "default", None, ["server"], 0
            )
        with mock.patch("builtins.print") as output:
            self.workspace.topology_logs(
                "temporary-retained", "server", 1, False
            )
        policy_header = output.call_args_list[0].args[0]
        self.assertIn("state-policy mode=temporary", policy_header)
        self.assertIn("lifecycle=retained", policy_header)
        self.assertIn(str(retained_path), policy_header)
        self.assertEqual(
            load_json(
                retained_path / workspace_module.TEMPORARY_STATE_METADATA
            )["state_policy"]["lifecycle"],
            "disposable",
        )
        self.assertEqual(
            self.workspace.topology_down("temporary-retained", timeout=5)[
                "state_policy"
            ]["lifecycle"],
            "retained",
        )
        retained_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        retained_item = next(
            item
            for item in retained_preview["items"]
            if item["path"] == str(retained_path)
        )
        self.assertEqual(retained_item["disposition"], "protected")
        self.assertIn("temporary_state_retained", retained_item["reasons"])
        implementation_marker.unlink()
        with self.assertRaisesRegex(
            WorkspaceError, "implementation marker"
        ):
            self.workspace.state_promote(
                "temporary-retained", "markerless-promotion"
            )
        self.assertNotIn("markerless-promotion", self.workspace._load_states())
        implementation_marker.write_bytes(implementation_payload)
        displaced_state = retained_path.with_name("promotion-displaced")
        replacement_sentinel = retained_path / "replacement-sentinel"
        real_durable_json_at = workspace_module.durable_atomic_json_at
        swapped = False

        def swap_before_provenance(
            directory_fd: int, name: str, value: object
        ) -> None:
            nonlocal swapped
            if name == workspace_module.PROMOTED_STATE_METADATA and not swapped:
                swapped = True
                retained_path.rename(displaced_state)
                retained_path.mkdir()
                replacement_sentinel.write_text(
                    "replacement\n", encoding="utf-8"
                )
            real_durable_json_at(directory_fd, name, value)

        with (
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json_at",
                side_effect=swap_before_provenance,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "temporary state changed during promotion"
            ),
        ):
            self.workspace.state_promote(
                "temporary-retained", "promotion-swap-review"
            )
        self.assertTrue(swapped)
        self.assertEqual(
            replacement_sentinel.read_text(encoding="utf-8"), "replacement\n"
        )
        self.assertNotIn("promotion-swap-review", self.workspace._load_states())
        shutil.rmtree(retained_path)
        displaced_state.rename(retained_path)
        (retained_path / workspace_module.PROMOTED_STATE_METADATA).unlink()
        atomic_json(retained_status_path, retained_record)
        real_policy_write = self.workspace._write_temporary_state_policy
        final_swap = False

        def swap_before_promoted_status(
            topology_name: str,
            current: dict[str, object],
            policy: dict[str, object],
        ) -> dict[str, object]:
            nonlocal final_swap
            if policy.get("lifecycle") == "promoted" and not final_swap:
                final_swap = True
                retained_path.rename(displaced_state)
                retained_path.mkdir()
                replacement_sentinel.write_text(
                    "final replacement\n", encoding="utf-8"
                )
            return real_policy_write(topology_name, current, policy)

        with (
            mock.patch.object(
                self.workspace,
                "_write_temporary_state_policy",
                side_effect=swap_before_promoted_status,
            ),
            self.assertRaises(WorkspaceError),
        ):
            self.workspace.state_promote(
                "temporary-retained", "promotion-final-swap-review"
            )
        self.assertTrue(final_swap)
        self.assertNotIn(
            "promotion-final-swap-review", self.workspace._load_states()
        )
        self.assertEqual(
            load_json(retained_status_path)["state_policy"]["lifecycle"],
            "promotion-pending",
        )
        shutil.rmtree(retained_path)
        displaced_state.rename(retained_path)
        (retained_path / workspace_module.PROMOTED_STATE_METADATA).unlink()
        atomic_json(retained_status_path, retained_record)
        sessions = [Workspace(self.wrapper), Workspace(self.wrapper)]
        with ThreadPoolExecutor(max_workers=2) as executor:
            futures = [
                executor.submit(
                    sessions[index].state_promote,
                    "temporary-retained",
                    f"promoted-review-{index}",
                )
                for index in range(2)
            ]
            outcomes: list[dict[str, object] | WorkspaceError] = []
            for future in futures:
                try:
                    outcomes.append(future.result(timeout=10))
                except WorkspaceError as error:
                    outcomes.append(error)
        successes = [value for value in outcomes if isinstance(value, dict)]
        failures = [value for value in outcomes if isinstance(value, WorkspaceError)]
        self.assertEqual(len(successes), 1)
        self.assertEqual(len(failures), 1)
        promoted = successes[0]
        self.assertEqual(promoted["path"], str(retained_path))
        self.assertEqual(
            promoted["state_policy"]["lifecycle"], "promoted"
        )
        self.assertEqual(
            self.workspace._state_location(str(promoted["name"])), retained_path
        )
        self.assertEqual(
            self.workspace.state_promote(
                "temporary-retained", str(promoted["name"])
            ),
            promoted,
        )
        origin_status = (
            self.workspace.paths.topologies / "temporary-retained" / "status.json"
        )
        hidden_status = origin_status.with_name("status.hidden")
        origin_status.rename(hidden_status)
        try:
            promoted_summary = self.workspace.topology_summary(
                "default", str(promoted["name"]), ["server"]
            )
        finally:
            hidden_status.rename(origin_status)
        self.assertEqual(
            promoted_summary["state_policy"]["owner"],
            {
                "kind": "promoted-topology-state",
                "topology": "temporary-retained",
                "generation": retained["control"]["generation"],
            },
        )
        self.assertEqual(
            promoted_summary["state_policy"]["lifecycle"],
            "persistent-promoted",
        )
        provenance = retained_path / workspace_module.PROMOTED_STATE_METADATA
        replaced_promoted = retained_path.with_name("promoted-original")
        retained_path.rename(replaced_promoted)
        retained_path.mkdir()
        shutil.copy2(
            replaced_promoted / workspace_module.PROMOTED_STATE_METADATA,
            provenance,
        )
        try:
            with self.assertRaisesRegex(
                WorkspaceError, "promoted state provenance is invalid"
            ):
                self.workspace.topology_summary(
                    "default", str(promoted["name"]), ["server"]
                )
        finally:
            shutil.rmtree(retained_path)
            replaced_promoted.rename(retained_path)
        provenance = retained_path / workspace_module.PROMOTED_STATE_METADATA
        provenance.unlink()
        origin_status.rename(hidden_status)
        ownership_marker = retained_path / MANAGED_MARKER
        hidden_ownership = retained_path / "managed.hidden"
        ownership_marker.rename(hidden_ownership)
        try:
            with self.assertRaisesRegex(
                WorkspaceError, "promoted state provenance is missing"
            ):
                self.workspace.topology_summary(
                    "default", str(promoted["name"]), ["server"]
                )
        finally:
            hidden_ownership.rename(ownership_marker)
            hidden_status.rename(origin_status)
        self.workspace.state_promote(
            "temporary-retained", str(promoted["name"])
        )
        self.assertTrue(provenance.is_file())
        (rendezvous / "temporary.bound").unlink()
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            retry = self.workspace.topology_up(
                "temporary-promotion-retry", "default", None, ["server"], 0
            )
        retry_path = Path(retry["state_policy"]["path"])
        self.workspace.topology_down(
            "temporary-promotion-retry", timeout=10, retain_state=True
        )
        write_policy = self.workspace._write_temporary_state_policy
        writes = 0

        def interrupt_after_registry(
            topology: str,
            current: dict[str, object],
            policy: dict[str, object],
        ) -> dict[str, object]:
            nonlocal writes
            writes += 1
            if writes == 2:
                raise WorkspaceError("simulated promoted-status interruption")
            return write_policy(topology, current, policy)

        with (
            mock.patch.object(
                self.workspace,
                "_write_temporary_state_policy",
                side_effect=interrupt_after_registry,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "simulated promoted-status interruption"
            ),
        ):
            self.workspace.state_promote(
                "temporary-promotion-retry", "promoted-retry"
            )
        with self.assertRaisesRegex(WorkspaceError, "state does not exist"):
            self.workspace._state_location("promoted-retry")
        self.assertEqual(
            self.workspace.topology_status("temporary-promotion-retry")[
                "state_policy"
            ]["lifecycle"],
            "promotion-pending",
        )
        with self.assertRaisesRegex(
            WorkspaceError, "promotion target cannot change"
        ):
            self.workspace.state_promote(
                "temporary-promotion-retry", "different-promoted-retry"
            )
        self.assertEqual(
            self.workspace.topology_status("temporary-promotion-retry")[
                "state_policy"
            ]["name"],
            "promoted-retry",
        )
        self.assertEqual(
            self.workspace.state_promote(
                "temporary-promotion-retry", "promoted-retry"
            )["state_policy"]["lifecycle"],
            "promoted",
        )

    def test_temporary_topology_state_is_retained_after_supervisor_crash(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for filename in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / filename).write_text("test\n", encoding="utf-8")
        rendezvous = self.root / "temporary-crash-rendezvous"
        rendezvous.mkdir()
        build_root = self.workspace.paths.builds / "temporary-crash-server"
        self.make_rendezvous_server_build(
            build_root, rendezvous, "temporary.bound", peers=1
        )
        with mock.patch.object(
            self.workspace, "_build_resolved", return_value=build_root
        ):
            status = self.workspace.topology_up(
                "temporary-crash", "default", None, ["server"], 0
            )
        state = Path(status["state_policy"]["path"])
        live_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        live_item = next(
            item for item in live_preview["items"] if item["path"] == str(state)
        )
        self.assertEqual(live_item["disposition"], "protected")
        self.assertIn("live_topology", live_item["reasons"])
        supervisor = status["supervisor"]
        pidfd = os.pidfd_open(supervisor["pid"])
        try:
            signal.pidfd_send_signal(pidfd, signal.SIGKILL)
        finally:
            os.close(pidfd)
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            crashed = self.workspace.topology_status("temporary-crash")
            if crashed["observation"]["process_tree_lease"] == "released":
                break
            time.sleep(0.05)
        self.assertEqual(crashed["state_policy"]["lifecycle"], "disposable")
        self.assertTrue(state.is_dir())
        recovered = self.workspace.topology_down("temporary-crash", timeout=5)
        self.assertEqual(recovered["state_policy"]["lifecycle"], "disposable")
        self.assertTrue(state.is_dir())
        status_path = (
            self.workspace.paths.topologies / "temporary-crash" / "status.json"
        )
        raw_status = load_json(status_path)
        without_port_evidence = copy.deepcopy(recovered)
        without_port_evidence["endpoint"] = None
        without_port_evidence["services"] = {}
        without_port_evidence["observation"]["port_reservation"] = None
        with mock.patch.object(
            self.workspace,
            "topology_status",
            return_value=without_port_evidence,
        ):
            missing_port = self.workspace.cleanup(
                ["temporary-states"], 0, [], False
            )
        missing_port_item = next(
            item for item in missing_port["items"] if item["path"] == str(state)
        )
        self.assertEqual(missing_port_item["disposition"], "protected")
        self.assertIn(
            "port_reservation_lease_unverifiable",
            missing_port_item["reasons"],
        )
        state_lock = Path(f"{state}.lock")
        saved_state_lock = state_lock.with_suffix(".saved-lock")
        state_lock.rename(saved_state_lock)
        missing_lease = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        missing_item = next(
            item for item in missing_lease["items"] if item["path"] == str(state)
        )
        self.assertEqual(missing_item["disposition"], "protected")
        self.assertIn("state_lease_unverifiable", missing_item["reasons"])
        state_lock.touch(mode=0o600)
        replaced_lease = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        replaced_item = next(
            item for item in replaced_lease["items"] if item["path"] == str(state)
        )
        self.assertEqual(replaced_item["disposition"], "protected")
        self.assertIn(
            "state_lease_identity_mismatch", replaced_item["reasons"]
        )
        state_lock.unlink()
        saved_state_lock.rename(state_lock)
        registered_alias = self.root / "registered-physical-alias"
        registered_alias.mkdir()
        real_state_identity = self.workspace._state_identity

        def alias_identity(path: Path) -> dict[str, int]:
            if path == registered_alias:
                return crashed["state_policy"]["identity"]
            return real_state_identity(path)

        with (
            mock.patch.object(
                self.workspace,
                "_load_states",
                return_value={"registered-alias": str(registered_alias)},
            ),
            mock.patch.object(
                self.workspace,
                "_state_identity",
                side_effect=alias_identity,
            ),
        ):
            aliased = self.workspace.cleanup(
                ["temporary-states"], 0, [], False
            )
        aliased_item = next(
            item for item in aliased["items"] if item["path"] == str(state)
        )
        self.assertEqual(aliased_item["disposition"], "protected")
        self.assertIn("registered_state", aliased_item["reasons"])
        real_mount_id = cleanup_module._descriptor_mount_id

        def simulated_root_mount_id(
            descriptor: int,
        ) -> int | tuple[int, int]:
            target = Path(os.readlink(f"/proc/self/fd/{descriptor}"))
            if target == state:
                return 999999997
            return real_mount_id(descriptor)

        with mock.patch.object(
            cleanup_module,
            "_descriptor_mount_id",
            side_effect=simulated_root_mount_id,
        ):
            root_mount_preview = self.workspace.cleanup(
                ["temporary-states"], 0, [], False
            )
        root_mount_item = next(
            item
            for item in root_mount_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(root_mount_item["disposition"], "protected")
        self.assertIn(
            "filesystem_traversal_error", root_mount_item["reasons"]
        )
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            with (
                mock.patch.object(
                    workspace_module,
                    "_descriptor_mount_id",
                    side_effect=simulated_root_mount_id,
                ),
                self.assertRaisesRegex(WorkspaceError, "root.*mount"),
            ):
                self.workspace._validate_temporary_state_integrity(
                    state_fd, state
                )
        finally:
            os.close(state_fd)

        def simulated_mount_id(descriptor: int) -> int | tuple[int, int]:
            target = Path(os.readlink(f"/proc/self/fd/{descriptor}"))
            if target == state / "keys":
                return 999999999
            return real_mount_id(descriptor)

        with mock.patch.object(
            cleanup_module,
            "_descriptor_mount_id",
            side_effect=simulated_mount_id,
        ):
            mounted_preview = self.workspace.cleanup(
                ["temporary-states"], 0, [], False
            )
        mounted_item = next(
            item
            for item in mounted_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(mounted_item["disposition"], "protected")
        self.assertIn("filesystem_traversal_error", mounted_item["reasons"])

        def simulated_file_mount_id(
            descriptor: int,
        ) -> int | tuple[int, int]:
            target = Path(os.readlink(f"/proc/self/fd/{descriptor}"))
            if target == state / "motd":
                return 999999998
            return real_mount_id(descriptor)

        with mock.patch.object(
            cleanup_module,
            "_descriptor_mount_id",
            side_effect=simulated_file_mount_id,
        ):
            mounted_file_preview = self.workspace.cleanup(
                ["temporary-states"], 0, [], False
            )
        mounted_file_item = next(
            item
            for item in mounted_file_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(mounted_file_item["disposition"], "protected")
        self.assertIn(
            "filesystem_traversal_error", mounted_file_item["reasons"]
        )
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            with (
                mock.patch.object(
                    workspace_module,
                    "_descriptor_mount_id",
                    side_effect=simulated_file_mount_id,
                ),
                self.assertRaisesRegex(WorkspaceError, "crossed a mount"),
            ):
                self.workspace._validate_temporary_state_integrity(
                    state_fd, state
                )
        finally:
            os.close(state_fd)
        required_file = state / "motd"
        saved_required_file = state / "motd.saved"
        required_file.rename(saved_required_file)
        malformed_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        malformed_item = next(
            item
            for item in malformed_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(malformed_item["disposition"], "protected")
        self.assertIn("malformed_state", malformed_item["reasons"])
        malformed_apply = self.workspace.cleanup(
            ["temporary-states"], 0, [], True
        )
        malformed_applied_item = next(
            item
            for item in malformed_apply["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(malformed_applied_item["disposition"], "protected")
        self.assertTrue(state.is_dir())
        saved_required_file.rename(required_file)
        required_directory = state / "keys"
        saved_required_directory = state / "keys.saved"
        required_directory.rename(saved_required_directory)
        missing_directory_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        missing_directory_item = next(
            item
            for item in missing_directory_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(missing_directory_item["disposition"], "protected")
        self.assertIn("malformed_state", missing_directory_item["reasons"])
        missing_directory_apply = self.workspace.cleanup(
            ["temporary-states"], 0, [], True
        )
        missing_directory_applied_item = next(
            item
            for item in missing_directory_apply["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(
            missing_directory_applied_item["disposition"], "protected"
        )
        self.assertTrue(state.is_dir())
        saved_required_directory.rename(required_directory)
        special = state / "unsafe-fifo"
        os.mkfifo(special)
        special_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        special_item = next(
            item
            for item in special_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(special_item["disposition"], "protected")
        self.assertIn("malformed_state", special_item["reasons"])
        special_apply = self.workspace.cleanup(
            ["temporary-states"], 0, [], True
        )
        special_applied_item = next(
            item
            for item in special_apply["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(special_applied_item["disposition"], "protected")
        self.assertTrue(state.is_dir())
        special.unlink()
        for metadata_path in (
            state / MANAGED_MARKER,
            state / workspace_module.TEMPORARY_STATE_METADATA,
            state.parent / MANAGED_MARKER,
            state.parent.parent / MANAGED_MARKER,
        ):
            metadata_payload = metadata_path.read_bytes()
            metadata_path.unlink()
            os.mkfifo(metadata_path)
            fifo_preview = self.workspace.cleanup(
                ["temporary-states"], 0, [], False
            )
            fifo_item = next(
                item
                for item in fifo_preview["items"]
                if item["path"] == str(state)
            )
            self.assertEqual(fifo_item["disposition"], "protected")
            self.assertTrue(
                {"invalid_temporary_state", "invalid_temporary_state_container"}
                & set(fifo_item["reasons"])
            )
            metadata_path.unlink()
            metadata_path.write_bytes(metadata_payload)
        state_identity = self.workspace._state_identity(state)
        removal_tombstone = workspace_module._owned_tree_tombstone_path(
            state, state_identity
        )
        state.rename(removal_tombstone)
        creation_path = removal_tombstone / workspace_module.TEMPORARY_STATE_METADATA
        creation_payload = creation_path.read_bytes()
        creation_path.unlink()
        os.mkfifo(creation_path)
        tombstone_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        tombstone_item = next(
            item
            for item in tombstone_preview["items"]
            if item["path"] == str(removal_tombstone)
        )
        self.assertEqual(tombstone_item["disposition"], "protected")
        creation_path.unlink()
        symlink_target = self.root / "temporary-state-creation-record"
        symlink_target.write_bytes(creation_payload)
        creation_path.symlink_to(symlink_target)
        symlink_tombstone_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        symlink_tombstone_item = next(
            item
            for item in symlink_tombstone_preview["items"]
            if item["path"] == str(removal_tombstone)
        )
        self.assertEqual(symlink_tombstone_item["disposition"], "protected")
        creation_path.unlink()
        creation_path.write_bytes(creation_payload)
        removal_tombstone.rename(state)

        status_path = state.parent.parent / "status.json"
        status_payload = status_path.read_bytes()
        status_path.unlink()
        os.mkfifo(status_path)
        invalid_status_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        invalid_status_item = next(
            item
            for item in invalid_status_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(invalid_status_item["disposition"], "protected")
        status_path.unlink()
        status_symlink_target = self.root / "temporary-topology-status"
        status_symlink_target.write_bytes(status_payload)
        status_path.symlink_to(status_symlink_target)
        symlink_status_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        symlink_status_item = next(
            item
            for item in symlink_status_preview["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(symlink_status_item["disposition"], "protected")
        status_path.unlink()
        status_path.write_bytes(status_payload)
        linked_file = next(
            path
            for path in state.rglob("*")
            if path.is_file()
            and not path.is_symlink()
            and path.name
            not in {
                MANAGED_MARKER,
                workspace_module.TEMPORARY_STATE_METADATA,
                workspace_module.STATE_IMPLEMENTATION_MARKER,
            }
        )
        external_link = self.root / "temporary-state-hardlink"
        os.link(linked_file, external_link)
        symlink = state / "unsafe-link"
        symlink.symlink_to(external_link)
        linked_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        linked_item = next(
            item for item in linked_preview["items"] if item["path"] == str(state)
        )
        self.assertEqual(linked_item["disposition"], "protected")
        self.assertIn("linked_state", linked_item["reasons"])
        self.assertFalse(any(key.startswith("_") for key in linked_item))
        symlink.unlink()
        external_link.unlink()
        preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        candidate = next(
            item for item in preview["items"] if item["path"] == str(state)
        )
        self.assertEqual(candidate["disposition"], "eligible")
        pending_state = state.parent / f".{state.name}.removal-pending"

        real_rmdir = os.rmdir

        def interrupt_after_root_tombstone(
            path: object, *args: object, **kwargs: object
        ) -> None:
            if Path(path).name == pending_state.name:
                raise PermissionError("simulated cleanup removal interruption")
            real_rmdir(path, *args, **kwargs)

        with mock.patch(
            "atrinik_workspace.workspace.os.rmdir",
            side_effect=interrupt_after_root_tombstone,
        ):
            interrupted = self.workspace.cleanup(
                ["temporary-states"], 0, [], True
            )
        interrupted_item = next(
            item for item in interrupted["items"] if item["path"] == str(state)
        )
        self.assertEqual(interrupted_item["disposition"], "error")
        self.assertEqual(
            self.workspace.topology_status("temporary-crash")["state_policy"][
                "lifecycle"
            ],
            "removed",
        )
        self.assertTrue(pending_state.is_dir())
        self.assertEqual(list(pending_state.iterdir()), [])
        with mock.patch.object(
            self.workspace,
            "_unlink_temporary_state_lock",
            side_effect=WorkspaceError(
                "simulated lease finalization interruption"
            ),
        ):
            lease_interrupted = self.workspace.cleanup(
                ["temporary-states"], 0, [], True
            )
        lease_interrupted_item = next(
            item
            for item in lease_interrupted["items"]
            if item["path"] == str(state)
        )
        self.assertEqual(lease_interrupted_item["disposition"], "error")
        self.assertFalse(pending_state.exists())
        self.assertTrue(Path(f"{state}.lock").is_file())
        applied = self.workspace.cleanup(
            ["temporary-states"], 0, [], True
        )
        removed = next(
            item for item in applied["items"] if item["path"] == str(state)
        )
        self.assertEqual(removed["disposition"], "removed", applied)
        self.assertFalse(state.exists())
        cleaned = self.workspace.topology_status("temporary-crash")
        self.assertEqual(cleaned["state_policy"]["lifecycle"], "removed")
        self.assertFalse(Path(f"{state}.lock").exists())

    def test_failed_post_spawn_temporary_startup_retains_diagnostic_state(self) -> None:
        source = self.workspace.paths.repositories / "server"
        (source / "tools").mkdir()
        for filename in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / filename).write_text("test\n", encoding="utf-8")
        rendezvous = self.root / "failed-startup-rendezvous"
        rendezvous.mkdir()
        build_root = self.workspace.paths.builds / "failed-startup-server"
        self.make_rendezvous_server_build(
            build_root, rendezvous, "unused.bound", peers=1
        )

        @contextmanager
        def fail_temporary_state_lease(
            path: Path, **_kwargs: object
        ):
            with exclusive_lock(
                Path(f"{path}.lock"), "simulated temporary state lease"
            ):
                raise WorkspaceError("simulated state lease admission failure")
            yield

        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(
                self.workspace,
                "_topology_state_lock",
                new=fail_temporary_state_lease,
            ),
            self.assertRaisesRegex(WorkspaceError, "lease admission failure"),
        ):
            self.workspace.topology_up(
                "temporary-lease-failure", "default", None, ["server"], 0
            )
        failed_container = (
            self.workspace.paths.topologies
            / "temporary-lease-failure"
            / "temporary-states"
        )
        self.assertEqual(
            {path.name for path in failed_container.iterdir()},
            {MANAGED_MARKER},
        )
        executable = build_root / "build" / "server" / "atrinik-server"
        executable.unlink()
        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            self.assertRaisesRegex(WorkspaceError, "supervisor failed"),
        ):
            self.workspace.topology_up(
                "temporary-pre-service-failure",
                "default",
                None,
                ["server"],
                0,
            )
        pre_service = self.workspace.topology_status(
            "temporary-pre-service-failure"
        )
        self.assertEqual(pre_service["services"], {})
        self.assertEqual(pre_service["state_policy"]["mode"], "temporary")
        self.assertIsNotNone(pre_service["error"])
        pre_service_status_path = (
            self.workspace.paths.topologies
            / "temporary-pre-service-failure"
            / "status.json"
        )
        raw_pre_service = load_json(pre_service_status_path)
        invalid_without_error = {**raw_pre_service, "error": None}
        atomic_json(pre_service_status_path, invalid_without_error)
        with self.assertRaisesRegex(
            WorkspaceError, "topology .*invalid"
        ):
            self.workspace.topology_status("temporary-pre-service-failure")
        atomic_json(pre_service_status_path, raw_pre_service)
        pre_service_state = Path(pre_service["state_policy"]["path"])
        recovered_pre_service = self.workspace.topology_down(
            "temporary-pre-service-failure", timeout=5
        )
        self.assertEqual(
            recovered_pre_service["state_policy"]["lifecycle"], "disposable"
        )
        self.assertTrue(pre_service_state.is_dir())
        executable.write_text(
            "#!/usr/bin/env python3\n"
            "import pathlib, sys\n"
            "datapath = pathlib.Path(next(value.split('=', 1)[1] for value in "
            "sys.argv if value.startswith('--datapath=')))\n"
            "(datapath / 'failed-startup-proof').write_text('retained\\n', "
            "encoding='utf-8')\n"
            "raise SystemExit(23)\n",
            encoding="utf-8",
        )
        executable.chmod(0o755)
        with (
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            self.assertRaisesRegex(WorkspaceError, "topology supervisor failed"),
        ):
            self.workspace.topology_up(
                "temporary-failed-startup", "default", None, ["server"], 0
            )
        failed = self.workspace.topology_status("temporary-failed-startup")
        failed_state = Path(failed["state_policy"]["path"])
        self.assertEqual(failed["state_policy"]["lifecycle"], "disposable")
        self.assertTrue(failed_state.is_dir())
        self.assertEqual(
            (failed_state / "failed-startup-proof").read_text(encoding="utf-8"),
            "retained\n",
        )
        self.assertNotIn(
            str(failed_state), self.workspace._load_states().values()
        )

    def test_stop_acknowledgement_without_clean_shutdown_proof_retains_state(
        self,
    ) -> None:
        control = {"generation": "a" * 64}
        stopped = {
            "control": control,
            "shutdown": None,
            "supervisor": {"running": False},
            "services": {"server": {"running": False}},
        }
        self.workspace._topology_directory(
            "acknowledged-then-crashed", create=True
        )
        with (
            mock.patch.object(
                self.workspace, "_topology_control_request", return_value=True
            ),
            mock.patch.object(
                self.workspace, "_topology_process_tree_active", return_value=False
            ),
            mock.patch.object(
                self.workspace, "topology_status", return_value=stopped
            ),
        ):
            observed, confirmed_clean = self.workspace._controlled_topology_down(
                "acknowledged-then-crashed",
                {"control": control},
                1,
            )
        self.assertIs(observed, stopped)
        self.assertFalse(confirmed_clean)

    def test_persisted_clean_shutdown_proof_allows_down_retry(self) -> None:
        control = {"generation": "a" * 64}
        stopped = {
            "control": control,
            "shutdown": {"control_requested": True, "clean": True},
            "error": None,
            "supervisor": {"running": False},
            "services": {"server": {"running": False}},
        }
        self.workspace._topology_directory("clean-down-retry", create=True)
        with (
            mock.patch.object(
                self.workspace, "_topology_control_request", return_value=False
            ),
            mock.patch.object(
                self.workspace, "_topology_process_tree_active", return_value=False
            ),
            mock.patch.object(
                self.workspace, "topology_status", return_value=stopped
            ),
        ):
            observed, confirmed_clean = self.workspace._controlled_topology_down(
                "clean-down-retry", {"control": control}, 1
            )
        self.assertIs(observed, stopped)
        self.assertTrue(confirmed_clean)

    def test_topology_port_selection_rejects_unavailable_port(self) -> None:
        candidate = mock.MagicMock()
        candidate.__enter__.return_value = candidate
        candidate.bind.side_effect = OSError("address in use")
        with mock.patch(
            "atrinik_workspace.workspace.socket.socket", return_value=candidate
        ):
            with self.assertRaisesRegex(WorkspaceError, "is unavailable"):
                self.workspace._select_topology_port(17300)

        candidate.reset_mock()
        candidate.bind.side_effect = None
        candidate.getsockname.return_value = ("0.0.0.0", 49152)
        with mock.patch(
            "atrinik_workspace.workspace.socket.socket", return_value=candidate
        ):
            self.assertEqual(self.workspace._select_topology_port(None), 49152)

        with self.assertRaisesRegex(WorkspaceError, "between 0 and 65535"):
            self.workspace._select_topology_port(True)

    def test_topology_status_rejects_boolean_process_id(self) -> None:
        root = self.workspace._topology_directory("invalid", create=True)
        atomic_json(
            root / "status.json",
            {
                "schema_version": 1,
                "name": "invalid",
                "profile": "default",
                "dependencies": [],
                "state": None,
                "build_root": "/tmp/build",
                "resolved": {},
                "endpoint": None,
                "ready": False,
                "started_at": "2026-08-06T00:00:00+00:00",
                "stopped_at": None,
                "supervisor": {"pid": True, "start_time": "1"},
                "services": {},
                "error": "test fixture",
            },
        )

        with self.assertRaisesRegex(WorkspaceError, "supervisor status is invalid"):
            self.workspace.topology_status("invalid")

    def test_topology_down_uses_legacy_fallback_for_empty_process_tree_lease(
        self,
    ) -> None:
        root = self.workspace._topology_directory("empty-lease", create=True)
        (root / workspace_module.TOPOLOGY_PROCESS_TREE_LEASE).touch(mode=0o600)
        status = {
            "supervisor": {"running": True},
            "services": {},
        }
        stopped = {
            "supervisor": {"running": False},
            "services": {},
        }

        with (
            mock.patch.object(
                self.workspace, "topology_status", return_value=status
            ),
            mock.patch.object(
                self.workspace,
                "_legacy_topology_down",
                return_value=stopped,
            ) as fallback,
        ):
            self.assertEqual(
                self.workspace.topology_down("empty-lease", timeout=0.1),
                stopped,
            )

        fallback.assert_called_once_with("empty-lease", status, 0.1)

    def test_topology_up_refuses_locked_process_tree_generation(self) -> None:
        root = self.workspace._topology_directory("locked-generation", create=True)
        descriptor = os.open(
            root / workspace_module.TOPOLOGY_PROCESS_TREE_LEASE,
            os.O_RDWR | os.O_CREAT,
            0o600,
        )
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with (
                mock.patch.object(self.workspace, "_require_classic_contracts"),
                self.assertRaisesRegex(WorkspaceError, "already running"),
            ):
                self.workspace._topology_up(
                    "locked-generation",
                    "default",
                    "default",
                    ["server"],
                )
        finally:
            os.close(descriptor)

    def test_topology_status_makes_pre_coordinate_records_inert(self) -> None:
        root = self.workspace._topology_directory("historical-coordinate", create=True)
        provider = self.workspace.manifest.provider("default", "server")
        checkout = self.workspace.paths.repositories / "server"
        base = {
            "schema_version": 1,
            "name": "historical-coordinate",
            "profile": "default",
            "stack": "default",
            "providers": {"server": "server"},
            "dependencies": ["server"],
            "state": "/tmp/state",
            "build_root": "/tmp/build",
            "resolved": {
                "server": {
                    "path": str(checkout),
                    "checkout_path": str(checkout),
                    "checkout": "server",
                    "source": ".",
                    "head": "a" * 40,
                    "dirty": False,
                }
            },
            "endpoint": None,
            "ready": False,
            "started_at": "2026-08-08T00:00:00+00:00",
            "stopped_at": None,
            "supervisor": {"pid": 999, "start_time": "1"},
            "services": {},
            "error": "historical fixture",
        }
        atomic_json(root / "status.json", base)

        with mock.patch(
            "atrinik_workspace.workspace.process_matches", return_value=False
        ):
            historical = self.workspace.topology_status("historical-coordinate")

        self.assertTrue(historical["inert_historical_record"])

        current = copy.deepcopy(base)
        current["resolved"]["server"]["repository"] = "atrinik/wrong"
        current["resolved"]["server"]["branch"] = provider.branch
        atomic_json(root / "status.json", current)
        with self.assertRaisesRegex(WorkspaceError, "component identity is invalid"):
            self.workspace.topology_status("historical-coordinate")

    def test_topology_status_recognizes_only_exact_retired_content_coordinate(self) -> None:
        root = self.workspace._topology_directory("retired-content", create=True)
        checkout = self.root / "content-1x"
        record = {
            "schema_version": 1,
            "name": "retired-content",
            "profile": "classic-review",
            "stack": "classic",
            "providers": {"content": "content-1x"},
            "dependencies": ["content"],
            "state": "/tmp/state",
            "build_root": "/tmp/build",
            "resolved": {
                "content-1x": {
                    "path": str(checkout),
                    "checkout_path": str(checkout),
                    "checkout": "content-1x",
                    "repository": "atrinik/content",
                    "branch": "1.x",
                    "source": ".",
                    "head": "a" * 40,
                    "dirty": False,
                }
            },
            "endpoint": None,
            "ready": False,
            "started_at": "2026-08-08T00:00:00+00:00",
            "stopped_at": "2026-08-08T01:00:00+00:00",
            "supervisor": {"pid": 999, "start_time": "1"},
            "services": {},
            "error": "historical fixture",
        }
        atomic_json(root / "status.json", record)

        with mock.patch(
            "atrinik_workspace.workspace.process_matches", return_value=False
        ):
            historical = self.workspace.topology_status("retired-content")

        self.assertTrue(historical["inert_historical_record"])
        record["resolved"]["content-1x"]["source"] = "maps"
        atomic_json(root / "status.json", record)
        with self.assertRaisesRegex(WorkspaceError, "no provider"):
            self.workspace.topology_status("retired-content")

    def test_client_only_topology_rejects_server_port(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "requires the server"):
            self.workspace.topology_up(
                "client-only", "default", "default", ["client"], 17300
            )

    def test_state_initializes_once_and_reuses_it(self) -> None:
        server = self.workspace.paths.repositories / "server"
        first = self.workspace.state_path("default", server)
        (first / "accounts").mkdir()
        second = self.workspace.state_path("default", server)
        self.assertEqual(first, second)
        self.assertTrue((second / "accounts").is_dir())

    def test_state_add_refuses_malformed_existing_directory(self) -> None:
        malformed = self.root / "valuable"
        malformed.mkdir()
        (malformed / "unrelated").write_text("keep\n", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "lacks required file"):
            self.workspace.state_add("bad", malformed)
        self.assertEqual((malformed / "unrelated").read_text(), "keep\n")

    def test_state_paths_reject_links_and_incompatible_implementation_markers(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        implementation = {
            "stack": "classic",
            "provider": "server",
            "repository": "atrinik/server",
        }
        self.workspace.state_path(
            "default",
            server,
            implementation=implementation,
            write_implementation=True,
        )
        self.assertEqual(
            load_json(state / workspace_module.STATE_IMPLEMENTATION_MARKER),
            {"schema_version": 1, **implementation},
        )
        atomic_json(
            state / workspace_module.STATE_IMPLEMENTATION_MARKER,
            {
                "schema_version": 1,
                "stack": "replacement",
                "provider": "server",
                "repository": "atrinik/server",
            },
        )
        with self.assertRaisesRegex(
            WorkspaceError, "does not match the selected server"
        ):
            self.workspace.state_path(
                "default",
                server,
                implementation=implementation,
            )

        linked = self.root / "linked-state"
        linked.symlink_to(state, target_is_directory=True)
        with self.assertRaisesRegex(WorkspaceError, "contains a symlink"):
            self.workspace.state_add("linked", linked)

    def test_prepared_state_rejects_directory_and_ancestor_replacement(self) -> None:
        server = self.workspace.paths.repositories / "server"
        implementation = {
            "stack": "classic",
            "provider": "server",
            "repository": "atrinik/server",
        }
        state = self.workspace.state_path(
            "default",
            server,
            implementation=implementation,
            write_implementation=True,
        )
        identity = self.workspace._state_identity(state)
        replacement = state.parent / "replacement"
        shutil.copytree(state, replacement)
        previous = state.parent / "previous"

        def replace_state(*_args: object, **_kwargs: object) -> tuple[Path, int]:
            state.rename(previous)
            replacement.rename(state)
            return state, os.open(previous, os.O_PATH | os.O_DIRECTORY)

        with (
            mock.patch.object(
                self.workspace, "state_path", side_effect=replace_state
            ),
            self.assertRaisesRegex(
                WorkspaceError, "identity changed while preparing topology"
            ),
        ):
            self.workspace._prepared_state_path(
                "default", server, state, implementation, identity
            )

        external_parent = self.root / "external-parent"
        external_parent.mkdir()
        external_state = external_parent / "state"
        shutil.copytree(previous, external_state)
        attacker_parent = self.root / "attacker-parent"
        attacker_parent.mkdir()
        shutil.copytree(previous, attacker_parent / "state")
        original_parent = self.root / "original-parent"
        external_identity = self.workspace._state_identity(external_state)

        def replace_ancestor(
            *_args: object, **_kwargs: object
        ) -> tuple[Path, int]:
            external_parent.rename(original_parent)
            external_parent.symlink_to(attacker_parent, target_is_directory=True)
            return (
                external_state,
                os.open(original_parent / "state", os.O_PATH | os.O_DIRECTORY),
            )

        with (
            mock.patch.object(
                self.workspace, "state_path", side_effect=replace_ancestor
            ),
            self.assertRaisesRegex(
                WorkspaceError, "without following links|contains a symlink"
            ),
        ):
            self.workspace._prepared_state_path(
                "external",
                server,
                external_state,
                implementation,
                external_identity,
            )

    def test_state_lock_replacement_does_not_bypass_live_owner(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        lock_path = Path(f"{state}.lock")
        lock_path.touch(mode=0o600)
        with exclusive_lock(lock_path, "original state") as original:
            original_identity = os.fstat(original.fileno())
            lock_path.unlink()
            lock_path.touch(mode=0o600)
            status = {
                "name": "owner",
                "state": str(state),
                "control": {"generation": "a" * 64},
                "observation": {"process_tree_lease": "retained"},
                "state_policy": {
                    "lease_identity": {
                        "device": original_identity.st_dev,
                        "inode": original_identity.st_ino,
                    }
                },
            }
            owner_root = self.workspace._topology_directory("owner", create=True)
            atomic_json(owner_root / "status.json", {"state": str(state)})
            with (
                mock.patch.object(
                    self.workspace, "topology_status", return_value=status
                ),
                self.assertRaisesRegex(
                    WorkspaceError, "owned by topology owner generation"
                ),
            ):
                with self.workspace._topology_state_lock(state):
                    self.fail("replacement lock must not grant state ownership")

    def test_physical_state_aliases_share_one_exclusive_lease(self) -> None:
        first = self.root / "state-alias-first"
        second = self.root / "state-alias-second"
        first.mkdir()
        second.mkdir()
        shared_identity = {"device": 41, "inode": 73}
        with (
            mock.patch.object(
                self.workspace, "_state_identity", return_value=shared_identity
            ),
            self.workspace._topology_state_lock(first),
            self.assertRaisesRegex(WorkspaceError, "exact owner cannot be confirmed"),
        ):
            with self.workspace._topology_state_lock(second):
                self.fail("physical aliases must not receive distinct leases")

    def test_open_state_directory_retains_inode_bound_lease(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        first = self.workspace._open_validated_state_directory(
            state, None, write_implementation=False
        )
        try:
            with self.assertRaisesRegex(
                WorkspaceError, "physical server state is already in use"
            ):
                self.workspace._open_validated_state_directory(
                    state, None, write_implementation=False
                )
        finally:
            os.close(first)
        reopened = self.workspace._open_validated_state_directory(
            state, None, write_implementation=False
        )
        os.close(reopened)

    def test_state_marker_no_replace_publication_retries_cleanly(self) -> None:
        state = self.root / "marker-publication"
        state.mkdir()
        directory_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY)
        marker = ".atrinik-state.json"
        value = {"schema_version": 1, "stack": "classic", "provider": "server"}
        try:
            with (
                mock.patch(
                    "atrinik_workspace.workspace.rename_no_replace_at",
                    side_effect=WorkspaceError("simulated publication interruption"),
                ),
                self.assertRaisesRegex(
                    WorkspaceError, "simulated publication interruption"
                ),
            ):
                self.workspace._write_state_json_no_replace_at(
                    directory_fd, marker, value
                )
            self.assertFalse((state / marker).exists())
            self.assertEqual(list(state.iterdir()), [])
            self.workspace._write_state_json_no_replace_at(
                directory_fd, marker, value
            )
            self.assertEqual(load_json(state / marker), value)
            self.assertEqual((state / marker).stat().st_nlink, 1)
        finally:
            os.close(directory_fd)

    def test_physical_state_alias_conflict_reports_live_owner(self) -> None:
        first = self.root / "owner-alias"
        second = self.root / "contender-alias"
        first.mkdir()
        second.mkdir()
        shared_identity = {"device": 51, "inode": 83}
        status = {
            "name": "alias-owner",
            "state": str(first),
            "control": {"generation": "d" * 64},
            "observation": {"process_tree_lease": "retained"},
            "state_policy": {"identity": shared_identity},
        }
        with (
            mock.patch.object(
                self.workspace, "_state_identity", return_value=shared_identity
            ),
            self.workspace._topology_state_lock(first),
            mock.patch.object(
                self.workspace, "topology_statuses", return_value=[status]
            ),
            self.assertRaisesRegex(
                WorkspaceError, "owned by topology alias-owner generation"
            ),
        ):
            with self.workspace._topology_state_lock(second):
                self.fail("physical alias conflict must report its live owner")

    def test_state_lease_rejects_identity_change_after_path_lock(self) -> None:
        state = self.root / "state-before-lock"
        state.mkdir()
        original_identity = self.workspace._state_identity(state)
        with self.workspace._topology_state_lock(state) as lease:
            self.assertEqual(lease.physical_identity, original_identity)
            state.rename(self.root / "state-after-lock")
            state.mkdir()
            with self.assertRaisesRegex(
                WorkspaceError, "state identity changed"
            ):
                lease.bind(self.workspace._state_identity(state))

    def test_temporary_lifecycle_rejects_replaced_state_lock(self) -> None:
        state = self.root / "temporary-lock-aba"
        state.mkdir()
        lock = Path(f"{state}.lock")
        lock.touch(mode=0o600)
        with exclusive_lock(lock, "recorded temporary state") as recorded:
            metadata = os.fstat(recorded.fileno())
            identity = {"device": metadata.st_dev, "inode": metadata.st_ino}
            lock.unlink()
            lock.touch(mode=0o600)
            with exclusive_lock(lock, "replacement temporary state") as replacement:
                with self.assertRaisesRegex(
                    WorkspaceError, "lease changed before lifecycle mutation"
                ):
                    self.workspace._validate_temporary_state_lock(
                        state, replacement, identity
                    )

    def test_temporary_state_publication_interruption_leaves_no_partial_state(self) -> None:
        topology = self.workspace._topology_directory("interrupted", create=True)
        generation = "a" * 64
        server = self.workspace.paths.repositories / "server"
        coordinate = self.scenario_resolved_fixture()["server"]
        real_rename_at = workspace_module.rename_no_replace_at

        def interrupt_publication(
            source_fd: int,
            source: str,
            destination_fd: int,
            destination: str,
        ) -> None:
            if destination == generation:
                raise WorkspaceError("simulated interruption")
            real_rename_at(source_fd, source, destination_fd, destination)

        with mock.patch(
            "atrinik_workspace.workspace.rename_no_replace_at",
            side_effect=interrupt_publication,
        ):
            with self.assertRaisesRegex(WorkspaceError, "simulated interruption"):
                self.workspace._create_temporary_state(
                    topology,
                    "interrupted",
                    "default",
                    generation,
                    server,
                    {
                        "stack": "default",
                        "provider": "server",
                        "repository": "atrinik/server",
                    },
                    coordinate,
                )
        container = topology / "temporary-states"
        self.assertEqual(
            {path.name for path in container.iterdir()}, {MANAGED_MARKER}
        )

    def test_temporary_state_publication_rejects_container_replacement(self) -> None:
        topology = self.workspace._topology_directory(
            "container-replaced", create=True
        )
        server = self.workspace.paths.repositories / "server"
        generation = "b" * 64
        original_copytree = shutil.copytree
        displaced = topology / "displaced-temporary-states"
        replacement = topology / "temporary-states"

        def replace_container(*args: object, **kwargs: object) -> object:
            result = original_copytree(*args, **kwargs)
            if Path(args[0]) != server / "install_data":
                return result
            replacement.rename(displaced)
            replacement.mkdir()
            atomic_json(
                replacement / MANAGED_MARKER,
                {
                    "schema_version": 1,
                    "purpose": "topology-temporary-states",
                },
            )
            (replacement / "sentinel").write_text(
                "preserve\n", encoding="utf-8"
            )
            return result

        with (
            mock.patch(
                "atrinik_workspace.workspace.shutil.copytree",
                side_effect=replace_container,
            ),
            self.assertRaisesRegex(WorkspaceError, "identity changed"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "container-replaced",
                "default",
                generation,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            (replacement / "sentinel").read_text(encoding="utf-8"),
            "preserve\n",
        )
        self.assertEqual(
            {path.name for path in displaced.iterdir()}, {MANAGED_MARKER}
        )

    def test_temporary_state_staging_rollback_rejects_replacement(self) -> None:
        topology = self.workspace._topology_directory(
            "staging-replaced", create=True
        )
        server = self.workspace.paths.repositories / "server"
        generation = "4" * 64
        original_digest = workspace_module._tree_digest
        displaced = topology / "displaced-state-staging"
        replaced = False

        def replace_staging(*args: object, **kwargs: object) -> str:
            nonlocal replaced
            digest = original_digest(*args, **kwargs)
            if not replaced and Path(args[0]) == server / "install_data":
                replaced = True
                container = topology / "temporary-states"
                staging = next(
                    path
                    for path in container.iterdir()
                    if path.name.startswith(f".{generation}.")
                )
                staging.rename(displaced)
                staging.mkdir()
                (staging / "sentinel").write_text(
                    "preserve\n", encoding="utf-8"
                )
            return digest

        with (
            mock.patch(
                "atrinik_workspace.workspace._tree_digest",
                side_effect=replace_staging,
            ),
            self.assertRaisesRegex(WorkspaceError, "identity changed"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "staging-replaced",
                "default",
                generation,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        replacement = next(
            path
            for path in (topology / "temporary-states").iterdir()
            if path.name.startswith(f".{generation}.")
        )
        self.assertEqual(
            (replacement / "sentinel").read_text(encoding="utf-8"),
            "preserve\n",
        )
        self.assertTrue(displaced.is_dir())

    def test_temporary_state_staging_rejects_hardlinks(self) -> None:
        topology = self.workspace._topology_directory(
            "staging-hardlink", create=True
        )
        server = self.workspace.paths.repositories / "server"
        generation = "8" * 64
        original_copytree = shutil.copytree

        def link_copied_file(*args: object, **kwargs: object) -> object:
            result = original_copytree(*args, **kwargs)
            if Path(args[0]) == server / "install_data":
                destination = Path(args[1])
                copied = destination / "motd"
                copied.unlink()
                os.link(server / "install_data" / "motd", copied)
            return result

        with (
            mock.patch(
                "atrinik_workspace.workspace.shutil.copytree",
                side_effect=link_copied_file,
            ),
            self.assertRaisesRegex(WorkspaceError, "linked file"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "staging-hardlink",
                "default",
                generation,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            {path.name for path in (topology / "temporary-states").iterdir()},
            {MANAGED_MARKER},
        )

    def test_temporary_state_revalidates_after_metadata_publication(self) -> None:
        topology = self.workspace._topology_directory(
            "staging-final-validation", create=True
        )
        server = self.workspace.paths.repositories / "server"
        generation = "7" * 64
        original_atomic_json_at = workspace_module.durable_atomic_json_at

        def add_late_link(
            directory_fd: int,
            name: str,
            value: object,
            **kwargs: object,
        ) -> None:
            original_atomic_json_at(directory_fd, name, value, **kwargs)
            if name == workspace_module.TEMPORARY_STATE_METADATA:
                os.symlink("motd", "late-link", dir_fd=directory_fd)

        with (
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json_at",
                side_effect=add_late_link,
            ),
            self.assertRaisesRegex(WorkspaceError, "symbolic link"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "staging-final-validation",
                "default",
                generation,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            {path.name for path in (topology / "temporary-states").iterdir()},
            {MANAGED_MARKER},
        )

    def test_temporary_state_revalidates_copied_bytes_before_publication(self) -> None:
        topology = self.workspace._topology_directory(
            "staging-content-validation", create=True
        )
        server = self.workspace.paths.repositories / "server"
        generation = "6" * 64
        original_atomic_json_at = workspace_module.durable_atomic_json_at

        def change_copied_file(
            directory_fd: int,
            name: str,
            value: object,
            **kwargs: object,
        ) -> None:
            original_atomic_json_at(directory_fd, name, value, **kwargs)
            if name == workspace_module.TEMPORARY_STATE_METADATA:
                motd_fd = os.open(
                    "motd",
                    os.O_WRONLY | os.O_TRUNC | os.O_NOFOLLOW,
                    dir_fd=directory_fd,
                )
                try:
                    os.write(motd_fd, b"changed after validation\n")
                finally:
                    os.close(motd_fd)

        with (
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json_at",
                side_effect=change_copied_file,
            ),
            self.assertRaisesRegex(WorkspaceError, "changed before.*publication"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "staging-content-validation",
                "default",
                generation,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            {path.name for path in (topology / "temporary-states").iterdir()},
            {MANAGED_MARKER},
        )

    def test_temporary_state_revalidates_metadata_at_publication(self) -> None:
        topology = self.workspace._topology_directory(
            "staging-metadata-validation", create=True
        )
        server = self.workspace.paths.repositories / "server"
        generation = "5" * 64
        original_digest = workspace_module._tree_digest_descriptor
        full_digest_calls = 0

        def change_metadata_after_snapshot(
            directory_fd: int,
            display: Path,
            root_exclusions: set[str] | None = None,
        ) -> str:
            nonlocal full_digest_calls
            digest = original_digest(directory_fd, display, root_exclusions)
            if root_exclusions is None:
                full_digest_calls += 1
                if full_digest_calls == 2:
                    state_fd = os.open(
                        workspace_module.TEMPORARY_STATE_METADATA,
                        os.O_WRONLY | os.O_TRUNC | os.O_NOFOLLOW,
                        dir_fd=directory_fd,
                    )
                    try:
                        os.write(state_fd, b'{}\n')
                    finally:
                        os.close(state_fd)
            return digest

        with (
            mock.patch(
                "atrinik_workspace.workspace._tree_digest_descriptor",
                side_effect=change_metadata_after_snapshot,
            ),
            self.assertRaisesRegex(WorkspaceError, "metadata changed"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "staging-metadata-validation",
                "default",
                generation,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            {path.name for path in (topology / "temporary-states").iterdir()},
            {MANAGED_MARKER},
        )

    def test_temporary_startup_rollback_removes_state_and_lease(self) -> None:
        topology = self.workspace._topology_directory("rollback", create=True)
        server = self.workspace.paths.repositories / "server"
        state, policy = self.workspace._create_temporary_state(
            topology,
            "rollback",
            "default",
            "e" * 64,
            server,
            {
                "stack": "default",
                "provider": "server",
                "repository": "atrinik/server",
            },
            self.scenario_resolved_fixture()["server"],
        )
        lock = Path(f"{state}.lock")
        with exclusive_lock(lock, "temporary rollback") as state_lease:
            metadata = os.fstat(state_lease.fileno())
            self.workspace._rollback_temporary_state_creation(
                state,
                state_lease,
                policy["identity"],
                {"device": metadata.st_dev, "inode": metadata.st_ino},
                implementation=policy["implementation"],
            )
        self.assertFalse(state.exists())
        self.assertFalse(lock.exists())

    def test_temporary_startup_rollback_orphan_lease_is_reclaimable(self) -> None:
        topology = self.workspace._topology_directory(
            "rollback-orphan", create=True
        )
        server = self.workspace.paths.repositories / "server"
        state, policy = self.workspace._create_temporary_state(
            topology,
            "rollback-orphan",
            "default",
            "d" * 64,
            server,
            {
                "stack": "default",
                "provider": "server",
                "repository": "atrinik/server",
            },
            self.scenario_resolved_fixture()["server"],
        )
        lock = Path(f"{state}.lock")
        with exclusive_lock(lock, "temporary rollback") as state_lease:
            metadata = os.fstat(state_lease.fileno())
            with (
                mock.patch(
                    "atrinik_workspace.workspace.Workspace."
                    "_unlink_temporary_state_lock",
                    side_effect=WorkspaceError("simulated lease interruption"),
                ),
                self.assertRaisesRegex(
                    WorkspaceError, "simulated lease interruption"
                ),
            ):
                self.workspace._rollback_temporary_state_creation(
                    state,
                    state_lease,
                    policy["identity"],
                    {"device": metadata.st_dev, "inode": metadata.st_ino},
                    implementation=policy["implementation"],
                )
        self.assertFalse(state.exists())
        preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        item = next(
            value for value in preview["items"] if value["path"] == str(state)
        )
        self.assertEqual(item["disposition"], "eligible")
        self.assertIn(
            "stale_orphan_temporary_state_lease", item["reasons"]
        )
        container_marker = state.parent / MANAGED_MARKER
        container_payload = container_marker.read_bytes()
        container_marker.unlink()
        unowned_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        unowned_item = next(
            value
            for value in unowned_preview["items"]
            if value["path"] == str(state)
        )
        self.assertEqual(unowned_item["disposition"], "protected")
        self.assertIn(
            "invalid_orphan_temporary_state_lease", unowned_item["reasons"]
        )
        container_marker.write_bytes(container_payload)
        with (
            mock.patch(
                "atrinik_workspace.cleanup._descriptor_mount_id",
                side_effect=[1, 2],
            ),
            self.assertRaisesRegex(WorkspaceError, "crossed a mount"),
        ):
            cleanup_module.Cleanup(
                self.workspace
            )._open_temporary_state_container(topology)
        cleanup = cleanup_module.Cleanup(self.workspace)
        normal_item = next(
            value
            for value in cleanup._temporary_states(0)
            if value["path"] == str(state)
        )
        cleanup._remove_temporary_state(normal_item, 0)
        self.assertFalse(lock.exists())
        with exclusive_lock(lock, "recreated orphan rollback lease"):
            pass
        lock_metadata = lock.stat(follow_symlinks=False)
        lock_tombstone = lock.parent / (
            f".{lock.name}.remove-{lock_metadata.st_dev:x}-"
            f"{lock_metadata.st_ino:x}"
        )
        lock.rename(lock_tombstone)
        tombstone_preview = self.workspace.cleanup(
            ["temporary-states"], 0, [], False
        )
        tombstone_item = next(
            value
            for value in tombstone_preview["items"]
            if value["path"] == str(state)
        )
        self.assertEqual(tombstone_item["disposition"], "eligible")
        applied = self.workspace.cleanup(
            ["temporary-states"], 0, [], True
        )
        applied_item = next(
            value for value in applied["items"] if value["path"] == str(state)
        )
        self.assertEqual(applied_item["disposition"], "removed")
        self.assertFalse(lock.exists())
        self.assertFalse(lock_tombstone.exists())

    def test_orphan_temporary_lease_inventory_fails_closed(self) -> None:
        topology = self.workspace._topology_directory(
            "orphan-lease-uncertainty", create=True
        )
        container, container_fd = self.workspace._temporary_state_container(topology)
        os.close(container_fd)
        generation = "a" * 64
        state = container / generation
        lock = Path(f"{state}.lock")
        with exclusive_lock(lock, "orphan lease fixture"):
            pass
        cleanup = cleanup_module.Cleanup(self.workspace)

        with self.assertRaisesRegex(WorkspaceError, "tombstone is invalid"):
            cleanup._orphan_temporary_state_lease_item(
                topology, lock, 0, tombstone=True
            )

        invalid_tombstone = container / f".{lock.name}.remove-0-0"
        invalid_tombstone.write_text("invalid\n", encoding="utf-8")
        invalid_item = cleanup._orphan_temporary_state_lease_item(
            topology, invalid_tombstone, 0, tombstone=True
        )
        self.assertIn(
            "invalid_orphan_temporary_state_lease", invalid_item["reasons"]
        )
        invalid_tombstone.unlink()

        state.mkdir()
        state_item = cleanup._orphan_temporary_state_lease_item(
            topology, lock, 0
        )
        self.assertIn(
            "invalid_orphan_temporary_state_lease", state_item["reasons"]
        )
        state.rmdir()

        with exclusive_lock(lock, "busy orphan lease"):
            busy_item = cleanup._orphan_temporary_state_lease_item(
                topology, lock, 0
            )
        self.assertIn("active_state_lease", busy_item["reasons"])

        with mock.patch.object(
            cleanup, "_lock_busy", return_value=(False, "invalid lease")
        ):
            lock_error_item = cleanup._orphan_temporary_state_lease_item(
                topology, lock, 0
            )
        self.assertIn("state_lease_error", lock_error_item["reasons"])

        with mock.patch.object(
            self.workspace,
            "topology_status",
            return_value={"control": {"generation": generation}},
        ):
            current_item = cleanup._orphan_temporary_state_lease_item(
                topology, lock, 0
            )
        self.assertIn("topology_generation_present", current_item["reasons"])

        future_cleanup = cleanup_module.Cleanup(self.workspace)
        future_cleanup.now = future_cleanup.now.replace(
            year=1970, month=1, day=1
        )
        future_item = future_cleanup._orphan_temporary_state_lease_item(
            topology, lock, 0
        )
        self.assertIn("future_creation_time", future_item["reasons"])
        young_item = cleanup_module.Cleanup(
            self.workspace
        )._orphan_temporary_state_lease_item(topology, lock, 999)
        self.assertIn("younger_than_grace_period", young_item["reasons"])

        extra_link = self.root / "orphan-lease-extra-link"
        os.link(lock, extra_link)
        linked_item = cleanup._orphan_temporary_state_lease_item(
            topology, lock, 0
        )
        self.assertIn(
            "invalid_orphan_temporary_state_lease", linked_item["reasons"]
        )
        extra_link.unlink()

        atomic_json(topology / "status.json", {})
        with mock.patch.object(
            self.workspace,
            "topology_status",
            side_effect=WorkspaceError("invalid topology status"),
        ):
            status_item = cleanup._orphan_temporary_state_lease_item(
                topology, lock, 0
            )
        self.assertIn("topology_status_unverifiable", status_item["reasons"])

    def test_detached_temporary_state_classifies_every_uncertainty(self) -> None:
        topology = self.workspace._topology_directory(
            "detached-state-uncertainty", create=True
        )
        container, container_fd = self.workspace._temporary_state_container(topology)
        os.close(container_fd)
        generation = "b" * 64
        state = container / generation
        state.mkdir()
        cleanup = cleanup_module.Cleanup(self.workspace)
        policy = {
            "mode": "temporary",
            "path": str(state),
            "owner": {
                "kind": "topology-generation",
                "topology": topology.name,
                "generation": generation,
            },
            "lifecycle": "removal-pending",
            "lease_identity": {"device": 1, "inode": 2},
            "created_at": "invalid",
        }
        uncertain = cleanup._detached_temporary_state_item(
            topology.name,
            {
                "state_policy": policy,
                "supervisor": {"liveness": "live"},
                "services": {"server": {"liveness": "unreachable"}},
                "observation": None,
            },
            0,
        )
        self.assertTrue(
            {
                "temporary_state_reappeared",
                "temporary_state_ownership_evidence_missing",
                "state_lease_unverifiable",
                "topology_liveness_unverifiable",
                "topology_observation_unverifiable",
                "invalid_temporary_state",
            }
            <= set(uncertain["reasons"])
        )
        state.rmdir()

        lock = Path(f"{state}.lock")
        with exclusive_lock(lock, "detached lease fixture"):
            pass
        lock_metadata = lock.stat(follow_symlinks=False)
        released_policy = {
            **policy,
            "lifecycle": "removed",
            "lease_identity": {
                "device": lock_metadata.st_dev,
                "inode": lock_metadata.st_ino,
            },
            "created_at": cleanup.now.replace(
                year=cleanup.now.year + 1, month=1, day=1
            ).isoformat(),
        }
        observed_status = {
            "state_policy": released_policy,
            "supervisor": {"liveness": "stale"},
            "services": {},
            "observation": {
                "control": "reachable",
                "process_tree_lease": "retained",
                "runtime_bundle_lease": "unverifiable",
                "port_reservation": None,
            },
            "endpoint": None,
        }
        with mock.patch.object(
            cleanup,
            "_state_lock_observation",
            return_value=(False, "invalid lease", None),
        ):
            lease_error = cleanup._detached_temporary_state_item(
                topology.name, observed_status, 0
            )
        self.assertTrue(
            {
                "state_lease_error",
                "reachable_topology_control",
                "process_tree_lease_unverifiable",
                "runtime_bundle_lease_unverifiable",
                "port_reservation_lease_unverifiable",
                "future_creation_time",
            }
            <= set(lease_error["reasons"])
        )
        with mock.patch.object(
            cleanup,
            "_state_lock_observation",
            return_value=(True, None, released_policy["lease_identity"]),
        ):
            busy = cleanup._detached_temporary_state_item(
                topology.name, observed_status, 0
            )
        self.assertIn("active_state_lease", busy["reasons"])
        with mock.patch.object(
            cleanup,
            "_state_lock_observation",
            return_value=(False, None, {"device": 9, "inode": 9}),
        ):
            mismatch = cleanup._detached_temporary_state_item(
                topology.name, observed_status, 0
            )
        self.assertIn("state_lease_identity_mismatch", mismatch["reasons"])

    def test_temporary_startup_rollback_retains_linked_state(self) -> None:
        topology = self.workspace._topology_directory(
            "linked-rollback", create=True
        )
        server = self.workspace.paths.repositories / "server"
        state, policy = self.workspace._create_temporary_state(
            topology,
            "linked-rollback",
            "default",
            "f" * 64,
            server,
            {
                "stack": "default",
                "provider": "server",
                "repository": "atrinik/server",
            },
            self.scenario_resolved_fixture()["server"],
        )
        lock = Path(f"{state}.lock")
        linked_target = self.root / "rollback-link-target"
        linked_target.write_text("preserve\n", encoding="utf-8")
        (state / "unsafe-link").symlink_to(linked_target)
        state_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        with exclusive_lock(lock, "temporary rollback") as state_lease:
            metadata = os.fstat(state_lease.fileno())
            with self.assertRaisesRegex(WorkspaceError, "symbolic link"):
                self.workspace._rollback_temporary_state_creation(
                    state,
                    state_lease,
                    policy["identity"],
                    {"device": metadata.st_dev, "inode": metadata.st_ino},
                    state_fd,
                )
        os.close(state_fd)
        self.assertTrue(state.is_dir())
        self.assertTrue((state / "unsafe-link").is_symlink())

    def test_temporary_startup_rollback_requires_exact_implementation(self) -> None:
        topology = self.workspace._topology_directory(
            "typed-rollback", create=True
        )
        server = self.workspace.paths.repositories / "server"
        state, policy = self.workspace._create_temporary_state(
            topology,
            "typed-rollback",
            "default",
            "9" * 64,
            server,
            {
                "stack": "default",
                "provider": "server",
                "repository": "atrinik/server",
            },
            self.scenario_resolved_fixture()["server"],
        )
        replacement = dict(policy["implementation"])
        replacement["provider"] = "replacement-server"
        atomic_json(
            state / workspace_module.STATE_IMPLEMENTATION_MARKER,
            {
                "schema_version": workspace_module.STATE_IMPLEMENTATION_SCHEMA_VERSION,
                **replacement,
            },
        )
        lock = Path(f"{state}.lock")
        with exclusive_lock(lock, "temporary rollback") as state_lease:
            metadata = os.fstat(state_lease.fileno())
            with self.assertRaisesRegex(
                WorkspaceError, "implementation marker is invalid"
            ):
                self.workspace._rollback_temporary_state_creation(
                    state,
                    state_lease,
                    policy["identity"],
                    {"device": metadata.st_dev, "inode": metadata.st_ino},
                    implementation=policy["implementation"],
                )
        self.assertTrue(state.is_dir())
        self.assertTrue(lock.is_file())

    def test_temporary_mutation_refuses_live_physical_alias(self) -> None:
        state = self.root / "temporary-physical-alias"
        state.mkdir()
        identity = self.workspace._state_identity(state)
        alias_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY)
        try:
            fcntl.flock(alias_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaisesRegex(
                WorkspaceError, "temporary state is already in use"
            ):
                self.workspace._lock_state_directory_mutation(state, identity)
        finally:
            os.close(alias_fd)

    def test_temporary_state_first_digest_failure_removes_staging(self) -> None:
        topology = self.workspace._topology_directory("digest-failure", create=True)
        server = self.workspace.paths.repositories / "server"
        with (
            mock.patch(
                "atrinik_workspace.workspace._tree_digest",
                side_effect=WorkspaceError("invalid install_data"),
            ),
            self.assertRaisesRegex(WorkspaceError, "invalid install_data"),
        ):
            self.workspace._create_temporary_state(
                topology,
                "digest-failure",
                "default",
                "c" * 64,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            {path.name for path in (topology / "temporary-states").iterdir()},
            {MANAGED_MARKER},
        )

    def test_temporary_state_rejects_install_data_mutation_during_copy(self) -> None:
        topology = self.workspace._topology_directory("copy-race", create=True)
        server = self.workspace.paths.repositories / "server"
        original_tree_digest = workspace_module._tree_digest
        digest_calls = 0

        def digest_then_mutate(*args: object, **kwargs: object) -> str:
            nonlocal digest_calls
            digest_calls += 1
            digest = original_tree_digest(*args, **kwargs)
            if digest_calls == 2:
                (server / "install_data" / "motd").write_text(
                    "changed\n", encoding="utf-8"
                )
            return digest

        with (
            mock.patch(
                "atrinik_workspace.workspace._tree_digest",
                side_effect=digest_then_mutate,
            ),
            self.assertRaisesRegex(
                WorkspaceError, "install_data changed during temporary state"
            ),
        ):
            self.workspace._create_temporary_state(
                topology,
                "copy-race",
                "default",
                "b" * 64,
                server,
                {
                    "stack": "default",
                    "provider": "server",
                    "repository": "atrinik/server",
                },
                self.scenario_resolved_fixture()["server"],
            )
        self.assertEqual(
            {path.name for path in (topology / "temporary-states").iterdir()},
            {MANAGED_MARKER},
        )

    def test_scenario_lifecycle_owns_isolated_state_and_credentials(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ) as provision:
            with mock.patch("builtins.print") as output:
                created = self.workspace.scenario_create(
                    "issue-42", "default", "basic-player"
                )
            output.assert_not_called()

            self.assertEqual(created["state"], "scenario-issue-42")
            self.assertEqual(created["account"], "scenario1dd9ee81")
            self.assertEqual(created["character"], "Scenario 1dd9ee81")
            scenario_root = self.workspace.paths.scenarios / "issue-42"
            state = scenario_root / "state"
            self.assertEqual(
                Path(self.workspace.list_states()["scenario-issue-42"]), state
            )
            self.assertEqual(
                stat.S_IMODE((scenario_root / "password").stat().st_mode), 0o600
            )
            credentials = self.workspace.scenario_credentials("issue-42")
            self.assertEqual(credentials["account"], created["account"])
            self.assertEqual(credentials["character"], created["character"])
            self.assertTrue(credentials["password"])
            scenario_summary = self.workspace.topology_summary(
                "default", "scenario-issue-42", ["server"]
            )
            self.assertEqual(
                scenario_summary["state_policy"]["owner"],
                {"kind": "scenario", "name": "issue-42"},
            )
            self.assertEqual(
                scenario_summary["state_policy"]["lifecycle"],
                "scenario-owned",
            )

            (state / "accounts").mkdir()
            reset = self.workspace.scenario_reset("issue-42")
            self.assertFalse((state / "accounts").exists())
            self.assertGreater(reset["provisioned_at"], created["provisioned_at"])

        self.assertEqual(provision.call_count, 2)

    def test_scenario_reset_fsync_uncertainty_retains_old_references(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("uncertain-reset", "default")

        scenario_path = (
            self.workspace.paths.scenarios / "uncertain-reset" / "scenario.json"
        )
        identity = hashlib.sha256(str(scenario_path.resolve()).encode()).hexdigest()
        old_sources = {
            str(Path(row["checkout_path"]).resolve()) for row in resolved.values()
        }
        replacement = copy.deepcopy(resolved)
        new_content = self.root / "replacement-content"
        new_content.mkdir()
        replacement["content"]["checkout_path"] = str(new_content)
        replacement["content"]["path"] = str(new_content)
        real_publish = workspace_module.durable_atomic_json

        def uncertain_write(target: Path, value: object) -> None:
            real_publish(target, value)
            if target == scenario_path:
                raise workspace_module.AtomicJsonCommitUncertain(
                    "simulated scenario durability uncertainty"
                )

        with (
            mock.patch.object(
                self.workspace,
                "_scenario_provision_state",
                return_value=replacement,
            ),
            mock.patch(
                "atrinik_workspace.workspace.durable_atomic_json",
                side_effect=uncertain_write,
            ),
            self.assertRaisesRegex(WorkspaceError, "durability uncertainty"),
        ):
            self.workspace.scenario_reset("uncertain-reset")

        record = load_json(
            self.workspace._lease_namespace
            / "profile-references"
            / f"{identity}.json"
        )
        self.assertTrue(old_sources.issubset(set(record["sources"])))
        self.assertIn(str(new_content.resolve()), record["sources"])

    def test_scenario_lifecycle_prepares_fresh_asset_staging(self) -> None:
        selected = {
            component: self.workspace.paths.repositories / component
            for component in ("server", "content", "resources", "libatrinik", "protocol")
        }
        source = selected["server"]
        (source / "tools").mkdir()
        for name in ("ca-bundle.crt", "permissions.cfg", "server.cfg"):
            (source / name).write_text("test\n", encoding="utf-8")

        build_root = self.workspace.paths.builds / "profiles" / "scenario-assets"
        managed_directory(build_root, self.workspace.paths.builds, "test-profile")
        binary = build_root / "build" / "server"
        binary.mkdir(parents=True)
        for name in ("atrinik-server", "libplugin_arena.so", "libplugin_python.so"):
            (binary / name).write_text("test\n", encoding="utf-8")
        for path in (
            build_root / "runtime" / "content" / "lib",
            build_root / "runtime" / "content" / "maps",
            build_root / "runtime" / "resources",
        ):
            path.mkdir(parents=True, exist_ok=True)
        self.make_region_map_cache(build_root)

        staged_assets: list[Path] = []

        def provision(
            arguments: list[str], *, cwd: Path | None = None, **kwargs: object
        ) -> object:
            if "--provision_scenario" not in arguments:
                return workspace_run(arguments, cwd=cwd, **kwargs)
            assert cwd is not None
            self.assertIn("--provision_preset=lighting-radiance-day", arguments)
            assetspath = Path(
                next(
                    argument.split("=", 1)[1]
                    for argument in arguments
                    if argument.startswith("--assetspath=")
                )
            )
            self.assertEqual(assetspath, cwd / "assets")
            self.assertFalse(assetspath.is_symlink())
            self.assertTrue((assetspath / "data").is_dir())
            self.assertFalse((assetspath / "data").is_symlink())
            self.assertTrue((assetspath / "client-maps" / "incuna_-1.png").is_file())
            self.assertFalse((assetspath / "client-maps").is_symlink())
            self.assertFalse((assetspath / "data" / "previous-run").exists())
            self.assertFalse((cwd / "data" / "http").exists())
            (assetspath / "data" / "previous-run").write_text(
                "generated\n", encoding="utf-8"
            )
            staged_assets.append(assetspath)
            return None

        with (
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch("atrinik_workspace.workspace.run", side_effect=provision),
        ):
            created = self.workspace.scenario_create(
                "fresh-assets", "default", "lighting-radiance-day"
            )
            self.assertEqual(
                self.workspace.scenario_show("fresh-assets")["state"],
                "scenario-fresh-assets",
            )
            reset = self.workspace.scenario_reset("fresh-assets")

        self.assertEqual(created["state"], "scenario-fresh-assets")
        self.assertEqual(reset["state"], "scenario-fresh-assets")
        self.assertEqual(len(staged_assets), 2)
        self.assertNotEqual(staged_assets[0], staged_assets[1])
        state = self.workspace.paths.scenarios / "fresh-assets" / "state"
        self.assertFalse((state / "assets").exists())
        self.assertFalse((state / "http").exists())

    def test_scenario_create_rolls_back_failed_provisioning(self) -> None:
        with mock.patch.object(
            self.workspace,
            "_scenario_provision_state",
            side_effect=WorkspaceError("provision failed"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "provision failed"):
                self.workspace.scenario_create("failed", "default")

        self.assertFalse((self.workspace.paths.scenarios / "failed").exists())
        self.assertNotIn("scenario-failed", self.workspace.list_states())

    def test_historical_default_scenario_is_inert_without_stack_identity(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("historical-default", "default")

        metadata_path = (
            self.workspace.paths.scenarios / "historical-default" / "scenario.json"
        )
        metadata = load_json(metadata_path)
        metadata["schema_version"] = 1
        del metadata["stack"]
        del metadata["providers"]
        atomic_json(metadata_path, metadata)

        with self.assertRaisesRegex(
            WorkspaceError,
            "historical scenario lacks immutable stack/provider identity and is inert",
        ):
            self.workspace.scenario_show("historical-default")

    def test_historical_scenario_is_inert_without_repository_identity(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("historical-coordinate", "default")

        metadata_path = (
            self.workspace.paths.scenarios
            / "historical-coordinate"
            / "scenario.json"
        )
        metadata = load_json(metadata_path)
        metadata["schema_version"] = 3
        for record in metadata["resolved"].values():
            del record["repository"]
            del record["branch"]
        atomic_json(metadata_path, metadata)

        with self.assertRaisesRegex(
            WorkspaceError,
            "historical scenario lacks immutable repository/branch identity and is inert",
        ):
            self.workspace.scenario_show("historical-coordinate")

    def test_scenario_list_preserves_inert_record_and_continues_inventory(self) -> None:
        resolved = self.scenario_resolved_fixture()
        self.workspace.create_profile("stale-profile")
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")
            self.workspace.scenario_create("historical", "stale-profile")

        profile_path = self.workspace.paths.profiles / "stale-profile.json"
        profile = load_json(profile_path)
        profile["components"].pop("content")
        atomic_json(profile_path, profile)
        scenario_path = (
            self.workspace.paths.scenarios / "historical" / "scenario.json"
        )
        profile_before = profile_path.read_bytes()
        scenario_before = scenario_path.read_bytes()
        states_before = self.workspace.paths.states_file.read_bytes()

        summaries = self.workspace.scenario_list()

        self.assertEqual(
            [summary["name"] for summary in summaries],
            ["current", "historical"],
        )
        self.assertEqual(summaries[0]["profile"], "default")
        self.assertEqual(
            summaries[1],
            {
                "name": "historical",
                "path": str(self.workspace.paths.scenarios / "historical"),
                "inert": True,
                "inert_reason": "profile_unresolvable",
            },
        )
        with self.assertRaisesRegex(
            WorkspaceError, "profile component set does not match manifest"
        ):
            self.workspace.scenario_show("historical")
        self.assertEqual(profile_path.read_bytes(), profile_before)
        self.assertEqual(scenario_path.read_bytes(), scenario_before)
        self.assertEqual(self.workspace.paths.states_file.read_bytes(), states_before)

    def test_scenario_list_preserves_resolved_path_for_symlinked_container(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")

        scenarios = self.workspace.paths.scenarios
        external = self.root / "external-scenarios"
        scenarios.rename(external)
        scenarios.symlink_to(external, target_is_directory=True)
        invalid = scenarios / "invalid\nname"
        invalid.mkdir()
        outside = self.root / "outside-scenario"
        outside.mkdir()
        escaped = scenarios / "escaped"
        escaped.symlink_to(outside, target_is_directory=True)

        summaries = {row["name"]: row for row in self.workspace.scenario_list()}
        self.assertEqual(
            summaries["current"]["path"],
            self.workspace.scenario_show("current")["path"],
        )
        self.assertEqual(
            summaries["invalid\nname"],
            {
                "name": "invalid\nname",
                "path": str(invalid),
                "inert": True,
                "inert_reason": "invalid_record",
            },
        )
        self.assertEqual(
            summaries["escaped"],
            {
                "name": "escaped",
                "path": str(escaped),
                "inert": True,
                "inert_reason": "invalid_record",
            },
        )

    def test_scenario_list_reports_invalid_record_without_error_detail(self) -> None:
        root = self.workspace.paths.scenarios / "malformed"
        root.mkdir(parents=True)
        invalid_name = self.workspace.paths.scenarios / "invalid\nname"
        invalid_name.mkdir()

        self.assertEqual(
            self.workspace.scenario_list(),
            [
                {
                    "name": "invalid\nname",
                    "path": str(invalid_name),
                    "inert": True,
                    "inert_reason": "invalid_record",
                },
                {
                    "name": "malformed",
                    "path": str(root),
                    "inert": True,
                    "inert_reason": "invalid_record",
                }
            ],
        )

    def test_scenario_list_isolates_unhashable_metadata_fields(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")
            self.workspace.scenario_create("malformed", "default")

        metadata_path = (
            self.workspace.paths.scenarios / "malformed" / "scenario.json"
        )
        metadata = load_json(metadata_path)
        metadata["preset"] = {"invalid": "object"}
        atomic_json(metadata_path, metadata)

        summaries = self.workspace.scenario_list()

        self.assertEqual(
            [summary["name"] for summary in summaries],
            ["current", "malformed"],
        )
        self.assertEqual(summaries[0]["profile"], "default")
        self.assertEqual(summaries[1]["inert_reason"], "invalid_record")

    def test_scenario_list_isolates_non_utf8_scenario_and_profile(self) -> None:
        resolved = self.scenario_resolved_fixture()
        self.workspace.create_profile("invalid-nesting")
        self.workspace.create_profile("invalid-profile")
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")
            self.workspace.scenario_create("invalid-integer", "default")
            self.workspace.scenario_create("invalid-metadata", "default")
            self.workspace.scenario_create("invalid-nesting", "invalid-nesting")
            self.workspace.scenario_create("invalid-profile", "invalid-profile")

        metadata_path = (
            self.workspace.paths.scenarios / "invalid-metadata" / "scenario.json"
        )
        profile_path = self.workspace.paths.profiles / "invalid-profile.json"
        integer_path = (
            self.workspace.paths.scenarios / "invalid-integer" / "scenario.json"
        )
        nesting_path = self.workspace.paths.profiles / "invalid-nesting.json"
        integer_path.write_bytes(b"1" * 5000)
        metadata_path.write_bytes(b"\xff")
        nesting_path.write_bytes(
            b"[" * 100_000 + b"0" + b"]" * 100_000
        )
        profile_path.write_bytes(b"\xff")
        integer_before = integer_path.read_bytes()
        metadata_before = metadata_path.read_bytes()
        nesting_before = nesting_path.read_bytes()
        profile_before = profile_path.read_bytes()

        previous_limit = sys.get_int_max_str_digits()
        try:
            sys.set_int_max_str_digits(4300)
            summaries = self.workspace.scenario_list()
        finally:
            sys.set_int_max_str_digits(previous_limit)

        self.assertEqual(
            [summary["name"] for summary in summaries],
            [
                "current",
                "invalid-integer",
                "invalid-metadata",
                "invalid-nesting",
                "invalid-profile",
            ],
        )
        self.assertEqual(summaries[0]["profile"], "default")
        self.assertEqual(summaries[1]["inert_reason"], "invalid_record")
        self.assertEqual(summaries[2]["inert_reason"], "invalid_record")
        self.assertEqual(summaries[3]["inert_reason"], "profile_unresolvable")
        self.assertEqual(summaries[4]["inert_reason"], "profile_unresolvable")
        self.assertEqual(integer_path.read_bytes(), integer_before)
        self.assertEqual(metadata_path.read_bytes(), metadata_before)
        self.assertEqual(nesting_path.read_bytes(), nesting_before)
        self.assertEqual(profile_path.read_bytes(), profile_before)

    def test_scenario_list_isolates_invalid_profile_fields(self) -> None:
        resolved = self.scenario_resolved_fixture()
        self.workspace.create_profile("invalid-path")
        self.workspace.create_profile("relative-path")
        self.workspace.create_profile("invalid-sound-mode")
        self.workspace.create_profile("invalid-selector-kind")
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")
            self.workspace.scenario_create("invalid-path", "invalid-path")
            self.workspace.scenario_create("relative-path", "relative-path")
            self.workspace.scenario_create(
                "invalid-sound-mode", "invalid-sound-mode"
            )
            self.workspace.scenario_create(
                "invalid-selector-kind", "invalid-selector-kind"
            )

        sound_path = self.workspace.paths.profiles / "invalid-sound-mode.json"
        sound_profile = load_json(sound_path)
        sound_profile["sound_mode"] = {"invalid": "object"}
        atomic_json(sound_path, sound_profile)
        selector_path = (
            self.workspace.paths.profiles / "invalid-selector-kind.json"
        )
        selector_profile = load_json(selector_path)
        selector_profile["components"]["content"]["kind"] = ["invalid"]
        atomic_json(selector_path, selector_profile)
        path_profile_path = self.workspace.paths.profiles / "invalid-path.json"
        path_profile = load_json(path_profile_path)
        path_profile["components"]["content"] = {
            "kind": "path",
            "value": "/tmp/\0invalid",
        }
        atomic_json(path_profile_path, path_profile)
        relative_path = self.workspace.paths.profiles / "relative-path.json"
        relative_profile = load_json(relative_path)
        relative_profile["components"]["content"] = {
            "kind": "path",
            "value": "relative/content",
        }
        atomic_json(relative_path, relative_profile)
        path_profile_before = path_profile_path.read_bytes()

        summaries = self.workspace.scenario_list()

        self.assertEqual(
            [summary["name"] for summary in summaries],
            [
                "current",
                "invalid-path",
                "invalid-selector-kind",
                "invalid-sound-mode",
                "relative-path",
            ],
        )
        self.assertEqual(summaries[0]["profile"], "default")
        self.assertEqual(summaries[1]["inert_reason"], "profile_unresolvable")
        self.assertEqual(summaries[2]["inert_reason"], "profile_unresolvable")
        self.assertEqual(summaries[3]["inert_reason"], "profile_unresolvable")
        self.assertEqual(summaries[4]["inert_reason"], "profile_unresolvable")
        self.assertEqual(path_profile_path.read_bytes(), path_profile_before)
        with self.assertRaisesRegex(WorkspaceError, "invalid profile selector"):
            self.workspace.scenario_show("invalid-path")

    def test_scenario_list_fails_closed_for_invalid_shared_state_registry(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")

        states = load_json(self.workspace.paths.states_file)
        states["schema_version"] = 999
        atomic_json(self.workspace.paths.states_file, states)

        with self.assertRaisesRegex(
            WorkspaceError, "states registry schema is invalid"
        ):
            self.workspace.scenario_list()

    def test_scenario_list_isolates_invalid_resolved_paths(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("current", "default")
            self.workspace.scenario_create("invalid-component-path", "default")
            self.workspace.scenario_create("invalid-state-path", "default")
            self.workspace.scenario_create("unregistered-state", "default")

        metadata_path = (
            self.workspace.paths.scenarios
            / "invalid-component-path"
            / "scenario.json"
        )
        metadata = load_json(metadata_path)
        metadata["resolved"]["server"]["checkout_path"] = "/tmp/\0invalid"
        atomic_json(metadata_path, metadata)
        states = load_json(self.workspace.paths.states_file)
        states["states"]["scenario-invalid-state-path"] = "/tmp/\0invalid"
        states["states"].pop("scenario-unregistered-state")
        atomic_json(self.workspace.paths.states_file, states)
        metadata_before = metadata_path.read_bytes()
        states_before = self.workspace.paths.states_file.read_bytes()

        summaries = self.workspace.scenario_list()

        self.assertEqual(
            [summary["name"] for summary in summaries],
            [
                "current",
                "invalid-component-path",
                "invalid-state-path",
                "unregistered-state",
            ],
        )
        self.assertEqual(summaries[0]["profile"], "default")
        self.assertEqual(summaries[1]["inert_reason"], "invalid_record")
        self.assertEqual(summaries[2]["inert_reason"], "invalid_record")
        self.assertEqual(summaries[3]["inert_reason"], "invalid_record")
        self.assertEqual(metadata_path.read_bytes(), metadata_before)
        self.assertEqual(self.workspace.paths.states_file.read_bytes(), states_before)
        with self.assertRaisesRegex(
            WorkspaceError, "scenario component metadata is invalid"
        ):
            self.workspace.scenario_show("invalid-component-path")

    def test_scenario_audit_records_only_server_dependency_closure(self) -> None:
        required = {"server", "content", "resources", "libatrinik", "protocol"}
        selected = {
            component: self.workspace.paths.repositories / component
            for component in (*sorted(required), "client")
        }
        metadata = {
            "profile": "default",
            "state": "scenario-audit",
            "account": "scenarioaudit",
            "character": "Scenario Audit",
            "archetype": "human_male",
        }
        runtime = self.root / "scenario-runtime"
        runtime.mkdir()
        with (
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=self.root / "build"
            ),
            mock.patch.object(
                self.workspace, "_prepare_server_runtime", return_value=runtime
            ),
            mock.patch("atrinik_workspace.workspace.run"),
            mock.patch(
                "atrinik_workspace.workspace.git", return_value="a" * 40
            ),
            mock.patch(
                "atrinik_workspace.workspace._is_clean", return_value=True
            ),
        ):
            resolved = self.workspace._scenario_provision_state(
                metadata, self.root / "state", self.root / "password"
            )

        self.assertEqual(set(resolved), required)
        self.assertNotIn("client", resolved)

    def test_scenario_rejects_insecure_password_permissions(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("permissions", "default")

        password = self.workspace.paths.scenarios / "permissions" / "password"
        password.chmod(0o644)
        with self.assertRaisesRegex(WorkspaceError, "mode 0600"):
            self.workspace.scenario_show("permissions")

    def test_scenario_reset_refuses_locked_state(self) -> None:
        resolved = self.scenario_resolved_fixture()
        with mock.patch.object(
            self.workspace, "_scenario_provision_state", return_value=resolved
        ):
            self.workspace.scenario_create("locked", "default")

        state = self.workspace.paths.scenarios / "locked" / "state"
        with exclusive_lock(Path(f"{state}.lock"), "test state"):
            with self.assertRaisesRegex(WorkspaceError, "already in use|busy"):
                self.workspace.scenario_reset("locked")

        descriptor = os.open(state, os.O_RDONLY | os.O_DIRECTORY)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaisesRegex(
                WorkspaceError, "scenario server state is already in use"
            ):
                self.workspace.scenario_reset("locked")
        finally:
            os.close(descriptor)

    def test_foreground_client_pins_matching_server_state(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        certificate = (
            "-----BEGIN PRIVATE KEY-----\nignored\n-----END PRIVATE KEY-----\n"
            "-----BEGIN CERTIFICATE-----\nAQID\n-----END CERTIFICATE-----\n"
        )
        (state / "quic-identity.pem").write_text(certificate, encoding="ascii")

        build_root = self.root / "client-build"
        executable = build_root / "build" / "client" / "atrinik"
        executable.parent.mkdir(parents=True)
        executable.write_text("client\n", encoding="utf-8")
        (build_root / "sources" / "client").mkdir(parents=True)
        atomic_json(
            build_root / workspace_module.BUILD_METADATA,
            {"sound": workspace_module.sound_source_record(self.wrapper / "sound")},
        )
        expected = "039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81"
        self.workspace._physical_lease_namespace = self.workspace._lease_namespace

        def execute_client(*_arguments: object, **_keywords: object) -> None:
            self.assertEqual(len(_keywords["pass_fds"]), 1)
            for path, description in (
                (
                    resource_lock_path(
                        self.workspace.paths.workspace, "profile", "default"
                    ),
                    "profile default",
                ),
                (
                    resource_lock_path(
                        self.workspace._lease_namespace,
                        "source",
                        self.workspace._source_coordinate(
                            "client", self.workspace.paths.repositories / "client"
                        ),
                    ),
                    "client source",
                ),
                (
                    self.workspace.paths.builds
                    / "locks"
                    / "client-build.lock",
                    "profile build default",
                ),
            ):
                with exclusive_lock(path, description, nonblocking=True):
                    pass
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    Path(_keywords["cwd"]).parent
                    / workspace_module.RUNTIME_GENERATION_LEASE,
                    "foreground runtime generation",
                    nonblocking=True,
                ):
                    self.fail("foreground client released its runtime generation")

        with (
            mock.patch.object(
                self.workspace,
                "_resolve_build_profile",
                return_value={
                    "client": self.wrapper / "client",
                    "sound": self.wrapper / "sound",
                },
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(
                self.workspace, "_topology_resolved_status", return_value={}
            ),
            mock.patch.object(
                self.workspace,
                "_selected_checkout_states",
                return_value=synthetic_checkout_states(self.wrapper / "client"),
            ),
            mock.patch("builtins.print") as output,
            mock.patch(
                "atrinik_workspace.workspace.run", side_effect=execute_client
            ) as execute,
            mock.patch.object(self.workspace, "_require_client_display"),
        ):
            result = self.workspace.run_client(
                "default", "default", 1731, ["--fullscreen"], False
            )

        self.assertEqual(result.name, "atrinik")
        self.assertFalse(result.exists())
        rendered = "\n".join(str(call.args[0]) for call in output.call_args_list)
        self.assertIn(f"--server=127.0.0.1 1731 {expected}", rendered)
        self.assertIn("--stun_server=off", rendered)
        self.assertIn("--nometa", rendered)
        self.assertIn("--fullscreen", rendered)
        self.assertIn("launch label: profile default (direct run)", rendered)
        environment = execute.call_args.kwargs["env"]
        self.assertEqual(
            environment["ATRINIK_LAUNCH_LABEL"],
            "profile default (direct run)",
        )

    def test_client_launch_label_is_bounded(self) -> None:
        long_profile = "p" * 96
        with self.assertRaisesRegex(WorkspaceError, "launch label exceeds 96 bytes"):
            client_launch_label(long_profile)

    def test_foreground_client_requires_initialized_server_identity(self) -> None:
        server = self.workspace.paths.repositories / "server"
        self.workspace.state_path("default", server)
        with self.assertRaisesRegex(WorkspaceError, "start the matching server"):
            self.workspace.run_client("default", "default", 1730, [], True)

    def test_foreground_client_rejects_symlinked_server_identity(self) -> None:
        server = self.workspace.paths.repositories / "server"
        state = self.workspace.state_path("default", server)
        target = self.root / "identity.pem"
        target.write_text(
            "-----BEGIN CERTIFICATE-----\nAQID\n-----END CERTIFICATE-----\n",
            encoding="ascii",
        )
        (state / "quic-identity.pem").symlink_to(target)
        with self.assertRaisesRegex(WorkspaceError, "cannot open server QUIC identity"):
            self.workspace.run_client("default", "default", 1730, [], True)

    def test_foreground_server_keeps_local_defaults_with_extra_arguments(self) -> None:
        server = self.workspace.paths.repositories / "server"
        build_root = self.root / "server-build"
        runtime = self.root / "server-runtime"
        runtime.mkdir()
        executable = runtime / "atrinik-server"
        executable.write_text("server\n", encoding="utf-8")
        selected = {"server": server}
        self.workspace._physical_lease_namespace = self.workspace._lease_namespace

        def publish_server(*_arguments: object, **_keywords: object) -> tuple[Path, int, dict[str, object]]:
            generation_root = self.root / "foreground-server-generation"
            server_runtime = generation_root / "server"
            server_runtime.mkdir(parents=True)
            generated_executable = server_runtime / "atrinik-server"
            generated_executable.write_text("server\n", encoding="utf-8")
            (server_runtime / "assets").mkdir()
            lease = generation_root / workspace_module.RUNTIME_GENERATION_LEASE
            descriptor = os.open(lease, os.O_RDWR | os.O_CREAT, 0o600)
            workspace_module.initialize_lease(descriptor, "a" * 64)
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            state_output = (
                self.workspace._state_location("default")
                / "tmp"
                / "runtime-assets"
                / "foreground-server-generation"
            )
            state_output.mkdir(parents=True)
            atomic_json(
                state_output / MANAGED_MARKER,
                {
                    "schema_version": 1,
                    "purpose": "runtime-state-output:foreground-server-generation",
                },
            )
            return (
                generation_root,
                descriptor,
                {
                    "mutable_state_outputs": [str(state_output)],
                    "mutable_state_output_identities": [
                        self.workspace._state_identity(state_output)
                    ],
                },
                os.open(
                    state_output,
                    os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                ),
            )

        def execute_server(*_arguments: object, **_keywords: object) -> None:
            self.assertEqual(len(_keywords["pass_fds"]), 5)
            asset_argument = next(
                value
                for value in reversed(_arguments[0])
                if value.startswith("--assetspath=")
            )
            asset_path = Path(asset_argument.split("=", 1)[1])
            self.assertEqual(asset_path.parent, Path("/proc/self/fd"))
            self.assertIn(int(asset_path.name), _keywords["pass_fds"])
            self.assertEqual(
                load_json(asset_path / MANAGED_MARKER)["purpose"],
                "runtime-state-output:foreground-server-generation",
            )
            for path, description in (
                (
                    resource_lock_path(
                        self.workspace.paths.workspace, "profile", "default"
                    ),
                    "profile default",
                ),
                (
                    resource_lock_path(
                        self.workspace._lease_namespace,
                        "source",
                        self.workspace._source_coordinate("server", server),
                    ),
                    "server source",
                ),
                (
                    self.workspace.paths.builds
                    / "locks"
                    / "server-build.lock",
                    "profile build default",
                ),
            ):
                with exclusive_lock(path, description, nonblocking=True):
                    pass
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    Path(f"{self.workspace._state_location('default')}.lock"),
                    "server state",
                    nonblocking=True,
                ):
                    self.fail("foreground server released its state")
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    Path(_keywords["cwd"]).parent
                    / workspace_module.RUNTIME_GENERATION_LEASE,
                    "foreground runtime generation",
                    nonblocking=True,
                ):
                    self.fail("foreground server released its runtime generation")

        with (
            mock.patch.object(
                self.workspace, "_resolve_build_profile", return_value=selected
            ),
            mock.patch.object(
                self.workspace, "_build_resolved", return_value=build_root
            ),
            mock.patch.object(
                self.workspace,
                "_topology_resolved_status",
                return_value={
                    "server": {
                        "repository": "https://github.com/atrinik/server.git"
                    }
                },
            ),
            mock.patch.object(
                self.workspace,
                "_selected_checkout_states",
                return_value=synthetic_checkout_states(server),
            ),
            mock.patch.object(
                self.workspace,
                "_publish_runtime_generation",
                side_effect=publish_server,
            ),
            mock.patch(
                "atrinik_workspace.workspace.run", side_effect=execute_server
            ),
            mock.patch("builtins.print") as output,
        ):
            result = self.workspace.run_server(
                "default",
                "default",
                1731,
                ["--no_console", "--assetspath=/tmp/untrusted"],
                False,
            )

        self.assertEqual(result.name, "atrinik-server")
        self.assertFalse(result.exists())
        implementation = load_json(
            self.workspace._state_location("default")
            / workspace_module.STATE_IMPLEMENTATION_MARKER
        )
        self.assertEqual(implementation["stack"], "default")
        self.assertEqual(implementation["provider"], "server")
        rendered = "\n".join(str(call.args[0]) for call in output.call_args_list)
        self.assertIn("--port_quic=1731", rendered)
        self.assertIn("--port_mapping=off", rendered)
        self.assertIn("--stun_server=off", rendered)
        self.assertIn("--no_console", rendered)
        self.assertLess(
            rendered.index("--assetspath=/tmp/untrusted"),
            rendered.index("--assetspath=/proc/self/fd/"),
        )
        self.assertIn("--datapath=/proc/self/fd/", rendered)

    def test_foreground_launch_rejects_invalid_port(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "between 1 and 65535"):
            self.workspace.run_client("default", "default", True, [], True)

    def test_redacts_join_passwords(self) -> None:
        displayed = display_arguments(
            ["server", "--join_password=secret", "--join-password", "also-secret"]
        )
        self.assertNotIn("secret", displayed)
        self.assertIn("<redacted>", displayed)

    def test_operational_subprocess_output_uses_stderr(self) -> None:
        completed = mock.MagicMock(stdout="")
        with mock.patch(
            "atrinik_workspace.workspace.subprocess.run", return_value=completed
        ) as invoke:
            workspace_run(["tool"])

        self.assertIs(invoke.call_args.kwargs["stdout"], os.sys.stderr)

    def test_operational_subprocess_inherits_active_lock_descriptors(self) -> None:
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        build = self.workspace.paths.builds / "locks" / "inherited-build.lock"
        state = self.workspace.paths.state / "inherited-state.lock"
        completed = mock.MagicMock(stdout="")
        with (
            shared_lock(layout, "repository layout") as layout_lease,
            exclusive_lock(build, "profile build inherited") as build_lease,
            exclusive_lock(state, "server state inherited") as state_lease,
            mock.patch(
                "atrinik_workspace.workspace.subprocess.run",
                return_value=completed,
            ) as invoke,
        ):
            workspace_run(["tool"])
            self.assertEqual(
                set(invoke.call_args.kwargs["pass_fds"]),
                {
                    layout_lease.fileno(),
                    build_lease.fileno(),
                    state_lease.fileno(),
                },
            )
            for path, description in (
                (layout, "repository layout"),
                (build, "profile build inherited"),
                (state, "server state inherited"),
            ):
                with self.assertRaisesRegex(WorkspaceError, "already in use"):
                    with exclusive_lock(path, description, nonblocking=True):
                        self.fail(f"subprocess lease did not protect {description}")

    def test_resource_listing_inherits_active_lock_descriptors(self) -> None:
        source = self.root / "resource-listing"
        source.mkdir()
        (source / workspace_module.RESOURCE_PATHS_MANIFEST).write_text(
            "paintings\n", encoding="utf-8"
        )
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        build = self.workspace.paths.builds / "locks" / "resource-build.lock"
        completed = mock.MagicMock(stdout=b"paintings/scene.jpg\0", stderr=b"")
        with (
            shared_lock(layout, "repository layout") as layout_lease,
            exclusive_lock(build, "profile build resources") as build_lease,
            mock.patch(
                "atrinik_workspace.workspace.subprocess.run",
                return_value=completed,
            ) as invoke,
        ):
            expected_fds = {layout_lease.fileno(), build_lease.fileno()}
            runtime_paths, tracked = self.workspace._resource_runtime_files(source)

        self.assertEqual(runtime_paths, ["paintings"])
        self.assertEqual(tracked, ["paintings/scene.jpg"])
        self.assertEqual(
            set(invoke.call_args.kwargs["pass_fds"]),
            expected_fds,
        )

    def test_orphaned_operational_subprocess_retains_active_leases(self) -> None:
        child_script = self.root / "lease-child.py"
        child_pid_path = self.root / "lease-child.pid"
        child_script.write_text(
            "import os, pathlib, sys, time\n"
            "path = pathlib.Path(sys.argv[1])\n"
            "path.write_text(str(os.getpid()), encoding='ascii')\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        build = self.workspace.paths.builds / "locks" / "orphan-build.lock"
        state = self.workspace.paths.state / "orphan-state.lock"
        context = multiprocessing.get_context("spawn")
        wrapper = context.Process(
            target=inherited_leases_wrapper_process,
            args=(
                str(layout),
                str(build),
                str(state),
                str(child_script),
                str(child_pid_path),
            ),
        )
        child_pidfd: int | None = None
        try:
            wrapper.start()
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and not child_pid_path.is_file():
                time.sleep(0.05)
            self.assertTrue(child_pid_path.is_file())
            child_pid = int(child_pid_path.read_text(encoding="ascii"))
            child_pidfd = os.pidfd_open(child_pid)
            wrapper.kill()
            wrapper.join(timeout=5)
            self.assertFalse(wrapper.is_alive())
            for path, description in (
                (layout, "repository layout"),
                (build, "profile build orphan"),
                (state, "server state orphan"),
            ):
                with self.assertRaisesRegex(WorkspaceError, "already in use"):
                    with exclusive_lock(path, description, nonblocking=True):
                        self.fail(f"orphaned child released {description}")
        finally:
            if wrapper.is_alive():
                wrapper.kill()
                wrapper.join(timeout=5)
            if child_pidfd is not None:
                try:
                    signal.pidfd_send_signal(child_pidfd, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                os.close(child_pidfd)

        deadline = time.monotonic() + 5
        while True:
            try:
                with (
                    exclusive_lock(layout, "repository layout", nonblocking=True),
                    exclusive_lock(build, "profile build orphan", nonblocking=True),
                    exclusive_lock(state, "server state orphan", nonblocking=True),
                ):
                    break
            except WorkspaceError:
                if time.monotonic() >= deadline:
                    self.fail("orphaned child did not release inherited leases")
                time.sleep(0.05)

    def test_orphaned_cleanup_child_retains_layout_lease(self) -> None:
        executable_directory = self.root / "fake-bin"
        executable_directory.mkdir()
        fake_git = executable_directory / "git"
        fake_git.write_text(
            f"#!{sys.executable}\n"
            "import os, pathlib, time\n"
            "pathlib.Path(os.environ['ATRINIK_TEST_CHILD_PID']).write_text("
            "str(os.getpid()), encoding='ascii')\n"
            "while True:\n"
            "    time.sleep(0.1)\n",
            encoding="utf-8",
        )
        fake_git.chmod(0o755)
        child_pid_path = self.root / "cleanup-child.pid"
        layout = self.workspace.paths.workspace / "repository-layout.lock"
        context = multiprocessing.get_context("spawn")
        wrapper = context.Process(
            target=cleanup_writer_wrapper_process,
            args=(
                str(layout),
                str(self.wrapper),
                str(executable_directory),
                str(child_pid_path),
            ),
        )
        child_pidfd: int | None = None
        try:
            wrapper.start()
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and not child_pid_path.is_file():
                time.sleep(0.05)
            self.assertTrue(child_pid_path.is_file())
            child_pidfd = os.pidfd_open(
                int(child_pid_path.read_text(encoding="ascii"))
            )
            wrapper.kill()
            wrapper.join(timeout=5)
            self.assertFalse(wrapper.is_alive())
            with self.assertRaisesRegex(WorkspaceError, "already in use"):
                with exclusive_lock(
                    layout, "repository layout", nonblocking=True
                ):
                    self.fail("orphaned cleanup child released the layout lock")
        finally:
            if wrapper.is_alive():
                wrapper.kill()
                wrapper.join(timeout=5)
            if child_pidfd is not None:
                try:
                    signal.pidfd_send_signal(child_pidfd, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                os.close(child_pidfd)

        deadline = time.monotonic() + 5
        while True:
            try:
                with exclusive_lock(
                    layout, "repository layout", nonblocking=True
                ):
                    break
            except WorkspaceError:
                if time.monotonic() >= deadline:
                    self.fail("cleanup child did not release the layout lease")
                time.sleep(0.05)


if __name__ == "__main__":
    unittest.main()
