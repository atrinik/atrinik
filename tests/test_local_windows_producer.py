from __future__ import annotations

import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.workspace import Workspace, WINDOWS_PACKAGE_VERSION


class LocalWindowsProducerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.llvm = self.root / "llvm"
        self.prefix = self.root / "windows"
        self.resource = self.llvm / "lib/clang/23"
        self.builtins = self.resource / "lib/windows/libclang_rt.builtins-x86_64.a"
        self.builtins.parent.mkdir(parents=True)
        self.builtins.write_bytes(b"test builtins")
        (self.llvm / "bin").mkdir()
        script = ("#!/bin/sh\ncase \"$*\" in\n"
                  "*--version*) echo 'clang version 23.1.3' ;;\n"
                  "*-dumpmachine*) echo x86_64-w64-windows-gnu ;;\n"
                  "*-print-resource-dir*) echo " + str(self.resource) + " ;;\n"
                  "*-print-libgcc-file-name*) echo " + str(self.builtins) + " ;;\n"
                  "*) exit 1 ;;\nesac\n")
        for name in ("clang", "x86_64-w64-mingw32-clang", "x86_64-w64-mingw32-clang++",
                     "x86_64-w64-mingw32-windres", "ld.lld", "llvm-ar", "llvm-ranlib",
                     "llvm-nm", "llvm-objcopy", "llvm-objdump", "llvm-readelf", "llvm-strip", "llvm-dlltool"):
            path = self.llvm / "bin" / name
            path.write_text(script)
            path.chmod(0o755)
        self.runtime = self.prefix / "bin"
        self.runtime.mkdir(parents=True)
        for name in ("libc++.dll", "libunwind.dll", "libwinpthread-1.dll", "x86_64-w64-mingw32-pkg-config"):
            (self.runtime / name).write_bytes(b"test producer input")
        self.toolchain = self.root / "toolchain.cmake"
        recipe = (Path(__file__).parent / "fixtures/windows_llvm/toolchain.cmake").read_text()
        recipe = "\n".join(line for line in recipe.splitlines() if "COMPILER_LAUNCHER" not in line) + "\n"
        self.toolchain.write_text(recipe.replace("/opt/llvm-mingw", str(self.llvm))
                                 .replace("/opt/atrinik-windows", str(self.prefix)))
        self.environment = {"ATRINIK_WINDOWS_TARGET": "x86_64-w64-mingw32",
                            "ATRINIK_WINDOWS_TOOLCHAIN_FILE": str(self.toolchain),
                            "ATRINIK_WINDOWS_RUNTIME_DIR": str(self.runtime),
                            "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR": str(self.runtime),
                            "ATRINIK_WINDOWS_PYTHON_LIBRARY": str(self.builtins),
                            "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR": str(self.runtime)}

    def identity(self):
        return Workspace._local_windows_producer_identity(self.environment)

    def test_actual_local_files_are_rehashed_for_each_admission(self):
        first = self.identity()
        self.assertEqual(first["target"], "x86_64-w64-mingw32")
        for path in (self.llvm / "bin/clang", self.builtins, self.runtime / "libunwind.dll", self.toolchain):
            original = path.read_bytes()
            path.write_bytes(original + b"\n# replacement\n")
            self.assertNotEqual(first, self.identity(), str(path))
            path.write_bytes(original)
        self.assertEqual(first, self.identity())

    def test_windows_environment_clears_driver_dependency_and_shell_overrides(self):
        hostile = dict(self.environment, CFLAGS="--config=/tmp/foreign.cfg", CCC_OVERRIDE_OPTIONS="+-O3",
                       CMAKE_PREFIX_PATH="/tmp/foreign", PKG_CONFIG="/tmp/pkg-config",
                       BASH_ENV="/tmp/startup", ENV="/tmp/startup", CCACHE_PREFIX="/tmp/launcher")
        hostile["BASH_FUNC_cmake%%"] = "() { false; }"
        with mock.patch.dict(os.environ, hostile, clear=True), \
             mock.patch("atrinik_workspace.workspace.shutil.which", return_value="/fixture/tool"):
            environment = Workspace._local_windows_build_environment()
        for name in ("CCC_OVERRIDE_OPTIONS", "CMAKE_PREFIX_PATH", "PKG_CONFIG", "BASH_ENV", "ENV",
                     "BASH_FUNC_cmake%%", "CCACHE_PREFIX"):
            self.assertNotIn(name, environment)
        self.assertEqual(environment["CFLAGS"], "--no-default-config")
        self.assertEqual(environment["CXXFLAGS"], "--no-default-config")
        self.assertEqual(environment["LDFLAGS"], "--no-default-config")
        self.assertEqual(environment["CCACHE_DISABLE"], "1")

    def test_selected_cache_launcher_bytes_are_fresh_inputs(self):
        self.toolchain.write_text(self.toolchain.read_text() + "set(CMAKE_C_COMPILER_LAUNCHER ccache)\n")
        launcher = self.llvm / "bin/ccache"
        launcher.write_text("#!/bin/sh\nexec \"$@\"\n")
        launcher.chmod(0o755)
        self.environment["PATH"] = str(launcher.parent) + os.pathsep + os.defpath
        first = self.identity()
        launcher.write_text(launcher.read_text() + "# replaced\n")
        self.assertNotEqual(first, self.identity())

    def test_rejects_foreign_toolchain_compiler_target_and_incomplete_runtime(self):
        original = self.toolchain.read_text()
        for recipe in (original + 'include("/tmp/foreign.cmake")\n',
                       original.replace('bin/${ATRINIK_WINDOWS_TARGET}-clang"', 'bin/gcc"')):
            self.toolchain.write_text(recipe)
            with self.assertRaises(WorkspaceError):
                self.identity()
        self.toolchain.write_text(original)
        compiler = self.llvm / "bin/x86_64-w64-mingw32-clang"
        original_compiler = compiler.read_text()
        for text in (original_compiler.replace("23.1.3", "22.1.0"),
                     original_compiler.replace("x86_64-w64-windows-gnu", "aarch64-linux-gnu")):
            compiler.write_text(text)
            with self.assertRaises(WorkspaceError):
                self.identity()
        compiler.write_text(original_compiler)
        (self.runtime / "libunwind.dll").unlink()
        with self.assertRaises(WorkspaceError):
            self.identity()

    def test_packaging_records_identity_and_refuses_mid_build_replacement(self):
        workspace = Workspace.__new__(Workspace)
        for change in (False, True):
            with self.subTest(change=change):
                staging = self.root / str(change)
                staging.mkdir()
                for component in ("client", "server"):
                    (staging / component).mkdir()
                original = self.runtime.joinpath("libunwind.dll").read_bytes()
                from atrinik_workspace import workspace as module
                actual_run = module.run

                def build(command, **kwargs):
                    if command[0] != "bash":
                        return actual_run(command, **kwargs)
                    component = kwargs["cwd"].name
                    name = f"atrinik-classic-{component}-{WINDOWS_PACKAGE_VERSION}-windows-x86_64.zip"
                    (staging / "packages" / name).write_bytes(b"archive fixture")
                    if change:
                        self.runtime.joinpath("libunwind.dll").write_bytes(b"replaced during build")

                with mock.patch.object(workspace, "_local_windows_build_environment", return_value=dict(self.environment)), \
                     mock.patch.object(module, "run", side_effect=build):
                    if change:
                        with self.assertRaisesRegex(WorkspaceError, "inputs changed"):
                            workspace._build_windows_profile_archives(staging)
                    else:
                        _, _, build_record = workspace._build_windows_profile_archives(staging)
                        self.assertEqual(build_record["producer"], self.identity())
                        json.dumps(build_record)
                self.runtime.joinpath("libunwind.dll").write_bytes(original)
