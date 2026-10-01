# Project delivery launcher

Replace the placeholders before submitting. This starts a bounded project; it
does not select scope or grant external mutations on its own.

```text
/goal Use $atrinik-project-delivery to deliver PARENT across REPOSITORIES.

Acceptance: ACCEPTANCE.
Authority: AUTHORIZED_TRACKING_AND_PUBLICATION_ACTIONS.
Stop before: MERGE_DEPLOYMENT_OR_OTHER_UNAUTHORIZED_ACTIONS.

Verify repository ownership, the live dependency/issue/PR graph, existing work,
and actual runtime capacity. Run ready disjoint lanes in parallel, with separate
limits for model workers and expensive builds. Give each writing worker one
owned native Linux worktree and exact paths; use docs/SOURCE_DELIVERY.md for new
source-only work. An isolated build container does not own delivery. Preserve
existing bound delivery, ledger, shared-runtime, and resource protocols without
migrating them.

For a new project, use a compact private milestone plan; do not initialize the
stateful project helper merely because the work spans multiple issues or a leaf
uses its own resource protocol. Resume existing helper state, or opt into it only
when its durable scheduler or tracking journal is specifically useful.

Manage and reuse workers directly. Continue implementation, tests, review,
commits, and authorized PR preparation through integrated validation and an
independent final review. Continue unaffected lanes when one is blocked. Do not
repeatedly poll unchanged state or ask me to relay worker messages.

At milestones, preserve a compact durable plan with goal and acceptance,
completed artifacts, valid revision-bound evidence, remaining dependencies,
exact ownership, authority limits, and the next runnable action. Do not claim
completion until every criterion has current evidence. If external approval or
access is all that remains, hand off the exact operation, owner, heads, evidence,
recovery coordinates, and retry event.
```
