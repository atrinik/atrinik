from __future__ import annotations

from datetime import date
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.provenance_identity import (
    MAX_DOCUMENT_BYTES,
    _git_blob,
    _git_environment,
    _git_output,
    _exact_keys,
    _iso_date,
    _load_bytes,
    _private_file_opener,
    _repository_path,
    _string_array,
    _validate_repository_trust,
    load_document,
    record_digest,
    validate_component_reference,
    validate_paths,
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
            return REVIEWERS.read_bytes()

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

    def test_unmerged_anchor_requires_explicit_non_authorizing_ref(self) -> None:
        with self.assertRaisesRegex(WorkspaceError, "not reachable from trusted ref"):
            _validate_repository_trust(
                ROOT, "51aa7ac9d5ae9c0ff0b2a24a46b5d3e97739bbe0", "main"
            )
        _validate_repository_trust(
            ROOT,
            "51aa7ac9d5ae9c0ff0b2a24a46b5d3e97739bbe0",
            "HEAD",
        )

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
