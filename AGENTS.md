# Atrinik workspace agent guide

## Overview

- This MIT Python 3.11+ repository coordinates Atrinik repositories, not source.
  `./atrinik` and `components.json` manage profiles, worktrees, builds, runtimes,
  cleanup, migration, and supply-chain reports.
- `default` selects the MIT replacement stack (Rust, Go, Protobuf, and Astro).
  Its standalone M1 foundations lack wrapper build/runtime integration.
  `classic` selects playable C17/CMake/Ninja plus the MIT playtester. Never mix
  providers or route unavailable replacement adapters through classic.

## Folder structure and ownership

- `atrinik` is the CLI, `atrinik_workspace/` owns orchestration, and `tests/`
  the `unittest` suite.
- Checkout, cohort, stack, role, source, and build contracts live in
  `components.json`; machine policy in `supply-chain/` and `governance/`; longer
  guidance in `docs/`, `README.md`, and `CONTRIBUTING.md`.
- Workflows live in `.agents/skills/`, composition in `.devcontainer/`,
  CI/release in `.github/`, and helpers in `scripts/`.
- Manifest destinations (`client/`, `server/`, `classic/`) are independent
  ignored repositories; `workspace/` and `build/` are ignored generated state,
  so root `git status` omits them.
- Resolve ownership through `components.json` and the checkout's nearest
  `AGENTS.md`; keep implementation, tests, packages, and releases there.
- `classic/` provides five `classic-*` components. Both stacks share
  `content@main`; Classic selects its publisher adapter. Retained `content-1x/`
  is historical only. `playtester/` is classic-only with wrapper `build: none`.
  `.devcontainer/` owns wrapper composition and `devcontainer/` reusable images.

## Core behaviors and patterns

- Use `atrinik-multi-repo-workspace` for ownership, profiles, worktrees,
  migration, cleanup, releases, or wrapper CLI/layout work. Add only the narrow
  C, content, protocol, runtime, scenario, or GitHub skill needed. Use
  `atrinik-guidance-maintenance` for periodic guidance audits or drift updates.
- Use `atrinik-issue-delivery` only when explicitly invoked for
  issue-to-ready-PR delivery; it stops before merge.
- Use `atrinik-program-delivery` only when explicitly invoked for an ordered
  master issue; it composes leaf delivery across human merge gates and stops
  before merge or issue closure.
- Never replace or move a dirty primary checkout, remove a dirty worktree, or
  overwrite mutable server data. Preserve recoverable migration inputs.
- Cleanup is explicit and preview-first: run
  `./atrinik cleanup --dry-run --json` before the same scoped `--apply`; never
  invoke it implicitly. Preserve dirty, detached, locked, active, referenced,
  or uncertain targets; historical proof fails closed.
- Worktrees belong to physical checkouts. Selecting `classic`, a `classic-*`
  component, or one of its roles selects one root for all five; profiles append
  manifest source directories.
- Use wrapper commands; never reconstruct paths. Isolate concurrent topology
  names, states, ports, and config.
- Keep completion bounded, local, parser-driven, secret-free, and before
  `Workspace` construction.
- Lease in order; gate same-coordinate readers; share the migration barrier.
- Unbound persisted records are historical and inert.
- On touch, refresh existing Atrinik-owned copyright terminal years and blanket
  holders per `CONTRIBUTING.md`; preserve precise attribution.
- MIT reuse follows `docs/PROVENANCE.md` and its canonical identity registry;
  any rights, identity, temporal, authorship, or scope uncertainty fails closed.
- Update `supply-chain/inventory.json` when dependency ownership or validation
  changes. Keep Actions/images immutable, add no submodules, and audit a
  complete profile. Only aggregate-root workflows and Dependabot are active.
- Commits and PR titles use `type(optional-scope)!: concise description`. PR
  bodies require renderable GitHub-Flavored Markdown and actual line breaks,
  never visible literal `\n` separators. Feed multi-section bodies by file or
  stdin; after create/edit, verify remote rendering. Use
  `atrinik-github-governance` for publication, policy, or native PR stacks.
  Semantic-release owns
  tags/assets; keep confidential or unreleased work off public surfaces.

## Working agreements and commands

Run from this repository root. Inspect before mutation: initialization clones
only missing repositories, and `sync` never initializes one:

```sh
./atrinik manifest validate
./atrinik status --json
./atrinik init
./atrinik init --with classic
```

Use this exact playable build/runtime lifecycle (`--follow` only for an
interactive log session):

```sh
./atrinik profile show classic --json
./atrinik build all --profile classic --test
./atrinik up --name classic-local --profile classic --state default
./atrinik ps classic-local --json
./atrinik logs classic-local server --tail 100
./atrinik down classic-local
```

Install test tooling once, then run the complete wrapper validation:

```sh
python3 -m pip install --requirement requirements-dev.txt
python3 -m coverage run -m unittest discover -v
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
./atrinik supply-chain validate
git diff --check
```

For cleanup changes also run:

```sh
./atrinik cleanup --scope all --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
```

Run ShellCheck for shell changes, actionlint for workflows, and
`./atrinik supply-chain audit --profile PROFILE` when dependency inputs change.
Preserve `.coveragerc` and OIDC Codecov boundaries.

Handoffs must name exact profiles, worktrees, topologies, services, states, and
scenarios, plus prerequisites, results, validation, and cleanup. Synchronize
this guide, affected skills, README, architecture, and contributor guidance
with contract changes; treat stale guidance as a defect.
