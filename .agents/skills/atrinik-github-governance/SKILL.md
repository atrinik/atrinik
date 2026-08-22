---
name: atrinik-github-governance
description: Publish Atrinik PRs, govern GitHub policy, or review and explicitly merge native PR stacks.
---

# Atrinik GitHub Governance

## Route and review the task

1. For native PR stack review or an explicitly authorized merge, read and
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

Titles use `type(optional-scope): concise description` by default. Add `!` only
when a reviewer explicitly requests a breaking change; not automatically.
PR bodies must be substantive, rendered GitHub-Flavored Markdown with actual
line breaks, never literal `\n` separators. The minimum body explains what changed
and why in a concise `Summary`, gives relevant `Implementation / behavior`
details, records `Validation` and its results, and states
`Limitations / follow-up` when applicable. Issue and pull-request reference
syntax remains supported, but an issue-closing line alone is insufficient. Feed
multi-section bodies by file/stdin. When an agent updates an existing pull
request, preserve contributor-authored text byte-for-byte and add or replace
only a separately
delivery-owned section when the delivery ownership rules authorize it. After
create/edit, inspect GitHub's rendered `bodyHTML`/`body_html`, not raw body;
verify headings, lists, inline code, issue-closing references, and validation
sections.

## Validate and hand off

For policy changes, run JSON/shell validation, ShellCheck, workflow actionlint,
publisher plan mode, and `git diff --check`; inspect the plan for unexpected
deletions or relaxations before apply. For PR-only work, run the owning
repository's validation and the rendered-view check.

For policy, document affected repositories, required check names, plan/rollback
steps, and unchanged live state. For PRs, record remote render verification. Use
Conventional Commits for commits.
