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
checkouts directly beside this README (`./client`, `./server`, `./classic`,
`./observatory`, `./deploy-control`, `./web-platform`, and so on). The `atrinik/classic` checkout at `./classic`
contains the logical
`classic-client`, `classic-server`, `classic-editor`,
`classic-libatrinik`, and `classic-protocol` source roots. Generated worktrees,
builds, profiles, and default state live under the ignored `workspace/`
directory.

## Requirements

- Python 3.11 or newer and Git
- Native Windows support uses Python's Windows path handling and PowerShell or
  Command Prompt; WSL and the devcontainer are not required for the supported
  wrapper surface below
- The GitHub CLI (`gh`) authenticated for GitHub operations
- CMake, Ninja, and the dependencies required by the selected component when
  building the classic native stack
- Node.js 20 or newer when building `metaserver-worker`

### Native Windows wrapper support

The coordinator can be invoked directly from a native Windows checkout with
`python .\atrinik ...`. The supported Windows surface includes CLI import and
help, manifest/provenance/supply-chain validation, replacement or Classic
initialization and synchronization, repository status, profile inspection and
publication, path resolution, and worktree inventory. For example:

~~~powershell
python .\atrinik manifest validate
python .\atrinik init --with classic
python .\atrinik status --json
python .\atrinik profile show classic --json
~~~

Native Windows locks use the kernel `LockFileEx` API, and child Git processes
receive the active lock handles. JSON publication uses an atomic replacement
and `FlushFileBuffers`; path and junction components are rejected. Windows
does not expose the POSIX descriptor-relative directory and process-identity
facilities used by the wrapper's supervised topology, build publication,
cleanup, migration, state, scenario, and direct-run workflows. Those commands
return a stable capability diagnostic naming Linux/WSL2 or the documented
Windows package workflow rather than weakening locking, cleanup, or topology
isolation. The complete support matrix is maintained in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

### Issue/PR delivery coordinator

Native Windows is a supported host for editing, native Git and GitHub UI, and
native D3D12 validation. It is not the authoritative coordinator for the
issue-delivery ledger because the ledger requires Linux/POSIX locking,
descriptor and mount identity, and durable no-follow filesystem proofs. Open
the ordinary pinned Linux devcontainer and run this read-only probe before
initializing or mutating delivery evidence:

~~~sh
python3 scripts/atrinik_coordinator_context.py --json
~~~

Continue only for `canonical-linux` with `authoritative: true`. The probe also
recognizes `native-windows`, `windows-cross`, and `unknown-or-unsafe` with a
bounded next action. The probe reports an entry mode as a diagnostic, but the
authoritative result comes only from the complete pinned identity, ownership,
workspace, ledger, Codex-home, and live-mount contract.

#### Codex entry modes

Delivery supports exactly two Codex entry modes:

- **Already inside the canonical VS Code devcontainer:** continue in the
  current plugin process, workspace, ledger root, worktree, and warm caches.
  Do not invoke Docker or the Dev Containers CLI merely to create, attach,
  recreate, remount, or re-enter another container.
- **Native-host bootstrap:** before delivery work, enter or attach to the
  pinned ordinary Linux devcontainer with Docker or the Dev Containers CLI.
  The native host may perform only that minimum bootstrap/attach and approved
  Git/GitHub/commit operations. Wrapper/context, ownership, repository and
  worktree setup, ledger locks/CAS/leases/recovery, edits, tests, builds,
  review, and validation all run inside the container.

In both modes, Codex must never launch or control VS Code, invoke `code` or
`code.cmd`, send a VS Code URI, or use GUI automation. VS Code setup text in
this README is for a human developer, not an agent handoff. A persistent
session is reusable only while its owner, pinned image, current
container/mount identity, exact worktree, and delivery-ledger coordinates
match. A secret-free session record may make those facts visible, but it never
grants authority. Reconnect or crash recovery reruns the probe, exact
worktree/ledger observation, CAS, and leases before continuing. Bound idle and
shutdown operations to the owned session, and give parallel sessions distinct
worktrees, leases, caches, credentials, ports, and mutable state.
Copied or stale session markers, arbitrary containers, nested coordinators,
and unsafe bind mounts never grant authority. Keep the `windows-cross` container for
package/build work and host-bound validation.

#### Agent-owned persistent sessions

A session is one agent-owned container plus its exact, live identity; it is
not a name, a copied marker, or a permission token. Keep a small ignored
record at `build/sessions/<delivery-slug>.json` when a delivery needs
continuity. The record is corroboration only and must contain no
credentials, private keys, access tokens, or mutable server data. Record the
agent identity, delivery scope and ledger, checkout/worktree and branch,
profile, container name and ID, pinned image digest, source mounts and live
identities, named volumes and targets, start/last-activity times, idle
deadline, active services, and cleanup owner.

A native host bootstraps once, then keeps using the returned container. It may
run only bootstrap/attach, exact identity inspection, and approved
Git/GitHub/commit operations. Require one exact active container row; ambiguity
fails closed. After selecting its exact ID, run the coordinator and wrapper
commands inside that container:

~~~sh
HOST_REPO="$(pwd)"
devcontainer up --workspace-folder "$HOST_REPO"
docker ps --filter "label=devcontainer.local_folder=$HOST_REPO" \
  --format '{{.ID}}\t{{.Names}}'
CONTAINER_ID=THE_EXACT_ID_FROM_THE_LIST
docker exec --workdir /workspaces/atrinik "$CONTAINER_ID" \
  python3 scripts/atrinik_coordinator_context.py --json
docker exec --workdir /workspaces/atrinik "$CONTAINER_ID" \
  ./atrinik worktree list --wrapper-self --json
~~~

Do not pass a remove-existing option or invoke bootstrap for every command.
When already inside the canonical container, keep the current process, shell,
worktree, ledger root, leases, and named build volume; do not start another
container. On reconnect, inspect the exact ID/name/image/status and rerun the
coordinator probe, wrapper worktree list, ledger `inventory`, fresh CAS
proofs, and leases. If the container stopped or disappeared, preserve the
worktree, report, ledger, and exact volumes; recover once with the pinned
configuration only after the old container is proven stopped and all
coordinates are re-proven. Stale metadata never authorizes recovery.

Set an idle deadline of 30 minutes and a maximum lifetime of 12 hours by
default; record UTC `last_activity_at` and `idle_deadline`. A build
lease keeps active work from being reclaimed but does not make a session
immortal. Only the owner may stop an idle session, and an abandoned session
is retained for fresh liveness and lease checks. Parallel sessions may share
immutable image layers and read-only inputs, but must use distinct exact
worktrees, delivery ledgers/coordinates, profiles and build roots, named
volume namespaces, Codex homes or credentials, topology/state names, ports,
and mutable caches.

For shutdown, finish or preserve the delivery evidence, then stop only the
owned exact container:

~~~sh
docker stop --time 10 "$CONTAINER_ID"
docker inspect "$CONTAINER_ID" --format '{{.Id}}\t{{.State.Status}}'
~~~

Leave exact named volumes for an authorized owner to inspect or remove after
all holders and leases are gone. Never use docker volume prune, broad
container cleanup, or ./atrinik cleanup --apply during delivery.

The Atrinik development container supplies the native build dependencies. Run
all commands below from this repository's root.

### Development container

For a human developer, open this wrapper repository in VS Code and choose
**Dev Containers: Reopen in Container** to use the pinned Linux build
environment. Codex does not perform that GUI action or ask another VS Code
session to reopen the container. On first creation, the container runs
`./atrinik init`; it clones only missing replacement/default
repositories and validates existing checkouts without updating or replacing
them. It never adds classic repositories, the MIT playtester, or the
MIT-by-default tools repository with its GPL-2.0-or-later `map-checker-qt/`
exception
implicitly, and never touches a retained historical `content@1.x` path. The Windows cross-build configuration is available at
`.devcontainer/windows-cross/devcontainer.json` after the required component
checkouts have been initialized.

The wrapper owns these launch configurations because they compose the complete
development workspace. The standalone `devcontainer` component owns only the
published Linux and Windows toolchain images they reference.

#### Docker storage topology

The ordinary pinned Linux configuration keeps live source checkouts, registered
worktrees, `workspace/state`, and the trusted `build/reviews` delivery ledger on
their Linux-native bind mounts. It mounts `workspace/build` as the named volume
`atrinik-${devcontainerId}-build-cache` with `volume-nocopy`. The host-side
`workspace/build` parent is created before the nested mount, and the one-time
`onCreateCommand` repairs the fresh volume root to the remote user's ownership.
The wrapper still discovers `workspace/build` through `Paths`, records its normal
markers and leases there, and applies preview-first cleanup to marker-owned
contents; the volume itself is never guessed or removed by a broad cleanup.

The Windows cross-build configuration uses the same per-container workspace
volume. Its Docker package fallback deliberately keeps the private immutable
source staging root and final package output on bind mounts. It attaches separate
namespaced named volumes for client/server CMake trees, ccache, and dependency
downloads, initializes their writable roots before the non-root build, and
records their exact names in the package build metadata. No source checkout,
registered worktree, server state, credential, or delivery ledger is copied into
a cache volume.

Volume names use the Dev Container identity through
`ATRINIK_DOCKER_VOLUME_NAMESPACE`; parallel containers therefore do not share
mutable build state. Stop all containers using a volume before removing that
exact volume. Wrapper cleanup remains preview-first for its marker-owned
contents, while volume removal is an explicit operator action after the
container lease is gone.

The repeatable synthetic comparison records environment, pinned image,
namespace, cold/warm bind and volume timings, interruption/cache-reuse
observations, host export checksums, and exact cleanup results:

~~~sh
python3 scripts/benchmark_devcontainer_storage.py \
  --image ghcr.io/atrinik/windows-build:1.2.1@sha256:d1f082eb28891600a9cf018a1d4310b9f3e1f985f82139fa48fbd4ac77b623bb \
  --output build/storage/windows-docker.json
~~~

The helper uses bounded deterministic input and refuses pre-existing benchmark
volumes. Use `--keep-volumes` only when the exact reported volume is needed for
manual inspection. The companion session benchmark measures repeated cold
starts, warm `docker exec` reuse, forced-stop recovery, and independent
parallel sessions while recording Docker Desktop/client/server environment
facts:

~~~sh
python3 scripts/benchmark_devcontainer_session.py \
  --output build/storage/devcontainer-session-benchmark.json
~~~

It creates only exact run-id-scoped benchmark containers and named volumes,
does not mount source, credentials, or server state, and removes its exact
temporary volumes unless `--keep-volumes` is requested.

For delivery work on a Windows host, the ordinary configuration is the
coordinator. Its source/worktree mount, `build/reviews` ledger root, wrapper
workspace, and `/home/ubuntu/.codex` mount must remain Linux-native or backed
by a trusted Docker volume with safe ownership, modes, and live mount identity.
The wrapper probe checks those facts without creating or repairing them; the
persistent-session and high-I/O volume topology described by #538/#543 must
preserve the same ledger and cache boundaries.

The workspace and container VS Code settings exclude the wrapper-owned
`workspace/` and top-level `build/` trees from recursive file watching, and the
root Pyright configuration excludes them from Python analysis. Primary
component checkouts beside the wrapper remain indexed. Open a managed worktree
under `workspace/worktrees/` as its own VS Code folder or window when it needs
language-service coverage.

### Shell completion

Activate native completion for the current Bash, Zsh, or Fish session without
changing a startup file:

~~~sh
# Bash (the same command works after putting atrinik on PATH)
source <(./atrinik completion bash)

# Zsh, after compinit is available
autoload -Uz compinit && compinit
source <(./atrinik completion zsh)

# Fish
./atrinik completion fish | source
~~~

For persistent per-user activation, create the shell's normal completion
directory yourself and write the deterministic output to its native file:

~~~sh
# Bash
./atrinik completion bash > ~/.local/share/bash-completion/completions/atrinik

# Zsh (ensure ~/.zfunc is in fpath before compinit)
./atrinik completion zsh > ~/.zfunc/_atrinik

# Fish
./atrinik completion fish > ~/.config/fish/completions/atrinik.fish
~~~

The wrapper never creates those directories or edits shell configuration.
Completion works for `./atrinik`, an `atrinik` command on `PATH`, and absolute
invocation paths. Commands, nested commands, exact options, mutually exclusive
flags, and static parser choices come from the live `argparse` tree. Local
manifest identities, built-in and saved profiles, owning-checkout worktree
labels, states, scenarios, and recorded topologies are reread on every keypress.
Filesystem values such as `--path` and `--output` remain native shell path
completion, and wrapper suggestions stop after the `run client|server --`
forwarding boundary.

The provider reads only bounded local metadata and fails quietly to static
parser candidates when optional state is absent or malformed. It does not
construct a `Workspace`, inspect Git, probe processes, read scenario password
files, or use the network. A development-container benchmark can be repeated
with the private adapter protocol below; measure one cold invocation, then the
median of repeated fresh processes after filesystem caches are warm:

~~~sh
time ./atrinik __complete 1 -- atrinik "" >/dev/null
for run in $(seq 1 30); do
  time ./atrinik __complete 1 -- atrinik "" >/dev/null
done
~~~

On the Python 3.14 development container used for this implementation, the
top-level candidate query measured 63 ms cold and 61 ms warm median. The
protocol is internal to the generated adapters; scripts should be obtained
through `completion bash|zsh|fish` rather than hand-written against it.

### MCP information-access contract

[`mcp/contract/v1`](mcp/contract/v1/README.md) defines the common, versioned
safety and measurement gates for future Atrinik MCP servers and evaluated
connectors. It pins exact coordinates, stable failures, pagination, cache
identity, hard record/byte/time/context limits, six known-answer domains, an
adversarial corpus, and build/configure/defer/reject decisions. This repository
does not yet ship or configure a production MCP server; direct wrapper,
repository CLI, `rg`, Git, `gh`, and browser workflows remain authoritative.

Validate or benchmark the contract without installing an MCP SDK:

~~~sh
python3 -m atrinik_workspace.mcp_contract validate
python3 -m atrinik_workspace.mcp_contract benchmark \
  --iterations 30 --output build/mcp/benchmark.json
~~~

The benchmark defaults offline and writes sanitized evidence under ignored
`build/`. See [the safety and measurement contract](docs/MCP_INFORMATION_ACCESS.md)
for bounds, threat coverage, capability ownership, optional live-GitHub
measurement, and downstream consumer gates.

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
a supported dependency path. The shared `deploy-control` checkout owns its
Worker/agent package lock, action pins, runner contract, and release/deployment
inputs; the wrapper records those boundaries without copying its implementation,
credentials, bindings, or mutable host state.

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
repository and branch identity before reading it. A review branch is accepted
only when its linked worktree shares the expected checkout primary's common Git
directory, so an unrelated or historical checkout cannot impersonate
`content@main`.

### Historical MIT provenance grants

[`docs/PROVENANCE.md`](docs/PROVENANCE.md) is the single exhaustive approved
grantor registry. The canonical privacy-preserving identity schema, public
registry and operating policy are in
[`governance/provenance-identities/`](governance/provenance-identities/) and
[`docs/PROVENANCE_IDENTITIES.md`](docs/PROVENANCE_IDENTITIES.md). Component
repositories keep exact material scope and an immutable pinned reference; they
never duplicate aliases or canonical identity records.

The cross-repository M1 evidence and exact reuse procedure are indexed in
[`docs/REPLACEMENT_FOUNDATIONS.md`](docs/REPLACEMENT_FOUNDATIONS.md). The
machine record maps every fresh repository to its local CI, provenance,
dependency, notices, packaging, SBOM, and mixed-license gates without making
this coordinator a second copy of their implementation policy.

The Classic repository remains GPL-2.0-or-later, but that is not a blanket
source-reuse ban. Exact, independently separable material proven to fall within
an applicable approved historical grant—including that row's temporal and
sole-original-authorship scope—may be inspected as implementation reference,
copied, migrated or ported, translated or adapted, and MIT-relicensed after the
canonical complete-history, identity, separability, embedded-material, and
destination-record review. Source-informed work from qualifying material is
provenance-approved reuse, not clean-room work. Later material needs
contemporaneous compatible permission. The source license and unrelated
dependencies, assets, and surrounding work do not change; destination evidence
cites the exact wrapper revision containing the registry used.

Validate the canonical registry and one or more component references through a
bounded local checkout. This reads only the pinned registry/schema Git blobs and
does not use the network:

~~~sh
./atrinik provenance validate --reference PATH
~~~

The MIT-by-default tools repository, its GPL-2.0-or-later `map-checker-qt/`
exception, and user-facing replacement/retirement paths are inventoried in
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
`content` from `atrinik/content@main`; compatible shared resources, sound,
metaserver code; the source-only shared `atrinik/deploy-control` checkout for
organization infrastructure; and required development infrastructure. It does
not clone
`atrinik/classic`, `atrinik/playtester`, or the MIT-by-default `tools`
repository with its GPL-2.0-or-later `map-checker-qt/` exception, and
does not touch a retained historical `atrinik/content@1.x` checkout.
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
./atrinik up --name classic-local --profile classic --temporary-state
./atrinik ps classic-local --json
./atrinik logs classic-local server --follow
# Press Ctrl-C to stop following logs; the services keep running.
./atrinik down classic-local
~~~

`build all` configures the selected Classic monorepo through its root CMake
project, so protocol and libatrinik are compiled once and shared by the client
and server. A component-specific build such as `build client` or `build server`
continues to exercise that module's supported standalone FetchContent path.

### Incremental Classic development

Use the development workflow when iterating on a playable Classic service:

~~~sh
./atrinik dev build --profile classic --services server,client
./atrinik dev build --profile classic --services server --json
./atrinik dev up --profile classic --name classic-local \
  --services server,client --temporary-state
./atrinik dev restart classic-local --service server
./atrinik dev restart classic-local --service client
./atrinik down classic-local
~~~

`dev build` and `dev up` resolve the complete client/server dependency closure
and use the same topology build root, so a warm direct development build warms
the paired launch. `--services server`, `--services client`, and
`--services both` select the playable service set; selecting one service limits
the CMake build targets and does not rebuild the other service. Content,
resources, region maps, source views, CMake configure state, and the shared
compiler cache report `reused` or `refreshed` decisions in JSON output. Add
`--test` when the selected development build should materialize the configured
CMake graph and run its CTest validation; this explicit mode may build both
playable services so the unfiltered suite has all of its targets. The
metaserver worker is not part of this playable scope; use the explicit
`build all --profile classic --test` workflow for full validation. JSON output
also includes elapsed build time and per-CMake configure/build/test timings for
the cold/warm and service-only rebuild matrix.

`dev up` keeps local paired launch offline: the server disables port mapping and
STUN, and the client disables the metaserver and STUN while connecting to the
reserved loopback endpoint. `dev restart` requires a currently supervised
topology, performs a controlled stop, rebuilds only the requested service
target, and publishes a fresh immutable runtime generation. Persistent state
and the reserved endpoint port are retained; disposable temporary state is
copied into the new generation and the superseded copy is removed only after
the replacement is ready. The topology may therefore restart both supervised
processes as one sealed-generation transaction while keeping compilation
service-selective and preserving the state/port contract.

Managed source views are reconciled in place, so unchanged links and copied
files retain their identity while changed, retargeted, and stale entries are
updated without following unsafe symlinks. Linked-directory structure and
symlink targets participate in view identity; a dirty Git source conservatively
runs configure on every build. CMake configuration is skipped only when the
source-view state and a fingerprint of the generator, CMake/compiler
toolchain, cache arguments, test mode, and relevant environment still match.
Ninja may still run CMake's dependency regeneration during the build. Use
`--force-reconfigure` to run the explicit configure step regardless. A skip
also requires the expected CMake cache and Ninja graph to identify the current
source and generator. Compiler, toolchain-file, or initialization-environment
changes safely reinitialize only that marker-owned binary tree while retaining
the shared compiler cache.

When `ccache` is available, native C and C++ builds automatically use the
marker-owned shared `workspace/build/compiler-cache`, bounded at 5 GiB. Debug
and source prefixes are normalized across equivalent profile roots when the
selected environment compiler proves support for the required prefix-map
switches. Opaque toolchain-selected compilers retain their own flag contract
instead of receiving incompatible GCC-style switches, and retain directory
hashing so objects with profile-specific debug paths cannot cross profile
roots. Pass `--no-ccache` for an explicit uncached build; an unavailable cache
command is reported with the same opt-out guidance.

Building the Classic server also runs its offline worldmaker and stages the
generated region-map `.png`/`.def` pairs in the profile build. Generation uses
the selected Classic, shared `content@main`, and resource views, so it requires the same
native build dependencies as the server. A marker-owned cache is reused only
while every selected checkout is clean and its recorded commit still matches;
dirty inputs regenerate on every build. Missing `incuna_-1` output, incomplete
pairs, malformed files, or generator failure stop the build while preserving
the last valid cache.

Server launch uses a disposable `assets` staging root with a writable `data`
directory and the selected generated `client-maps`. Generated game data never
needs to exist in, copy into, or overlay a registered state. A supervised
topology copies the maps into its owned runtime snapshot before the server
takes its immutable asset snapshot. QUIC serves that snapshot by default;
`http_url` only advertises an optional operator-managed HTTP(S) origin. Region
maps are reclaimed with their marker-owned profile build by the normal
preview-first `./atrinik cleanup --scope builds` lifecycle; topology snapshots
remain with the retained topology record and are atomically replaced the next
time that topology name is launched.

Building `metaserver-worker` retains a validated dependency installation keyed
by exact package and lockfile bytes, optional project `.npmrc`, Node/npm, Node
runtime and host platform identity, and effective npm configuration. External
file-backed npm configuration, custom script shells, and external Node preload
options fail closed; project
`.npmrc` is supported through an authenticated copy. The hashed lifecycle-script
environment and source
metadata also participate in the key. `npm ci` remains the only
installer. Copied root metadata is authenticated before the wrapper temporarily
restores owner-only write access needed by dependency-install and source-view
transactions; the source mode is reapplied before view publication. Sealed
immutable source generations therefore cannot block staging writes. Worker
checks receive temporary full owner access with group and other write access
disabled for allowed generated outputs, and the sealed root mode is restored
afterward. A second
input-identical build reuses that installation and an unchanged profile source
view. Because enabled dependency lifecycle scripts can observe root files, the
complete non-generated source digest participates in every dependency key and
source symlinks fail closed. The install root is stable and hashed; copied
modification times, filesystem flags, and extended metadata participate in the
key while staging access times are normalized; the lifecycle source
is authenticated before install; installed output that embeds its staging path
is rejected; and project `.npmrc` content is provided as a restrictive
temporary copy and never published in the shared cache. Wrapper transaction
metadata is hidden while lifecycle scripts run and restored before publication.
Every profile gets its
own `node_modules` copy, so its checks cannot mutate the shared cache. A
canonical no-follow digest covers
the complete installed tree, including modes and bounded relative links. The
isolated view permits only Vite's profile-local `.vite`/`.vite-temp` outputs
outside that immutable snapshot. Build output reports dependency installation/cache time and source-view
preparation/reuse time.

`--with classic` has one exact meaning: add the complete classic initialization
cohort to the replacement/default cohort. It is not a classic-only mode. The
cohort consists of one `atrinik/classic` checkout, the MIT
`atrinik/playtester` checkout, and the MIT-by-default `tools` repository with
its GPL-2.0-or-later `map-checker-qt/` exception. The
union initializes the shared `atrinik/content@main` checkout exactly once.
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
children of the wrapper root such as `./client`, `./classic`, `./content`,
`./observatory`, `./deploy-control`, and `./web-platform`.
Logical components name their owning checkout and a safe source root within
it. Both stacks map role `content` to that one shared checkout. A local
`./content-1x` directory may remain as preserved migration history, but it is
not initialized or selected as an active component.

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
`libatrinik`, `content`, `playtester`, source-only `observatory`, shared
source-only `deploy-control`, and the shared `web-platform` package to
providers within each stack. The `observatory`
and `deploy-control` roles are default-only and have no wrapper build adapter
or runtime dependency; `deploy-control` owns its Worker/agent package and
deployment workflow boundaries in its own repository. The shared
`web-platform` package is default-only and has no wrapper build adapter or
runtime dependency while its package contract is being established. The
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

## Migrating persisted content selectors

Workspaces created before the content consolidation can retain profiles that
name `content-1x`, historical build/scenario/topology records, or worktrees in
the former namespace. Keep all of that state in place and run the dedicated
checked migration:

~~~sh
./atrinik migrate content --dry-run --json
./atrinik migrate content --apply
./atrinik migrate content --audit --json
~~~

The dry run proves the canonical `content@main` consolidation commit and
inventories every legacy selector and affected resource. Apply refuses dirty,
detached, locked, live, colliding, in-progress, or unproven inputs. A simple
certified primary selector becomes `content@main`; a managed worktree moves to
the shared namespace only when its repository, common Git directory,
commit/tree ancestry, destination, and cleanliness all match. An old `1.x`
worktree or arbitrary external path is never silently repointed.

Original profile bytes are journaled and updated atomically. Audit is
read-only. Before any later remote branch retirement, the exact bytes and
proven worktree paths can be restored with
`./atrinik migrate content --restore --json`. The old `./content-1x` checkout,
stopped records, states, logs, and historical coordinates remain preserved and
inert; this migration never deletes them. Cleanup continues to inventory those
references and remains a separate preview-first operation.

## Recovering persisted filesystem identities after a remount

Persisted workspace, topology, lease, and delivery-ledger records created by
older wrapper versions may contain the host-specific filesystem device number.
Use the explicit, checked recovery transaction after a devcontainer rebuild or
other remount:

~~~sh
./atrinik migrate filesystem --dry-run --json
./atrinik migrate filesystem --apply --confirm-remount
./atrinik migrate filesystem --audit --json
~~~

The dry run is read-only. Apply snapshots every affected record, preserves
legacy evidence, rewrites durable identities atomically, and records portable
pre/post identities plus rollback state in a local journal. It refuses a
changed inode, replacement, symlink, foreign-owned target, or ambiguous
record; never hand-edit the JSON. Live descriptor and mount fencing continues
to use the complete ephemeral `(st_dev, st_ino)` check, while durable records
omit `st_dev` and therefore remain valid when the same workspace is mounted
again. Rename-prone lease records intentionally omit ctime as well; their
opened-descriptor, generation, and content fences remain live. See
[`docs/FILESYSTEM-IDENTITY.md`](docs/FILESYSTEM-IDENTITY.md) for the schema
boundary and recovery guarantees. This operation is separate from
cleanup, topology shutdown, scope release, and repository migration.

The current classic client command opens a graphical application. Verify that
the devcontainer display forwarding socket is live before launching it. Use
`--dry-run` to build and print either launch command without starting the
process.

### Manual verification handoffs

Change handoffs should end with a copy-pasteable verification recipe that uses
the thin wrapper instead of internal build paths or direct component binaries.
Use the actual profile, topology, and state policy for the change. A runtime
client/server review normally follows this shape; until replacement runtime
contracts land, `PROFILE` must be `classic` or derived from it:

~~~sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --temporary-state --json
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY client --follow
# Perform the feature-specific checks described in the handoff.
./atrinik down TOPOLOGY
~~~

Each current topology persists a random generation identity and a mode-0600
filesystem control endpoint. `ps` and `down` therefore work from another
supported devcontainer or sandbox that shares the workspace even when its PID
namespace cannot see the namespace-local process numbers. JSON status reports
`live`, `exited`, `stale`, or fail-closed `unreachable` liveness plus exact
runtime-generation, process-tree, state-policy owner/path/lifecycle, and port
observations and the safe next action. Text status prints the retained
generation identity and action. A control response must match both the topology name and generation;
`down` never falls back to signaling a mismatched or recycled PID.
The runtime places the socket under a short generation-derived name in the
shared workspace, avoiding ordinary managed-worktree and topology-name path
growth. The process-tree lease is tied to both the generation and exact file
identity; never replace,
unlink, or repair it by hand because an invalid current lease fails closed.

If a supervisor is killed rather than shut down, its same-namespace guardian
terminates exact process-tree lease holders and retains the runtime generation
and server state until their absence is proven. No layout or mutable build-root
lease survives publication. During that
interval another session reports `unreachable` and fails closed: wait, retry
`./atrinik ps TOPOLOGY --json`, then retry `./atrinik down TOPOLOGY`. If the
lease remains retained and control remains unreachable, preserve the exact
record and lease for operator diagnosis; do not scan `/proc`, signal recorded
PIDs, remove lease files, or reuse the topology name.

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
left in that worktree for normal Git resolution. Historical `content-1x`
paths and records are inert and are never treated as `content@main`. Create new
content review worktrees from the shared `content` checkout.

A target build resolves only that target's transitive provider inputs. During
its bounded preparation phase, each clean primary input is exported from its
exact Git commit into a wrapper-owned, read-only source generation; the build
restores entries omitted by repository-owned `export-ignore` release rules
directly from their recorded Git blob IDs before validating the complete tree;
then releases that primary's source lease before configure, compile, and tests.
The manifest may give a logical component strict checkout-relative
`source_includes` for shared sibling files or directories that its build reads
outside the logical `source` directory. Those inputs are exported beside the
logical source, enter the immutable generation key and authenticated closure
digest, and are reproduced beside the component's build source view. The
Classic client and server both declare the repository-root `cmake/` modules,
license, and attributions this way, so their supported scoped builds retain
`client/` and `server/` as the paths printed by `atrinik path` while using one
authoritative copy of each shared input. Archive publication retains its
temporary descriptor, and extraction creates entries and applies modes relative
to pinned, no-follow generation directories. CMake dependencies that run
mutation-based tests receive writable profile-local copies; the shared generation
itself remains sealed.
Consequently a long-running build from a Classic feature worktree does not
block `sync --with classic` from advancing unrelated or snapshotted clean
primaries. Dirty sources and selected worktrees remain live inputs and retain
their exact source leases. The authored Classic content publisher still proves
its target against a live Git checkout, so operations that consume it retain
that exact content lease. `build all` requests the union of every target's
dependency closure and retains or snapshots every input without weakening
coherence.

## Agent development scopes

Use one wrapper-owned development scope as the default entry point for
concurrent agent work. Provisioning resolves every logical selector to its
physical checkout, creates one full worktree per selected checkout, publishes
one immutable derived profile, reserves a topology name and state policy, and
publishes the completed scope record last:

~~~sh
./atrinik scope create classic-server --name issue-402 --from classic --json
~~~

Omit `--name` for a timestamped, collision-resistant `agent-*` name. A supplied
name is stable: repeating the exact completed request returns the existing
record, while a different component, label, branch, start point, topology, or
state request fails without replacing it. Multiple Classic selectors still
produce exactly one `classic` worktree containing all five `classic-*` source
directories. Per-checkout coordinates can be selected deliberately:

~~~sh
./atrinik scope create classic-server content --name issue-402 \
  --from classic \
  --label classic=issue-402-classic \
  --branch classic=feat/agent-development-scopes \
  --start-point classic=origin/main \
  --json
~~~

Classic has two selector namespaces: a logical component such as
`classic-client` is positional, while `--label`, `--branch`, and `--start-point`
overrides use the physical checkout key `classic`. A logical selector therefore
uses `--label classic=...`; `classic-client=...` is rejected as an unselected
checkout override. The scope name is the single namespace source: the derived
profile and topology are both `scope-<name>`. If `--topology` is supplied, it
must equal that canonical value; creation rejects any other value before
publishing a profile, topology, worktree, or scope record.

Automated scopes default to generation-owned temporary state. Persistent state
is opt-in with `--state NAME` for an existing registered state or
`--default-state` for the shared default. Provisioning does not start a
topology or create persistent state.

The secret-free JSON record under `workspace/scopes/<name>/scope.json` contains
the scope name/generation/request digest; base profile and stack; requested
logical components; every repository, checkout, label, branch, start point,
commit, tree, path, and common-Git identity; the immutable profile name/path
and digest; topology name/path; state ownership/lifecycle; and exact supported
path, build, topology, log, shutdown, and release commands. Use those returned
commands rather than reconstructing managed paths. `scope show` and `scope
list` return the same validated records.

Scope command maps are creation-time snapshots. A later manifest addition
therefore does not invalidate a completed scope or retroactively add commands
to its handoff; every retained entry must still be a valid current coordinate
and exact command, while removed or edited coordinates fail closed.

Creation journals every worktree, profile-reference, profile, and completed
record publication boundary. A failed transaction removes only exact newly
created worktrees and profiles that remain clean, unchanged, and unreferenced;
changed or uncertain inputs and the durable journal remain as explicit cleanup
references for recovery.

If a named create fails after Git has created the requested branch but before
the worktree is registered—for example because Git LFS or a checkout filter is
unavailable—the journal may finish `rolled-back` with that branch-only side
effect retained. Rerun the exact original `scope create` command, including
the same selectors, name, labels, branches, start points, profile, topology,
and state flags. The wrapper verifies the reservation generation, request
digest, retained row coordinates, repository/workspace/scope identities, base
commit, and branch head, then adopts the branch only when the destination and
all worktree, profile, topology, lease, and reference coordinates are absent
and conflict-free. Changed, dirty, checked-out, foreign, ambiguous, or
uncertain evidence fails closed while preserving the journal as audit evidence.
After recovery, use `scope show --json` and the delivery helper's
`scope-observe`/`scope-bind-cas`; never hand-edit the journal or ledger.

The returned commands form one complete lifecycle: build the scope profile,
start only its reserved topology with its recorded state policy, inspect it,
stop it, then preview and apply release. Two scopes may select distinct
worktrees of one physical checkout and run that lifecycle concurrently. A live
scope A does not prevent a disjoint scope B from stopping and releasing. Use
readiness markers, lease ownership, and published status transitions when
testing concurrency; timeouts bound failure but elapsed compiler or startup
time is not correctness evidence.

Scope release is explicit and hash-bound preview-first:

~~~sh
./atrinik scope release issue-402 --dry-run --json
./atrinik scope release issue-402 --apply --plan PLAN_SHA256 --json
~~~

Apply recomputes the plan under the exact leases and refuses a stale plan,
live or unreachable topology, retained generation, active lease, dirty,
detached, replaced, ambiguously owned, or unexpectedly referenced worktree,
changed profile, or uncertain build root. Named and default persistent state
is always retained. Stopped topology history remains owned by the separate
topology-cleanup lifecycle; its exact stopped scope-owned reference does not
prevent release only when current regular spec/status records bind the same
profile, generation, and resolved coordinates and prove a control-requested
clean shutdown with released runtime, port, and state leases. Stale, historical,
mismatched, or unrelated records fail closed. Release journals initial,
destructive-substep, build, profile, worktree, and final completion boundaries.
After interruption, run a fresh preview and resume with that new digest;
completed removals and exact pending reference/branch cleanup are recovered.
The completed scope record and release journal remain recovery evidence and
available to bounded, secret-free shell completion.
Each destructive action is recorded as in-flight before mutation, so a retry
can prove the exact absent post-state and finish after a crash between removal
and the completed-action journal update.

If a released scope's retained `requested_components` disagrees with the
schema-v1 planned selector, do not edit or delete the ledger, scope record, or
release journal. Use the delivery helper's `recover-released-scope` CAS with
the exact predecessor tuple, scope-show bytes, completed release journal, and
explicit-recovery authority. It accepts only the two proven Classic directions
(`classic`/`classic-client`), preserves the old name, branch, start SHA, roots,
and evidence, proves fresh replacement collisions, and replans the corrected
selector under a new scope name.

An older helper can instead leave a planned ledger whose live scope already
uses the canonical topology but whose request retained another topology name.
Preserve the exact predecessor tuple and raw `scope show`, worktree-list, and
safety outputs. The delivery helper's `recover-prebind-scope` accepts only an
explicit authority binding those bytes and coordinates; it CAS-updates the
topology request alone, records the predecessor digest in history, and retries
after a crash without manual ledger edits or deletion. Released, active-but-
ambiguous, changed, or unsafe evidence is rejected.

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
./atrinik worktree create content maps-pr \
  --branch review/maps-pr --existing
~~~

List or remove managed worktrees:

~~~sh
./atrinik worktree list
./atrinik worktree list --json
./atrinik worktree list --wrapper-self --json
./atrinik worktree remove content maps-pr
~~~

The default list remains manifest-owned. `--wrapper-self` instead emits the
complete parser-driven Git inventory for the wrapper repository, tagging both
its primary and linked worktrees as physical checkout `atrinik`. It cannot be
combined with component selectors; use its JSON bytes directly when a workflow
needs retained wrapper-self inventory evidence.

Removal refuses dirty worktrees. Each worktree is a full Git worktree of its
physical repository, so a classic worktree contains all five classic source
directories. Ordinary `git commit`, `git push`, and `gh pr create` work from
inside it. These primitive commands remain supported for manual composition;
prefer an atomic scope for concurrent automated development.

## Previewing and reclaiming stale workspace data

Cleanup is always operator-invoked and preview-first. With no options it
inventories registered worktrees and marker-owned profile builds older than
seven days, including immutable source generations and individually
marker-owned Worker dependency entries, without changing the filesystem:

~~~sh
./atrinik cleanup
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope worktrees classic-server --older-than 14
./atrinik cleanup --scope builds --scope npm-cache --scope compiler-cache --older-than 0
./atrinik cleanup --scope sound-cache --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --apply
./atrinik cleanup --scope temporary-states --older-than 7 --dry-run --json
./atrinik cleanup --scope cleanup-journals --older-than 30 --dry-run --json
./atrinik cleanup --scope cleanup-journals RECEIPT.json --older-than 30 --apply
./atrinik cleanup --scope all --older-than 7 --apply
~~~

`--dry-run` is the explicit spelling of the default mode; only `--apply`
mutates. Repeated `--scope` options combine `worktrees`, `builds`, and the
opt-in `temporary-states`, `npm-cache`, `compiler-cache`, and `sound-cache`;
`all` selects all six.
Topology history is a separate opt-in `topologies` scope and is deliberately
excluded from both the default and `all`, so a broad cache/worktree cleanup
cannot silently expand to runtime history.
For one abandoned topology, pass its exact name as the positional selector
and inspect the JSON before applying the identical request:

~~~sh
./atrinik ps TOPOLOGY --json
./atrinik cleanup --scope topologies TOPOLOGY --older-than 0 --dry-run --json
./atrinik cleanup --scope topologies TOPOLOGY --older-than 0 --apply --json
~~~

The preview is eligible only when `reference_classification` is
`inactive_topology`. That shared classification requires a published stop,
unreachable or legacy control, released process/runtime/port/layout leases,
removed temporary state, and complete mutable-output cleanup. Active,
reachable, retained, malformed, or uncertain records remain protected. This
is lifecycle-bound evidence; age never proves that a topology is safe, and an
exact selector prevents recovery from selecting another agent's topology.
Delivered cleanup receipts use the separate `cleanup-journals` scope, also
excluded from `all`. Broad inventory remains bounded; an exact-name selection
and its current or legacy journal retry remain recoverable above that bound.
Current and legacy recovery discovery advances one durable bounded page per
identical apply retry.
One globally leased cursor owns that discovery; other maintenance requests must
finish the recorded request first and cannot accumulate parallel cursors. The
cursor stores the exact canonical request and its digest so a later process can
identify and resume the required invocation without reconstructing its filters.
Only exact schema-v2, regular, owner-only receipts are eligible. Age is measured
from durable delivery; pending, malformed, linked, or otherwise unsafe receipts
remain protected. A receipt that records removals requires its exact filename
as the positional selector. Retirement takes an exclusive receipt lease, so it
cannot interleave with archive proof holding the matching shared lease.
Positional checkout or logical component names narrow worktree and sound-cache
inventory, exclude topology-owned temporary state, and still deduplicate
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

Active issue-delivery evidence is a separate protected ownership boundary.
When `build/reviews` exists, cleanup inventories it through the canonical
delivery-ledger helper and protects the review root, every active ledger's
report and lock, and each ledger-owned worktree. Missing reports or locks,
unsupported helper output, and unrecognized inventory errors protect the
complete cleanup plan and surface the inventory error on the affected review
root; the wrapper never recreates missing evidence. The repository migration
preview and apply paths use the same inventory and refuse while any active
ledger exists. Recovery therefore requires a separately authenticated
`explicit-recovery` grant and exact preserved bytes. A fresh ledger, timestamp,
branch, worktree, issue assignment, Project state, or push access cannot
substitute for lost evidence; when a required canonical report or member is
gone, the safe result is an explicit stop awaiting maintainer direction.

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

Immutable source generations use the same default `builds` scope and grace
period. Cleanup authenticates their checkout/key path, ownership marker,
and closed commit/tree/subpath metadata before considering removal. Build
selection separately proves the complete tree digest and exact path,
entry-type, executable-mode, symlink-target, and blob identity against the
recorded Git source tree. A mismatch under exact descriptor-verified marker
ownership atomically retains the generation as a recovery transaction and
rebuilds the canonical key; uncertain ownership fails the build without moving
data. Cleanup reports an owned canonical content-digest mismatch as an eligible
`corrupt_source_generation`, while recovery transactions remain independently
previewable. Inspect retained data with `./atrinik cleanup --scope builds
--older-than 0 --dry-run --json`; after reviewing the exact protected or
eligible generation, use the same scoped command with `--apply`. An active
build holds the generation's shared lock; apply takes its exclusive side and
repeats ownership, content state, and age validation before bounded removal.
Recovery and staging transactions within the marker-owned container are
separately inventoried, remain protected under an active generation lock, and
are reclaimable only after the normal grace period.

Worker dependency entries under `workspace/build/worker-dependencies/` use the
same default `builds` scope and grace period. Cleanup requires the exact parent
and entry markers, strict metadata and last-used time, a safe direct-child key,
and an idle per-key build lock; uncertainty protects the entry. Removing a
stale entry leaves the marker-owned container and npm download cache intact.
Recognizable interrupted staging and backup directories live below the
separately marker-owned `.transactions` container and are reclaimed by the
same preview-first scope only after their conservative tree-mtime/root-ctime
grace period and per-key lock checks. Apply repeats identity, ownership, and age
validation while holding that lock through removal. A valid matching backup is
restored under the lock before another install is attempted.

The shared `workspace/build/npm-cache` and `workspace/build/compiler-cache`
are never part of default cleanup. Their explicit scopes require exact
marker-owned paths, valid last-use metadata, safe fixed containment, age, and
absence of an active build. Only the npm path admits one legacy known cache
after the same proof. Invalid entries inside a valid Worker cache root remain
protected `worker-dependencies` items. Unmarked profile roots, invalid Worker
cache roots, other `workspace/build` children, and all remaining mixed
top-level `build/` entries are report-only `unmanaged-build` items. Deep-review reports, ad hoc
builds, packages, archives, and unregistered siblings are never recursively
deleted.

The explicit `sound-cache` scope inventories only 20-hex exact-input trees
below `build/atrinik-workspace/` in registered `atrinik/sound` primary and
linked worktrees. It requires the exact nonpublishing playtest marker, safe
Git-proven containment, conservative tree age, and an idle producer build
lock. Apply repeats those proofs under that lock before removing one tree;
invalid, active, young, unregistered, or uncertain entries remain protected.
Every remaining producer cache, plus any busy or invalid per-output lock,
protects its containing sound worktree from ordinary worktree cleanup,
including `--scope all`. Run the sound-cache apply first and preview worktrees
again afterward. Producer build/verify commands hold a shared Git-admin lease;
worktree removal holds its exclusive side from final proof through Git removal,
so a producer cannot start in the removal window. A sound worktree without the
versioned producer lease marker is not reclaimable by wrapper cleanup.

The explicit `topologies` scope inventories direct marker-owned directories
below `workspace/topologies/`. It reports liveness, control generation,
process-tree, runtime, port, and legacy repository-layout lease state,
conservative age, and every path apply would remove. Only old `exited` or
legacy `stale` records with unreachable controls and released leases are
eligible. Orderly records age from `stopped_at`; legacy stale records without
it use the newest no-follow tree mtime. Live, unreachable, young, locked,
linked, malformed, unowned, or unverifiable records remain protected. Apply
takes the exact topology coordinate lease and operation lock, repeats
identity/generation/lease/age/tree validation, and removes only that topology
directory. It never invokes `down`, signals processes, reuses a name, removes
build roots, or changes profiles, scenarios, source, or persistent state. A
topology containing any generation-owned temporary state remains protected;
reclaim eligible disposable state with the separate scope first.

The explicit `temporary-states` scope inventories generation-named states
below marker-owned topology records. An old disposable state becomes eligible
only when its topology/generation metadata, directory identity, registry
absence, and released exact lease all validate. Retained, promoted, live,
unreachable, busy, linked, malformed, registered, or uncertain state remains
protected. Apply holds the topology operation and state locks while repeating
the proof and never stops a topology.

Apply performs one complete inventory and size recomputation, then acquires the
exact target/reference leases and freshly revalidates each stable-sorted
candidate without rescanning unrelated report-only payloads. Any new uncertainty
fails closed. Busy targets are skipped so they do not convoy unrelated cleanup.
Eligible topology records come first, then profile builds and explicit
sound-cache entries, exact Git worktrees, other explicit caches, and safely
shared prunable Git metadata. `workspace/cleanup-journals/` records the exact
request, original report, ordered targets, and one write-ahead in-flight action.
Each action is first `prepared`; only a second durable `removing` checkpoint
written after exact locked revalidation authorizes absence or partial-removal
recovery. The same cleanup request resumes that journal after interruption, recognizes a
completed in-flight removal, and reaches one `complete-pending-output` receipt
without changing the original target selection. That receipt binds the exact
terminal report and replays it for an identical retry. The CLI flushes the
report before acknowledging it as `complete-delivered`; direct API callers
must call `cleanup_acknowledge(report)` after consuming it. Only a delivered
receipt permits a new run with the same selector. Busy or transiently failing
targets leave the journal resumable while disjoint targets may still complete.

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
./atrinik profile set maps-review content --worktree maps-pr
./atrinik profile set maps-review classic-server --worktree socket-review
./atrinik profile show maps-review
./atrinik build all --profile maps-review --test
~~~

The `classic-server` alias above has the same checkout-wide effect as naming
`classic`: every classic logical component comes from `socket-review`.
Resolution then returns the appropriate `server/`, `client/`, `editor/`,
`libatrinik/`, or `protocol/` source root for each component. Profile schema 5
rejects different selectors for logical components that share one physical
checkout and records the selected sound mode plus immutable released-product
coordinates when applicable. Existing schema-3 and schema-4 profiles load as
`source` or their previously selected mode and are upgraded when next changed.

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

The content migration reader accepts the old internal `migrated-worktree`
selector only as recovery input. Current profiles use normal `content`
primary, worktree, or proven absolute-path selectors. Historical `1.x`
selectors remain truthful and inert instead of being relabeled as `main`.

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

Before a server build or launch, the coordinator prepares marker-owned content
and resource caches inside the exact profile build. A cache is reused only when
its schema and provider, repository, branch, checkout, source path, checkout
path, and clean `HEAD` metadata match and its required output structure remains
valid. Dirty inputs rebuild every time and do not receive reusable metadata;
inputs are checked again after generation so a concurrent checkout change
cannot replace the previous valid cache. Only tracked files below the resource
repository's `runtime-paths.txt` allowlist are staged; development metadata and
untracked files cannot become server assets. Client builds expose the selected
sound checkout by default. A saved profile derived from `classic` may instead
opt into the complete local-only compatibility tree supplied by the selected
sound checkout:

~~~sh
./atrinik profile create classic-audio --from classic
./atrinik profile set classic-audio sound --worktree issue-27-playtest-tree
./atrinik profile sound-mode classic-audio local-playtest
./atrinik profile show classic-audio --json
./atrinik build classic-client --profile classic-audio --test
./atrinik topology show classic-audio --service client --json
~~~

The built-in `classic` profile and every replacement profile remain in `source`
mode. Local-playtest mode requires a clean selected sound checkout with the
public `tools/sound_release.py build-playtest-tree` command. That repository
owns conversion and full decoder verification; it writes only to its ignored
`build/atrinik-workspace/` state. The wrapper independently requires the
version-1 nonpublishing marker and schema, exact 339-key closure, every
source-manifest-derived byte-preserved Vorbis or rendered Opus mapping
(including payloads stored at legacy extensions), every payload/hash/count,
immutable input hashes, and the complete tree digest. It never falls back to
the raw mixed-format checkout.

Exact clean inputs reuse the same verified local output without network access.
The profile build and supervised client runtime link that one root, and their
JSON records include its mode, source commit/tree, clean-state result,
manifest/toolchain/schema/marker/blocker hashes, counts, and output digest.
The tree is not an archive or released-runtime input and cannot enter the
wrapper's `PACKAGE_TYPE=none` Classic build path as a package. A changed input
gets a distinct cache key; failed or racing generation leaves existing output
untouched. Wrapper-owned profile builds remain covered by ordinary retention.
Sound-owned ignored outputs are preserved by default and are reclaimed only
through the explicit preview-first `sound-cache` cleanup scope described above.

A saved Classic-derived profile may instead consume the publishable Classic
compatibility runtime from an immutable `atrinik/sound` release. Released mode
does not invoke a source builder and never falls back to `source` or
`local-playtest`. Supply every coordinate posted by the sound release. For
example, the published v1.4.1 Classic runtime is selected with:

~~~sh
./atrinik profile create classic-released-audio --from classic
./atrinik profile sound-mode classic-released-audio released \
  --release-repository atrinik/sound \
  --release-tag v1.4.1 \
  --release-product-version 1.4.1 \
  --release-source-commit 49a169bf41568e4e3b3ac70dfaf42b1a3eabe985 \
  --release-source-tree 92b81774820dfd55944f4d7b005c1dc344b43561 \
  --release-asset-url https://github.com/atrinik/sound/releases/download/v1.4.1/atrinik-sound-classic-runtime-1.4.1.tar.gz \
  --release-archive-sha256 8373868ab4632eda58ae7959909f414a10a43ce519dd1ef9e7f911d4fa208a52 \
  --release-manifest-sha256 7961ea27069c2cd54131466394571942d486e31e1007c9d957b97cb8b0d63b56 \
  --release-source-manifest-sha256 3aacd122abe16da771ac1eb6ad80c50c1c6e7ab43d555dc8772f21be24248366 \
  --release-schema-sha256 428e1312d9922ab4ec20c0ee89d93d842528db6d8cc75197c135f4d4f59066aa \
  --release-toolchain-sha256 ee842444c37df3c6784665c2dacef4ab9220f3abfc5c2daf9214fe4b40aadbf7 \
  --release-tree-sha256 2c3d42ea91ca088ac37e5215e3452dad31e5f1fb17018941ed5ad0a3c53060da
./atrinik profile show classic-released-audio --json
./atrinik build classic-client --profile classic-released-audio --test
./atrinik topology show classic-released-audio --service client --json
~~~

The wrapper downloads only that exact versioned GitHub release URL, verifies
the outer checksum, safely extracts one bounded product prefix, and then checks
canonical internal checksums, the publishable/non-playtest manifest and
packaged schema, the nonblocking remediation report, packaged notices and
licenses, exact 339-path closure, every payload hash and codec signature, the
189 byte-preserved Vorbis / 150 Opus split, and the logical-tree digest.
The verified archive and tree live below the coordinate-keyed profile build;
unchanged inputs reuse them offline. Build and topology records retain all
coordinates and verification results. A changed coordinate produces a distinct
build key, while incomplete or mismatched caches fail closed and remain covered
by normal preview-first build cleanup. Supply-chain audit output identifies the
selected archive, source commit/tree, and logical tree; license, CycloneDX, and
SPDX reports for that profile also carry the complete immutable coordinate set.

## Deterministic test scenarios

A scenario is a local, ready-to-login account and character plus its own
registered server state. It is useful for handing off an issue reproduction
without asking every reviewer to register another account and character.

~~~sh
./atrinik scenario create issue-42 --profile maps-review --preset basic-player
./atrinik scenario show issue-42 --json
./atrinik up --name issue-42 --profile maps-review --state scenario-issue-42
~~~

`basic-player` provisions a normal `human_male` first-login character through
the selected server's own account API. On first login the server supplies its
configured starting map, standard skills, and initial items. The wrapper builds
the selected server, creates the dedicated `scenario-issue-42` state, stores a
generated password in a mode-0600 ignored file, and prints exact
`topology show`/`up`/`ps`/`logs`/`down` commands. `show` and `list` never reveal
the password. A supervised server/client topology that selects the scenario
state validates its ownership and metadata, then launches the client with the
complete login automatically. `scenario credentials` remains available for
bounded local diagnosis. `scenario list` keeps the global inventory usable when
a retained scenario can no longer resolve its profile or fails current validation. JSON
output represents that entry with its `name`, `path`, `inert: true`, and a
stable `inert_reason`; bounded human output labels the same entry `inert`.
Control characters in inert names and paths are escaped. Exact `show`,
credential, and reset operations continue to fail closed.

The generated scenario password is disposable and local. Automatic login may
expose it in the local client process arguments, client output, or Codex
execution logs; this is the only intended exception. Never copy it to commits,
GitHub, public logs, durable handoffs, or final reports, and never reuse it.

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
preset if a future reproduction needs more than `basic-player`. The Classic
server also owns `lighting-radiance-day`, `lighting-radiance-dawn`, and
`lighting-radiance-night`: each places an unapplied mithril lamp in a character
at the Greyton lighting-review map and initializes the isolated world clock to
the named period. `lighting-radiance-inside` places the character beside the
same map's interior fireplace at night so ownership-gated roof and interior
rendering can be reviewed. These presets are reserved for repeatable renderer
reviews whose ordinary world progression would dominate the test.

## Supervised topologies and practical workflows

A profile is a declarative source topology. The `topology` and process-lifecycle
commands turn that selection into a Compose-like native development stack:

~~~sh
./atrinik topology show PROFILE
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state [--port UDP_PORT]
./atrinik ps [TOPOLOGY]
./atrinik logs TOPOLOGY [server|client] [--follow]
./atrinik down TOPOLOGY
~~~

Server topologies choose exactly one state policy: `--temporary-state` creates
a fresh generation-owned state for isolated automation, `--state NAME` selects
an existing registered persistent state, and `--default-state` explicitly
selects the legacy managed persistent default. Omitting all three remains
backward compatible with `--state default`. `topology show`, `up`, and
`ps --json` report the policy, exact owner, path identity, and lifecycle. Bounded
and followed service logs begin with the same policy context so captured output
retains the state ownership boundary.
Temporary state is never entered in `state list`; a confirmed clean `down`
removes it after its process and state leases are released. Clean proof
requires expected service exit status as well as released leases and remains
durable so an interrupted `down` can safely retry finalization. A crash,
unreachable supervisor, malformed record, or uncertain lease retains it for
diagnosis.

Retain a clean temporary state deliberately, then promote it without replacing
an existing name:

~~~sh
./atrinik down TOPOLOGY --retain-state
./atrinik state promote TOPOLOGY SAVED_NAME --json
~~~

Promotion registers the exact stopped directory in place and persistent policy
surfaces retain its topology-generation provenance in independent durable
metadata, including across a promotion retry or unavailable origin record.
Retained and promoted states are protected, and a topology name with unfinished temporary
state cannot be restarted until that state is promoted or explicitly reclaimed.
Abandoned disposable states participate only in
the explicit preview-first cleanup scope:

~~~sh
./atrinik cleanup --scope temporary-states --older-than 7 --dry-run --json
./atrinik cleanup --scope temporary-states --older-than 7 --apply --json
~~~

Cleanup never stops a topology and never removes a live, busy, linked,
malformed, registered, retained, promoted, or otherwise uncertain state.
For topology-history recovery, use the exact topology name with `ps` and the
`topologies` scope. Do not infer inactivity from directory age or choose a
different state/topology when another agent owns it; retained state, lease
uncertainty, and concurrent restart/replacement are deliberately fail-closed.

### Package a Classic review profile for Windows

Create one portable ZIP when the client must be reviewed as a native Windows
process rather than through the devcontainer display. The package cross-builds
the selected Classic client and server, includes the profile's collected maps,
generated client maps, resources, and sound, and snapshots one persistent
server state with its accounts and player data:

~~~sh
./atrinik down REVIEW_TOPOLOGY
./atrinik package windows \
  --profile REVIEW_PROFILE \
  --state REVIEW_STATE \
  --output build/packages/review-windows.zip \
  --json
~~~

The state must be stopped and must have been started at least once so its QUIC
identity exists. To transfer a temporary topology's state, stop it with
`--retain-state` and promote it to a new persistent name first. Packaging
refuses a live or otherwise busy state, an incompatible profile/state pairing,
an existing output file, or a non-Classic profile.

Run the command inside the `windows-cross` devcontainer to use its installed
toolchain directly. From the ordinary devcontainer or WSL2, the wrapper uses
Docker and the digest-pinned image in
`.devcontainer/windows-cross/devcontainer.json`. After the command succeeds,
copy the ZIP to Windows, extract it, and double-click `run.bat`. The launcher
starts the local packaged server on UDP port 1730 and pins the client to that
server's copied certificate fingerprint. Use `--port PORT` when 1730 is not
available on the Windows host.

The output file is mode 0600 on the packaging host and the command reports its
SHA-256 digest. Treat it as sensitive: `server/data` contains private player
data, credentials, and the server private identity. Do not upload the ZIP to a
public pull request, issue, CI artifact, or release. The wrapper never
overwrites an existing ZIP.

For a complete Classic profile, `up`, scenarios, and individual builds resolve
one manifest-derived build-root selection across the `client`, `server`,
`content`, `protocol`, `libatrinik`, `sound`, `resources`, and
`metaserver-worker` roles. Only the requested game service targets are built;
the wider selection keeps their incremental outputs and recorded coordinates
on one root. Partial profiles retain the requested service's dependency closure.

### Classic GPU shader toolchain

On x86-64 Linux, a managed Classic client build prepares the checksum-pinned
shader toolchain from the selected Classic checkout and passes its absolute
paths to CMake. The preparation is shared by standalone `build client`, paired
`up`, and the client portion of `build all`; its lockfile and shader-manifest
identity are retained in the profile build metadata. No caller `PATH` changes
are required.

For a host-provided toolchain, set both executable overrides and optionally the
DXC library directory before invoking the wrapper:

~~~sh
export ATRINIK_DXC_EXECUTABLE=/absolute/path/to/dxc
export ATRINIK_SPIRV_CROSS_EXECUTABLE=/absolute/path/to/spirv-cross
export ATRINIK_DXC_LIBRARY_DIRECTORY=/absolute/path/to/dxc/lib
./atrinik build client --profile classic --test
~~~

Alternatively, set `ATRINIK_GPU_SHADER_DIRECTORY` to an absolute directory
containing a generated cohort and its `SHA256SUMS` manifest. The explicit
cohort is validated by the Classic CMake contract. Do not combine that variable
with the executable overrides; on platforms where the pinned cohort is not
available, use one of these explicit overrides.

Provider selection is stack-coherent: a classic service never binds a
replacement protocol or `content@main`. Today the `classic` stack is the
runnable implementation; the `default` replacement profile will become
runnable as its component contracts land. For a runnable profile, `up` builds
the requested targets, reuses or refreshes content and resource caches, stages
sound, prepares an isolated runtime, and starts both game processes under one
supervisor. A server gets an available UDP port by default; `--port 0` has the
same automatic-allocation meaning, while `--port 1` through `--port 65535`
requests that exact port. Automatic selection briefly holds the workspace
allocator only while it chooses and publishes a unique generation-bound
reservation. Every port uses a short mode-0600 transaction file, and every
server retains its own immutable mode-0600 generation lease through
supervisor and guardian recovery, so startup preparation and readiness waits
for different ports overlap. An exact-port conflict fails without waiting for
an unrelated port and names the owning topology and generation plus the safe
retry action; once startup has published supervisor status, `ps` and `down`
are available. The supervisor rechecks kernel availability immediately
before launching the server; an external process that wins the bind race
produces a bounded startup error without changing another topology's
reservation. The supervisor waits for both the QUIC certificate fingerprint and
completed server initialization, then gives the paired client an authenticated
loopback endpoint, disables metaserver and STUN discovery in that client, and
disables STUN discovery and automatic port mapping in the server before
declaring the topology ready. Use
`--service server` or `--service client` for a single service. Client startup
requires a live forwarded display socket.

Ordinary wrapper operations coordinate through fair, exact-resource leases,
not the workspace-global layout lock. Workspace-local coordinates live under
`workspace/leases/`; physical-checkout Git administration and source
coordinates live under the wrapper's common-Git `atrinik-resource-leases/`
namespace so wrapper worktrees or relocated state roots cannot split
coordination. Every operation also retains its own wrapper-worktree source
coordinate, preventing cleanup from removing the code and ignored component
checkouts beneath an active wrapper view. Saved non-primary profile references
are also registered in this physical namespace, so cleanup from a relocated
state root observes references published by another root. First-use registry
backfill retains exact paths from missing profile and scenario sources as
conservative historical references without changing the authored records.
Those unbound records do not prevent `status`, `sync`, or unrelated commands
from constructing the workspace, but they protect a source that later reappears
at the same path. Genuine source-lease contention reports the exact coordinate,
owning operation and supported recovery action; an authored record that changes
during confirmation fails with a distinct retry diagnostic. Profile names,
topology names, scenarios, states, build roots, and cache keys remain
workspace-local.

Delivery live proof also reads saved profiles through a bounded, pinned,
descriptor-relative inventory without modifying them. A profile whose selector
map is a strict subset of its current stack is
historical for reference discovery: its retained selectors are still fully
validated, and any selector resolving to the candidate worktree remains a
blocking reference. Extra components, malformed selectors, and other uncertain
profile state fail closed.
Delivery scope observation and binding accept either an exact physical checkout
selector or one exact logical component selector. For Classic, `classic` is the
physical checkout and `classic-*` are its logical components; while older
operators are being migrated, use a positional `classic-*` selector and keep
per-checkout `--label`, `--branch`, and `--start-point` overrides keyed by
physical checkout `classic`. This preserves one checkout-wide worktree and
avoids treating the physical checkout name as a logical component.
Each lease request publishes its locked `waiting` owner record before main-lock
admission, then transitions it to `admitted` through a per-coordinate transition
gate under owner-directory serialization. The main lock is acquired before that
short gate, so a blocked request cannot convoy compatible peers or prevent the
current holder from releasing. Diagnostics list admitted holders before queued
waiters, rendezvous with every in-progress admission before returning a stable
snapshot, and cannot reap a partially published record. Failed publication
removes its exact token; if the filesystem also refuses that cleanup, the
operation fails closed and reports the retained token as cleanup uncertainty.
Teardown always releases the main lease even when owner-metadata finalization
fails, while reporting retained metadata uncertainty for later diagnostic
reaping. If main-lease release itself cannot be confirmed, the owner record is
durably marked `release-uncertain` and retained as explicit recovery evidence.
Requests acquire that deterministic order. A writer queued for one coordinate
precedes later readers of that coordinate, while unrelated resources continue:
two builds on different worktrees of one repository overlap, and init/sync for
distinct checkouts do not convoy.
Multi-source writers use all-or-none retry, so waiting for one busy source does
not retain earlier disjoint source coordinates. Distinct explicit topology
ports use exact startup locks; only automatic port selection briefly uses the
allocator mutex.

A build holds its profile-name lease while deriving the requested target's
transitive provider closure, then locks only those selected sources and
revalidates them. Clean primary inputs are exported atomically into reusable,
commit/tree/subpath-keyed read-only generations below
`workspace/build/source-generations/`. Publication flushes every sealed source
file and directory, compares a descriptor-pinned whole-tree identity, content,
and mount-boundary inventory immediately before the no-replace rename, then
flushes the containing directory; reuse verifies ownership metadata and a
complete tree digest plus the exact recorded Git tree and the captured checkout,
source, and common-Git filesystem identities. An incomplete generation with
exact marker ownership is atomically retained as a recovery transaction only
after a pinned traversal excludes nested mounts, then rebuilt at its canonical
key; uncertain ownership fails closed. Recovery
transactions remain visible to preview-first `builds` cleanup and its normal
grace period. No generation links or hard-links back to mutable primary files.
Existing generations are authenticated under a shared per-key lock; only an
absent or conclusively corrupt generation takes the exclusive creation lock and
rechecks after admission.
Every generated path is revalidated after the build takes its shared pin; only
then are its primary source leases released. Dirty primaries and worktrees stay
locked for as long as the build
can read them. Cache and region-map coordinates come from the captured snapshot
or generation metadata rather than rereading a released primary. The build
persists original filesystem/Git observations and immutable generation paths
and digests in
`.atrinik-profile-resolution.json`, and does not reread a mutable profile by
name. Profile or topology publication and worktree removal share any remaining
live target source lease, so cleanup cannot race a newly published exact
reference. Cleanup apply locks and revalidates one stable-sorted candidate at a
time, skips busy candidates, and journals completed actions in
`workspace/cleanup-journals/`.

Waits longer than 10 seconds report the resource kind, stable coordinate, known
owner operation, and supported recovery command without relying on a
namespace-local PID. Owner records are written and locked in a private staging
directory, then linked atomically into the diagnostic namespace; stale-record
scans therefore never observe a new record before its live-owner lock is
established. Legacy records are retained conservatively because their visible
publication predates that invariant. The wrapper fails closed when advisory
shared locking or resource identity is unavailable. Active preparation leases
are inherited by subprocesses. Build and startup hold profile, source, and
build-root leases only until a sealed runtime generation is published.
Foreground processes then retain the runtime-generation lease plus server state
when applicable;
supervisors and guardians retain only runtime-generation, process-tree,
server-state, and generation-specific port-reservation leases.

The common-Git `repository-layout.lock` is the bounded maintenance barrier for
schema/layout migration apply or restore. Ordinary exact-lease operations share
the barrier, so they overlap one another but cannot race an exclusive migration;
subprocesses inherit it only while they retain their exact operation leases.
Migration entry points require or derive this physical barrier rather than
falling back to the state-root-local legacy lock.

The supervisor records exact source commits/trees, build metadata, runtime
digests, state paths, and process start identities. `ps` without a name lists every recorded topology; a name
selects one. It distinguishes a live process from a reused PID, and `down`
signals only processes holding that topology's identity lease, including
orphaned services and descendants. The server state remains locked for the
complete supervised lifetime. Each topology also has a persistent isolated
client configuration/cache root and a complete sealed executable runtime, so
rebuilding a profile or changing/removing a selected worktree cannot alter a
live topology. Server-generated transport files live below a generation-named
directory inside the exclusively leased state and are reached through the
published state exception; copied maps are sealed beside that output and bound
into the manifest, while other served inputs remain sealed inside the runtime
generation. The inherited identity lease atomically
prevents the same topology name from restarting while any generation remains.
Server services do not inherit allocator, transaction, or generation
reservation descriptors; the supervisor and same-generation guardian retain
the exact reservation and release it after orderly shutdown or exact
process-tree recovery. `ps --json`
exposes its port, owner, generation, and retained/released observation without
credentials. Reservation records are never reclaimed from recorded PIDs.
Logs live below `workspace/topologies/`
and rotate at 10 MiB with three backups.

### Use case: run the latest playable classic branches

Use this after first cloning the wrapper, or when returning to development and
wanting a known-current baseline:

~~~sh
cd ~/atrinik
./atrinik init
./atrinik init --with classic
./atrinik sync --with classic
./atrinik topology show classic
./atrinik up --name classic-main --profile classic --default-state
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
git -C content fetch origin pull/CONTENT_PR/head:review/content-maps

./atrinik worktree create classic combined-classic \
  --branch review/combined-classic --from review/server-combat
./atrinik worktree create content maps-review \
  --branch review/content-maps --existing

./atrinik profile create combined-review --from classic
./atrinik profile set combined-review classic --worktree combined-classic
./atrinik profile set combined-review content --worktree maps-review

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
select only the shared `content@main` worktree:

~~~sh
./atrinik profile create map-check --from classic
./atrinik profile set map-check content --worktree maps-review
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
caches. A managed client's native window title includes both its topology and
profile, for example `Atrinik Client — topology classic-side - profile classic`.
Use the topology name shown there with `./atrinik ps NAME --json`,
`./atrinik logs NAME client`, and `./atrinik down NAME`; clients from the same
profile remain distinguishable by topology. Omit both `--port` options to have
the coordinator choose two available ports. Two live servers may not use the
same state directory; the state lock
turns that mistake into an immediate error instead of mutable-data corruption.
The same rule applies to parallel agents: allocate a unique topology name,
profile, state, and port for each run. Inspect or recover by exact topology
name (`./atrinik ps NAME --json`), never through a broad topology cleanup.

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
./atrinik cleanup --scope topologies combined-review --older-than 0 --dry-run --json
./atrinik cleanup --scope topologies combined-review --older-than 0 --apply --json
./atrinik profile set combined-review classic --primary
./atrinik profile set combined-review content --primary
./atrinik cleanup --scope worktrees --scope builds \
  classic content --older-than 7 --dry-run
./atrinik cleanup --scope worktrees --scope builds \
  classic content --older-than 7 --apply
~~~

The saved `combined-review` profile protects both selected worktrees until the
explicit `profile set` commands repoint it. Do that only when the retained
review selection and stopped topology history are no longer useful, then
preview each cleanup scope. Only the explicit `topologies` scope removes
eligible topology records and logs. Cleanup never removes profiles, scenarios,
registered or scenario state, migration evidence, commits, or branches. Only the explicit
`temporary-states` scope can reclaim an abandoned, unregistered, disposable
generation state after exact revalidation.

Delivery-ledger sidecars are not ordinary cleanup targets. A merge-ready PR
keeps its active coordinates and recovery evidence. After the PR is externally
merged and selected issues reach their expected states, a separately authorized
operator prepares exact post-merge evidence and runs:

Target base/head movement is likewise helper-owned: generic ledger CAS rejects
it. The target-refresh CAS pins the recorded primitive/scope worktree, proves
descendant commits and the exact live merge base, and rechecks immediately
before replacement. Fresh issue-mode draft PR binding is similarly
helper-owned: `bind-check` is only a diagnostic, while `pr-bind-cas` re-proves
the authenticated PR, comments, target, and bound worktree immediately before
its private CAS. An explicit recovery can correct one historical nonexistent
head plus its bound stale merge base while retaining both source generations.

~~~sh
python3 .agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py \
  release-preview build/reviews LEDGER_NAME release.json
python3 .agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py \
  release-apply build/reviews LEDGER_NAME release.json --plan PLAN_SHA256
~~~

Release uses a pinned `gh` against `github.com`, re-observes the exact terminal
PR/issue state and actor, and requires authority issued strictly after merge.
It holds wrapper leases while verifying the recorded clean worktree and either
Git ancestry or exact squash-change equivalence again after staging, then
durably marks the ledger inert; it removes nothing. Next run the relevant
`./atrinik cleanup --dry-run --json`, review it, and independently run the same
scoped command with `--apply`. Retain the exact raw preview/apply JSON in the
archive evidence; the helper validates both reports and
requires identical command coordinates, derives their canonical target
selection, verifies the wrapper's schema-2 journal request, original report,
completion, and timestamp order, and holds the exact wrapper coordinates through a
final absence recheck and archive installation.
An active scope can transition only when its live generation-matched scope
release journal is complete. That journal is the removal evidence for its
scope-produced worktree, which is not fabricated as a generic cleanup action.

After cleanup, a new explicit authority issued strictly after cleanup apply may bundle the canonical
ledger, release marker, lock, report, migration evidence, and retained intent:

~~~sh
python3 .agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py \
  archive-preview build/reviews LEDGER_NAME archive.json
python3 .agents/skills/atrinik-issue-delivery/scripts/delivery_ledger.py \
  archive-apply build/reviews LEDGER_NAME archive.json --plan PLAN_SHA256
~~~

The one bounded archive remains audit evidence without reserving active
coordinates. Once its recorded retention period has elapsed, use the
helper-clocked
`reclaim-preview` and pass the complete returned preview plus its digest to
`reclaim-apply`. Reclaim retains one fixed terminal completion checkpoint after
its quarantined exact-inode removal; the next successful reclaim replaces that
checkpoint, bounding review-root growth. These helper commands never delete worktrees, profiles,
topologies, state, branches, or runtime resources. See the issue-delivery
ledger reference for the strict evidence schemas and crash-recovery rules.

## Server state policies

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
Managed default, named, scenario, and temporary state records preserve their
distinct policy and owner identities; an optional implementation marker causes
future Classic/replacement mismatches to fail closed instead of sharing an
incompatible directory. Registered external paths remain outside wrapper
deletion ownership. Scenario state remains registered, scenario-owned, and
resettable only through `scenario reset`; it is not temporary topology state.
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
and starts with metaserver and STUN discovery disabled. Its native title shows
`profile PROFILE (direct run)` so it cannot be confused with a supervised
topology. The bounded launch identity exists only in the child process
environment; it is not saved to client configuration or used as package,
protocol, or network user-agent version data. Clients started outside the
wrapper retain the ordinary package title. Start the server first
so that its persistent `quic-identity.pem` exists. Additional arguments are
appended after these defaults, and join-password arguments are redacted from
command logging. Each command first publishes a random command-owned immutable
generation, releases source/layout/build leases, runs with only that generation
lease (and server state for the server), then reclaims the exact generation on
normal exit. Prefer `up` for routine use because it allocates a port,
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
python3 -m coverage run -m unittest discover -v --durations 50
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
./atrinik manifest validate
./atrinik cleanup --scope all --older-than 7 --dry-run --json
~~~

The required workflow runs deterministic measured shards, verifies exact-once
discovery, and combines branch coverage behind one stable `Integration
validation` check. Its latency budget and evidence method are documented in
[`docs/CI_PERFORMANCE.md`](docs/CI_PERFORMANCE.md).

For supported process-isolated local execution, see
[`docs/LOCAL_TESTING.md`](docs/LOCAL_TESTING.md). It covers targeted tests,
serial fallback, fast parallel runs, and parallel branch coverage.

Use `./atrinik build COMPONENT --profile classic --test` for current native
component integration. Replacement repositories run their standalone
aggregate validations today; wrapper builds still fail with a clear
unavailable-contract error until their adapters land. The `metaserver-worker`
wrapper contract runs its complete `npm run check` suite.
