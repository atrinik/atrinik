# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT

"""Explicit legacy MCP wire facade; all inspection stays in the pinned provider."""
from __future__ import annotations

import json
import threading

from atrinik_workspace.mcp_contract import ContractError, canonical_json
from atrinik_workspace.mcp_server import (
    CAPABILITIES_KEY, PROTOCOL_VERSION, VERSION_KEY, _pairs, _shape, serve,
)

VERSIONS = ("2025-06-18", "2025-11-25")
METHODS = frozenset(("tools/list", "tools/call", "resources/list",
                     "resources/templates/list", "resources/read"))


class LegacyServer:
    def __init__(self, provider):
        self.provider = provider
        self.phase = "new"
        self.lock = threading.Lock()

    def initialized(self, request):
        # Notifications have no response. Malformed or premature ones have no effect.
        if (set(request) - {"jsonrpc", "method", "params"}
                or request.get("jsonrpc") != "2.0"
                or request.get("method") != "notifications/initialized"
                or request.get("params", {}) != {}):
            return
        with self.lock:
            if self.phase == "initializing":
                self.phase = "ready"

    def modern(self, request, method, params, cancelled):
        params = dict(params)
        params["_meta"] = {VERSION_KEY: PROTOCOL_VERSION, CAPABILITIES_KEY: {}}
        result = self.provider.handle(
            {"jsonrpc": "2.0", "id": request["id"], "method": method, "params": params},
            cancelled,
        )
        if "result" in result:
            result["result"].pop("resultType", None)
            result["result"].pop("_meta", None)
        return result

    def handle(self, request, cancelled=None):
        identifier = request.get("id") if isinstance(request, dict) else None
        valid_id = ((type(identifier) is int and abs(identifier) <= 2**53 - 1)
                    or (isinstance(identifier, str) and len(identifier) <= 128
                        and identifier.isprintable()))
        response = {"jsonrpc": "2.0", "id": identifier if valid_id else None}
        try:
            if (not isinstance(request, dict) or request.get("jsonrpc") != "2.0"
                    or not valid_id or set(request) - {"jsonrpc", "id", "method", "params"}):
                raise ContractError("INVALID_ARGUMENT", "invalid JSON-RPC request")
            if len(canonical_json(request)) > 16384:
                raise ContractError("LIMIT_EXCEEDED", "request exceeds byte limit")
            _shape(request)
            params = request.get("params", {})
            if not isinstance(params, dict) or not isinstance(params.get("_meta", {}), dict):
                raise ContractError("INVALID_ARGUMENT", "invalid request parameters")
            method = request.get("method")
            if method == "ping":
                if set(params) - {"_meta"}:
                    raise ContractError("INVALID_ARGUMENT", "invalid ping parameters")
                response["result"] = {}
            elif method == "initialize":
                if (set(params) - {"protocolVersion", "capabilities", "clientInfo", "_meta"}
                        or not isinstance(params.get("capabilities"), dict)
                        or not isinstance(params.get("clientInfo"), dict)):
                    raise ContractError("INVALID_ARGUMENT", "invalid initialization parameters")
                info = params["clientInfo"]
                if any(not isinstance(info.get(key), str) or not info[key]
                       for key in ("name", "version")):
                    raise ContractError("INVALID_ARGUMENT", "invalid client information")
                version = params.get("protocolVersion")
                if version not in VERSIONS:
                    raise ContractError("UNSUPPORTED_OPERATION", "unsupported legacy protocol version")
                with self.lock:
                    if self.phase != "new":
                        raise ContractError("INVALID_ARGUMENT", "connection already initialized")
                    modern = self.modern(request, "server/discover", {}, cancelled)
                    if "error" in modern:
                        return modern
                    discovered = modern["result"]
                    response["result"] = {
                        "protocolVersion": version,
                        "serverInfo": {"name": self.provider.server_name, "version": "1.0.0"},
                        "capabilities": discovered["capabilities"],
                        "instructions": discovered.get("instructions", ""),
                    }
                    self.phase = "initializing"
            else:
                with self.lock:
                    ready = self.phase == "ready"
                if not ready:
                    raise ContractError("INVALID_ARGUMENT", "initialization is incomplete")
                if not isinstance(method, str) or method not in METHODS:
                    raise ContractError("UNSUPPORTED_OPERATION", "method unavailable")
                response = self.modern(request, method, params, cancelled)
        except ContractError as error:
            response["error"] = {"code": -32601 if error.code == "UNSUPPORTED_OPERATION" else -32602,
                                 "message": error.safe_message, "data": {"code": error.code}}
        except Exception:
            response["error"] = {"code": -32603, "message": "Inspection failed", "data": {"code": "INTERNAL"}}
        maximum = 65536 if isinstance(request, dict) and request.get("method") == "resources/read" else 32768
        if len(canonical_json(response)) + 1 > maximum:
            return {"jsonrpc": "2.0", "id": identifier if valid_id else None,
                    "error": {"code": -32602, "message": "Response exceeds wire byte limit",
                              "data": {"code": "LIMIT_EXCEEDED"}}}
        return response


class LifecycleInput:
    """Observe only initialized; the canonical receiver still handles cancellation."""
    def __init__(self, stream, server):
        self.stream = stream
        self.server = server

    def readline(self, maximum):
        while True:
            line = self.stream.readline(maximum)
            if not line or len(line) > 16385 or not line.endswith(b"\n"):
                return line
            try:
                request = json.loads(line, object_pairs_hook=_pairs,
                                     parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
                _shape(request)
            except (ValueError, RecursionError, ContractError):
                return line  # Canonical receiver emits its bounded parsing error.
            if (isinstance(request, dict) and "id" not in request
                    and request.get("method") == "notifications/initialized"):
                self.server.initialized(request)
                continue
            return line


def run(provider, input_stream, output_stream):
    server = LegacyServer(provider)
    serve(server, LifecycleInput(input_stream, server), output_stream)
    return 0
