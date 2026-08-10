from __future__ import annotations

import json
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load_json(path: str) -> dict[str, object]:
    return json.loads((ROOT / path).read_text(encoding="utf-8"))


class ReplacementFoundationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.inventory = load_json("governance/replacement-foundations.json")
        self.manifest = load_json("components.json")
        self.supply_chain = load_json("supply-chain/inventory.json")

    def test_every_replacement_component_has_one_complete_record(self) -> None:
        expected = {
            "client",
            "content-toolkit",
            "editor",
            "protocol",
            "renderer",
            "server",
            "website",
        }
        records = self.inventory["repositories"]
        names = [record["name"] for record in records]

        self.assertEqual(set(names), expected)
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(names, sorted(names))

        default = self.manifest["stacks"]["default"]
        for name in expected:
            self.assertIn(name, default["components"])
            self.assertEqual(default["providers"][name], name)

        required = {
            "repository",
            "language",
            "m1_issues",
            "proof_pull_requests",
            "aggregate_check",
            "workflow",
            "guidance",
            "machine_records",
            "notice",
            "sbom_contract",
            "package_contract",
            "license_boundary",
        }
        for record in records:
            with self.subTest(repository=record["repository"]):
                self.assertEqual(set(record) - {"name"}, required)
                self.assertTrue(record["repository"].startswith("atrinik/"))
                self.assertTrue(record["m1_issues"])
                self.assertTrue(record["proof_pull_requests"])
                self.assertTrue(record["machine_records"])
                self.assertIn("license", record["license_boundary"].lower())

    def test_root_policy_and_reproducible_examples_fail_closed(self) -> None:
        policy = self.inventory["root_policy"]
        self.assertEqual(policy["repository"], "atrinik/atrinik")
        self.assertRegex(policy["revision"], r"^[0-9a-f]{40}$")
        self.assertEqual(policy["path"], "AGENTS.md")
        registry = subprocess.run(
            [
                "git",
                "show",
                f"{policy['revision']}:{policy['path']}",
            ],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        self.assertIn("Zoey Rose", registry)
        self.assertIn("Daniel Liptrot", registry)

        required_fields = self.inventory["required_record_fields"]
        self.assertEqual(len(required_fields), len(set(required_fields)))
        self.assertIn("history_evidence", required_fields)
        self.assertIn("identity_evidence", required_fields)
        self.assertIn("embedded_material_review", required_fields)
        self.assertIn("reviewer", required_fields)

        examples = self.inventory["reproducible_examples"]
        self.assertIn("#lossless-core-model", examples["admitted"]["record"])
        self.assertIn("#remaining-content-tools", examples["excluded"]["record"])
        self.assertNotEqual(
            examples["admitted"]["verification"],
            examples["excluded"]["verification"],
        )

    def test_foundation_repositories_are_owned_in_supply_chain(self) -> None:
        supply_chain = {
            record["repository"]: record
            for record in self.supply_chain["repositories"]
            if record["supported"]
        }
        for record in self.inventory["repositories"]:
            with self.subTest(repository=record["repository"]):
                self.assertIn(record["repository"], supply_chain)
                self.assertEqual(supply_chain[record["repository"]]["branch"], "main")
                self.assertTrue(supply_chain[record["repository"]]["audit_ready"])

    def test_scheduled_audit_initializes_complete_profiles(self) -> None:
        workflow = (ROOT / ".github/workflows/supply-chain.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("run: ./atrinik init --with classic", workflow)
        self.assertIn("./atrinik supply-chain audit --profile default", workflow)
        self.assertIn("./atrinik supply-chain audit --profile classic", workflow)

        checkouts = {
            checkout["name"]: checkout for checkout in self.manifest["checkouts"]
        }
        default_components = set(self.manifest["stacks"]["default"]["components"])
        for record in self.inventory["repositories"]:
            with self.subTest(repository=record["repository"]):
                self.assertIn(record["name"], default_components)
                self.assertEqual(
                    checkouts[record["name"]]["repository"], record["repository"]
                )
                self.assertEqual(checkouts[record["name"]]["branch"], "main")

        dependencies = {
            dependency["id"]: dependency
            for dependency in self.supply_chain["dependencies"]
        }
        for name in {
            "client",
            "content-toolkit",
            "editor",
            "protocol",
            "renderer",
            "server",
            "website",
        }:
            self.assertTrue(
                any(name in dependency["scope"] for dependency in dependencies.values())
            )


class ClassicToolsInventoryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.inventory = load_json("governance/classic-tools.json")
        self.manifest = load_json("components.json")

    def test_every_tracked_top_level_tools_path_is_owned_exactly_once(self) -> None:
        expected = {
            "atrinik_bot",
            "gridarta-types-convert",
            "gridarta_materials.pl",
            "map-checker",
            "map-checker-qt",
            "map-maker",
            "mapset",
            "randomizer",
            "split_symbols.sh",
            "stacktrace.py",
            "worldviewer",
        }
        paths = [path for entry in self.inventory["entries"] for path in entry["paths"]]

        self.assertEqual(set(paths), expected)
        self.assertEqual(len(paths), len(set(paths)))
        ids = [entry["id"] for entry in self.inventory["entries"]]
        self.assertEqual(len(ids), len(set(ids)))

    def test_every_entry_has_owner_issue_milestone_and_safe_disposition(self) -> None:
        required = {
            "paths",
            "legacy_commands",
            "purpose",
            "inputs",
            "outputs",
            "current_usage",
            "owner_repository",
            "owner_issue",
            "owner_milestone",
            "disposition",
            "replacement_command",
            "licensing_method",
            "safety",
            "verification",
        }
        allowed_dispositions = {
            "migrate-then-retire",
            "replace",
            "retire",
            "retain-classic",
        }
        for entry in self.inventory["entries"]:
            with self.subTest(entry=entry["id"]):
                self.assertEqual(set(entry) - {"id"}, required)
                self.assertTrue(entry["owner_repository"].startswith("atrinik/"))
                self.assertTrue(entry["owner_issue"].startswith("https://github.com/"))
                self.assertRegex(entry["owner_issue"], r"/issues/[0-9]+$")
                self.assertRegex(entry["owner_milestone"], r"^M[0-9]+ — ")
                self.assertIn(entry["disposition"], allowed_dispositions)
                self.assertIn("GPL", entry["licensing_method"])
                self.assertTrue(entry["verification"])

    def test_tools_remain_explicit_classic_only_and_not_a_runtime_dependency(self) -> None:
        source = self.inventory["source"]
        self.assertTrue(source["history_complete"])
        self.assertFalse(source["ordinary_replacement_initialization"])
        self.assertTrue(source["classic_cohort_only"])
        self.assertFalse(source["replacement_build_runtime_dependency"])

        self.assertNotIn("tools", self.manifest["cohorts"]["default"])
        self.assertIn("tools", self.manifest["cohorts"]["classic"])
        self.assertNotIn("tools", self.manifest["stacks"]["default"]["components"])
        self.assertIn("tools", self.manifest["stacks"]["classic"]["components"])

        default_components = set(self.manifest["stacks"]["default"]["components"])
        for component in self.manifest["components"]:
            if component["name"] in default_components:
                self.assertNotIn("tools", component.get("requires", []))

    def test_bot_migration_is_owned_by_the_classic_only_playtester(self) -> None:
        bot = next(
            entry
            for entry in self.inventory["entries"]
            if entry["id"] == "atrinik-bot"
        )

        self.assertEqual(bot["owner_repository"], "atrinik/playtester")
        self.assertEqual(
            bot["owner_issue"], "https://github.com/atrinik/playtester/issues/1"
        )
        self.assertIn("MIT", bot["licensing_method"])
        self.assertIn("GPL", bot["licensing_method"])
        self.assertIn("OpenAI Codex", bot["licensing_method"])
        self.assertIn("direct supervision and steering", bot["licensing_method"])
        self.assertIn("rights she holds", bot["licensing_method"])
        self.assertNotIn("wholly authored", bot["licensing_method"])
        self.assertNotIn("playtester", self.manifest["cohorts"]["default"])
        self.assertIn("playtester", self.manifest["cohorts"]["classic"])
        self.assertNotIn(
            "playtester", self.manifest["stacks"]["default"]["components"]
        )
        self.assertEqual(
            self.manifest["stacks"]["classic"]["providers"]["playtester"],
            "playtester",
        )


if __name__ == "__main__":
    unittest.main()
