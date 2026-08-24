from __future__ import annotations

from datetime import date
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
from typing import Any

from .model import WorkspaceError


SCHEMA_VERSION = 1
POLICY_VERSION = 1
MAX_DOCUMENT_BYTES = 1024 * 1024
MAX_REVIEW_DAYS = 366
CANONICALIZATION = "atrinik-json-v1"
REGISTRY_PATH = Path("governance/provenance-identities/registry.json")
SCHEMA_PATH = Path("governance/provenance-identities/schema-v1.json")
REVIEWERS_PATH = Path("governance/provenance-identities/reviewers.json")
REVISION_MIGRATION_PATH = Path("governance/provenance-revision-migration.json")
REPLACEMENT_FOUNDATIONS_PATH = Path("governance/replacement-foundations.json")
CLASSIC_TOOLS_PATH = Path("governance/classic-tools.json")
PROVENANCE_FIXTURES_PATH = Path("tests/fixtures/provenance-identities")
MCP_WORKLOAD_PATH = Path("mcp/contract/v1/fixtures/workloads.json")
TRUSTED_SCHEMA_CANONICAL_SHA256 = "d80f49a1b17d00ad203a70229f2649d005bf69b738fc197c5ad6da8f944e57bd"
CONFIDENTIAL_RECORD_ID_PATTERN = re.compile(r"^pir-c-[0-9a-f]{32}$")
PUBLIC_RECORD_ID_PATTERN = re.compile(r"^pir-p-[0-9a-f]{32}$")
RECORD_ID_PATTERN = re.compile(r"^pir-[cp]-[0-9a-f]{32}$")
SCOPE_BINDING_PATTERN = re.compile(r"^psb-[0-9a-f]{32}$")
RESTRICTED_ID_PATTERN = re.compile(r"^restricted-[0-9a-f]{32}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
REVISION_PATTERN = re.compile(r"^[0-9a-f]{40}$")
REVIEWER_PATTERN = re.compile(r"^github:[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")
KEY_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{7,63}$")
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
    """Serialize the deliberately floating-point-free contract deterministically."""

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


def _repository_path(value: object, context: str) -> str:
    text = _text(value, context)
    path = PurePosixPath(text)
    if (
        text.startswith("/")
        or "\\" in text
        or path.as_posix() != text
        or any(part in {"", ".", ".."} for part in path.parts)
    ):
        raise WorkspaceError(f"{context}: must be a safe repository-relative path")
    return text


def _validate_schema(schema: dict[str, Any]) -> None:
    if sha256(canonical_bytes(schema)) != TRUSTED_SCHEMA_CANONICAL_SHA256:
        raise WorkspaceError("provenance identity schema differs from the trusted version")
    if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
        raise WorkspaceError("provenance identity schema must use draft 2020-12")
    if schema.get("$id") != "https://atrinik.org/schema/provenance-identities-v1.json":
        raise WorkspaceError("provenance identity schema has an unexpected $id")
    if schema.get("type") != "object" or schema.get("additionalProperties") is not False:
        raise WorkspaceError("provenance identity schema must reject non-object or extra fields")
    version = schema.get("properties", {}).get("schema_version", {}).get("const")
    if type(version) is not int or version != SCHEMA_VERSION:
        raise WorkspaceError("provenance identity schema has an unsupported version")


def validate_reviewers(value: dict[str, Any], *, as_of: date) -> dict[str, dict[str, Any]]:
    _exact_keys(value, {"reviewers", "schema_version"}, "provenance reviewer registry")
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        raise WorkspaceError("provenance reviewer registry has an unsupported version")
    raw = value["reviewers"]
    if not isinstance(raw, list) or not raw:
        raise WorkspaceError("provenance reviewer registry must contain reviewers")
    result: dict[str, dict[str, Any]] = {}
    for index, reviewer in enumerate(raw):
        context = f"provenance reviewer {index}"
        if not isinstance(reviewer, dict):
            raise WorkspaceError(f"{context}: must be an object")
        _exact_keys(
            reviewer,
            {"effective_on", "expires_on", "identity", "key_id", "public_key", "status", "synthetic"},
            context,
        )
        identity = reviewer["identity"]
        if not isinstance(identity, str) or not REVIEWER_PATTERN.fullmatch(identity):
            raise WorkspaceError(f"{context}.identity: invalid GitHub identity")
        key_id = reviewer["key_id"]
        if not isinstance(key_id, str) or not KEY_ID_PATTERN.fullmatch(key_id):
            raise WorkspaceError(f"{context}.key_id: invalid key identifier")
        if key_id in result:
            raise WorkspaceError(f"{context}: duplicate key identifier")
        if reviewer["status"] not in {"active", "revoked"}:
            raise WorkspaceError(f"{context}.status: invalid status")
        if not isinstance(reviewer["synthetic"], bool):
            raise WorkspaceError(f"{context}.synthetic: must be a boolean")
        effective = _iso_date(reviewer["effective_on"], f"{context}.effective_on")
        expires = _iso_date(reviewer["expires_on"], f"{context}.expires_on")
        if effective > expires:
            raise WorkspaceError(f"{context}: invalid effective interval")
        public_key = _text(reviewer["public_key"], f"{context}.public_key")
        if not re.fullmatch(r"ssh-ed25519 [A-Za-z0-9+/]+={0,2}", public_key):
            raise WorkspaceError(f"{context}.public_key: must be an Ed25519 SSH key")
        result[key_id] = reviewer
    if list(result) != sorted(result):
        raise WorkspaceError("provenance reviewers must be sorted by key_id")
    return result


def _approval_payload(record: dict[str, Any]) -> bytes:
    return canonical_bytes(
        {key: value for key, value in record.items() if key not in {"approval", "integrity"}}
    )


def _verify_approval(
    approval: object,
    payload: bytes,
    *,
    reviewer_identity: str,
    reviewed_on: date,
    reviewers: dict[str, dict[str, Any]],
    synthetic: bool,
    context: str,
) -> None:
    if not isinstance(approval, dict):
        raise WorkspaceError(f"{context}: must be an object")
    _exact_keys(approval, {"key_id", "signature"}, context)
    key_id = approval["key_id"]
    if key_id not in reviewers:
        raise WorkspaceError(f"{context}.key_id: reviewer key is not authorized")
    reviewer = reviewers[key_id]
    if reviewer["identity"] != reviewer_identity:
        raise WorkspaceError(f"{context}: key does not belong to the named reviewer")
    if reviewer["status"] != "active":
        raise WorkspaceError(f"{context}: reviewer key is revoked")
    if reviewer["synthetic"] != synthetic:
        raise WorkspaceError(f"{context}: synthetic reviewer boundary mismatch")
    if not (
        _iso_date(reviewer["effective_on"], f"{context}.effective_on")
        <= reviewed_on
        <= _iso_date(reviewer["expires_on"], f"{context}.expires_on")
    ):
        raise WorkspaceError(f"{context}: reviewer key was not effective at review")
    signature = _text(approval["signature"], f"{context}.signature")
    if not signature.startswith("-----BEGIN SSH SIGNATURE-----\n") or not signature.endswith(
        "\n-----END SSH SIGNATURE-----"
    ):
        raise WorkspaceError(f"{context}.signature: invalid SSH signature envelope")
    try:
        with tempfile.TemporaryDirectory(prefix="atrinik-provenance-signature-") as temporary:
            allowed = Path(temporary) / "allowed_signers"
            signature_path = Path(temporary) / "signature"
            with open(
                allowed, "x", encoding="utf-8", opener=_private_file_opener
            ) as stream:
                # Public verifier material, not a secret; ssh-keygen requires a file.
                stream.write(f"{reviewer_identity} {reviewer['public_key']}\n")  # lgtm[py/clear-text-storage-sensitive-data]
            with open(
                signature_path,
                "x", encoding="utf-8", opener=_private_file_opener
            ) as stream:
                # The checked-in public signature contains no private key material.
                stream.write(signature + "\n")  # lgtm[py/clear-text-storage-sensitive-data]
            result = subprocess.run(
                [
                    "ssh-keygen",
                    "-Y",
                    "verify",
                    "-f",
                    str(allowed),
                    "-I",
                    reviewer_identity,
                    "-n",
                    "atrinik-provenance-v1",
                    "-s",
                    str(signature_path),
                ],
                input=payload,
                capture_output=True,
                timeout=10,
            )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise WorkspaceError(f"{context}: cannot verify reviewer signature") from exc
    if result.returncode != 0:
        raise WorkspaceError(f"{context}: reviewer signature is invalid")


def _private_file_opener(path: str, flags: int) -> int:
    return os.open(path, flags, 0o600)


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


def _validate_common_record(
    record: dict[str, Any],
    context: str,
    as_of: date,
    reviewers: dict[str, dict[str, Any]],
) -> None:
    identifier = record.get("record_id")
    if not isinstance(identifier, str) or not RECORD_ID_PATTERN.fullmatch(identifier):
        raise WorkspaceError(f"{context}.record_id: must be an opaque pir identifier")
    if type(record.get("policy_version")) is not int or record["policy_version"] != POLICY_VERSION:
        raise WorkspaceError(f"{context}.policy_version: unsupported policy version")
    if record.get("status") not in {"active", "revoked", "superseded"}:
        raise WorkspaceError(f"{context}.status: unsupported status")
    reviewer = record.get("reviewer")
    if not isinstance(reviewer, str) or not REVIEWER_PATTERN.fullmatch(reviewer):
        raise WorkspaceError(f"{context}.reviewer: must be a public GitHub reviewer identity")
    reviewed = _iso_date(record.get("reviewed_on"), f"{context}.reviewed_on")
    expires = _iso_date(record.get("expires_on"), f"{context}.expires_on")
    if reviewed > as_of:
        raise WorkspaceError(f"{context}: review date is in the future")
    if expires <= reviewed or (expires - reviewed).days > MAX_REVIEW_DAYS:
        raise WorkspaceError(f"{context}: expires_on must be after reviewed_on")
    if record.get("status") == "active" and expires < as_of:
        raise WorkspaceError(f"{context}: active attestation is stale")
    claims = _string_array(record.get("claims"), f"{context}.claims")
    if not set(claims) <= KNOWN_CLAIMS:
        raise WorkspaceError(f"{context}.claims: contains an unknown claim")
    _validate_integrity(record, context)
    _verify_approval(
        record.get("approval"),
        _approval_payload(record),
        reviewer_identity=reviewer,
        reviewed_on=reviewed,
        reviewers=reviewers,
        synthetic=record["synthetic"],
        context=f"{context}.approval",
    )


def _validate_confidential_record(
    record: dict[str, Any], context: str, as_of: date, reviewers: dict[str, dict[str, Any]]
) -> None:
    _walk_confidential_keys(record, context)
    _exact_keys(
        record,
        {
            "claims",
            "approval",
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
            "status_detail",
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
    if not CONFIDENTIAL_RECORD_ID_PATTERN.fullmatch(record["record_id"]):
        raise WorkspaceError(f"{context}.record_id: confidential identifiers must be random hex")
    _validate_common_record(record, context, as_of, reviewers)
    if set(record["claims"]) != KNOWN_CLAIMS:
        raise WorkspaceError(f"{context}.claims: confidential review must prove all claims")


def _validate_public_alias_record(
    record: dict[str, Any], context: str, as_of: date, reviewers: dict[str, dict[str, Any]]
) -> None:
    _exact_keys(
        record,
        {
            "aliases",
            "approval",
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
            "status_detail",
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
    authorized = _iso_date(
        authorization["authorized_on"],
        f"{context}.publication_authorization.authorized_on",
    )
    if authorized > _iso_date(record["reviewed_on"], f"{context}.reviewed_on"):
        raise WorkspaceError(f"{context}.publication_authorization: cannot postdate review")
    if _string_array(authorization["fields"], f"{context}.publication_authorization.fields") != [
        "aliases",
        "display_name",
    ]:
        raise WorkspaceError(f"{context}.publication_authorization: must explicitly cover all identity fields")
    if not RESTRICTED_ID_PATTERN.fullmatch(str(authorization["restricted_record_id"])):
        raise WorkspaceError(f"{context}.publication_authorization.restricted_record_id: invalid identifier")
    if not PUBLIC_RECORD_ID_PATTERN.fullmatch(record["record_id"]):
        raise WorkspaceError(f"{context}.record_id: public identifiers must be random hex")
    _validate_common_record(record, context, as_of, reviewers)


def validate_registry(
    registry: dict[str, Any],
    schema: dict[str, Any],
    reviewers_value: dict[str, Any],
    *,
    as_of: date,
) -> dict[str, dict[str, Any]]:
    _validate_schema(schema)
    reviewers = validate_reviewers(reviewers_value, as_of=as_of)
    _exact_keys(
        registry,
        {"policy_version", "records", "reviewers", "schema", "schema_version"},
        "provenance identity registry",
    )
    if type(registry["schema_version"]) is not int or registry["schema_version"] != SCHEMA_VERSION:
        raise WorkspaceError("provenance identity registry has an unsupported schema version")
    if type(registry["policy_version"]) is not int or registry["policy_version"] != POLICY_VERSION:
        raise WorkspaceError("provenance identity registry has an unsupported policy version")
    if registry["schema"] != SCHEMA_PATH.as_posix():
        raise WorkspaceError("provenance identity registry names an unexpected schema")
    if registry["reviewers"] != REVIEWERS_PATH.as_posix():
        raise WorkspaceError("provenance identity registry names unexpected reviewers")
    records = registry["records"]
    if not isinstance(records, list):
        raise WorkspaceError("provenance identity registry records must be an array")
    result: dict[str, dict[str, Any]] = {}
    bindings: set[str] = set()
    restricted_ids: set[str] = set()
    restricted_integrities: set[str] = set()
    for index, record in enumerate(records):
        context = f"provenance identity record {index}"
        if not isinstance(record, dict):
            raise WorkspaceError(f"{context}: must be an object")
        identifier = record.get("record_id")
        if identifier in result:
            raise WorkspaceError(f"{context}: duplicate record identifier {identifier}")
        record_type = record.get("record_type")
        if record_type == "confidential-attestation":
            _validate_confidential_record(record, context, as_of, reviewers)
            binding = record["scope_binding"]
            restricted_id = record["restricted_evidence"]["record_id"]
            restricted_integrity = record["restricted_evidence"]["integrity"]
            if binding in bindings:
                raise WorkspaceError(f"{context}: duplicate scope binding")
            if restricted_id in restricted_ids:
                raise WorkspaceError(f"{context}: duplicate restricted evidence identifier")
            if restricted_integrity in restricted_integrities:
                raise WorkspaceError(f"{context}: duplicate restricted evidence integrity")
            bindings.add(binding)
            restricted_ids.add(restricted_id)
            restricted_integrities.add(restricted_integrity)
        elif record_type == "public-alias":
            _validate_public_alias_record(record, context, as_of, reviewers)
        else:
            raise WorkspaceError(f"{context}.record_type: unsupported record type")
        identifier = record["record_id"]
        result[identifier] = record
    if list(result) != sorted(result):
        raise WorkspaceError("provenance identity registry records must be sorted by record_id")
    for identifier, record in result.items():
        detail = record["status_detail"]
        if not isinstance(detail, dict):
            raise WorkspaceError(f"record {identifier}.status_detail: must be an object")
        effective = _iso_date(detail.get("effective_on"), f"record {identifier}.status_detail.effective_on")
        if effective < _iso_date(record["reviewed_on"], f"record {identifier}.reviewed_on"):
            raise WorkspaceError(f"record {identifier}: status predates review")
        if effective > as_of:
            raise WorkspaceError(f"record {identifier}: status effective date is in the future")
        if record["status"] == "active":
            _exact_keys(detail, {"effective_on"}, f"record {identifier}.status_detail")
        elif record["status"] == "revoked":
            _exact_keys(detail, {"effective_on", "reason"}, f"record {identifier}.status_detail")
            if detail["reason"] not in {"compromise", "correction", "dispute", "withdrawal"}:
                raise WorkspaceError(f"record {identifier}: invalid revocation reason")
        else:
            _exact_keys(detail, {"effective_on", "superseded_by"}, f"record {identifier}.status_detail")
            target = detail["superseded_by"]
            if target == identifier or target not in result:
                raise WorkspaceError(f"record {identifier}: invalid supersession target")
            if result[target]["status"] != "active":
                raise WorkspaceError(f"record {identifier}: supersession target is not active")
    return result


def _git_environment() -> dict[str, str]:
    environment = os.environ.copy()
    for name in (
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_CEILING_DIRECTORIES",
        "GIT_COMMON_DIR",
        "GIT_DIR",
        "GIT_DISCOVERY_ACROSS_FILESYSTEM",
        "GIT_INDEX_FILE",
        "GIT_OBJECT_DIRECTORY",
        "GIT_REPLACE_REF_BASE",
        "GIT_WORK_TREE",
    ):
        environment.pop(name, None)
    environment["GIT_NO_REPLACE_OBJECTS"] = "1"
    return environment


def _git_output(repository_root: Path, arguments: list[str], context: str) -> bytes:
    try:
        result = subprocess.run(
            ["git", *arguments],
            cwd=repository_root,
            check=True,
            capture_output=True,
            timeout=10,
            env=_git_environment(),
        )
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        raise WorkspaceError(context) from exc
    return result.stdout


def _validate_repository_trust(
    repository_root: Path, revision: str, trusted_ref: str
) -> None:
    if _git_output(
        repository_root,
        ["rev-parse", "--is-shallow-repository"],
        "cannot inspect coordinator checkout depth",
    ).strip() != b"false":
        raise WorkspaceError("coordinator checkout must be non-shallow")
    common = _git_output(
        repository_root,
        ["rev-parse", "--path-format=absolute", "--git-common-dir"],
        "cannot resolve coordinator Git directory",
    ).decode().strip()
    if (Path(common) / "info" / "grafts").exists():
        raise WorkspaceError("coordinator checkout has legacy grafts")
    origin = _git_output(
        repository_root,
        ["remote", "get-url", "origin"],
        "coordinator checkout requires origin",
    ).decode().strip()
    if origin not in {
        "https://github.com/atrinik/atrinik",
        "https://github.com/atrinik/atrinik.git",
        "git@github.com:atrinik/atrinik.git",
    }:
        raise WorkspaceError("coordinator origin is not atrinik/atrinik")
    _git_output(
        repository_root,
        ["merge-base", "--is-ancestor", revision, trusted_ref],
        f"coordinator revision is not reachable from trusted ref {trusted_ref}",
    )


def _git_blob(repository_root: Path, revision: str, path: str) -> bytes:
    object_name = f"{revision}:{path}"
    raw_size = _git_output(
        repository_root,
        ["cat-file", "-s", object_name],
        f"immutable reference cannot size {object_name}",
    )
    try:
        size = int(raw_size)
    except ValueError as exc:
        raise WorkspaceError(f"immutable reference returned invalid size for {object_name}") from exc
    if size > MAX_DOCUMENT_BYTES:
        raise WorkspaceError("immutable reference document exceeds the size limit")
    result = _git_output(
        repository_root,
        ["cat-file", "blob", object_name],
        f"immutable reference cannot resolve {object_name}",
    )
    if len(result) != size:
        raise WorkspaceError("immutable reference blob size changed during read")
    return result


def _preflight_git_output(
    repository_root: Path,
    arguments: list[str],
    object_name: str,
    repository: str,
) -> bytes:
    try:
        result = subprocess.run(
            ["git", *arguments],
            cwd=repository_root,
            check=False,
            capture_output=True,
            timeout=10,
            env=_git_environment(),
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise WorkspaceError(
            f"provenance preflight cannot resolve {repository} object {object_name}"
        ) from exc
    if result.returncode != 0:
        raise WorkspaceError(
            f"provenance preflight cannot resolve {repository} object {object_name}"
        )
    return result.stdout


def _preflight_revision(
    repository_root: Path,
    revision: str,
    trusted_ref: str,
    repository: str,
) -> None:
    _preflight_git_output(
        repository_root,
        ["rev-parse", "--verify", f"{revision}^{{commit}}"],
        f"{revision}^{{commit}}",
        repository,
    )
    _validate_repository_trust(repository_root, revision, trusted_ref)


def _preflight_blob(
    repository_root: Path,
    revision: str,
    path: str,
    repository: str,
    *,
    expected_git_object: str,
    expected_sha256: str,
) -> None:
    object_name = f"{revision}:{path}"
    actual_git_object = _preflight_git_output(
        repository_root,
        ["rev-parse", "--verify", object_name],
        object_name,
        repository,
    ).decode("ascii", errors="strict").strip()
    if actual_git_object != expected_git_object:
        raise WorkspaceError(
            f"provenance preflight object identity changed for {repository} {object_name}"
        )
    raw_size = _preflight_git_output(
        repository_root,
        ["cat-file", "-s", object_name],
        object_name,
        repository,
    )
    try:
        size = int(raw_size)
    except ValueError as exc:
        raise WorkspaceError(
            f"provenance preflight returned an invalid size for {repository} object {object_name}"
        ) from exc
    if size > MAX_DOCUMENT_BYTES:
        raise WorkspaceError(
            f"provenance preflight object {repository} {object_name} exceeds the size limit"
        )
    blob = _preflight_git_output(
        repository_root,
        ["cat-file", "blob", object_name],
        object_name,
        repository,
    )
    if len(blob) != size:
        raise WorkspaceError(
            f"provenance preflight object {repository} {object_name} changed during read"
        )
    if sha256(blob) != expected_sha256:
        raise WorkspaceError(
            f"provenance preflight SHA-256 mismatch for {repository} object {object_name}"
        )


def _revision_text(value: object, context: str) -> str:
    if not isinstance(value, str) or not REVISION_PATTERN.fullmatch(value):
        raise WorkspaceError(f"{context} must be a full Git SHA")
    return value


def _sha256_text(value: object, context: str) -> str:
    if not isinstance(value, str) or not SHA256_PATTERN.fullmatch(value):
        raise WorkspaceError(f"{context} must be a SHA-256 digest")
    return value


def _coordinator_pins(value: object, context: str = "") -> list[tuple[str, str, str]]:
    pins: list[tuple[str, str, str]] = []
    if isinstance(value, dict):
        if value.get("repository") == "atrinik/atrinik" and "revision" in value:
            path = value.get("path")
            if not isinstance(path, str) or not path:
                raise WorkspaceError(f"{context}.path must be present for a coordinator pin")
            pins.append(
                (
                    context,
                    _revision_text(value["revision"], f"{context}.revision"),
                    _repository_path(path, f"{context}.path"),
                )
            )
        for key, child in value.items():
            child_context = f"{context}.{key}" if context else key
            pins.extend(_coordinator_pins(child, child_context))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            pins.extend(_coordinator_pins(child, f"{context}[{index}]"))
    return pins


def _validate_external_reanchors(value: object) -> None:
    if not isinstance(value, list) or not value:
        raise WorkspaceError("provenance revision migration external_reanchors must be non-empty")
    for index, item in enumerate(value):
        context = f"provenance revision migration external_reanchors[{index}]"
        if not isinstance(item, dict):
            raise WorkspaceError(f"{context} must be an object")
        _exact_keys(
            item,
            {"file", "repository", "trusted_ref", "references", "disposition"},
            context,
        )
        _repository_path(item["file"], f"{context}.file")
        if item["repository"] != "atrinik/tools":
            raise WorkspaceError(f"{context}.repository is invalid")
        if item["trusted_ref"] != "origin/main":
            raise WorkspaceError(f"{context}.trusted_ref is invalid")
        _text(item["disposition"], f"{context}.disposition")
        references = item["references"]
        if not isinstance(references, list) or not references:
            raise WorkspaceError(f"{context}.references must be non-empty")
        seen_fields: set[str] = set()
        for reference_index, reference in enumerate(references):
            reference_context = f"{context}.references[{reference_index}]"
            if not isinstance(reference, dict):
                raise WorkspaceError(f"{reference_context} must be an object")
            _exact_keys(
                reference,
                {"field", "old_revision", "new_revision", "old_tree", "new_tree"},
                reference_context,
            )
            field = _text(reference["field"], f"{reference_context}.field")
            if field in seen_fields:
                raise WorkspaceError(f"{reference_context}.field is duplicated")
            seen_fields.add(field)
            for key in ("old_revision", "new_revision", "old_tree", "new_tree"):
                _revision_text(reference[key], f"{reference_context}.{key}")


def _validate_fixture_replacements(
    value: object, coordinator_revision: str
) -> None:
    if not isinstance(value, list) or not value:
        raise WorkspaceError("provenance revision migration fixture_replacements must be non-empty")
    seen: set[tuple[str, int]] = set()
    for index, item in enumerate(value):
        context = f"provenance revision migration fixture_replacements[{index}]"
        if not isinstance(item, dict):
            raise WorkspaceError(f"{context} must be an object")
        _exact_keys(
            item,
            {
                "file",
                "case_id",
                "coordinate_index",
                "repository",
                "old_revision",
                "new_revision",
            },
            context,
        )
        file = _repository_path(item["file"], f"{context}.file")
        case_id = _text(item["case_id"], f"{context}.case_id")
        coordinate_index = item["coordinate_index"]
        if (
            type(coordinate_index) is not int
            or coordinate_index < 0
        ):
            raise WorkspaceError(f"{context}.coordinate_index must be a non-negative integer")
        key = (case_id, coordinate_index)
        if key in seen:
            raise WorkspaceError(f"{context} identifies a duplicate fixture coordinate")
        seen.add(key)
        if item["repository"] != "atrinik/atrinik":
            raise WorkspaceError(f"{context}.repository is invalid")
        _revision_text(item["old_revision"], f"{context}.old_revision")
        _revision_text(item["new_revision"], f"{context}.new_revision")
        if item["new_revision"] != coordinator_revision:
            raise WorkspaceError(f"{context}.new_revision does not use the coordinator anchor")
        if file != MCP_WORKLOAD_PATH.as_posix():
            raise WorkspaceError(f"{context}.file is not the MCP workload fixture")


def _validate_revision_migration(
    value: dict[str, Any],
) -> tuple[
    dict[str, Any],
    dict[str, dict[str, Any]],
    dict[tuple[str, str], dict[str, Any]],
]:
    _exact_keys(
        value,
        {
            "schema_version",
            "decision",
            "issue",
            "rationale",
            "external_reanchors",
            "fixture_replacements",
            "coordinator",
            "replacements",
            "historical_references",
        },
        "provenance revision migration",
    )
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        raise WorkspaceError("provenance revision migration has an unsupported schema version")
    if value["decision"] != "reanchor-unreachable-history":
        raise WorkspaceError("provenance revision migration decision is invalid")
    _text(value["issue"], "provenance revision migration.issue")
    _text(value["rationale"], "provenance revision migration.rationale")
    coordinator = value["coordinator"]
    if not isinstance(coordinator, dict):
        raise WorkspaceError("provenance revision migration coordinator must be an object")
    _exact_keys(
        coordinator,
        {"repository", "trusted_ref", "revision", "objects"},
        "provenance revision migration coordinator",
    )
    if coordinator["repository"] != "atrinik/atrinik":
        raise WorkspaceError("provenance revision migration coordinator repository is invalid")
    if coordinator["trusted_ref"] != "origin/main":
        raise WorkspaceError("provenance revision migration trusted ref is invalid")
    coordinator_revision = _revision_text(
        coordinator["revision"], "provenance revision migration coordinator.revision"
    )
    objects = coordinator["objects"]
    if not isinstance(objects, list) or not objects:
        raise WorkspaceError("provenance revision migration coordinator.objects must be non-empty")
    object_map: dict[str, dict[str, Any]] = {}
    for index, item in enumerate(objects):
        if not isinstance(item, dict):
            raise WorkspaceError(f"provenance revision migration coordinator.objects[{index}] must be an object")
        _exact_keys(
            item,
            {"path", "git_object", "sha256"},
            f"provenance revision migration coordinator.objects[{index}]",
        )
        path = item["path"]
        if not isinstance(path, str) or not path or path in object_map:
            raise WorkspaceError("provenance revision migration coordinator object path is invalid or duplicated")
        _repository_path(path, f"coordinator.objects[{index}].path")
        _revision_text(item["git_object"], f"coordinator.objects[{index}].git_object")
        _sha256_text(item["sha256"], f"coordinator.objects[{index}].sha256")
        object_map[path] = item
    replacements = value["replacements"]
    if not isinstance(replacements, list) or not replacements:
        raise WorkspaceError("provenance revision migration replacements must be non-empty")
    replacement_map: dict[tuple[str, str], dict[str, Any]] = {}
    for index, item in enumerate(replacements):
        if not isinstance(item, dict):
            raise WorkspaceError(f"provenance revision migration replacements[{index}] must be an object")
        required = {
            "file",
            "field",
            "repository",
            "old_revision",
            "new_revision",
            "path",
        }
        optional = {"old_reviewers_sha256", "new_reviewers_sha256"}
        _exact_keys(
            item,
            required | (set(item) & optional),
            f"provenance revision migration replacements[{index}]",
        )
        file = item["file"]
        field = item["field"]
        if not isinstance(file, str) or not file:
            raise WorkspaceError(
                f"provenance revision migration replacements[{index}].file is invalid"
            )
        _repository_path(
            file, f"provenance revision migration replacements[{index}].file"
        )
        if not isinstance(field, str) or not field or field.strip() != field:
            raise WorkspaceError(
                f"provenance revision migration replacements[{index}].field is invalid"
            )
        replacement_key = (file, field)
        if replacement_key in replacement_map:
            raise WorkspaceError(
                "provenance revision migration replacement is duplicated"
            )
        replacement_map[replacement_key] = item
        _revision_text(item["old_revision"], f"replacements[{index}].old_revision")
        _revision_text(item["new_revision"], f"replacements[{index}].new_revision")
        if item.get("repository") != "atrinik/atrinik":
            raise WorkspaceError("provenance revision migration replacement repository is invalid")
        if item["new_revision"] != coordinator_revision:
            raise WorkspaceError(
                "provenance revision migration replacement does not use the coordinator anchor"
            )
        _text(item["path"], f"replacements[{index}].path")
        if bool(set(item) & optional) and not optional <= set(item):
            raise WorkspaceError(
                "provenance revision migration reviewer digests must be provided together"
            )
        if "old_reviewers_sha256" in item:
            _sha256_text(item["old_reviewers_sha256"], f"replacements[{index}].old_reviewers_sha256")
            _sha256_text(item["new_reviewers_sha256"], f"replacements[{index}].new_reviewers_sha256")
    _validate_external_reanchors(value["external_reanchors"])
    _validate_fixture_replacements(value["fixture_replacements"], coordinator_revision)
    historical = value["historical_references"]
    if not isinstance(historical, list) or not historical:
        raise WorkspaceError("provenance revision migration historical_references must be non-empty")
    for index, item in enumerate(historical):
        if not isinstance(item, dict):
            raise WorkspaceError(f"provenance revision migration historical_references[{index}] must be an object")
        _exact_keys(
            item,
            {"file", "context", "revision", "disposition"},
            f"provenance revision migration historical_references[{index}]",
        )
        _revision_text(item["revision"], f"historical_references[{index}].revision")
        if "preserved-unresolved" not in item["disposition"]:
            raise WorkspaceError("provenance revision migration historical disposition must preserve uncertainty")
    return coordinator, object_map, replacement_map


def _migration_field_value(
    document: dict[str, Any], field: str, context: str
) -> object:
    current: object = document
    for part in field.split("."):
        if not part or not isinstance(current, dict) or part not in current:
            raise WorkspaceError(f"{context} does not resolve migration field {field}")
        current = current[part]
    return current


def preflight_provenance_revisions(
    root: Path,
    *,
    migration_path: Path = REVISION_MIGRATION_PATH,
) -> tuple[int, int]:
    if not migration_path.is_absolute():
        migration_path = root / migration_path
    migration = load_document(migration_path)
    coordinator, object_map, replacement_map = _validate_revision_migration(migration)
    revision = coordinator["revision"]
    _preflight_revision(root, revision, coordinator["trusted_ref"], coordinator["repository"])

    governance_paths = (REPLACEMENT_FOUNDATIONS_PATH, CLASSIC_TOOLS_PATH)
    pins: list[tuple[str, str, str]] = []
    for relative_path in governance_paths:
        document = load_document(root / relative_path)
        pins.extend(_coordinator_pins(document, relative_path.as_posix()))
    if not pins:
        raise WorkspaceError("provenance preflight found no active coordinator revision pins")
    for context, pinned_revision, path in pins:
        if pinned_revision != revision:
            raise WorkspaceError(
                f"provenance preflight active pin {context} does not use migration anchor {revision}"
            )
        expected = object_map.get(path)
        if expected is None:
            raise WorkspaceError(
                f"provenance preflight migration lacks coordinator object {path}"
            )
        _preflight_blob(
            root,
            pinned_revision,
            path,
            coordinator["repository"],
            expected_git_object=expected["git_object"],
            expected_sha256=expected["sha256"],
        )

    for (relative_file, field), replacement in replacement_map.items():
        target = root / relative_file
        document = load_document(target)
        actual = _migration_field_value(document, field, relative_file)
        if actual != replacement["new_revision"]:
            raise WorkspaceError(
                f"provenance preflight migration row {relative_file}:{field} is not applied"
            )

    workloads = load_document(root / MCP_WORKLOAD_PATH)
    cases = workloads.get("cases")
    if not isinstance(cases, list):
        raise WorkspaceError("provenance preflight MCP workload cases must be an array")
    fixture_replacements = migration["fixture_replacements"]
    assert isinstance(fixture_replacements, list)
    replacement_coordinates: dict[tuple[str, int], dict[str, Any]] = {}
    for item in fixture_replacements:
        assert isinstance(item, dict)
        replacement_coordinates[(item["case_id"], item["coordinate_index"])] = item
    fixture_coordinates: set[tuple[str, int]] = set()
    for case in cases:
        if not isinstance(case, dict) or not isinstance(case.get("id"), str):
            raise WorkspaceError("provenance preflight MCP workload case is invalid")
        expected = case.get("expected")
        if not isinstance(expected, dict):
            raise WorkspaceError(
                f"provenance preflight MCP workload case {case['id']} has no expected object"
            )
        coordinates = expected.get("coordinates")
        if not isinstance(coordinates, list):
            raise WorkspaceError(
                f"provenance preflight MCP workload case {case['id']} coordinates are invalid"
            )
        for coordinate_index, coordinate in enumerate(coordinates):
            if not isinstance(coordinate, dict):
                raise WorkspaceError("provenance preflight MCP coordinate is invalid")
            if coordinate.get("repository") != coordinator["repository"]:
                continue
            coordinate_revision = _revision_text(
                coordinate.get("commit"),
                f"{MCP_WORKLOAD_PATH}: {case['id']} coordinate {coordinate_index}.commit",
            )
            if coordinate_revision != revision:
                raise WorkspaceError(
                    f"provenance preflight MCP fixture {case['id']} coordinate "
                    f"{coordinate_index} does not use migration anchor {revision}"
                )
            key = (case["id"], coordinate_index)
            fixture_coordinates.add(key)
            replacement = replacement_coordinates.get(key)
            if replacement is None or replacement["new_revision"] != coordinate_revision:
                raise WorkspaceError(
                    f"provenance preflight MCP fixture {case['id']} coordinate "
                    f"{coordinate_index} lacks an applied migration row"
                )
            _preflight_revision(root, coordinate_revision, coordinator["trusted_ref"], coordinator["repository"])
    if fixture_coordinates != set(replacement_coordinates):
        raise WorkspaceError("provenance preflight MCP fixture migration rows do not match active coordinates")

    fixture_revisions: set[str] = set()
    fixtures_root = root / PROVENANCE_FIXTURES_PATH
    try:
        fixture_paths = sorted(fixtures_root.rglob("*.json"))
    except OSError as exc:
        raise WorkspaceError("provenance preflight cannot enumerate provenance fixtures") from exc
    for path in fixture_paths:
        document = load_document(path)
        evidence = document.get("evidence_reference")
        if not isinstance(evidence, dict) or evidence.get("repository") != coordinator["repository"]:
            continue
        fixture_revision = _revision_text(
            evidence.get("revision"),
            f"{path}: evidence_reference.revision",
        )
        if fixture_revision != revision:
            raise WorkspaceError(
                f"provenance preflight fixture {path} does not use migration anchor {revision}"
            )
        fixture_revisions.add(fixture_revision)
    if not fixture_revisions:
        raise WorkspaceError("provenance preflight found no coordinator fixture revisions")

    for path, expected in object_map.items():
        _preflight_blob(
            root,
            revision,
            path,
            coordinator["repository"],
            expected_git_object=expected["git_object"],
            expected_sha256=expected["sha256"],
        )
    return 1, len(object_map)


def _migration_scope_payload(
    reference: dict[str, Any],
    replacement: dict[str, Any] | None,
) -> bytes | None:
    if replacement is None or replacement["field"] != "evidence_reference.revision":
        return None
    if "old_reviewers_sha256" not in replacement:
        return None
    evidence = reference["evidence_reference"]
    if evidence["revision"] != replacement["new_revision"]:
        return None
    if evidence["reviewers_sha256"] != replacement["new_reviewers_sha256"]:
        return None
    legacy_reference = dict(reference)
    legacy_evidence = dict(evidence)
    legacy_evidence["revision"] = replacement["old_revision"]
    legacy_evidence["reviewers_sha256"] = replacement["old_reviewers_sha256"]
    legacy_evidence["url"] = (
        f"https://github.com/atrinik/atrinik/blob/{replacement['old_revision']}/"
        f"{REGISTRY_PATH.as_posix()}#{legacy_evidence['record_id']}"
    )
    legacy_reference["evidence_reference"] = legacy_evidence
    return canonical_bytes(
        {key: value for key, value in legacy_reference.items() if key != "scope_approval"}
    )


def _load_bytes(value: bytes, context: str) -> dict[str, Any]:
    try:
        result = json.loads(value.decode("utf-8"), object_pairs_hook=_object_pairs)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise WorkspaceError(f"{context}: invalid JSON") from exc
    if not isinstance(result, dict):
        raise WorkspaceError(f"{context}: root must be an object")
    return result


def validate_component_reference(
    reference: dict[str, Any],
    *,
    repository_root: Path,
    as_of: date,
    trusted_ref: str,
    current_records: dict[str, dict[str, Any]],
    current_reviewers: dict[str, dict[str, Any]],
    migration_replacement: dict[str, Any] | None = None,
) -> None:
    _exact_keys(
        reference,
        {
            "destination",
            "evidence_reference",
            "schema_version",
            "scope_approval",
            "scope_binding",
            "source",
            "synthetic",
            "transformation",
        },
        "component provenance record",
    )
    if (
        type(reference["schema_version"]) is not int
        or reference["schema_version"] != SCHEMA_VERSION
    ):
        raise WorkspaceError("component provenance record has an unsupported schema version")
    if not isinstance(reference["synthetic"], bool):
        raise WorkspaceError("component provenance record synthetic must be a boolean")
    for key in ("source", "destination"):
        coordinate = reference[key]
        if not isinstance(coordinate, dict):
            raise WorkspaceError(f"component provenance record {key} must be an object")
        required = {"path", "repository", "revision"} if key == "source" else {"path", "repository"}
        _exact_keys(coordinate, required, f"component provenance record {key}")
        if not re.fullmatch(
            r"atrinik/[a-z0-9][a-z0-9._-]*", str(coordinate["repository"])
        ):
            raise WorkspaceError(f"component provenance record {key}.repository is invalid")
        _repository_path(coordinate["path"], f"component provenance record {key}.path")
        if key == "source":
            source_revision = _text(coordinate["revision"], "source.revision")
            if not re.fullmatch(
                r"[0-9a-f]{40}(?:\.\.[0-9a-f]{40})?", source_revision
            ):
                raise WorkspaceError(
                    "component provenance source.revision must be a full Git SHA or exact SHA range"
                )
    _text(reference["transformation"], "component provenance record transformation")
    binding = reference["scope_binding"]
    if not isinstance(binding, str) or not SCOPE_BINDING_PATTERN.fullmatch(binding):
        raise WorkspaceError("component provenance scope_binding is invalid")
    evidence = reference["evidence_reference"]
    if not isinstance(evidence, dict):
        raise WorkspaceError("component provenance evidence_reference must be an object")
    _exact_keys(
        evidence,
        {
            "record_id",
            "registry_sha256",
            "repository",
            "reviewers_sha256",
            "revision",
            "schema_sha256",
            "url",
        },
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
        raise WorkspaceError(
            "component provenance reference URL is not the canonical immutable permalink"
        )
    _validate_repository_trust(repository_root, revision, trusted_ref)
    for key in ("registry_sha256", "reviewers_sha256", "schema_sha256"):
        if not isinstance(evidence[key], str) or not SHA256_PATTERN.fullmatch(
            evidence[key]
        ):
            raise WorkspaceError(f"component provenance reference {key} is invalid")
    registry_blob = _git_blob(repository_root, revision, REGISTRY_PATH.as_posix())
    schema_blob = _git_blob(repository_root, revision, SCHEMA_PATH.as_posix())
    reviewers_blob = _git_blob(repository_root, revision, REVIEWERS_PATH.as_posix())
    if sha256(registry_blob) != evidence["registry_sha256"]:
        raise WorkspaceError("component provenance reference registry digest does not match")
    if sha256(schema_blob) != evidence["schema_sha256"]:
        raise WorkspaceError("component provenance reference schema digest does not match")
    if sha256(reviewers_blob) != evidence["reviewers_sha256"]:
        raise WorkspaceError("component provenance reference reviewers digest does not match")
    records = validate_registry(
        _load_bytes(registry_blob, "referenced registry"),
        _load_bytes(schema_blob, "referenced schema"),
        _load_bytes(reviewers_blob, "referenced reviewers"),
        as_of=as_of,
    )
    if record_id not in records:
        raise WorkspaceError("component provenance reference record is absent")
    record = records[record_id]
    if record["status"] != "active":
        raise WorkspaceError("component provenance reference record is not active")
    if record["record_type"] != "confidential-attestation":
        raise WorkspaceError(
            "component provenance reference must select a confidential attestation"
        )
    if record["scope_binding"] != reference["scope_binding"]:
        raise WorkspaceError("component provenance scope binding does not match the attestation")
    current = current_records.get(record_id)
    if current is None or current["status"] != "active":
        raise WorkspaceError(
            "component provenance record is not active in the current registry"
        )
    if current["scope_binding"] != reference["scope_binding"]:
        raise WorkspaceError("current provenance scope binding differs from the pinned record")
    if current["integrity"]["digest"] != record["integrity"]["digest"]:
        raise WorkspaceError("current provenance record differs from the pinned attestation")
    scope_payload = canonical_bytes(
        {key: value for key, value in reference.items() if key != "scope_approval"}
    )
    try:
        _verify_approval(
            reference["scope_approval"],
            scope_payload,
            reviewer_identity=record["reviewer"],
            reviewed_on=_iso_date(record["reviewed_on"], "referenced record reviewed_on"),
            reviewers=current_reviewers,
            synthetic=record["synthetic"],
            context="component provenance scope_approval",
        )
    except WorkspaceError as current_error:
        legacy_payload = _migration_scope_payload(reference, migration_replacement)
        if legacy_payload is None:
            raise
        try:
            _verify_approval(
                reference["scope_approval"],
                legacy_payload,
                reviewer_identity=record["reviewer"],
                reviewed_on=_iso_date(record["reviewed_on"], "referenced record reviewed_on"),
                reviewers=current_reviewers,
                synthetic=record["synthetic"],
                context="component provenance migrated scope_approval",
            )
        except WorkspaceError:
            raise current_error
    if record["synthetic"] != reference["synthetic"]:
        raise WorkspaceError(
            "component provenance synthetic boundary does not match the attestation"
        )


def validate_paths(
    root: Path,
    *,
    registry_path: Path,
    schema_path: Path,
    reviewers_path: Path,
    reference_paths: list[Path],
    as_of: date,
    trusted_ref: str,
) -> int:
    reviewers_value = load_document(reviewers_path)
    reviewers = validate_reviewers(reviewers_value, as_of=as_of)
    records = validate_registry(
        load_document(registry_path),
        load_document(schema_path),
        reviewers_value,
        as_of=as_of,
    )
    migration_replacements: dict[tuple[str, str], dict[str, Any]] = {}
    if reference_paths:
        _validate_repository_trust(root, trusted_ref, trusted_ref)
        trusted_registry = _load_bytes(
            _git_blob(root, trusted_ref, REGISTRY_PATH.as_posix()),
            "trusted current registry",
        )
        trusted_schema = _load_bytes(
            _git_blob(root, trusted_ref, SCHEMA_PATH.as_posix()),
            "trusted current schema",
        )
        trusted_reviewers_value = _load_bytes(
            _git_blob(root, trusted_ref, REVIEWERS_PATH.as_posix()),
            "trusted current reviewers",
        )
        records = validate_registry(
            trusted_registry,
            trusted_schema,
            trusted_reviewers_value,
            as_of=as_of,
        )
        reviewers = validate_reviewers(trusted_reviewers_value, as_of=as_of)
        migration_path = root / REVISION_MIGRATION_PATH
        if migration_path.is_file():
            _, _, migration_replacements = _validate_revision_migration(
                load_document(migration_path)
            )
    for path in reference_paths:
        try:
            relative_path = path.resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            relative_path = ""
        validate_component_reference(
            load_document(path),
            repository_root=root,
            as_of=as_of,
            trusted_ref=trusted_ref,
            current_records=records,
            current_reviewers=reviewers,
            migration_replacement=migration_replacements.get(
                (relative_path, "evidence_reference.revision")
            ),
        )
    return len(records)
