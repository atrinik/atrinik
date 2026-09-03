---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---
# Atrinik multi-repository workspace

## Scope and ownership

1. Read `AGENTS.md`, `components.json`, README/architecture, and
   [repository migration](references/repository-migration.md).
2. Resolve each physical checkout and nearest `AGENTS.md`; keep code, tests,
   packages, and releases with their physical owners.
3. Before repository work or expensive build/package/runtime/remote-mutation, consult
   ignored `build/agent-process-improvements.md`. Run `./atrinik agent-ledger update`;
   never manually edit ledgers. Report `Process improvements added: none` or keys/issues;
   follow digest/lock/retry and coordinator handoff across separate filesystems.

Checkouts are ignored repositories. One `classic` worktree holds five `classic-*`
components; stacks share `content@main`. `content-1x` and former 1.x are historical
migration evidence only, never active components or delivery targets.

## Safe worktrees

Inspect before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize absent repositories with `init` (`--with classic` adds classic);
`sync` never clones.

```sh
./atrinik init [COMPONENT...]
./atrinik init --with classic
./atrinik sync
./atrinik scope create COMPONENT... --name REVIEW --from PROFILE --json
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Sync only clean primaries; never alter dirty sources. Classic selectors create
`workspace/worktrees/classic/LABEL`; prefer atomic scopes.

Selectors are positional; overrides target physical checkout `classic`:

```sh
./atrinik scope create classic-client --name REVIEW --from classic \
  --label classic=LABEL --branch classic=TYPE/TOPIC \
  --start-point classic=BASE_SHA --temporary-state --json
```

Never use `classic-client=` as an override. `scope-<name>` is immutable
profile/topology; non-canonical `--topology` fails before publication. Compare
`requested_components`, topology, and checkout with ledger request; failed binds
recover only through the helper, never by editing/deleting ledger state.

Retry rolled-back named creates only after branch-only Git/LFS failure. Wrapper proves
generation/digest, rows/roots, base/head, and no coordinate conflict; drift/uncertainty
stops. Use `scope show`, `scope-observe`, and `scope-bind-cas`; never edit.
Release with a fresh preview:

```sh
./atrinik scope release REVIEW --dry-run --json
./atrinik scope release REVIEW --apply --plan PLAN_SHA256 --json
```

Release never stops topologies or deletes persistent state; resume interruptions
after preview; uncertainty retains journals.

Reclaim review data through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope sound-cache sound --older-than 7 --dry-run --json
./atrinik cleanup --scope worktrees sound --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

Repeat with `--apply`; defaults cover worktrees/builds; opt into caches/history; `all`
excludes topologies. Remove only stopped, released, exact-owned records; uncertainty fails closed.
Apply sound-cache before its worktree; retire receipts only via exact-name `cleanup-journals`
preview. Never clean pending/unsafe receipts or delivery sidecars.

## Coherent sources

Use `default` for replacement and `classic` for playable sources; never mix providers
or substitute classic C/CMake for missing adapters. Replacement repositories lack wrapper closure.

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW CHECKOUT_OR_COMPONENT --worktree LABEL
./atrinik profile show REVIEW --json
./atrinik path COMPONENT --profile REVIEW
./atrinik build COMPONENT --profile REVIEW --test
```

Classic selection is checkout-wide; builds pin snapshots, live inputs retain leases,
and cleanup owns staging.

Lease order: registry, profile, Git-admin, source, topology/scenario, state, build, cache.
Gate matching coordinates; multi-source writers retry all-or-none; fail closed on shared/incomplete
state. Migration alone takes the barrier. Published runtimes retain generation/process-tree/state/
port leases. Completion is bounded/read-only, secret-free, parser-driven before `Workspace`.

A persistent coordinator session belongs to one agent and exact delivery coordinate. Reuse
requires matching pinned container, mounts, worktree, profile/build roots, and ledger coordinates.
Recovery reruns probe, worktree list, ledger inventory/CAS, and leases. Bound idle/lifetime;
preserve failure evidence; stop only the owned container. Independent sessions need distinct
worktrees/coordinates, caches, credentials, ports, topology/state names, and mutable state.

Verify concurrency with distinct worktrees and readiness rendezvous; keep A live through B
release; count transitions/conflicts; timeouts bound failure, not compiler speed.

Classic execution loads `atrinik-server-runtime`; add `atrinik-test-scenario` for ready
characters. Never handcraft saves or expose credentials; use distinct topology/state names.

## Validate and hand off

Wrapper commands:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --temporary-state --json
./atrinik up --name TOPOLOGY --profile PROFILE --temporary-state
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

For native Windows Classic GPU handoff, follow [`docs/WINDOWS_GPU_PREFLIGHT.md`](../../../docs/WINDOWS_GPU_PREFLIGHT.md); reuse Classic package-smoke/D3D12 commands and keep
package, test-build, native runtime, and Linux coordinator evidence separate.

Record prerequisites/actions/results/cleanup and handoff commands; never replace
wrapper operations with internal executables/generated paths.

## Publication and policy

Use `atrinik-github-governance` for PRs. Titles use `type(optional-scope): concise description`;
add ! only when a reviewer explicitly requests a breaking change. PR bodies must be substantive
rendered GitHub-Flavored Markdown with actual line breaks, never literal `\n` separators. Include
`Summary`, `Implementation / behavior`, `Validation`, and applicable `Limitations / follow-up`;
issue-closing line alone is insufficient. preserve contributor-authored text byte-for-byte; change
only a delivery-owned section when authorized. Feed multi-section bodies by file/stdin; after
create/edit verify remote body. Semantic-release owns publication; dependency changes update
inventory and audit a profile. Follow `docs/PROVENANCE.md`; fail uncertainty.

## Maintain guidance

For wrapper or cross-repository contract changes, load `atrinik-guidance-maintenance`;
synchronize guidance and run inventory/validation.
