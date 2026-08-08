---
name: atrinik-multi-repo-workspace
description: Coordinate changes, reviews, builds, releases, and Git worktrees across Atrinik's repositories and monorepos through the atrinik/atrinik workspace. Use for tasks spanning components, choosing an owning repository, synchronizing default branches, creating or combining worktrees, collecting runtime resources, sharing server state, publishing coordinated changes, or changing the workspace manifest, CLI, layout, or operating procedures.
---

# Atrinik multi-repository workspace

## Establish scope and ownership

- Run commands from the `atrinik/atrinik` wrapper root. Read `AGENTS.md`,
  `components.json`, `README.md`, and `docs/ARCHITECTURE.md` before changing
  orchestration behavior.
- Treat `components.json` as the source of truth for physical checkout
  identity, repository, branch, destination, initialization-cohort membership,
  and for each logical component's safe source root, provider roles,
  requirements, license, and build contract. Inspect the selected component's
  own instructions at its resolved source root before editing it.
- Put implementation, tests, packages, and component release configuration in
  the owning physical repository. Put only orchestration, composition,
  manifest, and workspace documentation in the wrapper.
- Expect physical repositories at their manifest destinations below the
  wrapper root, such as `./client`, `./classic`, `./content`, and
  `./content-1x`. The single `atrinik/classic` checkout contains logical
  `classic-server`, `classic-client`, `classic-editor`,
  `classic-libatrinik`, and `classic-protocol` components at their respective
  source directories. The two content checkouts use the same repository but
  distinct `main` and `1.x` branches, destinations, cohorts, and roles.
  Generated worktrees, profiles, builds, and default state remain under
  `./workspace`; `ATRINIK_WORKSPACE_DIR` relocates only that generated and
  mutable data.
- Keep workspace-specific VS Code launch configurations in the wrapper's
  `.devcontainer/` directory. The standalone `devcontainer` component owns the
  reusable images; use `./atrinik init` for post-create repository setup.
- Preserve dirty checkouts and worktrees. Never move component source into the
  wrapper or replace persistent state manually. The checked repository
  migration preserves recoverable originals and may repair only a linked
  worktree's Git administrative `.git` pointer while leaving its path and
  working files untouched.

## Prepare repositories and worktrees

1. Validate and initialize only the required repositories:

   ```sh
   ./atrinik manifest validate
   ./atrinik init
   ./atrinik init COMPONENT...
   ./atrinik init --with classic
   ```

   Plain `init` resolves only the replacement/default cohort. Exact
   `--with classic` is additive: it resolves the default cohort plus the
   complete classic cohort consisting of the `atrinik/classic` monorepo,
   `content-1x@1.x`, and retained GPL tools. The option never means
   classic-only and has no competing alias. Explicit checkout or logical
   component initialization remains available for partial workspaces; aliases
   that own the same physical checkout are cloned once.
   Initialization follows the wrapper repository's GitHub SSH or HTTPS
   transport for new component clones. Keep the wrapper's `origin` (or
   `upstream`) transport current instead of adding per-component URL overrides.

2. Inspect local state with `./atrinik status --json`. This is a quiet,
   non-networked snapshot suitable for automation; its records describe
   physical checkouts, their logical modules, default/classic membership, and
   optional initialization state. Use `sync` before relying on cached
   ahead/behind counts as current GitHub state.
3. Synchronize clean primary checkouts before starting work. Plain
   `./atrinik sync` visits only initialized default-cohort checkouts and never
   clones a missing repository. Use `./atrinik sync --with classic` for
   already initialized default plus classic-cohort members, or `./atrinik sync
   CHECKOUT_OR_COMPONENT...` for exact identities. Multiple names that resolve
   to one physical checkout are synchronized once. Use `--worktrees merge` or
   `--worktrees rebase` only
   when updating every clean attached feature worktree is intentional.
4. Create checkout worktrees through the coordinator:

   ```sh
   ./atrinik worktree create CHECKOUT_OR_COMPONENT LABEL \
     --branch TYPE/TOPIC [--from START_POINT]
   ```

   A Git worktree always contains the complete physical repository. A classic
   worktree therefore lives at `workspace/worktrees/classic/LABEL` and contains
   all five classic source directories, even when it was requested through a
   logical component alias.

5. Commit and push from inside each checkout worktree. Use Conventional
   Commits and open the pull request in that physical repository. Do not create
   a wrapper commit for component-source-only changes.

## Migrate a pre-split workspace

Before replacing a workspace containing the former standalone classic
repositories, initialize the destination `classic` checkout and use the checked
migration workflow:

```sh
./atrinik init classic
./atrinik migrate repositories --dry-run
./atrinik migrate repositories --dry-run --json
./atrinik migrate repositories --apply
./atrinik migrate repositories --audit --json
```

`init classic` needs only the maintained classic `main` branch. Commit maps
prove integrated history. If a mapped branch-only rewritten object was retired,
migration imports the exact verified local source commit as a bridge parent;
do not recreate or fetch a `history/*` namespace.

- Dry-run and audit are read-only. Review every planned move, worktree repair,
  and profile rewrite before apply.
- Historical sources may be at their original canonical paths or the later
  `legacy-server`, `legacy-client`, `legacy-editor`, `legacy-libatrinik`, and
  `legacy-protocol` paths. Apply preflights their identities and matching
  `./classic/server`, `./classic/client`, `./classic/editor`,
  `./classic/libatrinik`, and `./classic/protocol` destinations as one
  recoverable operation. It preserves attributable history and recoverable
  originals, and refuses ambiguous identities, conflicting occupants, unsafe
  Git states, or live affected topologies.
- Leave linked worktree directories at their existing paths, including dirty
  linked worktrees. The migration may repair Git administrative metadata and a
  linked worktree's `.git` pointer as the former common Git directory is
  incorporated or archived; that repair must not change or move its working
  files.
- Rewrite saved classic profiles atomically from former repository roots to
  the new checkout-root/source-root model. Fail the complete profile migration
  when a selector cannot be proven; in particular, never assign an ambiguous
  content worktree to `content-1x` merely because an old profile predates the
  content branch split.
- Treat the migration-only `migrated-worktree` selector as provenance for an
  exact classic profile, not a general override. It is valid only for
  `content-1x` at the old managed content-worktree path while attached to the
  `content@main` Git directory; never create it through `profile set`.
  Exclude it before dirty checks and updates when synchronizing worktrees from
  `content@main`; use a normal `content-1x` worktree for new classic work.
- Do not move or reinterpret content checkouts, state directories, build trees,
  collected runtimes, scenario data, topology records, or logs. Repository
  migration never initializes a checkout. In a pre-split workspace, initialize
  only the destination with `init classic` before the dry run; additive
  `init --with classic` also preflights occupied replacement paths and belongs
  after migration when the rest of the classic cohort is needed.

## Compose and validate coordinated changes

Create a profile from the coherent stack that owns the work. `default` selects
the MIT replacement providers; `classic` selects the currently playable
classic providers. A profile retains this identity and cannot mix replacement
and classic providers in one runnable service closure:

```sh
./atrinik profile create REVIEW --from classic
./atrinik profile set REVIEW classic --worktree LABEL
./atrinik profile set REVIEW COMPONENT --path /absolute/checkout-root
./atrinik profile show REVIEW
./atrinik build all --profile REVIEW --test
```

Selecting physical checkout `classic`, logical component `classic-server`, or
role `server` has the same checkout-wide effect: every in-stack component owned
by `atrinik/classic` receives one identical selector. Profile schema 3 rejects
a profile that tries to select two roots for one physical checkout. Resolution
then appends each component's declared source (`server`, `client`, `editor`,
`libatrinik`, or `protocol`). Combine separate classic module branches into
one monorepo branch and worktree before selecting them in a profile; never
pretend a subdirectory is an independent Git worktree.

Create a related profile with `./atrinik profile create NEW --from REVIEW`.
Resolve a logical component source root for shell or tool use with
`./atrinik path COMPONENT --profile REVIEW`; do not reconstruct managed paths
in scripts. Prefer `--json` for status, worktree, profile, and state listings.

Use primary selectors for unaffected physical checkouts. Logical roles resolve
to one compatible provider per selected stack before paths or build commands
are chosen. The profile build collects the selected content, resources, and
sound into isolated generated views; do not manually copy dependencies between
repositories. Build a single dependency closure with `./atrinik build
COMPONENT --profile REVIEW --test` when a full system build is unnecessary.
Build keys and persisted scenario/topology resolutions include repository,
branch, checkout, source, and logical provider identities. Treat older records
without that full coordinate as inert; never reinterpret them through a newer
manifest.

The replacement `server`, `client`, `editor`, `protocol`, `renderer`,
`content-toolkit`, and `website` repositories are seeds until their owning
build/runtime contracts land. Do not route `default` through classic C/CMake
code or claim it is currently runnable. Use a profile derived from `classic`
for present game build, scenario, and runtime verification. GPL tools belong
only to that classic closure.

The resources repository owns its runtime distribution boundary in
`runtime-paths.txt`. Add a new tracked asset collection to that allowlist in
the same resources change; do not stage repository-wide files or alter the
wrapper to special-case individual metadata filenames. The coordinator serves
only tracked regular files selected by the manifest.

For current runtime testing, use a classic-derived profile. Register persistent
server state once and reuse it across compatible classic profiles:

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
for automatic allocation or choose a distinct explicit port. Once the
replacement stack becomes runnable, a concurrent `default`/`classic`
comparison must also keep the two profiles, topology names, state names,
generated views, and client configuration roots distinct. The supervisor waits
for completed server initialization and its fingerprint, pins the paired client
to it, and isolates each runtime's client configuration. Do not signal
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
  changes. Preserve physical checkout, logical component, source root,
  repository, branch, resolved commit, initialization cohort, role, and license
  in audits and generated SBOMs, including when several classic components
  share one checkout or `content` and `content-1x` share repository
  coordinates. Keep remote Actions on full commits with updater comments,
  images on manifest
  digests, and Git submodules absent. Validate a coordinated profile with
  `./atrinik supply-chain audit --profile PROFILE`; initialize every checkout
  selected by the profile first because an unavailable physical checkout or
  logical component fails the audit. A review-worktree override changes only
  its named checkout and never makes the other profile members optional. In
  CI, provision the selected stacks through wrapper-native `init` rather than
  duplicating the manifest as a checkout list. Generate ignored
  license/CycloneDX/SPDX reports with
  `./atrinik supply-chain report --profile PROFILE`; unresolved and
  non-selected component commits must remain explicitly unavailable. For a
  repeated repository coordinate, accept a supply-chain review-worktree
  override only when it shares the expected checkout primary's common Git
  directory; never infer `content` versus `content-1x` from one-way ancestry.
  In an aggregate monorepo, treat root workflows and Dependabot configuration
  as the active GitHub surface. Imported nested component workflows and
  Dependabot files are inert and must not supply active inventory evidence;
  dependency inputs elsewhere in each logical source remain audited.
- Keep each physical repository independently releasable. A squash commit in
  `atrinik/classic` changes and releases that monorepo; do not describe its
  logical source directories as independently cloned repositories. Never
  couple publication to a wrapper checkout or submodule pointer.
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

## Apply approved historical MIT provenance grants

The following registry is exhaustive. Each person explicitly grants permission
for the listed treatment of the listed original past Atrinik contributions.

| Grantor | Covered material | Permitted operations | Destination license |
| --- | --- | --- | --- |
| Zoey Rose | All original past Atrinik contributions solely authored by Zoey Rose | Copy, migrate, translate, or relicense | MIT |
| Daniel Liptrot | All original past Atrinik contributions solely authored by Daniel Liptrot | Copy, migrate, translate, or relicense | MIT |

A registry entry is not an automatic license change for a repository, file, or
current-blame region. Before applying a grant:

1. Identify the exact source repository, path, and revision or revision range,
   following renames and moves through the complete, non-shallow history.
2. Inspect the creation and every subsequent change to the selected material,
   including relevant commit diffs. Prove that it is the named grantor's
   original work and that the grantor solely authored it. Map historical author
   identities to that grantor with recorded evidence; do not treat current
   blame alone as proof.
3. Review the material for copied, generated, vendored, or otherwise embedded
   third-party work and for notices or licenses that conflict with MIT reuse.
   A history gap, unresolved identity, mixed authorship, or uncertain origin
   makes the material ineligible until independently resolved.
4. Limit reuse to independently separable material covered by the proof. Do
   not copy a whole mixed-authorship file merely because some surviving lines
   qualify.
5. Record the exact source and destination repositories and paths, source
   revision, complete history and identity evidence, transformation performed,
   third-party review, and the applicable grantor and grant in the destination
   pull request or a committed provenance manifest. Cite the exact wrapper
   repository revision containing the applicable registry entry as the grant
   evidence. Retain any notices required by material that is deliberately
   included under another compatible license.

## Maintain this guidance

After changing the wrapper CLI, manifest schema or entries, managed directory
layout, ownership boundary, profile/build/runtime behavior, release policy, or
cross-repository procedure, review and update this skill, root `AGENTS.md`,
`README.md`, and `docs/ARCHITECTURE.md` in the same wrapper change. Remove
superseded instructions instead of documenting parallel procedures. Validate
the skill with the repository's skill validator and exercise the affected CLI
workflow before committing.
