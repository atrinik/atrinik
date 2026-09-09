---
name: atrinik-issue-delivery
description: Deliver an Atrinik issue/PR to merge-ready handoff or manage its explicitly authorized post-merge ledger lifecycle. Explicit `$atrinik-issue-delivery` invocation only.
---

# Deliver an Atrinik issue or pull request

Choose exactly one type-explicit `ENTRY_MODE`: new `issue` or existing `PR`.
Both stop before merge.

Invocation permits ordinary pushes, selected/delivery-created PR updates,
gated readiness, one coordinate-bound selected-PR comment, and issue-mode draft
creation/claim. PR mode claims only an explicit verified issue; issue comments
are read-only. It does not authorize force-pushes, issue
create/close, merges, bypass, destructive resets, cleanup application,
self-approval, retargeting, unrelated changes, or PR-discovered issue mutation.
Create/resume a persistent goal only when explicitly asked.

Explicit `$atrinik-program-delivery` delegates only issue mode to ready live
children, never PR mode/unrelated PRs; do not create a nested leaf goal.

Load `atrinik-multi-repo-workspace`, `atrinik-github-governance`, applicable
implementation, and runtime/scenario skills only when needed. Follow the nearest
owner `AGENTS.md`; do not duplicate procedures.

## Prepare or recover the exact delivery

On first entry or recovery, read [preparation](references/preparation.md) and
perform its type-explicit selection, authenticated ownership, complete collision
inventory, canonical coordinator and dedicated worktree/ledger binding gates.
Read it again on coordinate or authority drift. Keep the helper's live operation
gates; a saved packet never grants authority or replaces CAS/inventory checks.

During an uninterrupted session, retain exact container/worktree/ledger handles
and helper-returned snapshots in private files. Run deterministic checks and
extract results in scripts; return compact success/failure and actionable changes
to the model. Avoid separate model turns for unchanged Docker inspection or
manual JSON reconstruction. Reconnect or uncertain ownership reruns the full
probe, worktree, inventory/CAS and lease checks. Never cache authentication or
ownership as a substitute for a required live helper check.

## Implement and publish or update PRs

Implement owner requirements/tests. Supply-chain diagnostics never gate work
or require inventory updates. Commit/validate coherent
Conventional checkpoints without rewriting published history. Reprove `origin`,
then run `git push origin HEAD_BRANCH` in the same scrubbed selector environment;
reject HTTP, conceal URLs/credentials, and retain credential helpers. In issue
mode, open one coherent draft per affected physical repository against
`TARGET_BRANCH` (for example, `--base TARGET_BRANCH`) and verify each base; keep
one canonical issue-closing path. In PR mode, update only the selected PR
without changing base, head, draft, or valid linkage; Never convert an
already-ready PR to draft; leave an already-ready PR ready and do not broaden
its closing references.

Use `type(optional-scope): concise description` by default; add `!` only when a
reviewer explicitly requests a breaking change. PR bodies must be substantive
rendered GitHub-Flavored Markdown with actual line breaks, never literal `\n`
separators. Include `Summary`, `Implementation / behavior`, `Validation`, and
applicable `Limitations / follow-up`; an issue-closing line alone is
insufficient. preserve contributor-authored text byte-for-byte and change only
a separately delivery-owned section when authorized. Feed multi-section bodies
by file/stdin. After creating or editing a pull request, inspect GitHub's
rendered `bodyHTML`/`body_html`, not raw body; verify headings, lists, inline
code, issue-closing references, and validation sections. Follow the ledger
reference's coordinate-bound remote-write protocol; ledger-retain each exact
initial PR/body/comment payload. A fresh contributor body is wholly read-only
outside the helper-planned terminal delivery section; copied live markers grant
no ownership. Refetch and verify rendered GFM/linkage after helper-bound updates.

Refetch bytes/timestamps and every marker; CAS intent, refetch, use only the
helper payload, and bind its exact result. For an issue-mode initial PR bind,
the helper's complete comment pagination classifies ordinary external comments
as non-delivery state without recording or deleting them; reserved delivery
comment markers and incomplete pagination stop the bind. Cancel only after
exact non-application proof and before drift. Zero comment matches permit one
never-started post; reuse one exact actor-authored match. Wrong-author,
malformed, duplicate, unexpected, or uncertain state stops. Verify rendered
GFM/linkage.

## Review and fix to the exit condition

Keep ledger/report current; never commit/publish either or include credentials,
confidential or excess vulnerability detail. Process/tooling reporting is optional;
read [the diagnostic protocol](references/tooling-issues.md) only when recording
a useful observation. Missing, malformed or contended diagnostics never block delivery.

Read [checklist](references/deep-review-checklist.md) in full. Review the
complete current base-to-head diff against issue/PR requirements. For non-trivial
changes, give independent fresh-context reviewers the raw requirements and diff,
not prior conclusions. Record stable finding IDs, evidence, resolution, status,
fixing commit, and validation.

Fix every actionable finding, add useful tests, update the report, commit/push a
checkpoint, rerun validation, then conduct a fresh whole-diff review. Repeat
until a complete post-fix pass finds zero known actionable findings and none has
reopened. Evidence out-of-scope deferrals; keep GitHub feedback concise.

## Verify with compatible resources

For provisioning or interactive runtime verification, read
[runtime verification](references/runtime-verification.md). Runtime-irrelevant
changes use the owner's required tests and report runtime as inapplicable.

## Finish only on final HEAD

Refetch every selected or delivery-created PR and exact target/head refs. Recheck
the complete diff, commits, mergeability, draft, repositories, branches, SHAs,
linkage, review threads/comments, and expected checks; recompute every merge
base. Any target/base/head or merge-base drift invalidates the affected review,
validation, and checks and restarts the shared convergence loop at the new
recorded coordinates.

Wait for all expected pre-readiness checks; required and applicable optional
checks must pass. Explain skipped/neutral checks and block on missing, failed,
or cancelled checks. Only after stable final coordinates, final validation, a
zero-finding review, all such checks pass, and live mergeability is determinate
and conflict-free with no non-human blocker other than draft state may a draft
be marked ready. Unknown or conflicting mergeability blocks. Requery after
ready, recheck mergeability, and wait for checks it triggers. Report required
human approval as a blocker, not literal merge eligibility; new actionable
feedback restarts the fix/validation/review loop. Missing human approval blocks
merging, not the ready transition.

Hand off PR URLs, exact per-target bases, repositories, branches, SHAs, merge
bases, commits, worktrees, findings, validation/checks, mergeability, runtime
applicability, resources, verification/repeat/shutdown/cleanup commands, and
blockers or `none`. Include issue URLs, claim state, and canonical closing paths
only when issues actually exist; do not fabricate placeholders.

Keep issues open, PRs unmerged, and evidence preserved. A separate post-merge
request must follow the ledger reference's terminal lifecycle; delivery grants
no cleanup authority and helper lifecycle commands never remove resources.
It must never trigger implicitly.

For unchanged bound targets on reconnect, use the public
`revalidate-current-targets-cas` exact-tuple proof in the issue-delivery
ledger protocol; saved safety fields or generic CAS are not live lease proof.
