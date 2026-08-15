from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import threading
import time
from typing import Any, BinaryIO

from .launch_identity import CLIENT_LAUNCH_LABEL_ENV, client_launch_label
from .model import durable_atomic_json
from .process_tree import control_socket_path, holders_exist, signal_holders
from .port_reservation import PortReservationError, validate_held


LOG_LIMIT = 10 * 1024 * 1024
LOG_BACKUPS = 3
SERVER_READY_TIMEOUT = 30
FINGERPRINT_PATTERN = re.compile(
    rb"QUIC certificate SHA-256:\s*([0-9a-fA-F]{64})"
)
SERVER_READY_PATTERN = re.compile(rb"Server ready\. Waiting for connections\.\.\.")


def process_start_time(pid: int) -> str | None:
    """Return Linux's stable process start tick for PID-reuse protection."""
    try:
        fields = (Path("/proc") / str(pid) / "stat").read_text().rsplit(")", 1)[1]
        return fields.split()[19]
    except (IndexError, OSError):
        return None


def process_matches(pid: int, start_time: str) -> bool:
    try:
        fields = (Path("/proc") / str(pid) / "stat").read_text().rsplit(")", 1)[1]
        values = fields.split()
        return values[0] != "Z" and values[19] == start_time
    except (IndexError, OSError):
        return False


def atomic_status(path: Path, value: dict[str, Any]) -> None:
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def open_log(path: Path) -> BinaryIO:
    flags = os.O_WRONLY | os.O_CREAT | os.O_APPEND | os.O_CLOEXEC
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags, 0o600)
    if not stat.S_ISREG(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise RuntimeError(f"service log is not a regular file: {path}")
    return os.fdopen(descriptor, "ab", buffering=0)


def _peek_exit_code(process: subprocess.Popen[bytes]) -> int | None:
    """Observe a child exit without reaping its process-group leader."""
    try:
        result = os.waitid(
            os.P_PID,
            process.pid,
            os.WEXITED | os.WNOHANG | os.WNOWAIT,
        )
    except ChildProcessError:
        return process.returncode
    if result is None:
        return None
    if result.si_code == os.CLD_EXITED:
        return result.si_status
    return -result.si_status


def terminate(
    processes: dict[str, subprocess.Popen[bytes]],
    process_tree_fd: int | None,
    timeout: float = 10,
    *,
    exclude: tuple[int | None, ...] = (),
) -> bool:
    # An unreaped session leader pins its numeric process-group identity, so
    # these groups remain safe to signal until the final waits below. The lease
    # additionally finds descendants whose recorded leader was already reaped.
    process_groups = [
        process.pid
        for process in processes.values()
        if process.returncode is None
    ]
    group_observation_certain = True

    def signal_groups(signum: signal.Signals) -> None:
        for process_group in process_groups:
            try:
                os.killpg(process_group, signum)
            except ProcessLookupError:
                pass

    def groups_exist() -> bool:
        nonlocal group_observation_certain
        groups = set(process_groups)
        try:
            entries = list(Path("/proc").iterdir())
        except OSError:
            group_observation_certain = False
            entries = []
        for entry in entries:
            if not entry.name.isdigit():
                continue
            try:
                fields = (entry / "stat").read_text().rsplit(")", 1)[1].split()
                state = fields[0]
                process_group = int(fields[2])
            except FileNotFoundError:
                continue
            except OSError:
                group_observation_certain = False
                continue
            except (IndexError, ValueError):
                group_observation_certain = False
                continue
            if process_group in groups and state != "Z":
                return True
        return False

    signal_groups(signal.SIGTERM)
    if process_tree_fd is not None:
        signal_holders(
            process_tree_fd,
            signal.SIGTERM,
            exclude=(os.getpid(), *(pid for pid in exclude if pid is not None)),
        )
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        running = groups_exist()
        if process_tree_fd is not None:
            running = running or holders_exist(
                process_tree_fd,
                exclude=(os.getpid(), *(pid for pid in exclude if pid is not None)),
            )
        if not running:
            break
        time.sleep(0.1)
    clean = not groups_exist() and group_observation_certain
    if process_tree_fd is not None:
        clean = clean and not holders_exist(
            process_tree_fd,
            exclude=(os.getpid(), *(pid for pid in exclude if pid is not None)),
        )
    signal_groups(signal.SIGKILL)
    if process_tree_fd is not None:
        signal_holders(
            process_tree_fd,
            signal.SIGKILL,
            exclude=(os.getpid(), *(pid for pid in exclude if pid is not None)),
        )
        kill_deadline = time.monotonic() + 2
        while time.monotonic() < kill_deadline and (
            groups_exist()
            or holders_exist(
                process_tree_fd,
                exclude=(os.getpid(), *(pid for pid in exclude if pid is not None)),
            )
        ):
            signal_groups(signal.SIGKILL)
            signal_holders(
                process_tree_fd,
                signal.SIGKILL,
                exclude=(os.getpid(), *(pid for pid in exclude if pid is not None)),
            )
            time.sleep(0.05)
    for process in processes.values():
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            clean = False
        if process.returncode not in {0, -signal.SIGTERM}:
            clean = False
    clean = clean and group_observation_certain
    return clean


class RotatingLog:
    def __init__(self, path: Path):
        self.path = path
        path.parent.mkdir(parents=True, exist_ok=True)
        self.stream = open_log(path)

    def write(self, content: bytes) -> None:
        if self.stream.tell() + len(content) > LOG_LIMIT:
            self.stream.close()
            oldest = self.path.with_name(f"{self.path.name}.{LOG_BACKUPS}")
            oldest.unlink(missing_ok=True)
            for index in range(LOG_BACKUPS - 1, 0, -1):
                source = self.path.with_name(f"{self.path.name}.{index}")
                if source.exists() or source.is_symlink():
                    source.replace(
                        self.path.with_name(f"{self.path.name}.{index + 1}")
                    )
            if self.path.exists() or self.path.is_symlink():
                self.path.replace(self.path.with_name(f"{self.path.name}.1"))
            self.stream = open_log(self.path)
        self.stream.write(content)

    def close(self) -> None:
        self.stream.close()


class ServerReadinessCapture:
    def __init__(self) -> None:
        self.event = threading.Event()
        self.fingerprint: str | None = None
        self.server_ready = False
        self.buffer = b""

    def feed(self, content: bytes) -> None:
        if self.event.is_set():
            return
        self.buffer = (self.buffer + content)[-4096:]
        if self.fingerprint is None:
            match = FINGERPRINT_PATTERN.search(self.buffer)
            if match is not None:
                self.fingerprint = match.group(1).decode("ascii").lower()
        self.server_ready = self.server_ready or bool(
            SERVER_READY_PATTERN.search(self.buffer)
        )
        if self.fingerprint is not None and self.server_ready:
            self.event.set()


def pump_output(
    process: subprocess.Popen[bytes],
    output: RotatingLog,
    capture: ServerReadinessCapture | None = None,
) -> None:
    assert process.stdout is not None
    writable = True
    while content := process.stdout.read1(65536):
        if capture is not None:
            capture.feed(content)
        if writable:
            try:
                output.write(content)
            except (OSError, RuntimeError) as error:
                writable = False
                print(f"service log write failed: {error}", file=sys.stderr)
    process.stdout.close()


def _initial_status(spec: dict[str, Any], supervisor_start_time: str) -> dict[str, Any]:
    control = spec.get("control")
    generation = control.get("generation") if isinstance(control, dict) else None
    status: dict[str, Any] = {
        "schema_version": spec.get("schema_version", 1),
        "name": spec["name"],
        "profile": spec["profile"],
        "dependencies": spec["dependencies"],
        "state": spec["state"],
        **(
            {"state_policy": spec["state_policy"]}
            if "state_policy" in spec
            else {}
        ),
        "build_root": spec["build_root"],
        "resolved": spec["resolved"],
        "endpoint": (
            {**spec["endpoint"], "fingerprint": None}
            if spec["endpoint"] is not None
            else None
        ),
        "ready": False,
        "started_at": datetime.now(timezone.utc).isoformat(),
        "stopped_at": None,
        **({"shutdown": None} if spec.get("schema_version") == 3 else {}),
        "supervisor": {
            "pid": os.getpid(),
            "start_time": supervisor_start_time,
            **({"generation": generation} if generation is not None else {}),
        },
        "services": {},
    }
    if control is not None:
        status["control"] = control
    if "runtime" in spec:
        status["runtime"] = spec["runtime"]
    if "stack" in spec or "providers" in spec:
        if not isinstance(spec.get("stack"), str) or not isinstance(
            spec.get("providers"), dict
        ):
            raise RuntimeError("topology spec stack/provider identity is incomplete")
        status["stack"] = spec["stack"]
        status["providers"] = spec["providers"]
    if "sound" in spec:
        status["sound"] = spec["sound"]
    if "port_reservation" in spec:
        status["port_reservation"] = spec["port_reservation"]
    return status


def _validate_port_reservation(
    spec: dict[str, Any], descriptor: int | None, _topology_root: Path
) -> None:
    reservation = spec.get("port_reservation")
    if reservation is None and descriptor is None:
        return
    if reservation is None or descriptor is None:
        raise RuntimeError("topology port reservation descriptor is incomplete")
    try:
        validated = validate_held(descriptor, reservation)
    except PortReservationError as error:
        raise RuntimeError(str(error)) from error
    control = spec.get("control")
    endpoint = spec.get("endpoint")
    if (
        not isinstance(control, dict)
        or validated["topology"] != spec.get("name")
        or validated["generation"] != control.get("generation")
        or not isinstance(endpoint, dict)
        or validated["port"] != endpoint.get("port")
    ):
        raise RuntimeError("topology port reservation does not match the topology")


def _require_server_port_available(spec: dict[str, Any]) -> None:
    endpoint = spec.get("endpoint")
    if not isinstance(endpoint, dict) or not isinstance(endpoint.get("port"), int):
        raise RuntimeError("topology server endpoint is invalid")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as candidate:
            candidate.bind(("0.0.0.0", endpoint["port"]))
    except OSError as error:
        raise RuntimeError(
            f"reserved topology UDP port {endpoint['port']} was claimed by an "
            f"external process before server startup: {error}"
        ) from error


def _guardian(
    read_fd: int,
    process_tree_fd: int,
    retained_fds: tuple[int | None, ...],
) -> None:
    """Release one orphaned topology tree after its supervisor disappears."""
    try:
        while os.read(read_fd, 4096):
            pass
    except OSError:
        pass
    finally:
        os.close(read_fd)

    # Pipe EOF proves the supervisor is gone. Do not exclude its bare numeric
    # PID: it may already have been reused by an exact lease-holding descendant.
    excluded = (os.getpid(),)
    signal_holders(process_tree_fd, signal.SIGTERM, exclude=excluded)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline and (
        holders_exist(process_tree_fd, exclude=excluded)
    ):
        time.sleep(0.1)
    signal_holders(process_tree_fd, signal.SIGKILL, exclude=excluded)
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline and (
        holders_exist(process_tree_fd, exclude=excluded)
    ):
        signal_holders(process_tree_fd, signal.SIGKILL, exclude=excluded)
        time.sleep(0.05)
    while holders_exist(process_tree_fd, exclude=excluded):
        time.sleep(0.5)
    for descriptor in retained_fds:
        if descriptor is not None:
            os.close(descriptor)
    os.close(process_tree_fd)


def _start_guardian(
    process_tree_fd: int | None,
    *retained_fds: int | None,
) -> tuple[int | None, int | None]:
    if process_tree_fd is None:
        return None, None
    read_fd, write_fd = os.pipe2(os.O_CLOEXEC)
    guardian_pid = os.fork()
    if guardian_pid:
        os.close(read_fd)
        return guardian_pid, write_fd

    os.close(write_fd)
    try:
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        _guardian(read_fd, process_tree_fd, retained_fds)
    finally:
        os._exit(0)


def _open_control(spec: dict[str, Any], topology_root: Path) -> socket.socket | None:
    control = spec.get("control")
    if control is None:
        return None
    if (
        not isinstance(control, dict)
        or set(control) != {"socket", "generation", "lease"}
        or not isinstance(control.get("socket"), str)
        or not isinstance(control.get("generation"), str)
        or not re.fullmatch(r"[0-9a-f]{64}", control["generation"])
        or control["socket"]
        != str(control_socket_path(topology_root, control["generation"]))
        or not isinstance(control.get("lease"), dict)
        or set(control["lease"]) != {"device", "inode"}
        or not all(
            isinstance(value, int)
            and not isinstance(value, bool)
            and value >= 0
            for value in control["lease"].values()
        )
    ):
        raise RuntimeError("topology control identity is invalid")
    endpoint = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        endpoint.bind(control["socket"])
        os.chmod(control["socket"], 0o600)
        endpoint.listen(32)
        endpoint.setblocking(False)
    except BaseException:
        endpoint.close()
        raise
    return endpoint


def _receive_control(connection: socket.socket) -> Any:
    payload = bytearray()
    while len(payload) <= 4096:
        chunk = connection.recv(4097 - len(payload))
        if not chunk:
            break
        payload.extend(chunk)
        if b"\n" in chunk:
            break
    if len(payload) > 4096:
        raise ValueError("control message is too large")
    return json.loads(payload)


def _serve_control(
    endpoint: socket.socket | None,
    spec: dict[str, Any],
) -> bool:
    """Serve at most one bounded request and return whether shutdown was asked."""
    if endpoint is None:
        return False
    stop = False
    deadline = time.monotonic() + 0.2
    for _request_index in range(32):
        try:
            connection, _address = endpoint.accept()
        except BlockingIOError:
            return stop
        with connection:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return stop
            connection.settimeout(min(0.05, remaining))
            try:
                request = _receive_control(connection)
                control = spec["control"]
                valid = (
                    isinstance(request, dict)
                    and set(request) == {"action", "name", "generation"}
                    and request.get("action") in {"status", "stop"}
                    and request.get("name") == spec["name"]
                    and request.get("generation") == control["generation"]
                )
                response = {
                    "ok": valid,
                    "name": spec["name"],
                    "generation": control["generation"],
                }
                connection.sendall(
                    json.dumps(response, sort_keys=True).encode() + b"\n"
                )
                stop = stop or bool(valid and request["action"] == "stop")
            except (OSError, TimeoutError, ValueError, json.JSONDecodeError):
                continue
    return stop


def supervise(
    spec_path: Path,
    lock_fd: int | None,
    layout_lock_fd: int | None,
    build_lock_fd: int | None,
    process_tree_fd: int | None,
    port_reservation_fd: int | None,
    runtime_lock_fd: int | None = None,
    state_directory_fd: int | None = None,
    state_output_fd: int | None = None,
    physical_state_lock_fd: int | None = None,
) -> int:
    with spec_path.open(encoding="utf-8") as stream:
        spec = json.load(stream)
    if spec.get("schema_version") in {2, 3}:
        for descriptor in (layout_lock_fd, build_lock_fd):
            if descriptor is not None:
                os.close(descriptor)
        layout_lock_fd = None
        build_lock_fd = None
    status_path = spec_path.parent / "status.json"
    stop = False
    control_stop_requested = False
    control_socket: socket.socket | None = None
    guardian_pid: int | None = None
    guardian_write_fd: int | None = None

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    supervisor_start_time = process_start_time(os.getpid())
    if supervisor_start_time is None:
        raise RuntimeError("cannot identify topology supervisor process")
    status = _initial_status(spec, supervisor_start_time)
    _validate_port_reservation(spec, port_reservation_fd, spec_path.parent)
    processes: dict[str, subprocess.Popen[bytes]] = {}
    logs: list[RotatingLog] = []
    pumps: list[threading.Thread] = []

    def start_service(
        name: str,
        extra_arguments: list[str] | None = None,
        capture: ServerReadinessCapture | None = None,
    ) -> subprocess.Popen[bytes]:
        service = spec["services"][name]
        log = Path(service["log"])
        log.parent.mkdir(parents=True, exist_ok=True)
        output = RotatingLog(log)
        logs.append(output)
        output.write(
            f"\n[{datetime.now(timezone.utc).isoformat()}] starting {name}\n".encode()
        )
        command = list(service["command"])
        command.extend(extra_arguments or [])
        environment = os.environ.copy()
        environment.update(service.get("environment", {}))
        if name == "client":
            environment[CLIENT_LAUNCH_LABEL_ENV] = client_launch_label(
                spec["profile"], spec["name"]
            )
        inherited_locks: list[int] = []
        if process_tree_fd is not None:
            inherited_locks.append(process_tree_fd)
        if state_directory_fd is not None and name == "server":
            inherited_locks.append(state_directory_fd)
        if state_output_fd is not None and name == "server":
            inherited_locks.append(state_output_fd)
        process = subprocess.Popen(
            command,
            cwd=service["cwd"],
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,
            pass_fds=tuple(inherited_locks),
        )
        processes[name] = process
        start_time = process_start_time(process.pid)
        if start_time is None:
            raise RuntimeError(f"cannot identify {name} process")
        pump = threading.Thread(
            target=pump_output,
            args=(process, output, capture),
            name=f"{name}-log",
            daemon=True,
        )
        pump.start()
        pumps.append(pump)
        status["services"][name] = {
            "pid": process.pid,
            "start_time": start_time,
            **(
                {"generation": spec["control"]["generation"]}
                if "control" in spec
                else {}
            ),
            "status": "starting" if capture is not None else "running",
            "exit_code": None,
            "log": str(log),
            "cwd": service["cwd"],
        }
        atomic_status(status_path, status)
        return process

    try:
        retained_fds = (
            (port_reservation_fd, lock_fd, runtime_lock_fd)
            if spec.get("schema_version") in {2, 3}
            else (
                lock_fd,
                layout_lock_fd,
                build_lock_fd,
                port_reservation_fd,
            )
        )
        if state_directory_fd is not None:
            retained_fds = (*retained_fds, state_directory_fd)
        if state_output_fd is not None:
            retained_fds = (*retained_fds, state_output_fd)
        if physical_state_lock_fd is not None:
            retained_fds = (*retained_fds, physical_state_lock_fd)
        guardian_pid, guardian_write_fd = _start_guardian(
            process_tree_fd,
            *retained_fds,
        )
        control_socket = _open_control(spec, spec_path.parent)
        if "server" in spec["services"]:
            _require_server_port_available(spec)
            capture = ServerReadinessCapture()
            server = start_service("server", capture=capture)
            deadline = time.monotonic() + SERVER_READY_TIMEOUT
            while not capture.event.wait(timeout=0.1):
                requested = _serve_control(control_socket, spec)
                control_stop_requested = control_stop_requested or requested
                stop = stop or requested
                code = _peek_exit_code(server)
                if code is not None:
                    raise RuntimeError(
                        f"server exited before becoming ready with code {code}"
                    )
                if stop:
                    raise RuntimeError("topology stopped before the server became ready")
                if time.monotonic() >= deadline:
                    raise RuntimeError(
                        f"server did not become ready within {SERVER_READY_TIMEOUT} seconds"
                    )
            assert capture.fingerprint is not None
            status["endpoint"]["fingerprint"] = capture.fingerprint
            status["services"]["server"]["status"] = "running"
            atomic_status(status_path, status)

        if "client" in spec["services"]:
            client_arguments: list[str] = []
            endpoint = status["endpoint"]
            if endpoint is not None:
                client_arguments = [
                    f"--server={endpoint['host']} {endpoint['port']} "
                    f"{endpoint['fingerprint']}",
                    f"--connect={endpoint['host']}",
                    "--stun_server=off",
                    "--nometa",
                ]
            start_service("client", client_arguments)

        status["ready"] = True
        atomic_status(status_path, status)

        while not stop:
            requested = _serve_control(control_socket, spec)
            control_stop_requested = control_stop_requested or requested
            stop = requested
            changed = False
            running = True
            for name, process in processes.items():
                code = _peek_exit_code(process)
                service_status = status["services"][name]
                if code is not None and service_status["status"] == "running":
                    service_status["status"] = "exited"
                    service_status["exit_code"] = code
                    changed = True
                    running = False
            if changed:
                atomic_status(status_path, status)
            if not running:
                break
            time.sleep(0.2)
    except BaseException as error:
        status["error"] = f"{type(error).__name__}: {error}"
        atomic_status(status_path, status)
        return 1
    finally:
        clean_termination = terminate(
            processes, process_tree_fd, exclude=(guardian_pid,)
        )
        for pump in pumps:
            pump.join(timeout=2)
        for name, process in processes.items():
            code = process.poll()
            service_status = status["services"].get(name)
            if service_status is None:
                continue
            service_status["status"] = "exited"
            service_status["exit_code"] = code
        status["stopped_at"] = datetime.now(timezone.utc).isoformat()
        status["ready"] = False
        status["shutdown"] = {
            "control_requested": control_stop_requested,
            "clean": (
                control_stop_requested
                and clean_termination
                and "error" not in status
            ),
        }
        durable_atomic_json(status_path, status)
        for output in logs:
            output.close()
        if control_socket is not None:
            control_socket.close()
            control_path = Path(spec["control"]["socket"])
            try:
                if stat.S_ISSOCK(control_path.lstat().st_mode):
                    control_path.unlink()
            except FileNotFoundError:
                pass
        # On orderly shutdown leave the guardian as the sole lease holder
        # before telling it to perform its final exact-holder sweep. On a
        # crash the kernel closes these descriptors before pipe EOF instead.
        if process_tree_fd is not None:
            os.close(process_tree_fd)
            process_tree_fd = None
        if lock_fd is not None:
            os.close(lock_fd)
        if layout_lock_fd is not None:
            os.close(layout_lock_fd)
        if build_lock_fd is not None:
            os.close(build_lock_fd)
        if port_reservation_fd is not None:
            os.close(port_reservation_fd)
        if runtime_lock_fd is not None:
            os.close(runtime_lock_fd)
        if state_directory_fd is not None:
            os.close(state_directory_fd)
        if state_output_fd is not None:
            os.close(state_output_fd)
        if physical_state_lock_fd is not None:
            os.close(physical_state_lock_fd)
        if guardian_write_fd is not None:
            os.close(guardian_write_fd)
        if guardian_pid is not None:
            try:
                os.waitpid(guardian_pid, 0)
            except ChildProcessError:
                pass
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--lock-fd", type=int)
    parser.add_argument("--layout-lock-fd", type=int)
    parser.add_argument("--build-lock-fd", type=int)
    parser.add_argument("--process-tree-fd", type=int)
    parser.add_argument("--port-reservation-fd", type=int)
    parser.add_argument("--runtime-lock-fd", type=int)
    parser.add_argument("--state-directory-fd", type=int)
    parser.add_argument("--state-output-fd", type=int)
    parser.add_argument("--physical-state-lock-fd", type=int)
    parser.add_argument("--daemonize", action="store_true")
    options = parser.parse_args()
    if options.daemonize and os.fork() != 0:
        return 0
    try:
        arguments = (
            options.spec,
            options.lock_fd,
            options.layout_lock_fd,
            options.build_lock_fd,
            options.process_tree_fd,
            options.port_reservation_fd,
            options.runtime_lock_fd,
        )
        return supervise(
            *arguments,
            options.state_directory_fd,
            options.state_output_fd,
            options.physical_state_lock_fd,
        )
    except BaseException as error:
        message = f"{type(error).__name__}: {error}"
        print(f"topology supervisor failed during startup: {message}", file=sys.stderr)
        try:
            atomic_status(options.spec.parent / "startup-error.json", {"error": message})
        except OSError as status_error:
            print(f"cannot record topology startup failure: {status_error}", file=sys.stderr)
        if options.lock_fd is not None:
            try:
                os.close(options.lock_fd)
            except OSError:
                pass
        if options.layout_lock_fd is not None:
            try:
                os.close(options.layout_lock_fd)
            except OSError:
                pass
        if options.build_lock_fd is not None:
            try:
                os.close(options.build_lock_fd)
            except OSError:
                pass
        if options.process_tree_fd is not None:
            try:
                os.close(options.process_tree_fd)
            except OSError:
                pass
        if options.port_reservation_fd is not None:
            try:
                os.close(options.port_reservation_fd)
            except OSError:
                pass
        if options.runtime_lock_fd is not None:
            try:
                os.close(options.runtime_lock_fd)
            except OSError:
                pass
        if options.state_directory_fd is not None:
            try:
                os.close(options.state_directory_fd)
            except OSError:
                pass
        if options.state_output_fd is not None:
            try:
                os.close(options.state_output_fd)
            except OSError:
                pass
        if options.physical_state_lock_fd is not None:
            try:
                os.close(options.physical_state_lock_fd)
            except OSError:
                pass
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
