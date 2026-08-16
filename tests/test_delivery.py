from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cleanup import Cleanup
from atrinik_workspace.delivery import (
    ActiveDeliveryEvidence,
    inventory_active_delivery_evidence,
)
from atrinik_workspace.migration import RepositoryMigration
from atrinik_workspace.model import WorkspaceError


class DeliveryEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.review_root = self.root / "build" / "reviews"
        self.review_root.mkdir(parents=True)
        self.helper = (
            self.root
            / ".agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py"
        )
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
        self.assertEqual(invoke.call_args.args[0][3], "inventory")
        self.assertEqual(invoke.call_args.kwargs["timeout"], 30)

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

        items = cleanup._unmanaged_builds(set(), references)

        item = next(row for row in items if Path(row["path"]) == self.review_root)
        self.assertEqual(item["disposition"], "protected")
        self.assertIn("delivery_reference", item["reasons"])
        self.assertEqual(item["references"]["delivery"], [self.name])


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
