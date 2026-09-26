"""Pure, bounded envelopes for retained Classic producer advancement.

The envelope records inputs and producer observations separately from historical
correction bytes. It is not authority to execute a build or control a runtime.
Live ownership, Git, producer and lease proofs belong to the delivery helper.
"""
from __future__ import annotations

import ast
import hashlib
import json
import re
from typing import Any

class WorkspaceError(ValueError):
    """Invalid bounded advancement evidence (independent of runtime imports)."""

STAGES = ("declare", "plan", "built", "topology")
PIN_FIELDS = ("CONSUMER_COMMIT", "PRODUCER_COMMIT", "IMAGE", "PLATFORM_MANIFEST",
              "METADATA_HASHES", "PRODUCER_FILES_SHA256")
HEX40 = re.compile(r"[0-9a-f]{40}")
HEX64 = re.compile(r"[0-9a-f]{64}")


def digest(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, ensure_ascii=True, sort_keys=True,
                                     separators=(",", ":")).encode("ascii")).hexdigest()


def exact(value: Any, keys: set[str], label: str) -> dict:
    if not isinstance(value, dict) or set(value) != keys:
        raise WorkspaceError(f"retained advance {label} has an unsupported shape")
    return value


def hex_value(value: Any, pattern: re.Pattern, label: str) -> str:
    if not isinstance(value, str) or pattern.fullmatch(value) is None:
        raise WorkspaceError(f"retained advance {label} is invalid")
    return value


def accepted_portable_pins(raw: bytes) -> dict:
    """Read literal pins from a proved accepted Git blob without executing it."""
    if len(raw) > 1024 * 1024:
        raise WorkspaceError("retained advance producer declaration exceeds bound")
    try:
        tree = ast.parse(raw.decode("utf-8"))
    except (UnicodeError, SyntaxError) as error:
        raise WorkspaceError("retained advance producer declaration is invalid") from error
    found = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        names = [target.id for target in node.targets if isinstance(target, ast.Name)]
        for name in set(names) & set(PIN_FIELDS):
            if name in found:
                raise WorkspaceError("retained advance producer declaration repeats a pin")
            try:
                found[name] = ast.literal_eval(node.value)
            except (ValueError, TypeError) as error:
                raise WorkspaceError("retained advance producer pin is not a literal") from error
    validate_pins(found)
    return found


def validate_pins(value: Any) -> dict:
    value = exact(value, set(PIN_FIELDS), "portable pins")
    for field in ("CONSUMER_COMMIT", "PRODUCER_COMMIT"):
        hex_value(value[field], HEX40, field)
    hex_value(value["PRODUCER_FILES_SHA256"], HEX64, "producer inventory")
    if not isinstance(value["IMAGE"], str) or re.fullmatch(
            r"ghcr\.io/atrinik/classic-portable-build@sha256:[0-9a-f]{64}", value["IMAGE"]) is None:
        raise WorkspaceError("retained advance portable image is not an immutable producer")
    if not isinstance(value["PLATFORM_MANIFEST"], str) or re.fullmatch(
            r"sha256:[0-9a-f]{64}", value["PLATFORM_MANIFEST"]) is None:
        raise WorkspaceError("retained advance platform manifest is invalid")
    metadata = value["METADATA_HASHES"]
    required = {"contract.json", "debian-sources.json", "installed.json", "runtime-abi.json",
                "runtime-sources.json", "shader-generation.json"}
    exact(metadata, required, "producer metadata inventory")
    for name, checksum in metadata.items():
        hex_value(checksum, HEX64, name)
    return value


def validate_plan(plan: Any, *, profile: str, wrapper: str, workspace: str,
                  classic_root: str, classic_head: str, content_input: dict | None = None) -> dict:
    """Check the complete public producer shape before its live recomputation."""
    keys = {"schema_version", "target", "profile", "tests", "force_reconfigure", "use_ccache",
            "targets", "manifest", "checkout_states", "source_fingerprints", "git_observations",
            "sources", "execution_sources", "wrapper_root", "workspace_root", "builds_root",
            "build_key", "build_root", "plan_sha256"}
    if isinstance(plan, dict) and "retained_content_input" in plan:
        keys.add("retained_content_input")
    exact(plan, keys, "public build plan")
    unsigned = {key: value for key, value in plan.items() if key != "plan_sha256"}
    if (plan["schema_version"] != 1 or isinstance(plan["schema_version"], bool)
            or plan["target"] != "server" or plan["tests"] is not True
            or not isinstance(plan["force_reconfigure"], bool)
            or not isinstance(plan["use_ccache"], bool)
            or not isinstance(plan["profile"], dict) or plan["profile"].get("name") != profile
            or plan["profile"].get("stack") != "classic"
            or plan["wrapper_root"] != wrapper or plan["workspace_root"] != workspace
            or plan["builds_root"] != workspace + "/build"
            or not isinstance(plan["build_key"], str)
            or re.fullmatch(r"[0-9a-f]{12}", plan["build_key"]) is None
            or plan["build_root"] != workspace + "/build/profiles/" + profile + "-" + plan["build_key"]
            or digest(unsigned) != plan["plan_sha256"]):
        raise WorkspaceError("retained advance build plan differs from declared producer coordinates")
    states = plan["checkout_states"]
    if (not isinstance(states, dict) or not isinstance(states.get("classic"), dict)
            or states["classic"].get("path") != classic_root
            or states["classic"].get("head") != classic_head
            or any(not isinstance(state, dict) or state.get("dirty") is not False for state in states.values())):
        raise WorkspaceError("retained advance build plan has unverified Classic/source heads")
    if content_input is not None:
        validate_content_input(content_input)
        content = states.get("content", {})
        if (plan.get("retained_content_input") != content_input["new_head"]
                or content.get("path") != content_input["root"]
                or content.get("head") != content_input["new_head"]
                or plan["sources"].get("content") != content_input["root"]
                or plan["execution_sources"].get("content") != content_input["root"]
                or not plan["source_fingerprints"].get("content")
                or plan["git_observations"].get("content", {}).get("clean") is not True):
            raise WorkspaceError("retained advance content plan lacks declared read-only source provenance")
    for field in ("manifest", "source_fingerprints", "git_observations", "sources", "execution_sources"):
        if not isinstance(plan[field], dict) or not plan[field]:
            raise WorkspaceError("retained advance build plan lacks " + field)
    if (not isinstance(plan["targets"], list) or "server" not in plan["targets"]
            or set(plan["sources"]) != set(plan["execution_sources"])):
        raise WorkspaceError("retained advance build roles differ")
    return plan


def validate_content_input(value: Any) -> dict:
    """One immutable content dependency; historical source coordinates stay fixed."""
    keys = {"repository", "repository_node_id", "branch", "root", "old_head",
            "new_head", "tree", "pull_request", "pull_request_node_id", "merge_commit"}
    if isinstance(value, dict) and "build_slot" in value:
        keys.add("build_slot")
        if not isinstance(value["build_slot"], str) or re.fullmatch(r"[a-z0-9][a-z0-9._-]{0,127}", value["build_slot"]) is None:
            raise WorkspaceError("retained content successor build slot is invalid")
    exact(value, keys, "content input")
    if value["repository"] != "atrinik/content" or value["branch"] != "main":
        raise WorkspaceError("retained advance content repository/branch differs")
    for field in ("old_head", "new_head", "tree", "merge_commit"):
        hex_value(value[field], HEX40, "content " + field)
    for field in ("repository_node_id", "pull_request_node_id"):
        if not isinstance(value[field], str) or re.fullmatch(r"[A-Za-z0-9_=-]{2,256}", value[field]) is None:
            raise WorkspaceError("retained advance content provenance identity is invalid")
    if (not isinstance(value["pull_request"], int) or isinstance(value["pull_request"], bool)
            or value["pull_request"] <= 0 or value["old_head"] == value["new_head"]):
        raise WorkspaceError("retained advance content transition lacks merged provenance")
    root = value["root"]
    if (not isinstance(root, str) or not root.startswith("/")
            or any(part in {"", ".", ".."} for part in root.split("/")[1:])
            or any(ord(char) < 32 or ord(char) == 127 for char in root)):
        raise WorkspaceError("retained advance content root is not canonical")
    return value


def stages(value: dict) -> tuple[str, ...]:
    return (("declare", "plan", "built", "content-plan", "content-built", "topology")
            if "build_slot" in value.get("content_input", {}) else STAGES)


def build_slot(value: dict) -> str:
    return value.get("content_input", {}).get("build_slot", value["declaration"]["build_slot"])


def validate_envelope(value: Any) -> dict:
    """Validate append-only retained records, never infer live authority from them."""
    keys = {"schema_version", "correction_sha256", "declaration", "steps"}
    if isinstance(value, dict) and "content_input" in value:
        keys.add("content_input")
    value = exact(value, keys, "envelope")
    if "content_input" in value:
        validate_content_input(value["content_input"])
    if value["schema_version"] != 1 or isinstance(value["schema_version"], bool):
        raise WorkspaceError("retained advance envelope version is unsupported")
    hex_value(value["correction_sha256"], HEX64, "correction anchor")
    declaration = exact(value["declaration"], {
        "accepted_wrapper_commit", "producer_blob_sha256", "portable_pins", "classic_root",
        "old_classic_head", "new_classic_head", "profile", "scenario", "topology", "build_slot",
    }, "declaration")
    for field in ("accepted_wrapper_commit", "old_classic_head", "new_classic_head"):
        hex_value(declaration[field], HEX40, field)
    hex_value(declaration["producer_blob_sha256"], HEX64, "producer blob")
    pins = validate_pins(declaration["portable_pins"])
    if (declaration["new_classic_head"] != pins["CONSUMER_COMMIT"]
            or (declaration["new_classic_head"] == declaration["old_classic_head"] and "content_input" not in value)):
        raise WorkspaceError("retained advance source transition differs from accepted declaration")
    root = declaration["classic_root"]
    if not isinstance(root, str) or not root.startswith("/") or any(part in {"", ".", ".."} for part in root.split("/")[1:]):
        raise WorkspaceError("retained advance Classic root is not canonical")
    for field in ("profile", "scenario", "topology", "build_slot"):
        if not isinstance(declaration[field], str) or re.fullmatch(r"[a-z0-9][a-z0-9._-]{0,127}", declaration[field]) is None:
            raise WorkspaceError("retained advance declaration name is invalid")
    steps = value["steps"]
    if not isinstance(steps, list) or not 1 <= len(steps) <= len(stages(value)):
        raise WorkspaceError("retained advance steps exceed the bounded lifecycle")
    previous_generation = 0
    for index, step in enumerate(steps):
        exact(step, {"stage", "generation", "predecessor_sha256", "request", "observations", "previous_topology_current"}, "step")
        if (step["stage"] != stages(value)[index] or isinstance(step["generation"], bool)
                or not isinstance(step["generation"], int) or step["generation"] <= previous_generation):
            raise WorkspaceError("retained advance steps are not an ordered prefix")
        if (step["stage"] == "topology") != isinstance(step["previous_topology_current"], dict):
            raise WorkspaceError("retained advance previous topology observation differs from stage")
        if step["stage"] != "topology" and step["previous_topology_current"] is not None:
            raise WorkspaceError("retained advance has unrelated previous topology evidence")
        previous_generation = step["generation"]
        hex_value(step["predecessor_sha256"], HEX64, "step predecessor")
        if not isinstance(step["request"], dict) or not isinstance(step["observations"], dict):
            raise WorkspaceError("retained advance step evidence is invalid")
    return value


def require_append(before: dict | None, after: dict) -> None:
    validate_envelope(after)
    if before is None:
        if len(after["steps"]) != 1:
            raise WorkspaceError("retained advance must begin with a declaration")
        return
    validate_envelope(before)
    # An already retained declaration may add content only with its first plan.
    # The original declaration and every prior step remain byte-for-byte intact.
    added_content = "content_input" not in before and "content_input" in after
    if added_content and (len(before["steps"]), after["steps"][-1]["stage"]) not in {(1, "plan"), (3, "content-plan")}:
        raise WorkspaceError("retained advance content intent must precede build production")
    compared_after = {key: val for key, val in after.items() if not (added_content and key == "content_input")}
    if ({key: val for key, val in before.items() if key != "steps"}
            != {key: val for key, val in compared_after.items() if key != "steps"}
            or len(after["steps"]) != len(before["steps"]) + 1
            or after["steps"][:-1] != before["steps"]):
        raise WorkspaceError("retained advance historical declaration/evidence is immutable")
