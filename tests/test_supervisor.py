from __future__ import annotations

import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

from atrinik_workspace import supervisor as supervisor_module
from atrinik_workspace.supervisor import (
    ServerReadinessCapture,
    _initial_status,
    _open_control,
    _receive_control,
    _serve_control,
    supervise,
)
from atrinik_workspace.process_tree import control_socket_path


class ServerReadinessCaptureTests(unittest.TestCase):
    def test_control_messages_are_bounded_and_may_arrive_in_chunks(self) -> None:
        connection = mock.Mock()
        connection.recv.side_effect = [b'{"action": "sta', b'tus"}\n']

        self.assertEqual(_receive_control(connection), {"action": "status"})

        oversized = mock.Mock()
        oversized.recv.return_value = b"x" * 4097
        with self.assertRaisesRegex(ValueError, "too large"):
            _receive_control(oversized)

    def test_control_endpoint_is_fixed_below_topology_root(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec = {
                "control": {
                    "socket": str(root.parent / "unowned.sock"),
                    "generation": "a" * 64,
                    "lease": {"device": 1, "inode": 2},
                }
            }

            with self.assertRaisesRegex(RuntimeError, "identity is invalid"):
                _open_control(spec, root)

    def test_control_endpoint_supports_long_managed_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory) / "workspace"
            root = workspace / "topologies" / ("managed-" + "x" * 110)
            root.mkdir(parents=True)
            generation = "a" * 64
            control = control_socket_path(root, generation)
            control.parent.mkdir(mode=0o700)
            spec = {
                "control": {
                    "socket": str(control),
                    "generation": generation,
                    "lease": {"device": 1, "inode": 2},
                }
            }

            endpoint = _open_control(spec, root)
            try:
                self.assertTrue(control.is_socket())
            finally:
                assert endpoint is not None
                endpoint.close()
                control.unlink()

    def test_control_endpoint_drains_concurrent_status_requests(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "workspace" / "topologies" / "review"
            root.mkdir(parents=True)
            generation = "a" * 64
            control = control_socket_path(root, generation)
            control.parent.mkdir(mode=0o700)
            spec = {
                "name": "review",
                "control": {
                    "socket": str(control),
                    "generation": generation,
                    "lease": {"device": 1, "inode": 2},
                },
            }
            endpoint = _open_control(spec, root)
            responses: list[dict[str, object]] = []

            def request() -> None:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                    client.settimeout(2)
                    client.connect(str(control))
                    client.sendall(
                        json.dumps(
                            {
                                "action": "status",
                                "name": "review",
                                "generation": generation,
                            }
                        ).encode()
                    )
                    client.shutdown(socket.SHUT_WR)
                    responses.append(json.loads(client.recv(4096)))

            clients = [threading.Thread(target=request) for _index in range(12)]
            try:
                for client in clients:
                    client.start()
                time.sleep(0.05)
                self.assertFalse(_serve_control(endpoint, spec))
                for client in clients:
                    client.join(timeout=2)
                self.assertEqual(len(responses), len(clients))
                self.assertTrue(all(response["ok"] is True for response in responses))
            finally:
                assert endpoint is not None
                endpoint.close()
                control.unlink(missing_ok=True)

    def test_peek_exit_code_observes_without_reaping(self) -> None:
        process = mock.Mock(pid=1234, returncode=7)
        with mock.patch.object(supervisor_module.os, "waitid", return_value=None):
            self.assertIsNone(supervisor_module._peek_exit_code(process))
        with mock.patch.object(
            supervisor_module.os,
            "waitid",
            return_value=mock.Mock(si_code=os.CLD_EXITED, si_status=3),
        ):
            self.assertEqual(supervisor_module._peek_exit_code(process), 3)
        with mock.patch.object(
            supervisor_module.os,
            "waitid",
            return_value=mock.Mock(si_code=os.CLD_KILLED, si_status=signal.SIGTERM),
        ):
            self.assertEqual(
                supervisor_module._peek_exit_code(process), -signal.SIGTERM
            )
        with mock.patch.object(
            supervisor_module.os, "waitid", side_effect=ChildProcessError
        ):
            self.assertEqual(supervisor_module._peek_exit_code(process), 7)

    def test_unidentified_service_is_registered_for_bounded_cleanup(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec = {
                "name": "cleanup",
                "profile": "default",
                "dependencies": ["client"],
                "state": None,
                "build_root": "/tmp/build",
                "resolved": {},
                "endpoint": None,
                "services": {
                    "client": {
                        "command": ["client"],
                        "cwd": str(root),
                        "log": str(root / "client.log"),
                    }
                },
            }
            spec_path = root / "spec.json"
            spec_path.write_text(json.dumps(spec), encoding="utf-8")
            process = mock.MagicMock(pid=1234)

            with (
                mock.patch.object(
                    supervisor_module,
                    "process_start_time",
                    side_effect=["1", None],
                ),
                mock.patch.object(
                    supervisor_module.subprocess, "Popen", return_value=process
                ),
                mock.patch.object(supervisor_module, "terminate") as terminate,
                mock.patch.object(supervisor_module.os, "close") as close,
            ):
                self.assertEqual(supervise(spec_path, None, 7, 8, None, None), 1)

            terminate.assert_called_once()
            self.assertEqual(terminate.call_args.args[0], {"client": process})
            self.assertIsNone(terminate.call_args.args[1])
            self.assertTrue(
                {7, 8} <= {call.args[0] for call in close.call_args_list}
            )

    def test_current_supervisor_releases_broad_leases_and_retains_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runtime = {
                "schema_version": 1,
                "generation": "a" * 64,
                "root": str(root / "runtime"),
                "lease": {"device": 1, "inode": 2},
            }
            spec = {
                "schema_version": 2,
                "name": "cleanup",
                "profile": "default",
                "dependencies": ["client"],
                "state": None,
                "build_root": "/tmp/build",
                "resolved": {},
                "endpoint": None,
                "runtime": runtime,
                "services": {
                    "client": {
                        "command": ["client"],
                        "cwd": str(root),
                        "log": str(root / "client.log"),
                    }
                },
            }
            spec_path = root / "spec.json"
            spec_path.write_text(json.dumps(spec), encoding="utf-8")
            process = mock.MagicMock(pid=1234)

            with (
                mock.patch.object(
                    supervisor_module,
                    "process_start_time",
                    side_effect=["1", None],
                ),
                mock.patch.object(
                    supervisor_module.subprocess, "Popen", return_value=process
                ),
                mock.patch.object(
                    supervisor_module,
                    "_start_guardian",
                    wraps=supervisor_module._start_guardian,
                ) as start_guardian,
                mock.patch.object(supervisor_module, "terminate") as terminate,
                mock.patch.object(supervisor_module.os, "close") as close,
            ):
                self.assertEqual(
                    supervise(spec_path, 6, 7, 8, None, None, 9), 1
                )

            start_guardian.assert_called_once_with(None, None, 6, 9)
            terminate.assert_called_once()
            self.assertTrue(
                {6, 7, 8, 9}
                <= {call.args[0] for call in close.call_args_list}
            )
            status = json.loads(
                (root / "status.json").read_text(encoding="utf-8")
            )
            self.assertEqual(status["runtime"], runtime)

    def test_terminate_cleans_group_after_service_leader_exits(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            descendant_path = Path(directory) / "descendant.pid"
            lease = Path(directory) / "process-tree.lease"
            lease_fd = os.open(lease, os.O_RDWR | os.O_CREAT, 0o600)
            process = subprocess.Popen(
                [
                    sys.executable,
                    "-c",
                    "import os, pathlib, signal, sys, time; "
                    "child = os.fork(); "
                    "path = pathlib.Path(sys.argv[1]); "
                    "child == 0 and signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                    "child == 0 and path.with_suffix('.tmp').write_text(str(os.getpid())); "
                    "child == 0 and path.with_suffix('.tmp').replace(path); "
                    "child == 0 and time.sleep(60); "
                    "os._exit(0)",
                    str(descendant_path),
                ],
                start_new_session=True,
                pass_fds=(lease_fd,),
            )
            descendant = None
            try:
                process.wait(timeout=5)
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and not descendant_path.is_file():
                    time.sleep(0.05)
                descendant = int(descendant_path.read_text(encoding="utf-8"))
                self.assertTrue(Path(f"/proc/{descendant}").exists())

                supervisor_module.terminate(
                    {"client": process}, lease_fd, timeout=0.1
                )
                deadline = time.monotonic() + 2
                while (
                    time.monotonic() < deadline
                    and Path(f"/proc/{descendant}").exists()
                ):
                    time.sleep(0.05)
                self.assertFalse(Path(f"/proc/{descendant}").exists())
            finally:
                os.close(lease_fd)
                if descendant is not None and Path(f"/proc/{descendant}").exists():
                    try:
                        os.kill(descendant, signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    def test_terminate_cleans_group_after_leader_exits_and_descendant_closes_lease(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as directory:
            descendant_path = Path(directory) / "descendant.pid"
            lease_fd = os.open(
                Path(directory) / "process-tree.lease",
                os.O_RDWR | os.O_CREAT,
                0o600,
            )
            process = subprocess.Popen(
                [
                    sys.executable,
                    "-c",
                    "import os, pathlib, signal, sys, time; "
                    "child = os.fork(); "
                    "path = pathlib.Path(sys.argv[1]); "
                    "child == 0 and os.close(int(sys.argv[2])); "
                    "child == 0 and signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                    "child == 0 and path.with_suffix('.tmp').write_text(str(os.getpid())); "
                    "child == 0 and path.with_suffix('.tmp').replace(path); "
                    "child == 0 and time.sleep(60); "
                    "os._exit(0)",
                    str(descendant_path),
                    str(lease_fd),
                ],
                start_new_session=True,
                pass_fds=(lease_fd,),
            )
            descendant = None
            try:
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and not descendant_path.is_file():
                    time.sleep(0.05)
                descendant = int(descendant_path.read_text(encoding="utf-8"))
                deadline = time.monotonic() + 5
                while (
                    time.monotonic() < deadline
                    and supervisor_module._peek_exit_code(process) is None
                ):
                    time.sleep(0.05)
                self.assertIsNotNone(supervisor_module._peek_exit_code(process))
                self.assertIsNone(process.returncode)

                supervisor_module.terminate(
                    {"client": process}, lease_fd, timeout=0.1
                )
                deadline = time.monotonic() + 2
                while (
                    time.monotonic() < deadline
                    and Path(f"/proc/{descendant}").exists()
                ):
                    time.sleep(0.05)
                self.assertFalse(Path(f"/proc/{descendant}").exists())
            finally:
                os.close(lease_fd)
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=2)
                if descendant is not None and Path(f"/proc/{descendant}").exists():
                    try:
                        os.kill(descendant, signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    def test_terminate_without_process_tree_lease_uses_group_fallback(self) -> None:
        process = subprocess.Popen(
            [
                sys.executable,
                "-c",
                "import signal, time; "
                "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                "print('ready', flush=True); "
                "time.sleep(60)",
            ],
            start_new_session=True,
            stdout=subprocess.PIPE,
        )
        try:
            assert process.stdout is not None
            self.assertEqual(process.stdout.readline(), b"ready\n")
            supervisor_module.terminate(
                {"client": process}, None, timeout=0.1
            )
            self.assertIsNotNone(process.poll())
        finally:
            if process.stdout is not None:
                process.stdout.close()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)

    def test_terminate_rejects_abnormal_service_exit_as_clean(self) -> None:
        process = subprocess.Popen(
            [
                sys.executable,
                "-c",
                "import signal, sys, time; "
                "signal.signal(signal.SIGTERM, lambda *_args: sys.exit(23)); "
                "print('ready', flush=True); time.sleep(60)",
            ],
            start_new_session=True,
            stdout=subprocess.PIPE,
        )
        try:
            assert process.stdout is not None
            self.assertEqual(process.stdout.readline(), b"ready\n")
            self.assertFalse(
                supervisor_module.terminate(
                    {"server": process}, None, timeout=2
                )
            )
            self.assertEqual(process.returncode, 23)
        finally:
            if process.stdout is not None:
                process.stdout.close()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)

    def test_terminate_timeout_cannot_be_clean(self) -> None:
        process = mock.Mock(pid=1234, returncode=0)
        process.wait.side_effect = subprocess.TimeoutExpired("server", 2)
        with (
            mock.patch.object(supervisor_module.Path, "iterdir", return_value=[]),
            mock.patch.object(supervisor_module.os, "killpg"),
        ):
            self.assertFalse(
                supervisor_module.terminate(
                    {"server": process}, None, timeout=0
                )
            )

    def test_terminate_rejects_process_group_observation_uncertainty(self) -> None:
        process = mock.Mock(pid=1234, returncode=0)
        with mock.patch.object(
            supervisor_module.Path,
            "iterdir",
            side_effect=OSError("proc unavailable"),
        ):
            self.assertFalse(
                supervisor_module.terminate(
                    {"server": process}, None, timeout=0
                )
            )

        for failure in (OSError("stat unavailable"), ValueError("malformed stat")):
            with self.subTest(failure=type(failure).__name__):
                with (
                    mock.patch.object(
                        supervisor_module.Path,
                        "iterdir",
                        return_value=[Path("/proc/1234")],
                    ),
                    mock.patch.object(
                        supervisor_module.Path,
                        "read_text",
                        side_effect=failure,
                    ),
                    mock.patch.object(supervisor_module.os, "killpg"),
                ):
                    self.assertFalse(
                        supervisor_module.terminate(
                            {"server": process}, None, timeout=0
                        )
                    )

    def test_requires_fingerprint_and_finished_server_startup(self) -> None:
        capture = ServerReadinessCapture()

        capture.feed(b"QUIC certificate SHA-256: " + b"A" * 64 + b"\n")

        self.assertEqual(capture.fingerprint, "a" * 64)
        self.assertFalse(capture.event.is_set())

        capture.feed(b"Server ready. Waiting for connections...\n")

        self.assertTrue(capture.event.is_set())

    def test_recognizes_readiness_messages_split_across_chunks(self) -> None:
        capture = ServerReadinessCapture()

        capture.feed(b"Server ready. Waiting for connec")
        capture.feed(b"tions...\nQUIC certificate SHA-256: " + b"b" * 31)
        capture.feed(b"b" * 33 + b"\n")

        self.assertEqual(capture.fingerprint, "b" * 64)
        self.assertTrue(capture.event.is_set())

    def test_historical_spec_remains_identifiably_historical(self) -> None:
        spec = {
            "name": "historical",
            "profile": "classic",
            "dependencies": ["server"],
            "state": "/tmp/state",
            "build_root": "/tmp/build",
            "resolved": {},
            "endpoint": None,
        }

        historical = _initial_status(spec, "123")

        self.assertNotIn("stack", historical)
        self.assertNotIn("providers", historical)

        current = _initial_status(
            {
                **spec,
                "stack": "classic",
                "providers": {"server": "classic-server"},
                "sound": {"mode": "local-playtest", "root": "/tmp/sound"},
            },
            "123",
        )
        self.assertEqual(current["stack"], "classic")
        self.assertEqual(current["providers"], {"server": "classic-server"})
        self.assertEqual(
            current["sound"],
            {"mode": "local-playtest", "root": "/tmp/sound"},
        )

        with self.assertRaisesRegex(RuntimeError, "identity is incomplete"):
            _initial_status({**spec, "stack": "classic"}, "123")

    def test_main_closes_runtime_lease_after_startup_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            spec_path = Path(directory) / "spec.json"
            with (
                mock.patch.object(
                    sys,
                    "argv",
                    [
                        "atrinik-supervisor",
                        "--spec",
                        str(spec_path),
                        "--runtime-lock-fd",
                        "9",
                    ],
                ),
                mock.patch.object(
                    supervisor_module,
                    "supervise",
                    side_effect=RuntimeError("invalid runtime"),
                ) as supervise_call,
                mock.patch.object(supervisor_module, "atomic_status") as status,
                mock.patch.object(supervisor_module.os, "close") as close,
                mock.patch.object(sys, "stderr"),
            ):
                self.assertEqual(supervisor_module.main(), 1)

        supervise_call.assert_called_once_with(
                spec_path, None, None, None, None, None, 9, None, None, None
        )
        status.assert_called_once_with(
            spec_path.parent / "startup-error.json",
            {"error": "RuntimeError: invalid runtime"},
        )
        close.assert_called_once_with(9)

    def test_guardian_releases_retained_leases_before_tree_barrier(self) -> None:
        with (
            mock.patch.object(supervisor_module.os, "read", return_value=b""),
            mock.patch.object(supervisor_module.os, "getpid", return_value=1234),
            mock.patch.object(supervisor_module.os, "close") as close,
            mock.patch.object(supervisor_module, "signal_holders"),
            mock.patch.object(
                supervisor_module, "holders_exist", return_value=False
            ),
            mock.patch.object(
                supervisor_module.time, "monotonic", side_effect=[0, 11, 11, 14]
            ),
        ):
            supervisor_module._guardian(10, 20, (30, None, 40))

        self.assertEqual(
            [call.args[0] for call in close.call_args_list],
            [10, 30, 40, 20],
        )

    def test_current_supervisor_orderly_stop_closes_exact_leases(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec_path = root / "spec.json"
            spec_path.write_text(
                json.dumps(
                    {
                        "schema_version": 2,
                        "name": "orderly",
                        "profile": "default",
                        "dependencies": [],
                        "state": None,
                        "build_root": "/tmp/build",
                        "resolved": {},
                        "endpoint": None,
                        "runtime": {"generation": "a" * 64},
                        "services": {},
                    }
                ),
                encoding="utf-8",
            )
            with (
                mock.patch.object(
                    supervisor_module, "process_start_time", return_value="1"
                ),
                mock.patch.object(
                    supervisor_module, "_validate_port_reservation"
                ),
                mock.patch.object(
                    supervisor_module, "_serve_control", return_value=True
                ),
                mock.patch.object(
                    supervisor_module, "_peek_exit_code", return_value=None
                ),
                mock.patch.object(supervisor_module, "terminate"),
                mock.patch.object(supervisor_module.os, "close") as close,
            ):
                self.assertEqual(
                    supervise(
                        spec_path,
                        None,
                        None,
                        None,
                        None,
                        8,
                        9,
                        10,
                        11,
                        12,
                    ),
                    0,
                )

            self.assertTrue(
                set(range(8, 13))
                <= {call.args[0] for call in close.call_args_list}
            )

    def test_guardian_startup_failure_preserves_tree_barrier_order(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec_path = root / "spec.json"
            spec_path.write_text(
                json.dumps(
                    {
                        "schema_version": 3,
                        "name": "guardian-failure",
                        "profile": "classic",
                        "dependencies": [],
                        "state": str(root / "state"),
                        "state_policy": {"mode": "named"},
                        "build_root": "/tmp/build",
                        "resolved": {},
                        "endpoint": None,
                        "runtime": {"generation": "a" * 64},
                        "services": {},
                    }
                ),
                encoding="utf-8",
            )
            with (
                mock.patch.object(
                    supervisor_module, "process_start_time", return_value="1"
                ),
                mock.patch.object(supervisor_module, "_validate_port_reservation"),
                mock.patch.object(
                    supervisor_module,
                    "_start_guardian",
                    side_effect=OSError("fork failed"),
                ),
                mock.patch.object(supervisor_module, "terminate"),
                mock.patch.object(supervisor_module.os, "close") as close,
            ):
                self.assertEqual(
                    supervise(
                        spec_path,
                        3,
                        None,
                        None,
                        4,
                        5,
                        6,
                        7,
                        8,
                        9,
                    ),
                    1,
                )

            self.assertEqual(
                [call.args[0] for call in close.call_args_list[-7:]],
                [3, 5, 6, 7, 8, 9, 4],
            )

    def test_server_launch_inherits_exact_state_descriptors(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            spec_path = root / "spec.json"
            spec_path.write_text(
                json.dumps(
                    {
                        "schema_version": 3,
                        "name": "state-fds",
                        "profile": "classic",
                        "dependencies": ["server"],
                        "state": str(root / "state"),
                        "state_policy": {"mode": "named"},
                        "build_root": "/tmp/build",
                        "resolved": {},
                        "endpoint": {
                            "host": "127.0.0.1",
                            "port": 13327,
                            "fingerprint": None,
                        },
                        "runtime": {"generation": "a" * 64},
                        "control": {
                            "generation": "a" * 64,
                            "socket": str(root / "control.sock"),
                        },
                        "services": {
                            "server": {
                                "command": ["server"],
                                "cwd": str(root),
                                "log": str(root / "server.log"),
                            }
                        },
                    }
                ),
                encoding="utf-8",
            )
            process = mock.MagicMock(pid=1234)
            process.poll.return_value = 0
            capture = mock.Mock(
                event=mock.Mock(wait=mock.Mock(side_effect=[False, True])),
                fingerprint="a" * 64,
            )
            control = mock.Mock()
            with (
                mock.patch.object(
                    supervisor_module,
                    "process_start_time",
                    side_effect=["1", "2"],
                ),
                mock.patch.object(supervisor_module, "_validate_port_reservation"),
                mock.patch.object(supervisor_module, "_require_server_port_available"),
                mock.patch.object(
                    supervisor_module, "_start_guardian", return_value=(4321, 99)
                ) as guardian,
                mock.patch.object(
                    supervisor_module, "_open_control", return_value=control
                ),
                mock.patch.object(
                    supervisor_module,
                    "_serve_control",
                    side_effect=[False, True],
                ),
                mock.patch.object(
                    supervisor_module, "_peek_exit_code", return_value=None
                ),
                mock.patch.object(
                    supervisor_module, "ServerReadinessCapture", return_value=capture
                ),
                mock.patch.object(
                    supervisor_module.subprocess, "Popen", return_value=process
                ) as popen,
                mock.patch.object(supervisor_module.threading, "Thread"),
                mock.patch.object(supervisor_module, "terminate", return_value=True),
                mock.patch.object(supervisor_module.os, "close") as close,
                mock.patch.object(supervisor_module.os, "waitpid") as waitpid,
            ):
                self.assertEqual(
                    supervise(
                        spec_path,
                        3,
                        None,
                        None,
                        4,
                        5,
                        6,
                        7,
                        8,
                        9,
                    ),
                    0,
                )
            self.assertEqual(popen.call_args.kwargs["pass_fds"], (4, 7, 8))
            guardian.assert_called_once_with(4, 5, 3, 6, 7, 8, 9)
            waitpid.assert_called_once_with(4321, 0)
            self.assertEqual(
                [call.args[0] for call in close.call_args_list[-8:]],
                [3, 5, 6, 7, 8, 9, 4, 99],
            )

    def test_main_daemon_parent_returns_without_supervising(self) -> None:
        with (
            mock.patch.object(
                sys,
                "argv",
                ["atrinik-supervisor", "--spec", "/tmp/spec.json", "--daemonize"],
            ),
            mock.patch.object(supervisor_module.os, "fork", return_value=1234),
            mock.patch.object(supervisor_module, "supervise") as supervise_call,
        ):
            self.assertEqual(supervisor_module.main(), 0)
        supervise_call.assert_not_called()

    def test_main_tolerates_runtime_lease_already_closed_on_failure(self) -> None:
        with (
            mock.patch.object(
                sys,
                "argv",
                [
                    "atrinik-supervisor",
                    "--spec",
                    "/tmp/spec.json",
                    "--runtime-lock-fd",
                    "9",
                ],
            ),
            mock.patch.object(
                supervisor_module,
                "supervise",
                side_effect=RuntimeError("invalid runtime"),
            ),
            mock.patch.object(supervisor_module, "atomic_status"),
            mock.patch.object(supervisor_module.os, "close", side_effect=OSError),
            mock.patch.object(sys, "stderr"),
        ):
            self.assertEqual(supervisor_module.main(), 1)

    def test_main_closes_every_current_lease_after_startup_failure(self) -> None:
        arguments = [
            "atrinik-supervisor",
            "--spec",
            "/tmp/spec.json",
            "--lock-fd",
            "3",
            "--layout-lock-fd",
            "4",
            "--build-lock-fd",
            "5",
            "--process-tree-fd",
            "6",
            "--port-reservation-fd",
            "7",
            "--runtime-lock-fd",
            "8",
            "--state-directory-fd",
            "9",
            "--state-output-fd",
            "10",
            "--physical-state-lock-fd",
            "11",
        ]
        with (
            mock.patch.object(sys, "argv", arguments),
            mock.patch.object(
                supervisor_module,
                "supervise",
                side_effect=RuntimeError("invalid runtime"),
            ),
            mock.patch.object(supervisor_module, "atomic_status"),
            mock.patch.object(supervisor_module.os, "close") as close,
            mock.patch.object(sys, "stderr"),
        ):
            self.assertEqual(supervisor_module.main(), 1)
        self.assertEqual(
            [call.args[0] for call in close.call_args_list],
            [3, 4, 5, 7, 8, 9, 10, 11, 6],
        )


if __name__ == "__main__":
    unittest.main()
