---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---
# Atrinik multi-repository workspace

Prefer `atrinik` MCP when available: [routing](../../../docs/MCP_CONTEXT.md).

## Scope and ownership

1. Read `AGENTS.md`, resolve ownership in `components.json`, and read the relevant
   README/architecture section. Load [repository migration](references/repository-migration.md)
   only for pre-split layouts.
2. Resolve each physical checkout and nearest `AGENTS.md`; keep code, tests,
   packages, and releases with their physical owners.
3. Process/tooling diagnostics are discretionary; skip routine ledger reads,
   writes and status lines. If recording a useful observation, use
   `./atrinik agent-ledger update`; reporting errors never block delivery.

Ignored checkouts retain physical ownership. One `classic` worktree holds all
`classic-*` components; stacks share `content@main`. Never target historical 1.x.

Repair local blockers under [local recovery](../../../docs/LOCAL_RECOVERY.md).

## Safe worktrees

New source-only changes use [source delivery](../../../docs/SOURCE_DELIVERY.md).
The source helper creates owned Git worktrees; verify interrupted edits. Existing
bound deliveries remain on their original recovery protocol. Use the managed
scope/worktree instructions below when the task needs those resources, not as a
prerequisite to an isolated source patch or fixture test.

Inspect before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize absent repositories with `init` (`--with classic` adds classic);
`sync` never clones.

Sync only clean primaries; never alter dirty sources. Managed Classic selectors create
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
recover through the helper; if its own bookkeeping is broken, follow local
recovery without inventing ledger state.

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

In an initialized workspace, `build --plan --json` returns execution coordinates;
use the same options with `--expected-plan SHA256`. Unbound intents require the
[issue recovery procedure](../atrinik-issue-delivery/references/delivery-ledger.md);
preserve terminal residuals. Use `resource_context` for wrapper worktrees.

Classic selection is checkout-wide; builds pin snapshots, live inputs retain leases,
and cleanup owns staging.

Lease order: registry, profile, Git-admin, source, topology/scenario, state, build, cache.
Gate matching coordinates; multi-source writers retry all-or-none; fail closed on shared/incomplete
state. Migration alone takes the barrier. Published runtimes retain generation/process-tree/state/
port leases. Completion is bounded/read-only, secret-free, parser-driven before `Workspace`.

Use an owned native Linux worktree. Bound resource work retains exact
host/user/profile/build/ledger coordinates. Use [short-lived pinned CPU build
workers](../../../docs/LINUX_EXECUTION.md) and isolated reusable caches for builds
and toolchain checks. Worker exit preserves native ownership. Bound recovery reruns
probe, actor, clean worktree, inventory/CAS and leases. Historical bound containers retain exact image/mounts and compatibility proof;
never adopt or replace them implicitly. Preserve evidence; stop only owned resources.
Deliveries need distinct worktrees/coordinates, caches, ports,
topology/state names and mutable state. Use [host GitHub
auth](../../../docs/COORDINATOR_AUTH.md); verify actor/capabilities and keep
credentials outside build workers.

Verify concurrency with distinct worktrees and readiness rendezvous; keep A live through B
release; count transitions/conflicts; timeouts bound failure, not compiler speed.

Classic execution loads `atrinik-server-runtime`; add `atrinik-test-scenario` for ready
characters. Never handcraft saves or expose credentials; use distinct topology/state names.

## Validate and hand off

Use the exact profile/build/topology/runtime lifecycle in root `AGENTS.md`.

For native Windows Classic GPU handoff, follow [`docs/WINDOWS_GPU_PREFLIGHT.md`](../../../docs/WINDOWS_GPU_PREFLIGHT.md); reuse Classic package-smoke/D3D12 commands and keep
package, test-build, native runtime, and Linux coordinator evidence separate.

Record applicable prerequisites, results and next actions; never replace managed
wrapper operations with internal executables/generated paths.

## Publication and policy

Use `atrinik-github-governance` for PR publication, contributor-text preservation
and rendered-body verification. Semantic-release publishes. Supply-chain
diagnostics never gate work or require inventory updates. Follow
`docs/PROVENANCE.md`; fail uncertainty.

## Maintain guidance

For wrapper or cross-repository contract changes, load `atrinik-guidance-maintenance`;
sync guidance and run inventory/validation.

Portable export requires verified released sound and the pinned producer;
raw source/local-playtest sound cannot be packaged.
