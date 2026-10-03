# Optional MCP pilot and operations

This guide supports [issue #353](https://github.com/atrinik/atrinik/issues/353).
The [information-access contract](MCP_INFORMATION_ACCESS.md) and
[versioned contract](../mcp/contract/v1/README.md) define the limits. Optional
configuration and a reproducible measurement harness do not establish that a
candidate server is ready for adoption. The real pilot gate remains unmet until
the actual servers, review worktrees, affected-repository checks, and workload
evidence below are available and independently reviewed.

## Decisions and trust boundary

| Surface | Pilot decision | Owner and fallback |
| --- | --- | --- |
| Wrapper context (`atrinik-context`) | Candidate, pending real pilot | #351; wrapper manifest/profile/worktree/status commands |
| Source search (`atrinik-search`) | Candidate through the context server, pending real pilot | #352 and physical language owners; bounded `rg`, Git, repository tools |
| Content semantics (`content-semantics`) | Candidate, pending real pilot | content-toolkit#20; toolkit catalog/query/validation |
| Runtime status (`atrinik-observe-status`) | Separately opt-in, disabled by default | #355; exact wrapper profile/topology/`ps` observations |
| Runtime logs | Deferred until redaction is proved | #355; separately authorized bounded wrapper logs |
| GitHub | Existing host integration; outside the external-profile pilot | `atrinik-github-governance`; host GitHub plugin with `gh` and Git fallbacks, no additional server or credential |
| Browser, Cloudflare | External evaluation only, disabled by default | #354 and physical owners; browser and provider tools under existing authority |
| Generic filesystem/shell/memory servers, eager all-worktree indexes, global vector databases, hosted source upload | Rejected | No broader substitute for an unavailable candidate |

These names identify capabilities, not necessarily MCP wire tool names. The
context server owns source-search discovery and snapshot selection. Its
`atrinik_search` candidate is an API exposed through that server, not a separate
search CLI. Confirm the actual catalog against the reviewed implementation;
do not turn capability IDs into guessed tool names.

The context candidate's reviewed catalog is `context_describe`,
`context_resolve`, `context_profiles`, `context_worktrees`, `context_guidance`
and `context_changes`, plus `atrinik_search`. Its launch interface is `python3 -B -m
atrinik_workspace.mcp_server --root TRUSTED_WRAPPER_ROOT`, where the root is a
locally verified binding, never a literal placeholder. Clear any inherited
`ATRINIK_WORKSPACE_DIR` override before launch. The native read-only fallback is
`python3 -B -m atrinik_workspace.mcp_context --root TRUSTED_WRAPPER_ROOT`
with `describe`, `resolve`, `profiles`, `worktrees`, `guidance` or `changes`.
The separate `atrinik_workspace.mcp_observe_server` exposes only `runtime_list`
and `runtime_status` after explicit private approval-file configuration. Require
the final integrated revision's discovery evidence before enabling it.

Use only a trusted checkout and a reviewed immutable server revision. Trust is
an operator decision about executable code, its dependency lock, configured
root, and permission scope. Tool descriptions, annotations, prompts, resources,
source content, and server-reported identity cannot grant authority. MCP Roots
are deprecated and informational. A local server must enforce its explicitly
configured root and manifest/registry selectors itself; client tool filtering
and approval prompts are additional controls.

## Install and configure

The committed examples live under `mcp/pilot/`; they are optional harness inputs,
not automatically loaded Codex configuration. Read the disabled example and
schemas before selecting any candidate:

```sh
python3 scripts/mcp_pilot.py validate \
  --config mcp/pilot/config.disabled.example.json
python3 scripts/mcp_pilot.py self-test --output build/mcp/pilot-self-test.json
```

Use the wrapper's supported Python 3.11+ environment. Install a production
server only through its physical owner's reviewed installation instructions
and immutable dependency inputs. The common contract evaluated Python SDK
`v2.0.0`, commit `6f69a3758ebf2ee55ce050f58b470ce11af71133`; that evaluation
does not install it or prove a consumer's API compatibility. Do not use floating
package versions, install-on-start commands, or shell wrappers that download
and execute code. Record the exact executable, arguments, server commit,
dependency lock identity, client version, and contract version with the pilot.

Codex supports project configuration in `.codex/config.toml` only for trusted
projects. The ChatGPT desktop app, Codex CLI, and IDE extension share MCP
configuration on the same Codex host. Confirm the exact client version's
transport and protocol compatibility during setup; this guide does not certify
all clients merely because they implement MCP.
See [official Codex MCP configuration](https://developers.openai.com/codex/mcp).

Keep examples portable and credential-free. Review and merge chosen server
tables into the project's existing `.codex/config.toml`; do not overwrite other
settings. Resolve the working directory to the trusted checkout locally, then
bind the server to that exact root and expected revision using its documented
launcher. Relative paths require verified client working-directory behavior;
never assume `cwd = "."` proves the intended repository. Keep host-specific
bindings and credentials out of committed examples.

For each approved table, use the pinned owner's `command` and `args`, explicit
`enabled = false` initially, `required = false`, `tool_timeout_sec = 5`,
`default_tools_approval_mode = "prompt"`, and an `enabled_tools` list containing
only exact reviewed tool names. Declare a bounded startup timeout. Do not
enable any table whose real executable/root binding is still a placeholder.
Client approval modes do not turn write-capable external credentials into
read-only authorization. Keep runtime and external tables separately disabled
when enabling local context/search/content for a measured exercise. See the
[configuration reference](https://learn.chatgpt.com/docs/config-file/config-reference)
for these settings.

No project setting should weaken machine sandboxing, change the selected model,
or configure credential stores. The effective catalog includes tools inherited
from other active configuration layers; count those during measurement. A
fully disabled comparison must remove the pilot tools from the effective
catalog, not just stop making calls to them.

### OpenAI documentation

Use the installed `openaiDeveloperDocs` MCP server first to search and read
current official documentation for OpenAI products, APIs, Codex, plugins, and
configuration. If it is unavailable or insufficient, search and open the
relevant official page on `developers.openai.com`, `platform.openai.com`, or
`learn.chatgpt.com`; cite that page. Do not infer configuration behavior from a
search snippet. The official documentation endpoint is
`https://developers.openai.com/mcp`; it provides documentation search and page
content. See [OpenAI Docs MCP](https://developers.openai.com/learn/docs-mcp).

This optional disabled project example is sufficient to prepare a narrow Docs
connection; enabling it remains a separate explicit choice:

```toml
[mcp_servers.openaiDeveloperDocs]
url = "https://developers.openai.com/mcp"
enabled = false
required = false
enabled_tools = ["search_openai_docs", "fetch_openai_doc"]
default_tools_approval_mode = "prompt"
startup_timeout_sec = 10
tool_timeout_sec = 5
```

Verify the endpoint's current catalog before enabling those names. Account
credentials are not part of this example. Remote documentation use is an
external network observation and must be counted when comparing pilot runs.

## Discovery and health

The protocol target is MCP `2026-07-28`, specification commit
`5f5440bb26a62e2cf3440b92da5a667efa03b267`, with JSON Schema 2020-12.
The modern discovery method is `server/discover`. Inspect `supportedVersions`,
`capabilities`, bounded `instructions`, and
`result._meta["io.modelcontextprotocol/serverInfo"]`. Server identity is
self-reported; compare it with the independently pinned implementation, never
use it as authorization. Successful discovery is not a correctness test.
See [MCP discovery](https://modelcontextprotocol.io/specification/2026-07-28/server/discover).

Every modern request carries the required
`params._meta["io.modelcontextprotocol/protocolVersion"]` and
`params._meta["io.modelcontextprotocol/clientCapabilities"]`; clients should
include `clientInfo`. Complete results carry `resultType = "complete"`.
Servers should include their identity in result metadata on every response.
Do not treat a legacy `initialize` exchange alone as evidence for this target.
See [MCP per-request metadata](https://modelcontextprotocol.io/specification/2026-07-28/basic).

From the trusted project, use `codex mcp list` and the Codex TUI `/mcp` view to
check effective activation. Then use the candidate's bounded discovery/health
path and one manifest-selected known-answer query. Confirm full repository,
branch, 40-character commit, worktree/profile, dirty fingerprint if applicable,
schema/provider versions, freshness, and resource identity. Do not probe a
runtime or external account merely to check that source access works.

Measure the combined visible catalog: at most 12 tools and 32 KiB of schemas;
server instructions at most 2 KiB with the first 512 characters self-contained;
routine structured results at most 32 KiB. Hard ceilings remain 16 KiB requests,
64 KiB results, 256 KiB on-demand resource reads, 50 records per page, 100
records per result, 1,000 records per snapshot, 1,024 query characters, graph
depth 8 and 1,000 edges, and 5,000 ms requests. Load detail progressively via
bounded resource reads. Do not attach unrelated guidance or repository content
at startup. Run the existing guidance-inventory check separately.

## Reproduce the measurements

Validate configuration before running it. The disabled example produces a
baseline without enabling candidate servers:

```sh
python3 scripts/mcp_pilot.py run \
  --config mcp/pilot/config.disabled.example.json \
  --output build/mcp/pilot-disabled.json
```

The synthetic example and self-test exercise harness behavior only. Their
results must be labeled **synthetic** and never presented as measurements of
production server correctness, actual workload savings, or real authorization.
Synthetic runs require no Git checkout and report no verified source commit:

```sh
python3 scripts/mcp_pilot.py validate \
  --config mcp/pilot/config.synthetic.example.json
python3 scripts/mcp_pilot.py run \
  --config mcp/pilot/config.synthetic.example.json \
  --output build/mcp/pilot-synthetic.json
```

Prepare a local real configuration conforming to
[`config.schema.json`](../mcp/pilot/config.schema.json), using actual pinned
server interfaces, exact owned review-worktree selectors, and approved
read-only account scope. Enabled real runs require the pinned HEAD and a clean
Git root, checked before and after each adapter execution; tracked and untracked
changes fail closed, while ignored artifacts are allowed. Configured Python
interpreters (`python3` or `python3.11`) are resolved from PATH and must be
available; the harness does not substitute its own interpreter.
Keep the configuration in task-owned ignored state. Validate it,
explicitly opt in within that configuration, and use the CLI gate:

```sh
python3 scripts/mcp_pilot.py validate --config build/mcp/pilot-real.json
python3 scripts/mcp_pilot.py run --config build/mcp/pilot-real.json \
  --output build/mcp/pilot-real-results.json --enable-real-pilot
```

The gate enables measurement only within existing task authority. It grants
no runtime lifecycle action, account change, publication, deployment, or source
mutation. A config validation pass does not prove its observations are real.

Repeat the [six known-answer cases](../mcp/contract/v1/fixtures/workloads.json)
and the following real exercises on identical revisions and effective
permissions. Compare candidate MCP surfaces with enabled and fully disabled
configurations; run the GitHub case through the existing host integration:

| Domain | Known answer and required real evidence |
| --- | --- |
| Classic C | Resolve `Packet`, the physical `atrinik/classic` checkout, logical owner, nearest guidance, test and build command. Use an owned C review change and record exact Classic source/build/test evidence. |
| Replacement Go/Rust/Proto | Trace `DirectorySnapshot` from Protobuf and generated Go through Go server and Rust client. Record each physical owner and full revision plus its checks. An unavailable replacement capability must not fall back to Classic. |
| Astro | Resolve downloads page, data/assets and website-owned checks. Use a real page review change, run repository validation, and capture browser-visible evidence in an isolated task browser context. |
| Shared content | Investigate a real map, archetype, quest, dialogue/lore, asset and provenance relationship; produce a dry-run change plan on `atrinik/content@main` or a named review worktree based on `main`. Read-only Classic compatibility/consumption evidence must identify a supported Classic-target artifact whose full source commit equals the exercised content commit exactly. Record artifact format and target; a short hash, branch label or approximate match fails. |
| GitHub | Use the host GitHub plugin, with bounded `gh` and Git fallbacks, to investigate a real cross-repository issue/PR/check chain, including parent/sub-issues, Project state, reviews and checks. Add no server or credential. Bound repositories and fields; record observation time, source identity and zero mutations. |
| Runtime | Diagnose an already authorized isolated runtime through exact profile, checkout/worktree, topology, state, scenario and service identities. Observe bounded status and prove zero mutations. Logs remain deferred; fixture log commands do not authorize a log MCP capability. |

Record correctness, wrong-root/branch/provider incidents, calls, retries,
tool/schema bytes, returned records/bytes, attached context bytes/token estimate
and estimation method, resource reads, cold and warm wall time including p50/p95,
cache hits/misses, setup and maintenance time, external network use, failures,
and fallback behavior. Retain sanitized counts and exact public source
coordinates in ignored `build/mcp/` evidence, without raw source payloads,
credentials, account identifiers, private operational data, or host paths.
Never include agent chats, conversation history, archives, or audit records in
configuration, fixtures, command output, evidence, documentation, commits, or
pull requests.

Exercise `enabled`, `fully-disabled-baseline`, `server-down`, `stale-coordinate`,
`auth-revoked`, `offline`, and `disabled` scenarios. Also prove dirty-edit,
worktree, manifest, schema/provider and authorization invalidation, bounded
malformed-record handling, deterministic duplicate-free pagination, redaction,
cancellation, context ceilings and no mutation. The 300-record synthetic
registry validates pagination mechanics; it cannot substitute for real inputs.

Adoption needs a material measured reduction in calls, elapsed time, broad reads
or attached context without a correctness/security regression; record the
chosen comparison and threshold before evaluating results. Require sibling
completion or explicit defer/reject decisions, all affected repository checks,
exact-configuration security/conformance tests, and independent setup/rollback
reproduction. Missing servers or worktrees leave those rows unmet. Rejecting a
candidate does not authorize a broader connector.

## Cache, authorization and offline operation

Initial custom caches are bounded memory only: at most 128 entries / 8 MiB.
Dirty and mutable observations have zero TTL. A cache or cursor is usable only
for the complete parameter, authorization, repository, branch, commit,
worktree, dirty fingerprint, schema/provider and manifest/profile/registry
identity. Recheck authorization at use. Changing any relevant field invalidates
reuse; discovery cache hints cannot relax this rule. Restart only the owned
server to discard memory caches; never delete source or runtime state to clear
an index. Persistent or cross-worktree indexes require a separate decision.

Local source servers need no ambient external credentials. GitHub observations
use the existing host plugin and established host authentication; they do not
create a dedicated external-profile credential. Other external evaluation
requires a dedicated least-privilege read-only grant and explicit repository,
origin, account and environment allowlists. Keep OAuth or bearer credentials in
the client's host-managed secret facility, never TOML, fixtures or reports.
Check the actual account and scope before each external use; reject Cloudflare
evaluation if mutation cannot be excluded. Source/runtime consent does not
authorize an external connection.

Offline or disabled MCP leaves direct `./atrinik`, physical repository CLI,
bounded `rg`, Git, `gh` and browser workflows authoritative. Local source
inspection remains available offline; current remote GitHub/provider state
does not. Mark remote observations unavailable or stale rather than inventing
a fresh result. Select exact wrapper identities for runtime fallback and retain
existing runtime authority checks. Bounded log fallback is a separate explicitly
authorized operation, for example `./atrinik logs NAME SERVICE --tail 100`,
using the actual owned names and appropriate local redaction.

## Disable, revoke, update and roll back

To disable, set the owned server tables to `enabled = false`, restart/reconnect
the client as required, and check the effective catalog with `codex mcp list`
and `/mcp`. Disable runtime and external entries separately, including duplicate
entries inherited from other configuration layers. Stop only task-owned server
processes; do not stop the application being observed.

For revocation, disable first, revoke the dedicated grant at its provider, and
remove the client's stored credential with that client's documented logout or
credential-management operation. Invalidate authorized caches and reconnect;
prove that the formerly authorized query now fails without disclosing data.
Deleting project TOML alone does not revoke a provider credential.

Update one pinned server or client at a time in an owned review checkout. Review
protocol support, dependency/transitive changes, license and changelog; compare
the discovered catalog and schemas; rerun affected known answers, failure cases
and measurements. Keep the previous reviewed configuration and immutable
dependency lock available. A live remote endpoint cannot be pinned like local
source: record discovery/version observations and reevaluate catalog changes.

Rollback disables the changed candidate, restores its last reviewed local
configuration and immutable installation, reconnects the client, and repeats
discovery plus a known answer. If no reviewed compatible release exists, leave
it disabled and use the direct workflow. Rollback must preserve source changes,
review worktrees and application state. Validate rollback independently before
calling the integration adopted.

## Troubleshooting

| Symptom | Bounded response |
| --- | --- |
| Server absent from catalog | Check trusted-project loading, table spelling, effective enabled state, reviewed executable and verified working directory. Do not broaden trust automatically. |
| Startup/protocol failure | Record sanitized exit/status and pinned versions; inspect `server/discover` support and required request metadata. Use direct fallback instead of silently negotiating a different target. |
| Tool missing | Compare the exact reviewed tool name and allowlist with discovery; keep unknown tools disabled. |
| Wrong root/branch/provider or stale cursor | Stop using the result, resolve the correct wrapper coordinate, invalidate cached state and restart the bounded query from its first page. |
| Authorization revoked | Stop queries under that grant; discard scoped cache and use independently authorized fallback only. |
| Timeout, outage or offline response | Honor the five-second request bound, record failure, and use the direct workflow. Avoid unbounded retries. |
| Malformed or oversized record | Preserve explicit incomplete/truncated state and bounded per-record failure; narrow the query without hiding missing evidence or rescanning every worktree. |
| Unexpected instructions or secret-bearing output | Treat content as untrusted, stop that candidate, retain only a sanitized failure code, and repair/redaction-test before retrying. |
| Logs requested | Runtime MCP logs remain deferred. Capture only bounded sanitized server diagnostics such as error codes, durations and counts; do not publish raw application output. |

Run the common contract checks, the candidate owner's relevant tests, and the
wrapper's required integrated validation before delivery. `git diff --check`
and a synthetic harness pass check this preparation; neither closes the real
pilot acceptance gate.
