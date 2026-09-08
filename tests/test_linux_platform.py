from __future__ import annotations

import os
import io
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import sys
import unittest
from unittest import mock

from atrinik_workspace import linux_platform as platform


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux filesystem and desktop capabilities")
class LinuxPlatformTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.root.chmod(0o700)
        self.socket = socket.socket(socket.AF_UNIX)
        self.addCleanup(self.socket.close)
        self.socket.bind(str(self.root / "wayland-1"))
        self.environment = {"XDG_RUNTIME_DIR": str(self.root), "WAYLAND_DISPLAY": "wayland-1"}

    def options(self, **values: object) -> list[str]:
        return platform.desktop_options(display="wayland", environment=self.environment,
                                        uid=os.geteuid(), gpu="nvidia", **values)

    def test_wayland_shares_one_socket_without_host_runtime_tree(self) -> None:
        args = self.options()
        mounts = [args[i + 1] for i, value in enumerate(args) if value == "--mount"]
        self.assertEqual(mounts, [
            f"type=bind,source={self.root}/wayland-1,target=/run/atrinik-wayland,readonly"])
        self.assertNotIn("--privileged", args)
        self.assertNotIn("--network", args)
        self.assertIn("NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics,display", args)
        self.assertFalse(any("PULSE" in value for value in args))

    def test_wayland_missing_forged_foreign_or_escaping_endpoint_rejected(self) -> None:
        for value in ("../wayland-1", "/tmp/socket", ".", "missing", "bad,readonly"):
            with self.subTest(value=value):
                self.environment["WAYLAND_DISPLAY"] = value
                with self.assertRaises(platform.PlatformError):
                    self.options()

    def test_symlink_and_wrong_owner_are_rejected(self) -> None:
        (self.root / "alias").symlink_to(self.root / "wayland-1")
        self.environment["WAYLAND_DISPLAY"] = "alias"
        with self.assertRaises(platform.PlatformError):
            self.options()
        self.environment["WAYLAND_DISPLAY"] = "wayland-1"
        with self.assertRaises(platform.PlatformError):
            platform.desktop_options(display="wayland", environment=self.environment,
                                     uid=os.geteuid() + 1, gpu="nvidia")

    def test_audio_requires_its_own_actual_socket(self) -> None:
        with self.assertRaises(platform.PlatformError):
            self.options(audio=True)
        (self.root / "pulse").mkdir(mode=0o700)
        audio = socket.socket(socket.AF_UNIX)
        self.addCleanup(audio.close)
        audio.bind(str(self.root / "pulse/native"))
        self.assertIn("PULSE_SERVER=unix:/run/atrinik-pulse", self.options(audio=True))

    def test_mesa_requires_explicit_actual_render_device(self) -> None:
        with self.assertRaises(platform.PlatformError):
            platform.desktop_options(display="wayland", environment=self.environment,
                                     uid=os.geteuid(), gpu="mesa")
        with self.assertRaises(platform.PlatformError):
            platform.desktop_options(display="wayland", environment=self.environment,
                                     uid=os.geteuid(), gpu="mesa", render_devices=(Path("/dev/null"),))

    def test_x11_requires_local_display_and_private_explicit_authority(self) -> None:
        for environment in ({}, {"DISPLAY": "localhost:10"}, {"DISPLAY": ":0"}):
            with self.subTest(environment=environment):
                with self.assertRaises(platform.PlatformError):
                    platform.desktop_options(display="x11", environment=environment,
                                             uid=os.geteuid(), gpu="nvidia")

    def test_mount_grammar_rejects_ambiguous_paths(self) -> None:
        for path in (Path("relative"), Path("/tmp/a,b"), Path("/tmp/a\nb")):
            with self.assertRaises(platform.PlatformError):
                platform._mount(path, "/run/session")

    @mock.patch.object(platform.shutil, "which", return_value="/usr/bin/tool")
    @mock.patch.object(platform.subprocess, "run")
    def test_headless_prerequisites_never_probe_docker_or_display(self, run: mock.Mock, which: mock.Mock) -> None:
        run.side_effect = [subprocess.CompletedProcess([], 0),
                           subprocess.CompletedProcess([], 0, stdout="git-lfs filter-process\n"),
                           subprocess.CompletedProcess([], 0, stdout="true\n")]
        with mock.patch.dict(os.environ, {}, clear=True):
            result = platform.prerequisite_report()
        self.assertTrue(result["ok"])
        self.assertFalse(result["graphics_required"])
        self.assertEqual(run.call_args_list[0].args[0], ["git", "lfs", "version"])
        self.assertNotIn(mock.call("docker"), which.call_args_list)

    @mock.patch.object(platform.shutil, "which", return_value=None)
    def test_missing_tools_are_structured_and_do_not_grant_authority(self, which: mock.Mock) -> None:
        result = platform.prerequisite_report(docker=True)
        self.assertFalse(result["ok"])
        self.assertIn("missing-tool:git-lfs", result["failures"])
        self.assertIn("missing-tool:docker", result["failures"])
        self.assertEqual(result["delivery_authority"], "requires-coordinator-probe")

    @mock.patch.object(platform.shutil, "which", return_value="/usr/bin/tool")
    @mock.patch.object(platform.subprocess, "run")
    def test_docker_permissions_are_not_graphics_failures(self, run: mock.Mock, which: mock.Mock) -> None:
        run.side_effect = [subprocess.CompletedProcess([], 0),
                           subprocess.CompletedProcess([], 0, stdout="git-lfs filter-process"),
                           subprocess.CompletedProcess([], 0, stdout="true"),
                           subprocess.CompletedProcess([], 1)]
        result = platform.prerequisite_report(docker=True)
        self.assertEqual(result["failures"], ["docker-daemon-access:check-daemon-and-user-permissions"])
        self.assertFalse(result["graphics_required"])

    def test_desktop_cli_emits_argument_array_and_actual_identity(self) -> None:
        with mock.patch.dict(os.environ, self.environment, clear=True), \
             mock.patch.object(platform, "desktop_options", return_value=["--env", "DISPLAY=:7"]) as compose, \
             mock.patch.object(platform.os, "geteuid", return_value=1245), \
             mock.patch.object(platform.os, "getegid", return_value=2345), \
             mock.patch("sys.stdout", new_callable=io.StringIO) as output:
            self.assertEqual(platform.main(["--desktop", "wayland", "--gpu", "nvidia"]), 0)
            document = json.loads(output.getvalue())
            self.assertEqual(document["docker_arguments"], ["--user", "1245:2345", "--env", "DISPLAY=:7"])
            self.assertFalse(document["delivery_authority"])
            self.assertFalse(document["hardware_qualified"])
            self.assertTrue(document["nvidia_container_toolkit_required"])
            self.assertEqual(compose.call_args.kwargs["uid"], 1245)

    def test_desktop_cli_failure_has_no_success_json(self) -> None:
        with mock.patch.object(platform, "desktop_options", side_effect=platform.PlatformError("missing-socket")), \
             mock.patch("sys.stdout", new_callable=io.StringIO) as output, \
             mock.patch("sys.stderr", new_callable=io.StringIO) as errors:
            self.assertEqual(platform.main(["--desktop", "x11", "--gpu", "nvidia"]), 2)
            self.assertEqual(output.getvalue(), "")
            self.assertIn("missing-socket", errors.getvalue())

    def test_desktop_cli_rejects_ambiguous_combinations(self) -> None:
        for arguments in (["--audio"], ["--desktop", "x11"],
                          ["--desktop", "x11", "--gpu", "nvidia", "--docker"],
                          ["--desktop", "x11", "--gpu", "nvidia", "--render-device", "/dev/dri/renderD128"]):
            with self.subTest(arguments=arguments), mock.patch("sys.stderr", new_callable=io.StringIO):
                with self.assertRaises(SystemExit) as raised:
                    platform.main(arguments)
                self.assertEqual(raised.exception.code, 2)

    def test_x11_preserves_local_cookie_hostname(self) -> None:
        authority = self.root / "authority"
        authority.write_bytes(b"private-cookie-fixture")
        authority.chmod(0o600)
        with mock.patch.object(platform, "_endpoint") as endpoint, \
             mock.patch.object(platform.Path, "lstat", return_value=mock.Mock(st_uid=os.geteuid())), \
             mock.patch.object(platform.socket, "gethostname", return_value="actual-desktop"):
            arguments = platform.desktop_options(display="x11", environment={"DISPLAY": ":2", "XAUTHORITY": str(authority)},
                                                 uid=os.geteuid(), gpu="nvidia")
        self.assertEqual(arguments[:2], ["--hostname", "actual-desktop"])
        self.assertIn("DISPLAY=:2", arguments)
        self.assertIn("XAUTHORITY=/run/atrinik-xauthority", arguments)
        self.assertEqual(endpoint.call_count, 2)


if __name__ == "__main__":
    unittest.main()
