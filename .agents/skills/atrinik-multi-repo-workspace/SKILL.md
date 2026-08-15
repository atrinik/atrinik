---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---

# Atrinik multi-repository workspace

## Resolve scope and ownership

Run from the wrapper root.

1. Read `AGENTS.md`, `components.json`, and relevant README/architecture
   sections. For pre-split repositories, see
   [repository migration](references/repository-migration.md).
2. Resolve each physical checkout and source root; read its nearest
   `AGENTS.md` before editing.
3. Keep code, tests, packages, and releases with their physical owner; only
   orchestration, composition, manifest, and wrapper docs live here.

Checkouts are ignored repositories. A `classic` worktree holds all
five `classic-*` components. Both stacks share `content@main`; `content-1x` is
historical. Composition belongs in `.devcontainer/`.

## Prepare safe worktrees

Inspect local state before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize only absent repositories. `init` selects replacement; `--with
classic` adds classic. `sync` never clones.

```sh
./atrinik init [COMPONENT...]
./atrinik init --with classic
./atrinik sync [COMPONENT...]
./atrinik scope create COMPONENT... --name REVIEW --from PROFILE --json
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Sync only clean primaries; never replace, move, or remove dirty sources.
Classic selectors create `workspace/worktrees/classic/LABEL`; work there.
Prefer atomic scopes: names resist collisions, retries are idempotent, and JSON
is secret-free. Temporary state is the default.
Release with the fresh preview digest:

```sh
./atrinik scope release REVIEW --dry-run --json
./atrinik scope release REVIEW --apply --plan PLAN_SHA256 --json
```

Release never stops topologies or deletes persistent state. Exact clean-down
permits it. Interrupted substeps resume after a fresh preview; uncertainty
fails closed and journals remain.

Reclaim review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

After review, repeat the scoped command with `--apply`. Defaults cover
worktrees/builds; caches and topology history are opt-in; `all` excludes
topologies. Reclaim only stopped, released, exact marker-owned records;
uncertainty fails closed. Apply sound-cache before its worktree. README and
architecture define historical proof and apply-time revalidation.

## Compose coherent sources

Use `default` for replacement sources and `classic` for the playable stack.
Never mix providers or substitute classic C/CMake for a missing adapter.
Replacement repositories lack wrapper build/runtime closure.

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW CHECKOUT_OR_COMPONENT --worktree LABEL
./atrinik profile show REVIEW --json
./atrinik path COMPONENT --profile REVIEW
./atrinik build COMPONENT --profile REVIEW --test
```

Classic selection is checkout-wide; subdirectories are not worktrees. Builds
pin snapshots and caches use their identity. Live inputs retain leases;
cleanup owns staging.

Lease order is registry, profile, Git-admin, source, topology/scenario, state,
build, cache. Writers gate matching coordinates; physical leases span state
roots and multi-source writers retry all-or-none. Only migration takes the
barrier exclusively. Published runtimes retain sealed generation, process-tree,
state, and port leases, not preparation leases. Fail closed without sharing.
Incomplete coordinates are inert. The wrapper owns paths, locks, state, PIDs,
logs, content/resources, and cleanup. Completion is bounded, local, read-only,
secret-free, and parser-driven before `Workspace`; it stops at `--` or `run`.

Verify concurrency with distinct worktrees, observable build/readiness
rendezvous, and A live through B's release. Count transitions and conflicts;
timeouts bound failure, not compiler speed.

For classic execution load `atrinik-server-runtime`; for a ready character add
`atrinik-test-scenario`. Never handcraft saves or expose credentials. Give
concurrent topologies distinct names/state; prefer temporary state.

## Validate and hand off

Use wrapper commands and concrete names:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --temporary-state --json
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

Record prerequisites, actions, results, and cleanup. If runtime is irrelevant,
hand off build/test/inspection commands. Never substitute internal executables
or generated paths for wrapper operations.

## Coordinate publication and policy

Use `atrinik-github-governance` for PRs or policy. Titles use
`type(optional-scope)!: concise description`; bodies require renderable
GitHub-Flavored Markdown and actual line breaks, never visible literal `\n`
separators. Feed multi-section bodies by file/stdin. After create/edit, verify remote
render.
Semantic-release owns publication. Dependency changes update the inventory and
audit a complete profile. Follow `docs/PROVENANCE.md` for Classic reuse and
fail uncertainty closed.

## Maintain guidance

Load `atrinik-guidance-maintenance` when wrapper or cross-repository contracts
change. Update affected canonical guidance, remove superseded instructions,
and run its inventory and validation workflow.
