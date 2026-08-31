from __future__ import annotations

import stat
from types import SimpleNamespace
import unittest

from atrinik_workspace.filesystem_identity import (
    FilesystemIdentityMigrationRequired,
    identity_matches,
    migrate_legacy_identity,
    pair_matches,
    portable_identity,
    portable_pair,
)


class FilesystemIdentityTests(unittest.TestCase):
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
        metadata = SimpleNamespace(
            st_mode=stat.S_IFDIR | 0o700,
            st_ino=1234,
            st_dev=88,
            st_ctime_ns=99,
        )
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
