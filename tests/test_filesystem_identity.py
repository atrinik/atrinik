from __future__ import annotations

import stat
import tempfile
from types import SimpleNamespace
import unittest
from pathlib import Path

from atrinik_workspace.filesystem_identity import (
    FilesystemIdentityError,
    FilesystemIdentityMigrationRequired,
    identity_digest,
    identity_matches,
    is_legacy_identity,
    is_portable_identity,
    migrate_legacy_identity,
    pair_matches,
    portable_identity,
    portable_identity_from_path,
    portable_device,
    portable_device_from_components,
    portable_pair,
    require_identity_match,
    validate_identity,
)


class FilesystemIdentityTests(unittest.TestCase):
    @staticmethod
    def metadata(
        mode: int,
        *,
        inode: int = 1234,
        device: int = 17,
        ctime_ns: int = 99,
    ) -> SimpleNamespace:
        return SimpleNamespace(
            st_mode=mode,
            st_ino=inode,
            st_dev=device,
            st_ctime_ns=ctime_ns,
        )

    def test_portable_identity_survives_a_changed_mount_device(self) -> None:
        metadata = SimpleNamespace(
            st_mode=stat.S_IFDIR | 0o700,
            st_ino=1234,
            st_dev=17,
            st_ctime_ns=99,
        )
        remounted = SimpleNamespace(
            st_mode=metadata.st_mode,
            st_ino=metadata.st_ino,
            st_dev=88,
            st_ctime_ns=metadata.st_ctime_ns,
        )

        identity = portable_identity(metadata)

        self.assertNotIn("st_dev", identity)
        self.assertTrue(identity_matches(identity, remounted))
        self.assertEqual(portable_pair(metadata), portable_pair(remounted))
        self.assertTrue(pair_matches(portable_pair(metadata), remounted))

    def test_regular_file_replacement_is_rejected_even_with_same_inode(self) -> None:
        original = SimpleNamespace(
            st_mode=stat.S_IFREG | 0o600,
            st_ino=1234,
            st_dev=17,
            st_ctime_ns=99,
        )
        replaced = SimpleNamespace(
            st_mode=original.st_mode,
            st_ino=original.st_ino,
            st_dev=88,
            st_ctime_ns=100,
        )

        self.assertFalse(identity_matches(portable_identity(original), replaced))

    def test_legacy_rebind_requires_explicit_confirmation_and_retains_evidence(self) -> None:
        metadata = self.metadata(stat.S_IFDIR | 0o700, device=88)
        legacy = {"device": 17, "inode": 1234}

        with self.assertRaises(FilesystemIdentityMigrationRequired):
            migrate_legacy_identity(
                legacy, metadata, "test identity", confirm_remount=False
            )
        migrated, evidence = migrate_legacy_identity(
            legacy, metadata, "test identity", confirm_remount=True
        )

        self.assertEqual(migrated["inode"], 1234)
        self.assertNotIn("device", migrated)
        self.assertEqual(evidence.json(), legacy)

    def test_validation_and_file_digest_rules_are_explicit(self) -> None:
        file_metadata = self.metadata(stat.S_IFREG | 0o600)
        directory_metadata = self.metadata(stat.S_IFDIR | 0o700)
        digest = "a" * 64
        identity = portable_identity(file_metadata, content_sha256=digest)

        self.assertTrue(is_portable_identity(identity))
        self.assertFalse(is_portable_identity({"device": 1, "inode": 2}))
        self.assertTrue(is_legacy_identity({"device": 1, "inode": 2}))
        self.assertFalse(is_legacy_identity({"device": True, "inode": 2}))
        self.assertEqual(identity_digest(identity), identity_digest(dict(identity)))
        self.assertTrue(identity_matches(identity, file_metadata, content_sha256=digest))
        self.assertFalse(identity_matches(identity, file_metadata))
        self.assertFalse(
            identity_matches(identity, file_metadata, content_sha256="b" * 64)
        )
        require_identity_match(
            identity, file_metadata, "portable file", content_sha256=digest
        )
        with self.assertRaisesRegex(FilesystemIdentityError, "portable file changed"):
            require_identity_match(
                identity,
                self.metadata(stat.S_IFREG | 0o600, ctime_ns=100),
                "portable file",
                content_sha256=digest,
            )

        without_ctime = portable_identity(file_metadata, include_ctime=False)
        self.assertNotIn("ctime_ns", without_ctime)
        with self.assertRaisesRegex(FilesystemIdentityError, "regular file or directory"):
            portable_identity(self.metadata(stat.S_IFIFO))
        with self.assertRaisesRegex(FilesystemIdentityError, "content digest"):
            portable_identity(file_metadata, content_sha256="not-a-digest")
        with self.assertRaisesRegex(FilesystemIdentityError, "directory identities"):
            portable_identity(directory_metadata, content_sha256=digest)

        invalid_values = (
            None,
            {"schema_version": 1, "kind": "socket", "inode": 1, "mode": stat.S_IFREG},
            {"schema_version": 1, "kind": "file", "inode": 1, "mode": stat.S_IFREG, "extra": 1},
            {"schema_version": 1, "kind": "file", "inode": 1, "mode": 1},
            {"schema_version": 1, "kind": "file", "inode": 1, "mode": stat.S_IFREG, "ctime_ns": -1},
            {"schema_version": 1, "kind": "file", "inode": 1, "mode": stat.S_IFREG, "sha256": "bad"},
            {"schema_version": 1, "kind": "directory", "inode": 1, "mode": stat.S_IFDIR, "sha256": digest},
        )
        for value in invalid_values:
            with self.subTest(value=value), self.assertRaises(FilesystemIdentityError):
                validate_identity(value, allow_legacy=False)

        for value in (
            {"device": -1, "inode": 2},
            {"device": True, "inode": 2},
            {"device": 1, "inode": -1},
            {"device": 1, "inode": 2, "extra": 3},
        ):
            self.assertFalse(pair_matches(value, file_metadata))
        self.assertFalse(pair_matches({"device": 1, "inode": 9999}, file_metadata))

    def test_path_and_historical_projection_helpers_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / "record"
            path.write_text("record\n", encoding="utf-8")
            self.assertEqual(portable_identity_from_path(path)["kind"], "file")
            link = root / "link"
            link.symlink_to(path)
            with self.assertRaises(FilesystemIdentityError):
                portable_identity_from_path(link)

        special = self.metadata(stat.S_IFLNK, inode=55, device=88)
        self.assertIsInstance(portable_device(special), int)
        self.assertEqual(
            portable_device_from_components(55, stat.S_IFREG, "file"),
            portable_device_from_components(55, stat.S_IFREG, "file"),
        )
        for inode, mode, kind in (
            (-1, stat.S_IFREG, "file"),
            (1, stat.S_IFREG, "socket"),
            (1, 1, "file"),
            (1, stat.S_IFDIR, "file"),
            (1, stat.S_IFREG, "directory"),
        ):
            with self.subTest(inode=inode, mode=mode, kind=kind), self.assertRaises(
                FilesystemIdentityError
            ):
                portable_device_from_components(inode, mode, kind)

    def test_legacy_and_portable_match_require_explicit_migration(self) -> None:
        metadata = self.metadata(stat.S_IFDIR | 0o700, device=88)
        legacy = {"device": 17, "inode": 1234}
        with self.assertRaises(FilesystemIdentityMigrationRequired):
            require_identity_match(legacy, metadata, "legacy directory")
        require_identity_match(
            {"device": 88, "inode": 1234}, metadata, "same-mount directory"
        )

        migrated, evidence = migrate_legacy_identity(
            portable_identity(metadata), metadata, "already portable", confirm_remount=False
        )
        self.assertEqual(migrated, portable_identity(metadata))
        self.assertIsNone(evidence)
        with self.assertRaisesRegex(FilesystemIdentityError, "inode changed"):
            migrate_legacy_identity(
                {"device": 17, "inode": 9999},
                metadata,
                "changed inode",
                confirm_remount=True,
            )
