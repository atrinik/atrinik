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

from atrinik_workspace.mcp_http import MCPHTTPServer, read_token, tls_context
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
