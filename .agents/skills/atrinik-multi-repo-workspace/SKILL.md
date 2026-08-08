---
name: atrinik-multi-repo-workspace
description: Coordinate Atrinik work across checkouts, profiles, worktrees, cleanup, wrapper builds/runtime, releases, or workspace CLI and layout. Use when ownership crosses a physical repository or the wrapper itself changes.
---

# Atrinik multi-repository workspace

## Resolve scope and ownership

Run commands from the `atrinik/atrinik` wrapper root.

1. Read root `AGENTS.md`. Read only the task-relevant wrapper source/docs:
   - ownership, cohorts, roles, or build contracts: `components.json`;
   - operator command behavior: the matching `README.md` section;
   - layout, locking, trust, or lifecycle design: the matching
     `docs/ARCHITECTURE.md` section;
   - pre-split repositories: [repository migration](references/repository-migration.md).
2. Resolve every affected logical component to its physical checkout and safe
   source root. Read that checkout's nearest `AGENTS.md` before editing.
3. Put component implementation/tests/packages/releases in the owning physical
   repository. Put only orchestration, composition, manifest, and wrapper docs
   here.

Physical checkouts are independent ignored Git repositories. One `classic`
worktree contains its five logical source directories. `content@main` and
`content-1x@1.x` are distinct checkouts even though their GitHub coordinate is
the same. Keep wrapper VS Code composition under `.devcontainer/`; reusable
images belong to the `devcontainer` checkout.

## Prepare safe worktrees

Inspect local state before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize only missing repositories. Plain `init` selects the replacement
cohort; exact `--with classic` adds the complete classic cohort. `sync` never
clones an absent checkout.

```sh
./atrinik init [COMPONENT...]
./atrinik init --with classic
./atrinik sync [COMPONENT...]
./atrinik sync --with classic
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Synchronize only clean primaries. Preserve dirty checkouts and worktrees; never
replace, move, or remove them implicitly. A logical classic name creates one
full `classic` repository worktree under
`workspace/worktrees/classic/LABEL`. Commit and push inside each owning
worktree with Conventional Commits.

Reclaim completed review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope worktrees --scope builds CHECKOUT... --older-than 7
./atrinik cleanup --scope all --older-than 7 --apply
```

The default covers registered worktrees and marker-owned builds; npm cache is
always opt-in. References, local/Git uncertainty, and unsafe path or marker
state protect a target. Apply recomputes under the repository-layout lock,
removes builds before non-force Git worktrees, and preserves profiles,
scenarios, states, topology records/logs, migrations, retention records,
branches, Git objects, review reports, and unmarked paths. For cleanup changes,
hand off exact fixture names, JSON preview/apply commands, expected reason
codes, and the preserved records.

## Compose coherent sources

Use `default` for replacement sources and `classic` for the currently playable
classic stack. Never mix providers or use classic C/CMake as a fallback for a
missing replacement adapter. Replacement repositories have standalone M1
foundations, but the wrapper replacement build/runtime closure is not yet
available.

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW CHECKOUT_OR_COMPONENT --worktree LABEL
./atrinik profile show REVIEW --json
./atrinik path COMPONENT --profile REVIEW
./atrinik build COMPONENT --profile REVIEW --test
```

Selecting checkout `classic`, one of its logical components, or a provided
role updates all five classic selectors to one checkout root. Resolution then
appends each manifest source path. Do not treat a classic subdirectory as an
independent worktree.

Build, scenario, and topology records bind the exact repository, branch,
checkout, source, component, and provider. Treat older records without current
immutable coordinates as inert. Let the wrapper collect content/resources and
own generated paths, locks, state, PIDs, logs, and process cleanup.

For classic server execution or diagnosis, load `atrinik-server-runtime`. For
a ready account and character, also load `atrinik-test-scenario`; never
handcraft saves or expose credentials. Keep every concurrent topology on a
distinct name and server state.

## Validate and hand off

Use wrapper-native commands wherever supported. A runtime handoff includes
concrete names and the complete lifecycle:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --json
./atrinik up --name TOPOLOGY --profile PROFILE --state STATE
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

State prerequisites, feature actions, expected results, and cleanup. When
runtime is irrelevant, say so and provide the applicable build/test and
inspection commands. Do not substitute internal executable or generated paths
for supported wrapper operations.

## Coordinate publication and policy

- Use `atrinik-github-governance` for Actions, permissions, required checks,
  merge/release policy, or repository settings. Inspect live and desired state
  before an authorized external change.
- Keep each physical repository independently releasable. Semantic-release
  owns version selection, tags, notes, and recovery; never publish manually.
- Update `supply-chain/inventory.json` for changed dependency inputs and audit
  the complete selected profile. Review overrides never make another member
  optional. Only aggregate-checkout root workflows/Dependabot are active.
- Apply historical MIT grants only under
  [`docs/PROVENANCE.md`](../../../docs/PROVENANCE.md). Fail closed and record the
  complete evidence in the destination repository.

## Maintain the contract

When the wrapper CLI, manifest, layout, ownership, profile/build/runtime,
cleanup, release, or cross-repository procedure changes, update the affected
root guide, skill, README, and architecture sections together. Remove
superseded instructions. Validate skills, exercise the affected CLI path, run
the complete wrapper tests/coverage and compileall, and finish with
`git diff --check`.
