# Development after the repository split

The integration repository assembles released component snapshots. Edit a
component in its own repository and use this repository when validating a set
of released versions together.

## Build and run the pinned integration snapshot

From the integration repository root inside the Linux devcontainer:

```sh
python3 scripts/components.py sync
scripts/build.sh
```

The build verifies all component locks, builds and tests both native programs,
and prepares the server runtime. Run the client from its component working
directory:

```sh
cd build/components/client
../../integration/client/atrinik
```

Run the server through its prepared launcher:

```sh
cd build/components/server
./server.sh --port_mapping=off --stun_server=off
```

The client command opens a graphical window and may enable audio. The server
command starts the game service; the shown flags disable automatic port mapping
and STUN discovery. `./server.sh --version` is the bounded smoke check used by
the integration build.

## Editable component worktrees

Keep one primary clone for each component and add feature worktrees beside it.
For example:

```sh
mkdir -p /workspaces/atrinik-dev/repos /workspaces/atrinik-dev/worktrees
gh repo clone atrinik/client /workspaces/atrinik-dev/repos/client
gh repo clone atrinik/server /workspaces/atrinik-dev/repos/server

git -C /workspaces/atrinik-dev/repos/client fetch origin
git -C /workspaces/atrinik-dev/repos/client worktree add \
  /workspaces/atrinik-dev/worktrees/client-ui -b feat/client-ui origin/master

git -C /workspaces/atrinik-dev/repos/server fetch origin
git -C /workspaces/atrinik-dev/repos/server worktree add \
  /workspaces/atrinik-dev/worktrees/server-rules -b feat/server-rules origin/master
```

Each component worktree owns its build directory and dependency checkout. This
avoids CMake cache collisions while ccache still shares compiled objects. Use
the component README for its exact build and test commands.

List and remove worktrees through the primary clone:

```sh
git -C /workspaces/atrinik-dev/repos/server worktree list
git -C /workspaces/atrinik-dev/repos/server worktree remove \
  /workspaces/atrinik-dev/worktrees/server-rules
git -C /workspaces/atrinik-dev/repos/server worktree prune
git -C /workspaces/atrinik-dev/repos/server branch -d feat/server-rules
```

Inspect `git status --short` before removal. Avoid `--force` unless the
worktree's uncommitted and ignored files have been deliberately preserved or
discarded.

## One mutable server data directory

Never copy live accounts, identities, generated maps, or other mutable server
state into every worktree. The integration helper initializes one external
directory, links a server worktree's ignored `data` path to it, prepares that
worktree's executable/plugins/content links, and takes an exclusive lock while
the server runs:

```sh
scripts/server-worktree.sh prepare \
  --worktree /workspaces/atrinik-dev/worktrees/server-rules

scripts/server-worktree.sh run \
  --worktree /workspaces/atrinik-dev/worktrees/server-rules \
  -- --port_mapping=off --stun_server=off
```

By default, linked integration worktrees share
`<primary-integration-clone>/build/shared/server-data`. To keep state elsewhere,
set one absolute path consistently:

```sh
export ATRINIK_SHARED_SERVER_DATA=/workspaces/atrinik-state/server-data
```

The helper refuses to replace an existing real `data` directory or a symlink
to a different location. Back up and deliberately migrate existing state
before linking it. It also refuses a shared directory inside the server
worktree and prevents concurrent server processes from writing the same data.
Removing a component worktree removes only its symlink; the external data and
lock file remain. Back up the shared directory independently before testing a
change that can rewrite accounts, identities, or persistent world state.
