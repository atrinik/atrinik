# Copyright 2026 The Atrinik Project
# SPDX-License-Identifier: MIT
"""Offline contract for the separately installed Atrinik skill provider."""
from __future__ import annotations

import json
from pathlib import Path
import re

DESCRIPTOR_PATH = ".agents/skill-provider.json"
REQUIRED_SKILLS = (
    "atrinik-c-change",
    "atrinik-content-change",
    "atrinik-github-governance",
    "atrinik-guidance-maintenance",
    "atrinik-issue-delivery",
    "atrinik-linux-gpu-qualification",
    "atrinik-multi-repo-workspace",
    "atrinik-program-delivery",
    "atrinik-project-delivery",
    "atrinik-protocol-change",
    "atrinik-server-runtime",
    "atrinik-test-scenario",
)
_FIELDS = {"schema_version", "repository", "revision", "marketplace", "plugin", "path", "required_skills"}


def _unique(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("duplicate skill-provider field: " + key)
        value[key] = item
    return value


def parse_provider(raw: str | bytes) -> dict:
    """Validate provider metadata for any selected repository, without loading it."""
    if len(raw.encode("utf-8") if isinstance(raw, str) else raw) > 32_768:
        raise ValueError("skill provider descriptor exceeds its byte bound")
    value = json.loads(raw, object_pairs_hook=_unique)
    if not isinstance(value, dict) or set(value) != _FIELDS:
        raise ValueError("skill provider requires exactly the documented descriptor fields")
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        raise ValueError("unsupported skill provider schema")
    repository = value["repository"]
    if (not isinstance(repository, str) or not re.fullmatch(
            r"https://github\.com/[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9][A-Za-z0-9_.-]{0,99}", repository)
            or repository.endswith(".git")):
        raise ValueError("skill provider requires a canonical GitHub repository URL")
    name_pattern = r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*"
    for field in ("marketplace", "plugin"):
        name = value[field]
        if not isinstance(name, str) or len(name) > 64 or not re.fullmatch(name_pattern, name):
            raise ValueError("invalid skill provider identity")
    path = value["path"]
    if (not isinstance(path, str) or len(path) > 512 or not 1 <= len(path.split("/")) <= 8
            or any(not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", part) for part in path.split("/"))):
        raise ValueError("skill provider path must be a safe relative directory")
    if (not isinstance(value["revision"], str) or not re.fullmatch(r"[0-9a-f]{40}", value["revision"])
            or value["revision"] == "0" * 40):
        raise ValueError("skill provider revision must be a full lowercase 40-character commit")
    skills = value["required_skills"]
    if (not isinstance(skills, list) or not 1 <= len(skills) <= 50
            or any(not isinstance(name, str) or len(name) > 64 or not re.fullmatch(name_pattern, name)
                   for name in skills) or len(set(skills)) != len(skills)):
        raise ValueError("skill provider requires bounded unique valid skill names")
    return value


def load_provider(root: Path) -> dict:
    """Enforce this wrapper's consumer contract in addition to the generic schema."""
    value = parse_provider((root / DESCRIPTOR_PATH).read_bytes())
    expected = {"repository": "https://github.com/atrinik/agent-skills",
                "marketplace": "atrinik", "plugin": "atrinik-development",
                "path": "plugins/atrinik-development"}
    if any(value[key] != item for key, item in expected.items()):
        raise ValueError("unsupported wrapper skill provider identity")
    if value["required_skills"] != list(REQUIRED_SKILLS):
        raise ValueError("skill provider required_skills must be the sorted unique wrapper contract")
    return value


def validate_routes(root: Path, provider: dict) -> None:
    """Validate local entry routing without inspecting installation or network."""
    guide = (root / "AGENTS.md").read_text(encoding="utf-8")
    routes = set(re.findall(r"`(atrinik-[a-z-]+)`", guide))
    if not routes <= set(provider["required_skills"]) | {provider["plugin"]}:
        raise ValueError("root guidance routes to an undeclared provider skill")
    if ".agents/skills/" in guide:
        raise ValueError("root guidance contains a retired local skill route")
    if ".agents/skill-provider.json" not in guide or "docs/SKILL_PROVIDER.md" not in guide:
        raise ValueError("root guidance must name the provider descriptor and setup route")
    setup = root / "docs/SKILL_PROVIDER.md"
    if not setup.is_file():
        raise ValueError("missing skill provider setup guidance")
    for relative in ("AGENTS.md", "README.md", "CONTRIBUTING.md", "docs/SOURCE_DELIVERY.md",
                     "docs/ARCHITECTURE.md", "docs/MCP_CONTEXT.md", "docs/SKILL_PROVIDER.md"):
        path = root / relative
        for target in re.findall(r"\[[^]]+\]\(([^)]+)\)", path.read_text(encoding="utf-8")):
            if "://" in target or target.startswith("#"):
                continue
            if ".agents/skills/" in target:
                raise ValueError(f"retired local skill link in {relative}")
            resolved = (path.parent / target.split("#", 1)[0]).resolve()
            if not resolved.is_relative_to(root.resolve()) or not resolved.exists():
                raise ValueError(f"unresolved local guidance route in {relative}")
