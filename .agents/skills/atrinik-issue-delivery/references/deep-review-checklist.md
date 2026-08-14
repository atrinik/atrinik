# Deep review checklist

Load this checklist only after the initial implementation is coherent and the
ignored report has been instantiated. Review the complete latest diff against
the current PR base. Record evidence and applicability; do not turn irrelevant
categories into invented findings.

## Contents

- [Review method](#review-method)
- [Requirements and diff inventory](#requirements-and-diff-inventory)
- [Correctness and compatibility](#correctness-and-compatibility)
- [Architecture and maintainability](#architecture-and-maintainability)
- [Reuse, duplication, and cleanup](#reuse-duplication-and-cleanup)
- [Tests and verification](#tests-and-verification)
- [Lifecycle, concurrency, and integrity](#lifecycle-concurrency-and-integrity)
- [Scale and performance](#scale-and-performance)
- [Safety, security, and supply chain](#safety-security-and-supply-chain)
- [Operations and human experience](#operations-and-human-experience)
- [Exit audit](#exit-audit)

## Review method

- Freeze and record the current base SHA, head SHA, merge-base, branch, worktree,
  commits, changed paths, and diff statistics.
- Record the explicit entry mode and selected issue/PR coordinates. Compare the
  raw issue when present, PR body and conversations, and acceptance criteria
  directly with the complete diff; never fabricate missing issue requirements.
- Inspect new files, deletions, renames, generated outputs, binary changes, and
  mode changes as well as ordinary hunks.
- Read enough surrounding code, tests, history, ownership guidance, and upstream
  callers/consumers to validate assumptions.
- Run independent passes from raw artifacts when available. Do not prime a
  reviewer with earlier findings or the intended conclusion.
- Give each finding a stable ID. Use severity `critical`, `high`, `medium`, or
  `low`; use `note` only for non-actionable evidence.
- Record exact location, evidence, impact, proposed resolution, status, fixing
  commit, and validation. Never mark a finding fixed solely because code changed.
- Re-open a finding when later changes invalidate its evidence or validation.

## Requirements and diff inventory

- Trace every desired outcome and acceptance criterion to implementation, test,
  documentation, manual verification, or a concrete evidenced deferral.
- Confirm the change solves the selected issue or PR problem rather than a
  neighboring one.
- Identify unrequested behavior, scope creep, hidden compatibility changes, and
  accidentally omitted files.
- Confirm each file belongs to the physical repository that owns its contract.
- Check cross-repository or cross-line changes have independent branches,
  commits, and PRs. When an issue exists, require one unambiguous issue-closing
  path: a default-branch closing keyword or documented manual post-merge close.
  When no issue exists, require no synthetic or broadened closing reference.
- Verify the fetched target branch and recorded base SHA are current and correct.
- Check public APIs, protocols, schemas, manifests, fixtures, docs, packages,
  release inputs, and generated consumers stay synchronized where applicable.
- Inspect formatting-only churn, line-ending changes, permission changes, and
  unrelated edits that obscure review.

## Correctness and compatibility

- Walk normal, boundary, empty, malformed, missing, duplicated, reordered, and
  maximum-size inputs through the changed behavior.
- Check initialization, success, failure, retry, cancellation, interruption,
  partial completion, restart, rollback, and cleanup paths.
- Look for stale state, wrong defaults, off-by-one errors, lossy conversions,
  unchecked parsing, time/date assumptions, overflow, truncation, and encoding
  or Unicode problems.
- Validate exception/error propagation, exit status, diagnostics, and whether
  failure leaves recoverable state.
- Check backward/forward compatibility and mixed-version or migrated state when
  the contract crosses versions or processes.
- Exercise absent, fresh-canonical, legacy-only, planned-migration, and completed
  migration states. Freeze the legacy bytes, parsed source coordinates, and
  creator/push identity separately from mutable current slots. Persist planned
  intent in a no-clobber sidecar before canonical-ledger creation, then
  atomically/durably compare-and-swap exact planned bytes to complete through a
  pre-recorded no-clobber same-directory temp and parent fsync. An interruption
  must leave a valid planned or complete marker; test before/during/after the
  transition and safe exact-temp resume. Unrecorded debris or concurrent change
  stops. A
  completed migration
  requires its legacy source, canonical ledger, and sidecar, and disappearance
  of any member stops. Compare the legacy source only to its immutable snapshot
  while comparing current slots to live state; accept ordinary descendant head
  advancement but not rewritten history. Exercise repeated resume, source or
  canonical loss, partial multi-owner sets, and digest/coordinate mismatch.
- Before any PR-mode claim or artifact mutation, boundedly inventory and parse
  all regular no-follow current or legacy issue-mode ledgers and migration
  sidecars under the exact ignored review root.
  Block any selected issue, PR, repository/head-branch, or worktree intersection
  even without native linkage or a local worktree; leave true nonmatches
  untouched and read-only. Exercise canonical-only ownership, the same explicit
  issue with different artifacts, a missing migration member, descendant-head
  overlap, and unrelated canonical reports. An incomplete or unsafe inventory
  stops.
- Confirm idempotency where commands or external mutations may be retried.
- Inspect platform-sensitive paths, quoting, case sensitivity, separators,
  symlinks, permissions, terminals, shells, and line endings.
- Check regressions in unchanged callers, consumers, defaults, and workflows.

## Architecture and maintainability

- Verify boundaries follow `components.json`, nearest `AGENTS.md`, README,
  architecture, and specialist-skill ownership.
- Prefer existing abstractions and one authoritative source of truth; flag
  parallel state or policy definitions that can drift.
- Check names reveal intent, functions and modules have focused responsibilities,
  and invariants are expressed near enforcement.
- Inspect API shape, visibility, dependency direction, coupling, test seams, and
  future replacement/classic migration impact.
- Identify magic values, fragile ordering, implicit global state, hidden side
  effects, and behavior that depends on undocumented environment state.
- Check comments and documentation explain non-obvious reasons rather than
  restating code, and remove stale or contradictory guidance.
- Confirm generated and authored files are distinguished and reproducible.

## Reuse, duplication, and cleanup

- Search the complete change for repeated branches, messages, literals, parsing,
  validation, serialization, setup, teardown, and test fixtures.
- Search the repository and owning dependencies for existing helpers that can
  replace new code without weakening clarity or contracts.
- Consider standard-library facilities before custom implementations.
- Consider a new dependency only when its maintained, licensed, supply-chain,
  binary-size, build-time, and long-term ownership costs are justified.
- Flag wrappers that only rename an existing operation, premature generic
  abstractions, dead branches, unused imports, unreachable code, stale TODOs,
  obsolete compatibility paths, and redundant comments/tests.
- Look for opportunities to delete superseded code, data, docs, flags, fixtures,
  and configuration in the same safe scope.
- Prefer the smallest clear implementation; do not deduplicate semantically
  different behavior merely because text looks similar.

## Tests and verification

- Map tests to each requirement, finding, branch, failure mode, and regression
  risk. Check assertions prove behavior rather than only execution.
- Review positive, negative, boundary, retry, interruption, migration, and
  compatibility coverage as applicable.
- Inspect fixture realism, isolation, determinism, cleanup, naming, permissions,
  and accidental reliance on network, clock, ordering, or developer state.
- Confirm mocks stop at the correct boundary and do not reproduce the
  implementation under test.
- Check error messages, exit codes, stable JSON/schema fields, and side effects.
- Run focused tests first, then repository-required aggregate validation.
- Inspect coverage changes and meaningful untested lines; do not optimize for a
  percentage while leaving risk untested.
- Make manual verification copy-pasteable with exact prerequisites, actions,
  expected results, repeat behavior, bounded logs, shutdown, and safe cleanup.
- Confirm forward tests cannot mutate live GitHub, shared state, or unrelated
  worktrees and cover every scenario required by the selected issue or PR.

## Lifecycle, concurrency, and integrity

- Identify shared state, ownership boundaries, locks, transactions, atomic
  writes, temporary files, caches, and cleanup responsibility.
- Check races between discovery and mutation, duplicate invocation, concurrent
  readers/writers, process death, signals, timeouts, and retries.
- Verify lock ordering, acquisition/release on all paths, stale-lock handling,
  and that diagnostics do not bypass synchronization.
- Check partial writes, rename/replace semantics, fsync expectations, corruption
  detection, rollback, and preservation of recoverable inputs.
- Confirm cleanup targets exact proven ownership and fails closed on ambiguity.
- Check external mutations are ordered, idempotent where possible, permission-
  scoped, and observable without leaking secrets.
- Confirm a complete ignored coordinate ledger is pre-recorded before branch,
  worktree, or PR mutation, with separate artifact slots whose intent/result is
  persisted around every mutation. Exercise interruption before, during, and
  after each creation: planned/absent creates after rechecks, planned/one exact
  match binds, created-or-adopted/exact reuses, and every duplicate, mismatch,
  incomplete coordinate, or disappeared recorded artifact stops and preserves
  work. A bound issue-mode branch with no worktree must attach via the exact
  existing-branch path rather than recreate the branch.
- Confirm PR-only delivery does not mutate incidental linked issues or Project
  items, and that any issue claim came from an explicitly supplied, verified
  issue coordinate.

## Scale and performance

- Estimate behavior at realistic and worst-supported counts, sizes, depths, and
  concurrency; identify algorithmic complexity and multiplicative scans.
- Check repeated filesystem traversal, subprocess startup, Git/GitHub calls,
  network round trips, serialization, regex backtracking, and unbounded output.
- Inspect memory retention, whole-file loading, copies, buffers, caches, and
  large-object lifetimes.
- Check CPU hot paths, blocking I/O, batching, pagination, timeouts, rate limits,
  backoff, and cancellation.
- Verify caches have correct keys, invalidation, ownership, bounds, and
  concurrency behavior; avoid caching mutable or credential-bearing data.
- Assess build/test/runtime duration, incremental-build behavior, artifact size,
  startup latency, and operator feedback during long work.
- Require measurement for performance claims; avoid speculative complexity that
  makes ordinary paths less clear.

## Safety, security, and supply chain

- Trace trust boundaries and validate untrusted paths, arguments, URLs, branch
  names, Markdown, JSON/YAML, archive contents, protocol data, and subprocess
  inputs at the correct boundary.
- Check command injection, shell expansion, path traversal, symlink races,
  unsafe temporary files, confused-deputy behavior, and overly broad deletion.
- Verify authentication versus authorization, least-privilege permissions,
  repository/project identity, target scoping, and TOCTOU exposure.
- Search for secrets, tokens, credentials, personal data, sensitive logs,
  reports, fixtures, command arguments, comments, and generated artifacts.
- Check error paths and observability redact sensitive values without hiding
  actionable context.
- Review dependency provenance, licenses, pins, hashes, update policy,
  transitive risk, install scripts, Actions/images, and inventory changes.
- Check imported code/assets have complete provenance and compatible licensing.
- Consider denial of service through size, count, recursion, expensive parsing,
  retries, API use, disk consumption, and log volume.
- Ensure reports and PR comments do not publish unnecessarily actionable
  vulnerability detail.

## Operations and human experience

- Check diagnostics state what failed, where, why, and the safe next action.
- Verify structured output remains stable and human output stays concise,
  bounded, correctly ordered, and automation-safe.
- Inspect logging, metrics, traces, progress, dry-run/plan output, and audit
  evidence needed to operate or recover the feature.
- Check installation, discoverability, help text, naming, examples, and defaults.
- Review accessibility, localization, keyboard/screen-reader behavior, display
  assumptions, and color-only communication when relevant.
- Confirm docs match actual current commands and capability boundaries; never
  present future or Classic fallback commands as working replacement behavior.
- Verify handoff names exact local/GitHub artifacts and all created, reused, and
  deliberately preserved resources.

## Exit audit

- Confirm every finding is fixed and validated or has a concrete evidenced
  out-of-scope disposition; no status is stale.
- Re-read the raw selected issue and/or PR requirements and acceptance criteria
  against final HEAD.
- Refetch every selected or delivery-created PR and target/head ref. Require the
  live repositories, branches, base/head SHAs, and recomputed merge bases to
  match the reviewed coordinates; any drift restarts the affected complete-diff
  review, validation, and check cycle.
- Re-run a complete base-to-head review after the last fix; require zero known
  actionable findings and confirm no prior finding reopened.
- Re-run required validation and verify it used final committed HEAD.
- Recheck PR diff, commits, rendered body, comments/threads, mergeability,
  draft/ready state, and all expected checks at the same head SHA.
- Explain skipped/neutral checks; block on expected missing, failed, or cancelled
  checks and on required human approval.
- Mark a draft ready only after stable coordinates, the zero-finding review,
  final-head validation, every expected pre-readiness check, and determinate
  conflict-free mergeability with no non-human blocker other than draft state.
  Leave an already-ready PR ready; requery mergeability and wait for checks
  triggered by transition. Unknown, conflicting, or otherwise blocked states
  stop; missing human approval blocks merging rather than the ready transition.
- Confirm selected issues, if any, remain open; every PR remains unmerged; no
  self-approval/force-push/destructive reset/cleanup apply occurred; and
  worktrees/reports remain available.
- Produce the exact capability-aware verification and cleanup handoff, with
  blockers explicitly listed or `none`.
