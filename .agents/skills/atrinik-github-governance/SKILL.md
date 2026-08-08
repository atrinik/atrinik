---
name: atrinik-github-governance
description: Change Atrinik GitHub settings, rulesets, merge/release policy, Actions permissions, required checks, or governance automation.
---

# Atrinik GitHub Governance

Use this skill for changes to the standalone `github-settings` repository or
to an Atrinik Actions workflow whose permissions, check names, dependency
policy, or branch-protection contract may change.

## Review desired and live state

1. Read the workspace and `github-settings/AGENTS.md` guides.
2. Inspect the relevant `config/*.json`, publisher script, workflow, and recent
   repository history. Treat configuration as desired state and the GitHub API
   as live state; do not infer policy from one alone.
3. Use the publisher's default plan mode before considering `--apply`.
   Applying settings, changing repositories, or dispatching workflows requires
   explicit authorization because it mutates external organization state.
4. Preserve the current Team-plan compatibility contract. Do not introduce an
   Enterprise-only control without a documented, reviewed migration.

## Change policy coherently

- Keep required workflow job names synchronized with repository rulesets.
- Update the Actions allowlist, permissions, dependency pinning, and required
  aggregate status when a workflow changes those contracts.
- Keep merge methods, semantic-release behavior, Conventional Commit policy,
  repository visibility, and default-branch assumptions consistent.
- Keep Codecov in the selected-actions policy and its GitHub App repository
  access in manual desired state while coverage workflows use OIDC uploads.
- Use least-privilege workflow permissions and immutable third-party action
  references where project policy requires them.
- Never print tokens or copy live secrets into configuration, logs, fixtures,
  commits, or review text.

## Validate and hand off

Run JSON parsing, shell syntax, ShellCheck, actionlint for workflow changes,
the publisher's plan mode, and `git diff --check`. Review the complete plan for
unexpected deletions or relaxations before any authorized apply.

Document affected repositories, required check names, expected plan changes,
rollback or re-plan steps, and whether live state was intentionally left
unchanged. Use Conventional Commits for commits and pull-request titles.
