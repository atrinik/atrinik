from __future__ import annotations

import copy
from contextlib import nullcontext
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tests.test_delivery_ledger import (
    archive_request, ledger, releasable_pr_ledger, release_request,
)


class LegacyTerminalPathTests(unittest.TestCase):
    def setUp(self):
        for name, replacement in (
            ("_authenticated_actor", lambda document, _context=None: document["actor"]),
            ("_prove_release_git", lambda *_args: None),
            ("_prove_release_github", lambda *_args: None),
            ("_release_live_safety", lambda *_args: nullcontext([])),
            ("_archive_live_safety", lambda *_args: nullcontext(lambda: None)),
        ):
            patcher = mock.patch.object(ledger, name, replacement)
            patcher.start()
            self.addCleanup(patcher.stop)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.snapshot = ledger.create(self.root, releasable_pr_ledger())
        self.request = release_request(self.snapshot)

    @staticmethod
    def legacy_identity(identity):
        identity.pop("path", None)
        identity.update(device=123456, inode=987654, ctime_ns=123)

    def release_bytes(self):
        document = ledger._release_document(self.snapshot, self.request)
        self.legacy_identity(document["ledger"])
        return ledger.canonical_bytes(document)

    def write(self, name, raw):
        with ledger._locked_root(self.root) as directory:
            ledger._write_exclusive(directory, name, raw)

    def install_release(self, raw=None):
        raw = self.release_bytes() if raw is None else raw
        self.write(ledger._release_name(self.snapshot.name), raw)
        return ledger.inventory(self.root).releases[0]

    def test_installed_release_preview_preserves_legacy_digest(self):
        released = self.install_release()
        preview = ledger.release_preview(self.root, self.snapshot.name, self.request)
        self.assertEqual(preview["plan_sha256"], released.digest)
        self.assertEqual((self.root / released.name).read_bytes(), released.raw)

    def test_installed_release_apply_preserves_legacy_bytes(self):
        released = self.install_release()
        actual = ledger.release_apply(
            self.root, self.snapshot.name, self.request, plan_sha256=released.digest,
        )
        self.assertEqual(actual.raw, released.raw)
        self.assertEqual(actual.digest, released.digest)

    def test_staged_release_retry_preserves_legacy_bytes(self):
        raw = self.release_bytes()
        digest = ledger.byte_digest(raw)
        stage = f".{self.snapshot.name}.release-{digest}.tmp"
        self.write(stage, raw)
        actual = ledger.release_apply(
            self.root, self.snapshot.name, self.request, plan_sha256=digest,
        )
        self.assertEqual(actual.raw, raw)
        self.assertFalse((self.root / stage).exists())

    def test_installed_release_retry_removes_legacy_stage(self):
        released = self.install_release()
        stage = f".{self.snapshot.name}.release-{released.digest}.tmp"
        (self.root / stage).hardlink_to(self.root / released.name)
        actual = ledger.release_apply(
            self.root, self.snapshot.name, self.request, plan_sha256=released.digest,
        )
        self.assertEqual(actual.raw, released.raw)
        self.assertFalse((self.root / stage).exists())

    def test_legacy_release_rejects_changed_authority(self):
        released = self.install_release()
        changed = copy.deepcopy(self.request)
        changed["authority"]["reference"] = "turn:another-release"
        with self.assertRaises(ledger.LedgerError):
            ledger.release_apply(
                self.root, self.snapshot.name, changed, plan_sha256=released.digest,
            )
        self.assertEqual((self.root / released.name).read_bytes(), released.raw)

    def test_legacy_release_rejects_wrong_explicit_path(self):
        document = ledger._decode(self.release_bytes(), "legacy test release")
        document["ledger"]["path"] = str(self.root / "other.ledger.json")
        self.write(ledger._release_name(self.snapshot.name), ledger.canonical_bytes(document))
        with self.assertRaises(ledger.LedgerError):
            ledger.release_preview(self.root, self.snapshot.name, self.request)

    def stage_archive(self):
        released = self.install_release()
        request = archive_request(self.snapshot, released)
        with ledger._locked_root(self.root) as directory:
            document, _raw, _digest, name = ledger._archive_plan_locked(
                directory, self.snapshot, released, request,
            )
        for member in document["members"]:
            self.legacy_identity(member)
        raw = ledger.canonical_bytes(document)
        digest = ledger.byte_digest(raw)
        stage = f".{self.snapshot.name}.archive-{self.snapshot.digest}-to-{digest}.tmp"
        self.write(stage, raw)
        return request, raw, digest, name, stage

    def test_staged_archive_retry_preserves_legacy_bytes(self):
        request, raw, digest, _name, stage = self.stage_archive()
        actual = ledger.archive_apply(
            self.root, self.snapshot.name, request, plan_sha256=digest,
        )
        self.assertEqual(actual.raw, raw)
        self.assertFalse((self.root / stage).exists())
        self.assertFalse((self.root / self.snapshot.name).exists())

    def test_installed_archive_retry_preserves_legacy_bytes(self):
        request, raw, digest, name, stage = self.stage_archive()
        (self.root / name).hardlink_to(self.root / stage)
        preview = ledger.archive_preview(self.root, self.snapshot.name, request)
        self.assertEqual(preview["plan_sha256"], digest)
        actual = ledger.archive_apply(
            self.root, self.snapshot.name, request, plan_sha256=digest,
        )
        self.assertEqual(actual.raw, raw)
        self.assertFalse((self.root / stage).exists())

    def test_staged_archive_rejects_changed_evidence(self):
        request, raw, digest, _name, stage = self.stage_archive()
        changed = copy.deepcopy(request)
        changed["authority"]["reference"] = "turn:another-archive"
        with self.assertRaises(ledger.LedgerError):
            ledger.archive_apply(
                self.root, self.snapshot.name, changed, plan_sha256=digest,
            )
        self.assertEqual((self.root / stage).read_bytes(), raw)
        self.assertTrue((self.root / self.snapshot.name).exists())

    def test_release_accepts_explicit_direct_path(self):
        document = ledger._decode(self.release_bytes(), "legacy test release")
        document["ledger"]["path"] = self.snapshot.name
        raw = ledger.canonical_bytes(document)
        released = self.install_release(raw)
        actual = ledger.release_apply(
            self.root, self.snapshot.name, self.request, plan_sha256=released.digest,
        )
        self.assertEqual(actual.raw, raw)

    def test_staged_release_still_requires_live_reproof(self):
        raw = self.release_bytes()
        digest = ledger.byte_digest(raw)
        stage = f".{self.snapshot.name}.release-{digest}.tmp"
        self.write(stage, raw)
        with mock.patch.object(
            ledger, "_prove_release_git",
            side_effect=[None, ledger.LedgerError("changed live Git proof")],
        ), self.assertRaisesRegex(ledger.LedgerError, "changed live Git proof"):
            ledger.release_apply(
                self.root, self.snapshot.name, self.request, plan_sha256=digest,
            )
        self.assertFalse((self.root / ledger._release_name(self.snapshot.name)).exists())
        self.assertEqual((self.root / stage).read_bytes(), raw)

    def test_staged_release_rejects_digest_mismatch(self):
        raw = self.release_bytes()
        digest = ledger.byte_digest(raw)
        stage = f".{self.snapshot.name}.release-{digest}.tmp"
        document = ledger._decode(raw, "legacy test release")
        document["authority"]["reference"] = "turn:tampered-release"
        changed = ledger.canonical_bytes(document)
        self.write(stage, changed)
        with self.assertRaises(ledger.LedgerError):
            ledger.release_apply(
                self.root, self.snapshot.name, self.request, plan_sha256=digest,
            )
        self.assertEqual((self.root / stage).read_bytes(), changed)

    def test_staged_archive_rejects_changed_member_bytes(self):
        report = self.root / self.snapshot.name.removesuffix(".ledger.json")
        report.write_bytes(b"original review evidence")
        request, raw, digest, _name, stage = self.stage_archive()
        report.write_bytes(b"changed review evidence")
        with self.assertRaisesRegex(ledger.LedgerError, "evidence differs"):
            ledger.archive_apply(
                self.root, self.snapshot.name, request, plan_sha256=digest,
            )
        self.assertEqual((self.root / stage).read_bytes(), raw)
        self.assertEqual(report.read_bytes(), b"changed review evidence")

    def test_staged_archive_rejects_changed_member_mode(self):
        request, raw, digest, _name, stage = self.stage_archive()
        (self.root / self.snapshot.name).chmod(0o400)
        with self.assertRaises(ledger.LedgerError):
            ledger.archive_apply(
                self.root, self.snapshot.name, request, plan_sha256=digest,
            )
        self.assertEqual((self.root / stage).read_bytes(), raw)
        self.assertTrue((self.root / self.snapshot.name).exists())
