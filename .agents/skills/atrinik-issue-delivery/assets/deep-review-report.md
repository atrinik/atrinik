# Delivery review: `<owner>/<repository> <issue|PR> #<number>`

Optional human evidence template; omit sections that do not help this change.
Keep it local and uncommitted, without credentials or confidential details.
This report never grants ownership or recovery authority. For retained bound
deliveries, the helper-managed schema-v1 JSON ledger remains authoritative and
the protocol's required report identity/evidence must be preserved.

## Reviewed revision

- Selected issue/PR: `<URLs; incidental issues remain read-only>`
- Repository, branch and owned worktree: `<exact coordinates>`
- Base / head / merge-base: `<SHAs>`
- Requirements and complete diff reviewed: `<scope and acceptance criteria>`
- Independent reviewer: `<reviewer and raw artifacts supplied>`
- Review result: `<actionable findings or none>`

## Findings and validation

| Finding | Location, evidence and impact | Resolution | Validation |
| --- | --- | --- | --- |
| `<stable ID>` | `<concrete failure or unmet contract>` | `<fix/commit or blocker>` | `<result>` |

Optional suggestions: `<separate nonblocking suggestions, or omit>`

| Command/check | Revision and relevant environment | Result |
| --- | --- | --- |
| `<required or relevant validation>` | `<SHA and inputs>` | `<pass/fail/pending>` |

Record affected evidence refreshed after a change; unchanged valid evidence need
not be rerun. Missing or timed-out checks are never success.

## Retained protocol only

- Ledger identity: `<path, schema, generation and digest>`
- Authority and recovery evidence: `<exact retained references>`
- Resources: `<exact bound identities and disposition>`
- Remote-write recovery: `<retained intent/result and rendered verification>`

## Handoff

- PR and exact final reviewed coordinates: `<URLs and SHAs>`
- Acceptance and independent integrated review: `<results>`
- Expected checks and mergeability: `<verified results at final head>`
- Runtime applicability: `<why inapplicable, or prerequisites/actions/results>`
- Relevant resource repeat/shutdown/cleanup commands: `<only when applicable>`
- Blockers: `<none or concrete unresolved items; human approval if required>`

Confirm issues remain open, PRs unmerged, contributor-owned text preserved and
worktrees/evidence available. Readiness requires passing review, validation and
expected checks; it grants no merge or cleanup authority.
