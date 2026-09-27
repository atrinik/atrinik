# Source delivery

Use this workflow for new code, tests, documentation and configuration changes.
An issue number is optional when the user has supplied a concrete goal. Resolve
the physical owner in `components.json` and keep each repository's changes in
its own dedicated worktree. This workflow grants no merge, deployment, cleanup,
credential change or shared-resource adoption authority.

## Choose the path by the operation

| Operation | Entry and evidence |
| --- | --- |
| New source change with isolated local tests | Owned Git worktree, recorded base, scoped collision check, validation and independent review |
| Repair of broken delivery tooling | The same source path; fixture tests cannot authorize live resources |
| Resume a source worktree created by the source helper | Match its creation receipt and current Git identity; explicitly confirm ownership of an interrupted dirty diff |
| Resume an existing bound delivery or operate on its resources | The existing [preparation and recovery protocol](../.agents/skills/atrinik-issue-delivery/references/preparation.md) |
| Build publication, shared runtime/state, export or cleanup | The owner-specific wrapper command and its live identity, plan, lease and authorization checks |

Existing ledgers and project scheduler state retain their contracts. Do not
convert them into source receipts, duplicate an already owned implementation,
hide journals or manually rewrite evidence. A source task can prepare changes
to these mechanisms without activating them against existing live state.

## Start and resume

Inspect the selected repository, worktrees, local branches and relevant live
PRs before assigning writers. Check the requested issue/goal and overlapping
paths, not every unrelated historical delivery. Use one writer per worktree;
reserve overlapping files in the coordinator's task assignments before launch.
An existing branch/PR is a collision to inspect, not permission to overwrite it.
Network unavailability need not stop isolated drafting, but remote collision,
actor and repository checks must succeed before publication.

Record a verified accepted base commit. Run the helper from the wrapper checkout
with a new branch and an absent destination outside managed `workspace/` and
`build/` trees. Values below are placeholders for the selected physical owner:

```sh
python3 scripts/source_delivery.py start \
  --repository /absolute/owner-repository \
  --worktree /absolute/task-worktrees/change \
  --branch fix/change --base ACCEPTED_COMMIT --owner TASK_OWNER
```

The helper performs local Git/path checks, creates the worktree and prints exact
coordinates in JSON. Its small Git-local creation receipt identifies its own
worktrees; it is not a delivery ledger, runtime permission or substitute for
fresh checks. Use the returned worktree for edits, tests, commits and review.
No coordinator probe, authenticated ledger genesis or global journal inventory
is required merely to edit source or run isolated fixtures.

```sh
python3 scripts/source_delivery.py resume \
  --worktree /absolute/task-worktrees/change \
  --branch fix/change --base ACCEPTED_COMMIT --owner TASK_OWNER
```

On interruption, verify the same task assignment, branch, base, worktree and
current diff. If the preserved dirty changes are known to belong to this task,
repeat with `--allow-dirty`. That flag is an explicit ownership assertion, not
proof that unknown edits belong to the caller. Unknown ownership, conflicts or
unexpected changes require inspection; never reset, clean or overwrite them.
Worktrees without a matching source receipt are not adopted by this helper.
Use their existing workflow or prepare a separate noncompeting repair after
resolving ownership. A failed create preserves partial resources for inspection.

## Repair without circular admission

Authorization to fix delivery tooling includes preparing the bounded repair in
a fresh source worktree when the broken admission helper cannot run. State the
failed operation and the repair scope, preserve existing delivery evidence,
and reproduce the defect with isolated fixtures. Do not require that same
broken helper to admit source edits, fixture tests, commits or the repair PR.

Independent review must cover the changed trust boundary and negative cases.
Candidate helpers may run against test-owned fixtures; they must not produce
authority for the existing live delivery. After accepted code is merged, refresh
it and resume that delivery through its original public helper. A fix to a
validator does not itself prove that any specific live resource is safe.

## Validate and publish

Use the owner's contributor checks and the applicable
[review checklist](../.agents/skills/atrinik-issue-delivery/references/deep-review-checklist.md).
Run focused checks while implementing, then the required integrated checks.
Keep builds and mutable caches isolated; pinned application build workers follow
[Linux execution](LINUX_EXECUTION.md). Lightweight wrapper/fixture tests can run
locally. A runtime-dependent acceptance criterion still needs actual runtime
evidence even when the source implementation is ready for review.

Preserve independent review of the complete integrated diff. Classify findings
as blocking defects or optional suggestions; fix and revalidate affected behavior.
Retain evidence only while its revision, relevant dependencies and environment
remain valid. Record exactly what a base/head change invalidates, and review the
final integrated revision. Missing acceptance is not a pass.

Before push/PR mutation, verify the actual GitHub actor, repository permissions,
remote destination, owned branch, current PR head and task authorization. Follow
[PR publication](../.agents/skills/atrinik-github-governance/SKILL.md#publish-pull-requests).
Do not overwrite another writer's branch or contributor text. Commit owned paths,
use ordinary non-forcing pushes, and create/update the authorized PR. A reviewable
draft may be opened while external acceptance remains blocked; say what is missing.
Mark it ready only after applicable required checks and final review pass.

Watch checks in one bounded process, supplying the actual expected acceptance
contexts from the owner's workflows and repository rules:

```sh
python3 scripts/wait_pr_checks.py PR --repo OWNER/REPOSITORY \
  --expected-head HEAD_SHA --expect 'Integration validation'
```

Add `--expect` for every required context; the example is not a universal check
list. The watcher reports changed snapshots, failure, drift, timeout or success.
It neither dispatches workflows nor waives missing checks. Changes to the PR's
base/head require reassessing affected integration evidence before a new watch.

## Blockers and handoff

A useful blocker records the operation, exact affected resource, observed
conflict, why it blocks this task, allowed alternative and event needed to retry.
Uncertainty about the selected target stops that operation; an unrelated historical
record is not a global source-work gate without a demonstrated dependency.
Continue independent work and prepare reviewable artifacts when allowed.

Preserve granted authorization by scope across turns. Ask only for a new material
action or an actual external restriction. Distinguish repository procedure from
tool/sandbox enforcement. On an unchanged blocker, retain the diagnosis and use
a supported watcher or compact blocked handoff; do not repeat full audits unless
relevant state changed. Required harness behavior still takes precedence.

At a coherent milestone retain the goal/acceptance, completed commits/PRs,
remaining dependencies, writer/path ownership, authorization and restrictions,
valid evidence, blockers and next runnable action. Keep private coordination
outside committed source and never publish credentials or raw chats. Refresh
context from this packet when needed; do not require the user to relay worker
messages or repeat unchanged authorization. Optional timing metrics should separate
implementation, infrastructure repair and external waiting; they never gate work.
