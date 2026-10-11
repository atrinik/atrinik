from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.workspace import Workspace


class WindowsPythonStartupTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.toolchain = self.root / "toolchain.cmake"
        self.toolchain.write_text("set(CMAKE_SYSTEM_NAME Windows)\n")
        self.runtime = self.root / "target-python-runtime"
        self.runtime.mkdir()
        self.environment = {
            "PATH": "/usr/bin:/bin", "HOME": str(self.root / "home"),
            "ATRINIK_WINDOWS_TARGET": "x86_64-w64-mingw32",
            "ATRINIK_WINDOWS_TOOLCHAIN_FILE": str(self.toolchain),
            "ATRINIK_WINDOWS_RUNTIME_DIR": str(self.runtime),
            "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR": str(self.runtime),
            "ATRINIK_WINDOWS_PYTHON_LIBRARY": str(self.toolchain),
            "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR": str(self.runtime),
        }

    def sanitized(self, environment):
        with mock.patch.dict(os.environ, environment, clear=True), \
             mock.patch("atrinik_workspace.workspace.shutil.which", return_value="/sdk/tool"):
            result = Workspace._local_windows_build_environment()
            self.assertEqual(dict(os.environ), environment)
        self.assertIsNotNone(result)
        return result

    def test_windows_startup_selectors_are_removed_without_changing_target_python(self):
        environment = {**self.environment, "PYTHONPATH": "/foreign/modules", "PYTHONHOME": "/foreign/python",
                       "PYTHONUSERBASE": "/foreign/site", "PYTHONNOUSERSITE": "0", "PYTHONSTARTUP": "/foreign/startup.py",
                       "PYTHONINSPECT": "1", "PYTHONWARNINGS": "error", "PYTHONFAKE_FUTURE_SETTING": "foreign",
                       "VIRTUAL_ENV": "/foreign/venv", "CONDA_PREFIX": "/foreign/conda", "CONDA_DEFAULT_ENV": "foreign"}
        sanitized = self.sanitized(environment)
        self.assertEqual({key: value for key, value in sanitized.items() if key.startswith("PYTHON")},
                         {"PYTHONNOUSERSITE": "1"})
        self.assertFalse({"VIRTUAL_ENV", "CONDA_PREFIX", "CONDA_DEFAULT_ENV"} & sanitized.keys())
        for key, value in self.environment.items():
            if key.startswith("ATRINIK_WINDOWS_PYTHON_"):
                self.assertEqual(sanitized[key], value)
        self.assertEqual(sanitized["HOME"], self.environment["HOME"])

    def test_foreign_host_interpreter_override_is_rejected(self):
        with self.assertRaisesRegex(WorkspaceError, "SDK host Python"):
            self.sanitized({**self.environment, "ATRINIK_WINDOWS_HOST_PYTHON": "/foreign/python3"})

    @unittest.skipUnless(Path("/usr/bin/python3").is_file(), "supported Debian SDK host Python")
    def test_fixed_host_interpreter_identity_records_actual_bytes_and_version(self):
        def fingerprint(path):
            return {"path": str(path), "resolved_path": str(path.resolve()),
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        record = Workspace._windows_host_python_identity(self.sanitized(self.environment), fingerprint)
        self.assertEqual(record["sha256"], hashlib.sha256(Path(record["resolved_path"]).read_bytes()).hexdigest())
        self.assertEqual(record["path"], "/usr/bin/python3")
        self.assertGreaterEqual(record["version"]["version"][:2], [3, 11])
        self.assertEqual(record["version"]["implementation"], "cpython")

    def test_old_or_linked_script_contract_fails_before_package_mutation(self):
        workspace = Workspace.__new__(Workspace)
        for linked in (False, True):
            staging = self.root / ("linked" if linked else "old")
            for component in ("client", "server"):
                tools = staging / component / "tools"
                tools.mkdir(parents=True)
                script = tools / "build-windows-package.sh"
                if linked:
                    external = self.root / (component + ".sh")
                    external.write_text("# atrinik-windows-host-python-contract: 1\n")
                    script.symlink_to(external)
                else:
                    script.write_text("#!/bin/bash\npython3 generator.py\n")
            with mock.patch.object(workspace, "_local_windows_build_environment", return_value=self.environment), \
                 mock.patch("atrinik_workspace.workspace.run") as execute, \
                 self.assertRaises(WorkspaceError):
                workspace._build_windows_profile_archives(staging)
            execute.assert_not_called()
            self.assertFalse((staging / "packages").exists())

    def test_admitted_script_contract_records_and_rechecks_source_bytes(self):
        staging = self.root / "current"
        for component in ("client", "server"):
            tools = staging / component / "tools"
            tools.mkdir(parents=True)
            (tools / "build-windows-package.sh").write_text("# atrinik-windows-host-python-contract: 1\n")
        first = Workspace._windows_host_python_script_proofs(staging)
        script = staging / "server/tools/build-windows-package.sh"
        self.assertEqual(first["server"]["sha256"], hashlib.sha256(script.read_bytes()).hexdigest())
        script.write_text(script.read_text() + "# changed selected source\n")
        self.assertNotEqual(first, Workspace._windows_host_python_script_proofs(staging))

    def test_script_change_between_and_after_package_components_is_refused(self):
        workspace = Workspace.__new__(Workspace)
        for phase in ("client", "server"):
            staging = self.root / ("changed-during-" + phase)
            for component in ("client", "server"):
                tools = staging / component / "tools"
                tools.mkdir(parents=True)
                (tools / "build-windows-package.sh").write_text("# atrinik-windows-host-python-contract: 1\n")
            def build(command, **kwargs):
                self.assertEqual(kwargs["env"]["ATRINIK_WINDOWS_HOST_PYTHON"], "/usr/bin/python3")
                if kwargs["cwd"].name == phase:
                    script = staging / "server/tools/build-windows-package.sh"
                    script.write_text(script.read_text() + "# replacement during package component\n")
            # Stub the unrelated SDK inventory boundary; exercise real script
            # hash observations and execution seams without native packages.
            with self.subTest(phase=phase), \
                 mock.patch.object(workspace, "_local_windows_build_environment", return_value=self.environment), \
                 mock.patch.object(workspace, "_local_windows_producer_identity", return_value={"tools": {"bash": {"path": "/bin/bash"}}}), \
                 mock.patch("atrinik_workspace.workspace.run", side_effect=build) as execute, \
                 self.assertRaisesRegex(WorkspaceError, "inputs changed (before|during) packaging"):
                workspace._build_windows_profile_archives(staging)
            self.assertEqual(execute.call_count, 1 if phase == "client" else 2)
            self.assertEqual(execute.call_args.kwargs["cwd"].name, phase)

    @unittest.skipUnless(Path("/usr/bin/python3").is_file(), "supported Debian SDK host Python")
    def test_host_interpreter_fingerprint_drift_during_real_probe_is_rejected(self):
        from atrinik_workspace import workspace as module
        fixture = self.root / "observed-interpreter-bytes"
        fixture.write_bytes(Path("/usr/bin/python3").read_bytes())
        def fingerprint(path):
            # Simulate changed executable storage using a task-owned copy;
            # both observations hash actual bytes, never guessed hashes.
            return {"path": str(path), "sha256": hashlib.sha256(fixture.read_bytes()).hexdigest()}
        actual_run = module.run
        def probe(command, **kwargs):
            result = actual_run(command, **kwargs)
            fixture.write_bytes(fixture.read_bytes() + b"replacement during probe")
            return result
        with mock.patch.object(module, "run", side_effect=probe), \
             self.assertRaisesRegex(WorkspaceError, "host Python changed during inspection"):
            Workspace._windows_host_python_identity(self.sanitized(self.environment), fingerprint)

    @unittest.skipUnless(Path("/usr/bin/python3").is_file(), "supported Debian SDK host Python")
    def test_real_host_python_refuses_caller_module_and_home_startup_callbacks(self):
        version = subprocess.check_output(["/usr/bin/python3", "-I", "-c",
                    "import sys; print(str(sys.version_info.major)+'.'+str(sys.version_info.minor))"], text=True).strip()
        modules = self.root / "foreign-modules"
        modules.mkdir()
        user_site = Path(self.environment["HOME"]) / ".local/lib" / ("python" + version) / "site-packages"
        user_site.mkdir(parents=True)
        callbacks = [(modules / "sitecustomize.py", "module"), (user_site / "usercustomize.py", "user"),
                     (user_site / "callback.pth", "pth")]
        for path, name in callbacks:
            path.write_text("import pathlib; pathlib.Path(" + repr(str(self.root / name)) + ").write_text('executed')\n")
        environment = {**self.environment, "PYTHONPATH": str(modules)}
        subprocess.run(["/usr/bin/python3", "-c", "pass"], env=environment, check=True, timeout=10)
        for _, name in callbacks:
            self.assertTrue((self.root / name).is_file(), name)
            (self.root / name).unlink()
        environment.update(PYTHONHOME=str(self.root / "invalid-python-home"), PYTHONNOUSERSITE="0",
                           PYTHONUSERBASE=str(self.root / "foreign-user-base"))
        sanitized = self.sanitized(environment)
        generator = self.root / "generator.py"
        output = self.root / "generated.json"
        generator.write_text("import json, pathlib, sys\npathlib.Path(sys.argv[1]).write_text(json.dumps({'ok': 7 * 6}))\n")
        subprocess.run(["/usr/bin/python3", str(generator), str(output)], env=sanitized, check=True, timeout=10)
        self.assertEqual(json.loads(output.read_text()), {"ok": 42})
        for _, name in callbacks:
            self.assertFalse((self.root / name).exists(), name)


if __name__ == "__main__":
    unittest.main()
