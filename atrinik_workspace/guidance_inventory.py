from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
SKILLS_ROOT = ROOT / ".agents" / "skills"

# Byte ceilings are model-independent regression guards. Exact tokenizer counts
# remain PR evidence because tokenizer vocabularies and prompt wrappers change.
MAX_ROOT_GUIDE_BYTES = 6_450
MAX_CATALOG_BYTES = 2_000
MAX_STARTUP_BYTES = 8_400
MAX_MULTI_SELECTED_BYTES = 15_100
# The delivery coordinator-context contract is intentionally kept in the
# issue-delivery skill so Windows-hosted agents see the gate at invocation.
MAX_ALL_SKILL_BYTES = 54_100

PROCESS_IMPROVEMENT_LEDGER = Path("build/agent-process-improvements.md")
PROCESS_TABLE_HEADER = (
    "| Key | Status | Observation | Expected benefit / proposed action | "
    "Related issue / PR | Last observed (UTC) |"
)
PROCESS_STATUSES = frozenset(
    {"active", "adopted", "deferred", "observed", "proposed", "resolved"}
)
PROCESS_KEY = re.compile(r"^[a-z0-9][a-z0-9-]{1,63}$")
PROCESS_TIMESTAMP = re.compile(
    r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z$"
)
PROCESS_SECRET_PATTERNS = (
    re.compile(r"-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----"),
    re.compile(
        r"\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,}|"
        r"glpat-[A-Za-z0-9_-]{20,}|xox[baprs]-[A-Za-z0-9-]{10,}|"
        r"AKIA[0-9A-Z]{16})\b"
    ),
    re.compile(
        r"(?i)\b(?:password|passphrase|secret|token|api(?:[_ -]?key)?|"
        r"access[_ -]?token|authorization|cookie)\s*[:=]\s*[^\s|`]+"
    ),
    re.compile(
        r"(?i)\b(?:player|account|character)\s+(?:id|name|password)"
        r"\s*[:=]\s*[^\s|`]+"
    ),
    re.compile(
        r"(?i)\b(?:host|hostname|server|private[_ -]?host)\s*[:=]\s*"
        r"[^\s|`]+"
    ),
    re.compile(r"(?i)(?:^|[\s|])(?:/home|/users)/[^\s|]+"),
    re.compile(r"(?i)(?:^|[\s|])[a-z]:[\\/]+users[\\/][^\s|]+"),
    re.compile(r"(?m)(?:^|[\s|])(?:\d{1,3}\.){3}\d{1,3}(?=$|[\s|])"),
)


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


def process_improvement_ledger_path(root: Path | None = None) -> Path:
    return (root or ROOT) / PROCESS_IMPROVEMENT_LEDGER


def _process_ledger_ignore_error(root: Path, path: Path) -> str | None:
    relative = path.relative_to(root).as_posix()
    try:
        result = subprocess.run(
            ["git", "check-ignore", "--no-index", "--quiet", "--", relative],
            cwd=root,
            capture_output=True,
            check=False,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired):
        return "cannot verify that the process-improvement ledger path is ignored"
    if result.returncode:
        return "process-improvement ledger path is not ignored"
    return None


def _valid_process_timestamp(value: str) -> bool:
    if not PROCESS_TIMESTAMP.fullmatch(value):
        return False
    try:
        datetime.fromisoformat(value[:-1] + "+00:00")
    except ValueError:
        return False
    return True


def validate_process_improvement_ledger(root: Path | None = None) -> list[str]:
    root = root or ROOT
    path = process_improvement_ledger_path(root)
    if path.is_symlink():
        return ["process-improvement ledger must not be a symlink"]
    if not path.exists():
        return []
    if not path.is_file():
        return ["process-improvement ledger must be a regular file"]

    errors: list[str] = []
    ignore_error = _process_ledger_ignore_error(root, path)
    if ignore_error:
        errors.append(ignore_error)
    try:
        raw = path.read_bytes()
        if len(raw) > 128 * 1024:
            errors.append("process-improvement ledger exceeds 128 KiB")
        text = raw.decode("utf-8")
    except (OSError, UnicodeError):
        return errors + ["process-improvement ledger must be bounded UTF-8"]

    if not text.endswith("\n"):
        errors.append("process-improvement ledger must end with a newline")
    if any(
        (ord(char) < 32 and char not in "\n\r\t") or ord(char) == 127
        for char in text
    ):
        errors.append("process-improvement ledger contains control characters")
    if any(pattern.search(text) for pattern in PROCESS_SECRET_PATTERNS):
        errors.append("process-improvement ledger contains secret-like content")

    lines = text.splitlines()
    if not lines or lines[0].strip() != "# Agent process improvements":
        errors.append("process-improvement ledger requires its canonical title")
    try:
        header_index = next(
            index for index, line in enumerate(lines) if line.strip() == PROCESS_TABLE_HEADER
        )
    except StopIteration:
        errors.append("process-improvement ledger requires its canonical table")
        return errors

    if header_index + 1 >= len(lines):
        errors.append("process-improvement ledger table requires a separator")
        return errors
    separator_cells = [
        cell.strip() for cell in lines[header_index + 1].split("|")[1:-1]
    ]
    if len(separator_cells) != 6 or any(
        not re.fullmatch(r":?-{3,}:?", cell) for cell in separator_cells
    ):
        errors.append("process-improvement ledger table separator is invalid")

    keys: set[str] = set()
    row_count = 0
    for line in lines[header_index + 2 :]:
        stripped = line.strip()
        if not stripped:
            if row_count:
                break
            continue
        if not stripped.startswith("|"):
            if row_count:
                break
            continue
        cells = [cell.strip() for cell in stripped.split("|")[1:-1]]
        if len(cells) != 6:
            errors.append("process-improvement ledger row must have six fields")
            continue
        row_count += 1
        key = cells[0].strip("`")
        if not PROCESS_KEY.fullmatch(key):
            errors.append("process-improvement ledger has an invalid stable key")
        elif key in keys:
            errors.append("process-improvement ledger has duplicate stable keys")
        keys.add(key)
        if cells[1].strip("`").lower() not in PROCESS_STATUSES:
            errors.append("process-improvement ledger has an invalid status")
        if any(not cell for cell in cells[2:5]):
            errors.append("process-improvement ledger rows require all descriptive fields")
        if not _valid_process_timestamp(cells[5].strip("`")):
            errors.append("process-improvement ledger requires UTC last-observed timestamps")

    if row_count == 0:
        errors.append("process-improvement ledger requires at least one row")
    return errors


def collect_inventory() -> dict[str, object]:
    process_errors = validate_process_improvement_ledger()
    if process_errors:
        raise ValueError("; ".join(process_errors))
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
        "process_improvements": {
            "path": PROCESS_IMPROVEMENT_LEDGER.as_posix(),
            "present": process_improvement_ledger_path().exists(),
        },
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
