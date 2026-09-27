# Project operator protocol

Use this protocol for an authorized multi-issue project. It coordinates work; it
does not broaden product scope, repository ownership, external write authority,
merge authority, or deployment authority.

## Select the leaf contract

For new source-only work, the coordinator creates an ordinary owned Git worktree
and follows [`docs/SOURCE_DELIVERY.md`](../../../../docs/SOURCE_DELIVERY.md). A
new source leaf does not need a delivery ledger merely because it is part of a
project.

Keep the applicable existing protocol when a leaf is already bound to an issue
delivery, worktree, ledger, native/container environment, shared runtime, lease,
or other resource. Do not copy its patch into a fresh leaf, rewrite its state, or
use the simpler source path as a migration. A retained unchanged issue target
still uses `revalidate-current-targets-cas`; a project snapshot is not live
lease, actor, worktree, or resource proof. Historical container deliveries keep
their accepted image/mount and recovery gates. New work normally develops,
reviews, and runs lightweight checks in an owned native Linux worktree; pinned
CPU containers may run isolated builds without owning the delivery.

The source-repair exception permits narrowly necessary code, fixtures, tests,
commits, and PR preparation. It does not independently grant live runtime
mutation, deployment, merge, issue edits, or other external writes beyond the
user's existing authorization.

## Use lightweight coordination by default

Observe the parent and complete relevant issue/PR graph, acceptance criteria,
repositories, default/base heads, current worktrees, existing deliveries, shared
resources, and granted tracking/publication operations. Reconcile both native
relationships and prose. Mark foreign or uncertain work as external; do not
assign it to a new writer.

For a new project, author a compact private milestone plan from live Git and PR
evidence. It records the goal and acceptance, completed artifacts and the
revision/environment where evidence is valid, remaining dependencies, exact
repository/worktree/path ownership, authority limits, and the next runnable
action. Update it at meaningful milestones. Ordinary source coordination needs
no `project_delivery` initialization, coordinator probe, lock, snapshot, or CAS.
A project with several issues, repositories, or a leaf that needs its own live
resource protocol can still use this lightweight route; apply the stricter
protocol only to that leaf or resource operation.

Schedule ready nonconflicting lanes directly from the plan and actual runtime
capacity. Maintain the same ownership, dependency, blocker, review, evidence,
and authorization rules below without translating them into helper state.
Record each result and exact revision in the milestone plan. For a blocked lane,
record the concrete operation/resource/conflict, its relevance, and the event
that makes retry useful; keep unaffected lanes moving. Route findings to the
owner, reuse related workers, and refresh evidence after relevant drift.

## Resume or explicitly choose stateful coordination

Resume `python3 -m atrinik_workspace.project_delivery` when the project already
owns that state. A new project may opt in only when its durable CAS scheduler or
tracking journal provides concrete value; multi-issue scope alone is not a
reason. Once chosen, keep its checks for that project and do not silently migrate
between lightweight and stateful records.

For the stateful route, author a private plan from [the maintained
template](../assets/project-plan.json). For every node record:

- an exact type-explicit coordinate and repository;
- dependencies (`ready`, `merged`, or `accepted`; prefer `merged` unless an
  authorized stacked delivery requires another condition);
- normalized repository-relative reads and writes (`.` owns the repository);
- shared resources, heavy-job classification, and external ownership; and
- testable acceptance with explicit owner nodes.

Helper permissions are an explicit subset of the user's authority. Authorization
carries across the stated project scope; do not repeatedly ask for the same
ordinary in-scope action. A real external restriction, unavailable identity, or
scope expansion must be escalated rather than inferred away.

Initialize newly opted-in state only after the plan is live-verified:

```sh
python3 -m atrinik_workspace.project_delivery init --plan /absolute/plan.json --authority USER_SESSION_REFERENCE
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT inspect
```

The returned ignored directory is the durable stateful project root. The helper
owns its state; never hand-edit, delete, or adopt a lock-only/partial root. A
second `init` cannot take it over. Resume only the same authorized session or a
proven takeover after reconciling every worker and owned resource.

For compact repeated operations, write every full snapshot to a new file in an
owned mode-0700 directory:

```sh
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT --snapshot-output /absolute/private/step-1.json --compact inspect
```

Every mutation consumes the current generation and digest through `--expected`.
A stale CAS, lost output, or partial result means inspect and reconcile before
retrying; it never authorizes repetition. `plan` and `terminal` do not emit a
snapshot. Use `--help` for exact command argument order rather than copying a
stale recipe.

## Schedule stateful work

Observe the runtime's actual capacity domain and complete worker inventory.
Expose all ready disjoint lanes that fit it. Do not invent a small worker cap or
infer available capacity from a requested number. Bound CPU/GPU/build work
separately from model workers. File ownership, dependencies, shared resources,
and heavy-job limits remain hard scheduling conflicts.

The helper's scalar scheduler uses retained-handle capacity:

```sh
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT plan --capacity OBSERVED_LIMIT --open-workers OBSERVED_OPEN --heavy-limit HEAVY_LIMIT
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT dispatch --capacity OBSERVED_LIMIT --open-workers OBSERVED_OPEN --heavy-limit HEAVY_LIMIT --expected /absolute/private/current.json
```

Do not pass an active-slot count as the scalar retained-handle limit. If the
runtime exposes only active capacity, no fresh scalar dispatch is supported;
use an applicable source/issue fallback and record the scheduling gap. The
unchanged `dispatch` and `reserve-existing` producers return a legacy
issue-delivery sentence for compatibility. Treat either request as scheduling
metadata: when authoring the actual task packet, select the source contract for
a new source-only leaf and retain the issue-delivery instruction only for a leaf
to which it applies. Reusing a worker never changes that selected leaf contract.

Record each actual spawn promptly with `worker COORD --attempt ATTEMPT --id
ACTUAL_ID`. Include the returned attempt in the worker packet. An uncertain spawn
outcome keeps its reservation; observe before retrying or starting a replacement.

Give independent workers compact packets with the exact repository/worktree,
base and branch, owned paths, goal, acceptance, dependencies, relevant
instructions, authority limits, validation, and stop boundary. State that other
workers are active and their edits must be preserved. Prefer `fork_turns="none"`
when the packet contains all necessary context. Keep private ledger and recovery
material out of model messages.

Use the source contract's canonical watcher entry for CI or remote transitions;
do not reproduce its commands here. A model should wake for a decision, failure,
meaningful state change, or completion—not repeatedly poll unchanged state.

## Stateful results and recovery

Record `result COORD --attempt ATTEMPT --state ready|blocked --evidence TEXT`
only after checking the exact revision and result. Worker prose alone is not
acceptance. A blocker must name:

1. the concrete operation, resource, or ownership conflict;
2. why it is relevant to acceptance or a runnable dependency; and
3. the event or changed evidence that makes retry useful.

Continue unaffected lanes. Route review or test findings to the owner and reuse
that worker. Use `reopen` for a retained owner and a fresh attempt; old-attempt
messages cannot satisfy it. Use `retry` only after proving the old attempt never
started or stopped and the exact leaf is safe to resume. A timeout is not that
proof.

`reserve-existing` is a compatibility adapter for the same leaf's eligible
retained idle direct-child worker after `retry` or `replan`. It does not adopt a
worker or worktree from another leaf. Supply a fresh complete runtime observation
with the actual root namespace, capacity domain and limit, every retained agent,
the current project generation/digest/path, project authority, selected worker,
coordinate, mode, retired attempt, and leaf-correlation evidence. The observation
must be an owned no-follow file, at most 128 KiB, with a UTC timestamp no more
than 60 seconds old. Use `whole-thread-tree` for retained-handle capacity or
`whole-thread-tree-active` only when the runtime explicitly exposes an active
limit.

The observation schema is:

```json
{
  "schema_version": 1,
  "observed_at": "2026-09-12T12:00:00Z",
  "snapshot": {"generation": 7, "digest": "EXACT_SHA256", "path": "/absolute/project.json"},
  "project": {"parent": "atrinik/atrinik#1", "authority": "EXACT_AUTHORITY", "actor": "actor"},
  "runtime": {
    "namespace": "/root",
    "capacity_domain": "whole-thread-tree-active",
    "capacity": 17,
    "complete": true,
    "agents": [
      {"agent_name": "/root", "agent_status": "running"},
      {"agent_name": "/root/leaf", "agent_status": {"completed": "actual result"}}
    ]
  },
  "selection": {
    "worker": "/root/leaf",
    "coordinate": "atrinik/atrinik#2",
    "entry_mode": "issue",
    "retired_attempt": "EXACT_ATTEMPT",
    "evidence": "Fresh runtime and exact leaf ownership correlation."
  }
}
```

Use actual runtime rows without rewriting statuses. The root is present and
running; the selected direct child is exactly `"idle"` or has the tagged
completed object. Include running, nested, review, and retained workers in the
complete inventory. The helper rejects stale or mismatched fields but cannot
detect a fabricated omission, so obtain the evidence from the real runtime and
current leaf state.

```sh
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT reserve-existing COORD --worker /root/leaf --runtime-observation /absolute/private/runtime.json --heavy-limit HEAVY_LIMIT --expected /absolute/private/current.json
```

After reservation, recheck the real worker is idle, follow up that exact worker,
then bind its new attempt with `worker`. A busy, unknown, or lost follow-up keeps
the reservation and requires reconciliation. Existing bound writers still run
their own ledger, actor, worktree, lease, and live-resource checks.

Use `refresh` after graph or merge changes, `replan --plan FILE` for bounded
newly discovered in-scope work, and `attest CRITERION --evidence TEXT` for
current revision/environment-bound acceptance. Replan must not silently drop
known nodes, change issue/PR mode, expand repositories or permissions, or absorb
foreign work.

## Tracking and completion

Coordinator tracking is distinct from leaf implementation authority. In the
lightweight route, perform authorized tracking through the ordinary supported
API and record its live result in the milestone plan; no helper root or snapshot
is required. Limit either route to authorized assignment, actor-owned milestone
comments, native links and dependencies, deduplicated missing-child creation,
existing Project Status updates, and parent closure.

For the stateful route, journal those operations before applying them:

```sh
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT tracking plan --kind KIND --target COORD --payload /absolute/payload.json --expected /absolute/private/current.json
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT tracking apply --operation OPERATION_ID --expected /absolute/private/planned.json
```

Supported kinds remain `assign`, `comment`, `link`, `dependency`,
`create-child`, `project-status`, and `close-parent`. Preserve human text and
other assignees; do not force-reparent or duplicate work. Resolve live Project
IDs and current status options instead of borrowing IDs. After creating a child,
establish its relationship before dispatch; also `replan` helper state when
using the stateful route.

The journal writes planned -> in-flight -> bound. A lost response uses
`tracking reconcile`, which observes and never reposts. Cancel only an unstarted
planned operation after live non-application proof. An ambiguous or in-flight
write stops that target pending evidence or an external decision.

At meaningful milestones, persist the goal and acceptance, completed artifacts,
revision/environment-bound evidence, remaining dependencies, exact ownership,
authority limits, and next runnable action. This handoff should let a new
coordinator continue without user relay or an unnecessary session restart.

Before completion, refresh the whole graph and heads, run relevant integrated
checks, and obtain an independent final review. Preserve producer interfaces,
tests, and the plan asset schema unless compatibility is demonstrated. Every
criterion needs current evidence; relevant head, base, dependency, environment,
or graph drift invalidates it. In the stateful route, use `terminal` to enumerate
helper gaps, not as proof that live observations are fresh. The lightweight
route enumerates the same gaps directly in its milestone plan.

Closing references belong only on the canonical closing delivery. Merge,
deployment, and parent closure each require their own granted authority. If an
external restriction is the last gate, report the exact operation, owner, heads,
evidence, recovery coordinates, and retry event. Never claim completion from
child counts, PR status, or a worker summary alone.
