# Durable master-publication ledger

This contract owns the single rolling master-issue comment. Apply it before
any master comment mutation; the program report mirrors evidence and never
authorizes a write.

## Canonical authority and record

Create or resume one canonical ledger at
`<workspace>/build/program-delivery/<owner>-<repo>-<number>.ledger.json`; prove
it and the report are ignored, regular, no-follow files under safe parents.
Lock the separate stable `<owner>-<repo>-<number>.ledger.lock`, created mode
`0600` without following links and never replaced or removed while retained.
Verify its device/inode after locking and hold that descriptor across ledger
read, pagination, persistence, remote mutation, and reconciliation. A lock on
the replaceable JSON inode is invalid.

Encode the closed schema below. Reject missing/unknown keys, booleans where
integers are required, non-NFC strings, invalid URLs/node IDs/digests, and
out-of-range values. Nullable fields are explicit JSON `null`:

```text
schema_version: 1
generation: integer >= 0
self: {device: integer >= 0, inode: integer > 0}
previous: null | {device, inode, sha256}
authority: {repository, repository_node_id, master_number, master_node_id,
            master_url, goal_thread_id, objective_sha256, actor_login,
            actor_node_id, graph_sha256}
next_authority: null | authority
ordered_graph: [graph entry]
next_ordered_graph: null | [graph entry]
leaf_snapshots: [leaf snapshot]
comment: {marker, phase, mutation, node_id, intended_body,
          intended_sha256, current_sha256, prior_sha256}
observation: {pages, comments, body_bytes, terminal_cursor,
              completed_at, stream_sha256}
```

Authority uses non-empty strings, a positive issue number, lowercase 64-hex
digests, and exact `owner/repository` plus HTTPS GitHub URL coordinates.
`goal_thread_id` is the durable goal's exact tool-returned thread ID.
`objective_sha256` hashes the exact UTF-8 objective returned by the goal API;
do not trim, normalize line endings, or otherwise rewrite it.

Graph entries have exactly `position` (contiguous integers from 1), repository
and issue node IDs, issue number, target branch/release line, and a sorted list
of dependency positions. The graph digest covers canonical `ordered_graph`
only, so ordinary leaf-ledger progress does not rekey the marker. Leaf
snapshots have exactly position, regular no-follow ledger path, generation,
and ledger SHA-256; they are mutable evidence excluded from `graph_sha256`.

Canonical JSON bytes use UTF-8 NFC strings, keys sorted by Unicode code point,
no insignificant whitespace, JSON `false`/`true`/`null`, decimal integers, no
floats, and one trailing LF. Produce the same form as Python
`json.dumps(value, ensure_ascii=False, allow_nan=False, sort_keys=True,
separators=(",", ":")) + "\n"` after schema/NFC checks; hash those exact bytes.

The marker payload is canonical JSON containing exactly `schema_version: 1`,
repository/master/goal/actor node IDs, and `graph_sha256`. Its SHA-256 forms:

```text
<!-- atrinik-program-delivery:v1 sha256=<64 lowercase hex> -->
```

Require the marker exactly once as the final line of `intended_body`, preceded
by one blank line and followed by LF. Any occurrence of the
`<!-- atrinik-program-delivery:` prefix outside that grammar is malformed.

The comment phase is exactly `none`, `planned`, `in-flight`, or `bound`;
mutation is `null`, `POST`, or `PATCH`; digests/body/node fields must match the
phase-specific transition below.

## Durable replacement

Replace generations only under the stable lock. Write a same-directory
no-clobber temporary, record its fstat device/inode as `self`, fsync it, recheck
the old ledger's fstat, generation, and canonical byte digest against
`previous`, atomically replace, then fsync the parent. On resume require the
canonical ledger fstat to equal `self`; generation zero has `previous: null`,
and each later generation names the immediately replaced identity/digest.
Reject stale CAS, inode substitution, corrupt/missing ledger, changed
goal/actor/master coordinate, or unrecorded temporary artifact.

The durable goal and ledger grant authority. GitHub linkage, marker text, the
human report, a leaf ledger, or possession of a comment node ID never does.
Never adopt live text or a matching marker into an absent ledger.

## Bounded complete pagination

Before every decision and after every call, stream GraphQL comment pages of at
most 100 nodes with advancing unique cursors/node IDs. Allow at most 100 pages,
10,000 comments, 65,536 UTF-8 bytes per body, 16 MiB total body bytes, a
30-second request timeout, and three read retries honoring `Retry-After`.
Retain only coordinates/digests and exact marker candidates. Limit excess,
timeout/retry exhaustion, cursor inconsistency, or `hasNextPage` without a
usable cursor means incomplete pagination and stops. Persist counts, terminal
cursor, completion time, and stream digest in `observation`; the stream digest
hashes the canonical API-order array of each comment's node ID, author node ID,
and body SHA-256. Before mutation require two consecutive complete scans with
identical stream digests and marker results.

Require zero matching markers in `none`/first-`planned`, or exactly one
actor-owned marker at the recorded node in `bound`/update recovery. Malformed,
wrong-author, duplicate, missing-bound, drifted, incomplete, uncertain, or
contradictory state stops before mutation.

## Comment state transitions

1. From `none`, after complete zero-match pagination, persist `planned` with
   exact intended body/digest and mutation `POST`.
2. Recheck pagination and CAS; persist `in-flight` before the first `POST`.
   Call once. Bind only one exact actor-owned result whose marker and complete
   body equal intended bytes; record its node ID/current digest in `bound`.
3. If first-`POST` state is `in-flight` and complete pagination has no exact
   result, stop as uncertain and never repost. Later resumptions may paginate
   again and bind one newly visible exact result, but never issue another
   `POST`. Wrong, partial, or duplicate results stop.
4. For update, start only from `bound`; paginate and require recorded node,
   actor, marker, and current digest. Persist new exact bytes as `planned` and
   `in-flight` before `PATCH`. Bind an exact intended result. Retry the same
   idempotent `PATCH` only when that node still has the exact recorded prior
   body; any other state stops.

Keep one concise rolling comment. Every body change gets a new intended digest
and generation; reproduce remote text byte-for-byte from the ledger. Mirror a
bound generation in the report only after the machine record is durable.

## Ordered-graph rekey

Changing graph membership, order, dependencies, target, or release line never
creates a new comment. From `bound`, fully paginate and prove the recorded
node, actor, old marker, and body. Retain current `authority`/`ordered_graph`;
persist the new values as `next_authority`/`next_ordered_graph`, with a
`planned` exact body ending in its new marker while retaining the node/current
digest. Recheck and persist `in-flight`
before one `PATCH` to that node. Bind and promote `next_authority` if the exact
new body is visible; retry identical bytes only if the exact old body remains;
otherwise stop. Interruption must never permit `POST`, absent-ledger adoption,
or a second node. Promote both next fields and clear them only in the durable
bound generation.

## Leaf composition and tests

Compose leaf ledgers read-only: record each dependency-ready leaf's exact
issue-mode ledger path, coordinate identity, generation, and digest against
its graph position. A leaf owns only its branches, worktrees, and PRs; it
cannot claim the master comment, another position, or an incidental issue.
Missing, overlapping, reordered, corrupt, or changed leaf evidence stops until
deliberately replanned under CAS.

Use no-live-mutation fixtures for every pre/post durable transition and remote
call; concurrent writers; stable-lock replacement; stale CAS/inodes; malformed,
duplicate, wrong-author, missing, and drifted markers; multi-page results and
pagination loss; ledger/report loss/corruption; accepted but invisible POSTs;
PATCH resumption; graph-rekey interruption at each boundary; and mismatched,
overlapping, or reordered leaf ledgers. Prove the comment node never changes.
