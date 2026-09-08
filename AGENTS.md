# Atrinik workspace agent guide

## Overview

- Python 3.11+ `./atrinik` coordinates repos; `components.json` owns profiles,
  worktrees, builds, runtimes, cleanup, migration, and supply-chain reports.
- `default` selects MIT replacement: Rust, Go, Protobuf, Astro, source-only
  Observatory, shared `web-platform`, source-only `deploy-control`; M1
  lacks wrapper integration. `classic` is playable C17/CMake/Ninja plus MIT
  playtester; never mix providers.
- Windows supports repository commands; delivery-ledger needs pinned Linux
  devcontainer; Linux-only commands fail stably.

## Folder structure and ownership

- `atrinik` CLI, `atrinik_workspace/` orchestration, `tests/` unittest suite.
- Checkout/cohort/stack/role/source/build contracts: `components.json`; machine
  policy: `governance/`; diagnostics: `supply-chain/`.
- Workflows: `.agents/skills/`; composition: `.devcontainer/`; CI/release:
  `.github/`; helpers: `scripts/`.
- Manifest destinations are ignored repos; `workspace/` and `build/` are ignored
  generated state omitted from root status.
- Resolve ownership via `components.json` and nearest `AGENTS.md`; keep
  implementation/tests/packages/releases with their physical owner.
- `classic/` provides `classic-*`; stacks share `content@main`.
  `content-1x/` and former 1.x branch are historical: preserve migration
  evidence; never select, recreate, backport, or deliver to them. `playtester/`
  is classic-only; `tools/` is MIT-default except GPL-2.0-or-later
  `map-checker-qt/` (`LicenseRef-Atrinik-Tools-Mixed`).

## Core behaviors and patterns

- Use `atrinik-multi-repo-workspace` for wrapper ownership/profiles/worktrees/
  migration/cleanup/releases/CLI/layout; add specialists and use
  `atrinik-guidance-maintenance` for audits.
- Invoke `atrinik-issue-delivery` explicitly for an issue or existing PR; it stops before merge.
- Native Windows Classic GPU preflight: follow `docs/WINDOWS_GPU_PREFLIGHT.md`; keep Linux coordinator and native results separate.
- Use `atrinik-project-delivery` for parallel projects; legacy `atrinik-program-delivery` remains explicit-only. See `docs/PROJECT_DELIVERY_GOAL.md`.
- Codex delivery has two entry modes: continue in the canonical VS Code devcontainer or bootstrap/attach the pinned Linux devcontainer from a native host; host work is limited to bootstrap/attach and approved Git/GitHub/commit operations; wrapper/ledger/worktree/edit/test/build/review/validation stay inside.
- Codex never launches/controls VS Code; no URIs, GUI automation, nesting or remounting. Reuse exact owner/container/image/mount/worktree/ledger coordinates only; reconnect rechecks probe, worktree, ledger and leases. Bound shutdown to owner. Isolate mutable state; share host GitHub auth read-only per `docs/COORDINATOR_AUTH.md`.
- Never replace dirty primaries/remove dirty worktrees or overwrite mutable server data; preserve migration inputs.
- Cleanup is preview-first; delivery grants none. Keep ledger transactions separate from `./atrinik cleanup`; preserve dirty/detached/locked/active/referenced/uncertain targets; history fails closed.
- Worktrees belong to physical checkouts; `classic`, `classic-*`, and its roles
  select one root for all five; profiles append manifest dirs.
- Prefer `scope create`; Classic uses selectors/physical overrides.
  `scope-<name>` is profile/topology; noncanonical overrides fail pre-publication;
  rerun exact named create after rollback and bind via helper CAS.
- Use wrapper paths; never reconstruct managed paths. Isolate topology/state, ports,
  client config; prefer temporary state and local scenario secrets.
- Process/tooling ledgers are optional local diagnostics. Do not read or update
  them routinely, require status lines, or block work/PR readiness on their
  absence, contents, contention, or reporting failures. If useful, update only
  through `./atrinik agent-ledger update`; never manually edit or publish them.
- Keep completion bounded, parser-driven, and secret-free; lease in order; gate
  same-coordinate readers; share migration barrier; unbound records inert.
- Optional SSH signing stays on host; follow `.agents/skills/atrinik-github-governance/references/ssh-signing.md`; never copy/mount private keys
  into a container.
- On touch, refresh existing Atrinik-owned copyright terminal years; blanket holders per
  `CONTRIBUTING.md`; preserve precise attribution.
- MIT reuse follows `docs/PROVENANCE.md` and its registry; rights/identity/temporal/
  authorship/scope uncertainty fails closed.
- Supply-chain inventory, license reports, and audits are optional diagnostics. Never
  require catalog updates or use findings to block work, PR readiness, or delivery.
  Keep Actions/images immutable; no submodules. Only root workflows/Dependabot are active.
- New content/Classic issues name `content@main` and its Classic-target artifact; no live
  1.x branch/checkout/release label/maintenance line/publication target/backport destination
  exists; historical evidence is immutable.
- Commits and PR titles use `type(optional-scope): concise description`; add `!` only when a reviewer explicitly requests a breaking change, not auto.
  PR bodies must be substantive rendered GitHub-Flavored Markdown with actual line breaks, never literal `\n` separators; include `Summary`, `Implementation / behavior`, `Validation`, and applicable `Limitations / follow-up`.
  An issue-closing line alone is insufficient; preserve contributor-authored text byte-for-byte outside the delivery-owned section. Feed multi-section bodies by file/stdin; after create/edit verify remote rendering. Use `atrinik-github-governance`.

## Working agreements and commands

At root; inspect first. See README: Windows workflow.
`init` clones missing repos; `sync` never initializes:

```sh
./atrinik manifest validate
./atrinik status --json
./atrinik init
./atrinik init --with classic
```

Use this playable build/runtime lifecycle (`--follow` only for interactive logs):

```sh
./atrinik profile show classic --json
./atrinik build all --profile classic --test
./atrinik up --name classic-local --profile classic --temporary-state
./atrinik ps classic-local --json
./atrinik logs classic-local server --tail 100
./atrinik down classic-local
```

Run complete wrapper validation:

```sh
python3 -m pip install --requirement requirements-dev.txt
python3 -m coverage run -m unittest discover -v --durations 50
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
git diff --check
```

For cleanup changes also run:

```sh
./atrinik cleanup --scope all --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

Run ShellCheck for shell changes and actionlint for workflows.
Diagnostics: `./atrinik supply-chain audit --profile PROFILE`.
Preserve `.coveragerc` and OIDC Codecov boundaries.

Handoffs name exact profiles, worktrees, topologies, services, states, scenarios,
prerequisites, results, validation, cleanup; synchronize this guide and
affected skills/docs with contract changes; stale guidance is a defect.
