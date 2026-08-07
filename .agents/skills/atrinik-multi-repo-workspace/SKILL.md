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
- Expect primary component repositories directly below the wrapper root, such
  as `./client` and `./server`. Generated worktrees, profiles, builds, and
  default state remain under `./workspace`; `ATRINIK_WORKSPACE_DIR` relocates
  only that generated and mutable data.
- Keep workspace-specific VS Code launch configurations in the wrapper's
  `.devcontainer/` directory. The standalone `devcontainer` component owns the
  reusable images; use `./atrinik init` for post-create repository setup.
- Preserve dirty checkouts and worktrees. Never move component source into the
  wrapper or replace persistent state.

## Prepare repositories and worktrees

1. Validate and initialize only the required repositories:

   ```sh
   ./atrinik manifest validate
   ./atrinik init [COMPONENT...]
   ```

   Initialization follows the wrapper repository's GitHub SSH or HTTPS
   transport for new component clones. Keep the wrapper's `origin` (or
   `upstream`) transport current instead of adding per-component URL overrides.

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

The resources repository owns its runtime distribution boundary in
`runtime-paths.txt`. Add a new tracked asset collection to that allowlist in
the same resources change; do not stage repository-wide files or alter the
wrapper to special-case individual metadata filenames. The coordinator serves
only tracked regular files selected by the manifest.

For runtime testing, register persistent server state once and reuse it across
profiles:

```sh
./atrinik state add NAME
./atrinik state add NAME --path /absolute/server-data
./atrinik run server --profile REVIEW --state NAME --port UDP_PORT --dry-run
./atrinik run client --profile REVIEW --state NAME --port UDP_PORT --dry-run
```

Remove `--dry-run` only when an actual launch is intended. For foreground use,
start the server first so its persistent QUIC identity exists, then use the
same state and port for the client. The client is pinned to that identity and
metaserver/STUN discovery is disabled; the server disables STUN discovery and
automatic port mapping. Verify display forwarding before opening the client.
Do not run two servers against one state directory outside the coordinator's
locking model. Prefer the supervised lifecycle below for routine paired use.

For a persistent client/server review session, treat the profile as a source
topology and use the supervised lifecycle:

```sh
./atrinik topology show REVIEW --json
./atrinik up --name RUNTIME --profile REVIEW --state NAME [--port UDP_PORT]
./atrinik ps [RUNTIME] --json
./atrinik logs RUNTIME server --follow
./atrinik down RUNTIME
```

Select one service with `up --service server` or `--service client`. Use a
distinct `--name` and state for every concurrent server topology; omit `--port`
for automatic allocation or choose a distinct explicit port. The supervisor
waits for completed server initialization and its fingerprint, pins the paired
client to it, and isolates each runtime's client configuration. Do not signal
recorded PIDs directly or tail internal files by reconstructed paths: the
coordinator verifies process start identity, rotates logs, performs graceful
shutdown, and holds the server-state lock through its supervisor.

## Provide manual verification handoffs

End every implementation or change handoff with copy-pasteable manual
verification commands using `./atrinik` wherever the coordinator supports the
workflow. Use the concrete names created for the task rather than placeholders.
Include the narrow automated build/test command and, when runtime behavior is
relevant, the complete supervised lifecycle:

```sh
./atrinik profile show PROFILE
./atrinik build COMPONENT --profile PROFILE --test
./atrinik topology show PROFILE --json
./atrinik up --name TOPOLOGY --profile PROFILE --state STATE
./atrinik ps TOPOLOGY --json
./atrinik logs TOPOLOGY [server|client] --follow
./atrinik down TOPOLOGY
```

State display forwarding or other prerequisites, describe the exact manual
actions and expected results between `up` and `down`, and always include the
cleanup command. Do not substitute reconstructed internal build/runtime paths
or direct executable invocations for a supported wrapper command. If runtime
verification is not applicable, say so and provide the wrapper build/test and
inspection commands that are applicable.

When the reviewer needs a ready login, also use the
`atrinik-test-scenario` skill. Provision with `./atrinik scenario create`,
retrieve the secret only through `scenario credentials`, and use the scenario's
dedicated state in the lifecycle above. Never construct account or player save
files directly.

## Coordinate releases and GitHub changes

- Treat `supply-chain/inventory.json` in the wrapper as the aggregate ownership
  catalog while component lockfiles remain their release integrity boundary.
  Update the catalog when a supported dependency, toolchain, action, image,
  vendored source, license, owner, cadence, EOL response, or validation path
  changes. Keep remote Actions on full commits with updater comments, images on
  manifest digests, and Git submodules absent. Validate a coordinated profile
  with `./atrinik supply-chain audit --profile PROFILE` and generate ignored
  license/CycloneDX/SPDX reports through the same command.
- Keep each repository independently releasable. A component squash commit
  changes and releases that component; never couple publication to a wrapper
  checkout or submodule pointer.
- Preserve semantic-release's Conventional Commits precedence: breaking
  changes are major, `feat` is minor, and other accepted squash commits are
  patch releases. Keep the catch-all patch rule below explicit major and minor
  rules so every squash commit releases without downgrading them.
- Keep release workflows manually dispatchable without alternate version
  inputs. After an Actions outage, rerun a failed release run or dispatch the
  standard semantic-release workflow; do not create tags by hand.
- Apply repository-policy, workflow, required-check, permission, or release
  changes using the Atrinik GitHub-governance instructions. Update
  `atrinik/github-settings` whenever desired organization state changes.
- Inspect every affected repository's status and remote default branch before
  publishing. Report repository-specific commits, checks, releases, and any
  incomplete external operation separately.

## Reuse Zoey Rose contributions under MIT

Zoey Rose grants permission to copy, migrate, translate, or relicense under
MIT any of her original past Atrinik contributions. Apply that grant only to
selected material for which a complete Git-history audit proves sole Zoey Rose
authorship and a content review finds no embedded third-party or
conflicting-licensed material. This grant is not an automatic license change
for a repository, file, or current-blame region.

Before applying the grant:

1. Identify the exact source repository, path, and revision or revision range,
   following renames and moves through the complete, non-shallow history.
2. Inspect the creation and every subsequent change to the selected material,
   including relevant commit diffs. Map historical author identities to Zoey
   Rose with recorded evidence; do not treat current blame alone as proof.
3. Review the material for copied, generated, vendored, or otherwise embedded
   third-party work and for notices or licenses that conflict with MIT reuse.
   A history gap, unresolved identity, mixed authorship, or uncertain origin
   makes the material ineligible until independently resolved.
4. Limit reuse to independently separable material covered by the proof. Do
   not copy a whole mixed-authorship file merely because some surviving lines
   qualify.
5. Record the exact source and destination repositories and paths, source
   revision, complete history and identity evidence, transformation performed,
   third-party review, and Zoey Rose's grant in the destination pull request or
   a committed provenance manifest. Retain any notices required by material
   that is deliberately included under another compatible license.

## Maintain this guidance

After changing the wrapper CLI, manifest schema or entries, managed directory
layout, ownership boundary, profile/build/runtime behavior, release policy, or
cross-repository procedure, review and update this skill, root `AGENTS.md`,
`README.md`, and `docs/ARCHITECTURE.md` in the same wrapper change. Remove
superseded instructions instead of documenting parallel procedures. Validate
the skill with the repository's skill validator and exercise the affected CLI
workflow before committing.
