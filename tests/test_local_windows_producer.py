from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
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
        self.build_tools = self.root / "build-tools"
        self.build_tools.mkdir()
        # These tools are fingerprinted, while packaging itself is mocked below.
        # Keep their bytes and discovery independent of host SDK installations.
        for name in ("cmake", "cpack", "ninja", "bash"):
            path = self.build_tools / name
            path.write_text(f"#!/bin/sh\n# {name} provenance fixture\nexit 1\n")
            path.chmod(0o755)
        self.environment = {"ATRINIK_WINDOWS_TARGET": "x86_64-w64-mingw32",
                            "ATRINIK_WINDOWS_TOOLCHAIN_FILE": str(self.toolchain),
                            "ATRINIK_WINDOWS_RUNTIME_DIR": str(self.runtime),
                            "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR": str(self.runtime),
                            "ATRINIK_WINDOWS_PYTHON_LIBRARY": str(self.builtins),
                            "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR": str(self.runtime),
                            "PATH": os.pathsep.join((str(self.build_tools), str(self.llvm / "bin")))}

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

    def test_owned_build_tools_are_required_and_rehashed_without_host_path(self):
        with mock.patch.dict(os.environ, {"PATH": ""}):
            first = self.identity()
            for name in ("cmake", "cpack", "ninja", "bash"):
                with self.subTest(tool=name):
                    path = self.build_tools / name
                    original = path.read_bytes()
                    self.assertEqual(first["tools"][name]["resolved_path"], str(path))
                    self.assertEqual(first["tools"][name]["sha256"], hashlib.sha256(original).hexdigest())
                    path.write_bytes(original + b"# changed owned build tool\n")
                    self.assertNotEqual(first, self.identity())
                    path.unlink()
                    with self.assertRaisesRegex(WorkspaceError, "build tool is unavailable: " + name + "$"):
                        self.identity()
                    path.write_bytes(original)
                    path.chmod(0o755)
                    self.assertEqual(first, self.identity())

    def test_windows_environment_clears_driver_dependency_and_shell_overrides(self):
        hostile = dict(self.environment, CFLAGS="--config=/tmp/foreign.cfg", CCC_OVERRIDE_OPTIONS="+-O3",
                       CMAKE_PREFIX_PATH="/tmp/foreign", PKG_CONFIG="/tmp/pkg-config",
                       BASH_ENV="/tmp/startup", ENV="/tmp/startup", CCACHE_PREFIX="/tmp/launcher",
                       RCFLAGS="-I/tmp/foreign @/tmp/foreign.rsp")
        hostile["BASH_FUNC_cmake%%"] = "() { false; }"
        with mock.patch.dict(os.environ, hostile, clear=True):
            environment = Workspace._local_windows_build_environment()
        for name in ("CCC_OVERRIDE_OPTIONS", "CMAKE_PREFIX_PATH", "PKG_CONFIG", "BASH_ENV", "ENV",
                     "BASH_FUNC_cmake%%", "CCACHE_PREFIX", "RCFLAGS"):
            self.assertNotIn(name, environment)
        self.assertEqual(environment["CFLAGS"], "--no-default-config")
        self.assertEqual(environment["CXXFLAGS"], "--no-default-config")
        self.assertEqual(environment["LDFLAGS"], "--no-default-config")
        self.assertEqual(environment["CCACHE_DISABLE"], "1")

    def test_relative_path_attack_executes_staged_shell_but_local_admission_rejects_it(self):
        workspace = Workspace.__new__(Workspace)
        for entry in ("build-tools", ".", ""):
            with self.subTest(entry=entry):
                staging = self.root / ("staged-" + (entry or "empty"))
                component = staging / "client"
                malicious = component / entry / "bash"
                malicious.parent.mkdir(parents=True)
                malicious.write_text('#!/bin/sh\nprintf staged-source > "$1"\n')
                malicious.chmod(0o755)
                trusted = self.root / entry / "bash"
                if not trusted.exists():
                    trusted.write_text("#!/bin/sh\nexit 1\n")
                    trusted.chmod(0o755)
                marker = staging / "executed"
                environment = {**self.environment, "PATH": entry + os.pathsep + self.environment["PATH"]}
                # Actual pre-fix lookup/execution semantics: the coordinator
                # observes its host shell, but exec searches from staged cwd.
                probe = (
                    "import pathlib, shutil, subprocess, sys\n"
                    "print(pathlib.Path(shutil.which('bash')).resolve(), flush=True)\n"
                    "subprocess.run(['bash', sys.argv[2]], cwd=sys.argv[1], check=True, timeout=10)\n"
                )
                observed = subprocess.check_output(
                    ["/usr/bin/python3", "-I", "-c", probe, str(component), str(marker)],
                    cwd=self.root, env=environment, text=True, timeout=15,
                )
                self.assertEqual(observed.strip(), str(trusted.resolve()))
                self.assertEqual(marker.read_text(), "staged-source")
                marker.unlink()
                with mock.patch.dict(os.environ, environment, clear=True), \
                     mock.patch("atrinik_workspace.workspace.run") as execute, \
                     self.assertRaisesRegex(WorkspaceError, "PATH requires nonempty absolute entries"):
                    workspace._build_windows_profile_archives(staging)
                execute.assert_not_called()
                self.assertFalse(marker.exists())
                self.assertFalse((staging / "packages").exists())

    def test_identity_rejects_each_nonabsolute_path_entry_before_probing_tools(self):
        valid = self.environment["PATH"]
        for search_path in ("", ".", "build-tools", ":" + valid, valid + ":", valid + "::/bin",
                            valid + ":./tools", valid + ":../tools", valid + ":~/bin"):
            with self.subTest(path=search_path), \
                 mock.patch("atrinik_workspace.workspace.run") as execute, \
                 mock.patch("atrinik_workspace.workspace.shutil.which") as discover, \
                 self.assertRaisesRegex(WorkspaceError, "PATH requires nonempty absolute entries"):
                Workspace._local_windows_producer_identity({**self.environment, "PATH": search_path})
            execute.assert_not_called()
            discover.assert_not_called()

    def test_nonlocal_environment_retains_fallback_with_relative_path(self):
        for environment in ({"PATH": ".:"}, {**self.environment, "PATH": "build-tools:"}):
            environment.pop("ATRINIK_WINDOWS_TOOLCHAIN_FILE", None)
            with self.subTest(environment=environment), mock.patch.dict(os.environ, environment, clear=True):
                self.assertIsNone(Workspace._local_windows_build_environment())
        # A missing host compiler still selects the existing Docker fallback.
        (self.llvm / "bin/x86_64-w64-mingw32-clang").unlink()
        with mock.patch.dict(os.environ, self.environment, clear=True):
            self.assertIsNone(Workspace._local_windows_build_environment())

    def test_real_packaging_executes_recorded_absolute_host_tools_from_staged_cwd(self):
        from atrinik_workspace import workspace as module
        trace = self.root / "host-tools.log"
        for name in ("bash", "cmake", "cpack", "ninja"):
            script = '#!/bin/sh\nprintf \'%s|%s\\n\' "$0" "$PWD" >> ' + shlex.quote(str(trace)) + "\n"
            if name == "bash":
                script += 'exec /bin/bash "$@"\n'
            (self.build_tools / name).write_text(script)
        staging = self.root / "actual-packaging"
        marker = self.root / "staged-tool-executed"
        for component in ("client", "server"):
            tools = staging / component / "tools"
            tools.mkdir(parents=True)
            archive = f"atrinik-classic-{component}-{WINDOWS_PACKAGE_VERSION}-windows-x86_64.zip"
            (tools / "build-windows-package.sh").write_text(
                "# atrinik-windows-host-python-contract: 1\nset -euo pipefail\n"
                'cmake\ncpack\nninja\nprintf "archive fixture" > "$1/' + archive + '"\n'
            )
            for directory in (staging / component, staging / component / "build-tools"):
                directory.mkdir(exist_ok=True)
                for name in ("bash", "cmake", "cpack", "ninja"):
                    path = directory / name
                    path.write_text("#!/bin/sh\nprintf malicious > " + shlex.quote(str(marker)) + "\nexit 1\n")
                    path.chmod(0o755)
        with mock.patch.dict(os.environ, self.environment, clear=True), \
             mock.patch.object(module, "run", wraps=module.run) as execute:
            client, server, record = Workspace.__new__(Workspace)._build_windows_profile_archives(staging)
        self.assertEqual(client.read_bytes(), b"archive fixture")
        self.assertEqual(server.read_bytes(), b"archive fixture")
        self.assertFalse(marker.exists())
        expected = [record["producer"]["tools"][name]["path"] + "|" + str(staging / component)
                    for component in ("client", "server") for name in ("bash", "cmake", "cpack", "ninja")]
        self.assertEqual(trace.read_text().splitlines(), expected)
        builds = [call for call in execute.call_args_list if "cwd" in call.kwargs]
        self.assertEqual(len(builds), 2)
        for call in builds:
            self.assertEqual(call.args[0][0], record["producer"]["tools"]["bash"]["path"])
            self.assertTrue(Path(call.args[0][0]).is_absolute())
            self.assertEqual(call.kwargs["env"]["PATH"], self.environment["PATH"])

    def test_selected_cache_launcher_bytes_are_fresh_inputs(self):
        self.toolchain.write_text(self.toolchain.read_text() + "set(CMAKE_C_COMPILER_LAUNCHER ccache)\n")
        launcher = self.llvm / "bin/ccache"
        launcher.write_text("#!/bin/sh\nexec \"$@\"\n")
        launcher.chmod(0o755)
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
                              ("crt", "client"), ("dll", "client"), ("header", "server"),
                              *((name, "client") for name in ("cmake", "cpack", "ninja", "bash"))):
            with self.subTest(change=change, phase=phase):
                staging = self.root / (str(change) + phase)
                staging.mkdir()
                for component in ("client", "server"):
                    tools = staging / component / "tools"
                    tools.mkdir(parents=True)
                    (tools / "build-windows-package.sh").write_text("# atrinik-windows-host-python-contract: 1\n")
                changed_path = {"header": self.header, "pc": self.pc, "crt": self.crt,
                                "dll": self.runtime / "libunwind.dll",
                                **{name: self.build_tools / name for name in ("cmake", "cpack", "ninja", "bash")}
                                }.get(change, self.header)
                original = changed_path.read_bytes()
                from atrinik_workspace import workspace as module
                actual_run = module.run

                def build(command, **kwargs):
                    if command[0] != str(self.build_tools / "bash"):
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
