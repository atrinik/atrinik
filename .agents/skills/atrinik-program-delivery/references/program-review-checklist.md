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
