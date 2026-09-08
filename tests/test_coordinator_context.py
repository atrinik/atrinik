from __future__ import annotations

from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/atrinik_coordinator_context.py"
SPEC = importlib.util.spec_from_file_location("atrinik_coordinator_context", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
context = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = context
SPEC.loader.exec_module(context)


class CoordinatorContextTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        temporary_root = Path(self.temporary.name)
        self.workspace = temporary_root / "workspace"
        self.repository = self.workspace / "worktree"
        self.codex_home = temporary_root / "codex"
        self.runtime = temporary_root / "runtime"
        self.mountinfo = temporary_root / "mountinfo"
        (self.repository / ".devcontainer/windows-cross").mkdir(parents=True)
        (self.workspace / "workspace").mkdir(parents=True)
        (self.workspace / "build/reviews").mkdir(parents=True)
        self.codex_home.mkdir()
        self.runtime.mkdir()
        self.codex_home.chmod(0o700)
        (self.runtime / ".dockerenv").write_text("", encoding="utf-8")
        (self.repository / ".git").write_text(
            "gitdir: /private/worktree/admin\n", encoding="utf-8"
        )
        (self.repository / "components.json").write_text(
            '{"schema_version": 3}\n', encoding="utf-8"
        )
        canonical = {
            "image": context.CANONICAL_IMAGE,
            "remoteUser": "ubuntu",
            "postCreateCommand": "./atrinik init",
            "updateRemoteUserUID": True,
            "workspaceFolder": "/workspaces/atrinik",
            "mounts": [
                "source=${localEnv:HOME}/.codex-atrinik,"
                "target=/home/ubuntu/.codex,type=bind"
            ],
            "containerEnv": {"CODEX_HOME": "/home/ubuntu/.codex"},
        }
        windows_cross = {
            "image": context.WINDOWS_CROSS_IMAGE,
            "remoteUser": "vscode",
            "remoteEnv": {"PATH": "/opt/mxe/usr/bin:${containerEnv:PATH}"},
        }
        (self.repository / ".devcontainer/devcontainer.json").write_text(
            "// canonical fixture\n" + json.dumps(canonical), encoding="utf-8"
        )
        (self.repository / ".devcontainer/windows-cross/devcontainer.json").write_text(
            json.dumps(windows_cross), encoding="utf-8"
        )
        self._write_mountinfo()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _write_mountinfo(self, filesystem: str = "ext4") -> None:
        device = self.workspace.stat().st_dev
        device_name = f"{os.major(device)}:{os.minor(device)}"

        def encode(path: Path) -> str:
            return str(path).replace("\\", "\\134").replace(" ", "\\040")

        self.mountinfo.write_text(
            "\n".join(
                (
                    f"100 1 {device_name} / {encode(self.workspace)} rw - "
                    f"{filesystem} /dev/test rw",
                    f"101 1 {device_name} / {encode(self.codex_home)} rw - "
                    "ext4 /dev/test rw",
                )
            )
            + "\n",
            encoding="utf-8",
        )

    def _probe(self, **overrides: object) -> dict[str, object]:
        values: dict[str, object] = {
            "system": "Linux",
            "environment": {
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/local/bin:/usr/bin",
                "REMOTE_CONTAINERS": "true",
            },
            "user_name": "ubuntu",
            "cwd": self.repository,
            "workspace_folder": self.workspace,
            "codex_home": self.codex_home,
            "runtime_root": self.runtime,
            "mountinfo": self.mountinfo,
            "effective_uid": os.geteuid(),
        }
        values.update(overrides)
        return context.probe(self.repository, **values)

    def test_canonical_linux_requires_the_complete_live_contract(self) -> None:
        before = {
            path: path.read_bytes()
            for path in (
                self.repository / "components.json",
                self.repository / ".devcontainer/devcontainer.json",
                self.mountinfo,
            )
        }

        result = self._probe()

        self.assertEqual(result["status"], "canonical-linux")
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_VSCODE)
        self.assertEqual(result["failed_checks"], [])
        self.assertEqual(
            {path: path.read_bytes() for path in before}, before
        )

    def test_environment_markers_without_container_and_mount_proof_fail_closed(
        self,
    ) -> None:
        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/bin",
                "REMOTE_CONTAINERS": "true",
            },
            runtime_root=Path(self.temporary.name) / "no-marker",
        )

        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertFalse(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_NATIVE_HOST)
        self.assertIn("native-host-proof-unavailable-or-unsafe", result["failed_checks"])

    def test_canonical_container_launched_without_vscode_can_be_authoritative(
        self,
    ) -> None:
        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/local/bin:/usr/bin",
                "DEVCONTAINER": "true",
            }
        )

        self.assertEqual(result["status"], "canonical-linux")
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_CONTAINER)

    def test_vscode_plugin_signal_can_be_term_program_only(self) -> None:
        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/local/bin:/usr/bin",
                "TERM_PROGRAM": "vscode",
            }
        )

        self.assertEqual(result["status"], "canonical-linux")
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_VSCODE)

    def test_direct_container_entry_needs_no_launcher_environment_signal(self) -> None:
        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/local/bin:/usr/bin",
            }
        )

        self.assertEqual(result["status"], "canonical-linux")
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_CONTAINER)

    def test_stale_session_signal_does_not_authorize_native_host(self) -> None:
        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/bin",
                "DEVCONTAINER": "true",
                "ATRINIK_COORDINATOR_SESSION": "stale-or-copied",
            },
            runtime_root=Path(self.temporary.name) / "no-marker",
        )

        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertFalse(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_NATIVE_HOST)
        self.assertIn("native-host-proof-unavailable-or-unsafe", result["failed_checks"])

    def test_nested_coordinator_signal_fails_closed(self) -> None:
        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/bin",
                "DEVCONTAINER": "true",
                "ATRINIK_COORDINATOR_DEPTH": "1",
            }
        )

        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertFalse(result["authoritative"])
        self.assertIn("nested-coordinator", result["failed_checks"])

    def test_arbitrary_container_layout_is_not_authoritative(self) -> None:
        arbitrary_workspace = Path(self.temporary.name) / "arbitrary-workspace"
        (arbitrary_workspace / "workspace").mkdir(parents=True)

        result = self._probe(
            environment={
                "HOME": "/home/ubuntu",
                "CODEX_HOME": "/home/ubuntu/.codex",
                "PATH": "/usr/bin",
                "DEVCONTAINER": "true",
            },
            workspace_folder=arbitrary_workspace,
        )

        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertFalse(result["authoritative"])
        self.assertTrue(
            {
                "canonical-workspace-folder",
                "current-directory-outside-workspace",
            }
            & set(result["failed_checks"])
        )

    def test_missing_kernel_user_identity_fails_closed(self) -> None:
        with mock.patch.object(context, "_current_user", return_value=None):
            result = self._probe(user_name=None)

        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertFalse(result["authoritative"])
        self.assertIn("runtime-user", result["failed_checks"])

    def test_stale_runtime_image_requires_owned_rebuild(self) -> None:
        result = self._probe(environment={
            "HOME": "/home/ubuntu",
            "CODEX_HOME": "/home/ubuntu/.codex",
            "DEVCONTAINER_IMAGE": "ghcr.io/atrinik/linux-build:old",
        })
        self.assertFalse(result["authoritative"])
        self.assertIn("runtime-image-mismatch", result["failed_checks"])
        self.assertIn(context.CANONICAL_IMAGE, result["next_action"])
        self.assertIn("owned session", result["next_action"])
        self.assertIn("preserve worktrees and state", result["next_action"])

    def test_pinned_config_mismatch_fails_closed(self) -> None:
        config_path = self.repository / ".devcontainer/devcontainer.json"
        config = json.loads(
            config_path.read_text(encoding="utf-8").split("\n", 1)[1]
        )
        config["image"] = context.WINDOWS_CROSS_IMAGE
        config_path.write_text(json.dumps(config), encoding="utf-8")

        result = self._probe()

        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertIn("canonical-image-pin", result["failed_checks"])

    def test_unsafe_ledger_mode_and_windows_mount_are_not_authoritative(self) -> None:
        (self.workspace / "build/reviews").chmod(0o777)
        result = self._probe()
        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertIn("unsafe-ledger-root-mode", result["failed_checks"])

        (self.workspace / "build/reviews").chmod(0o755)
        self._write_mountinfo(filesystem="virtiofs")
        result = self._probe()
        self.assertEqual(result["status"], "unknown-or-unsafe")
        self.assertIn("unsafe-workspace-folder-mount", result["failed_checks"])

    def test_native_windows_is_a_stable_host_boundary(self) -> None:
        result = context.probe(
            Path("C:/workspaces/atrinik"), system="Windows", environment={}
        )
        self.assertEqual(result["status"], "native-windows")
        self.assertFalse(result["authoritative"])
        self.assertEqual(result["entry_mode"], context.ENTRY_MODE_NATIVE_HOST)
        self.assertIn("posix-ledger-primitives", result["failed_checks"])

    def test_windows_cross_role_is_not_a_delivery_coordinator(self) -> None:
        result = self._probe(
            user_name="vscode",
            environment={
                "HOME": "/home/vscode",
                "CODEX_HOME": "/home/vscode/.codex",
                "PATH": f"{context.MXE_PATH}:/usr/bin",
            },
        )
        self.assertEqual(result["status"], "windows-cross")
        self.assertFalse(result["authoritative"])
        self.assertIn("mxe-toolchain-present", result["failed_checks"])

    def test_cli_json_and_human_output_are_bounded_and_stable(self) -> None:
        expected = context._result(
            "unknown-or-unsafe",
            False,
            ["container-runtime-marker"],
            "attach",
            "unknown-or-unsupported-context",
        )
        with mock.patch.object(context, "probe", return_value=expected):
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                self.assertEqual(context.main(["--json"]), 2)
            self.assertEqual(json.loads(stdout.getvalue()), expected)

            stdout = io.StringIO()
            with redirect_stdout(stdout):
                self.assertEqual(context.main([]), 2)
        self.assertIn("unknown-or-unsafe", stdout.getvalue())
        self.assertIn("authoritative=false", stdout.getvalue())
        self.assertIn("entry mode: unknown", stdout.getvalue())

    def test_probe_source_does_not_import_posix_locking(self) -> None:
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertNotIn("import fcntl", source)
        self.assertNotIn("delivery_ledger", source)


@unittest.skipUnless(sys.platform.startswith("linux"), "native Linux proof")
class NativeCoordinatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repository = self.root / "repository"
        self.home = self.root / "home"
        self.codex = self.home / ".codex"
        self.proc = self.root / "proc"
        for path in (self.repository, self.codex, self.proc):
            path.mkdir(parents=True)
        self.codex.chmod(0o700)
        (self.repository / ".devcontainer/windows-cross").mkdir(parents=True)
        (self.repository / ".git").mkdir()
        for relative in ("components.json", ".devcontainer/devcontainer.json",
                         ".devcontainer/windows-cross/devcontainer.json"):
            (self.repository / relative).write_bytes((ROOT / relative).read_bytes())
        self.executable = self.root / "systemd"
        self.executable.write_text("fixture")
        self.executable.chmod(0o755)
        self.uid = os.geteuid()
        self.environment = {"HOME": str(self.home), "CODEX_HOME": str(self.codex)}
        device = self.root.stat().st_dev
        device_name = f"{os.major(device)}:{os.minor(device)}"
        self.inputs = {
            **{str(self.repository / relative): "{}" for relative in (
                "components.json", ".devcontainer/devcontainer.json",
                ".devcontainer/windows-cross/devcontainer.json")},
            str(self.proc / str(os.getpid()) / "mountinfo"):
                f"1 0 {device_name} / / rw - ext4 /dev/test rw\n"
                f"2 1 {device_name} / {self.proc} rw - proc proc rw\n",
            str(self.proc / "1/status"): "Name:\tsystemd\nUid:\t0 0 0 0\n",
            str(self.proc / "sys/kernel/osrelease"): "7.0.0-generic\n",
            str(self.root / "usr/lib/os-release"): 'ID=ubuntu\nVERSION_ID="26.04"\n',
        }
        for process in (self.proc / str(os.getpid()), self.proc / "1"):
            for field in ("uid_map", "gid_map"):
                self.inputs[str(process / field)] = "         0          0 4294967295\n"

    def read(self, path: Path, uid: int, limit: int = 16384) -> str:
        return self.inputs[str(path)]

    def open(self, path: Path, uid: int, *, directory: bool = False) -> int:
        # Model kernel/OS facts at the I/O boundary. The real descriptor opener
        # has separate rejection tests; these are not native runtime evidence.
        if directory:
            return os.open(path if path.is_dir() else self.root, os.O_RDONLY | os.O_DIRECTORY)
        return os.open(self.executable, os.O_RDONLY)

    def probe(self) -> dict[str, object]:
        import pwd
        from types import SimpleNamespace
        with mock.patch.object(context, "_native_read", side_effect=self.read), \
             mock.patch.object(context, "_native_open", side_effect=self.open), \
             mock.patch.object(pwd, "getpwuid", return_value=SimpleNamespace(pw_dir=str(self.home))):
            return context.probe(self.repository, system="Linux", environment=self.environment,
                                 user_name="vscode", effective_uid=self.uid,
                                 runtime_root=self.root, cwd=self.repository)

    def test_supported_nonroot_host_does_not_require_pid1_exe_or_namespace_access(self) -> None:
        with mock.patch.object(os, "readlink", side_effect=PermissionError):
            result = self.probe()
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["status"], "native-linux")

    def test_user_namespace_maps_require_one_full_initial_identity_row(self) -> None:
        key = str(self.proc / str(os.getpid()) / "uid_map")
        for value in ("0 1000 1\n", "0 0 4294967295\n1 1 1\n", "", "0 0 invalid"):
            with self.subTest(value=value):
                self.inputs[key] = value
                result = self.probe()
                self.assertFalse(result["authoritative"])
                self.assertIn("native-initial-user-namespace", result["failed_checks"])

    def test_pid1_user_namespace_is_checked_separately(self) -> None:
        self.inputs[str(self.proc / "1/gid_map")] = "0 1000 1"
        self.assertFalse(self.probe()["authoritative"])

    def test_wsl_and_non_systemd_processes_are_not_native_authority(self) -> None:
        self.inputs[str(self.proc / "sys/kernel/osrelease")] = "microsoft-standard-WSL2"
        self.assertIn("native-wsl-host-boundary", self.probe()["failed_checks"])
        self.inputs[str(self.proc / "1/status")] = "Name: systemd\nUid: 1000 1000 1000 1000"
        self.assertIn("native-systemd-init", self.probe()["failed_checks"])

    def test_unsupported_distribution_and_container_environment_fail(self) -> None:
        self.inputs[str(self.root / "usr/lib/os-release")] = "ID=unknown\nVERSION_ID=1"
        self.assertIn("native-distribution-unsupported", self.probe()["failed_checks"])
        self.environment["DEVCONTAINER"] = "true"
        self.assertIn("native-container-environment-mismatch", self.probe()["failed_checks"])

    def test_caller_mountinfo_must_prove_procfs_and_live_device(self) -> None:
        key = str(self.proc / str(os.getpid()) / "mountinfo")
        self.inputs[key] = self.inputs[key].replace("proc proc", "ext4 proc")
        self.assertIn("native-procfs", self.probe()["failed_checks"])
        self.inputs[key] = self.inputs[key].replace(
            f"{os.major(self.root.stat().st_dev)}:{os.minor(self.root.stat().st_dev)}", "123:456")
        self.assertFalse(self.probe()["authoritative"])

    def test_caller_home_and_private_codex_are_required(self) -> None:
        self.environment["HOME"] = "/forged/home"
        self.assertIn("native-passwd-home", self.probe()["failed_checks"])
        self.environment["HOME"] = str(self.home)
        self.codex.chmod(0o755)
        self.assertIn("unsafe-codex-home-mode", self.probe()["failed_checks"])

    def test_descriptor_opener_checks_real_safe_paths_and_symlinks(self) -> None:
        build = ROOT / "build"
        build.mkdir(mode=0o700, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=build) as temporary:
            safe = Path(temporary)
            source = safe / "input"
            source.write_text("real bytes")
            source.chmod(0o600)
            self.assertEqual(context._native_read(source, self.uid), "real bytes")
            with self.assertRaisesRegex(context.ProbeError, "oversized"):
                context._native_read(source, self.uid, limit=3)
            link = safe / "link"
            link.symlink_to(source)
            with self.assertRaises(OSError):
                context._native_open(link, self.uid)
            source.chmod(0o666)
            with self.assertRaisesRegex(context.ProbeError, "owner-or-mode"):
                context._native_open(source, self.uid)
            source.chmod(0o600)
            with self.assertRaisesRegex(context.ProbeError, "owner-or-mode"):
                context._native_open(source, self.uid + 1)

    def test_actual_read_detects_in_place_mutation(self) -> None:
        build = ROOT / "build"
        build.mkdir(mode=0o700, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=build) as temporary:
            source = Path(temporary) / "input"
            source.write_text("before")
            original_fdopen = os.fdopen

            class RacingReader:
                def __init__(self, fd: int, mode: str) -> None:
                    self.stream = original_fdopen(fd, mode)

                def __enter__(self) -> object:
                    return self

                def __exit__(self, *args: object) -> None:
                    self.stream.close()

                def fileno(self) -> int:
                    return self.stream.fileno()

                def read(self, limit: int) -> bytes:
                    data = self.stream.read(limit)
                    source.write_text("changed while reading")
                    return data

            with mock.patch.object(context.os, "fdopen", side_effect=RacingReader):
                with self.assertRaisesRegex(context.ProbeError, "native-input-changed"):
                    context._native_read(source, self.uid)

    def test_malformed_os_release_quoting_and_duplicate_keys_fail(self) -> None:
        key = str(self.root / "usr/lib/os-release")
        for value in ('ID=ubuntu\nVERSION_ID="26.04', 'ID=ubuntu\nVERSION_ID=""26.04""',
                      "ID=ubuntu\nID=debian\nVERSION_ID=26.04"):
            with self.subTest(value=value):
                self.inputs[key] = value
                self.assertFalse(self.probe()["authoritative"])

    def test_public_probe_rejects_forged_configuration_before_native_facts(self) -> None:
        config = self.repository / ".devcontainer/devcontainer.json"
        value = context.json.loads(config.read_text())
        value["image"] = "forged"
        config.write_text(context.json.dumps(value))
        result = self.probe()
        self.assertFalse(result["authoritative"])
        self.assertIn("canonical-image-pin", result["failed_checks"])


class ProjectContextConsumerTests(unittest.TestCase):
    def test_accepts_only_actual_boolean_authority_and_supported_status(self) -> None:
        from atrinik_workspace import project_delivery
        from atrinik_workspace.project_coordinator import ProjectError
        import subprocess
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "components.json").write_text("{}")
            for status in ("canonical-linux", "native-linux"):
                result = {"authoritative": True, "status": status}
                with mock.patch.object(project_delivery.subprocess, "run", side_effect=[
                    subprocess.CompletedProcess([], 0, stdout=json.dumps(result).encode()),
                    subprocess.CompletedProcess([], 0, stdout=str(root / ".git") + "\n")]):
                    self.assertEqual(project_delivery.context(), root)
            invalid = [{}, [], {"authoritative": "true", "status": "native-linux"},
                       {"authoritative": False, "status": "native-linux"},
                       {"authoritative": 1, "status": "canonical-linux"},
                       {"authoritative": True, "status": "native-windows"},
                       {"authoritative": True, "status": "windows-cross"},
                       {"authoritative": True, "status": "unknown-or-unsafe"},
                       {"authoritative": True, "status": []}]
            for result in invalid:
                with self.subTest(result=result), mock.patch.object(
                    project_delivery.subprocess, "run",
                    return_value=subprocess.CompletedProcess([], 0, stdout=json.dumps(result).encode())
                ) as run:
                    with self.assertRaises(ProjectError):
                        project_delivery.context()
                    self.assertEqual(run.call_count, 1)

    def test_nonzero_and_malformed_probe_stop_before_git_or_state(self) -> None:
        from atrinik_workspace import project_delivery
        from atrinik_workspace.project_coordinator import ProjectError
        import subprocess
        for code, output in ((2, b"{}"), (0, b"not json")):
            with self.subTest(code=code), mock.patch.object(project_delivery.subprocess, "run",
                    return_value=subprocess.CompletedProcess([], code, stdout=output)) as run:
                with self.assertRaises((ProjectError, ValueError)):
                    project_delivery.context()
                self.assertEqual(run.call_count, 1)


if __name__ == "__main__":
    unittest.main()
