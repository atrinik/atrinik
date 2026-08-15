# Delivery ledger operator reference

Use this reference with `atrinik-issue-delivery` whenever creating, recovering,
or updating its authoritative schema-v1 sidecar. The helper, not Markdown, is
the ownership and recovery boundary.

## Contents

- [Establish the trust boundary](#establish-the-trust-boundary)
- [Use the command surface](#use-the-command-surface)
- [Apply the schema grammar](#apply-the-schema-grammar)
- [Create a fresh issue ledger](#create-a-fresh-issue-ledger)
- [Create a fresh PR ledger](#create-a-fresh-pr-ledger)
- [Provision a scope-produced worktree](#provision-a-scope-produced-worktree)
- [Create, inspect, and update](#create-inspect-and-update)
- [Bind a created PR](#bind-a-created-pr)
- [Plan and recover body updates](#plan-and-recover-body-updates)
- [Plan and recover comments](#plan-and-recover-comments)
- [Migrate prior evidence](#migrate-prior-evidence)
- [Recover interrupted transactions](#recover-interrupted-transactions)
- [Release, archive, and reclaim after merge](#release-archive-and-reclaim-after-merge)
- [Observe strict prohibitions](#observe-strict-prohibitions)

## Establish the trust boundary

Run every helper command from the skill directory so `scripts/delivery_ledger.py`
resolves to the bundled reviewed program. First prove the wrapper root, report
paths, and `build/reviews` are ignored. Set task-specific absolute paths; never
repurpose `HOME`, `CODEX_HOME`, or another ambient option variable.

```sh
DELIVERY_WRAPPER_ROOT=/workspaces/atrinik
DELIVERY_REVIEW_ROOT=/workspaces/atrinik/build/reviews
DELIVERY_LEDGER=atrinik-atrinik-issue-419.md.ledger.json
python3 scripts/delivery_ledger.py init-root "$DELIVERY_WRAPPER_ROOT"
python3 scripts/delivery_ledger.py inventory "$DELIVERY_REVIEW_ROOT"
```

`init-root WRAPPER_ROOT` opens the existing wrapper root without following
symlinks. Before creating anything, it requires trusted regular
`components.json` with the exact Atrinik top-level manifest shape, a trusted
executable `atrinik` launcher containing the canonical
`from atrinik_workspace.cli import main` line, and safe Git metadata: either a
trusted `.git` directory with one canonical commit/symbolic `HEAD`, or a trusted
regular gitfile naming an absolute normalized non-root Git directory whose
entire path opens no-follow to one stable trusted directory with canonical
`HEAD`. It then creates only `build/` and `build/reviews/` when absent, fsyncs
each publication, and returns the exact review-root path/device/inode. It is not
an arbitrary directory creator. All later root arguments name the returned
review root.

The helper accepts at most 1,048,576 bytes per JSON, body, section, comment, or
migration-source input. It inventories at most 4,096 directory entries and
32 MiB of relevant delivery files. File inputs must be stable regular no-follow
files addressed through a no-follow parent path. Only `prepare` accepts `-` as
bounded stdin; every other input operand must name such a file. Ledger/report/
source arguments inside the review root are direct child names, never paths.
Managed files are owned by the current effective user, mode `0600`, and have
only helper-modeled link counts.

Every successful command emits JSON. A policy, schema, safety, I/O, or recovery
failure emits `delivery-ledger: ...` on stderr and exits 2. Treat any nonzero
status as a stop, not permission to repair files manually.

## Use the command surface

These are the complete operator commands. Except for `prepare INPUT_OR_DASH`,
every file-input operand must name a regular bounded file so its intended bytes
remain inspectable.

```sh
python3 scripts/delivery_ledger.py init-root WRAPPER_ROOT
python3 scripts/delivery_ledger.py prepare INPUT_OR_DASH
python3 scripts/delivery_ledger.py inventory REVIEW_ROOT
python3 scripts/delivery_ledger.py inspect REVIEW_ROOT LEDGER_NAME
python3 scripts/delivery_ledger.py create REVIEW_ROOT INPUT
python3 scripts/delivery_ledger.py cas REVIEW_ROOT LEDGER_NAME INPUT \
  --expected-generation GENERATION \
  --expected-digest SHA256 \
  --expected-device DEVICE \
  --expected-inode INODE
python3 scripts/delivery_ledger.py migrate REVIEW_ROOT SOURCE_NAME INPUT \
  --kind legacy \
  --expected-source-digest SHA256
python3 scripts/delivery_ledger.py migrate REVIEW_ROOT SOURCE_NAME INPUT \
  --kind pre-schema \
  --expected-source-digest SHA256
python3 scripts/delivery_ledger.py check-reuse REVIEW_ROOT LEDGER_NAME --kind all
python3 scripts/delivery_ledger.py check-reuse REVIEW_ROOT LEDGER_NAME --kind artifacts
python3 scripts/delivery_ledger.py check-reuse REVIEW_ROOT LEDGER_NAME --kind resources
python3 scripts/delivery_ledger.py bind-check REVIEW_ROOT LEDGER_NAME SLOT_ID INPUT
python3 scripts/delivery_ledger.py worktree-bind REVIEW_ROOT LEDGER_NAME SLOT_ID \
  WORKTREE_LIST_JSON SAFETY_JSON [--create-output OUTPUT]
python3 scripts/delivery_ledger.py worktree-observe REVIEW_ROOT LEDGER_NAME SLOT_ID \
  WORKTREE_LIST_JSON OBSERVED_AT [--create-output OUTPUT]
python3 scripts/delivery_ledger.py worktree-bind-cas REVIEW_ROOT LEDGER_NAME SLOT_ID \
  WORKTREE_LIST_JSON SAFETY_JSON [--create-output OUTPUT] \
  --expected-generation GENERATION --expected-digest SHA256 \
  --expected-device DEVICE --expected-inode INODE
python3 scripts/delivery_ledger.py scope-bind REVIEW_ROOT LEDGER_NAME SLOT_ID \
  SCOPE_SHOW_JSON WORKTREE_LIST_JSON SAFETY_JSON
python3 scripts/delivery_ledger.py scope-observe REVIEW_ROOT LEDGER_NAME SLOT_ID \
  SCOPE_SHOW_JSON WORKTREE_LIST_JSON OBSERVED_AT
python3 scripts/delivery_ledger.py scope-bind-cas REVIEW_ROOT LEDGER_NAME SLOT_ID \
  SCOPE_SHOW_JSON WORKTREE_LIST_JSON SAFETY_JSON \
  --expected-generation GENERATION --expected-digest SHA256 \
  --expected-device DEVICE --expected-inode INODE
python3 scripts/delivery_ledger.py pr-create-payload REVIEW_ROOT LEDGER_NAME SLOT_ID
python3 scripts/delivery_ledger.py body-check REVIEW_ROOT LEDGER_NAME PR_NODE_ID BODY
python3 scripts/delivery_ledger.py body-plan REVIEW_ROOT LEDGER_NAME PR_NODE_ID BODY SECTION
python3 scripts/delivery_ledger.py body-recovery REVIEW_ROOT LEDGER_NAME PR_NODE_ID LIVE_SHA256 LIVE_UPDATED_AT
python3 scripts/delivery_ledger.py body-recovery REVIEW_ROOT LEDGER_NAME PR_NODE_ID absent absent
python3 scripts/delivery_ledger.py comment-check REVIEW_ROOT LEDGER_NAME PR_NODE_ID INPUT
python3 scripts/delivery_ledger.py release-preview REVIEW_ROOT LEDGER_NAME INPUT
python3 scripts/delivery_ledger.py release-apply REVIEW_ROOT LEDGER_NAME INPUT --plan SHA256
python3 scripts/delivery_ledger.py archive-preview REVIEW_ROOT LEDGER_NAME INPUT
python3 scripts/delivery_ledger.py archive-apply REVIEW_ROOT LEDGER_NAME INPUT --plan SHA256
python3 scripts/delivery_ledger.py reclaim-preview REVIEW_ROOT ARCHIVE_NAME
python3 scripts/delivery_ledger.py reclaim-apply REVIEW_ROOT PREVIEW --plan SHA256
```

`prepare` validates and returns a normalized document. Stored bytes are stricter:
ASCII JSON with sorted keys, no insignificant whitespace, and one terminal
newline. `inspect` returns `name`, `digest`, `device`, `inode`, and `document`;
use all four identity values from the same snapshot for CAS. `inventory` first
validates every recognized canonical ledger, report claim, lock, transaction,
migration marker, and staging file, then rejects overlaps or unsafe debris.
`worktree-bind` and `scope-bind` only diagnose retained evidence; they never
authorize a later caller-built CAS. Their `*-observe` counterparts derive the
safety document under pinned no-follow roots and workspace leases. Only
`*-bind-cas` is the mutation boundary: it reproves the live state immediately
before its internal CAS, constructs the sole next generation without a caller
candidate, and returns exactly `classification`, `slot_id`, `request_sha256`,
`result_sha256`, `path`, and installed `snapshot`
`{name, digest, device, inode, document}`. Pass one exact four-part tuple from
`inspect`; stale identity or changed live state stops. A pending predecessor
receipt can be recovered only with its original predecessor tuple. A fresh
current tuple cannot treat that receipt as an ordinary `bound-match` retry.

Before any dynamic `Workspace` import or Python execution, live proof performs
a bounded component-wise no-follow ownership/mode prevalidation of the complete
importable `atrinik_workspace` source/bytecode tree. It fingerprints every
path/type/mode/device/inode and file byte, then executes only retained `.py`
snapshot bytes under a fingerprint-specific private package name, never
bytecode. It then recomputes the full tree and rechecks loaded source paths. It
likewise pretrusts and rechecks managed worktree, scope, and profile ancestors
plus trusted-owner, non-group/world-writable `components.json`, scope/profile
authority files, local `config`/`config.worktree`, administrative
`HEAD`/`index`/`commondir`/`gitdir`, and the exact loose-or-packed requested ref,
including the modeled absence of optional authority entries. Split indexes are
unsupported. The
actual primary/worktree common Git directory is resolved, pinned, and compared;
it is never reconstructed as `PRIMARY/.git`, so a trusted linked-checkout
gitfile remains valid. A worktree `.git` gitfile is retained and rechecked
through live proof, and its admin directory must be one direct registration in
that exact common directory.

On both primary and worktree checkouts, Git route proof anchors exactly one raw
local `remote.origin.url` with includes disabled. After stripping dangerous
selector `GIT_*` variables but retaining default user/system credential and
URL-rewrite configuration, each also requires one effective fetch and one
effective push route for explicit `origin`. Every route must resolve to the
recorded GitHub repository over SSH or HTTPS; multiple, foreign, or plain-HTTP
routes stop. Diagnostics never expose the URL or credentials.
Immediately reprove that route and push only with explicit
`git push origin HEAD_BRANCH` in the same scrubbed selector environment. Never
use an implicit remote or disable credential helpers.

Release/archive/reclaim are a separate post-merge lifecycle. Their previews are
read-only and each apply accepts only the exact preview digest. A release marker
is the sole event that makes coordinates inert. Inventory reports active
records in `ledgers`, installed terminal markers in `releases`, and bundled
history in `archives`. `historical_ledgers` retains decoded inert snapshots for
audit and global node/coordinate identity consistency; staged or interrupted
work remains in `pending`.

## Apply the schema grammar

All objects use exactly the listed keys; unknown or missing keys fail. Arrays
whose entries carry identities are duplicate-free and sorted by their validated
identity, case-insensitively. Node-ID allowlists are unique bytewise-sorted
arrays. JSON booleans never satisfy integer fields.

### Lexical primitives

| Value | Grammar |
| --- | --- |
| SHA-256 | 64 lowercase hexadecimal characters |
| Git commit | 40 lowercase hexadecimal characters |
| External scope generation | 32 lowercase hexadecimal characters |
| Owner | lowercase GitHub owner, 1–40 letters/digits/hyphens, with alphanumeric ends |
| Repository | lowercase, 1–100 letters/digits/periods/underscores/hyphens, starting alphanumeric |
| Login | lowercase, 1–39 letters/digits/hyphens, with alphanumeric ends |
| Node ID | 2–256 ASCII letters/digits/underscore/equal/hyphen characters |
| Slot ID | 1–128 lowercase letters/digits/periods/underscores/hyphens, starting alphanumeric |
| Reference/name | 1–512 ASCII letters/digits plus `:/._#+-`, starting alphanumeric |
| Timestamp | a real UTC instant in `YYYY-MM-DDTHH:MM:SSZ`, optionally with 1–9 fractional digits; impossible dates/times fail |
| Branch | a 1–255 character ASCII canonical Git-compatible ref drawn from letters/digits and `._+/-`, beginning with a letter/digit/underscore/plus, never exact `HEAD`, with no `..`, `//`, `@{`, dot-prefixed component, `.lock` component, or terminal dot/slash |
| Path | absolute normalized non-root lexical path with no NUL, ASCII control/DEL, or doubled slash |

### Root and identity objects

The root has exactly:

```text
schema_version, ledger_id, entry_mode, actor, authority, program,
issues, selected_prs, targets, closing_scope, artifacts, resources,
generation, previous_byte_digest, history, migration
```

- `schema_version` is the JSON integer `1` exactly; `1.0` and booleans fail.
- `entry_mode` is `issue` or `pr`.
- `ledger_id` is `delivery-v1:issue:ISSUE_NODE_ID` for the sole explicit
  issue, or `delivery-v1:pr:PR_NODE_ID` for the sole selected PR.
- `actor` has exactly `login`, `node_id`, `push_repository_node_ids`. The list
  exactly equals the nonempty target-repository set; every selected head is in
  that same-repository set.
- A repository has exactly `owner`, `name`, `node_id`. Every repeated coordinate
  must retain one node ID, and one node ID cannot alias another coordinate.
- An issue has exactly `repository`, positive integer `number`, `node_id`.
  Issue coordinates and node IDs obey the same bidirectional uniqueness rule.
- `issues` has exactly sorted `explicit` and `incidental` arrays with no overlap.
  Issue mode has exactly one explicit issue. PR mode has zero or one.
- `closing_scope` is sorted and contains only mapped issues. In issue mode it is
  exactly the sole explicit issue, never an incidental issue. PR mode preserves
  live closing references to mapped issues; a verified explicit non-closing
  association may leave it empty. Incidental references remain read-only and
  never create authority by themselves.

Inventory enforces these coordinate/node-ID bijections across every ledger,
not merely within one document. It also rejects overlapping mutable artifact or
resource coordinates and every known planned-PR conflict: two planned PR slots
cannot claim the same repository/head branch, and a planned slot cannot collide
with a selected or bound PR in another ledger. A coordinate discovered in one
ledger therefore cannot be silently rebound under another node ID or intent.

`authority` has exactly `kind`, `reference`, `objective_sha256`, `issued_at`,
`actor_node_id`, and `allowed`. `kind` is `durable-goal`,
`explicit-invocation`, or `explicit-recovery`. `issued_at` must parse as the
real UTC instant at which this authority was issued, and the actor node must
equal the authenticated actor. `allowed` has exactly nonempty sorted
`repositories` and sorted `issues`/`pull_requests`; these are exact allowlists,
not supersets. Repositories equal all target/head repositories. Issues equal
the explicit issues plus both program coordinates, if any; incidental issues
are never allowed. Pull requests equal every pre-authorized selected or
immutable-known planned PR.

Every timestamp is compared as its parsed UTC instant, not raw spelling.
Optional fractional-second encodings that denote the same instant compare
equal, including body recovery and program authority-family `issued_at`.

The sole unknown-node exception begins with a fresh issue-mode generation 1
whose pull-request allowlist is empty. A later CAS may add exactly one selected
PR by changing exactly one pre-recorded PR slot from `planned` to `created`,
never `adopted`. The selected PR is an authenticated-actor-authored draft with
null ready intent and comment state none. Its same-repository head, full
unchanged target repository/base/head, immutable closing scope, and
`delivery-created`/`written` body must match the slot's exact intended identity;
the body has null observed/intended digest/payload, null section, equal
current/outside digest, and the planned PR slot retains the exact matching
initial payload. The first-bind CAS alone enforces those pristine remote
conditions. Once bound, normal authorized body, comment, readiness, and
coordinate transitions do not have to preserve the original draft/body state,
but the PR must forever retain that exact created-slot provenance and payload.
After
normalizing ledger generation/history, removing the new PR, and restoring the
slot, the first-bind candidate must equal the predecessor. Thus the exception
changes neither authority nor any unrelated field. Authority records proof; it
never synthesizes authority.

`program` is null or has exactly `master_issue`, `leaf_issue`, and nonnegative
integer `leaf_position`. It is issue-mode only; the distinct leaf equals the
sole explicit issue, and master and leaf node IDs are distinct. Inventory
reserves the canonical master coordinate against a direct non-program issue
ledger and every unpaired legacy master report. Program leaves may share that
master only at distinct positions and under one exact authority family:
authority kind, reference, objective digest, issued-at instant, actor node, and
the complete canonical master identity all match. Thus a master
repository/number/position has one owner, while a direct ledger for that master
conflicts with every program leaf regardless of position.

### PR, body, and comment objects

A selected PR has exactly:

```text
repository, head_repository, number, node_id, author_node_id,
base_branch, head_branch, draft, draft_intent, body, comment
```

The head repository must have the same normalized owner/name as the base
repository and recorded push authority. The positive number/node ID is globally
consistent. `draft_intent` is null or `ready`; a ready (`draft: false`) PR must
have null intent. Each selected PR matches exactly one target and one bound PR
artifact, including current body digest. PR mode has exactly one selected PR
and target. Issue-mode genesis has none; later selected PRs must be backed by a
newly bound planned PR slot. A delivery-created body requires the authenticated
actor to be the PR author.

`body` has exactly `ownership`, `state`, `observed_digest`, `intended_digest`,
`intended_payload`, `current_digest`, `outside_digest`, `section_digest`, and
`updated_at`:

| State | Exact constraints |
| --- | --- |
| contributor `observed` | ownership `contributor-owned`; observed=current=outside non-null; intended/payload/section null; timestamp non-null |
| delivery `written` | ownership `delivery-created` or `delivery-section`; current/outside/timestamp non-null; intended/payload null; delivery-section has a non-null section digest; a null section implies outside=current |
| delivery `update-planned` | delivery ownership; observed=current non-null; intended and exact payload present, matching, and different from current; timestamp non-null; outside digest matches the intended prefix |

An inline payload has exactly `{encoding: "base64", raw_base64, sha256}`. Its
base64 is canonical, decoded bytes are at most 512 KiB and valid UTF-8, and the
digest matches. Initial PR bodies and every planned body/comment write retain
these exact bytes in the authoritative ledger before the remote mutation.

Contributor bytes remain read-only, but PR-mode delivery may append one
separately owned terminal `delivery-section`. Its first plan changes
`contributor-owned`/`observed` to `delivery-section`/`update-planned` while
retaining the contributor digest as `outside_digest` and leaving
`section_digest` null until exact intended bytes bind. Only this never-applied
first plan may cancel back to contributor ownership. Later delivery-section and
delivery-created ownership is immutable.

The coordinate-bound start/end markers have canonical newline framing and are
terminal; their owned leading separator is excluded from outside bytes.
Duplicate, foreign, malformed, reversed, nonterminal, or contributor-copied
markers fail and never grant ownership. Every section update preserves the
exact outside digest. An observation-only refresh advances `updated_at`
strictly and updates the bound PR artifact digest in the same CAS. A PR-level
timestamp may advance while exact body bytes do not; this is observation only,
not mutation authority.

`comment` has exactly `state`, `marker`, `intended_digest`, `intended_payload`,
`node_id`, and `current_digest`:

| State | Exact constraints |
| --- | --- |
| `none` | every identity/payload field null |
| `planned` | coordinate marker, intended digest, and matching exact payload present; node/current both null for a first post or both present for a bound update; current differs from intended |
| `in-flight` | same shape and bytes as planned except state; it is persisted before the remote write |
| `bound` | marker/node/current present; intended digest/payload null |

The marker is exactly `<!-- atrinik-delivery:comment:TOKEN -->`; the helper
derives `TOKEN` from canonical `ledger_id`, `surface=comment`, and exact PR
repository/number/node. A classified comment contains that marker and the
entire `atrinik-delivery:comment:` namespace exactly once. Extra, malformed, or
foreign occurrences fail even when the exact marker is also present. A live
marker never grants ownership. Planned comment payload starts with the exact
marker and newline and contains the entire namespace exactly once.

### Target and artifact objects

Each target has exactly `repository`, `base`, `head`, `merge_base`. Base/head
have exactly `branch`, `initial_sha`, `current_sha`, `lineage`. Lineage is a
nonempty unique commit array from immutable initial through current.
`merge_base` has exactly immutable `initial_sha` and current `current_sha`.
Every target has exactly one branch, worktree, and PR artifact sharing its
repository/head branch.

An artifact has exactly `slot_id`, `kind`, `state`, `immutable`, `current`,
`safety`, `producer_resource_slot`; a worktree additionally has exact
`primitive_request` and `primitive_result` keys, and a PR additionally has exact
`initial_body_payload`. Kinds are `branch`, `worktree`, and `pull_request`;
states are `planned`, `created`, and `adopted`. Planned means `current` and
`safety` are null. A bound state requires both.

Immutable/current identity has exactly `repository`, `branch`, `path`,
`number`, `node_id`, `body_digest`; current additionally has `head_sha`.

- Branch: branch non-null; path/number/node/body null.
- Worktree: branch non-null; number/node/body null. Immutable path may be
  non-null only for an already-bound migrated known-path primitive. A deferred
  scope path has one
  `producer_resource_slot`; a deferred primitive path instead has one
  `primitive_request`; these are mutually exclusive. A bound worktree always
  has a current path. Every fresh planned worktree has null immutable path and
  exactly one of those deferred producers, including wrapper-self. A migration
  may preserve an already-bound known path; a planned known path is invalid.
- PR: branch/body digest non-null, path null, and number/node both null or both
  present. Bound PR current always has number/node/body/head; a preselected
  number/node cannot change. Planned/created PRs retain a non-null exact initial
  body payload matching the immutable digest. Adopted PRs may have null payload,
  and fresh PR-mode genesis requires null. A non-null payload contains no
  reserved body namespace and is immutable through CAS.
- Repository and branch never change. `producer_resource_slot` and
  `primitive_request` are immutable. Only a scope-produced or exact
  request-bound primitive worktree may fill a current path absent from
  immutable intent.
- Bound `head_sha` equals its target current head. Bound PR digest equals its
  selected PR current body.

A deferred primitive worktree request has exactly `component`,
`physical_checkout`, `label`, `repository`, `branch`, `expected_head_sha`, and
`roots`; its repository/branch equal immutable artifact intent and its head
equals the unchanged target head. `roots` has exact `wrapper`, `workspace`, and
`primary` path identities, each `{path, device, inode}`. The primary path is
`WRAPPER/PHYSICAL_CHECKOUT`; workspace is the precommitted, non-root managed
workspace directory and cannot be the wrapper or its ancestor. All three must
still be exact live canonical directories when binding.

`primitive_result` is null while planned. A bound result has exactly
`create_output`, `worktree_list`, and `safety_observation`. Each non-null value
is retained as `{encoding: "base64", raw_base64, sha256}`, with canonical base64,
matching digest, and at most 512 KiB of exact bytes. A successful normal
manifest-owner wrapper create must retain `create_output`; it decodes to exactly
one UTF-8 LF-terminated path line. It is null only for wrapper-self raw-Git
creation, which has no canonical wrapper stdout, or when recovery proves output
was genuinely unretained. `worktree_list` is mandatory wrapper JSON containing
one exact requested row at `WORKSPACE/worktrees/PHYSICAL_CHECKOUT/LABEL`, the
exact primary row, recorded head, and `refs/heads/BRANCH`, with no detached,
locked, prunable, duplicate, or ambiguous match.

The decoded safety observation has exactly `schema_version`, semantic
`observed_at`, `repository`, `component`, `physical_checkout`, `roots`, `path`,
`path_device`, `path_inode`, `branch`, `head_sha`, `worktree_list_sha256`,
`producer`, and `safety`. `producer` is exactly `{kind, result_sha256}`: kind is
`primitive` and its digest is the optional create-output digest, or kind is
`scope` and its digest is the raw scope-show digest. Every coordinate and digest
must match the retained request/results; safety is the exact reusable-safe
state. `worktree-observe` and `scope-observe`, never the caller, produce this
document from manifest, Git, wrapper, scope, reference, root/path, and safety
proof under component-wise no-follow descriptors and workspace leases. The
caller supplies a semantic timestamp only; it is metadata, not proof of
freshness. Atomic binding pins and reproves the same state immediately before
CAS. Known-path and scope-produced worktrees keep both primitive fields null;
scope retains the analogous evidence on its resource instead.

`safety` has exactly boolean `clean`, `detached`, `locked`, `active`,
`unowned_reference`, `foreign`, `certain`. Reuse requires clean/certain true
and every other flag false. Planned artifacts do not claim safety.

### Resource and history objects

A resource has exactly `slot_id`, `kind`, `state`, `immutable`, `current`; a
scope additionally has an immutable top-level `request`.
Kinds are `build`, `profile`, `reference`, `runtime`, `scenario`, `scope`,
`state`, `topology`; states use the artifact state enum. Immutable identity has
exactly `repository`, `name`, `path`. Current adds integer `generation`,
`external_generation`, `identity_digest`, `history`, `lifecycle`; scope current
also has retained `binding` and `observation`. Immutable
repository/name/path/request never
change. Names and non-null paths are singleton ownership coordinates across
ledgers. The scope request has exactly `name`, `component`, `profile`,
`physical_checkout`, `label`, `branch`, `start_sha`, `temporary_state`, exact
`state_policy`, `topology`, and the same exact `roots`; request
name/repository/branch/start commit must match resource, target, and produced
artifact intent. Scope `binding` retains raw scope-show bytes. Its `observation`
has exact retained `worktree_list` and `safety_observation` members governed by
the same fresh-live-evidence contract above.

Resource observation generation starts at 1; its history has exactly
`generation - 1` SHA-256 values. The external generation is required only for
scope and is immutable after binding. Lifecycles are kind-specific:

| Kind | Allowed lifecycle | Reusable state |
| --- | --- | --- |
| profile/reference/build | `static` | static |
| topology/runtime | `stopped`, `running` | stopped only |
| scope | `active`, `released` | active only |
| state/scenario | `active`, `consumed`, `ready`, `released`, `stopped` | ready or stopped |

Resources may be appended by CAS only as planned/null slots before their
external mutation. No resource may be removed. Binding preserves immutable
identity. An observed identity refresh either keeps lifecycle, generation,
digest, and history unchanged, or changes the observation by incrementing
generation exactly one, changing identity digest, and appending the old digest.
Every lifecycle change is such a new observation. Scope external generation
never advances within one resource slot, and `released` is terminal: it cannot
return to `active` or become reusable.

Ledger `generation` is a positive integer. Generation 1 always has null
`previous_byte_digest` and empty `history`. Fresh-create and migration input
also have null `migration`; a successfully installed migrated generation 1 is
the sole exception because the helper adds immutable complete migration
metadata before canonical publication. Each CAS increments exactly one, sets
previous to the exact old byte digest, and appends that digest to history.
Immutable root fields are schema/ledger/mode/actor/authority/program/issues/
closing/migration. Target set, branches, initial commits, merge-base anchor,
artifact slots/immutable intent, producer slot, and bound state are immutable;
legal live-observation CAS may refresh a bound artifact's safety.
Base or head advancement extends prior lineage; merge-base changes only with
such an advance. Rewrites and retargets fail. Generic `cas` cannot perform any
part of an initial deferred primitive or scope bind; only the purpose-specific
atomic binder may create the exact branch/worktree/resource candidate and its
tagged crash receipt.

Before target drift, cancel every `draft_intent`, delivery body
`update-planned` intent, and comment `planned` intent in a completed separate
CAS. Both the predecessor and replacement of the later coordinate CAS must be
intent-free; cancellation cannot be combined with coordinate drift. An
`in-flight` comment cannot be cancelled: recover it to bound or stop. A bound
comment carries no pending intent.

`migration` is null for create/migration input. A migrated installed ledger has
exactly `kind`, `state=complete`, `source`, `snapshot`, `canonical_report`,
`marker_name`. Source/snapshot each have exact direct `name`, SHA-256, device,
inode. The snapshot and marker names must be helper-canonical.

## Create a fresh issue ledger

Replace every synthetic identity/digest/path with live proven values. This is a
complete valid generation-1 deferred wrapper-self primitive-worktree issue
input; no field is omitted.

```json
{
  "actor": {
    "login": "zoeyrose",
    "node_id": "U_actor",
    "push_repository_node_ids": [
      "R_repo"
    ]
  },
  "artifacts": [
    {
      "current": null,
      "immutable": {
        "body_digest": null,
        "branch": "docs/issue-419",
        "node_id": null,
        "number": null,
        "path": null,
        "repository": {
          "name": "atrinik",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "kind": "branch",
      "producer_resource_slot": null,
      "safety": null,
      "slot_id": "branch",
      "state": "planned"
    },
    {
      "current": null,
      "immutable": {
        "body_digest": "d11168a907f543b3d6a169d182cb58d16fb7099bde2845c90f78a5d912d82e55",
        "branch": "docs/issue-419",
        "node_id": null,
        "number": null,
        "path": null,
        "repository": {
          "name": "atrinik",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "initial_body_payload": {
        "encoding": "base64",
        "raw_base64": "Q2xvc2VzICM0MTkK",
        "sha256": "d11168a907f543b3d6a169d182cb58d16fb7099bde2845c90f78a5d912d82e55"
      },
      "kind": "pull_request",
      "producer_resource_slot": null,
      "safety": null,
      "slot_id": "pull-request",
      "state": "planned"
    },
    {
      "current": null,
      "immutable": {
        "body_digest": null,
        "branch": "docs/issue-419",
        "node_id": null,
        "number": null,
        "path": null,
        "repository": {
          "name": "atrinik",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "kind": "worktree",
      "primitive_request": {
        "branch": "docs/issue-419",
        "component": "atrinik",
        "expected_head_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "label": "issue-419",
        "physical_checkout": "atrinik",
        "repository": {
          "name": "atrinik",
          "node_id": "R_repo",
          "owner": "atrinik"
        },
        "roots": {
          "primary": {
            "device": 1,
            "inode": 101,
            "path": "/workspaces/atrinik"
          },
          "workspace": {
            "device": 1,
            "inode": 102,
            "path": "/workspaces/atrinik/workspace"
          },
          "wrapper": {
            "device": 1,
            "inode": 101,
            "path": "/workspaces/atrinik"
          }
        }
      },
      "primitive_result": null,
      "producer_resource_slot": null,
      "safety": null,
      "slot_id": "worktree",
      "state": "planned"
    }
  ],
  "authority": {
    "actor_node_id": "U_actor",
    "allowed": {
      "issues": [
        "I_issue"
      ],
      "pull_requests": [],
      "repositories": [
        "R_repo"
      ]
    },
    "issued_at": "2026-08-14T18:00:00Z",
    "kind": "durable-goal",
    "objective_sha256": "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
    "reference": "goal:issue-419"
  },
  "closing_scope": [
    {
      "node_id": "I_issue",
      "number": 419,
      "repository": {
        "name": "atrinik",
        "node_id": "R_repo",
        "owner": "atrinik"
      }
    }
  ],
  "entry_mode": "issue",
  "generation": 1,
  "history": [],
  "issues": {
    "explicit": [
      {
        "node_id": "I_issue",
        "number": 419,
        "repository": {
          "name": "atrinik",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      }
    ],
    "incidental": []
  },
  "ledger_id": "delivery-v1:issue:I_issue",
  "migration": null,
  "previous_byte_digest": null,
  "program": null,
  "resources": [],
  "schema_version": 1,
  "selected_prs": [],
  "targets": [
    {
      "base": {
        "branch": "main",
        "current_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "initial_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "lineage": [
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        ]
      },
      "head": {
        "branch": "docs/issue-419",
        "current_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "initial_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "lineage": [
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        ]
      },
      "merge_base": {
        "current_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "initial_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      },
      "repository": {
        "name": "atrinik",
        "node_id": "R_repo",
        "owner": "atrinik"
      }
    }
  ]
}
```

Fresh issue create additionally requires no selected PR, an empty PR allowlist,
null number/node intent and an exact initial payload in every planned PR slot,
and every artifact/resource planned. Authority kind is `durable-goal` or
`explicit-invocation`, never
recovery authority. Each target starts from one
proven base commit: base/head initial and current SHAs and merge-base initial/
current SHA are all that same commit, and both lineages are the singleton
commit. The intended head is a new, lowercase, Git-compatible branch name
distinct from the base branch and whose case-folded name is not `head`; exact
`HEAD` is already forbidden for every branch field. Every
planned branch/worktree/PR slot matches it exactly. Its canonical filename is
`atrinik-atrinik-issue-419.md.ledger.json`. Create the ledger before assigning,
changing Project state, creating the branch/worktree/scope/PR, editing a body,
or posting a comment.

For this wrapper-self genesis, create only the exact precommitted managed path
after the ledger exists. Set the four `DELIVERY_EXPECTED_*` values from one
fresh `inspect`, then capture a fresh wrapper list and atomically bind:

```sh
DELIVERY_WORKTREE_LIST=/tmp/atrinik-issue-419-worktrees.json
DELIVERY_WORKTREE_SAFETY=/tmp/atrinik-issue-419-worktree-safety.json
DELIVERY_OBSERVED_AT=2026-08-14T18:05:00Z
git -C "$DELIVERY_WRAPPER_ROOT" worktree add -b docs/issue-419 \
  "$DELIVERY_WRAPPER_ROOT/workspace/worktrees/atrinik/issue-419" \
  aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
"$DELIVERY_WRAPPER_ROOT/atrinik" worktree list --wrapper-self --json \
  > "$DELIVERY_WORKTREE_LIST"
python3 scripts/delivery_ledger.py worktree-observe \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" worktree \
  "$DELIVERY_WORKTREE_LIST" "$DELIVERY_OBSERVED_AT" \
  > "$DELIVERY_WORKTREE_SAFETY"
python3 scripts/delivery_ledger.py worktree-bind-cas \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" worktree \
  "$DELIVERY_WORKTREE_LIST" "$DELIVERY_WORKTREE_SAFETY" \
  --expected-generation "$DELIVERY_EXPECTED_GENERATION" \
  --expected-digest "$DELIVERY_EXPECTED_DIGEST" \
  --expected-device "$DELIVERY_EXPECTED_DEVICE" \
  --expected-inode "$DELIVERY_EXPECTED_INODE"
```

Raw Git has no canonical wrapper create output, so omitting `--create-output`
and binding null `create_output`/producer `result_sha256` is the sole normal
exception. The wrapper-self list command parses the complete NUL-delimited Git
inventory, includes both primary and linked wrapper worktrees under physical
checkout `atrinik`, and retains its JSON bytes without reconstructing managed
paths. Observation and atomic bind-CAS remain mandatory; generic `cas` and
caller-authored safety or candidate documents cannot bind this worktree.

## Create a fresh PR ledger

This is one complete valid generation-1 PR-mode input with no explicitly
supplied issue for the manifest mapping `client` -> `atrinik/client`. It adopts
the existing same-repository PR, observes a wholly contributor-owned body, and
plans an absent local branch plus a request-bound local worktree. The exact
alternate genesis shapes for an existing branch or registered worktree follow.

```json
{
  "actor": {
    "login": "zoeyrose",
    "node_id": "U_actor",
    "push_repository_node_ids": [
      "R_repo"
    ]
  },
  "artifacts": [
    {
      "current": null,
      "immutable": {
        "body_digest": null,
        "branch": "docs/issue-419",
        "node_id": null,
        "number": null,
        "path": null,
        "repository": {
          "name": "client",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "kind": "branch",
      "producer_resource_slot": null,
      "safety": null,
      "slot_id": "branch",
      "state": "planned"
    },
    {
      "current": {
        "body_digest": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        "branch": "docs/issue-419",
        "head_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "node_id": "P_pr",
        "number": 423,
        "path": null,
        "repository": {
          "name": "client",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "immutable": {
        "body_digest": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        "branch": "docs/issue-419",
        "node_id": "P_pr",
        "number": 423,
        "path": null,
        "repository": {
          "name": "client",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "initial_body_payload": null,
      "kind": "pull_request",
      "producer_resource_slot": null,
      "safety": {
        "active": false,
        "certain": true,
        "clean": true,
        "detached": false,
        "foreign": false,
        "locked": false,
        "unowned_reference": false
      },
      "slot_id": "pull-request",
      "state": "adopted"
    },
    {
      "current": null,
      "immutable": {
        "body_digest": null,
        "branch": "docs/issue-419",
        "node_id": null,
        "number": null,
        "path": null,
        "repository": {
          "name": "client",
          "node_id": "R_repo",
          "owner": "atrinik"
        }
      },
      "kind": "worktree",
      "primitive_request": {
        "branch": "docs/issue-419",
        "component": "client",
        "expected_head_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "label": "issue-419",
        "physical_checkout": "client",
        "repository": {
          "name": "client",
          "node_id": "R_repo",
          "owner": "atrinik"
        },
        "roots": {
          "primary": {
            "device": 1,
            "inode": 103,
            "path": "/workspaces/atrinik/client"
          },
          "workspace": {
            "device": 1,
            "inode": 102,
            "path": "/workspaces/atrinik/workspace"
          },
          "wrapper": {
            "device": 1,
            "inode": 101,
            "path": "/workspaces/atrinik"
          }
        }
      },
      "primitive_result": null,
      "producer_resource_slot": null,
      "safety": null,
      "slot_id": "worktree",
      "state": "planned"
    }
  ],
  "authority": {
    "actor_node_id": "U_actor",
    "allowed": {
      "issues": [],
      "pull_requests": [
        "P_pr"
      ],
      "repositories": [
        "R_repo"
      ]
    },
    "issued_at": "2026-08-14T18:00:00Z",
    "kind": "explicit-invocation",
    "objective_sha256": "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
    "reference": "turn:pr-423"
  },
  "closing_scope": [],
  "entry_mode": "pr",
  "generation": 1,
  "history": [],
  "issues": {
    "explicit": [],
    "incidental": []
  },
  "ledger_id": "delivery-v1:pr:P_pr",
  "migration": null,
  "previous_byte_digest": null,
  "program": null,
  "resources": [],
  "schema_version": 1,
  "selected_prs": [
    {
      "author_node_id": "U_contributor",
      "base_branch": "main",
      "body": {
        "current_digest": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        "intended_digest": null,
        "intended_payload": null,
        "observed_digest": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        "outside_digest": "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        "ownership": "contributor-owned",
        "section_digest": null,
        "state": "observed",
        "updated_at": "2026-08-14T18:00:00Z"
      },
      "comment": {
        "current_digest": null,
        "intended_digest": null,
        "intended_payload": null,
        "marker": null,
        "node_id": null,
        "state": "none"
      },
      "draft": true,
      "draft_intent": null,
      "head_branch": "docs/issue-419",
      "head_repository": {
        "name": "client",
        "node_id": "R_repo",
        "owner": "atrinik"
      },
      "node_id": "P_pr",
      "number": 423,
      "repository": {
        "name": "client",
        "node_id": "R_repo",
        "owner": "atrinik"
      }
    }
  ],
  "targets": [
    {
      "base": {
        "branch": "main",
        "current_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "initial_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "lineage": [
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        ]
      },
      "head": {
        "branch": "docs/issue-419",
        "current_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "initial_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "lineage": [
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        ]
      },
      "merge_base": {
        "current_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "initial_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      },
      "repository": {
        "name": "client",
        "node_id": "R_repo",
        "owner": "atrinik"
      }
    }
  ]
}
```

The PR artifact is always `adopted`. Local branch/worktree state has exactly
these genesis alternatives:

| Proven local state | Branch artifact | Worktree artifact |
| --- | --- | --- |
| Remote PR head exists but its local branch and worktree are absent | `planned` | `planned` |
| Exact local branch exists but no worktree exists | `adopted` | `planned` |
| Exact local branch and exact safe wrapper-registered worktree exist | `adopted` | `adopted` |

An adopted worktree therefore requires an adopted branch. Every adopted
artifact has complete current identity and all safety flags proven safe;
planned artifacts have null current identity and safety. No artifact is
`created` at fresh PR genesis. There is no scope resource, and every other
resource is planned. The adopted PR has null `initial_body_payload`; the
contributor body is observed with no owned section and comment state is none.
After genesis, delivery may separately plan its terminal section without
claiming the outside contributor bytes. Authority kind is `durable-goal` or
`explicit-invocation`, never recovery authority. Base and head each begin with
singleton lineages at their respective
live commits, and merge-base initial/current are the same one proven live merge
base. Its canonical filename is `atrinik-client-pr-423.md.ledger.json`. An
explicitly supplied verified issue may occupy `issues.explicit`; preserve it in
`closing_scope` only when the live association is closing, while a verified
non-closing association leaves that array empty. Incidental issues remain
read-only and never enter authority merely because the PR links them.

For this manifest-owned deferred primitive worktree, snapshot and precommit the
live wrapper/workspace/primary identities, create the ledger first, and perform
only its exact recorded local branch/attachment operation. Capture its stdout
and a fresh complete wrapper worktree list without normalization. From one fresh
`inspect`, set the four `DELIVERY_EXPECTED_*` values used below:

```sh
DELIVERY_WORKTREE_LIST=/tmp/atrinik-pr-423-worktrees.json
DELIVERY_WORKTREE_SAFETY=/tmp/atrinik-pr-423-worktree-safety.json
DELIVERY_WORKTREE_BINDING=/tmp/atrinik-pr-423-worktree-binding.json
DELIVERY_WORKTREE_OUTPUT=/tmp/atrinik-pr-423-worktree-create.out
DELIVERY_OBSERVED_AT=2026-08-14T18:05:00Z
DELIVERY_PRIMARY_ROOT=/workspaces/atrinik/client
DELIVERY_HEAD_SHA=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
git -C "$DELIVERY_PRIMARY_ROOT" fetch --no-tags origin refs/heads/docs/issue-419
test "$(git -C "$DELIVERY_PRIMARY_ROOT" rev-parse --verify \
  --end-of-options 'FETCH_HEAD^{commit}')" = "$DELIVERY_HEAD_SHA"
git -C "$DELIVERY_PRIMARY_ROOT" branch --no-track -- \
  docs/issue-419 "$DELIVERY_HEAD_SHA"
"$DELIVERY_WRAPPER_ROOT/atrinik" worktree create client issue-419 \
  --branch docs/issue-419 --existing > "$DELIVERY_WORKTREE_OUTPUT"
"$DELIVERY_WRAPPER_ROOT/atrinik" worktree list --json > "$DELIVERY_WORKTREE_LIST"
python3 scripts/delivery_ledger.py worktree-observe \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" worktree \
  "$DELIVERY_WORKTREE_LIST" "$DELIVERY_OBSERVED_AT" \
  --create-output "$DELIVERY_WORKTREE_OUTPUT" \
  > "$DELIVERY_WORKTREE_SAFETY"
python3 scripts/delivery_ledger.py worktree-bind-cas \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" worktree \
  "$DELIVERY_WORKTREE_LIST" "$DELIVERY_WORKTREE_SAFETY" \
  --create-output "$DELIVERY_WORKTREE_OUTPUT" \
  --expected-generation "$DELIVERY_EXPECTED_GENERATION" \
  --expected-digest "$DELIVERY_EXPECTED_DIGEST" \
  --expected-device "$DELIVERY_EXPECTED_DEVICE" \
  --expected-inode "$DELIVERY_EXPECTED_INODE" \
  > "$DELIVERY_WORKTREE_BINDING"
```

Set `DELIVERY_PRIMARY_ROOT` from the ledger's exact precommitted
`roots.primary.path`, never by reconstructing a managed checkout path. The
fetch must return the still-live verified PR head. The equality check stops on
head drift, and non-forcing `git branch` stops if the supposedly absent local
branch appeared; only then may `--existing` attach it. For the genesis shape
whose exact local branch is already adopted, omit these three branch-creation
commands and reprove that its tip still equals `DELIVERY_HEAD_SHA` before the
attachment.

Proceed on `classification: bind-exact`; an exact completed retry returns
`bound-match`. `worktree-bind-cas` admits no candidate document: under pinned
roots and workspace leases it rechecks the manifest mapping, canonical remote,
Git common directory, registration, branch, head, clean state, absent ownership
references, exact helper-produced evidence, and safety immediately before it
constructs and installs the sole next generation. Its `snapshot` is the
installed ledger identity/document. It binds the planned branch at the
unchanged recorded head when needed and retains the exact list/safety bytes in
`primitive_result`. Current-main worktree creation emits the exact one-line
managed path: retain its stdout and pass `--create-output FILE`; producer
`result_sha256` equals that output digest. Never reconstruct the path or
deliberately discard successful create output.

After a completed bind, identical retained evidence classifies `bound-match`.
If recovery proves creation completed but its stdout was genuinely not
retained, do not create again: capture a new exact worktree list, run
`worktree-observe` and `worktree-bind-cas` without `--create-output`, and accept
only null `create_output` and null producer `result_sha256`. An absent registered
worktree permits only the originally planned operation; changed roots, head,
branch, checkout, label, path, registration, evidence, or safety stops.

## Provision a scope-produced worktree

New scope production is fresh issue-mode only. Its planned resource must be
present in the generation-1 create genesis and cannot be appended later. A
provenance-complete issue migration may only record an already-existing exact
scope. The ledger contract supports at most one scope resource per physical
repository. Each scope produces exactly one branch/worktree pair in that same
repository; the sole selector may resolve to multiple logical components in
that checkout.
Cross-checkout scope production, PR-mode adoption, and wrapper-self are
unsupported here and use the skill's primitive path.

For a manifest-owned `atrinik/client#419` delivery, first derive a complete
issue ledger from the wrapper-self example by replacing every issue, closing,
artifact, and target repository coordinate with the proven `atrinik/client`
identity and using canonical filename
`atrinik-client-issue-419.md.ledger.json`. Before `scope create`, replace that
ledger's primitive worktree with this complete planned object and replace
`resources` with this complete array:

```json
{
  "current": null,
  "immutable": {
    "body_digest": null,
    "branch": "docs/issue-419",
    "node_id": null,
    "number": null,
    "path": null,
    "repository": {
      "name": "client",
      "node_id": "R_repo",
      "owner": "atrinik"
    }
  },
  "kind": "worktree",
  "primitive_request": null,
  "primitive_result": null,
  "producer_resource_slot": "scope",
  "safety": null,
  "slot_id": "worktree",
  "state": "planned"
}
```

```json
[
  {
    "current": null,
    "immutable": {
      "name": "issue-419-scope",
      "path": null,
      "repository": {
        "name": "client",
        "node_id": "R_repo",
        "owner": "atrinik"
      }
    },
    "kind": "scope",
    "request": {
      "branch": "docs/issue-419",
      "component": "client",
      "label": "issue-419",
      "name": "issue-419-scope",
      "physical_checkout": "client",
      "profile": "default",
      "roots": {
        "primary": {
          "device": 1,
          "inode": 103,
          "path": "/workspaces/atrinik/client"
        },
        "workspace": {
          "device": 1,
          "inode": 102,
          "path": "/workspaces/atrinik/workspace"
        },
        "wrapper": {
          "device": 1,
          "inode": 101,
          "path": "/workspaces/atrinik"
        }
      },
      "start_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "state_policy": {
        "lifecycle": "remove-on-clean-stop",
        "mode": "temporary",
        "name": null,
        "ownership": "topology-generation"
      },
      "temporary_state": true,
      "topology": "scope-issue-419-scope"
    },
    "slot_id": "scope",
    "state": "planned"
  }
]
```

After durable create and a final collision recheck, provision exact intent:

```sh
DELIVERY_SCOPE_JSON=/tmp/atrinik-issue-419-scope.json
"$DELIVERY_WRAPPER_ROOT/atrinik" scope create client --name issue-419-scope --from default \
  --label client=issue-419 \
  --branch client=docs/issue-419 \
  --start-point client=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
  --temporary-state --json > "$DELIVERY_SCOPE_JSON"
```

The generation-1 scope resource's immutable `request` precommits its exact
name, one component/physical checkout, base profile, label, branch, start
commit, temporary-state flag and complete policy, topology, and live
wrapper/workspace/primary directory identities. Require successful create exit
and preserve its capture. Repeat the exact command for an idempotent completed
resume; partial/released/mismatched state stops.

Use `scope show`, not an internal path, as the identity surface. From one fresh
`inspect`, set the four `DELIVERY_EXPECTED_*` values used below:

```sh
DELIVERY_SCOPE_SHOW=/tmp/atrinik-issue-419-scope-show.json
DELIVERY_SCOPE_WORKTREES=/tmp/atrinik-issue-419-scope-worktrees.json
DELIVERY_SCOPE_SAFETY=/tmp/atrinik-issue-419-scope-safety.json
DELIVERY_SCOPE_BINDING=/tmp/atrinik-issue-419-scope-binding.json
DELIVERY_SCOPE_OBSERVED_AT=2026-08-14T18:05:00Z
"$DELIVERY_WRAPPER_ROOT/atrinik" scope show issue-419-scope --json > "$DELIVERY_SCOPE_SHOW"
"$DELIVERY_WRAPPER_ROOT/atrinik" worktree list --json > "$DELIVERY_SCOPE_WORKTREES"
python3 scripts/delivery_ledger.py scope-observe \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" scope \
  "$DELIVERY_SCOPE_SHOW" "$DELIVERY_SCOPE_WORKTREES" \
  "$DELIVERY_SCOPE_OBSERVED_AT" > "$DELIVERY_SCOPE_SAFETY"
python3 scripts/delivery_ledger.py scope-bind-cas \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" scope \
  "$DELIVERY_SCOPE_SHOW" "$DELIVERY_SCOPE_WORKTREES" "$DELIVERY_SCOPE_SAFETY" \
  --expected-generation "$DELIVERY_EXPECTED_GENERATION" \
  --expected-digest "$DELIVERY_EXPECTED_DIGEST" \
  --expected-device "$DELIVERY_EXPECTED_DEVICE" \
  --expected-inode "$DELIVERY_EXPECTED_INODE" \
  > "$DELIVERY_SCOPE_BINDING"
```

Proceed on `classification: bind-exact`; an exact completed retry returns
`bound-match`. The helper parses the retained bytes as the exact current-main
schema-v1 scope result, recomputes its canonical `request_sha256`, and requires
one matching checkout/repository/logical component/label/branch/start
commit/tree/path plus exact profile, topology, temporary state policy, commands,
and cleanup coordinates. Under pinned roots and workspace leases,
`scope-bind-cas` revalidates the live scope, wrapper list, Git state, and exact
`scope:NAME` plus `profile:scope-NAME` references immediately before its
internal CAS; any topology or other reference blocks. It also proves the live
profile file's exact retained digest/device/inode and absence of a release
journal. The creation journal is deliberately non-authoritative; cleanup
coordinates are compared from the scope record. It rejects internal path
reconstruction, extra rows, a released/partial result, and any request or target
head mismatch. The helper-produced safety observation has producer kind
`scope` and `result_sha256` equal to the raw scope-show digest. Its list digest,
safe flags, root identities, and scope-result worktree device/inode must all
match live.

The atomic command admits no candidate document and returns the installed
ledger in `snapshot`. The resource's first observation is generation 1, active,
with empty history; `external_generation` is the exact top-level lowercase
32-hex scope generation, `identity_digest` is SHA-256 of the raw `scope show`
stdout, and `binding` retains those exact bytes as canonical base64/digest. Its
`observation` retains the exact worktree-list and safety bytes. The branch and
worktree bind created/safe at the unchanged start head and exact returned path.
No partial scope-only, branch-only, or worktree-only bind is valid.

The producer slot on the worktree and scope request never change. An `active`
scope is safe only while the retained binding, exact record/result identities,
returned resources, worktree safety, and absence of release remain proven.
Preserve it while the PR is open. A later separately authorized release
advances the resource observation and transitions lifecycle to terminal
`released`, which retains its first binding digest in history and is historical
and non-reusable.

## Create, inspect, and update

Author the complete input as a bounded regular file. Validate it before any
mutation, inventory again, then create:

```sh
DELIVERY_INPUT=/tmp/atrinik-issue-419-ledger.json
python3 scripts/delivery_ledger.py prepare "$DELIVERY_INPUT"
python3 scripts/delivery_ledger.py inventory "$DELIVERY_REVIEW_ROOT"
python3 scripts/delivery_ledger.py create "$DELIVERY_REVIEW_ROOT" "$DELIVERY_INPUT"
python3 scripts/delivery_ledger.py inspect "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER"
```

`create` accepts only a generation-1 genesis appropriate to its entry mode. It
uses a candidate-digest-named stage and no-clobber publication. An exact retry
returns the same snapshot and completes safe staging cleanup; a different
candidate, collision, legacy claim, overlap, or unrelated pending transaction
stops.

For every update, start from one `inspect` result. Copy its document, apply one
legal transition, increment generation, set `previous_byte_digest` to the
returned digest, append that digest to `history`, and leave every other field
exact. Validate the replacement, then supply all four snapshot identity values:

```sh
DELIVERY_REPLACEMENT=/tmp/atrinik-issue-419-ledger-g2.json
DELIVERY_EXPECTED_GENERATION=1
DELIVERY_EXPECTED_DIGEST=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
DELIVERY_EXPECTED_DEVICE=2049
DELIVERY_EXPECTED_INODE=100001
python3 scripts/delivery_ledger.py prepare "$DELIVERY_REPLACEMENT"
python3 scripts/delivery_ledger.py cas \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" "$DELIVERY_REPLACEMENT" \
  --expected-generation "$DELIVERY_EXPECTED_GENERATION" \
  --expected-digest "$DELIVERY_EXPECTED_DIGEST" \
  --expected-device "$DELIVERY_EXPECTED_DEVICE" \
  --expected-inode "$DELIVERY_EXPECTED_INODE"
```

The values above illustrate shell shape only; copy actual values from the same
`inspect` output. Never combine values from different observations. CAS locks,
re-inventories, checks generation/digest/device/inode again immediately before
atomic replace, and fsyncs. Before rename, all four expected identity values
are mandatory concurrency guards. It also creates a durable hard-link receipt
whose name encodes ledger, old generation/digest/device/inode, and candidate
digest before rename:

```text
.LEDGER.update-proof-gOLDGEN-from-OLDDIGEST-dOLDDEV-iOLDINODE-to-CANDIDATE.tmp
```

If a crash leaves the replacement installed, an
exact retry succeeds only while that exact four-part predecessor receipt is
still a two-link inode with the installed candidate bytes; the helper then
removes it. A normal successful CAS removes the receipt before returning, so a
later retry with the old tuple stops and the operator must inspect the new
snapshot. Any receipt name, inode, bytes, tuple, or candidate mismatch stops.

Before reuse, live-inspect every artifact/resource and CAS its current identity,
lifecycle, and safety. Only then run:

```sh
python3 scripts/delivery_ledger.py check-reuse \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" --kind all
```

This command proves recorded state, not live state by itself. Planned artifacts
are ignored because they are not being reused. Every resource must be bound;
use `--kind artifacts` or `--kind resources` only when deliberately checking
that subset.

For target advancement, first CAS-cancel pending ready/body/comment intent.
Then refetch, prove descendant lineages, recompute merge base, CAS the new
coordinates, and restart affected review, validation, and checks. Never encode a
force-push/rebase/retarget as lineage.

## Bind a created PR

Issue-mode genesis pre-records one planned PR artifact per target with exact
repository, head branch, intended body digest, and immutable exact payload.
Immediately before the authorized remote create, recover only those durable
bytes and feed the decoded file to GitHub without normalization:

```sh
DELIVERY_PR_CREATE_PAYLOAD=/tmp/atrinik-issue-419-pr-create-payload.json
DELIVERY_PR_CREATE_BODY=/tmp/atrinik-issue-419-pr-create-body.md
python3 scripts/delivery_ledger.py pr-create-payload \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" pull-request \
  > "$DELIVERY_PR_CREATE_PAYLOAD"
jq -r '.body_payload.raw_base64' "$DELIVERY_PR_CREATE_PAYLOAD" | \
  base64 --decode > "$DELIVERY_PR_CREATE_BODY"
```

Require the returned body digest to equal the slot's immutable digest. Never
create from transient or reconstructed bytes. After the remote create, capture
this complete bounded identity object from live GitHub data:

```json
{
  "body_digest": "d11168a907f543b3d6a169d182cb58d16fb7099bde2845c90f78a5d912d82e55",
  "head_branch": "docs/issue-419",
  "head_sha": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "node_id": "P_pr",
  "number": 423,
  "repository": {
    "name": "atrinik",
    "node_id": "R_repo",
    "owner": "atrinik"
  }
}
```

Classify before adopting or retrying creation:

```sh
DELIVERY_PR_IDENTITY=/tmp/atrinik-issue-419-pr-identity.json
python3 scripts/delivery_ledger.py bind-check \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" pull-request \
  "$DELIVERY_PR_IDENTITY"
```

Proceed only on `classification: bind-exact`. CAS the planned PR artifact to
`created`, using the exact candidate as current identity, the target head, and
freshly proven artifact safety. Append exactly one selected PR. Its repository
and head repository are identical; its base/head match the wholly unchanged
target; its authenticated actor is the author; it remains draft with null ready
intent; its comment state is none; and its `delivery-created`/`written` body has
null observed/intended digest/payload, null section, and equal current/outside
digest matching the planned slot. Its PR artifact permanently retains the
initial payload. This exception applies only while immutable
`authority.allowed.pull_requests` is empty. The helper does not expand or
rewrite authority and never adopts this result.

This CAS may change only ledger generation/history, that one PR slot, and that
one matching selected PR. Normalizing generation/history, removing the added
PR, and restoring the slot must reproduce the entire predecessor document;
target drift, resource changes, another slot/PR/body/comment/readiness change,
or any root-coordinate change fails. Refetch and inspect after CAS.

If remote creation returned an error or timed out, search all candidate PRs and
run `bind-check` on the sole possible match, comparing the durable initial
payload digest. Zero is not permission to retry an uncertain create; multiple
or mismatched candidates stop. `pr-create-payload` reconstructs intent for
recovery but never authorizes another uncertain create. PR mode never uses this
path because its selected PR and adopted slot exist at genesis.

## Plan and recover body updates

Contributor bytes outside a delivery section remain byte-for-byte read-only.
Keep a wholly contributor body `observed`; a genuine contributor edit may
refresh observed/current/outside together only with a strictly later live
timestamp and matching PR artifact digest. The same digest with a later
PR-level timestamp is also an observation-only refresh. Neither grants mutation
authority, and a copied live marker never grants ownership.

For any intent-free observed/written body, capture exact raw current bytes and
the desired delivery section as bounded regular files. `body-plan` can first
append an independently owned section to contributor bytes or replace an
already owned section:

```sh
DELIVERY_BODY=/tmp/atrinik-pr-423-body.bin
DELIVERY_SECTION=/tmp/atrinik-pr-423-section.md
DELIVERY_BODY_PLAN=/tmp/atrinik-pr-423-body-plan.json
python3 scripts/delivery_ledger.py body-check \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" P_pr "$DELIVERY_BODY"
python3 scripts/delivery_ledger.py body-plan \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" P_pr \
  "$DELIVERY_BODY" "$DELIVERY_SECTION" > "$DELIVERY_BODY_PLAN"
```

`body-plan` requires the input digest to equal ledger current bytes and both
inputs to be valid UTF-8. It inserts or replaces one helper-derived
`<!-- atrinik-delivery:body:TOKEN:start -->` / `:end -->` section with canonical
newline framing, rejects any reserved namespace in the payload, preserves every
outside byte, and rejects a no-op. It returns body/section/outside digests,
`body_base64`, and the complete `body` object ready for CAS. That object retains
the full intended UTF-8 payload and changes contributor ownership only to the
separate `delivery-section` boundary. CAS the returned `body` byte-for-value and
leave the PR artifact at its recorded current body digest before any edit.
Decode only the durable payload after that CAS:

```sh
DELIVERY_INTENDED_BODY=/tmp/atrinik-pr-423-body-intended.bin
jq -r '.body.intended_payload.raw_base64' "$DELIVERY_BODY_PLAN" | \
  base64 --decode > "$DELIVERY_INTENDED_BODY"
sha256sum "$DELIVERY_INTENDED_BODY"
```

Refetch body bytes and `updatedAt` together immediately before mutation, hash
the exact bytes, and classify both values after the intent CAS and after every
interruption:

```sh
python3 scripts/delivery_ledger.py body-recovery \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" P_pr \
  LIVE_BODY_SHA256 LIVE_BODY_UPDATED_AT
```

| Result | Action |
| --- | --- |
| `apply-intended` | Live digest/timestamp exactly equal recorded current observation. Apply only the returned durable intended payload once, or cancel with this exact non-application proof. |
| `refresh-intent-observation` | Live digest still equals current but the PR-level timestamp is later. CAS the returned body/artifact projection only, refetch, and reclassify before write or cancellation. |
| `bind-intended` | Live digest equals intended and timestamp is equal or later. Do not edit again; CAS the returned written body and artifact digest. |
| `refresh-observation` | Intent-free contributor observation changed at a later timestamp, or exact written bytes have a later timestamp. CAS only the returned observation projection; make no remote write. |
| `none` | Written live digest and timestamp match the recorded observation; no mutation. |
| `read-only-match` | Contributor digest and timestamp match their observation; use comments only. |

Always pass both values from one live fetch; digest-only recovery is forbidden
because an equal byte string can hide an intervening edit. Any other digest/
timestamp pair—including a backward timestamp or third digest for an owned
body—stops. After editing, refetch, run `body-check`, and require exact intended
digest, outside digest, and semantic timestamp; `body-recovery` must return
`bind-intended`. CAS its body/artifact projection, then inspect rendered GFM.

Cancellation is a separate CAS before any coordinate drift and only after
`body-recovery` proves exact current bytes were not replaced. Clear both
intended digest and payload. A never-applied first delivery-section plan returns
to the exact contributor observation; another plan returns to `written` with
its recorded current/outside/section identity. Never cancel after intended or
unclassified bytes, and never reconstruct a payload from transient files.

## Plan and recover comments

Use one helper-derived marked PR comment when the body is contributor-owned or
a concise status surface is needed. Fetch every comment page and write exactly:

```json
{
  "comments": [],
  "pagination_complete": true
}
```

Each nonempty `comments` entry has exactly `node_id`, `author_node_id`, `body`.
Run:

```sh
DELIVERY_COMMENTS=/tmp/atrinik-pr-423-comments.json
python3 scripts/delivery_ledger.py comment-check \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER" P_pr "$DELIVERY_COMMENTS"
```

With ledger state none and no live match, `plan-required` returns the exact
marker. Construct UTF-8 comment bytes containing that marker exactly once,
as the first line followed by a newline; compute their SHA-256 and canonical
inline payload. Then CAS comment none to planned with marker/intended digest and
payload and null node/current. For a bound update, retain marker/node/current
and set a new different intended digest/payload. Refetch all pages and rerun
`comment-check`; only `mark-in-flight-before-write` permits CAS of the identical
plan to `in-flight`. Persist that CAS, decode only its returned
`intended_payload`, and write those exact bytes.

After the remote result, or after any interruption, refetch all pages and run
the classifier again:

| Result | Action |
| --- | --- |
| `bind-observed` | Intended bytes exist once under the actor; CAS bound with returned node/current and null intended digest/payload. |
| `apply-intended` | A known bound node still has old bytes; update that node once with the returned durable intended payload. |
| `bound-match` | One actor-owned node matches bound identity; no write. |

Wrong author, malformed/foreign marker namespace, duplicate matches, incomplete
pagination, disappearance, or unexpected bytes stop. A first-post in-flight
state with no live intended match is intentionally unretryable: it may represent
an uncertain POST, so wait for a later complete inventory or require new
authority rather than posting again. Only an exact complete `comment-check`
proving a planned write was not applied permits a separate pre-drift cancel: a
never-started first plan returns to none and a planned bound update returns to
bound, clearing digest and payload. Never cancel in-flight.

## Migrate prior evidence

Migration is the only way to establish schema-v1 ownership after artifacts
already exist. It does not infer ownership from timestamps, links, reflogs,
authorship, or push access. Use only:

- `legacy` for the sole exact issue-mode
  `OWNER-REPOSITORY-NUMBER.md` report; or
- `pre-schema` for the canonical
  `OWNER-REPOSITORY-issue-NUMBER.md` or
  `OWNER-REPOSITORY-pr-NUMBER.md` report.

The candidate is a complete generation-1 document with null migration. Unlike
fresh create, it records all exact current selected PRs, artifacts, resources,
bodies, comments, safety, and target coordinates. Authority must be a durable
authenticated goal that predates and continuously owns every recorded creation,
or `explicit-recovery` naming the exact artifacts and current coordinates.

For legacy, the report parser's exact issue URLs, PR URLs, target repository/head
rows, and worktree paths must equal the candidate; extra or missing claims stop.
Every unpaired recognized report reserves typed coordinates from its canonical
filename even when empty or unparseable: issue-mode and mode-less names reserve
that issue against direct ownership and program-master use; PR-mode names reserve
only that selected/known PR. Parsed contents add their exact issue, PR,
repository/head, and worktree reservations; those worktree claims are compared
against bound paths and precommitted deferred primitive/scope managed paths. A
reservation blocks overlap but never grants authority. Legacy migration is
issue-mode only. Pre-schema source must be the exact canonical Markdown report.
Compute and preserve its current digest, then invoke:

```sh
DELIVERY_MIGRATION_INPUT=/tmp/atrinik-issue-419-migration.json
DELIVERY_SOURCE=atrinik-atrinik-issue-419.md
DELIVERY_SOURCE_SHA256=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
python3 scripts/delivery_ledger.py prepare "$DELIVERY_MIGRATION_INPUT"
python3 scripts/delivery_ledger.py migrate \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_SOURCE" "$DELIVERY_MIGRATION_INPUT" \
  --kind pre-schema \
  --expected-source-digest "$DELIVERY_SOURCE_SHA256"
python3 scripts/delivery_ledger.py inventory "$DELIVERY_REVIEW_ROOT"
python3 scripts/delivery_ledger.py inspect \
  "$DELIVERY_REVIEW_ROOT" "$DELIVERY_LEDGER"
```

For legacy, set `DELIVERY_SOURCE=atrinik-atrinik-419.md` and `--kind legacy`.
Always substitute the actual source SHA-256. The helper durably publishes a
candidate-digest-named planned stage, immutable source snapshot, canonical
report copy when legacy, prepared marker, canonical ledger, and complete
marker. Here the candidate-bound suffix is the complete operation digest, not
the candidate byte digest alone. Its planned-stage pattern is:

```text
.LEDGER.migration.json.planned-OPERATION_SHA256.tmp
```

The operation digest hashes the complete canonical planned marker, binding
migration kind, candidate digest, source name/digest/device/inode, snapshot,
canonical report, and destination before even a short durable prefix can exist.
A different candidate or source identity can never resume or take over that
stage. The helper adds exact complete migration metadata to the installed
generation-1 ledger. Never add that metadata to input; its
predecessor/history fields remain null/empty.

Rerun the identical command with identical input, kind, source name, and
expected original digest after an interruption. Do not delete or rename any
stage. A completed legacy source and its immutable snapshot remain frozen. A
completed pre-schema human report may later change; its frozen snapshot,
complete marker, and ledger history retain the migration anchor. Loss or change
of a required member stops.

## Recover interrupted transactions

Run `inventory` first. If it succeeds with a recognized pending operation,
resume only the exact operation shown below. If inventory itself rejects debris,
identity, ownership, bounds, links, modes, or overlap, preserve everything and
stop for code-level recovery; never improvise file repair.

| Operation/state | Safe recovery |
| --- | --- |
| `create` stage only, including a durable short prefix | Rerun exact `create` with byte-identical candidate. It completes the deterministic stage. |
| `create` target plus two-link stage | Rerun exact `create`; it proves identical inode/bytes and removes only the stage link. |
| `create` target only | `inspect`; exact create retry is idempotent. Different bytes stop. |
| CAS stage before rename, including a durable short prefix | Rerun exact `cas` with identical replacement and original four-part expected tuple. |
| CAS target already replaced with its two-link update receipt | Rerun exact CAS with identical replacement and four-part predecessor tuple; the receipt name preserves generation/digest/device/inode and its shared inode proves the installed candidate. The helper removes the receipt. |
| Atomic worktree/scope bind interrupted at either CAS boundary | Rerun the identical `worktree-bind-cas` or `scope-bind-cas` with the same retained inputs and original four-part predecessor tuple, never generic `cas`. The helper freshly reproves live state before replacement and before accepting an installed post-rename receipt; drift preserves evidence and stops. |
| Migration operation-digest plan/snapshot/report/prepared/ledger/complete boundary | Rerun exact `migrate` with identical null-migration candidate, kind, direct source name, and original source identity/digest. A different candidate or source cannot reuse even a short planned-stage prefix. |
| Complete migration | `inventory` and `inspect`; require source/snapshot/marker/ledger coherence and no pending stage. |
| Planned PR slot after remote create uncertainty | Recover the immutable initial bytes with `pr-create-payload`, search live candidates, and use `bind-check`; bind one exact match only. Zero or a mismatch never permits another uncertain create. |
| Planned primitive branch/worktree after an uncertain local mutation | Reinspect the exact branch and wrapper registration. For one present exact worktree, capture a fresh list, use `worktree-observe`, then run `worktree-bind-cas` with one fresh four-part tuple. Retain manifest-owner create output; omit it only for wrapper-self raw Git or genuinely unretained recovery. The atomic command reproves before CAS. An exact still-absent artifact permits only the original planned operation. Mismatch or uncertainty stops; an adopted worktree requires the branch already bound. |
| Fresh issue planned scope after uncertain `scope create` | Rerun the exact idempotent create request, retain exact scope-show/list bytes, derive safety with `scope-observe`, then run `scope-bind-cas` with one fresh four-part tuple. It must atomically install the one scope/branch/worktree result; partial, released, changed, referenced, or cross-checkout state stops. |
| Body `update-planned` | Run `body-recovery` with one digest/timestamp observation. Refresh a newer exact-current observation first; apply only returned durable bytes, bind equal-or-later intended bytes, or cancel only on exact proven non-application. Other live bytes stop. |
| Comment `planned` | Run `comment-check`; CAS unchanged digest/payload intent to in-flight before any write, or separately cancel only after exact non-application proof. |
| First comment `in-flight` | Fully paginate; bind one exact intended actor-owned result. No match is uncertain and cannot be retried. |
| Bound-comment update `in-flight` | Fully paginate; bind intended result, or update the exact known node if it still has recorded old bytes. |
| Ready intent with draft still true | Refetch; mark ready only at the exit gate, or CAS-cancel before target drift. |

Every operation inventories under an exclusive no-follow root lock. Mutations also use a persistent per-ledger lock for active ledgers and re-inventory inside
it. Terminal lifecycle transactions remain under the root lock while archiving
that persistent lock itself. Recognized staging participates in collision
detection; a pending operation for the same target blocks an unrelated
lifecycle candidate.

## Release, archive, and reclaim after merge

PR-ready handoff, a merged PR, and an issue closing do not grant ledger cleanup
authority. Begin only from a new explicit post-merge request. Keep the issue
delivery itself stopped before merge and issue closure.

Prepare release evidence with exactly `authority`, `observed_at`,
`pull_requests`, `issues`, `mutation_state`, and `cleanup`. The authority kind
is `explicit-post-merge`; its actor is the delivery actor, its single allowed
ledger ID is exact, and its PR/issue allowlists equal the selected coordinates.
Its objective digest is the canonical object digest of:

```json
{"ledger_id":"LEDGER_ID","ledger_sha256":"CURRENT_LEDGER_SHA256","operation":"release"}
```

It must postdate the active authority. Each PR row has exact `repository`,
`number`, `node_id`, `state: "merged"`, equal recorded/observed head SHAs,
`merge_commit_sha`, `merged_at`, and
`ancestry: {"method":"git-merge-base-is-ancestor","result":"ancestor"}`.
The helper re-observes every exact PR and issue through authenticated `gh`,
requires the authenticated actor to equal the ledger actor, pins the exact
clean recorded worktree and Git authority, and runs the ancestry check itself.
Apply repeats the remote and Git proof after staging before installation. Each selected explicit/program
issue appears once with `closed` when in closing scope and `open` otherwise.
Both mutation-state fields are `idle`; PR body/comment/readiness intents are
terminal, and every artifact is bound and safe. Running resources and mutable
active state/scenario resources block. A completed active scope may remain only
so the separately previewed cleanup can release it after ledger release.
The cleanup object is exactly:

```json
{
  "apply_command": "./atrinik cleanup --apply --json",
  "policy": "explicit-preview-first",
  "preview_command": "./atrinik cleanup --dry-run --json"
}
```

Run `release-preview`, review it, then pass its `plan_sha256` to
`release-apply`. The crash-resumable complete marker is installed and fsynced
before inventory excludes that ledger from overlap ownership. It retains the
exact ledger generation/digest/device/inode and all post-merge evidence.

Cleanup remains an independent wrapper operation. Run the recorded dry-run,
retain its raw digest and canonical selection digest, review it, then run the
same scoped apply and retain its raw digest. Delivery never invokes cleanup.
Cleanup remains responsible for refusing dirty, detached, locked, active,
referenced, foreign, uncertain, or mismatched worktrees/resources.

Only after cleanup may archive evidence have exact `authority`, `archived_at`,
`retain_until`, and `cleanup`. Each cleanup preview/apply row retains exact
`{output, observed_at}`; `output` is the canonical inline payload of the raw
wrapper JSON. The helper parses schema 1, requires `dry-run` then successful
`apply`, no inventory/error/abort state, and derives the target selection from
preview `eligible` and apply `removed`/`completed_actions` rows. Those exact
sets and their canonical digests must match. The authority kind is
`explicit-post-cleanup`, allows exactly the ledger ID, uses the delivery actor,
postdates release, and binds this canonical objective:

```json
{"ledger_id":"LEDGER_ID","operation":"archive","release_sha256":"RELEASE_SHA256"}
```

Cleanup apply is later than preview. Its sorted worktree rows exactly
match the ledger and say `removed`, retaining the last safe observation. Its
resource rows exactly match every slot and say `removed` or `retained`, with the
same terminal lifecycle. The one exception converts a ledger-recorded active
scope to `removed`/`released`, reflecting its intervening wrapper release.
That exception also requires the live scope subsystem's exact completed,
generation-matched release journal; a caller-authored summary is insufficient.
Archive preview/apply never removes those resources.
It instead bundles canonical ledger, release, lock, optional report, migration
marker/snapshot/source, and embedded intent bytes into one canonical bounded
archive before exact member unlinking. The installed archive makes interrupted
member removal resumable and remains inert audit evidence.

`reclaim-preview` derives current UTC itself and accepts one exact archive only
at or after `retain_until`. Apply rechecks helper time and the original
digest/device/inode. Review the returned bound plan and
feed the complete preview to `reclaim-apply`. Reclaim removes only that bundle;
it never follows a path or touches a worktree/resource. A bounded exact receipt
makes an interrupted post-unlink apply recoverable; unrelated absent-archive
requests fail. Use a retention period required by
project policy; never shorten it merely to clear inventory pressure.

## Observe strict prohibitions

- Never hand-create, edit, chmod, link, unlink, rename, truncate, or replace a
  ledger, release, archive, lock, stage, migration marker, snapshot, or managed
  report copy.
- Never bypass `inventory`, ignore an unexpected entry, combine identities from
  separate `inspect` calls, or reuse a stale CAS tuple.
- Never run fresh `create` after a delivery-owned external mutation. Use only a
  provenance-complete helper migration when justified.
- Never use migration, a live marker, a legacy report, authorship, push access,
  timestamps, comments, links, or reflogs as ownership authority by themselves.
- Never alter immutable authority/program/issue/closing coordinates, target
  set, target branch/initial anchors, artifact intent, bound disposition, or
  external scope generation through CAS.
- Never remove a resource slot. Append it planned before mutation, then bind an
  exact result. Never bind a running/released/consumed or otherwise unsafe
  resource as reusable.
- Never invent a worktree path for scope production, reconstruct wrapper
  internals, hash reserialized scope JSON, or use one scope across physical
  repositories.
- Never rewrite contributor bytes outside the helper-planned terminal section
  or claim ownership from a live marker. Never change a recorded outside digest.
- Never post/update a comment without planned then in-flight durable state,
  complete pagination, exact actor/marker checks, and post-write binding. Never
  retry an uncertain first POST.
- Never perform an initial PR, body, or comment remote write from bytes absent
  from its authoritative ledger intent. Decode the exact retained payload only.
- Never advance target coordinates with pending body, comment, or readiness
  intent. Never encode rewritten Git history as descendant lineage.
- Never put credentials, tokens, passwords, confidential data, or unnecessary
  vulnerability detail in inputs, ledgers, comments, bodies, reports, or logs.
- Never merge, close issues, force-push, apply cleanup from issue delivery, or
  treat this local state machine as additional GitHub authority. Never infer
  post-merge release/archive authority from a delivery goal or ready PR.
