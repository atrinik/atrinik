# MCP information-access safety and measurement contract

## Status and scope

This document defines the common contract for Atrinik-owned MCP servers and
evaluated external connectors. The versioned machine source is
[`mcp/contract/v1`](../mcp/contract/v1/README.md). This phase ships schemas,
known answers, enforcement helpers, adversarial gates, a decision matrix, and a
benchmark harness. It does not ship a production server, enable a connector, or
add project-scoped Codex configuration.

The protocol target is MCP `2026-07-28`, pinned to specification commit
`5f5440bb26a62e2cf3440b92da5a667efa03b267`. Tool inputs and outputs use JSON
Schema 2020-12. Inputs remain closed objects; structured results are validated
against an output schema and return compact records and resource links before
optional exact content. MCP Roots are deprecated and informational, so they are
never an access-control boundary. Local Atrinik servers default to stdio and an
explicit configured root.

The direct `./atrinik`, repository CLI, `rg`, Git, `gh`, and browser workflows
remain authoritative and available offline. MCP is a bounded query layer, not
a second workspace controller, parser, runtime operator, or source of truth.

## Required identity and result shape

Every request selects manifest or wrapper-registry identities rather than a
caller path. Every result identifies:

- repository, branch, full 40-character commit, stable worktree/profile
  identity, and an optional 64-character dirty fingerprint;
- component, generation, owner, license, schema/provider version, and observed
  freshness where the result type needs them;
- deterministic pagination bound to the complete snapshot identity;
- explicit truncation and incomplete state with bounded per-record failures;
  and
- stable resource identities and `atrinik://` links for detail read only on
  demand.

Opaque cursors carry an integrity check, snapshot fingerprint, version, and
offset. A changed repository, commit, branch, worktree, dirty fingerprint,
authorization identity, manifest/profile/registry identity, schema, provider,
or effective parameter makes the cursor or cache entry unusable. Clean
immutable entries may be reused; dirty or mutable observations have zero TTL.
Initial custom servers use at most 128 in-memory entries / 8 MiB and no
persistent cross-worktree index.

## Hard bounds and context budgets

The contract starts conservatively; a consumer may lower but not raise a value
without a new measured contract revision.

| Surface | v1 maximum |
| --- | ---: |
| Request | 16 KiB |
| Query text | 1,024 characters |
| Records per result | 100 |
| Records per page | 50 |
| Structured result | 64 KiB hard, 32 KiB routine |
| On-demand resource read | 256 KiB |
| Graph | depth 8 / 1,000 edges |
| Schema evaluation depth | 32 |
| Request time | 5,000 ms |
| Combined visible tool catalog | 12 tools / 32 KiB schema |
| Server instructions | 2 KiB |

`enforce_context_budget()` checks all four MCP context ceilings. Contract
validation also invokes the existing guidance inventory, preserving its
separate current `main` guards instead of copying historical #294 constants.
No server activation may attach unrelated guides, schemas, repositories,
resources, or prior results automatically. Codex project configuration belongs
to the final pilot, remains optional and secret-free, and is accepted only for
trusted checkouts. Server instructions keep their first 512 characters
self-contained and stay within the stricter Atrinik 2 KiB ceiling.

## Security and no-mutation boundary

Source, docs, issue/comment content, logs, authored content, tool descriptions,
annotations, and client prompts are untrusted data. They retain source identity
and are never reclassified as instructions. Tool annotations, UI confirmation,
MCP Roots, client prompts, and cache scope are hints rather than authorization.

The default allowlist is `compare`, `inspect`, `list`, `read`, `search`, and
`validate`. The common guard rejects mutation operations, credentials, mutable
state, private player data, arbitrary/absolute/traversing paths, `.git`,
generated workspace/build state, secrets, oversized input, expired deadlines,
and cancellation. Configured-root reads walk descriptor-relative directories,
do not follow links, open nonblocking, accept regular files only, cap bytes,
and compare descriptor identity before and after the read.

The adversarial corpus and `tests.test_mcp_contract` cover:

| Threat | Required evidence |
| --- | --- |
| Traversal, symlink escape, FIFO/device, ignored/generated state | Selector guard and no-follow bounded-read tests |
| Secret-bearing errors and prompt-injected data | Value redaction and untrusted-data classification tests |
| Stale coordinates/cursors and TOCTOU | Full identity, cursor snapshot, and descriptor identity tests |
| Cross-repository/branch/worktree/authorization reuse | Required cache-field and unique-key tests |
| Oversized queries/graphs/results and context | Limit, pagination, and context-ceiling tests |
| Malicious descriptions and dependency replacement | Fixed authored catalog and immutable SDK/spec decision checks |
| External outage and malformed history | Offline fallback plus bounded incomplete-record fixtures |
| Unsupported writes, timeout, cancellation | Request guard and no-mutation allowlist tests |

Errors never echo caller values. Stable codes distinguish invalid arguments,
authorization, forbidden data, limits, stale identity/cursor, incomplete data,
offline operation, timeout, cancellation, unsupported operation, and internal
failure. One malformed historical record yields one bounded incomplete failure;
it does not trigger a broad rescan or suppress healthy records.

## Known-answer corpus

The six versioned cases resolve:

1. Classic `Packet` ownership through the one physical `atrinik/classic`
   checkout, its logical module, nearest guidance, unit test, and wrapper build;
2. the replacement metaserver directory contract across Protobuf/generated Go,
   Go server, and Rust client owners without a Classic fallback;
3. the Astro downloads page, its data/assets, and website-owned checks;
4. Clearhaven content relationships spanning map, archetype, quest,
   dialogue/lore, asset, and provenance on `atrinik/content@main`, plus a
   Classic artifact identifying the identical full content commit;
5. issue hierarchy, Project state, related PR/review/check state as a read-only
   GitHub observation; and
6. exact profile, checkout/worktree, topology, state, scenario, services, and
   bounded runtime view through wrapper identities.

Each case repeats under HEAD, dirty edit, worktree, manifest, authorization,
malformed-history, and offline changes. The synthetic registry has 300 records,
exceeding the observed 275-entry boundary, and tests prove complete,
duplicate-free deterministic pagination.

## Capability and dependency decision

The machine matrix records a physical owner, transport, authorization, data
classification, bounds, fallback, supply-chain impact, and decision for every
candidate. In summary:

- build the wrapper context/search surfaces, content-toolkit adapter, and
  separately enabled runtime-status surface behind their issue dependencies;
- defer runtime logs until redaction is proved;
- defer GitHub, browser, and Cloudflare profiles to their measured
  least-privilege evaluation, rejecting Cloudflare if mutation cannot be
  excluded; and
- reject generic filesystem, shell, memory, vector, eager all-worktree, and
  hosted source-upload servers.

The maintained Python SDK evaluation is
`modelcontextprotocol/python-sdk@v2.0.0`, immutable commit
`6f69a3758ebf2ee55ce050f58b470ce11af71133`, MIT. No SDK dependency is added in
this contract-only phase. Issue #351 owns the production API evaluation and,
if adopted, the immutable dependency and transitive
`supply-chain/inventory.json` update. Dependabot and that package owner review
protocol support, changelog, license, and transitive changes.

## Reproducible measurement

Run the benchmark from a clean final-head worktree. The harness validates the
contract first, runs one cold and repeated warm known-answer passes, proves
HEAD/dirty/authorization/schema/provider invalidation, and records correctness,
stable source identity, visible tool/schema bytes, calls/retries,
records/bytes/token estimate, resource reads, p50/p95 wall time, cache
hits/misses, external network use, failure, and fallback behavior.

```sh
python3 -m atrinik_workspace.mcp_contract benchmark \
  --iterations 30 --output build/mcp/benchmark.json
```

The default path is offline. A read-only live GitHub comparison is explicit:

```sh
python3 -m atrinik_workspace.mcp_contract benchmark \
  --iterations 30 --live-github --output build/mcp/benchmark-live.json
```

Both modes remeasure the current wrapper, `rg`, and Git command paths; live mode
also measures `gh`. Output contains only command IDs, counts, return codes,
sizes, durations, booleans, and Git coordinates. It records no raw command
output, credential, authorization identity, private workspace data, or host
path, and remains in ignored `build/` state rather than being uploaded.

## Consumer gate

Issues #351, #352, #354, #355, content-toolkit#20, and the final #353 pilot must
consume the v1 fixtures, schemas, limits, error/cursor/cache identity, context
gate, and benchmark fields. A consumer may extend its domain result under a new
schema but may not weaken these invariants or silently route an unavailable
replacement capability through Classic. Any production dependency,
configuration, external authorization, runtime observation, or content parser
remains owned by its physical repository and issue.

Validate this contract with:

```sh
python3 -m atrinik_workspace.mcp_contract validate
python3 -m unittest -v tests.test_mcp_contract
```
