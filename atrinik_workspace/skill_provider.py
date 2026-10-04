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
    value = json.loads(raw, object_pairs_hook=_unique)
    if not isinstance(value, dict) or set(value) != _FIELDS:
        raise ValueError("skill provider requires exactly the documented descriptor fields")
    expected = {"schema_version": 1, "repository": "https://github.com/atrinik/agent-skills",
                "marketplace": "atrinik", "plugin": "atrinik-development",
                "path": "plugins/atrinik-development"}
    if type(value["schema_version"]) is not int or any(value[key] != item for key, item in expected.items()):
        raise ValueError("unsupported skill provider identity or schema")
    if not isinstance(value["revision"], str) or not re.fullmatch(r"[0-9a-f]{40}", value["revision"]) or value["revision"] == "0" * 40:
        raise ValueError("skill provider revision must be a full lowercase 40-character commit")
    if value["required_skills"] != list(REQUIRED_SKILLS):
        raise ValueError("skill provider required_skills must be the sorted unique wrapper contract")
    return value


def load_provider(root: Path) -> dict:
    return parse_provider((root / DESCRIPTOR_PATH).read_bytes())


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
