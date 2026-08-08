from __future__ import annotations

import copy
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import Manifest, WorkspaceError
from atrinik_workspace.supply_chain import (
    ACTION_REFERENCE_PATTERN,
    CHECKOUT_METADATA_REQUIRED_FILES,
    DOCKER_PULL_PATTERN,
    Dependency,
    Evidence,
    Inventory,
    Repository,
    _audit_files,
    _container_references,
    _component_source_root,
    _cyclonedx_type,
    _git_repository_coordinate,
    _git_repository_branch_compatible,
    _git_head,
    _is_container_input,
    _is_dependency_input,
    _read_metadata,
    _relative_path,
    _safe_repository_path,
    _static_matrix_values,
    _string_array,
    _system_package_versions,
    _text,
    _tracked_files,
    _validate_container_reference,
    _workflow_job_blocks,
    _workflow_runners,
    _workflow_uses_unpinned_npx,
    repository_roots,
    report_component_commits,
    version_report,
    write_generated,
)


ROOT = Path(__file__).resolve().parents[1]


def fixture_dependency(**changes: object) -> Dependency:
    values: dict[str, object] = {
        "identifier": "action/example",
        "name": "Example action",
        "kind": "github-action",
        "owner": "fixture",
        "scope": ("fixture",),
        "required": True,
        "version": "v1",
        "version_source": "release tag",
        "license": "MIT",
        "source_url": "https://github.com/example/action",
        "locator": "example/action",
        "commit": "a" * 40,
        "checksum": None,
        "acquisition": "GitHub Actions",
        "update_cadence_days": 7,
        "update_mechanism": "Dependabot",
        "eol_response": "Replace it",
        "validation": "Run CI",
        "disposition": "retain",
        "packages": (),
        "evidence": (
            Evidence(
                "fixture",
                ".github/workflows/ci.yml",
                f"example/action@{'a' * 40} # v1",
            ),
        ),
    }
    values.update(changes)
    return Dependency(**values)


def fixture_repository(**changes: object) -> Repository:
    values: dict[str, object] = {
        "name": "fixture",
        "repository": "atrinik/fixture",
        "branch": "main",
        "checkout": "fixture",
        "source": ".",
        "cohorts": ("default",),
        "stacks": ("default",),
        "roles": ("fixture",),
        "license": "MIT",
        "commit": None,
        "supported": True,
        "audit_ready": True,
        "audit_mode": "full",
        "role": "Fixture repository",
    }
    values.update(changes)
    return Repository(**values)


class InventoryTests(unittest.TestCase):
    def load_inventory(self) -> Inventory:
        return Inventory.load(
            ROOT / "supply-chain" / "inventory.json", ROOT / "components.json"
        )

    def inventory_document(self) -> dict[str, object]:
        return json.loads(
            (ROOT / "supply-chain" / "inventory.json").read_text(encoding="utf-8")
        )

    def test_scheduled_audit_covers_both_content_lines_and_stacks(self) -> None:
        workflow = (ROOT / ".github/workflows/supply-chain.yml").read_text(
            encoding="utf-8"
        )

        self.assertIn(
            "repository: atrinik/content\n          path: content\n",
            workflow,
        )
        self.assertIn(
            "repository: atrinik/content\n          ref: 1.x\n"
            "          path: content-1x\n",
            workflow,
        )
        self.assertEqual(workflow.count("repository: atrinik/classic\n"), 1)
        self.assertNotIn("repository: atrinik/legacy-", workflow)
        for profile in ("default", "classic"):
            self.assertIn(
                f"./atrinik supply-chain audit --profile {profile}", workflow
            )
            for report in ("licenses.md", "cyclonedx.json", "spdx.json"):
                self.assertIn(
                    f"build/supply-chain/{profile}/{report}", workflow
                )

    def assert_invalid_document(
        self, document: object, expected: str
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "inventory.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, expected):
                Inventory.load(path, ROOT / "components.json")

    def make_audit_repository(self, root: Path) -> None:
        (root / ".github" / "workflows").mkdir(parents=True)
        (root / ".github" / "dependabot.yml").write_text(
            "updates:\n  - package-ecosystem: github-actions\n",
            encoding="utf-8",
        )
        (root / ".github" / "workflows" / "ci.yml").write_text(
            f"runs-on: ubuntu-24.04\nuses: example/action@{'a' * 40} # v1\n",
            encoding="utf-8",
        )
        subprocess.run(["git", "init", "-q", str(root)], check=True)

    def audit_inventory(self) -> Inventory:
        runner = fixture_dependency(
            identifier="runner/ubuntu-24.04",
            name="Ubuntu runner",
            kind="toolchain",
            locator="github-hosted-runner/ubuntu-24.04",
            commit=None,
            evidence=(
                Evidence(
                    "fixture", ".github/workflows/ci.yml", "runs-on: ubuntu-24.04"
                ),
            ),
        )
        return Inventory(
            "atrinik",
            "2026-08-07T00:00:00Z",
            [fixture_repository()],
            [fixture_dependency(), runner],
        )

    def test_inventory_and_schema_validate(self) -> None:
        inventory = self.load_inventory()
        inventory.validate_schema(ROOT / "supply-chain" / "schema.json")

        self.assertGreaterEqual(len(inventory.dependencies), 60)
        self.assertIn("nawerhals", inventory.repositories_by_name)
        self.assertFalse(inventory.repositories_by_name["nawerhals"].supported)
        self.assertEqual(
            inventory.repositories_by_name["content"].repository,
            inventory.repositories_by_name["content-1x"].repository,
        )
        self.assertNotEqual(
            inventory.repositories_by_name["content"].branch,
            inventory.repositories_by_name["content-1x"].branch,
        )
        classic = [
            inventory.repositories_by_name[name]
            for name in (
                "classic-client",
                "classic-editor",
                "classic-libatrinik",
                "classic-protocol",
                "classic-server",
            )
        ]
        self.assertEqual({repository.repository for repository in classic}, {"atrinik/classic"})
        self.assertEqual({repository.checkout for repository in classic}, {"classic"})
        self.assertTrue(all(repository.audit_mode == "full" for repository in classic))
        self.assertEqual(
            {repository.license for repository in classic},
            {"GPL-2.0-or-later"},
        )
        self.assertEqual(
            {repository.source for repository in classic},
            {"client", "editor", "libatrinik", "protocol", "server"},
        )
        aggregate = inventory.repositories_by_name["classic"]
        self.assertEqual(aggregate.repository, "atrinik/classic")
        self.assertEqual(aggregate.checkout, "classic")
        self.assertEqual(aggregate.source, ".")
        self.assertEqual(aggregate.roles, ("checkout-metadata",))
        self.assertEqual(aggregate.audit_mode, "metadata")
        self.assertNotIn("libatrinik", inventory.repositories_by_name)
        self.assertTrue(
            all(
                repository.commit is None
                for repository in inventory.repositories
                if repository.supported
            )
        )
        self.assertFalse(
            any(
                dependency.owner == "libatrinik"
                or "libatrinik" in dependency.scope
                or any(
                    evidence.repository == "libatrinik"
                    for evidence in dependency.evidence
                )
                for dependency in inventory.dependencies
            )
        )

    def test_inventory_rejects_invalid_root_and_repository_metadata(self) -> None:
        self.assert_invalid_document([], "root must be an object")

        mutations = [
            ("schema_version", 2, "unsupported.*schema"),
            ("organization", "other", "organization must be atrinik"),
            ("created", " padded ", "non-empty trimmed string"),
            ("repositories", [], "non-empty array"),
        ]
        for field, value, expected in mutations:
            with self.subTest(field=field):
                document = self.inventory_document()
                document[field] = value
                self.assert_invalid_document(document, expected)

        document = self.inventory_document()
        document["repositories"][0] = "client"
        self.assert_invalid_document(document, "repository 0 must be an object")

        repository_mutations = [
            ("name", "Client", "lowercase identifier"),
            ("repository", "example/client", "Atrinik repository"),
            ("branch", " padded ", "trimmed string"),
            ("checkout", "Bad Checkout", "lowercase identifier"),
            ("source", "../client", "safe repository-relative"),
            ("cohorts", ["z", "default"], "sorted and unique"),
            ("stacks", ["default", "classic"], "sorted and unique"),
            ("roles", [], "non-empty array"),
            ("license", "not a license!", "license is not an SPDX"),
            ("commit", "A" * 40, "full lowercase Git commit"),
            ("supported", "true", "must be a boolean"),
            ("audit_ready", "true", "must be a boolean"),
            ("audit_mode", "partial", "must be one of"),
            ("role", "", "non-empty trimmed string"),
        ]
        for field, value, expected in repository_mutations:
            with self.subTest(repository_field=field):
                document = self.inventory_document()
                document["repositories"][0][field] = value
                self.assert_invalid_document(document, expected)

        document = self.inventory_document()
        document["repositories"][1] = copy.deepcopy(document["repositories"][0])
        self.assert_invalid_document(document, "duplicate inventory repository")

        document = self.inventory_document()
        document["repositories"][0]["supported"] = False
        self.assert_invalid_document(document, "do not match components.json")

        document = self.inventory_document()
        document["repositories"][0]["repository"] = "atrinik/not-client"
        self.assert_invalid_document(document, "repository metadata mismatch")

        document = self.inventory_document()
        document["repositories"][1]["branch"] = "next"
        self.assert_invalid_document(document, "repository metadata mismatch")

        document = self.inventory_document()
        document["repositories"][0]["commit"] = "a" * 40
        self.assert_invalid_document(document, "must be null for a moving supported branch")

    def test_inventory_rejects_invalid_dependency_metadata(self) -> None:
        document = self.inventory_document()
        document["dependencies"] = []
        self.assert_invalid_document(document, "dependencies must be a non-empty array")

        document = self.inventory_document()
        document["dependencies"][0] = "dependency"
        self.assert_invalid_document(document, "dependency 0 must be an object")

        dependency_mutations = [
            ("id", "Action/example", "lowercase identifier"),
            ("name", "", "non-empty trimmed string"),
            ("kind", "mystery", "kind is unsupported"),
            ("owner", "unknown", "owner is not an inventory repository"),
            ("scope", [], "must be a non-empty array"),
            ("scope", ["unknown"], "scope contains an unknown repository"),
            ("scope", ["server", "client"], "scope must be sorted"),
            ("required", 1, "required must be a boolean"),
            ("license", "not a license!", "license is not an SPDX"),
            ("source_url", "http://example.com", "source_url must use HTTPS"),
            ("commit", "A" * 40, "full lowercase Git commit"),
            ("checksum", "sha256:abc", "lowercase SHA-256"),
            ("update_cadence_days", True, "between 1 and 366"),
            ("update_cadence_days", 367, "between 1 and 366"),
            ("disposition", "adopt", "disposition is unsupported"),
            ("packages", ["z", "a"], "packages must be sorted and unique"),
            ("packages", ["a", "a"], "packages must be sorted and unique"),
            ("evidence", [], "evidence must be a non-empty array"),
        ]
        for field, value, expected in dependency_mutations:
            with self.subTest(dependency_field=field, value=value):
                document = self.inventory_document()
                document["dependencies"][0][field] = value
                self.assert_invalid_document(document, expected)

        document = self.inventory_document()
        document["dependencies"][1]["id"] = document["dependencies"][0]["id"]
        self.assert_invalid_document(document, "duplicate dependency id")

        document = self.inventory_document()
        document["dependencies"][0], document["dependencies"][1] = (
            document["dependencies"][1],
            document["dependencies"][0],
        )
        self.assert_invalid_document(document, "dependencies must be sorted")

        document = self.inventory_document()
        actions = [
            dependency
            for dependency in document["dependencies"]
            if dependency["kind"] == "github-action"
        ]
        actions[1]["locator"] = actions[0]["locator"]
        self.assert_invalid_document(document, "Action locators must be unique")

    def test_inventory_rejects_invalid_dependency_evidence_and_actions(self) -> None:
        evidence_mutations = [
            (None, "must be an object"),
            ({"repository": "unknown", "path": "x", "contains": "x"}, "is unknown"),
            ({"repository": "client", "path": "../x", "contains": "x"}, "safe repository-relative"),
        ]
        for evidence, expected in evidence_mutations:
            with self.subTest(evidence=evidence):
                document = self.inventory_document()
                document["dependencies"][0]["evidence"][0] = evidence
                self.assert_invalid_document(document, expected)

        document = self.inventory_document()
        original = copy.deepcopy(document["dependencies"][0]["evidence"][0])
        document["dependencies"][0]["evidence"] = [original, original]
        self.assert_invalid_document(document, "evidence must be sorted and unique")

        document = self.inventory_document()
        document["dependencies"][0]["scope"] = ["server"]
        self.assert_invalid_document(document, "evidence repository must be present")

        document = self.inventory_document()
        document["dependencies"][0]["commit"] = None
        self.assert_invalid_document(document, "GitHub Actions require a commit")

        document = self.inventory_document()
        document["dependencies"][0]["locator"] = "not-an-action"
        self.assert_invalid_document(document, "owner/action coordinate")

    def test_schema_contract_rejects_weak_or_unexpected_schema(self) -> None:
        inventory = self.load_inventory()
        schema = json.loads(
            (ROOT / "supply-chain" / "schema.json").read_text(encoding="utf-8")
        )
        repository_schema = schema["properties"]["repositories"]["items"]
        self.assertFalse(repository_schema["additionalProperties"])
        self.assertIn("commit", repository_schema["required"])
        self.assertIn("audit_mode", repository_schema["required"])
        self.assertEqual(
            repository_schema["properties"]["audit_mode"]["enum"],
            ["full", "metadata"],
        )
        self.assertEqual(
            repository_schema["properties"]["commit"]["type"],
            ["string", "null"],
        )
        self.assertIsNone(
            repository_schema["allOf"][0]["then"]["properties"]["commit"]["const"]
        )
        cases = [
            ([], "schema root must be an object"),
            ({**schema, "$schema": "draft-07"}, "draft 2020-12"),
            ({**schema, "$id": "https://example.com/schema"}, "unexpected \\$id"),
            ({**schema, "additionalProperties": True}, "reject additional"),
        ]
        for value, expected in cases:
            with self.subTest(expected=expected):
                with tempfile.TemporaryDirectory() as temporary:
                    path = Path(temporary) / "schema.json"
                    path.write_text(json.dumps(value), encoding="utf-8")
                    with self.assertRaisesRegex(WorkspaceError, expected):
                        inventory.validate_schema(path)

    def test_wrapper_dependency_surface_audits_independently(self) -> None:
        messages = self.load_inventory().audit(
            {"atrinik": ROOT}, require_all=False
        )

        self.assertEqual(len(messages), 1)
        self.assertIn("action references", messages[0])

    def test_minimal_repository_audit_covers_actions_and_runners(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_audit_repository(root)
            workflow = root / ".github" / "workflows" / "ci.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8")
                + "- uses: ./.github/actions/local\n",
                encoding="utf-8",
            )

            messages = self.audit_inventory().audit({"fixture": root})

        self.assertEqual(len(messages), 1)
        self.assertIn("1 action references", messages[0])

    def test_monorepo_logical_sources_ignore_inert_github_workflows(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in CHECKOUT_METADATA_REQUIRED_FILES:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("metadata\n", encoding="utf-8")
            (root / ".github" / "dependabot.yml").write_text(
                "updates:\n  - package-ecosystem: github-actions\n",
                encoding="utf-8",
            )
            (root / ".github" / "workflows" / "check.yml").write_text(
                f"runs-on: ubuntu-24.04\n"
                f"uses: example/action@{'a' * 40} # v1\n",
                encoding="utf-8",
            )
            nested = root / "client" / ".github"
            (nested / "workflows").mkdir(parents=True)
            (nested / "dependabot.yml").write_text(
                "updates: []\n", encoding="utf-8"
            )
            (nested / "workflows" / "ci.yml").write_text(
                "runs-on: retired-runner\nuses: retired/action@v1\n",
                encoding="utf-8",
            )
            subprocess.run(["git", "init", "-q", str(root)], check=True)

            aggregate = fixture_repository(
                name="classic",
                repository="atrinik/classic",
                checkout="classic",
                roles=("checkout-metadata",),
                audit_mode="metadata",
            )
            logical = fixture_repository(
                name="classic-client",
                repository="atrinik/classic",
                checkout="classic",
                source="client",
                roles=("client",),
            )
            action = fixture_dependency(
                owner="classic",
                scope=("classic",),
                evidence=(
                    Evidence(
                        "classic",
                        ".github/workflows/check.yml",
                        f"example/action@{'a' * 40} # v1",
                    ),
                ),
            )
            runner = fixture_dependency(
                identifier="runner/ubuntu-24.04",
                name="Ubuntu runner",
                kind="toolchain",
                owner="classic",
                scope=("classic",),
                locator="github-hosted-runner/ubuntu-24.04",
                commit=None,
                evidence=(
                    Evidence(
                        "classic",
                        ".github/workflows/check.yml",
                        "runs-on: ubuntu-24.04",
                    ),
                ),
            )
            inventory = Inventory(
                "atrinik",
                "2026-08-08T00:00:00Z",
                [aggregate, logical],
                [action, runner],
            )

            messages = inventory.audit(
                {"classic": root, "classic-client": root / "client"}
            )
            inert_evidence = fixture_dependency(
                identifier="tool/inert-workflow",
                name="Inert workflow evidence",
                kind="toolchain",
                owner="classic-client",
                scope=("classic-client",),
                locator="pkg:generic/inert-workflow",
                commit=None,
                evidence=(
                    Evidence(
                        "classic-client",
                        ".github/workflows/ci.yml",
                        "retired/action@v1",
                    ),
                ),
            )
            with self.assertRaisesRegex(
                WorkspaceError, "inert nested GitHub metadata"
            ):
                Inventory(
                    "atrinik",
                    "2026-08-08T00:00:00Z",
                    [aggregate, logical],
                    [action, runner, inert_evidence],
                ).audit({"classic": root, "classic-client": root / "client"})
            (root / "client" / "package.json").write_text(
                "{}\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(
                WorkspaceError, "classic-client/package.json.*absent"
            ):
                inventory.audit(
                    {"classic": root, "classic-client": root / "client"}
                )

        self.assertEqual(len(messages), 2)
        self.assertIn("1 action references", messages[0])
        self.assertIn("0 action references", messages[1])

    def test_audit_rejects_invalid_roots_and_evidence(self) -> None:
        inventory = self.audit_inventory()
        with self.assertRaisesRegex(WorkspaceError, "roots are incomplete"):
            inventory.audit({})
        with self.assertRaisesRegex(WorkspaceError, "unknown.*repositories"):
            inventory.audit({"unknown": ROOT}, require_all=False)
        not_ready = Inventory(
            inventory.organization,
            inventory.created,
            [replace(inventory.repositories[0], audit_ready=False)],
            inventory.dependencies,
        )
        with self.assertRaisesRegex(WorkspaceError, "unknown.*repositories"):
            not_ready.audit({"fixture": ROOT})

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            file_path = root / "not-a-directory"
            file_path.write_text("x", encoding="utf-8")
            with self.assertRaisesRegex(WorkspaceError, "not a directory"):
                inventory.audit({"fixture": file_path})

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_audit_repository(root)
            broken = Inventory(
                inventory.organization,
                inventory.created,
                inventory.repositories,
                [
                    replace(
                        inventory.dependencies[0],
                        evidence=(
                            Evidence(
                                "fixture", ".github/workflows/ci.yml", "not present"
                            ),
                        ),
                    ),
                    inventory.dependencies[1],
                ],
            )
            with self.assertRaisesRegex(WorkspaceError, "expected text is absent"):
                broken.audit({"fixture": root})

    def test_audit_rejects_unmanaged_repository_inputs(self) -> None:
        inventory = self.audit_inventory()
        cases = [
            ("missing-dependabot", "missing .github/dependabot.yml"),
            ("bad-dependabot", "does not own GitHub Actions"),
            ("submodule", "Git submodules are not supported"),
            ("dependency-input", "dependency input is absent"),
        ]
        for case, expected in cases:
            with self.subTest(case=case):
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    self.make_audit_repository(root)
                    if case == "missing-dependabot":
                        (root / ".github" / "dependabot.yml").unlink()
                    elif case == "bad-dependabot":
                        (root / ".github" / "dependabot.yml").write_text(
                            "updates: []\n", encoding="utf-8"
                        )
                    elif case == "submodule":
                        (root / ".gitmodules").write_text("[submodule]\n", encoding="utf-8")
                    else:
                        (root / "package.json").write_text("{}\n", encoding="utf-8")
                    with self.assertRaisesRegex(WorkspaceError, expected):
                        inventory.audit({"fixture": root})

    def test_audit_rejects_unowned_or_incorrect_action_references(self) -> None:
        inventory = self.audit_inventory()
        semantic_inventory = Inventory(
            inventory.organization,
            inventory.created,
            inventory.repositories,
            [
                replace(
                    dependency,
                    evidence=(
                        Evidence(
                            "fixture",
                            ".github/dependabot.yml",
                            "package-ecosystem: github-actions",
                        ),
                    ),
                )
                for dependency in inventory.dependencies
            ],
        )
        cases = [
            (f"uses: unknown/action@{'a' * 40} # v1", "unowned GitHub Action"),
            ("uses: example/action@v1 # v1", "not pinned to a full commit"),
            (f"uses: example/action@{'b' * 40} # v1", "differs from inventory"),
            (f"uses: example/action@{'a' * 40} # v2", "updater hint"),
            ("runs-on: mystery-runner", "unowned workflow runner"),
            ("uses: ${{ matrix.action }}", "unsupported external Action"),
            ("uses: ./../outside", "local Action escapes"),
            ("runs-on: ${{ matrix.os }}", "runner matrix os has no static literals"),
        ]
        for line, expected in cases:
            with self.subTest(expected=expected):
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    self.make_audit_repository(root)
                    workflow = root / ".github" / "workflows" / "ci.yml"
                    original = workflow.read_text(encoding="utf-8")
                    if line.startswith("runs-on"):
                        original = original.replace("runs-on: ubuntu-24.04", line)
                    else:
                        original = original.replace(
                            f"uses: example/action@{'a' * 40} # v1", line
                        )
                    workflow.write_text(original, encoding="utf-8")
                    with self.assertRaisesRegex(WorkspaceError, expected):
                        semantic_inventory.audit({"fixture": root})

    def test_audit_rejects_inventory_scope_drift(self) -> None:
        inventory = self.audit_inventory()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_audit_repository(root)
            workflow = root / ".github" / "workflows" / "ci.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8").replace("uses:", "use:"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(WorkspaceError, "scope differs from audited use"):
                inventory.audit({"fixture": root})

        runner_only_drift = Inventory(
            inventory.organization,
            inventory.created,
            inventory.repositories,
            [
                inventory.dependencies[0],
                replace(
                    inventory.dependencies[1],
                    evidence=(
                        Evidence(
                            "fixture",
                            ".github/dependabot.yml",
                            "package-ecosystem: github-actions",
                        ),
                    ),
                ),
            ],
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_audit_repository(root)
            workflow = root / ".github" / "workflows" / "ci.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8").replace(
                    "runs-on: ubuntu-24.04\n", ""
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(WorkspaceError, "scope differs from audited use"):
                runner_only_drift.audit({"fixture": root})

    def test_audit_rejects_action_and_runner_uses_outside_declared_scope(self) -> None:
        inventory = self.audit_inventory()
        common_evidence = (
            Evidence(
                "fixture",
                ".github/dependabot.yml",
                "package-ecosystem: github-actions",
            ),
        )
        cases = [
            (
                [
                    replace(
                        inventory.dependencies[0],
                        scope=("other",),
                        evidence=common_evidence,
                    ),
                    inventory.dependencies[1],
                ],
                "example/action is absent from inventory scope",
            ),
            (
                [
                    inventory.dependencies[0],
                    replace(
                        inventory.dependencies[1],
                        scope=("other",),
                        evidence=common_evidence,
                    ),
                ],
                "ubuntu-24.04 is absent from inventory scope",
            ),
        ]
        for dependencies, expected in cases:
            with self.subTest(expected=expected):
                scoped = Inventory(
                    inventory.organization,
                    inventory.created,
                    inventory.repositories,
                    dependencies,
                )
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    self.make_audit_repository(root)
                    with self.assertRaisesRegex(WorkspaceError, expected):
                        scoped.audit({"fixture": root})

    def test_audit_validates_literal_workflow_container_pulls(self) -> None:
        inventory = self.audit_inventory()
        image = f"ghcr.io/atrinik/build:1@sha256:{'b' * 64}"
        container = fixture_dependency(
            identifier="container/build",
            kind="container-image",
            locator="ghcr.io/atrinik/build",
            commit=None,
            checksum="sha256:" + "b" * 64,
            evidence=(Evidence("fixture", ".github/workflows/ci.yml", image),),
        )
        audited = Inventory(
            inventory.organization,
            inventory.created,
            inventory.repositories,
            [*inventory.dependencies, container],
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_audit_repository(root)
            workflow = root / ".github" / "workflows" / "ci.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8")
                + f"run: docker pull {image}\n"
                + f"- uses: docker://{image}\n",
                encoding="utf-8",
            )

            self.assertEqual(len(audited.audit({"fixture": root})), 1)

    def test_reports_are_deterministic_and_well_formed(self) -> None:
        inventory = self.load_inventory()
        first = inventory.report("cyclonedx")
        second = inventory.report("cyclonedx")
        cyclonedx = json.loads(first)
        spdx = json.loads(inventory.report("spdx"))

        self.assertEqual(first, second)
        self.assertEqual(cyclonedx["specVersion"], "1.6")
        first_party = [
            repository for repository in inventory.repositories if repository.supported
        ]
        self.assertEqual(
            len(cyclonedx["components"]),
            len(inventory.dependencies) + len(first_party),
        )
        self.assertEqual(spdx["spdxVersion"], "SPDX-2.3")
        self.assertTrue(
            any(
                property_["name"] == "atrinik:declared-packages"
                for component in cyclonedx["components"]
                for property_ in component["properties"]
            )
        )
        self.assertEqual(
            sum(
                package["supplier"] == "Organization: Atrinik"
                for package in spdx["packages"]
            ),
            len(first_party),
        )
        content = next(
            component
            for component in cyclonedx["components"]
            if component["bom-ref"] == "atrinik:component:content-1x"
        )
        properties = {
            property_["name"]: property_["value"]
            for property_ in content["properties"]
        }
        self.assertEqual(properties["atrinik:branch"], "1.x")
        self.assertEqual(properties["atrinik:commit"], "unavailable")
        self.assertEqual(content["version"], "unavailable")
        self.assertEqual(properties["atrinik:stacks"], "classic")
        self.assertEqual(properties["atrinik:roles"], "content")
        spdx_content = next(
            package
            for package in spdx["packages"]
            if package["name"] == "content-1x"
        )
        self.assertIn("repository: atrinik/content", spdx_content["packageComment"])
        self.assertIn("branch: 1.x", spdx_content["packageComment"])
        self.assertIn("stacks: classic", spdx_content["packageComment"])
        self.assertIn("commit: unavailable", spdx_content["packageComment"])
        self.assertIn("| Dependency | Version |", inventory.report("licenses"))
        self.assertIn("| Component | Repository | Branch |", inventory.report("licenses"))

    def test_reports_resolve_profile_commits_without_conflating_content_branches(self) -> None:
        inventory = self.load_inventory()
        default_commit = "a" * 40
        classic_commit = "b" * 40
        root_commit = "c" * 40
        default = json.loads(
            inventory.report(
                "cyclonedx",
                {"atrinik": root_commit, "content": default_commit},
                "default",
            )
        )
        classic = json.loads(
            inventory.report(
                "cyclonedx",
                {"atrinik": root_commit, "content-1x": classic_commit},
                "classic",
            )
        )

        def properties(document: dict[str, object], name: str) -> dict[str, str]:
            component = next(
                item
                for item in document["components"]
                if item["bom-ref"] == f"atrinik:component:{name}"
            )
            return {
                item["name"]: item["value"] for item in component["properties"]
            }

        default_content = properties(default, "content")
        default_classic_content = properties(default, "content-1x")
        classic_content = properties(classic, "content-1x")
        classic_default_content = properties(classic, "content")
        self.assertEqual(default_content["atrinik:commit"], default_commit)
        self.assertEqual(default_content["atrinik:selected"], "true")
        self.assertEqual(default_classic_content["atrinik:commit"], "unavailable")
        self.assertEqual(default_classic_content["atrinik:selected"], "false")
        self.assertEqual(classic_content["atrinik:commit"], classic_commit)
        self.assertEqual(classic_content["atrinik:selected"], "true")
        self.assertEqual(classic_default_content["atrinik:commit"], "unavailable")
        self.assertEqual(classic_default_content["atrinik:selected"], "false")

        spdx = json.loads(
            inventory.report(
                "spdx",
                {"atrinik": root_commit, "content-1x": classic_commit},
                "classic",
            )
        )
        content_package = next(
            package for package in spdx["packages"] if package["name"] == "content-1x"
        )
        self.assertEqual(content_package["versionInfo"], classic_commit)
        self.assertEqual(
            content_package["externalRefs"][0]["referenceLocator"], classic_commit
        )

        with self.assertRaisesRegex(WorkspaceError, "unknown first-party"):
            inventory.report("spdx", {"unknown": root_commit}, "classic")
        with self.assertRaisesRegex(WorkspaceError, "outside the selected stack"):
            inventory.report("spdx", {"content": default_commit}, "classic")
        with self.assertRaisesRegex(WorkspaceError, "full lowercase Git commit"):
            inventory.report("spdx", {"content-1x": "short"}, "classic")
        with self.assertRaisesRegex(WorkspaceError, "unknown.*report stack"):
            inventory.report("spdx", {}, "mixed")

    def test_reports_cover_license_references_checksums_and_invalid_formats(self) -> None:
        license_reference = fixture_dependency(
            identifier="source/example",
            kind="source-archive",
            license="LicenseRef-Example",
            checksum="sha256:" + "b" * 64,
            packages=("example",),
        )
        inventory = Inventory(
            "atrinik",
            "2026-08-07T00:00:00Z",
            [fixture_repository()],
            [license_reference],
        )
        cyclonedx = json.loads(inventory.report("cyclonedx"))
        spdx = json.loads(inventory.report("spdx"))

        dependency_component = next(
            component
            for component in cyclonedx["components"]
            if component["bom-ref"] == license_reference.locator
        )
        self.assertEqual(dependency_component["type"], "library")
        self.assertEqual(dependency_component["hashes"][0]["alg"], "SHA-256")
        self.assertEqual(
            spdx["hasExtractedLicensingInfos"][0]["licenseId"],
            "LicenseRef-Example",
        )
        with self.assertRaisesRegex(WorkspaceError, "unsupported.*format"):
            inventory.report("unknown")
        self.assertEqual(_cyclonedx_type("container-image"), "container")
        self.assertEqual(_cyclonedx_type("external-tool"), "application")
        ordinary_spdx = json.loads(
            Inventory(
                "atrinik",
                "2026-08-07T00:00:00Z",
                [fixture_repository()],
                [fixture_dependency()],
            ).report("spdx")
        )
        self.assertNotIn("hasExtractedLicensingInfos", ordinary_spdx)

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
FROM alpine:3.23@sha256:{digest}
FROM toolchain AS validation
FROM toolchain AS final
""".format(frontend="b" * 64, digest="a" * 64)

        self.assertEqual(
            _container_references("Dockerfile", dockerfile),
            [
                f"docker/dockerfile:1@sha256:{'b' * 64}",
                f"ubuntu:26.04@sha256:{'a' * 64}",
                f"alpine:3.23@sha256:{'a' * 64}",
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

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "root"
            outside = Path(temporary) / "outside"
            root.mkdir()
            outside.mkdir()
            (root / "build").symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(WorkspaceError, "must not be a symlink"):
                write_generated(root, Path("build/report.json"), "{}\n")

    def test_duplicate_devcontainer_keys_are_rejected(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "duplicate JSON key"):
            _container_references(
                ".devcontainer/devcontainer.json",
                '{"image":"first@sha256:a","image":"second@sha256:b"}',
            )

    def test_container_reference_parsing_rejects_invalid_json(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "invalid devcontainer JSON"):
            _container_references(".devcontainer/devcontainer.json", "{")
        self.assertEqual(
            _container_references(".devcontainer/devcontainer.json", "[]"), []
        )
        self.assertEqual(
            _container_references(
                ".devcontainer/devcontainer.json", '{"image":"example/image"}'
            ),
            ["example/image"],
        )

    def test_container_reference_validation_normalizes_registry_coordinates(self) -> None:
        digest = "b" * 64
        for image, locator in [
            (f"ubuntu:26.04@sha256:{digest}", "docker.io/library/ubuntu"),
            (f"atrinik/build:1@sha256:{digest}", "docker.io/atrinik/build"),
            (f"ghcr.io/atrinik/build:1@sha256:{digest}", "ghcr.io/atrinik/build"),
        ]:
            with self.subTest(image=image):
                dependency = fixture_dependency(
                    identifier="container/example",
                    kind="container-image",
                    locator=locator,
                    commit=None,
                    checksum=f"sha256:{digest}",
                    evidence=(Evidence("fixture", "Dockerfile", image),),
                )
                _validate_container_reference(
                    [dependency], "fixture", "Dockerfile", image
                )

        with self.assertRaisesRegex(WorkspaceError, "movable container image"):
            _validate_container_reference([], "fixture", "Dockerfile", "ubuntu:latest")
        with self.assertRaisesRegex(WorkspaceError, "unowned container image"):
            _validate_container_reference(
                [], "fixture", "Dockerfile", f"ubuntu@sha256:{digest}"
            )

    def test_metadata_path_boundaries_reject_escapes_symlinks_and_bad_data(self) -> None:
        self.assertEqual(_text("value", "field"), "value")
        with self.assertRaisesRegex(WorkspaceError, "trimmed string"):
            _text(" value ", "field")
        self.assertEqual(_string_array([], "field", allow_empty=True), ())
        with self.assertRaisesRegex(WorkspaceError, "non-empty array"):
            _string_array([], "field")
        self.assertEqual(_relative_path("path/to/file", "field"), "path/to/file")
        for value in ("/absolute", "../escape"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(WorkspaceError, "safe repository-relative"):
                    _relative_path(value, "field")

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            regular = root / "regular"
            regular.write_text("ok", encoding="utf-8")
            self.assertEqual(_safe_repository_path(root, "regular"), regular)
            with self.assertRaisesRegex(WorkspaceError, "missing or escapes"):
                _safe_repository_path(root, "missing")
            directory = root / "directory"
            directory.mkdir()
            with self.assertRaisesRegex(WorkspaceError, "regular non-symlink"):
                _safe_repository_path(root, "directory")
            symlink = root / "symlink"
            symlink.symlink_to(regular)
            with self.assertRaisesRegex(WorkspaceError, "regular non-symlink"):
                _safe_repository_path(root, "symlink")

            with mock.patch(
                "atrinik_workspace.supply_chain.MAX_METADATA_BYTES", 1
            ):
                with self.assertRaisesRegex(WorkspaceError, "exceeds size limit"):
                    _read_metadata(regular)
            invalid_utf8 = root / "invalid"
            invalid_utf8.write_bytes(b"\xff")
            with self.assertRaisesRegex(WorkspaceError, "cannot read"):
                _read_metadata(invalid_utf8)

    def test_dependency_input_classification_is_content_aware(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            plain = root / "plain.cmake"
            plain.write_text("message(STATUS okay)\n", encoding="utf-8")
            dependency = root / "dependency.cmake"
            dependency.write_text("find_package(OpenSSL REQUIRED)\n", encoding="utf-8")

            self.assertTrue(_is_dependency_input("package.json", root))
            self.assertTrue(_is_dependency_input("Cargo.lock", root))
            self.assertTrue(_is_dependency_input("crates/example/Cargo.toml", root))
            self.assertTrue(_is_dependency_input("go.mod", root))
            self.assertTrue(_is_dependency_input("go.sum", root))
            self.assertTrue(_is_dependency_input("buf.gen.yaml", root))
            self.assertTrue(_is_dependency_input("rust-toolchain.toml", root))
            self.assertTrue(_is_dependency_input("deny.toml", root))
            self.assertTrue(_is_dependency_input("policy/dependencies.json", root))
            self.assertTrue(_is_dependency_input("vendor/uthash.h", root))
            self.assertTrue(_is_dependency_input("Dockerfile.release", root))
            self.assertFalse(_is_dependency_input("plain.cmake", root))
            self.assertTrue(_is_dependency_input("dependency.cmake", root))
            self.assertTrue(
                _is_dependency_input("tools/install-linux-ci-dependencies.sh", root)
            )
            self.assertFalse(_is_dependency_input("README.md", root))
            self.assertTrue(_is_container_input("Dockerfile.release"))
            self.assertTrue(_is_container_input(".devcontainer/devcontainer.json"))
            self.assertFalse(_is_container_input("package.json"))

    def test_workflow_runners_accept_only_static_literals_and_matrices(self) -> None:
        workflow = """
jobs:
  direct:
    runs-on: ubuntu-24.04
  matrix:
    strategy:
      matrix:
        os:
          - ubuntu-24.04
          - windows-2025
    runs-on: ${{ matrix.os }}
"""
        self.assertEqual(
            _workflow_runners(workflow),
            ("ubuntu-24.04", "ubuntu-24.04", "windows-2025"),
        )
        with self.assertRaisesRegex(WorkspaceError, "statically enumerated"):
            _workflow_runners("runs-on: ${{ fromJSON(inputs.runner) }}\n")
        with self.assertRaisesRegex(WorkspaceError, "no static literals"):
            _workflow_runners("runs-on: ${{ matrix.os }}\n")
        unrelated_matrix = """
jobs:
  static:
    strategy:
      matrix:
        os:
          - ubuntu-24.04
    runs-on: ubuntu-24.04
  dynamic:
    runs-on: ${{ matrix.os }}
"""
        with self.assertRaisesRegex(WorkspaceError, "no static literals"):
            _workflow_runners(unrelated_matrix)
        partly_dynamic = workflow.replace(
            "          - windows-2025", "          - ${{ inputs.runner }}"
        )
        with self.assertRaisesRegex(WorkspaceError, "no static literals"):
            _workflow_runners(partly_dynamic)

        commented = """
jobs:
  # The blank/comment paths must not affect structural indentation.

  matrix:
    strategy:
      matrix:
        # Runner values remain a direct static list.
        os:
          # Supported platforms.
          - ubuntu-24.04
        include:
          - label
    runs-on: ${{ matrix.os }}
permissions: {}
"""
        self.assertEqual(_workflow_runners(commented), ("ubuntu-24.04",))
        self.assertEqual(_static_matrix_values("matrix:\n", "os"), ())
        self.assertEqual(_static_matrix_values("plain: value\n", "os"), ())

    def test_workflow_job_blocks_are_bounded_to_jobs(self) -> None:
        standalone = "runs-on: ubuntu-24.04\n"
        self.assertEqual(_workflow_job_blocks(standalone), (standalone,))
        self.assertEqual(
            _workflow_job_blocks("jobs:\n  # no jobs\n\npermissions: {}\n"), ()
        )
        workflow = """
jobs:
  first:
    runs-on: ubuntu-24.04
  second:
    runs-on: windows-2025
permissions: {}
"""
        blocks = _workflow_job_blocks(workflow)
        self.assertEqual(len(blocks), 2)
        self.assertNotIn("permissions", blocks[-1])

    def test_npx_requires_an_immutable_setup_node_step(self) -> None:
        unpinned = "steps:\n  - run: npx --yes semantic-release\n"
        pinned = """
steps:
  - uses: actions/setup-node@820762786026740c76f36085b0efc47a31fe5020
  - run: npx --yes semantic-release
"""
        movable = """
steps:
  - uses: actions/setup-node@v7
  - run: npx --yes semantic-release
"""
        self.assertTrue(_workflow_uses_unpinned_npx(unpinned))
        self.assertFalse(_workflow_uses_unpinned_npx(pinned))
        self.assertTrue(_workflow_uses_unpinned_npx(movable))
        setup_after_npx = pinned.replace(
            "steps:", "steps:\n  - run: npx --yes semantic-release"
        )
        self.assertTrue(_workflow_uses_unpinned_npx(setup_after_npx))
        separate_jobs = """
jobs:
  setup:
    steps:
      - uses: actions/setup-node@820762786026740c76f36085b0efc47a31fe5020
  release:
    steps:
      - run: npx --yes semantic-release
"""
        self.assertTrue(_workflow_uses_unpinned_npx(separate_jobs))

        inventory = self.audit_inventory()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_audit_repository(root)
            workflow = root / ".github" / "workflows" / "ci.yml"
            workflow.write_text(
                workflow.read_text(encoding="utf-8")
                + "\n  - run: npx --yes semantic-release\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(WorkspaceError, "npx requires"):
                inventory.audit({"fixture": root})

    def test_metadata_audit_excludes_only_declared_logical_sources(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in CHECKOUT_METADATA_REQUIRED_FILES:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("metadata\n", encoding="utf-8")
            root_tool = root / "tools" / "future-dependencies.lock.json"
            root_tool.write_text("{}\n", encoding="utf-8")
            nested = root / "client" / "dependencies.lock.json"
            nested.parent.mkdir()
            nested.write_text("{}\n", encoding="utf-8")
            subprocess.run(["git", "init", "-q", str(root)], check=True)

            files = _audit_files(
                fixture_repository(
                    name="classic",
                    checkout="classic",
                    audit_mode="metadata",
                ),
                root,
                ("client", "server"),
            )

            self.assertIn("tools/future-dependencies.lock.json", files)
            self.assertNotIn("client/dependencies.lock.json", files)

            (root / "PROVENANCE.md").unlink()
            with self.assertRaisesRegex(
                WorkspaceError, "missing required files.*PROVENANCE.md"
            ):
                _audit_files(
                    fixture_repository(audit_mode="metadata"),
                    root,
                    ("client",),
                )

    def test_component_source_root_rejects_symlinks(self) -> None:
        component = Manifest.load(ROOT / "components.json").by_name["classic-server"]
        with tempfile.TemporaryDirectory() as temporary:
            checkout = Path(temporary)
            (checkout / "client").mkdir()
            (checkout / "server").symlink_to("client", target_is_directory=True)

            with self.assertRaisesRegex(WorkspaceError, "source uses a symlink"):
                _component_source_root(checkout, component)

    def test_git_file_listing_reports_command_and_encoding_failures(self) -> None:
        failed = subprocess.CompletedProcess(
            args=["git"], returncode=1, stdout=b"", stderr=b"not a repository"
        )
        invalid = subprocess.CompletedProcess(
            args=["git"], returncode=0, stdout=b"\xff\0", stderr=b""
        )
        with mock.patch("atrinik_workspace.supply_chain.subprocess.run", return_value=failed):
            with self.assertRaisesRegex(WorkspaceError, "cannot list tracked files"):
                _tracked_files(ROOT)
        with mock.patch("atrinik_workspace.supply_chain.subprocess.run", return_value=invalid):
            with self.assertRaisesRegex(WorkspaceError, "tracked path is not UTF-8"):
                _tracked_files(ROOT)

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

    def test_version_probe_and_package_fallbacks_are_machine_readable(self) -> None:
        with mock.patch(
            "atrinik_workspace.supply_chain.subprocess.run",
            side_effect=FileNotFoundError,
        ):
            versions = json.loads(version_report())
            packages = _system_package_versions(["missing"])

        self.assertTrue(all(not probe["available"] for probe in versions.values()))
        self.assertEqual(packages["missing"], {"available": False, "version": None})
        self.assertEqual(_system_package_versions([]), {})

        completed = subprocess.CompletedProcess(
            args=["dpkg-query"],
            returncode=1,
            stdout="present:amd64\t1.2.3\ninvalid line\nunknown\t9\n",
        )
        with mock.patch(
            "atrinik_workspace.supply_chain.subprocess.run", return_value=completed
        ):
            packages = _system_package_versions(["present", "missing"])
        self.assertEqual(
            packages["present"], {"available": True, "version": "1.2.3"}
        )
        self.assertFalse(packages["missing"]["available"])

        no_output = subprocess.CompletedProcess(
            args=["tool"], returncode=0, stdout="", stderr=""
        )
        with mock.patch(
            "atrinik_workspace.supply_chain.subprocess.run", return_value=no_output
        ):
            probes = json.loads(version_report())
        self.assertTrue(all(probe["available"] for probe in probes.values()))
        self.assertTrue(all(probe["version"] is None for probe in probes.values()))

    def test_repository_identity_accepts_https_and_ssh_only(self) -> None:
        for url in (
            "https://github.com/atrinik/client.git\n",
            "git@github.com:atrinik/client.git\n",
            "ssh://git@github.com/atrinik/client.git\n",
        ):
            with self.subTest(url=url):
                completed = subprocess.CompletedProcess(
                    args=["git"], returncode=0, stdout=url, stderr=""
                )
                with mock.patch(
                    "atrinik_workspace.supply_chain.subprocess.run",
                    return_value=completed,
                ):
                    self.assertEqual(_git_repository_coordinate(ROOT), "atrinik/client")

        failed = subprocess.CompletedProcess(
            args=["git"], returncode=1, stdout="", stderr="failure"
        )
        unsupported = subprocess.CompletedProcess(
            args=["git"], returncode=0, stdout="file:///tmp/client\n", stderr=""
        )
        misleading = subprocess.CompletedProcess(
            args=["git"],
            returncode=0,
            stdout="https://example.invalid/github.com/atrinik/client.git\n",
            stderr="",
        )
        for result, expected in [
            (failed, "cannot inspect repository identity"),
            (unsupported, "unsupported repository remote"),
            (misleading, "unsupported repository remote"),
        ]:
            with self.subTest(expected=expected):
                with mock.patch(
                    "atrinik_workspace.supply_chain.subprocess.run",
                    return_value=result,
                ):
                    with self.assertRaisesRegex(WorkspaceError, expected):
                        _git_repository_coordinate(ROOT)

    def test_repository_root_overrides_are_strict_and_identity_checked(self) -> None:
        workspace = mock.Mock()
        default_components = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )["stacks"]["default"]["components"]
        workspace.profile_summary.return_value = {
            "name": "profile",
            "stack": "default",
            "components": [
                {
                    "component": name,
                    "initialized": name != "resources",
                    "path": f"/workspace/profile/{name}",
                }
                for name in default_components
            ],
        }
        invalid = [
            ("bad", "must be NAME=PATH"),
            ("unknown=/tmp", "must be NAME=PATH"),
            ("client=/tmp", "not atrinik/client"),
            ("resources=relative", "must be absolute"),
        ]
        for override, expected in invalid:
            with self.subTest(override=override):
                with self.assertRaisesRegex(WorkspaceError, expected):
                    repository_roots(ROOT, workspace, "profile", [override])

        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary).resolve()
            override = f"resources={path}"
            with (
                mock.patch(
                    "atrinik_workspace.supply_chain._git_top_level",
                    return_value=path,
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_remote",
                    side_effect=WorkspaceError("wrong repository"),
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_branch_compatible",
                    return_value=True,
                ),
            ):
                with self.assertRaisesRegex(WorkspaceError, "is not atrinik/resources"):
                    repository_roots(ROOT, workspace, "profile", [override])
            with (
                mock.patch(
                    "atrinik_workspace.supply_chain._git_top_level",
                    return_value=path,
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_remote",
                    return_value="origin",
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_branch_compatible",
                    return_value=False,
                ),
            ):
                with self.assertRaisesRegex(WorkspaceError, "not based on.*@main"):
                    repository_roots(ROOT, workspace, "profile", [override])
            with (
                mock.patch(
                    "atrinik_workspace.supply_chain._git_top_level",
                    return_value=path,
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_remote",
                    return_value="origin",
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_branch_compatible",
                    return_value=True,
                ),
            ):
                with self.assertRaisesRegex(WorkspaceError, "duplicate.*override"):
                    repository_roots(
                        ROOT, workspace, "profile", [override, override]
                    )
                roots = repository_roots(ROOT, workspace, "profile", [override])

        self.assertEqual(roots["atrinik"], ROOT)
        self.assertEqual(roots["resources"], path)
        self.assertIn("server", roots)
        self.assertIn("client", roots)

    def test_repository_override_accepts_fork_origin_and_canonical_upstream(self) -> None:
        workspace = mock.Mock()
        default_components = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )["stacks"]["default"]["components"]
        workspace.profile_summary.return_value = {
            "name": "review",
            "stack": "default",
            "components": [
                {
                    "component": name,
                    "initialized": name != "resources",
                    "path": f"/workspace/review/{name}",
                }
                for name in default_components
            ],
        }

        def git(*arguments: str, cwd: Path) -> str:
            result = subprocess.run(
                ["git", *arguments],
                cwd=cwd,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            return result.stdout.strip()

        with tempfile.TemporaryDirectory() as temporary:
            review = Path(temporary) / "resources-review"
            review.mkdir()
            git("init", "-q", "-b", "main", cwd=review)
            git("config", "user.name", "Fixture", cwd=review)
            git("config", "user.email", "fixture@example.invalid", cwd=review)
            (review / "README.md").write_text("base\n", encoding="utf-8")
            git("add", "README.md", cwd=review)
            git("commit", "-q", "-m", "base", cwd=review)
            base = git("rev-parse", "HEAD", cwd=review)
            git(
                "remote",
                "add",
                "origin",
                "git@github.com:contributor/resources.git",
                cwd=review,
            )
            # The canonical-looking second URL must not make the fork origin
            # authoritative; Git fetches the first URL.
            git(
                "remote",
                "set-url",
                "--add",
                "origin",
                "https://github.com/atrinik/resources.git",
                cwd=review,
            )
            git(
                "remote",
                "add",
                "upstream",
                "https://github.com/atrinik/resources.git",
                cwd=review,
            )
            git("update-ref", "refs/remotes/upstream/main", base, cwd=review)
            git("checkout", "-q", "-b", "review/resources", cwd=review)
            (review / "README.md").write_text("review\n", encoding="utf-8")
            git("commit", "-qam", "review", cwd=review)

            roots = repository_roots(
                ROOT, workspace, "review", [f"resources={review}"]
            )
            self.assertEqual(roots["resources"], review.resolve())

            git("remote", "remove", "upstream", cwd=review)
            with self.assertRaisesRegex(
                WorkspaceError, "is not atrinik/resources"
            ):
                repository_roots(
                    ROOT, workspace, "review", [f"resources={review}"]
                )

    def test_repository_roots_reject_incomplete_profiles(self) -> None:
        workspace = mock.Mock()
        classic_components = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )["stacks"]["classic"]["components"]
        workspace.profile_summary.return_value = {
            "name": "classic-review",
            "stack": "classic",
            "components": [
                {
                    "component": name,
                    "initialized": name == "content-1x",
                    "path": f"/workspace/classic-review/{name}",
                }
                for name in classic_components
            ],
        }

        with self.assertRaisesRegex(
            WorkspaceError,
            "profile classic-review is incomplete.*classic-client.*tools",
        ):
            repository_roots(ROOT, workspace, "classic-review")

    def test_repository_roots_add_one_classic_checkout_metadata_root(self) -> None:
        workspace = mock.Mock()
        document = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )
        components = {
            component["name"]: component for component in document["components"]
        }
        classic_components = document["stacks"]["classic"]["components"]
        workspace.profile_summary.return_value = {
            "name": "classic",
            "stack": "classic",
            "components": [
                {
                    "component": name,
                    "initialized": True,
                    "checkout_path": (
                        "/workspace/classic"
                        if components[name]["checkout"] == "classic"
                        else f"/missing/{name}"
                    ),
                    "path": (
                        f"/workspace/classic/{components[name]['source']}"
                        if components[name]["checkout"] == "classic"
                        else f"/missing/{name}"
                    ),
                }
                for name in classic_components
            ],
        }

        roots = repository_roots(ROOT, workspace, "classic")

        self.assertEqual(roots["classic"], Path("/workspace/classic"))
        self.assertEqual(
            roots["classic-server"], Path("/workspace/classic/server")
        )

    def test_classic_override_resolves_the_logical_source_root(self) -> None:
        workspace = mock.Mock()
        manifest_document = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )
        classic_components = manifest_document["stacks"]["classic"]["components"]
        component_documents = {
            component["name"]: component
            for component in manifest_document["components"]
        }
        workspace.profile_summary.return_value = {
            "name": "classic-review",
            "stack": "classic",
            "components": [
                {
                    "component": name,
                    "initialized": True,
                    "checkout_path": (
                        "/workspace/classic"
                        if component_documents[name]["checkout"] == "classic"
                        else f"/missing/{name}"
                    ),
                    "path": (
                        f"/workspace/classic/{component_documents[name]['source']}"
                        if component_documents[name]["checkout"] == "classic"
                        else f"/missing/{name}"
                    ),
                }
                for name in classic_components
            ],
        }
        with tempfile.TemporaryDirectory() as temporary:
            checkout = Path(temporary) / "classic"
            checkout.mkdir()
            for component in component_documents.values():
                if component["checkout"] == "classic":
                    (checkout / component["source"]).mkdir()
            source = checkout / "server"
            subprocess.run(["git", "init", "-q", str(checkout)], check=True)
            checkout_link = Path(temporary) / "classic-link"
            checkout_link.symlink_to(checkout, target_is_directory=True)
            with self.assertRaisesRegex(WorkspaceError, "normal directory"):
                repository_roots(
                    ROOT,
                    workspace,
                    "classic-review",
                    [f"classic-server={checkout_link}"],
                )
            with (
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_remote",
                    return_value="origin",
                ),
                mock.patch(
                    "atrinik_workspace.supply_chain._git_repository_branch_compatible",
                    return_value=True,
                ),
            ):
                from_checkout = repository_roots(
                    ROOT,
                    workspace,
                    "classic-review",
                    [f"classic-server={checkout.resolve()}"],
                )
                from_source = repository_roots(
                    ROOT,
                    workspace,
                    "classic-review",
                    [f"classic-server={source.resolve()}"],
                )
                with self.assertRaisesRegex(
                    WorkspaceError, "duplicate.*override.*checkout classic"
                ):
                    repository_roots(
                        ROOT,
                        workspace,
                        "classic-review",
                        [
                            f"classic-server={checkout.resolve()}",
                            f"classic-client={checkout.resolve()}",
                        ],
                    )

        self.assertEqual(from_checkout["classic-server"], source.resolve())
        self.assertEqual(from_source["classic-server"], source.resolve())
        self.assertEqual(from_checkout["classic"], checkout.resolve())
        for name in (
            "classic-client",
            "classic-editor",
            "classic-libatrinik",
            "classic-protocol",
            "classic-server",
        ):
            expected = checkout / component_documents[name]["source"]
            self.assertEqual(from_checkout[name], expected.resolve())
            self.assertEqual(from_source[name], expected.resolve())

    def test_report_component_commits_resolve_only_initialized_profile_stack(self) -> None:
        workspace = mock.Mock()
        classic_components = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )["stacks"]["classic"]["components"]
        content_path = "/workspace/classic-review/content-1x"
        workspace.profile_summary.return_value = {
            "name": "classic-review",
            "stack": "classic",
            "components": [
                {
                    "component": name,
                    "initialized": name == "content-1x",
                    "path": content_path if name == "content-1x" else f"/missing/{name}",
                }
                for name in classic_components
            ],
        }
        root_commit = "a" * 40
        content_commit = "b" * 40
        with mock.patch(
            "atrinik_workspace.supply_chain._git_head",
            side_effect=[root_commit, content_commit],
        ) as git_head:
            stack, commits = report_component_commits(
                ROOT, workspace, "classic-review"
            )

        self.assertEqual(stack, "classic")
        self.assertEqual(commits["atrinik"], root_commit)
        self.assertEqual(commits["content-1x"], content_commit)
        self.assertIsNone(commits["classic-server"])
        self.assertIsNone(commits["classic"])
        self.assertNotIn("content", commits)
        self.assertEqual(git_head.call_count, 2)
        git_head.assert_any_call(Path(content_path))

        workspace.profile_summary.return_value["components"] = []
        with self.assertRaisesRegex(WorkspaceError, "component set does not match"):
            report_component_commits(ROOT, workspace, "classic-review")

    def test_report_component_commits_deduplicates_the_classic_checkout(self) -> None:
        workspace = mock.Mock()
        document = json.loads(
            (ROOT / "components.json").read_text(encoding="utf-8")
        )
        components = {
            component["name"]: component for component in document["components"]
        }
        classic_components = document["stacks"]["classic"]["components"]
        workspace.profile_summary.return_value = {
            "name": "classic-review",
            "stack": "classic",
            "components": [
                {
                    "component": name,
                    "initialized": components[name]["checkout"] == "classic",
                    "checkout_path": "/workspace/classic",
                    "path": f"/workspace/classic/{components[name]['source']}",
                }
                for name in classic_components
            ],
        }
        root_commit = "a" * 40
        classic_commit = "b" * 40
        with mock.patch(
            "atrinik_workspace.supply_chain._git_head",
            side_effect=[root_commit, classic_commit],
        ) as git_head:
            stack, commits = report_component_commits(
                ROOT, workspace, "classic-review"
            )

        self.assertEqual(stack, "classic")
        self.assertEqual(commits["classic"], classic_commit)
        for name in (
            "classic-client",
            "classic-editor",
            "classic-libatrinik",
            "classic-protocol",
            "classic-server",
        ):
            self.assertEqual(commits[name], classic_commit)
        self.assertEqual(git_head.call_count, 2)
        git_head.assert_any_call(Path("/workspace/classic"))

    def test_git_head_requires_a_full_commit(self) -> None:
        for result, expected in [
            (
                subprocess.CompletedProcess(
                    args=["git"], returncode=0, stdout="a" * 40 + "\n", stderr=""
                ),
                "a" * 40,
            ),
            (
                subprocess.CompletedProcess(
                    args=["git"], returncode=0, stdout="short\n", stderr=""
                ),
                None,
            ),
        ]:
            with self.subTest(expected=expected):
                with mock.patch(
                    "atrinik_workspace.supply_chain.subprocess.run",
                    return_value=result,
                ):
                    if expected is None:
                        with self.assertRaisesRegex(WorkspaceError, "full Git commit"):
                            _git_head(ROOT)
                    else:
                        self.assertEqual(_git_head(ROOT), expected)

    def test_repository_branch_compatibility_uses_ancestry_for_unique_coordinates(self) -> None:
        completed = subprocess.CompletedProcess(
            args=["git"], returncode=0, stdout="", stderr=""
        )
        incompatible = subprocess.CompletedProcess(
            args=["git"], returncode=1, stdout="", stderr=""
        )
        with mock.patch(
            "atrinik_workspace.supply_chain.subprocess.run",
            side_effect=[completed, completed],
        ) as run_command:
            self.assertTrue(_git_repository_branch_compatible(ROOT, "1.x"))
        self.assertIn(
            "refs/remotes/origin/1.x",
            " ".join(run_command.call_args_list[0].args[0]),
        )

        with mock.patch(
            "atrinik_workspace.supply_chain.subprocess.run",
            side_effect=[incompatible],
        ):
            self.assertFalse(_git_repository_branch_compatible(ROOT, "main"))

        with tempfile.TemporaryDirectory() as temporary:
            checkout = Path(temporary)
            subprocess.run(
                ["git", "init", "-q", "-b", "main", str(checkout)], check=True
            )
            subprocess.run(
                ["git", "-C", str(checkout), "config", "user.name", "Fixture"],
                check=True,
            )
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(checkout),
                    "config",
                    "user.email",
                    "fixture@example.invalid",
                ],
                check=True,
            )
            (checkout / "README.md").write_text("fixture\n", encoding="utf-8")
            subprocess.run(
                ["git", "-C", str(checkout), "add", "README.md"], check=True
            )
            subprocess.run(
                ["git", "-C", str(checkout), "commit", "-q", "-m", "fixture"],
                check=True,
            )
            self.assertFalse(
                _git_repository_branch_compatible(checkout, "main")
            )
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(checkout),
                    "update-ref",
                    "refs/remotes/origin/main",
                    "HEAD",
                ],
                check=True,
            )
            self.assertTrue(_git_repository_branch_compatible(checkout, "main"))

    def test_duplicate_repository_branches_and_review_worktrees_are_directional(self) -> None:
        def git(*arguments: str, cwd: Path | None = None) -> None:
            subprocess.run(
                ["git", *arguments],
                cwd=cwd,
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )

        with tempfile.TemporaryDirectory() as temporary:
            temporary_root = Path(temporary)
            source = temporary_root / "source"
            source.mkdir()
            git("init", "-q", "-b", "main", cwd=source)
            git("config", "user.name", "Fixture", cwd=source)
            git("config", "user.email", "fixture@example.invalid", cwd=source)
            (source / "README.md").write_text("fixture\n", encoding="utf-8")
            git("add", "README.md", cwd=source)
            git("commit", "-q", "-m", "fixture", cwd=source)
            git("branch", "1.x", cwd=source)

            content = temporary_root / "content"
            content_1x = temporary_root / "content-1x"
            separate_main = temporary_root / "separate-main"
            separate_1x = temporary_root / "separate-1x"
            git("clone", "-q", "--branch", "main", str(source), str(content))
            git("clone", "-q", "--branch", "1.x", str(source), str(content_1x))
            git("clone", "-q", "--branch", "main", str(source), str(separate_main))
            git("clone", "-q", "--branch", "1.x", str(source), str(separate_1x))
            review_main = temporary_root / "review-main"
            review_1x = temporary_root / "review-1x"
            git(
                "worktree",
                "add",
                "-q",
                "-b",
                "review-main",
                str(review_main),
                cwd=content,
            )
            git(
                "worktree",
                "add",
                "-q",
                "-b",
                "review-1x",
                str(review_1x),
                cwd=content_1x,
            )

            self.assertTrue(
                _git_repository_branch_compatible(content, "main", content)
            )
            self.assertTrue(
                _git_repository_branch_compatible(content_1x, "1.x", content_1x)
            )
            self.assertFalse(
                _git_repository_branch_compatible(content, "1.x", content_1x)
            )
            self.assertFalse(
                _git_repository_branch_compatible(content_1x, "main", content)
            )
            self.assertFalse(
                _git_repository_branch_compatible(separate_main, "main", content)
            )
            self.assertFalse(
                _git_repository_branch_compatible(separate_1x, "1.x", content_1x)
            )
            self.assertTrue(
                _git_repository_branch_compatible(separate_main, "main")
            )
            self.assertTrue(
                _git_repository_branch_compatible(separate_1x, "1.x")
            )
            self.assertTrue(
                _git_repository_branch_compatible(review_main, "main", content)
            )
            self.assertFalse(
                _git_repository_branch_compatible(review_main, "1.x", content_1x)
            )
            self.assertTrue(
                _git_repository_branch_compatible(review_1x, "1.x", content_1x)
            )
            self.assertFalse(
                _git_repository_branch_compatible(review_1x, "main", content)
            )

    def test_generated_output_can_be_printed_or_written_absolutely(self) -> None:
        with mock.patch("builtins.print") as output:
            write_generated(ROOT, None, "value\n")
        output.assert_called_once_with("value\n", end="")

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = (root / "build" / "absolute.json").resolve()
            with mock.patch("builtins.print") as output:
                write_generated(root, target, "{}\n")
            self.assertEqual(target.read_text(encoding="utf-8"), "{}\n")
            output.assert_called_once_with(target)


if __name__ == "__main__":
    unittest.main()
