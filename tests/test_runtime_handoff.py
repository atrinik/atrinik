from __future__ import annotations

import copy
import unittest

from atrinik_workspace import runtime_handoff as handoff
from atrinik_workspace.model import WorkspaceError


def envelope():
    value = {
        "schema_version": 1,
        "issue": {"repository": "atrinik/atrinik", "number": 604, "node_id": "I_owned"},
        "attempt_sha256": "a" * 64, "actor_node_id": "U_owned", "generation": 7,
        "ledger_sha256": "b" * 64, "wrapper": "/owned/wrapper", "workspace": "/owned/workspace",
        "profile": "owned-profile", "topology": "owned-runtime", "state": "scenario-owned",
        "plan": {"sha256": "c" * 64, "force_reconfigure": False, "use_ccache": True,
                 "retained_content_input": "d" * 40},
        "sources": [{"checkout": "content", "repository": "atrinik/content", "branch": "main",
                     "path": "/owned/content", "commit": "d" * 40, "tree": "e" * 40, "sha256": "f" * 64}],
        "artifacts": [{"path": "/owned/workspace/build/profiles/tested/server", "sha256": "1" * 64}],
        "content_sha256": "2" * 64, "issued_at": 1000, "expires_at": 1100,
        "lease_id": "3" * 64,
    }
    value["commands"] = handoff.commands(value)
    return value


class EnvelopeTests(unittest.TestCase):
    def test_canonical_round_trip_contains_only_public_projection(self):
        value = envelope()
        raw = handoff.canonical(value)
        self.assertEqual(handoff.decode(raw), value)
        self.assertEqual(len(handoff.digest(value)), 64)
        for private in (b"raw_base64", b"password", b"token", b"build_log", b"reviews/"):
            self.assertNotIn(private, raw)
        self.assertEqual(value["commands"]["inspect"][:4],
                         ["./atrinik", "topology", "show", "owned-profile"])

    def test_unknown_fields_at_every_boundary_are_rejected(self):
        for location in ((), ("issue",), ("plan",), ("sources", 0), ("artifacts", 0)):
            with self.subTest(location=location):
                value = envelope()
                row = value
                for key in location:
                    row = row[key]
                row["private_worker_result"] = "secret"
                with self.assertRaises(WorkspaceError):
                    handoff.validate(value)

    def test_malformed_bounded_and_noncanonical_json_is_rejected(self):
        raw = handoff.canonical(envelope())
        for invalid in (b"{" * (handoff.MAX_BYTES + 1), b"\xff", b"[]", raw + b" ",
                        raw.replace(b'"schema_version":1', b'"schema_version":1,"schema_version":1'),
                        b"[" * 2000 + b"]" * 2000):
            with self.subTest(size=len(invalid)), self.assertRaises(WorkspaceError):
                handoff.decode(invalid)

    def test_scalar_schema_and_path_rejections(self):
        cases = [("schema_version", True), ("generation", True), ("generation", 0),
                 ("issued_at", 0), ("expires_at", 2000), ("lease_id", "foreign"),
                 ("wrapper", "/owned/../foreign"), ("workspace", "/owned//workspace"),
                 ("profile", "../other"), ("state", None)]
        for key, new in cases:
            with self.subTest(key=key, new=new):
                value = envelope(); value[key] = new
                with self.assertRaises(WorkspaceError):
                    handoff.validate(value)

    def test_duplicate_or_oversized_coordinates_and_injected_commands_are_rejected(self):
        for field in ("sources", "artifacts"):
            value = envelope(); value[field] *= 2
            with self.assertRaisesRegex(WorkspaceError, "ambiguous"):
                handoff.validate(value)
        value = envelope(); value["commands"]["start"].extend(["--server-listener", "all-ipv4"])
        with self.assertRaisesRegex(WorkspaceError, "commands"):
            handoff.validate(value)
        value = envelope(); value["artifacts"][0]["path"] += "a" * handoff.MAX_BYTES
        with self.assertRaisesRegex(WorkspaceError, "bound"):
            handoff.validate(value)

    def test_exact_binding_and_freshness(self):
        value = envelope()
        bound = dict(issue="atrinik/atrinik#604", attempt="a" * 64, wrapper=value["wrapper"],
                     workspace=value["workspace"], profile=value["profile"], topology=value["topology"],
                     state=value["state"], plan=value["plan"]["sha256"], now=1050)
        handoff.require_binding(value, **bound)
        handoff.require_binding(value, **{**bound, "topology": None})
        for key in bound.keys() - {"now"}:
            with self.subTest(key=key), self.assertRaises(WorkspaceError):
                handoff.require_binding(value, **{**bound, key: "foreign"})
        for now in (999, 1100, 1101):
            with self.subTest(now=now), self.assertRaisesRegex(WorkspaceError, "stale"):
                handoff.require_binding(value, **{**bound, "now": now})
        self.assertEqual(value, envelope())


import contextlib
import multiprocessing
import os
from pathlib import Path
import socket
import sys
import tempfile
import time
from unittest.mock import patch

from atrinik_workspace.platform_compat import fcntl


def serve_fixture(value, ready, authority_path, generation_path):
    @contextlib.contextmanager
    def guard():
        with open(authority_path, "rb") as authority:
            fcntl.flock(authority, fcntl.LOCK_EX | fcntl.LOCK_NB)
            def recheck():
                if Path(generation_path).read_text() != str(value["generation"]):
                    raise WorkspaceError("fixture CAS changed")
            yield recheck
    handoff.publish(value, guard, ready=lambda _result: ready.set())


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux shared-mount handoff contract")
class PublicLeaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.builds = self.root / "build"
        self.builds.mkdir()
        self.value = envelope()
        now = int(time.time())
        self.value.update(workspace=str(self.root), issued_at=now, expires_at=now + 60)
        self.value["commands"] = handoff.commands(self.value)
        self.authority = self.root / "private-authority.lock"
        self.authority.touch(mode=0o600)
        self.generation = self.root / "private-generation"
        self.generation.write_text(str(self.value["generation"]))
        self.binding = dict(issue="atrinik/atrinik#604", attempt=self.value["attempt_sha256"],
                            wrapper=self.value["wrapper"], workspace=self.value["workspace"],
                            profile=self.value["profile"], topology=self.value["topology"],
                            state=self.value["state"], plan=self.value["plan"]["sha256"])
        self.context = multiprocessing.get_context("fork")
        self.ready = self.context.Event()
        self.process = self.context.Process(target=serve_fixture,
            args=(self.value, self.ready, self.authority, self.generation))
        self.process.start()
        self.addCleanup(self.stop)
        self.assertTrue(self.ready.wait(5), "publisher did not become ready")
        self.directory = self.builds / "runtime-handoffs" / self.value["lease_id"]

    def stop(self):
        if self.process.is_alive():
            self.process.terminate()
        self.process.join(5)
        self.assertFalse(self.process.is_alive())

    def consume(self):
        return handoff.consume(self.builds, self.value["lease_id"], self.binding)

    def test_forward_reconnect_and_operation_guard_without_private_reads(self):
        original = (self.directory / "envelope.json").read_bytes()
        for _ in range(2):
            # The executor's public consumer must not open coordinator files.
            original_open = os.open
            def public_only(path, *args, **kwargs):
                self.assertNotIn("private-", os.fspath(path))
                return original_open(path, *args, **kwargs)
            with patch.object(handoff.os, "open", side_effect=public_only):
                with self.consume() as (value, recheck):
                    self.assertEqual(value, self.value)
                    recheck()
                    with self.authority.open("rb") as writer:
                        with self.assertRaises(BlockingIOError):
                            fcntl.flock(writer, fcntl.LOCK_EX | fcntl.LOCK_NB)
        self.assertEqual((self.directory / "envelope.json").read_bytes(), original)

    def test_generation_change_is_rejected_before_consume(self):
        self.generation.write_text("8")
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("stale authority reached runtime mutation")

    def test_changed_declaration_and_endpoint_rejected_at_recheck(self):
        with self.consume() as (_value, recheck):
            endpoint = self.directory / "endpoint.sock"
            endpoint.unlink()
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as replacement:
                with handoff.public_directory(self.builds, self.value["lease_id"]) as fd:
                    replacement.bind(f"/proc/self/fd/{fd}/endpoint.sock")
                with self.assertRaises(WorkspaceError):
                    recheck()

    def test_publisher_death_rejected_at_recheck_preserves_history(self):
        raw = (self.directory / "envelope.json").read_bytes()
        with self.assertRaises(WorkspaceError):
            with self.consume() as (_value, recheck):
                self.stop()
                recheck()
        self.assertEqual((self.directory / "envelope.json").read_bytes(), raw)
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("dead publisher admitted")

    def test_revocation_preserves_history_and_rejects_reconnect(self):
        raw = (self.directory / "envelope.json").read_bytes()
        with self.assertRaises(WorkspaceError):
            handoff.revoke(self.builds, self.value["lease_id"], issue="atrinik/atrinik#1",
                           attempt=self.value["attempt_sha256"])
        result = handoff.revoke(self.builds, self.value["lease_id"], issue=self.binding["issue"],
                                attempt=self.binding["attempt"])
        self.assertEqual(result["status"], "revoked")
        self.process.join(5)
        self.assertEqual(self.process.exitcode, 0)
        self.assertEqual((self.directory / "envelope.json").read_bytes(), raw)
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("revoked publisher admitted")

    def test_exact_retry_and_foreign_retry(self):
        self.stop()
        self.ready.clear()
        self.process = self.context.Process(target=serve_fixture,
            args=(self.value, self.ready, self.authority, self.generation))
        self.process.start()
        self.assertTrue(self.ready.wait(5))
        with self.consume() as (_, recheck):
            recheck()
        self.stop()
        changed = copy.deepcopy(self.value); changed["generation"] += 1
        @contextlib.contextmanager
        def unused():
            yield lambda: None
        with self.assertRaisesRegex(WorkspaceError, "historical"):
            handoff.publish(changed, unused)

    def test_symlinked_namespace_or_envelope_rejected(self):
        self.stop()
        original = self.directory / "envelope.json"
        retained = self.directory / "retained.json"
        original.rename(retained)
        original.symlink_to(retained.name)
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("symlink admitted")

    def test_expiry_permissions_and_peer_mismatch_refuse_before_use(self):
        with patch.object(handoff.time, "time", return_value=self.value["expires_at"]):
            with self.assertRaises(WorkspaceError):
                with self.consume():
                    self.fail("expired envelope admitted")
        path = self.directory / "envelope.json"
        path.chmod(0o644)
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("publicly writable or readable envelope admitted")
        path.chmod(0o600)
        with patch.object(handoff, "_peer", side_effect=WorkspaceError("foreign peer")):
            with self.assertRaisesRegex(WorkspaceError, "foreign peer"):
                with self.consume():
                    self.fail("foreign peer admitted")

    def test_changed_envelope_refuses_without_altering_historical_file(self):
        path = self.directory / "envelope.json"
        changed = copy.deepcopy(self.value); changed["generation"] += 1
        raw = handoff.canonical(changed)
        path.write_bytes(raw)
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("changed envelope admitted")
        self.assertEqual(path.read_bytes(), raw)

    def test_revoked_lease_cannot_be_republished(self):
        handoff.revoke(self.builds, self.value["lease_id"], issue=self.binding["issue"],
                       attempt=self.binding["attempt"])
        self.process.join(5)
        @contextlib.contextmanager
        def unused():
            yield lambda: None
        with self.assertRaisesRegex(WorkspaceError, "permanently revoked"):
            handoff.publish(self.value, unused)

    def test_executor_death_releases_operation_guard(self):
        admitted = self.context.Event()
        def executor():
            with self.consume():
                admitted.set()
                os._exit(0)
        child = self.context.Process(target=executor)
        child.start()
        self.assertTrue(admitted.wait(5))
        child.join(5)
        self.assertEqual(child.exitcode, 0)
        # A new connection performs a fresh guard, proving the dead consumer
        # did not retain authority or deadlock a subsequent operation.
        with self.consume() as (_, recheck):
            recheck()


class HandoffCliTests(unittest.TestCase):
    def test_exact_consumer_coordinates_are_forwarded(self):
        from atrinik_workspace.cli import main
        for arguments, method in ((["up"], "topology_up"),
                                  (["topology", "show", "owned-profile"], "topology_summary")):
            with self.subTest(arguments=arguments), patch("atrinik_workspace.cli.Workspace") as workspace, patch("builtins.print"):
                getattr(workspace.return_value, method).return_value = {}
                self.assertEqual(main([*arguments, "--retained-build-plan", "c" * 64,
                    "--runtime-handoff", "3" * 64, "--handoff-issue", "atrinik/atrinik#604",
                    "--handoff-attempt", "a" * 64, "--json"]), 0)
                kwargs = getattr(workspace.return_value, method).call_args.kwargs
                self.assertEqual(kwargs["runtime_handoff"], "3" * 64)
                self.assertEqual(kwargs["handoff_issue"], "atrinik/atrinik#604")
                self.assertEqual(kwargs["handoff_attempt"], "a" * 64)
