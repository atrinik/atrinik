from __future__ import annotations

from datetime import date
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.provenance_identity import (
    _validate_repository_trust,
    load_document,
    record_digest,
    validate_component_reference,
    validate_registry,
    validate_reviewers,
)


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "governance/provenance-identities/registry.json"
SCHEMA = ROOT / "governance/provenance-identities/schema-v1.json"
FIXTURES = ROOT / "tests/fixtures/provenance-identities"
REVIEWERS = ROOT / "governance/provenance-identities/reviewers.json"
AS_OF = date(2026, 8, 13)


def registry() -> dict[str, object]:
    return json.loads(REGISTRY.read_text(encoding="utf-8"))


def schema() -> dict[str, object]:
    return json.loads(SCHEMA.read_text(encoding="utf-8"))


def reviewers() -> dict[str, object]:
    return json.loads(REVIEWERS.read_text(encoding="utf-8"))


def current() -> tuple[dict[str, dict[str, object]], dict[str, dict[str, object]]]:
    return (
        validate_registry(registry(), schema(), reviewers(), as_of=AS_OF),
        validate_reviewers(reviewers(), as_of=AS_OF),
    )


def refresh_digest(record: dict[str, object]) -> None:
    record["integrity"]["digest"] = record_digest(record)


class ProvenanceIdentityTests(unittest.TestCase):
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

    def test_duplicate_json_keys_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "duplicate.json"
            path.write_text('{"schema_version": 1, "schema_version": 1}\n')
            with self.assertRaisesRegex(WorkspaceError, "duplicate JSON key"):
                load_document(path)

    def test_duplicate_record_identifiers_fail_closed(self) -> None:
        value = registry()
        duplicate = json.loads(json.dumps(value["records"][0]))
        value["records"].append(duplicate)
        with self.assertRaisesRegex(WorkspaceError, "duplicate record identifier"):
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

    def test_unmerged_anchor_requires_explicit_non_authorizing_ref(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "not reachable from trusted ref"):
            _validate_repository_trust(
                ROOT, "98ff80ef66a71d8b7c48e7d0c71029abd5c90227", "main"
            )
        _validate_repository_trust(
            ROOT,
            "98ff80ef66a71d8b7c48e7d0c71029abd5c90227",
            "HEAD",
        )

    def test_repository_trust_accepts_github_actions_canonical_origin(self) -> None:
        outputs = [b"false\n", b"/tmp/coordinator.git\n", b"https://github.com/atrinik/atrinik\n", b""]
        with mock.patch(
            "atrinik_workspace.provenance_identity._git_output",
            side_effect=outputs,
        ), mock.patch("pathlib.Path.exists", return_value=False):
            _validate_repository_trust(ROOT, "1" * 40, "origin/main")

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

    def test_two_synthetic_component_references_validate_offline(self) -> None:
        records, reviewer_keys = current()
        for name in ("synthetic-alpha.json", "synthetic-beta.json"):
            with self.subTest(name=name):
                validate_component_reference(
                    load_document(FIXTURES / "positive" / name),
                    repository_root=ROOT,
                    as_of=AS_OF,
                    trusted_ref="HEAD",
                    current_records=records,
                    current_reviewers=reviewer_keys,
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
            return REVIEWERS.read_bytes()

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
