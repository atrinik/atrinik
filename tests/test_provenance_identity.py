from __future__ import annotations

from datetime import date
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from atrinik_workspace.cli import main, parser
from atrinik_workspace.model import WorkspaceError
from atrinik_workspace.provenance_identity import (
    load_document,
    record_digest,
    validate_component_reference,
    validate_registry,
)


ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "governance/provenance-identities/registry.json"
SCHEMA = ROOT / "governance/provenance-identities/schema-v1.json"
AS_OF = date(2026, 8, 13)


def registry() -> dict[str, object]:
    return json.loads(REGISTRY.read_text(encoding="utf-8"))


def schema() -> dict[str, object]:
    return json.loads(SCHEMA.read_text(encoding="utf-8"))


def refresh_digest(record: dict[str, object]) -> None:
    record["integrity"]["digest"] = record_digest(record)


class ProvenanceIdentityTests(unittest.TestCase):
    def test_canonical_registry_and_schema_are_valid(self) -> None:
        records = validate_registry(registry(), schema(), as_of=AS_OF)
        self.assertEqual(
            list(records),
            ["pir-synthetic-alpha-0001", "pir-synthetic-beta-00001"],
        )
        self.assertTrue(all(record["synthetic"] for record in records.values()))

    def test_parser_exposes_bounded_local_validation(self) -> None:
        options = parser().parse_args(
            ["provenance", "validate", "--as-of", "2026-08-13"]
        )
        self.assertEqual(options.command, "provenance")
        self.assertEqual(options.provenance_command, "validate")
        self.assertEqual(options.as_of, AS_OF)

        with mock.patch("builtins.print") as output:
            self.assertEqual(
                main(["provenance", "validate", "--as-of", "2026-08-13"]),
                0,
            )
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
            validate_registry(value, schema(), as_of=AS_OF)

    def test_confidential_subject_fields_fail_closed(self) -> None:
        value = registry()
        value["records"][0]["alias"] = "synthetic-private-alias"
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "unsafe public field 'alias'"):
            validate_registry(value, schema(), as_of=AS_OF)

    def test_stale_active_attestation_fails_closed(self) -> None:
        value = registry()
        value["records"][0]["reviewed_on"] = "2020-01-01"
        value["records"][0]["expires_on"] = "2026-08-12"
        refresh_digest(value["records"][0])
        with self.assertRaisesRegex(WorkspaceError, "active attestation is stale"):
            validate_registry(value, schema(), as_of=AS_OF)

    def test_unknown_policy_version_fails_closed(self) -> None:
        value = registry()
        value["policy_version"] = 2
        with self.assertRaisesRegex(WorkspaceError, "unsupported policy version"):
            validate_registry(value, schema(), as_of=AS_OF)

    def test_record_digest_detects_mutation(self) -> None:
        value = registry()
        value["records"][0]["claims"].remove("authorship")
        with self.assertRaisesRegex(WorkspaceError, "does not match canonical record"):
            validate_registry(value, schema(), as_of=AS_OF)

    def test_reference_rejects_noncanonical_url_without_git_access(self) -> None:
        value = {
            "schema_version": 1,
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
            "evidence_reference": {
                "repository": "atrinik/atrinik",
                "revision": "1" * 40,
                "record_id": "pir-synthetic-alpha-0001",
                "registry_sha256": "1" * 64,
                "schema_sha256": "2" * 64,
                "url": "https://example.invalid/movable",
            },
        }
        with self.assertRaisesRegex(WorkspaceError, "canonical immutable permalink"):
            validate_component_reference(value, repository_root=ROOT, as_of=AS_OF)


if __name__ == "__main__":
    unittest.main()
