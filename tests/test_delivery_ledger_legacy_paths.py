from __future__ import annotations

from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tests.test_delivery_ledger import issue_ledger, ledger, next_generation


class DeliveryLedgerLegacyPathTests(unittest.TestCase):
    def setUp(self):
        actor = mock.patch.object(
            ledger, "_authenticated_actor", side_effect=lambda document, _context=None: document["actor"]
        )
        actor.start()
        self.addCleanup(actor.stop)

    def test_legacy_compact_receipt_resumes_without_recomputing_object_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            initial = ledger.create(root, issue_ledger())
            candidate = next_generation(initial)
            arguments = {
                "expected_generation": initial.document["generation"],
                "expected_digest": initial.digest,
                "expected_path": initial.path,
            }
            with self.assertRaises(ledger.InjectedCrash):
                ledger.cas(root, initial.name, candidate, failpoint="cas:staged", **arguments)
            receipts = list(root.glob(".delivery-update-receipt-*.json"))
            self.assertEqual(len(receipts), 1)
            receipt_path = receipts[0]
            retained = ledger._decode(receipt_path.read_bytes(), "retained receipt")
            # Legacy compact names were derived partly from obsolete metadata.
            # Keep a distinct old transaction name so retry cannot accidentally
            # succeed by recomputing the new path-based transaction marker.
            marker = "e" * 64
            old_stage = retained["staging"]
            retained.update(
                marker=marker,
                staging=f".delivery-update-stage-{marker}.tmp",
                proof=f".delivery-update-proof-{marker}.tmp",
                receipt=f".delivery-update-receipt-{marker}.json",
                device=987654321,
                inode=123456789,
            )
            retained.pop("path")
            (root / old_stage).rename(root / retained["staging"])
            legacy_receipt = root / retained["receipt"]
            legacy_receipt.write_bytes(ledger.canonical_bytes(retained))
            legacy_receipt.chmod(0o600)
            receipt_path.unlink()
            observed = ledger.inventory(root)
            self.assertTrue(observed.pending)
            result = ledger.cas(root, initial.name, candidate, **arguments)
            self.assertEqual(result.document, candidate)
            self.assertEqual(result.path, initial.path)
            self.assertFalse(ledger.inventory(root).pending)


if __name__ == "__main__":
    unittest.main()
