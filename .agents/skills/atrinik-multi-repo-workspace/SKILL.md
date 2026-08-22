---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---
# Atrinik multi-repository workspace

## Resolve scope and ownership

1. Read `AGENTS.md`, `components.json`, and relevant README/architecture; see
   [repository migration](references/repository-migration.md) for pre-split repositories.
2. Resolve each physical checkout/source root; read its nearest `AGENTS.md`.
3. Keep code, tests, packages, and releases with their physical owner; keep
   orchestration/contracts here.

Checkouts are ignored repositories. One `classic` worktree holds all five
`classic-*` components; both stacks share `content@main`. `content-1x` and the
former 1.x branch are historical migration evidence only, never active
components or delivery targets.

## Prepare safe worktrees

Inspect local state before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize only absent repositories; `init` selects replacement, `--with classic`
adds classic, and `sync` never clones.

```sh
./atrinik init [COMPONENT...]
./atrinik init --with classic
./atrinik sync [COMPONENT...]
./atrinik scope create COMPONENT... --name REVIEW --from PROFILE --json
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Sync only clean primaries; never replace, move, or remove dirty sources.
Classic selectors create `workspace/worktrees/classic/LABEL`; prefer atomic scopes.

Classic scope selectors have two namespaces: the positional `components`
arguments may use a logical component such as `classic-client`, while
`--label`, `--branch`, and `--start-point` overrides are keyed by the physical
checkout `classic`. For example:

```sh
./atrinik scope create classic-client --name REVIEW --from classic \
  --label classic=LABEL --branch classic=TYPE/TOPIC \
  --start-point classic=BASE_SHA --temporary-state --json
```

Do not use `classic-client=` as an override key. The name derives immutable
`scope-<name>` profile/topology; non-canonical `--topology` fails before
publication. Compare `requested_components`, topology, and checkout with the
ledger request; a failed bind is recoverable only through the helper, never by
editing or deleting ledger state.
Release with a fresh preview digest:

```sh
./atrinik scope release REVIEW --dry-run --json
./atrinik scope release REVIEW --apply --plan PLAN_SHA256 --json
```

Release never stops topologies or deletes persistent state; interrupted steps
resume after a fresh preview and uncertainty retains journals.

Reclaim review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

Repeat with `--apply`. Defaults cover worktrees/builds; caches/history opt in;
`all` excludes topologies. Remove only stopped, released, exactly owned records;
uncertainty fails closed. Apply sound-cache before its worktree and retry
unchanged. Retire receipts only through an explicit `cleanup-journals` preview;
exact names are required on apply. Pending/unsafe receipts and delivery
sidecars are never cleanup targets.

## Compose coherent sources

Use `default` for replacement and `classic` for playable sources. Never mix
providers or substitute classic C/CMake for a missing adapter; replacement
repositories lack wrapper closure.

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW CHECKOUT_OR_COMPONENT --worktree LABEL
./atrinik profile show REVIEW --json
./atrinik path COMPONENT --profile REVIEW
./atrinik build COMPONENT --profile REVIEW --test
```

Classic selection is checkout-wide, not per subdirectory. Builds pin snapshots;
live inputs retain leases and cleanup owns staging.

Lease order is registry, profile, Git-admin, source, topology/scenario, state,
build, cache. Gate matching coordinates; multi-source writers retry all-or-none.
Only migration takes the barrier. Published runtimes retain generation,
process-tree, state, and port leases. Fail closed without sharing; incomplete
coordinates are inert. Completion is bounded, local, read-only, secret-free,
and parser-driven before `Workspace`.

Verify concurrency with distinct worktrees, observable readiness rendezvous,
and A live through B's release. Count transitions/conflicts; timeouts bound
failure, not compiler speed.

For classic execution load `atrinik-server-runtime`; add
`atrinik-test-scenario` for a ready character. Never handcraft saves or expose
credentials; give concurrent topologies distinct names/state.

## Validate and hand off

Use wrapper commands:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --temporary-state --json
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

Record prerequisites, actions, results, cleanup, and applicable handoff
commands; never replace wrapper operations with internal executables or generated
paths.

## Coordinate publication and policy

Use `atrinik-github-governance` for PRs. Titles use
`type(optional-scope): concise description` by default; add `!` only when a
reviewer explicitly requests a breaking change, not auto. PR bodies must be
substantive rendered GitHub-Flavored Markdown with actual line breaks, never
literal `\n` separators. Include `Summary`, `Implementation / behavior`,
`Validation`, and applicable `Limitations / follow-up`; an issue-closing line
alone is insufficient. preserve contributor-authored text byte-for-byte and
change only a separately delivery-owned section when authorized. Feed
multi-section bodies by file/stdin; after create/edit verify the rendered
remote body. Semantic-release owns publication; dependency changes update
inventory and audit a profile. Follow `docs/PROVENANCE.md`; fail uncertainty.

## Maintain guidance

Load `atrinik-guidance-maintenance` when wrapper or cross-repository contracts
change; update canonical guidance, remove superseded instructions, and run its
inventory/validation workflow.
