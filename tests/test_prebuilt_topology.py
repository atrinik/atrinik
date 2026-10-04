# Copyright 2026 The Atrinik Project
from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, contextmanager, redirect_stdout
import json
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock

from atrinik_workspace import prebuilt, workspace as implementation
from atrinik_workspace.model import MANAGED_MARKER, WorkspaceError, atomic_json, load_json
from atrinik_workspace.workspace import Workspace
from tests import test_workspace as fixtures


class PrebuiltTopologyTests(unittest.TestCase):
    """Real plans, receipts and publication; synthetic compiler output and services."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.wrapper = self.root / "wrapper"
        self.wrapper.mkdir()
        shutil.copyfile(Path(__file__).parents[1] / "components.json", self.wrapper / "components.json")
        self.git_init(self.wrapper)
        (self.wrapper / ".gitignore").write_text("/*/\n")
        self.environment = mock.patch.dict(os.environ, {
            "ATRINIK_WORKSPACE_DIR": str(self.root / "workspace"),
            "PYTHONDONTWRITEBYTECODE": "1",
        })
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.workspace = Workspace(self.wrapper)
        self.addCleanup(self.workspace.close)
        self.workspace.paths.ensure()
        profile = self.workspace._load_profile("classic", require_file=False)
        stack = self.workspace.manifest.stack("classic")
        roles = self.workspace._dependency_roles(profile, {"client", "server"})
        self.selected = {}
        checkouts = {}
        for role in roles:
            component = stack.providers[role]
            checkout = self.workspace._selector_root(profile, component)
            source = checkout / component.source
            source.mkdir(parents=True, exist_ok=True)
            (source / "README").write_text(role + "\n")
            self.selected[role] = source
            checkouts[checkout] = component
            for include in component.source_includes:
                path = checkout / include
                if path.suffix:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("{}\n")
                else:
                    path.mkdir(parents=True, exist_ok=True)
                    (path / "fixture.cmake").write_text("# fixture\n")
        classic = self.selected["client"].parent
        (classic / "CMakeLists.txt").write_text("project(fixture NONE)\n")
        (classic / "cmake" / "AtrinikVersion.cmake").write_text('set(ATRINIK_DEVELOPMENT_VERSION "5.1.0")\n')
        server = self.selected["server"]
        (server / "tools").mkdir()
        (server / "tools" / "tool.py").write_text("# tool\n")
        for name in ("ca-bundle.crt", "server.cfg", "permissions.cfg"):
            (server / name).write_text("fixture\n")
        install = server / "install_data"
        (install / "keys").mkdir(parents=True)
        (install / "unique-items").mkdir()
        for name in ("bans", "motd", "keys/test.pub", "unique-items/.keep"):
            (install / name).write_text("\n")
        for checkout, component in checkouts.items():
            self.git_init(checkout)
            self.git(checkout, "remote", "add", "origin", f"https://github.com/{component.repository}.git")
            self.commit(checkout)
        self.commit(self.wrapper)
        self.workspace.create_profile("prepared-profile", "classic")
        self.started = []
        self.addCleanup(self.stop_services)

    def git(self, root, *arguments):
        return subprocess.check_output(["git", "-C", str(root), *arguments], stderr=subprocess.DEVNULL, text=True).strip()

    def git_init(self, root):
        self.git(root, "init", "-b", "main")
        self.git(root, "config", "user.name", "Fixture")
        self.git(root, "config", "user.email", "fixture@example.invalid")

    def commit(self, root):
        self.git(root, "add", ".")
        self.git(root, "commit", "-m", "test: fixture")

    def stop_services(self):
        for name in self.started:
            self.workspace.topology_down(name, timeout=5)

    def materialize_compiler_output(self, root, selected, tests, **kwargs):
        directory = root / "build" / "integrated"
        directory.mkdir(parents=True, exist_ok=True)
        atomic_json(directory / implementation.CONFIGURE_METADATA, {
            "schema_version": implementation.CONFIGURE_SCHEMA_VERSION,
            "purpose": "cmake-configure", "build_testing": tests,
            "ccache": None, "compilers": {"c": {"version": "CPU fixture compiler"}},
        })
        for role, name in (("client", "atrinik"), ("server", "atrinik-server")):
            binary = directory / role
            binary.mkdir(exist_ok=True)
            executable = binary / name
            text = f"#!{sys.executable}\nimport time\n"
            if role == "server":
                text += f"print('QUIC certificate SHA-256: {'a' * 64}', flush=True)\n"
                text += "print('Server ready. Waiting for connections...', flush=True)\n"
            text += "while True: time.sleep(0.1)\n"
            executable.write_text(text)
            executable.chmod(0o700)
        for name in ("libplugin_arena.so", "libplugin_python.so"):
            (directory / "server" / name).write_text("bundled plugin\n")
        self.workspace._record_classic_graph(root, {"client", "server"}, "integrated")

    def materialize_content(self, root, *_):
        for name in ("lib", "maps"):
            directory = root / "runtime" / "content" / name
            directory.mkdir(parents=True, exist_ok=True)
            (directory / "payload").write_text("generated content\n")

    def materialize_resources(self, root, *_):
        directory = root / "runtime" / "resources"
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "asset").write_text("generated resource\n")

    def materialize_region_maps(self, root, *_):
        output = root / "runtime/client-maps"
        if not output.exists():
            fixtures.WorkspaceTests.make_region_map_cache(root)

    @contextmanager
    def producer(self):
        with (
            mock.patch.object(self.workspace, "_prepare_gpu_shader", return_value=None),
            mock.patch.object(self.workspace, "_collect_content", side_effect=self.materialize_content),
            mock.patch.object(self.workspace, "_stage_resources", side_effect=self.materialize_resources),
            mock.patch.object(self.workspace, "_build_integrated_classic", side_effect=self.materialize_compiler_output),
            mock.patch.object(self.workspace, "_generate_region_maps", side_effect=self.materialize_region_maps),
        ):
            yield

    def build(self):
        plan = self.workspace.build_plan("topology", "prepared-profile", False, use_ccache=False)
        with self.producer():
            root = self.workspace.build("topology", "prepared-profile", False, use_ccache=False,
                                        expected_plan=plan["plan_sha256"])
        self.assertRegex(self.workspace.prebuilt_build_digest, r"^[0-9a-f]{64}$")
        return root, self.workspace.prebuilt_build_digest

    @contextmanager
    def no_toolchain(self):
        with ExitStack() as stack:
            for method in ("_build_resolved", "_cmake", "_tool_identity", "_compiler_supports_prefix_maps"):
                stack.enter_context(mock.patch.object(self.workspace, method, side_effect=AssertionError("consumer invoked " + method)))
            stack.enter_context(mock.patch.object(self.workspace, "_require_client_display"))
            yield

    def up(self, digest, name="prepared"):
        with self.no_toolchain():
            result = self.workspace.topology_up(name, "prepared-profile", None, state_mode="temporary", prebuilt_build=digest)
        self.started.append(name)
        return result

    def test_fenced_build_then_toolchain_free_public_up_publishes_independent_bytes(self):
        root, digest = self.build()
        before = prebuilt.load(root, digest)
        result = self.up(digest)
        self.assertTrue(result["ready"])
        runtime = Path(result["runtime"]["path"])
        self.assertEqual((runtime / "server/libplugin_arena.so").read_text(), "bundled plugin\n")
        self.assertEqual((runtime / "server/resources/asset").read_text(), "generated resource\n")
        self.assertEqual(prebuilt.load(root, digest), before)
        (root / "runtime/resources/asset").write_text("later writer\n")
        self.assertEqual((runtime / "server/resources/asset").read_text(), "generated resource\n")

    def test_plan_only_and_failed_first_build_publish_no_completion(self):
        plan = self.workspace.build_plan("topology", "prepared-profile", use_ccache=False)
        root = Path(plan["build_root"])
        self.assertFalse((root / prebuilt.RECEIPT_NAME).exists())
        with self.producer(), mock.patch.object(self.workspace, "_generate_region_maps", side_effect=WorkspaceError("failed producer")):
            with self.assertRaisesRegex(WorkspaceError, "failed producer"):
                self.workspace.build("topology", "prepared-profile", False, use_ccache=False, expected_plan=plan["plan_sha256"])
        self.assertFalse((root / prebuilt.RECEIPT_NAME).exists())
        self.assertIsNone(self.workspace.prebuilt_build_digest)
        with self.no_toolchain(), mock.patch.object(self.workspace, "_reserve_topology_port") as port:
            with self.assertRaises(WorkspaceError):
                self.up("a" * 64)
        port.assert_not_called()

    def test_every_rebuild_invalidates_before_failure(self):
        root, digest = self.build()
        for options in ({}, {"build_services": {"client"}}, {"force_reconfigure": True}):
            if not (root / prebuilt.RECEIPT_NAME).exists():
                root, digest = self.build()
            def fail(*_args, **_kwargs):
                self.assertFalse((root / prebuilt.RECEIPT_NAME).exists())
                raise WorkspaceError("writer failed")
            with mock.patch.object(self.workspace, "_prepare_sound", side_effect=fail):
                with self.assertRaisesRegex(WorkspaceError, "writer failed"):
                    self.workspace._build_resolved("topology", "prepared-profile", False, ["client", "server"], self.selected, **options)
            self.assertFalse((root / prebuilt.RECEIPT_NAME).exists())

    def test_changed_runtime_inputs_reject_before_ports_without_reset(self):
        root, digest = self.build()
        candidates = [root / "build/integrated/client/atrinik", root / "build/integrated/server/libplugin_arena.so",
                      root / "runtime/resources/asset", root / "runtime/content/maps/payload",
                      self.selected["sound"] / "README"]
        for candidate in candidates:
            with self.subTest(path=candidate):
                original = candidate.read_bytes()
                candidate.write_bytes(original + b"changed")
                try:
                    with mock.patch.object(self.workspace, "_reserve_topology_port") as port, mock.patch.object(implementation, "managed_reset") as reset:
                        with self.assertRaises(WorkspaceError):
                            self.up(digest)
                    port.assert_not_called()
                    reset.assert_not_called()
                finally:
                    candidate.write_bytes(original)
        self.assertTrue((root / prebuilt.RECEIPT_NAME).exists())

    def test_source_profile_and_dirty_wrapper_drift_reject(self):
        root, digest = self.build()
        for path in (self.selected["client"] / "README", self.wrapper / "dirty-wrapper.py"):
            original = path.read_bytes() if path.exists() else None
            path.write_text("changed\n")
            try:
                with self.assertRaises(WorkspaceError):
                    self.up(digest)
            finally:
                path.unlink() if original is None else path.write_bytes(original)
        profile = self.workspace._load_profile("prepared-profile", require_file=False)
        profile["sound_mode"] = "local-playtest"
        atomic_json(self.workspace.paths.profiles / "prepared-profile.json", profile)
        with self.assertRaises(WorkspaceError):
            self.up(digest)

    def test_dirty_wrapper_cannot_publish_completion(self):
        (self.wrapper / "dirty.py").write_text("# uncommitted wrapper\n")
        with self.assertRaisesRegex(WorkspaceError, "clean unchanged wrapper"):
            self.build()
        self.assertIsNone(self.workspace.prebuilt_build_digest)

    def test_unsupported_launch_modes_reject_without_building(self):
        for options in ({"services": ["client"]}, {"retained_build_plan": "b" * 64}, {"build_services": {"client"}}, {"runtime_handoff": "handoff"}):
            with self.subTest(options=options), self.no_toolchain(), self.assertRaises(WorkspaceError):
                self.workspace.topology_up("unsupported", "prepared-profile", None, prebuilt_build="a" * 64, **options)

    def test_publication_holds_build_lock_and_source_leases(self):
        root, digest = self.build()
        entered, attempted = threading.Event(), threading.Event()
        original = self.workspace._publish_runtime_generation
        future = None
        with ThreadPoolExecutor(max_workers=1) as executor:
            def writer():
                attempted.set()
                with self.workspace._profile_build_lock(root, "prepared-profile"):
                    entered.set()
                    prebuilt.invalidate(root)
            def publish(*args, **kwargs):
                nonlocal future
                future = executor.submit(writer)
                self.assertTrue(attempted.wait(2))
                self.assertFalse(entered.wait(0.05))
                self.assertGreaterEqual(len(implementation.active_lock_fds()), 2)
                result = original(*args, **kwargs)
                self.assertFalse(entered.is_set())
                return result
            with mock.patch.object(self.workspace, "_publish_runtime_generation", side_effect=publish):
                result = self.up(digest)
            future.result(timeout=5)
        self.assertTrue(result["ready"])
        self.assertFalse((root / prebuilt.RECEIPT_NAME).exists())

    def test_public_cli_plan_and_execution_report_exact_receipt(self):
        from atrinik_workspace.cli import main
        output = io.StringIO()
        with mock.patch("atrinik_workspace.cli.Workspace", return_value=self.workspace), mock.patch.object(self.workspace, "close"):
            with redirect_stdout(output):
                result = main(["build", "topology", "--profile", "prepared-profile", "--no-ccache", "--plan", "--json"])
            self.assertEqual(result, 0)
            plan = json.loads(output.getvalue())
            self.assertNotIn("prebuilt_build", plan)
            self.assertFalse((Path(plan["build_root"]) / prebuilt.RECEIPT_NAME).exists())
            output = io.StringIO()
            with self.producer(), redirect_stdout(output):
                result = main(["build", "topology", "--profile", "prepared-profile", "--no-ccache", "--expected-plan", plan["plan_sha256"], "--json"])
            self.assertEqual(result, 0)
            produced = json.loads(output.getvalue())
        receipt = prebuilt.load(Path(produced["build_root"]), produced["prebuilt_build"])
        self.assertEqual(receipt["plan"], plan)
        self.assertFalse(receipt["plan"]["use_ccache"])

    def test_provisioning_runtime_writer_invalidates_completion_under_build_lock(self):
        root, digest = self.build()
        state = self.root / "provisioning-state"
        state.mkdir()
        with self.workspace._profile_build_lock(root, "prepared-profile"):
            self.workspace._prepare_server_runtime(root, self.selected, state, "fixture")
        self.assertFalse((root / prebuilt.RECEIPT_NAME).exists())
        with self.assertRaises(WorkspaceError):
            self.up(digest)

    def test_late_receipt_invalidation_rejects_before_port_reservation(self):
        root, digest = self.build()
        original = self.workspace._create_temporary_state
        def replace(*args, **kwargs):
            state = original(*args, **kwargs)
            with self.workspace._profile_build_lock(root, "prepared-profile"):
                prebuilt.invalidate(root)
            return state
        with mock.patch.object(self.workspace, "_create_temporary_state", side_effect=replace), mock.patch.object(self.workspace, "_reserve_topology_port") as port:
            with self.assertRaises(WorkspaceError):
                self.up(digest)
        port.assert_not_called()

    def test_source_commit_drift_during_staging_rejects_before_publication(self):
        _root, digest = self.build()
        original = self.workspace._seal_runtime_generation
        changed = False
        def advance(path):
            nonlocal changed
            marker = path / MANAGED_MARKER
            if marker.is_file() and load_json(marker).get("purpose") == "immutable-runtime-generation":
                self.git(self.selected["client"].parent, "commit", "--allow-empty", "-m", "test: concurrent source commit")
                changed = True
            original(path)
        with mock.patch.object(self.workspace, "_seal_runtime_generation", side_effect=advance):
            with self.assertRaisesRegex(WorkspaceError, "prebuilt sources"):
                self.up(digest)
        self.assertTrue(changed)
        topology = self.workspace.paths.topologies / "prepared"
        self.assertFalse((topology / "spec.json").exists())
        self.assertEqual(list((topology / "generations").iterdir()), [])

    def test_server_test_fixtures_are_excluded_from_prebuilt_normal_and_selective_launch(self):
        root, digest = self.build()
        binary = root / "build/integrated/server"
        for name in ("server-test-runtime-seed", "server-test-runtimes"):
            directory = binary / name
            directory.mkdir()
            (directory / "fixture").symlink_to("/missing-test-only-input")
        result = self.up(digest)
        for name in ("server-test-runtime-seed", "server-test-runtimes"):
            self.assertFalse((Path(result["runtime"]["path"]) / "server" / name).exists())
        for name, options in (("normal", {}), ("selective", {"build_services": {"client"}})):
            with mock.patch.object(self.workspace, "_build_resolved", return_value=root), mock.patch.object(self.workspace, "_require_client_display"):
                status = self.workspace.topology_up(name, "prepared-profile", None, state_mode="temporary", **options)
            self.started.append(name)
            self.assertTrue(status["ready"])
            self.assertFalse((Path(status["runtime"]["path"]) / "server/server-test-runtimes").exists())
        (binary / "production-library.so").symlink_to("/must-not-follow")
        with self.assertRaises(WorkspaceError):
            self.up(digest, name="unsafe-production-link")
