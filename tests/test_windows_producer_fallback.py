from __future__ import annotations

import os
from pathlib import Path
import shutil
from types import SimpleNamespace
import unittest
from unittest import mock

from atrinik_workspace import workspace as module
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.workspace import Workspace, WINDOWS_PACKAGE_VERSION
from tests import test_local_windows_producer as fixtures


class WindowsProducerFallbackTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.LocalWindowsProducerTests(
            "test_actual_local_files_are_rehashed_for_each_admission"
        )
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        self.environment = self.fixture.environment
        docker = self.fixture.build_tools / "docker"
        docker.write_text("#!/bin/sh\nexit 1\n")
        docker.chmod(0o755)
        self.workspace = Workspace.__new__(Workspace)
        self.workspace.paths = SimpleNamespace(repository=self.root / "wrapper")
        self.image = "fixture/windows@sha256:" + "a" * 64
        self.staging = self.root / "staging"
        for component in ("client", "server"):
            tools = self.staging / component / "tools"
            tools.mkdir(parents=True)
            (tools / "build-windows-package.sh").write_text(
                "# atrinik-windows-host-python-contract: 1\n"
            )
        self.actual_run = module.run
        self.build_commands = []

    def synthetic_build(self, command, **kwargs):
        if command[0] == "docker":
            self.build_commands.append(command)
            if command[command.index("--user") + 1] != "0:0":
                self.write_archives()
            return ""
        if command[0] == str(self.fixture.build_tools / "bash"):
            self.build_commands.append(command)
            self.write_archives()
            return ""
        return self.actual_run(command, **kwargs)

    def write_archives(self):
        for component in ("client", "server"):
            (self.staging / "packages" / (
                f"atrinik-classic-{component}-{WINDOWS_PACKAGE_VERSION}-windows-x86_64.zip"
            )).write_bytes(b"archive fixture")

    def build(self):
        with mock.patch.dict(os.environ, self.environment, clear=True), \
             mock.patch.object(self.workspace, "_windows_build_image", return_value=self.image), \
             mock.patch.object(module, "run", side_effect=self.synthetic_build):
            return self.workspace._build_windows_profile_archives(self.staging)

    def assert_docker_route(self):
        with mock.patch.object(self.workspace, "_local_windows_producer_identity") as identity:
            client, server, record = self.build()
        self.assertEqual(record["mode"], "container")
        self.assertEqual(record["image"], self.image)
        self.assertTrue(client.is_file())
        self.assertTrue(server.is_file())
        self.assertEqual(len(self.build_commands), 2)
        self.assertTrue(all(command[0] == "docker" for command in self.build_commands))
        identity.assert_not_called()

    def test_each_missing_host_tool_selects_docker_before_any_local_probe(self):
        for name in ("cmake", "cpack", "ninja", "bash"):
            with self.subTest(tool=name):
                path = self.fixture.build_tools / name
                content = path.read_bytes()
                path.unlink()
                self.assert_docker_route()
                path.write_bytes(content)
                path.chmod(0o755)
                self.reset_staging()

    def test_missing_cache_only_selects_docker_when_either_launcher_is_configured(self):
        original = self.fixture.toolchain.read_text()
        for name in ("CMAKE_C_COMPILER_LAUNCHER", "CMAKE_CXX_COMPILER_LAUNCHER"):
            with self.subTest(launcher=name):
                self.fixture.toolchain.write_text(original + f"set({name} ccache)\n")
                self.assert_docker_route()
                self.reset_staging()
        self.fixture.toolchain.write_text(original)
        _, _, record = self.build()
        self.assertEqual(record["mode"], "local")

    def test_each_missing_absolute_sdk_executable_selects_docker(self):
        for name in ("clang", "x86_64-w64-mingw32-clang", "x86_64-w64-mingw32-clang++",
                     "x86_64-w64-mingw32-windres", "ld.lld", "llvm-ar", "llvm-ranlib",
                     "llvm-nm", "llvm-objcopy", "llvm-objdump", "llvm-readelf", "llvm-strip",
                     "llvm-dlltool", "x86_64-w64-mingw32-pkg-config"):
            with self.subTest(tool=name):
                root = self.fixture.runtime if name.endswith("-pkg-config") else self.fixture.llvm / "bin"
                path = root / name
                content = path.read_bytes()
                linked = path.is_symlink()
                path.unlink()
                # dlltool is a personality of llvm-ar. Removing the underlying
                # ar implementation also removes that prerequisite, preserving
                # the distinction from an invalid dangling selected link.
                dlltool = self.fixture.llvm / "bin/llvm-dlltool"
                if name == "llvm-ar":
                    dlltool.unlink()
                self.assert_docker_route()
                if linked:
                    path.symlink_to("llvm-ar")
                else:
                    path.write_bytes(content)
                    path.chmod(0o755)
                if name == "llvm-ar":
                    dlltool.symlink_to("llvm-ar")
                self.reset_staging()

    def test_missing_fixed_host_python_selects_docker_without_a_probe(self):
        actual = Path.lstat

        def missing_python(path, *args, **kwargs):
            if path == Path("/usr/bin/python3"):
                raise FileNotFoundError(str(path))
            return actual(path, *args, **kwargs)

        with mock.patch.object(Path, "lstat", missing_python):
            self.assert_docker_route()

    def test_all_available_tools_use_local_route_and_record_fresh_identity(self):
        launcher = self.fixture.build_tools / "ccache"
        launcher.write_text("#!/bin/sh\nexit 1\n")
        launcher.chmod(0o755)
        self.fixture.toolchain.write_text(self.fixture.toolchain.read_text()
                                          + "set(CMAKE_C_COMPILER_LAUNCHER ccache)\n")
        client, server, record = self.build()
        self.assertEqual(record["mode"], "local")
        self.assertIn("ccache", record["producer"]["tools"])
        self.assertEqual(record["producer"], self.fixture.identity())
        self.assertTrue(client.is_file())
        self.assertTrue(server.is_file())
        self.assertEqual(len(self.build_commands), 2)
        self.assertTrue(all(command[0] == str(self.fixture.build_tools / "bash")
                            for command in self.build_commands))

    def test_invalid_toolchain_never_falls_back_even_with_a_missing_tool(self):
        self.fixture.toolchain.write_text(self.fixture.toolchain.read_text()
                                          + 'include("/tmp/unreviewed.cmake")\n')
        (self.fixture.build_tools / "bash").unlink()
        with self.assertRaisesRegex(WorkspaceError, "declarative LLVM toolchain"):
            self.build()
        self.assertEqual(self.build_commands, [])

    def test_invalid_present_executables_never_fall_back(self):
        for location in (self.fixture.build_tools / "bash",
                         self.fixture.llvm / "bin/x86_64-w64-mingw32-clang",
                         self.fixture.runtime / "x86_64-w64-mingw32-pkg-config"):
            for invalid in ("non-executable", "writable", "broken-link"):
                with self.subTest(path=location, invalid=invalid):
                    original = location.read_bytes()
                    if invalid == "broken-link":
                        location.unlink()
                        location.symlink_to("missing-executable")
                    else:
                        location.chmod(0o644 if invalid == "non-executable" else 0o777)
                    with self.assertRaises(WorkspaceError):
                        self.build()
                    self.assertEqual(self.build_commands, [])
                    if invalid == "broken-link":
                        location.unlink()
                        location.write_bytes(original)
                    location.chmod(0o755)

    def test_missing_host_tool_cannot_hide_unsafe_selected_sdk(self):
        (self.fixture.build_tools / "bash").unlink()
        self.fixture.header.chmod(0o666)
        with self.assertRaisesRegex(WorkspaceError, "permissions"):
            self.build()
        self.assertEqual(self.build_commands, [])

    def test_broken_selected_input_never_selects_docker(self):
        for name in ("ATRINIK_WINDOWS_TOOLCHAIN_FILE", "ATRINIK_WINDOWS_RUNTIME_DIR",
                     "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR", "ATRINIK_WINDOWS_PYTHON_LIBRARY",
                     "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR"):
            with self.subTest(input=name):
                broken = self.root / "broken-selected-input"
                broken.symlink_to("missing-input")
                original = self.environment[name]
                self.environment[name] = str(broken)
                with self.assertRaisesRegex(WorkspaceError, "selected input is invalid"):
                    self.build()
                self.assertEqual(self.build_commands, [])
                self.environment[name] = original
                broken.unlink()

    def test_each_missing_selected_input_retains_initial_docker_fallback(self):
        for name in ("ATRINIK_WINDOWS_TOOLCHAIN_FILE", "ATRINIK_WINDOWS_RUNTIME_DIR",
                     "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR", "ATRINIK_WINDOWS_PYTHON_LIBRARY",
                     "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR"):
            with self.subTest(input=name):
                original = self.environment[name]
                self.environment[name] = str(self.root / (name + "-absent"))
                self.assert_docker_route()
                self.environment[name] = original
                self.reset_staging()

    def test_each_unset_selector_validates_present_inputs_then_selects_docker(self):
        for name in ("ATRINIK_WINDOWS_TARGET", "ATRINIK_WINDOWS_TOOLCHAIN_FILE",
                     "ATRINIK_WINDOWS_RUNTIME_DIR", "ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR",
                     "ATRINIK_WINDOWS_PYTHON_LIBRARY", "ATRINIK_WINDOWS_PYTHON_RUNTIME_DIR"):
            with self.subTest(selector=name):
                original = self.environment.pop(name)
                self.assert_docker_route()
                self.environment[name] = original
                self.reset_staging()

    def test_malformed_present_toolchain_fails_with_absent_or_unset_selectors(self):
        self.fixture.toolchain.write_text(self.fixture.toolchain.read_text()
                                          + 'include("/tmp/unreviewed.cmake")\n')
        for name in ("ATRINIK_WINDOWS_PYTHON_LIBRARY", "ATRINIK_WINDOWS_TARGET"):
            for missing in ("absent", "unset"):
                with self.subTest(selector=name, missing=missing):
                    original = self.environment.pop(name)
                    if missing == "absent":
                        self.environment[name] = (str(self.root / "absent-library")
                                                  if name.endswith("LIBRARY") else "")
                    with mock.patch.object(module, "run") as execute, \
                         mock.patch.object(module.shutil, "which") as discover, \
                         mock.patch.dict(os.environ, self.environment, clear=True), \
                         self.assertRaisesRegex(WorkspaceError, "declarative LLVM toolchain"):
                        self.workspace._local_windows_build_environment()
                    execute.assert_not_called()
                    discover.assert_not_called()
                    self.environment[name] = original

    def test_missing_earlier_input_cannot_hide_a_broken_later_selected_link(self):
        broken = self.root / "broken-library"
        broken.symlink_to("missing-library")
        self.environment["ATRINIK_WINDOWS_PYTHON_LIBRARY"] = str(broken)
        for missing in ("absent", "unset"):
            with self.subTest(missing=missing):
                original = self.environment.pop("ATRINIK_WINDOWS_RUNTIME_DIR")
                if missing == "absent":
                    self.environment["ATRINIK_WINDOWS_RUNTIME_DIR"] = str(self.root / "absent-runtime")
                with self.assertRaisesRegex(WorkspaceError, "selected input is invalid"):
                    self.build()
                self.assertEqual(self.build_commands, [])
                self.environment["ATRINIK_WINDOWS_RUNTIME_DIR"] = original

    def test_partial_selection_cannot_hide_unsafe_sdk_or_incomplete_layout(self):
        self.environment.pop("ATRINIK_WINDOWS_PYTHON_LIBRARY")
        self.fixture.header.chmod(0o666)
        with self.assertRaisesRegex(WorkspaceError, "permissions"):
            self.build()
        self.assertEqual(self.build_commands, [])
        self.fixture.header.chmod(0o644)
        include = self.fixture.prefix / "include"
        include.rename(self.fixture.prefix / "held-include")
        with self.assertRaisesRegex(WorkspaceError, "SDK layout is incomplete"):
            self.build()
        self.assertEqual(self.build_commands, [])

    def test_missing_toolchain_cannot_hide_unsafe_present_selected_namespace(self):
        self.environment.pop("ATRINIK_WINDOWS_TOOLCHAIN_FILE")
        self.environment["ATRINIK_WINDOWS_PYTHON_INCLUDE_DIR"] = str(self.fixture.prefix / "include")
        self.fixture.header.chmod(0o666)
        with self.assertRaisesRegex(WorkspaceError, "permissions"):
            self.build()
        self.assertEqual(self.build_commands, [])

    def test_complete_identity_keeps_rejecting_an_unselected_target(self):
        with mock.patch.object(module, "run") as execute, \
             self.assertRaisesRegex(WorkspaceError, "target or prefix changed"):
            Workspace._local_windows_producer_identity({**self.environment, "ATRINIK_WINDOWS_TARGET": None})
        execute.assert_not_called()

    def test_present_toolchain_bounds_and_permissions_fail_before_parsing(self):
        original = self.fixture.toolchain.read_bytes()
        for invalid in ("oversized", "writable", "directory"):
            with self.subTest(input=invalid):
                if invalid == "oversized":
                    self.fixture.toolchain.write_bytes(b"#" * (1024 ** 2 + 1))
                elif invalid == "writable":
                    self.fixture.toolchain.chmod(0o666)
                else:
                    self.fixture.toolchain.unlink()
                    self.fixture.toolchain.mkdir()
                with mock.patch.object(Workspace, "_windows_producer_toolchain_contract") as parse, \
                     mock.patch.object(module, "run") as execute, \
                     mock.patch.dict(os.environ, self.environment, clear=True), \
                     self.assertRaisesRegex(WorkspaceError, "safe bounded regular file"):
                    self.workspace._local_windows_build_environment()
                parse.assert_not_called()
                execute.assert_not_called()
                if invalid == "directory":
                    self.fixture.toolchain.rmdir()
                self.fixture.toolchain.write_bytes(original)
                self.fixture.toolchain.chmod(0o644)

    def test_present_invalid_compiler_fails_closed_after_identity_probe(self):
        compiler = self.fixture.llvm / "bin/x86_64-w64-mingw32-clang"
        compiler.write_text(compiler.read_text().replace("23.1.3", "22.1.0"))
        with self.assertRaisesRegex(WorkspaceError, "LLVM 23.1.3"):
            self.build()
        self.assertEqual(self.build_commands, [])

    def test_tool_loss_after_local_selection_never_retries_docker(self):
        actual = self.workspace._local_windows_build_environment

        def disappear_after_selection():
            environment = actual()
            (self.fixture.build_tools / "cpack").unlink()
            return environment

        with mock.patch.object(self.workspace, "_local_windows_build_environment",
                               side_effect=disappear_after_selection), \
             self.assertRaisesRegex(WorkspaceError, "build tool is unavailable: cpack"):
            self.build()
        self.assertEqual(self.build_commands, [])

    def test_prerequisite_gate_fences_toolchain_changes_without_execution(self):
        actual = Workspace._windows_producer_toolchain_contract

        def change_after_parse(text, target):
            result = actual(text, target)
            self.fixture.toolchain.write_text(text + "# changed after parsing\n")
            return result

        with mock.patch.object(Workspace, "_windows_producer_toolchain_contract",
                               side_effect=change_after_parse), \
             mock.patch.object(module, "run") as execute, \
             mock.patch.dict(os.environ, self.environment, clear=True), \
             self.assertRaisesRegex(WorkspaceError, "changed during prerequisite inspection"):
            self.workspace._local_windows_build_environment()
        execute.assert_not_called()

    def reset_staging(self):
        # Only synthetic fixture artifacts are removed between subcases.
        shutil.rmtree(self.staging)
        for component in ("client", "server"):
            tools = self.staging / component / "tools"
            tools.mkdir(parents=True)
            (tools / "build-windows-package.sh").write_text(
                "# atrinik-windows-host-python-contract: 1\n"
            )
        self.build_commands.clear()


if __name__ == "__main__":
    unittest.main()
