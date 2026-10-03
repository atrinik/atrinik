# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Authenticated, bounded Streamable HTTP for the read-only context provider."""
from __future__ import annotations

import argparse
import base64
import binascii
import hashlib
import hmac
from http.client import LineTooLong
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import secrets
import socket
import ssl
import stat
import sys
import threading
import time

from .mcp_context import ContextService
from .mcp_contract import ContractError, canonical_json
from .mcp_legacy import LegacyServer, VERSIONS
from .mcp_server import ContextServer, PROTOCOL_VERSION, VERSION_KEY, _pairs, _shape

MAX_REQUEST = 16384
SESSION_LIMIT = 32
SESSION_TTL = 900
DEADLINE = 5


def read_token(path):
    """No command line bearer value, symlink, nonregular file or permissive mode."""
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o027 or info.st_size > 257:
            raise ValueError("invalid token file")
        value = os.read(fd, 258).removesuffix(b"\n")
        if not re.fullmatch(rb"[A-Za-z0-9_-]{32,256}", value):
            raise ValueError("invalid token file")
        return value
    finally:
        os.close(fd)


def tls_context(cert, key):
    """Load a private key through a pinned, regular, non-world-readable descriptor."""
    descriptors = []
    try:
        for path, private in ((cert, False), (key, True)):
            fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK)
            descriptors.append(fd)
            info = os.fstat(fd)
            if (not stat.S_ISREG(info.st_mode) or info.st_size > 65536
                    or (private and info.st_mode & 0o027)):
                raise ValueError("invalid TLS file")
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(*(f"/proc/self/fd/{fd}" for fd in descriptors), password=lambda: b"")
        return context
    finally:
        for fd in descriptors:
            os.close(fd)


class Session:
    def __init__(self, root, version, identity):
        self.provider = ContextServer(ContextService(root, authorization_identity=identity))
        self.codec = LegacyServer(self.provider) if version in VERSIONS else self.provider
        self.version = version
        self.touched = time.monotonic()
        self.active = {}
        self.closed = False

    def close(self):
        self.closed = True
        for event in self.active.values():
            event.set()
        with self.provider.resource_lock:
            self.provider.resources.clear()


class MCPHTTPServer(ThreadingHTTPServer):
    """Bound connections before creating threads; operation slots exclude notifications."""
    daemon_threads = True
    request_queue_size = 16

    def __init__(self, address, *, root, token, allowed_hosts, allowed_origins=(), tls=None):
        self.root = Path(root)
        self.tls = tls
        self.token_digest = hashlib.sha256(token).digest()
        self.modern = ContextServer(ContextService(self.root, authorization_identity="http:configured-principal"))
        self.allowed_hosts = frozenset(allowed_hosts)
        self.allowed_origins = frozenset(allowed_origins)
        if not self.allowed_hosts or any(not value or any(c.isspace() for c in value)
                                         for value in (*self.allowed_hosts, *self.allowed_origins)):
            raise ValueError("explicit host allowlist required")
        self.sessions = {}
        self.lock = threading.Lock()
        self.connections = threading.BoundedSemaphore(16)
        self.operations = threading.BoundedSemaphore(4)
        super().__init__(address, Handler)

    def process_request(self, request, client_address):
        if not self.connections.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.connections.release()
            raise

    def process_request_thread(self, request, client_address):
        # A wall clock limit defeats clients that trickle headers/body forever.
        def expire():
            try:
                request.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
        timer = threading.Timer(DEADLINE, expire)
        timer.daemon = True
        timer.start()
        request.settimeout(DEADLINE)
        try:
            if self.tls is not None:
                request = self.tls.wrap_socket(request, server_side=True)
            super().process_request_thread(request, client_address)
        except OSError:
            self.shutdown_request(request)
        finally:
            timer.cancel()
            self.connections.release()

    def handle_error(self, request, client_address):
        # Neither request paths, headers nor source exceptions enter access logs.
        pass

    def reap(self):
        now = time.monotonic()
        for key, session in list(self.sessions.items()):
            if now - session.touched >= SESSION_TTL:
                session.close()
                del self.sessions[key]

    def server_close(self):
        with self.lock:
            for session in self.sessions.values():
                session.close()
            self.sessions.clear()
        super().server_close()


class HeaderBudget:
    """Bound request line plus headers while reading, before HTTP parser allocation."""
    def __init__(self, stream):
        self.stream = stream
        self.remaining = MAX_REQUEST

    def readline(self, maximum=-1):
        limit = self.remaining + 1
        line = self.stream.readline(min(maximum, limit) if maximum >= 0 else limit)
        self.remaining -= len(line)
        if self.remaining < 0:
            raise LineTooLong("request headers")
        return line

    def __getattr__(self, name):
        return getattr(self.stream, name)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "Atrinik"
    sys_version = ""

    def setup(self):
        super().setup()
        self.rfile = HeaderBudget(self.rfile)

    def log_message(self, format, *args):
        pass

    def handle_expect_100(self):
        self.reply(417)
        return False

    def send_error(self, code, message=None, explain=None):
        self.reply(code)

    def reply(self, status, payload=None, *, session=None, extra=()):
        body = canonical_json(payload) if payload is not None else b""
        self.close_connection = True
        self.send_response(status)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        if body:
            self.send_header("Content-Type", "application/json")
        if session:
            self.send_header("MCP-Session-Id", session)
        for key, value in extra:
            self.send_header(key, value)
        self.end_headers()
        if body:
            self.wfile.write(body)

    def gate(self):
        # Duplicate security/framing headers are never interpreted ambiguously.
        for key in ("Host", "Origin", "Authorization", "Content-Length", "Content-Type",
                    "Accept", "MCP-Session-Id", "MCP-Protocol-Version", "Mcp-Method", "Mcp-Name", "Transfer-Encoding"):
            if len(self.headers.get_all(key, [])) > 1:
                self.reply(400)
                return False
        if sum(len(k) + len(v) + 4 for k, v in self.headers.items()) > MAX_REQUEST:
            self.reply(431)
            return False
        if self.headers.get("Host") not in self.server.allowed_hosts:
            self.reply(403)
            return False
        origin = self.headers.get("Origin")
        if origin is not None and origin not in self.server.allowed_origins:
            self.reply(403)
            return False
        if self.command == "GET" and self.path == "/healthz":
            self.reply(200, {"status": "ok"})
            return False
        supplied = self.headers.get("Authorization", "")
        candidate = supplied[7:].encode("utf-8") if supplied.startswith("Bearer ") else b""
        if not hmac.compare_digest(hashlib.sha256(candidate).digest(), self.server.token_digest):
            self.reply(401, extra=(("WWW-Authenticate", 'Bearer realm="atrinik"'),))
            return False
        if self.path != "/mcp":
            self.reply(404)
            return False
        version = self.headers.get("MCP-Protocol-Version")
        if version is not None and version not in (*VERSIONS, PROTOCOL_VERSION):
            self.reply(400, {"jsonrpc": "2.0", "id": None, "error": {"code": -32022,
                "message": "Unsupported protocol version", "data": {"supported": [*VERSIONS, PROTOCOL_VERSION]}}})
            return False
        return True

    def get_session(self):
        identifier = self.headers.get("MCP-Session-Id")
        with self.server.lock:
            self.server.reap()
            session = self.server.sessions.get(identifier)
            if session is not None:
                version = self.headers.get("MCP-Protocol-Version", session.version)
                if version != session.version:
                    self.reply(400)
                    return None
                session.touched = time.monotonic()
        if session is None:
            self.reply(404 if identifier is not None else 400)
        return session

    def do_GET(self):
        if not self.gate():
            return
        if self.headers.get("MCP-Protocol-Version") == PROTOCOL_VERSION or self.get_session() is not None:
            self.reply(405, extra=(("Allow", "POST, DELETE"),))

    def do_DELETE(self):
        if not self.gate():
            return
        if self.headers.get("MCP-Protocol-Version") == PROTOCOL_VERSION:
            self.reply(405, extra=(("Allow", "POST"),))
            return
        session = self.get_session()
        if session is not None:
            with self.server.lock:
                session.close()
                self.server.sessions.pop(self.headers["MCP-Session-Id"], None)
            self.reply(204)

    def do_POST(self):
        if not self.gate():
            return
        if self.headers.get("Transfer-Encoding") is not None:
            self.reply(400)
            return
        if self.headers.get("Content-Type", "").lower() not in ("application/json", "application/json; charset=utf-8"):
            self.reply(415)
            return
        accept = {item.strip().split(";", 1)[0] for item in self.headers.get("Accept", "").split(",")}
        if not {"application/json", "text/event-stream"}.issubset(accept):
            self.reply(406)
            return
        length = self.headers.get("Content-Length", "")
        if not re.fullmatch(r"[0-9]{1,8}", length):
            self.reply(411)
            return
        if not 0 < int(length) <= MAX_REQUEST:
            self.reply(413)
            return
        raw = self.rfile.read(int(length))
        try:
            if len(raw) != int(length):
                raise ValueError()
            request = json.loads(raw.decode("utf-8"), object_pairs_hook=_pairs,
                                 parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
            _shape(request)
        except (ValueError, RecursionError, ContractError):
            self.reply(400, {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Invalid JSON"}})
            return
        if not isinstance(request, dict) or request.get("jsonrpc") != "2.0" or not isinstance(request.get("method"), str):
            self.reply(400, {"jsonrpc": "2.0", "id": None, "error": {"code": -32600, "message": "Invalid request"}})
            return
        method = request["method"]
        params = request.get("params", {})
        meta = params.get("_meta", {}) if isinstance(params, dict) else {}
        if (self.headers.get("MCP-Protocol-Version") == PROTOCOL_VERSION
                or (isinstance(meta, dict) and VERSION_KEY in meta)):
            self.modern_request(request)
            return
        new = method == "initialize" and "MCP-Session-Id" not in self.headers
        if new:
            params = request.get("params", {})
            version = params.get("protocolVersion") if method == "initialize" and isinstance(params, dict) else PROTOCOL_VERSION
            if version not in (*VERSIONS, PROTOCOL_VERSION) or (method == "initialize" and version == PROTOCOL_VERSION):
                self.reply(400)
                return
            if self.headers.get("MCP-Protocol-Version", version) != version:
                self.reply(400)
                return
            identifier = secrets.token_urlsafe(32)
            with self.server.lock:
                self.server.reap()
                if len(self.server.sessions) >= SESSION_LIMIT:
                    self.reply(503, extra=(("Retry-After", "1"),))
                    return
                session = Session(self.server.root, version, "http:" + identifier)
                self.server.sessions[identifier] = session
        else:
            identifier = self.headers.get("MCP-Session-Id")
            session = self.get_session()
            if session is None:
                return
        if "id" not in request:
            if new:
                with self.server.lock:
                    session.close()
                    self.server.sessions.pop(identifier, None)
                self.reply(400)
                return
            self.notification(session, request)
            return
        request_id = request["id"]
        if not ((type(request_id) is int and abs(request_id) <= 2**53 - 1)
                or (isinstance(request_id, str) and len(request_id) <= 128 and request_id.isprintable())):
            result = {"jsonrpc": "2.0", "id": None, "error": {"code": -32600, "message": "Invalid request identifier"}}
        else:
            with self.server.lock:
                admitted = not session.closed and request_id not in session.active and self.server.operations.acquire(blocking=False)
                if admitted:
                    event = threading.Event()
                    session.active[request_id] = event
            if not admitted:
                if new:
                    with self.server.lock:
                        session.close()
                        self.server.sessions.pop(identifier, None)
                self.reply(503, extra=(("Retry-After", "1"),))
                return
            try:
                result = session.codec.handle(request, event)
            finally:
                with self.server.lock:
                    session.active.pop(request_id, None)
                self.server.operations.release()
        if new and "error" in result:
            with self.server.lock:
                session.close()
                self.server.sessions.pop(identifier, None)
            identifier = None
        self.reply(200, result, session=identifier if new else None)

    def modern_request(self, request):
        params = request.get("params", {})
        meta = params.get("_meta", {}) if isinstance(params, dict) else {}
        identifier = request.get("id")
        valid_id = ((type(identifier) is int and abs(identifier) <= 2**53 - 1)
                    or (isinstance(identifier, str) and len(identifier) <= 128 and identifier.isprintable()))
        if not valid_id:
            # Modern HTTP defines no core client notifications, including cancellation.
            self.reply(400)
            return
        method = request["method"]
        valid_headers = (isinstance(meta, dict)
                         and self.headers.get("MCP-Protocol-Version") == meta.get(VERSION_KEY) == PROTOCOL_VERSION
                         and self.headers.get("Mcp-Method") == method)
        if method in ("tools/call", "resources/read", "prompts/get"):
            name = self.headers.get("Mcp-Name")
            if isinstance(name, str) and name.startswith("=?base64?") and name.endswith("?="):
                try:
                    name = base64.b64decode(name[9:-2], validate=True).decode("utf-8")
                except (ValueError, UnicodeError, binascii.Error):
                    name = None
            elif name is not None and (name != name.strip() or any(ord(c) < 32 or ord(c) > 126 for c in name)):
                name = None
            valid_headers = valid_headers and isinstance(name, str) and name == params.get("uri" if method == "resources/read" else "name")
        if not valid_headers:
            self.reply(400, {"jsonrpc": "2.0", "id": identifier,
                            "error": {"code": -32020, "message": "Required header missing or mismatched"}})
            return
        if not self.server.operations.acquire(blocking=False):
            self.reply(503, extra=(("Retry-After", "1"),))
            return
        try:
            # No protocol session or shared cancellation map: duplicate IDs on
            # independent modern requests cannot interfere with each other.
            result = self.server.modern.handle(request, threading.Event())
        finally:
            self.server.operations.release()
        status = 404 if result.get("error", {}).get("code") == -32601 else 200
        self.reply(status, result)

    def notification(self, session, request):
        params = request.get("params", {})
        if set(request) - {"jsonrpc", "method", "params"} or not isinstance(params, dict):
            self.reply(400)
            return
        if request["method"] == "notifications/initialized":
            if not isinstance(session.codec, LegacyServer) or params:
                self.reply(400)
                return
            session.codec.initialized(request)
        elif request["method"] == "notifications/cancelled":
            request_id = params.get("requestId")
            if (set(params) - {"requestId", "reason", "_meta"}
                    or type(request_id) not in (int, str)):
                self.reply(400)
                return
            with self.server.lock:
                if request_id in session.active:
                    session.active[request_id].set()
        # Unknown well-formed notifications have no effect and no RPC reply.
        self.reply(202)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--token-file", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--allowed-host", action="append", required=True)
    parser.add_argument("--allowed-origin", action="append", default=[])
    parser.add_argument("--tls-cert", type=Path)
    parser.add_argument("--tls-key", type=Path)
    args = parser.parse_args(argv)
    try:
        token = read_token(args.token_file)
        if bool(args.tls_cert) != bool(args.tls_key):
            raise ValueError("TLS certificate and key must be paired")
        tls = tls_context(args.tls_cert, args.tls_key) if args.tls_cert else None
        ContextService(args.root)
        server = MCPHTTPServer((args.host, args.port), root=args.root, token=token,
                               allowed_hosts=args.allowed_host, allowed_origins=args.allowed_origin, tls=tls)
    except (OSError, ValueError, ContractError):
        print("atrinik-http: invalid or unavailable server configuration", file=sys.stderr)
        return 1
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
