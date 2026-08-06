# Atrinik workspace repository guide

- This repository owns multi-repository development orchestration, not
  component source. Never copy component implementation into it.
- Component checkouts, worktrees, builds, profiles, and mutable state belong
  below the ignored workspace directory or an explicit external path.
- Never replace a dirty checkout, remove a dirty worktree, or overwrite an
  existing mutable server-data directory.
- Use precise component and protocol names; do not use vague age-based labels.
- Do not mention confidential or unreleased Atrinik work in committed files or
  public project surfaces.
- Pull-request titles and commits use Conventional Commits style. Every squash
  merge is released by semantic-release.
- Run the complete Python test suite, compileall, ShellCheck for shell changes,
  actionlint for workflow changes, and `git diff --check` before finishing.
- Deep-review reports are ignored local artifacts under `build/`; do not commit
  them.
