from __future__ import annotations

import fcntl
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import process_tree


class ProcessTreeIdentityTests(unittest.TestCase):
    def test_lease_lock_is_namespace_independent_liveness_coordinate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "process-tree.lease"
            descriptor = os.open(path, os.O_RDWR | os.O_CREAT, 0o600)
            try:
                self.assertFalse(process_tree.lease_locked(path))
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self.assertTrue(process_tree.lease_locked(path))
            finally:
                os.close(descriptor)

    def test_bound_lease_accepts_same_path_and_generation_after_copy(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "process-tree.lease"
            descriptor = os.open(path, os.O_RDWR | os.O_CREAT, 0o600)
            try:
                fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
                identity = process_tree.initialize_lease(descriptor, "a" * 64)
                self.assertTrue(
                    process_tree.bound_lease_locked(path, "a" * 64, identity)
                )
                path.unlink()
                path.write_text("a" * 64 + "\n", encoding="utf-8")
                self.assertFalse(process_tree.bound_lease_locked(path, "a" * 64, identity))
            finally:
                os.close(descriptor)

    def test_bound_lease_rejects_missing_symlink_and_changed_generation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "process-tree.lease"
            descriptor = os.open(path, os.O_RDWR | os.O_CREAT, 0o600)
            identity = process_tree.initialize_lease(descriptor, "a" * 64)
            os.close(descriptor)
            with self.assertRaisesRegex(OSError, "generation changed"):
                process_tree.bound_lease_locked(path, "b" * 64, identity)
            path.unlink()
            with self.assertRaises(FileNotFoundError):
                process_tree.bound_lease_locked(path, "a" * 64, identity)
            target = root / "target"
            target.write_text("a" * 64 + "\n", encoding="utf-8")
            path.symlink_to(target)
            with self.assertRaises(OSError):
                process_tree.bound_lease_locked(path, "a" * 64, identity)

    def test_holders_require_path_generation_and_read_write_access(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "lease"
            payload = ("a" * 64 + "\n").encode()
            path.write_bytes(payload)
            reader = os.open(path, os.O_RDONLY)
            try:
                self.assertFalse(process_tree._holds_lease(os.getpid(), str(path), payload))
                writer = os.open(path, os.O_RDWR)
                try:
                    self.assertTrue(process_tree._holds_lease(os.getpid(), str(path), payload))
                    self.assertFalse(process_tree._holds_lease(os.getpid(), str(path), b"wrong"))
                    self.assertFalse(process_tree._holds_lease(os.getpid(), str(path) + "-other", payload))
                    with mock.patch.object(Path, "read_text", return_value="flags:\tinvalid\n"):
                        self.assertFalse(process_tree._holds_lease(os.getpid(), str(path), payload))
                    path.rename(path.with_name("moved"))
                    self.assertFalse(process_tree._holds_lease(os.getpid(), str(path), payload))
                finally:
                    os.close(writer)
            finally:
                os.close(reader)

    @unittest.skipUnless(hasattr(os, "O_PATH"), "requires Linux O_PATH observers")
    def test_path_observer_can_find_and_signal_holders_without_becoming_one(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "lease"
            writer = os.open(path, os.O_RDWR | os.O_CREAT, 0o600)
            try:
                process_tree.initialize_lease(writer, "a" * 64)
                observer = os.open(path, os.O_PATH | os.O_CLOEXEC)
                try:
                    self.assertTrue(process_tree.holders_exist(observer))
                    self.assertFalse(process_tree.holders_exist(observer, exclude=(os.getpid(),)))
                    # Signal zero probes existence without changing process state.
                    self.assertEqual(process_tree.signal_holders(observer, 0), 1)
                    self.assertEqual(process_tree.signal_holders(observer, 0, exclude=(os.getpid(),)), 0)
                    os.close(writer)
                    writer = None
                    self.assertFalse(process_tree.holders_exist(observer))
                    self.assertEqual(process_tree.signal_holders(observer, 0), 0)
                finally:
                    os.close(observer)
            finally:
                if writer is not None:
                    os.close(writer)

    def test_bound_lease_ignores_legacy_filesystem_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "lease"
            path.write_text("a" * 64 + "\n")
            self.assertFalse(process_tree.bound_lease_locked(
                path, "a" * 64, {"device": 123, "inode": 456, "ctime_ns": 789}
            ))
            with self.assertRaisesRegex(OSError, "path changed"):
                process_tree.bound_lease_locked(path, "a" * 64, {"path": str(path) + "-other"})


if __name__ == "__main__":
    unittest.main()
