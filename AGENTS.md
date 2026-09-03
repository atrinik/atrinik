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
  policy: `supply-chain/`, `governance/`.
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
- Use `atrinik-issue-delivery` for an issue or existing PR; it stops before merge.
  Invoke explicitly.
- Use `atrinik-program-delivery` only for explicitly invoked ordered master
  issues; it composes leaves across merge gates and stops before merge/closure.
- Codex delivery has two entry modes: continue an already-running session
  inside the canonical VS Code devcontainer, or bootstrap/attach that pinned
  Linux devcontainer from a native host. In the latter mode, host work is
  limited to bootstrap/attach and approved Git/GitHub/commit operations; all
  wrapper, ledger, worktree, edit, test, build, review, and validation work
  stays inside the container.
- Codex never launches or controls VS Code, sends VS Code URIs, or uses GUI
  automation; do not nest/remount or trust copied/stale evidence. Reuse only
  with exact owner/container/image/mount/worktree/ledger coordinates; reconnect
  reruns fresh probe, worktree, ledger, and lease checks. Idle/shutdown is
  bounded to the owner; parallel sessions need separate caches, credentials,
  ports, and mutable state.
- Never replace dirty primaries, remove dirty worktrees, or overwrite mutable server data;
  preserve migration inputs.
- Cleanup is preview-first; delivery grants none. Keep ledger transactions separate
  from `./atrinik cleanup`; preserve dirty, detached, locked, active, referenced,
  or uncertain targets; history fails closed.
- Worktrees belong to physical checkouts; `classic`, `classic-*`, and its roles
  select one root for all five; profiles append manifest dirs.
- Prefer `scope create`; Classic uses selectors/physical overrides.
  `scope-<name>` is profile/topology; noncanonical overrides fail pre-publication;
  rerun exact named create after rollback and bind via helper CAS.
- Use wrapper paths; never reconstruct managed paths. Isolate topology/state, ports,
  client config; prefer temporary state and local scenario secrets.
- Before repository or expensive build/package/runtime/remote-mutation work, consult
  ignored `build/agent-process-improvements.md`; update recurring keys; report
  `Process improvements added: none` or keys/issues.
- Keep completion bounded, parser-driven, and secret-free; lease in order; gate
  same-coordinate readers; share migration barrier; unbound records inert. Report
  `Tooling issues: none` or stable keys in ignored
  `build/agent-tooling-issues.md`; never commit/publish/copy to product issues.
- Optional SSH commit signing stays on the host; use
  `.agents/skills/atrinik-github-governance/references/ssh-signing.md` for the
  host/container boundary, and never copy or mount private signing keys into a
  container.
- On touch, refresh existing Atrinik-owned copyright terminal years and blanket holders per
  `CONTRIBUTING.md`; preserve precise attribution.
- MIT reuse follows `docs/PROVENANCE.md` and its canonical registry; rights, identity,
  temporal, authorship, or scope uncertainty fails closed.
- Update `supply-chain/inventory.json` when dependency ownership/validation
  changes; keep Actions/images immutable, add no submodules, and audit a full
  profile; only aggregate-root workflows and Dependabot are active.
- New content/Classic issues name `content@main` and its Classic-target
  artifact; no live 1.x branch/checkout/release label/maintenance line/
  publication target/backport destination exists; historical evidence is
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

Run from repository root; inspect first. `init` clones missing repos;
`sync` never initializes:

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

Handoffs name exact profiles, worktrees, topologies, services, states, scenarios,
prerequisites, results, validation, cleanup; synchronize this guide and
affected skills/docs with contract changes; stale guidance is a defect.
