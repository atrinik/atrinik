# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Separate, opt-in stdio server for approved runtime publications."""
import argparse
from pathlib import Path
import sys

from .mcp_context import ContextService, check_request
from .mcp_contract import ContractError
from .mcp_runtime import RuntimeApproval, RuntimeService, _decode, _read, runtime_tools
from .mcp_server import ContextServer, serve


def configured_server(root: Path, approvals_path: Path, runtime_root: Path | None = None):
    """Only process startup configuration grants access; no tool can approve it."""
    admitted_bytes = _read(approvals_path, lambda: None)
    value = _decode(admitted_bytes)
    if (set(value) != {"schema_version", "authorization_identity", "approvals"}
            or value["schema_version"] != 1 or not isinstance(value["approvals"], list)
            or len(value["approvals"]) > 1000
            or not isinstance(value["authorization_identity"], str)):
        raise ContractError("INVALID_ARGUMENT", "observation configuration is invalid")
    approvals = []
    for record in value["approvals"]:
        if not isinstance(record, dict) or set(record) != {"name", "profile", "generation", "spec_sha256"}:
            raise ContractError("INVALID_ARGUMENT", "observation approval is invalid")
        approvals.append(RuntimeApproval(**record))
    context = ContextService(root, authorization_identity=value["authorization_identity"])
    service = RuntimeService(context, approvals=approvals, enabled=True, runtime_root=runtime_root)
    class ApprovedServer(ContextServer):
        def dispatch(self, method, params):
            def authorized():
                try:
                    current = _read(approvals_path, check_request)
                except ContractError:
                    raise ContractError("UNAUTHORIZED", "observation approval is unavailable") from None
                if current != admitted_bytes:
                    raise ContractError("UNAUTHORIZED", "observation approval changed; restart required")
            authorized()
            result = super().dispatch(method, params)
            authorized()
            return result
    return ApprovedServer(context, runtime_tools(service), include_context_tools=False, server_name="atrinik-observe")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--approvals", type=Path, required=True)
    parser.add_argument("--runtime-root", type=Path)
    args = parser.parse_args(argv)
    try:
        server = configured_server(args.root, args.approvals, args.runtime_root)
        serve(server, sys.stdin.buffer, sys.stdout.buffer)
        return 0
    except (ContractError, OSError, TypeError):
        print("atrinik-observe: configured observations unavailable", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
