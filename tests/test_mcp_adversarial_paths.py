# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Adversarial coverage for bounded MCP validation and startup paths."""
from __future__ import annotations

import io
import json
import os
from collections import OrderedDict
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from atrinik_workspace.mcp_contract import ContractError, Coordinate
import atrinik_workspace.mcp_context as context
import atrinik_workspace.mcp_observe_server as observe
import atrinik_workspace.mcp_runtime as runtime
import atrinik_workspace.mcp_search as search
import atrinik_workspace.mcp_server as server


class AdversarialMcpPaths(unittest.TestCase):
    def assert_code(self, code, operation):
        with self.assertRaises(ContractError) as caught:
            operation()
        self.assertEqual(caught.exception.code, code)

    def search_snapshot(self, root):
        metadata = {"source": ".", "component": "component", "checkout": "repo",
                    "profile": "default", "stack": "default", "roles": [],
                    "generation": None, "owner": "atrinik", "license": "MIT", "build": None}
        return SimpleNamespace(root=Path(root), metadata=metadata, identity={"fixture": True},
            coordinate=Coordinate("atrinik/repo", "main", "a" * 40, "primary", None),
            assert_current=lambda: None)

    def test_context_rejects_deadlines_paths_text_and_credentials(self):
        for timeout in (0, 5001):
            self.assert_code("TIMEOUT", lambda timeout=timeout: context.request_scope(timeout_ms=timeout).__enter__())
        with patch.object(context.sys, "platform", "win32"):
            self.assert_code("UNSUPPORTED_OPERATION", lambda: context.directory(Path("/tmp")).__enter__())
        for path in (Path("relative"), Path("/tmp/../tmp")):
            self.assert_code("FORBIDDEN", lambda path=path: context.directory(path).__enter__())
        self.assert_code("NOT_FOUND", lambda: context.directory(Path("/definitely-missing-atrinik-fixture")).__enter__())
        self.assert_code("INCOMPLETE", lambda: context._text(b"\xff"))
        self.assert_code("INCOMPLETE", lambda: context._text(b"line\x00value"))
        self.assert_code("INVALID_ARGUMENT", lambda: context._name("../invalid"))
        for value in (".env.local", "identity.pem", "nested/passwords.json"):
            self.assert_code("FORBIDDEN", lambda value=value: context._source_selector(value))

    def test_context_git_failures_are_bounded_and_sanitized(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch.object(context.subprocess, "Popen", side_effect=OSError("secret executable")):
                self.assert_code("OFFLINE", lambda: context._git(root, "rev-parse", "HEAD"))
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            self.assertEqual(context._git(root, "config", "--get", "missing", _missing_ok=True), b"")
            self.assert_code("NOT_FOUND", lambda: context._git(root, "config", "--get", "missing"))
            with patch.object(context, "_git", return_value=b"filter.fixture.clean\x00"):
                self.assert_code("FORBIDDEN", lambda: context.check_git_read_policy(root))

    def test_dirty_content_handles_deletes_exclusions_and_caps(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch.object(context, "_git", return_value=b"stage\x00"), patch.object(
                    context, "_safe_read", return_value=b"changed") as read:
                result = json.loads(context._dirty_content(root, b" D source.txt\x00"))
                self.assertEqual(len(result), 1)
                read.assert_not_called()
                result = json.loads(context._dirty_content(root, b" M .env.local\x00"))
                self.assertEqual(result[0][1], "excluded")
                context._dirty_content(root, b"R  renamed.txt\x00old.txt\x00")
                read.assert_called_once_with(root, "renamed.txt")
            self.assert_code("INCOMPLETE", lambda: context._dirty_content(root, b"bad\x00"))
            records = b"".join(f" M file-{index}\x00".encode() for index in range(101))
            with patch.object(context, "_git", return_value=b""), patch.object(context, "_safe_read", return_value=b""):
                self.assert_code("LIMIT_EXCEEDED", lambda: context._dirty_content(root, records))
            with patch.object(context, "_git", return_value=b""), patch.object(
                    context, "_safe_read", return_value=b"x" * (context.MAX_BYTES + 1)):
                self.assert_code("LIMIT_EXCEEDED", lambda: context._dirty_content(root, b" M huge.txt\x00"))

    def test_clean_snapshot_read_detects_worktree_bytes_diverging_from_commit(self):
        service = SimpleNamespace(resolve=lambda **_selection: SimpleNamespace(identity={"stable": True}))
        snapshot = context.Snapshot(Path("/tmp"), Coordinate("atrinik/repo", "main", "a" * 40, "primary", None),
                                    {"stable": True}, {}, service, {})
        with patch.object(context, "_git", side_effect=(b"H source.txt\x00", b"committed\n")), patch.object(
                context, "_safe_read", return_value=b"different\n"):
            self.assert_code("STALE_COORDINATE", lambda: snapshot.read("source.txt"))

    def test_runtime_approval_coordinates_decode_and_read_contracts(self):
        expected = SimpleNamespace(repository="atrinik/repo", checkout_name="repo", branch="main", source=".")
        manifest = SimpleNamespace(by_name={"component": expected})
        good = {"component": {"repository": "atrinik/repo", "checkout": "repo", "branch": "main",
                "source": ".", "checkout_path": "/private/repo", "head": "a" * 40, "dirty": False}}
        projected = runtime._coordinates(good, manifest)
        self.assertNotIn("checkout_path", projected[0])
        for value, code in (({}, "INCOMPLETE"), ({"unknown": good["component"]}, "FORBIDDEN"),
                            ({"component": {**good["component"], "head": "bad"}}, "INCOMPLETE"),
                            ({"component": {**good["component"], "dirty": 1}}, "INCOMPLETE"),
                            ({"component": {**good["component"], "source": "../secret"}}, "FORBIDDEN")):
            self.assert_code(code, lambda value=value: runtime._coordinates(value, manifest))
        for approval in (("bad/name", "p", "g", "0" * 64), ("n", "p", "g", "bad")):
            self.assert_code("INVALID_ARGUMENT", lambda approval=approval: runtime.RuntimeApproval(*approval))
        self.assert_code("INCOMPLETE", lambda: runtime._decode(b'{"x": NaN}'))
        nested = b'{"x":' + b"[" * 33 + b"0" + b"]" * 33 + b"}"
        self.assert_code("LIMIT_EXCEEDED", lambda: runtime._decode(nested))
        self.assert_code("FORBIDDEN", lambda: runtime._read(Path("relative"), lambda: None))
        with patch.object(runtime.os, "O_NOFOLLOW", 0):
            self.assert_code("UNSUPPORTED_OPERATION", lambda: runtime._read(Path("/tmp/x"), lambda: None))

    def test_search_metadata_cancellation_and_safe_path_fail_closed(self):
        snapshot = SimpleNamespace(root=Path("relative"), metadata={}, identity={},
            coordinate=Coordinate("atrinik/repo", "main", "a" * 40, "primary", None))
        self.assert_code("INTERNAL", lambda: search._metadata(snapshot, "source"))
        for source in (1, "/absolute", "../escape", "build/output"):
            snapshot.metadata = {"source": source}
            self.assert_code("INTERNAL", lambda: search._source_root(snapshot))
        for source in (".env.local", "credentials.json", "nested/secrets.toml", "private.key"):
            snapshot.metadata = {"source": source}
            self.assert_code("FORBIDDEN", lambda: search._source_root(snapshot))
        for cancellation in (object(),):
            self.assert_code("INVALID_ARGUMENT", lambda: search._cancelled(cancellation))
        event = threading.Event()
        event.set()
        self.assert_code("CANCELLED", lambda: search._check_deadline(time.monotonic() + 10, event))
        self.assert_code("TIMEOUT", lambda: search._check_deadline(time.monotonic() - 1, None))
        snapshot.metadata = {"source": "."}
        self.assert_code("INTERNAL", lambda: search._safe_root(snapshot))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "regular.txt").write_text("source")
            (root / "archive.zip").write_text("archive")
            (root / "directory").mkdir()
            (root / "link").symlink_to("regular.txt")
            for raw in ("", "../regular.txt", "regular\\.txt", ".env", "archive.zip", "directory", "link", "missing"):
                self.assertIsNone(search._safe_relative(root, raw))
            self.assertEqual(search._safe_relative(root, "regular.txt"), "regular.txt")
        with patch.object(search.subprocess, "Popen", side_effect=OSError("secret")):
            self.assert_code("UNSUPPORTED_OPERATION", lambda: search._capture(
                ["missing"], root=Path("/tmp"), deadline=time.monotonic() + 1, cancellation=None))

    def test_search_capture_bounds_output_and_fallback_git_filter_policy(self):
        deadline = time.monotonic() + 10
        output, errors, code, limited = search._capture(
            [sys.executable, "-c", "import sys; sys.stdout.buffer.write(b'x' * 4300000)"],
            root=Path("/tmp"), deadline=deadline, cancellation=None)
        self.assertTrue(limited)
        self.assertEqual(len(output), search._RG_OUTPUT_BYTES)
        self.assertEqual(errors, b"")
        self.assertNotEqual(code, 0)

        real_import = __import__
        def isolated_import(name, *args, **kwargs):
            if name == "atrinik_workspace.mcp_context":
                raise ImportError("isolated fixture")
            return real_import(name, *args, **kwargs)
        snapshot = SimpleNamespace(root=Path("/tmp"))
        with patch("builtins.__import__", side_effect=isolated_import), patch.object(
                search, "_safe_open_flags", return_value=(os.O_RDONLY, os.O_RDONLY)), patch.object(
                search.os, "open", return_value=91), patch.object(search.os, "close") as close, patch.object(
                search, "_capture", return_value=(b"", b"", 1, False)):
            search._check_context_git_policy(snapshot, deadline, None)
            close.assert_called_once_with(91)
        for captured, code, limited, expected in ((b"filter.fixture.clean\x00", 0, False, "FORBIDDEN"),
                                                   (b"", 2, False, "INCOMPLETE"),
                                                   (b"", 1, True, "INCOMPLETE")):
            with patch("builtins.__import__", side_effect=isolated_import), patch.object(
                    search, "_safe_open_flags", return_value=(os.O_RDONLY, os.O_RDONLY)), patch.object(
                    search.os, "open", return_value=91), patch.object(search.os, "close"), patch.object(
                    search, "_capture", return_value=(captured, b"", code, limited)):
                self.assert_code(expected, lambda: search._check_context_git_policy(snapshot, deadline, None))

    def test_descriptor_batch_rejects_malformed_index_and_tree_identity(self):
        snapshot = self.search_snapshot("/tmp")
        deadline = time.monotonic() + 10
        search._validate_descriptor_batch(snapshot, {}, deadline, None)
        descriptor = {91: "source.py"}
        cases = (
            (((b"", 2, False),), "STALE_COORDINATE"),
            (((b"\xff\x00", 0, False),), "STALE_COORDINATE"),
            (((b"broken\x00", 0, False),), "STALE_COORDINATE"),
            (((b"H source.py\x00", 0, False), (b"", 2, False)), "STALE_COORDINATE"),
            (((b"H source.py\x00", 0, False), (b"broken\x00", 0, False)), "STALE_COORDINATE"),
            (((b"H source.py\x00", 0, False), (b"120000 blob " + b"a" * 40 + b"\tsource.py\x00", 0, False)), "FORBIDDEN"),
            (((b"H source.py\x00", 0, False), (b"100644 blob " + b"a" * 40 + b"\tother.py\x00", 0, False)), "STALE_COORDINATE"),
            (((b"H source.py\x00", 0, False), (b"100644 blob short\tsource.py\x00", 0, False)), "STALE_COORDINATE"),
        )
        for captures, expected in cases:
            with patch.object(search, "_git_capture", side_effect=captures):
                self.assert_code(expected, lambda: search._validate_descriptor_batch(
                    snapshot, descriptor, deadline, None))
        tree = b"100644 blob " + b"a" * 64 + b"\tsource.py\x00"
        with patch.object(search, "_git_capture", side_effect=((b"H source.py\x00", 0, False), (tree, 0, False))), patch.object(
                search, "_descriptor_git_oid", return_value="a" * 64) as digest:
            search._validate_descriptor_batch(snapshot, descriptor, deadline, None)
        digest.assert_called_once_with(91, "sha256", deadline, None)

    def test_path_search_skips_unsafe_and_oversized_inventory_members(self):
        snapshot = self.search_snapshot("/tmp")
        deadline = time.monotonic() + 10
        with patch.object(search, "_inventory", return_value=(["unsafe"], False)), patch.object(
                search, "_open_source_root", return_value=90), patch.object(
                search, "_open_regular", side_effect=ContractError("LIMIT_EXCEEDED", "large")), patch.object(
                search, "_validate_descriptor_batch") as validate_batch, patch.object(search.os, "close"):
            records, limited = search._path_records(snapshot, "path", "unsafe", True, deadline, None)
        self.assertEqual(records, [])
        self.assertTrue(limited)
        validate_batch.assert_called_once_with(snapshot, {}, deadline, None)

    def test_content_search_isolates_unsafe_process_events(self):
        snapshot = self.search_snapshot("/tmp")
        deadline = time.monotonic() + 10
        with patch.object(search, "_inventory", return_value=([], False)), patch.object(
                search.Path, "is_dir", return_value=False):
            self.assert_code("UNSUPPORTED_OPERATION", lambda: search._content_records(
                snapshot, "exact", "x", True, deadline, None))
        with patch.object(search, "_inventory", return_value=(["source.py"], False)), patch.object(
                search, "_open_source_root", return_value=90), patch.object(
                search, "_open_regular", side_effect=ContractError("INTERNAL", "failure")), patch.object(search.os, "close"):
            self.assert_code("INTERNAL", lambda: search._content_records(
                snapshot, "exact", "x", True, deadline, None))
        with patch.object(search, "_inventory", return_value=(["source.py"], False)), patch.object(
                search, "_open_source_root", return_value=90), patch.object(
                search, "_open_regular", side_effect=ContractError("FORBIDDEN", "unsafe")), patch.object(
                search, "_validate_descriptor_batch") as validate_batch, patch.object(search.os, "close"):
            records, limited = search._content_records(snapshot, "exact", "x", True, deadline, None)
        self.assertEqual(records, [])
        self.assertTrue(limited)
        validate_batch.assert_not_called()
        events = b"\n".join((
            json.dumps({"type": "match", "data": {"path": {"text": "unknown"}, "line_number": 1,
                "lines": {"text": "x"}, "submatches": [{"start": 0, "end": 1}]}}).encode(),
            json.dumps({"type": "match", "data": {"path": {"text": "/proc/self/fd/91"}, "line_number": 1,
                "lines": {"text": "x"}, "submatches": [{"start": "bad", "end": 1}]}}).encode(),
            b"{malformed",
        ))
        identity = SimpleNamespace(st_dev=1, st_ino=2, st_size=3, st_mtime_ns=4)
        with patch.object(search, "_inventory", return_value=(["source.py"], False)), patch.object(
                search, "_open_source_root", return_value=90), patch.object(search, "_open_regular", return_value=91), patch.object(
                search, "_validate_descriptor_batch"), patch.object(search, "_capture", return_value=(events, b"", 0, False)), patch.object(
                search.os, "fstat", return_value=identity), patch.object(search.os, "close"):
            records, limited = search._content_records(snapshot, "exact", "x", False, deadline, None)
        self.assertEqual(records, [])
        self.assertTrue(limited)

    def test_stdio_writer_uses_bounded_descriptor_frames(self):
        read_fd, write_fd = os.pipe()
        try:
            with os.fdopen(write_fd, "wb", buffering=0, closefd=True) as output:
                server.serve(SimpleNamespace(handle=lambda request, _event=None: {"jsonrpc": "2.0", "id": request["id"]}),
                             io.BytesIO(b"not-json\n"), output)
            payload = os.read(read_fd, 4096)
        finally:
            os.close(read_fd)
        self.assertEqual(json.loads(payload)["error"]["code"], -32700)

    def test_context_manifest_and_registry_corruption_are_isolated(self):
        service = object.__new__(context.ContextService)
        service.root = Path("/tmp")
        service.authorization_identity = "actor"
        with patch.object(context, "_safe_read", return_value=b"not json"):
            self.assert_code("INCOMPLETE", service._model)
        with patch.object(context, "_git", return_value=(
                b"worktree /tmp/one\x00HEAD " + b"a" * 40 + b"\x00\x00"
                b"worktree /tmp/one\x00HEAD " + b"b" * 40 + b"\x00\x00")):
            records, _identity = service._registry(Path("/tmp"))
        self.assertEqual(len(records), 2)
        self.assertTrue(all(record.get("incomplete") == "true" for record in records))
        oversized = b"\x00\x00".join(
            b"worktree /tmp/w" + str(index).encode() + b"\x00HEAD " + b"a" * 40
            for index in range(1001))
        with patch.object(context, "_git", return_value=oversized):
            self.assert_code("LIMIT_EXCEEDED", lambda: service._registry(Path("/tmp")))
        for identity in ("", "x" * 1025):
            with patch.object(context, "directory"):
                self.assert_code("UNAUTHORIZED", lambda identity=identity: context.ContextService(Path("/tmp"), identity))

    def test_context_registration_names_isolate_malformed_mutable_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace = root / "workspace"
            workspace.mkdir()
            topologies = workspace / "topologies"
            topologies.mkdir()
            (topologies / "healthy").mkdir()
            (topologies / "not-a-directory").write_text("private")
            (topologies / "Bad").mkdir()
            paths = SimpleNamespace(workspace=workspace, state=workspace / "states", topologies=topologies,
                                    scenarios=workspace / "missing-scenarios")
            snapshot = SimpleNamespace(identity={"snapshot": 1}, assert_current=lambda: None)
            service = object.__new__(context.ContextService)
            service.authorization_identity = "actor"
            service.resolve = lambda: snapshot
            service._model = lambda: (None, paths, "manifest")
            service._page = lambda rows, _identity, _page_size, _cursor: {"items": rows}
            self.assert_code("INVALID_ARGUMENT", lambda: service.registrations("unknown"))
            self.assert_code("INVALID_ARGUMENT", lambda: service.list_profiles(kind="topologies", name_prefix="healthy"))

            (workspace / "states.json").write_text(json.dumps({"schema_version": 1, "states": {"../bad": "relative"}}))
            result = service.registrations("states")
            self.assertTrue(result["items"][0]["incomplete"])
            (workspace / "states.json").write_text(json.dumps({"schema_version": 2, "states": {}}))
            self.assertEqual(service.registrations("states")["items"][0]["error"], "INCOMPLETE")
            (workspace / "states.json").write_text(json.dumps(
                {"schema_version": 1, "states": {f"s{index}": f"/state/{index}" for index in range(1001)}}))
            self.assert_code("LIMIT_EXCEEDED", lambda: service.registrations("states"))

            rows = service.registrations("topologies")["items"]
            self.assertEqual({row.get("name") for row in rows if "name" in row}, {"healthy", "not-a-directory"})
            self.assertTrue(any(row.get("incomplete") for row in rows))
            self.assertEqual(service.registrations("scenarios")["items"], [])

    def test_search_request_validation_rejects_ambiguous_inputs_before_io(self):
        valid = {"mode": "exact", "query": "needle", "case_sensitive": True,
                 "page_size": 10, "timeout_ms": 1000}
        cases = [
            ([], "INVALID_ARGUMENT"),
            ({**valid, "unknown": True}, "INVALID_ARGUMENT"),
            ({**valid, "query": 1}, "INVALID_ARGUMENT"),
            ({**valid, "cursor": 1}, "INVALID_ARGUMENT"),
            ({**valid, "mode": "unknown"}, "INVALID_ARGUMENT"),
            ({**valid, "provenance": "yes"}, "INVALID_ARGUMENT"),
            ({**valid, "provenance": True}, "INVALID_ARGUMENT"),
            ({**valid, "mode": "history", "provenance": False, "path": "x"}, "UNSUPPORTED_OPERATION"),
            ({**valid, "mode": "history", "provenance": True}, "INVALID_ARGUMENT"),
        ]
        snapshot = SimpleNamespace(coordinate=Coordinate("atrinik/repo", "main", "a" * 40, "primary", None),
                                   identity={}, metadata={"source": "."}, root=Path("/tmp"))
        for request, code in cases:
            self.assert_code(code, lambda request=request: search.search(
                request, [snapshot], authorization_identity="actor"))
        self.assert_code("UNAUTHORIZED", lambda: search.search(valid, [snapshot], authorization_identity=""))
        self.assert_code("NOT_FOUND", lambda: search.search(valid, [], authorization_identity="actor"))
        self.assert_code("INVALID_ARGUMENT", lambda: search.search(valid, [snapshot, snapshot], authorization_identity="actor"))
        self.assert_code("LIMIT_EXCEEDED", lambda: search.search(
            {**valid, "page_size": 51}, [snapshot], authorization_identity="actor"))

    def test_language_adapter_records_validate_every_untrusted_field(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "source.py").write_text("symbol = 1\n")
            snapshot = self.search_snapshot(root)
            class Adapter:
                identity = "fixture-index-v1"
                def __init__(self, records=None, failure=None):
                    self.records, self.failure = records, failure
                def search(self, **_kwargs):
                    if self.failure:
                        raise self.failure
                    return self.records
            deadline = time.monotonic() + 10
            records, limited = search._adapter_records(snapshot, Adapter([{
                "path": "source.py", "line": 1, "column": 2, "symbol": "token=SYNTHETIC_SECRET",
                "language": "python"}]), "symbols", "symbol", deadline, None)
            self.assertFalse(limited)
            self.assertEqual(records[0]["line"], 1)
            self.assertNotIn("SYNTHETIC_SECRET", json.dumps(records))
            self.assert_code("UNSUPPORTED_OPERATION", lambda: search._adapter_records(
                snapshot, None, "symbols", "x", deadline, None))
            for raw in (1, {"path": "missing.py"}, {"path": "source.py", "line": True},
                        {"path": "source.py", "column": 0}, {"path": "source.py", "symbol": 1}):
                self.assert_code("INCOMPLETE", lambda raw=raw: search._adapter_records(
                    snapshot, Adapter([raw]), "symbols", "x", deadline, None))
            self.assert_code("INCOMPLETE", lambda: search._adapter_records(
                snapshot, Adapter(failure=RuntimeError("secret")), "symbols", "x", deadline, None))

    def test_history_parser_bounds_malformed_and_renamed_records(self):
        snapshot = self.search_snapshot("/tmp")
        deadline = time.monotonic() + 10
        valid = "\0".join(("b" * 40, "c" * 40, "Synthetic Author", "2026-01-01T00:00:00Z",
                            "Fix token=SYNTHETIC_SECRET", "\nsource.py")) + "\0"
        malformed_path = "\0".join(("d" * 40, "", "Author", "date", "subject", "../secret")) + "\0"
        malformed_commit = "\0".join(("invalid", "", "Author", "date", "Fix malformed", "source.py")) + "\0"
        with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")), patch.object(
                search, "_git_capture", return_value=((valid + malformed_path + malformed_commit + "tail").encode(), 0, False)):
            records, limited = search._history_records(snapshot, "fix", False, "source.py", deadline, None)
        self.assertTrue(limited)
        self.assertEqual(len(records), 1)
        self.assertNotIn("SYNTHETIC_SECRET", json.dumps(records))
        self.assertEqual(records[0]["historical_path"], "source.py")
        with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")), patch.object(
                search, "_git_capture", return_value=(b"", 2, False)):
            self.assert_code("INCOMPLETE", lambda: search._history_records(
                snapshot, "", True, "source.py", deadline, None))
        with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")), patch.object(
                search, "_git_capture", return_value=(b"\xff", 0, False)):
            self.assert_code("INCOMPLETE", lambda: search._history_records(
                snapshot, "", True, "source.py", deadline, None))

    def test_blame_parser_requires_safe_complete_porcelain_records(self):
        snapshot = self.search_snapshot("/tmp")
        deadline = time.monotonic() + 10
        commit = "b" * 40
        output = (f"{commit} 1 7 1\nauthor Synthetic token=SYNTHETIC_SECRET\n"
                  "author-time 123\nfilename source.py\n\tline token=SYNTHETIC_SECRET\n"
                  f"{commit} 2 8 1\nfilename ../secret\n\tignored\n"
                  f"{commit} 3 9 1\nauthor-time invalid\n\torphan\n"
                  f"{commit} 4 10 1\nauthor Pending")
        with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")), patch.object(
                search, "_git_capture", return_value=(output.encode(), 0, False)):
            records, limited = search._blame_records(snapshot, "source.py", 7, deadline, None)
        self.assertTrue(limited)
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]["line"], 7)
        self.assertNotIn("SYNTHETIC_SECRET", json.dumps(records))
        for line in (0, True, "1"):
            with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")):
                self.assert_code("INVALID_ARGUMENT", lambda line=line: search._blame_records(
                    snapshot, "source.py", line, deadline, None))
        with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")), patch.object(
                search, "_git_capture", return_value=(b"", 2, False)):
            self.assert_code("INCOMPLETE", lambda: search._blame_records(
                snapshot, "source.py", None, deadline, None))
        with patch.object(search, "_provenance_path", return_value=("source.py", "source.py")), patch.object(
                search, "_git_capture", return_value=(b"\xff", 0, False)):
            self.assert_code("INCOMPLETE", lambda: search._blame_records(
                snapshot, "source.py", None, deadline, None))

    def test_historical_path_and_snippet_projection_are_bounded(self):
        snapshot = self.search_snapshot("/tmp")
        for value in ("", "bad\\path", "../secret", "/absolute", "archive.zip", ".env"):
            self.assertIsNone(search._historical_component_path(snapshot, value))
        snapshot.metadata["source"] = "src"
        self.assertIsNone(search._historical_component_path(snapshot, "other/file.py"))
        self.assertEqual(search._historical_component_path(snapshot, "src/file.py"), ("file.py", "src/file.py"))
        snippet, clipped = search._clip_snippet("é" * 3000)
        self.assertTrue(clipped)
        self.assertLessEqual(len(snippet.encode()), search._SNIPPET_BYTES)

    def test_server_schema_shape_handle_and_main_failure_paths(self):
        def rejected(value, schema, code="INVALID_ARGUMENT"):
            self.assert_code(code, lambda: server.validate(value, schema))
        rejected({}, server.object_schema({"required": server.STRING}, ("required",)))
        rejected({"other": "x"}, server.object_schema({}))
        rejected("", server.STRING, "LIMIT_EXCEEDED")
        rejected("bad", {"type": "string", "pattern": "^good$"})
        rejected("line\x00", server.STRING)
        rejected(True, {"type": "integer"})
        rejected(4, {"type": "integer", "maximum": 3}, "LIMIT_EXCEEDED")
        rejected([1, 2], {"type": "array", "maxItems": 1, "items": {"type": "integer"}}, "LIMIT_EXCEEDED")
        self.assertIsNone(server.validate(None, {"type": ["null", "string"]}))
        deep = {}
        cursor = deep
        for _ in range(34):
            cursor["x"] = {}
            cursor = cursor["x"]
        self.assert_code("LIMIT_EXCEEDED", lambda: server._shape(deep))
        self.assert_code("INVALID_ARGUMENT", lambda: server._shape({1: "value"}))
        with patch.object(server, "ContextService", side_effect=ContractError("NOT_FOUND", "missing")):
            self.assertEqual(server.main(["--root", "/missing"]), 1)
        with patch.object(observe, "configured_server", side_effect=ContractError("INVALID_ARGUMENT", "bad")):
            self.assertEqual(observe.main(["--root", "/tmp", "--approvals", "/tmp/missing"]), 1)

    def test_context_cli_routes_each_operation_and_serializes_snapshots(self):
        class FakeSnapshot:
            def json(self):
                return {"snapshot": "0" * 64}
        snapshot = FakeSnapshot()
        fake = SimpleNamespace(
            list_profiles=lambda: {"operation": "profiles"},
            describe=lambda profile: {"operation": "describe", "profile": profile},
            resolve=lambda **_kwargs: snapshot,
            list_worktrees=lambda **_kwargs: {"operation": "worktrees"},
            guidance=lambda **_kwargs: {"operation": "guidance"},
            changes=lambda **_kwargs: {"operation": "changes"},
        )
        with patch.object(context, "ContextService", return_value=fake), patch.object(
                context, "Snapshot", FakeSnapshot), patch("builtins.print") as output:
            for operation in ("profiles", "describe", "resolve", "worktrees", "guidance", "changes"):
                self.assertEqual(context.main(["--root", "/tmp", operation]), 0)
            self.assertEqual(output.call_count, 6)
        with patch.object(context, "ContextService", side_effect=ContractError("NOT_FOUND", "missing")), patch(
                "builtins.print") as output:
            self.assertEqual(context.main(["--root", "/tmp", "resolve"]), 1)
            self.assertIn("NOT_FOUND", output.call_args.args[0])

    def test_server_dispatch_rejects_invalid_resources_and_search_results(self):
        rpc = object.__new__(server.ContextServer)
        rpc.resources = OrderedDict()
        rpc.resource_lock = threading.Lock()
        rpc.tools = {}
        rpc.service = SimpleNamespace(authorization_identity="actor")
        for method, params, code in (
            ("unknown", {}, "UNSUPPORTED_OPERATION"),
            ("tools/list", {"extra": True}, "INVALID_ARGUMENT"),
            ("resources/read", {"uri": 1}, "INVALID_ARGUMENT"),
            ("resources/read", {"uri": "atrinik://missing"}, "NOT_FOUND"),
            ("tools/call", {"name": "missing"}, "UNSUPPORTED_OPERATION"),
        ):
            self.assert_code(code, lambda method=method, params=params: rpc.dispatch(method, params))
        for method, expected in (("server/discover", "supportedVersions"), ("resources/list", "resources"),
                                 ("resources/templates/list", "resourceTemplates")):
            self.assertIn(expected, rpc.dispatch(method, {}))
        snapshot = SimpleNamespace(identity={"snapshot": 1}, root=Path("/tmp"),
            coordinate=Coordinate("atrinik/repo", "main", "a" * 40, "primary", None),
            metadata={"component": "component"}, assert_current=lambda: None,
            json=lambda: {"private": "token=SYNTHETIC_SECRET"},
            read=lambda _path: b"token=SYNTHETIC_SECRET\n")
        rpc.resources = OrderedDict()
        root_uri = rpc.resource(snapshot)["uri"]
        source_uri = rpc.resource(snapshot, "source.txt")["uri"]
        root_result = rpc.dispatch("resources/read", {"uri": root_uri})
        source_result = rpc.dispatch("resources/read", {"uri": source_uri})
        self.assertNotIn("SYNTHETIC_SECRET", json.dumps((root_result, source_result)))
        binary_uri = rpc.resource(SimpleNamespace(**{**vars(snapshot), "read": lambda _path: b"bad\x00value"}), "source.txt")["uri"]
        self.assert_code("FORBIDDEN", lambda: rpc.dispatch("resources/read", {"uri": binary_uri}))
        historical_uri = rpc.resource(snapshot, "source.txt", "b" * 40)["uri"]
        with patch.object(server, "_git", return_value=b"120000 blob " + b"a" * 40 + b"\x00"):
            self.assert_code("FORBIDDEN", lambda: rpc.dispatch("resources/read", {"uri": historical_uri}))
        with patch.object(server, "_git", side_effect=(b"100644 blob " + b"a" * 40 + b"\x00", b"\xff")):
            self.assert_code("FORBIDDEN", lambda: rpc.dispatch("resources/read", {"uri": historical_uri}))

    def test_server_handle_rejects_bad_ids_metadata_and_oversize_results(self):
        rpc = object.__new__(server.ContextServer)
        rpc.server_name = "fixture"
        rpc.dispatch = lambda _method, _params: {"value": "ok"}
        meta = {server.VERSION_KEY: server.PROTOCOL_VERSION, server.CAPABILITIES_KEY: {}}
        base = {"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {"_meta": meta}}
        for request in (None, {**base, "id": True}, {**base, "id": 2**54}, {**base, "id": "x" * 129},
                        {**base, "extra": True}, {**base, "params": {}},
                        {**base, "params": {"_meta": {server.VERSION_KEY: "invalid", server.CAPABILITIES_KEY: {}}}},
                        {**base, "params": {"_meta": {server.VERSION_KEY: server.PROTOCOL_VERSION}}}):
            self.assertIn("error", rpc.handle(request))
        mismatch = {**base, "params": {"_meta": {server.VERSION_KEY: "2025-01-01", server.CAPABILITIES_KEY: {}}}}
        self.assertEqual(rpc.handle(mismatch)["error"]["code"], -32022)
        with patch.object(rpc, "dispatch", return_value={"value": "x" * 40000}):
            self.assertEqual(rpc.handle(base)["error"]["data"]["code"], "LIMIT_EXCEEDED")

    def test_observe_configuration_rejects_malformed_and_replaced_approval(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            approvals = root / "approvals.json"
            for payload in ({}, {"schema_version": 1, "authorization_identity": "actor", "approvals": [1]},
                            {"schema_version": 1, "authorization_identity": "actor", "approvals": [
                                {"name": "n", "profile": "p", "generation": "g"}]}):
                approvals.write_text(json.dumps(payload))
                self.assert_code("INVALID_ARGUMENT", lambda: observe.configured_server(root, approvals))


if __name__ == "__main__":
    unittest.main()
