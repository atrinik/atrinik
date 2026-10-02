# Repair local execution blockers

Authorization to complete a source or runtime task includes bounded repairs to
local tooling, dependencies, configuration and recoverable bookkeeping needed
for that task. An explicit request to repair a blocker also authorizes its
local repair. Do not ask again solely because earlier guidance says to stop,
preserve, or never edit an internal file. Those instructions protect evidence
and active ownership; they do not prohibit a diagnosed, reversible repair.

Prefer the existing inspect, retry, recovery, rebuild or release operation.
When it cannot run because its own metadata is broken, diagnose the exact
failure and repair the smallest affected coordinate. Prepare source changes in
an owned worktree and retain independent review. Continue unaffected work.

Before changing shared metadata, freshly verify the path, object type, user
ownership, relevant generation and current use. Obtain the owning workflow's
exclusive coordination lock and recheck the evidence under it. If another
process holds it, wait boundedly or continue other work; do not remove or
replace the lock, bypass a lease, or signal an unrelated process.

Preserve suspect bytes or links in a private, task-owned quarantine without
following links, and record the diagnosis and restoration source. Restore an
exact surviving valid record when available. Recreate only bookkeeping that
the normal wrapper can derive from authoritative current inputs; a new empty
registry is not a substitute for missing ownership or reference evidence.
Never fabricate authorization, lease ownership, delivery provenance, generation
proof or successful validation. If these cannot be established, stop the
affected mutation and report the specific missing evidence.

After repair, rerun the original public operation and its relevant checks.
Keep the preserved evidence until the repair is verified. Add a regression
when code caused the failure; test fixtures must isolate their Git, workspace,
lease, state and cache roots from real checkouts, including when temporary
directories are nested inside a repository.

This permission does not grant unrelated cleanup, adoption of another task's
worktree, changes to shared player data, credential changes, deployment, remote
publication or merge. Use the task's existing authorization for those actions.
Active runtimes and immutable published generations remain protected: repair
an owned stopped coordinate or publish a new generation through the wrapper,
rather than rewriting bytes used by a live process.
