# Atrinik workspace repository guide

## Ownership and routing

- This repository owns multi-repository orchestration, not component source.
  Keep implementation, tests, packages, and component release configuration in
  the owning physical repository.
- Use `atrinik-multi-repo-workspace` for cross-checkout ownership, profiles,
  worktrees, migration, cleanup, releases, or wrapper CLI/layout changes. Add
  only the narrow specialist skill needed for classic C, protocol, content,
  runtime, scenarios, or GitHub governance work.
- Resolve ownership from `components.json`, then read the owning checkout's
  nearest `AGENTS.md`. Wrapper skills coordinate those contracts; they must not
  duplicate component implementation guidance.
- Checkouts are independent ignored Git repositories at manifest destinations.
  `./classic` is one monorepo owning five `classic-*` logical components;
  `content` is `atrinik/content@main`, while `content-1x` is the independent
  `atrinik/content@1.x` checkout. `.devcontainer/` owns workspace composition;
  the `devcontainer` checkout owns reusable images.

## Safety and coherent stacks

- Never replace or move a dirty primary checkout, remove a dirty worktree, or
  overwrite mutable server data. Preserve recoverable originals during checked
  migration; only the migration command may repair a linked worktree's Git
  administrative pointer without changing its working files.
- Cleanup is explicit: preview `./atrinik cleanup --dry-run --json` before exact
  `--apply`; never run it implicitly. Text uses IEC sizes; JSON keeps exact
  bytes. Preserve dirty, detached, locked, in-progress, referenced, or
  uncertain worktrees; profiles, scenarios, states, topology records/logs,
  migrations, retention records, branches, Git objects, review reports, and
  unmarked paths. The npm cache is opt-in. Apply recomputes under the layout
  lock, uses non-force Git worktree removal, and reports partial failures.
  Status uses `--ignore-submodules=none`; populated submodules protect
  worktree-specific refs, reflogs, and objects. Historical wrapper cleanup
  allows only direct `build/worktrees/` children with authenticated PR and
  frozen ancestry proof. It ignores replace refs, rejects `info/grafts`, fails
  closed on ambiguity, and is rerun by apply.
- Use profiles for source selection. `default` selects the MIT replacement
  stack and `classic` the playable classic stack. Never mix providers or route
  unavailable replacement adapters through classic code. Replacement
  repositories have standalone M1 foundations, but their wrapper build/runtime
  adapters and integrated service closure have not landed.
- A worktree belongs to a physical checkout. Selecting `classic`, a
  `classic-*` component, or one of its roles changes all five classic selectors
  to the same checkout root; profile resolution appends the manifest source.
- Use `./atrinik path`, `topology show`, `up`, `ps`, `logs`, and `down`; do not
  reconstruct managed build, runtime, PID, log, lock, or state paths. Persisted
  records without current immutable repository/branch/checkout/source/provider
  coordinates are historical and inert.
- Give concurrent topologies distinct names and server states. Keep comparison
  profiles, ports, generated roots, and client configuration isolated.
- Do not mention confidential or unreleased Atrinik work in commits, issues,
  pull requests, or other public surfaces.

## Provenance, dependencies, and publication

- Follow [`docs/PROVENANCE.md`](docs/PROVENANCE.md) for the exhaustive approved
  historical MIT grantor registry and evidence contract. Reuse fails closed on
  incomplete history, mixed authorship, uncertain identity, or embedded
  third-party material. Cite the exact wrapper revision containing the registry
  used.
- `supply-chain/inventory.json` is the organization dependency-ownership source.
  Update it for changed toolchains, packages, Actions, images, vendored inputs,
  licenses, owners, cadences, EOL responses, or validation paths. Keep Actions
  and images immutable, retain updater hints, add no submodules, initialize the
  complete selected profile, and run `./atrinik supply-chain audit --profile
  PROFILE`. A review override never makes another profile member optional.
- Treat only an aggregate checkout's root workflows and Dependabot file as
  active GitHub configuration. Nested imported copies are inert history.
- Commits and pull-request titles use Conventional Commits. Semantic-release
  owns every squash release; never create or move release tags/assets manually.
  Use `atrinik-github-governance` for policy, Actions, or release contracts.

## Workflow and validation

- `./atrinik init` initializes the replacement/default cohort. Exact
  `./atrinik init --with classic` adds the complete classic cohort; `sync`
  never initializes repositories. Use the multi-repository skill's migration
  reference for a pre-split workspace.
- Handoffs must provide copy-pasteable wrapper commands with exact profile,
  worktree, topology, service, state, and scenario names. Include automated
  validation, prerequisites, observable results, and cleanup. Use
  `atrinik-test-scenario` for a ready account/character; never handcraft saves or
  expose credentials.
- Before finishing wrapper changes, run the complete unittest/coverage suite,
  compileall, manifest validation, `git diff --check`, ShellCheck for shell
  changes, actionlint for workflows, and the profile-aware supply-chain audit
  when dependency inputs change. Preserve `.coveragerc` and OIDC Codecov
  boundaries.
- Keep this guide, affected skills, `README.md`, and `docs/ARCHITECTURE.md`
  synchronized when wrapper ownership, commands, layout, or safety contracts
  change. Treat stale agent guidance as a defect. Deep-review reports are
  ignored local artifacts under `build/`.
