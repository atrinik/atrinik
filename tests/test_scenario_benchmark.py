from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import time
import unittest
from unittest import mock

from atrinik_workspace.model import MANAGED_MARKER, WorkspaceError
from atrinik_workspace import scenario_benchmark as benchmark


ROUTE = b"""<live-movement-route version="1" timeout-ms="1000" step-timeout-ms="250">
  <checkpoint map="/scorn/shops/shop" x="10" y="20" direction="0"/>
  <checkpoint map="/scorn/shops/shop" x="11" y="20" direction="3"/>
</live-movement-route>"""


def write_private(path: Path, data: bytes) -> None:
    path.write_bytes(data)
    path.chmod(0o400)


def valid_summary(route: dict, source: dict) -> dict:
    count = len(route["checkpoints"])
    return {
        "status": "success",
        "route_sha256": route["sha256"],
        "source_revision": source["head"],
        "source_dirty": False,
        "arrivals": count,
        "presented_checkpoints": count,
        "expected_checkpoints": count,
        "frames": 4,
        "presented_frames": 3,
        "gpu_backend": "OpenGL",
        "gpu_device": "fixture GPU",
    }


class RouteTests(unittest.TestCase):
    def test_parse_route_accepts_canonical_route(self) -> None:
        parsed = benchmark.parse_route(ROUTE)
        self.assertEqual(parsed["sha256"], hashlib.sha256(ROUTE).hexdigest())
        self.assertEqual(parsed["timeout_ms"], 1000)
        self.assertEqual([row["direction"] for row in parsed["checkpoints"]], [0, 3])

    def test_parse_route_matches_native_checkpoint_and_map_path_bounds(self) -> None:
        one = ROUTE.replace(b'  <checkpoint map="/scorn/shops/shop" x="11" y="20" direction="3"/>\n', b'')
        with self.assertRaisesRegex(WorkspaceError, "at least two"):
            benchmark.parse_route(one)
        accepted = ROUTE.replace(b'/scorn/shops/shop', b'/' + b'a' * 510)
        self.assertEqual(len(benchmark.parse_route(accepted)["checkpoints"]), 2)
        rejected = ROUTE.replace(b'/scorn/shops/shop', b'/' + b'a' * 511)
        with self.assertRaisesRegex(WorkspaceError, "map path"):
            benchmark.parse_route(rejected)

    def test_parse_route_rejects_unknown_fields_declarations_and_malformed_xml(self) -> None:
        invalid = {
            "root field": ROUTE.replace(b'timeout-ms="1000"', b'timeout-ms="1000" extra="x"'),
            "checkpoint field": ROUTE.replace(b'direction="3"', b'direction="3" extra="x"'),
            "declaration": b'<?xml version="1.0"?>' + ROUTE,
            "doctype": b'<!DOCTYPE route>' + ROUTE,
            "malformed": ROUTE[:-2],
        }
        for label, data in invalid.items():
            with self.subTest(label=label), self.assertRaises(WorkspaceError):
                benchmark.parse_route(data)

    def test_parse_route_rejects_invalid_directions(self) -> None:
        invalid = {
            "initial movement": ROUTE.replace(b'direction="0"', b'direction="1"', 1),
            "stationary later": ROUTE.replace(b'direction="3"', b'direction="0"'),
            "center later": ROUTE.replace(b'direction="3"', b'direction="5"'),
            "out of range": ROUTE.replace(b'direction="3"', b'direction="10"'),
            "noncanonical": ROUTE.replace(b'direction="3"', b'direction="03"'),
        }
        for label, data in invalid.items():
            with self.subTest(label=label), self.assertRaises(WorkspaceError):
                benchmark.parse_route(data)

    def test_parse_route_rejects_noncanonical_paths_and_numeric_bounds(self) -> None:
        invalid = {
            "relative path": ROUTE.replace(b'/scorn/shops/shop', b'scorn/shops/shop', 1),
            "parent path": ROUTE.replace(b'/scorn/shops/shop', b'/scorn/../shop', 1),
            "double slash": ROUTE.replace(b'/scorn/shops/shop', b'/scorn//shop', 1),
            "trailing slash": ROUTE.replace(b'/scorn/shops/shop', b'/scorn/shop/', 1),
            "coordinate": ROUTE.replace(b'x="11"', b'x="256"'),
            "leading zero": ROUTE.replace(b'x="11"', b'x="011"'),
            "zero timeout": ROUTE.replace(b'timeout-ms="1000"', b'timeout-ms="0"'),
            "step timeout": ROUTE.replace(b'step-timeout-ms="250"', b'step-timeout-ms="60001"'),
        }
        for label, data in invalid.items():
            with self.subTest(label=label), self.assertRaises(WorkspaceError):
                benchmark.parse_route(data)

    def test_read_regular_rejects_symlinks_links_unsafe_modes_and_bounds(self) -> None:
        with tempfile.TemporaryDirectory(dir="/tmp") as temporary:
            root = Path(temporary)
            source = root / "source"
            source.write_bytes(b"fixture")
            source.chmod(0o600)
            self.assertEqual(benchmark.read_regular(source, 7), b"fixture")
            with self.assertRaises(WorkspaceError):
                benchmark.read_regular(source, 6)

            linked = root / "linked"
            linked.symlink_to(source)
            with self.assertRaises(WorkspaceError):
                benchmark.read_regular(linked, 7)

            hardlink = root / "hardlink"
            os.link(source, hardlink)
            with self.assertRaises(WorkspaceError):
                benchmark.read_regular(source, 7)
            hardlink.unlink()

            source.chmod(0o622)
            with self.assertRaises(WorkspaceError):
                benchmark.read_regular(source, 7)


class LaunchArgumentTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(dir="/tmp")
        self.root = Path(self.temporary.name) / "topology"
        self.root.mkdir(mode=0o700)
        self.evidence = self.root / "benchmark"
        self.evidence.mkdir(mode=0o700)
        self.route = self.evidence / "route.xml"
        self.report = self.evidence / "frames.jsonl"
        write_private(self.route, ROUTE)
        self.value = {
            "scenario": "brynknot",
            "nonce": "a" * 32,
            "route_sha256": hashlib.sha256(ROUTE).hexdigest(),
            "route": str(self.route),
            "report": str(self.report),
        }

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_launch_arguments_returns_only_owned_route_and_report(self) -> None:
        self.assertEqual(
            benchmark.launch_arguments(self.value, self.root, "scenario-brynknot"),
            ["--live-movement-route", str(self.route), "--live-movement-report", str(self.report)],
        )

    def test_launch_arguments_requires_exact_fields_private_paths_and_digest(self) -> None:
        mutations = {}
        missing = dict(self.value)
        missing.pop("nonce")
        mutations["missing field"] = (missing, self.root, "scenario-brynknot")
        extra = dict(self.value, arbitrary="value")
        mutations["extra field"] = (extra, self.root, "scenario-brynknot")
        wrong_type = dict(self.value, nonce=1)
        mutations["wrong type"] = (wrong_type, self.root, "scenario-brynknot")
        bad_nonce = dict(self.value, nonce="A" * 32)
        mutations["nonce"] = (bad_nonce, self.root, "scenario-brynknot")
        bad_digest = dict(self.value, route_sha256="0" * 64)
        mutations["digest"] = (bad_digest, self.root, "scenario-brynknot")
        outside = dict(self.value, route=str(self.root / "route.xml"))
        mutations["route path"] = (outside, self.root, "scenario-brynknot")
        outside_report = dict(self.value, report=str(self.root / "report.jsonl"))
        mutations["report path"] = (outside_report, self.root, "scenario-brynknot")
        mutations["state"] = (self.value, self.root, "scenario-other")
        for label, (value, root, state) in mutations.items():
            with self.subTest(label=label), self.assertRaises(WorkspaceError):
                benchmark.launch_arguments(value, root, state)

        self.evidence.chmod(0o755)
        with self.assertRaises(WorkspaceError):
            benchmark.launch_arguments(self.value, self.root, "scenario-brynknot")

    def test_launch_arguments_refuses_preexisting_report_even_if_symlink(self) -> None:
        self.report.symlink_to(self.route)
        with self.assertRaises(WorkspaceError):
            benchmark.launch_arguments(self.value, self.root, "scenario-brynknot")


class RouteProvenanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(dir="/tmp")
        self.root = Path(self.temporary.name)
        self.route = self.root / "route.xml"
        write_private(self.route, ROUTE)
        self.expected = benchmark.parse_route(ROUTE)
        self.scenario = {"name": "brynknot", "profile": "classic"}
        self.source = {
            "path": "/fixture/client",
            "checkout_path": "/fixture/client",
            "checkout": "classic",
            "repository": "atrinik/client",
            "branch": "main",
            "source": "client",
            "head": "1" * 40,
            "dirty": False,
        }
        self.value = {
            "schema_version": 1,
            "producer": "brynknot-v1",
            "scenario": "brynknot",
            "profile": "classic",
            "profile_generation": "2" * 64,
            "route_sha256": self.expected["sha256"],
            "output": str(self.route),
            "sources": {"client": self.source},
        }

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_provenance(self, value: dict) -> None:
        companion = self.route.with_name(self.route.name + ".provenance.json")
        write_private(companion, json.dumps(value).encode())

    def test_route_provenance_accepts_absent_or_exact_companion(self) -> None:
        self.assertEqual(
            benchmark.route_provenance(self.route, self.expected, self.scenario),
            {"producer": "user-supplied"},
        )
        self.write_provenance(self.value)
        self.assertEqual(
            benchmark.route_provenance(self.route, self.expected, self.scenario),
            self.value,
        )

    def test_route_provenance_rejects_stale_identity_and_unknown_fields(self) -> None:
        invalid = {
            "sha": dict(self.value, route_sha256="0" * 64),
            "scenario": dict(self.value, scenario="other"),
            "profile": dict(self.value, profile="default"),
            "output": dict(self.value, output=str(self.root / "other.xml")),
            "unknown": dict(self.value, extra="field"),
            "dirty source": dict(self.value, sources={"client": dict(self.source, dirty=True)}),
            "source hash": dict(self.value, sources={"client": dict(self.source, head="invalid")}),
        }
        companion = self.route.with_name(self.route.name + ".provenance.json")
        for label, value in invalid.items():
            with self.subTest(label=label):
                if companion.exists():
                    companion.chmod(0o600)
                self.write_provenance(value)
                with self.assertRaisesRegex(WorkspaceError, "invalid benchmark route provenance"):
                    benchmark.route_provenance(self.route, self.expected, self.scenario)

    def test_validate_source_provenance_rejects_selected_source_drift(self) -> None:
        topology = self.root / "topology"
        evidence = topology / "benchmark"
        evidence.mkdir(parents=True)
        summary = evidence / "summary.json"
        write_private(summary, json.dumps({"route_origin": self.value}).encode())
        benchmark.validate_source_provenance(topology, {"client": self.source})
        with self.assertRaisesRegex(WorkspaceError, "selected source generation"):
            benchmark.validate_source_provenance(
                topology,
                {"client": dict(self.source, head="3" * 40)},
            )


class BoundedProcessTests(unittest.TestCase):
    def test_run_bounded_returns_stdout_and_drains_stderr(self) -> None:
        output = benchmark.run_bounded(
            [sys.executable, "-I", "-c", "import sys; print('accepted'); sys.stderr.write('diagnostic')"],
            timeout=5,
            limit=64,
        )
        self.assertEqual(output.strip(), b"accepted")

    def test_run_bounded_rejects_combined_output_over_bound(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "output exceeded"):
            benchmark.run_bounded(
                [sys.executable, "-I", "-c", "import sys; sys.stderr.write('x' * 65)"],
                timeout=5,
                limit=64,
            )

    def test_run_bounded_rejects_nonzero_exit(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "rejected"):
            benchmark.run_bounded([sys.executable, "-I", "-c", "raise SystemExit(3)"], timeout=5, limit=64)

    def test_run_bounded_kills_child_at_deadline(self) -> None:
        started = time.monotonic()
        with self.assertRaisesRegex(WorkspaceError, "deadline"):
            benchmark.run_bounded(
                [sys.executable, "-I", "-c", "import time; time.sleep(5)"],
                timeout=0.1,
                limit=64,
            )
        self.assertLess(time.monotonic() - started, 2)


class VerifyReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(dir="/tmp")
        self.root = Path(self.temporary.name)
        self.route = self.root / "route.xml"
        self.report = self.root / "frames.jsonl"
        self.verifier = self.root / "verify.py"
        write_private(self.route, ROUTE)
        self.report.write_text('{"frame":1}\n', encoding="utf-8")
        self.report.chmod(0o600)
        self.expected = benchmark.parse_route(ROUTE)
        self.source = {"head": "1" * 40, "dirty": False}

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_verify_report_accepts_native_core_summary(self) -> None:
        summary = valid_summary(self.expected, self.source)
        with mock.patch.object(benchmark, "run_bounded", return_value=json.dumps(summary).encode()):
            self.assertEqual(
                benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source),
                summary,
            )

    def test_verify_report_rejects_malformed_and_incomplete_zero_exit_summary(self) -> None:
        self.verifier.write_text("print('{}')\n", encoding="utf-8")
        self.verifier.chmod(0o400)
        with self.assertRaisesRegex(WorkspaceError, "identity or complete"):
            benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source)

        with mock.patch.object(benchmark, "run_bounded", return_value=b"not JSON"):
            with self.assertRaisesRegex(WorkspaceError, "invalid benchmark verifier summary"):
                benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source)

    def test_verify_report_relies_on_strict_native_verifier_for_terminal_record(self) -> None:
        self.verifier.write_text(
            "import pathlib, sys\n"
            "report = pathlib.Path(sys.argv[2]).read_text(encoding='utf-8')\n"
            "raise SystemExit(0 if '\\\"terminal\\\"' in report else 4)\n",
            encoding="utf-8",
        )
        self.verifier.chmod(0o400)
        with self.assertRaisesRegex(WorkspaceError, "rejected"):
            benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source)

    def test_verify_report_rejects_source_digest_count_and_hardware_mismatch(self) -> None:
        base = valid_summary(self.expected, self.source)
        invalid = {
            "source revision": dict(base, source_revision="2" * 40),
            "source dirty": dict(base, source_dirty=True),
            "route digest": dict(base, route_sha256="0" * 64),
            "count": dict(base, arrivals=1),
            "boolean count": dict(base, arrivals=True),
            "frames": dict(base, presented_frames=0),
            "backend": dict(base, gpu_backend=""),
            "device": dict(base, gpu_device=None),
        }
        for label, summary in invalid.items():
            with self.subTest(label=label):
                with mock.patch.object(benchmark, "run_bounded", return_value=json.dumps(summary).encode()):
                    with self.assertRaisesRegex(WorkspaceError, "identity or complete"):
                        benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source)

    def test_verify_report_rejects_route_changed_from_expected_digest(self) -> None:
        different = ROUTE.replace(b'x="11"', b'x="12"')
        self.route.chmod(0o600)
        write_private(self.route, different)
        summary = valid_summary(self.expected, self.source)
        with mock.patch.object(benchmark, "run_bounded", return_value=json.dumps(summary).encode()):
            with self.assertRaisesRegex(WorkspaceError, "route changed"):
                benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source)


class FakePaths:
    def __init__(self, workspace: Path) -> None:
        self.workspace = workspace
        self.topologies = workspace / "topologies"

    def ensure(self) -> None:
        self.workspace.mkdir(mode=0o700, exist_ok=True)
        self.topologies.mkdir(mode=0o700, exist_ok=True)


class FakeWorkspace:
    def _guard_recovered_resource(self, kind, root, name):
        pass

    def __init__(self, root: Path, *, create_report: bool = True, dirty: bool = False) -> None:
        self.paths = FakePaths(root / "workspace")
        self.paths.ensure()
        self.client = root / "client"
        (self.client / "tools").mkdir(parents=True)
        write_private(self.client / "tools" / "verify_live_movement.py", b"print('{}')\n")
        self.create_report = create_report
        self.dirty = dirty
        self.statuses: list[dict] = []
        self.launch = None
        self.component_calls: list[tuple[str, str]] = []
        self.down_calls: list[tuple[str, str | None]] = []

    def _load_scenario(self, name: str) -> dict:
        return {
            "name": name,
            "profile": "classic",
            "preset": "brynknot-idle",
            "state": "scenario-" + name,
            "password": "fixture-password-must-not-be-durable",
        }

    def _require_classic_contracts(self, profile: str, roles: set[str]) -> None:
        if (profile, roles) != ("classic", {"client", "server"}):
            raise AssertionError("unexpected contract selection")

    def _topology_directory(self, name: str, create: bool = False) -> Path:
        root = self.paths.topologies / name
        marker = json.loads((root / MANAGED_MARKER).read_text(encoding="utf-8"))
        if marker["purpose"] != "topology:" + name or not create:
            raise AssertionError("invalid topology reservation")
        return root

    def component_path(self, role: str, profile: str) -> Path:
        self.component_calls.append((role, profile))
        return self.client

    def topology_up(self, name: str, profile: str, state: str, services: list[str], **kwargs) -> dict:
        self.launch = kwargs["scenario_benchmark"]
        if self.create_report:
            report = Path(self.launch["report"])
            report.write_text('{"terminal":true}\n', encoding="utf-8")
            report.chmod(0o600)
        source = {"source": "client", "path": str(self.client), "head": "1" * 40, "dirty": self.dirty}
        return {"control": {"generation": "generation-1"}, "resolved": {"client": source}}

    def topology_status(self, name: str) -> dict:
        if self.statuses:
            return self.statuses.pop(0)
        return {
            "control": {"generation": "generation-1"},
            "supervisor": {"running": False},
            "services": {"client": {"running": False, "exit_code": 0}, "server": {"running": False, "exit_code": 0}},
        }

    def topology_down(self, name: str, *, expected_generation: str | None = None) -> dict:
        self.down_calls.append((name, expected_generation))
        return {
            "control": {"generation": "generation-1"},
            "supervisor": {"running": False},
            "services": {"client": {"running": False, "exit_code": 0}, "server": {"running": False, "exit_code": 0}},
            "shutdown": {"clean": False},
            "observation": {"client_exit_code": 0},
        }


class RunBenchmarkTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(dir="/tmp")
        self.root = Path(self.temporary.name)
        self.route = self.root / "input-route.xml"
        write_private(self.route, ROUTE)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def run_success(self, workspace: FakeWorkspace) -> dict:
        route = benchmark.parse_route(ROUTE)
        native = valid_summary(route, {"head": "1" * 40})
        with mock.patch.object(benchmark, "verify_report", return_value=native):
            return benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=2)

    def test_run_benchmark_reserves_fresh_private_topology_and_records_evidence(self) -> None:
        workspace = FakeWorkspace(self.root)
        result = self.run_success(workspace)
        topology = workspace.paths.topologies / "bench"
        evidence = topology / "benchmark"
        self.assertEqual(result["status"], "success")
        self.assertEqual(stat.S_IMODE(topology.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE(evidence.stat().st_mode), 0o700)
        self.assertEqual(json.loads((topology / MANAGED_MARKER).read_text())["purpose"], "topology:bench")
        self.assertEqual((evidence / "route.xml").read_bytes(), ROUTE)
        self.assertEqual(workspace.component_calls, [("client", "classic")])
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])
        self.assertEqual(result["shutdown"]["generation"], "generation-1")
        durable = (evidence / "summary.json").read_text(encoding="utf-8")
        self.assertNotIn("fixture-password-must-not-be-durable", durable)
        self.assertNotIn("password", result)

    def test_recovery_reservation_denial_creates_no_topology(self) -> None:
        workspace = FakeWorkspace(self.root)
        with mock.patch.object(workspace, "_guard_recovered_resource", side_effect=WorkspaceError("reserved recovery")):
            with self.assertRaisesRegex(WorkspaceError, "reserved recovery"):
                self.run_success(workspace)
        self.assertFalse((workspace.paths.topologies / "bench").exists())
        self.assertEqual(workspace.down_calls, [])

    def test_server_failure_or_topology_error_rejects_successful_report(self) -> None:
        for code, error in ((1, None), (-9, None), (0, "runtime failure")):
            with self.subTest(code=code, error=error), tempfile.TemporaryDirectory(dir="/tmp") as directory:
                workspace = FakeWorkspace(Path(directory))
                status = workspace.topology_down("unused", expected_generation="generation-1")
                workspace.down_calls.clear()
                status["services"]["server"]["exit_code"] = code
                if error:
                    status["error"] = error
                workspace.statuses = [status]
                with self.assertRaisesRegex(WorkspaceError, "server failed|runtime error"):
                    self.run_success(workspace)
                self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_refuses_existing_topology_root(self) -> None:
        workspace = FakeWorkspace(self.root)
        (workspace.paths.topologies / "bench").mkdir()
        with self.assertRaisesRegex(WorkspaceError, "fresh topology"):
            benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=2)

    def test_run_benchmark_generation_change_uses_fenced_shutdown(self) -> None:
        workspace = FakeWorkspace(self.root)
        workspace.statuses = [{
            "control": {"generation": "generation-2"},
            "supervisor": {"running": False},
            "services": {"client": {"running": False, "exit_code": 0}, "server": {"running": False, "exit_code": 0}},
        }]
        with self.assertRaisesRegex(WorkspaceError, "generation changed"):
            benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=2)
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_rejects_clean_native_exit_without_report_and_cleans_up(self) -> None:
        workspace = FakeWorkspace(self.root, create_report=False)
        with self.assertRaisesRegex(WorkspaceError, "No such file|cannot find|report"):
            benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=2)
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_timeout_cleans_up_owned_generation(self) -> None:
        workspace = FakeWorkspace(self.root)
        running = {
            "control": {"generation": "generation-1"},
            "supervisor": {"running": True},
            "services": {"client": {"running": True, "exit_code": None}},
        }
        workspace.statuses = [running]
        with (
            mock.patch.object(benchmark.time, "monotonic", side_effect=[0.0, 2.0]),
            mock.patch.object(benchmark.time, "sleep"),
        ):
            with self.assertRaisesRegex(WorkspaceError, "deadline expired"):
                benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=1)
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_rejects_dirty_selected_source(self) -> None:
        workspace = FakeWorkspace(self.root, dirty=True)
        with self.assertRaisesRegex(WorkspaceError, "clean committed sources"):
            benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=2)
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_detects_selected_verifier_hash_change(self) -> None:
        workspace = FakeWorkspace(self.root)

        original_up = workspace.topology_up
        def changed_up(*args, **kwargs):
            status = original_up(*args, **kwargs)
            verifier = workspace.client / "tools" / "verify_live_movement.py"
            verifier.chmod(0o600)
            verifier.write_bytes(b"print('changed')\n")
            verifier.chmod(0o400)
            return status

        workspace.topology_up = changed_up
        with self.assertRaisesRegex(WorkspaceError, "verifier changed"):
            benchmark.run_benchmark(workspace, "brynknot", "bench", self.route, timeout=2)
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])


if __name__ == "__main__":
    unittest.main()
