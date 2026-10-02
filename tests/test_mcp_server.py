# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
import io
import json
import threading
import unittest
from unittest.mock import patch

from atrinik_workspace.mcp_server import (CAPABILITIES_KEY, VERSION_KEY, PROTOCOL_VERSION,
    ContextServer, Tool, object_schema, serve)
from atrinik_workspace.mcp_contract import ContractError, canonical_json
from tests import test_mcp_context as fixture


def request(method, **params):
    return {"jsonrpc": "2.0", "id": 1, "method": method,
            "params": {"_meta": {VERSION_KEY: PROTOCOL_VERSION, CAPABILITIES_KEY: {}}, **params}}


class ServerTests(unittest.TestCase):
    def setUp(self):
        fixture.ContextFixture.setUp(self)
        self.server = ContextServer(self.service)

    def test_modern_discovery_and_no_runtime_catalog(self):
        response = self.server.handle(request("server/discover"))
        self.assertEqual(response["result"]["supportedVersions"], [PROTOCOL_VERSION])
        self.assertEqual(response["result"]["resultType"], "complete")
        tools = self.server.handle(request("tools/list"))["result"]["tools"]
        self.assertEqual(len(tools), 7)
        self.assertLess(len(canonical_json(tools)), 32768)
        self.assertTrue(all(tool["inputSchema"]["additionalProperties"] is False for tool in tools))
        self.assertFalse(any("runtime" in tool["name"] or "shell" in tool["name"] for tool in tools))

    def test_metadata_version_and_unknown_operations(self):
        old = request("server/discover")
        old["params"]["_meta"][VERSION_KEY] = "2025-11-25"
        self.assertEqual(self.server.handle(old)["error"]["code"], -32022)
        self.assertIn("error", self.server.handle({"jsonrpc": "2.0", "id": 1, "method": "initialize"}))
        for method in ("up", "shutdown", "shell", "roots/list"):
            self.assertEqual(self.server.handle(request(method))["error"]["code"], -32601)

    def test_final_wire_bounds_include_rpc_metadata_and_escaped_resources(self):
        for method, result in (
                ("tools/list", {"text": "x" * 32700}),
                ("resources/read", {"contents": [{"text": '"' * 40000}]}),
                ("tools/list", {"text": "é" * 16400})):
            with patch.object(self.server, "dispatch", return_value=result):
                response = self.server.handle(request(method))
            self.assertEqual(response["error"]["data"]["code"], "LIMIT_EXCEEDED")
            self.assertLess(len(canonical_json(response)) + 1, 32768)

    def test_strict_schema_no_arbitrary_arguments_or_secret_echo(self):
        for arguments in ({"root": "/tmp/secret"}, {"page_size": True}, {"page_size": 51}, {"profile": "bad\nvalue"}):
            response = self.server.handle(request("tools/call", name="context_describe", arguments=arguments))
            self.assertIn("error", response)
            self.assertNotIn("/tmp/secret", json.dumps(response))
        response = self.server.handle(request("tools/call", name="apply", arguments={}))
        self.assertIn("error", response)

    def test_cli_api_parity_and_on_demand_resource_fencing(self):
        response = self.server.handle(request("tools/call", name="context_describe", arguments={}))
        self.assertEqual(response["result"]["structuredContent"]["data"], self.service.describe())
        guidance = self.server.handle(request("tools/call", name="context_guidance", arguments={}))
        data = guidance["result"]["structuredContent"]["data"]
        uri = data["resources"][0]["uri"]
        result = self.server.handle(request("resources/read", uri=uri))
        self.assertIn("Synthetic guidance", result["result"]["contents"][0]["text"])
        (self.root / "sample.txt").write_text("changed\n")
        self.assertEqual(self.server.handle(request("resources/read", uri=uri))["error"]["data"]["code"], "STALE_COORDINATE")
        self.assertIn("error", self.server.handle(request("resources/read", uri="file:///tmp/example")))

    def test_search_and_historical_rename_resources_use_registered_snapshots(self):
        response = self.server.handle(request("tools/call", name="atrinik_search",
            arguments={"mode": "exact", "query": "original"}))
        item = response["result"]["structuredContent"]["data"]["items"][0]
        contents = self.server.handle(request("resources/read", uri=item["resource_uri"]))
        self.assertEqual(contents["result"]["contents"][0]["text"], "original\n")
        fixture.git(self.root, "mv", "sample.txt", "renamed.txt")
        fixture.git(self.root, "commit", "-m", "rename")
        response = self.server.handle(request("tools/call", name="atrinik_search",
            arguments={"mode": "history", "provenance": True, "path": "renamed.txt"}))
        data = response["result"]["structuredContent"]["data"]
        self.assertTrue(data["items"])
        for item in data["items"]:
            contents = self.server.handle(request("resources/read", uri=item["resource_uri"]))
            self.assertEqual(contents["result"]["contents"][0]["text"], "original\n")

    def test_provenance_at_head_reads_committed_bytes_in_dirty_checkout(self):
        (self.root / "sample.txt").write_text("committed replacement\n")
        fixture.git(self.root, "add", "sample.txt")
        fixture.git(self.root, "commit", "-m", "update sample")
        (self.root / "sample.txt").write_text("dirty replacement\n")
        for mode in ("history", "blame"):
            response = self.server.handle(request("tools/call", name="atrinik_search",
                arguments={"mode": mode, "provenance": True, "path": "sample.txt"}))
            items = response["result"]["structuredContent"]["data"]["items"]
            self.assertTrue(items)
            self.assertEqual(items[0]["commit"], items[0]["snapshot_commit"])
            contents = self.server.handle(request("resources/read", uri=items[0]["resource_uri"]))
            self.assertEqual(contents["result"]["contents"][0]["text"], "committed replacement\n")

    def test_shared_content_cross_profile_search_is_not_duplicated(self):
        fixture.repository(self.root / "content", "atrinik/content")
        response = self.server.handle(request("tools/call", name="atrinik_search", arguments={
            "mode": "exact", "query": "original", "selections": [
                {"profile": "default", "component": "content"}, {"profile": "classic", "component": "content"}]}))
        data = response["result"]["structuredContent"]["data"]
        self.assertEqual(len(data["items"]), 1)
        self.assertEqual(data["selected_profiles"], ["classic", "default"])

    def test_cancelled_request_does_not_run_git(self):
        event = threading.Event()
        event.set()
        with patch("atrinik_workspace.mcp_context.subprocess.Popen") as popen:
            response = self.server.handle(request("tools/call", name="context_resolve", arguments={}), event)
        self.assertEqual(response["error"]["data"]["code"], "CANCELLED")
        popen.assert_not_called()

    def test_protocol_frames_duplicate_keys_oversize_and_no_secret_errors(self):
        source = io.BytesIO(b'{"id":1,"id":2}\n' + b'[]\n')
        output = io.BytesIO()
        serve(self.server, source, output)
        responses = [json.loads(line) for line in output.getvalue().splitlines()]
        self.assertEqual(responses[0]["error"]["code"], -32700)
        self.assertIn("error", responses[1])
        output = io.BytesIO()
        serve(self.server, io.BytesIO(b"x" * 17000), output)
        self.assertEqual(len(output.getvalue().splitlines()), 1)
        self.assertLess(len(output.getvalue()), 256)

    def test_catalog_budget_and_duplicate_registration(self):
        extra = Tool("context_resolve", "duplicate", object_schema({}), lambda _: {})
        with self.assertRaises(ContractError):
            ContextServer(self.service, (extra,))
        extras = tuple(Tool(f"extra_{n}", "Read fixture", object_schema({}), lambda _: {}) for n in range(7))
        with self.assertRaises(ContractError):
            ContextServer(self.service, extras)

    def test_concurrent_reads_share_no_mutable_snapshot_cache(self):
        from concurrent.futures import ThreadPoolExecutor
        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(lambda _: self.server.handle(request("tools/call", name="context_resolve", arguments={})), range(12)))
        snapshots = {item["result"]["structuredContent"]["data"]["snapshot"] for item in results}
        self.assertEqual(len(snapshots), 1)
        self.assertFalse((self.root / "workspace").exists())

    def test_internal_errors_do_not_echo_exception(self):
        with patch.object(self.service, "describe", side_effect=ValueError("SYNTHETIC_SECRET")):
            result = self.server.handle(request("tools/call", name="context_describe", arguments={}))
        self.assertEqual(result["error"]["code"], -32603)
        self.assertNotIn("SYNTHETIC_SECRET", json.dumps(result))

    def test_stdio_receiver_cancels_inflight_work(self):
        import time
        from atrinik_workspace.mcp_context import check_request
        entered = threading.Event()
        def slow(_):
            entered.set()
            while True:
                check_request()
                time.sleep(0.001)
        server = ContextServer(self.service, (Tool("slow_fixture", "Synthetic bounded read", object_schema({}), slow),))
        first = canonical_json(request("tools/call", name="slow_fixture", arguments={})) + b"\n"
        cancellation = canonical_json({"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 1}}) + b"\n"
        class Input:
            step = 0
            def readline(self, maximum):
                self.step += 1
                if self.step == 1:
                    return first
                if self.step == 2:
                    if not entered.wait(2):
                        raise AssertionError("request was not executed")
                    return cancellation
                return b""
        output = io.BytesIO()
        serve(server, Input(), output)
        response = json.loads(output.getvalue())
        self.assertEqual(response["error"]["data"]["code"], "CANCELLED")
