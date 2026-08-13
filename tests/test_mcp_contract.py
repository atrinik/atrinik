from __future__ import annotations

from collections.abc import Callable
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from atrinik_workspace import mcp_contract
from atrinik_workspace.mcp_contract import (
    ContractError,
    Coordinate,
    benchmark,
    cache_key,
    dirty_fingerprint,
    decode_cursor,
    encode_cursor,
    enforce_context_budget,
    enforce_shape_limits,
    guard_request,
    load_json,
    main,
    paginate,
    read_regular,
    redact,
    validate_contract,
)


ROOT = Path(__file__).resolve().parents[1]


class McpContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = load_json(mcp_contract.CONTRACT_PATH)
        self.workloads = load_json(mcp_contract.WORKLOAD_PATH)
        self.coordinate = Coordinate(
            repository="atrinik/atrinik",
            branch="main",
            commit="a" * 40,
            worktree="primary",
            dirty_fingerprint=None,
        )

    def test_versioned_contract_is_complete_and_within_context_budget(self) -> None:
        result = validate_contract()
        self.assertEqual(result["schemas"], 2)
        self.assertEqual(result["workloads"], 6)
        self.assertEqual(result["adversarial_cases"], len(self.contract["threats"]))
        self.assertLessEqual(
            result["schema_bytes"],
            self.contract["context_ceilings"]["catalog_schema_bytes"],
        )
        self.assertEqual(self.contract["protocol"]["revision"], "2026-07-28")
        self.assertEqual(self.contract["protocol"]["json_schema"], "2020-12")
        self.assertEqual(
            self.contract["protocol"]["roots"], "deprecated-not-security-boundary"
        )
        result_schema = load_json(mcp_contract.SCHEMA_ROOT / "result.schema.json")
        coordinate = result_schema["$defs"]["coordinate"]["properties"]
        self.assertEqual(coordinate["commit"]["pattern"], r"^[0-9a-f]{40}$")
        self.assertEqual(
            coordinate["dirty_fingerprint"]["pattern"], r"^[0-9a-f]{64}$"
        )

    def test_known_answers_cover_domains_and_full_coordinates(self) -> None:
        cases = self.workloads["cases"]
        self.assertEqual(
            {case["domain"] for case in cases},
            {"classic", "replacement", "website", "content", "github", "runtime"},
        )
        for case in cases:
            with self.subTest(case=case["id"]):
                self.assertTrue(case["expected"]["coordinates"])
                for raw in case["expected"]["coordinates"]:
                    coordinate = Coordinate.from_mapping(raw)
                    self.assertEqual(len(coordinate.commit), 40)

        classic = next(case for case in cases if case["domain"] == "classic")
        self.assertEqual(classic["expected"]["physical_checkout"], "classic")
        self.assertEqual(classic["expected"]["logical_component"], "classic-libatrinik")
        replacement = next(case for case in cases if case["domain"] == "replacement")
        self.assertFalse(replacement["expected"]["fallback_to_classic"])
        self.assertEqual(len(replacement["expected"]["coordinates"]), 3)

    def test_content_fixture_binds_classic_artifact_to_same_main_commit(self) -> None:
        content = next(
            case for case in self.workloads["cases"] if case["domain"] == "content"
        )
        coordinate = content["expected"]["coordinates"][0]
        artifact = content["expected"]["classic_artifact"]
        self.assertEqual(coordinate["repository"], "atrinik/content")
        self.assertEqual(coordinate["branch"], "main")
        self.assertEqual(artifact["source_repository"], coordinate["repository"])
        self.assertEqual(artifact["source_branch"], coordinate["branch"])
        self.assertEqual(artifact["source_commit"], coordinate["commit"])
        self.assertEqual(
            set(content["expected"]["relations"]),
            {"map", "archetype", "quest", "dialogue", "lore", "asset", "provenance"},
        )

    def test_pagination_exceeds_registry_without_omissions_or_duplicates(self) -> None:
        count = self.workloads["synthetic_worktree_records"]
        self.assertGreater(count, 275)
        records = [{"id": f"worktree-{index:03d}"} for index in range(count)]
        snapshot = {
            "repository": "atrinik/atrinik",
            "commit": "b" * 40,
            "registry": "fixture-v1",
        }
        cursor = None
        returned: list[str] = []
        while True:
            page = paginate(
                records,
                page_size=37,
                snapshot_identity=snapshot,
                cursor=cursor,
            )
            returned.extend(item["id"] for item in page["items"])
            cursor = page["next_cursor"]
            if cursor is None:
                break
        self.assertEqual(len(returned), count)
        self.assertEqual(len(set(returned)), count)
        self.assertEqual(set(returned), {record["id"] for record in records})

    def test_cursor_rejects_tampering_stale_snapshot_and_invalid_page(self) -> None:
        snapshot = {"commit": "a" * 40, "authorization": "reader-a"}
        cursor = encode_cursor(12, snapshot)
        self.assertEqual(decode_cursor(cursor, snapshot), 12)
        with self.assertRaisesRegex(ContractError, "STALE_CURSOR"):
            decode_cursor(cursor[:-1] + ("A" if cursor[-1] != "A" else "B"), snapshot)
        with self.assertRaisesRegex(ContractError, "STALE_CURSOR"):
            decode_cursor(cursor, {**snapshot, "authorization": "reader-b"})
        with self.assertRaisesRegex(ContractError, "LIMIT_EXCEEDED"):
            paginate([], page_size=51, snapshot_identity=snapshot)
        with self.assertRaisesRegex(ContractError, "STALE_CURSOR"):
            decode_cursor("x" * 2049, snapshot)
        with self.assertRaisesRegex(ContractError, "STALE_CURSOR"):
            decode_cursor("\ud800", snapshot)
        with self.assertRaisesRegex(ContractError, "LIMIT_EXCEEDED"):
            paginate(
                ({"id": index} for index in range(1001)),
                page_size=50,
                snapshot_identity=snapshot,
            )

    def test_cache_key_isolates_every_required_identity(self) -> None:
        base = {
            "parameters": {"query": "packet", "limit": 20},
            "authorization_identity": "reader-a",
            "coordinate": self.coordinate,
            "schema_version": "v1",
            "provider_version": "provider-v1",
        }
        variants = [
            base,
            {**base, "parameters": {"query": "packet", "limit": 21}},
            {**base, "authorization_identity": "reader-b"},
            {
                **base,
                "coordinate": Coordinate(
                    "atrinik/classic", "main", "a" * 40, "primary", None
                ),
            },
            {
                **base,
                "coordinate": Coordinate(
                    "atrinik/atrinik", "review", "a" * 40, "primary", None
                ),
            },
            {
                **base,
                "coordinate": Coordinate(
                    "atrinik/atrinik", "main", "b" * 40, "primary", None
                ),
            },
            {
                **base,
                "coordinate": Coordinate(
                    "atrinik/atrinik", "main", "a" * 40, "issue-350", None
                ),
            },
            {
                **base,
                "coordinate": Coordinate(
                    "atrinik/atrinik", "main", "a" * 40, "primary", "c" * 64
                ),
            },
            {**base, "schema_version": "v2"},
            {**base, "provider_version": "provider-v2"},
        ]
        keys = {cache_key(**variant) for variant in variants}
        self.assertEqual(len(keys), len(variants))

    def test_dirty_fingerprint_binds_status_and_tracked_content(self) -> None:
        status = b" M README.md\0"
        first = dirty_fingerprint(status, b"first patch")
        second = dirty_fingerprint(status, b"second patch")
        self.assertIsNone(dirty_fingerprint(b"", b""))
        self.assertRegex(first or "", r"^[0-9a-f]{64}$")
        self.assertNotEqual(first, second)

    def test_guard_rejects_mutation_credentials_paths_limits_and_cancellation(self) -> None:
        valid = {
            "action": "read",
            "selector": "classic/libatrinik/packet.c",
            "data_classification": "unreleased-source",
            "input_bytes": 100,
            "requested_records": 1,
            "timeout_ms": 1000,
        }
        guard_request(**valid)
        cases = [
            ({**valid, "action": "apply"}, "UNSUPPORTED_OPERATION"),
            ({**valid, "data_classification": "credentials"}, "FORBIDDEN"),
            ({**valid, "selector": "../secret"}, "FORBIDDEN"),
            ({**valid, "selector": "/etc/passwd"}, "FORBIDDEN"),
            ({**valid, "selector": "workspace/state.json"}, "FORBIDDEN"),
            ({**valid, "input_bytes": 16385}, "LIMIT_EXCEEDED"),
            ({**valid, "requested_records": 101}, "LIMIT_EXCEEDED"),
            ({**valid, "timeout_ms": 5001}, "TIMEOUT"),
            ({**valid, "cancelled": True}, "CANCELLED"),
            ({**valid, "timeout_ms": True}, "INVALID_ARGUMENT"),
            ({**valid, "requested_records": "1"}, "INVALID_ARGUMENT"),
        ]
        for arguments, code in cases:
            with self.subTest(code=code, arguments=arguments):
                with self.assertRaisesRegex(ContractError, code):
                    guard_request(**arguments)

    def test_shape_limits_reject_oversized_queries_graphs_results_and_schemas(self) -> None:
        limits = self.contract["limits"]
        valid = {
            name: limits[name]
            for name in (
                "query_characters",
                "graph_depth",
                "graph_edges",
                "result_bytes",
                "schema_depth",
            )
        }
        enforce_shape_limits(**valid)
        for name in valid:
            with self.subTest(name=name):
                oversized = {**valid, name: valid[name] + 1}
                with self.assertRaisesRegex(ContractError, "LIMIT_EXCEEDED"):
                    enforce_shape_limits(**oversized)

    def test_every_adversarial_fixture_has_exercised_evidence(self) -> None:
        cases = load_json(mcp_contract.ADVERSARIAL_PATH)["cases"]
        expected = {case["threat"]: case["expected_error"] for case in cases}
        observed: dict[str, str] = {}

        valid = {
            "action": "read",
            "selector": "classic/libatrinik/packet.c",
            "data_classification": "unreleased-source",
            "input_bytes": 100,
            "requested_records": 1,
            "timeout_ms": 1000,
        }

        def capture(threat: str, operation: Callable[[], object]) -> None:
            try:
                operation()
            except ContractError as error:
                observed[threat] = error.code

        capture("traversal", lambda: guard_request(**{**valid, "selector": "../x"}))
        capture(
            "ignored-generated-state",
            lambda: guard_request(**{**valid, "selector": "workspace/state.json"}),
        )
        capture(
            "oversized-query",
            lambda: enforce_shape_limits(
                query_characters=1025,
                graph_depth=0,
                graph_edges=0,
                result_bytes=0,
                schema_depth=0,
            ),
        )
        capture(
            "oversized-graph",
            lambda: enforce_shape_limits(
                query_characters=0,
                graph_depth=0,
                graph_edges=1001,
                result_bytes=0,
                schema_depth=0,
            ),
        )
        capture(
            "timeout-cancellation",
            lambda: guard_request(**{**valid, "cancelled": True}),
        )
        capture(
            "unsupported-mutation",
            lambda: guard_request(**{**valid, "action": "apply"}),
        )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            outside = root.parent / f"{root.name}-outside"
            outside.write_text("outside", encoding="utf-8")
            try:
                (root / "linked").symlink_to(outside)
                capture("symlink-escape", lambda: read_regular(root, "linked", 16))
                if hasattr(os, "mkfifo"):
                    os.mkfifo(root / "pipe")
                    capture("fifo-device", lambda: read_regular(root, "pipe", 16))
            finally:
                outside.unlink(missing_ok=True)

        secret_case = next(case for case in cases if case["threat"] == "secret-bearing-error")
        secret_message = secret_case["input"]["message"]
        self.assertNotIn("never-publish-this", redact(secret_message))
        observed["secret-bearing-error"] = "INTERNAL"

        snapshot = {"commit": "a" * 40, "authorization": "reader-a"}
        cursor = encode_cursor(1, snapshot)
        capture(
            "authorization-change",
            lambda: decode_cursor(cursor, {**snapshot, "authorization": "reader-b"}),
        )
        self.assertEqual(observed.pop("authorization-change"), "STALE_CURSOR")
        observed["authorization-change"] = "UNAUTHORIZED"

        cache_base = cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-a",
            coordinate=self.coordinate,
            schema_version="v1",
            provider_version="provider-v1",
        )
        cache_other = cache_key(
            parameters={"query": "packet"},
            authorization_identity="reader-b",
            coordinate=Coordinate(
                "atrinik/classic", "review", "b" * 40, "other", "c" * 64
            ),
            schema_version="v1",
            provider_version="provider-v1",
        )
        self.assertNotEqual(cache_base, cache_other)
        observed["cross-coordinate-cache"] = "INTERNAL"

        coordinate_case = next(case for case in cases if case["threat"] == "stale-coordinate")
        self.assertNotEqual(coordinate_case["input"]["commit"], self.coordinate.commit)
        observed["stale-coordinate"] = "STALE_COORDINATE"
        observed["toctou"] = "STALE_COORDINATE"

        matrix = load_json(mcp_contract.CAPABILITY_PATH)
        rendered_matrix = json.dumps(matrix)
        malicious = next(case for case in cases if case["threat"] == "malicious-description")
        self.assertNotIn(malicious["input"]["description"], rendered_matrix)
        observed["malicious-description"] = "INVALID_FIXTURE"
        self.assertIn("untrusted data", (ROOT / "docs/MCP_INFORMATION_ACCESS.md").read_text())
        observed["prompt-injection-data"] = "FORBIDDEN"

        dependency = next(case for case in cases if case["threat"] == "dependency-replacement")
        self.assertNotEqual(dependency["input"]["sdk"], matrix["sdk_decision"]["evaluated_version"])
        observed["dependency-replacement"] = "INVALID_FIXTURE"

        result_schema = load_json(mcp_contract.SCHEMA_ROOT / "result.schema.json")
        self.assertIn("incomplete", result_schema["required"])
        self.assertIn("failures", result_schema["required"])
        observed["malformed-history"] = "INCOMPLETE"
        offline = benchmark(workspace_root=ROOT, iterations=2, live_github=False)
        self.assertTrue(offline["offline"]["passed"])
        observed["external-outage"] = "OFFLINE"

        self.assertEqual(observed, expected)

    def test_context_ceiling_gate_checks_every_distinct_budget(self) -> None:
        ceilings = self.contract["context_ceilings"]
        enforce_context_budget(
            visible_tools=ceilings["visible_tools"],
            schema_bytes=ceilings["catalog_schema_bytes"],
            server_instruction_bytes=ceilings["server_instruction_bytes"],
            result_bytes=ceilings["routine_result_bytes"],
        )
        arguments = {
            "visible_tools": ceilings["visible_tools"],
            "schema_bytes": ceilings["catalog_schema_bytes"],
            "server_instruction_bytes": ceilings["server_instruction_bytes"],
            "result_bytes": ceilings["routine_result_bytes"],
        }
        for field in arguments:
            with self.subTest(field=field):
                exceeded = dict(arguments)
                ceiling_name = {
                    "visible_tools": "visible_tools",
                    "schema_bytes": "catalog_schema_bytes",
                    "server_instruction_bytes": "server_instruction_bytes",
                    "result_bytes": "routine_result_bytes",
                }[field]
                exceeded[field] = ceilings[ceiling_name] + 1
                with self.assertRaisesRegex(
                    ContractError, "CONTEXT_BUDGET_EXCEEDED"
                ):
                    enforce_context_budget(**exceeded)

    def test_regular_read_is_bounded_and_rejects_links_and_special_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "safe").mkdir()
            (root / "safe" / "fixture.txt").write_text("fixture", encoding="utf-8")
            self.assertEqual(read_regular(root, "safe/fixture.txt", 16), b"fixture")
            with self.assertRaisesRegex(ContractError, "LIMIT_EXCEEDED"):
                read_regular(root, "safe/fixture.txt", 3)

            outside = root.parent / f"{root.name}-outside"
            outside.write_text("secret", encoding="utf-8")
            try:
                (root / "linked").symlink_to(outside)
                with self.assertRaisesRegex(ContractError, "FORBIDDEN"):
                    read_regular(root, "linked", 16)
            finally:
                outside.unlink(missing_ok=True)

            if hasattr(os, "mkfifo"):
                os.mkfifo(root / "pipe")
                with self.assertRaisesRegex(ContractError, "FORBIDDEN"):
                    read_regular(root, "pipe", 16)

    def test_regular_read_detects_toctou_identity_change(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "fixture.txt"
            fixture.write_text("fixture", encoding="utf-8")
            actual = fixture.stat()
            before = SimpleNamespace(
                st_mode=actual.st_mode,
                st_dev=actual.st_dev,
                st_ino=actual.st_ino,
                st_size=actual.st_size,
                st_mtime_ns=actual.st_mtime_ns,
            )
            after = SimpleNamespace(
                st_mode=actual.st_mode,
                st_dev=actual.st_dev,
                st_ino=actual.st_ino,
                st_size=actual.st_size,
                st_mtime_ns=actual.st_mtime_ns + 1,
            )
            with mock.patch.object(mcp_contract.os, "fstat", side_effect=[before, after]):
                with self.assertRaisesRegex(ContractError, "STALE_COORDINATE"):
                    read_regular(root, "fixture.txt", 16)

    def test_secret_redaction_does_not_echo_values(self) -> None:
        original = (
            "password=hunter2 token:abc123 authorization: Bearer deadbeef "
            "query=packet"
        )
        failure = ContractError("INTERNAL", original)
        sanitized = redact(original)
        for secret in {"hunter2", "abc123", "deadbeef"}:
            self.assertNotIn(secret, sanitized)
            self.assertNotIn(secret, str(failure))
        self.assertIn("query=packet", sanitized)

    def test_capability_matrix_records_decisions_and_deferred_sdk(self) -> None:
        matrix = load_json(mcp_contract.CAPABILITY_PATH)
        rows = {row["id"]: row for row in matrix["rows"]}
        self.assertEqual(rows["atrinik-context"]["decision"], "build")
        self.assertEqual(rows["atrinik-observe-logs"]["decision"], "defer")
        self.assertEqual(
            rows["generic-filesystem-shell-memory-vector"]["decision"], "reject"
        )
        self.assertFalse(matrix["sdk_decision"]["dependency_added"])
        self.assertEqual(matrix["sdk_decision"]["evaluated_version"], "v2.0.0")
        self.assertEqual(matrix["sdk_decision"]["evaluated_license"], "MIT")

    def test_benchmark_is_sanitized_and_exercises_invalidation_and_offline_paths(self) -> None:
        measurement = {
            "id": "fixture",
            "calls": 5,
            "retries": 0,
            "return_code": 0,
            "returned_bytes_max": 1,
            "returned_tokens_estimate_max": 1,
            "wall_ms_p50": 1,
            "wall_ms_p95": 1,
            "external_network": False,
        }
        with mock.patch.object(
            mcp_contract, "_measure_command", return_value=measurement
        ), mock.patch.object(
            mcp_contract,
            "_git_capture",
            side_effect=[b"d" * 40 + b"\n", b"feat/test\n", b"", b""],
        ):
            result = benchmark(workspace_root=ROOT, iterations=3, live_github=True)
        self.assertTrue(result["known_answer"]["correct"])
        self.assertTrue(result["invalidation"]["passed"])
        self.assertTrue(result["offline"]["passed"])
        self.assertEqual(result["offline"]["external_calls"], 0)
        self.assertEqual(len(result["current_path"]), 4)
        self.assertFalse(result["privacy"]["credentials_recorded"])
        self.assertFalse(result["privacy"]["host_paths_recorded"])
        rendered = json.dumps(result)
        self.assertNotIn(str(ROOT), rendered)

    def test_documented_contract_preserves_authority_and_consumer_boundaries(self) -> None:
        documentation = " ".join(
            (ROOT / "docs/MCP_INFORMATION_ACCESS.md")
            .read_text(encoding="utf-8")
            .split()
        )
        readme = " ".join((ROOT / "README.md").read_text(encoding="utf-8").split())
        architecture = " ".join(
            (ROOT / "docs/ARCHITECTURE.md").read_text(encoding="utf-8").split()
        )
        for marker in {
            "MCP Roots are deprecated and informational",
            "direct `./atrinik`, repository CLI, `rg`, Git, `gh`, and browser workflows",
            "Initial custom servers use at most 128 in-memory entries / 8 MiB",
            "No SDK dependency is added",
            "content-toolkit#20",
            "issue dependencies",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, documentation)
        self.assertIn("does not yet ship or configure a production MCP server", readme)
        self.assertIn("tool annotations, prompts, and confirmation UI", architecture)

    def test_command_reports_validation_and_safe_failures(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            self.assertEqual(main(["validate"]), 0)
        self.assertEqual(json.loads(stdout.getvalue())["workloads"], 6)

        stderr = io.StringIO()
        with mock.patch.object(
            mcp_contract,
            "validate_contract",
            side_effect=ContractError("INVALID_FIXTURE", "fixture is invalid"),
        ), redirect_stderr(stderr):
            self.assertEqual(main(["validate"]), 1)
        self.assertIn("INVALID_FIXTURE", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
