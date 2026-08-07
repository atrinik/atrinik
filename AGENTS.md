# Atrinik workspace repository guide

- This repository owns multi-repository development orchestration, not
  component source. Never copy component implementation into it.
- For work that spans component repositories, read and follow
  `.agents/skills/atrinik-multi-repo-workspace/SKILL.md` before changing a
  checkout, worktree, profile, release configuration, or GitHub repository.
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
- Pull-request titles and commits use Conventional Commits style. Every squash
  merge is released by semantic-release.
- Run the complete Python test suite, compileall, ShellCheck for shell changes,
  actionlint for workflow changes, and `git diff --check` before finishing.
- Deep-review reports are ignored local artifacts under `build/`; do not commit
  them.
- Keep this guide, the multi-repository skill, `README.md`, and
  `docs/ARCHITECTURE.md` synchronized with changes to the `atrinik` command,
  `components.json`, managed layout, ownership boundaries, or cross-repository
  development and release procedures.
