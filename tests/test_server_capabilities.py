from __future__ import annotations

import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import supervisor
from atrinik_workspace.model import WorkspaceError, validate_server_listener_spec
from atrinik_workspace.platform_compat import fcntl
from atrinik_workspace.server_capabilities import (
    CAPABILITY_FILE, server_datapath_arguments, validate_server_datapath_launch,
)


@unittest.skipUnless(sys.platform == "linux", "inherited Classic capability is Linux-only")
class ServerCapabilitiesTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.runtime = self.root / "generation" / "server"
        self.runtime.mkdir(parents=True)
        self.state = self.root / "state"
        self.state.mkdir(mode=0o700)
        self.state_fd = os.open(self.state, os.O_RDONLY | os.O_DIRECTORY)
        self.addCleanup(os.close, self.state_fd)
        fcntl.flock(self.state_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        self.lease_fd = os.open(self.runtime.parent / "generation.lease", os.O_RDWR | os.O_CREAT, 0o600)
        self.addCleanup(os.close, self.lease_fd)
        fcntl.flock(self.lease_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        (self.runtime / "data").symlink_to(f"/proc/self/fd/{self.state_fd}")
        self.binary = self.runtime / "atrinik-server"
        self.binary.write_bytes(b"server fixture")
        self.artifact = self.runtime / CAPABILITY_FILE
        self.value = {"schema_version": 1, "datapath_fd": True,
                      "server_sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest()}
        self.write_capability(self.value)

    def write_capability(self, value: object) -> None:
        self.artifact.write_text(json.dumps(value))

    def arguments(self) -> list[str]:
        return server_datapath_arguments(self.runtime, self.state_fd, self.lease_fd)

    def command(self) -> list[str]:
        return [str(self.binary), "--port_quic=12345", "--port_mapping=off", "--stun_server=off",
                *self.arguments(), "--assetspath=/proc/self/fd/99", "--no_console"]

    def test_absent_artifact_preserves_legacy_argv(self) -> None:
        self.artifact.unlink()
        self.assertEqual(self.arguments(), [f"--datapath=/proc/self/fd/{self.state_fd}"])
        self.assertEqual(server_datapath_arguments(self.runtime, None, None), [])

    def test_exact_capability_and_syntax_for_both_listeners(self) -> None:
        self.assertEqual(self.arguments(), [f"--datapath_fd={self.state_fd}", "--datapath=./data"])
        command = self.command()
        validate_server_datapath_launch(command, self.runtime, self.state_fd, self.lease_fd, self.state)
        for listener in ("loopback", "all-ipv4"):
            spec = {"server_listener": listener,
                    "endpoint": {"host": "127.0.0.1", "port": 12345},
                    "runtime": {"path": str(self.runtime.parent)},
                    "services": {"server": {"cwd": str(self.runtime), "command": command.copy()}}}
            if listener == "all-ipv4":
                spec["services"]["server"]["command"].append("--network_stack=ipv4=0.0.0.0")
            validate_server_listener_spec(spec)
            for value in ("0", "1", "2", "03", "+3", "-3", "x"):
                bad = copy.deepcopy(spec)
                bad["services"]["server"]["command"][4] = "--datapath_fd=" + value
                with self.subTest(value=value), self.assertRaises(WorkspaceError):
                    validate_server_listener_spec(bad)

    def test_rejects_malformed_unknown_types_and_hashes(self) -> None:
        for value in (None, [], {}, {**self.value, "extra": True},
                      {**self.value, "schema_version": True}, {**self.value, "schema_version": 2},
                      {**self.value, "datapath_fd": 1}, {**self.value, "datapath_fd": False},
                      {**self.value, "server_sha256": "A" * 64},
                      {**self.value, "server_sha256": "0" * 64},
                      {**self.value, "server_sha256": 123}):
            with self.subTest(value=value):
                self.write_capability(value)
                with self.assertRaises(WorkspaceError):
                    self.arguments()
        for payload in (b"{", b"\xff", b" " * 4097,
                        json.dumps(self.value).replace('"schema_version": 1',
                            '"schema_version": 1, "schema_version": 1').encode()):
            self.artifact.write_bytes(payload)
            with self.subTest(payload=payload[:30]), self.assertRaises(WorkspaceError):
                self.arguments()

    def test_rejects_links_special_files_and_changed_binary(self) -> None:
        for target in (self.artifact, self.binary):
            original = target.read_bytes()
            target.unlink()
            target.symlink_to(self.root / "missing")
            with self.subTest(target=target), self.assertRaises(WorkspaceError):
                self.arguments()
            target.unlink()
            os.mkfifo(target)
            with self.assertRaises(WorkspaceError):
                self.arguments()
            target.unlink()
            target.write_bytes(original)
        self.binary.write_bytes(b"changed executable")
        with self.assertRaises(WorkspaceError):
            self.arguments()

    def test_rejects_untrusted_modes_and_platform(self) -> None:
        for target in (self.artifact, self.binary, self.runtime, self.state):
            mode = target.stat().st_mode & 0o777
            target.chmod(mode | 0o020)
            with self.subTest(target=target), self.assertRaises(WorkspaceError):
                self.arguments()
            target.chmod(mode)
        with mock.patch("atrinik_workspace.server_capabilities.sys.platform", "win32"):
            with self.assertRaisesRegex(WorkspaceError, "Linux"):
                self.arguments()

    def test_rejects_missing_unlocked_or_rebound_descriptors(self) -> None:
        for descriptor, label in ((self.lease_fd, "runtime lease"), (self.state_fd, "state")):
            fcntl.flock(descriptor, fcntl.LOCK_UN)
            with self.subTest(label=label), self.assertRaisesRegex(WorkspaceError, "exclusive lock"):
                self.arguments()
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        closed = os.dup(self.state_fd)
        os.close(closed)
        for descriptor in (None, 0, 2, closed):
            with self.subTest(descriptor=descriptor), self.assertRaises(WorkspaceError):
                server_datapath_arguments(self.runtime, descriptor, self.lease_fd)
        with self.assertRaises(WorkspaceError):
            server_datapath_arguments(self.runtime, self.state_fd, self.state_fd)
        with self.assertRaises(WorkspaceError):
            server_datapath_arguments(self.runtime, self.state_fd, None)

    def test_rejects_argv_link_and_path_mismatches(self) -> None:
        command = self.command()
        for changed in ([*command[:4], "--datapath_fd=999", *command[5:]],
                        [*command, "--datapath=elsewhere"],
                        [*command[:4], f"--datapath=/proc/self/fd/{self.state_fd}", *command[6:]],
                        ["other-server", *command[1:]]):
            with self.subTest(command=changed), self.assertRaises(WorkspaceError):
                validate_server_datapath_launch(changed, self.runtime, self.state_fd, self.lease_fd)
        self.artifact.unlink()
        with self.assertRaises(WorkspaceError):
            validate_server_datapath_launch(command, self.runtime, self.state_fd, self.lease_fd)
        self.write_capability(self.value)
        link = self.runtime / "data"
        link.unlink()
        link.symlink_to(self.state)
        with self.assertRaisesRegex(WorkspaceError, "data link"):
            self.arguments()
        link.unlink()
        link.symlink_to(f"/proc/self/fd/{self.state_fd}")
        self.state.rename(self.root / "old-state")
        self.state.mkdir(mode=0o700)
        with self.assertRaisesRegex(WorkspaceError, "state path identity"):
            validate_server_datapath_launch(command, self.runtime, self.state_fd, self.lease_fd, self.state)

    def test_borrower_dup_preserves_exclusive_lock_after_close(self) -> None:
        command = self.command()
        validate_server_datapath_launch(command, self.runtime, self.state_fd, self.lease_fd, self.state)
        script = """import fcntl, os, sys
fd = int(sys.argv[1])
borrowed = os.dup(fd)
fcntl.flock(borrowed, fcntl.LOCK_EX | fcntl.LOCK_NB)
os.close(borrowed)
"""
        subprocess.run([sys.executable, "-c", script, str(self.state_fd)],
                       pass_fds=(self.state_fd,), check=True)
        other = os.open(self.state, os.O_RDONLY | os.O_DIRECTORY)
        try:
            with self.assertRaises(BlockingIOError):
                fcntl.flock(other, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.arguments()
        finally:
            os.close(other)

    def test_failure_closes_only_owned_validation_handles(self) -> None:
        descriptors = set(os.listdir("/proc/self/fd"))
        self.write_capability({})
        with self.assertRaises(WorkspaceError):
            self.arguments()
        self.assertEqual(set(os.listdir("/proc/self/fd")), descriptors)
        os.fstat(self.state_fd)
        os.fstat(self.lease_fd)

    def test_supervisor_rejects_mismatch_before_popen_and_releases_owned_handles(self) -> None:
        for mutation in ("argv", "link", "digest"):
            with self.subTest(mutation=mutation):
                state_fd = os.dup(self.state_fd)
                lease_fd = os.dup(self.lease_fd)
                link = self.runtime / "data"
                link.unlink()
                link.symlink_to(f"/proc/self/fd/{state_fd}")
                self.write_capability(self.value)
                command = [str(self.binary), "--port_quic=12345", "--port_mapping=off",
                           "--stun_server=off", f"--datapath_fd={state_fd}",
                           "--datapath=./data", "--assetspath=/proc/self/fd/99", "--no_console"]
                if mutation == "argv":
                    command[4] = "--datapath_fd=999"
                elif mutation == "link":
                    link.unlink()
                    link.symlink_to(self.state)
                else:
                    self.write_capability({**self.value, "server_sha256": "0" * 64})
                spec = {"schema_version": 3, "name": "capability-test", "profile": "classic",
                        "stack": "classic", "providers": {}, "dependencies": ["server"],
                        "state": str(self.state), "build_root": str(self.root), "resolved": {},
                        "server_listener": "loopback",
                        "endpoint": {"host": "127.0.0.1", "port": 12345},
                        "runtime": {"path": str(self.runtime.parent), "generation": "a" * 64},
                        "services": {"server": {"cwd": str(self.runtime), "command": command,
                                                 "log": str(self.root / "server.log")}}}
                spec_path = self.root / "spec.json"
                spec_path.write_text(json.dumps(spec))
                with (mock.patch.object(supervisor, "_validate_port_reservation"),
                      mock.patch.object(supervisor, "_require_server_port_available"),
                      mock.patch.object(supervisor, "_start_guardian", return_value=(None, None)),
                      mock.patch.object(supervisor, "_open_control", return_value=None),
                      mock.patch.object(supervisor, "terminate", return_value=True),
                      mock.patch.object(supervisor.subprocess, "Popen") as popen):
                    self.assertEqual(supervisor.supervise(spec_path, None, None, None, None, None,
                                                         lease_fd, state_fd), 1)
                popen.assert_not_called()
                self.assertIn("capability", json.loads((self.root / "status.json").read_text())["error"])
                for descriptor in (state_fd, lease_fd):
                    with self.assertRaises(OSError):
                        os.fstat(descriptor)
                os.fstat(self.state_fd)
                os.fstat(self.lease_fd)
