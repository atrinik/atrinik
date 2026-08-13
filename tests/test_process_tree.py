from __future__ import annotations

import fcntl
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
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

    def test_bound_lease_rejects_replacement_and_generation_reuse(self) -> None:
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
                with self.assertRaisesRegex(OSError, "identity changed"):
                    process_tree.bound_lease_locked(path, "a" * 64, identity)
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

    def test_malformed_fdinfo_is_ignored(self) -> None:
        descriptor = mock.Mock()
        descriptor.name = "7"
        descriptor.stat.return_value = SimpleNamespace(st_dev=11, st_ino=22)

        for fdinfo in ("position:\t0\n", "flags:\tinvalid\n"):
            with self.subTest(fdinfo=fdinfo):
                with (
                    mock.patch.object(Path, "iterdir", return_value=[descriptor]),
                    mock.patch.object(Path, "read_text", return_value=fdinfo),
                ):
                    self.assertFalse(process_tree._holds_identity(123, (11, 22)))

    def test_observer_descriptor_is_not_a_process_tree_holder(self) -> None:
        descriptor = mock.Mock()
        descriptor.name = "7"
        descriptor.stat.return_value = SimpleNamespace(st_dev=11, st_ino=22)

        with (
            mock.patch.object(Path, "iterdir", return_value=[descriptor]),
            mock.patch.object(
                Path,
                "read_text",
                return_value=f"flags:\t{getattr(os, 'O_PATH', 0):o}\n",
            ),
        ):
            self.assertFalse(process_tree._holds_identity(123, (11, 22)))

        with (
            mock.patch.object(Path, "iterdir", return_value=[descriptor]),
            mock.patch.object(Path, "read_text", return_value="flags:\t0\n"),
        ):
            self.assertTrue(process_tree._holds_identity(123, (11, 22)))


if __name__ == "__main__":
    unittest.main()
