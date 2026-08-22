---
name: atrinik-issue-delivery
description: Deliver an Atrinik issue/PR to merge-ready handoff or manage its explicitly authorized post-merge ledger lifecycle. Explicit `$atrinik-issue-delivery` invocation only.
---

# Deliver an Atrinik issue or pull request

Choose exactly one type-explicit `ENTRY_MODE`: `issue` for new issue-first work
or `PR` for one existing pull request. Both stop before merge.

Invocation permits ordinary pushes, selected/delivery-created PR updates,
gated readiness, one coordinate-bound selected-PR comment, and issue-mode draft
creation/claim. PR mode claims only an explicit verified issue; issue comments
are read-only. It does not authorize force-pushes, issue
creation/closure, merges, policy bypass, destructive resets, cleanup
application, self-approval, retargeting, unrelated changes, or PR-discovered
issue mutation.
Create/resume a persistent goal only when explicitly asked.

Explicit `$atrinik-program-delivery` delegates only issue mode to ready live
children, never PR mode/unrelated PRs; do not create a nested leaf goal.

Load `atrinik-multi-repo-workspace`, `atrinik-github-governance`, applicable
implementation, and runtime/scenario skills only when needed. Follow the nearest
owner `AGENTS.md`; do not duplicate procedures.

## Select and verify the entry mode

1. Require `issue` or `PR` plus `owner/repository#number`, or equally
   unambiguous structured type. A bare `owner/repository#number` is ambiguous:
   stop before mutation. With both coordinates, require explicit PR mode and
   prove they describe the same work.
2. Using the requested GitHub path, verify identity, repository/remotes, live
   default/target branches, coordinates, linked work, and collisions without
   exposing credentials. Stop on repository mismatch, ambiguous/competing work,
   unsafe branch/worktree/report coordinates, or a changed/unavailable head.
   Before an active-PR decision or delivery-owned mutation, run the bundled
   ledger helper's read-only bounded no-follow `inventory` of every issue/PR
   sidecar and recognized migration/stage; inspect both human-report paths.
   Only the exact same delivery may resume; unsafe, incomplete, duplicate, or
   issue/PR/repository-head/worktree overlap blocks.
3. Issue mode requires an open issue; inspect assignees, labels, comments,
   hierarchy, Project, links, and candidate PRs. If active PRs already own
   the work, resume only when each is recorded by this exact schema-v1
   issue-mode ledger or uniquely matches its pre-recorded pending PR slot, and
   step 6 matches. Complete helper migration or binding before mutation;
   otherwise stop at the exact PR and require type-explicit PR mode.
4. In PR mode, require an existing open, unmerged PR. Record its repository,
   author/head repository, target/head branches, base/head SHAs, merge base,
   draft state, links/closing references, body, reviews, conversations, checks,
   and mergeability. Require a same-repository head and ordinary push authority.
   A fork, foreign/protected head, unavailable ref, or failed authority proof is
   a blocker/read-only surface, never branch-takeover authority.
5. In PR mode, record zero or more incidental linked issues as read-only
   traceability. Without one explicitly supplied and verified issue, make zero
   issue/Project mutations and add no closing reference. With one, require it
   open and prove its exact existing association; only it may enter authority,
   claim, or Project handling. Other linked issues remain incidental and
   read-only; their presence alone is not ambiguity. Stop on a contradictory or
   unproved relationship. Preserve live closing scope; a verified non-closing
   association may keep `closing_scope=[]`.
6. Resume only when `ENTRY_MODE`, selected issue and/or complete recorded or
   pre-recorded PR set, repositories, bases, heads, branches, worktrees, and
   ledger identity agree exactly with the prior delivery. The narrow helper
   migration below may establish the new ledger identity. This exact
   recorded-delivery match is the sole exception to the fresh-delivery
   no-active-PR rule.

## Resolve ownership and isolate work

- Run `./atrinik manifest validate` and `./atrinik status --json`. Resolve
  `components.json`, read every physical owner's guide, and inspect primary and
  registered worktrees. Preserve dirty, detached, locked, active, referenced,
  foreign, or uncertain work.
- Before any delivery-owned mutation, prove Git ignores the review root and use
  one human-report identity from [the report asset](assets/deep-review-report.md):

  ```text
  <wrapper-root>/build/reviews/<owner>-<repository>-issue-<number>.md
  <wrapper-root>/build/reviews/<owner>-<repository>-pr-<number>.md
  ```

  Markdown is non-authoritative. Follow [the ledger protocol](references/delivery-ledger.md)
  exactly; strict schema-v1 `<report>.ledger.json`, managed only through
  `python3 scripts/delivery_ledger.py`, is the sole ownership/recovery record.
  Safely initialize/inventory the ignored root, then create or explicitly
  migrate the prepared document before any claim, Project, remote-write,
  artifact, scope, or resource mutation. CAS every result/live refresh through
  the helper; never hand-roll state, locks, markers, or recovery.

  Planned/absent creates after collision rechecks; planned/exact binds;
  created-or-adopted/exact reuses. Missing, duplicate, mismatched, dirty, or
  unsafe artifacts stop. Attach a bound branch with absent worktree through
  manifest-owner `--existing` or wrapper-self non-`-b`; never recreate it.

  Migrate only through the helper. An old
  `<owner>-<repository>-<issue>.md` is issue-mode-only. Pre-schema migration
  needs a durable authenticated goal predating/continuously owning every exact
  artifact, or new explicit recovery authority naming them. Other evidence only
  corroborates. Preserve required source/snapshot/marker members; any loss or
  mismatch stops. Generic CAS never changes target coordinates; use the live
  target-refresh CAS for a proven descendant base/head and recomputed merge-base
  refresh.

### Claim only explicitly authorized issues

- Only after the authoritative ledger above exists, in issue mode or PR mode
  with one explicitly supplied and verified issue, assign `zoeyrose`
  idempotently. For Atrinik, add the issue to **Atrinik work** when needed and
  set its existing Status to **In progress** per
  `github-settings/config/planning.json`; never invent an `in-progress` label.
- In PR mode without one explicitly supplied and verified issue, make zero
  issue/Project mutations. Such a PR is complete; never create a placeholder or
  broaden its closing scope.
- For content, author only on `main` and validate every affected target and
  consumer. The retired `1.x` line, artifacts, and local paths are immutable
  migration/release evidence; never publish a new `1.x` PR. A content `main`
  PR may close an explicitly selected issue when merged, but leave every issue
  open for maintainers.

- In issue mode, set `TARGET_BRANCH` to the explicit or manifest/live target,
  fetch it, record exact `BASE_SHA`, and choose safe lowercase
  mode-coordinate-derived names. In fresh manifest-owned work, use one scope
  invocation per physical repository. Before mutation, require exactly one
  physical checkout; extra rows fail. Ledger-plan its request with exact root
  identities and scope/branch/worktree slots with the path deferred, then run:

  ```sh
  ./atrinik scope create COMPONENT --name SCOPE --from PROFILE \
    --label CHECKOUT=LABEL --branch CHECKOUT=TYPE/TOPIC \
    --start-point CHECKOUT=BASE_SHA --temporary-state --json
  ./atrinik scope show SCOPE --json
  ```

  For Classic, use the logical component as the positional selector and the
  physical checkout for every override key: `scope create classic-client ...
  --label classic=... --branch classic=... --start-point classic=...`. The
  wrapper records `requested_components` separately from its checkout-wide
  logical component rows; never substitute `classic-client=` for the `classic`
  override key. Scope creation derives the immutable `scope-SCOPE` profile and
  topology names; a supplied `--topology` must equal `scope-SCOPE` and is
  rejected before publication otherwise. Bind only the exact returned scope
  through the ledger helper.

  Feed raw `scope show`/list JSON to `scope-observe`; call `scope-bind-cas` with
  a fresh inspect tuple. It pins/reproves before CAS. Partial, released,
  referenced, cross-checkout, or mismatched evidence stops. Generic `cas`
  cannot bind it; `scope-bind` only diagnoses. An exact live pre-bind topology
  mismatch from an older helper may use `recover-prebind-scope` with retained
  scope-show, worktree-list, safety, and explicit-recovery evidence; it changes
  only the proven topology request through CAS and preserves the predecessor.
- In PR mode, set `TARGET_BRANCH`, `BASE_SHA`, `HEAD_BRANCH`, `HEAD_SHA`, and
  `MERGE_BASE` from the live PR. Fetch and verify the exact base/head refs
  without rewriting published history. Create the local head branch at the
  verified `HEAD_SHA` only when absent; if present, require its tip and remote
  ownership to match and require it not to be checked out in a conflicting
  worktree. Never recreate, rebase, retarget, or force-update it.
- `scope create` requires a new branch and cannot select wrapper-self. Otherwise
  use primitives: manifest owners attach with `./atrinik worktree create
  COMPONENT LABEL --branch HEAD_BRANCH --existing`; wrapper-self attaches
  existing heads without `-b`, or uses exact ledger-planned `git worktree add -b
  TYPE/TOPIC ... BASE_SHA` for a fresh issue.
  Every fresh planned worktree has a deferred request: pre-record root identities,
  retain manifest-create stdout (raw Git has no canonical output) and the wrapper
  list, run `worktree-observe`, then atomic `worktree-bind-cas`. Generic `cas`
  cannot bind it; `worktree-bind` only diagnoses. Never reconstruct paths.
  Verify initial `HEAD` equals the mode's recorded SHA.
- Reuse only when repository, branch, head, ownership, mode, and ledger
  coordinates match. Never resume or edit a dirty, detached, locked, active,
  foreign, or uncertain worktree. A ledger-recorded profile/reference requires
  a matching selector and stopped, live-verified holders; refresh via CAS. In
  proof, an incomplete current-stack profile is inert only if present selectors
  validate and miss the candidate; otherwise stop. Retain a complete unreleased
  scope only while its external
  generation, raw digest, identities, absent release journal, and safety match.
  Released scopes and other references block. Give each physical repository
  its own worktree, branch, commits, validation, and PR in its owner. In PR mode,
  this does not authorize a companion PR; another repository needs separate
  type-explicit delivery authority.

## Implement and publish or update PRs

Implement requirements and owner tests/contracts. Commit/validate coherent
Conventional checkpoints without rewriting published history. Reprove the
helper's effective `origin` route, then run `git push origin HEAD_BRANCH` in the
same scrubbed selector environment; reject HTTP, conceal URLs/credentials, and
retain credential helpers.

In issue mode, open one coherent draft per affected physical repository against
its `TARGET_BRANCH` (for example, `--base TARGET_BRANCH`) and verify every
returned base. Keep one unambiguous canonical issue-closing path. In PR mode,
update only the selected PR without changing its base, head, draft state, or
valid linkage as a shortcut. Never convert an already-ready PR to draft merely
to replay this workflow. Preserve valid existing closing references and never
invent or broaden them.

Use `type(optional-scope): concise description` for the title by default; add
`!` only when a reviewer explicitly requests a breaking change. PR bodies must
be substantive, rendered GitHub-Flavored Markdown
with actual line breaks, never literal `\n` separators. Its minimum concise
sections explain what changed and why (`Summary`), relevant implementation or
behavior details (`Implementation / behavior`), validation and results
(`Validation`), and limitations or follow-up when applicable
(`Limitations / follow-up`). An issue-closing line alone is insufficient, while
issue and pull-request reference syntax remains supported. Feed multi-section
bodies by file/stdin. After creating or editing a pull request, inspect
GitHub's rendered `bodyHTML`/`body_html`, not only the raw body; verify
headings, lists, inline code, issue-closing references, and validation sections.
Follow the ledger reference's coordinate-bound remote-write protocol:
ledger-retain each exact initial PR/body/comment payload.

For an existing contributor-owned PR, preserve contributor-authored text
byte-for-byte. An agent may add or replace only the separately delivery-owned
section when the helper ledger and ownership rules authorize it;
copied live markers never grant ownership. Refetch and verify rendered GFM and
linkage after the helper-bound update. A fresh contributor body is wholly
read-only outside that helper-planned terminal delivery section; preserve every
outside byte.

Refetch bytes/timestamps and every marker match; CAS intent, refetch, then use
only the helper-returned payload and bind its exact result. Cancel separately
only after exact non-application proof and before drift. Zero comment matches
permits one never-started post; reuse one exact actor-authored match.
Wrong-author, malformed, duplicate, unexpected, or
uncertain state stops without another post. Verify rendered GFM/linkage.

## Review and fix to the exit condition

Keep the helper ledger and separate human report current. Never commit or
publish either or include credentials, confidential data, or unnecessary
vulnerability detail.

Read [the checklist](references/deep-review-checklist.md) in full. Review the
complete current base-to-head diff against the selected issue and/or PR
requirements. For non-trivial changes, give independent fresh-context reviewers
the raw selected requirements and diff, not prior conclusions. Record stable
finding IDs, evidence, resolution, status, fixing commit, and validation.

Fix every actionable finding, add useful tests, update the report, commit/push a
checkpoint, rerun validation, then conduct a fresh whole-diff review. Repeat
until a complete post-fix pass finds zero known actionable findings and none has
reopened. Evidence any out-of-scope deferral. Keep GitHub feedback to one concise
updated summary unless an inline note materially helps.

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

Reuse rather than recreate exact compatible profiles, stopped topologies,
wrapper-owned state/data, or stopped/unlocked delivery scenarios. Prefer
generation-owned temporary state; use named/default only for persistence or an
existing account. Match immutable repository, branch, checkout, source,
provider, commit, profile, generation, and owner to final HEAD; incomplete
records are inert. Ordinary state must be test-safe with an appropriate account;
never reset it.

Let wrapper metadata choose generated paths/builds; never reconstruct, copy,
edit, delete, or select internals. Never stop unrelated topologies, reset
shared/default/external state, handcraft saves, or reuse/publish credentials.
A disposable scenario password may appear only in local auto-login
argv/client/Codex logs. Create a unique `basic-player` scenario only when
interactive verification needs it and no exact delivery scenario exists; add a tested
server-owned preset only when ordinary play is impractical.

For Classic, compose the runtime/scenario skills. Give exact profile, build,
scenario, automatic-login topology, bounded logs, action/result, repeat,
shutdown, cleanup, and state-policy commands. Initial `down` applies only to
that topology; reset only delivery-owned data. State display/login
prerequisites.

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

Keep issues open, PRs unmerged, and evidence preserved. A separate post-merge
request must follow the ledger reference's terminal lifecycle; delivery grants
no cleanup authority and helper lifecycle commands never remove resources.
It must never trigger implicitly.
