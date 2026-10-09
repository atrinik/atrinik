from __future__ import annotations

from contextlib import contextmanager
import json
import stat
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError, atomic_json
from atrinik_workspace import scenario_route


ROUTE = (
    b'<live-movement-route version="1" timeout-ms="1000" step-timeout-ms="100">'
    b'<checkpoint map="/shattered_islands/world_0_70" x="20" y="8" direction="0"/>'
    b'<checkpoint map="/shattered_islands/world_2_67" x="7" y="23" direction="1"/>'
    b'</live-movement-route>'
)


class Provider:
    name = "classic-server"
    checkout_name = "classic"
    repository = "atrinik/atrinik"
    branch = "master"
    source = "server"


class Stack:
    providers = {"server": Provider()}


class Manifest:
    @staticmethod
    def stack(name):
        if name != "classic":
            raise AssertionError(name)
        return Stack()


class Snapshot:
    generation = "f" * 64

    def __init__(self, source: Path, dirty: bool = False):
        self.source = source
        self.dirty = dirty

    @staticmethod
    def profile():
        return {"stack": "classic"}

    def paths(self):
        return {"server": self.source}

    def checkout_states(self):
        return {
            "classic": {
                "path": self.source.parent,
                "head": "a" * 40,
                "dirty": self.dirty,
            }
        }


class Paths:
    def __init__(self, root):
        self.builds = root / "build"

    def ensure(self):
        pass


class Workspace:
    manifest = Manifest()

    def __init__(self, root: Path, dirty: bool = False):
        self.root = root
        self.paths = Paths(root)
        self.source = root / "classic" / "server"
        (self.source / "install_data").mkdir(parents=True)
        (self.source / "install_data" / "settings").write_text("fresh\n")
        self.scenario_state = root / "scenario-state"
        self.scenario_state.mkdir()
        (self.scenario_state / "sentinel").write_text("unchanged\n")
        self.password = root / "password"
        self.password.write_text("unchanged\n")
        self.snapshot = Snapshot(self.source, dirty)
        self.prepared_state = None

    def _load_scenario(self, name):
        return {
            "name": name,
            "profile": "classic",
            "preset": "basic-player",
            "state": f"scenario-{name}",
            "stack": "classic",
            "providers": {"server": "classic-server"},
            "resolved": {"server": {"head": "b" * 40, "dirty": False}},
        }

    def _require_classic_contracts(self, profile, roles):
        self.required = (profile, roles)

    @contextmanager
    def _resolved_profile_operation(self, profile, roles, operation):
        self.operation = (profile, roles, operation)
        yield self.snapshot

    @staticmethod
    def _lease_request(kind, name, mode, operation):
        return (kind, name, mode, operation)

    @contextmanager
    def _resource_locks(self, requests):
        self.requests = requests
        yield

    def _build_resolved(self, target, profile, tests, targets, selected):
        self.built = (target, profile, tests, targets, selected)
        build = self.root / "build"
        build.mkdir()
        return build

    @contextmanager
    def _profile_build_lock(self, root, profile):
        yield

    @staticmethod
    def _make_tree_owner_writable(path):
        pass

    @staticmethod
    def _validate_state(path):
        if not (path / "tmp").is_dir():
            raise AssertionError("fresh state lacks tmp")

    @staticmethod
    def _server_runtime_coordinate(root, state, state_name):
        key = "fixture-state-key"
        return (
            root / "run" / "server" / f"{state_name}-{key}",
            f"server-runtime:{key}",
        )

    def _prepare_server_runtime(self, root, selected, state, state_name):
        self.prepared_state = state
        self.asserted_fresh = (state / "settings").read_text() == "fresh\n"
        runtime, purpose = self._server_runtime_coordinate(
            root, state, state_name
        )
        (runtime / "assets").mkdir(parents=True)
        atomic_json(
            runtime / ".atrinik-workspace-managed.json",
            {"schema_version": 1, "purpose": purpose},
        )
        return runtime


class ScenarioRouteTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def test_prepare_route_uses_fresh_state_and_publishes_pinned_pair(self):
        workspace = Workspace(self.root)
        output = self.root / "route.xml"
        framed = scenario_route.BEGIN + ROUTE + scenario_route.END
        with mock.patch.object(scenario_route, "run_bounded", return_value=framed) as run:
            record = scenario_route.prepare_route(workspace, "brynknot", output)

        self.assertEqual(output.read_bytes(), ROUTE)
        companion = Path(str(output) + ".provenance.json")
        self.assertEqual(json.loads(companion.read_text()), record)
        self.assertEqual(record["producer"], "brynknot-v1")
        self.assertEqual(record["scenario"], "brynknot")
        self.assertEqual(record["profile"], "classic")
        self.assertEqual(record["profile_generation"], "f" * 64)
        self.assertEqual(record["sources"]["classic-server"]["head"], "a" * 40)
        self.assertEqual(workspace.requests[0][:3], ("scenario", "brynknot", "shared"))
        self.assertEqual((workspace.scenario_state / "sentinel").read_text(), "unchanged\n")
        self.assertEqual(workspace.password.read_text(), "unchanged\n")
        self.assertTrue(workspace.asserted_fresh)
        self.assertFalse(workspace.prepared_state.exists())
        self.assertEqual(
            list((self.root / "build" / "run" / "server").glob("route-*")), []
        )
        arguments, options = run.call_args
        self.assertEqual(arguments[0][1], "--content_benchmark_route=brynknot-v1")
        self.assertEqual(options["timeout"], 60)
        self.assertEqual(options["cwd"], Path(arguments[0][0]).parent)

    def test_rejects_relative_or_existing_output_before_loading_scenario(self):
        workspace = mock.Mock()
        with self.assertRaisesRegex(WorkspaceError, "absolute XML"):
            scenario_route.prepare_route(workspace, "brynknot", Path("route.xml"))
        workspace._load_scenario.assert_not_called()
        output = self.root / "route.xml"
        output.write_bytes(b"owned")
        with self.assertRaisesRegex(WorkspaceError, "already exists"):
            scenario_route.prepare_route(workspace, "brynknot", output)
        self.assertEqual(output.read_bytes(), b"owned")

    def test_rejects_dirty_selected_source_before_build_or_process(self):
        workspace = Workspace(self.root, dirty=True)
        workspace._build_resolved = mock.Mock()
        with self.assertRaisesRegex(WorkspaceError, "clean committed"):
            scenario_route.prepare_route(workspace, "brynknot", self.root / "route.xml")
        workspace._build_resolved.assert_not_called()

    def test_rejects_producer_failure_and_does_not_publish(self):
        workspace = Workspace(self.root)
        output = self.root / "route.xml"
        with mock.patch.object(
            scenario_route,
            "run_bounded",
            side_effect=WorkspaceError("benchmark helper rejected the evidence"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "rejected the evidence"):
                scenario_route.prepare_route(workspace, "brynknot", output)
        self.assertFalse(output.exists())
        self.assertFalse(Path(str(output) + ".provenance.json").exists())

    def test_strict_framing_and_route_validation(self):
        with self.assertRaisesRegex(WorkspaceError, "framing"):
            scenario_route._route_payload(
                b"noise" + scenario_route.BEGIN + ROUTE + scenario_route.END
            )
        with self.assertRaisesRegex(WorkspaceError, "invalid live movement route"):
            scenario_route._route_payload(scenario_route.BEGIN + b"<bad/>" + scenario_route.END)

    def test_output_bound(self):
        oversized = b"x" * (
            scenario_route.MAX_ROUTE_BYTES
            + len(scenario_route.BEGIN)
            + len(scenario_route.END)
            + 1
        )
        with self.assertRaisesRegex(WorkspaceError, "stdout bound"):
            scenario_route._route_payload(oversized)

    def test_second_publication_failure_removes_only_owned_new_output(self):
        output = self.root / "route.xml"
        companion = Path(str(output) + ".provenance.json")
        companion.write_bytes(b"other-owner")
        with self.assertRaisesRegex(WorkspaceError, "already exists"):
            scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertFalse(output.exists())
        self.assertEqual(companion.read_bytes(), b"other-owner")

    def test_success_persists_staging_removal_before_return(self):
        output = self.root / "route.xml"
        companion = Path(str(output) + ".provenance.json")
        real_fsync = scenario_route.os.fsync
        links_at_sync = []

        def observe_directory_sync(descriptor):
            if stat.S_ISDIR(scenario_route.os.fstat(descriptor).st_mode):
                links_at_sync.append((output.stat().st_nlink, companion.stat().st_nlink))
                if len(links_at_sync) == 2:
                    self.assertEqual(set(self.root.iterdir()), {output, companion})
            return real_fsync(descriptor)

        with mock.patch.object(scenario_route.os, "fsync", observe_directory_sync):
            scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertEqual(links_at_sync, [(2, 2), (1, 1)])
        self.assertEqual(set(self.root.iterdir()), {output, companion})

    def test_staging_removal_sync_failure_retracts_publication(self):
        output = self.root / "route.xml"
        companion = Path(str(output) + ".provenance.json")
        real_fsync = scenario_route.os.fsync
        directory_syncs = 0

        def fail_staging_removal_sync(descriptor):
            nonlocal directory_syncs
            if stat.S_ISDIR(scenario_route.os.fstat(descriptor).st_mode):
                directory_syncs += 1
                if directory_syncs == 2:
                    self.assertEqual(output.stat().st_nlink, 1)
                    self.assertEqual(companion.stat().st_nlink, 1)
                    self.assertEqual(set(self.root.iterdir()), {output, companion})
                    raise OSError("staging removal sync failed")
            return real_fsync(descriptor)

        with mock.patch.object(scenario_route.os, "fsync", fail_staging_removal_sync):
            with self.assertRaisesRegex(WorkspaceError, "cannot publish"):
                scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertEqual(directory_syncs, 2)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_failed_publication_removes_route_before_provenance(self):
        output = self.root / "route.xml"
        companion = Path(str(output) + ".provenance.json")
        real_fsync = scenario_route.os.fsync
        real_rename = scenario_route._rename_noreplace
        removed = []

        def fail_directory_sync(descriptor):
            if stat.S_ISDIR(scenario_route.os.fstat(descriptor).st_mode):
                raise OSError("publication sync failed")
            return real_fsync(descriptor)

        def observe_claim(directory, name, claim):
            if name == output.name:
                self.assertEqual(companion.read_bytes(), b"{}\n")
                removed.append("route")
            elif name == companion.name:
                self.assertFalse(output.exists())
                removed.append("provenance")
            return real_rename(directory, name, claim)

        with mock.patch.object(scenario_route.os, "fsync", fail_directory_sync), \
                mock.patch.object(scenario_route, "_rename_noreplace", observe_claim):
            with self.assertRaisesRegex(WorkspaceError, "cannot publish"):
                scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertEqual(removed, ["route", "provenance"])
        self.assertEqual(list(self.root.iterdir()), [])

    def test_failed_or_interrupted_route_removal_retains_provenance(self):
        for index, failure in enumerate((OSError("unlink failed"), KeyboardInterrupt())):
            with self.subTest(failure=type(failure).__name__):
                directory = self.root / str(index)
                directory.mkdir()
                output = directory / "route.xml"
                companion = Path(str(output) + ".provenance.json")
                real_fsync = scenario_route.os.fsync
                real_rename = scenario_route._rename_noreplace

                def fail_directory_sync(descriptor):
                    if stat.S_ISDIR(scenario_route.os.fstat(descriptor).st_mode):
                        raise OSError("publication sync failed")
                    return real_fsync(descriptor)

                def fail_route_removal(directory_fd, name, claim):
                    if name == output.name:
                        raise failure
                    return real_rename(directory_fd, name, claim)

                with mock.patch.object(scenario_route.os, "fsync", fail_directory_sync), \
                        mock.patch.object(scenario_route, "_rename_noreplace", fail_route_removal):
                    with self.assertRaises(type(failure)):
                        scenario_route._publish_pair(output, ROUTE, b"{}\n")
                self.assertEqual(output.read_bytes(), ROUTE)
                self.assertEqual(companion.read_bytes(), b"{}\n")
                self.assertEqual(set(directory.iterdir()), {output, companion})

    def test_replacement_between_stat_and_claim_is_preserved_with_provenance(self):
        for obstruct_restore in (False, True):
            with self.subTest(obstruct_restore=obstruct_restore):
                directory = self.root / str(obstruct_restore)
                directory.mkdir()
                output = directory / "route.xml"
                companion = Path(str(output) + ".provenance.json")
                replacement = directory / "replacement"
                replacement.write_bytes(b"other-owner")
                real_stat = scenario_route.os.stat
                real_fsync = scenario_route.os.fsync
                real_rename = scenario_route._rename_noreplace
                rolling_back = False
                replaced = False

                def fail_directory_sync(descriptor):
                    nonlocal rolling_back
                    if stat.S_ISDIR(scenario_route.os.fstat(descriptor).st_mode):
                        rolling_back = True
                        raise OSError("publication sync failed")
                    return real_fsync(descriptor)

                def replace_after_stat(name, **options):
                    nonlocal replaced
                    result = real_stat(name, **options)
                    if name == output.name and rolling_back and not replaced:
                        replaced = True
                        scenario_route.os.replace(replacement, output)
                    return result

                def obstruct_restoration(directory_fd, source, destination):
                    if obstruct_restore and destination == output.name:
                        output.write_bytes(b"new-owner")
                    return real_rename(directory_fd, source, destination)

                with mock.patch.object(scenario_route.os, "fsync", fail_directory_sync), \
                        mock.patch.object(scenario_route.os, "stat", replace_after_stat), \
                        mock.patch.object(scenario_route, "_rename_noreplace", obstruct_restoration):
                    with self.assertRaisesRegex(WorkspaceError, "replacement"):
                        scenario_route._publish_pair(output, ROUTE, b"{}\n")
                self.assertTrue(replaced)
                self.assertEqual(companion.read_bytes(), b"{}\n")
                if obstruct_restore:
                    self.assertEqual(output.read_bytes(), b"new-owner")
                    claims = list(directory.glob(".route.xml.*.rollback"))
                    self.assertEqual(len(claims), 1)
                    self.assertEqual(claims[0].read_bytes(), b"other-owner")
                else:
                    self.assertEqual(output.read_bytes(), b"other-owner")
                    self.assertEqual(set(directory.iterdir()), {output, companion})

    def test_claim_collision_cannot_replace_existing_entry(self):
        output = self.root / "route.xml"
        output.write_bytes(ROUTE)
        identity = scenario_route._file_identity(output.stat())
        claim = self.root / (".route.xml." + "a" * 32 + ".rollback")
        claim.write_bytes(b"other-owner")
        descriptor = scenario_route.os.open(self.root, scenario_route.os.O_DIRECTORY)
        try:
            with mock.patch.object(scenario_route.secrets, "token_hex", return_value="a" * 32):
                with self.assertRaisesRegex(WorkspaceError, "cannot allocate"):
                    scenario_route._unlink_owned(descriptor, output.name, identity)
        finally:
            scenario_route.os.close(descriptor)
        self.assertEqual(output.read_bytes(), ROUTE)
        self.assertEqual(claim.read_bytes(), b"other-owner")

    def test_route_stat_failure_rolls_back_published_pair(self):
        output = self.root / "route.xml"
        real_stat = scenario_route.os.stat
        failed = False

        def fail_first_route_stat(name, **options):
            nonlocal failed
            if name == output.name and not failed:
                failed = True
                raise OSError("route stat failed")
            return real_stat(name, **options)

        with mock.patch.object(scenario_route.os, "stat", fail_first_route_stat):
            with self.assertRaisesRegex(WorkspaceError, "cannot publish"):
                scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertTrue(failed)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_rollback_preserves_replacement_provenance(self):
        output = self.root / "route.xml"
        companion = Path(str(output) + ".provenance.json")
        replacement = self.root / "replacement"
        replacement.write_bytes(b"other-owner")
        real_fsync = scenario_route.os.fsync

        def replace_before_directory_sync(descriptor):
            if stat.S_ISDIR(scenario_route.os.fstat(descriptor).st_mode):
                companion.unlink()
                companion.symlink_to(replacement.name)
                raise OSError("publication sync failed")
            return real_fsync(descriptor)

        with mock.patch.object(scenario_route.os, "fsync", replace_before_directory_sync):
            with self.assertRaisesRegex(WorkspaceError, "cannot publish"):
                scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertFalse(output.exists())
        self.assertTrue(companion.is_symlink())
        self.assertEqual(companion.read_bytes(), b"other-owner")
        self.assertEqual(replacement.read_bytes(), b"other-owner")

    def test_staging_replacement_cannot_publish_different_route(self):
        output = self.root / "route.xml"
        attacker = self.root / "attacker"
        attacker.write_bytes(b"different")
        real_link = scenario_route.os.link
        replaced = False

        def replace_then_link(source, destination, **options):
            nonlocal replaced
            if not replaced and destination == output.name:
                replaced = True
                directory = options["src_dir_fd"]
                scenario_route.os.unlink(source, dir_fd=directory)
                scenario_route.os.symlink(
                    attacker.name, source, dir_fd=directory
                )
            return real_link(source, destination, **options)

        with mock.patch.object(scenario_route.os, "link", replace_then_link):
            with self.assertRaisesRegex(WorkspaceError, "differs from its staging"):
                scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertTrue(output.is_symlink())
        self.assertFalse(Path(str(output) + ".provenance.json").exists())
        self.assertEqual(attacker.read_bytes(), b"different")

    def test_rollback_preserves_replacement_of_published_name(self):
        output = self.root / "route.xml"
        replacement = self.root / "replacement"
        replacement.write_bytes(b"other-owner")
        real_link = scenario_route.os.link
        replaced = False

        def link_then_replace(source, destination, **options):
            nonlocal replaced
            result = real_link(source, destination, **options)
            if not replaced and destination == output.name:
                replaced = True
                directory = options["dst_dir_fd"]
                scenario_route.os.unlink(destination, dir_fd=directory)
                scenario_route.os.symlink(
                    replacement.name, destination, dir_fd=directory
                )
            return result

        with mock.patch.object(scenario_route.os, "link", link_then_replace):
            with self.assertRaisesRegex(WorkspaceError, "differs from its staging"):
                scenario_route._publish_pair(output, ROUTE, b"{}\n")
        self.assertTrue(output.is_symlink())
        self.assertEqual(output.read_bytes(), b"other-owner")
        self.assertFalse(Path(str(output) + ".provenance.json").exists())


if __name__ == "__main__":
    unittest.main()
