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
        "lease_id": "3" * 64, "publisher_key": "4" * 64,
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
                 ("wrapper", "/owned/../foreign"), ("wrapper", "//owned/wrapper"), ("workspace", "/owned//workspace"),
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
                     state=value["state"], plan=value["plan"]["sha256"],
                     publisher=handoff.publisher_fingerprint(value["publisher_key"]), endpoint="5" * 64, now=1050)
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


def serve_fixture(value, ready, authority_path, generation_path, signer):
    @contextlib.contextmanager
    def guard():
        with open(authority_path, "rb") as authority:
            fcntl.flock(authority, fcntl.LOCK_EX | fcntl.LOCK_NB)
            def recheck():
                if Path(generation_path).read_text() != str(value["generation"]):
                    raise WorkspaceError("fixture CAS changed")
            yield recheck
    handoff.publish(value, guard, signer=signer, ready=lambda _result: ready.set())


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux shared-mount handoff contract")
class PublicLeaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.builds = self.root / "build"
        self.builds.mkdir()
        self.value = envelope()
        self.signer = handoff.PublisherSigner()
        self.value["publisher_key"] = self.signer.public_key
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
                            state=self.value["state"], plan=self.value["plan"]["sha256"],
                            publisher=handoff.publisher_fingerprint(self.signer.public_key))
        self.context = multiprocessing.get_context("fork")
        self.ready = self.context.Event()
        self.process = self.context.Process(target=serve_fixture,
            args=(self.value, self.ready, self.authority, self.generation, self.signer))
        self.process.start()
        self.addCleanup(self.stop)
        self.assertTrue(self.ready.wait(5), "publisher did not become ready")
        self.directory = self.builds / "runtime-handoffs" / self.value["lease_id"]
        self.pin_endpoint()

    def pin_endpoint(self):
        with handoff.public_directory(self.builds, self.value["lease_id"]) as directory:
            self.binding["endpoint"] = handoff.endpoint_fingerprint(directory, self.builds, self.value["lease_id"])

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
                           attempt=self.value["attempt_sha256"], publisher=self.binding["publisher"], endpoint=self.binding["endpoint"])
        result = handoff.revoke(self.builds, self.value["lease_id"], issue=self.binding["issue"],
                                attempt=self.binding["attempt"], publisher=self.binding["publisher"], endpoint=self.binding["endpoint"])
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
            args=(self.value, self.ready, self.authority, self.generation, self.signer))
        self.process.start()
        self.assertTrue(self.ready.wait(5))
        self.pin_endpoint()
        with self.consume() as (_, recheck):
            recheck()
        self.stop()
        changed = copy.deepcopy(self.value); changed["generation"] += 1
        @contextlib.contextmanager
        def unused():
            yield lambda: None
        with self.assertRaisesRegex(WorkspaceError, "historical"):
            handoff.publish(changed, unused, signer=self.signer)

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
                       attempt=self.binding["attempt"], publisher=self.binding["publisher"], endpoint=self.binding["endpoint"])
        self.process.join(5)
        @contextlib.contextmanager
        def unused():
            yield lambda: None
        with self.assertRaisesRegex(WorkspaceError, "permanently revoked"):
            handoff.publish(self.value, unused, signer=self.signer)

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


    def test_fabricated_noop_publisher_cannot_replace_pinned_helper(self):
        self.stop()
        history = self.directory.with_name("history")
        self.directory.rename(history)
        attacker = handoff.PublisherSigner()
        forged = copy.deepcopy(self.value)
        forged["publisher_key"] = attacker.public_key
        forged["commands"] = handoff.commands(forged)
        self.ready.clear()
        def forge():
            @contextlib.contextmanager
            def noop():
                yield lambda: None
            handoff.publish(forged, noop, signer=attacker, ready=lambda _: self.ready.set())
        self.process = self.context.Process(target=forge)
        self.process.start()
        self.assertTrue(self.ready.wait(5))
        with self.assertRaises(WorkspaceError):
            with self.consume():
                self.fail("forged publisher reached runtime mutation")
        self.assertTrue((history / "envelope.json").exists())

    def test_namespace_rename_rejected_inside_active_session(self):
        with self.assertRaises(WorkspaceError):
            with self.consume() as (_, recheck):
                self.directory.rename(self.directory.with_name("renamed"))
                self.directory.mkdir(mode=0o700)
                recheck()
                self.fail("renamed namespace reached runtime mutation")

    def test_parent_namespace_rename_rejected_inside_active_session(self):
        with self.assertRaises(WorkspaceError):
            with self.consume() as (_, recheck):
                namespace = self.directory.parent
                namespace.rename(namespace.with_name("old-handoffs"))
                namespace.mkdir(mode=0o700)
                recheck()
                self.fail("renamed parent reached runtime mutation")

    def test_namespace_changed_while_waiting_for_proof_is_rejected(self):
        original = handoff._receive
        def replace_after_response(connection):
            response = original(connection)
            self.directory.rename(self.directory.with_name("replaced-during-proof"))
            self.directory.mkdir(mode=0o700)
            return response
        with self.assertRaises(WorkspaceError):
            with self.consume() as (_, recheck):
                with patch.object(handoff, "_receive", side_effect=replace_after_response):
                    recheck()
                self.fail("namespace replacement during proof reached mutation")

    def test_moved_leaf_under_replacement_parent_is_rejected(self):
        with self.assertRaises(WorkspaceError):
            with self.consume() as (_, recheck):
                namespace = self.directory.parent
                old = namespace.with_name("old-parent")
                namespace.rename(old)
                namespace.mkdir(mode=0o700)
                (old / self.value["lease_id"]).rename(self.directory)
                recheck()
                self.fail("original leaf under foreign parent reached mutation")

    def test_relay_endpoint_cannot_release_guard_then_admit_consumer(self):
        ready = self.context.Event()
        def relay():
            with handoff.public_directory(self.builds, self.value["lease_id"]) as directory:
                os.rename("endpoint.sock", "original.sock", src_dir_fd=directory, dst_dir_fd=directory)
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                    listener.bind(f"/proc/self/fd/{directory}/endpoint.sock")
                    os.chmod("endpoint.sock", 0o600, dir_fd=directory)
                    listener.listen(1)
                    listener.settimeout(2)
                    ready.set()
                    try:
                        incoming, _ = listener.accept()
                    except socket.timeout:
                        return
                    with incoming, socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as genuine:
                        genuine.connect(f"/proc/self/fd/{directory}/original.sock")
                        handoff._send(genuine, handoff._receive(incoming))
                        signed = handoff._receive(genuine)
                        genuine.close()  # Release coordinator guard before forwarding.
                        handoff._send(incoming, signed)
        proxy = self.context.Process(target=relay)
        proxy.start()
        try:
            self.assertTrue(ready.wait(5))
            with self.assertRaises(WorkspaceError):
                with self.consume():
                    self.fail("relay admitted consumer without coordinator guard")
        finally:
            proxy.join(3)
            if proxy.is_alive(): proxy.terminate(); proxy.join(5)
        self.assertEqual(proxy.exitcode, 0)

    def test_fifo_envelope_is_rejected_without_blocking(self):
        self.stop()
        path = self.directory / "envelope.json"
        path.unlink()
        os.mkfifo(path, 0o600)
        finished = self.context.Event()
        def read_fifo():
            try:
                with self.consume():
                    return
            except WorkspaceError:
                finished.set()
        reader = self.context.Process(target=read_fifo)
        reader.start()
        try:
            self.assertTrue(finished.wait(2), "FIFO blocked the public read")
        finally:
            if reader.is_alive(): reader.terminate()
            reader.join(5)

    def test_atime_only_read_change_is_not_content_drift(self):
        path = self.directory / "envelope.json"
        before = path.stat()
        os.utime(path, ns=(before.st_atime_ns - 10_000_000_000, before.st_mtime_ns))
        with self.consume() as (_, recheck):
            recheck()

    def test_signed_reply_rejects_nonce_replay_and_signature_forgery(self):
        expected = {"nonce": "1" * 64, "status": "verified"}
        packet = {"response": expected, "signature": self.signer.sign(handoff.canonical(expected))}
        self.assertTrue(handoff.verify_response(packet, self.signer.public_key, expected))
        self.assertFalse(handoff.verify_response(packet, self.signer.public_key, {**expected, "nonce": "2" * 64}))
        packet["signature"] = "0" * 128
        self.assertFalse(handoff.verify_response(packet, self.signer.public_key, expected))


class HandoffCliTests(unittest.TestCase):
    def test_exact_consumer_coordinates_are_forwarded(self):
        from atrinik_workspace.cli import main
        for arguments, method in ((["up"], "topology_up"),
                                  (["topology", "show", "owned-profile"], "topology_summary")):
            with self.subTest(arguments=arguments), patch("atrinik_workspace.cli.Workspace") as workspace, patch("builtins.print"):
                getattr(workspace.return_value, method).return_value = {}
                self.assertEqual(main([*arguments, "--retained-build-plan", "c" * 64,
                    "--runtime-handoff", "3" * 64, "--handoff-issue", "atrinik/atrinik#604",
                    "--handoff-attempt", "a" * 64, "--handoff-publisher", "b" * 64, "--handoff-endpoint", "c" * 64, "--json"]), 0)
                kwargs = getattr(workspace.return_value, method).call_args.kwargs
                self.assertEqual(kwargs["runtime_handoff"], "3" * 64)
                self.assertEqual(kwargs["handoff_issue"], "atrinik/atrinik#604")
                self.assertEqual(kwargs["handoff_attempt"], "a" * 64)
                self.assertEqual(kwargs["handoff_publisher"], "b" * 64)
                self.assertEqual(kwargs["handoff_endpoint"], "c" * 64)
