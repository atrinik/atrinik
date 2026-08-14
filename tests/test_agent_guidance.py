from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import re
import subprocess
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
        registry = " ".join(
            (ROOT / "docs/PROVENANCE.md").read_text(encoding="utf-8").split()
        )
        self.assertIn("Zoey Rose", registry)
        self.assertIn("Daniel Liptrot", registry)
        for marker in {
            "affirmative permissions",
            "used as implementation reference",
            "clean-room isolation is not required",
            "original past Atrinik contributions",
            "not a prospective grant",
            "cannot be combined to cover a jointly authored contribution",
            "not relicensed by these historical grants",
            "does not by itself place agent-generated output",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, registry)

        identity_policy = " ".join(
            (ROOT / "docs/PROVENANCE_IDENTITIES.md")
            .read_text(encoding="utf-8")
            .split()
        )
        for marker in {
            "Prior public appearance is not authorization",
            "Removing an `alias` field alone does not prevent re-identification",
            "provenance-custodians",
            "provenance-reviewers",
            "seven years after the last reliance is withdrawn",
            "Suspected disclosure freezes new attestations",
            "Key compromise rotates recipients and HMAC keys",
            "Unkeyed hashes of names, aliases, emails, commits, or ranges are forbidden",
            "full 40-character coordinator commit reachable from canonical `origin/main`",
            "Current named grants in `docs/PROVENANCE.md` are not silently converted",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, identity_policy)

        schema = json.loads(
            (ROOT / "governance/provenance-identities/schema-v1.json").read_text(
                encoding="utf-8"
            )
        )
        public_registry = json.loads(
            (ROOT / "governance/provenance-identities/registry.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(schema["properties"]["schema_version"]["const"], 1)
        self.assertTrue(public_registry["records"])
        self.assertTrue(
            all(record["synthetic"] for record in public_registry["records"])
        )
        fixtures = ROOT / "tests/fixtures/provenance-identities"
        self.assertEqual(len(list((fixtures / "positive").glob("*.json"))), 2)
        self.assertTrue(list((fixtures / "negative").glob("*.json")))

    def test_copyright_header_contract_is_complete(self) -> None:
        guide = " ".join(
            (ROOT / "AGENTS.md").read_text(encoding="utf-8").split()
        )
        contributing = " ".join(
            (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8").split()
        )

        for marker in {
            "On touch, refresh existing Atrinik-owned copyright terminal years",
            "blanket holders",
            "`CONTRIBUTING.md`",
            "preserve precise attribution",
        }:
            with self.subTest(surface="AGENTS.md", marker=marker):
                self.assertIn(marker, guide)

        for marker in {
            "Use `The Atrinik Project` as the exact collective holder",
            "already predominates in modern MIT source headers",
            "exact blanket format is `Copyright START[-END] The Atrinik Project`",
            "Only the surrounding comment delimiters vary by file format",
            "omit `(C)`, `(c)`, `©`, commas, and trailing punctuation",
            "migrate prospectively",
            "each existing Atrinik-owned copyright notice",
            "retain its original start year",
            "current calendar year",
            "Crossfire, Daimonin and other upstream notices",
            "Leave upstream and third-party notice years unchanged",
            "SPDX identifiers",
            "authoritative generator or template",
            "a separate legal and attribution surface",
        }:
            with self.subTest(surface="CONTRIBUTING.md", marker=marker):
                self.assertIn(marker, contributing)

        for example in {
            "Copyright 2021-2026 The Atrinik Project",
            "Copyright 2026 The Atrinik Project",
            "Copyright 2024-2026 The Atrinik Project",
            "Copyright (C) 2009-2026 Zoey Rose and Atrinik Development Team",
        }:
            with self.subTest(example=example):
                self.assertIn(example, contributing)

    def test_inventory_is_complete_and_within_budget(self) -> None:
        inventory = collect_inventory()
        self.assertEqual(inventory["summary"]["skill_count"], 10)
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
        self.assertEqual(inventory["summary"]["skill_count"], 10)

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

    def test_issue_delivery_skill_is_explicit_and_complete(self) -> None:
        skill = ROOT / ".agents/skills/atrinik-issue-delivery"
        package = {
            path.relative_to(skill).as_posix()
            for path in skill.rglob("*")
            if path.is_file()
        }
        self.assertEqual(
            package,
            {
                "SKILL.md",
                "agents/openai.yaml",
                "assets/deep-review-report.md",
                "references/deep-review-checklist.md",
            },
        )

        body = (skill / "SKILL.md").read_text(encoding="utf-8")
        interface = (skill / "agents/openai.yaml").read_text(encoding="utf-8")
        report = (skill / "assets/deep-review-report.md").read_text(
            encoding="utf-8"
        )
        checklist = (
            skill / "references/deep-review-checklist.md"
        ).read_text(encoding="utf-8")
        normalized_body = " ".join(body.split())
        normalized_interface = " ".join(interface.split())

        self.assertIn("never trigger implicitly", body.lower())
        self.assertIn("$atrinik-issue-delivery", body)
        self.assertIn("policy:\n  allow_implicit_invocation: false", interface)
        self.assertIn("$atrinik-issue-delivery", interface)
        self.assertIn('display_name: "Atrinik Issue or PR Delivery"', interface)
        self.assertIn(
            'short_description: "Deliver Atrinik issues or PRs to merge-ready"',
            interface,
        )
        for mutation in {
            "claim an explicitly authorized issue",
            "push ordinary branches",
            "create a draft PR in issue mode",
            "update the selected PR",
            "mark a draft ready after exit conditions",
            "post brief delivery comments",
        }:
            with self.subTest(mutation=mutation):
                self.assertIn(mutation, normalized_interface)
        for prohibited in {
            "do not create placeholder issues",
            "mutate incidental linked issues in PR-only mode",
            "force-push",
            "close",
            "merge",
            "self-approve",
            "bypass policy",
            "destructively reset",
            "retarget",
            "apply cleanup",
        }:
            with self.subTest(prohibited=prohibited):
                self.assertIn(prohibited, normalized_interface)

        for contract in {
            "exactly one type-explicit `ENTRY_MODE`",
            "bare `owner/repository#number` is ambiguous",
            "active PR already owns the work",
            "existing open, unmerged PR",
            "author, head repository",
            "reviews, conversations, checks, and mergeability",
            "Require a same-repository head",
            "zero or more incidental linked issues as read-only",
            "make no linked-issue or Project mutation",
            "A PR without an issue is a complete valid input",
            "--from BASE_SHA",
            "HEAD` equals `BASE_SHA",
            "--existing",
            "HEAD` equals the recorded `HEAD_SHA`, not `BASE_SHA`",
            "--base TARGET_BRANCH",
            "wrapper-owned",
            "never reconstruct, copy",
            "skipped/neutral checks",
            "required human",
            "concise PR update",
            "Reuse rather than recreate",
            "delivery profile selecting the final",
            "blocks merging, not the ready transition",
            "Never convert an already-ready PR to draft",
            "leave an already-ready PR ready",
            "issue-<number>.md",
            "pr-<number>.md",
            "only when issues actually exist",
            "do not fabricate placeholders",
        }:
            with self.subTest(contract=contract):
                self.assertIn(contract, normalized_body)

        self.assertIn("references/deep-review-checklist.md", body)
        self.assertIn("assets/deep-review-report.md", body)
        self.assertIn("| ID | Severity | Location |", report)
        self.assertIn("Entry mode: `<issue|PR>`", report)
        self.assertIn("Selected issue(s): `<URL(s) or none>`", report)
        self.assertIn("Claim/linkage state:", report)
        self.assertIn("## Scale and performance", checklist)
        self.assertIn("## Safety, security, and supply chain", checklist)
        self.assertIn("never fabricate missing issue requirements", checklist)
        self.assertIn("require no synthetic or broadened closing reference", checklist)

        root_guide = " ".join(
            (ROOT / "AGENTS.md").read_text(encoding="utf-8").split()
        )
        self.assertIn(
            "for an issue or existing PR; it stops before merge",
            root_guide,
        )

    def test_content_issue_delivery_uses_main_as_sole_authored_source(self) -> None:
        content = " ".join(
            (
                ROOT / ".agents/skills/atrinik-content-change/SKILL.md"
            ).read_text(encoding="utf-8").split()
        )
        delivery_root = ROOT / ".agents/skills/atrinik-issue-delivery"
        delivery = " ".join(
            (delivery_root / "SKILL.md").read_text(
                encoding="utf-8"
            ).split()
        )
        report = (delivery_root / "assets/deep-review-report.md").read_text(
            encoding="utf-8"
        )
        checklist = (
            delivery_root / "references/deep-review-checklist.md"
        ).read_text(encoding="utf-8")

        for marker in {
            "`content@main` is the sole authored source",
            "select the target-specific publisher",
            "never edit or create a PR against the retired `1.x` line",
        }:
            with self.subTest(surface="content", marker=marker):
                self.assertIn(marker, content)
        for marker in {
            "author only on `main`",
            "validate every affected target and consumer",
            "never publish a new `1.x` PR",
            "A content `main` PR may close an explicitly selected issue",
            "per-target bases",
        }:
            with self.subTest(surface="delivery", marker=marker):
                self.assertIn(marker, delivery)
        self.assertIn("| Release line / owner |", report)
        self.assertIn("cross-repository or cross-line", checklist)
        self.assertIn("Issue-closing path", report)
        self.assertIn("manual post-merge close", checklist)

    def test_program_delivery_is_explicit_and_merge_gated(self) -> None:
        skill = ROOT / ".agents/skills/atrinik-program-delivery"
        package = {
            path.relative_to(skill).as_posix()
            for path in skill.rglob("*")
            if path.is_file()
        }
        self.assertEqual(
            package,
            {
                "SKILL.md",
                "agents/openai.yaml",
                "assets/program-delivery-report.md",
                "references/program-review-checklist.md",
            },
        )

        body = (skill / "SKILL.md").read_text(encoding="utf-8")
        interface = (skill / "agents/openai.yaml").read_text(encoding="utf-8")
        report = (skill / "assets/program-delivery-report.md").read_text(
            encoding="utf-8"
        )
        checklist = (
            skill / "references/program-review-checklist.md"
        ).read_text(encoding="utf-8")
        normalized_body = " ".join(body.split())

        self.assertIn("$atrinik-program-delivery", body)
        self.assertIn(
            "../atrinik-issue-delivery/SKILL.md",
            body,
        )
        self.assertIn("policy:\n  allow_implicit_invocation: false", interface)
        self.assertIn("$atrinik-program-delivery", interface)
        self.assertIn('display_name: "Atrinik Program Delivery"', interface)
        self.assertIn(
            'short_description: "Deliver ordered Atrinik programs through merge gates"',
            interface,
        )
        for boundary in {
            "Do not infer merge authority",
            "Do not merge or close anything",
            "If and only if the user explicitly requested `/goal`",
            "Never create a nested or per-leaf goal",
            "A merge-ready leaf is progress, not goal completion",
            "Do not mark a draft ready until both its leaf review",
            "query GitHub rather than trusting the reported action",
            "prior green check proves only the old head/base combination",
            "follow the environment's active/blocked goal lifecycle",
            "exact ordered count of remaining squash commits",
            "any missing, extra, reordered, or out-of-allowlist commit",
            "Never require a commit to contain its own immutable hash",
            "then repeat the terminal audit",
            "master is genuinely ready to close",
        }:
            with self.subTest(boundary=boundary):
                self.assertIn(boundary, normalized_body)
        self.assertIn("references/program-review-checklist.md", body)
        self.assertIn("assets/program-delivery-report.md", body)
        self.assertIn("## Ordered stage matrix", report)
        self.assertIn("## Merge and human gate log", report)
        self.assertIn("Declared terminal suffix count/order by line", report)
        self.assertIn("Per-ordinal changed-path allowlists", report)
        self.assertIn("## Cross-change integration", checklist)
        self.assertIn("## Validation currency", checklist)
        self.assertIn("assign `zoeyrose`", body)
        self.assertIn("**Atrinik work**", body)
        self.assertIn("destination is ignored before writing", normalized_body)

        leaf_delivery = (
            ROOT / ".agents/skills/atrinik-issue-delivery/SKILL.md"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "program-delivery invocation delegates this contract",
            " ".join(leaf_delivery.split()),
        )
        self.assertIn(
            "delegates this contract only in issue mode",
            " ".join(leaf_delivery.split()),
        )
        self.assertIn("selecting its explicit issue mode", normalized_body)
        self.assertIn("does not delegate PR-mode adoption", normalized_body)
        self.assertIn(
            "program delegation does not authorize switching to PR mode",
            normalized_body,
        )

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

    def test_native_pr_stack_governance_is_guarded_and_complete(self) -> None:
        skill = ROOT / ".agents/skills/atrinik-github-governance"
        package = {
            path.relative_to(skill).as_posix()
            for path in skill.rglob("*")
            if path.is_file()
        }
        self.assertEqual(
            package,
            {
                "SKILL.md",
                "agents/openai.yaml",
                "references/pr-stack-review-and-merge.md",
            },
        )

        body = (skill / "SKILL.md").read_text(encoding="utf-8")
        interface = (skill / "agents/openai.yaml").read_text(encoding="utf-8")
        reference = (
            skill / "references/pr-stack-review-and-merge.md"
        ).read_text(encoding="utf-8")
        normalized = " ".join(reference.split())

        _, description = skill_frontmatter(skill / "SKILL.md")
        self.assertIn("review and explicitly merge native PR stacks", description)
        self.assertIn("references/pr-stack-review-and-merge.md", body)
        self.assertIn("Review-only work is read-only", body)
        self.assertIn("skip steps 3–6", body)
        self.assertNotIn("skip steps 2–5", body)
        self.assertIn("review a native PR stack", interface)
        self.assertIn("explicit user authority", interface)

        for marker in {
            "X-GitHub-Api-Version: 2026-03-10",
            "GET /repos/{owner}/{repo}/stacks?pull_request={number}",
            "GET /repos/{owner}/{repo}/stacks/{stack_number}",
            "Never infer membership from branch names",
            "same-repository linear dependency chains",
            "keep independent pull requests independent",
            "[Establish the exact native PR stack](#establish-the-exact-native-pr-stack)",
            "[Freeze and review both views](#freeze-and-review-both-views)",
            "[Require exact merge authority and current preflight](#require-exact-merge-authority-and-current-preflight)",
            "[Execute the guarded native atomic operation](#execute-the-guarded-native-atomic-operation)",
            "[Verify the result and preserve remaining work](#verify-the-result-and-preserve-remaining-work)",
            "[Read-only forward fixture](#read-only-forward-fixture)",
            "Define the selected portion as every position through the named top PR",
            "contiguous already-merged lower prefix",
            "every later selected layer must be open",
            "Reverify each prefix member's `merged_at`",
            "closed, reordered, missing, or inconsistent prefix",
            "The active suffix is the mutation scope",
            "With no merged prefix",
            "position 1 must target the frozen current trunk branch/SHA",
            "With a merged prefix, verify it from merge-result ancestry",
            "first active-suffix PR to target the frozen current trunk branch/SHA",
            "only each later active PR to base on the preceding active head",
            "cannot list unknown requests",
            "parent-to-head incremental diff",
            "trunk-to-selected-top cumulative diff",
            "deep-review checklist",
            "integration checklist",
            "Any push, rebase, retarget, membership change, lower-layer merge",
            "every required and applicable check exists and passes",
            "no self-approval is used",
            "actionable review conversation is resolved",
            "skipped-but-required",
            "GitHub CLI 2.97.0",
            "`github/gh-stack` v0.1.0",
            "does not send the asynchronous API's `sha` field",
            "repos/OWNER/REPOSITORY/pulls/TOP_PR/merge-async",
            "gh api --include --method PUT",
            "-f sha=REVIEWED_TOP_HEAD_SHA",
            "-f merge_method=squash",
            "-f merge_action=direct_merge",
            "`202 pending`",
            "`409` existing request",
            "Immediate `200 merged` or `200 enqueued`, or `400 failed`",
            "requested method/action/head",
            "Explicitly record a UUID as unavailable",
            "never invent absent response fields",
            "`403`, `404`, or `422`",
            "Do not resubmit without renewed authority, review, and preflight",
            "fixed upper bound",
            "gh api --include",
            "repos/OWNER/REPOSITORY/pulls/TOP_PR/merge-async/UUID",
            "separate the HTTP status from the body status",
            "For every poll, likewise separate and retain the included HTTP status",
            "body-level `pending` returned under polling HTTP `200`",
            "A timeout, transport loss, malformed response",
            "polling `403`, `404`, an expired result",
            "any unexpected non-`200` polling response",
            "whatever stack, selected and remaining PR, target ref/history",
            "stop with the UUID and exact response in a recovery handoff",
            "any later mutation requires refreshed authority, review, and preflight",
            "full authority, review, and preflight contract is refreshed",
            "Never emulate atomic merge with sequential `gh pr merge`",
            "For a partial-stack merge",
            "the previously merged prefix remains unchanged",
            "exactly the active suffix merged",
            "no unselected upper layer merged",
            "For every attempted operation, record in the handoff its HTTP status",
            "returned UUID or option fields only when supplied",
            "explicitly mark absent server fields unavailable",
            "For every terminal operation also record stack number and trunk",
            "stack number and trunk",
            "every selected PR's reviewed base/head and resulting squash SHA",
            "final target branch and tip",
            "Never call a merge endpoint in a forward test",
        }:
            with self.subTest(marker=marker):
                self.assertIn(marker, normalized)

        self.assertEqual(reference.count("gh api --include"), 2)

        for prohibited in {
            "Never force-push",
            "bypass or relax rules",
            "enable auto-merge",
            "close issues manually",
            "apply cleanup",
            "blindly resubmit",
        }:
            with self.subTest(prohibited=prohibited):
                self.assertIn(prohibited, normalized)

        self.assertIn(
            "Merge-ready handoffs from issue or program delivery are inputs",
            normalized,
        )
        self.assertNotIn("stacked pull request", reference.lower())
        terminology = "\n".join(
            path.read_text(encoding="utf-8")
            for path in {
                ROOT / "AGENTS.md",
                skill / "SKILL.md",
                skill / "agents/openai.yaml",
                skill / "references/pr-stack-review-and-merge.md",
                ROOT / "atrinik_workspace/guidance_inventory.py",
            }
        )
        self.assertNotIn("PR-stack", terminology)
        self.assertIn(
            "During every standalone PR stack review phase",
            normalized,
        )
        self.assertIn("including review under an explicit merge request", normalized)
        self.assertIn("do not create or update any report or other file", normalized)
        self.assertIn("Only a separately write-authorized delivery", normalized)
        self.assertIn(
            "In every standalone PR stack review phase, reuse only their review",
            normalized,
        )
        self.assertIn(
            "this contract overrides the deep-review checklist's report-instantiation",
            normalized,
        )
        self.assertIn("Keep that evidence in memory and the response", normalized)
        issue_delivery = (
            ROOT / ".agents/skills/atrinik-issue-delivery/SKILL.md"
        ).read_text(encoding="utf-8")
        program_delivery = (
            ROOT / ".agents/skills/atrinik-program-delivery/SKILL.md"
        ).read_text(encoding="utf-8")
        self.assertIn("It does not authorize", issue_delivery)
        self.assertIn("merges", issue_delivery)
        self.assertIn("Do not infer merge authority", program_delivery)
        self.assertNotIn("merge-async", issue_delivery)
        self.assertNotIn("merge-async", program_delivery)

    def test_pull_request_publication_contract_is_synchronized(self) -> None:
        root_guide = ROOT / "AGENTS.md"
        contributing = ROOT / "CONTRIBUTING.md"
        governance_skill = (
            ROOT / ".agents/skills/atrinik-github-governance/SKILL.md"
        )
        workspace_skill = (
            ROOT / ".agents/skills/atrinik-multi-repo-workspace/SKILL.md"
        )
        governed = [
            root_guide,
            contributing,
            governance_skill,
            workspace_skill,
        ]
        markers = {
            "type(optional-scope)!: concise description",
            "GitHub-Flavored Markdown",
            "actual line breaks",
            "literal `\\n` separators",
            "multi-section",
        }
        for path in governed:
            guidance = " ".join(path.read_text(encoding="utf-8").split())
            for marker in markers:
                with self.subTest(path=path.relative_to(ROOT), marker=marker):
                    self.assertIn(marker, guidance)
            with self.subTest(path=path.relative_to(ROOT), marker="body input"):
                self.assertRegex(
                    guidance,
                    r"multi-section bod(?:y|ies)[^.]{0,200}file"
                    r"[^.]{0,200}(?:standard input|stdin)",
                )
            with self.subTest(path=path.relative_to(ROOT), marker="remote render"):
                self.assertRegex(
                    guidance,
                    r"[Aa]fter (?:create/edit|creating or editing a pull request)"
                    r"[^.]{0,160}(?:inspect|verify)[^.]{0,80}(?:remote|GitHub)",
                )

        for path in [contributing, governance_skill]:
            guidance = " ".join(path.read_text(encoding="utf-8").split())
            for marker in {
                "headings",
                "lists",
                "inline code",
                "issue-closing references",
                "validation sections",
                "bodyHTML",
                "raw body",
            }:
                with self.subTest(path=path.relative_to(ROOT), marker=marker):
                    self.assertIn(marker, guidance)

        title_workflow = (ROOT / ".github/workflows/pr-title.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("pull_request_target:", title_workflow)
        self.assertIn(
            "types: [opened, edited, synchronize, reopened]", title_workflow
        )
        self.assertIn("name: Conventional PR title", title_workflow)
        self.assertIn("type(optional-scope)!: concise description", title_workflow)

        run_match = re.search(
            r"(?m)^ {8}run: \|\n(?P<script>(?:^ {10}.*(?:\n|$))+)",
            title_workflow,
        )
        if run_match is None:
            self.fail("PR title workflow does not declare its validation script")
        validation_script = "\n".join(
            line[10:] for line in run_match.group("script").splitlines()
        )
        title_cases = {
            True: {
                "chore: refresh guidance",
                "docs(agents): govern PR publication",
                "feat!: revise the contract",
            },
            False: {
                "Docs: uppercase type",
                "docs(): empty scope",
                "docs: ",
                "update guidance",
            },
        }
        for should_match, titles in title_cases.items():
            for title in titles:
                with self.subTest(title=title, should_match=should_match):
                    result = subprocess.run(
                        ["bash", "-c", validation_script],
                        check=False,
                        capture_output=True,
                        env={"PR_TITLE": title},
                        text=True,
                    )
                    self.assertEqual(
                        result.returncode == 0,
                        should_match,
                        result.stderr,
                    )

        _, governance_description = skill_frontmatter(governance_skill)
        self.assertIn("Publish Atrinik PRs", governance_description)
        governance_interface = (
            governance_skill.parent / "agents/openai.yaml"
        ).read_text(encoding="utf-8")
        self.assertIn("Publish PRs", governance_interface)
        self.assertIn("$atrinik-github-governance", governance_interface)

        unrelated = [
            path
            for path in (ROOT / ".agents/skills").glob("*/SKILL.md")
            if path not in governed
        ]
        for path in unrelated:
            with self.subTest(path=path.relative_to(ROOT)):
                guidance = " ".join(path.read_text(encoding="utf-8").split())
                self.assertFalse(
                    all(marker in guidance for marker in markers),
                    "unrelated skill duplicates the complete PR publication contract",
                )


if __name__ == "__main__":
    unittest.main()
