"""Temporary-state cleanup path recovery contracts."""

from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

from atrinik_workspace.cleanup import Cleanup, _temporary_state_lock_tombstone
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.path_identity import path_record


class TemporaryStatePathRecoveryTests(unittest.TestCase):
    def test_recovery_records_use_named_paths_without_policy_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cleanup = Cleanup.__new__(Cleanup)
            cleanup.paths = SimpleNamespace(topologies=root)
            state = root / "example" / "temporary-states" / ("a" * 64)
            pending = state.parent / f".{state.name}.removal-pending"
            item = {
                "path": str(state), "topology": "example",
                "generation": state.name, "_physical_path": str(pending),
                "state_policy": {"mode": "temporary", "path": str(state)},
            }
            evidence = cleanup._temporary_state_recovery_evidence(item)
            self.assertEqual(evidence["filesystem_identity"], path_record(state, kind="directory"))
            self.assertEqual(evidence["container_identity"], path_record(state.parent, kind="directory"))
            self.assertEqual(evidence["lease_identity"], path_record(Path(f"{state}.lock"), kind="file"))
            action = {"kind": "temporary-state", "path": str(state)}
            self.assertTrue(cleanup._valid_temporary_state_recovery(action, evidence))
            evidence["physical_path"] = str(root / "unrelated")
            self.assertFalse(cleanup._valid_temporary_state_recovery(action, evidence))

    def test_lease_tombstone_uses_path_and_rejects_ambiguity(self):
        with tempfile.TemporaryDirectory() as temporary:
            lock = Path(temporary) / ("a" * 64 + ".lock")
            tombstone = lock.parent / f".{lock.name}.remove-pending"
            tombstone.touch()
            identity = path_record(lock, kind="file")
            self.assertEqual(_temporary_state_lock_tombstone(lock, identity), tombstone)
            tombstone.unlink()
            tombstone.touch()
            self.assertEqual(_temporary_state_lock_tombstone(lock, identity), tombstone)
            legacy = lock.parent / f".{lock.name}.remove-1-2"
            legacy.touch()
            with self.assertRaisesRegex(WorkspaceError, "ambiguous"):
                _temporary_state_lock_tombstone(lock, identity)
            legacy.unlink()
            tombstone.unlink()
            tombstone.symlink_to(lock)
            self.assertIsNone(_temporary_state_lock_tombstone(lock, identity))
