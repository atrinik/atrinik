---
name: atrinik-multi-repo-workspace
description: Coordinate changes, reviews, builds, releases, and Git worktrees across Atrinik's standalone repositories through the atrinik/atrinik workspace. Use for tasks spanning components, choosing an owning repository, synchronizing default branches, creating or combining worktrees, collecting runtime resources, sharing server state, publishing coordinated changes, or changing the workspace manifest, CLI, layout, or operating procedures.
---

# Atrinik multi-repository workspace

## Establish scope and ownership

- Run commands from the `atrinik/atrinik` wrapper root. Read `AGENTS.md`,
  `components.json`, `README.md`, and `docs/ARCHITECTURE.md` before changing
  orchestration behavior.
- Treat `components.json` as the source of truth for repository identity,
  default branch, and build contract. Inspect the selected component's own
  instructions before editing its source.
- Put implementation, tests, packages, and component release configuration in
  the owning component repository. Put only orchestration, composition,
  manifest, and workspace documentation in the wrapper.
- Preserve dirty checkouts and worktrees. Never move component source into the
  wrapper or replace persistent state.

## Prepare repositories and worktrees

1. Validate and initialize only the required repositories:

   ```sh
   ./atrinik manifest validate
   ./atrinik init [COMPONENT...]
   ```

2. Inspect local state with `./atrinik status --json`. This is a quiet,
   non-networked snapshot suitable for automation; use `sync` before relying on
   its cached ahead/behind counts as current GitHub state.
3. Synchronize clean primary checkouts before starting work. Use
   `./atrinik sync [COMPONENT...]`; use `--worktrees merge` or
   `--worktrees rebase` only when updating every clean attached feature
   worktree is intentional.
4. Create component worktrees through the coordinator:

   ```sh
   ./atrinik worktree create COMPONENT LABEL \
     --branch TYPE/TOPIC [--from START_POINT]
   ```

5. Commit and push from inside each component worktree. Use Conventional
   Commits and open the pull request in that component repository. Do not
   create a wrapper commit for component-only changes.

## Compose and validate coordinated changes

Create a profile when validation needs a non-default combination:

```sh
./atrinik profile create REVIEW
./atrinik profile set REVIEW COMPONENT --worktree LABEL
./atrinik profile set REVIEW COMPONENT --path /absolute/checkout
./atrinik profile show REVIEW
./atrinik build all --profile REVIEW --test
```

Create a related profile with `./atrinik profile create NEW --from REVIEW`.
Resolve a checkout for shell or tool use with
`./atrinik path COMPONENT --profile REVIEW`; do not reconstruct managed paths
in scripts. Prefer `--json` for status, worktree, profile, and state listings.

Use primary selectors for unaffected components. The profile build collects the
selected content, resources, and sound into isolated generated views; do not
manually copy dependencies between repositories. Build a single dependency
closure with `./atrinik build COMPONENT --profile REVIEW --test` when a full
system build is unnecessary.

For runtime testing, register persistent server state once and reuse it across
profiles:

```sh
./atrinik state add NAME
./atrinik state add NAME --path /absolute/server-data
./atrinik run server --profile REVIEW --state NAME --dry-run
./atrinik run client --profile REVIEW --dry-run
```

Remove `--dry-run` only when an actual launch is intended. Verify display
forwarding before opening the client. Do not run two servers against one state
directory outside the coordinator's locking model.

## Coordinate releases and GitHub changes

- Keep each repository independently releasable. A component squash commit
  changes and releases that component; never couple publication to a wrapper
  checkout or submodule pointer.
- Preserve semantic-release's Conventional Commits precedence: breaking
  changes are major, `feat` is minor, and other accepted squash commits are
  patch releases. Keep the catch-all patch rule below explicit major and minor
  rules so every squash commit releases without downgrading them.
- Apply repository-policy, workflow, required-check, permission, or release
  changes using the Atrinik GitHub-governance instructions. Update
  `atrinik/github-settings` whenever desired organization state changes.
- Inspect every affected repository's status and remote default branch before
  publishing. Report repository-specific commits, checks, releases, and any
  incomplete external operation separately.

## Maintain this guidance

After changing the wrapper CLI, manifest schema or entries, managed directory
layout, ownership boundary, profile/build/runtime behavior, release policy, or
cross-repository procedure, review and update this skill, root `AGENTS.md`,
`README.md`, and `docs/ARCHITECTURE.md` in the same wrapper change. Remove
superseded instructions instead of documenting parallel procedures. Validate
the skill with the repository's skill validator and exercise the affected CLI
workflow before committing.
