from __future__ import annotations

import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tests.test_delivery_ledger import issue_ledger, ledger, next_generation


class DeliveryLedgerPathCasTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        actor = mock.patch.object(
            ledger, "_authenticated_actor", side_effect=lambda document, _context=None: document["actor"]
        )
        actor.start()
        self.addCleanup(actor.stop)

    def arguments(self, snapshot: object) -> dict[str, object]:
        return {
            "expected_generation": snapshot.document["generation"],
            "expected_digest": snapshot.digest,
            "expected_path": str(self.root / snapshot.name),
        }

    def test_same_path_replacement_with_identical_bytes_remains_updateable(self) -> None:
        initial = ledger.create(self.root, issue_ledger())
        replacement = self.root / "replacement"
        replacement.write_bytes(initial.raw)
        replacement.chmod(0o600)
        os.replace(replacement, self.root / initial.name)
        updated = ledger.cas(
            self.root, initial.name, next_generation(initial), **self.arguments(initial)
        )
        self.assertEqual(updated.document["generation"], 2)
        self.assertEqual(updated.path, str(self.root / initial.name))

    def test_different_expected_path_is_rejected(self) -> None:
        initial = ledger.create(self.root, issue_ledger())
        arguments = self.arguments(initial)
        arguments["expected_path"] = str(self.root / "different" / initial.name)
        with self.assertRaisesRegex(ledger.LedgerError, "path"):
            ledger.cas(self.root, initial.name, next_generation(initial), **arguments)
        self.assertEqual(ledger.inspect(self.root, initial.name).raw, initial.raw)
        self.assertEqual(ledger.inventory(self.root).pending, ())

    def test_generation_and_digest_remain_required(self) -> None:
        initial = ledger.create(self.root, issue_ledger())
        for key, value in (("expected_generation", 2), ("expected_digest", "0" * 64)):
            with self.subTest(key=key):
                arguments = {**self.arguments(initial), key: value}
                with self.assertRaises(ledger.LedgerError):
                    ledger.cas(self.root, initial.name, next_generation(initial), **arguments)
        self.assertEqual(ledger.inspect(self.root, initial.name).raw, initial.raw)

    def test_pending_cas_retries_keep_digest_proofs(self) -> None:
        for failpoint in (
            "cas:receipted", "cas:staged", "cas:proofed", "cas:renamed",
            "cas:installed", "cas:receipt-consumed",
        ):
            with self.subTest(failpoint=failpoint), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                initial = ledger.create(root, issue_ledger())
                arguments = {
                    "expected_generation": initial.document["generation"],
                    "expected_digest": initial.digest,
                    "expected_path": str(root / initial.name),
                }
                candidate = next_generation(initial)
                with self.assertRaises(ledger.InjectedCrash):
                    ledger.cas(root, initial.name, candidate, failpoint=failpoint, **arguments)
                updated = ledger.cas(root, initial.name, candidate, **arguments)
                self.assertEqual(updated.document["generation"], 2)
                self.assertEqual(ledger.inventory(root).pending, ())

    def test_retained_legacy_stage_resumes_without_filesystem_metadata(self) -> None:
        initial = ledger.create(self.root, issue_ledger())
        candidate = next_generation(initial)
        raw = ledger.canonical_bytes(candidate)
        stage = (
            f".{initial.name}.update-g2-from-{initial.digest}-"
            f"to-{ledger.byte_digest(raw)}.tmp"
        )
        with ledger._locked_root(self.root) as directory:
            ledger._ensure_stage(directory, stage, raw)
        updated = ledger.cas(
            self.root, initial.name, candidate, **self.arguments(initial)
        )
        self.assertEqual(updated.document["generation"], 2)
        self.assertEqual(ledger.inventory(self.root).pending, ())

    def test_pending_candidate_with_changed_bytes_is_not_consumed(self) -> None:
        initial = ledger.create(self.root, issue_ledger())
        candidate = next_generation(initial)
        arguments = self.arguments(initial)
        with self.assertRaises(ledger.InjectedCrash):
            ledger.cas(
                self.root, initial.name, candidate,
                failpoint="cas:staged", **arguments,
            )
        pending = ledger.inventory(self.root).pending
        stage = next(row.staging for row in pending if row.kind == "update")
        (self.root / stage).write_bytes(b"changed candidate bytes")
        with self.assertRaises(ledger.LedgerError):
            ledger.cas(self.root, initial.name, candidate, **arguments)
        self.assertEqual((self.root / initial.name).read_bytes(), initial.raw)
        self.assertEqual((self.root / stage).read_bytes(), b"changed candidate bytes")

    def test_cli_accepts_path_cas_coordinate(self) -> None:
        parsed = ledger.parser().parse_args([
            "cas", str(self.root), "ledger.md.ledger.json", "input.json",
            "--expected-generation", "1", "--expected-digest", "a" * 64,
            "--expected-path", str(self.root / "ledger.md.ledger.json"),
        ])
        self.assertEqual(parsed.expected_path, str(self.root / "ledger.md.ledger.json"))


if __name__ == "__main__":
    unittest.main()
