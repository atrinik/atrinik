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
The classic-only `playtester` component occupies `atrinik/playtester@main` at
`./playtester`, provides the `playtester` role, and requires the classic stack's
`content`, `libatrinik`, and `protocol` providers. Its `build: none` contract
keeps repository-owned installation and validation outside wrapper adapters.

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

Source provenance is also a cross-repository contract.
[`PROVENANCE.md`](PROVENANCE.md) is the single exhaustive historical MIT
grantor registry and evidence procedure. Reuse fails closed unless complete,
rename-aware history proves sole original authorship and identity, separates
eligible material, excludes conflicting embedded work, and records the exact
source, destination, transformation, review, and wrapper registry revision.

The `supply-chain` command resolves component inputs through the same profile
selectors as builds, then reads Git-indexed files without mutating a checkout.
Before selecting audit-ready roots, it requires every physical checkout and
logical component in the profile to resolve. Review-worktree overrides replace
specific roots but never relax that completeness invariant. An unavailable
member therefore fails the aggregate operation instead of turning it into a
partial audit.
The audit requires immutable remote Actions and container images, updater
hints, an owned catalog entry for every dependency input, weekly GitHub Actions
update configuration, and no submodules. It discovers npm, Cargo, Go, Buf,
Rust-toolchain, and repository dependency-policy manifests. Workflow runner
expressions are admitted only when a literal static matrix enumerates every
value, and an `npx` invocation is owned only when the same workflow first pins
an immutable `setup-node` action. For a shared monorepo checkout, only the root
repository workflows and Dependabot file are active GitHub metadata; logical
source audits exclude imported nested copies while continuing to audit their
dependency inputs. A named profile audit refuses to run unless every
audit-ready component in the stack is initialized or explicitly overridden;
absence cannot degrade a complete audit into a partial report. Explicit
worktree overrides are
absolute and must match the expected repository and branch identity. A review
branch for a repeated coordinate must share the expected checkout primary's
common Git directory, preventing `content` and `content-1x` from being
interchanged. Deterministic
license, CycloneDX, SPDX, and local version reports are generated only below
the ignored build directory; report generation resolves full commit IDs through
`--profile PROFILE`. The scheduled audit delegates checkout composition to
`./atrinik init --with classic`, audits both complete stacks including both
content release lines, and publishes stack-separated reports; regression tests
bind that initialization ordering, the fail-closed profile contract, and the
`content@1.x` workflow-runner policy.

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
  build/compiler-cache/              bounded shared native compiler cache
  build/retention.json               optional strict build pin/rollback record
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
alias. The `classic` monorepo, GPL `tools`, MIT `playtester`, and
`content-1x@1.x` are absent from ordinary initialization. Explicit checkout or
logical-component initialization remains available for partial workspaces.
Aliases that own one physical checkout are deduplicated. Initialization is
idempotent, stages clones away from their destination, and does not update or
repurpose an existing checkout.

Synchronization is intentionally narrower than initialization. With no names
it visits only already-initialized default-cohort primaries; `--with classic`
adds already-initialized classic-cohort members, and explicit names select
exact physical checkouts through either checkout or logical-component
identities. One checkout is synchronized only once. A missing optional
repository is reported and skipped rather than cloned as a synchronization
side effect.

## Cleanup inventory and retention

`cleanup` is an explicit garbage-collection boundary, never an implicit step
of initialization, synchronization, build, or startup. Default and explicit
`--dry-run` modes are read-only. `--apply` first takes the same
repository-layout lock used by checkout, build, topology, foreground-run, and
scenario operations, then performs one complete inventory and size
recomputation. Immediately before each removal it freshly revalidates that
target's safety dependencies without rescanning unrelated report-only
payloads; any new ambiguity fails closed. Build roots are removed before Git
worktrees; the explicitly selected npm cache and safe prunable Git metadata
come last. A race before the first mutation aborts the plan. A later failure
stops the ordered sequence and reports completed reclamation without attempting
to reconstruct generated data.

Inventory records are stable-sorted and carry a kind, physical owner and
repository, exact path, no-follow allocated size, age and age basis,
disposition, stable reason codes, and applicable profile/scenario/topology/
migration/retention or merged-PR evidence. JSON retains exact integer byte
fields; text renders compact base-1024 IEC sizes. Size accounting uses
device/inode identity and excludes registered nested worktree roots from mixed
container records, preventing hard links or overlapping roots from inflating
global byte totals. Filesystem traversal or metadata ambiguity protects the
affected scope.

Current-checkout worktrees are owned only as direct children of
`workspace/worktrees/<checkout>/`. Current wrapper-self worktrees are owned
only when Git registers them directly under `workspace/worktrees/atrinik/`;
historical wrapper worktrees are recognized only as direct children of the
top-level `build/worktrees/` namespace. Common-Git-directory and canonical
repository identity, named/unlocked state, absence of in-progress Git
operations, and ordinary tracked/untracked cleanliness are mandatory. Saved
profile selectors of kind `worktree`, absolute `path`, or migration-only
`migrated-worktree`; retained scenario coordinates; live topology coordinates;
and every original/destination/composite migration path protect exact
worktrees. Ignored output is sized and disclosed but does not defeat
eligibility. GitHub's authenticated commit-associated-pulls API normally must
prove an exact merged head against the checkout's manifest branch, no open
association, and the requested grace age.

Only a historical wrapper worktree directly below `build/worktrees/` may use
the historical PR-base proof. Its expected coordinate remains
`atrinik/atrinik@main`, while the PR base must be `master`. Authenticated API
evidence supplies the exact `head.sha`, `base.sha`, and `merge_commit_sha`; the
local merge commit's first parent must equal the API base SHA and that merge
commit must be an ancestor of the frozen final-`master` boundary
`ee5ba2096c94bce0161629423d4962a966bc61d8`. Graph inspection ignores replace
refs and fails closed when `info/grafts` exists, so neither replacement nor
graft metadata can rewrite the ancestry proof. Missing, mismatched,
unavailable, or ambiguous evidence protects the target. Apply reruns this API
and local Git proof immediately before removal.

Worktree status is inspected with `--ignore-submodules=none`. A populated
submodule protects the worktree even when its visible files are clean because
per-worktree Git directories can retain private refs, reflogs, and objects. A
missing worktree whose administrative record retains such a `modules`
directory protects its repository-wide prune scope. Removal remains entirely
non-force and preserves local and remote branch refs and Git objects. Prunable
administrative records use the same PR and ownership proof and are pruned only
when no protected prunable sibling shares that Git prune operation.

Profile build ownership is a direct-child path plus an exact regular
`.atrinik-workspace-managed.json` purpose marker. Each use under the per-build
lock atomically refreshes `.atrinik-build.json` with the profile/key, purpose,
exact role-to-repository/branch/checkout/source paths and commits, and
`last_used_at`. Older marker-owned roots fall back to the maximum mtime from
the same no-follow size walk. Live topology `build_root` values, busy build
locks, registered Git worktrees, and exact absolute roots in strict schema-1
`workspace/build/retention.json` protect a build. A shared `managed_remove()`
helper repeats containment, symlink, marker, schema, and purpose validation
immediately before deletion.

The npm and compiler caches have exact purpose markers and atomically refreshed
`.atrinik-cache.json` timestamps. The compiler cache metadata also fixes its
5 GiB bound. Both scopes are opt-in. One pre-marker npm cache at its fixed path
can be treated as a legacy known cache only after the workspace marker, fixed
containment, no-symlink shape, age, and inactive-build checks pass; apply
adopts the marker before calling the common removal helper. No other unmarked
path receives that exception. Unmarked profile roots, unknown
`workspace/build` children, and the mixed top-level `build/` tree remain
visible report-only `unmanaged-build` records. Cleanup never targets profiles,
scenarios, state, topology records/logs, migration archives/evidence, branches,
Git objects, or arbitrary unmarked paths.

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
classic workspace derives one common buildable role set from the selected
stack's providers and dependency graph, so `build all --profile classic`,
component builds, scenarios, and classic launches share incremental output.
Resolution validates each selected physical checkout once before deriving its
logical role paths and coordinate records. A partial workspace uses only the
requested target's dependency closure; a preferred checkout that is present
but malformed fails validation rather than silently shrinking that closure.
This separates combinations across distinct physical checkouts while
preserving compiler output when the same worktrees advance, and makes pre-split
build trees inert rather than reinterpreting them under replacement identities.
The coordinator reconciles marker-owned source views in place rather than
writing dependency links or output into source checkouts. It retains unchanged
safe links and copies, replaces changed or retargeted entries, removes stale
entries, and rejects source symlinks that escape their selected source root.
Linked-tree structure and symlink targets participate in view identity; dirty
Git sources conservatively force configure so untracked or otherwise unmodeled
CMake inputs cannot remain hidden behind a warm graph.

The currently playable classic build flow is:

~~~text
selected content-1x -> build_runtime.py -> isolated content/lib + content/maps
selected tracked resource allowlist -> isolated resource view
full Classic closure -> integrated source view -> one protocol/libatrinik graph
                                             +-> client targets -> CMake/Ninja
                                             +-> server targets -> CMake/Ninja
component-only client/server request -> standalone source view -> CMake/Ninja
selected Classic + content + resources ---> offline worldmaker -> region-map cache
selected Worker -------------------------> npm source view -> npm run check
~~~

The integrated graph is selected only when client, server, protocol, and
libatrinik resolve to sibling directories in one physical Classic checkout.
Its binaries remain below a distinct `build/integrated` tree, and per-role
marker records select those artifacts for later runtime staging. A successful
standalone component build updates only that role's marker. This prevents an
older graph from being selected merely because its output directory exists,
while keeping partial Classic checkouts and component-specific FetchContent
validation supported.

The replacement MIT `server`, `client`, `editor`, `protocol`, `renderer`,
`content-toolkit`, and `website` repositories have validated standalone M1
build, package, provenance, and dependency contracts. Their manifest entries
and default role graph are valid, but wrapper build/runtime adapters and the
integrated service closure have not landed, so game build/runtime commands
still reject them clearly. They are never routed through the classic C/CMake
implementation merely to make `default` appear runnable.

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

Each CMake binary tree stores an atomic configure fingerprint covering the
source-view identity, Ninja generator, CMake and compiler identities, toolchain
and cache arguments, `BUILD_TESTING`, and relevant compiler/platform
environment. The wrapper skips only its explicit configure command when both
that fingerprint and the reconciled view are unchanged and the CMake cache and
Ninja graph still identify the expected source/generator; `cmake --build`
remains able to invoke CMake/Ninja dependency regeneration. Compiler,
toolchain-file, and initialization-environment changes reinitialize only the
marker-owned CMake binary tree. A forced configure is available through
`--force-reconfigure` without resetting an otherwise matching tree.

When `ccache` is discoverable, the wrapper sets supported C and C++ CMake
compiler launchers and a marker-owned shared cache with a fixed 5 GiB maximum.
The per-build cache base and compiler prefix-map arguments normalize equivalent
profile roots to stable `/atrinik/source` and `/atrinik/build` diagnostics
without sharing mutable CMake binary trees. Prefix-map flags are added only
after the selected environment compiler proves support; opaque toolchain
compilers retain their own flag syntax and directory-sensitive cache keys.
Compiler, toolchain, test-mode, or relevant environment changes invalidate
configure state; source dependency changes continue through Ninja.
`--no-ccache` explicitly opts out and clears any cached launcher setting.

After a Classic server build, the coordinator runs the built server's offline
worldmaker in a temporary runtime assembled from the selected server,
collected content, and staged resources. The generator writes into a sibling
temporary directory; the coordinator accepts it only when every output is a
nonempty regular `.png` or UTF-8 `.def`, the basename sets form complete pairs,
and the expected `incuna_-1` pair exists. It then installs the marker-owned
cache atomically, preserving the previous valid cache on failure. Cache
metadata records provider, repository, branch, checkout, source, path, and
commit coordinates for the server dependency closure consumed by the
worldmaker. Unrelated roles in the common profile build root do not invalidate
that cache. It is reusable only for clean input checkouts with an exact metadata
match; dirty inputs deliberately regenerate.

## Runtime and state

For the currently runnable classic profile, server launch preparation assembles
a disposable working directory with links to the selected build, plugins,
collected `content-1x`, resources, configuration, GPL tools, and named
persistent state. GPL tools are not part of the replacement/default role graph.
First use initializes a state atomically from the selected server's
`install_data`; an existing directory is validated and never overlaid. State
inside a server source worktree is rejected. Replacement runtime preparation
remains unavailable until its native component contracts land.

The server's configured `assetspath` is a disposable transport-neutral runtime
view. Its real `data` directory receives generated core data, while its
`client-maps` directory copies the validated generated cache. Neither belongs to
the named persistent state. A supervised topology copies the cache into its
owned runtime before launch, so a later build cannot change the server's
immutable startup asset snapshot. QUIC serves the snapshot by default;
`http_url` separately advertises an optional operator-managed HTTP(S) origin.
The cache follows marker-owned profile-build cleanup; topology copies follow
topology retention and are replaced on the next launch of that topology name.

The coordinator takes an advisory exclusive lock next to the state directory
before build/runtime preparation and holds it for the lifetime of a launched
server. Profile builds have their own blocking lock, and server launch views are
keyed by state path. Processes started outside the coordinator do not
participate in the state lock, so operators must not point those processes at
the same state concurrently.

Profiles are stack-aware source-topology definitions for supervised runtime as
well as builds. For a complete Classic profile, `up` resolves the common
manifest-derived build-root selection once while building only the requested
service targets; a partial profile retains the requested service's dependency
closure. It records the exact paths, commits, and build root, prepares the same
isolated views used by foreground launches, and hands the state-lock file
descriptor through a short forking bootstrap to a detached native supervisor
and its server child. The lock remains held for the server lifetime without a
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
A supervised client also receives a bounded, process-only launch label naming
its topology and profile; a foreground client receives its profile and direct
run mode. The client uses this label only for its native window title. It is
not part of persisted settings, package or protocol identity, or network
metadata, and unmanaged launches receive no label.
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

Shell completion is a separate read-only path ahead of `Workspace`
construction and normal command dispatch. One bounded line-oriented protocol
walks the same `argparse` metadata as execution and supplies all three generated
shell adapters. It loads `components.json` directly and uses shallow,
no-follow reads of existing profile, worktree-label, state, scenario, and
topology names under the path selected by `ATRINIK_WORKSPACE_DIR`. Directory
scans and metadata sizes are capped; malformed, stale, unreadable, symlinked,
or unsafe/control-character records fail quietly without removing static
parser candidates. Filesystem arguments are delegated to native shell path
completion, and the protocol never reads scenario passwords or external state.

The completion path does not create the workspace or its ownership marker,
directories, locks, profiles, or registries. It does not inspect Git, call a
subprocess, use the network, validate repositories, probe topology liveness,
or enter initialization, synchronization, build, migration, cleanup, runtime,
or credential dispatch. Each keypress starts a fresh bounded query, so local
additions and removals are visible without a persistent cache. Generated
adapters pass the invocation and words as argument arrays and never evaluate a
candidate as shell code; wrapper completion stops at `--` and the `run`
remainder boundary.

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
