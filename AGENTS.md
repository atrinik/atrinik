# Atrinik workspace repository guide

- This repository owns multi-repository development orchestration, not
  component source. Never copy component implementation into it.
- For work that spans physical repositories or logical component source roots,
  read and follow
  `.agents/skills/atrinik-multi-repo-workspace/SKILL.md` before changing a
  checkout, worktree, profile, release configuration, or GitHub repository.
- Use the narrowest specialist skill that owns the change: `atrinik-c-change`
  for native code, `atrinik-protocol-change` for wire contracts,
  `atrinik-content-change` for authored world data, `atrinik-server-runtime`
  for classic server execution, and `atrinik-github-governance` for repository
  policy or Actions. Combine one with the multi-repository skill when ownership
  crosses component boundaries.
- Physical checkouts are independent ignored Git repositories at the
  manifest's explicit destinations directly below the wrapper root. Logical
  components resolve to safe source roots within those checkouts. In
  particular, the single `atrinik/classic` checkout at `./classic` owns the
  `classic-server`, `classic-client`, `classic-editor`,
  `classic-libatrinik`, and `classic-protocol` source directories. Treat
  `content` (`atrinik/content@main`) and `content-1x`
  (`atrinik/content@1.x`) as distinct physical checkouts. Checkout worktrees,
  builds, profiles, and default mutable state belong below the ignored
  workspace directory or an explicit external path.
- Keep wrapper-specific VS Code launch configuration under `.devcontainer/`.
  The standalone `devcontainer` component owns reusable toolchain images, not
  workspace composition. Plain `./atrinik init` is replacement/default-only;
  only exact `./atrinik init --with classic` adds the complete classic cohort:
  the `classic` monorepo, `content-1x`, and retained GPL tools.
- Never manually replace or move a dirty primary checkout, remove a dirty
  worktree, or overwrite an existing mutable server-data directory. A checked
  repository migration preserves recoverable originals and may repair a dirty
  linked worktree's Git administrative `.git` pointer while leaving its
  working directory and files in place; no other dirty-worktree mutation is
  implicit.
- Use profiles to declare coherent component sources. `default` selects the MIT
  replacement stack and `classic` selects the current playable classic stack;
  never mix replacement and classic providers in one runnable service closure.
  Use `topology show` to audit exact role resolution and `up`/`ps`/`logs`/`down`
  for persistent supervised testing; do not reconstruct internal build,
  runtime, PID, log, or state-lock paths in ad hoc scripts.
- Build namespaces and persisted scenario/topology resolution records bind
  every provider's repository, branch, checkout, source, and logical identity.
  Records lacking the current immutable coordinate shape are historical and
  inert; never reinterpret them through the current manifest.
- Replacement `server`, `client`, `editor`, `protocol`, `renderer`,
  `content-toolkit`, and `website` repositories are seeds until their own build
  and runtime contracts land. Do not route `default` through classic code or
  claim it is currently runnable. Current game build/runtime verification uses
  a profile created from `classic`.
- Give concurrent topologies distinct names and server states. A concurrent
  classic/replacement comparison also uses distinct `classic`/`default`
  profiles and generated roots. Let `up` choose ports or assign distinct
  `--port` values; do not manually assemble a client endpoint or share client
  configuration directories between topology names.
- Use precise component and protocol names; do not use vague age-based labels.
- A worktree always belongs to a physical checkout. Classic worktrees therefore
  live below `workspace/worktrees/classic/` and contain every monorepo source
  directory. In `profile set`, selecting physical checkout `classic`, any
  `classic-*` logical component, or one of its roles changes all five classic
  selectors together. They must always name the same full checkout root.
  Profile resolution then appends each component's manifest `source` path.
- Do not mention confidential or unreleased Atrinik work in committed files or
  public project surfaces.
- Apply a historical MIT provenance grant only for a person listed in the
  approved grantors table below and only within that row's stated scope. Every
  use must satisfy the shared proof and recording requirements below the table.
- Pull-request titles and commits use Conventional Commits style. Every squash
  merge is released by semantic-release.
- Run the complete Python test suite, compileall, ShellCheck for shell changes,
  actionlint for workflow changes, and `git diff --check` before finishing.
  Coordinator logic changes must also preserve the `.coveragerc` source and
  omission boundaries and the OIDC-authenticated Codecov report.
- `supply-chain/inventory.json` is the organization-wide dependency ownership
  source. Update it with every supported toolchain, package source, action,
  image, vendored input, license, owner, cadence, EOL response, and validation
  path. Keep Actions and images immutable, retain updater hints, do not add Git
  submodules, and run the profile-aware `./atrinik supply-chain audit`.
  For an aggregate monorepo checkout, inventory and audit active Actions and
  Dependabot configuration at the checkout root; imported `.github/workflows`
  and `.github/dependabot.yml` files below logical component source roots are
  inert history, not active dependency evidence.
  Records and SBOMs must distinguish physical checkout, logical component,
  source root, repository, branch, commit, cohort, role, and license even when
  repository coordinates repeat or components share one checkout.
- `sync` never initializes a repository. With no names it touches only
  initialized default-cohort checkouts; opt-in classic-cohort synchronization
  uses exact `--with classic` or explicit checkout/component names. Operations
  deduplicate aliases that resolve to the same physical checkout.
- Before combining former standalone classic repositories, use
  exact `./atrinik init classic`, then
  `./atrinik migrate repositories --dry-run`, `--apply`, and `--audit`.
  Initialization uses classic `main` only. Migration must bridge a verified
  branch-only local commit directly when its retired rewritten map target is
  absent; never recreate or depend on a classic `history/*` namespace.
  Do not run additive `init --with classic` until after migration in a
  pre-split workspace because its default-cohort preflight must reject the
  former classic repositories occupying replacement paths.
  Migration preflights proven former primaries for the corresponding
  `./classic/<source>` directories, preserves recoverable originals and
  linked-worktree paths, rewrites proven classic profiles atomically, and
  refuses ambiguous or unsafe states, live affected topologies, or conflicting
  destinations. It leaves content, states, builds, runtimes, and logs
  untouched.
- Every implementation or change handoff must include copy-pasteable manual
  verification commands that use the thin `./atrinik` wrapper whenever it
  supports the workflow. Use exact profile, worktree, topology, service, and
  state names; include the automated build/test command, the complete
  `topology show`/`up`/`ps`/`logs`/`down` lifecycle when runtime verification
  is relevant, feature-specific actions and expected results, display or other
  prerequisites, and the cleanup command. Do not replace supported wrapper
  commands with reconstructed build paths or direct executable invocations.
- Use `.agents/skills/atrinik-test-scenario/SKILL.md` when manual verification
  benefits from a ready local account and character. Keep scenario credentials
  and state ignored and isolated; never handcraft account or player files.
- Keep component-specific instructions in the owning physical repository and
  its appropriate logical source root.
  Wrapper skills may coordinate those contracts, but must not duplicate their
  implementation or become a second source of truth for component commands.
- Deep-review reports are ignored local artifacts under `build/`; do not commit
  them.
- Keep this guide, the multi-repository skill, `README.md`, and
  `docs/ARCHITECTURE.md` synchronized with changes to the `atrinik` command,
  `components.json`, managed layout, ownership boundaries, or cross-repository
  development and release procedures.
- When major rework changes ownership, layout, commands, safety boundaries, or
  validation contracts, update every affected `AGENTS.md` and skill in the same
  change. Treat stale agent guidance as an implementation defect.

## Approved historical MIT provenance grantors

This table is exhaustive. Each person explicitly grants permission for the
listed treatment of the listed original past Atrinik contributions.

| Grantor | Covered material | Permitted operations | Destination license |
| --- | --- | --- | --- |
| Zoey Rose | All original past Atrinik contributions solely authored by Zoey Rose | Copy, migrate, translate, or relicense | MIT |
| Daniel Liptrot | All original past Atrinik contributions solely authored by Daniel Liptrot | Copy, migrate, translate, or relicense | MIT |

Apply a grant only when a complete, non-shallow Git-history audit, including
renames and moves, proves that the selected material is the named grantor's
original work and that the grantor solely authored it. Verify historical author
identities and review the material for embedded third-party or
conflicting-licensed work. Current blame alone and mixed, incomplete, or
uncertain history are not sufficient; fail closed until every doubt is
independently resolved. Reuse only independently separable material covered by
the proof. Record the exact source repository, path, and revision; destination
repository and path; complete history and author-identity evidence;
transformation and third-party review; and the applicable grantor and grant in
the destination pull request or a committed provenance manifest. Cite the exact
wrapper repository revision containing the applicable registry entry as the
grant evidence.
