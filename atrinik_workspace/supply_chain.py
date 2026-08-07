from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from typing import Any, Iterable

from .model import Manifest, WorkspaceError, load_json, require_keys


SCHEMA_VERSION = 1
ACTION_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
CHECKSUM_PATTERN = re.compile(r"^sha256:[0-9a-f]{64}$")
SPDX_PATTERN = re.compile(r"^(?:[A-Za-z0-9-.+]+|NOASSERTION|LicenseRef-[A-Za-z0-9-.]+)$")
ACTION_REFERENCE_PATTERN = re.compile(
    r"^\s*(?:-\s*)?uses:\s*"
    r"([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*)"
    r"@([^\s#]+)(?:\s*#\s*([^\s]+))?\s*$",
    re.MULTILINE,
)
USES_DECLARATION_PATTERN = re.compile(
    r"^\s*(?:-\s*)?uses:\s*(.*?)\s*$", re.MULTILINE
)
FROM_PATTERN = re.compile(
    r"^\s*FROM(?:\s+--platform=[^\s]+)?\s+([^\s]+)(?:\s+AS\s+([^\s]+))?",
    re.IGNORECASE,
)
SYNTAX_PATTERN = re.compile(r"^\s*#\s*syntax=([^\s]+)", re.IGNORECASE | re.MULTILINE)
DOCKER_PULL_PATTERN = re.compile(r"\bdocker\s+pull\s+([^\s'\"\\]+)")
RUNNER_PATTERN = re.compile(r"^\s*runs-on:\s*(.*?)\s*$", re.MULTILINE)
DEPENDENCY_FILE_NAMES = {
    "catalog.lock.json",
    "dependencies.lock.json",
    "devcontainer-lock.json",
    "package-lock.json",
    "package.json",
    "pyproject.toml",
    "requirements-dev.txt",
}
VENDORED_FILE_NAMES = {"uthash.h", "utarray.h", "utlist.h"}
DEPENDENCY_KINDS = {
    "container-feature",
    "container-image",
    "external-tool",
    "github-action",
    "language-package-set",
    "source-archive",
    "system-library",
    "system-package-set",
    "toolchain",
    "vendored-source",
}
DISPOSITIONS = {"isolate", "remove", "replace", "retain"}
MAX_METADATA_BYTES = 16 * 1024 * 1024


@dataclass(frozen=True)
class Evidence:
    repository: str
    path: str
    contains: str


@dataclass(frozen=True)
class Dependency:
    identifier: str
    name: str
    kind: str
    owner: str
    scope: tuple[str, ...]
    required: bool
    version: str
    version_source: str
    license: str
    source_url: str
    locator: str
    commit: str | None
    checksum: str | None
    acquisition: str
    update_cadence_days: int
    update_mechanism: str
    eol_response: str
    validation: str
    disposition: str
    packages: tuple[str, ...]
    evidence: tuple[Evidence, ...]


@dataclass(frozen=True)
class Repository:
    name: str
    repository: str
    supported: bool
    role: str


class Inventory:
    def __init__(
        self,
        organization: str,
        created: str,
        repositories: list[Repository],
        dependencies: list[Dependency],
    ):
        self.organization = organization
        self.created = created
        self.repositories = repositories
        self.dependencies = dependencies
        self.repositories_by_name = {repository.name: repository for repository in repositories}
        self.dependencies_by_id = {dependency.identifier: dependency for dependency in dependencies}

    @classmethod
    def load(cls, path: Path, manifest_path: Path) -> "Inventory":
        root = load_json(path)
        if not isinstance(root, dict):
            raise WorkspaceError("supply-chain inventory root must be an object")
        require_keys(
            root,
            {"schema_version", "organization", "created", "repositories", "dependencies"},
            "supply-chain inventory",
        )
        if root["schema_version"] != SCHEMA_VERSION:
            raise WorkspaceError("unsupported supply-chain inventory schema")
        if root["organization"] != "atrinik":
            raise WorkspaceError("supply-chain organization must be atrinik")
        created = _text(root["created"], "inventory.created")

        raw_repositories = root["repositories"]
        if not isinstance(raw_repositories, list) or not raw_repositories:
            raise WorkspaceError("inventory.repositories must be a non-empty array")
        repositories: list[Repository] = []
        repository_names: set[str] = set()
        repository_coordinates: set[str] = set()
        for index, value in enumerate(raw_repositories):
            context = f"inventory repository {index}"
            if not isinstance(value, dict):
                raise WorkspaceError(f"{context} must be an object")
            require_keys(value, {"name", "repository", "supported", "role"}, context)
            name = _identifier(value["name"], f"{context}.name")
            coordinate = _text(value["repository"], f"{context}.repository")
            if not re.fullmatch(r"atrinik/[a-z0-9][a-z0-9._-]*", coordinate):
                raise WorkspaceError(f"{context}.repository must be an Atrinik repository")
            if not isinstance(value["supported"], bool):
                raise WorkspaceError(f"{context}.supported must be a boolean")
            role = _text(value["role"], f"{context}.role")
            if name in repository_names or coordinate in repository_coordinates:
                raise WorkspaceError(f"duplicate inventory repository: {name}")
            repository_names.add(name)
            repository_coordinates.add(coordinate)
            repositories.append(Repository(name, coordinate, value["supported"], role))

        manifest = Manifest.load(manifest_path)
        expected_supported = {"atrinik", *(component.name for component in manifest.components)}
        actual_supported = {repository.name for repository in repositories if repository.supported}
        if actual_supported != expected_supported:
            missing = sorted(expected_supported - actual_supported)
            extra = sorted(actual_supported - expected_supported)
            raise WorkspaceError(
                "inventory supported repositories do not match components.json: "
                f"missing={missing}, extra={extra}"
            )
        expected_coordinates = {component.name: component.repository for component in manifest.components}
        expected_coordinates["atrinik"] = "atrinik/atrinik"
        for repository in repositories:
            if repository.supported and expected_coordinates[repository.name] != repository.repository:
                raise WorkspaceError(
                    f"inventory repository mismatch for {repository.name}: {repository.repository}"
                )

        raw_dependencies = root["dependencies"]
        if not isinstance(raw_dependencies, list) or not raw_dependencies:
            raise WorkspaceError("inventory.dependencies must be a non-empty array")
        dependencies: list[Dependency] = []
        identifiers: set[str] = set()
        for index, value in enumerate(raw_dependencies):
            dependencies.append(
                _load_dependency(value, index, repository_names, identifiers)
            )
        if [dependency.identifier for dependency in dependencies] != sorted(identifiers):
            raise WorkspaceError("inventory dependencies must be sorted by id")
        action_locators = [
            dependency.locator
            for dependency in dependencies
            if dependency.kind == "github-action"
        ]
        if len(action_locators) != len(set(action_locators)):
            raise WorkspaceError("inventory GitHub Action locators must be unique")
        return cls(root["organization"], created, repositories, dependencies)

    def validate_schema(self, schema_path: Path) -> None:
        schema = load_json(schema_path)
        if not isinstance(schema, dict):
            raise WorkspaceError("supply-chain schema root must be an object")
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise WorkspaceError("supply-chain schema must use JSON Schema draft 2020-12")
        if schema.get("$id") != "https://atrinik.org/schema/supply-chain-inventory-v1.json":
            raise WorkspaceError("supply-chain schema has an unexpected $id")
        if schema.get("additionalProperties") is not False:
            raise WorkspaceError("supply-chain schema must reject additional root properties")

    def audit(self, roots: dict[str, Path], *, require_all: bool = True) -> list[str]:
        supported = {repository.name for repository in self.repositories if repository.supported}
        supplied = set(roots)
        if require_all and supplied != supported:
            raise WorkspaceError(
                "supply-chain audit roots are incomplete: "
                f"missing={sorted(supported - supplied)}, extra={sorted(supplied - supported)}"
            )
        unknown = supplied - supported
        if unknown:
            raise WorkspaceError(f"unknown supply-chain audit repositories: {sorted(unknown)}")
        normalized: dict[str, Path] = {}
        for name, root in roots.items():
            resolved = root.resolve(strict=True)
            if not resolved.is_dir():
                raise WorkspaceError(f"audit root is not a directory: {resolved}")
            normalized[name] = resolved

        action_dependencies = {
            dependency.locator: dependency
            for dependency in self.dependencies
            if dependency.kind == "github-action"
        }
        runner_dependencies = {
            dependency.locator: dependency
            for dependency in self.dependencies
            if dependency.kind == "toolchain"
            and dependency.locator.startswith("github-hosted-runner/")
        }
        evidence_paths = {
            (evidence.repository, evidence.path)
            for dependency in self.dependencies
            for evidence in dependency.evidence
        }
        messages: list[str] = []
        observed_action_scopes = {
            locator: set() for locator in action_dependencies
        }
        observed_runner_scopes = {
            locator: set() for locator in runner_dependencies
        }
        for dependency in self.dependencies:
            for evidence in dependency.evidence:
                if evidence.repository not in normalized:
                    continue
                path = _safe_repository_path(normalized[evidence.repository], evidence.path)
                text = _read_metadata(path)
                if evidence.contains not in text:
                    raise WorkspaceError(
                        f"{dependency.identifier}: expected text is absent from "
                        f"{evidence.repository}/{evidence.path}: {evidence.contains}"
                    )

        for repository_name, root in sorted(normalized.items()):
            tracked = _tracked_files(root)
            if ".gitmodules" in tracked:
                raise WorkspaceError(f"{repository_name}: Git submodules are not supported")
            dependabot = ".github/dependabot.yml"
            if dependabot not in tracked:
                raise WorkspaceError(f"{repository_name}: missing {dependabot}")
            if "package-ecosystem: github-actions" not in _read_metadata(root / dependabot):
                raise WorkspaceError(
                    f"{repository_name}: Dependabot does not own GitHub Actions updates"
                )

            action_count = 0
            for relative in sorted(
                path for path in tracked if path.startswith(".github/workflows/") and path.endswith((".yml", ".yaml"))
            ):
                text = _read_metadata(root / relative)
                for match in USES_DECLARATION_PATTERN.finditer(text):
                    reference = match.group(1).partition("#")[0].strip()
                    if reference.startswith("./"):
                        if ".." in PurePosixPath(reference).parts:
                            raise WorkspaceError(
                                f"{repository_name}/{relative}: local Action escapes "
                                f"its repository: {reference}"
                            )
                        continue
                    if reference.startswith("docker://"):
                        _validate_container_reference(
                            self.dependencies,
                            repository_name,
                            relative,
                            reference.removeprefix("docker://"),
                        )
                        continue
                    if not re.fullmatch(
                        r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+"
                        r"(?:/[A-Za-z0-9_.-]+)*@[^\s#]+",
                        reference,
                    ):
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: unsupported external "
                            f"Action reference {reference or '<empty>'}"
                        )
                for match in ACTION_REFERENCE_PATTERN.finditer(text):
                    locator, commit, comment = match.groups()
                    dependency = action_dependencies.get(locator)
                    if dependency is None:
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: unowned GitHub Action {locator}"
                        )
                    if not ACTION_COMMIT_PATTERN.fullmatch(commit):
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: {locator} is not pinned to a full commit"
                        )
                    if dependency.commit != commit:
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: {locator}@{commit} differs from inventory {dependency.commit}"
                        )
                    if comment != dependency.version:
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: {locator} must retain the '# {dependency.version}' updater hint"
                        )
                    if repository_name not in dependency.scope:
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: {locator} is absent from inventory scope"
                        )
                    observed_action_scopes[locator].add(repository_name)
                    action_count += 1
                for match in DOCKER_PULL_PATTERN.finditer(text):
                    image = match.group(1)
                    _validate_container_reference(
                        self.dependencies, repository_name, relative, image
                    )
                for match in RUNNER_PATTERN.finditer(text):
                    runner = match.group(1).partition("#")[0].strip()
                    if not re.fullmatch(r"[A-Za-z0-9_.-]+", runner):
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: workflow runner must be "
                            f"an explicit literal: {runner or '<empty>'}"
                        )
                    locator = f"github-hosted-runner/{runner}"
                    dependency = runner_dependencies.get(locator)
                    if dependency is None:
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: unowned workflow runner {runner}"
                        )
                    if repository_name not in dependency.scope:
                        raise WorkspaceError(
                            f"{repository_name}/{relative}: {runner} is absent from inventory scope"
                        )
                    observed_runner_scopes[locator].add(repository_name)

            for relative in sorted(tracked):
                if not _is_dependency_input(relative, root):
                    continue
                if (repository_name, relative) not in evidence_paths:
                    raise WorkspaceError(
                        f"{repository_name}/{relative}: dependency input is absent from the inventory"
                    )
                text = _read_metadata(root / relative)
                if _is_container_input(relative):
                    for image in _container_references(relative, text):
                        _validate_container_reference(
                            self.dependencies, repository_name, relative, image
                        )

            messages.append(
                f"{repository_name}: audited {len(tracked)} version-controlled inputs and {action_count} action references"
            )
        for locator, dependency in sorted(action_dependencies.items()):
            expected_scope = set(dependency.scope) & supplied
            actual_scope = observed_action_scopes[locator]
            if actual_scope != expected_scope:
                raise WorkspaceError(
                    f"{locator}: inventory scope differs from audited use: "
                    f"missing={sorted(expected_scope - actual_scope)}, "
                    f"extra={sorted(actual_scope - expected_scope)}"
                )
        for locator, dependency in sorted(runner_dependencies.items()):
            expected_scope = set(dependency.scope) & supplied
            actual_scope = observed_runner_scopes[locator]
            if actual_scope != expected_scope:
                raise WorkspaceError(
                    f"{locator}: inventory scope differs from audited use: "
                    f"missing={sorted(expected_scope - actual_scope)}, "
                    f"extra={sorted(actual_scope - expected_scope)}"
                )
        return messages

    def report(self, format_name: str) -> str:
        if format_name == "licenses":
            return self._license_report()
        if format_name == "cyclonedx":
            return json.dumps(self._cyclonedx(), indent=2, sort_keys=True) + "\n"
        if format_name == "spdx":
            return json.dumps(self._spdx(), indent=2, sort_keys=True) + "\n"
        raise WorkspaceError(f"unsupported supply-chain report format: {format_name}")

    def _license_report(self) -> str:
        lines = [
            "# Atrinik third-party dependency and provenance report",
            "",
            "Generated deterministically from `supply-chain/inventory.json`.",
            "",
            "| Dependency | Version | License | Owner | Disposition | Source |",
            "| --- | --- | --- | --- | --- | --- |",
        ]
        for dependency in self.dependencies:
            lines.append(
                f"| {dependency.name} | {dependency.version} | {dependency.license} | "
                f"{dependency.owner} | {dependency.disposition} | {dependency.source_url} |"
            )
        return "\n".join(lines) + "\n"

    def _cyclonedx(self) -> dict[str, Any]:
        components = []
        for dependency in self.dependencies:
            license_value = (
                {"name": dependency.license}
                if dependency.license == "NOASSERTION"
                or dependency.license.startswith("LicenseRef-")
                else {"id": dependency.license}
            )
            component: dict[str, Any] = {
                "type": _cyclonedx_type(dependency.kind),
                "bom-ref": dependency.locator,
                "name": dependency.name,
                "version": dependency.version,
                "licenses": [{"license": license_value}],
                "externalReferences": [
                    {"type": "website", "url": dependency.source_url}
                ],
                "properties": [
                    {"name": "atrinik:owner", "value": dependency.owner},
                    {"name": "atrinik:scope", "value": ",".join(dependency.scope)},
                    {"name": "atrinik:disposition", "value": dependency.disposition},
                    {
                        "name": "atrinik:update-cadence-days",
                        "value": str(dependency.update_cadence_days),
                    },
                ],
            }
            if dependency.packages:
                component["properties"].append(
                    {
                        "name": "atrinik:declared-packages",
                        "value": ",".join(dependency.packages),
                    }
                )
            if dependency.checksum is not None:
                component["hashes"] = [
                    {"alg": "SHA-256", "content": dependency.checksum.removeprefix("sha256:")}
                ]
            components.append(component)
        return {
            "bomFormat": "CycloneDX",
            "specVersion": "1.6",
            "version": 1,
            "metadata": {"component": {"type": "application", "name": "Atrinik"}},
            "components": components,
        }

    def _spdx(self) -> dict[str, Any]:
        packages = []
        relationships = []
        extracted_licenses: dict[str, dict[str, str]] = {}
        for dependency in self.dependencies:
            suffix = hashlib.sha256(dependency.identifier.encode()).hexdigest()[:16]
            spdx_id = f"SPDXRef-Dependency-{suffix}"
            package = {
                "SPDXID": spdx_id,
                "name": dependency.name,
                "versionInfo": dependency.version,
                "downloadLocation": dependency.source_url,
                "filesAnalyzed": False,
                "licenseConcluded": dependency.license,
                "licenseDeclared": dependency.license,
                "supplier": "NOASSERTION",
                "packageComment": (
                    f"Atrinik owner: {dependency.owner}; scope: "
                    f"{','.join(dependency.scope)}; disposition: {dependency.disposition}."
                ),
            }
            if dependency.packages:
                package["packageComment"] += (
                    f" Declared packages: {','.join(dependency.packages)}."
                )
            packages.append(package)
            relationships.append(
                {
                    "spdxElementId": "SPDXRef-DOCUMENT",
                    "relationshipType": "DESCRIBES",
                    "relatedSpdxElement": spdx_id,
                }
            )
            if dependency.license.startswith("LicenseRef-"):
                extracted_licenses[dependency.license] = {
                    "licenseId": dependency.license,
                    "extractedText": (
                        "The applicable license and provenance are recorded by the "
                        "dependency's source-local licensing files and inventory evidence."
                    ),
                    "name": dependency.license,
                }
        document = {
            "spdxVersion": "SPDX-2.3",
            "dataLicense": "CC0-1.0",
            "SPDXID": "SPDXRef-DOCUMENT",
            "name": "Atrinik supply-chain inventory",
            "documentNamespace": "https://atrinik.org/spdx/supply-chain-v1",
            "creationInfo": {
                "created": self.created,
                "creators": ["Organization: Atrinik"],
            },
            "packages": packages,
            "relationships": relationships,
        }
        if extracted_licenses:
            document["hasExtractedLicensingInfos"] = [
                extracted_licenses[key] for key in sorted(extracted_licenses)
            ]
        return document


def _load_dependency(
    value: object,
    index: int,
    repository_names: set[str],
    identifiers: set[str],
) -> Dependency:
    context = f"inventory dependency {index}"
    if not isinstance(value, dict):
        raise WorkspaceError(f"{context} must be an object")
    keys = {
        "id",
        "name",
        "kind",
        "owner",
        "scope",
        "required",
        "version",
        "version_source",
        "license",
        "source_url",
        "locator",
        "commit",
        "checksum",
        "acquisition",
        "update_cadence_days",
        "update_mechanism",
        "eol_response",
        "validation",
        "disposition",
        "packages",
        "evidence",
    }
    require_keys(value, keys, context)
    identifier = _identifier(value["id"], f"{context}.id")
    if identifier in identifiers:
        raise WorkspaceError(f"duplicate dependency id: {identifier}")
    identifiers.add(identifier)
    name = _text(value["name"], f"{context}.name")
    kind = _text(value["kind"], f"{context}.kind")
    if kind not in DEPENDENCY_KINDS:
        raise WorkspaceError(f"{context}.kind is unsupported: {kind}")
    owner = _identifier(value["owner"], f"{context}.owner")
    if owner not in repository_names:
        raise WorkspaceError(f"{context}.owner is not an inventory repository")
    scope = _string_array(value["scope"], f"{context}.scope")
    if not scope or set(scope) - repository_names:
        raise WorkspaceError(f"{context}.scope contains an unknown repository")
    if list(scope) != sorted(scope):
        raise WorkspaceError(f"{context}.scope must be sorted")
    if not isinstance(value["required"], bool):
        raise WorkspaceError(f"{context}.required must be a boolean")
    version = _text(value["version"], f"{context}.version")
    version_source = _text(value["version_source"], f"{context}.version_source")
    license_id = _text(value["license"], f"{context}.license")
    if not SPDX_PATTERN.fullmatch(license_id):
        raise WorkspaceError(f"{context}.license is not an SPDX expression or LicenseRef")
    source_url = _text(value["source_url"], f"{context}.source_url")
    if not source_url.startswith("https://"):
        raise WorkspaceError(f"{context}.source_url must use HTTPS")
    locator = _text(value["locator"], f"{context}.locator")
    commit = value["commit"]
    if commit is not None and (
        not isinstance(commit, str) or not ACTION_COMMIT_PATTERN.fullmatch(commit)
    ):
        raise WorkspaceError(f"{context}.commit must be null or a full lowercase Git commit")
    checksum = value["checksum"]
    if checksum is not None and (
        not isinstance(checksum, str) or not CHECKSUM_PATTERN.fullmatch(checksum)
    ):
        raise WorkspaceError(f"{context}.checksum must be null or a lowercase SHA-256")
    acquisition = _text(value["acquisition"], f"{context}.acquisition")
    cadence = value["update_cadence_days"]
    if not isinstance(cadence, int) or isinstance(cadence, bool) or not 1 <= cadence <= 366:
        raise WorkspaceError(f"{context}.update_cadence_days must be between 1 and 366")
    update_mechanism = _text(value["update_mechanism"], f"{context}.update_mechanism")
    eol_response = _text(value["eol_response"], f"{context}.eol_response")
    validation = _text(value["validation"], f"{context}.validation")
    disposition = _text(value["disposition"], f"{context}.disposition")
    if disposition not in DISPOSITIONS:
        raise WorkspaceError(f"{context}.disposition is unsupported")
    packages = _string_array(value["packages"], f"{context}.packages", allow_empty=True)
    if list(packages) != sorted(packages) or len(set(packages)) != len(packages):
        raise WorkspaceError(f"{context}.packages must be sorted and unique")

    raw_evidence = value["evidence"]
    if not isinstance(raw_evidence, list) or not raw_evidence:
        raise WorkspaceError(f"{context}.evidence must be a non-empty array")
    evidence: list[Evidence] = []
    for evidence_index, raw in enumerate(raw_evidence):
        evidence_context = f"{context}.evidence[{evidence_index}]"
        if not isinstance(raw, dict):
            raise WorkspaceError(f"{evidence_context} must be an object")
        require_keys(raw, {"repository", "path", "contains"}, evidence_context)
        repository = _identifier(raw["repository"], f"{evidence_context}.repository")
        if repository not in repository_names:
            raise WorkspaceError(f"{evidence_context}.repository is unknown")
        path = _relative_path(raw["path"], f"{evidence_context}.path")
        contains = _text(raw["contains"], f"{evidence_context}.contains")
        evidence.append(Evidence(repository, path, contains))

    evidence_keys = [
        (item.repository, item.path, item.contains) for item in evidence
    ]
    if evidence_keys != sorted(evidence_keys) or len(evidence_keys) != len(set(evidence_keys)):
        raise WorkspaceError(f"{context}.evidence must be sorted and unique")
    for item in evidence:
        if item.repository not in scope:
            raise WorkspaceError(
                f"{context}.evidence repository must be present in dependency scope"
            )

    if kind == "github-action":
        if commit is None:
            raise WorkspaceError(f"{context}: GitHub Actions require a commit")
        if not re.fullmatch(
            r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)*", locator
        ):
            raise WorkspaceError(f"{context}.locator must be an owner/action coordinate")
    return Dependency(
        identifier,
        name,
        kind,
        owner,
        scope,
        value["required"],
        version,
        version_source,
        license_id,
        source_url,
        locator,
        commit,
        checksum,
        acquisition,
        cadence,
        update_mechanism,
        eol_response,
        validation,
        disposition,
        packages,
        tuple(evidence),
    )


def _text(value: object, context: str) -> str:
    if not isinstance(value, str) or not value or value != value.strip():
        raise WorkspaceError(f"{context} must be a non-empty trimmed string")
    return value


def _identifier(value: object, context: str) -> str:
    text = _text(value, context)
    if not re.fullmatch(r"[a-z0-9][a-z0-9._/-]*", text):
        raise WorkspaceError(f"{context} must use lowercase identifier characters")
    return text


def _string_array(value: object, context: str, *, allow_empty: bool = False) -> tuple[str, ...]:
    if not isinstance(value, list) or (not value and not allow_empty):
        raise WorkspaceError(f"{context} must be {'an' if allow_empty else 'a non-empty'} array")
    return tuple(_text(item, f"{context} item") for item in value)


def _relative_path(value: object, context: str) -> str:
    text = _text(value, context)
    path = PurePosixPath(text)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise WorkspaceError(f"{context} must be a safe repository-relative path")
    return path.as_posix()


def _safe_repository_path(root: Path, relative: str) -> Path:
    path = root.joinpath(*PurePosixPath(relative).parts)
    try:
        path.resolve(strict=True).relative_to(root)
    except (FileNotFoundError, ValueError) as error:
        raise WorkspaceError(f"dependency evidence is missing or escapes its repository: {path}") from error
    if not path.is_file() or path.is_symlink():
        raise WorkspaceError(f"dependency evidence must be a regular non-symlink file: {path}")
    return path


def _read_metadata(path: Path) -> str:
    try:
        if path.stat().st_size > MAX_METADATA_BYTES:
            raise WorkspaceError(f"dependency metadata exceeds size limit: {path}")
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise WorkspaceError(f"cannot read dependency metadata {path}: {error}") from error


def _tracked_files(root: Path) -> set[str]:
    result = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=30,
    )
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise WorkspaceError(f"cannot list tracked files for {root}: {detail}")
    try:
        return {
            PurePosixPath(path).as_posix()
            for path in result.stdout.decode("utf-8").split("\0")
            if path
        }
    except UnicodeDecodeError as error:
        raise WorkspaceError(f"tracked path is not UTF-8 in {root}") from error


def _is_dependency_input(relative: str, root: Path) -> bool:
    path = PurePosixPath(relative)
    if path.name in DEPENDENCY_FILE_NAMES:
        return True
    if path.name in VENDORED_FILE_NAMES:
        return True
    if path.name.startswith("Dockerfile"):
        return True
    if path.name == "CMakeLists.txt" or path.suffix == ".cmake":
        text = _read_metadata(root / relative)
        return bool(re.search(r"find_package\(|pkg_check_modules\(|FetchContent", text))
    if relative in {
        ".devcontainer/devcontainer.json",
        ".devcontainer/windows-cross/devcontainer.json",
        "tools/install-linux-ci-dependencies.sh",
    }:
        return True
    return False


def _is_container_input(relative: str) -> bool:
    return PurePosixPath(relative).name.startswith("Dockerfile") or relative in {
        ".devcontainer/devcontainer.json",
        ".devcontainer/windows-cross/devcontainer.json",
    }


def _container_references(relative: str, text: str) -> list[str]:
    if PurePosixPath(relative).name.startswith("Dockerfile"):
        references = SYNTAX_PATTERN.findall(text)
        stages: set[str] = set()
        for line in text.splitlines():
            match = FROM_PATTERN.match(line)
            if match is None:
                continue
            image, stage = match.groups()
            if image.casefold() not in stages:
                references.append(image)
            if stage is not None:
                stages.add(stage.casefold())
        return references
    try:
        value = json.loads(text, object_pairs_hook=_reject_duplicate_json_keys)
    except json.JSONDecodeError as error:
        raise WorkspaceError(f"invalid devcontainer JSON {relative}: {error}") from error
    image = value.get("image") if isinstance(value, dict) else None
    return [image] if isinstance(image, str) else []


def _validate_container_reference(
    dependencies: list[Dependency],
    repository: str,
    relative: str,
    image: str,
) -> None:
    coordinate, separator, digest = image.rpartition("@sha256:")
    if not separator or not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise WorkspaceError(f"{repository}/{relative}: movable container image {image}")
    if ":" in coordinate.rsplit("/", 1)[-1]:
        coordinate = coordinate.rsplit(":", 1)[0]
    first_part = coordinate.partition("/")[0]
    if "/" not in coordinate:
        coordinate = f"docker.io/library/{coordinate}"
    elif "." not in first_part and ":" not in first_part and first_part != "localhost":
        coordinate = f"docker.io/{coordinate}"
    expected_checksum = f"sha256:{digest}"
    if not any(
        dependency.kind == "container-image"
        and dependency.locator == coordinate
        and dependency.checksum == expected_checksum
        and repository in dependency.scope
        and any(
            evidence.repository == repository
            and evidence.path == relative
            and image in evidence.contains
            for evidence in dependency.evidence
        )
        for dependency in dependencies
    ):
        raise WorkspaceError(
            f"{repository}/{relative}: unowned container image {image}"
        )


def _reject_duplicate_json_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise WorkspaceError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _cyclonedx_type(kind: str) -> str:
    if kind in {"container-image", "container-feature"}:
        return "container"
    if kind in {"external-tool", "github-action", "toolchain"}:
        return "application"
    return "library"


def version_report(inventory: Inventory | None = None) -> str:
    probes: dict[str, list[str]] = {
        "actionlint": ["actionlint", "--version"],
        "clang": ["clang", "--version"],
        "cmake": ["cmake", "--version"],
        "curl": ["curl", "--version"],
        "flex": ["flex", "--version"],
        "gcc": ["gcc", "--version"],
        "gh": ["gh", "--version"],
        "git": ["git", "--version"],
        "library:libcurl": ["pkg-config", "--modversion", "libcurl"],
        "library:libxml2": ["pkg-config", "--modversion", "libxml-2.0"],
        "library:miniupnpc": ["pkg-config", "--modversion", "miniupnpc"],
        "library:openssl": ["pkg-config", "--modversion", "openssl"],
        "library:sdl3": ["pkg-config", "--modversion", "sdl3"],
        "library:sdl3-image": ["pkg-config", "--modversion", "sdl3-image"],
        "library:sdl3-mixer": ["pkg-config", "--modversion", "sdl3-mixer"],
        "library:sdl3-ttf": ["pkg-config", "--modversion", "sdl3-ttf"],
        "library:zlib": ["pkg-config", "--modversion", "zlib"],
        "ninja": ["ninja", "--version"],
        "node": ["node", "--version"],
        "npm": ["npm", "--version"],
        "openssl": ["openssl", "version"],
        "pkg-config": ["pkg-config", "--version"],
        "python": ["python3", "--version"],
        "shellcheck": ["shellcheck", "--version"],
    }
    versions: dict[str, dict[str, object]] = {}
    for name, command in probes.items():
        try:
            result = subprocess.run(
                command,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=10,
            )
        except FileNotFoundError:
            versions[name] = {"available": False, "version": None}
            continue
        lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        available = result.returncode == 0
        version = None
        if available and lines:
            version = next((line for line in lines if re.search(r"\d", line)), lines[0])
        versions[name] = {"available": available, "version": version}
    if inventory is not None:
        versions["declared-dependencies"] = {
            dependency.identifier: {
                "checksum": dependency.checksum,
                "commit": dependency.commit,
                "kind": dependency.kind,
                "version": dependency.version,
            }
            for dependency in inventory.dependencies
        }
        package_names = sorted(
            {
                package
                for dependency in inventory.dependencies
                if dependency.kind in {"system-library", "system-package-set", "toolchain"}
                for package in dependency.packages
            }
        )
        versions["system-packages"] = _system_package_versions(package_names)
    return json.dumps(versions, indent=2, sort_keys=True) + "\n"


def _system_package_versions(package_names: list[str]) -> dict[str, dict[str, object]]:
    versions = {
        package: {"available": False, "version": None} for package in package_names
    }
    if not package_names:
        return versions
    try:
        result = subprocess.run(
            [
                "dpkg-query",
                "--show",
                "--showformat=${binary:Package}\t${Version}\n",
                "--",
                *package_names,
            ],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=30,
        )
    except FileNotFoundError:
        return versions
    for line in result.stdout.splitlines():
        package, separator, version = line.partition("\t")
        package = package.removesuffix(":amd64").removesuffix(":i386")
        if separator and package in versions and version:
            versions[package] = {"available": True, "version": version}
    return versions


def repository_roots(
    root: Path,
    workspace: Any,
    profile: str,
    overrides: Iterable[str] = (),
) -> dict[str, Path]:
    manifest = Manifest.load(root / "components.json")
    known = {component.name for component in manifest.components}
    expected = {component.name: component.repository for component in manifest.components}
    selected: dict[str, Path] = {}
    for override in overrides:
        name, separator, raw_path = override.partition("=")
        if not separator or name not in known or not raw_path:
            raise WorkspaceError(
                f"supply-chain repository override must be NAME=PATH for a component: {override}"
            )
        if name in selected:
            raise WorkspaceError(f"duplicate supply-chain repository override: {name}")
        path = Path(raw_path)
        if not path.is_absolute():
            raise WorkspaceError(f"supply-chain repository override must be absolute: {override}")
        resolved = path.resolve(strict=True)
        if _git_repository_coordinate(resolved) != expected[name]:
            raise WorkspaceError(
                f"supply-chain repository override is not {expected[name]}: {resolved}"
            )
        selected[name] = resolved
    roots = {"atrinik": root}
    roots.update(
        {
            component.name: selected.get(component.name)
            or workspace.component_path(component.name, profile)
            for component in manifest.components
        }
    )
    return roots


def _git_repository_coordinate(root: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(root), "remote", "get-url", "origin"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=10,
    )
    if result.returncode != 0:
        raise WorkspaceError(f"cannot inspect repository identity for {root}")
    url = result.stdout.strip().removesuffix("/").removesuffix(".git")
    match = re.search(r"github\.com(?::|/)([^/]+)/([^/]+)$", url)
    if match is None:
        raise WorkspaceError(f"unsupported repository remote for supply-chain audit: {url}")
    return f"{match.group(1)}/{match.group(2)}"


def write_generated(root: Path, output: Path | None, value: str) -> None:
    if output is None:
        print(value, end="")
        return
    root = root.resolve(strict=True)
    build_entry = root / "build"
    if build_entry.is_symlink():
        raise WorkspaceError(
            f"generated output directory must not be a symlink: {build_entry}"
        )
    target = output if output.is_absolute() else root / output
    target = target.resolve(strict=False)
    build = build_entry.resolve(strict=False)
    try:
        target.relative_to(build)
    except ValueError as error:
        raise WorkspaceError(f"generated supply-chain output must be under {build}") from error
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(value, encoding="utf-8")
    print(target)
