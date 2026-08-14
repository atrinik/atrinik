---
name: atrinik-issue-delivery
description: Deliver an open Atrinik issue or existing PR through isolated work, review/fix cycles, checks, and merge-ready handoff. Use only as explicitly invoked `$atrinik-issue-delivery`; never trigger implicitly.
---

# Deliver an Atrinik issue or pull request

Choose exactly one type-explicit `ENTRY_MODE`: `issue` for a new issue-first
delivery or `PR` for adoption of one existing pull request. Both converge on
the same implementation, complete-diff review, validation, readiness, and
stop-before-merge contract.

Explicit invocation authorizes ordinary branch pushes, updates to selected or
delivery-created PRs, draft-to-ready transition after exit conditions, and
brief delivery comments. Issue mode also authorizes draft PR creation and the
issue claim below. PR mode authorizes that claim only when the invocation
explicitly names both the PR and its associated issue and preflight verifies
their relationship.
It does not authorize force-pushes, issue creation or closure, merges, policy
bypass, destructive resets, cleanup application, self-approval, retargeting,
unrelated external changes, or mutation of issues found only through a PR.
Create or resume a persistent goal only when explicitly asked.

For an ordered master issue with merge-dependent leaves, use the explicitly
invoked `$atrinik-program-delivery` orchestrator instead of stretching this
single-delivery contract across the program. An explicit program-delivery
invocation delegates this contract only in issue mode to each dependency-ready
child identified from that master's live graph; it never delegates PR mode or
authority over an unrelated existing PR. Do not create a nested leaf goal.

Load `atrinik-multi-repo-workspace`, `atrinik-github-governance`, and only the
applicable implementation skill. Add `atrinik-server-runtime` and
`atrinik-test-scenario` only when verification needs them. Follow those skills
and each physical owner's nearest `AGENTS.md`; do not duplicate their runtime or
scenario procedures.

## Select and verify the entry mode

1. Require the invocation to say `issue` or `PR` with its
   `owner/repository#number`, or prove the type from equally unambiguous
   structured context. A bare `owner/repository#number` is ambiguous: stop
   before mutation instead of querying both types and guessing. When both an
   issue and PR are supplied, keep PR mode explicit and verify they describe
   the same work before mutation.
2. With the requested GitHub access path, verify identity, repository/remotes,
   the live default and target branches, coordinate state, linked work, and
   collisions. Never expose credentials. Stop for repository mismatch,
   ambiguous implementation, competing work, unsafe branch/worktree/report
   coordinates, or a changed or unavailable selected head.
3. In issue mode, require an existing open issue. Inspect assignees, labels,
   comments, native hierarchy, Project item, linked work, and all candidate
   PRs. If active PRs already own the work, resume only when every one is either
   recorded as created by this same issue-mode delivery or uniquely matches a
   pre-recorded pending PR slot, and every coordinate in step 6 matches. Bind
   any exact pending-slot match in the ledger before further mutation. Otherwise
   stop with the exact active PR coordinates and require a type-explicit PR-mode
   invocation rather than creating competing work.
4. In PR mode, require an existing open, unmerged PR. Record its repository,
   author, head repository, target and head branches, base and head SHAs, merge
   base, draft state, linked issues, body and closing references, reviews,
   conversations, checks, and mergeability. Require a same-repository head and
   verify ordinary push authority before edits. Treat a fork, foreign or
   protected head, unavailable ref, or failed push-authority proof as a blocker
   or read-only review surface, never as authority to take over the branch.
5. In PR-only mode, record zero or more incidental linked issues as read-only
   traceability. Do not require, synthesize, assign, move, close, comment on, or
   add a closing reference for any issue. When one issue is explicitly supplied
   with the PR, require it to be open and verify that exact issue has an
   unambiguous existing association. Other linked issues remain incidental and
   read-only; their presence alone is not ambiguity. Fail closed when the exact
   supplied relationship is contradictory or cannot be proved, and preserve the
   PR's existing closing scope.
6. Resume only when `ENTRY_MODE`, selected issue and/or complete recorded or
   pre-recorded PR set, repositories, bases, heads, branches, worktrees, and
   report identity agree exactly with the prior delivery. In issue mode, this
   exact recorded-delivery match is the sole exception to the fresh-delivery
   no-active-PR rule.

## Claim only explicitly authorized issues

- In issue mode, or PR mode with one explicitly supplied and verified issue,
  assign `zoeyrose` idempotently. For Atrinik, add the issue to **Atrinik work**
  when needed and set its existing Status to **In progress** per
  `github-settings/config/planning.json`; never invent an `in-progress` label.
- In PR-only mode, make no linked-issue or Project mutation. A PR without an
  issue is a complete valid input; never create a placeholder issue or broaden
  which issues the PR closes.
- For content, author only on `main` and validate every affected target and
  consumer. The retired `1.x` line, artifacts, and local paths are immutable
  migration/release evidence; never publish a new `1.x` PR. A content `main`
  PR may close an explicitly selected issue when merged, but leave every issue
  open for maintainers.

## Resolve ownership and isolate work

- Run `./atrinik manifest validate` and `./atrinik status --json`. Resolve
  `components.json`, read every physical owner's guide, and inspect primary and
  registered worktrees. Preserve dirty, detached, locked, active, referenced,
  foreign, or uncertain work.
- Before any branch, worktree, or PR mutation, prove Git ignores the mode-stable
  report path and instantiate [the report asset](assets/deep-review-report.md)
  as a durable coordinate ledger at one of:

  ```text
  <wrapper-root>/build/reviews/<owner>-<repository>-issue-<number>.md
  <wrapper-root>/build/reviews/<owner>-<repository>-pr-<number>.md
  ```

  Pre-record the complete planned physical target set: entry mode, selected
  issue/PR or issue-mode pending PR slots, repositories, target/base and
  existing/planned heads, branches, worktree labels/paths, authenticated creator
  or push identity, and closing scope. Durably refresh the ledger immediately
  after each artifact creation and before the next mutation. After interruption,
  bind or reuse only one exact live/local artifact matching each planned slot;
  stop on an absent, duplicate, incomplete, or mismatched coordinate.
- In issue mode, set `TARGET_BRANCH` to an explicit valid target or otherwise
  the manifest/live target. Fetch it, record the exact commit as `BASE_SHA`, and
  choose safe lowercase mode-coordinate-derived branch and label names. Reject
  branch/path collisions. For manifest owners run `./atrinik worktree create
  COMPONENT LABEL --branch TYPE/TOPIC --from BASE_SHA`; for wrapper-self run
  `git worktree add -b TYPE/TOPIC workspace/worktrees/atrinik/LABEL BASE_SHA`.
  Verify the new worktree's initial `HEAD` equals `BASE_SHA`.
- In PR mode, set `TARGET_BRANCH`, `BASE_SHA`, `HEAD_BRANCH`, `HEAD_SHA`, and
  `MERGE_BASE` from the live PR. Fetch and verify the exact base/head refs
  without rewriting published history. Create the local head branch at the
  verified `HEAD_SHA` only when absent; if present, require its tip and remote
  ownership to match and require it not to be checked out in a conflicting
  worktree. Never recreate, rebase, retarget, or force-update it.
- After the PR head branch is safely local, manifest owners use `./atrinik
  worktree create COMPONENT LABEL --branch HEAD_BRANCH --existing`. For
  wrapper-self, run `git worktree add workspace/worktrees/atrinik/LABEL
  HEAD_BRANCH` from the wrapper root. Verify the adopted worktree's initial
  `HEAD` equals the recorded `HEAD_SHA`, not `BASE_SHA`.
- Reuse a registered worktree only when its immutable repository, branch, head,
  ownership, mode, and report coordinates match. Even with matching
  coordinates, never resume or edit a dirty, detached, locked, active,
  referenced, foreign, or uncertain worktree; stop and preserve it. Give each
  physical repository its own worktree, branch, commits, validation, and PR;
  keep implementation in its physical owner. In PR mode, this ownership rule
  does not authorize a companion PR: stop and require an additional
  type-explicit delivery coordinate and authority for another repository.

## Implement and publish or update PRs

Implement requirements with owner tests/contracts. Commit coherent Conventional
Commit checkpoints, preserve published history, validate, and push normally.

In issue mode, open one coherent draft per affected physical repository against
its `TARGET_BRANCH` (for example, `--base TARGET_BRANCH`) and verify every
returned base. Keep one unambiguous canonical issue-closing path. In PR mode,
update only the selected PR without changing its base, head, draft state, or
valid linkage as a shortcut. Never convert an already-ready PR to draft merely
to replay this workflow. Preserve valid existing closing references and never
invent or broaden them.

Use a Conventional Commit title and newline-preserving GitHub-Flavored Markdown
body for each PR. Record available issue linkage, exact base/head branches and
SHAs, worktree, commits, scope, validation, and verification. Inspect GitHub's
rendered bodies after each material edit and keep them current.

## Review and fix to the exit condition

Continue the durable coordinate ledger instantiated before artifact mutation,
completing its review sections against current reality. Never commit or publish
it, and exclude credentials, confidential data, and unnecessarily actionable
vulnerability detail.

Read [the checklist](references/deep-review-checklist.md) in full. Review the
complete current base-to-head diff against the selected issue and/or PR
requirements. For non-trivial changes, give independent fresh-context reviewers
the raw selected requirements and diff, not prior conclusions. Record stable
finding IDs, evidence, resolution, status, fixing commit, and validation.

Fix every actionable finding, add tests where useful, update the report, commit
and push a logical checkpoint, rerun validation, then conduct a fresh whole-diff
review. Repeat until a complete post-fix pass finds zero known actionable
findings and none has reopened. Evidence any out-of-scope deferral. Keep GitHub
feedback to one concise updated summary unless an inline note materially helps.

## Verify with compatible resources

Before provisioning, run the supported inventories as applicable:

```sh
./atrinik status --json
./atrinik worktree list --json
./atrinik scenario list --json
./atrinik state list --json
./atrinik ps --json
./atrinik profile show PROFILE --json
./atrinik topology show PROFILE --temporary-state --json
```

Reuse rather than recreate a compatible delivery profile, stopped topology,
wrapper-owned state and server/client data, or delivery-owned scenario. Prefer
a fresh generation-owned temporary state for automation; select named/default
state only when persistence or an existing account is required. Reuse only
resources whose immutable repository, branch, checkout, source, provider,
commit, profile, generation, and state-owner coordinates match final HEAD;
incomplete historical records are inert. Scenarios must also be dedicated to
the delivery, stopped, and unlocked. Ordinary state/data may be used only when
safe to mutate for the test and the reviewer already has an appropriate
account; never reset it.

Let wrapper metadata choose generated paths/builds; never reconstruct, copy,
edit, delete, or manually select internal generated paths. Never stop unrelated
active topologies, reset shared/default/external state, handcraft saves, reuse
unrelated credentials, or disclose passwords. Create a unique `basic-player`
scenario only when interactive verification needs it and no exact-compatible
delivery scenario exists; add a tested server-owned preset only when ordinary
play is impractical.

For Classic, compose the runtime/scenario skills and provide concrete
copy-pasteable profile, build, scenario, credentials-local-only, topology,
bounded-log, action/result, repeat, shutdown, and cleanup commands. State the
temporary, named, default, or scenario policy explicitly. Initial `down`
applies only to that exact running topology; reset only delivery-owned data.
State display/login prerequisites.

For replacement work, create or reuse a delivery profile selecting the final
worktree and inspect current capabilities. While integrated adapters are
absent, give owner-native validation plus supported profile/topology inspection,
identify boundaries #266, #269, and #270, and never substitute Classic. If
runtime is irrelevant, explain why and give exact applicable tests. Put the
same exact capability-aware recipe in a concise PR update and final handoff.

## Finish only on final HEAD

Refetch every selected or delivery-created PR and its exact target and head
refs. Recheck the complete diff, commits, mergeability, draft state, base and
head repositories, branches and SHAs, linkage, review threads/comments, and
expected checks; recompute every merge base. Any target/base/head or merge-base
drift invalidates the affected review, validation, and checks and restarts the
shared convergence loop at the new recorded coordinates.

Wait for all expected pre-readiness checks; required and applicable optional
checks must pass. Explain skipped/neutral checks and block on an expected
missing, failed, or cancelled check. Only after stable final coordinates, final
validation, a zero-finding review, all such checks pass, and live mergeability
is determinate and conflict-free with no non-human blocker other than draft
state may a draft be marked ready; leave an already-ready PR ready. Unknown or
conflicting mergeability and any other non-human blocker wait or block. Requery
after a ready transition, recheck mergeability, and wait for any expected checks
it triggers. Report required human approval as a blocker rather than claiming
literal merge eligibility. New actionable feedback restarts the fix,
validation, and whole-diff loop. Missing human approval blocks merging, not the
ready transition after the stated exit conditions pass.

Hand off the PR URLs and exact per-target bases, head repositories, branches,
SHAs, merge bases, commits, worktrees, review findings, validation/checks,
mergeability, runtime applicability, resources,
verification/repeat/shutdown/cleanup commands, and blockers or `none`. Include
issue URLs, claim state, and the canonical closing path only when issues
actually exist; do not fabricate placeholders.

Keep selected issues open, keep every PR unmerged, and preserve worktrees and
reports while the PRs are open. Cleanup requires a separate post-merge request
beginning with `./atrinik cleanup --dry-run --json`.
