"""Bounded orchestration for a real Classic client movement benchmark."""
from __future__ import annotations

import hashlib
import binascii
import json
import math
import os
from pathlib import Path, PurePosixPath
import platform
import re
import secrets
import selectors
import signal
import stat
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from .model import MANAGED_MARKER, SCHEMA_VERSION, WorkspaceError, atomic_json, validate_name

MAX_ROUTE_BYTES = 8 * 1024 * 1024
MAX_REPORT_BYTES = 128 * 1024 * 1024
MAX_CAPTURE_BYTES = 64 * 1024 * 1024
MAX_RECORDING_BYTES = 0x7fffffff
MAX_RECORDING_FRAMES = 108000
MAX_RECORDING_SECONDS = 60 * 60
LIGHTING_PHASES = {"day", "new-moon", "full-moon"}
MAX_VERIFIER_BYTES = 512 * 1024
MAX_SUMMARY_BYTES = 1024 * 1024
LAUNCH_KEYS = {"scenario", "nonce", "route_sha256", "route", "report"}


def _identity(info):
    return (info.st_dev, info.st_ino, info.st_mode, info.st_uid, info.st_nlink,
            info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def read_regular(path: Path, limit: int) -> bytes:
    """Read bounded regular bytes without following a substituted leaf."""
    flags = os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW | os.O_CLOEXEC
    try:
        descriptor = os.open(path, flags)
        try:
            before = os.fstat(descriptor)
            if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1
                    or before.st_uid not in {0, os.geteuid()} or before.st_mode & 0o022
                    or before.st_size > limit):
                raise WorkspaceError("benchmark input owner, type, mode, or size is unsafe")
            chunks = []
            remaining = limit + 1
            while remaining:
                chunk = os.read(descriptor, min(65536, remaining))
                if not chunk:
                    break
                chunks.append(chunk)
                remaining -= len(chunk)
            data = b"".join(chunks)
            if len(data) > limit or _identity(before) != _identity(os.fstat(descriptor)):
                raise WorkspaceError("benchmark input changed or exceeded its bound")
            if _identity(before) != _identity(path.stat(follow_symlinks=False)):
                raise WorkspaceError("benchmark input path changed")
            return data
        finally:
            os.close(descriptor)
    except (OSError, ValueError) as error:
        raise WorkspaceError(f"cannot read bounded benchmark input: {path}") from error


def parse_route(data: bytes) -> dict:
    if not data or len(data) > MAX_ROUTE_BYTES:
        raise WorkspaceError("benchmark route is empty or exceeds 8 MiB")
    try:
        text = data.decode("utf-8")
        if "\x00" in text or "<!" in text or "<?" in text:
            raise ValueError("declarations, entities, and processing instructions are unsupported")
        root = ET.fromstring(text)
        if root.tag != "live-movement-route" or set(root.attrib) != {"version", "timeout-ms", "step-timeout-ms"}:
            raise ValueError("unsupported root or fields")
        if root.attrib["version"] != "1":
            raise ValueError("unsupported route version")
        def integer(value, minimum, maximum):
            if re.fullmatch(r"0|[1-9][0-9]*", value) is None:
                raise ValueError("noncanonical integer")
            number = int(value)
            if not minimum <= number <= maximum:
                raise ValueError("integer out of bounds")
            return number
        timeout = integer(root.attrib["timeout-ms"], 1, 3600000)
        step_timeout = integer(root.attrib["step-timeout-ms"], 1, 60000)
        if root.text and root.text.strip():
            raise ValueError("unexpected route text")
        checkpoints = []
        for index, child in enumerate(root):
            if child.tag != "checkpoint" or set(child.attrib) != {"map", "x", "y", "direction"} or len(child):
                raise ValueError("unsupported checkpoint fields")
            if (child.text and child.text.strip()) or (child.tail and child.tail.strip()):
                raise ValueError("unexpected checkpoint text")
            path = child.attrib["map"]
            if (len(path) > 511 or path.startswith("//") or re.fullmatch(r"/[A-Za-z0-9_./-]+", path) is None
                    or str(PurePosixPath(path)) != path or ".." in PurePosixPath(path).parts):
                raise ValueError("invalid server map path")
            direction = integer(child.attrib["direction"], 0, 9)
            if (index == 0 and direction != 0) or (index > 0 and direction in {0, 5}):
                raise ValueError("invalid checkpoint movement direction")
            checkpoints.append({"map": path, "x": integer(child.attrib["x"], 0, 255),
                                "y": integer(child.attrib["y"], 0, 255), "direction": direction})
            if len(checkpoints) > 50000:
                raise ValueError("too many checkpoints")
        if len(checkpoints) < 2:
            raise ValueError("route requires at least two checkpoints")
    except (UnicodeError, ET.ParseError, ValueError) as error:
        raise WorkspaceError(f"invalid live movement route: {error}") from error
    return {"sha256": hashlib.sha256(data).hexdigest(), "timeout_ms": timeout,
            "step_timeout_ms": step_timeout, "checkpoints": checkpoints}


def _write_new(path: Path, data: bytes) -> None:
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o400)
    with os.fdopen(descriptor, "wb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def launch_arguments(value: dict, root: Path, state_name: str | None) -> list[str]:
    optional = {"capture", "lighting_phase", "record_video"}
    if (not isinstance(value, dict) or not LAUNCH_KEYS <= set(value)
            or not set(value) <= LAUNCH_KEYS | optional):
        raise WorkspaceError("invalid scenario benchmark launch fields")
    if any(not isinstance(item, str) for item in value.values()):
        raise WorkspaceError("invalid scenario benchmark launch values")
    if "capture" in value and value["capture"] != "true":
        raise WorkspaceError("invalid benchmark capture request")
    if "lighting_phase" in value and value["lighting_phase"] not in LIGHTING_PHASES:
        raise WorkspaceError("invalid benchmark lighting phase")
    if "lighting_phase" in value and "capture" not in value:
        raise WorkspaceError("benchmark lighting phase requires capture")
    if "record_video" in value and value["record_video"] != "true":
        raise WorkspaceError("invalid benchmark recording request")
    validate_name(value["scenario"], "scenario name")
    if state_name != "scenario-" + value["scenario"] or re.fullmatch(r"[a-f0-9]{32}", value["nonce"]) is None:
        raise WorkspaceError("benchmark does not match its scenario-owned state")
    evidence = root / "benchmark"
    for directory in (root, evidence):
        info = directory.stat(follow_symlinks=False)
        if (not stat.S_ISDIR(info.st_mode) or info.st_uid != os.geteuid()
                or info.st_mode & 0o077):
            raise WorkspaceError("benchmark evidence directory is not private")
    route, report = evidence / "route.xml", evidence / "frames.jsonl"
    if value["route"] != str(route) or value["report"] != str(report):
        raise WorkspaceError("benchmark paths are outside the owned topology")
    record = parse_route(read_regular(route, MAX_ROUTE_BYTES))
    if record["sha256"] != value["route_sha256"]:
        raise WorkspaceError("benchmark route digest changed")
    if report.exists() or report.is_symlink():
        raise WorkspaceError("benchmark report already exists")
    arguments = [f"--live-movement-route={route}", f"--live-movement-report={report}"]
    if "capture" in value:
        for kind in ("initial", "final"):
            path = evidence / f"{kind}.png"
            if path.exists() or path.is_symlink():
                raise WorkspaceError("benchmark capture already exists")
            arguments.append(f"--live-movement-{kind}-capture={path}")
    if "lighting_phase" in value:
        arguments.append(f"--live-movement-lighting-phase={value['lighting_phase']}")
    if "record_video" in value:
        recording = evidence / "gameplay.avi"
        if root.resolve() != root or evidence.resolve() != evidence:
            raise WorkspaceError("benchmark recording directory is not canonical")
        if recording.exists() or recording.is_symlink():
            raise WorkspaceError("benchmark recording already exists")
        arguments.append(f"--record-video={recording}")
    return arguments


def _json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON field")
        result[key] = value
    return result


def run_bounded(arguments: list[str], *, timeout: float, limit: int, cwd: Path | None = None) -> bytes:
    """Drain both pipes with one bounded total budget, terminating only this child."""
    output = bytearray()
    total = 0
    deadline = time.monotonic() + timeout
    with subprocess.Popen(arguments, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE) as process:
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ, True)
                selector.register(process.stderr, selectors.EVENT_READ, False)
                while selector.get_map():
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise WorkspaceError("benchmark helper deadline expired")
                    for key, _ in selector.select(min(remaining, 0.5)):
                        chunk = os.read(key.fd, 65536)
                        if not chunk:
                            selector.unregister(key.fileobj)
                            continue
                        total += len(chunk)
                        if total > limit:
                            raise WorkspaceError("benchmark helper output exceeded its bound")
                        if key.data:
                            output.extend(chunk)
                if process.wait(timeout=max(0.001, deadline - time.monotonic())):
                    raise WorkspaceError("benchmark helper rejected the evidence")
        except BaseException:
            process.kill()
            process.wait()
            raise
    return bytes(output)


def verify_report(verifier: Path, route: Path, report: Path, expected: dict, source: dict) -> dict:
    before = report.stat(follow_symlinks=False)
    if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1
            or before.st_uid != os.geteuid() or before.st_mode & 0o022
            or not 0 < before.st_size <= MAX_REPORT_BYTES):
        raise WorkspaceError("benchmark report owner, type, mode, or size is unsafe")
    output = run_bounded([sys.executable, "-I", str(verifier), str(route), str(report)],
                         timeout=30, limit=MAX_SUMMARY_BYTES)
    if _identity(before) != _identity(report.stat(follow_symlinks=False)):
        raise WorkspaceError("benchmark report changed during verification")
    if parse_route(read_regular(route, MAX_ROUTE_BYTES))["sha256"] != expected["sha256"]:
        raise WorkspaceError("benchmark route changed during verification")
    try:
        summary = json.loads(output, object_pairs_hook=_json_object,
                             parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite JSON")))
    except (ValueError, UnicodeError) as error:
        raise WorkspaceError("invalid benchmark verifier summary") from error
    count = len(expected["checkpoints"])
    if (not isinstance(summary, dict) or summary.get("status") != "success"
            or summary.get("route_sha256") != expected["sha256"]
            or summary.get("source_revision") != source["head"]
            or summary.get("source_dirty") is not False
            or any(type(summary.get(key)) is not int or summary[key] != count
                   for key in ("arrivals", "presented_checkpoints", "expected_checkpoints"))
            or any(type(summary.get(key)) is not int or summary[key] <= 0
                   for key in ("frames", "presented_frames"))
            or not isinstance(summary.get("gpu_backend"), str) or not summary["gpu_backend"]
            or not isinstance(summary.get("gpu_device"), str) or not summary["gpu_device"]):
        raise WorkspaceError("benchmark summary identity or complete rendered coverage differs")
    return summary


def route_provenance(route: Path, expected: dict, scenario: dict) -> dict:
    companion = route.with_name(route.name + ".provenance.json")
    if not companion.exists() and not companion.is_symlink():
        return {"producer": "user-supplied"}
    try:
        value = json.loads(read_regular(companion, MAX_SUMMARY_BYTES), object_pairs_hook=_json_object)
        if (not isinstance(value, dict) or set(value) != {
                "schema_version", "producer", "scenario", "profile", "profile_generation",
                "route_sha256", "output", "sources"}
                or type(value["schema_version"]) is not int or value["schema_version"] != 1
                or value["producer"] != "brynknot-v1"
                or value["scenario"] != scenario["name"] or value["profile"] != scenario["profile"]
                or value["route_sha256"] != expected["sha256"] or value["output"] != str(route)
                or not isinstance(value["profile_generation"], str)
                or re.fullmatch(r"[a-f0-9]{64}", value["profile_generation"]) is None
                or not isinstance(value["sources"], dict) or not value["sources"]):
            raise ValueError("route provenance differs from this scenario and route")
        for key, row in value["sources"].items():
            if (not isinstance(key, str) or not isinstance(row, dict) or set(row) != {
                    "path", "checkout_path", "checkout", "repository", "branch", "source", "head", "dirty"}
                    or row["dirty"] is not False
                    or any(not isinstance(item, str) or not item for field, item in row.items() if field != "dirty")
                    or re.fullmatch(r"[a-f0-9]{40,64}", row["head"]) is None):
                raise ValueError("route producer source identity is invalid")
        return value
    except (ValueError, UnicodeError) as error:
        raise WorkspaceError("invalid benchmark route provenance") from error


def validate_source_provenance(root: Path, sources: dict) -> None:
    manifest = json.loads(read_regular(root / "benchmark" / "summary.json", MAX_SUMMARY_BYTES),
                          object_pairs_hook=_json_object)
    provenance = manifest["route_origin"]
    for key, produced in provenance.get("sources", {}).items():
        current = sources.get(key, {})
        if any(current.get(field) != produced[field] for field in (
                "checkout", "repository", "branch", "source", "head", "dirty")):
            raise WorkspaceError("route producer differs from the selected source generation")


def png_dimensions(payload: bytes) -> tuple[int, int]:
    """Validate the complete bounded PNG chunk stream without decoding pixels."""
    if len(payload) > MAX_CAPTURE_BYTES or payload[:8] != b"\x89PNG\r\n\x1a\n":
        raise WorkspaceError("benchmark capture is not a bounded PNG")
    offset = 8
    dimensions = None
    color_type = None
    palette = False
    image_bytes = 0
    image_started = False
    image_closed = False
    while offset < len(payload):
        if len(payload) - offset < 12:
            raise WorkspaceError("benchmark PNG chunk is truncated")
        length = struct.unpack_from(">I", payload, offset)[0]
        kind = payload[offset + 4:offset + 8]
        end = offset + 12 + length
        if (end > len(payload) or re.fullmatch(rb"[A-Za-z]{4}", kind) is None
                or kind[2] & 0x20):
            raise WorkspaceError("benchmark PNG chunk length or type is invalid")
        data = memoryview(payload)[offset + 8:offset + 8 + length]
        expected_crc = struct.unpack_from(">I", payload, offset + 8 + length)[0]
        actual_crc = binascii.crc32(data, binascii.crc32(kind)) & 0xffffffff
        if actual_crc != expected_crc:
            raise WorkspaceError("benchmark PNG chunk CRC differs")
        if dimensions is None and kind != b"IHDR":
            raise WorkspaceError("benchmark PNG must start with IHDR")
        if kind == b"IHDR":
            if dimensions is not None or length != 13:
                raise WorkspaceError("benchmark PNG IHDR is invalid")
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", data)
            depths = {0: {1, 2, 4, 8, 16}, 2: {8, 16}, 3: {1, 2, 4, 8}, 4: {8, 16}, 6: {8, 16}}
            if (not 0 < width <= 0x7fffffff or not 0 < height <= 0x7fffffff
                    or depth not in depths.get(color_type, set())
                    or compression != 0 or filtering != 0 or interlace not in {0, 1}):
                raise WorkspaceError("benchmark PNG image fields are invalid")
            dimensions = width, height
        elif kind == b"PLTE":
            if (palette or image_started or not 3 <= length <= 768 or length % 3
                    or color_type in {0, 4} or color_type == 3 and length // 3 > 1 << depth):
                raise WorkspaceError("benchmark PNG palette is invalid")
            palette = True
        elif kind == b"IDAT":
            if image_closed or color_type == 3 and not palette:
                raise WorkspaceError("benchmark PNG image data ordering is invalid")
            image_started = True
            image_bytes += length
        elif kind == b"IEND":
            if length or not image_bytes or end != len(payload):
                raise WorkspaceError("benchmark PNG final image boundary is invalid")
            return dimensions
        elif not kind[0] & 0x20:
            raise WorkspaceError("benchmark PNG contains an unsupported critical chunk")
        if image_started and kind != b"IDAT":
            image_closed = True
        offset = end
    raise WorkspaceError("benchmark PNG is incomplete")


def verify_captures(summary: dict, evidence: Path, requested: bool) -> None:
    captures = summary.get("captures")
    if not requested:
        if "captures" in summary:
            raise WorkspaceError("unrequested benchmark captures were reported")
        return
    if not isinstance(captures, dict) or set(captures) != {"initial", "final"}:
        raise WorkspaceError("both requested benchmark captures are required")
    for kind, record in captures.items():
        path = evidence / f"{kind}.png"
        if not isinstance(record, dict) or record.get("path") != str(path):
            raise WorkspaceError("benchmark capture path differs from its owned output")
        payload = read_regular(path, MAX_CAPTURE_BYTES)
        dimensions = png_dimensions(payload)
        if (type(record.get("size_bytes")) is not int or record["size_bytes"] != len(payload)
                or record.get("sha256") != hashlib.sha256(payload).hexdigest()
                or any(type(record.get(field)) is not int or record[field] <= 0
                       for field in ("width", "height"))
                or dimensions != (record["width"], record["height"])):
            raise WorkspaceError("benchmark capture bytes differ from the verified PNG identity")


def _pread_exact(descriptor: int, size: int, offset: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = os.pread(descriptor, size - len(data), offset + len(data))
        if not chunk:
            raise WorkspaceError("benchmark recording is truncated")
        data.extend(chunk)
    return bytes(data)


def verify_recording(path: Path) -> dict:
    """Validate the bounded RIFF/MJPEG framing without decoding video frames."""
    flags = os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW | os.O_CLOEXEC
    try:
        descriptor = os.open(path, flags)
        try:
            before = os.fstat(descriptor)
            if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1
                    or before.st_uid not in {0, os.geteuid()} or before.st_mode & 0o022
                    or not 224 < before.st_size <= MAX_RECORDING_BYTES):
                raise WorkspaceError(
                    "benchmark recording owner, type, mode, or size is unsafe"
                )
            header = _pread_exact(descriptor, 224, 0)
            little = lambda offset: struct.unpack_from("<I", header, offset)[0]
            if (header[:4] != b"RIFF" or little(4) != before.st_size - 8
                    or header[8:12] != b"AVI "
                    or header[12:16] != b"LIST" or little(16) != 192
                    or header[20:24] != b"hdrl"
                    or header[24:28] != b"avih" or little(28) != 56
                    or header[88:92] != b"LIST" or little(92) != 116
                    or header[96:100] != b"strl"
                    or header[100:104] != b"strh" or little(104) != 56
                    or header[108:112] != b"vids" or header[112:116] != b"MJPG"
                    or header[164:168] != b"strf" or little(168) != 40
                    or little(172) != 40 or header[188:192] != b"MJPG"
                    or header[212:216] != b"LIST" or header[220:224] != b"movi"):
                raise WorkspaceError("benchmark recording RIFF/MJPEG header is invalid")
            microseconds, main_flags = little(32), little(44)
            frames, streams = little(48), little(56)
            width, height = little(64), little(68)
            scale, rate, stream_frames = little(128), little(132), little(140)
            bitmap_width, bitmap_height = little(176), little(180)
            planes, bit_count = struct.unpack_from("<HH", header, 184)
            if (microseconds != 50000 or main_flags != 0x10 or streams != 1
                    or not 0 < frames <= MAX_RECORDING_FRAMES
                    or stream_frames != frames or scale != 1 or rate != 20
                    or frames * scale > MAX_RECORDING_SECONDS * rate
                    or not 0 < width <= 4096 or not 0 < height <= 4096
                    or width * height > 8388608
                    or (bitmap_width, bitmap_height) != (width, height)
                    or planes != 1 or bit_count != 24):
                raise WorkspaceError("benchmark recording stream timing or format is invalid")
            index_offset = 220 + little(216)
            if not 224 < index_offset <= before.st_size - 8:
                raise WorkspaceError("benchmark recording movi boundary is invalid")
            chunks = []
            offset = 224
            while offset < index_offset:
                if len(chunks) >= MAX_RECORDING_FRAMES:
                    raise WorkspaceError("benchmark recording frame count exceeds its bound")
                chunk = _pread_exact(descriptor, 8, offset)
                size = struct.unpack_from("<I", chunk, 4)[0]
                padded = size + (size & 1)
                if (chunk[:4] != b"00dc" or size < 4
                        or offset + 8 + padded > index_offset
                        or _pread_exact(descriptor, 2, offset + 8) != b"\xff\xd8"
                        or _pread_exact(descriptor, 2, offset + 8 + size - 2) != b"\xff\xd9"):
                    raise WorkspaceError("benchmark recording MJPEG frame is invalid")
                chunks.append((offset, size))
                offset += 8 + padded
            if offset != index_offset or len(chunks) != frames:
                raise WorkspaceError("benchmark recording frame count or movi size differs")
            index_header = _pread_exact(descriptor, 8, index_offset)
            index_size = struct.unpack_from("<I", index_header, 4)[0]
            if (index_header[:4] != b"idx1" or index_size != frames * 16
                    or index_offset + 8 + index_size != before.st_size):
                raise WorkspaceError("benchmark recording index boundary is invalid")
            for number, (chunk_offset, chunk_size) in enumerate(chunks):
                entry = _pread_exact(descriptor, 16, index_offset + 8 + number * 16)
                tag, entry_flags, relative, size = struct.unpack("<4sIII", entry)
                if (tag != b"00dc" or entry_flags != 0x10
                        or relative != chunk_offset - 220 or size != chunk_size):
                    raise WorkspaceError("benchmark recording index entry differs")
            after = os.fstat(descriptor)
            if _identity(before) != _identity(after):
                raise WorkspaceError("benchmark recording changed during verification")
            try:
                visible = path.stat(follow_symlinks=False)
            except OSError as error:
                raise WorkspaceError("benchmark recording path changed") from error
            if _identity(before) != _identity(visible):
                raise WorkspaceError("benchmark recording path changed")
            return {
                "path": str(path),
                "status": "verified-container",
                "size_bytes": before.st_size,
                "frames": frames,
                "width": width,
                "height": height,
                "frames_per_second": 20,
                "codec": "MJPG",
                "visual_decode_verified": False,
            }
        finally:
            os.close(descriptor)
    except WorkspaceError:
        raise
    except (OSError, ValueError, struct.error) as error:
        raise WorkspaceError(f"cannot validate benchmark recording: {path}") from error


def run_benchmark(workspace, scenario_name: str, name: str, route: Path, timeout: int | None = None,
                  *, capture: bool = False, lighting_phase: str | None = None,
                  record_video: bool = False) -> dict:
    validate_name(name, "benchmark topology name")
    if type(capture) is not bool:
        raise WorkspaceError("benchmark capture must be a boolean request")
    if type(record_video) is not bool:
        raise WorkspaceError("benchmark video recording must be a boolean request")
    if lighting_phase is not None and (not isinstance(lighting_phase, str)
            or lighting_phase not in LIGHTING_PHASES or not capture):
        raise WorkspaceError("benchmark lighting phase requires capture and a supported phase")
    if not route.is_absolute():
        raise WorkspaceError("benchmark route must be an absolute path")
    data = read_regular(route, MAX_ROUTE_BYTES)
    route_record = parse_route(data)
    if timeout is None:
        timeout = math.ceil(route_record["timeout_ms"] / 1000)
    if type(timeout) is not int or not 1 <= timeout <= 3600:
        raise WorkspaceError("benchmark timeout must be between 1 and 3600 seconds")
    scenario = workspace._load_scenario(scenario_name)
    provenance = route_provenance(route, route_record, scenario)
    workspace._require_classic_contracts(scenario["profile"], {"client", "server"})
    workspace.paths.ensure()
    root = workspace.paths.topologies / name
    workspace._guard_recovered_resource("topology", root, name)
    # mkdir is the reservation: no existing or concurrently created topology
    # can be adopted by a benchmark invocation.
    try:
        root.mkdir(mode=0o700)
    except FileExistsError as error:
        raise WorkspaceError("benchmark requires a fresh topology name") from error
    atomic_json(root / MANAGED_MARKER,
                {"schema_version": SCHEMA_VERSION, "purpose": f"topology:{name}"})
    root = workspace._topology_directory(name, create=True)
    evidence = root / "benchmark"
    evidence.mkdir(mode=0o700)
    staged_route, report = evidence / "route.xml", evidence / "frames.jsonl"
    _write_new(staged_route, data)
    launch = {"scenario": scenario_name, "nonce": secrets.token_hex(16),
              "route_sha256": route_record["sha256"], "route": str(staged_route), "report": str(report)}
    if capture:
        launch["capture"] = "true"
    if lighting_phase is not None:
        launch["lighting_phase"] = lighting_phase
    if record_video:
        launch["record_video"] = "true"
    manifest = {"schema_version": 1, "name": name, "status": "running", "scenario": scenario_name,
                "profile": scenario["profile"], "preset": scenario["preset"], "state": scenario["state"],
                "route_sha256": route_record["sha256"], "expected_checkpoints": len(route_record["checkpoints"]),
                "timeout_seconds": timeout, "host": {"system": platform.system(), "machine": platform.machine()},
                "evidence": str(evidence), "route_origin": provenance, "launch_nonce": launch["nonce"]}
    manifest["capture_requested"] = capture
    recording = evidence / "gameplay.avi"
    if record_video:
        manifest["recording"] = {
            "path": str(recording),
            "status": "pending",
            "performance_comparable": False,
        }
    if lighting_phase is not None:
        manifest["lighting_phase"] = lighting_phase
    manifest["scenario_identity"] = {
        key: scenario[key] for key in ("schema_version", "stack", "providers", "resolved", "provisioned_at")
        if key in scenario
    }
    atomic_json(evidence / "summary.json", manifest)
    generation = None
    failure = None

    def remember_generation(published: str) -> None:
        nonlocal generation
        if generation is not None and generation != published:
            raise WorkspaceError("benchmark topology generation changed during publication")
        generation = published
        manifest["generation"] = published

    try:
        verifier_bytes = read_regular(workspace.component_path("client", scenario["profile"]) / "tools" / "verify_live_movement.py", MAX_VERIFIER_BYTES)
        staged_verifier = evidence / "verify_live_movement.py"
        _write_new(staged_verifier, verifier_bytes)
        manifest["verifier_sha256"] = hashlib.sha256(verifier_bytes).hexdigest()
        status = workspace.topology_up(name, scenario["profile"], scenario["state"],
                                       ["server", "client"], scenario_benchmark=launch,
                                       generation_published=remember_generation)
        returned_generation = status["control"]["generation"]
        if generation != returned_generation:
            raise WorkspaceError("benchmark topology returned a different generation")
        manifest["generation"] = generation
        manifest["sources"] = status["resolved"]
        manifest["build"] = {key: status.get(key) for key in ("build_root", "stack", "providers")}

        sources = status["resolved"]
        if not sources or any(row.get("dirty") is not False for row in sources.values()):
            raise WorkspaceError("benchmark requires clean committed sources")
        for key, produced in provenance.get("sources", {}).items():
            current_source = sources.get(key, {})
            if any(current_source.get(field) != produced[field] for field in (
                    "checkout", "repository", "branch", "source", "head", "dirty")):
                raise WorkspaceError("route producer differs from the launched source generation")
        client = next(row for row in sources.values() if row["source"] == "client")
        if read_regular(Path(client["path"]) / "tools" / "verify_live_movement.py", MAX_VERIFIER_BYTES) != verifier_bytes:
            raise WorkspaceError("selected benchmark verifier changed before launch")
        atomic_json(evidence / "summary.json", manifest)
        deadline = time.monotonic() + timeout
        while True:
            current = workspace.topology_status(name)
            if current.get("control", {}).get("generation") != generation:
                raise WorkspaceError("benchmark topology generation changed")
            if report.exists() or report.is_symlink():
                info = report.stat(follow_symlinks=False)
                if not stat.S_ISREG(info.st_mode) or info.st_size > MAX_REPORT_BYTES:
                    raise WorkspaceError("benchmark report exceeded its bound or changed type")
            services = current.get("services", {})
            if not current.get("supervisor", {}).get("running") and not any(row.get("running") for row in services.values()):
                if current.get("error") is not None:
                    raise WorkspaceError("benchmark topology reported a runtime error")
                if services.get("client", {}).get("exit_code") != 0:
                    raise WorkspaceError("benchmark client failed or disconnected")
                if services.get("server", {}).get("exit_code") not in {0, -signal.SIGTERM}:
                    raise WorkspaceError("benchmark server failed during the run")
                break
            if time.monotonic() >= deadline:
                raise WorkspaceError("benchmark deadline expired")
            time.sleep(0.2)
        if read_regular(staged_verifier, MAX_VERIFIER_BYTES) != verifier_bytes:
            raise WorkspaceError("staged benchmark verifier changed")
        manifest["native"] = verify_report(staged_verifier, staged_route, report, route_record, client)
        if lighting_phase is not None:
            native_identity = manifest["native"].get("identity")
            if not isinstance(native_identity, dict) or native_identity.get("lighting_phase") != lighting_phase:
                raise WorkspaceError("benchmark native lighting phase differs from its request")
        verify_captures(manifest["native"], evidence, capture)
        if record_video:
            manifest["recording"] = {
                **verify_recording(recording),
                "performance_comparable": False,
            }
        manifest["status"] = "success"
    except (WorkspaceError, OSError, KeyError, StopIteration, ValueError, subprocess.SubprocessError, KeyboardInterrupt) as error:
        failure = str(error) or type(error).__name__
        if record_video and manifest["recording"]["status"] == "pending":
            manifest["recording"].update(status="failure", error=failure)
        manifest.update(status="failure", error=failure)
    finally:
        if generation is not None:
            try:
                stopped = workspace.topology_down(name, expected_generation=generation)
                if (stopped.get("control", {}).get("generation") != generation
                        or stopped.get("error") is not None
                        or stopped.get("supervisor", {}).get("running") is not False
                        or any(row.get("running") for row in stopped.get("services", {}).values())):
                    raise WorkspaceError("benchmark shutdown did not confirm stopped owned generation")
                manifest["shutdown"] = {"generation": generation,
                                        "result": stopped.get("shutdown"),
                                        "observation": stopped.get("observation"),
                                        "exit_codes": {key: row.get("exit_code") for key, row in stopped.get("services", {}).items()}}
            except (WorkspaceError, OSError) as error:
                failure = "benchmark shutdown failed: " + str(error)
                manifest.update(status="failure", error=failure)
        atomic_json(evidence / "summary.json", manifest)
    if failure:
        raise WorkspaceError(f"{failure}; benchmark evidence: {evidence}")
    return manifest
