# Durable master-publication ledger

This contract owns the single rolling master-issue comment. Apply it before
any master comment mutation; the program report mirrors evidence and never
authorizes a write.

## Canonical authority and record

Create or resume exactly one canonical machine-readable ledger at
`<workspace>/build/program-delivery/<owner>-<repo>-<number>.ledger.json` and
prove it and the human report are ignored. Require regular no-follow files and
safe parents. Hold an exclusive lock on the coordinate ledger while deciding
or publishing so concurrent zero-match creators serialize.

Key the ledger to the exact repository and master issue node IDs, number, and
URL; the durable goal authority and SHA-256 of its normalized objective; the
authenticated actor node ID and login; and the complete ordered leaf graph
with its digest. Store `schema_version`, a monotonic `generation`, the prior
canonical-file inode and byte digest used for compare-and-swap, and one comment
record containing:

- a deterministic marker bound to the repository node, master node, goal
  authority, actor, and ordered-leaf-graph digest;
- phase `none`, `planned`, `in-flight`, or `bound`;
- intended and current body SHA-256 digests, exact intended bytes, and the
  remote comment node ID when bound; and
- the last completely paginated observation and mutation kind (`POST` or
  `PATCH`).

Create and replace generations durably with a same-directory, no-clobber
temporary file, file fsync, generation/digest/inode CAS, atomic replace, and
parent fsync. Reject an unknown schema/key, noncanonical value, missing
generation, stale CAS, inode substitution, corrupt or missing ledger, changed
goal/actor/master/graph coordinate, or unrecorded temporary artifact. The
durable goal and ledger grant authority; GitHub linkage, marker text, the human
report, a leaf ledger, or possession of a comment node ID never does.

## Remote reconciliation and state transitions

Before every decision and after every call, fully paginate all master comments
and validate bounded node IDs, authors, bodies, and markers. Require zero
matching markers in `none`/first-`planned`, or exactly one actor-owned marker
with the recorded node ID in `bound`/update recovery. Any malformed marker,
wrong author, duplicate, missing bound comment, drifted body, pagination gap,
API uncertainty, or contradictory result fails closed before mutation. Never
adopt live text or a matching marker into an absent ledger.

Publish only through this state machine:

1. From `none`, after complete zero-match pagination, persist `planned` with
   the exact intended body and digest.
2. Recheck comments and CAS, then durably persist `in-flight` before the first
   `POST`. Call once. Bind only one exact actor-owned result whose marker and
   complete body equal the intended bytes, recording its node ID and current
   digest in `bound`.
3. If first-`POST` state is `in-flight` and complete pagination finds no exact
   result, stop as an uncertain write and never repost. A wrong, partial, or
   duplicate result also stops; preserve the ledger for reconciliation.
4. For an update, start only from `bound`; paginate and require the recorded
   node, actor, marker, and current digest, then persist the new exact bytes as
   `planned` and `in-flight` before `PATCH`. After an interruption, bind an
   exact intended result. If the same node still has the exact recorded prior
   body, repeating the same idempotent `PATCH` is allowed after full pagination
   and CAS; any other remote state stops.

Keep one concise rolling comment. Every body change requires a new intended
digest and generation; never edit remote text that is not reproduced
byte-for-byte from the ledger. Mirror each bound generation, body digests,
comment node ID, and pagination evidence in the human report only after the
machine record is durable.

## Leaf composition and tests

Compose leaf ledgers read-only by recording each dependency-ready leaf's exact
issue-mode ledger path, coordinate identity, generation, and digest against
its declared graph position. A leaf ledger may own only that leaf's branches,
worktrees, and PRs: it cannot claim the master comment, another leaf position,
or an incidental issue. A missing, overlapping, reordered, corrupt, or changed
leaf ledger invalidates the graph snapshot and stops master publication until
the program ledger is deliberately replanned under CAS.

Use no-live-mutation fixtures for crash points before/after each durable state
transition and remote call; concurrent writers; stale CAS and inode changes;
malformed, duplicate, wrong-author, missing, and drifted markers; multi-page
results and pagination loss; ledger/report loss or corruption; accepted but
temporarily invisible POSTs; idempotent PATCH resumption; and mismatched,
overlapping, or reordered leaf ledgers.
