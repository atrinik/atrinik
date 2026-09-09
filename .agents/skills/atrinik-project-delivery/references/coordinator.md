# Project operator protocol

## Authority, state and recovery

The coordinator owns scheduling and authorized project tracking, not leaf
worktrees. Read the existing leaf ledger requirements before delegation. Every
writing worker remains a complete type-explicit issue/PR delivery, with its own
safe worktree, ledger and supported environment. The parent session supplies
the user's scoped invocation; this is not a legacy program delegation and
does not mutate leaf `program` fields. Leaf issue-mutation restrictions stay
intact: send tracking requests to the coordinator.

Use the installed agent runtime spawn/send/wait/close tools. Discover their
actual availability and capacity, include completed-but-open and review workers,
and close only your finished workers. If no reliable limit is exposed, probe
capacity incrementally with productive read-only tasks; do not reserve 16 on
configuration alone. No available spawn tool means serial coordination with
an explicit limitation, not imaginary parallel delivery.

Inspect wrapper status, manifests, nearest guides, complete review-ledger
inventory, foreign containers/active worktrees, live issue/PR graph and exact
candidate diff/head identities. Record private recovery paths for dirty work;
never copy it into a new implementation without supported explicit adoption.
Do not add a live session as a writable node. Mark it `external:true`, with
its actual file/resource reservations, until externally completed. Separate
filesystems require the existing supported handoff; copying state is not it.

The CLI derives an ignored project directory from the canonical wrapper and
parent. Keep its returned path, actor, session authority and snapshot tuple in
the private handoff. A second `init` cannot adopt it. Resume only the same
authorized session or an explicit takeover after proving the old owner stopped,
rerunning the live probe/ledger/worktree/lease checks and reconciling every
recorded worker with the runtime. There is no automated takeover command.
Never delete a lock-only/partial root or hand-edit a record to make it reusable.

## Commands and plan

Author a private plan using [the template](../assets/project-plan.json).
Replace example coordinates with verified live values. List every physical
repository, exact type-explicit node, normalized repository-relative reads and
writes (`.` reserves the whole repository), shared resource names, dependencies,
heavy-job flag and external ownership. Dependency conditions mean merge-ready
(`ready`), actually merged/completed (`merged`), or accepted integration
(`accepted`); default to `merged` when publishing a stack is not explicitly
part of scope. Acceptance lists real testable outcomes and their owner nodes.
Permissions are an explicit subset, never inferred from issue prose.

Run from the supported coordinator with Git/GitHub outside the sandbox:

```sh
python3 -m atrinik_workspace.project_delivery init --plan /absolute/plan.json --authority USER_SESSION_REFERENCE
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT inspect
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT plan --capacity OBSERVED_LIMIT --open-workers OBSERVED_OPEN --heavy-limit 1
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT dispatch --capacity OBSERVED_LIMIT --open-workers OBSERVED_OPEN --heavy-limit 1 --expected /absolute/snapshot.json
```

For repeated operations, save exact snapshots directly and return compact output:

```sh
# Use a private, owned 0700 directory and a NEW absolute file for each result.
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT --snapshot-output /absolute/private/step-1.json --compact inspect
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT --snapshot-output /absolute/private/step-2.json --compact dispatch --capacity OBSERVED_LIMIT --open-workers OBSERVED_OPEN --heavy-limit HEAVY_LIMIT --expected /absolute/private/step-1.json
```

The helper retains complete snapshot bytes in the private file and emits CAS
metadata plus the actionable result. Tracking output includes only operation
`id`, `kind`, `target` and `phase`; read the operation in the exported snapshot
when assessing exact payloads or remote evidence. Compact output alone is never
proof of retry safety or acceptance. It never overwrites an existing file or
replaces live ownership checks. Keep full output when needed for diagnosis.
`plan` and `terminal` do not return snapshots; call them without these flags.
If a command fails or a CAS is stale, inspect into a fresh file and reconcile
before retrying; a partial output is never authority to repeat a mutation.
Default output remains unchanged: save `inspect` stdout directly as the bounded
expected-snapshot input; mutations return `{snapshot,result}` and `init`
returns `{root,snapshot}`. If using default output,
Extract `.snapshot` without editing the authoritative file. Every mutation
requires the current generation/digest/device/inode tuple together. Stale CAS
means inspect and reconsider, not overwrite. Commands below share `--root` and
`--expected`; use `--help` for exact argument order:

- `worker COORD --attempt ATTEMPT --id ACTUAL_ID`: after actual spawn.
- `result COORD --attempt ATTEMPT --state ready|blocked --evidence TEXT`: after
  inspecting the exact leaf-ledger/head/report and worker result. Retain actual
  worker identity for follow-up; no merge/accept claim from a worker.
- `retry COORD --attempt ATTEMPT --evidence TEXT`: only after runtime proof of
  non-start/stopped attempt and safe exact leaf recovery. Never abandon a live
  worker because a timeout elapsed. Unknown spawn outcomes require observation.
  This also retires a ready worker already closed to free capacity: verify its
  stopped runtime identity and exact leaf handoff, then reserve a replacement
  attempt for that same delivery. It is not permission to adopt another ledger.
- `reopen COORD --attempt ATTEMPT --evidence TEXT --heavy-limit 1`: reactivate
  the same retained live worker for findings after ready/blocked. Reprove worker,
  leaf ownership and current head first; resource/dependency gates still apply.
  Send its newly returned attempt ID to the retained worker and accept only a
  fresh result for that ID; queued old-head messages cannot complete it.
  After postmerge invalidation without a live worker, current terminal owner
  observations plus all fresh criterion attestations restore accepted state.
- `refresh`: complete live graph/PR observation before scheduling after merges.
- `replan --plan FILE`: bounded newly discovered scope, never silently drop
  known nodes, change active boundaries, expand repository/permission authority,
  or switch issue mode to PR. New scope outside the user's project needs approval.
- `attest CRITERION --evidence TEXT`: name exact tests, sources/heads and output
  evidence. The helper binds the current owner observations; it does not run
  or assess the tests. Re-attest after any relevant drift.
- `terminal`: list gaps; refresh first. No helper command merges or deploys.

On resumed sessions, check current worker states before calling `dispatch`.
Include the exact returned attempt in the spawn message, so runtime history can
correlate it after a lost spawn response. If the runtime cannot enumerate or
prove that attempt, leave its reservation blocked; never guess a worker ID.
If a worker's bound delivery already owns a PR, continue that exact delivery,
not a second issue-mode claim. If additional independent PR work is needed,
add a separately authorized type-explicit lane after collision checks.

## GitHub tracking journal

Plan an operation before applying it:

```sh
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT tracking plan --kind comment --target atrinik/atrinik#NUMBER --payload /absolute/comment.json --expected /absolute/snapshot.json
python3 -m atrinik_workspace.project_delivery --root RETURNED_ROOT tracking apply --operation RETURNED_ID --expected /absolute/new-snapshot.json
```

Supported payloads:

| Kind | Exact payload | Boundary |
| --- | --- | --- |
| `assign` | `{"login":"authenticated-actor"}` | Add self, preserve other assignees |
| `comment` | `{"body":"concise milestone update"}` | New actor-owned marked comment; no human-body rewrite |
| `link` | `{"issue":"atrinik/repository#N"}` | Target is parent; never force reparent |
| `dependency` | `{"issue":"atrinik/repository#N"}` | Target is blocked by payload issue; bounded cycle check |
| `create-child` | `{"repository":"atrinik/owner","title":"...","body":"..."}` | Authorized parent only; complete duplicate search |
| `project-status` | `{"project":"ID","item":"ID","field":"ID","option":"ID","status":"Review"}` | Existing Atrinik work issue item and Status option only |
| `close-parent` | `{}` | Authorized terminal parent only |

Resolve Projects IDs from live paginated GraphQL; never paste another issue's
item or invent a field/option. Supported statuses: In progress, Blocked, Review,
Done. Done requires observed terminal issue work. Leaf claim already adds its
issue to the configured Project; the coordinator only updates existing items.
An already-satisfied operation makes no write; inspect and record the fact.

After child creation, read its exact result, replan the graph to include it,
then separately journal its native parent/dependency links. Do not dispatch an
unlinked ambiguous child. Create only genuinely missing in-scope requirements
after searching issue titles, bodies, relationships, PRs and existing patches;
the helper's exact-title check is an additional guard, not semantic deduplication.

The journal records planned → in-flight before one API write → bound by live
result proof. On a lost response, use `tracking reconcile` with the operation ID
and fresh expected snapshot. It observes only, never reposts. Missing/ambiguous
results or incomplete pagination stop that target. An unstarted `planned`
operation may use `tracking cancel` after live non-application proof, then a
fresh plan; cancelled history remains preserved. An `in-flight` operation cannot
cancel or repost: preserve evidence for observation or an external maintainer
decision. No automated override exists. Keep progress comments to meaningful
milestones, not polls. Independently satisfied assignment, Project status or relationship
intents may retire while still `planned`; issue/comment creation with an
unexpected matching marker remains ambiguous and cannot use that exception.
Never expose private ledger/tooling content publicly.

## Merge gates, closure and acceptance

Keep each leaf's closing keyword under its existing ledger/governance protocol.
A component PR may say `Closes atrinik/atrinik#N` only when it is the selected
canonical closing delivery; other companion PRs use non-closing references.
Do not put a closing keyword for a whole parent on a partial implementation PR.

After a maintainer merge, refresh the complete graph, verify head/base/merge
identities, route dependent fixes to their owners, update Project status and
post concise parent/dependent milestone evidence. Never rewrite contributor PR
text; the owning leaf updates its own PR through the existing protocol.

The observer conservatively treats every cross-referenced PR as required:
an unmerged reference, untracked native child, incomplete pagination or closed
not-planned issue prevents automatic terminal closure. Explain incidental or
superseded work for human resolution; do not remove evidence to turn it green.
Every declared acceptance criterion requires current integration evidence.
All native children and blockers at every declared depth must also be declared
in the plan, including externally owned work. Acceptance binds transitive native
and planned requirements, so a grandchild's head/check drift invalidates its
ancestor's integration evidence. Undeclared required nodes block closure even
if their issue state is closed.
Referenced/selected PR checks and legacy statuses must be complete and passing;
failed, pending or cancelled checks block terminal observation. Neutral/skipped
checks need applicability evidence in acceptance, just as in leaf delivery.
Required contexts and app identities come from live classic protection plus
[applicable inherited branch rules](https://docs.github.com/en/rest/repos/rules#get-rules-for-a-branch).
Missing required jobs block completion. Unavailable protection data or required
workflow/deployment/security rules without supported evidence stop automatic
terminal decisions; no checks returned is not proof that none are required.
Parent-only PR heads/checks/merge evidence also bind every integration criterion.
Project Done uses the same fresh complete required graph; a parent's Done status
additionally requires full acceptance, not only a closed issue.
Children merged is necessary, not sufficient. Close the authorized parent only
after these checks; otherwise provide the exact remaining closure action.

Before shipping coordinator changes, run production helper unit/concurrency/
failure tests, guidance inventory, wrapper validation and a fresh independent
forward-test using realistic blocked/parallel/merge-gated requests. A fixture
pilot is not evidence that real workers delivered real project PRs. Keep live
pilots read-only unless their specific tracking/implementation scope is authorized.

## Same-owner unchanged-target reconnect

A retained issue worker with unchanged current target coordinates must complete
the issue ledger's public `revalidate-current-targets-cas` proof after
canonical context, live selection and inventory checks. Retain its exact
helper-returned generation/digest/device/inode. Project scheduling renewal
does not itself prove worktree leases or transfer ownership. Generic CAS,
stored check-reuse flags and private helper contexts grant no reconnect proof.
Use the accepted helper only; a proposed helper change cannot authorize its
own reconnect or another paused worker before that change is actually merged.
