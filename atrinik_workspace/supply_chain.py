from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from typing import Any, Iterable

from .model import (
    Checkout,
    Component,
    Manifest,
    Stack,
    WorkspaceError,
    load_json,
    require_keys,
)


SCHEMA_VERSION = 3
ACTION_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
CHECKSUM_PATTERN = re.compile(r"^sha256:[0-9a-f]{64}$")
GIT_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}(?:[0-9a-f]{24})?$")
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
NPX_PATTERN = re.compile(r"(?:^|[\s;])npx(?:[\s\\]|$)", re.MULTILINE)
DEPENDENCY_FILE_NAMES = {
    "Cargo.lock",
    "Cargo.toml",
    "buf.gen.yaml",
    "buf.yaml",
    "catalog.lock.json",
    "dependencies.lock.json",
    "devcontainer-lock.json",
    "deny.toml",
    "go.mod",
    "go.sum",
    "package-lock.json",
    "package.json",
    "pyproject.toml",
    "requirements-dev.txt",
    "rust-toolchain.toml",
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
AUDIT_MODES = {"full", "metadata"}
CHECKOUT_METADATA_REQUIRED_FILES = {
    ".github/dependabot.yml",
    ".github/workflows/check.yml",
    ".github/workflows/pr-title.yml",
    "AGENTS.md",
    "ATTRIBUTIONS.md",
    "DEPENDENCIES.md",
    "LICENSE.md",
    "PROVENANCE.md",
    "docs/history/imports.json",
    "tools/verify_import_history.py",
}


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
    branch: str
    checkout: str
    source: str
    cohorts: tuple[str, ...]
    stacks: tuple[str, ...]
    roles: tuple[str, ...]
    license: str
    commit: str | None
    supported: bool
    audit_ready: bool
    audit_mode: str
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
        repository_coordinates: set[tuple[str, str, str, str]] = set()
        for index, value in enumerate(raw_repositories):
            context = f"inventory repository {index}"
            if not isinstance(value, dict):
                raise WorkspaceError(f"{context} must be an object")
            require_keys(
                value,
                {
                    "name",
                    "repository",
                    "branch",
                    "checkout",
                    "source",
                    "cohorts",
                    "stacks",
                    "roles",
                    "license",
                    "commit",
                    "supported",
                    "audit_ready",
                    "audit_mode",
                    "role",
                },
                context,
            )
            name = _identifier(value["name"], f"{context}.name")
            coordinate = _text(value["repository"], f"{context}.repository")
            if not re.fullmatch(r"atrinik/[a-z0-9][a-z0-9._-]*", coordinate):
                raise WorkspaceError(f"{context}.repository must be an Atrinik repository")
            branch = _text(value["branch"], f"{context}.branch")
            checkout = _identifier(value["checkout"], f"{context}.checkout")
            source = _source_path(value["source"], f"{context}.source")
            cohorts = _string_array(
                value["cohorts"], f"{context}.cohorts", allow_empty=True
            )
            stacks = _string_array(
                value["stacks"], f"{context}.stacks", allow_empty=True
            )
            roles = _string_array(value["roles"], f"{context}.roles")
            if list(cohorts) != sorted(set(cohorts)):
                raise WorkspaceError(f"{context}.cohorts must be sorted and unique")
            if list(stacks) != sorted(set(stacks)):
                raise WorkspaceError(f"{context}.stacks must be sorted and unique")
            if list(roles) != sorted(set(roles)):
                raise WorkspaceError(f"{context}.roles must be sorted and unique")
            license_name = _text(value["license"], f"{context}.license")
            if not SPDX_PATTERN.fullmatch(license_name):
                raise WorkspaceError(
                    f"{context}.license is not an SPDX expression or LicenseRef"
                )
            commit = value["commit"]
            if commit is not None and (
                not isinstance(commit, str) or not GIT_COMMIT_PATTERN.fullmatch(commit)
            ):
                raise WorkspaceError(
                    f"{context}.commit must be null or a full lowercase Git commit"
                )
            if not isinstance(value["supported"], bool):
                raise WorkspaceError(f"{context}.supported must be a boolean")
            if value["supported"] and commit is not None:
                raise WorkspaceError(
                    f"{context}.commit must be null for a moving supported branch; "
                    "resolve it from a validated profile"
                )
            if not isinstance(value["audit_ready"], bool):
                raise WorkspaceError(f"{context}.audit_ready must be a boolean")
            audit_mode = value["audit_mode"]
            if audit_mode not in AUDIT_MODES:
                raise WorkspaceError(
                    f"{context}.audit_mode must be one of {sorted(AUDIT_MODES)}"
                )
            role = _text(value["role"], f"{context}.role")
            repository_identity = (coordinate, branch, checkout, source)
            if name in repository_names or repository_identity in repository_coordinates:
                raise WorkspaceError(f"duplicate inventory repository: {name}")
            repository_names.add(name)
            repository_coordinates.add(repository_identity)
            repositories.append(
                Repository(
                    name,
                    coordinate,
                    branch,
                    checkout,
                    source,
                    cohorts,
                    stacks,
                    roles,
                    license_name,
                    commit,
                    value["supported"],
                    value["audit_ready"],
                    audit_mode,
                    role,
                )
            )
        if [repository.name for repository in repositories] != sorted(repository_names):
            raise WorkspaceError("inventory repositories must be sorted by name")

        manifest = Manifest.load(manifest_path)
        metadata_checkouts = _metadata_checkouts(manifest)
        expected_supported = {
            "atrinik",
            *(component.name for component in manifest.components),
            *(checkout.name for checkout in metadata_checkouts),
        }
        actual_supported = {repository.name for repository in repositories if repository.supported}
        if actual_supported != expected_supported:
            missing = sorted(expected_supported - actual_supported)
            extra = sorted(actual_supported - expected_supported)
            raise WorkspaceError(
                "inventory supported repositories do not match components.json: "
                f"missing={missing}, extra={extra}"
            )
        expected_metadata = {
            component.name: {
                "repository": component.repository,
                "branch": component.branch,
                "checkout": component.checkout_name,
                "source": component.source,
                "cohorts": tuple(sorted(manifest.component_cohorts(component.name))),
                "stacks": tuple(sorted(manifest.component_stacks(component.name))),
                "roles": tuple(sorted(component.provides)),
                "license": component.license,
                "audit_mode": "full",
            }
            for component in manifest.components
        }
        for checkout in metadata_checkouts:
            expected_metadata[checkout.name] = {
                "repository": checkout.repository,
                "branch": checkout.branch,
                "checkout": checkout.name,
                "source": ".",
                "cohorts": tuple(sorted(manifest.checkout_cohorts(checkout.name))),
                "stacks": tuple(sorted(manifest.checkout_stacks(checkout.name))),
                "roles": ("checkout-metadata",),
                "license": checkout.license,
                "audit_mode": "metadata",
            }
        expected_metadata["atrinik"] = {
            "repository": "atrinik/atrinik",
            "branch": "main",
            "checkout": "atrinik",
            "source": ".",
            "cohorts": (),
            "stacks": (),
            "roles": ("workspace",),
            "license": "MIT",
            "audit_mode": "full",
        }
        for repository in repositories:
            if not repository.supported:
                continue
            actual = {
                "repository": repository.repository,
                "branch": repository.branch,
                "checkout": repository.checkout,
                "source": repository.source,
                "cohorts": repository.cohorts,
                "stacks": repository.stacks,
                "roles": repository.roles,
                "license": repository.license,
                "audit_mode": repository.audit_mode,
            }
            if actual != expected_metadata[repository.name]:
                raise WorkspaceError(
                    f"inventory repository metadata mismatch for {repository.name}: {actual}"
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
        if schema.get("$id") != "https://atrinik.org/schema/supply-chain-inventory-v3.json":
            raise WorkspaceError("supply-chain schema has an unexpected $id")
        if schema.get("additionalProperties") is not False:
            raise WorkspaceError("supply-chain schema must reject additional root properties")

    def audit(self, roots: dict[str, Path], *, require_all: bool = True) -> list[str]:
        supported = {
            repository.name
            for repository in self.repositories
            if repository.supported and repository.audit_ready
        }
        supplied = set(roots)
        if require_all and not supplied:
            raise WorkspaceError(
                "supply-chain audit roots are incomplete: no audit-ready repositories supplied"
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
        logical_sources = {
            repository.checkout: tuple(
                sorted(
                    candidate.source
                    for candidate in self.repositories
                    if candidate.checkout == repository.checkout
                    and candidate.audit_mode == "full"
                    and candidate.source != "."
                )
            )
            for repository in self.repositories
            if repository.audit_mode == "metadata"
        }
        for dependency in self.dependencies:
            for evidence in dependency.evidence:
                if evidence.repository not in normalized:
                    continue
                evidence_repository = self.repositories_by_name[evidence.repository]
                evidence_sources = logical_sources.get(
                    evidence_repository.checkout, ()
                )
                if _uses_checkout_metadata(
                    evidence_repository, evidence_sources
                ) and _is_inert_checkout_github_metadata(evidence.path):
                    raise WorkspaceError(
                        f"{dependency.identifier}: {evidence.repository}/"
                        f"{evidence.path} is inert nested GitHub metadata and "
                        "cannot be active dependency evidence"
                    )
                path = _safe_repository_path(normalized[evidence.repository], evidence.path)
                text = _read_metadata(path)
                if evidence.contains not in text:
                    raise WorkspaceError(
                        f"{dependency.identifier}: expected text is absent from "
                        f"{evidence.repository}/{evidence.path}: {evidence.contains}"
                    )

        for repository_name, root in sorted(normalized.items()):
            repository = self.repositories_by_name[repository_name]
            checkout_sources = logical_sources.get(repository.checkout, ())
            tracked = _audit_files(
                repository, root, checkout_sources
            )
            if ".gitmodules" in tracked:
                raise WorkspaceError(f"{repository_name}: Git submodules are not supported")
            dependabot = ".github/dependabot.yml"
            if not _uses_checkout_metadata(repository, checkout_sources):
                if dependabot not in tracked:
                    raise WorkspaceError(f"{repository_name}: missing {dependabot}")
                if "package-ecosystem: github-actions" not in _read_metadata(
                    root / dependabot
                ):
                    raise WorkspaceError(
                        f"{repository_name}: Dependabot does not own GitHub Actions updates"
                    )

            action_count = 0
            for relative in sorted(
                path for path in tracked if path.startswith(".github/workflows/") and path.endswith((".yml", ".yaml"))
            ):
                text = _read_metadata(root / relative)
                if _workflow_uses_unpinned_npx(text):
                    raise WorkspaceError(
                        f"{repository_name}/{relative}: npx requires an immutable "
                        "actions/setup-node step in the same workflow"
                    )
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
                try:
                    runners = _workflow_runners(text)
                except WorkspaceError as error:
                    raise WorkspaceError(
                        f"{repository_name}/{relative}: {error}"
                    ) from error
                for runner in runners:
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

    def report(
        self,
        format_name: str,
        component_commits: dict[str, str | None] | None = None,
        selected_stack: str | None = None,
    ) -> str:
        if selected_stack is not None and not any(
            selected_stack in repository.stacks for repository in self.repositories
        ):
            raise WorkspaceError(
                f"unknown supply-chain report stack: {selected_stack}"
            )
        resolved_commits = self._resolved_first_party_commits(
            component_commits, selected_stack
        )
        if format_name == "licenses":
            return self._license_report(resolved_commits, selected_stack)
        if format_name == "cyclonedx":
            return json.dumps(
                self._cyclonedx(resolved_commits, selected_stack),
                indent=2,
                sort_keys=True,
            ) + "\n"
        if format_name == "spdx":
            return json.dumps(
                self._spdx(resolved_commits, selected_stack),
                indent=2,
                sort_keys=True,
            ) + "\n"
        raise WorkspaceError(f"unsupported supply-chain report format: {format_name}")

    def _resolved_first_party_commits(
        self,
        component_commits: dict[str, str | None] | None,
        selected_stack: str | None,
    ) -> dict[str, str | None]:
        resolved = {
            repository.name: (
                repository.commit if selected_stack is None else None
            )
            for repository in self._first_party_repositories()
        }
        if component_commits is None:
            return resolved
        unknown = sorted(set(component_commits) - set(resolved))
        if unknown:
            raise WorkspaceError(
                f"unknown first-party component commits: {', '.join(unknown)}"
            )
        non_selected = sorted(
            name
            for name, commit in component_commits.items()
            if commit is not None
            and name != "atrinik"
            and selected_stack is not None
            and selected_stack not in self.repositories_by_name[name].stacks
        )
        if non_selected:
            raise WorkspaceError(
                "resolved commits include components outside the selected stack: "
                + ", ".join(non_selected)
            )
        for name, commit in component_commits.items():
            if commit is not None and (
                not isinstance(commit, str) or not GIT_COMMIT_PATTERN.fullmatch(commit)
            ):
                raise WorkspaceError(
                    f"resolved commit for {name} must be a full lowercase Git commit"
                )
            resolved[name] = commit
        return resolved

    @staticmethod
    def _selected_for_report(
        repository: Repository, selected_stack: str | None
    ) -> bool:
        return repository.name == "atrinik" or (
            selected_stack is not None and selected_stack in repository.stacks
        )

    def _license_report(
        self, commits: dict[str, str | None], selected_stack: str | None
    ) -> str:
        lines = [
            "# Atrinik component, dependency, and provenance report",
            "",
            "Generated deterministically from `supply-chain/inventory.json`.",
            "",
            "## First-party components",
            "",
            "| Component | Repository | Branch | Checkout | Source | Commit | Selected | Cohorts | Stacks | Roles | License | Audit ready | Audit mode |",
            "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
        ]
        for repository in self._first_party_repositories():
            commit = commits[repository.name] or "unavailable"
            selected = self._selected_for_report(repository, selected_stack)
            lines.append(
                f"| {repository.name} | {repository.repository} | {repository.branch} | "
                f"{repository.checkout} | {repository.source} | {commit} | "
                f"{'yes' if selected else 'no'} | "
                f"{','.join(repository.cohorts) or '-'} | "
                f"{','.join(repository.stacks) or '-'} | "
                f"{','.join(repository.roles)} | {repository.license} | "
                f"{'yes' if repository.audit_ready else 'no'} | "
                f"{repository.audit_mode} |"
            )
        lines.extend(
            [
                "",
                "## Third-party dependencies",
                "",
                "| Dependency | Version | License | Owner | Disposition | Source |",
                "| --- | --- | --- | --- | --- | --- |",
            ]
        )
        for dependency in self.dependencies:
            lines.append(
                f"| {dependency.name} | {dependency.version} | {dependency.license} | "
                f"{dependency.owner} | {dependency.disposition} | {dependency.source_url} |"
            )
        return "\n".join(lines) + "\n"

    def _first_party_repositories(self) -> list[Repository]:
        return [repository for repository in self.repositories if repository.supported]

    @staticmethod
    def _cyclonedx_license(license_name: str) -> dict[str, str]:
        if license_name == "NOASSERTION" or license_name.startswith("LicenseRef-"):
            return {"name": license_name}
        return {"id": license_name}

    @staticmethod
    def _repository_properties(
        repository: Repository,
        commit: str | None,
        selected_stack: str | None,
        selected: bool,
    ) -> list[dict[str, str]]:
        return [
            {"name": "atrinik:component", "value": repository.name},
            {"name": "atrinik:repository", "value": repository.repository},
            {"name": "atrinik:branch", "value": repository.branch},
            {"name": "atrinik:checkout", "value": repository.checkout},
            {"name": "atrinik:source", "value": repository.source},
            {"name": "atrinik:commit", "value": commit or "unavailable"},
            {
                "name": "atrinik:report-stack",
                "value": selected_stack or "unresolved",
            },
            {"name": "atrinik:selected", "value": str(selected).lower()},
            {"name": "atrinik:cohorts", "value": ",".join(repository.cohorts)},
            {"name": "atrinik:stacks", "value": ",".join(repository.stacks)},
            {"name": "atrinik:roles", "value": ",".join(repository.roles)},
            {"name": "atrinik:license", "value": repository.license},
            {
                "name": "atrinik:audit-ready",
                "value": str(repository.audit_ready).lower(),
            },
            {"name": "atrinik:audit-mode", "value": repository.audit_mode},
        ]

    def _cyclonedx(
        self, commits: dict[str, str | None], selected_stack: str | None
    ) -> dict[str, Any]:
        components: list[dict[str, Any]] = []
        for repository in self._first_party_repositories():
            commit = commits[repository.name]
            selected = self._selected_for_report(repository, selected_stack)
            components.append(
                {
                    "type": "application",
                    "bom-ref": f"atrinik:component:{repository.name}",
                    "name": repository.name,
                    "version": commit or "unavailable",
                    "licenses": [
                        {"license": self._cyclonedx_license(repository.license)}
                    ],
                    "externalReferences": [
                        {
                            "type": "vcs",
                            "url": (
                                f"https://github.com/{repository.repository}/tree/"
                                f"{commit or repository.branch}"
                            ),
                        }
                    ],
                    "properties": self._repository_properties(
                        repository, commit, selected_stack, selected
                    ),
                }
            )
        for dependency in self.dependencies:
            license_value = self._cyclonedx_license(dependency.license)
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

    def _spdx(
        self, commits: dict[str, str | None], selected_stack: str | None
    ) -> dict[str, Any]:
        packages: list[dict[str, Any]] = []
        relationships: list[dict[str, str]] = []
        extracted_licenses: dict[str, dict[str, str]] = {}
        for repository in self._first_party_repositories():
            commit = commits[repository.name]
            commit_value = commit or "unavailable"
            selected = self._selected_for_report(repository, selected_stack)
            suffix = hashlib.sha256(repository.name.encode()).hexdigest()[:16]
            spdx_id = f"SPDXRef-Component-{suffix}"
            packages.append(
                {
                    "SPDXID": spdx_id,
                    "name": repository.name,
                    "versionInfo": commit_value,
                    "downloadLocation": f"https://github.com/{repository.repository}",
                    "filesAnalyzed": False,
                    "licenseConcluded": repository.license,
                    "licenseDeclared": repository.license,
                    "supplier": "Organization: Atrinik",
                    "externalRefs": [
                        {
                            "referenceCategory": "OTHER",
                            "referenceType": "atrinik-component-commit",
                            "referenceLocator": commit_value,
                        }
                    ],
                    "packageComment": (
                        f"Atrinik component identity: {repository.name}; repository: "
                        f"{repository.repository}; branch: {repository.branch}; commit: "
                        f"{commit_value}; checkout: {repository.checkout}; source: "
                        f"{repository.source}; cohorts: "
                        f"{','.join(repository.cohorts)}; stacks: "
                        f"{','.join(repository.stacks)}; roles: "
                        f"{','.join(repository.roles)}; license: {repository.license}; "
                        f"report stack: {selected_stack or 'unresolved'}; selected: "
                        f"{str(selected).lower()}; "
                        f"audit-ready: {str(repository.audit_ready).lower()}."
                        f" audit-mode: {repository.audit_mode}."
                    ),
                }
            )
            relationships.append(
                {
                    "spdxElementId": "SPDXRef-DOCUMENT",
                    "relationshipType": "DESCRIBES",
                    "relatedSpdxElement": spdx_id,
                }
            )
            if repository.license.startswith("LicenseRef-"):
                extracted_licenses[repository.license] = {
                    "licenseId": repository.license,
                    "extractedText": (
                        "The applicable license and provenance are recorded by the "
                        "component's source-local licensing files and inventory metadata."
                    ),
                    "name": repository.license,
                }
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


def _source_path(value: object, context: str) -> str:
    text = _text(value, context)
    if text == ".":
        return text
    return _relative_path(text, context)


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


def _audit_files(
    repository: Repository, root: Path, logical_sources: tuple[str, ...] = ()
) -> set[str]:
    tracked = _tracked_files(root)
    if repository.audit_mode == "full":
        if _uses_checkout_metadata(repository, logical_sources):
            return {
                relative
                for relative in tracked
                if not _is_inert_checkout_github_metadata(relative)
            }
        return tracked
    missing = sorted(CHECKOUT_METADATA_REQUIRED_FILES - tracked)
    if missing:
        raise WorkspaceError(
            f"{repository.name}: checkout metadata audit is missing required files: "
            + ", ".join(missing)
        )
    return {
        relative
        for relative in tracked
        if not any(
            relative == source or relative.startswith(f"{source}/")
            for source in logical_sources
        )
    }


def _uses_checkout_metadata(
    repository: Repository, logical_sources: tuple[str, ...]
) -> bool:
    return (
        repository.audit_mode == "full"
        and repository.source != "."
        and repository.source in logical_sources
    )


def _is_inert_checkout_github_metadata(relative: str) -> bool:
    return relative == ".github/dependabot.yml" or relative.startswith(
        ".github/workflows/"
    )


def _is_dependency_input(relative: str, root: Path) -> bool:
    path = PurePosixPath(relative)
    if relative == "policy/dependencies.json":
        return True
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


def _workflow_runners(text: str) -> tuple[str, ...]:
    runners: list[str] = []
    for job in _workflow_job_blocks(text):
        for match in RUNNER_PATTERN.finditer(job):
            runner = match.group(1).partition("#")[0].strip()
            if re.fullmatch(r"[A-Za-z0-9_.-]+", runner):
                runners.append(runner)
                continue

            matrix = re.fullmatch(
                r"\$\{\{\s*matrix\.([A-Za-z0-9_-]+)\s*\}\}", runner
            )
            if matrix is None:
                raise WorkspaceError(
                    "workflow runner must be an explicit literal or a statically "
                    f"enumerated matrix value: {runner or '<empty>'}"
                )
            values = _static_matrix_values(job, matrix.group(1))
            if not values:
                raise WorkspaceError(
                    f"workflow runner matrix {matrix.group(1)} has no static literals"
                )
            runners.extend(values)
    return tuple(runners)


def _workflow_uses_unpinned_npx(text: str) -> bool:
    setup_pattern = re.compile(
        r"^\s*(?:-\s*)?uses:\s*actions/setup-node@[0-9a-f]{40}", re.MULTILINE
    )
    for job in _workflow_job_blocks(text):
        npx = NPX_PATTERN.search(job)
        if npx is None:
            continue
        setup = setup_pattern.search(job)
        if setup is None or setup.start() > npx.start():
            return True
    return False


def _workflow_job_blocks(text: str) -> tuple[str, ...]:
    lines = text.splitlines(keepends=True)
    jobs_index = next(
        (
            index
            for index, line in enumerate(lines)
            if re.fullmatch(r"\s*jobs:\s*(?:#.*)?\n?", line)
        ),
        None,
    )
    if jobs_index is None:
        return (text,)

    jobs_indent = len(lines[jobs_index]) - len(lines[jobs_index].lstrip())
    jobs_end = len(lines)
    for index in range(jobs_index + 1, len(lines)):
        line = lines[index]
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if len(line) - len(line.lstrip()) <= jobs_indent:
            jobs_end = index
            break
    candidates = [
        len(line) - len(line.lstrip())
        for line in lines[jobs_index + 1 : jobs_end]
        if line.strip()
        and not line.lstrip().startswith("#")
        and len(line) - len(line.lstrip()) > jobs_indent
    ]
    if not candidates:
        return ()
    job_indent = min(candidates)
    starts = [
        index
        for index, line in enumerate(
            lines[jobs_index + 1 : jobs_end], jobs_index + 1
        )
        if len(line) - len(line.lstrip()) == job_indent
        and re.fullmatch(r"\s*[A-Za-z0-9_.-]+:\s*(?:#.*)?\n?", line)
    ]
    return tuple(
        "".join(
            lines[
                start : starts[offset + 1]
                if offset + 1 < len(starts)
                else jobs_end
            ]
        )
        for offset, start in enumerate(starts)
    )


def _static_matrix_values(text: str, key: str) -> tuple[str, ...]:
    values: set[str] = set()
    lines = text.splitlines()
    matrix_pattern = re.compile(r"^(\s*)matrix:\s*(?:#.*)?$")
    key_pattern = re.compile(rf"^(\s*){re.escape(key)}:\s*$")
    value_pattern = re.compile(r"^(\s*)-\s*([A-Za-z0-9_.-]+)\s*$")
    for matrix_index, line in enumerate(lines):
        matrix_match = matrix_pattern.fullmatch(line)
        if matrix_match is None:
            continue
        matrix_indent = len(matrix_match.group(1))
        block_end = len(lines)
        for index in range(matrix_index + 1, len(lines)):
            candidate = lines[index]
            if not candidate.strip() or candidate.lstrip().startswith("#"):
                continue
            indent = len(candidate) - len(candidate.lstrip())
            if indent <= matrix_indent:
                block_end = index
                break
        child_indents = [
            len(candidate) - len(candidate.lstrip())
            for candidate in lines[matrix_index + 1 : block_end]
            if candidate.strip() and not candidate.lstrip().startswith("#")
        ]
        if not child_indents:
            continue
        child_indent = min(child_indents)
        for key_index in range(matrix_index + 1, block_end):
            key_match = key_pattern.fullmatch(lines[key_index])
            if key_match is None or len(key_match.group(1)) != child_indent:
                continue
            key_indent = len(key_match.group(1))
            list_indent: int | None = None
            for candidate in lines[key_index + 1 : block_end]:
                if not candidate.strip() or candidate.lstrip().startswith("#"):
                    continue
                indent = len(candidate) - len(candidate.lstrip())
                if indent <= key_indent:
                    break
                if list_indent is None:
                    list_indent = indent
                value_match = value_pattern.fullmatch(candidate)
                if value_match is None or indent != list_indent:
                    return ()
                values.add(value_match.group(2))
    return tuple(sorted(values))


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


def _metadata_checkouts(manifest: Manifest) -> list[Checkout]:
    component_counts = {
        checkout.name: sum(
            component.checkout_name == checkout.name
            for component in manifest.components
        )
        for checkout in manifest.checkouts
    }
    return [
        checkout
        for checkout in manifest.checkouts
        if component_counts[checkout.name] > 1
    ]


def _summary_checkout_path(row: dict[str, Any], component: Component) -> Path:
    checkout_path = row.get("checkout_path")
    if checkout_path is not None:
        return Path(checkout_path).resolve()
    path = Path(row["path"])
    if component.source == ".":
        return path.resolve()
    for _part in PurePosixPath(component.source).parts:
        path = path.parent
    return path.resolve()


def _component_source_root(checkout_root: Path, component: Component) -> Path:
    source = (
        checkout_root
        if component.source == "."
        else checkout_root.joinpath(*PurePosixPath(component.source).parts)
    )
    lexical = checkout_root
    parts = () if component.source == "." else PurePosixPath(component.source).parts
    for part in parts:
        lexical = lexical / part
        if lexical.is_symlink():
            raise WorkspaceError(
                f"{component.name} source uses a symlink in {checkout_root}: {lexical}"
            )
    try:
        resolved = source.resolve(strict=True)
    except OSError as error:
        raise WorkspaceError(
            f"{component.name} source is unavailable in {checkout_root}: {source}"
        ) from error
    if not resolved.is_dir():
        raise WorkspaceError(
            f"{component.name} source is not a directory in {checkout_root}: {source}"
        )
    try:
        resolved.relative_to(checkout_root.resolve(strict=True))
    except ValueError as error:
        raise WorkspaceError(
            f"{component.name} source escapes checkout {checkout_root}: {source}"
        ) from error
    return resolved


def _profile_component_rows(
    stack: Stack, summary: dict[str, Any]
) -> dict[str, dict[str, Any]]:
    raw_rows = summary["components"]
    rows = {row["component"]: row for row in raw_rows}
    expected_components = {component.name for component in stack.components}
    if len(rows) != len(raw_rows) or set(rows) != expected_components:
        raise WorkspaceError(
            f"profile summary component set does not match {stack.name} stack"
        )
    return rows


def repository_roots(
    root: Path,
    workspace: Any,
    profile: str,
    overrides: Iterable[str] = (),
) -> dict[str, Path]:
    manifest = Manifest.load(root / "components.json")
    inventory = Inventory.load(
        root / "supply-chain" / "inventory.json", root / "components.json"
    )
    summary = workspace.profile_summary(profile)
    stack = manifest.stack(summary["stack"])
    stack_components = {component.name: component for component in stack.components}
    audit_ready = {
        repository.name
        for repository in inventory.repositories
        if repository.supported and repository.audit_ready
    }
    selected: dict[str, Path] = {}
    selected_checkouts: dict[str, Path] = {}
    overridden_checkouts: set[str] = set()
    for override in overrides:
        name, separator, raw_path = override.partition("=")
        if not separator or name not in stack_components or not raw_path:
            raise WorkspaceError(
                "supply-chain repository override must be NAME=PATH for an "
                f"audit-ready component in the selected stack: {override}"
            )
        if name not in audit_ready:
            raise WorkspaceError(
                f"supply-chain repository override is not audit-ready: {name}"
            )
        component = stack_components[name]
        if component.checkout_name in overridden_checkouts:
            raise WorkspaceError(
                "duplicate supply-chain repository override for checkout "
                f"{component.checkout_name}: {name}"
            )
        if name in selected:
            raise WorkspaceError(f"duplicate supply-chain repository override: {name}")
        path = Path(raw_path)
        if not path.is_absolute():
            raise WorkspaceError(f"supply-chain repository override must be absolute: {override}")
        if path.is_symlink() or not path.is_dir():
            raise WorkspaceError(
                f"supply-chain repository override must be a normal directory: {override}"
            )
        resolved = path.resolve(strict=True)
        try:
            checkout_root = _git_top_level(resolved)
        except WorkspaceError as error:
            raise WorkspaceError(
                f"supply-chain repository override is not {component.repository}: "
                f"{resolved}: {error}"
            ) from error
        try:
            remote = _git_repository_remote(checkout_root, component.repository)
        except WorkspaceError as error:
            raise WorkspaceError(
                f"supply-chain repository override is not {component.repository}: "
                f"{resolved}: {error}"
            ) from error
        expected_source = _component_source_root(checkout_root, component)
        if resolved not in {checkout_root, expected_source}:
            raise WorkspaceError(
                "supply-chain repository override must select the checkout root "
                f"or {component.name} source directory: {resolved}"
            )
        checkout = manifest.checkout_for(component)
        variants = [
            candidate
            for candidate in manifest.checkouts
            if candidate.repository == checkout.repository
        ]
        canonical: Path | None = None
        exact_primary = (
            _git_current_branch(checkout_root) == checkout.branch
            and _git_repository_branch_compatible(
                checkout_root, component.branch, remote=remote
            )
        )
        if len(variants) > 1 and not exact_primary:
            canonical_path = root / checkout.path
            if not canonical_path.is_dir() or canonical_path.is_symlink():
                raise WorkspaceError(
                    f"cannot prove {checkout.name}@{checkout.branch} lineage; "
                    f"initialize its primary checkout first: {canonical_path}"
                )
            canonical = canonical_path.resolve()
            try:
                canonical_root = _git_top_level(canonical)
                _git_repository_remote(canonical_root, component.repository)
            except WorkspaceError as error:
                raise WorkspaceError(
                    f"cannot prove {checkout.name}@{checkout.branch} primary "
                    f"checkout identity: {canonical}: {error}"
                ) from error
            if canonical_root != canonical:
                raise WorkspaceError(
                    f"canonical checkout is not a Git top level: {canonical}"
                )
        if not _git_repository_branch_compatible(
            checkout_root,
            component.branch,
            canonical=canonical,
            remote=remote,
        ):
            raise WorkspaceError(
                "supply-chain repository override is not based on "
                f"{component.repository}@{component.branch}: {resolved}"
            )
        overridden_checkouts.add(component.checkout_name)
        for member in stack.components:
            if member.checkout_name != component.checkout_name:
                continue
            selected[member.name] = _component_source_root(checkout_root, member)
            selected_checkouts[member.name] = checkout_root
    rows = _profile_component_rows(stack, summary)
    missing_checkouts: dict[str, list[str]] = {}
    for component in stack.components:
        if component.name in selected or rows[component.name]["initialized"]:
            continue
        missing_checkouts.setdefault(component.checkout_name, []).append(
            component.name
        )
    if missing_checkouts:
        missing = ", ".join(
            f"{checkout} ({', '.join(components)})"
            for checkout, components in sorted(missing_checkouts.items())
        )
        if profile == stack.name:
            initialization = (
                "./atrinik init --with classic"
                if stack.name == "classic"
                else "./atrinik init"
            )
            remediation = (
                "initialize every selected checkout before auditing with "
                f"{initialization}"
            )
        else:
            remediation = (
                "repair its selectors or initialize their selected checkouts "
                "before auditing"
            )
        raise WorkspaceError(
            f"supply-chain profile {profile} is incomplete; {remediation}: {missing}"
        )

    roots = {"atrinik": root}
    for component in stack.components:
        if component.name not in audit_ready:
            continue
        if component.name in selected:
            roots[component.name] = selected[component.name]
        elif rows[component.name]["initialized"]:
            roots[component.name] = Path(rows[component.name]["path"])
    missing = sorted(
        component.name
        for component in stack.components
        if component.name in audit_ready and component.name not in roots
    )
    if missing:
        raise WorkspaceError(
            f"profile {profile} is incomplete for supply-chain audit; initialize "
            f"or override: {', '.join(missing)}"
        )
    for repository in inventory.repositories:
        if (
            not repository.supported
            or not repository.audit_ready
            or repository.audit_mode != "metadata"
            or stack.name not in repository.stacks
        ):
            continue
        members = [
            component
            for component in stack.components
            if component.checkout_name == repository.checkout
        ]
        override_candidates = {
            selected_checkouts[component.name]
            for component in members
            if component.name in selected_checkouts
        }
        candidates = override_candidates or {
            _summary_checkout_path(rows[component.name], component)
            for component in members
            if rows[component.name]["initialized"]
        }
        if len(candidates) > 1:
            raise WorkspaceError(
                f"profile {profile} resolves checkout {repository.checkout} "
                "to multiple physical roots"
            )
        if candidates:
            roots[repository.name] = candidates.pop()
    return roots


def report_component_commits(
    root: Path, workspace: Any, profile: str
) -> tuple[str, dict[str, str | None]]:
    manifest = Manifest.load(root / "components.json")
    summary = workspace.profile_summary(profile)
    stack = manifest.stack(summary["stack"])
    rows = _profile_component_rows(stack, summary)
    commits: dict[str, str | None] = {"atrinik": _git_head(root)}
    checkout_commits: dict[str, str] = {}
    for component in stack.components:
        row = rows[component.name]
        if not row["initialized"]:
            commits[component.name] = None
            continue
        if component.checkout_name not in checkout_commits:
            checkout_commits[component.checkout_name] = _git_head(
                _summary_checkout_path(row, component)
            )
        commits[component.name] = checkout_commits[component.checkout_name]
    for checkout in _metadata_checkouts(manifest):
        if stack.name in manifest.checkout_stacks(checkout.name):
            commits[checkout.name] = checkout_commits.get(checkout.name)
    return stack.name, commits


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
    url = result.stdout.strip()
    repository = _github_repository_from_url(url)
    if repository is None:
        raise WorkspaceError(f"unsupported repository remote for supply-chain audit: {url}")
    return repository


def _github_repository_from_url(url: str) -> str | None:
    normalized = url.strip().removesuffix(".git")
    prefixes = (
        "git@github.com:",
        "ssh://git@github.com/",
        "https://github.com/",
    )
    for prefix in prefixes:
        if normalized.startswith(prefix):
            repository = normalized.removeprefix(prefix)
            return repository if repository.count("/") == 1 else None
    return None


def _git_repository_remote(root: Path, repository: str) -> str:
    """Return the effective origin/upstream that names ``repository``.

    Git fetches the first URL configured for a remote.  A later URL that merely
    looks canonical must therefore not prove checkout identity.  Fork-based
    review worktrees remain valid when their effective ``origin`` is the fork
    and their effective ``upstream`` is the Atrinik repository.
    """

    candidates: list[str] = []
    for remote in ("origin", "upstream"):
        result = subprocess.run(
            ["git", "-C", str(root), "remote", "get-url", "--all", remote],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=10,
        )
        if result.returncode != 0:
            continue
        urls = result.stdout.splitlines()
        if not urls:
            continue
        url = urls[0].strip()
        candidates.append(f"{remote}={url}")
        if _github_repository_from_url(url) == repository:
            return remote
    detail = ", ".join(candidates) or "no effective origin/upstream URLs"
    raise WorkspaceError(
        f"checkout has no effective origin/upstream for {repository}: {detail}"
    )


def _git_repository_branch_compatible(
    root: Path,
    branch: str,
    canonical: Path | None = None,
    remote: str = "origin",
) -> bool:
    if canonical is not None:
        if not canonical.is_dir() or _git_current_branch(canonical) != branch:
            return False
        checkout_common = _git_common_directory(root)
        canonical_common = _git_common_directory(canonical)
        return checkout_common is not None and checkout_common == canonical_common
    reference = f"refs/remotes/{remote}/{branch}"
    verify = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "--verify", f"{reference}^{{commit}}"],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=10,
    )
    if verify.returncode != 0:
        return False
    ancestry = subprocess.run(
        ["git", "-C", str(root), "merge-base", "--is-ancestor", reference, "HEAD"],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=10,
    )
    return ancestry.returncode == 0


def _git_current_branch(root: Path) -> str | None:
    result = subprocess.run(
        ["git", "-C", str(root), "branch", "--show-current"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        timeout=10,
    )
    branch = result.stdout.strip()
    return branch if result.returncode == 0 and branch else None


def _git_common_directory(root: Path) -> Path | None:
    result = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "rev-parse",
            "--path-format=absolute",
            "--git-common-dir",
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        timeout=10,
    )
    if result.returncode != 0 or not result.stdout.strip():
        return None
    return Path(result.stdout.strip()).resolve(strict=False)


def _git_top_level(root: Path) -> Path:
    result = subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "rev-parse",
            "--path-format=absolute",
            "--show-toplevel",
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        timeout=10,
    )
    if result.returncode != 0 or not result.stdout.strip():
        raise WorkspaceError(f"cannot resolve Git checkout root for {root}")
    return Path(result.stdout.strip()).resolve(strict=True)


def _git_head(root: Path) -> str:
    result = subprocess.run(
        ["git", "-C", str(root), "rev-parse", "HEAD"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=10,
    )
    commit = result.stdout.strip()
    if result.returncode != 0 or not GIT_COMMIT_PATTERN.fullmatch(commit):
        raise WorkspaceError(f"cannot resolve full Git commit for {root}")
    return commit


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
