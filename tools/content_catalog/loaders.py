"""Load Atrinik's authored content into a typed identity graph."""

from __future__ import annotations

import posixpath
import re
from pathlib import Path
from typing import Dict, Iterable, Iterator, List, Optional, Sequence, Tuple
from xml.parsers import expat

from .model import ContentCatalog, ContentId, SourceLocation


QUEST_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_.-]*$")
QUEST_PART_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_.-]*$")
OBJECT_DOMAINS = ("archetype", "artifact")


def _iter_source_lines(path: Path) -> Iterator[Tuple[int, str]]:
    """Yield significant lines while respecting legacy msg/endmsg blocks."""

    in_message = False
    with path.open(encoding="utf-8") as source:
        for line_number, raw_line in enumerate(source, 1):
            line = raw_line.strip()
            if in_message:
                if line == "endmsg":
                    in_message = False
                continue
            if line == "msg":
                in_message = True
                continue
            if not line or line.startswith("#"):
                continue
            yield line_number, line


def _split(line: str) -> Tuple[str, str]:
    parts = line.split(None, 1)
    return parts[0], parts[1] if len(parts) == 2 else ""


def _column(line: str, value: str) -> int:
    position = line.find(value)
    return position + 1 if position >= 0 else 1


def _load_archetypes(catalog: ContentCatalog, arch_root: Path) -> None:
    for path in sorted(arch_root.rglob("*.arc")):
        current: Optional[ContentId] = None
        current_type: Optional[str] = None
        current_location: Optional[SourceLocation] = None
        multipart_continuation = False
        inside_object = False

        for line_number, line in _iter_source_lines(path):
            field, value = _split(line)
            if field == "More" and not value:
                multipart_continuation = True
                continue
            if field == "Object" and value:
                inside_object = True
                current_type = None
                current_location = catalog.location(path, line_number, _column(line, value))
                if multipart_continuation:
                    current = None
                else:
                    current = catalog.add_definition(
                        "archetype", value, current_location
                    )
                multipart_continuation = False
                continue
            if not inside_object:
                continue
            if field == "type" and value:
                current_type = value.split()[0]
            elif field == "other_arch" and value:
                catalog.add_reference(
                    value.split()[0],
                    ("archetype",),
                    catalog.location(path, line_number, _column(line, value)),
                    "other_arch",
                    current,
                )
            elif field == "randomitems" and value:
                catalog.add_reference(
                    value.split()[0],
                    ("treasure",),
                    catalog.location(path, line_number, _column(line, value)),
                    "randomitems",
                    current,
                )
            elif field == "end" and not value:
                if current is not None and current_type in ("29", "43"):
                    domain = "spell" if current_type == "29" else "skill"
                    catalog.add_definition(
                        domain,
                        current.key,
                        current_location or catalog.location(path, line_number),
                        {"archetype": current.key},
                    )
                inside_object = False
                current = None
                current_type = None


def _load_runtime_identity_tables(catalog: ContentCatalog, server_root: Path) -> None:
    """Validate explicit stable IDs used by process-local C lookup tables."""

    table_specs = (
        (server_root / "src/include/spellist.h", "spell", "spell table id"),
        (server_root / "src/include/skillist.h", "skill", "skill table id"),
    )
    for path, domain, field in table_specs:
        if not path.is_file():
            continue
        seen: Dict[str, SourceLocation] = {}
        with path.open(encoding="utf-8") as source:
            for line_number, line in enumerate(source, 1):
                for match in re.finditer(r'\{"((?:spell|skill)_[a-z0-9_]+)"\s*,', line):
                    key = match.group(1)
                    location = catalog.location(path, line_number, match.start(1) + 1)
                    previous = seen.get(key)
                    if previous is not None:
                        catalog.add_diagnostic(
                            "duplicate-runtime-id",
                            "duplicate {} {}; first declared at {}".format(
                                domain, key, previous.display()
                            ),
                            location,
                            related=previous,
                        )
                        continue
                    seen[key] = location
                    # Some legacy skill enum slots do not have an obtainable
                    # skill archetype. Existing authored skills must map to
                    # stable table IDs; unused slots remain process-local.
                    if domain == "spell" or any(
                        definition.content_id == ContentId(domain, key)
                        for definition in catalog.definitions
                    ):
                        catalog.add_reference(key, (domain,), location, field)

        table_ids = set(seen)
        for definition in catalog.definitions:
            if definition.content_id.domain != domain:
                continue
            if definition.content_id.key not in table_ids:
                catalog.add_diagnostic(
                    "missing-runtime-id",
                    "{} {} has no stable entry in {}".format(
                        domain, definition.content_id.key, path.name
                    ),
                    definition.location,
                )


def _load_artifacts(catalog: ContentCatalog, roots: Sequence[Path]) -> None:
    for root in roots:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*.art")):
            current: Optional[ContentId] = None
            for line_number, line in _iter_source_lines(path):
                field, value = _split(line)
                if field == "artifact" and value:
                    location = catalog.location(path, line_number, _column(line, value))
                    current = catalog.add_definition("artifact", value, location)
                elif field == "def_arch" and value:
                    catalog.add_reference(
                        value.split()[0],
                        OBJECT_DOMAINS,
                        catalog.location(path, line_number, _column(line, value)),
                        "def_arch",
                        current,
                    )
                elif field == "end" and not value:
                    current = None


def _load_treasures(catalog: ContentCatalog, roots: Sequence[Path]) -> None:
    for root in roots:
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*.trs")):
            current: Optional[ContentId] = None
            for line_number, line in _iter_source_lines(path):
                field, value = _split(line)
                if field in ("treasure", "treasureone") and value:
                    location = catalog.location(path, line_number, _column(line, value))
                    current = catalog.add_definition("treasure", value, location)
                elif field == "arch" and value:
                    catalog.add_reference(
                        value.split()[0],
                        OBJECT_DOMAINS,
                        catalog.location(path, line_number, _column(line, value)),
                        "treasure arch",
                        current,
                    )
                elif field == "list" and value and value != "NONE":
                    catalog.add_reference(
                        value.split()[0],
                        ("treasure",),
                        catalog.location(path, line_number, _column(line, value)),
                        "treasure list",
                        current,
                    )


def _load_factions(catalog: ContentCatalog, maps_root: Path) -> None:
    parents: Dict[str, Tuple[str, SourceLocation]] = {}
    for path in sorted(maps_root.rglob("*.factions")):
        stack: List[ContentId] = []
        for line_number, line in _iter_source_lines(path):
            field, value = _split(line)
            if field == "faction" and value:
                location = catalog.location(path, line_number, _column(line, value))
                content_id = catalog.add_definition("faction", value, location)
                if stack:
                    parent = stack[-1]
                    catalog.add_reference(
                        parent.key, ("faction",), location, "nested faction parent", content_id
                    )
                    parents[content_id.key] = (parent.key, location)
                stack.append(content_id)
            elif field == "parent" and value and stack:
                location = catalog.location(path, line_number, _column(line, value))
                catalog.add_reference(value, ("faction",), location, "faction parent", stack[-1])
                parents[stack[-1].key] = (value, location)
            elif field == "enemy" and value:
                catalog.add_reference(
                    value,
                    ("faction",),
                    catalog.location(path, line_number, _column(line, value)),
                    "faction enemy",
                    stack[-1] if stack else None,
                )
            elif field == "end" and not value and stack:
                stack.pop()
    catalog.check_cycles("faction", parents)


def _load_regions(catalog: ContentCatalog, maps_root: Path) -> None:
    path = maps_root / "regions.reg"
    if not path.is_file():
        return
    current: Optional[ContentId] = None
    parents: Dict[str, Tuple[str, SourceLocation]] = {}
    for line_number, line in _iter_source_lines(path):
        field, value = _split(line)
        if field == "region" and value:
            location = catalog.location(path, line_number, _column(line, value))
            current = catalog.add_definition("region", value, location)
        elif field == "parent" and value and current is not None:
            location = catalog.location(path, line_number, _column(line, value))
            catalog.add_reference(value, ("region",), location, "region parent", current)
            parents[current.key] = (value, location)
        elif field in ("map_first", "jail") and value:
            map_key = value.split()[0]
            catalog.add_reference(
                _canonical_map_path(map_key),
                ("map",),
                catalog.location(path, line_number, _column(line, map_key)),
                "region {}".format(field),
                current,
            )
        elif field == "end" and not value:
            current = None
    catalog.check_cycles("region", parents)


def _canonical_map_path(path: str, base: Optional[str] = None) -> str:
    if path.startswith("/"):
        normalized = posixpath.normpath(path)
    else:
        normalized = posixpath.normpath(posixpath.join(base or "/", path))
    return "/" + normalized.lstrip("/")


def _is_map_file(path: Path) -> bool:
    try:
        with path.open("rb") as source:
            prefix = source.read(4096)
    except OSError:
        return False
    if b"\0" in prefix:
        return False
    try:
        text = prefix.decode("utf-8")
    except UnicodeDecodeError:
        return False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        return line == "arch map"
    return False


def _load_maps(catalog: ContentCatalog, maps_root: Path) -> None:
    ignored_suffixes = {
        ".art",
        ".dtd",
        ".factions",
        ".md",
        ".png",
        ".py",
        ".pyc",
        ".reg",
        ".rst",
        ".trs",
        ".txt",
        ".xml",
    }
    for path in sorted(item for item in maps_root.rglob("*") if item.is_file()):
        if path.suffix.lower() in ignored_suffixes or not _is_map_file(path):
            continue
        map_key = "/" + path.relative_to(maps_root).as_posix()
        map_id = catalog.add_definition("map", map_key, catalog.location(path, 1))
        in_header = True
        # A map may place thousands of identical objects. One edge per target
        # preserves identity validation without turning the catalog artifact
        # into a second, much larger encoding of the map itself.
        seen_archetypes = set()
        for line_number, line in _iter_source_lines(path):
            field, value = _split(line)
            if in_header:
                if field == "region" and value:
                    catalog.add_reference(
                        value,
                        ("region",),
                        catalog.location(path, line_number, _column(line, value)),
                        "map region",
                        map_id,
                    )
                elif field.startswith("tile_path_") and value:
                    target = _canonical_map_path(value, posixpath.dirname(map_key))
                    catalog.add_reference(
                        target,
                        ("map",),
                        catalog.location(path, line_number, _column(line, value)),
                        field,
                        map_id,
                    )
                elif field == "end" and not value:
                    in_header = False
            elif field == "arch" and value:
                target = value.split()[0]
                if target in seen_archetypes:
                    continue
                seen_archetypes.add(target)
                catalog.add_reference(
                    target,
                    OBJECT_DOMAINS,
                    catalog.location(path, line_number, _column(line, value)),
                    "map arch",
                    map_id,
                )


class _InterfaceLoader:
    QUEST_FIELDS = ("start", "complete", "fail", "started", "finished", "completed", "failed")

    def __init__(self, catalog: ContentCatalog, path: Path, quest_key: Optional[str]):
        self.catalog = catalog
        self.path = path
        self.quest_key = quest_key
        self.quest_id: Optional[ContentId] = None
        self.part_stack: List[str] = []
        self.parser = expat.ParserCreate()
        self.parser.StartElementHandler = self._start
        self.parser.EndElementHandler = self._end

    def location(self, attribute_value: Optional[str] = None) -> SourceLocation:
        column = self.parser.CurrentColumnNumber + 1
        if attribute_value:
            column += 1
        return self.catalog.location(self.path, self.parser.CurrentLineNumber, column)

    def parse(self) -> None:
        try:
            with self.path.open("rb") as source:
                self.parser.ParseFile(source)
        except expat.ExpatError as error:
            self.catalog.add_diagnostic(
                "invalid-xml",
                str(error),
                self.catalog.location(self.path, error.lineno, error.offset + 1),
            )

    def _start(self, name: str, attrs: Dict[str, str]) -> None:
        if name == "quest":
            if self.quest_key is None:
                self.catalog.add_diagnostic(
                    "quest-location",
                    "quest definitions must be stored below maps/interfaces/quests/<uid>/",
                    self.location(),
                )
            else:
                if not QUEST_ID_RE.fullmatch(self.quest_key):
                    self.catalog.add_diagnostic(
                        "invalid-quest-id",
                        "quest directory '{}' is not a stable identifier".format(self.quest_key),
                        self.location(),
                    )
                self.quest_id = self.catalog.add_definition(
                    "quest", self.quest_key, self.location(), {"name": attrs.get("name", "")}
                )
        elif name == "part":
            uid = attrs.get("uid", "")
            if not QUEST_PART_ID_RE.fullmatch(uid):
                self.catalog.add_diagnostic(
                    "invalid-quest-part-id",
                    "quest part uid '{}' must match {}".format(uid, QUEST_PART_ID_RE.pattern),
                    self.location(uid),
                )
            self.part_stack.append(uid)
            if self.quest_key is not None:
                key = self.quest_key + "::" + "::".join(self.part_stack)
                self.catalog.add_definition(
                    "quest-part", key, self.location(uid), {"uid": uid}
                )

        source = self.quest_id
        if name in ("item", "object") and attrs.get("arch"):
            self.catalog.add_reference(
                attrs["arch"], OBJECT_DOMAINS, self.location(attrs["arch"]), "{} arch".format(name), source
            )
        if attrs.get("cast"):
            spell_key = "spell_" + re.sub(r"\s+", "_", attrs["cast"].strip().lower())
            self.catalog.add_reference(spell_key, ("spell",), self.location(attrs["cast"]), "cast", source)
        if attrs.get("teleport"):
            target = attrs["teleport"].split()[0]
            self.catalog.add_reference(
                _canonical_map_path(target), ("map",), self.location(target), "teleport", source
            )
        if attrs.get("region_map"):
            self.catalog.add_reference(
                attrs["region_map"], ("region",), self.location(attrs["region_map"]), "region_map", source
            )
        for attribute, value in attrs.items():
            if attribute.startswith("faction_"):
                self.catalog.add_reference(
                    value, ("faction",), self.location(value), attribute, source
                )
        if self.quest_key is not None:
            for field in self.QUEST_FIELDS:
                value = attrs.get(field)
                if value:
                    key = self.quest_key + "::" + value
                    self.catalog.add_reference(
                        key, ("quest-part",), self.location(value), field, source
                    )

    def _end(self, name: str) -> None:
        if name == "part" and self.part_stack:
            self.part_stack.pop()


def _load_interfaces(catalog: ContentCatalog, maps_root: Path) -> None:
    interfaces_root = maps_root / "interfaces"
    quests_root = interfaces_root / "quests"
    if not interfaces_root.is_dir():
        return
    for path in sorted(interfaces_root.rglob("*.xml")):
        quest_key = None
        try:
            relative = path.relative_to(quests_root)
            if len(relative.parts) >= 2:
                quest_key = relative.parts[0]
        except ValueError:
            pass
        _InterfaceLoader(catalog, path, quest_key).parse()


def load_catalog(root: Path) -> ContentCatalog:
    """Build and resolve a catalog from an Atrinik source tree."""

    catalog = ContentCatalog(root)
    arch_root = root / "arch"
    maps_root = root / "maps"
    _load_archetypes(catalog, arch_root)
    _load_runtime_identity_tables(catalog, root / "server")
    _load_artifacts(catalog, (arch_root, maps_root))
    _load_treasures(catalog, (arch_root, maps_root))
    _load_factions(catalog, maps_root)
    _load_maps(catalog, maps_root)
    _load_regions(catalog, maps_root)
    _load_interfaces(catalog, maps_root)
    catalog.resolve_references()
    return catalog
