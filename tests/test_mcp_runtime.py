from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from atrinik_workspace.mcp_contract import ContractError, Coordinate
from atrinik_workspace.mcp_runtime import RuntimeApproval, RuntimeService, MAX_BYTES


class Snapshot:
    coordinate = Coordinate("atrinik/atrinik", "main", "a" * 40, "primary", None)

    def assert_current(self):
        pass


class Context:
    authorization_identity = "synthetic-actor"

    def __init__(self, root):
        self.root = root

    def resolve(self, **kwargs):
        return Snapshot()

    def manifest(self):
        return SimpleNamespace(by_name={"classic-server": SimpleNamespace(
            repository="atrinik/classic", checkout_name="classic", branch="main", source="server")})


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.context = Context(self.root)
        self.approval = self.fixture("demo")
        self.service = RuntimeService(self.context, approvals=(self.approval,), enabled=True)

    def fixture(self, name):
        directory = self.root / "workspace" / "topologies" / name
        directory.mkdir(parents=True)
        spec = {"schema_version": 3, "name": name, "profile": "classic",
                "control": {"generation": "test-generation", "socket": "/synthetic/private.sock"},
                "state": "/synthetic/private-state", "state_policy": {}, "runtime": {},
                "providers": {"server": "classic-server"}, "stack": "classic",
                "resolved": {"classic-server": {"head": "b" * 40, "repository": "atrinik/classic",
                    "checkout": "classic", "branch": "main", "source": "server", "dirty": False,
                    "checkout_path": "/synthetic/classic", "path": "/synthetic/classic/server"}},
                "services": {"server": {"command": ["SENTINEL_ENV_SECRET"], "cwd": "/private"}}}
        generation = directory / "generations/test-generation"
        generation.mkdir(parents=True)
        (generation / ".atrinik-workspace-managed.json").write_text(json.dumps(
            {"schema_version": 1, "purpose": "immutable-runtime-generation"}))
        manifest = {"schema_version": 1, "profile": "classic", "generation": "test-generation",
                    "resolved": spec["resolved"], "build": {"metadata_sha256": "c" * 64},
                    "source_trees": {"classic-server": "d" * 40}}
        manifest_bytes = json.dumps(manifest).encode()
        (generation / "manifest.json").write_bytes(manifest_bytes)
        spec["runtime"] = {"schema_version": 1, "generation": "test-generation", "path": str(generation),
                           "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest()}
        payload = json.dumps(spec).encode()
        (directory / "spec.json").write_bytes(payload)
        status = {**spec, "ready": True, "password": "SENTINEL_PASSWORD", "player": "SENTINEL_PLAYER",
                  "save": "SENTINEL_SAVE", "log": "SENTINEL_LOG", "pid": 99001,
                  "services": {"server": {"status": "running", "generation": "test-generation"}}}
        (directory / "status.json").write_text(json.dumps(status))
        (directory / ".atrinik-workspace-managed.json").write_text(json.dumps(
            {"schema_version": 1, "purpose": "topology:" + name}))
        return RuntimeApproval(name, "classic", "test-generation", hashlib.sha256(payload).hexdigest())

    def assert_code(self, code, fn):
        with self.assertRaises(ContractError) as caught:
            fn()
        self.assertEqual(code, caught.exception.code)
        self.assertNotIn("SENTINEL", str(caught.exception))

    def test_disabled_and_exact_allowlist(self):
        self.assert_code("UNAUTHORIZED", lambda: RuntimeService(self.context).status("demo"))
        self.assert_code("UNAUTHORIZED", lambda: self.service.status("other"))
        for name in ("../demo", "/tmp/demo", "demo/status.json", "99001"):
            self.assert_code("UNAUTHORIZED" if name == "99001" else "INVALID_ARGUMENT",
                             lambda: self.service.status(name))

    def test_observe_catalog_is_separate_and_approval_revocation_fails(self):
        from atrinik_workspace.mcp_observe_server import configured_server
        from atrinik_workspace.mcp_server import ContextServer
        path = self.root / "approvals.json"
        path.write_text(json.dumps({"schema_version": 1, "authorization_identity": "synthetic-actor",
                                   "approvals": [vars(self.approval)]}))
        with patch("atrinik_workspace.mcp_observe_server.ContextService", return_value=self.context):
            observed = configured_server(self.root, path)
        self.assertEqual({tool["name"] for tool in observed.catalog()}, {"runtime_list", "runtime_status"})
        routine = ContextServer(self.context)
        self.assertFalse(any(tool["name"].startswith("runtime_") for tool in routine.catalog()))
        result = observed.dispatch("tools/call", {"name": "runtime_status", "arguments": {"topology": "demo"}})
        self.assertEqual(result["structuredContent"]["data"]["runtime_sources"][0]["head"], "b" * 40)
        path.write_text("{}")
        self.assert_code("UNAUTHORIZED", lambda: observed.dispatch("tools/list", {}))

    def test_safe_projection_and_no_mutation(self):
        before = {p: p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        with patch("os.kill", side_effect=AssertionError("signal forbidden")), patch(
                "subprocess.run", side_effect=AssertionError("shell forbidden")):
            result = self.service.status("demo")
        self.assertEqual(result["service_counts"]["running"], 1)
        self.assertEqual(result["freshness"]["ttl_ms"], 0)
        self.assertEqual(result["runtime_sources"][0]["head"], "b" * 40)
        self.assertEqual(result["build_record"]["metadata_sha256"], "c" * 64)
        encoded = json.dumps(result)
        for forbidden in ("SENTINEL", "/synthetic", "99001", "/synthetic/private-state"):
            self.assertNotIn(forbidden, encoded)
        self.assertEqual(before, {p: p.read_bytes() for p in self.root.rglob("*") if p.is_file()})

    def test_stale_spec_generation_and_source(self):
        path = self.root / "workspace/topologies/demo/spec.json"
        path.write_bytes(path.read_bytes() + b" ")
        self.assert_code("STALE_COORDINATE", lambda: self.service.status("demo"))
        with patch.object(Snapshot, "assert_current", side_effect=ContractError("STALE_COORDINATE", "changed")):
            self.assert_code("STALE_COORDINATE", lambda: self.service.status("demo"))

    def test_status_stale_generation(self):
        path = self.root / "workspace/topologies/demo/status.json"
        status = json.loads(path.read_text())
        status["control"]["generation"] = "replaced"
        path.write_text(json.dumps(status))
        self.assert_code("STALE_COORDINATE", lambda: self.service.status("demo"))

    def test_symlink_fifo_and_oversize(self):
        path = self.root / "workspace/topologies/demo/status.json"
        path.unlink()
        path.symlink_to("/etc/passwd")
        self.assert_code("FORBIDDEN", lambda: self.service.status("demo"))
        path.unlink()
        os.mkfifo(path)
        self.assert_code("FORBIDDEN", lambda: self.service.status("demo"))
        path.unlink()
        path.write_bytes(b"x" * (MAX_BYTES + 1))
        self.assert_code("LIMIT_EXCEEDED", lambda: self.service.status("demo"))

    def test_cancellation_timeout_and_midread(self):
        self.assert_code("CANCELLED", lambda: self.service.status("demo", cancelled=lambda: True))
        self.assert_code("TIMEOUT", lambda: self.service.status("demo", timeout_ms=0))
        with patch("atrinik_workspace.mcp_runtime.time.monotonic", side_effect=[0, 6]):
            self.assert_code("TIMEOUT", lambda: self.service.status("demo"))
        calls = 0
        def cancel():
            nonlocal calls
            calls += 1
            return calls > 12
        self.assert_code("CANCELLED", lambda: self.service.status("demo", cancelled=cancel))

    def test_malformed_values_are_sanitized(self):
        path = self.root / "workspace/topologies/demo/status.json"
        for payload in (b'{"private":"SENTINEL_PASSWORD",', b'{"x":1,"x":2}', b'[]'):
            path.write_bytes(payload)
            self.assert_code("INCOMPLETE", lambda: self.service.status("demo"))

    def test_77_records_pagination_malformed_and_cursor_drift(self):
        approvals = [self.approval] + [self.fixture(f"demo-{i:03}") for i in range(76)]
        service = RuntimeService(self.context, approvals=approvals, enabled=True)
        (self.root / "workspace/topologies/demo-000/status.json").write_text("{malformed")
        first = service.list(page_size=25)
        records = list(first["items"])
        cursor = first["next_cursor"]
        while cursor:
            page = service.list(page_size=25, cursor=cursor)
            records.extend(page["items"])
            cursor = page["next_cursor"]
        self.assertEqual(len(records), 77)
        self.assertEqual(len({record["topology"] for record in records}), 77)
        self.assertEqual(sum("error" in record for record in records), 1)
        path = self.root / "workspace/topologies/demo/status.json"
        status = json.loads(path.read_text())
        status["ready"] = False
        path.write_text(json.dumps(status))
        self.assert_code("STALE_CURSOR", lambda: service.list(page_size=25, cursor=first["next_cursor"]))

    def test_duplicate_approvals_rejected(self):
        self.assert_code("INVALID_ARGUMENT", lambda: RuntimeService(
            self.context, approvals=(self.approval, self.approval)))

    def test_runtime_filters_are_bounded_and_cursor_bound(self):
        approvals = [self.approval, self.fixture("other")]
        service = RuntimeService(self.context, approvals=approvals, enabled=True)
        result = service.list(name_prefix="demo", profile="classic", service_state="running")
        self.assertEqual([item["topology"] for item in result["items"]], ["demo"])
        self.assertEqual(service.list(state_identity="0" * 64)["items"], [])
        result = service.list(page_size=1)
        self.assert_code("STALE_CURSOR", lambda: service.list(page_size=1,
            cursor=result["next_cursor"], service_state="running"))
        with patch.object(self.context, "resolve", wraps=self.context.resolve) as resolve:
            service.list()
        self.assertEqual(resolve.call_count, 1)

    def test_service_exit_and_clean_shutdown_are_associated(self):
        path = self.root / "workspace/topologies/demo/status.json"
        status = json.loads(path.read_text())
        status["services"]["server"].update(status="exited", exit_code=137)
        status["shutdown"] = {"clean": False, "control_requested": False, "private": "SENTINEL"}
        path.write_text(json.dumps(status))
        result = self.service.status("demo")
        self.assertEqual(result["services"]["server"], {"status": "exited", "exit_code": 137})
        self.assertEqual(result["shutdown"], {"clean": False, "control_requested": False})
        self.assertNotIn("SENTINEL", json.dumps(result))

    def test_list_rechecks_source_and_ownership_at_handoff(self):
        import atrinik_workspace.mcp_runtime as runtime
        original = self.service.status
        def revoke_source(*args, **kwargs):
            result = original(*args, **kwargs)
            Snapshot.assert_current = lambda _: (_ for _ in ()).throw(
                ContractError("STALE_COORDINATE", "source changed"))
            return result
        with patch.object(Snapshot, "assert_current", Snapshot.assert_current):
            with patch.object(self.service, "status", side_effect=revoke_source):
                self.assert_code("STALE_COORDINATE", lambda: self.service.list())
        def revoke_marker(*args, **kwargs):
            result = original(*args, **kwargs)
            path = self.root / "workspace/topologies/demo/.atrinik-workspace-managed.json"
            path.write_text("{}")
            return result
        with patch.object(self.service, "status", side_effect=revoke_marker):
            self.assert_code("STALE_COORDINATE", lambda: self.service.list())

    def test_replaced_publication_during_read(self):
        import atrinik_workspace.mcp_runtime as runtime
        original = runtime._read
        calls = 0
        def read(path, check):
            nonlocal calls
            result = original(path, check)
            calls += 1
            if calls == 3:
                path.write_bytes(result + b" ")
            return result
        with patch.object(runtime, "_read", side_effect=read):
            self.assert_code("STALE_COORDINATE", lambda: self.service.status("demo"))


if __name__ == "__main__":
    unittest.main()
