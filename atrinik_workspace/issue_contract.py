from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys


MAX_ISSUE_BYTES = 512 * 1024
_RETIRED_REFERENCE = r"(?:`?1\.x`?|`?content-1x`?)"
_ACTION = (
    r"(?:backport|back-port|publish|release|ship|select|target|checkout|"
    r"create|recreate|maintain|use|merge|edit|modify|label|branch|maintenance)"
)
_ACTIVE_PATTERNS = (
    re.compile(
        rf"\b{_ACTION}\b[^.!?\n]{{0,120}}\b{_RETIRED_REFERENCE}\b",
        re.IGNORECASE,
    ),
    re.compile(
        rf"\b{_RETIRED_REFERENCE}\b[^.!?\n]{{0,120}}\b{_ACTION}\b",
        re.IGNORECASE,
    ),
    re.compile(
        rf"\breleased\s+on\s+@?{_RETIRED_REFERENCE}\b",
        re.IGNORECASE,
    ),
)
_HISTORICAL_CONTEXT = re.compile(
    r"\b(?:historical|retired|former|immutable|inert|preserved|frozen|"
    r"no\s+longer|no\s+live|no\s+future|no\s+new|no\s+active|never|"
    r"do\s+not|does\s+not|must\s+not|"
    r"should\s+not|cannot|without)\b",
    re.IGNORECASE,
)
_MARKDOWN_BLOCK_START = re.compile(
    r"^(?:[-+*]\s+|\d+[.)]\s+|#{1,6}\s+|>\s?|\|)"
)


class IssueContractError(ValueError):
    """A bounded, stable issue-contract validation failure."""


@dataclass(frozen=True)
class ContractViolation:
    line: int
    text: str


def _markdown_blocks(lines: list[str]) -> tuple[tuple[int, str], ...]:
    """Join wrapped Markdown blocks while retaining each block's first line."""

    blocks: list[tuple[int, str]] = []
    start: int | None = None
    current: list[str] = []

    def flush() -> None:
        nonlocal start, current
        if start is not None:
            blocks.append((start, " ".join(part.strip() for part in current)))
        start = None
        current = []

    for index, line in enumerate(lines, start=1):
        stripped = line.strip()
        if not stripped:
            flush()
            continue
        if current and _MARKDOWN_BLOCK_START.match(stripped):
            flush()
        if start is None:
            start = index
        current.append(line)
    flush()
    return tuple(blocks)


def find_violations(body: str) -> tuple[ContractViolation, ...]:
    """Return active retired-line requirements found in one issue body."""

    if len(body.encode("utf-8")) > MAX_ISSUE_BYTES:
        raise IssueContractError(
            f"issue body exceeds the {MAX_ISSUE_BYTES}-byte validation bound"
        )

    violations: list[ContractViolation] = []
    for line_number, block in _markdown_blocks(body.splitlines()):
        if not re.search(_RETIRED_REFERENCE, block, re.IGNORECASE):
            continue
        if _HISTORICAL_CONTEXT.search(block):
            continue
        if any(pattern.search(block) for pattern in _ACTIVE_PATTERNS):
            violations.append(
                ContractViolation(
                    line=line_number,
                    text=block or "retired-line requirement",
                )
            )
    return tuple(violations)


def validate_issue_contract(body: str) -> None:
    violations = find_violations(body)
    if violations:
        details = "; ".join(
            f"line {violation.line}: {violation.text}" for violation in violations
        )
        raise IssueContractError(
            "active 1.x/content-1x delivery requirement; use content@main "
            "and the Classic-target artifact instead ("
            f"{details})"
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Reject active retired 1.x delivery requirements in an issue body."
    )
    parser.add_argument("path", nargs="?", type=Path)
    parser.add_argument(
        "--stdin", action="store_true", help="read the issue body from standard input"
    )
    args = parser.parse_args(argv)
    if args.stdin == (args.path is not None):
        parser.error("provide exactly one of PATH or --stdin")

    body = sys.stdin.read() if args.stdin else args.path.read_text(encoding="utf-8")
    try:
        validate_issue_contract(body)
    except (OSError, UnicodeError, IssueContractError) as error:
        print(f"issue-contract: {error}", file=sys.stderr)
        return 1
    print("issue-contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
