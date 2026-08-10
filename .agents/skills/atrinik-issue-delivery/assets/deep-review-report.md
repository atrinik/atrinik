# Deep self-review: `<owner>/<repository>#<issue>`

This ignored local report tracks evidence for the complete current-base diff.
Do not include credentials, confidential material, or unnecessarily actionable
vulnerability detail. Do not commit or publish this report.

## Coordinates

- Issue: `<URL>`
- Pull request(s): `<URL(s)>`
- Issue-closing path: `<default-branch PR URL or manual post-merge close>`
- Review started / last refreshed: `<UTC timestamps>`

| Release line / owner | Target / fetched base | Head branch / SHA | Merge base | Worktree | Commits reviewed |
| --- | --- | --- | --- | --- | --- |
| `<repository@line>` | `<branch>` / `<sha>` | `<branch>` / `<sha>` | `<sha>` | `<absolute path>` | `<sha and subject list>` |

## Acceptance traceability

| Requirement | Implementation | Tests or verification | Status / evidence |
| --- | --- | --- | --- |
| `<issue requirement>` | `<paths/symbols>` | `<commands/actions>` | `<met/deferred + evidence>` |

## Complete diff inventory

- Diff command: `<exact base-to-head command>`
- Diff summary: `<files, insertions, deletions>`
- Added: `<paths or none>`
- Modified: `<paths or none>`
- Deleted/renamed/mode/binary: `<paths or none>`
- Cross-repository or cross-line consumers/contracts: `<impact or none>`
- Generated versus authored files: `<inventory>`
- Unrelated or formatting-only churn: `<none or explanation>`

## Review rounds

### Round `<n>` — `<head SHA>`

- Reviewer/context: `<independent agent or primary pass; raw inputs supplied>`
- Scope: `<complete diff and surrounding contracts inspected>`
- Checklist sections: `<all, or specific applicability notes>`
- Validation evidence: `<commands and results>`
- Outcome: `<finding IDs / zero known actionable findings>`

## Findings

Use stable IDs such as `R001`. Severities are `critical`, `high`, `medium`,
`low`, and non-actionable `note`.

| ID | Severity | Location | Evidence and impact | Proposed resolution | Status | Fixing commit | Validation |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `R001` | `<severity>` | `<path:line or surface>` | `<specific evidence>` | `<concrete fix>` | `<open/fixed/validated/deferred/reopened>` | `<sha or n/a>` | `<command/result or pending>` |

## Focused analyses

### Correctness, recovery, and compatibility

`<edge cases, failures, regressions, platform/mixed-version impact>`

### Architecture, ownership, and future migration

`<physical owner, APIs/protocols/schemas, coupling, replacement/classic impact>`

### Cleanup, simplification, and duplication

- Within this change: `<candidates or none with evidence>`
- Existing repository helpers: `<reuse candidates or none>`
- Standard library: `<replacement candidates or none>`
- Justified new dependencies: `<candidate/cost or none>`
- Dead/superseded code and docs: `<candidates or none>`

### Tests and verification quality

`<positive/negative/boundary assertions, fixtures, coverage, manual gaps>`

### Concurrency, lifecycle, rollback, and data integrity

`<locks, retries, interruption, atomicity, state ownership, cleanup>`

### Scale and performance

`<complexity, CPU, memory, I/O, network/API, cache, build impact and measurements>`

### Safety, security, privacy, and supply chain

`<trust boundaries, permissions, injection/path risks, secrets, provenance,
licenses, dependencies, denial-of-service considerations>`

### Operations and human experience

`<logs/errors/progress, developer/operator workflow, cross-platform behavior,
accessibility/localization, docs and manual verification>`

## Resource discovery and disposition

| Kind | Exact name/coordinates | Reused, created, or preserved | Reason / cleanup rule |
| --- | --- | --- | --- |
| Profile/build | `<value or none>` | `<disposition>` | `<evidence>` |
| Server/client data | `<value or none>` | `<disposition>` | `<evidence>` |
| Scenario/state | `<value or none>` | `<disposition>` | `<evidence>` |
| Topology/services | `<value or none>` | `<disposition>` | `<evidence>` |

## Validation ledger

| Final-head command or check | Result | Evidence / notes |
| --- | --- | --- |
| `<command>` | `<pass/fail/not run>` | `<summary and head SHA>` |

## Manual verification handoff

- Applicability: `<Classic / replacement / runtime irrelevant and why>`
- Prerequisites: `<display, tools, profile, credentials-local-only>`
- Exact commands: `<copy-pasteable lifecycle/test commands>`
- Feature actions: `<precise reproduction steps>`
- Expected results: `<observable outcomes>`
- Repeat: `<safe repeat/reset instructions>`
- Shutdown and cleanup: `<exact commands; no cleanup apply>`

## Exit audit

- [ ] Every requirement is traced.
- [ ] Every finding is fixed and validated or concretely deferred out of scope.
- [ ] A fresh complete post-fix review found zero known actionable findings.
- [ ] No prior finding reopened.
- [ ] Required validation passed at final committed HEAD.
- [ ] Rendered PR body, comments/threads, mergeability, and expected checks were
      rechecked at that same HEAD.
- [ ] Issue remains open; no merge, self-approval, force-push, destructive reset,
      credential disclosure, or cleanup apply occurred.
- [ ] Exact resources, verification, shutdown, repeat, cleanup, and blockers are
      ready for handoff.

Final known actionable findings: `<zero or list>`

Blockers: `<none or exact blocker>`
