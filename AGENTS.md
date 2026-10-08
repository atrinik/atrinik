# Atrinik workspace

## Map

- `./atrinik` (Python 3.11+) coordinates repos; `components.json` owns profiles,
  worktrees, builds, runtimes, cleanup, migration, and supply-chain reports.
- `default` selects MIT replacement: Rust, Go, Protobuf, Astro, source-only
  Observatory, shared `web-platform`, source-only `deploy-control`; M1
  lacks wrapper integration. `classic` is playable C17/CMake/Ninja plus MIT
  playtester; never mix providers.
- Use [source delivery](docs/SOURCE_DELIVERY.md). Bound deliveries and shared
  resources retain live Linux authority checks.

## Ownership

- `atrinik` CLI, `atrinik_workspace/` orchestration, `tests/` unittest suite.
- Checkout/cohort/stack/role/source/build contracts: `components.json`; machine
  policy: `governance/`; diagnostics: `supply-chain/`.
- Skills: `atrinik-development`; pin: `.agents/skill-provider.json`;
  [setup/references](docs/SKILL_PROVIDER.md).
  Composition: `.devcontainer/`; CI/release: `.github/`; helpers: `scripts/`.
- Manifest repos, `workspace/` and `build/` are ignored; root status omits them.
- Resolve ownership via `components.json` and nearest `AGENTS.md`; keep
  implementation/tests/packages/releases with their physical owner.
- `classic/` provides `classic-*`; stacks share `content@main`.
  `content-1x/` and former 1.x branch are historical: preserve migration
  evidence; never select, recreate, backport, or deliver to them. `playtester/`
  is classic-only; `tools/` is MIT-default except GPL-2.0-or-later
  `map-checker-qt/` (`LicenseRef-Atrinik-Tools-Mixed`).

## Rules

- Use `atrinik-multi-repo-workspace` for wrapper ownership/profiles/worktrees/
  migration/cleanup/releases/CLI/layout; add specialists and use
  `atrinik-guidance-maintenance` for audits.
- Prefer connected `atrinik` MCP for navigation, guidance and bounded search;
  compare snapshot commits with local Git. Use local tools for writes.
  Docker MCP: `docs/MCP_CONTEXT.md`; runtime/external opt-in.
- Use `atrinik-issue-delivery` for explicitly selected issues/PRs; standalone
  source goals use `docs/SOURCE_DELIVERY.md`. Both stop before merge.
- Portable export needs clean Classic sources, verified released sound and the
  pinned producer. Keep byte, gameplay and audible proof separate.
- Native Windows Classic GPU: `docs/WINDOWS_GPU_PREFLIGHT.md`; separate Linux coordinator/native results.
- Parallel projects: `atrinik-project-delivery`, `docs/PROJECT_DELIVERY_GOAL.md`; legacy `atrinik-program-delivery` is explicit-only.
- Develop, run Git and review in owned native Linux worktrees. New source work
  needs no ledger admission; existing bound deliveries retain context/auth/
  worktree/ledger/CAS/leases. Prepare/test isolated tooling repairs without live
  resource admission. Candidate code cannot grant live authority.
- Pinned CPU workers run application builds/toolchain checks with isolated caches;
  see `docs/LINUX_EXECUTION.md`. Native Windows has no ledger authority.
- Codex never launches/controls VS Code, nests or remounts coordinators. Resume
  source work by verified task ownership, preserving known interrupted edits;
  bound deliveries retain their recovery gates. Worker exit preserves ownership.
  Stop only owned resources. Auth: `docs/COORDINATOR_AUTH.md`.
- Repair local blockers under [local recovery](docs/LOCAL_RECOVERY.md); preserve evidence and live ownership.
- Never replace dirty primaries/remove dirty worktrees or overwrite mutable server data; preserve migration inputs.
- Cleanup is preview-first; delivery grants none. Keep ledger transactions separate from `./atrinik cleanup`; preserve dirty/detached/locked/active/referenced/uncertain targets; history fails closed.
- Worktrees belong to physical checkouts; `classic`, `classic-*`, and its roles
  select one root for all five; profiles append manifest dirs.
- For managed build/runtime work prefer `scope create`; Classic uses selectors/physical overrides.
  `scope-<name>` is profile/topology; noncanonical overrides fail pre-publication;
  rerun exact named create after rollback and bind via helper CAS.
- Plan builds with `build --plan --json`; use `--expected-plan` to fence drift.
  Unbound resource recovery uses the issue helper; preserve terminal residual reservations.
- Use wrapper paths for managed resources; never reconstruct managed paths. Isolate topology/state, ports,
  client config; prefer temporary state and local scenario secrets.
- Classic inherited state requires the executable-bound capability artifact and
  retained generation/state locks; see [runtime contracts](docs/ARCHITECTURE.md).
- Process/tooling ledgers are optional diagnostics. No routine reads/updates,
  required status lines, or work/PR readiness gates on absence, contents,
  contention or reporting failures. Update only
  through `./atrinik agent-ledger update`; never manually edit or publish them.
- Keep completion bounded, parser-driven, and secret-free; lease in order; gate
  same-coordinate readers; share migration barrier; unbound records inert.
- Optional SSH signing stays on host: `atrinik-github-governance`; never copy/mount private keys into containers.
- On touch, refresh existing Atrinik-owned copyright terminal years; blanket holders per
  `CONTRIBUTING.md`; preserve precise attribution.
- MIT reuse follows `docs/PROVENANCE.md` and its registry; rights/identity/temporal/
  authorship/scope uncertainty fails closed.
- Optional supply-chain/license diagnostics never gate work/PR readiness/delivery or
  require catalog updates. Keep standard locks/integrity manifests; avoid redundant
  version assertions. Immutable Actions/images; no submodules. Only root
  workflows/Dependabot are active.
- New content/Classic issues name `content@main` and its Classic-target artifact; no live
  1.x branch/checkout/release label/maintenance line/publication target/backport destination
  exists; historical evidence is immutable.
- Use `atrinik-github-governance` for PR publication and Conventional Commits;
  preserve contributor text and verify rendered PR bodies. No implicit merge.
- Tie blockers to actual operation/resource conflicts. Continue unaffected work;
  keep compact handoffs and bounded watchers for unchanged CI.

## Commands

`init` clones; `sync` never initializes:

```sh
./atrinik manifest validate
./atrinik status --json
./atrinik init
./atrinik init --with classic
```

Playable lifecycle (`--follow` only for interactive logs):

```sh
./atrinik profile show classic --json
./atrinik build all --profile classic --test
./atrinik up --name classic-local --profile classic --temporary-state
./atrinik ps classic-local --json
./atrinik logs classic-local server --tail 100
./atrinik down classic-local
```

Validation:

```sh
python3 -m pip install --requirement requirements-dev.txt
python3 -m coverage run -m unittest discover -v --durations 50
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
git diff --check
```

Cleanup: see `atrinik-multi-repo-workspace`.

Shell changes: ShellCheck; workflows: actionlint.
Diagnostics: `./atrinik supply-chain audit --profile PROFILE`.
Preserve `.coveragerc` and OIDC Codecov boundaries.

Handoffs name profiles, worktrees, topologies, services, states, scenarios,
prerequisites, validation and cleanup; synchronize this guide and
affected skills/docs with contract changes; stale guidance is a defect.
