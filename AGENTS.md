# Atrinik workspace agent guide

## Overview

- This Python 3.11+ wrapper coordinates Atrinik repositories; `./atrinik` and
  `components.json` own profiles, worktrees, builds, runtimes, cleanup,
  migration, and supply-chain reports.
- `default` selects the MIT replacement stack (Rust, Go, Protobuf, Astro,
  source-only Observatory) plus source-only `deploy-control`; M1 foundations
  lack wrapper integration. `classic` is playable C17/CMake/Ninja plus MIT
  playtester; never mix providers.
- Windows supports repository commands; delivery-ledger coordination needs
  pinned Linux devcontainer; other Linux-only commands return stable errors.

## Folder structure and ownership

- `atrinik` is the CLI, `atrinik_workspace/` owns orchestration, and `tests/`
  the `unittest` suite.
- Checkout, cohort, stack, role, source, and build contracts live in
  `components.json`; machine policy lives in `supply-chain/` and `governance/`.
- Workflows live in `.agents/skills/`, composition in `.devcontainer/`,
  CI/release in `.github/`, and helpers in `scripts/`.
- Manifest destinations are ignored repositories; `workspace/` and `build/` are
  ignored generated state omitted from root status.
- Resolve ownership through `components.json` and the checkout's nearest
  `AGENTS.md`; keep implementation, tests, packages, and releases there.
- `classic/` provides `classic-*`; stacks share `content@main`. `content-1x/`
  and the former 1.x branch are historical only: preserve any local migration
  evidence, but never select, recreate, or backport delivery to them.
  `playtester/` is classic-only. `tools/` is MIT-default except GPL-2.0-or-later
  `map-checker-qt/` (`LicenseRef-Atrinik-Tools-Mixed`).

## Core behaviors and patterns

- Use `atrinik-multi-repo-workspace` for wrapper ownership, profiles, worktrees,
  migration, cleanup, releases, CLI, or layout; add applicable specialist skills
  and use `atrinik-guidance-maintenance` for guidance audits.
- Use `atrinik-issue-delivery` only when explicitly invoked for an issue or
  existing PR; it stops before merge.
- Use `atrinik-program-delivery` only when explicitly invoked for an ordered
  master issue; it composes leaves across merge gates and stops before merge or
  issue closure.
- Never replace dirty primaries, remove dirty worktrees, or overwrite mutable
  server data; preserve migration inputs.
- Cleanup is preview-first; delivery completion grants none. Ledger terminal
  transactions stay separate from `./atrinik cleanup`. Preserve dirty,
  detached, locked, active, referenced, or uncertain targets; history fails
  closed.
- Worktrees belong to physical checkouts. Selecting `classic`, a `classic-*`
  component, or one of its roles selects one root for all five; profiles append
  manifest source directories.
- Prefer `scope create`; Classic uses selectors/physical overrides.
  `scope-<name>` is the profile/topology; noncanonical overrides fail
  pre-publication.
  Rerun exact named create after rollback; bind via helper CAS.
- Use wrapper paths; never reconstruct managed paths. Isolate topology, state,
  ports, and client config; prefer temporary state and keep scenario secrets local.
- Keep completion bounded, parser-driven, and secret-free.
- Lease in order; gate same-coordinate readers; share the migration barrier.
- Unbound persisted records are historical and inert.
- On touch, refresh existing Atrinik-owned copyright terminal years and blanket
  holders per `CONTRIBUTING.md`; preserve precise attribution.
- MIT reuse follows `docs/PROVENANCE.md` and its canonical identity registry;
  rights, identity, temporal, authorship, or scope uncertainty fails closed.
- Update `supply-chain/inventory.json` when dependency ownership/validation
  changes. Keep Actions/images immutable, add no submodules, and audit a full
  profile; only aggregate-root workflows and Dependabot are active.
- New content/Classic issues name `content@main` and its Classic-target
  artifact. No live 1.x branch, checkout, release label, maintenance line,
  publication target, or backport destination exists; historical evidence is
  immutable.
- Commits and PR titles use `type(optional-scope): concise description`; add
  `!` only when a reviewer explicitly requests a breaking change, not auto.
  PR bodies must be substantive rendered GitHub-Flavored Markdown with actual
  line breaks, never literal `\n` separators: include `Summary`,
  `Implementation / behavior`, and `Validation` sections plus applicable
  `Limitations / follow-up`. An issue-closing line alone is insufficient;
  preserve issue/PR references and preserve contributor-authored text
  byte-for-byte under the delivery-owned section rules. Feed
  multi-section bodies by file/stdin; after create/edit verify the rendered
  remote body. Use `atrinik-github-governance` for PRs.

## Working agreements and commands

Run from this repository root. Inspect first; `init` clones only missing
repositories and `sync` never initializes one:

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

Handoffs name exact profiles, worktrees, topologies, services, states,
scenarios, prerequisites, results, validation, and cleanup. Synchronize this
guide and affected skills/docs with contract changes; stale guidance is a defect.
