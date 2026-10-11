from __future__ import annotations

from contextlib import ExitStack, contextmanager
from pathlib import Path
from types import SimpleNamespace
import tempfile
import threading
import unittest
from unittest import mock

from atrinik_workspace import linux_portable
from atrinik_workspace.model import WorkspaceError, atomic_json
from atrinik_workspace.workspace import CONFIGURE_METADATA, Workspace


class RecordingContext:
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

    def cmake_patches(self, fail=None):
        stack = ExitStack()
        self.addCleanup(stack.close)
        fingerprint = {"build_tree_identity": {"toolchain_file": None}, "source": {"configure_skip_safe": True}}
        stack.enter_context(mock.patch.object(self.workspace, "_add_debug_prefix_environment", return_value={}))
        stack.enter_context(mock.patch.object(self.workspace, "_configure_fingerprint", return_value=fingerprint))
        stack.enter_context(mock.patch.object(self.workspace, "_cmake_state_valid", return_value=True))
        stack.enter_context(mock.patch.object(linux_portable, "installed_metadata", return_value=({}, {"compiler-evidence.json": {}})))
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
