from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.supply_chain import (
    ACTION_REFERENCE_PATTERN,
    DOCKER_PULL_PATTERN,
    Inventory,
    _container_references,
    version_report,
    write_generated,
)


ROOT = Path(__file__).resolve().parents[1]


class InventoryTests(unittest.TestCase):
    def load_inventory(self) -> Inventory:
        return Inventory.load(
            ROOT / "supply-chain" / "inventory.json", ROOT / "components.json"
        )

    def test_inventory_and_schema_validate(self) -> None:
        inventory = self.load_inventory()
        inventory.validate_schema(ROOT / "supply-chain" / "schema.json")

        self.assertGreaterEqual(len(inventory.dependencies), 60)
        self.assertIn("nawerhals", inventory.repositories_by_name)
        self.assertFalse(inventory.repositories_by_name["nawerhals"].supported)

    def test_wrapper_dependency_surface_audits_independently(self) -> None:
        messages = self.load_inventory().audit(
            {"atrinik": ROOT}, require_all=False
        )

        self.assertEqual(len(messages), 1)
        self.assertIn("action references", messages[0])

    def test_reports_are_deterministic_and_well_formed(self) -> None:
        inventory = self.load_inventory()
        first = inventory.report("cyclonedx")
        second = inventory.report("cyclonedx")
        cyclonedx = json.loads(first)
        spdx = json.loads(inventory.report("spdx"))

        self.assertEqual(first, second)
        self.assertEqual(cyclonedx["specVersion"], "1.6")
        self.assertEqual(len(cyclonedx["components"]), len(inventory.dependencies))
        self.assertEqual(spdx["spdxVersion"], "SPDX-2.3")
        self.assertTrue(
            any(
                property_["name"] == "atrinik:declared-packages"
                for component in cyclonedx["components"]
                for property_ in component["properties"]
            )
        )
        self.assertTrue(
            all(package["supplier"] == "NOASSERTION" for package in spdx["packages"])
        )
        self.assertIn("| Dependency | Version |", inventory.report("licenses"))

    def test_nested_action_coordinates_are_recognized(self) -> None:
        match = ACTION_REFERENCE_PATTERN.search(
            "uses: organization/actions/.github/workflows/check.yml@" + "a" * 40
        )

        self.assertIsNotNone(match)
        self.assertEqual(
            match.group(1), "organization/actions/.github/workflows/check.yml"
        )

    def test_workflow_container_pulls_are_recognized(self) -> None:
        match = DOCKER_PULL_PATTERN.search(
            "run: docker pull ghcr.io/atrinik/build:1@sha256:" + "a" * 64
        )

        self.assertIsNotNone(match)
        self.assertEqual(
            match.group(1), "ghcr.io/atrinik/build:1@sha256:" + "a" * 64
        )

    def test_internal_docker_stages_are_not_external_images(self) -> None:
        dockerfile = """# syntax=docker/dockerfile:1@sha256:{frontend}
FROM ubuntu:26.04@sha256:{digest} AS toolchain
FROM toolchain AS validation
FROM toolchain AS final
""".format(frontend="b" * 64, digest="a" * 64)

        self.assertEqual(
            _container_references("Dockerfile", dockerfile),
            [
                f"docker/dockerfile:1@sha256:{'b' * 64}",
                f"ubuntu:26.04@sha256:{'a' * 64}",
            ],
        )

    def test_generated_output_is_restricted_to_build(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(WorkspaceError, "must be under"):
                write_generated(root, Path("report.json"), "{}\n")

            output = Path("build/supply-chain/report.json")
            write_generated(root, output, "{}\n")
            self.assertEqual((root / output).read_text(encoding="utf-8"), "{}\n")

    def test_duplicate_devcontainer_keys_are_rejected(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "duplicate JSON key"):
            _container_references(
                ".devcontainer/devcontainer.json",
                '{"image":"first@sha256:a","image":"second@sha256:b"}',
            )

    def test_version_report_is_machine_readable(self) -> None:
        versions = json.loads(version_report(self.load_inventory()))

        self.assertTrue(versions["python"]["available"])
        self.assertTrue(versions["git"]["available"])
        self.assertIn("system-packages", versions)
        self.assertIn("git", versions["system-packages"])
        self.assertEqual(
            versions["declared-dependencies"]["container/ubuntu-26.04"]["checksum"],
            "sha256:678c6550cc43645e08669028bc177f50be4e7c5b8cca677067b1914d4afc7a03",
        )


if __name__ == "__main__":
    unittest.main()
