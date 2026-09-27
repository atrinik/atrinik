---
name: atrinik-issue-delivery
description: Deliver an explicitly selected Atrinik issue or PR to a reviewed, validated handoff before merge; resume retained bound deliveries through their existing protocol. Explicit invocation only.
---

# Deliver an Atrinik issue or pull request

Select an unambiguous `issue` or `PR` coordinate. Both modes stop before merge.
Invocation authorizes ordinary pushes, draft PR creation for issue mode,
selected/delivery-created PR updates, readiness after validation, one selected-PR
delivery comment, and claiming an explicitly selected issue. Keep issue comments
read-only. PR mode keeps
incidental linked issues read-only and preserves existing closing scope. Do not
create placeholder issues, force-push, retarget, self-approve, bypass checks,
close issues, merge, destructively reset, or apply cleanup. Create a persistent
goal only when explicitly asked. Program delegation remains issue-mode-only.

## Choose the route

For new source-only work, follow [source delivery](../../../docs/SOURCE_DELIVERY.md).
It is the canonical start/resume procedure: resolve the physical owner and task
coordinate, check local branch/worktree collisions, then use an ordinary owned
Git worktree. Verify live GitHub identity and push authority before publication,
not before local drafting. This route requires no
schema ledger, global inventory, managed scope, runtime, or unrelated resource
admission. Follow its dirty-resume and blocker rules at the affected operation.

For an exact existing bound ledger/delivery or retained managed resources, read
[retained preparation](references/preparation.md) and the relevant parts of
[the ledger protocol](references/delivery-ledger.md). Keep helper live checks,
inventory, ownership, CAS, leases and recovery evidence at each protected use;
never silently migrate bound work. For a proven false resource observation, use
[its specialized recovery](references/resource-observation-recovery.md).
Isolated tooling repair may prepare and test a PR using the source route, but
candidate code grants no live authority or retained-resource admission.

Read the physical owner's `AGENTS.md`. Load `atrinik-github-governance` for
publication and `atrinik-multi-repo-workspace` or implementation specialists only
when the selected work needs their contracts.

## Implement, review and publish

Implement the selected acceptance criteria, run relevant owner-required tests,
and commit coherent Conventional checkpoints. Preserve existing user work and
published history. Before a push, reprove the intended repository and permitted
origin route without exposing credentials. Each physical repository has its own
branch, worktree, validation and PR; PR mode does not authorize companion PRs.

Use [the focused review checklist](references/deep-review-checklist.md).
Require an independent final integrated review and resolve actionable defects;
optional suggestions do not block. Reuse evidence only while its revision,
environment and relevant inputs remain valid. Changes to helper/lifecycle
contracts additionally use [specialist certification](references/helper-lifecycle-review.md).

Create one coherent issue-mode draft per affected repository against the verified
target; retain one canonical issue-closing path. PR mode updates only the selected
PR, preserving its base/head, valid linkage and existing ready state. Follow
GitHub governance for substantive rendered bodies and contributor-owned text;
verify the rendered result after remote edits. Bound deliveries additionally use
their retained exact-payload remote-write protocol; never hand-roll ledger state.

For runtime work, read [runtime verification](references/runtime-verification.md).
Runtime-irrelevant work uses relevant tests and records runtime as inapplicable.
Process/tooling reports and supply-chain inventories remain optional diagnostics;
[tooling notes](references/tooling-issues.md) apply only when recording useful
observations. Their absence or contention does not block delivery.

## Finish on the verified revision

Refetch selected PRs and target/head refs; verify the reviewed base/head/merge-base
coordinates, current feedback, expected checks and determinate conflict-free
mergeability. Assess drift and refresh affected evidence. Missing, failed,
cancelled or timed-out expected checks are not success; explain skipped/neutral
results. Wait through the bounded check-watcher procedure in source delivery.
Mark a draft ready only after final validation, independent review and expected
pre-readiness checks pass, then verify checks triggered by readiness. Required
human approval blocks merging, not the ready transition.

Hand off PR URLs, exact reviewed coordinates, validation/review results and
remaining blockers. Include resource and runtime commands only when applicable.
Leave issues open, PRs unmerged and owned work preserved. A separately authorized
post-merge retained lifecycle follows the ledger protocol and never runs implicitly.
