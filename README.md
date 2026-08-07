# Atrinik development workspace

[![Coverage](https://codecov.io/gh/atrinik/atrinik/graph/badge.svg?branch=main)](https://codecov.io/gh/atrinik/atrinik)

This repository is a small, MIT-licensed coordinator for Atrinik's standalone
component repositories. Each component remains a normal Git checkout with its
own branches, remotes, commits, and worktrees. The coordinator supplies one
command for cloning and synchronizing them, composing arbitrary worktrees into
review profiles, building the resulting system, and sharing persistent server
state safely.

No component is a submodule. Primary component repositories are ignored,
independent Git checkouts directly beside this README (`./client`, `./server`,
and so on). Generated worktrees, builds, profiles, and default state live under
the ignored `workspace/` directory.

## Requirements

- Python 3.11 or newer; supervised topologies require Linux process identity
  and pidfd support
- Git and the authenticated GitHub CLI (`gh`)
- CMake, Ninja, and the dependencies required by the selected component
- Node.js 20 or newer when building `metaserver-worker`

The Atrinik development container supplies the native build dependencies. Run
all commands below from this repository's root.

### Development container

Open this wrapper repository in VS Code and choose **Dev Containers: Reopen in
Container** to use the pinned Linux build environment. On first creation, the
container runs `./atrinik init`; it clones missing component repositories and
validates existing checkouts without updating or replacing them. The Windows
cross-build configuration is available at
`.devcontainer/windows-cross/devcontainer.json` after `./atrinik init` has
prepared the component checkouts.

The wrapper owns these launch configurations because they compose the complete
development workspace. The standalone `devcontainer` component owns only the
published Linux and Windows toolchain images they reference.

## Dependency and supply-chain ownership

`supply-chain/inventory.json` records every supported repository and the owned
toolchains, actions, images, source archives, system libraries, optional tools,
vendored sources, licenses, update cadences, EOL responses, and validation
paths they consume. The strict schema is checked without a third-party Python
dependency. GitHub Actions and container images use immutable commits or
digests with human-readable update hints, and every active repository enables
weekly Dependabot updates for its supported ecosystems. Git submodules are not
a supported dependency path.

Validate the catalog alone or audit the exact checkouts selected by a profile:

~~~sh
./atrinik supply-chain validate
./atrinik supply-chain audit --profile default
./atrinik supply-chain versions --output build/supply-chain/versions.json
./atrinik supply-chain report --format licenses \
  --output build/supply-chain/licenses.md
./atrinik supply-chain report --format cyclonedx \
  --output build/supply-chain/cyclonedx.json
./atrinik supply-chain report --format spdx \
  --output build/supply-chain/spdx.json
~~~

Generated reports remain ignored under `build/`. A scheduled organization
audit checks out every active repository, rejects unowned dependency inputs,
movable workflow/image references, and submodules, prints exact available tool
versions, and publishes deterministic license, CycloneDX, and SPDX artifacts.
When auditing a wrapper worktree that cannot safely share its containing
workspace directory, use repeated absolute `--repository NAME=PATH` overrides;
the command verifies each override's GitHub repository identity before reading
it.

## Quick start

~~~sh
./atrinik init
./atrinik build all --test
./atrinik up
./atrinik ps
./atrinik logs default --follow
# In another terminal:
./atrinik down
~~~

`init` clones the repositories in `components.json` directly into the wrapper
root, such as `./client` and `./server`.
It follows the GitHub transport of the wrapper's `origin` (then `upstream`): an
SSH wrapper clone produces SSH component clones, while an HTTPS wrapper clone
produces HTTPS component clones. If neither remote supplies a recognized
GitHub URL, initialization falls back to public HTTPS.
Rerunning it is safe and validates existing checkouts without changing them.
`build all` builds the protocol, shared library, client, server, collected
content, and metaserver Worker. Repositories without a deterministic local
build contract remain available for direct development at `./COMPONENT/`.
Partial initialization is supported: for example, `./atrinik init content`
followed by `./atrinik build content` needs no unrelated checkout.

The client command opens a graphical application. Verify that the devcontainer
display forwarding socket is live before launching it. Use `--dry-run` to
build and print either launch command without starting the process.

### Manual verification handoffs

Change handoffs should end with a copy-pasteable verification recipe that uses
the thin wrapper instead of internal build paths or direct component binaries.
Use the actual profile, topology, and state names for the change. A runtime
client/server review normally follows this shape:

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
./atrinik status client server --json
~~~

Status is deliberately fast and does not contact GitHub. Ahead/behind counts
compare against each checkout's cached remote default-branch ref; run `sync` to
fetch and fast-forward when current remote state is required. JSON output is
stable machine-readable input for scripts and AI tools.

Fast-forward every primary checkout to its configured default branch:

~~~sh
./atrinik sync
~~~

Limit the operation to named components, or also update every clean attached
feature worktree from the newly synchronized primary branch:

~~~sh
./atrinik sync client server
./atrinik sync --worktrees merge
./atrinik sync --worktrees rebase
~~~

The command stops instead of changing a dirty checkout. Worktree merge or
rebase conflicts are left in that worktree for normal Git resolution.

## Component worktrees

Create a branch and worktree from the component's remote default branch:

~~~sh
./atrinik worktree create server socket-review --branch feat/socket-review
~~~

Choose another start point or attach an existing local branch:

~~~sh
./atrinik worktree create client release-fix \
  --branch fix/release --from origin/main
./atrinik worktree create content maps-pr \
  --branch review/maps-pr --existing
~~~

List or remove managed worktrees:

~~~sh
./atrinik worktree list
./atrinik worktree list --json
./atrinik worktree remove content maps-pr
~~~

Removal refuses dirty worktrees. Each worktree is a full Git worktree of its
component repository, so ordinary `git commit`, `git push`, and `gh pr create`
work from inside it.

## Mixing component worktrees

A profile chooses one checkout for every component. This makes it possible to
review content from one pull request with client and server code from their
primary checkouts, or to combine several coordinated branches.

~~~sh
./atrinik profile create maps-review
./atrinik profile set maps-review content --worktree maps-pr
./atrinik profile set maps-review server --worktree socket-review
./atrinik profile show maps-review
./atrinik build all --profile maps-review --test
~~~

Clone an existing profile when starting a related combination instead of
repeating every selector:

~~~sh
./atrinik profile create maps-review-2 --from maps-review
~~~

An existing checkout outside the managed workspace can also be selected:

~~~sh
./atrinik profile set maps-review client --path /absolute/path/to/client
~~~

The repository identity is validated for every selector. Generated build trees
are keyed by the profile's resolved paths, so profiles and worktree combinations
do not overwrite one another.

Print an exact selected checkout path for use with `cd`, editors, or other
tools. Read-only profile, worktree, and state listings also support `--json`.

~~~sh
cd "$(./atrinik path server --profile maps-review)"
./atrinik profile show maps-review --json
./atrinik state list --json
~~~

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
profile plus every resolved provisioning dependency's path, commit, and dirty
status. Account files contain the normal
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

`up` resolves the selected client, server, content, protocol, library, sound,
and resources automatically. It builds the required closure, collects content,
stages resources/sound, prepares an isolated runtime, and starts both game
processes under one supervisor. A server gets an available UDP port by default;
use `--port` when a stable port is useful. The supervisor waits for both the
QUIC certificate fingerprint and completed server initialization, then gives
the paired client an authenticated loopback endpoint, disables metaserver and
STUN discovery in that client, and disables STUN discovery and automatic port
mapping in the server before declaring the topology ready. Use `--service server` or
`--service client` for a single service. Client startup requires a live
forwarded display socket.

The supervisor records exact source commits, build and state paths, and process
start identities. `ps` without a name lists every recorded topology; a name
selects one. It distinguishes a live process from a reused PID, `down` signals
only the matching supervisor, and the server state remains locked for the
complete supervised lifetime. Each topology also has a persistent isolated
client configuration/cache root and its own collected content and resource
snapshot, so a subsequent build cannot replace files underneath a running
topology. Logs live below `workspace/topologies/` and rotate at 10 MiB with
three backups.

### Use case: run the latest main branches

Use this after first cloning the wrapper, or when returning to development and
wanting a known-current baseline:

~~~sh
cd ~/atrinik
./atrinik init
./atrinik sync
./atrinik topology show default
./atrinik up
./atrinik ps
./atrinik logs default server --follow
# Press Ctrl-C to stop following logs; the services keep running.
./atrinik down
~~~

This is useful for ordinary playtesting and for proving that a problem also
exists without any feature worktree. `sync` refuses dirty primary repositories,
so it cannot silently overwrite in-progress component work.

### Use case: develop one component without copying the whole project

Create the branch and worktree in its owning repository, then select it in a
profile. Everything else stays on its primary checkout:

~~~sh
./atrinik worktree create server socket-fix \
  --branch fix/socket-timeout
./atrinik profile create socket-fix
./atrinik profile set socket-fix server --worktree socket-fix
./atrinik topology show socket-fix
./atrinik up --profile socket-fix --state default

cd "$(./atrinik path server --profile socket-fix)"
git status
# Edit, test, commit, and push here as in any normal Git repository.

cd ~/atrinik
./atrinik logs socket-fix server --follow
./atrinik down socket-fix
~~~

This is useful for a client- or server-only pull request: the wrapper rebuilds
only the affected dependency closure and supplies all unchanged repositories
without nesting their histories into the feature branch.

### Use case: combine client, server, and content pull requests

Fetch each pull request into a local review branch, attach its worktree, then
compose the three labels. Substitute the actual pull-request numbers:

~~~sh
git -C client fetch origin pull/CLIENT_PR/head:review/client-ui
git -C server fetch origin pull/SERVER_PR/head:review/server-combat
git -C content fetch origin pull/CONTENT_PR/head:review/content-maps

./atrinik worktree create client ui-review \
  --branch review/client-ui --existing
./atrinik worktree create server combat-review \
  --branch review/server-combat --existing
./atrinik worktree create content maps-review \
  --branch review/content-maps --existing

./atrinik profile create combined-review
./atrinik profile set combined-review client --worktree ui-review
./atrinik profile set combined-review server --worktree combat-review
./atrinik profile set combined-review content --worktree maps-review
./atrinik topology show combined-review --json
./atrinik up --profile combined-review --state review
./atrinik ps combined-review --json
./atrinik logs combined-review --follow
./atrinik down combined-review
~~~

This is the main cross-repository review workflow. Content is collected from
the selected content worktree before launch, while client/server sources use
their selected protocol, library, sound, and resource inputs. The JSON topology
output is an exact bill of materials for review notes or agent automation.

### Use case: review new maps with unchanged binaries

Keep client and server on their synchronized primary checkouts and select only
the content worktree:

~~~sh
./atrinik profile create map-check
./atrinik profile set map-check content --worktree maps-review
./atrinik topology show map-check
./atrinik up --profile map-check --state review
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
./atrinik up --profile combined-review --state shared
./atrinik down combined-review
./atrinik up --profile default --state shared
./atrinik down default
~~~

This is useful for comparing a feature topology with main using the same
accounts and world state. Run them sequentially: the coordinator deliberately
refuses to start two servers against the same state directory at once.

### Use case: compare two source combinations safely

Clone a profile, change one selector, inspect both bills of materials, and run
them one after the other against the same named state:

~~~sh
./atrinik profile create candidate-b --from combined-review
./atrinik profile set candidate-b client --primary

./atrinik topology show combined-review --json
./atrinik topology show candidate-b --json

./atrinik up --profile combined-review --state shared
# Exercise the first combination.
./atrinik down combined-review

./atrinik up --profile candidate-b --state shared
# Repeat the same exercise with the alternate client.
./atrinik down candidate-b
~~~

Separate path-keyed build trees prevent the two combinations from overwriting
one another, while the common state makes the comparison repeatable.

### Use case: run two topologies at the same time

Use distinct topology names, server states, and ports to run a baseline beside
a candidate. The profile selects source code; `--name` selects the runtime
instance, so the same profile can also be launched more than once:

~~~sh
./atrinik state add baseline
./atrinik state add candidate

./atrinik up --name baseline --profile default \
  --state baseline --port 17300
./atrinik up --name candidate --profile candidate-b \
  --state candidate --port 17301

./atrinik ps
./atrinik logs baseline server --follow
./atrinik logs candidate client --follow

./atrinik down baseline
./atrinik down candidate
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
./atrinik up --profile combined-review --state review --service server
./atrinik ps combined-review
./atrinik logs combined-review server --follow
./atrinik down combined-review
~~~

This is useful for protocol, content, runtime, and server work where opening a
graphical client would be distracting or impossible.

### Use case: clean up after a completed review

Stop the topology before removing component worktrees. Removal refuses dirty
worktrees, so commit, stash, or otherwise preserve intentional edits first:

~~~sh
./atrinik down combined-review
./atrinik worktree remove client ui-review
./atrinik worktree remove server combat-review
./atrinik worktree remove content maps-review
~~~

Profiles and rotated logs are small ignored metadata and may remain as a review
record. Component commits and branches remain owned by their standalone Git
repositories.

## Persistent server state

The built-in `default` state is initialized once at
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

Primary component repositories remain directly beside the wrapper even when
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
~~~

Use `./atrinik build COMPONENT --test` for component integration. The
`metaserver-worker` contract always runs its complete `npm run check` suite.
