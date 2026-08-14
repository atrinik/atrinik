# Program integration review checklist

Use this after each leaf reaches its delivery exit conditions and again on the
exact final repository tips. Leaf review remains governed by
`atrinik-issue-delivery`; this checklist covers interactions that a leaf diff
cannot prove alone.

## Graph and ownership

- Does every acceptance item have one clear existing owner issue or an
  evidence-backed exception?
- Do native parent/subissue relationships preserve existing ownership?
- Are newly discovered issues genuinely absent rather than duplicates under a
  different title, PR, branch, or parent?
- Are closed, transferred, superseded, and maintainer-attested items described
  accurately?
- Does the machine ledger bind the exact repository/master nodes, durable goal
  authority, authenticated actor, exact objective digest, and complete
  ordered leaf graph rather than trusting linkage, marker text, or the report?
- Is every composed leaf ledger regular, no-follow, coordinate-exact, current,
  read-only, and limited to its declared position and owned artifacts?

## Master publication recovery

- Is the canonical ledger ignored, schema-valid, complete, and protected by an
  exclusive stable non-replaced lock file plus generation/digest/inode CAS and
  durable same-directory replacement?
- Was complete bounded comment pagination performed before every decision and
  after every call, with zero markers for a first post or exactly one recorded
  actor-owned marker for a bound update?
- Do wrong-author, malformed, duplicate, drifted, missing, partially paginated,
  corrupt, stale-CAS, inode-substituted, and uncertain states stop before a
  remote mutation?
- Does `none -> planned -> in-flight -> bound` persist exact intended bytes,
  intended/current SHA-256 digests, marker identity, mutation kind, and remote
  node ID at the required boundaries?
- After a first `POST` becomes `in-flight`, does no exact result stop without
  reposting? For `PATCH`, is retry limited to the same recorded node and bytes
  when the remote body is exactly the prior or intended digest?
- Do crash-point tests cover before/after every durable transition and remote
  call, including an accepted-but-not-yet-visible result and resumption?
- Do concurrency, stale-CAS, marker, full-pagination, ledger/report loss,
  corrupt-state, duplicate, wrong-author, remote-drift, and leaf-composition
  tests prove fail-closed behavior without live GitHub mutation?
- Does the human report mirror only already-durable generations and make clear
  that it cannot authorize or recover a write?
- Is canonical JSON, objective/graph hashing, marker grammar, stable lock
  identity, streaming limits, and ordered-graph same-node rekeying exact?
- When no durable goal exists, do summaries remain local with no ledger or
  master-comment mutation?
- Is a proposed child recorded with exact repository/title/body/parent bytes,
  two stable complete duplicate scans, and separate create/link state slots?
- After create or link becomes `in-flight`, does an absent result stop without
  reposting until one exact issue or native relationship can be bound?
- Does create reconciliation use a unique child marker and durable pre-call
  stream rather than client time, and does link binding prove the parent-child pair from
  both `parent` and completely paginated `subIssues` without inventing an edge
  node ID?
- Is the proposed child's graph position/dependency set fixed before creation
  and reproduced exactly during the same-node graph rekey?

## Order and gates

- Is the next stage dependency-ready according to the explicit plan rather than
  merely green?
- Are there cycles, hidden prerequisites, shared files, schemas, ledgers,
  baselines, or closing paths that require serialization?
- Will the proposed merge order leave every issue open until all of its required
  release lines land?
- After each external merge, which heads, digests, baselines, reviews, and checks
  are stale?

## Cross-change integration

- Compare cumulative resulting trees and semantic behavior, not only commit
  counts or independently green PRs.
- Check overlapping authored paths, generated consumers, descriptor versions,
  validation schemas, benchmark inputs, inventory counts, and release metadata.
- Confirm that conflict resolution preserves both the newly merged behavior and
  the leaf's intended change; never restore a deliberately deleted surface.
- Verify that temporary diagnostics and generated evidence remain in their
  governed ignored or deployment locations.
- Look for duplicate implementations, mutually obsolete work, repeated
  rationales, order-only churn, and unnecessary cross-line divergence.

## Release lines and publication

- Does every shared change have separate current-base validation for each
  required line, or a documented format, consumer, runtime, or provenance
  reason for a single-line exception?
- Do companion PRs link without closing while the canonical/default-line PR
  alone closes the owner issue?
- Are PR titles, bodies, checklists, head coordinates, issue links, validation
  claims, and remote rendering current?
- Are draft/ready states intentional, with no unresolved reviews, conversations,
  or actionable comments?

## Validation currency

- Record exact base and head SHAs for every local validation and remote check.
- Treat skipped or neutral checks according to repository policy; never call
  stale, cancelled, pending, or old-head checks current.
- Rerun all affected focused, aggregate, generated-output, archive, syntax,
  runtime, scenario, and diff checks after reconciliation.
- Review the raw cumulative diff and current final trees after the last merge;
  prior leaf reports are supporting evidence, not a substitute.

## Close readiness

- Are all non-master stages externally merged, closed, or explicitly excepted
  with exact evidence?
- Does the terminal audit cover the final tips and include its own resulting
  changes in any ledger it creates?
- Does a self-maintaining ledger predeclare each line's immutable horizon,
  exact suffix count/order, and per-ordinal changed-path allowlist, fail on any
  other suffix, and record the resulting SHAs durably in the ignored report and
  final handoff pending a program-level ledger?
- Does the contributor master body remain read-only while the ignored report
  and final handoff stay synchronized with current counts, coordinates,
  dependencies, and artifact requirements?
- Is there zero known actionable work, and is the only remaining action an
  explicitly named human closure or release step?
- Is the final close-ready master summary bound at the recorded comment node
  with exact intended/current digests after complete final pagination?
