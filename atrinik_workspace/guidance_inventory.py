from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
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
TOOLING_LEDGER_MAX_BYTES = 128 * 1024
TOOLING_LEDGER_RELATIVE = Path('build/agent-tooling-issues.md')
TOOLING_LEDGER_COLUMNS = (
    'stable key',
    'status',
    'observation',
    'impact',
    'recommended action',
)
_TOOLING_KEY = re.compile(
    r'mechanism=[a-z0-9][a-z0-9._-]*;remediation=[a-z0-9][a-z0-9._-]*'
)
_TOOLING_STATUSES = frozenset({'open', 'monitoring', 'resolved', 'blocked'})
_SECRET_FIELD = re.compile(
    r'(?:password|passphrase|secret|token|credential|api(?:[_ -]?key)?|'
    r'private(?:[_ -]?key)?|authorization|cookie|host(?:name)?|'
    r'machine(?:[_ -]?id)?|user(?:name)?|email|path)',
    re.IGNORECASE,
)
_SECRET_VALUE = re.compile(
    r'(?:-----BEGIN [^-]*PRIVATE KEY-----|'
    r'\b(?:gh[pousr]_|github_pat_|xox[baprs]-|AKIA[0-9A-Z]{16})[A-Za-z0-9._-]+|'
    r'\b(?:password|passwd|passphrase|secret|token|credential|api[_ -]?key|'
    r'authorization|cookie|host(?:name)?|machine(?:[_ -]?id)?|'
    r'user(?:name)?|email|path)\b\s*[:=]\s*'
    r'(?!<redacted>|redacted\b|omitted\b|none\b)\S+|'
    r'https?://[^/\s:@]+:[^@\s]+@|'
    r'(?<![\w.])(?:\d{1,3}\.){3}\d{1,3}(?![\w.]))',
    re.IGNORECASE,
)
_PRIVATE_HOST_PATH = re.compile(
    r'(?:[A-Za-z]:[\\/]|\\\\[^\\/\s]+[\\/]|'
    r'/(?:home|Users|mnt/[A-Za-z]/Users)(?:/|$))',
    re.IGNORECASE,
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


def _markdown_row(line: str) -> list[str] | None:
    stripped = line.strip()
    if not (stripped.startswith('|') and stripped.endswith('|')):
        return None
    return [cell.strip() for cell in stripped[1:-1].split('|')]


def _tooling_cell(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] == '`':
        return value[1:-1].strip()
    return value


def _table_separator(row: list[str] | None) -> bool:
    return bool(row) and all(re.fullmatch(r':?-{3,}:?', cell) for cell in row)


def _git_check(root: Path, *arguments: str) -> int | None:
    try:
        return subprocess.run(
            ['git', *arguments],
            cwd=root,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        ).returncode
    except (OSError, ValueError):
        return None


def validate_tooling_ledger(root: Path = ROOT) -> list[str]:
    '''Validate optional local tooling-ledger state without requiring it.'''

    relative = TOOLING_LEDGER_RELATIVE.as_posix()
    failures: list[str] = []
    ignored = _git_check(
        root, 'check-ignore', '--quiet', '--no-index', '--', relative
    )
    if ignored != 0:
        failures.append(f'{relative} is not ignored')
    tracked = _git_check(root, 'ls-files', '--error-unmatch', '--', relative)
    if tracked == 0:
        failures.append(f'{relative} is tracked')

    path = root / TOOLING_LEDGER_RELATIVE
    if not path.exists() and not path.is_symlink():
        return failures
    if path.is_symlink() or not path.is_file():
        failures.append(f'{relative} is not a regular file')
        return failures
    try:
        raw = path.read_bytes()
    except OSError:
        failures.append(f'{relative} could not be read')
        return failures
    if len(raw) > TOOLING_LEDGER_MAX_BYTES:
        failures.append(f'{relative} exceeds the size limit')
        return failures
    try:
        text = raw.decode('utf-8')
    except UnicodeDecodeError:
        failures.append(f'{relative} is not UTF-8')
        return failures
    if '\x00' in text:
        failures.append(f'{relative} contains binary data')
    if _SECRET_VALUE.search(text) or _PRIVATE_HOST_PATH.search(text):
        failures.append(f'{relative} contains a secret-like value or private host path')

    lines = text.splitlines()
    for index, line in enumerate(lines[:-1]):
        header = _markdown_row(line)
        separator = _markdown_row(lines[index + 1])
        if header and _table_separator(separator) and any(
            _SECRET_FIELD.search(_tooling_cell(cell)) for cell in header
        ):
            failures.append(f'{relative} contains a secret-like field')
            break

    header_index = next(
        (
            index
            for index, line in enumerate(lines)
            if (row := _markdown_row(line))
            and [_tooling_cell(cell).casefold() for cell in row]
            == list(TOOLING_LEDGER_COLUMNS)
        ),
        None,
    )
    if header_index is None:
        failures.append(f'{relative} is missing the required Markdown table')
        return failures
    separator = (
        _markdown_row(lines[header_index + 1])
        if header_index + 1 < len(lines)
        else None
    )
    if not _table_separator(separator) or len(separator) != len(TOOLING_LEDGER_COLUMNS):
        failures.append(f'{relative} has an invalid Markdown table separator')
        return failures

    keys: set[str] = set()
    for line in lines[header_index + 2 :]:
        row = _markdown_row(line)
        if row is None:
            if line.strip().startswith('|'):
                failures.append(f'{relative} has a malformed table row')
            break
        if len(row) != len(TOOLING_LEDGER_COLUMNS):
            failures.append(f'{relative} has a malformed table row')
            continue
        values = [_tooling_cell(cell) for cell in row]
        key, status = values[:2]
        if any(not value for value in values):
            failures.append(f'{relative} has an empty required field')
        if not _TOOLING_KEY.fullmatch(key):
            failures.append(f'{relative} has an invalid stable key')
        if status not in _TOOLING_STATUSES:
            failures.append(f'{relative} has an invalid status')
        if key in keys:
            failures.append(f'{relative} has a duplicate stable key')
        keys.add(key)
    return failures


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
    tooling_failures = validate_tooling_ledger(ROOT) if args.check else []
    for failure in failures:
        print(f"guidance budget failed: {failure}", file=sys.stderr)
    for failure in tooling_failures:
        print(f'guidance tooling ledger failed: {failure}', file=sys.stderr)
    return 1 if failures or tooling_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
