from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from tests import test_delivery_ledger as fixtures

ledger = fixtures.ledger


class LegacyCorrectionPathTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        fixtures.DeliveryLedgerTests.setUpClass()

    @classmethod
    def tearDownClass(cls) -> None:
        fixtures.DeliveryLedgerTests.tearDownClass()

    def setUp(self) -> None:
        fixtures.DeliveryLedgerTests.setUp(self)

    def tearDown(self) -> None:
        fixtures.DeliveryLedgerTests.tearDown(self)

    def test_partial_current_receipt_resumes_but_nonprefix_bytes_are_retained(self) -> None:
        bad_head = "f0f8d7493278dc691710056c79d0d63f1d802488"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            predecessor, _, actual_head, base_head, _ = fixtures.target_refresh_setup(
                self.live_base, root, "partial-correction-receipt"
            )
            erroneous, recovery_raw = fixtures.stale_coordinate_recovery(
                root, predecessor, actual_head, base_head, bad_head
            )
            arguments = {
                **fixtures.cas_arguments(erroneous),
                "bad_head": bad_head,
                "actual_head": actual_head,
                "actual_merge_base": base_head,
            }

            def correct(**extra):
                return ledger.correct_target_head(
                    root, erroneous.name, predecessor.raw, recovery_raw,
                    **arguments, **extra,
                )

            with self.assertRaises(ledger.InjectedCrash):
                correct(failpoint="correct-target-head:receipt")
            receipt_path = root / (
                f".{erroneous.name}.correct-target-head-{erroneous.digest}.json"
            )
            expected_raw = receipt_path.read_bytes()
            corrupt = b'{"unexpected":'
            receipt_path.write_bytes(corrupt)
            with self.assertRaises(ledger.LedgerError):
                correct()
            self.assertEqual(receipt_path.read_bytes(), corrupt)
            self.assertEqual((root / erroneous.name).read_bytes(), erroneous.raw)
            for size in (0, 1, len(expected_raw) // 2, len(expected_raw) - 1):
                with self.subTest(prefix_bytes=size):
                    receipt_path.write_bytes(expected_raw[:size])
                    with self.assertRaises(ledger.InjectedCrash):
                        correct(failpoint="correct-target-head:receipt")
                    self.assertEqual(receipt_path.read_bytes(), expected_raw)
                    self.assertEqual((root / erroneous.name).read_bytes(), erroneous.raw)
            corrected = correct()
            self.assertEqual(corrected.document["generation"], erroneous.document["generation"] + 1)
            self.assertEqual(receipt_path.read_bytes(), expected_raw)
            self.assertEqual(ledger.inventory(root).pending, ())

    def test_legacy_receipt_retains_bytes_and_reproves_content_and_git(self) -> None:
        bad_head = "f0f8d7493278dc691710056c79d0d63f1d802488"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            predecessor, _, actual_head, base_head, live = fixtures.target_refresh_setup(
                self.live_base, root, "legacy-correction-receipt"
            )
            erroneous, recovery_raw = fixtures.stale_coordinate_recovery(
                root, predecessor, actual_head, base_head, bad_head
            )
            recovery = json.loads(recovery_raw)
            installed = recovery["intent"]["installed"]
            installed.pop("path")
            installed.update(device=123, inode=456)
            recovery["grant"]["objective_sha256"] = ledger.canonical_object_digest(
                recovery["intent"]
            )
            recovery_raw = ledger.canonical_bytes(recovery)
            arguments = {
                **fixtures.cas_arguments(erroneous),
                "bad_head": bad_head,
                "actual_head": actual_head,
                "actual_merge_base": base_head,
            }

            def correct(**extra):
                return ledger.correct_target_head(
                    root, erroneous.name, predecessor.raw, recovery_raw,
                    **arguments, **extra,
                )

            with self.assertRaises(ledger.InjectedCrash):
                correct(failpoint="correct-target-head:receipt")
            receipt_path = root / (
                f".{erroneous.name}.correct-target-head-{erroneous.digest}.json"
            )
            receipt = json.loads(receipt_path.read_bytes())
            for field in ("source", "predecessor_snapshot", "erroneous_snapshot"):
                receipt[field].pop("path", None)
                receipt[field].update(device="ignored", inode=None, ctime_ns=-1)
            retained_raw = ledger.canonical_bytes(receipt)
            receipt_path.write_bytes(retained_raw)
            self.assertTrue(ledger.inventory(root).pending)

            for field in ("authority_sha256", "correction_digest", "source_generation"):
                with self.subTest(field=field):
                    changed = copy.deepcopy(receipt)
                    if field == "authority_sha256":
                        changed[field] = "0" * 64
                    elif field == "correction_digest":
                        changed["correction"]["sha256"] = "0" * 64
                    else:
                        changed["source"]["generation"] += 1
                    changed_raw = ledger.canonical_bytes(changed)
                    receipt_path.write_bytes(changed_raw)
                    with self.assertRaises(ledger.LedgerError):
                        correct()
                    self.assertEqual(receipt_path.read_bytes(), changed_raw)
                    self.assertEqual((root / erroneous.name).read_bytes(), erroneous.raw)
            receipt_path.write_bytes(retained_raw)
            dirty = live / "legacy-retry-dirty.txt"
            dirty.write_text("uncommitted change\n", encoding="utf-8")
            with self.assertRaises(ledger.LedgerError):
                correct()
            self.assertEqual(receipt_path.read_bytes(), retained_raw)
            dirty.unlink()

            corrected = correct()
            self.assertEqual(corrected.document["generation"], erroneous.document["generation"] + 1)
            self.assertEqual(receipt_path.read_bytes(), retained_raw)
            self.assertEqual(ledger.inventory(root).pending, ())
            self.assertEqual(correct().raw, corrected.raw)
            self.assertEqual(receipt_path.read_bytes(), retained_raw)


if __name__ == "__main__":
    unittest.main()
