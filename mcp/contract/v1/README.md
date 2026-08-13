# Atrinik MCP information-access contract v1

This directory is the versioned, wrapper-owned contract consumed by Atrinik MCP
implementations and connector evaluations. It is not a production MCP server or
project configuration.

- `contract.json` pins MCP `2026-07-28`, JSON Schema 2020-12, stable failures,
  hard request/result/resource/time/graph limits, cache identity, and distinct
  catalog/startup/routine-result ceilings.
- `schemas/` defines closed tool-input and structured-result schemas with exact
  coordinates, freshness, pagination, truncation, incomplete results, resource
  identity, and per-record failures.
- `fixtures/workloads.json` supplies six known-answer domains and 300 synthetic
  worktree records. The content fixture binds its Classic-derived artifact to
  the same full `atrinik/content@main` commit.
- `fixtures/adversarial.json` names every required threat and stable failure.
- `capabilities.json` is the reviewed build/configure/defer/reject and
  dependency decision matrix.

Validate the machine contracts and focused tests from the wrapper root:

```sh
python3 -m atrinik_workspace.mcp_contract validate
python3 -m unittest -v tests.test_mcp_contract
```

Generate sanitized local benchmark evidence in ignored state. The default is
offline; `--live-github` adds one read-only current-path measurement and must be
run with an already authenticated `gh` under the applicable GitHub workflow.

```sh
python3 -m atrinik_workspace.mcp_contract benchmark \
  --iterations 30 --output build/mcp/benchmark.json
python3 -m atrinik_workspace.mcp_contract benchmark \
  --iterations 30 --live-github --output build/mcp/benchmark-live.json
```

Consumers must reuse these files and the enforcement helpers in
`atrinik_workspace.mcp_contract`; they may narrow bounds but must not invent a
weaker coordinate, cursor, cache, error, or data-access policy.
