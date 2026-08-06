"""Typed content identity graph and deterministic diagnostics."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple


@dataclass(frozen=True, order=True)
class ContentId:
    """A canonical identifier qualified by its content domain."""

    domain: str
    key: str

    def __str__(self) -> str:
        return "{}:{}".format(self.domain, self.key)


@dataclass(frozen=True, order=True)
class SourceLocation:
    """A stable, repository-relative source location."""

    path: str
    line: int
    column: int = 1

    def display(self) -> str:
        return "{}:{}:{}".format(self.path, self.line, self.column)

    def to_dict(self) -> Mapping[str, object]:
        return {
            "path": self.path,
            "line": self.line,
            "column": self.column,
        }


@dataclass(frozen=True)
class Definition:
    """A canonical content definition."""

    content_id: ContentId
    location: SourceLocation
    metadata: Mapping[str, object] = field(default_factory=dict)

    def to_dict(self) -> Mapping[str, object]:
        return {
            "domain": self.content_id.domain,
            "key": self.content_id.key,
            "location": self.location.to_dict(),
            "metadata": dict(sorted(self.metadata.items())),
        }


@dataclass(frozen=True)
class Reference:
    """A typed edge from authored content to a canonical definition."""

    key: str
    allowed_domains: Tuple[str, ...]
    location: SourceLocation
    field: str
    source: Optional[ContentId] = None

    def to_dict(self) -> Mapping[str, object]:
        data = {
            "key": self.key,
            "allowed_domains": list(self.allowed_domains),
            "field": self.field,
            "location": self.location.to_dict(),
        }
        if self.source is not None:
            data["source"] = {
                "domain": self.source.domain,
                "key": self.source.key,
            }
        return data


@dataclass(frozen=True, order=True)
class Diagnostic:
    """An actionable catalog problem."""

    location: SourceLocation
    code: str
    message: str
    severity: str = "error"
    related: Optional[SourceLocation] = field(default=None, compare=True)

    def format(self) -> str:
        return "{}: {} {}: {}".format(
            self.location.display(), self.severity, self.code, self.message
        )

    def to_dict(self) -> Mapping[str, object]:
        data = {
            "severity": self.severity,
            "code": self.code,
            "message": self.message,
            "location": self.location.to_dict(),
        }
        if self.related is not None:
            data["related"] = self.related.to_dict()
        return data


class ContentCatalog:
    """Mutable builder for definitions, references, and diagnostics."""

    SCHEMA_VERSION = 1

    def __init__(self, root: Path):
        self.root = root.resolve()
        self._definitions: Dict[ContentId, Definition] = {}
        self._references: List[Reference] = []
        self._diagnostics: List[Diagnostic] = []

    @property
    def definitions(self) -> Sequence[Definition]:
        return tuple(
            sorted(
                self._definitions.values(),
                key=lambda definition: (
                    definition.content_id.domain,
                    definition.content_id.key,
                    definition.location,
                ),
            )
        )

    @property
    def references(self) -> Sequence[Reference]:
        return tuple(
            sorted(
                self._references,
                key=lambda reference: (
                    reference.location,
                    reference.field,
                    reference.allowed_domains,
                    reference.key,
                ),
            )
        )

    @property
    def diagnostics(self) -> Sequence[Diagnostic]:
        return tuple(
            sorted(
                self._diagnostics,
                key=lambda diagnostic: (
                    diagnostic.location,
                    diagnostic.severity,
                    diagnostic.code,
                    diagnostic.message,
                    diagnostic.related or SourceLocation("", 0, 0),
                ),
            )
        )

    @property
    def has_errors(self) -> bool:
        return any(item.severity == "error" for item in self._diagnostics)

    def location(self, path: Path, line: int, column: int = 1) -> SourceLocation:
        try:
            relative = path.resolve().relative_to(self.root)
        except ValueError:
            relative = path.resolve()
        return SourceLocation(relative.as_posix(), line, column)

    def add_definition(
        self,
        domain: str,
        key: str,
        location: SourceLocation,
        metadata: Optional[Mapping[str, object]] = None,
    ) -> ContentId:
        content_id = ContentId(domain, key)
        definition = Definition(content_id, location, metadata or {})
        previous = self._definitions.get(content_id)
        if previous is not None:
            self.add_diagnostic(
                "duplicate-id",
                "duplicate {}; first defined at {}".format(
                    content_id, previous.location.display()
                ),
                location,
                related=previous.location,
            )
        else:
            self._definitions[content_id] = definition
        return content_id

    def add_reference(
        self,
        key: str,
        allowed_domains: Iterable[str],
        location: SourceLocation,
        field: str,
        source: Optional[ContentId] = None,
    ) -> None:
        self._references.append(
            Reference(key, tuple(sorted(set(allowed_domains))), location, field, source)
        )

    def add_diagnostic(
        self,
        code: str,
        message: str,
        location: SourceLocation,
        severity: str = "error",
        related: Optional[SourceLocation] = None,
    ) -> None:
        self._diagnostics.append(
            Diagnostic(location, code, message, severity, related)
        )

    def resolve_references(self) -> None:
        """Resolve all typed edges after every domain has been loaded."""

        keys_by_domain: Dict[str, set] = {}
        domains_by_key: Dict[str, set] = {}
        for content_id in self._definitions:
            keys_by_domain.setdefault(content_id.domain, set()).add(content_id.key)
            domains_by_key.setdefault(content_id.key, set()).add(content_id.domain)

        for reference in self._references:
            matches = [
                domain
                for domain in reference.allowed_domains
                if reference.key in keys_by_domain.get(domain, set())
            ]
            if len(matches) == 1:
                continue
            if len(matches) > 1:
                self.add_diagnostic(
                    "ambiguous-reference",
                    "{} '{}' resolves in multiple allowed domains: {}".format(
                        reference.field, reference.key, ", ".join(matches)
                    ),
                    reference.location,
                )
                continue

            actual_domains = sorted(domains_by_key.get(reference.key, set()))
            if actual_domains:
                self.add_diagnostic(
                    "wrong-domain-reference",
                    "{} '{}' expects {}, but the key exists as {}".format(
                        reference.field,
                        reference.key,
                        ", ".join(reference.allowed_domains),
                        ", ".join(actual_domains),
                    ),
                    reference.location,
                )
            else:
                self.add_diagnostic(
                    "missing-reference",
                    "{} '{}' does not resolve in {}".format(
                        reference.field,
                        reference.key,
                        ", ".join(reference.allowed_domains),
                    ),
                    reference.location,
                )

    def check_cycles(self, domain: str, edges: Mapping[str, Tuple[str, SourceLocation]]) -> None:
        """Report cycles in a single-parent identity hierarchy."""

        visited = set()
        for start in sorted(edges):
            if start in visited:
                continue
            path: List[str] = []
            positions = {}
            current = start
            while current in edges and current not in visited:
                if current in positions:
                    cycle = path[positions[current] :] + [current]
                    _, location = edges[current]
                    self.add_diagnostic(
                        "identity-cycle",
                        "{} hierarchy contains a cycle: {}".format(
                            domain, " -> ".join(cycle)
                        ),
                        location,
                    )
                    break
                positions[current] = len(path)
                path.append(current)
                current = edges[current][0]
            visited.update(path)

    def check_shared_namespace(self, namespace: str, domains: Iterable[str]) -> None:
        """Reject IDs that collide in a runtime namespace shared by domains."""

        selected_domains = set(domains)
        definitions_by_key: Dict[str, List[Definition]] = {}
        for definition in self._definitions.values():
            if definition.content_id.domain in selected_domains:
                definitions_by_key.setdefault(definition.content_id.key, []).append(
                    definition
                )

        for key, definitions in sorted(definitions_by_key.items()):
            if len(definitions) < 2:
                continue
            definitions.sort(key=lambda definition: definition.content_id.domain)
            first = definitions[0]
            for definition in definitions[1:]:
                self.add_diagnostic(
                    "shared-namespace-collision",
                    "{} key '{}' is defined in both {} and {}".format(
                        namespace,
                        key,
                        first.content_id.domain,
                        definition.content_id.domain,
                    ),
                    definition.location,
                    related=first.location,
                )

    def counts(self) -> Mapping[str, int]:
        counts: Dict[str, int] = {}
        for definition in self._definitions.values():
            domain = definition.content_id.domain
            counts[domain] = counts.get(domain, 0) + 1
        return dict(sorted(counts.items()))

    def to_dict(self) -> Mapping[str, object]:
        return {
            "schema_version": self.SCHEMA_VERSION,
            "counts": self.counts(),
            "definitions": [definition.to_dict() for definition in self.definitions],
            "references": [reference.to_dict() for reference in self.references],
            "diagnostics": [diagnostic.to_dict() for diagnostic in self.diagnostics],
        }
