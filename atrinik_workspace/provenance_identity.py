from __future__ import annotations

from datetime import date
import hashlib
import json
from pathlib import Path
import re
import subprocess
from typing import Any

from .model import WorkspaceError


SCHEMA_VERSION = 1
POLICY_VERSION = 1
MAX_DOCUMENT_BYTES = 1024 * 1024
CANONICALIZATION = "atrinik-json-v1"
REGISTRY_PATH = Path("governance/provenance-identities/registry.json")
SCHEMA_PATH = Path("governance/provenance-identities/schema-v1.json")
RECORD_ID_PATTERN = re.compile(r"^pir-[a-z0-9][a-z0-9-]{15,63}$")
SCOPE_BINDING_PATTERN = re.compile(r"^psb-[0-9a-f]{32}$")
RESTRICTED_ID_PATTERN = re.compile(r"^restricted-[0-9a-f]{32}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
REVIEWER_PATTERN = re.compile(r"^github:[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")
KNOWN_CLAIMS = {
    "authorship",
    "identity",
    "rights-grant",
    "temporal-scope",
}
CONFIDENTIAL_FORBIDDEN_KEYS = {
    "alias",
    "aliases",
    "commit",
    "contact",
    "destination",
    "display_name",
    "email",
    "grantor",
    "handle",
    "legal_name",
    "login",
    "name",
    "path",
    "repository",
    "revision",
    "source",
    "subject",
    "username",
}


def _object_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise WorkspaceError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_document(path: Path) -> dict[str, Any]:
    try:
        if path.stat().st_size > MAX_DOCUMENT_BYTES:
            raise WorkspaceError(f"{path}: document exceeds {MAX_DOCUMENT_BYTES} bytes")
        value = json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=_object_pairs
        )
    except WorkspaceError:
        raise
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise WorkspaceError(f"{path}: cannot load JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise WorkspaceError(f"{path}: root must be an object")
    return value


def canonical_bytes(value: object) -> bytes:
    """Serialize the deliberately integer-free public contract deterministically."""

    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")


def sha256(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def record_digest(record: dict[str, Any]) -> str:
    payload = {key: value for key, value in record.items() if key != "integrity"}
    return sha256(canonical_bytes(payload))


def _exact_keys(value: dict[str, Any], expected: set[str], context: str) -> None:
    missing = expected - set(value)
    extra = set(value) - expected
    if missing or extra:
        details = []
        if missing:
            details.append(f"missing {', '.join(sorted(missing))}")
        if extra:
            details.append(f"unexpected {', '.join(sorted(extra))}")
        raise WorkspaceError(f"{context}: {'; '.join(details)}")


def _text(value: object, context: str) -> str:
    if not isinstance(value, str) or not value or value.strip() != value:
        raise WorkspaceError(f"{context}: must be non-empty trimmed text")
    return value


def _iso_date(value: object, context: str) -> date:
    text = _text(value, context)
    try:
        parsed = date.fromisoformat(text)
    except ValueError as exc:
        raise WorkspaceError(f"{context}: must be an ISO date") from exc
    if parsed.isoformat() != text:
        raise WorkspaceError(f"{context}: must use canonical YYYY-MM-DD form")
    return parsed


def _string_array(value: object, context: str) -> list[str]:
    if not isinstance(value, list) or not value:
        raise WorkspaceError(f"{context}: must be a non-empty array")
    result = [_text(item, f"{context}[{index}]") for index, item in enumerate(value)]
    if result != sorted(set(result)):
        raise WorkspaceError(f"{context}: must be sorted and unique")
    return result


def _validate_schema(schema: dict[str, Any]) -> None:
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        raise WorkspaceError("provenance identity schema must use draft 2020-12")
    if schema.get("$id") != "https://atrinik.org/schema/provenance-identities-v1.json":
        raise WorkspaceError("provenance identity schema has an unexpected $id")
    if schema.get("type") != "object" or schema.get("additionalProperties") is not False:
        raise WorkspaceError("provenance identity schema must reject non-object or extra fields")
    version = schema.get("properties", {}).get("schema_version", {}).get("const")
    if version != SCHEMA_VERSION:
        raise WorkspaceError("provenance identity schema has an unsupported version")


def _walk_confidential_keys(value: object, context: str) -> None:
    if isinstance(value, dict):
        for key, child in value.items():
            if key.lower() in CONFIDENTIAL_FORBIDDEN_KEYS:
                raise WorkspaceError(
                    f"{context}: confidential record contains unsafe public field {key!r}"
                )
            _walk_confidential_keys(child, context)
    elif isinstance(value, list):
        for child in value:
            _walk_confidential_keys(child, context)


def _validate_integrity(record: dict[str, Any], context: str) -> None:
    integrity = record.get("integrity")
    if not isinstance(integrity, dict):
        raise WorkspaceError(f"{context}.integrity: must be an object")
    _exact_keys(integrity, {"algorithm", "canonicalization", "digest"}, f"{context}.integrity")
    if integrity["algorithm"] != "sha256":
        raise WorkspaceError(f"{context}.integrity.algorithm: must be sha256")
    if integrity["canonicalization"] != CANONICALIZATION:
        raise WorkspaceError(
            f"{context}.integrity.canonicalization: must be {CANONICALIZATION}"
        )
    digest = integrity["digest"]
    if not isinstance(digest, str) or not SHA256_PATTERN.fullmatch(digest):
        raise WorkspaceError(f"{context}.integrity.digest: must be lowercase SHA-256")
    if digest != record_digest(record):
        raise WorkspaceError(f"{context}.integrity.digest: does not match canonical record")


def _validate_common_record(record: dict[str, Any], context: str, as_of: date) -> None:
    identifier = record.get("record_id")
    if not isinstance(identifier, str) or not RECORD_ID_PATTERN.fullmatch(identifier):
        raise WorkspaceError(f"{context}.record_id: must be an opaque pir identifier")
    if record.get("policy_version") != POLICY_VERSION:
        raise WorkspaceError(f"{context}.policy_version: unsupported policy version")
    if record.get("status") not in {"active", "revoked", "superseded"}:
        raise WorkspaceError(f"{context}.status: unsupported status")
    reviewer = record.get("reviewer")
    if not isinstance(reviewer, str) or not REVIEWER_PATTERN.fullmatch(reviewer):
        raise WorkspaceError(f"{context}.reviewer: must be a public GitHub reviewer identity")
    reviewed = _iso_date(record.get("reviewed_on"), f"{context}.reviewed_on")
    expires = _iso_date(record.get("expires_on"), f"{context}.expires_on")
    if expires <= reviewed:
        raise WorkspaceError(f"{context}: expires_on must be after reviewed_on")
    if record.get("status") == "active" and expires < as_of:
        raise WorkspaceError(f"{context}: active attestation is stale")
    claims = _string_array(record.get("claims"), f"{context}.claims")
    if not set(claims) <= KNOWN_CLAIMS:
        raise WorkspaceError(f"{context}.claims: contains an unknown claim")
    _validate_integrity(record, context)


def _validate_confidential_record(record: dict[str, Any], context: str, as_of: date) -> None:
    _walk_confidential_keys(record, context)
    _exact_keys(
        record,
        {
            "claims",
            "expires_on",
            "integrity",
            "policy_version",
            "publication_review",
            "record_id",
            "record_type",
            "restricted_evidence",
            "reviewed_on",
            "reviewer",
            "scope_binding",
            "status",
            "synthetic",
        },
        context,
    )
    if not isinstance(record["synthetic"], bool):
        raise WorkspaceError(f"{context}.synthetic: must be a boolean")
    binding = record["scope_binding"]
    if not isinstance(binding, str) or not SCOPE_BINDING_PATTERN.fullmatch(binding):
        raise WorkspaceError(f"{context}.scope_binding: must be an opaque binding")
    evidence = record["restricted_evidence"]
    if not isinstance(evidence, dict):
        raise WorkspaceError(f"{context}.restricted_evidence: must be an object")
    _exact_keys(evidence, {"integrity", "record_id"}, f"{context}.restricted_evidence")
    if not RESTRICTED_ID_PATTERN.fullmatch(str(evidence["record_id"])):
        raise WorkspaceError(f"{context}.restricted_evidence.record_id: invalid identifier")
    if not re.fullmatch(r"hmac-sha256:[0-9a-f]{64}", str(evidence["integrity"])):
        raise WorkspaceError(f"{context}.restricted_evidence.integrity: invalid keyed digest")
    review = record["publication_review"]
    if not isinstance(review, dict):
        raise WorkspaceError(f"{context}.publication_review: must be an object")
    _exact_keys(review, {"correlation_risk", "fields_reviewed"}, f"{context}.publication_review")
    if review["correlation_risk"] != "approved":
        raise WorkspaceError(f"{context}.publication_review: correlation risk is not approved")
    fields = _string_array(review["fields_reviewed"], f"{context}.publication_review.fields_reviewed")
    if fields != sorted(record):
        raise WorkspaceError(f"{context}.publication_review: must cover every public field")
    _validate_common_record(record, context, as_of)


def _validate_public_alias_record(record: dict[str, Any], context: str, as_of: date) -> None:
    _exact_keys(
        record,
        {
            "aliases",
            "claims",
            "display_name",
            "expires_on",
            "integrity",
            "policy_version",
            "publication_authorization",
            "record_id",
            "record_type",
            "reviewed_on",
            "reviewer",
            "status",
            "synthetic",
        },
        context,
    )
    if not isinstance(record["synthetic"], bool):
        raise WorkspaceError(f"{context}.synthetic: must be a boolean")
    _text(record["display_name"], f"{context}.display_name")
    _string_array(record["aliases"], f"{context}.aliases")
    authorization = record["publication_authorization"]
    if not isinstance(authorization, dict):
        raise WorkspaceError(f"{context}.publication_authorization: must be an object")
    _exact_keys(
        authorization,
        {"authorized_on", "fields", "restricted_record_id"},
        f"{context}.publication_authorization",
    )
    _iso_date(authorization["authorized_on"], f"{context}.publication_authorization.authorized_on")
    if _string_array(authorization["fields"], f"{context}.publication_authorization.fields") != [
        "aliases",
        "display_name",
    ]:
        raise WorkspaceError(f"{context}.publication_authorization: must explicitly cover all identity fields")
    if not RESTRICTED_ID_PATTERN.fullmatch(str(authorization["restricted_record_id"])):
        raise WorkspaceError(f"{context}.publication_authorization.restricted_record_id: invalid identifier")
    _validate_common_record(record, context, as_of)


def validate_registry(
    registry: dict[str, Any], schema: dict[str, Any], *, as_of: date
) -> dict[str, dict[str, Any]]:
    _validate_schema(schema)
    _exact_keys(
        registry,
        {"policy_version", "records", "schema", "schema_version"},
        "provenance identity registry",
    )
    if registry["schema_version"] != SCHEMA_VERSION:
        raise WorkspaceError("provenance identity registry has an unsupported schema version")
    if registry["policy_version"] != POLICY_VERSION:
        raise WorkspaceError("provenance identity registry has an unsupported policy version")
    if registry["schema"] != SCHEMA_PATH.as_posix():
        raise WorkspaceError("provenance identity registry names an unexpected schema")
    records = registry["records"]
    if not isinstance(records, list):
        raise WorkspaceError("provenance identity registry records must be an array")
    result: dict[str, dict[str, Any]] = {}
    bindings: set[str] = set()
    restricted_ids: set[str] = set()
    for index, record in enumerate(records):
        context = f"provenance identity record {index}"
        if not isinstance(record, dict):
            raise WorkspaceError(f"{context}: must be an object")
        identifier = record.get("record_id")
        if identifier in result:
            raise WorkspaceError(f"{context}: duplicate record identifier {identifier}")
        record_type = record.get("record_type")
        if record_type == "confidential-attestation":
            _validate_confidential_record(record, context, as_of)
            binding = record["scope_binding"]
            restricted_id = record["restricted_evidence"]["record_id"]
            if binding in bindings:
                raise WorkspaceError(f"{context}: duplicate scope binding")
            if restricted_id in restricted_ids:
                raise WorkspaceError(f"{context}: duplicate restricted evidence identifier")
            bindings.add(binding)
            restricted_ids.add(restricted_id)
        elif record_type == "public-alias":
            _validate_public_alias_record(record, context, as_of)
        else:
            raise WorkspaceError(f"{context}.record_type: unsupported record type")
        identifier = record["record_id"]
        result[identifier] = record
    if list(result) != sorted(result):
        raise WorkspaceError("provenance identity registry records must be sorted by record_id")
    return result


def _git_blob(repository_root: Path, revision: str, path: str) -> bytes:
    try:
        result = subprocess.run(
            ["git", "show", f"{revision}:{path}"],
            cwd=repository_root,
            check=True,
            capture_output=True,
            timeout=10,
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        raise WorkspaceError(f"immutable reference cannot resolve {revision}:{path}") from exc
    if len(result.stdout) > MAX_DOCUMENT_BYTES:
        raise WorkspaceError("immutable reference document exceeds the size limit")
    return result.stdout


def _load_bytes(value: bytes, context: str) -> dict[str, Any]:
    try:
        result = json.loads(value.decode("utf-8"), object_pairs_hook=_object_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise WorkspaceError(f"{context}: invalid JSON") from exc
    if not isinstance(result, dict):
        raise WorkspaceError(f"{context}: root must be an object")
    return result


def validate_component_reference(
    reference: dict[str, Any], *, repository_root: Path, as_of: date
) -> None:
    _exact_keys(
        reference,
        {
            "destination",
            "evidence_reference",
            "schema_version",
            "scope_binding",
            "source",
            "transformation",
        },
        "component provenance record",
    )
    if reference["schema_version"] != SCHEMA_VERSION:
        raise WorkspaceError("component provenance record has an unsupported schema version")
    for key in ("source", "destination"):
        coordinate = reference[key]
        if not isinstance(coordinate, dict):
            raise WorkspaceError(f"component provenance record {key} must be an object")
        required = {"path", "repository", "revision"} if key == "source" else {"path", "repository"}
        _exact_keys(coordinate, required, f"component provenance record {key}")
        if not re.fullmatch(r"atrinik/[a-z0-9][a-z0-9._-]*", str(coordinate["repository"])):
            raise WorkspaceError(f"component provenance record {key}.repository is invalid")
        _text(coordinate["path"], f"component provenance record {key}.path")
        if key == "source" and not _text(coordinate["revision"], "source.revision"):
            raise AssertionError("unreachable")
    _text(reference["transformation"], "component provenance record transformation")
    evidence = reference["evidence_reference"]
    if not isinstance(evidence, dict):
        raise WorkspaceError("component provenance evidence_reference must be an object")
    _exact_keys(
        evidence,
        {"record_id", "registry_sha256", "repository", "revision", "schema_sha256", "url"},
        "component provenance evidence_reference",
    )
    if evidence["repository"] != "atrinik/atrinik":
        raise WorkspaceError("component provenance reference must use atrinik/atrinik")
    revision = evidence["revision"]
    if not isinstance(revision, str) or not REVISION_PATTERN.fullmatch(revision):
        raise WorkspaceError("component provenance reference revision must be a full Git SHA")
    record_id = evidence["record_id"]
    if not isinstance(record_id, str) or not RECORD_ID_PATTERN.fullmatch(record_id):
        raise WorkspaceError("component provenance reference record_id is invalid")
    expected_url = (
        f"https://github.com/atrinik/atrinik/blob/{revision}/"
        f"{REGISTRY_PATH.as_posix()}#{record_id}"
    )
    if evidence["url"] != expected_url:
        raise WorkspaceError("component provenance reference URL is not the canonical immutable permalink")
    for key in ("registry_sha256", "schema_sha256"):
        if not isinstance(evidence[key], str) or not SHA256_PATTERN.fullmatch(evidence[key]):
            raise WorkspaceError(f"component provenance reference {key} is invalid")
    registry_blob = _git_blob(repository_root, revision, REGISTRY_PATH.as_posix())
    schema_blob = _git_blob(repository_root, revision, SCHEMA_PATH.as_posix())
    if sha256(registry_blob) != evidence["registry_sha256"]:
        raise WorkspaceError("component provenance reference registry digest does not match")
    if sha256(schema_blob) != evidence["schema_sha256"]:
        raise WorkspaceError("component provenance reference schema digest does not match")
    records = validate_registry(
        _load_bytes(registry_blob, "referenced registry"),
        _load_bytes(schema_blob, "referenced schema"),
        as_of=as_of,
    )
    if record_id not in records:
        raise WorkspaceError("component provenance reference record is absent")
    record = records[record_id]
    if record["status"] != "active":
        raise WorkspaceError("component provenance reference record is not active")
    if record["record_type"] != "confidential-attestation":
        raise WorkspaceError("component provenance reference must select a confidential attestation")
    if record["scope_binding"] != reference["scope_binding"]:
        raise WorkspaceError("component provenance scope binding does not match the attestation")
    if record["synthetic"] and not str(reference["destination"]["repository"]).startswith("atrinik/synthetic-"):
        raise WorkspaceError("synthetic attestations may only be used by synthetic component fixtures")


def validate_paths(
    root: Path,
    *,
    registry_path: Path,
    schema_path: Path,
    reference_paths: list[Path],
    as_of: date,
) -> int:
    records = validate_registry(load_document(registry_path), load_document(schema_path), as_of=as_of)
    for path in reference_paths:
        validate_component_reference(load_document(path), repository_root=root, as_of=as_of)
    return len(records)
