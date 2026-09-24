# Recover a proven erroneous resource observation

Use only after the correction helper and its workspace proof support actually
merge. Candidate helpers are fixture-only; never run them against a paused real
delivery. This protocol does not transfer ownership, create resources, stop
processes, reset state, register aliases, or apply cleanup.

Normal observation order is: record original resource intent; obtain the exact
public scenario/state names, build JSON result and topology generation; preserve
those original bytes; then bind only their verified identities. A Docker label
is an external executor coordinate, not an alias for a wrapper state name.
Retain build JSON stdout separately from diagnostic stderr. A tested build can
use sealed source generations while `up` creates a distinct build from live
sources. Neither build inherits the other's test result.

## Correct the bounded prepublication shape

This bounded producer shape is a server-only topology; other service shapes
refuse. Require the existing canonical/native context, live authenticated actor and open
issue, complete collision inventory, clean bound targets, and exact canonical
ledger path/generation/digest. Keep original worktree storage and resource
workspace separate. Select the bound wrapper worktree with
`{"kind":"bound-wrapper-worktree","worktree_slot":"worktree"}`; the helper
proves its default `workspace` through accepted primary code and its pinned
manifest. Caller paths never select a resource namespace.

The request contains exactly:

- `state_slot`, `scenario_slot`, `build_slot`, `topology_slot`: four distinct
  original resource slots. The state, scenario and tested build are created
  observations with deferred paths; the topology remains genuinely planned/null.
- `planned_slots`: every remaining original planned/null client reference/runtime
  slot, sorted and unique. These records remain inert and byte-identical.
- `resource_context`: the bound-wrapper-worktree selector above.
- `planning_ledger`: exact canonical original planned ledger bytes whose digest
  occurs in the erroneous predecessor's history.
- `state_observation`, `scenario_output`, `build_observation`, `build_plan`,
  `build_result`, `build_log`, `topology_plan`, `topology_output`: original retained
  byte payloads, each using `{encoding,raw_base64,sha256}`. The three recorded
  observation digests must match. The state observation binds the actual scenario
  output; the build observation binds the original plan, JSON stdout result and
  separate diagnostic log. Do not reconstruct producer outputs.

```sh
python3 scripts/delivery_ledger.py correct-resource-observations-cas \
  REVIEW_ROOT LEDGER_NAME REQUEST_JSON \
  --expected-generation GENERATION --expected-digest SHA256 \
  --expected-path CANONICAL_LEDGER_PATH
```

The helper proves the exact earlier scenario owns the registered state and the
false extra state name is absent. It terminally retires only that observation,
retaining its erroneous current identity and immutable intent. It materializes
the tested-build path and binds the originally planned stopped topology from
verified public output and live metadata. Exact predecessor bytes, request,
resource context and observations remain immutable in the correction proof.
Ordinary CAS cannot create, change or remove that proof.

All bound resources must otherwise satisfy their normal safe lifecycle gates.
Running resources refuse. Stop and inspect the exact owned external executor
through its existing owner before using ordinary public observation CAS to
record a new stopped observation with a new digest, incremented resource
generation and appended history. The ledger is not a Docker verifier and this
protocol adds no trusted-host Docker interface.

The transaction tag is `-correct-observations`. Retry an interruption using the
same original path/generation/digest and exact request. Staged and installed
retries reprove actor, targets, namespace, resource metadata and complete ordered
physical/legacy leases. Drift refuses and preserves recovery evidence. After
receipt consumption, an old tuple cannot recover a lost result.

## Admit the same owner's unfinished delivery

Strict `check-reuse` and `revalidate-current-targets-cas` continue to reject
unfinished client plans. After correction and any required accepted-main
`target-refresh-cas`, use this distinct target-admission operation:

```sh
python3 scripts/delivery_ledger.py admit-in-progress-targets-cas \
  REVIEW_ROOT LEDGER_NAME ADMISSION_REQUEST_JSON \
  --expected-generation GENERATION --expected-digest SHA256 \
  --expected-path CANONICAL_LEDGER_PATH
```

The admission request has exactly `correction_slot` (the retired false-state
slot), `correction_sha256` (canonical digest of its immutable correction proof),
and `planned_slots` (the exact sorted original client reference/runtime slots).
The helper derives the accepted resource context from the correction proof.
It rechecks all targets and safe bound resources under the complete ordered lease
union and appends only one neutral ledger generation/digest/history observation.
No resource becomes absent, recovered, reusable, created or adopted by admission.
All selected plan records and authority/issue/target coordinates remain unchanged.

The dedicated receipt binds the canonical request digest as
`-admit-targets-REQUEST_SHA256`; changing the selection cannot reuse an installed
neutral successor. Retry only the same request and original tuple while its
receipt remains. Admission grants no resource-use or cross-delivery authority.
Normal resource-specific preparation is still required before later creation.

Keep generic external-runtime release/archive limitations explicit. Recovery and
in-progress admission are not terminal cleanup or whole-ledger release proof.
A retained correction also blocks archive when no external runtime remains:
this protocol does not consume its resource reservations or grant terminal
resource authority, and must never fall back to the storage namespace.

The tested build retains its complete original plan digest. Configuration proof
uses a bounded hash-only query over pinned flat default system, user and local
Git configuration origins, including absent higher-priority files. Includes,
unsafe links, origin drift and a mismatched original configuration digest refuse.
Unsupported original environment selectors are not normalized away. The complete source proof uses the read-only planning guard, which disables
filters, fsmonitor and optional index writes. Fixed Classic identity queries
may consult the same pinned default origins. Raw configuration is never logged
or retained.

## Continue after gameplay and a clean stop

The correction proof, false-state retirement and original tested-build attribution
stay immutable. A normal restart records a new topology observation through
ordinary CAS, preserving its exact corrected name/path/repository and extending
its digest/generation/history. Running observations still prohibit admission.
After the owner cleanly stops the same topology, record the canonical digest of
its fresh public stopped status with another ordinary observation CAS. Re-run
`admit-in-progress-targets-cas` with the original correction anchor and exactly
the original client plans that are still planned/null. Completed client slots
are no longer selected; they must satisfy their normal bound-resource checks.

Fresh proof requires the same scenario producer, registry/path, profile, source,
spec ownership and runtime build. Mutable saves are observed afresh and must stay
unchanged across the current locked transaction; their correction-time bytes
remain historical evidence, not a permanent freeze on gameplay. After all client
resources are properly prepared and bound by their owners, ordinary strict
`revalidate-current-targets-cas` can again succeed with every resource safe.
One-shot correction itself cannot be replayed onto this newer history.

For a resumed server generation, accepted status validates the new control,
runtime bundle and port reservation; all three leases must be released. Spec
ownership fields, service roles and logs remain fixed. The helper validates the
exact server executable/cwd/argument grammar against that generation; only its
endpoint, generation paths and decimal inherited-descriptor arguments vary.
Runtime-build `last_used_at` is current usage metadata, not tested-build provenance.
The complete fresh observations are compared again before publication.

The finite Classic `server_listener` selection is generation-local. New original
producer plans, specs and output must agree; historical absent fields preserve
loopback and their exact original argv. A later owner-prepared generation may
select `all-ipv4` through public `topology show` and `up`, followed by ordinary
running/stopped observation CAS. Fresh proof checks its exact spec/status and
seven- or eight-element argv; the historical correction bytes remain unchanged.
The fixed-provenance comparison excludes only this current selection, while
full observations must still remain equal across authentication and publication.
Admission does not authorize retuning a live topology or changing host publication.
