# Atrinik workspace agent guide

## Overview

- This MIT Python 3.11+ repository coordinates independent Atrinik Git
  repositories, not component source. `./atrinik` and `components.json` manage
  profiles, worktrees, builds, supervised runtimes, cleanup, migration, and
  supply-chain reports.
- `default` selects the MIT replacement stack (Rust, Go, Protobuf, and Astro).
  Its standalone M1 foundations lack wrapper build/runtime integration.
  `classic` selects the playable C17/CMake/Ninja stack. Never mix providers or
  route unavailable replacement adapters through classic.

## Folder structure and ownership

- `atrinik` is the CLI; `atrinik_workspace/` owns orchestration and `tests/` the
  standard-library `unittest` suite.
- Checkout, cohort, stack, role, source, and build contracts live in
  `components.json`; machine policy in `supply-chain/` and `governance/`; longer
  guidance in `docs/`, `README.md`, and `CONTRIBUTING.md`.
- Task workflows live in `.agents/skills/`, workspace composition in
  `.devcontainer/`, CI/release in `.github/`, and helpers in `scripts/`.
- Manifest destinations such as `client/`, `server/`, and `classic/` are
  independent ignored repositories; `workspace/` and `build/` are ignored
  generated state. Root `git status` omits them.
- Resolve ownership through `components.json` and the owning checkout's nearest
  `AGENTS.md`. Keep implementation, tests, packages, and component releases in
  that physical repository.
- The `classic/` monorepo provides five `classic-*` components. `content/` is
  `atrinik/content@main`; `content-1x/` is its separate `1.x` checkout.
  `.devcontainer/` owns wrapper composition and `devcontainer/` reusable images.

## Core behaviors and patterns

- Use `atrinik-multi-repo-workspace` for ownership, profiles, worktrees,
  migration, cleanup, releases, or wrapper CLI/layout work. Add only the narrow
  C, content, protocol, runtime, scenario, or GitHub skill needed. Use
  `atrinik-guidance-maintenance` for periodic guidance audits or drift updates.
- Use `atrinik-issue-delivery` only when explicitly invoked for
  issue-to-ready-PR delivery; it stops before merge.
- Never replace or move a dirty primary checkout, remove a dirty worktree, or
  overwrite mutable server data. Preserve recoverable migration inputs.
- Cleanup is explicit and preview-first. Run
  `./atrinik cleanup --dry-run --json` before the same scoped `--apply`; never
  invoke cleanup implicitly.
  Preserve dirty, detached, locked, active, referenced, or uncertain targets.
  Historical eligibility fails closed. The multi-repository skill owns the
  full cleanup proof and retention contract.
- A worktree belongs to its physical checkout. Selecting `classic`, any
  `classic-*` component, or one of its roles selects one root for all five;
  profile resolution appends each manifest source directory.
- Use `./atrinik path`, `topology show`, `up`, `ps`, `logs`, and `down`. Do not
  reconstruct managed build, PID, log, lock, runtime, or state paths. Give
  concurrent topologies distinct names, states, ports, and client config.
- Persisted records lacking current immutable repository, branch, checkout,
  source, and provider coordinates are historical and inert.
- Follow `docs/PROVENANCE.md` for historical MIT reuse; incomplete history,
  mixed authorship, uncertain identity, or embedded third-party material fails
  closed. Cite the exact wrapper registry revision used.
- Update `supply-chain/inventory.json` when dependency ownership or validation
  changes. Keep Actions/images immutable, add no submodules, and audit a
  complete profile. Only aggregate-root workflows and Dependabot are active.
- Commits and PR titles use `type(optional-scope)!: concise description`. PR
  bodies require renderable GitHub-Flavored Markdown and actual line breaks,
  never visible literal `\n` separators. Feed multi-section bodies by file or
  stdin; after create/edit, verify remote rendering. Use
  `atrinik-github-governance` for publication and policy. Semantic-release owns
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
```

Run ShellCheck for shell changes, actionlint for workflows, and
`./atrinik supply-chain audit --profile PROFILE` when dependency inputs change.
Preserve `.coveragerc` and OIDC Codecov boundaries.

Handoffs must name exact profiles, worktrees, topologies, services, states, and
scenarios, plus prerequisites, results, validation, and cleanup. Synchronize
this guide, affected skills, README, architecture, and contributor guidance
with contract changes; treat stale guidance as a defect.
