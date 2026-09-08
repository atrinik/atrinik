---
name: atrinik-project-delivery
description: Coordinate Atrinik multi-issue projects with parallel workers, dependencies, recovery, and scoped GitHub tracking; retain issue delivery for individual leaves.
---

# Coordinate an Atrinik project

Own one authorized parent and its acceptance criteria through delivery and
integration. Launch and manage workers yourself; do not ask the user to relay
messages, launch chats, or exchange commits. Create a persistent goal only on
explicit request. Automatic skill selection is not write authority.

## Establish authority and readiness

Read `atrinik-multi-repo-workspace`, the live `atrinik-issue-delivery` entry,
coordinator and ledger requirements, and [the operator protocol](references/coordinator.md).
Verify the live parent, repositories, existing children/PRs, dependency graph,
acceptance and authorized tracking operations before initialization. Reconcile
native relationships and prose; neither alone proves completeness.

Use a live-proven canonical container or supported native Linux coordinator
per [the execution contract](../../../docs/LINUX_EXECUTION.md). Inventory all existing leaf
ledgers, containers and worktrees before claiming anything. Dirty candidates,
existing PRs and foreign sessions remain external, not new implementations.
The project record schedules work; it never grants worktree/ledger reuse.
Keep issue delivery and legacy program delivery unchanged as fallbacks; do not
migrate their state or reinterpret their `program` authority field.

## Run the ready graph

Use `python3 -m atrinik_workspace.project_delivery`, never hand-edit its state.
Inspect actual runtime worker capacity and existing open workers; a requested
16 slots is not proof. Reserve ready disjoint lanes before spawning. Record the
actual returned worker ID, exact entry mode/coordinate and attempt immediately.
If spawning fails or its outcome is uncertain, reconcile before retrying.

Each writing worker explicitly invokes `$atrinik-issue-delivery` for its one
selected issue or PR. It completes the existing authenticated genesis,
inventory, safe dedicated worktree binding and exact-coordinate gates before
edits/tests. Never share a writing worktree or credential/cache namespace.
An existing PR needs explicit PR-mode authority; do not adopt it as new issue
work. Reserve shared files/resources; subdivide only when ownership permits.
Use additional read-only workers for exploration, tests and independent review;
count them against actual capacity. Limit heavy builds separately.

Route findings to the owning worker, reuse it for fixes, and keep independent
ready lanes moving. Workers deliver through final-head validation and fresh
whole-diff review, not merely PR creation or one green run. Verify their live
leaf-ledger/report and remote head evidence before recording results; worker
prose alone is not acceptance. Integrate through owned PR branches, with ordinary
commits and refreshed base/head evidence; never force-push or bypass leaf gates.

## Maintain scoped tracking and finish

Routine local fixes, tests, reviews, commits, ordinary pushes and necessary
in-scope PR deliveries need no repeated approval when project delivery is
authorized. Coordinator tracking is separate from leaf permissions: use the
journal only for authorized assignment, owned progress comments, native links,
dependencies, bounded missing-child creation and existing Project Status.
Preserve human text; never force reparent, unassign others or duplicate issues.

After observed merges, refresh parents/dependants, invalidate stale integration
evidence, update tracking, and run newly unblocked lanes. Retain one canonical
closing PR per leaf; parent references are non-closing until all requirements
are fulfilled. Readiness is not merge authority. Stop for merge/deployment
approval, unavailable authentication, uncertain ownership or expanded scope;
continue unaffected work first. Never bypass policy or copy credentials.

Close a parent only with explicit closure authority, fresh complete graph/PR
observations and criterion-by-criterion terminal evidence. Otherwise hand off
the exact remaining closure action. Report delivered PRs and heads, acceptance,
remaining gates, external owners, recovery root and next runnable lanes. Do not
claim completion while only child counts or PR status look finished.
