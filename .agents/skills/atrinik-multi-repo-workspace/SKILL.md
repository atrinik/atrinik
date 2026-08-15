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

Release never stops topologies or deletes persistent state; uncertainty retains
recovery journals.

Reclaim review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

Repeat a scoped command with `--apply` after review. Defaults cover
worktrees/builds; caches and topology history are opt-in, and topologies are
excluded from `all`. Reclaim only stopped, released, exact marker-owned records;
unsafe or uncertain targets fail closed. Apply sound-cache before its worktree.

Delivery sidecars are not cleanup targets. Follow issue delivery's separate
terminal contract; its helper never removes workspace resources.

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

Classic selection is checkout-wide; subdirectories are not worktrees. Clean
target builds pin generated snapshots before releasing primaries; caches
use snapshot identity. Live inputs retain leases; interrupted staging is
cleanup-owned.

Lease order is registry, profile, Git-admin, source, topology/scenario, state,
build, cache. Writers gate matching coordinates; physical leases span state
roots and multi-source writers retry all-or-none. Only migration takes the
barrier exclusively. Published runtimes retain sealed generation, process-tree,
state, and port leases, not preparation leases. Fail closed without sharing.
Incomplete coordinates are inert; wrapper owns paths, locks, state, PIDs, logs,
content/resources and cleanup. Completion is bounded, local, read-only,
secret-free and parser-driven before `Workspace`; stops at `--` or a `run`
remainder.

For classic server execution or diagnosis, load `atrinik-server-runtime`. For
a ready account and character, also load `atrinik-test-scenario`; never
handcraft saves or expose credentials. Keep every concurrent topology on a
distinct name and state policy; prefer generation-owned temporary state for
isolated automation.

## Validate and hand off

Use wrapper-native commands wherever supported; a runtime handoff uses concrete
names and follows this lifecycle:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --temporary-state --json
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

Record prerequisites, actions, expected results, and cleanup. If runtime is
irrelevant, hand off applicable build/test/inspection commands. Do not
substitute internal executables or generated paths for supported wrapper
operations.

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
