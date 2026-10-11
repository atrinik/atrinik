from __future__ import annotations

from contextlib import ExitStack
import os
from pathlib import Path
import subprocess
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest import mock

from atrinik_workspace import linux_portable
from atrinik_workspace.workspace import Workspace


class StartupContext:
    host_tools = {"fixture": "host-tools interface only"}

    def verify_host_tools(self):
        pass

    def validate_source_binding(self, binary, source):
        pass

    def capture_configure(self, binary, source):
        pass

    def capture_output(self, binary):
        pass


@unittest.skipUnless(Path("/usr/bin/python3").is_file(), "requires producer-style Python")
class PortablePythonStartupTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.home = self.root / "home"
        self.home.mkdir()
        self.environment = {"HOME": str(self.home), "PATH": os.defpath,
                            "PYTHONNOUSERSITE": "", "PYTHONPATH": "/caller/modules"}
        site_path = self.python("import site; print(site.getusersitepackages())", no_site=True)
        user_site = Path(site_path.stdout.strip())
        self.assertTrue(user_site.is_relative_to(self.home))
        user_site.mkdir(parents=True)
        self.pth_marker = self.root / "pth-executed"
        self.customize_marker = self.root / "usercustomize-executed"
        (user_site / "hostile.pth").write_text(
            f"import pathlib; pathlib.Path({str(self.pth_marker)!r}).write_text('executed')\n"
        )
        (user_site / "usercustomize.py").write_text(
            f"from pathlib import Path\nPath({str(self.customize_marker)!r}).write_text('executed')\n"
        )
        self.workspace = object.__new__(Workspace)
        self.workspace.paths = SimpleNamespace(builds=self.root / "builds")
        self.workspace._build_state = threading.local()
        self.workspace._use_ccache = False
        self.binary = self.workspace.paths.builds / "profiles/test/build/client"

    def python(self, script, *, environment=None, no_site=False):
        # Match the admitted shader Python command's lack of -I/-s. Only the
        # fixture's path-discovery probe uses -S, before the hooks are created.
        command = ["/usr/bin/python3", *(["-S"] if no_site else []), "-c", script]
        return subprocess.run(command, cwd=self.source,
                              env=self.environment if environment is None else environment,
                              check=True, text=True, capture_output=True, timeout=10)

    def cmake(self, portable, probe):
        fingerprint = {"build_tree_identity": {"toolchain_file": None},
                       "source": {"configure_skip_safe": True}}
        with ExitStack() as stack:
            stack.enter_context(mock.patch.dict(os.environ, self.environment, clear=True))
            stack.enter_context(mock.patch.object(linux_portable, "PortableBuildContext", StartupContext))
            stack.enter_context(mock.patch.object(self.workspace, "_add_debug_prefix_environment", return_value={}))
            stack.enter_context(mock.patch.object(self.workspace, "_configure_fingerprint", return_value=fingerprint))
            stack.enter_context(mock.patch.object(self.workspace, "_cmake_state_valid", return_value=True))
            stack.enter_context(mock.patch.object(linux_portable, "installed_metadata", return_value=(
                {}, {"compiler-evidence.json": {}})))
            stack.enter_context(mock.patch.object(linux_portable, "_verify_compiler_bytes"))
            stack.enter_context(mock.patch("atrinik_workspace.workspace.run", side_effect=probe))
            self.workspace._cmake(self.source, self.binary, [], False, portable=portable,
                                  portable_context=StartupContext() if portable else None)

    def test_portable_configure_and_build_block_hostile_user_startup_hooks(self):
        # Prove the fixture is executable under the same interpreter first.
        self.python("import site; print(site.ENABLE_USER_SITE)")
        self.assertTrue(self.pth_marker.is_file())
        self.assertTrue(self.customize_marker.is_file())
        self.pth_marker.unlink()
        self.customize_marker.unlink()
        phases = []

        def probe(command, **kwargs):
            phases.append("configure" if "-S" in command else "build")
            environment = kwargs["env"]
            self.assertEqual(environment["HOME"], str(self.home))
            self.assertEqual(environment["PYTHONNOUSERSITE"], "1")
            self.assertNotIn("PYTHONPATH", environment)
            result = self.python("import site; print(site.ENABLE_USER_SITE)", environment=environment)
            self.assertEqual(result.stdout.strip(), "False")
            self.assertFalse(self.pth_marker.exists())
            self.assertFalse(self.customize_marker.exists())

        self.cmake(True, probe)
        self.assertEqual(phases, ["configure", "build"])

    def test_ordinary_build_preserves_caller_python_startup_policy(self):
        phases = []

        def probe(command, **kwargs):
            phases.append("configure" if "-S" in command else "build")
            environment = kwargs["env"]
            self.assertEqual(environment["PYTHONNOUSERSITE"], "")
            self.assertEqual(environment["PYTHONPATH"], "/caller/modules")
            result = self.python("import site; print(site.ENABLE_USER_SITE)", environment=environment)
            self.assertEqual(result.stdout.strip(), "True")
            self.assertTrue(self.pth_marker.is_file())
            self.assertTrue(self.customize_marker.is_file())

        self.cmake(False, probe)
        self.assertEqual(phases, ["configure", "build"])
