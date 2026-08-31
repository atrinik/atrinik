from __future__ import annotations

import json
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock

import atrinik_workspace.filesystem_migration as migration
from atrinik_workspace.filesystem_identity import portable_identity, portable_pair


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

        review = self.repository / "build" / "reviews" / "demo.md.ledger.json"
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
            {str(topology_record), str(review), str(release), str(reclaim_complete)},
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
