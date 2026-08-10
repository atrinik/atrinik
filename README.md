# Atrinik development workspace

[![Coverage](https://codecov.io/gh/atrinik/atrinik/graph/badge.svg?branch=main)](https://codecov.io/gh/atrinik/atrinik)

This repository is a small, MIT-licensed coordinator for Atrinik's physical
repositories and logical components. Each checkout remains a normal Git
repository with its own branches, remotes, commits, and worktrees. A logical
component may occupy that checkout's root or a declared source directory. The
coordinator keeps the MIT replacement stack and the opt-in playable classic
stack as explicit, coherent sets. It supplies one command for safe
initialization and synchronization, composes worktrees into stack-aware review
profiles, builds components whose contracts are available, and shares
persistent server state safely.

No checkout is a submodule. Primary repositories are ignored, independent Git
checkouts directly beside this README (`./client`, `./server`, `./classic`, and
so on). The `atrinik/classic` checkout at `./classic` contains the logical
`classic-client`, `classic-server`, `classic-editor`,
`classic-libatrinik`, and `classic-protocol` source roots. Generated worktrees,
builds, profiles, and default state live under the ignored `workspace/`
directory.

## Requirements

- Python 3.11 or newer; supervised topologies require Linux process identity
  and pidfd support
- Git and the authenticated GitHub CLI (`gh`)
- CMake, Ninja, and the dependencies required by the selected component when
  building the classic native stack
- Node.js 20 or newer when building `metaserver-worker`

The Atrinik development container supplies the native build dependencies. Run
all commands below from this repository's root.

### Development container

Open this wrapper repository in VS Code and choose **Dev Containers: Reopen in
Container** to use the pinned Linux build environment. On first creation, the
container runs `./atrinik init`; it clones only missing replacement/default
repositories and validates existing checkouts without updating or replacing
them. It never adds classic repositories, `content@1.x`, the MIT playtester,
or GPL tools implicitly. The Windows cross-build configuration is available at
`.devcontainer/windows-cross/devcontainer.json` after the required component
checkouts have been initialized.

The wrapper owns these launch configurations because they compose the complete
development workspace. The standalone `devcontainer` component owns only the
published Linux and Windows toolchain images they reference.

## Dependency and supply-chain ownership

`supply-chain/inventory.json` records every supported repository and the owned
toolchains, actions, images, source archives, system libraries, optional tools,
vendored sources, licenses, update cadences, EOL responses, and validation
paths they consume. Repository records distinguish physical checkout, logical
component, source root, repository coordinate, branch, commit-resolution
state, cohort, role, and license; generated reports resolve the full commit
from the selected profile and explicitly mark uninitialized or non-selected
components unavailable. That distinction is mandatory both for logical
components sharing `atrinik/classic` and for the two `atrinik/content`
checkouts. In an aggregate monorepo checkout, the physical checkout record
audits the active root workflows and Dependabot policy. Logical component
records still audit dependency inputs throughout their source directories, but
ignore imported nested workflows and nested Dependabot files because GitHub
does not activate them. The strict schema is checked without a third-party
Python dependency. GitHub
Actions and container images use immutable commits or
digests with human-readable update hints, and every active repository enables
weekly Dependabot updates for its supported ecosystems. Git submodules are not
a supported dependency path.

Validate the catalog alone or audit the exact checkouts selected by a complete
profile. Initialize the replacement stack before its audit; use the additive
Classic initialization when auditing either stack together or the Classic
stack itself:

~~~sh
./atrinik supply-chain validate
./atrinik init
./atrinik supply-chain audit --profile default
./atrinik init --with classic
./atrinik supply-chain audit --profile classic
./atrinik supply-chain versions --output build/supply-chain/versions.json
./atrinik supply-chain report --profile default --format licenses \
  --output build/supply-chain/default/licenses.md
./atrinik supply-chain report --profile default --format cyclonedx \
  --output build/supply-chain/default/cyclonedx.json
./atrinik supply-chain report --profile default --format spdx \
  --output build/supply-chain/default/spdx.json
./atrinik supply-chain report --profile classic --format licenses \
  --output build/supply-chain/classic/licenses.md
~~~

An audit fails before scanning when any physical checkout or logical component
selected by its profile is unavailable, including components that are not yet
marked audit-ready. This prevents an isolated wrapper worktree or partial local
workspace from silently weakening an aggregate audit. Absolute
`--repository NAME=PATH` overrides can select review worktrees, but cannot make
the rest of the profile optional.

Generated reports remain ignored under `build/`. The scheduled organization
audit uses `./atrinik init --with classic` to materialize the manifest-defined
union of both stacks, audits their physical checkout metadata and logical
component source roots, rejects unowned dependency inputs, movable
workflow/image references, and submodules, prints exact available tool
versions, and publishes
separate deterministic license, CycloneDX, and SPDX artifacts for each stack.
Dependency-input discovery covers npm, Cargo, Go, Buf, Rust-toolchain, and
repository dependency-policy manifests. Workflow audits accept only literal
runners or statically enumerated matrix runner values, and every `npx` use must
be preceded in the same workflow by an immutable `setup-node` action.
When auditing review code, initialize the complete profile beside that wrapper
checkout and use repeated absolute `--repository NAME=PATH` overrides only for
the components under review. The command verifies each override's GitHub
repository and branch identity before reading it. For duplicate coordinates, a
review branch is accepted only when its linked worktree shares the expected
checkout primary's common Git directory, so `content` and `content-1x` cannot
be exchanged.

### Historical MIT provenance grants

[`docs/PROVENANCE.md`](docs/PROVENANCE.md) is the single exhaustive approved
grantor registry and evidence contract. Do not duplicate its table in component
guides or skills.

The cross-repository M1 evidence and exact reuse procedure are indexed in
[`docs/REPLACEMENT_FOUNDATIONS.md`](docs/REPLACEMENT_FOUNDATIONS.md). The
machine record maps every fresh repository to its local CI, provenance,
dependency, notices, packaging, SBOM, and mixed-license gates without making
this coordinator a second copy of their implementation policy.

Historical reuse fails closed unless the canonical contract proves complete
history, sole original authorship, identity, separability, and third-party
review. Destination evidence cites the exact wrapper revision containing the
registry used.

The GPL classic utilities and their user-facing replacement/retirement paths
are inventoried in
[`docs/CLASSIC_TOOLS_MIGRATION.md`](docs/CLASSIC_TOOLS_MIGRATION.md). They are
available only through the explicit classic cohort and are not a production
replacement build or runtime dependency.

## Component sets and quick start

~~~sh
./atrinik init
./atrinik status --json
./atrinik profile show default
~~~

With no component arguments, `init` clones only the replacement/default
initialization cohort. That includes the replacement MIT `server`, `client`,
`editor`, `protocol`, `renderer`, `content-toolkit`, and `website` repositories;
`content` from `atrinik/content@main`; compatible shared resources, sound, and
metaserver code; and required development infrastructure. It does not clone
`atrinik/classic`, `atrinik/content@1.x`, `atrinik/playtester`, or the GPL
`tools` repository.
The replacement repositories have independently validated M1 build, package,
provenance, and dependency contracts. Their wrapper manifest build/runtime
adapters and complete service integration have not landed, so `default` is
inspectable and editable but is not yet a buildable or runnable game profile.

Add the complete currently playable classic stack explicitly:

~~~sh
./atrinik init --with classic
./atrinik status --json
./atrinik profile show classic
./atrinik build all --profile classic --test
./atrinik up --name classic-local --profile classic --state default
./atrinik ps classic-local --json
./atrinik logs classic-local server --follow
# Press Ctrl-C to stop following logs; the services keep running.
./atrinik down classic-local
~~~

Building the Classic server also runs its offline worldmaker and stages the
generated region-map `.png`/`.def` pairs in the profile build. Generation uses
the selected Classic, `content-1x`, and resource views, so it requires the same
native build dependencies as the server. A marker-owned cache is reused only
while every selected checkout is clean and its recorded commit still matches;
dirty inputs regenerate on every build. Missing `incuna_-1` output, incomplete
pairs, malformed files, or generator failure stop the build while preserving
the last valid cache.

Server launch uses a disposable HTTP asset root that combines the named
state's existing `http/data` with those generated maps. It does not copy into
or overlay an already registered state. A supervised topology copies the maps
into its owned runtime snapshot before the server takes its immutable asset
snapshot. Region maps are reclaimed with their marker-owned profile build by
the normal preview-first `./atrinik cleanup --scope builds` lifecycle; topology
snapshots remain with the retained topology record and are atomically replaced
the next time that topology name is launched.

`--with classic` has one exact meaning: add the complete classic initialization
cohort to the replacement/default cohort. It is not a classic-only mode. The
cohort consists of one `atrinik/classic` checkout, a distinct `content-1x`
checkout of `atrinik/content@1.x`, the MIT `atrinik/playtester` checkout, and
the retained GPL `tools` repository.
The classic monorepo supplies logical `classic-server`, `classic-client`,
`classic-editor`, `classic-libatrinik`, and `classic-protocol` components from
its `server/`, `client/`, `editor/`, `libatrinik/`, and `protocol/` source
directories. Compatible shared assets and infrastructure are reused from the
default cohort. Rerunning either initialization mode is idempotent and never
updates, rebranches, replaces, or repurposes an existing checkout.

The `playtester` logical component provides the classic-only `playtester` role
and declares `content`, `libatrinik`, and `protocol` inputs. Its wrapper build
contract is `none`: initialization, profiles, worktrees, and supply-chain
identity are coordinated here, while installation and tests remain owned by
`atrinik/playtester`.

Checkout entries have explicit local destinations; normally these are direct
children of the wrapper root such as `./client`, `./classic`, `./content`, and
`./content-1x`. Logical components name their owning checkout and a safe source
root within it. `content` and `content-1x` deliberately name the same GitHub
repository but use different branches, destinations, cohort memberships, and
logical roles.

Initialization follows the GitHub transport of the wrapper's `origin` (then
`upstream`): an SSH wrapper clone produces SSH component clones, while an HTTPS
wrapper clone produces HTTPS component clones. If neither remote supplies a
recognized GitHub URL, initialization falls back to public HTTPS. Existing
explicit partial initialization remains supported, for example
`./atrinik init content renderer`. A logical component name is also accepted;
all names resolving to one checkout are cloned only once. Absent optional
checkouts are reported as optional rather than invalidating status or a
built-in profile.

The manifest assigns logical roles such as `client`, `server`, `protocol`,
`libatrinik`, `content`, and `playtester` to providers within each stack. The
built-in `default` and `classic` profiles resolve exactly one compatible
provider for every role they require.
A runnable service closure cannot combine replacement and classic
implementations; shared read-only repositories are reusable only where the
manifest declares them compatible.

`status --json` is non-networked, stable machine-readable output. For each
requested or manifest-wide physical checkout it reports default/classic cohort
and stack membership, logical modules and roles, license, destination,
initialization state, and whether an absent checkout is optional. It therefore
distinguishes an intentionally absent classic checkout from a broken default
workspace.

## Migrating former standalone classic repositories

An older workspace may have the classic sources as five independent
repositories, either at former canonical paths or the later `legacy-server`,
`legacy-client`, `legacy-editor`, `legacy-libatrinik`, and `legacy-protocol`
paths. Initialize only the destination classic checkout, then use the checked
migration instead of moving directories by hand:

~~~sh
./atrinik init classic
./atrinik migrate repositories --dry-run
./atrinik migrate repositories --dry-run --json
./atrinik migrate repositories --apply
./atrinik migrate repositories --audit --json
~~~

`init classic` needs only the maintained `main` branch. The migration verifies
integrated commits against the immutable commit maps. When an old local
worktree points at a branch-only commit whose rewritten object is no longer
published, apply imports that exact local source commit as a bridge parent
instead of depending on a retired `history/*` ref.

The dry run produces the complete migration plan without changing the
workspace. Apply combines each proven former repository at the matching
`./classic/server`, `./classic/client`, `./classic/editor`,
`./classic/libatrinik`, or `./classic/protocol` source directory, preserves
recoverable originals and existing linked-worktree directories, repairs Git
administrative links when necessary, and rewrites proven classic profiles to
the checkout-root/source-root schema as one checked operation. Source history
remains attributable through the monorepo import rather than becoming a
history-free copy.

Preflight refuses ambiguous identities, conflicting occupants, live affected
topologies, unsafe Git states, and selectors it cannot prove. Review the report,
resolve the named condition, and rerun the complete dry run; never move paths
by hand merely to silence the audit. Content and `content-1x`, server states,
build trees, collected runtimes, scenario data, topology records and logs are
not moved or reinterpreted. `--audit` is read-only and verifies the
post-migration repository, worktree, and profile invariants.

After apply and audit, `./atrinik init --with classic` can fill in the rest of
the classic cohort. Do not run that additive command first in a pre-split
workspace: its default-cohort preflight correctly refuses former classic
repositories that still occupy replacement checkout paths.
Pre-split scenario records that lack immutable stack and provider identities
remain on disk but are deliberately inert; recreate one before using it rather
than allowing its old `default` profile name to acquire replacement providers.

The current classic client command opens a graphical application. Verify that
the devcontainer display forwarding socket is live before launching it. Use
`--dry-run` to build and print either launch command without starting the
process.

### Manual verification handoffs

Change handoffs should end with a copy-pasteable verification recipe that uses
the thin wrapper instead of internal build paths or direct component binaries.
Use the actual profile, topology, and state names for the change. A runtime
client/server review normally follows this shape; until replacement runtime
contracts land, `PROFILE` must be `classic` or derived from it:

~~~sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --json
./atrinik up --name TOPOLOGY --profile PROFILE --state STATE
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY client --follow
# Perform the feature-specific checks described in the handoff.
./atrinik down TOPOLOGY
~~~

The handoff should name any display or runtime prerequisite, list the exact
manual actions and expected results, and include `down` for cleanup. When no
runtime check applies, it should say so and still provide the relevant wrapper
build/test and topology-inspection commands.

## Synchronizing repositories

Inspect the local primary checkouts before changing them:

~~~sh
./atrinik status
./atrinik status client classic --json
~~~

Status is deliberately fast and does not contact GitHub. Ahead/behind counts
compare against each checkout's cached remote default-branch ref; run `sync` to
fetch and fast-forward when current remote state is required. JSON output is
stable machine-readable input for scripts and AI tools. Set membership and
optional/initialized metadata remain present even when an optional checkout is
absent. Status is checkout-oriented: the `classic` record lists the logical
modules supplied by that one repository.

With no component arguments, fast-forward only initialized members of the
replacement/default cohort:

~~~sh
./atrinik sync
~~~

`sync` never clones a missing checkout. Request initialized classic-cohort
members by using the same exact additive option as initialization, or limit the
operation to explicit checkout or logical component identities. Names that
resolve to the same checkout are deduplicated. Also update every clean attached
feature worktree from a newly synchronized primary branch only when intended:

~~~sh
./atrinik sync --with classic
./atrinik sync client classic
./atrinik sync --worktrees merge
./atrinik sync --worktrees rebase
~~~

`sync --with classic` considers initialized default and classic checkouts; it
does not turn an optional missing checkout into a clone. The command stops
instead of changing a dirty checkout. Worktree merge or rebase conflicts are
left in that worktree for normal Git resolution. A migration-retained classic
content worktree is explicitly excluded from `content@main` worktree sync,
even when dirty, and is reported as skipped. Create an ordinary `content-1x`
worktree for ongoing classic content changes.

## Checkout worktrees

Create a branch and full-repository worktree from a checkout's remote default
branch. A logical component name resolves to its owning checkout, so this
classic example creates `workspace/worktrees/classic/socket-review`:

~~~sh
./atrinik worktree create classic-server socket-review \
  --branch feat/socket-review
~~~

Choose another start point or attach an existing local branch:

~~~sh
./atrinik worktree create classic release-fix \
  --branch fix/release --from origin/main
./atrinik worktree create content-1x maps-pr \
  --branch review/maps-pr --existing
~~~

List or remove managed worktrees:

~~~sh
./atrinik worktree list
./atrinik worktree list --json
./atrinik worktree remove content-1x maps-pr
~~~

Removal refuses dirty worktrees. Each worktree is a full Git worktree of its
physical repository, so a classic worktree contains all five classic source
directories. Ordinary `git commit`, `git push`, and `gh pr create` work from
inside it.

## Previewing and reclaiming stale workspace data

Cleanup is always operator-invoked and preview-first. With no options it
inventories registered worktrees and marker-owned profile builds older than
seven days without changing the filesystem:

~~~sh
./atrinik cleanup
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope worktrees classic-server --older-than 14
./atrinik cleanup --scope builds --scope npm-cache --older-than 0
./atrinik cleanup --scope all --older-than 7 --apply
~~~

`--dry-run` is the explicit spelling of the default mode; only `--apply`
mutates. Repeated `--scope` options combine `worktrees`, `builds`, and the
opt-in `npm-cache`; `all` selects all three. Positional checkout or logical
component names narrow only the worktree inventory and still deduplicate
aliases to one physical checkout. The special `atrinik` filter selects wrapper
worktrees. JSON output is stable schema-versioned data and keeps Git/GitHub
diagnostics off stdout; its byte fields remain exact integers. Text output uses
compact base-1024 IEC sizes (`KiB`, `MiB`, `GiB`, and larger) for allocated,
ignored, candidate, protected, and removed bytes.

The worktree inventory covers every initialized physical checkout plus current
wrapper worktrees that Git proves are direct children of exactly
`workspace/worktrees/atrinik/`. Historical wrapper worktrees are considered
only as direct children of the top-level `build/worktrees/` namespace. A
worktree becomes eligible only when it is registered to the expected common
Git directory and repository, named and unlocked, has no Git operation or
ordinary tracked/untracked change, and is not retained by a profile, scenario,
live topology, or repository-migration record. Normally, the authenticated
`gh` commit-associated-pulls query must prove one merged PR with the manifest
base branch, an exact `head.sha` match, no associated open PR, and a merge age
beyond the grace period.

The only historical-base exception is a wrapper worktree directly below
`build/worktrees/`. Its expected coordinate remains `atrinik/atrinik@main`, but
the PR base must be `master`. The authenticated API evidence must include the
exact head, `base.sha`, and `merge_commit_sha`; the local merge commit's first
parent must equal that base SHA, and the merge commit must be an ancestor of
the frozen final-`master` boundary
`ee5ba2096c94bce0161629423d4962a966bc61d8`. The local graph proof ignores
replace refs and fails closed if `info/grafts` exists. Missing, unavailable,
mismatched, or ambiguous API or local Git evidence protects the item, and
apply reruns the proof immediately before removal. Wrong base, closed-unmerged
PR, advanced branch, detached worktree, external path, or any other ownership
uncertainty also protects it.

Status inspection uses `--ignore-submodules=none`, so repository configuration
cannot hide submodule changes. A worktree with populated submodules remains
protected even when otherwise clean because its per-worktree Git directories
can contain private refs, reflogs, and objects. If only its prunable
administrative record remains, that record protects the repository-wide prune
scope for the same reason. Ignored compiler/dependency output does not make a
worktree dirty, but its paths and allocated bytes are reported because
`git worktree remove` will reclaim it. Apply uses only ordinary non-force
worktree removal and preserves local and remote branch refs and Git objects.

Profile builds are eligible only as direct
`workspace/build/profiles/<profile>-<key>` children with an exact regular
ownership marker. Each build use atomically refreshes strict metadata with its
profile/key, selected repository/branch/checkout/source coordinates, commits,
and `last_used_at`. A marker-proven build created before that metadata uses the
maximum no-follow tree mtime as a conservative legacy age. A build selected by
a live topology, busy build lock, registered worktree, or the optional strict
`workspace/build/retention.json` record is protected. That retention record is
schema 1 and contains exactly `schema_version` plus an absolute `build_roots`
array. A build sourced from a worktree eligible in the same plan may be
reclaimed regardless of age unless another protection applies.

The shared `workspace/build/npm-cache` is never part of default cleanup. Its
explicit scope accepts the exact marker-owned path or the one legacy known
cache at that fixed location after proving the workspace marker, path shape,
age, and absence of an active build. Unmarked profile roots, other
`workspace/build` children, and all remaining mixed top-level `build/`
entries are report-only `unmanaged-build` items. Deep-review reports, ad hoc
builds, packages, archives, and unregistered siblings are never recursively
deleted.

Apply holds the repository-layout lock and performs one complete inventory and
size recomputation. Immediately before each removal it freshly revalidates the
target's safety dependencies without rescanning unrelated report-only payloads.
Any new uncertainty fails closed. It removes eligible builds first, exact Git
worktrees second, and the explicitly selected cache or safely shared prunable
Git metadata last. A pre-mutation race aborts without deletion; after the first
successful mutation, the deterministic policy stops on the first error and
reports exactly what was reclaimed without claiming rollback.

## Composing coherent component sources

A profile retains a stack identity and chooses one physical checkout root for
every logical component owned by that checkout. Start a currently runnable
profile from the built-in `classic` base; start replacement-only source
composition from `default`. Overrides may change a provider's source checkout,
but they cannot replace it with a provider from the other stack or leave a
required logical role ambiguous. Commands accept an in-stack component
identity or its logical role, so role `client` resolves to `classic-client` in
a classic-derived profile. Selecting checkout `classic`, any `classic-*`
component, or one of its roles updates all five classic selectors together.

~~~sh
./atrinik profile create maps-review --from classic
./atrinik profile set maps-review content-1x --worktree maps-pr
./atrinik profile set maps-review classic-server --worktree socket-review
./atrinik profile show maps-review
./atrinik build all --profile maps-review --test
~~~

The `classic-server` alias above has the same checkout-wide effect as naming
`classic`: every classic logical component comes from `socket-review`.
Resolution then returns the appropriate `server/`, `client/`, `editor/`,
`libatrinik/`, or `protocol/` source root for each component. Profile schema 3
rejects different selectors for logical components that share one physical
checkout.

Clone an existing profile when starting a related combination instead of
repeating every selector:

~~~sh
./atrinik profile create maps-review-2 --from maps-review
~~~

An existing checkout outside the managed workspace can also be selected:

~~~sh
./atrinik profile set maps-review classic-client \
  --path /absolute/path/to/classic-checkout
~~~

The `classic-client` alias still updates all five classic selectors. The path
is always a Git checkout root, never a component subdirectory. The repository
identity is validated for every selector, and the manifest's safe `source`
path is appended only after validation. Generated build trees are keyed by
profile schema, stack generation, exact repository/branch/checkout/source and
role-provider identities, and resolved paths, so classic and replacement output—and
different combinations of distinct physical checkouts—cannot overwrite one
another. Scenario and topology records retain those same coordinates; older
records without them are inert.

Migration may retain a proven classic content worktree using an internal
`migrated-worktree` selector. `profile set` cannot create that selector (while
cloning an already migrated profile preserves it): resolution accepts it only
for `content-1x`, only at its original managed
`workspace/worktrees/content/<label>` path, and only while it remains attached
to the canonical content Git directory. New worktrees use ordinary
`content-1x` selectors. Normal `content@main` worktree synchronization always
skips these migration-only paths.

Print an exact selected logical source path for use with `cd`, editors, or
other tools. Read-only profile, worktree, and state listings also support
`--json`.

~~~sh
cd "$(./atrinik path classic-server --profile maps-review)"
./atrinik profile show maps-review --json
./atrinik state list --json
~~~

`path classic-server` prints the resolved `server/` source root. Use checkout
commands such as `status`, `sync`, and `worktree` when the full repository root
is required.

Before every server build or launch, the coordinator collects the selected
content checkout into an isolated runtime tree and stages the selected resource
repository. Only tracked files below the resource repository's
`runtime-paths.txt` allowlist are staged; development metadata and untracked
files cannot become server assets. Client builds similarly expose the selected
sound checkout. These operations never write generated files into a component
checkout.

## Deterministic test scenarios

A scenario is a local, ready-to-login account and character plus its own
registered server state. It is useful for handing off an issue reproduction
without asking every reviewer to register another account and character.

~~~sh
./atrinik scenario create issue-42 --profile maps-review --preset basic-player
./atrinik scenario show issue-42 --json
./atrinik scenario credentials issue-42
~~~

`basic-player` provisions a normal `human_male` first-login character through
the selected server's own account API. On first login the server supplies its
configured starting map, standard skills, and initial items. The wrapper builds
the selected server, creates the dedicated `scenario-issue-42` state, stores a
generated password in a mode-0600 ignored file, and prints exact
`topology show`/`up`/`ps`/`logs`/`down` commands. `show` and `list` never reveal
the password; request it explicitly with `credentials` immediately before
login.

Scenarios live below ignored `workspace/scenarios/`. Their metadata records the
profile plus every resolved provisioning dependency's checkout, source root,
path, commit, and dirty status. Account files contain the normal
Argon2id password record, and the reserved empty player file deliberately
causes the first client login to use normal character initialization. The
offline provisioner starts no listener, plugin, metaserver, or console and
refuses existing account or character identities.

After stopping the topology, reset only the scenario-owned mutable state while
keeping its login and source profile stable:

~~~sh
./atrinik down issue-42
./atrinik scenario reset issue-42
~~~

Reset refuses a running or otherwise locked state and validates the ownership
marker, state registration, state shape, and credential permissions before it
replaces anything. External and shared states are never scenario reset targets.
Do not create static account or player fixtures; add a tested server-owned
preset if a future reproduction needs more than `basic-player`.

## Supervised topologies and practical workflows

A profile is a declarative source topology. The `topology` and process-lifecycle
commands turn that selection into a Compose-like native development stack:

~~~sh
./atrinik topology show PROFILE
./atrinik up --name TOPOLOGY --profile PROFILE --state STATE [--port UDP_PORT]
./atrinik ps [TOPOLOGY]
./atrinik logs TOPOLOGY [server|client] [--follow]
./atrinik down TOPOLOGY
~~~

`up` resolves the selected providers for the `client`, `server`, `content`,
`protocol`, `libatrinik`, `sound`, and `resources` roles automatically.
Provider selection is stack-coherent: a classic service never binds a
replacement protocol or `content@main`. Today the `classic` stack is the
runnable implementation; the `default` replacement profile will become
runnable as its component contracts land. For a runnable profile, `up` builds
the required closure, collects content, stages resources/sound, prepares an
isolated runtime, and starts both game processes under one supervisor. A server
gets an available UDP port by default; use `--port` when a stable port is
useful. The supervisor waits for both the QUIC certificate fingerprint and
completed server initialization, then gives the paired client an authenticated
loopback endpoint, disables metaserver and STUN discovery in that client, and
disables STUN discovery and automatic port mapping in the server before
declaring the topology ready. Use
`--service server` or `--service client` for a single service. Client startup
requires a live forwarded display socket.

The supervisor records exact source commits, build and state paths, and process
start identities. `ps` without a name lists every recorded topology; a name
selects one. It distinguishes a live process from a reused PID, `down` signals
only the matching supervisor, and the server state remains locked for the
complete supervised lifetime. Each topology also has a persistent isolated
client configuration/cache root and its own collected content and resource
snapshot, so a subsequent build cannot replace files underneath a running
topology. Logs live below `workspace/topologies/` and rotate at 10 MiB with
three backups.

### Use case: run the latest playable classic branches

Use this after first cloning the wrapper, or when returning to development and
wanting a known-current baseline:

~~~sh
cd ~/atrinik
./atrinik init
./atrinik init --with classic
./atrinik sync --with classic
./atrinik topology show classic
./atrinik up --name classic-main --profile classic --state default
./atrinik ps classic-main
./atrinik logs classic-main server --follow
# Press Ctrl-C to stop following logs; the services keep running.
./atrinik down classic-main
~~~

This is useful for ordinary playtesting and for proving that a problem also
exists without any feature worktree. The initial replacement-only `init` is
shown separately to emphasize that `--with classic` adds to it. `sync` refuses
dirty primary repositories and never initializes an absent checkout, so it
cannot silently overwrite work or expand the workspace.

### Use case: develop the classic server in a monorepo worktree

Create one full classic repository worktree and select it for the entire
classic checkout. The branch may change only `server/`, but all five logical
components resolve from the same branch:

~~~sh
./atrinik worktree create classic socket-fix \
  --branch fix/socket-timeout
./atrinik profile create socket-fix --from classic
./atrinik profile set socket-fix classic --worktree socket-fix
./atrinik topology show socket-fix
./atrinik state add socket-fix
./atrinik up --name socket-fix --profile socket-fix --state socket-fix

cd "$(./atrinik path classic-server --profile socket-fix)"
git status
# Edit, test, commit, and push here as in any normal Git repository.

cd ~/atrinik
./atrinik logs socket-fix server --follow
./atrinik down socket-fix
~~~

This is useful for a server-focused pull request: the wrapper resolves
`server/` and every related classic source from one coherent monorepo commit,
then rebuilds the affected dependency closure. Unchanged source directories in
that worktree naturally match the branch's base commit; they are not selected
from a second classic checkout.

### Use case: combine client, server, and content pull requests

Client and server pull requests now belong to the same `atrinik/classic`
repository. Fetch both, combine them into one monorepo branch and worktree,
then select that checkout alongside the independent content worktree. A profile
cannot select two worktrees for one physical checkout. Substitute the actual
pull-request numbers:

~~~sh
git -C classic fetch origin pull/CLIENT_PR/head:review/client-ui
git -C classic fetch origin pull/SERVER_PR/head:review/server-combat
git -C content-1x fetch origin pull/CONTENT_PR/head:review/content-maps

./atrinik worktree create classic combined-classic \
  --branch review/combined-classic --from review/server-combat
./atrinik worktree create content-1x maps-review \
  --branch review/content-maps --existing

./atrinik profile create combined-review --from classic
./atrinik profile set combined-review classic --worktree combined-classic
./atrinik profile set combined-review content-1x --worktree maps-review

CLASSIC_SOURCE="$(./atrinik path classic-server --profile combined-review)"
git -C "$CLASSIC_SOURCE" merge --no-edit review/client-ui

./atrinik topology show combined-review --json
./atrinik state add review
./atrinik up --name combined-review --profile combined-review --state review
./atrinik ps combined-review --json
./atrinik logs combined-review --follow
./atrinik down combined-review
~~~

Resolve and commit any merge conflicts before building. Git accepts the
resolved `server/` source as `-C` because it belongs to the full classic
worktree; the merge still combines the entire repository. Content is collected
from its separate worktree before launch, while all classic source roots come
from the one combined commit. The JSON topology output is an exact
checkout/source bill of materials for review notes or agent automation.

### Use case: review new maps with unchanged binaries

Keep the classic client and server on their synchronized primary checkouts and
select only the `content-1x` worktree:

~~~sh
./atrinik profile create map-check --from classic
./atrinik profile set map-check content-1x --worktree maps-review
./atrinik topology show map-check
./atrinik state add map-check
./atrinik up --name map-check --profile map-check --state map-check
./atrinik logs map-check server --follow
./atrinik down map-check
~~~

This is useful when a map/content pull request needs a real server and client
but should not be tested against unrelated native branches. Automatic
collection means there is no manual copy step and no generated output is
written into the content worktree.

### Use case: reuse one persistent server world across worktrees

Register an existing server-data directory once, then select it by name from
any topology:

~~~sh
./atrinik state add shared --path /workspaces/atrinik/server-data
./atrinik up --name combined-review --profile combined-review --state shared
./atrinik down combined-review
./atrinik up --name classic-main --profile classic --state shared
./atrinik down classic-main
~~~

This is useful for comparing a feature topology with main using the same
accounts and world state. Run them sequentially: the coordinator deliberately
refuses to start two servers against the same state directory at once.

### Use case: compare two source combinations safely

Clone a profile, change one selector, inspect both bills of materials, and run
them one after the other against the same named state:

~~~sh
./atrinik profile create candidate-b --from combined-review
./atrinik profile set candidate-b classic --primary

./atrinik topology show combined-review --json
./atrinik topology show candidate-b --json

./atrinik up --name combined-review --profile combined-review --state shared
# Exercise the first combination.
./atrinik down combined-review

./atrinik up --name candidate-b --profile candidate-b --state shared
# Repeat the same exercise with the primary classic checkout.
./atrinik down candidate-b
~~~

Separate path-keyed build trees prevent the two combinations from overwriting
one another, while the common state makes the comparison repeatable.

### Use case: run classic and replacement topologies at the same time

Once the replacement components' wrapper build/runtime adapters and integrated
service closure land, use distinct profiles, topology names, server states, and
ports to run `classic` beside `default`. This is the required isolation
pattern; today only the classic command is runnable:

~~~sh
./atrinik state add classic-world
./atrinik state add replacement-world

./atrinik up --name classic-side --profile classic \
  --state classic-world --port 17300
# Available after replacement runtime contracts land:
./atrinik up --name replacement-side --profile default \
  --state replacement-world --port 17301

./atrinik ps
./atrinik logs classic-side server --follow
./atrinik logs replacement-side client --follow

./atrinik down classic-side
./atrinik down replacement-side
~~~

This is useful for side-by-side regression checks, protocol experiments, and
reviewing two pull-request combinations without stopping either one. Each
client is pinned to its own server fingerprint and has separate settings and
caches. Omit both `--port` options to have the coordinator choose two available
ports. Two live servers may not use the same state directory; the state lock
turns that mistake into an immediate error instead of mutable-data corruption.

### Use case: run only a headless server

In a terminal without display forwarding, start only the server and inspect it
from another shell:

~~~sh
./atrinik up --name combined-review --profile combined-review \
  --state review --service server
./atrinik ps combined-review
./atrinik logs combined-review server --follow
./atrinik down combined-review
~~~

This is useful for protocol, content, runtime, and server work where opening a
graphical client would be distracting or impossible.

### Use case: clean up after a completed review

Stop the topology before removing checkout worktrees. Removal refuses dirty
worktrees, so commit, stash, or otherwise preserve intentional edits first:

~~~sh
./atrinik down combined-review
./atrinik profile set combined-review classic --primary
./atrinik profile set combined-review content-1x --primary
./atrinik cleanup --scope worktrees --scope builds \
  classic content-1x --older-than 7 --dry-run
./atrinik cleanup --scope worktrees --scope builds \
  classic content-1x --older-than 7 --apply
~~~

The saved `combined-review` profile protects both selected worktrees until the
explicit `profile set` commands repoint it. Do that only when the retained
review selection is no longer useful, then preview cleanup. Cleanup never removes profiles,
scenarios, topology records/logs, state, migration evidence, commits, or
branches.

## Persistent server state

The built-in state named `default` is initialized once at
`workspace/state/server/default` and then reused by every profile. Register a
second managed state or an existing compatible directory when isolation is
useful:

~~~sh
./atrinik state add review
./atrinik state add existing --path /absolute/path/to/server-data
./atrinik state list
./atrinik run server --profile maps-review --state review --port 1730
# In another terminal, after the server reports that it is ready:
./atrinik run client --profile maps-review --state review --port 1730
~~~

State contains player, key, unique-item, and other mutable runtime data. It is
never regenerated over an existing directory. A nonblocking lock prevents two
coordinator-launched servers from using the same state simultaneously.
Repository migration never moves, rewrites, or retags an existing state; state
ownership evolution is a separate migration contract.

Arguments after `--` are forwarded to the selected executable:

~~~sh
./atrinik run server --profile maps-review --state review -- --version
./atrinik run client --profile maps-review --state review -- --help
~~~

The foreground commands are a paired local-development path: use the same
`--state` and `--port` in both terminals. The server always starts with
automatic port mapping and STUN discovery disabled. The client reads the
state's generated QUIC certificate, adds its authenticated loopback endpoint,
and starts with metaserver and STUN discovery disabled. Start the server first
so that its persistent `quic-identity.pem` exists. Additional arguments are
appended after these defaults, and join-password arguments are redacted from
command logging. Prefer `up` for routine use because it allocates a port,
captures the fingerprint, and sequences both processes automatically.

## Workspace location and recovery

Set `ATRINIK_WORKSPACE_DIR` to an absolute path before invoking the command to
place generated worktrees, profiles, builds, and state elsewhere:

~~~sh
export ATRINIK_WORKSPACE_DIR=/workspaces/atrinik-data
./atrinik init
~~~

Primary physical repositories remain directly beside the wrapper even when
this variable is set. The coordinator marks directories it owns and refuses to
replace unmarked paths. Component Git history is authoritative; profiles are
small ignored JSON files and can be recreated. Back up persistent state
separately. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for data-flow and safety details.

## Validation

~~~sh
python3 -m pip install --requirement requirements-dev.txt
python3 -m coverage run -m unittest discover -v
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
./atrinik manifest validate
./atrinik cleanup --scope all --older-than 7 --dry-run --json
~~~

Use `./atrinik build COMPONENT --profile classic --test` for current native
component integration. Replacement repositories run their standalone
aggregate validations today; wrapper builds still fail with a clear
unavailable-contract error until their adapters land. The `metaserver-worker`
wrapper contract runs its complete `npm run check` suite.
