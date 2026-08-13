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
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Sync only clean primaries; never implicitly replace, move, or remove a
dirty checkout or worktree. A logical classic selector creates the full
repository under `workspace/worktrees/classic/LABEL`. Commit and push from each
owning worktree.

Reclaim review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --apply
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
```

Default cleanup covers worktrees/builds; caches are opt-in. Apply sound-cache
before a fresh worktree preview because each cache protects its worktree. Sound
build/verify shares the versioned Git-admin lease; removal locks its inode.
Missing, replaced, invalid, busy, referenced, or unsafe targets fail closed.
The sole historical-base exception is the frozen
`atrinik/atrinik@main` `build/worktrees/` contract at
`ee5ba2096c94bce0161629423d4962a966bc61d8`. Apply locks and revalidates each
exact target with its reference-publication coordinates, journals completed
actions, skips busy targets, and uses non-force Git while preserving refs,
state, records, objects, reports, and unmarked paths.

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
exact-coordinate content, resource, and region-map caches are reused; runtimes
copy independent topology-owned snapshots.

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
distinct name and server state.

## Validate and hand off

Use wrapper-native commands wherever supported; a runtime handoff uses concrete
names and follows this lifecycle:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --json
./atrinik up --name TOPOLOGY --profile PROFILE --state STATE
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

Record prerequisites, actions, expected results, and cleanup. If runtime is
irrelevant, hand off applicable build/test/inspection commands. Do not
substitute internal executables or generated paths for supported wrapper
operations.

## Coordinate publication and policy

- Use `atrinik-github-governance` for PR publication or policy; compare live and
  desired state before authorized policy mutations.
- PR titles use `type(optional-scope)!: concise description`; bodies use
  renderable GitHub-Flavored Markdown and actual line breaks, never visible
  literal `\n` separators. Give multi-section bodies by file or stdin; after
  create/edit, verify remote rendering.
- Keep physical repositories independently releasable; semantic-release owns
  versions, tags, notes, and recovery. Never publish manually. Dependency
  changes require `supply-chain/inventory.json` and a
  complete-profile audit; overrides stay mandatory and only aggregate roots own
  workflows/Dependabot.
- Follow [`docs/PROVENANCE.md`](../../../docs/PROVENANCE.md) for grant-proven
  Classic source reference, copy, migration/port, translation/adaptation, or
  MIT relicensing. Fail temporal, authorship, or separability uncertainty closed.

## Maintain guidance

Load `atrinik-guidance-maintenance` when wrapper ownership, CLI, layout,
profile/build/runtime, cleanup, release, or cross-repository contracts change.
Update only affected canonical guidance, remove superseded instructions, and
run that skill's inventory and validation workflow.
