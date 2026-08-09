from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import re
import tempfile
import unittest
from unittest import mock

from atrinik_workspace import guidance_inventory
from atrinik_workspace.guidance_inventory import (
    budget_failures,
    collect_inventory,
    file_metrics,
    main,
    render_text,
    skill_frontmatter,
)


ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"\[[^]]+\]\(([^)]+)\)")


class AgentGuidanceTests(unittest.TestCase):
    def test_current_provenance_registry_is_complete(self) -> None:
        registry = (ROOT / "docs/PROVENANCE.md").read_text(encoding="utf-8")
        self.assertIn("Zoey Rose", registry)
        self.assertIn("Daniel Liptrot", registry)

    def test_inventory_is_complete_and_within_budget(self) -> None:
        inventory = collect_inventory()
        self.assertEqual(inventory["summary"]["skill_count"], 8)
        self.assertIn(
            "atrinik-guidance-maintenance",
            [skill["name"] for skill in inventory["skills"]],
        )
        self.assertEqual(
            [skill["name"] for skill in inventory["skills"]],
            sorted(
                path.parent.name
                for path in (ROOT / ".agents/skills").glob("*/SKILL.md")
            ),
        )
        self.assertEqual(budget_failures(inventory), [])

        rendered = render_text(inventory)
        self.assertTrue(rendered.startswith("path\tbytes\tlines\twords\n"))
        self.assertIn("summary\tcatalog=", rendered)

        metrics = file_metrics(ROOT / "AGENTS.md")
        self.assertEqual(metrics.path, "AGENTS.md")
        self.assertGreater(metrics.bytes, 0)
        self.assertLess(metrics.lines, 150)

    def test_frontmatter_validation_fails_closed(self) -> None:
        cases = {
            "missing": "# no frontmatter\n",
            "unterminated": "---\nname: skill\n",
            "unsupported": "---\nname: skill\nsummary: no\n---\n",
            "duplicate": (
                "---\nname: skill\nname: skill\ndescription: duplicate\n---\n"
            ),
            "incomplete": "---\nname: skill\n---\n",
            "mismatched": "---\nname: other\ndescription: mismatch\n---\n",
        }
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "skill" / "SKILL.md"
            path.parent.mkdir()
            for name, content in cases.items():
                with self.subTest(name=name):
                    path.write_text(content, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        skill_frontmatter(path)

    def test_inventory_requires_the_workspace_skill(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with mock.patch.object(
                guidance_inventory, "SKILLS_ROOT", Path(temporary)
            ):
                with self.assertRaisesRegex(ValueError, "missing atrinik-multi"):
                    collect_inventory()

    def test_command_output_and_failures(self) -> None:
        stdout = io.StringIO()
        with redirect_stdout(stdout):
            self.assertEqual(main([]), 0)
        self.assertIn("summary\tcatalog=", stdout.getvalue())

        stdout = io.StringIO()
        with redirect_stdout(stdout):
            self.assertEqual(main(["--json"]), 0)
        inventory = json.loads(stdout.getvalue())
        self.assertEqual(inventory["summary"]["skill_count"], 8)

        stderr = io.StringIO()
        with mock.patch.object(
            guidance_inventory, "budget_failures", return_value=["test ceiling"]
        ), redirect_stdout(io.StringIO()), redirect_stderr(stderr):
            self.assertEqual(main(["--check"]), 1)
        self.assertIn("guidance budget failed: test ceiling", stderr.getvalue())

        stderr = io.StringIO()
        with mock.patch.object(
            guidance_inventory,
            "collect_inventory",
            side_effect=ValueError("invalid guidance"),
        ), redirect_stderr(stderr):
            self.assertEqual(main([]), 1)
        self.assertIn(
            "guidance inventory failed: invalid guidance", stderr.getvalue()
        )

    def test_local_guidance_links_resolve(self) -> None:
        paths = [ROOT / "AGENTS.md"]
        paths.extend(sorted((ROOT / ".agents/skills").glob("*/SKILL.md")))
        paths.extend(sorted((ROOT / ".agents/skills").glob("*/references/*.md")))
        for path in paths:
            for target in LINK.findall(path.read_text(encoding="utf-8")):
                if "://" in target or target.startswith("#"):
                    continue
                with self.subTest(path=path.relative_to(ROOT), target=target):
                    resolved = (path.parent / target).resolve()
                    self.assertTrue(resolved.is_relative_to(ROOT))
                    self.assertTrue(resolved.is_file())

    def test_removed_stale_routes_do_not_return(self) -> None:
        paths = [ROOT / "AGENTS.md"]
        paths.extend(sorted((ROOT / ".agents/skills").glob("*/SKILL.md")))
        corpus = "\n".join(path.read_text(encoding="utf-8") for path in paths)
        for stale in {
            "mixed-component profile",
            "more than one standalone repository",
            "Today this seed repository",
            "scenario create NAME --state",
        }:
            with self.subTest(stale=stale):
                self.assertNotIn(stale, corpus)


if __name__ == "__main__":
    unittest.main()
