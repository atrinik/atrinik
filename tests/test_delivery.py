# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
from __future__ import annotations

import json
from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cleanup import Cleanup
from atrinik_workspace.delivery import (
    ActiveDeliveryEvidence,
    _ledger_lock_name,
    inventory_active_delivery_evidence,
)
from atrinik_workspace.migration import RepositoryMigration
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.workspace import Workspace


class DeliveryEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.review_root = self.root / "build" / "reviews"
        self.review_root.mkdir(parents=True)
        self.helper = self.root / "scripts/delivery_ledger.py"
        self.helper.parent.mkdir(parents=True)
        self.helper.write_text("trusted helper\n", encoding="utf-8")
        self.name = "atrinik-atrinik-issue-471.md.ledger.json"
        for filename in (
            self.name,
            self.name.removesuffix(".ledger.json"),
            f".{self.name}.lock",
        ):
            (self.review_root / filename).write_text("evidence\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def inventory_output(self, worktree: Path) -> str:
        document = {
            "artifacts": [
                {"kind": "worktree", "current": {"path": str(worktree)}}
            ]
        }
        return json.dumps(
            {
                "schema_version": 1,
                "ledgers": [{"name": self.name, "document": document}],
                "pending": [],
                "legacy_reports": [],
                "releases": [],
                "archives": [],
                "reclaims": [],
                "historical_ledgers": [],
            }
        )

    def test_projects_active_evidence_and_worktree(self) -> None:
        worktree = self.root / "workspace" / "worktrees" / "active"
        completed = SimpleNamespace(
            returncode=0, stdout=self.inventory_output(worktree), stderr=""
        )
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run", return_value=completed
        ) as invoke:
            evidence = inventory_active_delivery_evidence(self.root)

        self.assertEqual(evidence.ledgers, (self.name,))
        self.assertEqual(evidence.references[worktree.resolve()], (self.name,))
        self.assertIn(self.review_root.resolve(), evidence.references)
        self.assertEqual(invoke.call_args.args[0][2], str(self.helper))
        self.assertEqual(invoke.call_args.args[0][3], "inventory")
        self.assertEqual(invoke.call_args.kwargs["timeout"], 30)

    def test_selected_checkout_layout_and_current_precedence(self) -> None:
        legacy = self.root / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
        legacy.parent.mkdir(parents=True)
        legacy.write_text("historical helper\n")
        completed = SimpleNamespace(returncode=0, stdout=self.inventory_output(self.root), stderr="")
        with mock.patch("atrinik_workspace.delivery.subprocess.run", return_value=completed) as invoke:
            inventory_active_delivery_evidence(self.root)
            self.assertEqual(invoke.call_args.args[0][2], str(self.helper))
            self.helper.unlink()
            inventory_active_delivery_evidence(self.root)
            self.assertEqual(invoke.call_args.args[0][2], str(legacy))
            self.assertEqual(invoke.call_args.kwargs["cwd"], self.root)
        legacy.unlink()
        with mock.patch("atrinik_workspace.delivery.subprocess.run") as invoke:
            with self.assertRaisesRegex(WorkspaceError, "helper is missing"):
                inventory_active_delivery_evidence(self.root)
            invoke.assert_not_called()

    def test_unsafe_current_helper_never_falls_back_to_valid_legacy(self) -> None:
        legacy = self.root / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
        legacy.parent.mkdir(parents=True)
        legacy.write_text("historical helper\n")
        self.helper.unlink()
        for target in (legacy, self.root / "missing-helper"):
            with self.subTest(target=target):
                self.helper.symlink_to(target)
                with mock.patch("atrinik_workspace.delivery.subprocess.run") as invoke:
                    with self.assertRaisesRegex(WorkspaceError, "not a regular file"):
                        inventory_active_delivery_evidence(self.root)
                    invoke.assert_not_called()
                self.helper.unlink()
        self.helper.mkdir()
        with mock.patch("atrinik_workspace.delivery.subprocess.run") as invoke:
            with self.assertRaisesRegex(WorkspaceError, "not a regular file"):
                inventory_active_delivery_evidence(self.root)
            invoke.assert_not_called()
        self.helper.rmdir()
        self.helper.write_text("trusted helper\n")
        original = Path.lstat
        def foreign_owner(path, *args, **kwargs):
            value = original(path, *args, **kwargs)
            if path == self.helper:
                return os.stat_result((value.st_mode, value.st_ino, value.st_dev, value.st_nlink,
                                       os.geteuid() + 1, value.st_gid, value.st_size,
                                       value.st_atime, value.st_mtime, value.st_ctime))
            return value
        with mock.patch.object(Path, "lstat", foreign_owner), mock.patch(
            "atrinik_workspace.delivery.subprocess.run"
        ) as invoke:
            with self.assertRaisesRegex(WorkspaceError, "not owned"):
                inventory_active_delivery_evidence(self.root)
            invoke.assert_not_called()

    def test_unsafe_helper_ancestor_or_legacy_helper_fails_closed(self) -> None:
        legacy = self.root / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
        legacy.parent.mkdir(parents=True)
        legacy.write_text("historical helper\n")
        self.helper.unlink()
        self.helper.parent.rmdir()
        self.helper.parent.symlink_to(legacy.parent, target_is_directory=True)
        with mock.patch("atrinik_workspace.delivery.subprocess.run") as invoke:
            with self.assertRaisesRegex(WorkspaceError, "ancestor is not a regular directory"):
                inventory_active_delivery_evidence(self.root)
            invoke.assert_not_called()
        self.helper.parent.unlink()
        legacy.unlink()
        legacy.symlink_to(self.root / "missing-helper")
        with mock.patch("atrinik_workspace.delivery.subprocess.run") as invoke:
            with self.assertRaisesRegex(WorkspaceError, "not a regular file"):
                inventory_active_delivery_evidence(self.root)
            invoke.assert_not_called()

    def test_helper_inspection_error_never_falls_back(self) -> None:
        legacy = self.root / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
        legacy.parent.mkdir(parents=True)
        legacy.write_text("historical helper\n")
        original = Path.lstat
        def denied(path, *args, **kwargs):
            if path == self.helper:
                raise PermissionError("fixture denial")
            return original(path, *args, **kwargs)
        with mock.patch.object(Path, "lstat", denied), mock.patch(
            "atrinik_workspace.delivery.subprocess.run"
        ) as invoke:
            with self.assertRaisesRegex(WorkspaceError, "cannot inspect delivery-ledger helper"):
                inventory_active_delivery_evidence(self.root)
            invoke.assert_not_called()

    def test_failed_selected_helper_does_not_retry_legacy(self) -> None:
        legacy = self.root / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
        legacy.parent.mkdir(parents=True)
        legacy.write_text("historical helper\n")
        with mock.patch("atrinik_workspace.delivery.subprocess.run",
            return_value=SimpleNamespace(returncode=2, stdout="", stderr="invalid evidence")) as invoke:
            with self.assertRaisesRegex(WorkspaceError, "invalid evidence"):
                inventory_active_delivery_evidence(self.root)
            self.assertEqual(invoke.call_count, 1)
            self.assertEqual(invoke.call_args.args[0][2], str(self.helper))

    def test_projects_original_and_residual_reservations_as_typed_evidence(self):
        worktree = self.root / "workspace/worktrees/active"
        original = self.root / "workspace/build/profiles/classic-111111111111"
        residual = original.with_name("classic-222222222222")
        generation = self.root / "workspace/build/source-generations/classic/generation"
        output = json.loads(self.inventory_output(worktree))
        output["ledgers"][0]["document"]["resources"] = [{
            "kind": "build", "state": "recovered", "immutable": {"name": original.name, "path": str(original)},
            "recovery": {"observation": {"reservations": [
                {"name": original.name, "path": str(original)},
                {"name": residual.name, "path": str(residual)},
                {"name": generation.name, "path": str(generation)}]}}}]
        with mock.patch("atrinik_workspace.delivery.subprocess.run",
            return_value=SimpleNamespace(returncode=0, stdout=json.dumps(output), stderr="")):
            evidence = inventory_active_delivery_evidence(self.root)
        self.assertEqual({Path(row["path"]) for row in evidence.recovered}, {original, residual, generation})
        for path in (original, residual, generation):
            self.assertIn(self.name, evidence.references[path])
        self.assertNotIn(worktree, {Path(row["path"]) for row in evidence.recovered})

    def test_compact_ledger_lock_coordinate_obeys_name_limit(self) -> None:
        ledger_name = "a" * 240 + ".md.ledger.json"
        lock_name = _ledger_lock_name(self.review_root, ledger_name)
        self.assertTrue(lock_name.startswith(".delivery-ledger-lock-"))
        self.assertLessEqual(
            len(lock_name.encode("utf-8")), os.pathconf(self.review_root, "PC_NAME_MAX")
        )

    def test_missing_report_fails_closed(self) -> None:
        (self.review_root / self.name.removesuffix(".ledger.json")).unlink()
        completed = SimpleNamespace(
            returncode=0, stdout=self.inventory_output(self.root), stderr=""
        )
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run", return_value=completed
        ):
            with self.assertRaisesRegex(WorkspaceError, "active delivery report"):
                inventory_active_delivery_evidence(self.root)

    def test_projects_planned_worktree_request(self) -> None:
        worktree = self.root / "workspace" / "worktrees" / "atrinik" / "planned"
        document = {
            "artifacts": [
                {
                    "kind": "worktree",
                    "current": None,
                    "primitive_request": {
                        "roots": {"workspace": {"path": str(self.root / "workspace")}},
                        "physical_checkout": "atrinik",
                        "label": "planned",
                    },
                }
            ]
        }
        output = {
            "schema_version": 1,
            "ledgers": [{"name": self.name, "document": document}],
            "pending": [],
            "legacy_reports": [],
            "releases": [],
            "archives": [],
            "reclaims": [],
            "historical_ledgers": [],
        }
        completed = SimpleNamespace(returncode=0, stdout=json.dumps(output), stderr="")
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run", return_value=completed
        ):
            evidence = inventory_active_delivery_evidence(self.root)

        self.assertEqual(evidence.references[worktree.resolve()], (self.name,))

    def test_invalid_helper_inventory_fails_closed(self) -> None:
        completed = SimpleNamespace(returncode=2, stdout="", stderr="old schema")
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run", return_value=completed
        ):
            with self.assertRaisesRegex(WorkspaceError, "old schema"):
                inventory_active_delivery_evidence(self.root)

    def test_absent_and_invalid_review_roots_fail_closed(self) -> None:
        absent = self.root / "absent"
        evidence = inventory_active_delivery_evidence(absent)
        self.assertEqual(evidence.references, {})

        invalid = self.root / "invalid"
        invalid.mkdir()
        (invalid / "build").mkdir()
        (invalid / "build" / "reviews").write_text("not a directory", encoding="utf-8")
        with self.assertRaisesRegex(WorkspaceError, "regular directory"):
            inventory_active_delivery_evidence(invalid)

    def test_helper_missing_timeout_and_output_limit_fail_closed(self) -> None:
        self.helper.unlink()
        with self.assertRaisesRegex(WorkspaceError, "delivery-ledger helper is missing"):
            inventory_active_delivery_evidence(self.root)

        self.helper.write_text("trusted helper\n", encoding="utf-8")
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run",
            side_effect=subprocess.TimeoutExpired("inventory", 30),
        ):
            with self.assertRaisesRegex(WorkspaceError, "inventory failed"):
                inventory_active_delivery_evidence(self.root)

        completed = SimpleNamespace(returncode=0, stdout="12", stderr="")
        with mock.patch(
            "atrinik_workspace.delivery._INVENTORY_LIMIT", 1
        ), mock.patch(
            "atrinik_workspace.delivery.subprocess.run", return_value=completed
        ):
            with self.assertRaisesRegex(WorkspaceError, "output limit"):
                inventory_active_delivery_evidence(self.root)

    def test_helper_json_and_inventory_shapes_fail_closed(self) -> None:
        outputs = (
            ("not json", "valid JSON"),
            (json.dumps({"schema_version": 2}), "unsupported"),
            (json.dumps({"schema_version": 1}), "ledgers are invalid"),
            (json.dumps({"schema_version": 1, "ledgers": ["bad"]}), r"ledgers\[0\] is invalid"),
            (
                json.dumps({"schema_version": 1, "ledgers": [{"name": "bad", "document": {}}]}),
                r"ledgers\[0\] is invalid",
            ),
            (
                json.dumps(
                    {
                        "schema_version": 1,
                        "ledgers": [{"name": self.name, "document": {"artifacts": {}}}],
                    }
                ),
                "artifacts is invalid",
            ),
        )
        for output, message in outputs:
            with self.subTest(message=message), mock.patch(
                "atrinik_workspace.delivery.subprocess.run",
                return_value=SimpleNamespace(returncode=0, stdout=output, stderr=""),
            ):
                with self.assertRaisesRegex(WorkspaceError, message):
                    inventory_active_delivery_evidence(self.root)

    def test_inventory_rejects_invalid_worktree_paths_and_preserves_old_evidence(self) -> None:
        output = {
            "schema_version": 1,
            "ledgers": [
                {
                    "name": self.name,
                    "document": {
                        "artifacts": [
                            {"kind": "worktree", "current": "invalid"},
                        ]
                    },
                }
            ],
            "pending": [{"target": self.name}],
            "legacy_reports": [{"name": self.name}],
            "releases": [],
            "archives": [{"ledger_name": self.name}],
            "reclaims": [{"name": self.name}],
            "historical_ledgers": [{"name": self.name}],
        }
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run",
            return_value=SimpleNamespace(returncode=0, stdout=json.dumps(output), stderr=""),
        ):
            with self.assertRaisesRegex(WorkspaceError, "current is invalid"):
                inventory_active_delivery_evidence(self.root)

        output["ledgers"][0]["document"]["artifacts"] = [
            {"kind": "worktree", "current": {"path": "relative"}}
        ]
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run",
            return_value=SimpleNamespace(returncode=0, stdout=json.dumps(output), stderr=""),
        ):
            with self.assertRaisesRegex(WorkspaceError, "current.path is not an absolute path"):
                inventory_active_delivery_evidence(self.root)

    def test_inventory_preserves_pending_legacy_and_terminal_evidence(self) -> None:
        output = json.loads(self.inventory_output(self.root))
        output.update(
            {
                "pending": [{"target": self.name}],
                "legacy_reports": [{"name": self.name}],
                "releases": [{"target": self.name}],
                "archives": [{"ledger_name": self.name}],
                "reclaims": [{"name": self.name}],
                "historical_ledgers": [{"name": self.name}],
            }
        )
        with mock.patch(
            "atrinik_workspace.delivery.subprocess.run",
            return_value=SimpleNamespace(returncode=0, stdout=json.dumps(output), stderr=""),
        ):
            evidence = inventory_active_delivery_evidence(self.root)

        labels = evidence.references[self.review_root.resolve()]
        self.assertIn(f"pending:{self.name}", labels)
        self.assertIn(f"legacy:{self.name}", labels)
        self.assertIn(f"release:{self.name}", labels)
        self.assertIn(f"archive:{self.name}", labels)
        self.assertIn(f"reclaim:{self.name}", labels)
        self.assertIn(f"historical:{self.name}", labels)
        self.assertIn(f"pending:{self.name}", evidence.transition_blockers)

    def test_unmanaged_review_root_is_protected(self) -> None:
        paths = SimpleNamespace(
            repository=self.root,
            builds=self.root / "workspace" / "build",
        )
        paths.builds.mkdir(parents=True)
        cleanup = Cleanup.__new__(Cleanup)
        cleanup._wrapper_primary = self.root
        cleanup.paths = paths
        cleanup.now = datetime.now(timezone.utc)
        references = {"delivery": {self.review_root.resolve(): [self.name]}}

        items = cleanup._unmanaged_builds(set(), references, set())

        item = next(row for row in items if Path(row["path"]) == self.review_root)
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("delivery_reference", item["reasons"])
        self.assertEqual(item["references"]["delivery"], [self.name])

    def test_delivery_inventory_error_is_visible_on_review_root(self) -> None:
        paths = SimpleNamespace(
            repository=self.root,
            builds=self.root / "workspace" / "build",
        )
        paths.builds.mkdir(parents=True)
        cleanup = Cleanup.__new__(Cleanup)
        cleanup._wrapper_primary = self.root
        cleanup.paths = paths
        cleanup.now = datetime.now(timezone.utc)

        items = cleanup._unmanaged_builds(
            set(), {"delivery": {}}, {"delivery_inventory_error"}
        )

        item = next(row for row in items if Path(row["path"]) == self.review_root)
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("delivery_inventory_error", item["reasons"])


class MixedDeliveryLayoutTests(unittest.TestCase):
    def test_cleanup_inventories_each_primary_and_linked_checkout_helper(self):
        current = Path("scripts/delivery_ledger.py")
        legacy = Path(".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py")
        for primary_layout, linked_layout in ((legacy, current), (current, legacy)):
            with self.subTest(primary_layout=primary_layout), tempfile.TemporaryDirectory() as temporary:
                base = Path(temporary)
                primary, linked = base / "primary", base / "linked"
                protected = []
                for root, layout in ((primary, primary_layout), (linked, linked_layout)):
                    review = root / "build/reviews"
                    review.mkdir(parents=True)
                    name = f"{root.name}.md.ledger.json"
                    for filename in (name, name.removesuffix(".ledger.json"), f".{name}.lock"):
                        (review / filename).write_text("unchanged evidence\n")
                    retained = root / "workspace/worktrees/reserved"
                    protected.append(retained)
                    output = {"schema_version": 1, "ledgers": [{"name": name, "document": {
                        "artifacts": [{"kind": "worktree", "current": {"path": str(retained)}}]}}],
                        "pending": [], "legacy_reports": [], "releases": [], "archives": [],
                        "reclaims": [], "historical_ledgers": []}
                    helper = root / layout
                    helper.parent.mkdir(parents=True)
                    helper.write_text(
                        "import os, sys\n"
                        f"assert os.getcwd() == {str(root)!r}\n"
                        f"assert sys.argv[1:] == ['inventory', {str(review)!r}]\n"
                        f"print({json.dumps(output)!r})\n"
                    )
                # Exercise the actual linked-root discovery and cleanup projection;
                # only Git's worktree observation is replaced by a fixture.
                (linked / ".git").write_text("fixture linked Git metadata\n")
                workspace = object.__new__(Workspace)
                workspace.paths = SimpleNamespace(repository=linked)
                cleanup = object.__new__(Cleanup)
                cleanup.workspace = workspace
                references = {"delivery": {}}
                before = {path: path.read_bytes() for path in base.rglob("*") if path.is_file()}
                with mock.patch("atrinik_workspace.workspace._worktree_records",
                                return_value=[{"worktree": str(primary)}, {"worktree": str(linked)}]):
                    cleanup._delivery_references(references, set())
                for root, retained in zip((primary, linked), protected):
                    self.assertEqual(references["delivery"][retained], [f"{root.name}.md.ledger.json"])
                    self.assertIn(root / "build/reviews", references["delivery"])
                after = {path: path.read_bytes() for path in base.rglob("*") if path.is_file()}
                self.assertEqual(after, before)


class MigrationDeliveryBarrierTests(unittest.TestCase):
    def test_active_delivery_evidence_refuses_repository_migration(self) -> None:
        migration = object.__new__(RepositoryMigration)
        migration.repository_root = Path("/tmp/wrapper")
        migration.paths = SimpleNamespace(workspace=Path("/tmp/workspace"))
        migration.manifest = SimpleNamespace()
        with mock.patch(
            "atrinik_workspace.migration.inventory_active_delivery_evidence",
            return_value=ActiveDeliveryEvidence(
                Path("/tmp/reviews"), {}, ("delivery.json",), ("delivery.json",)
            ),
        ), mock.patch.object(
            migration, "_inspect_classic", return_value=(None, {}, [])
        ), mock.patch.object(
            migration, "_inspect_profiles", return_value=([], [], [], [])
        ), mock.patch.object(
            migration, "_topology_inventory", return_value=([], [])
        ), mock.patch.object(
            migration, "_inert_inventory", return_value=([], [])
        ):
            inspection = migration._inspect()

        self.assertEqual(
            [row["code"] for row in inspection.plan["refusals"]],
            ["active_delivery"],
        )


if __name__ == "__main__":
    unittest.main()
