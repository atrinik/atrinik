# Delivery review

Review the complete base-to-head diff against the selected issue or PR's actual
requirements. Record the base, head and merge-base SHAs with the evidence.
An ordinary source review requires no authoritative ledger or report template.

## Scope and findings

- Check acceptance behavior, regressions, ownership, relevant callers and tests,
  and documentation affected by the change. Include added/deleted files and
  generated outputs in the diff review.
- Inspect failure paths, compatibility, security, concurrency and performance
  where the changed behavior makes them relevant. Do not invent work in an
  unrelated category merely to complete a checklist.
- Separate actionable defects from optional suggestions. A blocker needs a
  concrete failure, unmet acceptance criterion, or violated contract, with its
  location, impact and evidence. Style preferences and speculative redesigns
  do not block delivery. Record any unresolved scope decision explicitly.
- Keep a concise finding record with resolution and validation. A small change
  can use a short note; [the report asset](../assets/deep-review-report.md) is
  optional. Keep local reports private and free of credentials.

## Validation and independent review

Run tests proportional to the change and the physical owner's required checks.
Use behavioral assertions and regression coverage where useful; do not add tests
that merely repeat implementation or match prose. Runtime-irrelevant changes
need no playable session. For required runtime work, use
[runtime verification](runtime-verification.md).

Require an independent final integrated review of the complete current diff,
using raw requirements and changed artifacts, without supplying an intended
conclusion. Fix actionable findings and validate the affected behavior. The
reviewer must assess fixes and their effect on the integrated result; do not
restart unrelated reviews or certifications after every correction.

For changes to retained helper authority, binding, recovery, remote writes or
resource lifecycle contracts, additionally read
[helper and lifecycle certification](helper-lifecycle-review.md).

## Final evidence

Retain review and test evidence while its revision, environment and relevant
inputs remain valid. Refetch final base/head refs and recompute the merge base;
drift invalidates affected evidence and requires assessment of the new complete
diff. Missing or timed-out checks are not success. New actionable feedback
requires resolution before handoff.

Confirm the final head's expected checks, rendered PR body, review threads and
determinate conflict-free mergeability. Mark a draft ready only after relevant
validation, independent review and all expected pre-readiness checks pass;
recheck checks triggered by that transition. Required human approval blocks
merging, not readiness. Leave already-ready PRs ready, issues open and PRs
unmerged. Report remaining blockers and optional suggestions separately.
