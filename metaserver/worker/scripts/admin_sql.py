#!/usr/bin/env python3
"""Generate reviewable D1 SQL for metaserver ownership administration.

This tool never connects to Cloudflare. Redirect its output to a file, review it,
and then pass that file to ``wrangler d1 execute --remote --file``.
"""

from __future__ import annotations

import argparse
import ipaddress
import json
import re
import sys
from pathlib import Path
from typing import Any

HEX_128 = re.compile(r"^[0-9a-f]{128}$")
HEX_64 = re.compile(r"^[0-9a-f]{64}$")


def server_identity(value: object) -> str:
    if not isinstance(value, str) or HEX_64.fullmatch(value.lower()) is None:
        raise ValueError("server_id must be 64 hexadecimal characters")
    return value.lower()


def sql_string(value: object) -> str:
    if not isinstance(value, str):
        raise ValueError(f"expected a string, got {type(value).__name__}")
    if "\x00" in value:
        raise ValueError("SQL strings cannot contain NUL")
    return "'" + value.replace("'", "''") + "'"


def timestamp(record: dict[str, Any], field: str) -> int:
    value = record.get(field, 0)
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ValueError(f"{field} must be a non-negative integer")
    return value


def owner_insert(record: dict[str, Any]) -> str:
    server_id = server_identity(record.get("server_id"))
    auth_key = record.get("auth_key")
    if not isinstance(auth_key, str) or HEX_128.fullmatch(auth_key) is None:
        raise ValueError(f"{server_id}: auth_key must be 128 lowercase hex characters")
    current_ip_value = record.get("current_ip")
    if not isinstance(current_ip_value, str):
        raise ValueError(f"{server_id}: current_ip must be a string")
    current_ip = str(ipaddress.ip_address(current_ip_value))
    ip_changed_at = timestamp(record, "ip_changed_at")
    created_at = timestamp(record, "created_at")
    updated_at = timestamp(record, "updated_at")
    return (
        "INSERT INTO server_owners "
        "(server_id, auth_key, current_ip, ip_changed_at, created_at, updated_at) "
        f"VALUES ({sql_string(server_id)}, {sql_string(auth_key)}, "
        f"{sql_string(current_ip)}, {ip_changed_at}, {created_at}, {updated_at});"
    )


def load_owner_records(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError("owner import must be a JSON array")
    records: list[dict[str, Any]] = []
    seen: set[str] = set()
    for index, item in enumerate(data):
        if not isinstance(item, dict):
            raise ValueError(f"owner record {index} must be an object")
        server_id = server_identity(item.get("server_id"))
        if server_id in seen:
            raise ValueError(f"duplicate server identity: {server_id}")
        seen.add(server_id)
        records.append(item)
    return records


def command_import_owners(args: argparse.Namespace) -> str:
    statements = [owner_insert(record) for record in load_owner_records(args.input)]
    return "\n".join([*statements, ""])


def command_reset_owner(args: argparse.Namespace) -> str:
    server_id = server_identity(args.server_id)
    quoted = sql_string(server_id)
    return (
        "-- Destructive recovery: the server must delete its local metaserver key "
        "before re-registering this identity.\n"
        f"DELETE FROM servers WHERE server_id = {quoted};\n"
        f"DELETE FROM server_owners WHERE server_id = {quoted};\n"
    )


def validate_glob(pattern: str) -> str:
    if not pattern or len(pattern) > 253 or "\x00" in pattern:
        raise ValueError("blacklist pattern must contain 1-253 characters")
    return pattern.lower()


def command_blacklist_add(args: argparse.Namespace) -> str:
    pattern = validate_glob(args.pattern)
    if len(args.reason) > 256:
        raise ValueError("blacklist reason must be at most 256 characters")
    return (
        "INSERT INTO server_blacklist (pattern, reason, created_at) "
        f"VALUES ({sql_string(pattern)}, {sql_string(args.reason)}, unixepoch()) "
        "ON CONFLICT(pattern) DO UPDATE SET "
        "reason = excluded.reason, created_at = excluded.created_at;\n"
    )


def command_blacklist_remove(args: argparse.Namespace) -> str:
    return (
        "DELETE FROM server_blacklist WHERE pattern = "
        f"{sql_string(validate_glob(args.pattern))};\n"
    )


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)

    import_owners = commands.add_parser(
        "import-owners",
        help="convert a JSON owner export into a reviewable D1 SQL import",
    )
    import_owners.add_argument("input", type=Path)
    import_owners.set_defaults(handler=command_import_owners)

    reset_owner = commands.add_parser(
        "reset-owner",
        help="generate SQL that removes one owner and all of its listings",
    )
    reset_owner.add_argument("server_id")
    reset_owner.set_defaults(handler=command_reset_owner)

    blacklist_add = commands.add_parser("blacklist-add")
    blacklist_add.add_argument("pattern")
    blacklist_add.add_argument("reason")
    blacklist_add.set_defaults(handler=command_blacklist_add)

    blacklist_remove = commands.add_parser("blacklist-remove")
    blacklist_remove.add_argument("pattern")
    blacklist_remove.set_defaults(handler=command_blacklist_remove)
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        sys.stdout.write(args.handler(args))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
