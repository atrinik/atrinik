# Atrinik workspace repository guide

- This repository owns multi-repository development orchestration, not
  component source. Never copy component implementation into it.
- For work that spans component repositories, read and follow
  `.agents/skills/atrinik-multi-repo-workspace/SKILL.md` before changing a
  checkout, worktree, profile, release configuration, or GitHub repository.
- Use the narrowest specialist skill that owns the change: `atrinik-c-change`
  for native code, `atrinik-protocol-change` for wire contracts,
  `atrinik-content-change` for authored world data, `atrinik-server-runtime`
  for classic server execution, and `atrinik-github-governance` for repository
  policy or Actions. Combine one with the multi-repository skill when ownership
  crosses component boundaries.
- Primary component checkouts are independent ignored Git repositories directly
  below the wrapper root. Component worktrees, builds, profiles, and default
  mutable state belong below the ignored workspace directory or an explicit
  external path.
- Keep wrapper-specific VS Code launch configuration under `.devcontainer/`.
  The standalone `devcontainer` component owns reusable toolchain images, not
  workspace composition; initialize checkouts through `./atrinik init`.
- Never replace a dirty checkout, remove a dirty worktree, or overwrite an
  existing mutable server-data directory.
- Use profiles to declare mixed component sources. Use `topology show` to audit
  their exact resolution and `up`/`ps`/`logs`/`down` for persistent supervised
  client/server testing; do not reconstruct internal build, runtime, PID, log,
  or state-lock paths in ad hoc scripts.
- Give concurrent topologies distinct names and server states. Let `up` choose
  ports or assign distinct `--port` values; do not manually assemble a client
  endpoint or share client configuration directories between topology names.
- Use precise component and protocol names; do not use vague age-based labels.
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
- Keep component-specific instructions in the owning component repository.
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
