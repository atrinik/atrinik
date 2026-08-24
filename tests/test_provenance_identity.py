from __future__ import annotations

import copy
from datetime import date
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.provenance_identity import (
    MAX_DOCUMENT_BYTES,
    _coordinator_pins,
    _git_blob,
    _git_environment,
    _git_output,
    _migration_field_value,
    _migration_scope_payload,
    _preflight_blob,
    _preflight_git_output,
    _exact_keys,
    _iso_date,
    _load_bytes,
    _private_file_opener,
    _repository_path,
    _string_array,
    _validate_external_reanchors,
    _validate_repository_trust,
    _validate_fixture_replacements,
    _validate_revision_migration,
    load_document,
    record_digest,
    validate_component_reference,
    validate_paths,
    validate_registry,
    validate_reviewers,
    preflight_provenance_revisions,
)


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "governance/provenance-identities/registry.json"
SCHEMA = ROOT / "governance/provenance-identities/schema-v1.json"
FIXTURES = ROOT / "tests/fixtures/provenance-identities"
REVIEWERS = ROOT / "governance/provenance-identities/reviewers.json"
PINNED_REVISION = "f2d8eda70776ef42acdaf9150223aaecded103b1"
PINNED_REVIEWERS_PATH = "governance/provenance-identities/reviewers.json"
AS_OF = date(2026, 8, 13)


def registry() -> dict[str, object]:
    return json.loads(REGISTRY.read_text(encoding="utf-8"))


def schema() -> dict[str, object]:
    return json.loads(SCHEMA.read_text(encoding="utf-8"))


def reviewers() -> dict[str, object]:
    return json.loads(REVIEWERS.read_text(encoding="utf-8"))


def revision_migration() -> dict[str, object]:
    return json.loads(
        (ROOT / "governance/provenance-revision-migration.json").read_text(
            encoding="utf-8"
        )
    )


def current() -> tuple[dict[str, dict[str, object]], dict[str, dict[str, object]]]:
    return (
        validate_registry(registry(), schema(), reviewers(), as_of=AS_OF),
        validate_reviewers(reviewers(), as_of=AS_OF),
    )


def refresh_digest(record: dict[str, object]) -> None:
    record["integrity"]["digest"] = record_digest(record)


class ProvenanceIdentityTests(unittest.TestCase):
    def test_bounded_document_and_json_helpers_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            oversized = root / "oversized.json"
            oversized.write_bytes(b" " * (MAX_DOCUMENT_BYTES + 1))
            with self.assertRaisesRegex(WorkspaceError, "exceeds"):
                load_document(oversized)
            with self.assertRaisesRegex(WorkspaceError, "cannot load JSON"):
                load_document(root / "missing.json")
            array = root / "array.json"
            array.write_text("[]")
            with self.assertRaisesRegex(WorkspaceError, "root must be an object"):
                load_document(array)
        with self.assertRaisesRegex(WorkspaceError, "invalid JSON"):
            _load_bytes(b"not-json", "fixture")
        with self.assertRaisesRegex(WorkspaceError, "root must be an object"):
            _load_bytes(b"[]", "fixture")

    def test_reviewer_registry_rejects_malformed_authorities(self) -> None:
        mutations = (
            (lambda value: value.update(schema_version=True), "unsupported version"),
            (lambda value: value.update(reviewers=[]), "contain reviewers"),
            (lambda value: value["reviewers"].__setitem__(0, "bad"), "must be an object"),
            (lambda value: value["reviewers"][0].update(identity="invalid"), "invalid GitHub"),
            (lambda value: value["reviewers"][0].update(key_id="short"), "invalid key"),
            (lambda value: value["reviewers"][0].update(status="unknown"), "invalid status"),
            (lambda value: value["reviewers"][0].update(synthetic="yes"), "must be a boolean"),
            (lambda value: value["reviewers"][0].update(effective_on="2028-01-01"), "invalid effective"),
            (lambda value: value["reviewers"][0].update(public_key="ssh-rsa bad"), "Ed25519"),
        )
        for mutate, message in mutations:
            with self.subTest(message=message):
                value = reviewers()
                mutate(value)
                with self.assertRaisesRegex(WorkspaceError, message):
                    validate_reviewers(value, as_of=AS_OF)
        value = reviewers()
        duplicate = json.loads(json.dumps(value["reviewers"][0]))
        value["reviewers"].append(duplicate)
        with self.assertRaisesRegex(WorkspaceError, "duplicate key"):
            validate_reviewers(value, as_of=AS_OF)

    def test_public_alias_and_status_transitions_validate(self) -> None:
        alias = {
            "record_id": "pir-p-33333333333333333333333333333333",
            "record_type": "public-alias",
            "status": "active",
            "status_detail": {"effective_on": "2026-08-13"},
            "synthetic": True,
            "policy_version": 1,
            "reviewer": "github:synthetic-reviewer",
            "reviewed_on": "2026-08-13",
            "expires_on": "2027-08-13",
            "claims": ["identity"],
            "display_name": "Synthetic Contributor",
            "aliases": ["synthetic-contributor"],
            "publication_authorization": {
                "authorized_on": "2026-08-13",
                "fields": ["aliases", "display_name"],
                "restricted_record_id": "restricted-33333333333333333333333333333333",
            },
            "approval": {"key_id": "synthetic-reviewer-2026", "signature": "synthetic"},
            "integrity": {
                "algorithm": "sha256",
                "canonicalization": "atrinik-json-v1",
                "digest": "",
            },
        }
        refresh_digest(alias)
        value = registry()
        value["records"] = [alias]
        with mock.patch("atrinik_workspace.provenance_identity._verify_approval"):
            self.assertIn(alias["record_id"], validate_registry(value, schema(), reviewers(), as_of=AS_OF))

            value = registry()
            value["records"][0]["status"] = "revoked"
            value["records"][0]["status_detail"] = {
                "effective_on": "2026-08-13",
                "reason": "withdrawal",
            }
            refresh_digest(value["records"][0])
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

            value = registry()
            value["records"][0]["status"] = "superseded"
            value["records"][0]["status_detail"] = {
                "effective_on": "2026-08-13",
                "superseded_by": value["records"][1]["record_id"],
            }
            refresh_digest(value["records"][0])
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_future_status_effective_date_fails_closed(self) -> None:
        value = registry()
        value["records"][0]["status_detail"]["effective_on"] = "2026-08-14"
        refresh_digest(value["records"][0])
        with mock.patch(
            "atrinik_workspace.provenance_identity._verify_approval"
        ), self.assertRaisesRegex(WorkspaceError, "effective date is in the future"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_git_trust_and_blob_bounds_fail_closed(self) -> None:
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output", return_value=b"true\n"
        ):
            with self.assertRaisesRegex(WorkspaceError, "non-shallow"):
                _validate_repository_trust(ROOT, "1" * 40, "origin/main")
        outputs = [b"false\n", b"/tmp/coordinator.git\n", b"https://example.invalid/repo\n"]
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output", side_effect=outputs
        ), mock.patch("pathlib.Path.exists", return_value=False):
            with self.assertRaisesRegex(WorkspaceError, "origin is not"):
                _validate_repository_trust(ROOT, "1" * 40, "origin/main")
        with mock.patch("subprocess.run", side_effect=OSError("missing git")):
            with self.assertRaisesRegex(WorkspaceError, "cannot run git"):
                _git_output(ROOT, ["status"], "cannot run git")
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output", return_value=b"invalid"
        ):
            with self.assertRaisesRegex(WorkspaceError, "invalid size"):
                _git_blob(ROOT, "1" * 40, "registry.json")

    def test_reference_validation_uses_current_state_from_trusted_ref(self) -> None:
        reference = FIXTURES / "positive" / "synthetic-alpha.json"
        pinned_registry = REGISTRY.read_bytes()
        pinned_reviewers = _git_blob(ROOT, PINNED_REVISION, PINNED_REVIEWERS_PATH)
        revoked = registry()
        revoked["records"][0]["status"] = "revoked"
        revoked["records"][0]["status_detail"] = {
            "effective_on": "2026-08-13",
            "reason": "withdrawal",
        }
        refresh_digest(revoked["records"][0])
        trusted_registry = (json.dumps(revoked) + "\n").encode()

        def blob(_root: Path, revision: str, path: str) -> bytes:
            if path == "governance/provenance-identities/registry.json":
                return trusted_registry if revision == "trusted" else pinned_registry
            if path.endswith("schema-v1.json"):
                return SCHEMA.read_bytes()
            return REVIEWERS.read_bytes() if revision == "trusted" else pinned_reviewers

        with mock.patch(
            "atrinik_workspace.provenance_identity._validate_repository_trust"
        ), mock.patch(
            "atrinik_workspace.provenance_identity._git_blob", side_effect=blob
        ), mock.patch(
            "atrinik_workspace.provenance_identity._verify_approval"
        ), self.assertRaisesRegex(WorkspaceError, "current registry"):
            validate_paths(
                ROOT,
                registry_path=REGISTRY,
                schema_path=SCHEMA,
                reviewers_path=REVIEWERS,
                reference_paths=[reference],
                as_of=AS_OF,
                trusted_ref="trusted",
            )
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output",
            return_value=str(MAX_DOCUMENT_BYTES + 1).encode(),
        ):
            with self.assertRaisesRegex(WorkspaceError, "exceeds"):
                _git_blob(ROOT, "1" * 40, "registry.json")
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output",
            side_effect=[b"2", b"x"],
        ):
            with self.assertRaisesRegex(WorkspaceError, "size changed"):
                _git_blob(ROOT, "1" * 40, "registry.json")

    def test_canonical_registry_and_schema_are_valid(self) -> None:
        records = validate_registry(registry(), schema(), reviewers(), as_of=AS_OF)
        self.assertEqual(
            list(records),
            [
                "pir-c-11111111111111111111111111111111",
                "pir-c-22222222222222222222222222222222",
            ],
        )
        self.assertTrue(all(record["synthetic"] for record in records.values()))

    def test_parser_exposes_bounded_local_validation(self) -> None:
        options = parser().parse_args(["provenance", "validate"])
        self.assertEqual(options.command, "provenance")
        self.assertEqual(options.provenance_command, "validate")
        with mock.patch("builtins.print") as output:
            self.assertEqual(main(["provenance", "validate"]), 0)
        self.assertIn("2 records, 0 references", output.call_args.args[0])

    def test_parser_exposes_provenance_revision_preflight(self) -> None:
        options = parser().parse_args(["provenance", "preflight"])
        self.assertEqual(options.command, "provenance")
        self.assertEqual(options.provenance_command, "preflight")
        self.assertEqual(preflight_provenance_revisions(ROOT), (1, 6))
        with mock.patch("builtins.print") as output:
            self.assertEqual(main(["provenance", "preflight"]), 0)
        self.assertIn("1 coordinator revisions, 6 Git objects", output.call_args.args[0])
        self.assertIn("governance/provenance-revision-migration.json", output.call_args.args[0])

    def test_cli_preflight_keeps_external_migration_path_displayable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            migration = Path(temporary) / "migration.json"
            with mock.patch(
                "atrinik_workspace.cli.preflight_provenance_revisions",
                return_value=(1, 6),
            ), mock.patch("builtins.print") as output:
                self.assertEqual(
                    main(["provenance", "preflight", "--migration", str(migration)]),
                    0,
                )
        self.assertIn(str(migration), output.call_args.args[0])

    def test_provenance_revision_preflight_reports_missing_object(self) -> None:
        missing = "deadbeef" * 5 + ":AGENTS.md"
        result = subprocess.CompletedProcess(
            ["git", "cat-file", "-e", missing], 1, b"", b"missing"
        )
        with mock.patch(
            "atrinik_workspace.provenance_identity.subprocess.run",
            return_value=result,
        ), self.assertRaisesRegex(
            WorkspaceError,
            f"cannot resolve atrinik/atrinik object {re.escape(missing)}",
        ):
            _preflight_git_output(
                ROOT,
                ["cat-file", "-e", missing],
                missing,
                "atrinik/atrinik",
            )

    def test_preflight_object_integrity_failures_are_bounded(self) -> None:
        object_name = f"{PINNED_REVISION}:AGENTS.md"
        for failure in (
            OSError("git unavailable"),
            subprocess.TimeoutExpired(["git"], 10),
        ):
            with self.subTest(failure=type(failure).__name__), mock.patch(
                "atrinik_workspace.provenance_identity.subprocess.run",
                side_effect=failure,
            ), self.assertRaisesRegex(WorkspaceError, "cannot resolve"):
                _preflight_git_output(ROOT, ["cat-file", "-e", object_name], object_name, "atrinik/atrinik")

        failures = (
            ([b"different-git-object"], "object identity changed"),
            ([b"expected-git-object", b"not-a-size"], "invalid size"),
            ([b"expected-git-object", str(MAX_DOCUMENT_BYTES + 1).encode()], "exceeds the size limit"),
            ([b"expected-git-object", b"3", b"xx"], "changed during read"),
            ([b"expected-git-object", b"2", b"xx"], "SHA-256 mismatch"),
        )
        for outputs, message in failures:
            with self.subTest(message=message), mock.patch(
                "atrinik_workspace.provenance_identity._preflight_git_output",
                side_effect=outputs,
            ), self.assertRaisesRegex(WorkspaceError, message):
                _preflight_blob(
                    ROOT,
                    PINNED_REVISION,
                    "AGENTS.md",
                    "atrinik/atrinik",
                    expected_git_object="expected-git-object",
                    expected_sha256="0" * 64,
                )

    def test_revision_migration_shape_validation_fails_closed(self) -> None:
        base = revision_migration()
        mutations = (
            ("schema version", lambda value: value.update(schema_version=2)),
            ("decision", lambda value: value.update(decision="rewrite-history")),
            ("issue", lambda value: value.update(issue=None)),
            ("rationale", lambda value: value.update(rationale=None)),
            ("coordinator", lambda value: value.update(coordinator=None)),
            (
                "coordinator repository",
                lambda value: value["coordinator"].update(repository="outside/project"),
            ),
            (
                "coordinator ref",
                lambda value: value["coordinator"].update(trusted_ref="HEAD"),
            ),
            (
                "coordinator revision",
                lambda value: value["coordinator"].update(revision="short"),
            ),
            (
                "empty objects",
                lambda value: value["coordinator"].update(objects=[]),
            ),
            (
                "object entry",
                lambda value: value["coordinator"].update(objects=[None]),
            ),
            (
                "duplicate object path",
                lambda value: value["coordinator"]["objects"][1].update(
                    path=value["coordinator"]["objects"][0]["path"]
                ),
            ),
            (
                "object path",
                lambda value: value["coordinator"]["objects"][0].update(path="/absolute"),
            ),
            (
                "object revision",
                lambda value: value["coordinator"]["objects"][0].update(git_object="short"),
            ),
            (
                "object digest",
                lambda value: value["coordinator"]["objects"][0].update(sha256="short"),
            ),
            ("empty replacements", lambda value: value.update(replacements=[])),
            (
                "replacement entry",
                lambda value: value.update(replacements=[None]),
            ),
            (
                "replacement file",
                lambda value: value["replacements"][0].update(file=None),
            ),
            (
                "replacement field",
                lambda value: value["replacements"][0].update(field=" field"),
            ),
            (
                "duplicate replacement",
                lambda value: value["replacements"].append(
                    copy.deepcopy(value["replacements"][0])
                ),
            ),
            (
                "replacement old revision",
                lambda value: value["replacements"][0].update(old_revision="short"),
            ),
            (
                "replacement new revision",
                lambda value: value["replacements"][0].update(new_revision="short"),
            ),
            (
                "replacement repository",
                lambda value: value["replacements"][0].update(repository="outside/project"),
            ),
            (
                "replacement anchor",
                lambda value: value["replacements"][0].update(new_revision="0" * 40),
            ),
            (
                "replacement path",
                lambda value: value["replacements"][0].update(path=None),
            ),
            (
                "partial reviewer digests",
                lambda value: value["replacements"][0].update(old_reviewers_sha256="0" * 64),
            ),
            (
                "reviewer digest",
                lambda value: value["replacements"][2].update(old_reviewers_sha256="short"),
            ),
            ("empty external reanchors", lambda value: value.update(external_reanchors=[])),
            ("empty fixture replacements", lambda value: value.update(fixture_replacements=[])),
            ("empty historical references", lambda value: value.update(historical_references=[])),
            (
                "historical entry",
                lambda value: value.update(historical_references=[None]),
            ),
            (
                "historical revision",
                lambda value: value["historical_references"][0].update(revision="short"),
            ),
            (
                "historical file",
                lambda value: value["historical_references"][0].update(file=None),
            ),
            (
                "historical context",
                lambda value: value["historical_references"][0].update(context=None),
            ),
            (
                "historical disposition",
                lambda value: value["historical_references"][0].update(disposition="resolved"),
            ),
            (
                "historical disposition type",
                lambda value: value["historical_references"][0].update(disposition=None),
            ),
        )
        for name, mutate in mutations:
            with self.subTest(mutation=name):
                value = copy.deepcopy(base)
                mutate(value)
                with self.assertRaises(WorkspaceError):
                    _validate_revision_migration(value)

    def test_revision_migration_nested_validators_fail_closed(self) -> None:
        migration = revision_migration()
        external = migration["external_reanchors"]
        external_mutations = (
            ("item", lambda value: value.__setitem__(0, None)),
            ("file", lambda value: value[0].update(file="/absolute")),
            ("repository", lambda value: value[0].update(repository="outside/project")),
            ("trusted ref", lambda value: value[0].update(trusted_ref="HEAD")),
            ("disposition", lambda value: value[0].update(disposition=None)),
            ("references", lambda value: value[0].update(references=[])),
            (
                "reference entry",
                lambda value: value[0].update(references=[None]),
            ),
            (
                "duplicate field",
                lambda value: value[0].update(
                    references=[
                        value[0]["references"][0],
                        copy.deepcopy(value[0]["references"][0]),
                    ]
                ),
            ),
            (
                "reference revision",
                lambda value: value[0]["references"][0].update(old_revision="short"),
            ),
        )
        for name, mutate in external_mutations:
            with self.subTest(external=name):
                value = copy.deepcopy(external)
                mutate(value)
                with self.assertRaises(WorkspaceError):
                    _validate_external_reanchors(value)

        fixture = migration["fixture_replacements"]
        fixture_mutations = (
            ("empty", lambda value: value.clear()),
            ("item", lambda value: value.__setitem__(0, None)),
            ("file", lambda value: value[0].update(file="/absolute")),
            ("case id", lambda value: value[0].update(case_id=None)),
            ("boolean index", lambda value: value[0].update(coordinate_index=True)),
            ("negative index", lambda value: value[0].update(coordinate_index=-1)),
            (
                "duplicate coordinate",
                lambda value: value.append(copy.deepcopy(value[0])),
            ),
            ("repository", lambda value: value[0].update(repository="outside/project")),
            ("old revision", lambda value: value[0].update(old_revision="short")),
            ("new revision", lambda value: value[0].update(new_revision="short")),
            ("anchor", lambda value: value[0].update(new_revision="0" * 40)),
            ("workload file", lambda value: value[0].update(file="other.json")),
        )
        for name, mutate in fixture_mutations:
            with self.subTest(fixture=name):
                value = copy.deepcopy(fixture)
                mutate(value)
                with self.assertRaises(WorkspaceError):
                    _validate_fixture_replacements(value, PINNED_REVISION)

    def test_migration_helpers_require_exact_paths_and_digests(self) -> None:
        with self.assertRaises(WorkspaceError):
            _coordinator_pins(
                {"repository": "atrinik/atrinik", "revision": PINNED_REVISION},
                "pin",
            )
        with self.assertRaisesRegex(WorkspaceError, "revision must be present"):
            _coordinator_pins(
                {"repository": "atrinik/atrinik", "path": "AGENTS.md"},
                "pin",
            )
        with self.assertRaises(WorkspaceError):
            _coordinator_pins(
                {
                    "repository": "atrinik/atrinik",
                    "revision": "short",
                    "path": "AGENTS.md",
                },
                "pin",
            )
        with self.assertRaisesRegex(WorkspaceError, "does not resolve"):
            _migration_field_value({}, "root_policy.revision", "fixture")

        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        replacement = next(
            item
            for item in revision_migration()["replacements"]
            if item["file"].endswith("synthetic-alpha.json")
        )
        self.assertIsNone(_migration_scope_payload(reference, None))
        self.assertIsNone(
            _migration_scope_payload(reference, {**replacement, "field": "other"})
        )
        without_old_reviewers = dict(replacement)
        without_old_reviewers.pop("old_reviewers_sha256")
        self.assertIsNone(_migration_scope_payload(reference, without_old_reviewers))
        self.assertIsNone(
            _migration_scope_payload(
                reference,
                {**replacement, "new_revision": "0" * 40},
            )
        )
        changed_reviewers = dict(reference)
        changed_reviewers["evidence_reference"] = dict(reference["evidence_reference"])
        changed_reviewers["evidence_reference"]["reviewers_sha256"] = "0" * 64
        self.assertIsNone(_migration_scope_payload(changed_reviewers, replacement))
        self.assertIsInstance(_migration_scope_payload(reference, replacement), bytes)

    def test_preflight_rejects_unapplied_pins_and_fixture_coordinates(self) -> None:
        workload_path = ROOT / "mcp/contract/v1/fixtures/workloads.json"
        active_pin = [("pin", PINNED_REVISION, "AGENTS.md")]
        original_load_document = load_document

        def run_failure(loader, message, *, pins=active_pin):
            with mock.patch(
                "atrinik_workspace.provenance_identity.load_document",
                side_effect=loader,
            ), mock.patch(
                "atrinik_workspace.provenance_identity._preflight_revision"
            ), mock.patch(
                "atrinik_workspace.provenance_identity._preflight_blob"
            ), mock.patch(
                "atrinik_workspace.provenance_identity._coordinator_pins",
                return_value=pins,
            ), self.assertRaisesRegex(WorkspaceError, message):
                preflight_provenance_revisions(ROOT)

        run_failure(
            original_load_document,
            "found no active coordinator revision pins",
            pins=[],
        )
        run_failure(
            original_load_document,
            "does not use migration anchor",
            pins=[("pin", "0" * 40, "AGENTS.md")],
        )
        run_failure(
            original_load_document,
            "lacks coordinator object",
            pins=[("pin", PINNED_REVISION, "missing.md")],
        )

        foundations_path = ROOT / "governance/replacement-foundations.json"

        def unapplied_foundation(path):
            value = original_load_document(path)
            if path.resolve() == foundations_path.resolve():
                value = copy.deepcopy(value)
                value["root_policy"]["revision"] = "0" * 40
            return value

        run_failure(unapplied_foundation, "migration row .* is not applied")

        def workload_failure(mutator):
            def loader(path):
                value = original_load_document(path)
                if path.resolve() == workload_path.resolve():
                    value = copy.deepcopy(value)
                    mutator(value)
                return value

            return loader

        workload_mutations = (
            ("cases", lambda value: value.update(cases=None), "cases must be an array"),
            ("case", lambda value: value.update(cases=[None]), "case is invalid"),
            (
                "expected",
                lambda value: next(
                    case for case in value["cases"] if case["id"] == "github-planning-read"
                ).update(expected=None),
                "has no expected object",
            ),
            (
                "coordinates",
                lambda value: next(
                    case for case in value["cases"] if case["id"] == "github-planning-read"
                )["expected"].update(coordinates=None),
                "coordinates are invalid",
            ),
            (
                "coordinate",
                lambda value: next(
                    case for case in value["cases"] if case["id"] == "github-planning-read"
                )["expected"]["coordinates"].__setitem__(0, None),
                "coordinate is invalid",
            ),
            (
                "revision",
                lambda value: next(
                    case for case in value["cases"] if case["id"] == "github-planning-read"
                )["expected"]["coordinates"][0].update(commit="0" * 40),
                "does not use migration anchor",
            ),
            (
                "migration row",
                lambda value: next(
                    case for case in value["cases"] if case["id"] == "github-planning-read"
                ).update(id="unmapped"),
                "lacks an applied migration row",
            ),
            ("coordinate set", lambda value: value.update(cases=[]), "do not match active coordinates"),
        )
        for name, mutate, message in workload_mutations:
            with self.subTest(workload=name):
                run_failure(workload_failure(mutate), message)

        def fixture_loader(path, *, external=False):
            value = original_load_document(path)
            if path.resolve().is_relative_to(
                (ROOT / "tests/fixtures/provenance-identities").resolve()
            ):
                value = copy.deepcopy(value)
                evidence = value.get("evidence_reference")
                if isinstance(evidence, dict):
                    if external:
                        evidence["repository"] = "atrinik/other"
            return value

        with self.subTest(fixture="external repository"):
            run_failure(
                lambda path: fixture_loader(path, external=True),
                "found no coordinator fixture revisions",
            )
        wrong_fixture = ROOT / "tests/fixtures/provenance-identities/extra-wrong.json"

        def wrong_fixture_loader(path):
            if path.resolve() == wrong_fixture.resolve():
                return {
                    "evidence_reference": {
                        "repository": "atrinik/atrinik",
                        "revision": "0" * 40,
                    }
                }
            return original_load_document(path)

        with self.subTest(fixture="wrong revision"), mock.patch(
            "atrinik_workspace.provenance_identity.load_document",
            side_effect=wrong_fixture_loader,
        ), mock.patch(
            "pathlib.Path.rglob",
            return_value=[wrong_fixture],
        ), mock.patch(
            "atrinik_workspace.provenance_identity._preflight_revision"
        ), mock.patch(
            "atrinik_workspace.provenance_identity._preflight_blob"
        ), mock.patch(
            "atrinik_workspace.provenance_identity._coordinator_pins",
            return_value=active_pin,
        ), self.assertRaisesRegex(WorkspaceError, "does not use migration anchor"):
            preflight_provenance_revisions(ROOT)

        with mock.patch(
            "pathlib.Path.rglob",
            side_effect=OSError("fixture scan unavailable"),
        ), mock.patch(
            "atrinik_workspace.provenance_identity._preflight_revision"
        ), mock.patch(
            "atrinik_workspace.provenance_identity._preflight_blob"
        ), mock.patch(
            "atrinik_workspace.provenance_identity._coordinator_pins",
            return_value=active_pin,
        ), self.assertRaisesRegex(WorkspaceError, "cannot enumerate provenance fixtures"):
            preflight_provenance_revisions(ROOT)

    def test_failed_migration_signature_fallback_restores_current_error(self) -> None:
        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        records, reviewer_keys = current()
        replacement = next(
            item
            for item in revision_migration()["replacements"]
            if item["file"].endswith("synthetic-alpha.json")
        )
        def reject_only_component_scope(*_args, context, **_kwargs):
            if context == "component provenance scope_approval":
                raise WorkspaceError("current signature")
            if context == "component provenance migrated scope_approval":
                raise WorkspaceError("legacy signature")

        with mock.patch(
            "atrinik_workspace.provenance_identity._verify_approval",
            side_effect=reject_only_component_scope,
        ), self.assertRaisesRegex(WorkspaceError, "current signature"):
            validate_component_reference(
                reference,
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
                migration_replacement=replacement,
            )

    def test_duplicate_json_keys_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "duplicate.json"
            path.write_text('{"schema_version": 1, "schema_version": 1}\n')
            with self.assertRaisesRegex(WorkspaceError, "duplicate JSON key"):
                load_document(path)

    def test_primitive_contract_helpers_reject_ambiguous_inputs(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "missing wanted; unexpected extra"):
            _exact_keys({"extra": True}, {"wanted"}, "fixture")
        for value in (None, "", " untrimmed"):
            with self.subTest(text=value), self.assertRaisesRegex(
                WorkspaceError, "non-empty trimmed text"
            ):
                _iso_date(value, "fixture date")
        for value in ("not-a-date", "2026-8-3"):
            with self.subTest(date=value), self.assertRaisesRegex(
                WorkspaceError, "ISO date|canonical YYYY-MM-DD"
            ):
                _iso_date(value, "fixture date")
        for value in (None, [], ["beta", "alpha"], ["same", "same"]):
            with self.subTest(array=value), self.assertRaises(WorkspaceError):
                _string_array(value, "fixture array")
        for value in ("/absolute", "parent/../escape", "windows\\path"):
            with self.subTest(path=value), self.assertRaisesRegex(
                WorkspaceError, "safe repository-relative path"
            ):
                _repository_path(value, "fixture path")
        for value in (b"not-json", b"[]"):
            with self.subTest(document=value), self.assertRaisesRegex(
                WorkspaceError, "invalid JSON|root must be an object"
            ):
                _load_bytes(value, "fixture document")
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "private"
            descriptor = _private_file_opener(
                str(path), os.O_WRONLY | os.O_CREAT | os.O_EXCL
            )
            os.close(descriptor)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_duplicate_record_identifiers_fail_closed(self) -> None:
        value = registry()
        duplicate = json.loads(json.dumps(value["records"][0]))
        value["records"].append(duplicate)
        with self.assertRaisesRegex(WorkspaceError, "duplicate record identifier"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_registry_container_contract_fails_closed(self) -> None:
        mutations = (
            lambda value: value.update(schema="unexpected.json"),
            lambda value: value.update(reviewers="unexpected.json"),
            lambda value: value.update(records=None),
            lambda value: value.update(records=[None]),
            lambda value: value["records"][0].update(record_type="unknown"),
            lambda value: value.update(records=list(reversed(value["records"]))),
        )
        for index, mutate in enumerate(mutations):
            with self.subTest(mutation=index):
                value = registry()
                mutate(value)
                with self.assertRaises(WorkspaceError):
                    validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_duplicate_restricted_integrity_fails_closed(self) -> None:
        value = registry()
        value["records"][1]["restricted_evidence"]["integrity"] = value["records"][0][
            "restricted_evidence"
        ]["integrity"]
        refresh_digest(value["records"][1])
        with mock.patch(
            "atrinik_workspace.provenance_identity._verify_approval"
        ), self.assertRaisesRegex(WorkspaceError, "duplicate restricted evidence integrity"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_confidential_subject_fields_fail_closed(self) -> None:
        value = registry()
        value["records"][0]["alias"] = "synthetic-private-alias"
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "unsafe public field 'alias'"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_stale_active_attestation_fails_closed(self) -> None:
        value = registry()
        value["records"][0]["expires_on"] = "2026-08-12"
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "expires_on must be after reviewed_on"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_unknown_policy_version_fails_closed(self) -> None:
        value = registry()
        value["policy_version"] = 2
        with self.assertRaisesRegex(WorkspaceError, "unsupported policy version"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_boolean_versions_and_schema_drift_fail_closed(self) -> None:
        value = registry()
        value["schema_version"] = True
        with self.assertRaisesRegex(WorkspaceError, "unsupported schema version"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

        drifted = schema()
        drifted["title"] = "untrusted drift"
        with self.assertRaisesRegex(WorkspaceError, "trusted version"):
            validate_registry(registry(), drifted, reviewers(), as_of=AS_OF)

    def test_schema_status_shapes_match_the_validator_contract(self) -> None:
        definitions = schema()["$defs"]
        expected = {
            "active": "activeStatusDetail",
            "revoked": "revokedStatusDetail",
            "superseded": "supersededStatusDetail",
        }
        for record_name in ("publicAlias", "confidentialAttestation"):
            conditions = definitions[record_name]["allOf"]
            actual = {
                condition["if"]["properties"]["status"]["const"]:
                condition["then"]["properties"]["status_detail"]["$ref"].rsplit("/", 1)[-1]
                for condition in conditions
            }
            self.assertEqual(actual, expected)
        self.assertEqual(
            set(definitions["activeStatusDetail"]["properties"]), {"effective_on"}
        )
        self.assertEqual(
            set(definitions["revokedStatusDetail"]["required"]),
            {"effective_on", "reason"},
        )
        self.assertEqual(
            set(definitions["supersededStatusDetail"]["required"]),
            {"effective_on", "superseded_by"},
        )

    def test_future_review_and_readable_confidential_id_fail_closed(self) -> None:
        value = registry()
        value["records"][0]["reviewed_on"] = "2026-08-14"
        value["records"][0]["expires_on"] = "2027-08-14"
        refresh_digest(value["records"][0])
        with mock.patch("atrinik_workspace.provenance_identity._verify_approval"):
            with self.assertRaisesRegex(WorkspaceError, "review date is in the future"):
                validate_registry(value, schema(), reviewers(), as_of=AS_OF)

        value = registry()
        value["records"][0]["record_id"] = "pir-c-john-smith-readable-identity"
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "random hex"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_untrusted_reviewer_and_signature_mutation_fail_closed(self) -> None:
        value = registry()
        value["records"][0]["approval"]["key_id"] = "unknown-reviewer-key"
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "reviewer key is not authorized"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

        value = registry()
        value["records"][0]["claims"].remove("authorship")
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "reviewer signature is invalid"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_branch_only_anchor_requires_explicit_non_authorizing_ref(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            checkout = root / "checkout"
            subprocess.run(
                ["git", "init", "-b", "main", checkout],
                check=True,
                capture_output=True,
            )
            subprocess.run(
                [
                    "git",
                    "-C",
                    checkout,
                    "remote",
                    "add",
                    "origin",
                    "https://github.com/atrinik/atrinik.git",
                ],
                check=True,
            )
            environment = {
                **os.environ,
                "GIT_AUTHOR_NAME": "Test",
                "GIT_AUTHOR_EMAIL": "test@example.invalid",
                "GIT_COMMITTER_NAME": "Test",
                "GIT_COMMITTER_EMAIL": "test@example.invalid",
            }
            (checkout / "fixture").write_text("main\n", encoding="utf-8")
            subprocess.run(["git", "-C", checkout, "add", "fixture"], check=True)
            subprocess.run(
                ["git", "-C", checkout, "commit", "-m", "main"],
                check=True,
                env=environment,
                capture_output=True,
            )
            subprocess.run(["git", "-C", checkout, "branch", "audit"], check=True)
            subprocess.run(
                ["git", "-C", checkout, "checkout", "audit"],
                check=True,
                capture_output=True,
            )
            (checkout / "fixture").write_text("audit\n", encoding="utf-8")
            subprocess.run(
                ["git", "-C", checkout, "commit", "-am", "audit"],
                check=True,
                env=environment,
                capture_output=True,
            )
            revision = _git_output(
                checkout, ["rev-parse", "HEAD"], "cannot resolve audit HEAD"
            ).decode().strip()
            with self.assertRaisesRegex(
                WorkspaceError, "not reachable from trusted ref"
            ):
                _validate_repository_trust(checkout, revision, "main")
            _validate_repository_trust(checkout, revision, revision)

    def test_repository_trust_accepts_github_actions_canonical_origin(self) -> None:
        outputs = [b"false\n", b"/tmp/coordinator.git\n", b"https://github.com/atrinik/atrinik\n", b""]
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output",
            side_effect=outputs,
        ), mock.patch("pathlib.Path.exists", return_value=False):
            _validate_repository_trust(ROOT, "1" * 40, "origin/main")

    def test_git_environment_cannot_redirect_the_coordinator_repository(self) -> None:
        selecting = {
            "GIT_ALTERNATE_OBJECT_DIRECTORIES",
            "GIT_CEILING_DIRECTORIES",
            "GIT_COMMON_DIR",
            "GIT_DIR",
            "GIT_DISCOVERY_ACROSS_FILESYSTEM",
            "GIT_INDEX_FILE",
            "GIT_OBJECT_DIRECTORY",
            "GIT_REPLACE_REF_BASE",
            "GIT_WORK_TREE",
        }
        with mock.patch.dict(os.environ, {name: "/tmp/untrusted" for name in selecting}):
            environment = _git_environment()
        self.assertTrue(selecting.isdisjoint(environment))
        self.assertEqual(environment["GIT_NO_REPLACE_OBJECTS"], "1")

    def test_record_digest_detects_mutation(self) -> None:
        value = registry()
        value["records"][0]["claims"].remove("authorship")
        with self.assertRaisesRegex(WorkspaceError, "does not match canonical record"):
            validate_registry(value, schema(), reviewers(), as_of=AS_OF)

    def test_reference_rejects_noncanonical_url_without_git_access(self) -> None:
        value = {
            "schema_version": 1,
            "synthetic": True,
            "source": {
                "repository": "atrinik/synthetic-source",
                "path": "src/example.c",
                "revision": "1" * 40,
            },
            "destination": {
                "repository": "atrinik/synthetic-alpha",
                "path": "src/example.rs",
            },
            "transformation": "synthetic translation",
            "scope_binding": "psb-11111111111111111111111111111111",
            "scope_approval": {"key_id": "synthetic-reviewer-2026", "signature": "invalid"},
            "evidence_reference": {
                "repository": "atrinik/atrinik",
                "revision": "1" * 40,
                "record_id": "pir-c-11111111111111111111111111111111",
                "registry_sha256": "1" * 64,
                "reviewers_sha256": "3" * 64,
                "schema_sha256": "2" * 64,
                "url": "https://example.invalid/movable",
            },
        }
        records, reviewer_keys = current()
        with self.assertRaisesRegex(WorkspaceError, "canonical immutable permalink"):
            validate_component_reference(
                value,
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
            )

    def test_component_reference_shape_fails_closed_before_blob_reads(self) -> None:
        baseline = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        records, reviewer_keys = current()
        mutations = (
            lambda value: value.update(schema_version=True),
            lambda value: value.update(synthetic="yes"),
            lambda value: value.update(source=None),
            lambda value: value["destination"].update(extra=True),
            lambda value: value["source"].update(repository="outside/project"),
            lambda value: value["destination"].update(path="/absolute"),
            lambda value: value["source"].update(revision="short"),
            lambda value: value.update(transformation=""),
            lambda value: value.update(scope_binding="readable"),
            lambda value: value.update(evidence_reference=None),
            lambda value: value["evidence_reference"].update(extra=True),
            lambda value: value["evidence_reference"].update(repository="outside/project"),
            lambda value: value["evidence_reference"].update(revision="short"),
            lambda value: value["evidence_reference"].update(record_id="readable"),
            lambda value: value["evidence_reference"].update(url="https://example.invalid"),
            lambda value: value["evidence_reference"].update(registry_sha256="invalid"),
            lambda value: value["evidence_reference"].update(reviewers_sha256=True),
            lambda value: value["evidence_reference"].update(schema_sha256="A" * 64),
        )
        for index, mutate in enumerate(mutations):
            with self.subTest(mutation=index):
                value = json.loads(json.dumps(baseline))
                mutate(value)
                with mock.patch(
                    "atrinik_workspace.provenance_identity._validate_repository_trust"
                ), mock.patch(
                    "atrinik_workspace.provenance_identity._git_blob",
                    side_effect=AssertionError("unexpected blob read"),
                ), self.assertRaises(WorkspaceError):
                    validate_component_reference(
                        value,
                        repository_root=ROOT,
                        as_of=AS_OF,
                        trusted_ref="HEAD",
                        current_records=records,
                        current_reviewers=reviewer_keys,
                    )

    def test_two_synthetic_component_references_validate_offline(self) -> None:
        validate_paths(
            ROOT,
            registry_path=REGISTRY,
            schema_path=SCHEMA,
            reviewers_path=REVIEWERS,
            reference_paths=[
                FIXTURES / "positive" / "synthetic-alpha.json",
                FIXTURES / "positive" / "synthetic-beta.json",
            ],
            as_of=AS_OF,
            trusted_ref="HEAD",
        )

    def test_external_reference_path_is_not_given_a_migration_replacement(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            reference_path = Path(temporary) / "external.json"
            reference_path.write_text("{}\n", encoding="utf-8")
            with mock.patch(
                "atrinik_workspace.provenance_identity.validate_component_reference"
            ) as validate_reference:
                self.assertEqual(
                    validate_paths(
                        ROOT,
                        registry_path=REGISTRY,
                        schema_path=SCHEMA,
                        reviewers_path=REVIEWERS,
                        reference_paths=[reference_path],
                        as_of=AS_OF,
                        trusted_ref="HEAD",
                    ),
                    2,
                )
            self.assertIsNone(
                validate_reference.call_args.kwargs["migration_replacement"]
            )

    def test_broken_immutable_reference_fixture_fails_closed(self) -> None:
        records, reviewer_keys = current()
        with self.assertRaisesRegex(WorkspaceError, "registry digest does not match"):
            validate_component_reference(
                load_document(FIXTURES / "negative" / "broken-reference.json"),
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
            )

    def test_reference_to_revoked_attestation_fails_closed(self) -> None:
        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        pinned_reviewers = _git_blob(ROOT, PINNED_REVISION, PINNED_REVIEWERS_PATH)
        registry_blob = json.loads(
            REGISTRY.read_text(encoding="utf-8")
        )
        registry_blob["records"][0]["status"] = "revoked"
        registry_blob["records"][0]["status_detail"] = {
            "effective_on": "2026-08-13",
            "reason": "withdrawal",
        }
        refresh_digest(registry_blob["records"][0])
        encoded_registry = (json.dumps(registry_blob) + "\n").encode()
        reference["evidence_reference"]["registry_sha256"] = hashlib.sha256(
            encoded_registry
        ).hexdigest()

        def blob(_root: Path, _revision: str, path: str) -> bytes:
            if path.endswith("registry.json"):
                return encoded_registry
            if path.endswith("schema-v1.json"):
                return SCHEMA.read_bytes()
            return pinned_reviewers

        records, reviewer_keys = current()
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_blob", side_effect=blob
        ), mock.patch(
            "atrinik_workspace.provenance_identity._validate_repository_trust"
        ), mock.patch(
            "atrinik_workspace.provenance_identity._verify_approval"
        ):
            with self.assertRaisesRegex(WorkspaceError, "record is not active"):
                validate_component_reference(
                    reference,
                    repository_root=ROOT,
                    as_of=AS_OF,
                    trusted_ref="HEAD",
                    current_records=records,
                    current_reviewers=reviewer_keys,
                )

    def test_current_revocation_overrides_active_pinned_record(self) -> None:
        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        records, reviewer_keys = current()
        records[reference["evidence_reference"]["record_id"]]["status"] = "revoked"
        with self.assertRaisesRegex(WorkspaceError, "current registry"):
            validate_component_reference(
                reference,
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
            )

    def test_current_registry_cannot_replace_an_active_pinned_record(self) -> None:
        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        records, reviewer_keys = current()
        records[reference["evidence_reference"]["record_id"]]["integrity"]["digest"] = (
            "f" * 64
        )
        with self.assertRaisesRegex(WorkspaceError, "differs from the pinned"):
            validate_component_reference(
                reference,
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
            )

    def test_scope_replay_invalidates_reviewer_signature(self) -> None:
        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        reference["source"]["path"] = "engine/unapproved.c"
        records, reviewer_keys = current()
        with self.assertRaisesRegex(WorkspaceError, "reviewer signature is invalid"):
            validate_component_reference(
                reference,
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
            )

    def test_component_reference_cannot_cross_the_synthetic_boundary(self) -> None:
        reference = load_document(FIXTURES / "positive" / "synthetic-alpha.json")
        reference["synthetic"] = False
        records, reviewer_keys = current()
        with self.assertRaisesRegex(WorkspaceError, "reviewer signature is invalid"):
            validate_component_reference(
                reference,
                repository_root=ROOT,
                as_of=AS_OF,
                trusted_ref="HEAD",
                current_records=records,
                current_reviewers=reviewer_keys,
            )


if __name__ == "__main__":
    unittest.main()
