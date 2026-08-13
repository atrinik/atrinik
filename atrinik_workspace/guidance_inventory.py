from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
SKILLS_ROOT = ROOT / ".agents" / "skills"

# Byte ceilings are model-independent regression guards. Exact tokenizer counts
# remain PR evidence because tokenizer vocabularies and prompt wrappers change.
MAX_ROOT_GUIDE_BYTES = 6_000
MAX_CATALOG_BYTES = 2_000
MAX_STARTUP_BYTES = 8_000
MAX_MULTI_SELECTED_BYTES = 14_500
MAX_ALL_SKILL_BYTES = 50_000


@dataclass(frozen=True)
class FileMetrics:
    path: str
    bytes: int
    lines: int
    words: int


@dataclass(frozen=True)
class SkillMetrics:
    name: str
    description: str
    file: FileMetrics


def file_metrics(path: Path) -> FileMetrics:
    text = path.read_text(encoding="utf-8")
    return FileMetrics(
        path=path.relative_to(ROOT).as_posix(),
        bytes=len(text.encode("utf-8")),
        lines=len(text.splitlines()),
        words=len(text.split()),
    )


def skill_frontmatter(path: Path) -> tuple[str, str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "---":
        raise ValueError(f"{path}: missing YAML frontmatter")
    try:
        end = lines.index("---", 1)
    except ValueError as exc:
        raise ValueError(f"{path}: unterminated YAML frontmatter") from exc

    values: dict[str, str] = {}
    for line in lines[1:end]:
        key, separator, value = line.partition(":")
        if not separator or key not in {"name", "description"} or not value.strip():
            raise ValueError(f"{path}: unsupported frontmatter line: {line!r}")
        if key in values:
            raise ValueError(f"{path}: duplicate frontmatter key: {key}")
        values[key] = value.strip()
    if set(values) != {"name", "description"}:
        raise ValueError(f"{path}: frontmatter requires name and description")
    if path.parent.name != values["name"]:
        raise ValueError(f"{path}: skill name must match its directory")
    return values["name"], values["description"]


def collect_inventory() -> dict[str, object]:
    root_guide = file_metrics(ROOT / "AGENTS.md")
    skills = []
    for path in sorted(SKILLS_ROOT.glob("*/SKILL.md")):
        name, description = skill_frontmatter(path)
        skills.append(SkillMetrics(name, description, file_metrics(path)))

    catalog_text = "".join(
        f"{skill.name}\t{skill.description}\t{skill.file.path}\n" for skill in skills
    )
    catalog_bytes = len(catalog_text.encode("utf-8"))
    skill_bytes = sum(skill.file.bytes for skill in skills)
    multi = next(
        (
            skill
            for skill in skills
            if skill.name == "atrinik-multi-repo-workspace"
        ),
        None,
    )
    if multi is None:
        raise ValueError("missing atrinik-multi-repo-workspace skill")
    startup_bytes = root_guide.bytes + catalog_bytes

    return {
        "root_guide": asdict(root_guide),
        "skills": [asdict(skill) for skill in skills],
        "summary": {
            "skill_count": len(skills),
            "catalog_bytes": catalog_bytes,
            "startup_bytes": startup_bytes,
            "multi_selected_bytes": startup_bytes + multi.file.bytes,
            "all_skill_bytes": skill_bytes,
        },
    }


def budget_failures(inventory: dict[str, object]) -> list[str]:
    root = inventory["root_guide"]
    summary = inventory["summary"]
    checks = {
        "root guide": (root["bytes"], MAX_ROOT_GUIDE_BYTES),
        "skill catalog": (summary["catalog_bytes"], MAX_CATALOG_BYTES),
        "wrapper startup": (summary["startup_bytes"], MAX_STARTUP_BYTES),
        "wrapper + multi skill": (
            summary["multi_selected_bytes"],
            MAX_MULTI_SELECTED_BYTES,
        ),
        "all skill bodies": (summary["all_skill_bytes"], MAX_ALL_SKILL_BYTES),
    }
    return [
        f"{name}: {actual} bytes exceeds {limit}"
        for name, (actual, limit) in checks.items()
        if actual > limit
    ]


def render_text(inventory: dict[str, object]) -> str:
    rows = [inventory["root_guide"]]
    rows.extend(skill["file"] for skill in inventory["skills"])
    lines = ["path\tbytes\tlines\twords"]
    lines.extend(
        f"{row['path']}\t{row['bytes']}\t{row['lines']}\t{row['words']}"
        for row in rows
    )
    summary = inventory["summary"]
    lines.append(
        "summary"
        f"\tcatalog={summary['catalog_bytes']}"
        f"\tstartup={summary['startup_bytes']}"
        f"\tmulti-selected={summary['multi_selected_bytes']}"
        f"\tall-skills={summary['all_skill_bytes']}"
    )
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Inventory wrapper agent guidance")
    parser.add_argument("--json", action="store_true", help="emit stable JSON")
    parser.add_argument("--check", action="store_true", help="enforce byte ceilings")
    args = parser.parse_args(argv)

    try:
        inventory = collect_inventory()
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"guidance inventory failed: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(inventory, indent=2, sort_keys=True))
    else:
        print(render_text(inventory))

    failures = budget_failures(inventory) if args.check else []
    for failure in failures:
        print(f"guidance budget failed: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
