---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---

# Atrinik multi-repository workspace

## Resolve scope and ownership

Run from the `atrinik/atrinik` wrapper root.

1. Read root `AGENTS.md` and task-relevant wrapper sources:
   - ownership, cohorts, roles, and build contracts: `components.json`;
   - operator behavior: the matching `README.md` section;
   - layout, locking, trust, and lifecycle design: the matching
     `docs/ARCHITECTURE.md` section;
   - pre-split repositories: [repository migration](references/repository-migration.md).
2. Resolve each component to its physical checkout and safe source root;
   read that checkout's nearest `AGENTS.md` before editing.
3. Keep implementation, tests, packages, and releases in the physical owner;
   keep only orchestration, composition, manifest, and wrapper docs here.

Checkouts are independent ignored repositories. A `classic` worktree contains
all five `classic-*` components. Both stacks share `content@main`; retained
`content-1x` paths are historical. Keep composition in `.devcontainer/`.

## Prepare safe worktrees

Inspect local state before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize only absent repositories. Plain `init` selects the replacement
cohort; exact `--with classic` adds the complete classic cohort. `sync` never
clones.

```sh
./atrinik init [COMPONENT...]
./atrinik init --with classic
./atrinik sync [COMPONENT...]
./atrinik scope create COMPONENT... --name REVIEW --from PROFILE --json
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Sync only clean primaries; never replace, move, or remove dirty sources.
Classic selectors create the full repository under
`workspace/worktrees/classic/LABEL`. Commit and push in its owning worktree.
Prefer atomic scopes for concurrent agents; generated names are
collision-resistant, exact retries idempotent, and JSON commands secret-free.
Temporary state is the default; persistent state is deliberate. Primitives
remain supported. Release only with the freshly previewed plan digest:

```sh
./atrinik scope release REVIEW --dry-run --json
./atrinik scope release REVIEW --apply --plan PLAN_SHA256 --json
```

Release never stops topologies or deletes persistent state. After `down`, exact
stopped scope history permits release; mismatched or uncertain references fail
closed. Interrupted journaled actions are retained: preview again, apply the
new digest, and resume completed actions.

Reclaim review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

Repeat a scoped command with `--apply` after review. Defaults cover worktrees
and builds; caches and topology history are opt-in, and `all` excludes
topologies. Reclaim only stopped, released, marker-owned records; uncertainty
fails closed. Apply sound-cache before its worktree. See `README.md` and
`docs/ARCHITECTURE.md` for historical proof and apply-time revalidation.

## Compose coherent sources

Use `default` for replacement sources and `classic` for the playable stack.
Never mix providers or substitute classic C/CMake for a missing replacement
adapter. Replacement repositories have standalone M1 foundations, but no
wrapper replacement build/runtime closure yet.

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW CHECKOUT_OR_COMPONENT --worktree LABEL
./atrinik profile show REVIEW --json
./atrinik path COMPONENT --profile REVIEW
./atrinik build COMPONENT --profile REVIEW --test
```

Classic selection is checkout-wide; subdirectories are not worktrees. Builds
pin snapshots before releasing primaries; caches use snapshot identity. Live
inputs retain leases; cleanup owns interrupted staging.

Lease order is registry, profile, Git-admin, source, topology/scenario, state,
build, cache. Writers gate matching coordinates; multi-source writers retry
all-or-none. Only migration takes the barrier exclusively. Published runtimes
retain generation, process-tree, state, and port leases. Incomplete coordinates
are inert; wrapper owns generated resources and cleanup. Completion is bounded,
local, read-only, secret-free, parser-driven before `Workspace`, and stops at
`--` or a `run` remainder.

Concurrency verification composes returned scope commands: use distinct
worktrees of one checkout, rendezvous on observable build/readiness markers,
keep A live through B's release, then release A. Count ownership transitions
and conflicts; timeouts bound failure, not compiler speed.

For classic execution load `atrinik-server-runtime`; for a ready character also
load `atrinik-test-scenario`. Never handcraft saves or expose credentials. Give
concurrent topologies distinct names and state; prefer temporary state.

## Validate and hand off

Use concrete names and wrapper-native lifecycle commands:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --temporary-state --json
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
./atrinik scope release SCOPE --dry-run --json
./atrinik scope release SCOPE --apply --plan PLAN_SHA256 --json
```

Record prerequisites, results, and cleanup. Do not substitute internal paths
for wrapper operations.

## Coordinate publication and policy

Use `atrinik-github-governance` for PRs or policy. PR titles use
`type(optional-scope)!: concise description`; bodies require renderable
GitHub-Flavored Markdown and actual line breaks, never visible literal `\n`
separators. Feed multi-section bodies by file or standard input; after
create/edit, verify the remote GitHub render.
Semantic-release owns publication. Dependency changes update the inventory and
audit a complete profile. Follow `docs/PROVENANCE.md` for Classic reuse and
fail uncertainty closed.

## Maintain guidance

Load `atrinik-guidance-maintenance` when wrapper ownership, CLI, layout,
profile/build/runtime, cleanup, release, or cross-repository contracts change.
Update only affected canonical guidance, remove superseded instructions, and
run that skill's inventory and validation workflow.
