from __future__ import annotations

import copy
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.supply_chain import (
    ACTION_REFERENCE_PATTERN,
    DOCKER_PULL_PATTERN,
    Dependency,
    Evidence,
    Inventory,
    Repository,
    _container_references,
    _cyclonedx_type,
    _git_repository_coordinate,
    _is_container_input,
    _is_dependency_input,
    _read_metadata,
    _relative_path,
    _safe_repository_path,
    _string_array,
    _system_package_versions,
    _text,
    _tracked_files,
    _validate_container_reference,
    repository_roots,
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


class InventoryTests(unittest.TestCase):
    def load_inventory(self) -> Inventory:
        return Inventory.load(
            ROOT / "supply-chain" / "inventory.json", ROOT / "components.json"
        )

    def inventory_document(self) -> dict[str, object]:
        return json.loads(
            (ROOT / "supply-chain" / "inventory.json").read_text(encoding="utf-8")
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
            [Repository("fixture", "atrinik/fixture", True, "fixture")],
            [fixture_dependency(), runner],
        )

    def test_inventory_and_schema_validate(self) -> None:
        inventory = self.load_inventory()
        inventory.validate_schema(ROOT / "supply-chain" / "schema.json")

        self.assertGreaterEqual(len(inventory.dependencies), 60)
        self.assertIn("nawerhals", inventory.repositories_by_name)
        self.assertFalse(inventory.repositories_by_name["nawerhals"].supported)

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
            ("supported", "true", "must be a boolean"),
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
        self.assert_invalid_document(document, "repository mismatch")

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

    def test_audit_rejects_invalid_roots_and_evidence(self) -> None:
        inventory = self.audit_inventory()
        with self.assertRaisesRegex(WorkspaceError, "roots are incomplete"):
            inventory.audit({})
        with self.assertRaisesRegex(WorkspaceError, "unknown.*repositories"):
            inventory.audit({"unknown": ROOT}, require_all=False)

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
            ("runs-on: ${{ matrix.os }}", "runner must be an explicit literal"),
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
            [Repository("fixture", "atrinik/fixture", True, "fixture")],
            [license_reference],
        )
        cyclonedx = json.loads(inventory.report("cyclonedx"))
        spdx = json.loads(inventory.report("spdx"))

        self.assertEqual(cyclonedx["components"][0]["type"], "library")
        self.assertEqual(cyclonedx["components"][0]["hashes"][0]["alg"], "SHA-256")
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
                [Repository("fixture", "atrinik/fixture", True, "fixture")],
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
        for result, expected in [
            (failed, "cannot inspect repository identity"),
            (unsupported, "unsupported repository remote"),
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
        workspace.component_path.side_effect = lambda name, profile: Path(
            f"/workspace/{profile}/{name}"
        )
        invalid = [
            ("bad", "must be NAME=PATH"),
            ("unknown=/tmp", "must be NAME=PATH"),
            ("client=relative", "must be absolute"),
        ]
        for override, expected in invalid:
            with self.subTest(override=override):
                with self.assertRaisesRegex(WorkspaceError, expected):
                    repository_roots(ROOT, workspace, "profile", [override])

        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary).resolve()
            override = f"client={path}"
            with mock.patch(
                "atrinik_workspace.supply_chain._git_repository_coordinate",
                return_value="atrinik/server",
            ):
                with self.assertRaisesRegex(WorkspaceError, "is not atrinik/client"):
                    repository_roots(ROOT, workspace, "profile", [override])
            with mock.patch(
                "atrinik_workspace.supply_chain._git_repository_coordinate",
                return_value="atrinik/client",
            ):
                with self.assertRaisesRegex(WorkspaceError, "duplicate.*override"):
                    repository_roots(
                        ROOT, workspace, "profile", [override, override]
                    )
                roots = repository_roots(ROOT, workspace, "profile", [override])

        self.assertEqual(roots["atrinik"], ROOT)
        self.assertEqual(roots["client"], path)
        self.assertIn("server", roots)

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
