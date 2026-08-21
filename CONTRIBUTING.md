# Contributing

Use Conventional Commits syntax for commits and pull-request titles:

~~~text
type(optional-scope): concise description
~~~

Examples include `fix(cli): reject mismatched checkout paths` and
`feat(agents): revise the profile schema`. Add the optional `!` marker only
when a reviewer explicitly requests a breaking change that should trigger the
next major release; agents should not add it automatically. Pull requests are
squash-merged, and the squash title becomes the release-driving commit on
`main`.

## Release branches

`main` is the forward development line. Every release-driving mainline commit
starts the next minor line: after `vX.Y.0`, a patch-like commit produces
`vX.(Y+1).0`. A maintenance branch uses the native semantic-release `X.Y.x`
form and is cut from the existing `vX.Y.0` tag, for example:

~~~sh
git switch --create 5.50.x v5.50.0
git push origin 5.50.x
~~~

The first fix on `5.50.x` publishes `v5.50.1`; later fixes publish
`v5.50.2`, `v5.50.3`, and so on. The baseline `v5.50.0` tag is never
recreated. Semantic-release constrains the maintenance line to its `X.Y.x`
range and keeps feature/breaking transitions from escaping it, so an
out-of-range or conflicting version is refused. The already-published `v8.0.1`
is preserved as historical evidence; do not rewrite its tag or release without
a separate recovery decision.

After a maintenance fix is released, forward-port it to `main` through an
explicit pull request. Merge the maintenance branch when its ancestry makes
that transfer clear; otherwise open a main-targeted pull request carrying the
equivalent change. Do not merge `main` back into a maintenance branch, because
that can introduce commits outside its patch range.

Write pull-request descriptions as renderable GitHub-Flavored Markdown with
actual line breaks, never visible literal `\n` separators. For a multi-section
body, prefer `gh pr create --body-file FILE` or `gh pr edit --body-file FILE`;
`--body-file -` reads standard input. After creating or editing a pull request,
inspect GitHub's rendered web view or rendered `bodyHTML`/`body_html`, not only
the raw body. Verify that headings, lists, inline code, issue-closing references,
and validation sections render normally.

Keep component implementation in its owning repository. Changes here should
remain focused on the manifest, multi-repository workflows, or their tests and
documentation. Preserve dirty physical checkouts and persistent state during
manual validation.

## Copyright headers

Use `The Atrinik Project` as the exact collective holder for every new or
updated blanket Atrinik copyright header. Existing blanket forms such as
`Atrinik contributors`, `Atrinik Development Team`, or bare `Atrinik` migrate
prospectively: do not churn untouched files, but normalize the holder when an
otherwise edited Atrinik-authored file already carries that blanket header.
`The Atrinik Project` is canonical because it already predominates in modern
MIT source headers; the other forms remain historical inventory, not templates
for new blanket attribution.

The exact blanket format is `Copyright START[-END] The Atrinik Project`: use a
single year when `START` is the current year and an ASCII-hyphenated range
otherwise. Only the surrounding comment delimiters vary by file format. When
normalizing an existing blanket form, omit `(C)`, `(c)`, `©`, commas, and
trailing punctuation so the notice matches this form exactly.

Whenever an Atrinik-authored file is edited, update each existing Atrinik-owned
copyright notice in the same change: retain its original start year and set its
terminal year to the current calendar year. For example, in 2026:

- `Copyright 2021-2024 The Atrinik Project` becomes
  `Copyright 2021-2026 The Atrinik Project`;
- `Copyright 2026 The Atrinik Project` remains a single-year notice;
- `Copyright 2024 The Atrinik Project` becomes
  `Copyright 2024-2026 The Atrinik Project`; and
- `Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team` becomes
  `Copyright (C) 2009-2026 Zoey Rose and Atrinik Development Team`.

The named-holder example deliberately retains its more precise mixed
attribution instead of replacing it with the canonical blanket holder. Always
preserve named holders, original start years, Crossfire, Daimonin and other
upstream notices, third-party attribution, SPDX identifiers, license terms,
and provenance text. Leave upstream and third-party notice years unchanged. Do
not add a header to a file that lacks one, and do not rewrite vendored,
imported, preserved-history, or third-party files under this rule. Update
generated headers through their authoritative generator or template rather
than editing generated output.

Repository `LICENSE` notice lines are a separate legal and attribution surface.
Do not normalize them as source headers; change one only through deliberate
repository-owned legal review.

The canonical `server`, `client`, `editor`, `protocol`, `renderer`,
`content-toolkit`, and `website` repositories form the MIT replacement stack.
Plain `./atrinik init` is replacement/default-only. Exact
`./atrinik init --with classic` adds the complete currently playable classic
cohort: the `atrinik/classic` monorepo checkout, the independent MIT
`atrinik/playtester` checkout, and tools that are MIT by default with a
GPL-2.0-or-later `map-checker-qt/` exception. Both stacks reuse the
one `content@main` checkout initialized by the default cohort; Classic selects
its target-specific publisher adapter. The
monorepo supplies the logical classic client, server, editor, protocol, and
libatrinik components from source subdirectories. The playtester remains
classic-only, has a wrapper `build: none` contract, and owns its installation
and validation in its physical repository. Do not put those checkouts in the
default cohort or mix replacement and classic providers in one runnable
profile. Replacement repositories have validated standalone M1
foundations; their wrapper build/runtime adapters and integrated service
closure have not landed, so current game integration uses a profile created
from `classic`.

Runtime cohort and Classic maintenance boundaries do not impose a blanket
clean-room rule. Under [`docs/PROVENANCE.md`](docs/PROVENANCE.md), exact,
independently separable Classic material proven to fall within an applicable
approved historical grant—including that row's temporal and
sole-original-authorship scope—may be inspected as source reference, copied,
migrated or ported, translated or adapted, and MIT-relicensed after the required
audit and
destination record. Later material needs contemporaneous compatible permission.
The Classic source and notices stay GPL; this does not approve a GPL dependency
or bundle, and any contribution needing permission but lacking it is excluded.

For a pre-split workspace, initialize only the destination with
`./atrinik init classic`, run `./atrinik migrate repositories --dry-run`
before apply, and finish with `--audit`. The checked migration combines proven
pre-monorepo repositories under `classic/`, preserves recoverable
originals and worktree state, refuses ambiguous or unsafe layouts, and rewrites
proven classic profiles atomically. States, builds, runtimes, and logs remain
outside the repository-layout migration. It uses integrated commit-map targets
when available and imports an exact verified local commit when a branch-only
target disappeared with the retired classic `history/*` namespace.

Before opening a pull request, run:

~~~sh
python3 -m pip install --requirement requirements-dev.txt
python3 -m coverage run -m unittest discover -v --durations 50
python3 -m coverage report --show-missing
python3 -m compileall -q atrinik atrinik_workspace tests
python3 -m atrinik_workspace.guidance_inventory --check
python3 -m atrinik_workspace.mcp_contract validate
./atrinik manifest validate
./atrinik provenance validate
./atrinik supply-chain validate
git diff --check
~~~

The required workflow partitions the same discovery set with measured timing
weights, verifies that every discovered test ran exactly once, and combines
branch coverage before publishing the stable `Integration validation` check.
See [CI performance](docs/CI_PERFORMANCE.md) for the budget, evidence format,
and comparable-run method.

Use [local test execution](docs/LOCAL_TESTING.md) for targeted, serial, or
process-isolated parallel runs before the complete validation recipe.

When changing the repository-local skill, also run the skill validator
available in the active Codex installation; its exact path is
environment-specific.

Exercise the smallest relevant real profile build as well. Changes to current
source-view, collection, runtime, or CMake composition should validate both
classic client and server with `--profile classic --test`. Replacement
components use their repository-owned aggregate validation today and remain
inspectable through wrapper manifest/profile contracts until wrapper build
adapters are implemented.

For lease-graph, profile publication, Git administration, or cleanup changes,
add process-rendezvous coverage for same-coordinate exclusion and fairness,
disjoint-coordinate overlap, immutable profile generations, descriptor
inheritance, reference-publication/removal races, and fail-closed diagnostics.
Run the cleanup dry-run required by `AGENTS.md`; never use cleanup apply as
validation.

Cleanup or repository-layout changes must also cover active issue-delivery
evidence: an active ledger protects its review root, report, lock, and
worktree; missing or unsupported evidence fails closed; and repository
migration refuses active ledgers without mutating them. Diagnostics must
surface delivery-inventory failure on the affected review root. Recovery tests
must prove that the wrapper does not synthesize missing ledger bytes or
authority, and that only a separately authenticated explicit-recovery grant
can select a provenance-preserving migration.

For development-scope changes, also cover distinct scopes sharing one physical
checkout, same-name/label/branch races, every publication boundary, exact JSON
handoff commands, state-policy opt-in, live-scope isolation, and hash-bound
release refusal for dirty, detached, referenced, replaced, busy, retained, or
ambiguous inputs. Scope release tests may apply only to temporary test-owned
workspaces; repository validation remains preview-only.

For delivery-ledger lifecycle changes, cover post-merge authority and exact
authenticated live PR/issue matching, post-stage clean-worktree ancestry,
exact squash integration, hostile GitHub CLI environment refusal, live wrapper
lease/reference/resource rechecks, intent/resource refusal,
release-before-inert ordering, cleanup preview/apply selection binding,
scope-release journal/plan/action/absence proof, quarantined archive member
identity, helper-clocked retention reclaim and terminal receipts, stale plans, and same-
coordinate crash/retry/concurrency at every publication boundary. Tests may
apply archive/reclaim only inside temporary test-owned review roots. Delivery
must never invoke wrapper cleanup, and repository validation remains
preview-only.

For CMake/cache changes, also repeat an unchanged build, exercise
`--force-reconfigure` and `--no-ccache`, inspect `ccache --show-stats` when the
command is installed, and preview shared-cache retention with
`./atrinik cleanup --scope compiler-cache --dry-run --json`.

For local-playtest sound staging changes, use a clean sound checkout that owns
the public playtest-tree builder. Run the focused wrapper sound fixtures, build
`classic-client` twice from the same Classic-derived local-playtest profile,
inspect matching build/topology sound records, and run the complete Classic
supply-chain audit. The generated tree remains ignored, nonpublishable local
state; never attach, package, upload, or commit it.
