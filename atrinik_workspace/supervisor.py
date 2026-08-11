from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import tempfile
import threading
import time
from typing import Any, BinaryIO

from .launch_identity import CLIENT_LAUNCH_LABEL_ENV, client_launch_label


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


def terminate(processes: dict[str, subprocess.Popen[bytes]]) -> None:
    for process in processes.values():
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline and any(
        process.poll() is None for process in processes.values()
    ):
        time.sleep(0.1)
    for process in processes.values():
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    for process in processes.values():
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            pass


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
    status: dict[str, Any] = {
        "schema_version": 1,
        "name": spec["name"],
        "profile": spec["profile"],
        "dependencies": spec["dependencies"],
        "state": spec["state"],
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
        "supervisor": {
            "pid": os.getpid(),
            "start_time": supervisor_start_time,
        },
        "services": {},
    }
    if "stack" in spec or "providers" in spec:
        if not isinstance(spec.get("stack"), str) or not isinstance(
            spec.get("providers"), dict
        ):
            raise RuntimeError("topology spec stack/provider identity is incomplete")
        status["stack"] = spec["stack"]
        status["providers"] = spec["providers"]
    return status


def supervise(
    spec_path: Path,
    lock_fd: int | None,
    layout_lock_fd: int | None,
    build_lock_fd: int | None,
) -> int:
    with spec_path.open(encoding="utf-8") as stream:
        spec = json.load(stream)
    status_path = spec_path.parent / "status.json"
    stop = False

    def request_stop(_signum: int, _frame: object) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    supervisor_start_time = process_start_time(os.getpid())
    if supervisor_start_time is None:
        raise RuntimeError("cannot identify topology supervisor process")
    status = _initial_status(spec, supervisor_start_time)
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
        if name == "server" and lock_fd is not None:
            inherited_locks.append(lock_fd)
        if layout_lock_fd is not None:
            inherited_locks.append(layout_lock_fd)
        if build_lock_fd is not None:
            inherited_locks.append(build_lock_fd)
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
            "status": "starting" if capture is not None else "running",
            "exit_code": None,
            "log": str(log),
            "cwd": service["cwd"],
        }
        atomic_status(status_path, status)
        return process

    try:
        if "server" in spec["services"]:
            capture = ServerReadinessCapture()
            server = start_service("server", capture=capture)
            deadline = time.monotonic() + SERVER_READY_TIMEOUT
            while not capture.event.wait(timeout=0.1):
                if server.poll() is not None:
                    raise RuntimeError(
                        f"server exited before becoming ready with code {server.returncode}"
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

        while not stop and all(
            process.poll() is None for process in processes.values()
        ):
            changed = False
            for name, process in processes.items():
                code = process.poll()
                service_status = status["services"][name]
                if code is not None and service_status["status"] == "running":
                    service_status["status"] = "exited"
                    service_status["exit_code"] = code
                    changed = True
            if changed:
                atomic_status(status_path, status)
            time.sleep(0.2)
    except BaseException as error:
        status["error"] = f"{type(error).__name__}: {error}"
        atomic_status(status_path, status)
        return 1
    finally:
        terminate(processes)
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
        atomic_status(status_path, status)
        for output in logs:
            output.close()
        if lock_fd is not None:
            os.close(lock_fd)
        if layout_lock_fd is not None:
            os.close(layout_lock_fd)
        if build_lock_fd is not None:
            os.close(build_lock_fd)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--lock-fd", type=int)
    parser.add_argument("--layout-lock-fd", type=int)
    parser.add_argument("--build-lock-fd", type=int)
    parser.add_argument("--daemonize", action="store_true")
    options = parser.parse_args()
    if options.daemonize and os.fork() != 0:
        return 0
    try:
        return supervise(
            options.spec,
            options.lock_fd,
            options.layout_lock_fd,
            options.build_lock_fd,
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
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
