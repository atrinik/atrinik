from __future__ import annotations

import os
from pathlib import Path
import tempfile
import unittest


from tests.test_delivery_ledger import ledger


class DeliveryLedgerPathIOTests(unittest.TestCase):
    def test_same_path_regular_replacement_can_be_removed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with ledger._locked_root(root) as directory:
                expected = ledger._write_exclusive(directory, "source", b"retained")
                (root / "source").rename(root / "previous")
                ledger._write_exclusive(directory, "source", b"retained")
                ledger._unlink_exact(directory, "source", expected)
                self.assertFalse((root / "source").exists())
                self.assertEqual((root / "previous").read_bytes(), b"retained")

    def test_unlink_rejects_wrong_expected_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with ledger._locked_root(root) as directory:
                expected = ledger._write_exclusive(directory, "source", b"retained")
                with self.assertRaises(ledger.LedgerError):
                    ledger._unlink_exact(directory, "source", expected,
                                         expected_path=str(root / "another"))
                self.assertEqual((root / "source").read_bytes(), b"retained")

    def test_legacy_quarantine_ignores_obsolete_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with ledger._locked_root(root) as directory:
                ledger._write_exclusive(directory, "source", b"retained")
                quarantine = root / ledger._UNLINK_QUARANTINE
                quarantine.mkdir(mode=0o700)
                transaction = quarantine / ("a" * 64)
                transaction.mkdir(mode=0o700)
                receipt = transaction / "receipt.json"
                receipt.write_bytes(ledger.canonical_bytes({
                    "schema_version": 1, "name": "source",
                    "device": {"ignored": True}, "inode": None,
                }))
                receipt.chmod(0o600)
                (root / "source").rename(transaction / "payload")
                ledger._recover_unlink_quarantine(directory)
                self.assertFalse(transaction.exists())
                self.assertFalse((root / "source").exists())

    def test_content_and_nofollow_guards_remain(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with ledger._locked_root(root) as directory:
                ledger._write_exclusive(directory, "stage", b"actual")
                os.link(root / "stage", root / "proof")
                self.assertFalse(ledger._discard_exact_cas_pending(
                    directory, "stage", "proof", b"different"))
                self.assertTrue((root / "stage").exists())
                (root / "symlink").symlink_to(root / "stage")
                with self.assertRaises(ledger.LedgerError):
                    ledger._read_regular(directory, "symlink")


if __name__ == "__main__":
    unittest.main()
