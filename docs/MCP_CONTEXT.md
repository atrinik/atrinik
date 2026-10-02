# Read-only workspace context

The wrapper owns `atrinik_workspace.mcp_context`: one result-producing API used
by its JSON CLI and `atrinik_workspace.mcp_server` stdio binding. It reuses
`Manifest.from_value`, canonical profile validation, and the wrapper Git
worktree parser. It never constructs the operational `Workspace`: that
constructor creates leases and may backfill references. Inspection does not
invoke `Paths.ensure`, profile resolution that creates directories, or runtime
commands.

From an installed, trusted wrapper checkout on Linux, start the optional server
with an explicit absolute root:

```sh
python3 -B -m atrinik_workspace.mcp_server --root /absolute/trusted/atrinik
```

`-B` prevents Python import-cache writes. This release supports the configured
root's canonical workspace only; external `ATRINIK_WORKSPACE_DIR` layouts fail
closed. No command changes client configuration or enables the server. Removing
the optional client entry and terminating its owned stdio process disables it;
there is no database, persistent index or cache to migrate or clean up.

The direct CLI calls the identical result APIs:

```sh
python3 -B -m atrinik_workspace.mcp_context --root /absolute/trusted/atrinik describe --profile classic
python3 -B -m atrinik_workspace.mcp_context --root /absolute/trusted/atrinik resolve --profile classic --component classic-client
```

`./atrinik`, repository commands, Git and `rg` remain authoritative fallbacks.
Missing checkouts return a stable unavailable error; they are not initialized.

## Catalog and identity

Six fixed tools cover manifest descriptions and dependency closure, exact
coordinate resolution, profile/registration names, one checkout's registered
worktrees, effective guidance resources, and tracked changed paths. The
`context_profiles` tool defaults to profiles; its explicit `kind` selector can
list registered topology, state or scenario names without opening save data,
credentials, logs or process status. Profile listing supports a name prefix.
Runtime observation is a separately enabled sibling surface.

Tool inputs are closed JSON Schema objects. Structured results carry the
contract's coordinate schema, wrapper/provider schema versions, owner,
component, generation, license, profile, freshness and snapshot identity.
Aggregate results identify their observed wrapper coordinate; worktree records
carry their own recorded commits. Registry records are observations, not proof
that an unselected historical checkout remains healthy. An exact `resolve`
performs the live source checks. The five Classic modules share one physical
checkout identity. Missing replacement providers stay unavailable.

`ContextService(root, authorization_identity)` is configured by the trusted
host, never by a protocol request. Its `resolve(profile, component, role,
worktree)` returns a `Snapshot` with a physical checkout `root`, a contract
`Coordinate`, `identity`, and logical-source `metadata`. `assert_current()`
fences subsequent use; `read(path, max_bytes)` reads only tracked, permitted
checkout-relative source with descriptor-relative no-follow reads and fences
before and after the read. A worktree selector is an opaque ID from the
selected checkout's registry, never a caller-supplied absolute path.

Fingerprints include HEAD, branch, tracked dirty bytes, manifest, normalized
profile, registry, authorization scope, root inode, schema and provider. Dirty
observations have no reusable cache. Untracked contents are never read or
indexed. Main-based content review worktrees preserve a separate main base
commit; the primary content branch must be `main`.

The resource registry holds at most 128 snapshot/path references and no file
payloads. `atrinik://context/` links are issued only for selected coordinates
and guidance. Resource listing attaches nothing automatically. Evicted or stale
resources require a new lookup; a URI conveys no independent authority.

## Protocol and dependency decision

The transport implements the modern-only
[MCP 2026-07-28 binding](https://modelcontextprotocol.io/specification/2026-07-28/basic/versioning)
and its [stdio framing](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio).
Every request declares `io.modelcontextprotocol/protocolVersion` and
`io.modelcontextprotocol/clientCapabilities` in `params._meta`.
`server/discover` advertises supported versions and capabilities; successful
results carry `resultType: complete`. Legacy initialization clients are not
supported. Client-provided identities, capabilities and Roots grant no access.
The pinned [schema source](https://github.com/modelcontextprotocol/modelcontextprotocol/blob/5f5440bb26a62e2cf3440b92da5a667efa03b267/schema/2026-07-28/schema.ts)
is the protocol reference.

The maintained MIT Python SDK v2.0.0, commit
`6f69a3758ebf2ee55ce050f58b470ce11af71133`, was evaluated against its
[versioned API documentation](https://py.sdk.modelcontextprotocol.io/v2/)
and immutable package metadata. It supports modern and legacy routing. This
small Linux-only binding instead uses the Python standard library so its
pre-decode 16-KiB framing, four-request capacity, cancellation and fixed method
allowlist remain explicit without introducing the SDK's wider transport and
dependency surface. No SDK code is copied or vendored, and no production
package is added. The wrapper owns protocol maintenance and conformance tests;
re-evaluate the SDK if transport/client support expands.

Requests, scans, subprocess output, resources and results obey contract v1.
Malformed JSON, duplicate keys, oversized frames, unknown fields and methods
fail without reflecting caller values. Git commands are fixed internal reads,
use pinned directory descriptors, disable optional locks, replacement objects,
lazy fetching and external diff/textconv/fsmonitor, reject configured clean or
process filters, and are killed on cancellation or deadline. Git must support
`--no-lazy-fetch`; unsupported Git versions fail closed. EOF cancels
outstanding requests. No shell, network, runtime control or source write method
is registered. Source/guidance text remains untrusted data.

`Tool(name, description, input_schema, handler)` lets an explicitly assembled
server register an approved sibling adapter. The complete assembled catalog
must pass the shared 12-tool/32-KiB check; registering a tool does not approve
its data boundary. Startup guidance stays below 2 KiB, normal structured
results below 32 KiB, and exact optional resource reads below 256 KiB.

## Validation

Run focused contract and transport checks, then the complete wrapper checks in
`AGENTS.md` on the integrated revision:

```sh
python3 -m unittest -v tests.test_mcp_contract tests.test_mcp_context tests.test_mcp_server
python3 -m atrinik_workspace.mcp_contract validate
python3 -m compileall -q atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
git diff --check
```

Synthetic tests cover current manifest mapping, Classic sharing, 301-record
pagination, stale cursors and snapshots, profile corruption, branch/dirty/
authorization changes, no-follow source access, origin mismatch, cancellation,
concurrent reads, protocol bounds and unchanged source/registry state. Real
cross-repository pilot and before/after retrieval measurements belong to the
integration pilot; focused fixtures alone do not establish those results.
