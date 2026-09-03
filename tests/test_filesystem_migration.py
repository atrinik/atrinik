from __future__ import annotations

import base64
import json
import os
from pathlib import Path
import stat
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

import atrinik_workspace.filesystem_migration as migration
from atrinik_workspace.filesystem_identity import portable_identity, portable_pair
from atrinik_workspace.filesystem_migration import FilesystemMigrationError


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


class FilesystemMigrationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.repository = Path(self.temporary.name)
        (self.repository / ".git").mkdir()
        self.workspace = self.repository / "workspace"
        self.workspace.mkdir()

    def make_state_record(self, name: str) -> tuple[Path, Path]:
        state = self.workspace / f"state-{name}"
        state.mkdir()
        record = self.workspace / f"{name}.json"
        write_json(
            record,
            {
                "state_policy": {
                    "identity": {
                        "device": state.stat().st_dev + 1,
                        "inode": state.stat().st_ino,
                    },
                    "path": str(state),
                }
            },
        )
        return record, state

    def test_dry_run_and_apply_cover_topology_and_delivery_ledger_records(self) -> None:
        topology = self.workspace / "topologies" / "demo"
        topology.mkdir(parents=True)
        process_lease = topology / "process-tree.lease"
        process_lease.write_text("process\n", encoding="utf-8")
        generation = topology / "generations" / "g1"
        generation.mkdir(parents=True)
        generation_lease = generation / "generation.lease"
        generation_lease.write_text("generation\n", encoding="utf-8")
        state = topology / "state"
        state.mkdir()
        state_lock = Path(f"{state}.lock")
        state_lock.write_text("state\n", encoding="utf-8")
        output = state / "tmp" / "runtime-assets" / "g1"
        output.mkdir(parents=True)
        reservation = self.repository / "reservations"
        reservation.mkdir()
        reservation_lease = reservation / "demo.lease"
        reservation_lease.write_text("reservation\n", encoding="utf-8")
        topology_record = topology / "status.json"
        old = lambda path: {"device": path.stat().st_dev + 1, "inode": path.stat().st_ino}
        write_json(
            topology_record,
            {
                "control": {"lease": old(process_lease)},
                "port_reservation": {
                    "directory": old(reservation),
                    "lease": old(reservation_lease),
                    "path": str(reservation_lease),
                },
                "runtime": {
                    "lease": old(generation_lease),
                    "mutable_state_output_identities": [old(output)],
                    "mutable_state_outputs": [str(output)],
                    "path": str(generation),
                },
                "state_policy": {
                    "identity": old(state),
                    "lease_identity": old(state_lock),
                    "path": str(state),
                },
            },
        )

        review_root = self.repository / "build" / "reviews"
        review = review_root / "demo.md.ledger.json"
        write_json(
            review,
            {
                "artifacts": [
                    {
                        "primitive_request": {
                            "roots": {
                                "primary": {
                                    **old(self.repository),
                                    "path": str(self.repository),
                                },
                                "workspace": {
                                    **old(self.workspace),
                                    "path": str(self.workspace),
                                },
                            }
                        }
                    }
                ]
            },
        )
        review_inode = review.stat().st_ino
        compact_receipt = review_root / (
            ".delivery-update-receipt-" + "a" * 64 + ".json"
        )
        write_json(
            compact_receipt,
            {
                "device": review.stat().st_dev + 1,
                "inode": review_inode,
                "target": review.name,
            },
        )
        release = self.repository / "build" / "reviews" / ".demo.md.ledger.json.release.json"
        write_json(
            release,
            {
                "ledger": {
                    "name": review.name,
                    "device": review.stat().st_dev + 1,
                    "inode": review.stat().st_ino,
                }
            },
        )
        reclaim_complete = (
            self.repository
            / "build"
            / "reviews"
            / ".delivery-ledger-reclaim-complete.json"
        )
        write_json(
            reclaim_complete,
            {
                "preview": {
                    "archive": review.name,
                    "device": review.stat().st_dev + 1,
                    "inode": review_inode,
                }
            },
        )

        plan = migration.migrate_filesystem_records(self.repository, "dry-run")

        self.assertTrue(plan["requires_confirm_remount"])
        planned_paths = {item["path"] for item in plan["records"]}
        self.assertEqual(
            planned_paths,
            {
                str(topology_record),
                str(review),
                str(compact_receipt),
                str(release),
                str(reclaim_complete),
            },
        )
        self.assertNotIn("before_base64", plan["records"][0])

        result = migration.migrate_filesystem_records(
            self.repository, "apply", confirm_remount=True
        )

        self.assertEqual(result["status"], "complete")
        converted = json.loads(topology_record.read_text(encoding="utf-8"))
        self.assertEqual(
            converted["state_policy"]["identity"], portable_identity(state.stat())
        )
        ledger = json.loads(review.read_text(encoding="utf-8"))
        workspace_root = ledger["artifacts"][0]["primitive_request"]["roots"][
            "workspace"
        ]
        self.assertEqual(workspace_root["device"], portable_pair(self.workspace.stat())["device"])
        self.assertEqual(workspace_root["inode"], self.workspace.stat().st_ino)
        self.assertEqual(workspace_root["path"], str(self.workspace))
        release_value = json.loads(release.read_text(encoding="utf-8"))
        self.assertEqual(
            release_value["ledger"]["device"], portable_pair(review.stat())["device"]
        )
        self.assertEqual(release_value["ledger"]["inode"], review.stat().st_ino)
        compact_receipt_value = json.loads(compact_receipt.read_text(encoding="utf-8"))
        self.assertEqual(
            compact_receipt_value["device"],
            migration.portable_device_from_components(
                review_inode, stat.S_IFREG, "file"
            ),
        )
        self.assertEqual(compact_receipt_value["inode"], review_inode)
        reclaim_value = json.loads(reclaim_complete.read_text(encoding="utf-8"))
        self.assertEqual(
            reclaim_value["preview"]["device"],
            migration.portable_device_from_components(
                review_inode, stat.S_IFREG, "file"
            ),
        )
        self.assertEqual(reclaim_value["preview"]["inode"], review_inode)
        journal = json.loads(
            (self.workspace / migration.FILESYSTEM_MIGRATION_RECORD).read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(journal["state"], "complete")
        self.assertTrue(all(record["after_identity"] for record in journal["records"]))
        audit = migration.migrate_filesystem_records(self.repository, "audit")
        self.assertEqual({row["status"] for row in audit["records"]}, {"converted"})

    def test_changed_same_content_record_fails_audit(self) -> None:
        record, state = self.make_state_record("changed")
        migration.migrate_filesystem_records(
            self.repository, "apply", confirm_remount=True
        )
        replacement = record.with_name("replacement.json")
        replacement.write_bytes(record.read_bytes())
        os.replace(replacement, record)

        with self.assertRaisesRegex(migration.FilesystemMigrationError, "identity changed"):
            migration.migrate_filesystem_records(self.repository, "audit")
        self.assertEqual(
            json.loads(record.read_text(encoding="utf-8"))["state_policy"]["identity"],
            portable_identity(state.stat()),
        )

    def test_path_and_git_common_inode_changes_refuse_rebind(self) -> None:
        target = self.workspace / "target"
        target.mkdir()
        path_record = self.workspace / "path.json"
        write_json(
            path_record,
            {
                "path": str(target),
                "path_device": target.stat().st_dev + 1,
                "path_inode": target.stat().st_ino + 1,
            },
        )
        with self.assertRaisesRegex(
            migration.FilesystemMigrationError, "path identity inode changed"
        ):
            migration.migrate_filesystem_records(self.repository, "dry-run")

        path_record.unlink()
        common_record = self.workspace / "common.json"
        write_json(
            common_record,
            {
                "git_common": str(target),
                "git_common_device": target.stat().st_dev + 1,
                "git_common_inode": target.stat().st_ino + 1,
            },
        )
        with self.assertRaisesRegex(
            migration.FilesystemMigrationError, "git_common_pair inode changed"
        ):
            migration.migrate_filesystem_records(self.repository, "dry-run")

    def test_failed_apply_rolls_back_and_can_resume(self) -> None:
        first, _ = self.make_state_record("a")
        second, _ = self.make_state_record("b")
        before = {path: path.read_bytes() for path in (first, second)}
        original_write = migration._write_json_bytes
        failed = False

        def fail_once(path: Path, raw: bytes) -> None:
            nonlocal failed
            if Path(path).name == second.name and not failed:
                failed = True
                raise migration.FilesystemMigrationError("injected write failure")
            original_write(path, raw)

        with mock.patch.object(migration, "_write_json_bytes", side_effect=fail_once):
            with self.assertRaisesRegex(
                migration.FilesystemMigrationError, "injected write failure"
            ):
                migration.migrate_filesystem_records(
                    self.repository, "apply", confirm_remount=True
                )

        self.assertEqual(first.read_bytes(), before[first])
        self.assertEqual(second.read_bytes(), before[second])
        journal_path = self.workspace / migration.FILESYSTEM_MIGRATION_RECORD
        journal = json.loads(journal_path.read_text(encoding="utf-8"))
        self.assertEqual(journal["state"], "prepared")
        self.assertTrue(all(record["rollback_identity"] for record in journal["records"]))

        resumed = migration.migrate_filesystem_records(
            self.repository, "apply", confirm_remount=True
        )
        self.assertEqual(resumed["status"], "complete")

    def test_historical_pair_uses_type_evidence_without_guessing_a_live_target(self) -> None:
        document = self.workspace / "historical.json"
        value = {
            "archive": {
                "device": 17,
                "inode": 1234,
                "kind": "file",
                "raw_base64": "e30=",
            }
        }

        transformed = migration._transform_document(document, value)

        assert transformed is not None
        self.assertEqual(transformed["archive"]["inode"], 1234)
        self.assertNotEqual(transformed["archive"]["device"], 17)

    def test_historical_directory_path_is_treated_as_historical_evidence(self) -> None:
        value = {"snapshot": {"device": 17, "inode": 1234}}

        transformed = migration._transform_document(
            self.repository / "historical" / "record.json", value
        )

        assert transformed is not None
        self.assertEqual(transformed["snapshot"]["inode"], 1234)
        self.assertNotEqual(transformed["snapshot"]["device"], 17)

    def test_modes_namespace_records_and_empty_repository_are_fail_closed(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported filesystem migration mode"):
            migration.migrate_filesystem_records(self.repository, "convert")

        empty = migration.migrate_filesystem_records(
            self.repository, "apply", confirm_remount=True
        )
        self.assertEqual(empty["status"], "complete")
        self.assertEqual(empty["records"], [])

        journal = self.workspace / migration.FILESYSTEM_MIGRATION_RECORD
        namespace = self.repository / ".git" / "atrinik-resource-leases"
        namespace.mkdir()
        identity_record = namespace.parent / "atrinik-resource-leases.identity.json"
        write_json(
            identity_record,
            {"schema_version": 1, "device": namespace.stat().st_dev + 1, "inode": namespace.stat().st_ino},
        )

        plan = migration.migrate_filesystem_records(self.repository, "dry-run")
        self.assertEqual(plan["records"][0]["path"], str(identity_record))
        with self.assertRaisesRegex(FilesystemMigrationError, "explicit remount"):
            migration.migrate_filesystem_records(self.repository, "apply")
        applied = migration.migrate_filesystem_records(
            self.repository, "apply", confirm_remount=True
        )
        self.assertEqual(applied["status"], "complete")
        self.assertEqual(
            migration.migrate_filesystem_records(
                self.repository, "apply", confirm_remount=True
            )["status"],
            "complete",
        )

        journal.unlink()
        write_json(
            identity_record,
            {
                "schema_version": 2,
                "identity": portable_identity(namespace.stat()),
            },
        )
        self.assertEqual(
            migration.migrate_filesystem_records(self.repository, "dry-run")[
                "requires_confirm_remount"
            ],
            False,
        )

        write_json(identity_record, {"schema_version": 99})
        with self.assertRaisesRegex(FilesystemMigrationError, "unsupported schema"):
            migration.migrate_filesystem_records(self.repository, "dry-run")

        identity_record.unlink()
        identity_record.mkdir()
        with self.assertRaisesRegex(FilesystemMigrationError, "not a regular file"):
            migration.migrate_filesystem_records(self.repository, "dry-run")
        identity_record.rmdir()
        target = self.repository / "identity-target.json"
        target.write_text("{}\n", encoding="utf-8")
        identity_record.symlink_to(target)
        with self.assertRaisesRegex(FilesystemMigrationError, "not a regular file"):
            migration.migrate_filesystem_records(self.repository, "dry-run")

        identity_record.unlink()
        write_json(
            identity_record,
            {"schema_version": 1, "device": namespace.stat().st_dev, "inode": namespace.stat().st_ino + 1},
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "inode changed"):
            migration.migrate_filesystem_records(self.repository, "dry-run")

    def test_unfinished_journal_requires_confirmation_and_planning_is_read_only(self) -> None:
        record, _state = self.make_state_record("unfinished")
        original_write = migration._write_json_bytes
        with mock.patch.object(
            migration,
            "_write_json_bytes",
            side_effect=migration.FilesystemMigrationError("injected unfinished write"),
        ):
            with self.assertRaisesRegex(
                migration.FilesystemMigrationError, "unfinished write"
            ):
                migration.migrate_filesystem_records(
                    self.repository, "apply", confirm_remount=True
                )

        journal_path = self.workspace / migration.FILESYSTEM_MIGRATION_RECORD
        before_journal = journal_path.read_bytes()
        dry_run = migration.migrate_filesystem_records(self.repository, "dry-run")
        self.assertEqual(dry_run["status"], "dry-run")
        self.assertEqual(journal_path.read_bytes(), before_journal)
        with self.assertRaisesRegex(
            migration.FilesystemMigrationError, "confirm-remount"
        ):
            migration.migrate_filesystem_records(self.repository, "apply")

        with mock.patch.object(
            migration,
            "_write_json_bytes",
            side_effect=original_write,
        ):
            resumed = migration.migrate_filesystem_records(
                self.repository, "apply", confirm_remount=True
            )
        self.assertEqual(resumed["status"], "complete")
        self.assertTrue(record.exists())

    def test_record_discovery_and_transformations_cover_context_rules(self) -> None:
        target = self.workspace / "target"
        target.mkdir()
        pair = portable_pair(target.stat())
        document = self.workspace / "record.json"

        self.assertIsNone(migration._transform_document(document, {"unrelated": True}))
        with self.assertRaisesRegex(
            migration.FilesystemMigrationError, "root is not an object"
        ):
            migration._transform_document(
                document,
                [{"path": str(target), "device": target.stat().st_dev + 1, "inode": target.stat().st_ino}],
            )
        same_pair = migration._transform_document(
            document,
            {"identity": {"path": str(target), **pair}},
        )
        self.assertIsNone(same_pair)
        ledger = self.workspace / "demo.md.ledger.json"
        ledger.write_text("{}\n", encoding="utf-8")
        self.assertIsNone(
            migration._transform_document(
                ledger, {"ledger": portable_pair(ledger.stat())}
            )
        )

        changed_path = migration._transform_document(
            document,
            {
                "path": str(target),
                "path_device": target.stat().st_dev + 1,
                "path_inode": target.stat().st_ino,
            },
        )
        assert changed_path is not None
        self.assertEqual(changed_path["path_inode"], target.stat().st_ino)

        changed_named = migration._transform_document(
            document,
            {
                "git_common": str(target),
                "git_common_device": target.stat().st_dev + 1,
                "git_common_inode": target.stat().st_ino,
            },
        )
        assert changed_named is not None
        self.assertEqual(changed_named["git_common_inode"], target.stat().st_ino)

        with self.assertRaisesRegex(FilesystemMigrationError, "cannot locate"):
            migration._transform_document(
                document, {"identity": {"device": 1, "inode": 2}}
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "path identity inode changed"):
            migration._transform_document(
                document,
                {
                    "path": str(target),
                    "path_device": target.stat().st_dev,
                    "path_inode": target.stat().st_ino + 1,
                },
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "git_common_pair inode changed"):
            migration._transform_document(
                document,
                {
                    "git_common": str(target),
                    "git_common_device": target.stat().st_dev,
                    "git_common_inode": target.stat().st_ino + 1,
                },
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "no safe git_common"):
            migration._transform_document(
                document,
                {"git_common": "relative", "git_common_device": 1, "git_common_inode": 2},
            )

        historical = self.workspace / "historical" / "record"
        with self.assertRaisesRegex(FilesystemMigrationError, "type evidence"):
            migration._transform_document(
                historical,
                {"archive": {"device": 1, "inode": 2}},
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "full live identity"):
            migration._transform_document(
                historical,
                {"identity": {"device": 1, "inode": 2}},
            )
        self.assertIsNone(
            migration._transform_document(
                historical,
                {
                    "archive": {
                        "device": migration.portable_device_from_components(
                            2, stat.S_IFREG, "file"
                        ),
                        "inode": 2,
                        "kind": "file",
                    }
                },
            )
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "historical path identity"):
            migration._transform_document(
                historical,
                {"path": "/no/such/path", "path_device": 1, "path_inode": 2},
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "historical git_common"):
            migration._transform_document(
                historical,
                {
                    "git_common": "/no/such/path",
                    "git_common_device": 1,
                    "git_common_inode": 2,
                },
            )

        review_root = self.repository / "build" / "reviews"
        review_root.mkdir(parents=True)
        (review_root / "unrelated.json").write_text("{}\n", encoding="utf-8")
        (review_root / "skip.lock").write_text("{}\n", encoding="utf-8")
        (review_root / "skip.tmp.json").write_text("{}\n", encoding="utf-8")
        selected = list(
            migration._workspace_json_records(
                review_root, skip={review_root / "skip.lock"}
            )
        )
        self.assertEqual(selected, [])
        self.assertEqual(list(migration._workspace_json_records(self.workspace / "absent", skip=set())), [])
        bad_root = self.workspace / "bad-root"
        bad_root.write_text("not a directory", encoding="utf-8")
        with self.assertRaisesRegex(FilesystemMigrationError, "not a directory"):
            list(migration._workspace_json_records(bad_root, skip=set()))
        symlink = self.workspace / "symlink-root"
        symlink.symlink_to(self.workspace)
        with self.assertRaisesRegex(FilesystemMigrationError, "not a directory"):
            list(migration._workspace_json_records(symlink, skip=set()))
        candidate_root = self.workspace / "candidate-root"
        candidate_root.mkdir()
        candidate = candidate_root / "candidate.json"
        candidate.symlink_to(target)
        with self.assertRaisesRegex(FilesystemMigrationError, "is a symlink"):
            list(migration._workspace_json_records(candidate_root, skip=set()))
        candidate.unlink()
        candidate.mkdir()
        with self.assertRaisesRegex(FilesystemMigrationError, "not a regular file"):
            list(migration._workspace_json_records(candidate_root, skip=set()))

    def test_identity_target_and_historical_inference_cover_all_record_shapes(self) -> None:
        document = self.workspace / "record.json"
        state = self.workspace / "state"
        state.mkdir()
        generation = state / "generation"
        generation.mkdir()
        output = state / "tmp" / "runtime-assets" / "g1"
        output.mkdir(parents=True)
        reservation = self.workspace / "reservation"
        reservation.mkdir()
        reservation_lease = reservation / "reservation.lease"
        reservation_lease.write_text("lease\n", encoding="utf-8")
        target = self.workspace / "target"
        target.mkdir()

        target_path, _ = migration._identity_target(
            document,
            {"path": str(target)},
            "identity",
            (),
        )
        self.assertEqual(target_path, target)
        physical_path, _ = migration._identity_target(
            document,
            {"physical_path": str(target)},
            "identity",
            (),
        )
        self.assertEqual(physical_path, target)
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "directory",
                (("port_reservation", {"port_reservation": {"path": str(reservation_lease)}}),),
            ),
            (reservation, "full"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "lease",
                (("port_reservation", {"port_reservation": {"path": str(reservation_lease)}}),),
            ),
            (reservation_lease, "stable"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "lease",
                (("runtime", {"runtime": {"path": str(generation)}}),),
            ),
            (generation / "generation.lease", "full"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "lease",
                (("control", {"control": {}}),),
            ),
            (self.workspace / "process-tree.lease", "full"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "output_identity",
                (
                    ("state", {"state": str(state)}),
                    ("generation", {"generation": "g1"}),
                ),
            ),
            (output, "full"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "mutable_state_output_identities",
                ((0, {"mutable_state_outputs": [str(output)]}),),
            ),
            (output, "full"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "lease_identity",
                (("state_policy", {"state_policy": {"path": str(state)}}),),
            ),
            (Path(f"{state}.lock"), "stable"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "lease_identity",
                (("owner", {"path": str(state)}),),
            ),
            (Path(f"{state}.lock"), "stable"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "identity",
                (("owner", {"physical_path": str(target)}),),
            ),
            (target, "full"),
        )
        self.assertEqual(
            migration._identity_target(
                document,
                {},
                "mutable_state_output_identities",
                (),
            ),
            (None, "full"),
        )
        ledger = self.workspace / "record.md.ledger.json"
        self.assertEqual(
            migration._identity_target(ledger, {}, "source", ()),
            (ledger, "pair"),
        )
        self.assertEqual(
            migration._identity_target(ledger, {}, None, (("device", {}),)),
            (ledger, "pair"),
        )
        self.assertEqual(
            migration._projection_for("identity", ()),
            "full",
        )
        self.assertEqual(
            migration._projection_for("mutable_state_output_identities", ()),
            "full",
        )
        self.assertEqual(
            migration._projection_for(
                "directory",
                (("port_reservation", {"port_reservation": {}}),),
            ),
            "full",
        )
        self.assertEqual(
            migration._projection_for(
                "lease", (("control", {"control": {}}),)
            ),
            "full",
        )

        self.assertEqual(
            migration._historical_pair({"inode": 2}, "lease", (), document)["inode"],
            2,
        )
        self.assertEqual(
            migration._historical_pair(
                {"inode": 2, "file_type": "directory"}, None, (), document
            )["device"],
            migration.portable_device_from_components(2, stat.S_IFDIR, "directory"),
        )
        self.assertEqual(
            migration._historical_pair(
                {"inode": 2}, None, (("roots", {}),), document
            )["inode"],
            2,
        )
        self.assertEqual(
            migration._historical_pair(
                {"inode": 2}, None, (("control", {}),), document
            )["inode"],
            2,
        )
        self.assertEqual(
            migration._historical_pair(
                {"inode": 2}, None, (), self.workspace / "record.lock"
            )["inode"],
            2,
        )
        self.assertIsNone(migration._historical_pair({"inode": -1}, None, (), document))
        self.assertIsNone(
            migration._historical_pair(
                {"inode": 2}, None, (), self.workspace / "record"
            )
        )
        self.assertTrue(migration._is_historical_pair(self.workspace / ".archive-record", {}, ()))
        self.assertTrue(
            migration._is_historical_pair(document, {}, (("archive", {}),))
        )
        self.assertTrue(
            migration._is_historical_pair(document, {"raw_base64": "e30="}, ())
        )
        self.assertFalse(migration._is_historical_pair(document, {}, ()))

        self.assertEqual(
            migration._nearby_named_target(
                document, {"name": str(target)}, ()
            ),
            target,
        )
        self.assertEqual(
            migration._nearby_named_target(document, {"name": target.name}, ()),
            target,
        )
        self.assertIsNone(
            migration._nearby_named_target(
                document,
                {"name": "../outside", "target": "bad\x00name"},
                (),
            )
        )

    def test_journal_validation_preserves_snapshots_and_rejects_tampering(self) -> None:
        target = self.workspace / "journal-target.json"
        before_value = {"before": True}
        after_value = {"after": True}
        write_json(target, before_value)
        record = migration._plan_record(target, before_value, after_value)
        journal_path = self.workspace / "manual-journal.json"
        journal = migration._new_journal([record], confirm_remount=True)
        write_json(journal_path, journal)
        normalized = migration._read_journal(journal_path)
        self.assertEqual(normalized["records"][0]["path"], str(target))
        self.assertEqual(
            migration._public_record(normalized["records"][0])["path"],
            str(target),
        )

        invalid_top_level = (
            [],
            {**journal, "extra": True},
            {**journal, "schema_version": 2},
            {**journal, "transaction": "other"},
            {**journal, "state": "unknown"},
            {**journal, "created_at": None},
            {**journal, "confirm_remount": "yes"},
            {**journal, "records": {}},
        )
        for value in invalid_top_level:
            with self.subTest(value=value), self.assertRaisesRegex(
                FilesystemMigrationError, "journal is invalid"
            ):
                write_json(journal_path, value)
                migration._read_journal(journal_path)

        for created_at, message in (
            ("not-a-time", "timestamp is invalid"),
            ("2026-01-01T00:00:00", "no timezone"),
        ):
            value = {**journal, "created_at": created_at}
            with self.subTest(created_at=created_at), self.assertRaisesRegex(
                FilesystemMigrationError, message
            ):
                write_json(journal_path, value)
                migration._read_journal(journal_path)
        for value, message in (
            ({**journal, "completed_at": 1}, "journal is invalid"),
            ({**journal, "completed_at": "2026-01-01T00:00:00Z"}, "unfinished state"),
            ({**journal, "rollback_error": 1}, "journal is invalid"),
        ):
            with self.subTest(value=value), self.assertRaisesRegex(
                FilesystemMigrationError, message
            ):
                write_json(journal_path, value)
                migration._read_journal(journal_path)

        invalid_records = []
        invalid_records.append({"not": "a record"})
        self.assertEqual(record["path"], str(target))
        self.assertTrue(record["before_base64"])
        for bad_record, message in (
            (invalid_records[0], "record is invalid"),
            ({**record, "path": str(journal_path)}, "own control file"),
            (
                {**record, "path": str(self.workspace / migration.FILESYSTEM_MIGRATION_LOCK)},
                "own control file",
            ),
            ({**record, "before_sha256": "b" * 64}, "before snapshot is invalid"),
            ({**record, "after_sha256": "b" * 64}, "after snapshot is invalid"),
        ):
            with self.subTest(message=message), self.assertRaisesRegex(
                FilesystemMigrationError, message
            ):
                write_json(journal_path, {**journal, "records": [bad_record]})
                migration._read_journal(journal_path)

        link_parent = self.workspace / "journal-link"
        link_parent.symlink_to(self.workspace)
        with self.assertRaisesRegex(FilesystemMigrationError, "not canonical"):
            write_json(
                journal_path,
                {**journal, "records": [{**record, "path": str(link_parent / target.name)}]},
            )
            migration._read_journal(journal_path)

        missing = self.workspace / "missing-target.json"
        with self.assertRaisesRegex(FilesystemMigrationError, "cannot inspect"):
            write_json(
                journal_path,
                {**journal, "records": [{**record, "path": str(missing)}]},
            )
            migration._read_journal(journal_path)

        with self.assertRaisesRegex(FilesystemMigrationError, "identity"):
            write_json(
                journal_path,
                {
                    **journal,
                    "records": [{**record, "before_identity": {}}],
                },
            )
            migration._read_journal(journal_path)
        directory = self.workspace / "journal-directory"
        directory.mkdir()
        with self.assertRaisesRegex(FilesystemMigrationError, "not a file"):
            migration._validate_journal_identity(
                portable_identity(directory.stat()), target, "before"
            )

        legacy_record = {
            key: record[key] for key in migration._LEGACY_JOURNAL_RECORD_KEYS
        }
        write_json(journal_path, {**journal, "records": [legacy_record]})
        legacy_normalized = migration._read_journal(journal_path)
        self.assertTrue(legacy_normalized["records"][0]["before_identity"])

        second = self.workspace / "second-target.json"
        write_json(second, before_value)
        second_record = migration._plan_record(second, before_value, {"other": True})
        for records in (
            [record, record],
            [second_record, record],
        ):
            with self.subTest(records=records), self.assertRaisesRegex(
                FilesystemMigrationError, "unique and ordered"
            ):
                write_json(journal_path, {**journal, "records": records})
                migration._read_journal(journal_path)

    def test_low_level_snapshot_decoding_and_filesystem_guards_fail_closed(self) -> None:
        target = self.workspace / "snapshot.json"
        write_json(target, {"value": 1})
        raw, identity = migration._read_snapshot(target)
        self.assertEqual(json.loads(raw), {"value": 1})
        self.assertEqual(identity["kind"], "file")
        migration._write_json_bytes(target, raw)

        with self.assertRaisesRegex(FilesystemMigrationError, "canonical JSON"):
            migration._write_json_bytes(target, b'{ "value": 1 }\n')
        with self.assertRaisesRegex(FilesystemMigrationError, "not JSON"):
            migration._decode_json(b"\xff", target)
        with self.assertRaisesRegex(FilesystemMigrationError, "not JSON"):
            migration._decode_json(b'{"value": 1, "value": 2}', target)
        with self.assertRaisesRegex(FilesystemMigrationError, "encoding is invalid"):
            migration._decode_snapshot(None, target)
        with self.assertRaisesRegex(FilesystemMigrationError, "not valid base64"):
            migration._decode_snapshot("!", target)
        noncanonical = base64.b64encode(b'{ "value": 1 }\n').decode("ascii")
        with self.assertRaisesRegex(FilesystemMigrationError, "canonical JSON"):
            migration._decode_snapshot(noncanonical, target)
        with mock.patch.object(migration, "MAX_MIGRATION_SNAPSHOT_BYTES", 1):
            with self.assertRaisesRegex(FilesystemMigrationError, "too large"):
                migration._decode_snapshot(base64.b64encode(raw).decode("ascii"), target)

        directory = self.workspace / "snapshot-directory"
        directory.mkdir()
        with self.assertRaisesRegex(FilesystemMigrationError, "not an owned regular file"):
            migration._read_snapshot(directory)
        with mock.patch.object(migration, "MAX_MIGRATION_SNAPSHOT_BYTES", 0):
            with self.assertRaisesRegex(FilesystemMigrationError, "too large"):
                migration._read_snapshot(target)
        with mock.patch.object(migration.os, "open", side_effect=OSError("open failed")):
            with self.assertRaisesRegex(FilesystemMigrationError, "cannot read"):
                migration._read_snapshot(target)
        with mock.patch.object(migration.os, "read", return_value=b""):
            with self.assertRaisesRegex(FilesystemMigrationError, "changed while reading"):
                migration._read_snapshot(target)
        original_limit = migration.MAX_MIGRATION_SNAPSHOT_BYTES
        real_read = os.read

        def read_then_shrink(fd: int, size: int) -> bytes:
            value = real_read(fd, size)
            migration.MAX_MIGRATION_SNAPSHOT_BYTES = 0
            return value

        try:
            with mock.patch.object(migration.os, "read", side_effect=read_then_shrink):
                with self.assertRaisesRegex(FilesystemMigrationError, "too large"):
                    migration._read_snapshot(target)
        finally:
            migration.MAX_MIGRATION_SNAPSHOT_BYTES = original_limit
        visible = SimpleNamespace(
            st_mode=stat.S_IFREG,
            st_dev=target.stat().st_dev + 1,
            st_ino=target.stat().st_ino,
        )
        with mock.patch.object(Path, "stat", return_value=visible):
            with self.assertRaisesRegex(FilesystemMigrationError, "was replaced"):
                migration._read_snapshot(target)

        with self.assertRaisesRegex(FilesystemMigrationError, "unsafe"):
            migration._assert_safe_path(Path("relative"))
        with self.assertRaisesRegex(FilesystemMigrationError, "unsafe"):
            migration._assert_safe_path(Path("/"))
        parent_link = self.workspace / "parent-link"
        parent_link.symlink_to(self.workspace)
        with self.assertRaisesRegex(FilesystemMigrationError, "symlinked parent"):
            migration._assert_safe_path(parent_link / target.name)

        with self.assertRaisesRegex(FilesystemMigrationError, "cannot inspect"):
            migration._metadata(self.workspace / "does-not-exist", target)
        fifo = self.workspace / "snapshot.fifo"
        os.mkfifo(fifo)
        with self.assertRaisesRegex(FilesystemMigrationError, "not a file/directory"):
            migration._metadata(fifo, target)
        with mock.patch.object(migration.os, "geteuid", return_value=os.geteuid() + 1):
            with self.assertRaisesRegex(FilesystemMigrationError, "foreign ownership"):
                migration._metadata(target, target)
        with self.assertRaises(FilesystemMigrationError):
            migration._validate_portable_record_identity({}, target)

        self.assertEqual(migration._physical_lease_namespace(self.repository), self.repository / ".git" / "atrinik-resource-leases")
        fallback = self.repository / "fallback"
        fallback.mkdir()
        (fallback / "workspace").mkdir()
        self.assertEqual(
            migration._physical_lease_namespace(fallback),
            fallback / "workspace" / "atrinik-resource-leases",
        )
        marker = fallback / ".git"
        marker.write_text("not-a-gitdir\n", encoding="utf-8")
        self.assertEqual(
            migration._physical_lease_namespace(fallback),
            fallback / "workspace" / "atrinik-resource-leases",
        )
        linked = self.repository / "linked"
        linked.mkdir()
        gitdir = self.repository / "common" / "worktrees" / "linked"
        gitdir.mkdir(parents=True)
        (linked / ".git").write_text("gitdir: ../common/worktrees/linked\n", encoding="utf-8")
        self.assertEqual(
            migration._physical_lease_namespace(linked),
            self.repository / "common" / "atrinik-resource-leases",
        )
        with mock.patch.object(Path, "read_text", side_effect=OSError("marker read")):
            with self.assertRaisesRegex(FilesystemMigrationError, "cannot read Git"):
                migration._physical_lease_namespace(linked)

    def test_journal_apply_resume_audit_and_rollback_paths_are_durable(self) -> None:
        def make_record(
            name: str, before: object, after: object
        ) -> tuple[Path, dict[str, object]]:
            target = self.workspace / name
            write_json(target, before)
            return target, migration._plan_record(target, before, after)

        target, record = make_record(
            "already-after.json", {"before": True}, {"after": True}
        )
        write_json(target, {"after": True})
        record["after_identity"] = migration.portable_identity(target.stat())
        journal_path = self.workspace / "already-after-journal.json"
        result = migration._apply_journal(
            journal_path,
            migration._new_journal([record], confirm_remount=True),
        )
        self.assertEqual(result["status"], "complete")

        resume = migration._resume_journal(
            journal_path,
            {
                **migration._new_journal([record], confirm_remount=True),
                "state": "prepared",
            },
        )
        self.assertEqual(resume["status"], "complete")

        changed_target, changed_record = make_record(
            "changed-before.json", {"before": True}, {"after": True}
        )
        write_json(changed_target, {"unexpected": True})
        with self.assertRaisesRegex(FilesystemMigrationError, "input changed"):
            migration._apply_journal(
                self.workspace / "changed-before-journal.json",
                migration._new_journal([changed_record], confirm_remount=True),
            )

        no_op_target, no_op_record = make_record(
            "no-longer-applies.json", {"unrelated": True}, {"after": True}
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "no longer applies"):
            migration._apply_journal(
                self.workspace / "no-longer-applies-journal.json",
                migration._new_journal([no_op_record], confirm_remount=True),
            )
        oversize_state = self.workspace / "state-for-oversize"
        oversize_state.mkdir()
        oversized_target, oversized_record = make_record(
            "oversized-after.json",
            {
                "identity": {
                    "path": str(oversize_state),
                    "device": oversize_state.stat().st_dev + 1,
                    "inode": oversize_state.stat().st_ino,
                }
            },
            {"after": True},
        )
        with mock.patch.object(
            migration,
            "MAX_MIGRATION_SNAPSHOT_BYTES",
            len(oversized_target.read_bytes()),
        ):
            # The record must still have a live target so transformation can
            # reach the post-transform size guard.
            with self.assertRaisesRegex(FilesystemMigrationError, "too large"):
                migration._apply_journal(
                    self.workspace / "oversized-after-journal.json",
                    migration._new_journal([oversized_record], confirm_remount=True),
                )
            self.assertTrue(oversized_target.exists())

        state = self.workspace / "state-for-rewrite"
        state.mkdir()
        rewrite_target, rewrite_record = make_record(
            "rewrite-plan.json",
            {
                "identity": {
                    "path": str(state),
                    "device": state.stat().st_dev + 1,
                    "inode": state.stat().st_ino,
                }
            },
            {"placeholder": True},
        )
        rewritten = migration._apply_journal(
            self.workspace / "rewrite-plan-journal.json",
            migration._new_journal([rewrite_record], confirm_remount=True),
        )
        self.assertEqual(rewritten["status"], "complete")
        self.assertEqual(
            json.loads(rewrite_target.read_text(encoding="utf-8"))["identity"][
                "inode"
            ],
            state.stat().st_ino,
        )

        verify_target, verify_record = make_record(
            "verify-output.json",
            {
                "identity": {
                    "path": str(state),
                    "device": state.stat().st_dev + 1,
                    "inode": state.stat().st_ino,
                }
            },
            {"placeholder": True},
        )
        def write_wrong_output(path: Path, raw: bytes) -> None:
            del raw
            path.write_bytes(b'{"wrong": true}\n')

        with mock.patch.object(
            migration, "_write_json_bytes", side_effect=write_wrong_output
        ), mock.patch.object(migration, "_rollback_record", return_value=None):
            with self.assertRaisesRegex(FilesystemMigrationError, "output could not be verified"):
                migration._apply_journal(
                    self.workspace / "verify-output-journal.json",
                    migration._new_journal([verify_record], confirm_remount=True),
                )
        self.assertTrue(verify_target.exists())

        first_target, first_record = make_record(
            "rollback-failure-first.json",
            {
                "identity": {
                    "path": str(state),
                    "device": state.stat().st_dev + 1,
                    "inode": state.stat().st_ino,
                }
            },
            {"after": 1},
        )
        second_target, second_record = make_record(
            "rollback-failure-second.json",
            {
                "identity": {
                    "path": str(state),
                    "device": state.stat().st_dev + 1,
                    "inode": state.stat().st_ino,
                }
            },
            {"after": 2},
        )
        real_write = migration._write_json_bytes

        def fail_second(path: Path, raw: bytes) -> None:
            if Path(path) == second_target:
                raise FilesystemMigrationError("second write failed")
            real_write(path, raw)

        with mock.patch.object(migration, "_write_json_bytes", side_effect=fail_second), mock.patch.object(
            migration, "_rollback_record", side_effect=FilesystemMigrationError("rollback failed")
        ):
            with self.assertRaisesRegex(FilesystemMigrationError, "rollback is uncertain"):
                migration._apply_journal(
                    self.workspace / "rollback-failure-journal.json",
                    migration._new_journal(
                        [first_record, second_record], confirm_remount=True
                    ),
                )
        self.assertTrue(first_target.exists())

        persist_target, persist_record = make_record(
            "journal-persist-failure.json",
            {
                "identity": {
                    "path": str(state),
                    "device": state.stat().st_dev + 1,
                    "inode": state.stat().st_ino,
                }
            },
            {"after": True},
        )
        real_durable = migration.durable_atomic_json

        def fail_failed_journal(path: Path, value: object) -> None:
            if isinstance(value, dict) and "rollback_error" in value:
                raise OSError("journal storage failed")
            real_durable(path, value)

        with mock.patch.object(
            migration,
            "_write_json_bytes",
            side_effect=FilesystemMigrationError("write failed"),
        ), mock.patch.object(
            migration, "durable_atomic_json", side_effect=fail_failed_journal
        ):
            with self.assertRaisesRegex(FilesystemMigrationError, "could not be persisted"):
                migration._apply_journal(
                    self.workspace / "journal-persist-failure-control.json",
                    migration._new_journal([persist_record], confirm_remount=True),
                )
        self.assertTrue(persist_target.exists())

        ambiguous_target, ambiguous_record = make_record(
            "ambiguous.json", {"before": True}, {"after": True}
        )
        write_json(ambiguous_target, {"ambiguous": True})
        with self.assertRaisesRegex(FilesystemMigrationError, "ambiguous"):
            migration._resume_journal(
                self.workspace / "ambiguous-journal.json",
                migration._new_journal([ambiguous_record], confirm_remount=True),
            )
        audit_target, audit_record = make_record(
            "legacy-audit.json", {"before": True}, {"after": True}
        )
        audit = migration._audit_journal(
            self.workspace / "audit-journal.json",
            {
                **migration._new_journal([audit_record], confirm_remount=True),
                "state": "prepared",
            },
        )
        self.assertEqual(audit["records"][0]["status"], "legacy")
        write_json(audit_target, {"changed": True})
        with self.assertRaisesRegex(FilesystemMigrationError, "record changed"):
            migration._audit_journal(
                self.workspace / "audit-journal.json",
                {
                    **migration._new_journal([audit_record], confirm_remount=True),
                    "state": "prepared",
                },
            )

        rollback_target, rollback_record = make_record(
            "rollback-changed.json", {"before": True}, {"after": True}
        )
        write_json(rollback_target, {"unexpected": True})
        with self.assertRaisesRegex(FilesystemMigrationError, "rollback target changed"):
            migration._rollback_record(rollback_record)

        verify_rollback_target, verify_rollback_record = make_record(
            "rollback-unverified.json", {"before": True}, {"after": True}
        )
        write_json(verify_rollback_target, {"after": True})
        verify_rollback_record["after_identity"] = migration.portable_identity(
            verify_rollback_target.stat()
        )

        def write_wrong_rollback(path: Path, raw: bytes) -> None:
            del raw
            path.write_bytes(b'{"not-before": true}\n')

        with mock.patch.object(
            migration, "_write_json_bytes", side_effect=write_wrong_rollback
        ):
            with self.assertRaisesRegex(
                FilesystemMigrationError, "rollback could not be verified"
            ):
                migration._rollback_record(verify_rollback_record)

    def test_remaining_rebind_and_planning_guards_are_exercised(self) -> None:
        target = self.workspace / "target"
        target.mkdir()
        document = self.workspace / "record.json"
        missing = self.workspace / "missing"

        with self.assertRaisesRegex(FilesystemMigrationError, "cannot locate"):
            migration._transform_document(
                document,
                {"identity": {"path": str(missing), "device": 1, "inode": 2}},
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "safe path identity"):
            migration._transform_document(
                document,
                {"path": str(missing), "path_device": 1, "path_inode": 2},
            )
        self.assertIsNone(
            migration._transform_document(
                document,
                {
                    "path": str(target),
                    "path_device": portable_pair(target.stat())["device"],
                    "path_inode": target.stat().st_ino,
                },
            )
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "inode changed"):
            migration._transform_document(
                document,
                {
                    "identity": {
                        "path": str(target),
                        "device": target.stat().st_dev,
                        "inode": target.stat().st_ino + 1,
                    }
                },
            )
        historical = self.workspace / "historical" / "record"
        same_historical_pair = {
            "device": migration.portable_device_from_components(
                2, stat.S_IFREG, "file"
            ),
            "inode": 2,
        }
        self.assertIsNone(
            migration._transform_document(
                historical, {"source": same_historical_pair}
            )
        )
        changed_historical_path = migration._transform_document(
            historical,
            {
                "path": str(missing),
                "path_device": 1,
                "path_inode": 2,
                "inode": 2,
                "kind": "directory",
            },
        )
        assert changed_historical_path is not None
        self.assertNotEqual(changed_historical_path["path_device"], 1)
        self.assertIsNone(
            migration._transform_document(
                historical,
                {
                    "path": str(missing),
                    "path_device": migration.portable_device_from_components(
                        2, stat.S_IFDIR, "directory"
                    ),
                    "path_inode": 2,
                    "inode": 2,
                    "kind": "directory",
                },
            )
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "inode changed"):
            migration._transform_document(
                document,
                {
                    "identity": {
                        "path": str(target),
                        "device": target.stat().st_dev,
                        "inode": target.stat().st_ino + 1,
                    }
                },
            )
        with self.assertRaisesRegex(FilesystemMigrationError, "inode changed"):
            migration._transform_document(
                document,
                {
                    "git_common": str(target),
                    "git_common_device": target.stat().st_dev,
                    "git_common_inode": target.stat().st_ino + 1,
                },
            )
        self.assertIsNone(
            migration._transform_document(
                document,
                {
                    "git_common": str(target),
                    "git_common_device": portable_pair(target.stat())["device"],
                    "git_common_inode": target.stat().st_ino,
                },
            )
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "target is missing"):
            migration._transform_document(
                document,
                {
                    "git_common": str(missing),
                    "git_common_device": 1,
                    "git_common_inode": 2,
                },
            )
        self.assertIsNotNone(
            migration._transform_document(
                self.workspace / "historical" / "record.json",
                {
                    "git_common": str(missing),
                    "git_common_device": 1,
                    "git_common_inode": 2,
                    "kind": "file",
                },
            )
        )

        ancestor_path = migration._identity_target(
            document, {}, "identity", (("bad", "not-a-dict"),)
        )
        self.assertEqual(ancestor_path, (None, "full"))
        self.assertEqual(
            migration._projection_for(
                "lease",
                (("port_reservation", {"port_reservation": {}}),),
            ),
            "full",
        )

        identity_record = self.repository / ".git" / "atrinik-resource-leases.identity.json"
        namespace = self.repository / ".git" / "atrinik-resource-leases"
        namespace.mkdir()
        write_json(
            identity_record,
            {
                "schema_version": 1,
                "device": namespace.stat().st_dev,
                "inode": namespace.stat().st_ino + 1,
            },
        )
        with self.assertRaisesRegex(FilesystemMigrationError, "inode changed"):
            migration._transform_document(
                identity_record, json.loads(identity_record.read_text(encoding="utf-8"))
            )
        identity_record.unlink()
        namespace.rmdir()

        plan_path = self.workspace / "plan.json"
        write_json(plan_path, {"unchanged": True})
        discovered = migration.Paths.discover(self.repository)
        with mock.patch.object(
            migration, "_workspace_json_records", return_value=[plan_path]
        ):
            self.assertEqual(migration._plan(discovered, confirm_remount=True), [])
        with mock.patch.object(
            migration, "_workspace_json_records", return_value=[plan_path, plan_path]
        ):
            self.assertEqual(migration._plan(discovered, confirm_remount=True), [])
        with mock.patch.object(
            Path, "rglob", side_effect=OSError("enumeration failed")
        ):
            with self.assertRaisesRegex(FilesystemMigrationError, "cannot enumerate"):
                list(migration._workspace_json_records(self.workspace, skip=set()))
        skip_file = self.workspace / "skip.json"
        skip_file.write_text("{}\n", encoding="utf-8")
        self.assertEqual(
            list(
                migration._workspace_json_records(
                    self.workspace, skip={skip_file}
                )
            ),
            [plan_path],
        )
        ignored = self.workspace / "ignored.tmp.json"
        ignored.write_text("{}\n", encoding="utf-8")
        self.assertNotIn(
            ignored,
            migration._workspace_json_records(self.workspace, skip=set()),
        )

        with self.assertRaisesRegex(FilesystemMigrationError, "too large"):
            with mock.patch.object(
                migration,
                "_read_snapshot",
                return_value=(b"{}\n", {"kind": "file"}),
            ), mock.patch.object(migration, "MAX_MIGRATION_SNAPSHOT_BYTES", 1):
                migration._plan_record(
                    plan_path, {"unchanged": True}, {"changed": True}
                )
        with self.assertRaisesRegex(FilesystemMigrationError, "no change"):
            migration._plan_record(
                plan_path, {"unchanged": True}, {"unchanged": True}
            )

        journal_path = self.workspace / migration.FILESYSTEM_MIGRATION_RECORD
        journal_path.write_text("{}\n", encoding="utf-8")
        with mock.patch.object(
            migration,
            "_read_journal",
            return_value={"state": "unsupported", "records": []},
        ):
            with self.assertRaisesRegex(FilesystemMigrationError, "unsupported state"):
                migration.migrate_filesystem_records(
                    self.repository, "apply", confirm_remount=True
                )
