# Workspace architecture

## Ownership boundary

This repository owns only orchestration code and the checkout/component
manifest. Implementation, release packaging, and component-specific tests
belong to the physical repositories listed in `components.json`. The workspace
coordinator does not vendor, pin, or commit component source. A physical
checkout may provide one logical component at its root or several components
from declared source directories.

The wrapper also owns the VS Code launch configurations that compose this
multi-repository workspace. The default configuration delegates repository
setup to replacement-only `./atrinik init`, preserving existing checkouts
while cloning missing default-cohort repositories. It never opts into classic
repositories. The specialized Windows cross-build configuration validates the
same manifest after initialization. The standalone `devcontainer` repository
owns the versioned toolchain images, not the wrapper-specific launch
configuration.

`components.json` schema 3 separates physical `checkouts` from logical
`components`. A checkout owns its repository, branch, local destination,
generation, license, and initialization-cohort membership. A component names
its checkout and a safe relative `source`, plus provider roles, requirements,
license, generation, and local build contract. The replacement/default and
opt-in classic cohorts contain physical checkout identities. The built-in
`default` and `classic` profiles are coherent stacks of logical components
rather than aliases for every manifest entry.

The `atrinik/classic@main` checkout at `./classic` provides five logical
components: `classic-client` from `client/`, `classic-server` from `server/`,
`classic-editor` from `editor/`, `classic-libatrinik` from `libatrinik/`, and
`classic-protocol` from `protocol/`. `content` remains
`atrinik/content@main` at `./content` for the replacement stack, while
`content-1x` is `atrinik/content@1.x` at `./content-1x` for the classic stack.

Manifest validation rejects duplicate checkout or component names, duplicate
local destinations, unsafe or overlapping source roots within one checkout,
unknown cohorts, checkouts, or roles, generation mismatches, dependency
cycles, multiple providers for one required role, and incompatible roles on
one implementation. Two checkouts may name one GitHub repository only when
their names, branches, destinations, cohorts, and roles distinguish them
completely. Profile and state schemas are likewise strict: duplicate keys,
missing fields, unknown fields, invalid names, and repository mismatches fail
before an operation changes data.

Supply-chain ownership is a wrapper-level cross-repository contract.
`supply-chain/inventory.json` names every active or archived organization
repository and records each supported dependency's owner, consumers, version
source, license, acquisition path, update cadence, EOL response, validation,
and retain/isolate/replace/remove disposition. Component lockfiles remain the
integrity boundary for independently released source and runtime archives; the
aggregate catalog describes and audits those boundaries instead of copying
their fetching implementations into the wrapper. Inventory records preserve an
explicit nullable commit-resolution field rather than hard-coding moving
branch heads. Profile-aware generated SBOM records preserve physical checkout,
logical component and source root, repository, branch, resolved full commit,
cohort, stack, role, and license, and mark uninitialized or non-selected commits
unavailable so repeated coordinates such as `atrinik/content` or shared
checkouts such as `atrinik/classic` cannot be collapsed into one ambiguous
input.

Source provenance is also a cross-repository contract. Its exhaustive approved
historical MIT provenance grantor registry is:

| Grantor | Covered material | Permitted operations | Destination license |
| --- | --- | --- | --- |
| Zoey Rose | All original past Atrinik contributions solely authored by Zoey Rose | Copy, migrate, translate, or relicense | MIT |
| Daniel Liptrot | All original past Atrinik contributions solely authored by Daniel Liptrot | Copy, migrate, translate, or relicense | MIT |

A grant applies only when complete, non-shallow Git history follows renames and
moves, proves the selected material is the named grantor's original work and
was solely authored by that grantor, and supports the historical author-identity
mapping. Review must exclude embedded third-party or conflicting-licensed work.
A migration records the exact source repository, path, and revision;
destination repository and path; author identity and complete-history evidence;
transformation; third-party review; applicable grantor and grant; and exact
wrapper repository revision containing the registry entry as grant evidence in
its pull request or a committed provenance manifest. Current blame, incomplete
or uncertain history, or authorship of only part of a mixed file does not
establish eligibility for the whole material; the process fails closed until
every doubt is independently resolved.

The `supply-chain` command resolves component inputs through the same profile
selectors as builds, then reads Git-indexed files without mutating a checkout.
The audit requires immutable remote Actions and container images, updater
hints, an owned catalog entry for every dependency input, weekly GitHub Actions
update configuration, and no submodules. For a shared monorepo checkout, only
the root repository workflows and Dependabot file are active GitHub metadata;
logical source audits exclude imported nested copies while continuing to audit
their dependency inputs. Explicit worktree overrides are
absolute and must match the expected repository and branch identity. A review
branch for a repeated coordinate must share the expected checkout primary's
common Git directory, preventing `content` and `content-1x` from being
interchanged. Deterministic
license, CycloneDX, SPDX, and local version reports are generated only below
the ignored build directory; report generation resolves full commit IDs through
`--profile PROFILE`. The scheduled audit composes and audits the initialized
default and classic stacks explicitly, including both content release lines,
and publishes stack-separated reports; pull-request CI validates the catalog
and its implementation without silently accepting a partial organization
snapshot.

Read-only inspection commands keep structured data on stdout and suppress Git
traces so callers can safely consume `status --json`, `worktree list --json`,
`profile show --json`, `state list --json`, and `path`. Status never fetches;
its ahead/behind fields describe the cached canonical remote ref and are null
when that ref is unavailable. Its physical-checkout records also expose
default/classic membership, logical modules, and optional initialization state,
so an absent opt-in checkout is not mistaken for a broken default workspace.

## Managed layout

~~~text
<wrapper>/
  <checkout-destination>/           primary independent Git checkouts
workspace/
  worktrees/<checkout>/<label>/      physical-checkout Git worktrees
  profiles/<name>.json               logical component -> checkout selectors
  build/profiles/<name>-<key>/       isolated sources, builds, and runtime
  build/npm-cache/                   shared package download cache
  topologies/<name>/                 supervised process state and rotated logs
  state/server/<name>/               persistent mutable server data
  states.json                        named external-state registry
~~~

`ATRINIK_WORKSPACE_DIR` relocates the `workspace/` layout but never the primary
physical repositories beside the wrapper. Manifest checkout destinations and
clone-staging directories are ignored by the wrapper repository. Existing
paths are validated against the expected repository, branch, and checkout
identity and are never overwritten. The top-level workspace, replaceable build
directories, and generated views carry schema-versioned ownership markers.
Replacement helpers require the exact expected marker and verify that the
target remains below the build root.

Component initialization derives its GitHub clone transport from the wrapper's
first recognized `origin` or `upstream` URL. SSH and HTTPS wrapper clones
therefore produce component clones with the same authentication transport;
public HTTPS is the fallback when the wrapper has no recognized GitHub remote.

With no explicit arguments, initialization resolves only the
replacement/default cohort. `init --with classic` resolves the union of the
default and classic cohorts; the option is additive and has no classic-only
alias. The `classic` monorepo, GPL `tools`, and `content-1x@1.x` are absent from
ordinary initialization. Explicit checkout or logical-component
initialization remains available for partial workspaces. Aliases that own one
physical checkout are deduplicated. Initialization is idempotent, stages clones
away from their destination, and does not update or repurpose an existing
checkout.

Synchronization is intentionally narrower than initialization. With no names
it visits only already-initialized default-cohort primaries; `--with classic`
adds already-initialized classic-cohort members, and explicit names select
exact physical checkouts through either checkout or logical-component
identities. One checkout is synchronized only once. A missing optional
repository is reported and skipped rather than cloned as a synchronization
side effect.

## Classic monorepo migration

`migrate repositories` is a checked transaction for workspaces that still
contain the five former standalone classic repositories. After the destination
`./classic` checkout is initialized with exact `./atrinik init classic`,
`--dry-run` computes and reports the full plan, `--apply` executes that plan,
and `--audit` verifies the resulting
invariants; each mode supports `--json` for machine-readable output.
Additive `init --with classic` is deferred until after migration because its
default-cohort preflight refuses former classic repositories that still occupy
replacement checkout destinations.

Planning inventories repository identities and histories, branches, tracked
and untracked changes, remotes, common Git directories, attached worktrees,
locks, destination conflicts, saved profiles, topology liveness, and relevant
generated paths. Historical inputs may be at the original canonical paths or
at `legacy-server`, `legacy-client`, `legacy-editor`,
`legacy-libatrinik`, and `legacy-protocol`. Their matching destinations are
`classic/server`, `classic/client`, `classic/editor`,
`classic/libatrinik`, and `classic/protocol`.

Apply is preflighted as one recoverable operation: it preserves attributable
source history, recoverable original repositories, and linked-worktree
directories; repairs only the Git administrative relationships required by
the new monorepo layout; and rewrites proven classic profiles atomically to the
checkout-root/source-root schema. It refuses uncertain repository identity,
ambiguous or conflicting occupants, unsafe Git states, running affected
topologies, or selectors that cannot be proven. The operator resolves a
reported condition and reruns the complete dry run rather than moving paths by
hand. Interruption and rollback handling never deletes a user path.

Commit-map targets already integrated into classic history are reused as
bridge parents. A branch-only map target that disappeared with the retired
`history/*` namespace is not an error: apply imports the exact commit from the
verified local source checkout and records it as the bridge parent.

Repository migration does not move or reinterpret `content`, `content-1x`,
state directories, build trees, collected runtimes, scenario data, topology
records, or logs. Typed state ownership is a separate migration boundary.
Scenario metadata records its stack and exact checkout/component/source
providers. An old scenario without that immutable identity is kept as an inert
record and cannot inherit the current meaning of a reused profile name.

## Profile resolution and build flow

A profile records a stack and maps each logical component in that stack to its
primary checkout root, a managed physical-checkout worktree label, or an
absolute external checkout root. The built-in `default` profile chooses
replacement providers; the built-in `classic` profile chooses the five
`classic-*` providers from `atrinik/classic` plus `content-1x`. Cloning a
built-in or saved profile with `profile create --from` retains that stack
identity. Resolution verifies that every selected root is a Git worktree whose
`origin` or `upstream` and checkout identity match the manifest. Primary
checkouts additionally require the manifest branch; feature-worktree and
explicit-path selectors may use review branches. Only after validation does
resolution append the logical component's declared source path.

Worktrees are always full physical repositories and are keyed by checkout, so
every classic worktree lives below `workspace/worktrees/classic/<label>` and
contains all five source directories. A `profile set` selector naming physical
checkout `classic`, logical component `classic-server`, or role `server`
updates all five in-stack components to the same root. Profile schema 3
requires identical selectors for every logical component sharing a physical
checkout and rejects attempts to combine two classic worktrees in one profile.
Separate module branches must first be combined into one monorepo branch and
worktree. Resolution then appends each logical component's own source without
treating a subdirectory as an independent Git worktree.

An exact historical schema-1 or schema-2 classic profile may be migrated with
an internal `migrated-worktree` selector for an existing content worktree. This exception
is restricted to `content-1x`, the old managed content-worktree namespace, and
the `content@main` Git common directory; normal selectors retain the exact
branch/common-directory rules. Profile-aware sync treats every such path as a
classic migration artifact and excludes it before checking or updating
`content@main` worktrees.

The role graph is resolved before component source paths or commands. Each required role
has exactly one provider in the selected stack, and the transitive service
closure cannot contain both replacement and classic client, server, protocol,
`libatrinik`, or content providers. Shared read-only inputs may appear in both
graphs only when the manifest declares compatibility. Mutable and generated
inputs are isolated by profile, build key, topology, and state.

The profile schema, stack generation, exact repository/branch/checkout/source
and role-provider identities, and normalized absolute roots are hashed into the
profile build key. Topology and scenario records persist the same repository
and branch coordinates; records from before that identity existed remain inert
instead of being reinterpreted through a changed manifest. A fully initialized
classic workspace uses the common buildable-component
selection, so `build all --profile classic`, component builds, and classic
launches share incremental output. A partial workspace uses only the requested
target's dependency closure. This separates combinations across distinct
physical checkouts while preserving compiler output when the same worktrees advance,
and makes pre-split build trees inert rather than reinterpreting them under
replacement identities. The coordinator creates disposable source views
rather than writing dependency links or output into source checkouts.

The currently playable classic build flow is:

~~~text
selected content-1x -> build_runtime.py -> isolated content/lib + content/maps
selected tracked resource allowlist -> isolated resource view
selected classic protocol/library + sound -> client source view -> CMake/Ninja
selected classic protocol/library --------> server source view -> CMake/Ninja
selected Worker -------------------------> npm source view -> npm run check
~~~

The replacement MIT `server`, `client`, `editor`, `protocol`, `renderer`,
`content-toolkit`, and `website` repositories are seed components. Their
manifest entries and default role graph are valid, but the game build and
runtime commands reject unavailable contracts clearly until those contracts
land. They are never routed through the classic C/CMake implementation merely
to make `default` appear runnable.

The server source view uses a copied `install_data` directory because its CTest
preparation uses CMake directory-copy semantics. Other authored inputs remain
links to their selected worktrees. The Worker view is copied because Node's
module resolver follows configuration-file links and would otherwise search the
Worker checkout instead of the isolated dependency directory. Collected
content and resource dependency metadata identify the selected path and commit.
The resources repository's `runtime-paths.txt` is the distribution boundary:
only tracked regular files below those paths enter the runtime view. This keeps
repository metadata, local untracked files, and symlinks out of the asset
protocol.

## Runtime and state

For the currently runnable classic profile, server launch preparation assembles
a disposable working directory with links to the selected build, plugins,
collected `content-1x`, resources, configuration, GPL tools, and named
persistent state. GPL tools are not part of the replacement/default role graph.
First use initializes a state atomically from the selected server's
`install_data`; an existing directory is validated and never overlaid. State
inside a server source worktree is rejected. Replacement runtime preparation
remains unavailable until its native component contracts land.

The coordinator takes an advisory exclusive lock next to the state directory
before build/runtime preparation and holds it for the lifetime of a launched
server. Profile builds have their own blocking lock, and server launch views are
keyed by state path. Processes started outside the coordinator do not
participate in the state lock, so operators must not point those processes at
the same state concurrently.

Profiles are stack-aware source-topology definitions for supervised runtime as
well as builds. `up` resolves the requested service's logical provider closure
once, records the exact paths/commits and build root, prepares the same isolated
views used by foreground launches, and hands the state-lock file descriptor
through a short forking bootstrap to a detached native supervisor and its
server child. The lock remains held for the server lifetime without a
long-lived invoking CLI process.

The supervisor owns child lifetimes and size-bounded rotating logs. Status
records include Linux process start ticks in addition to PIDs; `ps` and `down`
require both to match, preventing an old status file from targeting an
unrelated reused PID. Shutdown signals the supervisor, which gracefully stops
children before releasing state. For a paired topology it starts the server
first, waits for its fingerprint and final ready signal, and then pins the
client to that authenticated loopback endpoint. Available UDP ports are
allocated under a workspace-wide lock, or callers may request an explicit
port. Each runtime name owns an isolated persistent client configuration base.
A server topology takes ownership of its collected content and staged resource
directories after the shared incremental build, so later builds cannot mutate
the filesystem seen by a running process.
A topology may select one service, and distinct runtime names permit concurrent
combinations as long as their server ports and mutable state directories do not
conflict. When replacement runtime support lands, a concurrent `classic` and
`default` comparison must use separate profiles, topology names, server states,
ports, client configuration roots, and generated views. The stack boundary is
never relaxed for a comparison.

Test scenarios are owned directories below `workspace/scenarios/`, each with a
strict metadata record, ownership marker, mode-0600 password file, and dedicated
registered server state. Creation resolves and builds the scenario profile,
initializes state from the selected server's `install_data`, and invokes that
server in offline provisioning mode. The server uses its normal validation,
Argon2id password, atomic account-save, and exclusive player-reservation paths;
the empty player record causes first login to follow normal character creation.
No scenario secret is stored in metadata or passed as a process argument.
The metadata records physical checkout, logical source, path, commit, and dirty
status for the selected server, content, resources, protocol, and library
inputs so an audit does not imply that a scenario provisioned from local edits
came from a clean commit.

Creation and reset serialize on a scenario-operation lock. State registry
writes also serialize independently, preventing concurrent state additions
from losing entries. Reset validates that the scenario marker, metadata,
credentials, registered path, and server-state shape still match, then takes
the same nonblocking state lock used by server launches. It provisions a fresh
staging state before atomically replacing only the scenario-owned state, so a
running topology, external state, symlink, malformed directory, or failed
provision cannot be overwritten.

Verification handoffs are wrapper-native operating procedures. They identify
the exact profile, topology, and state selected for a change; use `build
--test` for automated validation; use `topology show`, `up`, `ps`, and `logs`
for runtime inspection; describe the feature-specific actions and expected
results; and finish with `down`. When a ready player is useful, the handoff
adds `scenario credentials` without copying its password. This keeps manual
review on the same resolved source topology, isolated client configuration,
supervised processes, logs, and state locks as automated workspace validation.

The two-terminal foreground launch path uses the same named state and explicit
UDP port for both commands. Its client reads only the certificate block from
the state's persistent QUIC identity and hashes the DER certificate to produce
the pinned loopback endpoint. The identity must already have been generated by
the matching server. Foreground and supervised local launches disable client
metaserver/STUN discovery and server STUN/automatic port mapping; caller
arguments are appended so an intentional executable-level override remains
possible.

## Trust and command execution

Selected checkout worktrees and component source roots are executable source:
CMake, Python collection, npm, and runtime commands execute code from the
selected profile. Review a pull request before selecting its worktree.
Profiles do not provide a security sandbox.

Subprocesses use argument arrays rather than a shell. User-provided executable
arguments are not evaluated as shell syntax, and recognized join-password
forms are redacted from diagnostics. Git operations refuse dirty primary
checkouts and dirty managed worktrees where an automatic update or removal
could lose work. Repository migration may repair only the Git administrative
`.git` pointer of a dirty linked worktree after moving its clean primary/common
Git directory; it leaves that linked worktree's path, index, tracked files, and
untracked files untouched.

## Release model

Pull-request titles and squash commits use Conventional Commits syntax. A push
to `main` runs semantic-release with the standard conventional-commits
parser: `fix` and other recognized work produce a patch, `feat` produces a
minor, and a breaking change produces a major. The catch-all patch rule ensures
every accepted squash commit produces a release. Releases attach the exact
component manifest and its SHA-256 checksum; component artifacts remain owned
by their respective repositories. The same workflow has a no-input manual
dispatch trigger for recovering a missed or interrupted Actions run; semantic-
release remains responsible for selecting the unreleased commit range and next
version. Release ownership follows physical repositories: the five classic
logical components share the `atrinik/classic` repository's commit and release
history rather than publishing as independent checkouts.
