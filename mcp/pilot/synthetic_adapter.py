#!/usr/bin/env python3
"""Synthetic, corpus-backed adapter used only to test the pilot harness."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKLOADS = ROOT / "mcp" / "contract" / "v1" / "fixtures" / "workloads.json"
ERRORS = {
    "fully-disabled-baseline": "UNSUPPORTED_OPERATION",
    "server-down": "OFFLINE",
    "stale-coordinate": "STALE_COORDINATE",
    "auth-revoked": "UNAUTHORIZED",
    "offline": "OFFLINE",
    "disabled": "UNSUPPORTED_OPERATION",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workload", required=True)
    parser.add_argument("--scenario", required=True)
    args = parser.parse_args()
    cases = json.loads(WORKLOADS.read_text(encoding="utf-8"))["cases"]
    case = next((item for item in cases if item["id"] == args.workload), None)
    if case is None or args.scenario not in {"enabled", *ERRORS}:
        return 2
    result = {
        "schema_version": "atrinik.mcp.pilot-adapter/v1",
        "synthetic": True,
        "workload_id": case["id"],
        "scenario": args.scenario,
        "mutation_count": 0,
        "metrics": {
            "calls": 1 if args.scenario == "enabled" else 0,
            "retries": 0,
            "tool_schema_bytes": 0,
            "context_bytes": 0,
            "cache_hits": 0,
            "cache_misses": 0,
            "external_network": False,
        },
    }
    if args.scenario == "enabled":
        result["answer"] = case["expected"]
    else:
        result["error_code"] = ERRORS[args.scenario]
        result["fallback_used"] = True
    print(json.dumps(result, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
