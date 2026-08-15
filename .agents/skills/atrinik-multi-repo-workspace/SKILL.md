---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---

# Atrinik multi-repository workspace

## Resolve scope and ownership

Run from the wrapper root.

1. Read `AGENTS.md`, `components.json`, and relevant README/architecture. For
   pre-split repositories, see
   [repository migration](references/repository-migration.md).
2. Resolve each physical checkout and source root; read its nearest
   `AGENTS.md` before editing.
3. Keep code, tests, packages, and releases with their physical owner; keep
   orchestration and wrapper contracts here.

Checkouts are ignored repositories. One `classic` worktree holds all five
`classic-*` components. Both stacks share `content@main`; `content-1x` is
historical.

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
Classic selectors create `workspace/worktrees/classic/LABEL`. Prefer atomic,
idempotent scopes and temporary state.
Release with the fresh preview digest:

```sh
./atrinik scope release REVIEW --dry-run --json
./atrinik scope release REVIEW --apply --plan PLAN_SHA256 --json
```

Release never stops topologies or deletes persistent state. Interrupted steps
resume after a fresh preview; uncertainty retains journals.

Reclaim review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

After review, repeat the scoped command with `--apply`. Defaults cover
worktrees/builds; caches/history are opt-in; `all` excludes topologies. Reclaim
only stopped, released, exact marker-owned records; uncertainty fails closed.
Apply sound-cache before its worktree. Retry interrupted apply with identical
arguments; its write-ahead journal resumes the original selection.

Delivery sidecars are not cleanup targets. Follow issue delivery's separate
terminal contract; its helper never removes workspace resources.

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
pin snapshots; live inputs retain leases and cleanup owns staging.

Lease order is registry, profile, Git-admin, source, topology/scenario, state,
build, cache. Gate matching coordinates; multi-source writers retry
all-or-none. Only migration takes the barrier exclusively. Published runtimes
retain generation, process-tree, state, and port leases. Fail closed without
sharing; incomplete coordinates are inert. Completion is bounded, local,
read-only, secret-free, and parser-driven before `Workspace`.

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

Record prerequisites, actions, results, and cleanup. Hand off applicable
build/test/inspection commands. Never replace wrapper operations with internal
executables or generated paths.

## Coordinate publication and policy

Use `atrinik-github-governance` for PRs or policy. Titles use
`type(optional-scope)!: concise description`; bodies require renderable
GitHub-Flavored Markdown and actual line breaks, never visible literal `\n`
separators. Feed multi-section bodies by file/stdin. After create/edit, verify remote
render.
Semantic-release owns publication. Dependency changes update inventory and
audit a profile. Follow `docs/PROVENANCE.md`; fail uncertainty closed.

## Maintain guidance

Load `atrinik-guidance-maintenance` when wrapper or cross-repository contracts
change. Update affected canonical guidance, remove superseded instructions,
and run its inventory and validation workflow.
