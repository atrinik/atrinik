# Review and merge native PR stacks

Use this contract only for GitHub native PR stacks. A review request is
read-only. Merge authority exists only when the user explicitly identifies the
repository, native PR stack or top pull request, entire PR stack or through-PR
portion, and any human or issue-closing gate. Requests to finish, deliver, make
ready, or review never authorize a merge.

Merge-ready handoffs from issue or program delivery are inputs to this phase,
never inherited merge authority.

Native PR stacks are same-repository linear dependency chains. Keep
cross-repository or release-line programs under `atrinik-program-delivery`, and
keep independent pull requests independent.

## Establish the exact native PR stack

Use authenticated `gh` through the environment's required access path. Send
`X-GitHub-Api-Version: 2026-03-10`, or a documented supported successor, on
every Stacks or asynchronous-merge REST request. Never print authentication
material.

1. Normalize `OWNER/REPOSITORY` and either the native PR-stack number or top pull
   request. Verify repository identity, visibility, default branch, merge
   policy, rulesets, and merge-queue state.
2. If given a pull request, query
   `GET /repos/{owner}/{repo}/stacks?pull_request={number}` and require exactly
   one result. Query
   `GET /repos/{owner}/{repo}/stacks/{stack_number}` for the canonical stack
   object. Never infer membership from branch names, PR prose, issue links,
   timing, or local metadata.
3. Query every member PR. Record PR-stack ID/number, trunk, ordered position and
   size, PR URL/number, author, base and head repositories/branches/SHAs, state,
   draft status, reviews, unresolved conversations, checks, mergeability,
   linked issues, and any known asynchronous-merge UUID and its live result.
4. Reject absent or duplicate positions; size/membership disagreement; an
   unexpected trunk; a fork or repository mismatch; a closed-but-unmerged
   selected layer; and any ambiguous scope. Position 1 must target the trunk.
   Every later base branch and SHA must equal the preceding head at the reviewed
   coordinates.

Define the selected portion as every position through the named top PR. An
entire PR stack selection ends at the highest position. It may begin with a
contiguous already-merged lower prefix, but every later selected layer must be
open and form one contiguous active suffix through the named top PR. Reverify
each prefix member's `merged_at`, resulting squash SHA, target-branch ancestry,
dependency order, and contribution to the cumulative tree; reject a merely
closed, reordered, missing, or inconsistent prefix. The active suffix is the
mutation scope, and the endpoint must not newly merge anything outside it.

Read-only discovery can query a recorded asynchronous-merge UUID but cannot
list unknown requests; the first authorized SHA-guarded submission detects one
as `409` and must adopt its matching UUID rather than create another request.

For review of a historical fixture, closed layers are allowed only when no
mutation is proposed and the recorded merge results are part of the evidence.

## Freeze and review both views

Before reviewing, freeze the repository and stack numbers, selected portion,
trunk SHA, ordered base/head branches and SHAs, merge bases, changed paths, and
the resulting cumulative tree. For review-only work, keep this evidence in
memory and the response; do not create or update any report or other file.
Only a separately write-authorized delivery or fix workflow may persist it in
that workflow's existing ignored report or a new ignored review report.

- Review each layer's exact parent-to-head incremental diff against that PR's
  issue, requirements, tests, generated consumers, and closing behavior.
- Review the trunk-to-selected-top cumulative diff and resulting tree for
  integration defects, duplicated or contradictory changes, ordering
  assumptions, schemas, ledgers, release inputs, and issue closure.
- Apply the issue-delivery
  [deep-review checklist](../../atrinik-issue-delivery/references/deep-review-checklist.md)
  to implementation layers and the program-delivery
  [integration checklist](../../atrinik-program-delivery/references/program-review-checklist.md)
  to the cumulative result. For non-trivial work, use independent
  fresh-context passes from raw issues and diffs.
- Report findings without mutation for review-only requests. When separately
  authorized to fix, fix every actionable finding through its owning delivery
  workflow, validate, and repeat both incremental and cumulative review at the
  final heads.

Any push, rebase, retarget, membership change, lower-layer merge, or changed
trunk/base invalidates the affected evidence and every layer above it. Refresh
coordinates, diffs, reviews, validation, checks, and policy before proceeding.

## Require exact merge authority and current preflight

Immediately before mutation, re-query the canonical stack object and every
member. Compare them field-for-field with the reviewed snapshot and require:

- the selected repository, stack, portion, trunk, order, bases, and heads are
  unchanged;
- the verified merged prefix is unchanged, and every PR in the active suffix is
  open, non-draft, ready, mergeable, and still at its reviewed head;
- every required and applicable check exists and passes for the current
  head/base combination;
- required human approvals exist, no self-approval is used, and every
  actionable review conversation is resolved;
- one unambiguous issue-closing path remains, with any requested human gate or
  closing order satisfied; and
- squash remains permitted and the target branch has no unexpected merge
  queue.

Fail closed on missing, pending, skipped-but-required, failed, cancelled,
stale, or ambiguous evidence. Never force-push, bypass or relax rules, enable
auto-merge, merge unselected upper layers, close issues manually, or apply
cleanup under this authority.

## Execute the guarded native atomic operation

Verify the adopted workspace coordinates with `gh --version` and
`gh stack --version`: GitHub CLI 2.97.0 and `github/gh-stack` v0.1.0. Do not
install or upgrade tooling incidentally. An absent, unpinned, or unexpected
version blocks mutation.

Use `gh stack` for native discovery and navigation only when its live output
matches the canonical REST snapshot. Use `gh stack merge` as the primary merge
interface only after a reviewed pinned version both guards the selected top
head SHA and exposes enough request/result state for bounded recovery. The
reviewed v0.1.0 command does not send the asynchronous API's `sha` field, so it
does not satisfy that mutation contract; do not invoke it to merge.

For the current pinned toolchain, submit the exact selected top PR through the
native asynchronous endpoint with authenticated `gh api` outside the sandbox:

```sh
gh api --method PUT \
  -H 'X-GitHub-Api-Version: 2026-03-10' \
  repos/OWNER/REPOSITORY/pulls/TOP_PR/merge-async \
  -f sha=REVIEWED_TOP_HEAD_SHA \
  -f merge_method=squash \
  -f merge_action=direct_merge
```

Persist the response before polling. A `pending` result must include a UUID,
`expected_head_sha` equal to the selected head, `merge_method: squash`, and
`merge_action: direct_merge`. Poll no faster than once per second with a fixed
upper bound, for example 180 attempts:

```sh
gh api \
  -H 'X-GitHub-Api-Version: 2026-03-10' \
  repos/OWNER/REPOSITORY/pulls/TOP_PR/merge-async/UUID
```

Treat `merged` and `failed` as terminal. `enqueued` is terminal for the request
but not proof of a merge; it is unexpected under the no-queue preflight and
requires a recovery handoff. A timeout, transport loss, malformed response,
or `409` is unknown state, never permission to submit another merge. Preserve
the UUID when available and query that request plus the stack, selected PRs,
and target branch. If a lost submission response left no UUID, refresh all
coordinates first; only an identical SHA-guarded request may be used to recover
the existing UUID from `409`, and its stored method, action, and expected head
must match before polling. Never emulate atomic merge with sequential
`gh pr merge`, the synchronous REST merge endpoint, or GraphQL merge mutations.

## Verify the result and preserve remaining work

After a terminal result, re-query the canonical stack, every member PR, target
branch, issue state, reviews/checks, and branch refs. Verify that:

- the previously merged prefix remains unchanged, exactly the active suffix
  merged, and no unselected upper layer merged;
- each resulting squash commit occurs on the target branch in dependency
  order, and the target contains the reviewed cumulative result;
- expected server-side head deletion is distinguished from an error; and
- issue closure matches the single declared closing path.

For a partial-stack merge, record GitHub's new bases, heads, retargeting, and
rebases for every remaining layer. Invalidate and repeat incremental and
cumulative review, validation, checks, and policy at those new coordinates.
On failure or inconsistent state, stop with the exact observed request, stack,
PR, and branch coordinates; never blindly resubmit.

Keep worktrees and ignored reports while any PR remains. Local branch,
worktree, profile, or report cleanup is a separate preview-first request.

## Read-only forward fixture

Completed `atrinik/classic` native PR stack 73 is the non-mutating fixture.
Inspect it with:

```sh
gh api \
  -H 'X-GitHub-Api-Version: 2026-03-10' \
  repos/atrinik/classic/stacks/73
```

Require trunk `main`, positions 1 and 2 for PRs 55 and 72, and the exact
base/head chain. Verify the resulting squash commits `70e7bdc` then `fa5042a`
on `main`, with the latter's parent equal to the former. Never call a merge
endpoint in a forward test.
