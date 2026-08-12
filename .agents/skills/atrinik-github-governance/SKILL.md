---
name: atrinik-github-governance
description: Publish Atrinik PRs, govern GitHub policy, or review and explicitly merge native PR stacks.
---

# Atrinik GitHub Governance

## Route and review the task

1. For native PR-stack review or an explicitly authorized merge, read and
   follow [the stack contract](references/pr-stack-review-and-merge.md) in
   full. Review-only work is read-only; never infer merge authority.
2. For PR-only work, read its repository's `AGENTS.md`, use **Publish pull
   requests**, and skip steps 3–6.
3. For policy, settings, Actions, checks, or release governance, read the
   workspace and `github-settings/AGENTS.md` guides.
4. Compare relevant configuration, publisher, workflows, and history with the
   live GitHub API; inspect desired and live state.
5. Run the publisher plan before `--apply`; external changes and workflow
   dispatch need explicit authorization.
6. Keep controls Team-plan compatible; document and review Enterprise-only
   migrations.

## Change policy coherently

- Synchronize required workflow job names with repository rulesets.
- When workflow contracts change, update the Actions allowlist, permissions,
  dependency pins, and required aggregate status.
- Keep merge methods, semantic-release, Conventional Commit policy, visibility,
  and default branches consistent.
- Keep Codecov in the selected-actions policy and its GitHub App access in
  manual desired state while coverage uses OIDC.
- Use least-privilege permissions and policy-required immutable action refs.
- Never print tokens or copy live secrets into configuration, logs, fixtures,
  commits, or review text.

## Publish pull requests

Use `type(optional-scope)!: concise description` for PR titles. Write bodies as
renderable GitHub-Flavored Markdown with actual line breaks, never visible
literal `\n` separators. Pass multi-section bodies by file, standard input, or
another newline-preserving method. After create/edit, inspect GitHub's rendered
web view or rendered `bodyHTML`/`body_html`, not only the raw body. Verify
headings, lists, inline code, issue-closing references, and validation sections
render normally.

## Validate and hand off

For policy changes, run JSON/shell validation, ShellCheck, workflow actionlint,
publisher plan mode, and `git diff --check`; inspect the plan for unexpected
deletions or relaxations before apply. For PR-only work, run the owning
repository's validation and the rendered-view check.

For policy, document affected repositories, required check names, plan/rollback
steps, and unchanged live state. For PRs, record remote render verification. Use
Conventional Commits for commits.
