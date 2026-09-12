"""Path-based cleanup inventory regressions."""

import os
from pathlib import Path
import shutil
import tempfile
import sys
import unittest
from unittest import mock

from atrinik_workspace.cleanup import Cleanup, _tree_usage, _temporary_tree_usage, _tree_usage_descriptor, _topology_tree_snapshot
from atrinik_workspace.path_identity import canonical_path, descriptor_path, path_record
from atrinik_workspace.workspace import _owned_tree_tombstone_path
from types import SimpleNamespace


@unittest.skipUnless(sys.platform.startswith("linux"), "requires POSIX descriptor paths")
class CleanupPathInventoryTests(unittest.TestCase):
    def test_usage_keys_are_paths_and_links_are_not_followed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "tree"
            root.mkdir()
            (root / "file").write_text("contents")
            os.link(root / "file", root / "alias")
            (root / "link").symlink_to(root, target_is_directory=True)
            sizes, _, error = _tree_usage(root)
            self.assertIsNone(error)
            self.assertEqual(set(sizes), {canonical_path(root), *(canonical_path(root / name) for name in ("file", "alias", "link"))})
            temporary_sizes, _, error = _temporary_tree_usage(root)
            self.assertIsNone(error)
            self.assertEqual(temporary_sizes, sizes)

    def test_copied_generation_preserves_content_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "tree"
            root.mkdir()
            (root / "file").write_text("contents")
            def observe():
                descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    return _tree_usage_descriptor(descriptor, root)
                finally:
                    os.close(descriptor)
            before = observe()
            replacement = root.with_name("replacement")
            shutil.copytree(root, replacement, copy_function=shutil.copy2)
            shutil.rmtree(root)
            replacement.rename(root)
            after = observe()
            self.assertIsNone(before[2])
            self.assertIsNone(after[2])
            self.assertEqual(before[4], after[4])
            self.assertEqual(set(before[0]), set(after[0]))
            (root / "file").write_text("changed content")
            self.assertNotEqual(after[4], observe()[4])

    def test_topology_snapshot_traverses_nested_directory_by_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "topology"
            nested = root / "nested"
            nested.mkdir(parents=True)
            (nested / "file").write_text("contents")
            digest, paths, _observed, sizes, error = _topology_tree_snapshot(root)
            self.assertIsNone(error)
            self.assertIsNotNone(digest)
            self.assertIn(canonical_path(nested / "file"), paths)
            self.assertIn(canonical_path(nested / "file"), sizes)

    def test_cleanup_receipt_resumes_named_tombstone(self):
        with tempfile.TemporaryDirectory() as temporary:
            workspace = Path(temporary)
            receipts = workspace / "cleanup-journals"
            receipts.mkdir()
            path = receipts / "receipt.json"
            tombstone = _owned_tree_tombstone_path(path, path_record(path))
            tombstone.write_text("{}")
            tombstone.chmod(0o600)
            cleanup = object.__new__(Cleanup)
            cleanup.paths = SimpleNamespace(workspace=workspace)
            cleanup._remove_cleanup_journal(
                {"path": canonical_path(path)}, path_record(path, kind="file"),
                removal_started=None,
            )
            self.assertFalse(tombstone.exists())

    def test_traversal_rejects_distinct_mountpoint_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "tree"
            root.mkdir()
            child = root / "mounted-file"
            child.write_text("contents")
            def mount_path(descriptor):
                return canonical_path(child) if descriptor_path(descriptor) == canonical_path(child) else "/"
            with mock.patch("atrinik_workspace.cleanup._descriptor_mount_path", side_effect=mount_path):
                descriptor = os.open(root, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    self.assertIsNotNone(_tree_usage_descriptor(descriptor, root)[2])
                finally:
                    os.close(descriptor)
                self.assertIn("mount", _temporary_tree_usage(root)[2])
                self.assertIn("mount", _topology_tree_snapshot(root)[4])
