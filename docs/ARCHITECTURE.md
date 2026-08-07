# Workspace architecture

## Ownership boundary

This repository owns only orchestration code and the component manifest.
Implementation, release packaging, and component-specific tests belong to the
standalone repositories listed in `components.json`. The workspace coordinator
does not vendor, pin, or commit component source.

The wrapper also owns the VS Code launch configurations that compose this
multi-repository workspace. The default configuration delegates repository
setup to `./atrinik init`, preserving existing checkouts while cloning missing
ones; the specialized Windows cross-build configuration validates the same
manifest after initialization. The standalone `devcontainer` repository owns
the versioned toolchain images, not the wrapper-specific launch configuration.

`components.json` is the source of truth for repository identity, default
branch, and local build contract. Profile and state schemas are intentionally
strict: duplicate keys, missing fields, unknown fields, invalid names, and
repository mismatches fail before an operation changes data.

Read-only inspection commands keep structured data on stdout and suppress Git
traces so callers can safely consume `status --json`, `worktree list --json`,
`profile show --json`, `state list --json`, and `path`. Status never fetches;
its ahead/behind fields describe the cached canonical remote ref and are null
when that ref is unavailable.

## Managed layout

~~~text
<wrapper>/
  <component>/                       primary independent Git checkouts
workspace/
  worktrees/<component>/<label>/     component Git worktrees
  profiles/<name>.json               checkout selectors
  build/profiles/<name>-<key>/       isolated sources, builds, and runtime
  build/npm-cache/                   shared package download cache
  topologies/<name>/                 supervised process state and rotated logs
  state/server/<name>/               persistent mutable server data
  states.json                        named external-state registry
~~~

`ATRINIK_WORKSPACE_DIR` relocates the `workspace/` layout but never the primary
component repositories beside the wrapper. Exact root-level component names
and clone-staging directories are ignored by the wrapper repository. Existing
paths are validated as the expected standalone Git repository and are never
overwritten. The top-level workspace, replaceable build directories, and
generated views carry schema-versioned ownership markers. Replacement helpers
require the exact expected marker and verify that the target remains below the
build root.

Component initialization derives its GitHub clone transport from the wrapper's
first recognized `origin` or `upstream` URL. SSH and HTTPS wrapper clones
therefore produce component clones with the same authentication transport;
public HTTPS is the fallback when the wrapper has no recognized GitHub remote.

## Profile resolution and build flow

A profile maps every manifest component to its primary checkout, a managed
worktree label, or an absolute external checkout. Resolution verifies that each
path is a Git worktree whose `origin` or `upstream` identifies the expected
`atrinik/*` repository.

The normalized absolute paths are hashed into the profile build key. A fully
initialized workspace uses the common buildable-component selection so
`build all`, component builds, and launches share incremental output. A partial
workspace uses only the requested target's dependency closure. This separates
worktree combinations while preserving compiler output when the same worktrees
advance. The coordinator creates disposable source views rather than writing
dependency links or output into source checkouts.

The playable build flow is:

~~~text
selected content -> build_runtime.py -> isolated content/lib + content/maps
selected tracked resource allowlist -> isolated resource view
selected protocol + library + sound -> client source view -> CMake/Ninja
selected protocol + library --------> server source view -> CMake/Ninja
selected Worker --------------------> npm source view -> npm run check
~~~

The server source view uses a copied `install_data` directory because its CTest
preparation uses CMake directory-copy semantics. Other authored inputs remain
links to their selected worktrees. The Worker view is copied because Node's
module resolver follows configuration-file links and would otherwise search the
component checkout instead of the isolated dependency directory. Collected
content and resource dependency metadata identify the selected path and commit.
The resources repository's `runtime-paths.txt` is the distribution boundary:
only tracked regular files below those paths enter the runtime view. This keeps
repository metadata, local untracked files, and symlinks out of the asset
protocol.

## Runtime and state

Server launch preparation assembles a disposable working directory with links
to the selected build, plugins, collected content, resources, configuration,
tools, and named persistent state. First use initializes a state atomically from
the selected server's `install_data`; an existing directory is validated and
never overlaid. State inside a server source worktree is rejected.

The coordinator takes an advisory exclusive lock next to the state directory
before build/runtime preparation and holds it for the lifetime of a launched
server. Profile builds have their own blocking lock, and server launch views are
keyed by state path. Processes started outside the coordinator do not
participate in the state lock, so operators must not point those processes at
the same state concurrently.

Profiles are source-topology definitions for supervised runtime as well as
builds. `up` resolves the requested service dependency closure once, records
the exact paths/commits and build root, prepares the same isolated views used by
foreground launches, and hands the state-lock file descriptor through a short
forking bootstrap to a detached native supervisor and its server child. The
lock remains held for the server lifetime without a long-lived invoking CLI
process.

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
conflict.

Test scenarios are owned directories below `workspace/scenarios/`, each with a
strict metadata record, ownership marker, mode-0600 password file, and dedicated
registered server state. Creation resolves and builds the scenario profile,
initializes state from the selected server's `install_data`, and invokes that
server in offline provisioning mode. The server uses its normal validation,
Argon2id password, atomic account-save, and exclusive player-reservation paths;
the empty player record causes first login to follow normal character creation.
No scenario secret is stored in metadata or passed as a process argument.
The metadata records path, commit, and dirty status for the selected server,
content, resources, protocol, and library inputs so an audit does not imply
that a scenario provisioned from local edits came from a clean commit.

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

Component worktrees are executable source: CMake, Python collection, npm, and
runtime commands execute code from the selected profile. Review a pull request
before selecting its worktree. Profiles do not provide a security sandbox.

Subprocesses use argument arrays rather than a shell. User-provided executable
arguments are not evaluated as shell syntax, and recognized join-password
forms are redacted from diagnostics. Git operations refuse dirty primary
checkouts and dirty managed worktrees where an automatic update or removal
could lose work.

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
version.
