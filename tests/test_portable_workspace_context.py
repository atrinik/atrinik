from __future__ import annotations

from contextlib import ExitStack, contextmanager
from pathlib import Path
from types import SimpleNamespace
import hashlib
import os
import tempfile
import threading
import unittest
from unittest import mock

from atrinik_workspace import linux_portable
from atrinik_workspace.linux_portable import PortableBuildContext as AdmittedContext
from atrinik_workspace.model import WorkspaceError, atomic_json
from atrinik_workspace.workspace import CONFIGURE_METADATA, Workspace
from tests.test_linux_portable_acceptance import fixture_build_tools, fixture_tool_path_protected


class RecordingContext:
    evidence = {}
    host_tools = {"fixture": "host-tools interface only"}
    build_tools = {"fixture": "producer-build-tools interface only"}

    def verify_host_tools(self):
        pass

    def __init__(self, events):
        self.events = events

    def validate_selected(self, selected):
        pass

    def validate_source_binding(self, binary, source):
        pass

    def bind_source_views(self, root, views):
        self.events.append(("sources", root, views))

    def capture_configure(self, binary, source):
        self.events.append(("configure-evidence", binary, source))

    def capture_output(self, binary):
        self.events.append(("output-evidence", binary))


class PortableWorkspaceContextTests(unittest.TestCase):
    def setUp(self):
        self.capture_shell = linux_portable.command_shell.capture_identity
        self.verify_shell = linux_portable.command_shell.verify_identity
        for method, result in (("capture_identity", {"fixture": "command shell"}), ("verify_identity", None)):
            shell_patch = mock.patch.object(linux_portable.command_shell, method, return_value=result)
            shell_patch.start()
            self.addCleanup(shell_patch.stop)
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.workspace = object.__new__(Workspace)
        self.workspace.paths = SimpleNamespace(builds=self.root / "builds")
        self.workspace._build_state = threading.local()
        self.workspace._use_ccache = False
        self.source = self.root / "source"
        self.source.mkdir()
        self.binary = self.workspace.paths.builds / "profiles" / "test" / "build" / "client"
        self.events = []
        self.context = RecordingContext(self.events)
        patcher = mock.patch.object(linux_portable, "PortableBuildContext", RecordingContext, create=True)
        patcher.start()
        self.addCleanup(patcher.stop)

    def cmake_patches(self, fail=None, *, real_compiler=False):
        stack = ExitStack()
        self.addCleanup(stack.close)
        fingerprint = {"build_tree_identity": {"toolchain_file": None}, "source": {"configure_skip_safe": True}}
        stack.enter_context(mock.patch.object(self.workspace, "_add_debug_prefix_environment", return_value={}))
        stack.enter_context(mock.patch.object(self.workspace, "_configure_fingerprint", return_value=fingerprint))
        stack.enter_context(mock.patch.object(self.workspace, "_cmake_state_valid", return_value=True))
        stack.enter_context(mock.patch.object(linux_portable, "installed_metadata", return_value=({}, {"compiler-evidence.json": self.context.evidence})))
        if not real_compiler:
            stack.enter_context(mock.patch.object(linux_portable, "_verify_compiler_bytes"))

        def run(command, **kwargs):
            phase = "configure" if "-S" in command else "build"
            self.events.append((phase, command))
            if phase == fail:
                raise WorkspaceError("failed " + phase)

        stack.enter_context(mock.patch("atrinik_workspace.workspace.run", side_effect=run))
        return fingerprint

    def test_portable_forces_fresh_configure_over_matching_forged_cache(self):
        fingerprint = self.cmake_patches()
        self.workspace._prepare_cmake_binary(self.binary)
        atomic_json(self.binary / CONFIGURE_METADATA, fingerprint)
        sentinel = self.binary / "forged-output"
        sentinel.write_text("cached")
        stale_header = self.binary / "src" / "include" / "stdio.h"
        stale_header.parent.mkdir(parents=True)
        stale_header.write_text("stale generated header")
        def capture_configure(binary, source):
            self.assertFalse(stale_header.exists())
            self.events.append(("configure-evidence", binary, source))
        self.context.capture_configure = capture_configure
        self.workspace._source_view_unchanged[str(self.source)] = True
        self.workspace._cmake(self.source, self.binary, ["-DBUILD_TESTING=ON"], False,
                              portable=True, portable_context=self.context)
        self.assertFalse(sentinel.exists())
        self.assertEqual([event[0] for event in self.events],
                         ["configure", "configure-evidence", "build", "output-evidence"])
        self.assertEqual(self.events[0][1][-1], "-DBUILD_TESTING=OFF")
        self.assertEqual(self.events[1][1:], (self.binary, self.source))

    def test_actual_tool_replacement_stops_probes_build_and_output_capture(self):
        import shutil
        import subprocess
        # Test-only policy seam for private copied tools; production accepts root only.
        policy = mock.patch.object(AdmittedContext, "_build_tool_path_protected",
                                   side_effect=fixture_tool_path_protected)
        policy.start()
        self.addCleanup(policy.stop)
        for phase in ("fingerprint", "configure", "prebuild", "postbuild"):
            for name in ("cmake", "ninja", "pkg_config"):
                self.events.clear()
                installation = self.root / (phase + "-" + name)
                tools = installation / "bin"
                expected = fixture_build_tools(installation)
                records = expected["tools"]
                paths = {tool: record["path"] for tool, record in records.items()}
                with mock.patch.dict(linux_portable.export.PORTABLE_BUILD_TOOL_PATHS, paths, clear=True):
                    admitted = AdmittedContext({}, {}, {}, {}, build_tools=expected)
                admitted.build_tools = admitted._capture_build_tools()
                admitted.host_tools = {"command_shell": linux_portable._capture_command_shell(),
                                       "shader_python": admitted._capture_generator_python()}
                self.context.build_tools = admitted.build_tools
                self.context.host_tools = admitted.host_tools
                self.context.verify_host_tools = admitted.verify_host_tools
                def replace():
                    path = tools / name
                    replacement = path.with_suffix(".replacement")
                    shutil.copyfile(path, replacement)
                    replacement.chmod(0o700)
                    replacement.replace(path)
                fingerprint = self.cmake_patches()
                def probe(*args, **kwargs):
                    if phase == "fingerprint": replace()
                    return fingerprint
                def run(command, **kwargs):
                    action = "configure" if "-S" in command else "build"
                    self.events.append((action, command))
                    if phase == "configure" and name == "pkg_config":
                        # Exercise an actual admitted dependency query before
                        # replacing its bytes/identity during the configure step.
                        packages = installation / "pkgconfig"
                        packages.mkdir()
                        (packages / "fixture.pc").write_text(
                            "Name: fixture\nDescription: test-owned probe\nVersion: 1.2.3\nLibs:\nCflags:\n")
                        queried = subprocess.run([records["pkg_config"]["path"], "--modversion", "fixture"],
                            check=True, capture_output=True, text=True, timeout=15,
                            env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "PKG_CONFIG_PATH": "",
                                 "PKG_CONFIG_LIBDIR": str(packages)})
                        self.assertEqual(queried.stdout, "1.2.3\n")
                    if (phase == "configure" and action == "configure") or (phase == "postbuild" and action == "build"):
                        replace()
                def capture(binary, source):
                    self.events.append(("configure-evidence", binary, source))
                    if phase == "prebuild": replace()
                self.context.capture_configure = capture
                with self.subTest(phase=phase, tool=name), \
                     mock.patch.object(self.workspace, "_configure_fingerprint", side_effect=probe), \
                     mock.patch("atrinik_workspace.workspace.run", side_effect=run), \
                     self.assertRaisesRegex(WorkspaceError, "build tool executable changed"):
                    self.workspace._cmake(self.source, self.binary, [], False,
                                          portable=True, portable_context=self.context)
                actions = [event[0] for event in self.events]
                self.assertNotIn("output-evidence", actions)
                if phase != "postbuild": self.assertNotIn("build", actions)
                if phase == "fingerprint": self.assertNotIn("configure", actions)

    def compiler_context(self, root):
        root.mkdir()
        records = {}
        for role in ("compiler", "cxx_compiler", "linker", "archiver", "ranlib", "nm",
                     "strip", "objcopy", "objdump", "readelf", "runtime", "unwinder"):
            path = root / role
            path.write_bytes(("authenticated fixture " + role).encode())
            records[role] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        evidence = {"tools": {name: row for name, row in records.items()
                               if name not in {"runtime", "unwinder"}},
                    "runtime": records["runtime"], "unwinder": records["unwinder"]}
        # No compiler executable is invoked by these byte-drift controls.
        with mock.patch.object(linux_portable.export, "portable_build_tools", return_value={}):
            admitted = AdmittedContext({}, {}, {}, evidence, build_tools={})
        admitted.build_tools = {}
        python = root / "python"
        python.write_bytes(b"fixture host-tool receipt")
        python_patch = mock.patch.object(linux_portable, "PORTABLE_GENERATOR_PYTHON", python)
        python_patch.start()
        self.addCleanup(python_patch.stop)
        admitted.host_tools = {"command_shell": linux_portable._capture_command_shell(),
                               "shader_python": {"path": str(python), "resolved_path": str(python),
                                                **admitted._record(python), "version": {}}}
        return admitted, records, evidence

    def test_whole_compiler_cohort_drift_stops_next_build_phase(self):
        for phase in ("prefix", "fingerprint", "configure", "prebuild", "postbuild"):
            for role in ("compiler", "cxx_compiler", "linker", "archiver", "ranlib", "nm",
                         "strip", "objcopy", "objdump", "readelf", "runtime", "unwinder"):
                with self.subTest(phase=phase, role=role), ExitStack() as stack:
                    self.events.clear()
                    admitted, records, evidence = self.compiler_context(self.root / (phase + role))
                    self.context.evidence = admitted.evidence
                    self.context.verify_host_tools = admitted.verify_host_tools
                    self.context.host_tools = admitted.host_tools
                    self.context.build_tools = admitted.build_tools
                    fingerprint = self.cmake_patches(real_compiler=True)
                    def mutate():
                        path = Path(records[role]["path"])
                        path.write_bytes(b"changed compiler cohort member")
                        # Refreshing caller data must not replace in-memory authority.
                        records[role]["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
                    def prefix(*args, **kwargs):
                        if phase == "prefix": mutate()
                        return {}
                    def probe(*args, **kwargs):
                        if phase == "fingerprint": mutate()
                        return fingerprint
                    def run(command, **kwargs):
                        action = "configure" if "-S" in command else "build"
                        self.events.append((action, command))
                        if (phase == "configure" and action == "configure") or (phase == "postbuild" and action == "build"):
                            mutate()
                    def capture(binary, source):
                        self.events.append(("configure-evidence", binary, source))
                        if phase == "prebuild": mutate()
                    self.context.capture_configure = capture
                    stack.enter_context(mock.patch.object(self.workspace, "_add_debug_prefix_environment", side_effect=prefix))
                    stack.enter_context(mock.patch.object(self.workspace, "_configure_fingerprint", side_effect=probe))
                    stack.enter_context(mock.patch("atrinik_workspace.workspace.run", side_effect=run))
                    with self.assertRaisesRegex(WorkspaceError, "selected tool or runtime bytes differ"):
                        self.workspace._cmake(self.source, self.binary, [], False,
                                              portable=True, portable_context=self.context)
                    actions = [event[0] for event in self.events]
                    self.assertNotIn("output-evidence", actions)
                    if phase != "postbuild": self.assertNotIn("build", actions)
                    if phase in {"prefix", "fingerprint"}: self.assertNotIn("configure", actions)

    @staticmethod
    def change_shell(fixture, change):
        if change == "bytes":
            fixture.target.write_bytes(fixture.raw[:-1] + b"x")
        elif change == "replacement":
            replacement = fixture.root / "replacement"
            replacement.write_bytes(fixture.raw)
            replacement.chmod(0o755)
            replacement.replace(fixture.target)
        elif change == "link":
            path = fixture.root / "usr/bin/sh"
            path.unlink()
            path.symlink_to("./dash")
        elif change == "target-alias":
            fixture.target.rename(fixture.root / "other")
            fixture.target.symlink_to("../../other")
        elif change == "mode":
            fixture.target.chmod(0o775)
        elif change == "hardlink":
            os.link(fixture.target, fixture.root / "alias")
        elif change == "missing":
            (fixture.root / "usr/bin/sh").unlink()
        else:
            raise AssertionError("unknown shell fixture mutation")

    def test_actual_shell_drift_stops_next_build_phase(self):
        from tests.test_linux_portable_shell import ShellFixture
        for phase in ("prefix", "fingerprint", "configure", "prebuild", "postbuild"):
            for change in ("bytes", "replacement", "link", "target-alias", "mode", "hardlink", "missing"):
                with self.subTest(phase=phase, change=change), ExitStack() as stack:
                    self.events.clear()
                    admitted, _, _ = self.compiler_context(self.root / (phase + change))
                    fixture = ShellFixture()
                    stack.callback(fixture.close)
                    stack.enter_context(mock.patch.object(linux_portable.command_shell, "capture_identity", self.capture_shell))
                    stack.enter_context(mock.patch.object(linux_portable.command_shell, "verify_identity", self.verify_shell))
                    admitted.host_tools["command_shell"] = linux_portable._capture_command_shell()
                    self.context.evidence = admitted.evidence
                    self.context.verify_host_tools = admitted.verify_host_tools
                    self.context.host_tools = admitted.host_tools
                    self.context.build_tools = admitted.build_tools
                    fingerprint = self.cmake_patches(real_compiler=True)
                    def mutate(): self.change_shell(fixture, change)
                    def prefix(*args, **kwargs):
                        if phase == "prefix": mutate()
                        return {}
                    def probe(*args, **kwargs):
                        if phase == "fingerprint": mutate()
                        return fingerprint
                    def run(command, **kwargs):
                        action = "configure" if "-S" in command else "build"
                        self.events.append((action, command))
                        if (phase == "configure" and action == "configure") or (phase == "postbuild" and action == "build"):
                            mutate()
                    def capture(binary, source):
                        self.events.append(("configure-evidence", binary, source))
                        if phase == "prebuild": mutate()
                    self.context.capture_configure = capture
                    stack.enter_context(mock.patch.object(self.workspace, "_add_debug_prefix_environment", side_effect=prefix))
                    stack.enter_context(mock.patch.object(self.workspace, "_configure_fingerprint", side_effect=probe))
                    stack.enter_context(mock.patch("atrinik_workspace.workspace.run", side_effect=run))
                    with self.assertRaisesRegex(WorkspaceError, "portable-shell"):
                        self.workspace._cmake(self.source, self.binary, [], False,
                                              portable=True, portable_context=self.context)
                    actions = [event[0] for event in self.events]
                    self.assertNotIn("output-evidence", actions)
                    if phase != "postbuild": self.assertNotIn("build", actions)
                    if phase in {"prefix", "fingerprint"}: self.assertNotIn("configure", actions)

    def test_actual_shell_drift_precedes_capture_and_revalidation_probes(self):
        from tests.test_linux_portable_shell import ShellFixture
        for boundary in ("capture_output", "revalidate"):
            for change in ("bytes", "replacement", "link", "target-alias", "mode", "hardlink", "missing"):
                with self.subTest(boundary=boundary, change=change), ExitStack() as stack:
                    admitted, _, _ = self.compiler_context(self.root / (boundary + change))
                    fixture = ShellFixture()
                    stack.callback(fixture.close)
                    stack.enter_context(mock.patch.object(linux_portable.command_shell, "capture_identity", self.capture_shell))
                    stack.enter_context(mock.patch.object(linux_portable.command_shell, "verify_identity", self.verify_shell))
                    admitted.host_tools["command_shell"] = linux_portable._capture_command_shell()
                    admitted.binary, admitted.source, admitted.baseline = self.binary, self.source, {}
                    self.change_shell(fixture, change)
                    with mock.patch.object(admitted, "_ninja") as ninja, \
                         mock.patch.object(admitted, "_generated_inventory") as inventory, \
                         mock.patch.object(linux_portable.export, "_tool_report") as driver, \
                         self.assertRaisesRegex(linux_portable.export.ExportError, "portable-shell"):
                        getattr(admitted, boundary)(self.binary)
                    ninja.assert_not_called()
                    inventory.assert_not_called()
                    driver.assert_not_called()
                    self.assertIsNone(admitted.output)

    def test_refreshed_disk_evidence_cannot_replace_caller_authority(self):
        self.cmake_patches()
        with mock.patch.object(linux_portable, "installed_metadata", return_value=(
                {}, {"compiler-evidence.json": {"foreign": "metadata"}})), \
             mock.patch.object(self.workspace, "_add_debug_prefix_environment") as prefix, \
             mock.patch.object(self.workspace, "_configure_fingerprint") as fingerprint, \
             self.assertRaisesRegex(WorkspaceError, "compiler evidence changed from caller context"):
            self.workspace._cmake(self.source, self.binary, [], False,
                                  portable=True, portable_context=self.context)
        prefix.assert_not_called()
        fingerprint.assert_not_called()
        self.assertEqual(self.events, [])

    def test_context_cohort_drift_precedes_capture_and_revalidation_probes(self):
        for boundary in ("capture_output", "revalidate"):
            for role in ("compiler", "cxx_compiler", "linker", "archiver", "ranlib", "nm",
                         "strip", "objcopy", "objdump", "readelf", "runtime", "unwinder"):
                with self.subTest(boundary=boundary, role=role):
                    admitted, records, _ = self.compiler_context(self.root / (boundary + role))
                    admitted.binary, admitted.source, admitted.baseline = self.binary, self.source, {}
                    Path(records[role]["path"]).write_bytes(b"changed after build return")
                    with mock.patch.object(admitted, "_ninja") as ninja, \
                         mock.patch.object(admitted, "_generated_inventory") as inventory, \
                         mock.patch.object(linux_portable.export, "_tool_report") as driver, \
                         self.assertRaisesRegex(linux_portable.export.ExportError, "selected tool or runtime bytes differ"):
                        getattr(admitted, boundary)(self.binary)
                    ninja.assert_not_called()
                    inventory.assert_not_called()
                    driver.assert_not_called()
                    self.assertIsNone(admitted.output)

    def test_ordinary_matching_cache_still_reuses_configure(self):
        fingerprint = self.cmake_patches()
        self.workspace._prepare_cmake_binary(self.binary)
        atomic_json(self.binary / CONFIGURE_METADATA, fingerprint)
        sentinel = self.binary / "normal-output"
        sentinel.write_text("cached")
        self.workspace._source_view_unchanged[str(self.source)] = True
        self.workspace._cmake(self.source, self.binary, [], False)
        self.assertTrue(sentinel.exists())
        self.assertEqual([event[0] for event in self.events], ["build"])

    def test_failed_configure_does_not_capture_receipt_or_build(self):
        self.cmake_patches(fail="configure")
        with self.assertRaisesRegex(WorkspaceError, "failed configure"):
            self.workspace._cmake(self.source, self.binary, [], False,
                                  portable=True, portable_context=self.context)
        self.assertEqual([event[0] for event in self.events], ["configure"])

    def test_failed_build_does_not_capture_output(self):
        self.cmake_patches(fail="build")
        with self.assertRaisesRegex(WorkspaceError, "failed build"):
            self.workspace._cmake(self.source, self.binary, [], False,
                                  portable=True, portable_context=self.context)
        self.assertEqual([event[0] for event in self.events], ["configure", "configure-evidence", "build"])

    def test_context_admission_precedes_mutation(self):
        with mock.patch.object(self.workspace, "_prepare_cmake_binary") as prepare:
            for context in (None, object()):
                with self.assertRaisesRegex(WorkspaceError, "process build context"):
                    self.workspace._cmake(self.source, self.binary, [], False, portable=True, portable_context=context)
            with self.assertRaisesRegex(WorkspaceError, "BUILD_TESTING=OFF"):
                self.workspace._cmake(self.source, self.binary, [], True, portable=True, portable_context=self.context)
            with self.assertRaisesRegex(WorkspaceError, "requires a portable"):
                self.workspace._cmake(self.source, self.binary, [], False, portable_context=self.context)
            prepare.assert_not_called()
        with mock.patch.object(self.workspace, "_profile_build_key") as key:
            with self.assertRaisesRegex(WorkspaceError, "process build context"):
                self.workspace._build_resolved("client", "classic", False, ["client"], {}, portable=True)
            key.assert_not_called()

    def test_foreign_context_selection_fails_before_mutation(self):
        with (
            mock.patch.object(self.context, "validate_selected", side_effect=ValueError("foreign selected source")),
            mock.patch.object(self.workspace, "_profile_build_key") as key,
        ):
            with self.assertRaisesRegex(WorkspaceError, "foreign selected source"):
                self.workspace._build_resolved("client", "classic", False, ["client"], {},
                                               portable=True, portable_context=self.context)
            key.assert_not_called()
        with (
            mock.patch.object(self.context, "validate_selected", side_effect=ValueError("foreign selected source")),
            mock.patch.object(self.workspace, "_profile_source_view") as view,
            mock.patch.object(self.workspace, "_prepare_immutable_client_source_view") as immutable_view,
            mock.patch.object(self.workspace, "_mutable_cmake_source_view") as mutable_view,
        ):
            with self.assertRaisesRegex(WorkspaceError, "foreign selected source"):
                self.workspace._build_client(self.root, {}, False, component=object(),
                                             portable=True, portable_context=self.context)
            view.assert_not_called()
            immutable_view.assert_not_called()
            mutable_view.assert_not_called()
        with (
            mock.patch.object(self.context, "validate_source_binding", side_effect=ValueError("foreign staged source")),
            mock.patch.object(self.workspace, "_prepare_cmake_binary") as prepare,
        ):
            with self.assertRaisesRegex(WorkspaceError, "foreign staged source"):
                self.workspace._cmake(self.source, self.binary, [], False,
                                      portable=True, portable_context=self.context)
            prepare.assert_not_called()

    def test_client_binds_actual_views_and_forwards_same_context(self):
        component = SimpleNamespace(name="classic-client", checkout_name="classic", source_includes=[])
        self.workspace.manifest = SimpleNamespace(components=[])
        views = {name: self.root / name for name in ("client", "protocol", "libatrinik")}
        with ExitStack() as stack:
            for name in ("_prepare_component_source_includes", "_source_view_link", "_record_classic_graph"):
                stack.enter_context(mock.patch.object(self.workspace, name))
            stack.enter_context(mock.patch.object(self.workspace, "_profile_source_view", return_value=views["client"]))
            stack.enter_context(mock.patch.object(self.workspace, "_mutable_cmake_source_view", side_effect=lambda root, name, path: views[name]))
            stack.enter_context(mock.patch.object(self.workspace, "_classic_identity_arguments", return_value=[]))
            stack.enter_context(mock.patch.object(linux_portable, "portable_configure_arguments", return_value=[]))
            cmake = stack.enter_context(mock.patch.object(self.workspace, "_cmake"))
            self.workspace._build_client(self.root, {**views, "sound": self.root / "sound"}, False,
                                         component=component, portable=True, portable_context=self.context)
        self.assertEqual(self.events, [("sources", self.root, views)])
        self.assertIs(cmake.call_args.kwargs["portable_context"], self.context)

    def test_resolved_forwards_context_under_original_build_lock(self):
        component = SimpleNamespace(name="classic-client")
        self.workspace.manifest = SimpleNamespace(stack=lambda name: SimpleNamespace(name="classic", providers={"client": component}))
        locked = []
        @contextmanager
        def lock(*args):
            locked.append(True)
            yield
            locked.pop()
        with ExitStack() as stack:
            for name in ("_guard_recovered_resource", "_guard_retained_build", "_refresh_build_metadata"):
                stack.enter_context(mock.patch.object(self.workspace, name))
            stack.enter_context(mock.patch.object(self.workspace, "_profile_build_key", return_value="key"))
            stack.enter_context(mock.patch.object(self.workspace, "_load_profile", return_value={"stack": "classic"}))
            stack.enter_context(mock.patch.object(self.workspace, "_profile_build_lock", side_effect=lock))
            stack.enter_context(mock.patch.object(self.workspace, "_uses_integrated_classic_build", return_value=False))
            stack.enter_context(mock.patch.object(self.workspace, "_gpu_shader_external_record", return_value={}))
            def build(*args, **kwargs):
                self.assertEqual(locked, [True])
                self.assertIs(kwargs["portable_context"], self.context)
            client = stack.enter_context(mock.patch.object(self.workspace, "_build_client", side_effect=build))
            self.workspace._build_resolved("client", "classic", False, ["client"], {"client": self.source},
                                           portable=True, portable_context=self.context)
            client.assert_called_once()
