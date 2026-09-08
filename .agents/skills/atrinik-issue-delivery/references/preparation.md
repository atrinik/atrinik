# Delivery preparation and recovery

Read on first entry, recovery, or coordinate/authority drift; retain the proven
coordinate packet during an uninterrupted session. The helper still performs
its required live checks before each protected action.

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
  one human-report identity from [the report asset](../assets/deep-review-report.md):

  ```text
  <wrapper-root>/build/reviews/<owner>-<repository>-issue-<number>.md
  <wrapper-root>/build/reviews/<owner>-<repository>-pr-<number>.md
  ```

  Markdown is non-authoritative. Follow [the ledger protocol](delivery-ledger.md)
  exactly; strict schema-v1 `<report>.ledger.json`, managed only through
  `python3 scripts/delivery_ledger.py`, is the sole ownership/recovery record.
  Safely initialize/inventory the ignored root, then create or explicitly
  migrate the prepared document before any claim, Project, remote-write,
  artifact, scope, or resource mutation. CAS every result/live refresh through
  the helper; never hand-roll state, locks, markers, or recovery.

  Optional process-improvement and tooling-issue diagnostics never gate work.
  When useful to record, they have one supported edit path: `./atrinik agent-ledger update --ledger ...`. It resolves the
  canonical shared root, reads and validates the latest bytes under a stable
  shared lock, publishes atomically, and returns a digest/CAS result. Never
  manually edit, overwrite, or truncate either file; follow stale/lock/retry
  output. Separate filesystems require a coordinator or event handoff.

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

### Prove the authoritative coordinator context

Before any ledger mutation, run
`python3 scripts/atrinik_coordinator_context.py --json` and require
`status: "canonical-linux"` with `authoritative: true`. The bounded,
secret-free probe combines the pinned config with live Linux/POSIX, user,
Codex, no-follow, mode, and mount checks; runtime markers never authorize it.
`native-windows`, `windows-cross`, and `unknown-or-unsafe` stop delivery.

Use exactly two entry modes:

- Already in a canonical VS Code devcontainer: retain the current process,
  workspace, bound worktree, ledger, leases and caches; never bootstrap another.
- Native host: bootstrap/attach once with Docker or the Dev Containers CLI into
  the pinned ordinary Linux image. Thereafter the host performs only approved
  Git/GitHub/commit operations; ownership, ledger, worktree, edit, test, build,
  review and validation run inside the coordinator.

An `entry_mode` marker, container name or copied session record is corroboration,
not authority. Codex never launches or controls VS Code; no executable/URI or
GUI automation, nested containers, live remounts or foreign/stale coordinates.
Wrapper-required Docker operations retain their own operation contract.

### Reuse one owned devcontainer session

Retain one pinned container per scope and an ignored, secret-free record of its
agent, ledger, worktree, image/mount/volume identities, lifecycle and cleanup owner.
Reconnect/crash recovery re-proves the live probe, exact worktree, ledger CAS and
leases. Bound idle/lifetime to 30 minutes/12 hours; preserve stopped evidence and
stop only owned resources. Parallel scopes use distinct coordinates, worktrees,
profiles/build roots, volumes, Codex homes, caches, ports and topology/state.

Trusted workers share [host GitHub auth read-only](../../../../docs/COORDINATOR_AUTH.md).
The host owns login/refresh/account changes; workers verify actor and required
repository/Project/package capabilities together before ledger genesis or resume.
Bundle missing scopes into one host action; do not repeat pending login requests
or ask again for a task mutation already authorized in the session. Keep
other mutable credential stores private. Credential availability grants no
additional task authority. The session benchmark remains credential/source-free.

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
  consumer. The former `1.x` branch no longer exists as a live delivery target;
  its tags, artifacts, and local paths are immutable migration/release
  evidence. Never recreate it, publish a new `1.x` PR, or request a backport
  there. A content `main` PR may close an explicitly selected issue when
  merged, but leave every issue open for maintainers.

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
- In issue mode, `bind-check` only diagnoses a remotely created planned PR.
  Use the helper-owned `pr-bind-cas` with the exact PR number and a fresh
  four-part ledger tuple. It re-proves the authenticated actor, same-repository
  draft PR, durable body, complete paginated comment collection, target, and
  bound worktree immediately before its private CAS; generic `cas` cannot
  perform this initial PR bind. Ordinary Codecov, reviewer, and other external
  comments are expected and remain untouched. Any malformed or reserved
  `atrinik-delivery:comment:` marker, invalid comment page, duplicate node, or
  incomplete/bounded-out pagination fails closed.
- Issue-mode actor proof precedes genesis; pre-bind changes use audited recovery.
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
  Released scopes and other references block. Each physical repository needs
  its own worktree, branch, commits, validation, and PR. In PR mode,
  this does not authorize a companion PR; another repository needs separate
  type-explicit delivery authority.


## Revalidate unchanged bound targets on reconnect

After the canonical probe, authenticated selection and complete collision
inventory above, run the ledger protocol's public
`revalidate-current-targets-cas` with the exact helper-returned
generation/digest/device/inode. It derives a neutral observation while holding
every current bound target's live guards. Preserve immutable initial requests;
do not manufacture a commit, repeat initial binding, call private helper
contexts, or use generic CAS/check-reuse as a substitute for live proof.
Actual target drift still uses `target-refresh-cas`. A changed actor, dirty,
foreign, locked, active or uncertain target blocks this operation.
