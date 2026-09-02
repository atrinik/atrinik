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
        self.assertIn("container-runtime-marker", result["failed_checks"])

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
        self.assertIn("container-runtime-marker", result["failed_checks"])

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


if __name__ == "__main__":
    unittest.main()
