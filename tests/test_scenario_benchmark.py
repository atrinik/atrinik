from __future__ import annotations

import binascii
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
import time
import unittest
from unittest import mock
import zlib

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


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png_chunk(kind: bytes, data: bytes) -> bytes:
    checksum = binascii.crc32(data, binascii.crc32(kind)) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)


def png_fixture(width: int = 8, height: int = 6) -> bytes:
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    scanlines = b"".join(b"\x00" + b"\x00\x00\x00\xff" * width for _ in range(height))
    return (
        PNG_SIGNATURE
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(scanlines))
        + png_chunk(b"IEND", b"")
    )


def capture_record(path: Path, payload: bytes, width: int = 8, height: int = 6) -> dict:
    return {
        "path": str(path),
        "size_bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
        "width": width,
        "height": height,
    }


def avi_fixture(frame_payloads: tuple[bytes, ...] = (
        b"\xff\xd8one\xff\xd9", b"\xff\xd8two\xff\xd9")) -> bytes:
    header = bytearray(224)
    header[0:4], header[8:12] = b"RIFF", b"AVI "
    header[12:16], header[20:24] = b"LIST", b"hdrl"
    header[24:28] = b"avih"
    header[88:92], header[96:100] = b"LIST", b"strl"
    header[100:104], header[108:112], header[112:116] = b"strh", b"vids", b"MJPG"
    header[164:168], header[188:192] = b"strf", b"MJPG"
    header[212:216], header[220:224] = b"LIST", b"movi"
    values = {
        16: 192, 28: 56, 32: 50000, 44: 0x10,
        48: len(frame_payloads), 56: 1, 64: 8, 68: 6,
        92: 116, 104: 56, 128: 1, 132: 20,
        140: len(frame_payloads), 168: 40, 172: 40, 176: 8, 180: 6,
    }
    for offset, value in values.items():
        struct.pack_into("<I", header, offset, value)
    struct.pack_into("<HH", header, 184, 1, 24)
    payload = bytearray(header)
    chunks = []
    for frame in frame_payloads:
        offset = len(payload)
        payload.extend(b"00dc" + struct.pack("<I", len(frame)) + frame)
        if len(frame) & 1:
            payload.append(0)
        chunks.append((offset, len(frame)))
    index_offset = len(payload)
    struct.pack_into("<I", payload, 216, index_offset - 220)
    payload.extend(b"idx1" + struct.pack("<I", len(chunks) * 16))
    for offset, size in chunks:
        payload.extend(struct.pack("<4sIII", b"00dc", 0x10, offset - 220, size))
    struct.pack_into("<I", payload, 4, len(payload) - 8)
    return bytes(payload)


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
            [f"--live-movement-route={self.route}", f"--live-movement-report={self.report}"],
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

    def test_launch_arguments_uses_only_fixed_capture_paths_and_lighting_flag(self) -> None:
        for phase in ("day", "new-moon", "full-moon"):
            with self.subTest(phase=phase):
                value = dict(self.value, capture="true", lighting_phase=phase)
                self.assertEqual(
                    benchmark.launch_arguments(value, self.root, "scenario-brynknot"),
                    [
                        f"--live-movement-route={self.route}",
                        f"--live-movement-report={self.report}",
                        f"--live-movement-initial-capture={self.evidence / 'initial.png'}",
                        f"--live-movement-final-capture={self.evidence / 'final.png'}",
                        f"--live-movement-lighting-phase={phase}",
                    ],
                )

    def test_launch_arguments_rejects_capture_outputs_that_already_exist(self) -> None:
        value = dict(self.value, capture="true")
        for filename in ("initial.png", "final.png"):
            with self.subTest(filename=filename):
                path = self.evidence / filename
                path.symlink_to(self.route)
                with self.assertRaisesRegex(WorkspaceError, "capture already exists"):
                    benchmark.launch_arguments(value, self.root, "scenario-brynknot")
                path.unlink()

    def test_launch_arguments_rejects_phase_without_capture_and_unknown_phase(self) -> None:
        invalid = (
            dict(self.value, lighting_phase="day"),
            dict(self.value, capture="true", lighting_phase="twilight"),
            dict(self.value, capture="false"),
        )
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(WorkspaceError):
                benchmark.launch_arguments(value, self.root, "scenario-brynknot")

    def test_launch_arguments_uses_only_fixed_absent_recording_path(self) -> None:
        value = dict(self.value, record_video="true")
        self.assertEqual(
            benchmark.launch_arguments(value, self.root, "scenario-brynknot"),
            [
                f"--live-movement-route={self.route}",
                f"--live-movement-report={self.report}",
                f"--record-video={self.evidence / 'gameplay.avi'}",
            ],
        )
        recording = self.evidence / "gameplay.avi"
        recording.symlink_to(self.route)
        with self.assertRaisesRegex(WorkspaceError, "recording already exists"):
            benchmark.launch_arguments(value, self.root, "scenario-brynknot")

    def test_launch_arguments_rejects_invalid_recording_request(self) -> None:
        for value in (
            dict(self.value, record_video="false"),
            dict(self.value, record_video=True),
        ):
            with self.subTest(value=value), self.assertRaises(WorkspaceError):
                benchmark.launch_arguments(value, self.root, "scenario-brynknot")


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

    def test_verify_report_rejects_nested_overflow(self) -> None:
        summary = valid_summary(self.expected, self.source)
        summary["metrics"] = {"nested": [{"value": "OVERFLOW"}]}
        for value in ("1e999", "-1e999", "NaN", "Infinity", "-Infinity"):
            with self.subTest(value=value):
                output = json.dumps(summary).replace('"OVERFLOW"', value).encode()
                with mock.patch.object(benchmark, "run_bounded", return_value=output):
                    with self.assertRaisesRegex(WorkspaceError, "invalid benchmark verifier summary"):
                        benchmark.verify_report(self.verifier, self.route, self.report, self.expected, self.source)

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


class VerifyCaptureTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(dir="/tmp")
        self.evidence = Path(self.temporary.name) / "benchmark"
        self.evidence.mkdir(mode=0o700)
        self.payload = png_fixture()
        self.summary = {"status": "success", "captures": {}}
        for kind in ("initial", "final"):
            path = self.evidence / f"{kind}.png"
            path.write_bytes(self.payload)
            path.chmod(0o600)
            self.summary["captures"][kind] = capture_record(path, self.payload)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_verify_captures_accepts_exact_owned_png_pair(self) -> None:
        benchmark.verify_captures(self.summary, self.evidence, True)

    def test_verify_captures_rejects_missing_or_malformed_pair(self) -> None:
        invalid = (
            {"status": "success"},
            {"status": "success", "captures": None},
            {"status": "success", "captures": {"initial": self.summary["captures"]["initial"]}},
            {"status": "success", "captures": dict(self.summary["captures"], extra={})},
        )
        for summary in invalid:
            with self.subTest(summary=summary), self.assertRaisesRegex(
                WorkspaceError, "both requested benchmark captures"
            ):
                benchmark.verify_captures(summary, self.evidence, True)

        malformed = json.loads(json.dumps(self.summary))
        malformed["captures"]["initial"] = "not a record"
        with self.assertRaisesRegex(WorkspaceError, "path differs"):
            benchmark.verify_captures(malformed, self.evidence, True)

    def test_verify_captures_rejects_unrequested_capture_metadata(self) -> None:
        benchmark.verify_captures({"status": "success"}, self.evidence, False)
        with self.assertRaisesRegex(WorkspaceError, "unrequested"):
            benchmark.verify_captures(self.summary, self.evidence, False)

    def test_verify_captures_rejects_path_hash_size_and_dimensions(self) -> None:
        invalid_records = {
            "path": {"path": str(self.evidence / "other.png")},
            "hash": {"sha256": "0" * 64},
            "size": {"size_bytes": len(self.payload) + 1},
            "width": {"width": 9},
            "height": {"height": 7},
            "boolean dimension": {"width": True},
        }
        for label, changes in invalid_records.items():
            with self.subTest(label=label):
                summary = json.loads(json.dumps(self.summary))
                summary["captures"]["initial"].update(changes)
                with self.assertRaises(WorkspaceError):
                    benchmark.verify_captures(summary, self.evidence, True)

    def test_verify_captures_rejects_invalid_png_and_symlink(self) -> None:
        initial = self.evidence / "initial.png"
        initial.chmod(0o600)
        initial.write_bytes(b"not a PNG")
        initial.chmod(0o600)
        summary = json.loads(json.dumps(self.summary))
        summary["captures"]["initial"] = capture_record(initial, b"not a PNG")
        with self.assertRaisesRegex(WorkspaceError, "PNG"):
            benchmark.verify_captures(summary, self.evidence, True)

        initial.unlink()
        initial.symlink_to(self.evidence / "final.png")
        summary["captures"]["initial"] = capture_record(initial, self.payload)
        with self.assertRaises(WorkspaceError):
            benchmark.verify_captures(summary, self.evidence, True)

    def test_verify_captures_rejects_structurally_invalid_png(self) -> None:
        valid = png_fixture()
        ihdr_end = 8 + 4 + 4 + 13 + 4
        malformed = {
            "truncated chunk": valid[:-2],
            "missing IDAT": valid[:ihdr_end] + png_chunk(b"IEND", b""),
            "missing IEND": valid[:-12],
            "trailing data": valid + b"trailing",
            "excessive chunk length": (
                valid[:ihdr_end] + struct.pack(">I", 0xFFFFFFFF) + b"IDAT"
            ),
        }
        bad_crc = bytearray(valid)
        bad_crc[ihdr_end - 1] ^= 0x01
        malformed["bad CRC"] = bytes(bad_crc)

        initial = self.evidence / "initial.png"
        for label, payload in malformed.items():
            with self.subTest(label=label):
                initial.chmod(0o600)
                initial.write_bytes(payload)
                initial.chmod(0o600)
                summary = json.loads(json.dumps(self.summary))
                summary["captures"]["initial"] = capture_record(initial, payload)
                with self.assertRaisesRegex(WorkspaceError, "PNG"):
                    benchmark.verify_captures(summary, self.evidence, True)

    def test_png_rejects_invalid_compressed_scanlines(self) -> None:
        header = png_fixture()[:33]
        raw = (b"\x00" + bytes(32)) * 6
        for compressed in (b"not-zlib-data", zlib.compress(raw)[:-1],
                           zlib.compress(raw[:-1]), zlib.compress(raw + b"x"),
                           zlib.compress(b"\x05" + raw[1:]),
                           zlib.compress(raw) + b"trailing", zlib.compress(raw) * 2):
            with self.subTest(compressed=compressed):
                payload = header + png_chunk(b"IDAT", compressed) + png_chunk(b"IEND", b"")
                with self.assertRaisesRegex(WorkspaceError, "PNG"):
                    benchmark.png_dimensions(payload)
        oversized = struct.pack(">IIBBBBB", 0x7fffffff, 0x7fffffff, 16, 6, 0, 0, 0)
        payload = PNG_SIGNATURE + png_chunk(b"IHDR", oversized)
        payload += png_chunk(b"IDAT", zlib.compress(b"")) + png_chunk(b"IEND", b"")
        with self.assertRaisesRegex(WorkspaceError, "decoded image exceeds"):
            benchmark.png_dimensions(payload)

    def test_png_accepts_color_depth_filters_and_adam7(self) -> None:
        formats = {0: (1, (1, 2, 4, 8, 16)), 2: (3, (8, 16)),
                   3: (1, (1, 2, 4, 8)), 4: (2, (8, 16)), 6: (4, (8, 16))}
        for color, (channels, depths) in formats.items():
            for depth in depths:
                for interlace in (0, 1):
                    for width, height in ((1, 1), (9, 9)):
                        with self.subTest(color=color, depth=depth, interlace=interlace, size=(width, height)):
                            passes = ((0, 0, 1, 1),) if not interlace else (
                                (0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8),
                                (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))
                            raw = bytearray()
                            for x, y, dx, dy in passes:
                                columns, rows = len(range(x, width, dx)), len(range(y, height, dy))
                                if columns:
                                    for row in range(rows):
                                        raw.extend(bytes([row % 5]) + bytes((columns * channels * depth + 7) // 8))
                            payload = PNG_SIGNATURE + png_chunk(b"IHDR", struct.pack(
                                ">IIBBBBB", width, height, depth, color, 0, 0, interlace))
                            if color == 3:
                                payload += png_chunk(b"PLTE", bytes(6))
                            compressed = zlib.compress(raw)
                            for byte in compressed:
                                payload += png_chunk(b"IDAT", bytes([byte]))
                            payload += png_chunk(b"IEND", b"")
                            self.assertEqual(benchmark.png_dimensions(payload), (width, height))

    def test_png_spans_multiple_inflate_blocks(self) -> None:
        self.assertEqual(benchmark.png_dimensions(png_fixture(512, 128)), (512, 128))

    def test_png_bounds_inflate_output(self) -> None:
        # The compressed input is tiny compared with the invalid inflated payload.
        payload = png_fixture()[:33] + png_chunk(b"IDAT", zlib.compress(bytes(1024 * 1024)))
        payload += png_chunk(b"IEND", b"")
        with self.assertRaisesRegex(WorkspaceError, "exceeds its scanlines"):
            benchmark.png_dimensions(payload)

    def test_verify_captures_enforces_private_mode_and_64_mib_bound(self) -> None:
        initial = self.evidence / "initial.png"
        initial.chmod(0o622)
        with self.assertRaisesRegex(WorkspaceError, "owner, type, mode, or size"):
            benchmark.verify_captures(self.summary, self.evidence, True)

        initial.chmod(0o600)
        with initial.open("wb") as stream:
            stream.truncate(benchmark.MAX_CAPTURE_BYTES + 1)
        initial.chmod(0o600)
        with self.assertRaisesRegex(WorkspaceError, "owner, type, mode, or size"):
            benchmark.verify_captures(self.summary, self.evidence, True)


class VerifyRecordingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(dir="/tmp")
        self.path = Path(self.temporary.name) / "gameplay.avi"
        write_private(self.path, avi_fixture())

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_verify_recording_accepts_indexed_mjpeg_container(self) -> None:
        result = benchmark.verify_recording(self.path)
        self.assertEqual(result["status"], "verified-container")
        self.assertEqual(result["codec"], "MJPG")
        self.assertEqual(result["sha256"], hashlib.sha256(self.path.read_bytes()).hexdigest())
        self.assertEqual(result["frames"], 2)
        self.assertEqual((result["width"], result["height"]), (8, 6))
        self.assertFalse(result["visual_decode_verified"])

    def test_recording_digest_binds_frame_contents(self) -> None:
        first = benchmark.verify_recording(self.path)
        replacement = avi_fixture((b"\xff\xd8ONE\xff\xd9", b"\xff\xd8two\xff\xd9"))
        self.path.chmod(0o600)
        write_private(self.path, replacement)
        second = benchmark.verify_recording(self.path)
        self.assertEqual(first["size_bytes"], second["size_bytes"])
        self.assertNotEqual(first["sha256"], second["sha256"])
        self.assertEqual(second["sha256"], hashlib.sha256(replacement).hexdigest())

    def test_recording_replacement_during_hash_is_rejected(self) -> None:
        original = benchmark._pread_exact
        def replaced(descriptor, size, offset):
            data = original(descriptor, size, offset)
            if size == self.path.stat().st_size:
                alternate = self.path.with_name("replacement.avi")
                write_private(alternate, avi_fixture())
                alternate.replace(self.path)
            return data
        with mock.patch.object(benchmark, "_pread_exact", side_effect=replaced):
            with self.assertRaisesRegex(WorkspaceError, "recording .*changed"):
                benchmark.verify_recording(self.path)

    def test_verify_recording_rejects_header_timing_frames_and_index_drift(self) -> None:
        mutations = {
            "codec": (112, b"H264"),
            "timing": (32, struct.pack("<I", 40000)),
            "zero frames": (48, struct.pack("<I", 0)),
            "jpeg marker": (224 + 8, b"NO"),
            "index offset": (-8, b"idx1"),
        }
        original = avi_fixture()
        for label, (offset, replacement) in mutations.items():
            with self.subTest(label=label):
                payload = bytearray(original)
                position = len(payload) + offset if offset < 0 else offset
                payload[position:position + len(replacement)] = replacement
                self.path.chmod(0o600)
                self.path.write_bytes(payload)
                self.path.chmod(0o400)
                with self.assertRaises(WorkspaceError):
                    benchmark.verify_recording(self.path)

    def test_verify_recording_rejects_missing_symlink_unsafe_mode_and_size(self) -> None:
        self.path.unlink()
        with self.assertRaisesRegex(WorkspaceError, "cannot validate"):
            benchmark.verify_recording(self.path)
        target = Path(self.temporary.name) / "target.avi"
        write_private(target, avi_fixture())
        self.path.symlink_to(target)
        with self.assertRaisesRegex(WorkspaceError, "cannot validate"):
            benchmark.verify_recording(self.path)
        self.path.unlink()
        self.path.write_bytes(avi_fixture())
        self.path.chmod(0o622)
        with self.assertRaisesRegex(WorkspaceError, "owner, type, mode, or size"):
            benchmark.verify_recording(self.path)
        self.path.chmod(0o600)
        with self.path.open("wb") as stream:
            stream.truncate(benchmark.MAX_RECORDING_BYTES + 1)
        self.path.chmod(0o400)
        with self.assertRaisesRegex(WorkspaceError, "owner, type, mode, or size"):
            benchmark.verify_recording(self.path)


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

    def __init__(
        self,
        root: Path,
        *,
        create_report: bool = True,
        create_captures: bool = True,
        create_recording: bool = True,
        dirty: bool = False,
    ) -> None:
        self.paths = FakePaths(root / "workspace")
        self.paths.ensure()
        self.client = root / "client"
        (self.client / "tools").mkdir(parents=True)
        write_private(self.client / "tools" / "verify_live_movement.py", b"print('{}')\n")
        self.create_report = create_report
        self.create_captures = create_captures
        self.create_recording = create_recording
        self.dirty = dirty
        self.statuses: list[dict] = []
        self.launch = None
        self.topology_options = None
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
        kwargs["benchmark_prepared"]()
        self.topology_options = kwargs
        self.launch = kwargs["scenario_benchmark"]
        generation_published = kwargs.get("generation_published")
        if generation_published is not None:
            generation_published("generation-1")
        if self.create_report:
            report = Path(self.launch["report"])
            report.write_text('{"terminal":true}\n', encoding="utf-8")
            report.chmod(0o600)
        if self.create_captures and self.launch.get("capture") == "true":
            for kind in ("initial", "final"):
                path = Path(self.launch["report"]).with_name(f"{kind}.png")
                path.write_bytes(png_fixture())
                path.chmod(0o600)
        if self.create_recording and self.launch.get("record_video") == "true":
            path = Path(self.launch["report"]).with_name("gameplay.avi")
            path.write_bytes(avi_fixture())
            path.chmod(0o600)
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

    def run_success(
        self,
        workspace: FakeWorkspace,
        *,
        capture: bool = False,
        lighting_phase: str | None = None,
        record_video: bool = False,
        prebuilt_build: str | None = None,
    ) -> dict:
        route = benchmark.parse_route(ROUTE)
        native = valid_summary(route, {"head": "1" * 40})
        if capture:
            evidence = workspace.paths.topologies / "bench" / "benchmark"
            payload = png_fixture()
            native["captures"] = {
                kind: capture_record(evidence / f"{kind}.png", payload)
                for kind in ("initial", "final")
            }
        if lighting_phase is not None:
            native["identity"] = {"lighting_phase": lighting_phase}
        with mock.patch.object(benchmark, "verify_report", return_value=native):
            return benchmark.run_benchmark(
                workspace,
                "brynknot",
                "bench",
                self.route,
                timeout=2,
                capture=capture,
                lighting_phase=lighting_phase,
                record_video=record_video,
                prebuilt_build=prebuilt_build,
            )

    def test_scenario_reset_before_locked_preparation_leaves_no_topology(self) -> None:
        workspace = FakeWorkspace(self.root)
        initial = workspace._load_scenario("brynknot")
        reset = {**initial, "provisioned_at": "new-generation"}
        with mock.patch.object(workspace, "_load_scenario", side_effect=[initial, reset]):
            with self.assertRaisesRegex(WorkspaceError, "scenario changed"):
                self.run_success(workspace)
        self.assertFalse((workspace.paths.topologies / "bench").exists())
        self.assertEqual(workspace.down_calls, [])
        self.assertIsNone(workspace.launch)

    def test_startup_refusal_before_locked_preparation_leaves_no_topology(self) -> None:
        workspace = FakeWorkspace(self.root)

        def busy(*args, **kwargs):
            self.assertFalse((workspace.paths.topologies / "bench").exists())
            raise WorkspaceError("topology lease busy")

        workspace.topology_up = busy
        with self.assertRaisesRegex(WorkspaceError, "topology lease busy"):
            self.run_success(workspace)
        self.assertFalse((workspace.paths.topologies / "bench").exists())
        self.assertEqual(workspace.down_calls, [])

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
        self.assertNotIn("recording", result)

    def test_run_benchmark_forwards_and_records_exact_prebuilt_receipt(self) -> None:
        workspace = FakeWorkspace(self.root)
        digest = "a" * 64
        result = self.run_success(workspace, prebuilt_build=digest)
        self.assertEqual(result["prebuilt_build"], digest)
        self.assertEqual(workspace.topology_options["prebuilt_build"], digest)
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_rejects_invalid_prebuilt_receipt_before_mutation(self) -> None:
        workspace = FakeWorkspace(self.root)
        with self.assertRaisesRegex(WorkspaceError, "prebuilt build must be a SHA-256 digest"):
            benchmark.run_benchmark(
                workspace,
                "brynknot",
                "bench",
                self.route,
                timeout=2,
                prebuilt_build="A" * 64,
            )
        self.assertFalse((workspace.paths.topologies / "bench").exists())
        self.assertEqual(workspace.component_calls, [])
        self.assertEqual(workspace.down_calls, [])

    def test_run_benchmark_capture_uses_fixed_outputs_and_selected_lighting(self) -> None:
        workspace = FakeWorkspace(self.root)
        result = self.run_success(workspace, capture=True, lighting_phase="full-moon")
        evidence = workspace.paths.topologies / "bench" / "benchmark"
        self.assertEqual(result["status"], "success")
        self.assertTrue(result["capture_requested"])
        self.assertEqual(result["lighting_phase"], "full-moon")
        self.assertEqual(workspace.launch["capture"], "true")
        self.assertEqual(workspace.launch["lighting_phase"], "full-moon")
        self.assertEqual(set(result["native"]["captures"]), {"initial", "final"})
        for kind in ("initial", "final"):
            path = evidence / f"{kind}.png"
            self.assertEqual(result["native"]["captures"][kind]["path"], str(path))
            self.assertEqual(path.read_bytes(), png_fixture())
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_recording_uses_fixed_output_and_marks_overhead(self) -> None:
        workspace = FakeWorkspace(self.root)
        result = self.run_success(workspace, record_video=True)
        recording = workspace.paths.topologies / "bench" / "benchmark" / "gameplay.avi"
        self.assertEqual(result["status"], "success")
        self.assertEqual(workspace.launch["record_video"], "true")
        self.assertEqual(result["recording"]["path"], str(recording))
        self.assertEqual(result["recording"]["status"], "verified-container")
        self.assertFalse(result["recording"]["performance_comparable"])
        self.assertFalse(result["recording"]["visual_decode_verified"])

    def test_run_benchmark_missing_requested_recording_fails_honestly(self) -> None:
        workspace = FakeWorkspace(self.root, create_recording=False)
        with self.assertRaisesRegex(WorkspaceError, "cannot validate benchmark recording"):
            self.run_success(workspace, record_video=True)
        summary = json.loads(
            (workspace.paths.topologies / "bench" / "benchmark" / "summary.json").read_text()
        )
        self.assertEqual(summary["status"], "failure")
        self.assertEqual(summary["recording"]["status"], "failure")
        self.assertEqual(
            summary["recording"]["path"],
            str(workspace.paths.topologies / "bench" / "benchmark" / "gameplay.avi"),
        )
        self.assertFalse(summary["recording"]["performance_comparable"])
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_interrupt_after_generation_publication_stops_exact_generation(self) -> None:
        workspace = FakeWorkspace(self.root)
        topology_up = workspace.topology_up

        def interrupted(*args, **kwargs):
            topology_up(*args, **kwargs)
            raise KeyboardInterrupt

        workspace.topology_up = interrupted
        with self.assertRaisesRegex(WorkspaceError, "KeyboardInterrupt"):
            self.run_success(workspace)
        summary = json.loads(
            (workspace.paths.topologies / "bench" / "benchmark" / "summary.json").read_text()
        )
        self.assertEqual(summary["status"], "failure")
        self.assertEqual(summary["generation"], "generation-1")
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_interrupt_before_generation_publication_stops_nothing(self) -> None:
        workspace = FakeWorkspace(self.root)

        def interrupted(*_args, **kwargs):
            kwargs["benchmark_prepared"]()
            raise KeyboardInterrupt

        workspace.topology_up = interrupted
        with self.assertRaisesRegex(WorkspaceError, "KeyboardInterrupt"):
            self.run_success(workspace)
        summary = json.loads(
            (workspace.paths.topologies / "bench" / "benchmark" / "summary.json").read_text()
        )
        self.assertEqual(summary["status"], "failure")
        self.assertNotIn("generation", summary)
        self.assertEqual(workspace.down_calls, [])

    def test_generation_is_durable_at_publication_callback(self) -> None:
        workspace = FakeWorkspace(self.root)
        observed = []
        def interrupted(*_args, **kwargs):
            kwargs["benchmark_prepared"]()
            kwargs["generation_published"]("generation-1")
            summary = workspace.paths.topologies / "bench" / "benchmark" / "summary.json"
            observed.append(json.loads(summary.read_text())["generation"])
            raise KeyboardInterrupt
        workspace.topology_up = interrupted
        with mock.patch.object(benchmark, "durable_atomic_json", wraps=benchmark.durable_atomic_json) as durable:
            with self.assertRaisesRegex(WorkspaceError, "KeyboardInterrupt"):
                self.run_success(workspace)
            durable.assert_called_once()
        self.assertEqual(observed, ["generation-1"])

    def test_generation_persistence_failure_prevents_startup(self) -> None:
        workspace = FakeWorkspace(self.root)
        started = []
        def startup(*_args, **kwargs):
            kwargs["benchmark_prepared"]()
            kwargs["generation_published"]("generation-1")
            started.append(True)
        workspace.topology_up = startup
        with mock.patch.object(benchmark, "durable_atomic_json", side_effect=OSError("fsync failed")):
            with self.assertRaisesRegex(WorkspaceError, "fsync failed"):
                self.run_success(workspace)
        self.assertEqual(started, [])
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_startup_failure_after_publication_stops_exact_generation(self) -> None:
        workspace = FakeWorkspace(self.root)

        def failed(*_args, **kwargs):
            kwargs["benchmark_prepared"]()
            kwargs["generation_published"]("generation-1")
            raise WorkspaceError("startup failed")

        workspace.topology_up = failed
        with self.assertRaisesRegex(WorkspaceError, "startup failed"):
            self.run_success(workspace)
        summary = json.loads(
            (workspace.paths.topologies / "bench" / "benchmark" / "summary.json").read_text()
        )
        self.assertEqual(summary["status"], "failure")
        self.assertEqual(summary["generation"], "generation-1")
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

    def test_run_benchmark_rejects_lighting_without_capture_or_unknown_phase(self) -> None:
        invalid = ((False, "day"), (True, "twilight"))
        for capture, phase in invalid:
            with self.subTest(capture=capture, phase=phase), tempfile.TemporaryDirectory(
                dir="/tmp"
            ) as directory:
                workspace = FakeWorkspace(Path(directory))
                with self.assertRaisesRegex(WorkspaceError, "lighting phase requires capture"):
                    benchmark.run_benchmark(
                        workspace,
                        "brynknot",
                        "bench",
                        self.route,
                        timeout=2,
                        capture=capture,
                        lighting_phase=phase,
                    )
                self.assertFalse((workspace.paths.topologies / "bench").exists())
                self.assertEqual(workspace.down_calls, [])

    def test_run_benchmark_requested_capture_missing_fails_and_cleans_up(self) -> None:
        workspace = FakeWorkspace(self.root, create_captures=False)
        with self.assertRaisesRegex(WorkspaceError, "cannot read bounded benchmark input"):
            self.run_success(workspace, capture=True, lighting_phase="day")
        self.assertEqual(workspace.down_calls, [("bench", "generation-1")])
        summary = json.loads(
            (workspace.paths.topologies / "bench" / "benchmark" / "summary.json").read_text()
        )
        self.assertEqual(summary["status"], "failure")

    def test_run_benchmark_rejects_missing_or_wrong_native_lighting_identity(self) -> None:
        identities = (("missing", None), ("wrong", {"lighting_phase": "day"}))
        for label, identity in identities:
            with self.subTest(label=label), tempfile.TemporaryDirectory(dir="/tmp") as directory:
                workspace = FakeWorkspace(Path(directory))
                route = benchmark.parse_route(ROUTE)
                native = valid_summary(route, {"head": "1" * 40})
                evidence = workspace.paths.topologies / "bench" / "benchmark"
                payload = png_fixture()
                native["captures"] = {
                    kind: capture_record(evidence / f"{kind}.png", payload)
                    for kind in ("initial", "final")
                }
                if identity is not None:
                    native["identity"] = identity
                with mock.patch.object(benchmark, "verify_report", return_value=native):
                    with self.assertRaisesRegex(WorkspaceError, "native lighting phase differs"):
                        benchmark.run_benchmark(
                            workspace,
                            "brynknot",
                            "bench",
                            self.route,
                            timeout=2,
                            capture=True,
                            lighting_phase="full-moon",
                        )
                self.assertEqual(workspace.down_calls, [("bench", "generation-1")])

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
