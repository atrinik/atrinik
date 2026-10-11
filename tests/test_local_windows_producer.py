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
        (self.llvm / "bin/llvm-dlltool").unlink()
        (self.llvm / "bin/llvm-dlltool").symlink_to("llvm-ar")
        for path in (self.llvm / "bin").iterdir():
            if path.name not in {"clang", "x86_64-w64-mingw32-clang", "x86_64-w64-mingw32-clang++", "llvm-dlltool"}:
                family = "LLD" if path.name == "ld.lld" else "LLVM version"
                path.write_text(path.read_text().replace("clang version 23.1.3", family + " 23.1.3"))
        self.sysroot = self.llvm / "x86_64-w64-mingw32"
        for root in (self.prefix, self.llvm, self.sysroot):
            for name in ("include", "lib"):
                (root / name).mkdir(parents=True, exist_ok=True)
        self.header = self.prefix / "include/dependency.h"
        self.header.write_text("#define DEPENDENCY 1\n")
        self.pc = self.prefix / "lib/dependency.pc"
        self.pc.write_text("Name: dependency\n")
        self.crt = self.sysroot / "lib/crt2.o"
        self.crt.write_bytes(b"CRT")
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
        for path in (self.llvm / "bin/clang", self.builtins, self.runtime / "libunwind.dll", self.toolchain,
                     self.header, self.pc, self.crt):
            original = path.read_bytes()
            path.write_bytes(original + b"\n# replacement\n")
            self.assertNotEqual(first, self.identity(), str(path))
            path.write_bytes(original)
        self.assertEqual(first, self.identity())

    def test_windows_environment_clears_driver_dependency_and_shell_overrides(self):
        hostile = dict(self.environment, CFLAGS="--config=/tmp/foreign.cfg", CCC_OVERRIDE_OPTIONS="+-O3",
                       CMAKE_PREFIX_PATH="/tmp/foreign", PKG_CONFIG="/tmp/pkg-config",
                       BASH_ENV="/tmp/startup", ENV="/tmp/startup", CCACHE_PREFIX="/tmp/launcher",
                       RCFLAGS="-I/tmp/foreign @/tmp/foreign.rsp")
        hostile["BASH_FUNC_cmake%%"] = "() { false; }"
        with mock.patch.dict(os.environ, hostile, clear=True), \
             mock.patch("atrinik_workspace.workspace.shutil.which", return_value="/fixture/tool"):
            environment = Workspace._local_windows_build_environment()
        for name in ("CCC_OVERRIDE_OPTIONS", "CMAKE_PREFIX_PATH", "PKG_CONFIG", "BASH_ENV", "ENV",
                     "BASH_FUNC_cmake%%", "CCACHE_PREFIX", "RCFLAGS"):
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

    def test_all_selected_llvm_tool_versions_and_family_are_enforced(self):
        for name in ("llvm-ar", "llvm-ranlib", "llvm-nm", "llvm-objcopy", "llvm-objdump",
                     "llvm-readelf", "llvm-strip", "x86_64-w64-mingw32-windres"):
            path = self.llvm / "bin" / name
            original = path.read_text()
            for invalid in (original.replace("23.1.3", "22.1.0"),
                            original.replace("LLVM version", "GNU version")):
                with self.subTest(tool=name, family=invalid == original.replace("LLVM version", "GNU version")):
                    path.write_text(invalid)
                    with self.assertRaisesRegex(WorkspaceError, "LLVM 23.1.3"):
                        self.identity()
            path.write_text(original)

    def test_multiline_versions_and_dlltool_multicall_identity(self):
        ar = self.llvm / "bin/llvm-ar"
        ar.write_text(ar.read_text().replace("LLVM version 23.1.3", "LLVM (http://llvm.org/):\n  LLVM version 23.1.3"))
        first = self.identity()
        self.assertEqual(first["tools"]["CMAKE_DLLTOOL"]["resolved_path"], str(ar))
        dlltool = self.llvm / "bin/llvm-dlltool"
        dlltool.unlink()
        dlltool.write_text(ar.read_text())
        dlltool.chmod(0o755)
        with self.assertRaisesRegex(WorkspaceError, "dlltool"):
            self.identity()

    def test_internal_sdk_links_and_namespace_mutations(self):
        link = self.sysroot / "include/generic.h"
        link.symlink_to("../../include/generic.h")
        generic = self.llvm / "include/generic.h"
        generic.write_bytes(b"generic header")
        first = self.identity()
        generic.write_bytes(b"changed header")
        self.assertNotEqual(first, self.identity())
        generic.write_bytes(b"generic header")
        link.unlink()
        link.symlink_to("../../include/./generic.h")
        self.assertNotEqual(first, self.identity())
        extra = self.prefix / "lib/dependency.a"
        extra.write_bytes(b"static archive")
        second = self.identity()
        extra.unlink()
        self.assertNotEqual(second, self.identity())

    def test_rejects_foreign_links_selected_paths_and_unsafe_entries(self):
        foreign = self.root / "foreign"
        foreign.write_bytes(b"must not be hashed")
        link = self.prefix / "include/escape"
        link.symlink_to(foreign)
        with self.assertRaisesRegex(WorkspaceError, "escaped"):
            self.identity()
        link.unlink()
        self.environment["ATRINIK_WINDOWS_PYTHON_LIBRARY"] = str(foreign)
        with self.assertRaisesRegex(WorkspaceError, "escaped"):
            self.identity()
        self.environment["ATRINIK_WINDOWS_PYTHON_LIBRARY"] = str(self.builtins)
        os.mkfifo(link)
        with self.assertRaisesRegex(WorkspaceError, "special file"):
            self.identity()
        link.unlink()
        self.header.chmod(0o666)
        with self.assertRaisesRegex(WorkspaceError, "permissions"):
            self.identity()
        self.header.chmod(0o644)
        private = self.prefix / ".ssh"
        private.mkdir()
        with self.assertRaisesRegex(WorkspaceError, "private or hidden"):
            self.identity()

    def test_tree_changes_during_inspection_are_rejected(self):
        actual = os.scandir
        changed = False
        def mutate(fd):
            nonlocal changed
            if not changed and "dependency.pc" in os.listdir(fd):
                changed = True
                self.header.write_bytes(b"changed after it was hashed")
            return actual(fd)
        with mock.patch("atrinik_workspace.workspace.os.scandir", side_effect=mutate):
            with self.assertRaisesRegex(WorkspaceError, "changed during inspection"):
                self.identity()

    def test_descriptor_walk_rejects_ancestor_replacement_without_reading(self):
        ancestor = self.root / "selected"
        ancestor.mkdir()
        selected = ancestor / "input"
        selected.write_bytes(b"selected bytes")
        foreign = self.root / "foreign"
        foreign.mkdir()
        (foreign / "input").write_bytes(b"private fixture")
        actual_open = os.open
        def replace_parent(path, *args, **kwargs):
            if path == "selected":
                ancestor.rename(self.root / "original")
                ancestor.symlink_to(foreign, target_is_directory=True)
            return actual_open(path, *args, **kwargs)
        with mock.patch("atrinik_workspace.workspace.os.open", side_effect=replace_parent):
            with self.assertRaises(OSError):
                Workspace._windows_producer_open(selected)

    def test_rejects_oversized_sdk_file_before_open(self):
        with self.header.open("wb") as stream:
            stream.truncate(512 * 1024 ** 2 + 1)
        with self.assertRaisesRegex(WorkspaceError, "byte bound"):
            self.identity()

    def test_foreign_compiler_runtime_is_rejected_before_reading(self):
        foreign = self.root / "libclang_rt.builtins-foreign.a"
        foreign.write_bytes(b"private fixture")
        compiler = self.llvm / "bin/x86_64-w64-mingw32-clang"
        compiler.write_text(compiler.read_text().replace(str(self.builtins), str(foreign)))
        actual_open = Workspace._windows_producer_open
        def guarded_open(path, *args, **kwargs):
            if path == foreign:
                self.fail("foreign compiler runtime was opened")
            return actual_open(path, *args, **kwargs)
        with mock.patch.object(Workspace, "_windows_producer_open", side_effect=guarded_open):
            with self.assertRaisesRegex(WorkspaceError, "runtime escaped"):
                self.identity()

    def test_packaging_records_identity_and_refuses_mid_build_replacement(self):
        workspace = Workspace.__new__(Workspace)
        for change, phase in ((None, "client"), ("header", "client"), ("pc", "client"),
                              ("crt", "client"), ("dll", "client"), ("header", "server")):
            with self.subTest(change=change, phase=phase):
                staging = self.root / (str(change) + phase)
                staging.mkdir()
                for component in ("client", "server"):
                    (staging / component).mkdir()
                changed_path = {"header": self.header, "pc": self.pc, "crt": self.crt,
                                "dll": self.runtime / "libunwind.dll"}.get(change, self.header)
                original = changed_path.read_bytes()
                from atrinik_workspace import workspace as module
                actual_run = module.run

                def build(command, **kwargs):
                    if command[0] != "bash":
                        return actual_run(command, **kwargs)
                    component = kwargs["cwd"].name
                    name = f"atrinik-classic-{component}-{WINDOWS_PACKAGE_VERSION}-windows-x86_64.zip"
                    (staging / "packages" / name).write_bytes(b"archive fixture")
                    if change and component == phase:
                        changed_path.write_bytes(b"replaced during build")

                with mock.patch.object(workspace, "_local_windows_build_environment", return_value=dict(self.environment)), \
                     mock.patch.object(module, "run", side_effect=build):
                    if change:
                        with self.assertRaisesRegex(WorkspaceError, "inputs changed"):
                            workspace._build_windows_profile_archives(staging)
                    else:
                        _, _, build_record = workspace._build_windows_profile_archives(staging)
                        self.assertEqual(build_record["producer"], self.identity())
                        json.dumps(build_record)
                changed_path.write_bytes(original)
