# Atrinik development workspace

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

- Python 3.11 or newer
- Git and the authenticated GitHub CLI (`gh`)
- CMake, Ninja, and the dependencies required by the selected component
- Node.js 20 or newer when building `metaserver-worker`

The Atrinik development container supplies the native build dependencies. Run
all commands below from this repository's root.

## Quick start

~~~sh
./atrinik init
./atrinik build all --test
./atrinik run server
./atrinik run client
~~~

`init` clones the repositories in `components.json` directly into the wrapper
root, such as `./client` and `./server`.
Rerunning it is safe and validates existing checkouts without changing them.
`build all` builds the protocol, shared library, client, server, collected
content, and metaserver Worker. Repositories without a deterministic local
build contract remain available for direct development at `./COMPONENT/`.
Partial initialization is supported: for example, `./atrinik init content`
followed by `./atrinik build content` needs no unrelated checkout.

The client command opens a graphical application. Verify that the devcontainer
display forwarding socket is live before launching it. Use `--dry-run` to
build and print either launch command without starting the process.

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
  --branch fix/release --from origin/master
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
repository. Client builds similarly expose the selected sound checkout. These
operations never write generated files into a component checkout.

## Persistent server state

The built-in `default` state is initialized once at
`workspace/state/server/default` and then reused by every profile. Register a
second managed state or an existing compatible directory when isolation is
useful:

~~~sh
./atrinik state add review
./atrinik state add existing --path /absolute/path/to/server-data
./atrinik state list
./atrinik run server --profile maps-review --state review
~~~

State contains player, key, unique-item, and other mutable runtime data. It is
never regenerated over an existing directory. A nonblocking lock prevents two
coordinator-launched servers from using the same state simultaneously.

Arguments after `--` are forwarded to the selected executable:

~~~sh
./atrinik run server --profile maps-review --state review -- --version
./atrinik run client --profile maps-review -- --help
~~~

Join-password arguments are redacted from command logging. With no explicit
arguments, the server disables automatic port mapping and STUN discovery for a
local development launch.

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
python3 -m unittest discover -v
python3 -m compileall -q atrinik atrinik_workspace tests
./atrinik manifest validate
~~~

Use `./atrinik build COMPONENT --test` for component integration. The
`metaserver-worker` contract always runs its complete `npm run check` suite.
