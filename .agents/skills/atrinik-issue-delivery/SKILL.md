---
name: atrinik-issue-delivery
description: Deliver Atrinik issues through worktrees, draft PRs, deep reviews/fixes, passing checks, and merge-ready handoff without merging. Use only as explicitly invoked `$atrinik-issue-delivery`; never trigger implicitly.
---

# Deliver an Atrinik issue

Explicit invocation authorizes issue assignment, canonical Project status,
ordinary branch pushes, draft PR creation/updates, transition to ready after
exit conditions, and brief delivery comments. It does not authorize
force-pushes, issue closure, merges, policy bypass, destructive resets, cleanup
application, self-approval, or unrelated external changes. Track a persistent
goal only when explicitly asked.

For an ordered master issue with merge-dependent leaves, use the explicitly
invoked `$atrinik-program-delivery` orchestrator instead of stretching this
single-issue contract across the whole program.
An explicit program-delivery invocation delegates this contract to each
dependency-ready child identified from that master's live graph and counts as
explicit leaf authorization; do not create a separate goal for the leaf.

Load `atrinik-multi-repo-workspace`, `atrinik-github-governance`, and only the
applicable implementation skill. Add `atrinik-server-runtime` and
`atrinik-test-scenario` only when verification needs them. Follow those skills
and each physical owner's nearest `AGENTS.md`; do not duplicate their runtime or
scenario procedures.

## Preflight and claim

1. Normalize `owner/repository#number`. With required outside-sandbox access,
   use `gh` to verify identity, repository/remotes, live default/target branches,
   issue state, assignees, labels, comments, linked work, Project item, and
   existing PRs before mutation. Never expose credentials.
2. Stop for a closed issue, repository mismatch, unsafe collision, or ambiguous
   implementation. Resume only when issue, repository, base, branch, worktree,
   and PR coordinates agree.
3. Assign `zoeyrose` idempotently. For Atrinik, add the item to **Atrinik work**
   when needed and set its existing Status to **In progress** per
   `github-settings/config/planning.json`; never invent an `in-progress` label.
   Use a status label only where an authorized repository makes it canonical.
4. For content, author only on `main` and validate every affected target and
   consumer. The retired `1.x` line, artifacts, and local paths are immutable
   migration/release evidence; never publish a new `1.x` PR. A content `main`
   PR may close its issue when merged, but leave the issue open for maintainers.

## Resolve ownership and isolate work

- Run `./atrinik manifest validate` and `./atrinik status --json`. Resolve
  `components.json`, read every physical owner's guide, and inspect primary and
  registered worktrees. Preserve dirty, detached, locked, active, referenced,
  or uncertain work.
- Set `TARGET_BRANCH` to an explicit valid target or otherwise the manifest/live
  target. Fetch it and record the exact commit as `BASE_SHA`; never assume
  `main`. Reject branch/path collisions. Use safe lowercase issue-derived names.
- For manifest owners run `./atrinik worktree create COMPONENT LABEL --branch
  TYPE/TOPIC --from BASE_SHA`. For wrapper-self run `git worktree add -b
  TYPE/TOPIC workspace/worktrees/atrinik/LABEL BASE_SHA`.
- Verify each new worktree's initial `HEAD` equals `BASE_SHA` before mutation.
- Give each physical repository its own worktree, branch, commits, validation,
  and PR. Keep implementation, tests, packages, and releases in their owner.

## Implement and publish a draft

Implement requirements with owner tests/contracts. Commit coherent Conventional
Commit checkpoints; never rewrite published history without separate authority.
Validate, push normally, and open a coherent draft against `TARGET_BRANCH`
(for example, `--base TARGET_BRANCH`); verify the returned base.

Use a Conventional Commit title and newline-preserving GitHub-Flavored Markdown
body. Record issue linkage, base/head branches and SHAs, worktree, commits,
scope, validation, and verification. Inspect GitHub's rendered body after each
material edit and keep it current.

## Review and fix to the exit condition

Verify Git ignores
`<wrapper-root>/build/reviews/<owner>-<repository>-<issue>.md`, then instantiate
[the report asset](assets/deep-review-report.md) there. Never commit or publish
it, and exclude credentials, confidential data, and unnecessarily actionable
vulnerability detail.

Read [the checklist](references/deep-review-checklist.md) in full. Review the
complete current base-to-head diff. For non-trivial changes, give independent
fresh-context reviewers the raw issue and diff, not prior conclusions. Record
stable finding IDs, evidence, resolution, status, fixing commit, and validation.

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
./atrinik topology show PROFILE --state STATE --json
```

Reuse rather than recreate a compatible issue profile, stopped issue topology,
wrapper-owned state and server/client data, or issue-owned scenario. Reuse only
resources
whose immutable repository, branch, checkout, source, provider, commit, profile,
generation, and state-owner coordinates match final HEAD; incomplete historical
records are inert. Scenarios must also be dedicated to the issue, stopped, and
unlocked. Ordinary state/data may be used only when safe to mutate for the test
and the reviewer already has an appropriate account; never reset it.

Let wrapper metadata choose generated paths/builds; never reconstruct, copy,
edit, delete, or manually select internal generated paths. Never stop unrelated
active topologies, reset shared/default/external state, handcraft saves, reuse
unrelated credentials, or disclose passwords. Create a unique `basic-player`
scenario only when interactive verification needs it and no exact-compatible
issue scenario exists; add a tested server-owned preset only when ordinary play
is impractical.

For Classic, compose the runtime/scenario skills and provide concrete
copy-pasteable profile, build, scenario, credentials-local-only, topology,
bounded-log, action/result, repeat, shutdown, and cleanup commands. Initial
`down` applies only to that exact running topology; reset only issue-owned data.
State display/login prerequisites.

For replacement work, create or reuse an issue profile selecting the final
worktree and inspect current capabilities. While integrated adapters are absent,
give owner-native validation plus supported profile/topology inspection,
identify boundaries #266, #269, and #270, and never substitute Classic. If
runtime is irrelevant, explain why and give exact applicable tests.
Put the same exact capability-aware recipe in a concise PR update and the final
handoff.

## Finish only on final HEAD

Recheck the complete diff, commits, head SHA, mergeability, draft state, review
threads/comments, and expected checks. Mark ready only after final validation
and the zero-finding review. Wait for all expected checks; required and
applicable optional checks must pass. Explain skipped/neutral checks and block
on an expected missing, failed, or cancelled check. Report required human
approval as a blocker rather than claiming literal merge eligibility. New
actionable feedback restarts the fix, validation, and whole-diff loop.
Missing human approval blocks merging, not the ready transition after the stated
exit conditions pass.

Hand off issue/PR URLs; per-target bases, heads, branches, commits, worktrees,
review findings, validation/checks, mergeability, runtime applicability,
resources, verification/repeat/shutdown/cleanup commands, and blockers or
`none`.

Keep worktrees and reports while PRs are open. Stop before merge or closure.
Cleanup requires a separate post-merge request beginning with
`./atrinik cleanup --dry-run --json`.
