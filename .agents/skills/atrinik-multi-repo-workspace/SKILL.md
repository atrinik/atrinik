---
name: atrinik-multi-repo-workspace
description: Coordinate work across checkouts, profiles, worktrees, cleanup, releases, or wrapper CLI and layout.
---

# Atrinik multi-repository workspace

## Resolve scope and ownership

Run commands from the `atrinik/atrinik` wrapper root.

1. Read root `AGENTS.md` and only task-relevant wrapper sources:
   - ownership, cohorts, roles, and build contracts: `components.json`;
   - operator behavior: the matching `README.md` section;
   - layout, locking, trust, and lifecycle design: the matching
     `docs/ARCHITECTURE.md` section;
   - pre-split repositories: [repository migration](references/repository-migration.md).
2. Resolve each logical component to its physical checkout and safe source root;
   read that checkout's nearest `AGENTS.md` before editing.
3. Keep implementation, tests, packages, and releases in the physical owner;
   keep only orchestration, composition, manifest, and wrapper docs here.

Checkouts are independent ignored Git repositories. One `classic` worktree
contains all five `classic-*` components; `content@main` and `content-1x@1.x`
are separate checkouts. Keep wrapper VS Code composition under `.devcontainer/`
and reusable images in the `devcontainer` checkout.

## Prepare safe worktrees

Inspect local state before mutation:

```sh
./atrinik manifest validate
./atrinik status --json
```

Initialize only absent repositories. Plain `init` selects the replacement
cohort; exact `--with classic` adds the complete classic cohort. `sync` never
clones.

```sh
./atrinik init [COMPONENT...]
./atrinik init --with classic
./atrinik sync [COMPONENT...]
./atrinik sync --with classic
./atrinik worktree create COMPONENT LABEL --branch TYPE/TOPIC
```

Synchronize only clean primaries; never implicitly replace, move, or remove a
dirty checkout or worktree. A logical classic selector creates the full
repository under `workspace/worktrees/classic/LABEL`. Commit and push from each
owning worktree.

Reclaim completed review data only through preview-first cleanup:

```sh
./atrinik cleanup --dry-run --json
./atrinik cleanup --scope worktrees --scope builds CHECKOUT... --older-than 7
./atrinik cleanup --scope all --older-than 7 --apply
```

Default scope covers registered worktrees and marker-owned builds; npm cache is
opt-in. Text uses IEC sizes; JSON keeps exact bytes. References, ambiguous Git,
and unsafe paths/markers protect targets. Only an `atrinik/atrinik@main`
worktree directly below `build/worktrees/` has the historical-base exception:
its PR targets `master` and supplies head, base, and merge SHAs; the merge's
first parent equals the base and is ancestral to frozen boundary
`ee5ba2096c94bce0161629423d4962a966bc61d8`. Proof ignores replace refs and
rejects `info/grafts`. Under the layout lock, `--apply` reinventories and reruns
proof plus exact-target safety; uncertainty fails closed. Status uses
`--ignore-submodules=none`; populated submodule Git data protects. Cleanup
removes builds first via non-force Git, preserving branch refs, profiles,
scenarios, states, topology records/logs, migrations, retention records, Git
objects, review reports, and unmarked paths. Hand off exact fixtures, JSON
commands, reasons, and preserved records.

## Compose coherent sources

Use `default` for replacement sources and `classic` for the playable stack.
Never mix providers or substitute classic C/CMake for a missing replacement
adapter. Replacement repositories have standalone M1 foundations, but no
wrapper replacement build/runtime closure yet.

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW CHECKOUT_OR_COMPONENT --worktree LABEL
./atrinik profile show REVIEW --json
./atrinik path COMPONENT --profile REVIEW
./atrinik build COMPONENT --profile REVIEW --test
```

Selecting `classic`, one of its components, or a provided role updates all five
classic selectors to one root; resolution appends each manifest source. Never
treat a classic subdirectory as an independent worktree.

Build, scenario, and topology records bind exact repository, branch, checkout,
source, component, and provider coordinates. Records lacking current immutable
coordinates are inert. Let the wrapper collect content/resources and own
generated paths, locks, state, PIDs, logs, and process cleanup.

For classic server execution or diagnosis, load `atrinik-server-runtime`. For
a ready account and character, also load `atrinik-test-scenario`; never
handcraft saves or expose credentials. Keep every concurrent topology on a
distinct name and server state.

## Validate and hand off

Use wrapper-native commands wherever supported; a runtime handoff uses concrete
names and follows this lifecycle:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --json
./atrinik up --name TOPOLOGY --profile PROFILE --state STATE
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

Record prerequisites, actions, expected results, and cleanup. If runtime is
irrelevant, hand off applicable build/test/inspection commands. Do not
substitute internal executables or generated paths for supported wrapper
operations.

## Coordinate publication and policy

- Use `atrinik-github-governance` for PR publication or policy; compare live and
  desired state before authorized policy mutations.
- PR titles use `type(optional-scope)!: concise description`; bodies use
  renderable GitHub-Flavored Markdown and actual line breaks, never visible
  literal `\n` separators. Give multi-section bodies by file or stdin; after
  create/edit, verify remote rendering.
- Keep physical repositories independently releasable; semantic-release owns
  publication. Dependency changes require `supply-chain/inventory.json` and a
  complete-profile audit; overrides stay mandatory and only aggregate roots own
  workflows/Dependabot.
- Apply historical MIT grants only under
  [`docs/PROVENANCE.md`](../../../docs/PROVENANCE.md); fail closed and record
  complete evidence in the destination.

## Maintain guidance

Load `atrinik-guidance-maintenance` when wrapper ownership, CLI, layout,
profile/build/runtime, cleanup, release, or cross-repository contracts change.
Update only affected canonical guidance, remove superseded instructions, and
run that skill's inventory and validation workflow.
