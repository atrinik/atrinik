from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

from atrinik_workspace.path_identity import (
    PathRecordError, canonical_path, descriptor_path, path_record,
    path_record_matches, validate_path_record,
)


class PathRecordTests(unittest.TestCase):
    def test_copy_at_same_coordinate_preserves_record(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "state"
            target.mkdir()
            (target / "record").write_text("generation one")
            record = path_record(target, kind="directory")
            moved = root / "old-state"
            target.rename(moved)
            shutil.copytree(moved, target)
            self.assertTrue(path_record_matches(record, target))
            self.assertFalse(path_record_matches(record, moved))
            self.assertEqual(record, {"path": os.path.normcase(str(target)), "kind": "directory"})

    def test_content_assertion_survives_copy_but_rejects_edits(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "record"
            path.write_bytes(b"one")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            record = path_record(path, kind="file", content_sha256=digest)
            replacement = path.with_suffix(".replacement")
            shutil.copyfile(path, replacement)
            replacement.replace(path)
            self.assertTrue(path_record_matches(record, path, content_sha256=digest))
            path.write_bytes(b"two")
            self.assertFalse(path_record_matches(record, path, content_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
            self.assertFalse(path_record_matches(record, path))

    def test_type_check_does_not_follow_symlinks(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "target"
            target.write_text("safe")
            link = root / "link"
            try:
                link.symlink_to(target)
            except OSError as error:
                self.skipTest(f"symbolic links unavailable: {error}")
            self.assertEqual(canonical_path(link), os.path.normcase(str(link)))
            self.assertFalse(path_record_matches(path_record(link, kind="file"), link))

    @unittest.skipUnless(sys.platform.startswith("linux") or sys.platform == "darwin", "descriptor paths require Linux or macOS")
    def test_descriptor_tracks_named_path_and_rejects_unlinked_target(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "record"
            with path.open("w+") as stream:
                self.assertEqual(descriptor_path(stream.fileno()), str(path))
                renamed = path.with_name("renamed")
                path.rename(renamed)
                self.assertEqual(descriptor_path(stream.fileno()), str(renamed))
                renamed.unlink()
                with self.assertRaises(OSError):
                    descriptor_path(stream.fileno())

    def test_invalid_records_are_rejected(self) -> None:
        for value in (None, {}, {"path": "relative"}, {"path": "/tmp", "extra": 1},
                      {"path": "/tmp", "kind": "socket"}, {"path": "/tmp", "kind": []}, {"path": "/tmp", "sha256": "bad"},
                      {"path": "/tmp", "kind": "directory", "sha256": "a" * 64}):
            with self.subTest(value=value), self.assertRaises(PathRecordError):
                validate_path_record(value)
        for path in ("", "bad\x00path"):
            with self.assertRaises(PathRecordError):
                canonical_path(path)

    def test_coordinate_normalizes_relative_components(self) -> None:
        root = Path(tempfile.gettempdir())
        self.assertEqual(canonical_path(root / "a" / ".." / "b"), os.path.normcase(str(root / "b")))
        self.assertEqual(canonical_path(Path(".")), os.path.normcase(os.getcwd()))
