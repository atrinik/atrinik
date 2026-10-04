# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
import concurrent.futures
import http.client
import json
from pathlib import Path
import socket
import ssl
import subprocess
import threading
import time
import unittest
from unittest.mock import patch

from atrinik_workspace.mcp_http import ConnectionDeadline, Handler, MCPHTTPServer, read_token, tls_context
from atrinik_workspace.mcp_context import check_request, request_scope
from atrinik_workspace.mcp_contract import canonical_json
from atrinik_workspace.mcp_http_health import main as health_check
from atrinik_workspace.mcp_server import VERSION_KEY, CAPABILITIES_KEY, PROTOCOL_VERSION
from tests import test_mcp_context as fixture

TOKEN = b"test-only-" + b"x" * 40


class HTTPTests(unittest.TestCase):
    def setUp(self):
        fixture.ContextFixture.setUp(self)
        self.http = MCPHTTPServer(("127.0.0.1", 0), root=self.root, token=TOKEN,
                                  allowed_hosts=["localhost"], allowed_origins=["https://allowed.invalid"])
        self.thread = threading.Thread(target=self.http.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop)

    def stop(self):
        self.http.shutdown()
        self.http.server_close()
        self.thread.join(timeout=2)

    def send(self, request=None, *, method="POST", path="/mcp", session=None, headers=None, raw=None):
        connection = http.client.HTTPConnection(*self.http.server_address, timeout=7)
        chosen = {"Host": "localhost", "Authorization": "Bearer " + TOKEN.decode(),
                  "Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if session:
            chosen["MCP-Session-Id"] = session
        chosen.update(headers or {})
        for key in list(chosen):
            if chosen[key] is None:
                del chosen[key]
        body = raw if raw is not None else (json.dumps(request) if request is not None else None)
        try:
            connection.request(method, path, body, chosen)
            response = connection.getresponse()
            data = response.read()
            return response.status, dict(response.getheaders()), json.loads(data) if data else None
        finally:
            connection.close()

    def initialize(self, version="2025-11-25"):
        status, headers, data = self.send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": version, "capabilities": {}, "clientInfo": {"name": "fixture", "version": "1"}}})
        self.assertEqual(status, 200, data)
        self.assertEqual(data["result"]["protocolVersion"], version)
        session = headers["MCP-Session-Id"]
        self.assertEqual(self.send({"jsonrpc": "2.0", "method": "notifications/initialized"}, session=session)[0], 202)
        return session

    def rpc(self, session, method="tools/list", **params):
        return self.send({"jsonrpc": "2.0", "id": 2, "method": method, "params": params}, session=session)

    def test_native_legacy_discovery_and_delete(self):
        for version in ("2025-06-18", "2025-11-25"):
            session = self.initialize(version)
            status, headers, data = self.rpc(session)
            self.assertEqual(status, 200)
            self.assertEqual(headers["Content-Type"], "application/json")
            self.assertEqual(len(data["result"]["tools"]), 7)
            self.assertNotIn("resultType", data["result"])
            self.assertEqual(self.send(method="GET", session=session)[0], 405)
            self.assertEqual(self.send(method="DELETE", session=session)[0], 204)
            self.assertEqual(self.rpc(session)[0], 404)

    def test_auth_host_origin_and_health_do_not_disclose_catalog(self):
        for headers, expected in (({"Authorization": "Bearer invalid"}, 401),
                                   ({"Authorization": None}, 401),
                                   ({"Host": "attacker.invalid"}, 403),
                                   ({"Origin": "https://attacker.invalid"}, 403),
                                   ({"Origin": "null"}, 403)):
            status, _, data = self.send({"jsonrpc": "2.0", "id": 1, "method": "tools/list"}, headers=headers)
            self.assertEqual(status, expected)
            self.assertIsNone(data)
        self.assertEqual(self.send(method="GET", path="/healthz", headers={"Authorization": None})[2], {"status": "ok"})
        self.assertEqual(self.send(method="GET", path="/healthz?secret", headers={"Authorization": None})[0], 401)

    def test_framing_json_and_protocol_rejections(self):
        session = self.initialize()
        cases = [(b'{}', {"Content-Type": "text/plain"}, 415),
                 (b'{}', {"Accept": "application/json"}, 406),
                 (b'{}', {"Transfer-Encoding": "chunked"}, 400),
                 (b'x' * 16385, {}, 413),
                 (b'{"jsonrpc":"2.0","jsonrpc":"2.0"}', {}, 400),
                 (b'[]', {}, 400), (b'NaN', {}, 400),
                 (b'{}', {"MCP-Protocol-Version": "1999-01-01"}, 400),
                 (b'{}', {"MCP-Protocol-Version": "2025-06-18"}, 400)]
        for raw, headers, expected in cases:
            self.assertEqual(self.send(raw=raw, headers=headers, session=session)[0], expected)
        self.assertEqual(self.rpc(None)[0], 400)
        self.assertEqual(self.rpc("unknown")[0], 404)

    def test_http_search_resources_continue_within_wire_limit(self):
        payload = 'é😀"\\\n' * 24000
        (self.root / "sample.txt").write_text(payload)
        fixture.git(self.root, "add", "sample.txt")
        fixture.git(self.root, "commit", "-m", "large search source fixture")
        first, second = self.initialize(), self.initialize()
        response = self.rpc(first, "tools/call", name="atrinik_search",
                            arguments={"mode": "filename", "query": "sample.txt"})[2]
        uri = response["result"]["structuredContent"]["data"]["items"][0]["resource_uri"]
        parts, offset = [], 0
        while uri:
            status, _, response = self.rpc(first, "resources/read", uri=uri)
            self.assertEqual(status, 200)
            self.assertNotIn("error", response)
            self.assertLessEqual(len(canonical_json(response)) + 1, 65536)
            item = response["result"]["contents"][0]
            self.assertEqual(item["_meta"]["atrinik/offset_characters"], offset)
            parts.append(item["text"])
            offset += len(item["text"])
            uri = item["_meta"]["atrinik/next_uri"]
            if uri and len(parts) == 1:
                self.assertEqual(self.rpc(second, "resources/read", uri=uri)[2]["error"]["data"]["code"], "NOT_FOUND")
        self.assertGreater(len(parts), 2)
        self.assertEqual("".join(parts), payload)

    def test_http_changes_pages_traverse_long_paths(self):
        expected = fixture.long_changed_paths(self.root)
        session = self.initialize()
        seen, cursor = [], None
        while True:
            arguments = {"page_size": 50, **({"cursor": cursor} if cursor else {})}
            status, _, response = self.rpc(session, "tools/call", name="context_changes", arguments=arguments)
            self.assertEqual(status, 200)
            self.assertNotIn("error", response)
            self.assertLessEqual(len(canonical_json(response)) + 1, 32768)
            page = response["result"]["structuredContent"]["data"]
            self.assertEqual(page["returned_records"], len(page["items"]))
            self.assertEqual(page["truncated"], page["next_cursor"] is not None)
            seen.extend(item["path"] for item in page["items"])
            cursor = page["next_cursor"]
            if cursor is None:
                break
        self.assertEqual(set(seen), expected)
        self.assertEqual(len(seen), len(expected))

    def test_resource_and_cancellation_session_isolation(self):
        first, second = self.initialize(), self.initialize()
        result = self.rpc(first, "tools/call", name="context_guidance", arguments={})[2]
        uri = result["result"]["structuredContent"]["data"]["resources"][0]["uri"]
        self.assertIn("Synthetic guidance", self.rpc(first, "resources/read", uri=uri)[2]["result"]["contents"][0]["text"])
        self.assertEqual(self.rpc(second, "resources/read", uri=uri)[2]["error"]["data"]["code"], "NOT_FOUND")
        started = threading.Event()
        observed = []
        def blocking(request, event):
            started.set()
            observed.append(event.wait(2))
            return {"jsonrpc": "2.0", "id": 2, "result": {}}
        with patch.object(self.http.sessions[first].codec, "handle", side_effect=blocking):
            with concurrent.futures.ThreadPoolExecutor() as pool:
                future = pool.submit(self.rpc, first)
                self.assertTrue(started.wait(1))
                notice = {"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 2}}
                self.assertEqual(self.send(notice, session=second)[0], 202)
                self.assertFalse(self.http.sessions[first].active[2].is_set())
                self.assertEqual(self.send(notice, session=first)[0], 202)
                self.assertEqual(future.result()[0], 200)
        self.assertEqual(observed, [True])

    def test_modern_stateless_headers_and_strict_metadata(self):
        request = {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {
            "_meta": {VERSION_KEY: PROTOCOL_VERSION, CAPABILITIES_KEY: {}}}}
        headers = {"MCP-Protocol-Version": PROTOCOL_VERSION, "Mcp-Method": "tools/list"}
        status, response_headers, data = self.send(request, headers=headers)
        self.assertEqual(status, 200)
        self.assertEqual(data["result"]["resultType"], "complete")
        self.assertEqual(len(data["result"]["tools"]), 7)
        self.assertNotIn("MCP-Session-Id", response_headers)
        self.assertEqual(len(self.http.sessions), 0)
        for overrides in ({"Mcp-Method": "tools/call"}, {"Mcp-Method": None}, {"MCP-Protocol-Version": None}):
            result = self.send(request, headers={**headers, **overrides})
            self.assertEqual(result[0], 400)
            self.assertEqual(result[2]["error"]["code"], -32020)
        request["method"] = "missing/method"
        result = self.send(request, headers={**headers, "Mcp-Method": "missing/method"})
        self.assertEqual(result[0], 404)
        self.assertEqual(result[2]["error"]["code"], -32601)
        request["method"] = "tools/call"
        request["params"].update(name="context_describe", arguments={})
        headers.update({"Mcp-Method": "tools/call", "Mcp-Name": "=?base64?Y29udGV4dF9kZXNjcmliZQ==?="})
        self.assertEqual(self.send(request, headers=headers)[0], 200)
        self.assertEqual(self.send(request, headers={**headers, "Mcp-Name": "other"})[2]["error"]["code"], -32020)
        del request["params"]["_meta"][CAPABILITIES_KEY]
        self.assertIn("error", self.send(request, headers=headers)[2])
        self.assertEqual(self.send(method="GET", headers=headers)[0], 405)
        self.assertEqual(self.send(method="DELETE", headers=headers)[0], 405)

    def test_session_limit_expiry_and_delete_cancel(self):
        with patch("atrinik_workspace.mcp_http.SESSION_LIMIT", 1):
            session = self.initialize()
            request = {"jsonrpc": "2.0", "id": 3, "method": "initialize", "params": {
                "protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "test", "version": "1"}}}
            self.assertEqual(self.send(request)[0], 503)
            event = threading.Event()
            self.http.sessions[session].active[10] = event
            self.http.sessions[session].touched -= 901
            self.assertEqual(self.rpc(session)[0], 404)
            self.assertTrue(event.is_set())
            self.assertEqual(len(self.http.sessions), 0)

    def test_operation_capacity_preserves_notification_and_health(self):
        session = self.initialize()
        for _ in range(4):
            self.http.operations.acquire()
        try:
            self.assertEqual(self.rpc(session)[0], 503)
            self.assertEqual(self.send({"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 1}}, session=session)[0], 202)
            self.assertEqual(self.send(method="GET", path="/healthz")[0], 200)
        finally:
            for _ in range(4):
                self.http.operations.release()

    def assert_slots_released(self):
        until = time.monotonic() + 2
        while time.monotonic() < until:
            acquired = sum(self.http.connections.acquire(blocking=False) for _ in range(16))
            for _ in range(acquired):
                self.http.connections.release()
            if acquired == 16:
                break
            time.sleep(0.01)
        self.assertEqual(acquired, 16)
        acquired = sum(self.http.operations.acquire(blocking=False) for _ in range(4))
        for _ in range(acquired):
            self.http.operations.release()
        self.assertEqual(acquired, 4)

    def test_ingress_does_not_consume_provider_or_timeout_response_budget(self):
        session = self.initialize()
        for modern in (False, True):
            for timeout in (False, True):
                with self.subTest(modern=modern, timeout=timeout):
                    request = {"jsonrpc": "2.0", "id": 7, "method": "tools/list", "params": {}}
                    headers = {"Host": "localhost", "Authorization": "Bearer " + TOKEN.decode(),
                               "Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
                    if modern:
                        request["params"]["_meta"] = {VERSION_KEY: PROTOCOL_VERSION, CAPABILITIES_KEY: {}}
                        headers.update({"MCP-Protocol-Version": PROTOCOL_VERSION, "Mcp-Method": "tools/list"})
                        provider = self.http.modern
                    else:
                        headers["MCP-Session-Id"] = session
                        provider = self.http.sessions[session].provider
                    body = json.dumps(request).encode()
                    headers["Content-Length"] = str(len(body))
                    def dispatch(*args):
                        time.sleep(0.3)
                        check_request()
                        return {"tools": []}
                    def scope(event):
                        return request_scope(event, timeout_ms=250 if timeout else 450)
                    with patch("atrinik_workspace.mcp_http.DEADLINE", 0.35), \
                            patch("atrinik_workspace.mcp_http.PROVIDER_DEADLINE", 0.45), \
                            patch("atrinik_workspace.mcp_http.RESPONSE_DEADLINE", 0.2), \
                            patch("atrinik_workspace.mcp_server.request_scope", side_effect=scope), \
                            patch.object(provider, "dispatch", side_effect=dispatch):
                        connection = http.client.HTTPConnection(*self.http.server_address, timeout=2)
                        try:
                            started = time.monotonic()
                            connection.request("POST", "/mcp", headers=headers)
                            time.sleep(0.15)
                            connection.send(body)
                            response = connection.getresponse()
                            payload = json.loads(response.read())
                            self.assertEqual(response.status, 200)
                            if timeout:
                                self.assertEqual(payload["error"]["data"]["code"], "TIMEOUT")
                            else:
                                self.assertEqual(payload["result"]["tools"], [])
                            self.assertLess(time.monotonic() - started, 1.5)
                        finally:
                            connection.close()
                    self.assert_slots_released()

    def test_expiry_cancels_operation_and_releases_slots(self):
        session = self.initialize()
        for modern in (False, True):
            observed = []
            def blocked(request, event):
                observed.append(event.wait(1))
                return {"jsonrpc": "2.0", "id": 2, "result": {}}
            provider = self.http.modern if modern else self.http.sessions[session].codec
            with patch("atrinik_workspace.mcp_http.PROVIDER_DEADLINE", 0.1), \
                    patch("atrinik_workspace.mcp_http.RESPONSE_DEADLINE", 0.1), \
                    patch.object(provider, "handle", side_effect=blocked):
                with self.assertRaises(http.client.RemoteDisconnected):
                    if modern:
                        self.send({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {
                            "_meta": {VERSION_KEY: PROTOCOL_VERSION, CAPABILITIES_KEY: {}}}},
                            headers={"MCP-Protocol-Version": PROTOCOL_VERSION, "Mcp-Method": "tools/list"})
                    else:
                        self.rpc(session)
            self.assert_slots_released()
            self.assertEqual(observed, [True])
            self.assertEqual(self.http.sessions[session].active, {})

    def test_stalled_response_has_absolute_output_bound(self):
        session = self.initialize()
        setup = Handler.setup
        def small_buffer(handler):
            setup(handler)
            handler.connection.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
        # Synthetic oversized output forces a blocked send even on loopback.
        result = {"jsonrpc": "2.0", "id": 2, "result": {"text": "x" * (8 * 1024 * 1024)}}
        body = json.dumps({"jsonrpc": "2.0", "id": 2, "method": "tools/list"}).encode()
        with patch("atrinik_workspace.mcp_http.RESPONSE_DEADLINE", 0.1), \
                patch.object(Handler, "setup", small_buffer), \
                patch.object(self.http.sessions[session].codec, "handle", return_value=result):
            with socket.create_connection(self.http.server_address, timeout=2) as connection:
                connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                connection.sendall(("POST /mcp HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer "
                    + TOKEN.decode() + "\r\nContent-Type: application/json\r\nAccept: application/json, text/event-stream"
                    + "\r\nMCP-Session-Id: " + session + "\r\nContent-Length: " + str(len(body)) + "\r\n\r\n").encode() + body)
                time.sleep(0.3)
                self.assert_slots_released()

    def test_body_trickle_does_not_extend_ingress_and_connection_capacity(self):
        with patch("atrinik_workspace.mcp_http.DEADLINE", 0.2):
            with socket.create_connection(self.http.server_address, timeout=2) as connection:
                connection.sendall(("POST /mcp HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer "
                    + TOKEN.decode() + "\r\nContent-Type: application/json\r\nAccept: application/json, text/event-stream"
                    + "\r\nContent-Length: 100\r\n\r\n{").encode())
                time.sleep(0.12)
                connection.sendall(b" ")
                self.assertEqual(connection.recv(4096), b"")
        self.assert_slots_released()
        for _ in range(16):
            self.http.connections.acquire()
        try:
            with socket.create_connection(self.http.server_address, timeout=2) as connection:
                self.assertEqual(connection.recv(4096), b"")
        finally:
            for _ in range(16):
                self.http.connections.release()
        self.assert_slots_released()

    def test_stale_expiry_cannot_close_next_phase(self):
        first, second = socket.socketpair()
        try:
            with patch("atrinik_workspace.mcp_http.DEADLINE", 0.3):
                budget = ConnectionDeadline(first)
                generation = budget.generation
                try:
                    budget.phase(0.5, final=True)
                    budget.expire(generation)
                    first.sendall(b"ok")
                    self.assertEqual(second.recv(2), b"ok")
                    self.assertFalse(budget.expired)
                    # A delayed timer callback cannot let an expired phase revive.
                    budget.until = time.monotonic() - 1
                    with self.assertRaises(TimeoutError):
                        budget.phase(0.5)
                    event = threading.Event()
                    with self.assertRaises(TimeoutError):
                        budget.attach(event)
                    self.assertTrue(event.is_set())
                finally:
                    budget.close()
        finally:
            first.close()
            second.close()

    def test_tokenfile_rejects_world_access_symlink_and_weak_secret(self):
        path = Path(self.temp.name) / "token"
        path.write_bytes(TOKEN + b"\n")
        path.chmod(0o600)
        self.assertEqual(read_token(path), TOKEN)
        path.chmod(0o644)
        with self.assertRaises(ValueError):
            read_token(path)
        path.chmod(0o600)
        alias = path.with_name("alias")
        alias.symlink_to(path)
        with self.assertRaises(OSError):
            read_token(alias)
        path.write_bytes(b"short")
        with self.assertRaises(ValueError):
            read_token(path)

    def test_tls_actual_handshake_and_private_key_permissions(self):
        cert, key = (Path(self.temp.name) / name for name in ("cert.pem", "key.pem"))
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                        "-keyout", str(key), "-out", str(cert)], check=True, capture_output=True)
        key.chmod(0o600)
        self.http.tls = tls_context(cert, key)
        self.assertEqual(self.http.tls.minimum_version, ssl.TLSVersion.TLSv1_2)
        context = ssl.create_default_context(cafile=str(cert))
        connection = http.client.HTTPSConnection("localhost", self.http.server_address[1], context=context)
        connection.request("GET", "/healthz", headers={"Host": "localhost"})
        response = connection.getresponse()
        self.assertEqual(response.status, 200)
        response.read()
        connection.close()
        self.http.allowed_hosts |= {"localhost:" + str(self.http.server_address[1])}
        self.assertEqual(health_check(["--ca-file", str(cert), "--port", str(self.http.server_address[1])]), 0)
        with patch("atrinik_workspace.mcp_http.DEADLINE", 0.1):
            with socket.create_connection(self.http.server_address, timeout=2) as stalled:
                # No ClientHello: TLS must still retain the ingress wall clock limit.
                self.assertEqual(stalled.recv(4096), b"")
        self.assert_slots_released()
        key.chmod(0o644)
        with self.assertRaises(ValueError):
            tls_context(cert, key)

    def test_duplicate_security_header_rejected_and_slow_connection_expires(self):
        with socket.create_connection(self.http.server_address) as connection:
            connection.sendall(b"GET /healthz HTTP/1.1\r\nHost: localhost\r\nHost: attacker\r\n\r\n")
            self.assertIn(b"400", connection.recv(4096).split(b"\r\n")[0])
        with socket.create_connection(self.http.server_address) as connection:
            connection.sendall(b"GET /healthz HTTP/1.1\r\nHost: localhost\r\nX-Padding: " + b"x" * 17000 + b"\r\n\r\n")
            self.assertIn(b"431", connection.recv(4096).split(b"\r\n")[0])
        with patch("atrinik_workspace.mcp_http.DEADLINE", 0.1):
            with socket.create_connection(self.http.server_address, timeout=2) as connection:
                connection.sendall(b"POST /mcp HTTP/1.1\r\nHost: local")
                self.assertEqual(connection.recv(4096), b"")


if __name__ == "__main__":
    unittest.main()
