---
name: atrinik-project-delivery
description: Coordinate an authorized Atrinik multi-issue project across repositories with parallel owned worktrees, dependencies, review, and acceptance. Use issue delivery instead for a single existing issue or PR.
---

# Coordinate an Atrinik project

Own the authorized project through verified acceptance. Launch, direct, reuse,
and review workers yourself; do not ask the user to relay messages or restart a
session merely to cross a milestone. Create a persistent goal only when the user
explicitly requests one. Skill selection is not write authority.

Read [the operator protocol](references/coordinator.md). Use
`atrinik-multi-repo-workspace` when wrapper topology, profiles, or managed
worktrees change. Use the canonical [source delivery contract](../../../docs/SOURCE_DELIVERY.md)
for each new source-only leaf. Invoke `atrinik-issue-delivery` only for an
existing bound issue/PR delivery or an operation that needs its ledger, shared
runtime, or resource protocol; never silently migrate existing state.
New source work normally uses an owned native Linux worktree; an isolated build
container may run toolchain checks without owning the delivery.

## Plan and delegate

Verify the live parent, acceptance, repository ownership, issue/PR graph,
dependencies, existing work, and granted external operations. Exact repository
ownership belongs in the plan and every task packet. Existing dirty worktrees,
bound deliveries, and foreign sessions remain external until their owner hands
them off through an applicable supported protocol.

Use `python3 -m atrinik_workspace.project_delivery` for durable project state;
do not hand-edit it. The project record schedules work but grants neither Git
worktree reuse nor external authority. See the protocol for command and recovery
details.

Expose all ready, disjoint work up to actual runtime capacity. Keep model-worker
capacity separate from CPU/GPU/build limits, dependencies, file ownership, and
shared-resource conflicts. Prefer:

- GPT-6 Astra for architecture, ambiguity, security/concurrency decisions, and
  independent integrated review;
- GPT-5.6 Sol for ordinary implementation;
- Terra for narrow tests and mechanical refactors; and
- Luna for bounded extraction or inventory.

Use an available sufficiently capable model if a preference is unavailable,
and escalate when uncertainty or failed approaches require stronger reasoning.
Reuse a related worker for follow-up instead of growing unrelated history.

Each writing worker owns one repository worktree and a disjoint path or branch.
Tell it other workers are active and that it must preserve their changes. Send a
compact task packet with the goal, acceptance, exact repository/base/branch and
owned paths, dependencies, authority limits, relevant instructions, validation,
and stop boundary. Keep credentials and private recovery data out of packets.
Workers may create narrowly required code, fixtures, tests, commits, and PR
preparation to repair an in-scope source failure; that exception never
self-authorizes live runtime, deployment, merge, or external mutation.

## Converge and hand off

Keep unaffected lanes moving. A blocker report must identify the concrete
operation, resource or conflict, explain why it matters, and name the event that
makes retry useful. Do not repeatedly poll unchanged state; use a watcher or a
pending tool call where available.

Treat worker output as provisional. Route findings to the owning worker, then
run integrated validation and an independent final review. Preserve project
producer interfaces, tests, and plan schema unless compatibility evidence proves
a removal safe. Readiness is not merge or deployment authority.

At every milestone, leave a compact durable plan containing:

- goal and acceptance;
- completed artifacts and valid evidence, including revision/environment;
- remaining dependencies and exact ownership;
- authority limits; and
- the next runnable action.

Refresh evidence after relevant base, head, dependency, environment, or graph
changes. Close the parent only with explicit closure authority and current
criterion-by-criterion terminal evidence. Otherwise report exact remaining
actions, owners, recovery coordinates, and runnable lanes without claiming the
project complete.

Existing bound deliveries retain their accepted reconnect and resource gates.
For an unchanged retained issue target, use the issue-delivery ledger's public
`revalidate-current-targets-cas` proof; project scheduling does not replace live
lease, worktree, actor, or resource checks.
