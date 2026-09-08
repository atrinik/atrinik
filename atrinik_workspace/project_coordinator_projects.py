"""Exact existing Projects-v2 Status updates; never create fields or items."""
from __future__ import annotations

from .project_coordinator import require

MUTATION = """mutation($project:ID!, $item:ID!, $field:ID!, $option:String!) {
  updateProjectV2ItemFieldValue(input:{projectId:$project,itemId:$item,fieldId:$field,
    value:{singleSelectOptionId:$option}}) { projectV2Item { id } }
}"""
QUERY = """query($project:ID!, $item:ID!, $field:ID!) {
  project:node(id:$project) { ... on ProjectV2 { id title owner {
    ... on Organization { login } ... on User { login }
  } } }
  item:node(id:$item) { ... on ProjectV2Item { id project { id } content {
    ... on Issue { id } } fieldValueByName(name:"Status") {
      ... on ProjectV2ItemFieldSingleSelectValue { optionId field {
        ... on ProjectV2SingleSelectField { id } } }
    } } }
  field:node(id:$field) { ... on ProjectV2SingleSelectField { id name
    project { id } options { id name } } }
}"""


def validate(payload: dict) -> dict:
    require(set(payload) == {"project", "item", "field", "option", "status"},
            "exact existing Project/issue item/Status field/option required")
    require(all(isinstance(v, str) and 0 < len(v) <= 256 for v in payload.values()),
            "invalid Project coordinate")
    require(payload["status"] in {"In progress", "Blocked", "Review", "Done"}, "unsupported project status")
    return {"query": MUTATION, "variables": {k: payload[k] for k in ("project", "item", "field", "option")}}


def observe(operation: dict, github) -> dict:
    payload = operation["payload"]
    validate(payload)
    response = github.request("graphql", "POST", {"query": QUERY,
        "variables": {k: payload[k] for k in ("project", "item", "field")}})
    require(not response.get("errors"), "incomplete Project proof")
    data = response["data"]
    project, item, field = data["project"], data["item"], data["field"]
    issue = github.issue(operation["target"])
    require(project and item and field and project["id"] == payload["project"]
            and project["owner"]["login"] == "atrinik" and project["title"] == "Atrinik work",
            "foreign Project")
    require(item["id"] == payload["item"] and item["project"]["id"] == project["id"]
            and item["content"] and item["content"]["id"] == issue["node_id"], "foreign Project issue item")
    require(field["id"] == payload["field"] and field["name"] == "Status"
            and field["project"]["id"] == project["id"], "foreign Project field")
    require(sum(o["id"] == payload["option"] and o["name"] == payload["status"]
                for o in field["options"]) == 1, "Project option drift")
    current = item["fieldValueByName"]
    require(current is None or current["field"]["id"] == field["id"], "ambiguous Status field")
    value = current["optionId"] if current else None
    return {"identity": issue["node_id"], "current": value,
            "match": item["id"] if value == payload["option"] else None}


def gate(project: dict, operation: dict, github) -> None:
    """Done reflects a live terminal issue, never just a green or ready PR."""
    if operation["payload"]["status"] == "Done":
        require(github.observe(operation["target"], "issue")["terminal"], "Done requires live terminal issue")
