from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest

from atrinik_workspace import linux_export as export


class LinuxExportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "client"
        self.root.mkdir()
        self.manifest = {"schema_version": 1,
                         "sources": {"atrinik/classic@main": "a" * 40},
                         "application_libraries": ["lib/libapp.so"],
                         "notices": ["licenses/NOTICE"], "files": {}}
        self.add_file("lib/libapp.so", b"library")
        self.add_file("licenses/NOTICE", b"license")
        self.add_file("bin/client", b"executable", executable=True)
        self.write_manifest()

    def add_file(self, name: str, content: bytes, *, executable: bool = False) -> None:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        path.chmod(0o755 if executable else 0o644)
        self.manifest["files"][name] = {"sha256": hashlib.sha256(content).hexdigest(),
                                      "size": len(content), "executable": executable}

    def write_manifest(self) -> None:
        (self.root / export.MANIFEST_NAME).write_text(json.dumps(self.manifest))

    def test_exact_closure_survives_movement_without_original_path(self) -> None:
        target = self.root.parent / "moved folder"
        self.root.rename(target)
        self.assertEqual(export.verify_export(target), self.manifest)

    def test_missing_corrupt_extra_and_wrong_mode_payloads_fail(self) -> None:
        target = self.root / "lib/libapp.so"
        for data in (b"corrupt", b"longer library", b""):
            target.write_bytes(data)
            with self.assertRaises(export.ExportError):
                export.verify_export(self.root)
        target.write_bytes(b"library")
        target.chmod(0o755)
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)
        target.chmod(0o644)
        extra = self.root / "unexpected"
        extra.write_text("extra")
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)
        extra.unlink()
        target.unlink()
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)

    def test_pointer_text_is_rejected_even_with_matching_hash(self) -> None:
        self.add_file("media/image.png", export.LFS_HEADER + b"oid sha256:" + b"a" * 64 + b"\nsize 123\n")
        self.write_manifest()
        with self.assertRaisesRegex(export.ExportError, "export-lfs-pointer"):
            export.verify_export(self.root)

    def test_symlink_payload_and_ancestor_are_rejected(self) -> None:
        original = self.root / "lib/libapp.so"
        original.unlink()
        original.symlink_to(self.root / "licenses/NOTICE")
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)
        alias = self.root.parent / "alias"
        alias.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(export.ExportError):
            export.verify_export(alias)

    def test_special_file_does_not_block_the_verifier(self) -> None:
        path = self.root / "lib/libapp.so"
        path.unlink()
        os.mkfifo(path)
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)

    def test_manifest_duplicate_keys_and_missing_legal_closure_rejected(self) -> None:
        with self.assertRaises(export.ExportError):
            export.load_manifest(b'{"schema_version":1,"schema_version":1}')
        for names in ([], ["missing"], [{}]):
            self.manifest["notices"] = names
            with self.assertRaises(export.ExportError):
                export.load_manifest(json.dumps(self.manifest).encode())

    def test_escaping_and_ambiguous_paths_rejected(self) -> None:
        for name in ("../out", "/out", "a/../b", "a//b", "a/./b", "a\\b", "a\nb", ""):
            with self.subTest(name=name):
                with self.assertRaises(export.ExportError):
                    export.relative_path(name)

    def test_false_schema_and_negative_sizes_rejected(self) -> None:
        self.manifest["schema_version"] = True
        with self.assertRaises(export.ExportError):
            export.load_manifest(json.dumps(self.manifest).encode())
        self.manifest["schema_version"] = 1
        self.manifest["files"]["bin/client"]["size"] = -1
        with self.assertRaises(export.ExportError):
            export.load_manifest(json.dumps(self.manifest).encode())

    def test_setid_payload_is_rejected(self) -> None:
        (self.root / "bin/client").chmod(0o4755)
        with self.assertRaises(export.ExportError):
            export.verify_export(self.root)


if __name__ == "__main__":
    unittest.main()
