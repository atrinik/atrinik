from __future__ import annotations

import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

from atrinik_workspace import supervisor as supervisor_module
from atrinik_workspace.supervisor import (
    ServerReadinessCapture,
    _initial_status,
    supervise,
)


class ServerReadinessCaptureTests(unittest.TestCase):
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
                self.assertEqual(supervise(spec_path, None, 7, 8, None), 1)

            terminate.assert_called_once()
            self.assertEqual(terminate.call_args.args[0], {"client": process})
            self.assertIsNone(terminate.call_args.args[1])
            self.assertEqual(
                {call.args[0] for call in close.call_args_list}, {7, 8}
            )

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
                    "child == 0 and signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                    "child == 0 and pathlib.Path(sys.argv[1]).write_text(str(os.getpid())); "
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
                    "child == 0 and os.close(int(sys.argv[2])); "
                    "child == 0 and signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                    "child == 0 and pathlib.Path(sys.argv[1]).write_text(str(os.getpid())); "
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
            },
            "123",
        )
        self.assertEqual(current["stack"], "classic")
        self.assertEqual(current["providers"], {"server": "classic-server"})

        with self.assertRaisesRegex(RuntimeError, "identity is incomplete"):
            _initial_status({**spec, "stack": "classic"}, "123")


if __name__ == "__main__":
    unittest.main()
