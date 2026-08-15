# Durable program-publication ledger

This contract owns the single rolling master-issue comment and one proposed
missing-child publication. Apply it before either remote mutation. The program
report mirrors evidence and never authorizes a write.

## Canonical authority, paths, and bytes

Create or resume one canonical ledger at
`<workspace>/build/program-delivery/<coordinate-sha256>.ledger.json`. The digest
hashes canonical JSON including its LF with exactly `domain` set to
`atrinik-program-delivery-coordinate`, `schema_version` set to 1, and exact
`repository_node_id` and `master_node_id`. Prove
it and the report are ignored, regular, no-follow files under safe parents.
Lock the master-coordinate `<coordinate-sha256>.publication.lock`, created mode
`0600` without following links and never replaced or removed while retained.
Every goal/objective for that master uses this arbitration lock; goal-specific
locks are forbidden. This injective node-identity digest prevents owner/repo
hyphen collisions. Verify its device/inode after locking, require it equal
the ledger's `lock` identity, and hold that descriptor across ledger
read, pagination, persistence, remote mutation, and reconciliation. A lock on
the replaceable JSON inode is invalid.
Immediately before and after each observation, durable replacement, or remote
call, lstat/open the lock path no-follow and require its device/inode equal both
the held descriptor and ledger identity. Path replacement stops the writer.

Canonical JSON is UTF-8 with NFC strings, keys sorted by Unicode code point,
no insignificant whitespace, JSON booleans/null, decimal integers, no floats,
and one trailing LF. Produce the same bytes as
`json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True,
separators=(",", ":")) + "\n"` after schema/NFC validation. SHA-256 always
hashes these exact bytes unless an exact UTF-8 body is named. Reject unknown or
missing keys, booleans where integers are required, non-NFC strings, invalid
URLs/node IDs/digests, and out-of-range values. Nullable fields are explicit
JSON `null`. All integers are unsigned 64-bit (`0` through
`18446744073709551615`) before the narrower per-field rules apply; reject an
overlong token before converting it. Strings and object keys must contain only
Unicode scalar values, so lone surrogate code points are corrupt.

The ledger is bounded independently of remote pagination. Before reading or
JSON decoding, `fstat` the open no-follow descriptor and reject a size above
8,388,608 bytes; read through that descriptor with an 8,388,609-byte ceiling
and reject excess or size drift. The validated canonical serialization must
also be at most 8,388,608 bytes. Each array or object has at most 10,000
members, every UTF-8 string is at most 65,536 bytes, and a ledger path is at
most 4,096 UTF-8 bytes. JSON value depth is at most 16, counting the top-level
object as depth 1. These limits apply recursively, including graph,
snapshot, dependency, observation, body, and evidence values. Check them
iteratively before digesting or allocating derived canonical collections;
parser or serializer recursion exhaustion is corrupt state. A limit failure
stops; never truncate, partially parse, or rewrite it. Reject duplicate object
keys and require the retained bytes themselves to equal the canonical
serialization exactly, so whitespace, BOM, escapes, or a missing LF cannot be
silently normalized.

## Closed schema

The top-level object has exactly:

```text
schema_version: 1
generation: integer >= 0
self: {device: integer >= 0, inode: integer > 0}
lock: {device: integer >= 0, inode: integer > 0}
previous: null | {device: integer >= 0, inode: integer > 0, sha256: digest}
authority: authority
next_authority: null | authority
ordered_graph: [graph_entry]
next_ordered_graph: null | [graph_entry]
leaf_snapshots: [leaf_snapshot]
comment: comment_slot
child: null | child_slot
observation: {comment_scan: null | comment_scan,
              child_scan: null | child_scan,
              parent_scan: null | parent_scan}
```

`authority` has exactly `repository`, `repository_node_id`, `master_number`,
`master_node_id`, `master_url`, `goal_thread_id`, `objective_sha256`,
`actor_login`, `actor_node_id`, and `graph_sha256`. It uses exact non-empty
`owner/repository`, HTTPS GitHub URL, node IDs, positive issue number, and
lowercase 64-hex digests. The goal thread ID is the durable goal's exact tool
result. `objective_sha256` hashes the exact UTF-8 objective returned by the goal
API; do not trim, normalize line endings, or otherwise rewrite it.

Each `graph_entry` has exactly `position`, `repository`, `repository_node_id`,
`issue_number`, `issue_node_id`, `target`, `release_line`, and `dependencies`.
Positions are contiguous integers from 1; issue numbers are positive;
dependencies are sorted unique earlier positions; all other values are
non-empty strings. `graph_sha256` hashes canonical `ordered_graph` only, so
ordinary leaf progress does not rekey it. Each `leaf_snapshot` has exactly
`position`, `ledger_path`, `generation`, and `ledger_sha256`; positions form a
unique subset of dependency-ready graph positions. Absence means that position
is not ready and authorizes nothing. Paths are absolute regular no-follow
issue-delivery ledgers, generations are nonnegative, and digests are lowercase
64-hex.

The marker payload has exactly `schema_version`, `repository_node_id`,
`master_node_id`, `goal_thread_id`, `objective_sha256`, `actor_node_id`, and
`graph_sha256`, with schema version 1 and values equal to authority. Hash its
canonical JSON without its trailing LF to form:

```text
<!-- atrinik-program-delivery:v1 sha256=<64 lowercase hex> -->
```

Require the marker exactly once as the final line of `intended_body`, preceded
by one blank line and followed by LF. Any occurrence of the
`<!-- atrinik-program-delivery:` prefix outside that grammar is malformed.

`comment_slot` has exactly `marker`, `phase`, `mutation`, `node_id`,
`intended_body`, `intended_sha256`, `current_sha256`, and `prior_sha256`.
Phase is `none`, `planned`, `in-flight`, or `bound`; mutation is null, `POST`,
or `PATCH`. `none` requires marker and all remaining fields null.
`planned`/`in-flight` require marker, mutation, intended body/digest; POST
requires node/current/prior null, while PATCH requires all three and
`prior_sha256 == current_sha256`. `bound` requires marker, mutation null, node,
intended body/digest, `current_sha256 == intended_sha256`, and prior null.

`child_slot` has exactly:

```text
proposal: {position, dependencies, repository, repository_node_id, title, body,
           body_sha256, child_marker, parent_repository, parent_number,
           parent_node_id}
search: null | {query_sha256, pages, issues, issue_node_ids, terminal_cursor,
         completed_at, stream_sha256, candidates, proven_missing}
create: {phase, mutation, pre_call_stream_sha256, pre_call_issue_node_ids,
         issue_number, issue_node_id, issue_url, creator_node_id,
         created_at, intended_sha256}
link: {phase, mutation, parent_node_id, child_node_id,
       pre_call_parent_stream_sha256, pre_call_subissue_node_ids,
       bound_parent_stream_sha256, intended_sha256}
```

Proposal position is the intended contiguous graph position, dependencies are
sorted unique existing earlier positions, strings are exact NFC bytes, and its
body digest hashes exact UTF-8 body; parent coordinates equal authority. Each
candidate has exactly `repository`, `issue_number`, `issue_node_id`, `state`,
`creator_node_id`, `created_at`, `title_sha256`, and `body_sha256`. Candidates
remain in canonical API order. Search counts are
bounded nonnegative integers, cursor/time are validated as below, and
`proven_missing` is boolean. `child_marker` uses the comment marker grammar with
namespace `atrinik-program-child`; its payload contains authority's repository,
master, goal, objective, and actor IDs plus proposal position, dependencies,
target repository, and title SHA-256. It occurs exactly once as the final body
line and makes the exact intended child unique without trusting client time.
Null search is allowed only while create and link
are `none`; a create plan requires a non-null proven-missing search. Create/link
use the four phases. `none` requires mutation and all call/result/intended
fields null. `planned` requires mutation `POST`, exact canonical request digest,
and null call/result fields. `in-flight` additionally requires the exact stable
pre-call issue-stream digest and ordered issue-node vector. Bound create requires
mutation null, every issue result including server `created_at`, creator equal
to authority, and clears the pre-call fields. Link planned/in-flight likewise
retain the complete pre-call subissue-node vector and stream digest. Bound link
requires mutation null, the proposal's exact
parent and bound child IDs, and the digest of the complete parent stream that
proves the pair. GitHub exposes no relationship node ID; never invent one.
Link cannot leave `none`
until create is bound. `child: null` means the execution plan proves no missing
child is proposed and permits no child mutation.

`comment_scan` has exactly `pages`, `comments`, `body_bytes`, `node_ids`,
`terminal_cursor`, `completed_at`, and `stream_sha256`; `child_scan` is exactly
the search object; `parent_scan` has exactly `pages`, `relationships`,
`target_occurrences`, `child_parent_node_id`, `subissue_node_ids`,
`terminal_cursor`, `completed_at`, and `stream_sha256`. Counts are bounded
nonnegative integers, time is UTC RFC 3339 ending `Z`, cursor is non-empty after
a nonempty scan and otherwise null, and hashes are lowercase 64-hex. Any phase
matrix mismatch or impossible cross-field combination is corrupt and stops.
Every node-ID vector contains non-empty unique IDs in canonical API order, has
at most 10,000 entries, and has length equal to its corresponding count. Store
IDs only, never bodies or other unnecessary private data. Parent
`relationships` counts the complete subissue stream while
`target_occurrences` counts only the proposed child (zero before link and
exactly one when bound); `child_parent_node_id` is null before link and equals
the master node when bound. Retain full scan evidence in each planned and
in-flight intent generation so recovery can compare observations and prove a
create result node or target link pair was absent. Clear the pre-call identity
vectors only in the durably bound phase.

## Durable replacement and initialization

Replace generations only under the stable lock. Write a same-directory
no-clobber temporary, record its fstat device/inode as `self`, fsync it, recheck
the old ledger's fstat, generation, and canonical byte digest against
`previous`, atomically replace, then fsync the parent. On resume require the
canonical ledger fstat to equal `self`; generation zero has `previous: null`,
and each later generation names the immediately replaced identity/digest.
Construct and fully validate the prospective next record, including canonical
size and all epoch invariants, before writing any byte or changing the retained
in-memory record; validation failure preserves the prior generation exactly.

Fresh initialization is only `(lock absent, ledger absent)`: atomically create
the lock, acquire it, prove the ledger remains absent, perform the complete
remote namespace scans below, and write generation zero. `(lock present,
ledger absent)` is ambiguous, including a crash before generation zero, and
stops. Recovery requires restoring the exact ledger from durable backup; a new
goal does not change the master coordinate or bypass this stop. Otherwise a
maintainer must supply a different master issue coordinate. Never delete/reuse
a lock-only artifact or infer freshness from remote state alone.

Reject stale CAS, inode substitution, corrupt/missing ledger, changed
goal/actor/master coordinate, or unrecorded temporary artifact. The durable
goal and ledger grant authority. GitHub linkage, marker text, the report, a
leaf ledger, or possession of a result node ID never does. Never adopt live
text, an issue, a relationship, or a marker into an absent ledger.

## Bounded complete observations

Before every decision and after every call, stream GraphQL pages of at most 100
nodes with advancing unique cursors/node IDs. Allow at most 100 pages, 10,000
nodes, 65,536 UTF-8 bytes per body, 16 MiB total body bytes, a 30-second request
timeout, and three read retries honoring `Retry-After`. Limit excess, timeout,
retry exhaustion, cursor inconsistency, or `hasNextPage` without a usable
cursor means incomplete pagination and stops. Persist counts, ordered node-ID
vectors, terminal cursor, completion time, and stream digest. A stream digest
hashes the canonical
API-order array of node ID, actor/creator node ID, and exact relevant body/title
digests. Derive it from that array; never accept a caller-supplied digest label
or compare a digest without its retained node vector. Before mutation require
two consecutive complete scans with identical stream digests, node vectors,
and candidate results.

For fresh comment `none`/first-POST planning, require zero occurrences of the
`<!-- atrinik-program-delivery:` namespace across every comment, regardless of
digest, grammar, or author. With an authoritative ledger, require exactly one
actor-owned matching marker at the recorded node for bound/update recovery and
zero other namespace occurrences. A valid nonmatching old marker conflicts;
it never authorizes creation. Malformed, wrong-author, duplicate,
missing-bound, drifted, incomplete, uncertain, or contradictory state stops.

## Comment transitions

1. From `none`, after complete empty-namespace pagination, persist `planned`
   with exact intended body/digest and mutation POST.
2. Recheck pagination and CAS; persist `in-flight` before the first POST. Call
   once. Bind only one exact actor-owned result whose marker and complete body
   equal intended bytes; record its node ID/current digest in `bound`.
3. If first-POST state is `in-flight` and complete pagination has no exact
   result, stop as uncertain and never repost. A later resume may bind one
   newly visible exact result, but never issue another POST. Other results stop.
4. For update, start only from `bound`; require recorded node, actor, marker,
   and current digest. Persist new bytes as `planned` and `in-flight` before
   PATCH. Bind the exact result. Retry the same idempotent PATCH only when that
   node still has the exact recorded prior body; any other state stops.

Keep one concise rolling comment. Every body change gets a new digest and
generation. Mirror a bound generation only after the machine record is durable.

## Ordered-graph same-node rekey

Graph changes never create a new comment. From bound, fully prove the recorded
node, actor, old marker, and body. Retain current authority/graph and persist
new values in `next_authority`/`next_ordered_graph`. Next authority must equal
current authority byte-for-byte except `graph_sha256`, which must equal the
recomputed next graph digest; rekey cannot transfer repository, master, goal,
objective, or actor authority. PATCH a planned then in-flight exact new-marker
body on the same node. Promote/clear both next fields only in durable bound.
Retry identical bytes only while the exact old body remains. Any interruption
must never permit POST, absent-ledger adoption, or a second node.

## Missing-child creation and native linking

Record the exact proposal before searching. Exhaustively paginate repository
issues in open and closed states and the master's native subissue graph using
the bounds above. Deterministically inspect every bounded issue client-side;
a candidate has an exact title SHA-256, final body SHA-256, master backlink,
native parent, or valid child marker whose payload matches this authority and
proposal. A malformed or unrelated child marker is a candidate only when one
of those other predicates matches; otherwise record it as bounded noncandidate
evidence. The query digest covers these literal predicates and fields. Record
every candidate in API order. Any candidate,
incomplete scan, or changing stream means `proven_missing: false` and stops.

Only a stable zero-candidate search may persist create `planned` with the exact
proposal and request digest. Recheck search and CAS, persist create `in-flight`,
then call issue creation once. Bind only one issue in the proposed repository
whose creator, exact title/body/child marker, and identity match and which was
absent from the recorded pre-call stream. Server `created_at` remains evidence,
not a client-clock authority. If none is visible after in-flight create, stop as uncertain
and never create again. Later resumptions repeat the full search and may bind
one exact result; zero or multiple results stop.

After create is durably bound, fully paginate the native parent graph and
require the relationship absent before link `planned`, then `in-flight`, with
exact parent/child IDs and request digest. Call native linking once. Bind only
one exact parent-child pair, proving both the child's `parent` equals the master
and the completely paginated master `subIssues` contains that child, then store
the parent-stream digest. If none is visible after in-flight link, stop as
uncertain and never link again; later resumes may bind the exact relationship
but never repost. An existing link is accepted only while reconciling this
recorded in-flight intent. Wrong parent, duplicate relationship, disappeared
child, changed proposal/body, or ambiguous result stops. Comment, create, and
link slots advance independently under the same lock and generation CAS, so a
crash at any boundary cannot authorize the next remote call.

After child create/link binding, rekey the ordered graph on the existing master
comment node. The new entry must use the proposal's recorded position,
dependencies, repository identity, and bound issue number/node; every other
entry stays byte-identical. Any different placement or dependency set stops
instead of transferring the child intent.

## Leaf composition and executable tests

Compose leaf ledgers read-only: record each dependency-ready leaf's exact
issue-mode ledger path, identity, generation, and digest at its graph position.
A leaf owns only its branches, worktrees, and PRs; it cannot claim the master
comment, another position, or an incidental issue. Missing, overlapping,
reordered, corrupt, or changed evidence stops until deliberately replanned.

Use no-live-mutation fixtures for every pre/post durable transition and remote
call; concurrent writers; stable-lock replacement; stale CAS/inodes; malformed,
duplicate, wrong-author, missing, and drifted markers; multi-page results and
pagination loss; ledger/report loss/corruption; accepted but invisible POSTs;
PATCH and graph-rekey resumption; full duplicate search; uncertain create/link;
wrong-parent linking; and mismatched/reordered leaf ledgers. Prove the comment
node never changes and each child create/link call count is at most one. Run
the repository's executable `ProgramLedgerModelTests` harness; it models
crash/resume and CAS without live GitHub mutation and complements the prose
contract assertions.
