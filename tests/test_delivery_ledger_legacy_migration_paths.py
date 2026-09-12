from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_delivery_ledger import issue_ledger, ledger, legacy_report_bytes


class LegacyMigrationPathTests(unittest.TestCase):
    def _source(self, root):
        document = issue_ledger()
        name = "atrinik-atrinik-419.md"
        raw = legacy_report_bytes(document)
        (root / name).write_bytes(raw)
        return document, name, ledger.byte_digest(raw)

    def _crash(self, root, document, name, digest, point):
        with self.assertRaises(ledger.InjectedCrash):
            ledger.migrate(root, name, document, kind="legacy",
                           expected_source_digest=digest, failpoint=point)

    def _old_identity(self, identity):
        retained = dict(identity)
        retained.pop("path", None)
        retained.update(device={"obsolete": True}, inode=None)
        return retained

    def test_retained_legacy_plan_preserves_original_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document, name, digest = self._source(root)
            self._crash(root, document, name, digest, "migration:plan-staged")
            staged = next(root.glob("*.planned-*.tmp"))
            marker = json.loads(staged.read_bytes())
            marker["source"] = self._old_identity(marker["source"])
            retained_raw = ledger.canonical_bytes(marker)
            marker_name = f".{ledger.canonical_name(document)}.migration.json"
            retained = root / f"{marker_name}.planned-{ledger.byte_digest(retained_raw)}.tmp"
            staged.rename(retained)
            retained.write_bytes(retained_raw)
            self._crash(root, document, name, digest, "migration:planned")
            self.assertEqual((root / marker_name).read_bytes(), retained_raw)
            resumed = ledger.migrate(root, name, document, kind="legacy",
                                     expected_source_digest=digest)
            self.assertEqual(resumed.document["migration"]["source"], marker["source"])
            self.assertEqual(ledger.inventory(root).pending, ())

    def _retained_preparation(self, root):
        document, name, digest = self._source(root)
        self._crash(root, document, name, digest, "migration:report")
        target = ledger.canonical_name(document)
        marker_name = f".{target}.migration.json"
        marker_file = root / marker_name
        marker = json.loads(marker_file.read_bytes())
        marker["source"] = self._old_identity(marker["source"])
        marker_file.write_bytes(ledger.canonical_bytes(marker))
        snapshot_name = marker["snapshot_name"]
        snapshot = self._old_identity({
            "name": snapshot_name,
            "sha256": ledger.byte_digest((root / snapshot_name).read_bytes()),
        })
        migration = {
            "kind": "legacy", "state": "complete", "source": marker["source"],
            "snapshot": snapshot, "canonical_report": marker["canonical_report"],
            "marker_name": marker_name,
        }
        expected = ledger.canonical_bytes(ledger.prepare({**document, "migration": migration}))
        prepared = {**marker, "state": "prepared", "snapshot": snapshot,
                    "destination_digest": ledger.byte_digest(expected)}
        staged = root / f"{marker_name}.prepared.tmp"
        staged.write_bytes(ledger.canonical_bytes(prepared))
        staged.chmod(0o600)
        return document, name, digest, expected, staged

    def test_retained_preparation_preserves_destination_digest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document, name, digest, expected, _ = self._retained_preparation(root)
            resumed = ledger.migrate(root, name, document, kind="legacy",
                                     expected_source_digest=digest)
            self.assertEqual(resumed.raw, expected)
            self.assertEqual(resumed.digest, ledger.byte_digest(expected))
            self.assertEqual(ledger.inventory(root).pending, ())

    def test_retained_preparation_rejects_changed_destination_digest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document, name, digest, _, staged = self._retained_preparation(root)
            retained = json.loads(staged.read_bytes())
            retained["destination_digest"] = "f" * 64
            staged.write_bytes(ledger.canonical_bytes(retained))
            with self.assertRaises(ledger.LedgerError):
                ledger.migrate(root, name, document, kind="legacy",
                               expected_source_digest=digest)
            self.assertFalse((root / ledger.canonical_name(document)).exists())

    def test_retained_preparation_rejects_changed_authority(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document, name, digest, _, staged = self._retained_preparation(root)
            changed = copy.deepcopy(document)
            changed["authority"]["reference"] = "goal:another-authority"
            retained = staged.read_bytes()
            with self.assertRaises(ledger.LedgerError):
                ledger.migrate(root, name, changed, kind="legacy",
                               expected_source_digest=digest)
            self.assertEqual(staged.read_bytes(), retained)
            self.assertFalse((root / ledger.canonical_name(document)).exists())


if __name__ == "__main__":
    unittest.main()
