---
name: atrinik-github-governance
description: Publish Atrinik PRs, govern GitHub policy, or review and explicitly merge native PR stacks.
---

# Atrinik GitHub Governance

## Route and review the task

1. For native PR stack review or an explicitly authorized merge, read and
   follow [the stack contract](references/pr-stack-review-and-merge.md) in full;
   Review-only work is read-only and never grants merge authority.
2. For PR-only work, read its repository's `AGENTS.md`, use **Publish pull
   requests**, and skip steps 3–6.
3. For policy, settings, Actions, checks, or release governance, read the
   workspace and `github-settings/AGENTS.md` guides. Compare configuration,
   publisher, workflows, and history with the live GitHub API.
4. Run the publisher plan before `--apply`; external changes and workflow
   dispatch need explicit authorization.
5. Keep controls Team-plan compatible; document and review Enterprise-only
   migrations.

## Optional commit-signing guidance

For optional SSH commit-signing setup and the host/container boundary, read
[the SSH signing reference](references/ssh-signing.md). It keeps personal
keys and signing configuration out of the repository and does not change
repository commit-signing policy.

## Change policy coherently

- Synchronize required workflow jobs with repository rulesets; when workflows
  change, update the Actions allowlist, permissions, dependency pins, and
  required aggregate status.
- Keep merge methods, semantic-release, Conventional Commit policy, visibility,
  default branches, and Codecov/OIDC policy consistent.
- Use least-privilege permissions and policy-required immutable action refs.
- Never print tokens or copy live secrets into configuration, logs, fixtures,
  commits, or review text.

## Publish pull requests

Titles use `type(optional-scope): concise description` by default; add `!` only
when a reviewer explicitly requests a breaking change, not automatically.
PR bodies must be substantive rendered GitHub-Flavored Markdown with actual
line breaks, never literal `\n` separators. Include `Summary`,
`Implementation / behavior`, `Validation`, and applicable
`Limitations / follow-up`; an issue-closing line alone is insufficient.
preserve contributor-authored text byte-for-byte, changing only a separately
delivery-owned section when authorized. Feed multi-section bodies by file/stdin.
After create/edit, inspect GitHub's rendered `bodyHTML`/`body_html`, not raw
body; verify headings, lists, inline code, issue-closing references, and
validation sections.

## Validate and hand off

For policy changes, run JSON/shell validation, ShellCheck, actionlint, publisher
plan mode, and `git diff --check`; inspect the plan before apply. For PR-only
work, run repository validation and the rendered-view check.

For policy, document affected repositories, required checks, plan/rollback
steps, and unchanged live state. For PRs, record remote render verification.
Use Conventional Commits.
