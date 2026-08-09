---
name: atrinik-guidance-maintenance
description: Audit and synchronize Atrinik AGENTS.md, skills, README, architecture, and contributor guidance after CLI, ownership, layout, safety, or validation changes and during daily, weekly, or other periodic drift reviews.
---

# Maintain Atrinik agent guidance

Run from the wrapper root. Stay read-only until evidence identifies drift; do
not initialize, sync, clean up, launch, or mutate external state for an audit.

## Gather evidence

1. Inspect `git status --short`, `./atrinik manifest validate`, and `./atrinik
   status --json`. Preserve dirty checkouts, report unavailable ones, and
   remember root Git status omits ignored repositories.
2. Review recent/path-specific history. Verify behavior against code, CLI
   `--help`, the manifest, CI, and relevant README/architecture sections.
3. Map affected components to physical checkouts; read each nearest
   `AGENTS.md` and relevant skill. Exclude generated/preserved copies below
   `workspace/` and `build/` from the authoritative inventory.
4. Record path, owner, lines, evidence, and status: current, stale, missing,
   duplicated, or unverifiable.

## Correct ownership and drift

- Keep root `AGENTS.md` below 150 lines; put the overview, folder map, routing,
  universal safety, and exact contributor commands there.
- Put component architecture/validation in the nearest nested guide, never in
  the wrapper. Put repetitive non-obvious procedures in skills; use only
  `name` and a trigger-optimized `description` in frontmatter, and align the
  concise imperative body with `agents/openai.yaml`.
- Put operator behavior in `README.md`, lifecycle/trust invariants in
  `docs/ARCHITECTURE.md`, and contributor checks in `CONTRIBUTING.md`.
- Remove stale duplication and link to its owner. Edit only evidenced drift and
  synchronize only surfaces sharing the contract. For skill additions/removals,
  update inventory regressions and UI metadata. Load
  `atrinik-multi-repo-workspace` when the cross-checkout contract itself changes.

## Validate

```sh
python3 -m coverage run -m unittest discover -v
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
./atrinik manifest validate
git diff --check
```

Run the active Codex `skill-creator` validator for changed skills. Add
ShellCheck, actionlint, profile builds, or supply-chain audits only when
relevant. Never claim coverage for unread checkouts.

## Report

```text
Guidance audit
Scope: repositories and revision/range reviewed
History/direction: evidence-backed themes
Inventory: path | owner | lines | status
Changes:
- path — correction and reason
Validation:
- command — pass, fail, or not run with reason
Gaps:
- inaccessible/unverified contract, or none
```

For a no-change audit, write `Changes: none` and cite supporting evidence.
