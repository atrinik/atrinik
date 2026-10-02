from __future__ import annotations

import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parent
PROFILE_PATH = ROOT / "profiles.json"
ACCEPTANCE_PATH = ROOT / "fixtures" / "acceptance.json"
MAX_TOOLS = 12
DECISIONS = {"build", "configure", "defer", "reject"}
MUTATION_WORDS = {
    "add",
    "create",
    "delete",
    "deploy",
    "dispatch",
    "merge",
    "publish",
    "remove",
    "rerun",
    "trigger",
    "update",
    "write",
}
SECRET_VALUE_MARKERS = ("ghp_", "github_pat_", "glsa_", "bearer ")


class ProfileError(ValueError):
    pass


def _load(path: Path) -> Any:
    def unique(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ProfileError(f"duplicate key in {path.name}: {key}")
            result[key] = value
        return result

    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)
    except (OSError, UnicodeError, ValueError, RecursionError) as error:
        raise ProfileError(f"invalid external profile file: {path.name}") from error


def _tool_sets(profile: dict[str, Any]) -> list[list[str]]:
    catalog = profile["catalog"]
    if "tools" in catalog:
        return [catalog["tools"]]
    return list(catalog.get("task_tool_sets", {}).values())


def validate_documents(
    document: dict[str, Any], acceptance: dict[str, Any], template: str
) -> dict[str, int]:
    if document.get("schema_version") != "atrinik.mcp.external-profiles/v1":
        raise ProfileError("unsupported profile schema")
    if document.get("live_acceptance") is not False:
        raise ProfileError("evaluation must not claim live acceptance")

    profiles = document.get("profiles")
    if not isinstance(profiles, list) or not profiles:
        raise ProfileError("profiles must be a non-empty list")
    identifiers = [profile.get("id") for profile in profiles]
    if len(identifiers) != len(set(identifiers)):
        raise ProfileError("profile identifiers must be unique")
    if set(identifiers) != {
        "browser-verification",
        "cloudflare-operational",
        "grafana-prometheus-dashboard-read-only",
    }:
        raise ProfileError("required external profiles are incomplete")

    visible_catalogs = 0
    for profile in profiles:
        decision = profile.get("decision")
        if decision not in DECISIONS:
            raise ProfileError(f"invalid decision for {profile['id']}")
        for field in (
            "authorization",
            "catalog",
            "data_boundary",
            "failure_behavior",
            "fallback",
            "implementation",
            "required_flags",
            "status",
            "trust",
        ):
            if field not in profile:
                raise ProfileError(f"{profile['id']} lacks {field}")

        maximum = profile["catalog"].get("maximum_visible_tools")
        if not isinstance(maximum, int) or isinstance(maximum, bool):
            raise ProfileError(f"{profile['id']} has invalid catalog maximum")
        for tools in _tool_sets(profile):
            if len(tools) != len(set(tools)):
                raise ProfileError(f"{profile['id']} repeats a tool")
            if len(tools) > MAX_TOOLS or len(tools) > maximum:
                raise ProfileError(f"{profile['id']} exceeds the tool budget")
            visible_catalogs += 1
            if decision != "reject":
                for tool in tools:
                    words = set(tool.lower().split("_"))
                    if words & MUTATION_WORDS:
                        raise ProfileError(
                            f"{profile['id']} exposes mutation-shaped tool {tool}"
                        )

        implementation = profile["implementation"]
        if decision != "reject" and (
            implementation.get("version") in (None, "", "latest", "unavailable")
            or implementation.get("revision") in (None, "", "unavailable")
        ):
            raise ProfileError(f"{profile['id']} is not pinned")
        credential_environment = profile["authorization"].get(
            "credential_environment", []
        )
        if not all(
            isinstance(name, str) and name and name.upper() == name
            for name in credential_environment
        ):
            raise ProfileError(f"{profile['id']} has an invalid credential name")

    browser = next(
        profile for profile in profiles if profile["id"] == "browser-verification"
    )
    if "--isolated=true" not in browser["required_flags"]:
        raise ProfileError("browser must use a temporary profile")
    for flag in (
        "--javascript-evaluation=false",
        "--performance-crux=false",
        "--usage-statistics=false",
    ):
        if flag not in browser["required_flags"]:
            raise ProfileError(f"browser safety flag is absent: {flag}")
    if not any(flag.startswith("--allowed-url-pattern=") for flag in browser["required_flags"]):
        raise ProfileError("browser origin allowlist is absent")
    cloudflare = next(
        profile for profile in profiles if profile["id"] == "cloudflare-operational"
    )
    if cloudflare["decision"] != "reject" or any(_tool_sets(cloudflare)):
        raise ProfileError("Cloudflare must remain rejected with no tools")
    grafana = next(
        profile
        for profile in profiles
        if profile["id"] == "grafana-prometheus-dashboard-read-only"
    )
    if grafana["required_flags"] != [
        "--enabled-tools=prometheus,dashboard",
        "--disable-write",
    ]:
        raise ProfileError("Grafana must retain the narrow read-only flags")
    if "loki" in " ".join(_tool_sets(grafana)[0]).lower():
        raise ProfileError("Grafana profile must not expose Loki")

    rendered = "\n".join(
        (
            json.dumps(document, sort_keys=True),
            json.dumps(acceptance, sort_keys=True),
            template,
        )
    ).lower()
    if any(marker in rendered for marker in SECRET_VALUE_MARKERS):
        raise ProfileError("profile files appear to contain a credential value")

    cases = acceptance.get("cases", [])
    if {case.get("profile") for case in cases} != set(identifiers):
        raise ProfileError("acceptance fixtures do not cover every profile")
    if not all(case.get("synthetic") is True for case in cases):
        raise ProfileError("acceptance fixtures must be synthetic")
    if not all(
        isinstance(case.get("assertions"), list) and case["assertions"]
        for case in cases
    ):
        raise ProfileError("acceptance vectors must retain concrete assertions")
    adversarial = {
        case.get("id"): case.get("expected")
        for case in acceptance.get("adversarial", [])
    }
    required_adversarial = {
        "cancellation": "CANCELLED",
        "credential-in-error": "FORBIDDEN_DATA",
        "cross-authorization-cache": "STALE_IDENTITY",
        "external-outage": "OFFLINE",
        "oversized-result": "LIMIT_EXCEEDED",
        "prompt-injected-remote-content": "UNTRUSTED_DATA",
        "redirect-origin-escape": "AUTHORIZATION",
        "version-drift": "VERSION_DRIFT",
    }
    if adversarial != required_adversarial:
        raise ProfileError("adversarial acceptance coverage is incomplete")

    return {
        "profiles": len(profiles),
        "catalogs": visible_catalogs,
        "acceptance_cases": len(cases),
        "adversarial_cases": len(adversarial),
    }


def validate() -> dict[str, int]:
    document = _load(PROFILE_PATH)
    acceptance = _load(ACCEPTANCE_PATH)
    try:
        template = (ROOT / "templates" / "codex.config.toml.example").read_text(
            encoding="utf-8"
        )
    except (OSError, UnicodeError) as error:
        raise ProfileError("invalid external profile template") from error
    return validate_documents(document, acceptance, template)


def main() -> int:
    try:
        result = validate()
    except ProfileError as error:
        print(f"external profile validation failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
