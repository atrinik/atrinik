# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Offline consumer checks, independent of a host's installed plugins."""
from contextlib import redirect_stderr, redirect_stdout
import copy
import io
import json
import os
import subprocess
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from atrinik_workspace import guidance_inventory as inventory
from atrinik_workspace.skill_provider import (
    DESCRIPTOR_PATH, REQUIRED_SKILLS, load_provider, parse_provider, validate_routes,
)

ROOT = Path(__file__).resolve().parents[1]


class SkillProviderTests(unittest.TestCase):
    def test_descriptor_and_routes_validate_offline(self):
        provider = load_provider(ROOT)
        self.assertEqual(provider["required_skills"], list(REQUIRED_SKILLS))
        validate_routes(ROOT, provider)
        # No discovery of a local/installed provider and no implicit subprocess.
        with patch.object(Path, "glob", side_effect=AssertionError("implicit provider lookup")), patch.object(
            inventory.subprocess, "run", side_effect=AssertionError("implicit external command")
        ):
            result = inventory.collect_inventory()
        self.assertEqual(result["external_metrics"], "unmeasured")
        self.assertTrue(all(value is None for value in result["summary"].values()))
        self.assertEqual(inventory.budget_failures(result), [])

    def test_descriptor_rejects_invalid_identity_shape_and_pin(self):
        provider = load_provider(ROOT)
        cases = [[], None, {}, {**provider, "unexpected": True}]
        for field, invalid in {
            "schema_version": [True, 2, "1"],
            "repository": ["https://example.invalid/provider", "https://github.com/atrinik/agent-skills.git",
                           "https://user:secret@github.com/atrinik/skills", "https://github.com/../skills"],
            "revision": ["0" * 40, "main", "a" * 39, "A" * 40, "../" + "a" * 40, 123],
            "marketplace": ["../other", "a" * 65], "plugin": ["Other", "name/child"],
            "path": ["../plugins/atrinik-development", "/plugins/atrinik-development", "plugins//name",
                     "plugins/./name", "plugins/../name", "plugins\\name", "a/" * 9 + "a", "a" * 513],
            "required_skills": [[], list(REQUIRED_SKILLS) + [REQUIRED_SKILLS[0]],
                                "atrinik-issue-delivery", ["../name"], [False], ["a" * 65],
                                [f"skill-{index}" for index in range(51)]],
        }.items():
            cases.extend({**provider, field: value} for value in invalid)
        for value in cases:
            with self.subTest(value=value), self.assertRaises(ValueError):
                parse_provider(json.dumps(value))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            parse_provider('{"schema_version": 1, "schema_version": 1}')
        with self.assertRaises(ValueError):
            parse_provider(b"not json")

    def test_generic_descriptor_does_not_replace_wrapper_consumer_checks(self):
        provider = load_provider(ROOT)
        classic = json.loads((ROOT / "tests/fixtures/classic-skill-provider.json").read_bytes())
        self.assertEqual(parse_provider(json.dumps(classic)), classic)
        candidates = [classic, {**provider, "required_skills": list(reversed(REQUIRED_SKILLS))}]
        candidates.extend({**provider, key: replacement} for key, replacement in {
            "repository": "https://github.com/example/skills", "marketplace": "other",
            "plugin": "other", "path": "other/plugin",
        }.items())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / ".agents").mkdir()
            for candidate in candidates:
                self.assertEqual(parse_provider(json.dumps(candidate)), candidate)
                (root / DESCRIPTOR_PATH).write_text(json.dumps(candidate))
                with self.subTest(candidate=candidate), self.assertRaises(ValueError):
                    load_provider(root)
        with self.assertRaisesRegex(ValueError, "bound"):
            parse_provider(" " * 32769)

    def test_direct_script_works_without_pythonpath_and_ignores_cwd_shadow(self):
        environment = {key: value for key, value in os.environ.items() if key != "PYTHONPATH"}
        with tempfile.TemporaryDirectory() as temporary:
            (Path(temporary) / "skill_provider.py").write_text("raise RuntimeError('cwd shadow imported')")
            for flags in ([], ["-I"]):
                completed = subprocess.run(
                    [sys.executable, *flags, str(ROOT / "atrinik_workspace/guidance_inventory.py"), "--check", "--json"],
                    cwd=temporary, env=environment, text=True, capture_output=True, check=False,
                )
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertEqual(json.loads(completed.stdout)["external_metrics"], "unmeasured")

    def test_missing_invalid_descriptor_and_stale_routes_fail_locally(self):
        provider = load_provider(ROOT)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch.object(inventory, "ROOT", root), self.assertRaises(OSError):
                inventory.collect_inventory()
            (root / ".agents").mkdir()
            (root / DESCRIPTOR_PATH).write_text("{}")
            with patch.object(inventory, "ROOT", root), self.assertRaises(ValueError):
                inventory.collect_inventory()
            for text in (
                "Use `atrinik-invented` .agents/skill-provider.json docs/SKILL_PROVIDER.md",
                "Read .agents/skills/old/SKILL.md .agents/skill-provider.json docs/SKILL_PROVIDER.md",
                "No setup route",
            ):
                (root / "AGENTS.md").write_text(text)
                with self.subTest(text=text), self.assertRaises(ValueError):
                    validate_routes(root, provider)

    def _skills(self, root):
        for name in REQUIRED_SKILLS:
            path = root / name / "SKILL.md"
            path.parent.mkdir(parents=True)
            path.write_text(f"---\nname: {name}\ndescription: Fixture workflow.\n---\nFixture.\n")
        # Provider-only entries must not change the wrapper's twelve-entry budget.
        for index in range(3):
            path = root / f"provider-only-{index}" / "SKILL.md"
            path.parent.mkdir()
            path.write_text("Not a wrapper requirement\n")

    def test_explicit_integration_counts_required_twelve_and_checks_cli(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._skills(root)
            result = inventory.collect_inventory(root)
            self.assertEqual(result["external_metrics"], "measured")
            self.assertEqual(result["summary"]["skill_count"], 12)
            self.assertGreater(result["summary"]["all_skill_bytes"], 0)
            self.assertEqual(inventory.budget_failures(result), [])
            with redirect_stdout(io.StringIO()):
                self.assertEqual(inventory.main(["--skills-root", str(root), "--check"]), 0)
            path = root / REQUIRED_SKILLS[-1] / "SKILL.md"
            path.unlink()
            with self.assertRaisesRegex(ValueError, "missing required"):
                inventory.collect_inventory(root)
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(inventory.main(["--skills-root", str(root), "--check"]), 1)
            path.write_text("---\nname: invalid\ndescription: Invalid name.\n---\n")
            with self.assertRaisesRegex(ValueError, "match its directory"):
                inventory.collect_inventory(root)

    def test_all_measured_budgets_enforce_original_ceilings(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._skills(root)
            baseline = inventory.collect_inventory(root)
        for section, key, limit in (
            ("root_guide", "bytes", inventory.MAX_ROOT_GUIDE_BYTES),
            ("summary", "catalog_bytes", inventory.MAX_CATALOG_BYTES),
            ("summary", "startup_bytes", inventory.MAX_STARTUP_BYTES),
            ("summary", "multi_selected_bytes", inventory.MAX_MULTI_SELECTED_BYTES),
            ("summary", "all_skill_bytes", inventory.MAX_ALL_SKILL_BYTES),
        ):
            value = copy.deepcopy(baseline)
            value[section][key] = limit
            self.assertEqual(inventory.budget_failures(value), [])
            value[section][key] += 1
            self.assertEqual(len(inventory.budget_failures(value)), 1)

    def test_local_terminology_remains_current(self):
        for relative in ("AGENTS.md", "atrinik_workspace/guidance_inventory.py"):
            self.assertNotIn("PR-stack", (ROOT / relative).read_text())
