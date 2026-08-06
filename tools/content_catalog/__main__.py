"""Command-line interface for the Atrinik content catalog."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Optional, Sequence

from .loaders import load_catalog


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate Atrinik content identities and cross-references."
    )
    parser.add_argument(
        "command", choices=("validate", "emit"), nargs="?", default="validate"
    )
    parser.add_argument(
        "--root", type=Path, default=Path.cwd(), help="Atrinik source-tree root"
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="JSON output path (required for emit; relative paths use --root)",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _parser().parse_args(argv)
    root = args.root.resolve()
    catalog = load_catalog(root)

    for diagnostic in catalog.diagnostics:
        print(diagnostic.format(), file=sys.stderr)

    counts = ", ".join(
        "{}={}".format(domain, count) for domain, count in catalog.counts().items()
    )
    print(
        "Content catalog: {} definitions, {} references ({})".format(
            len(catalog.definitions), len(catalog.references), counts
        )
    )

    if args.command == "emit":
        if args.output is None:
            print("error: emit requires --output", file=sys.stderr)
            return 2
        output = args.output if args.output.is_absolute() else root / args.output
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("w", encoding="utf-8", newline="\n") as destination:
            json.dump(catalog.to_dict(), destination, indent=2, sort_keys=True)
            destination.write("\n")

    return 1 if catalog.has_errors else 0


if __name__ == "__main__":
    sys.exit(main())
