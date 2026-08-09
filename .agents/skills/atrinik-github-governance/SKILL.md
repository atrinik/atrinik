---
name: atrinik-github-governance
description: Manage Atrinik PR publication, GitHub policy, Actions, checks, settings, or releases.
---

# Atrinik GitHub Governance

## Review desired and live state

1. Read the workspace and `github-settings/AGENTS.md` guides.
2. Inspect relevant configuration, publisher code, workflows, and history.
   Treat configuration as desired state and the GitHub API as live state.
3. Run the publisher's plan before `--apply`. External settings changes and
   workflow dispatch require explicit authorization.
4. Keep controls Team-plan compatible; document and review any migration to an
   Enterprise-only feature.

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

## Publish pull requests

Use `type(optional-scope)!: concise description` for PR titles. Write bodies as
renderable GitHub-Flavored Markdown with actual line breaks, never visible
literal `\n` separators. Pass multi-section bodies by file, standard input, or
another newline-preserving method. After create/edit, inspect the remote PR and
verify headings, lists, inline code, issue-closing references, and validation
sections render normally.

## Validate and hand off

Run JSON parsing, shell syntax, ShellCheck, actionlint for workflow changes,
the publisher's plan mode, and `git diff --check`. Review the complete plan for
unexpected deletions or relaxations before any authorized apply.

Document affected repositories, required checks, expected plan changes,
rollback or re-plan steps, and intentionally unchanged live state. Use
Conventional Commits for commits.
