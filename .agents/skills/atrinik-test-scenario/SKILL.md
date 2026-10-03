---
name: atrinik-test-scenario
description: Provision deterministic classic account/character scenarios with isolated state through `./atrinik`.
---

# Atrinik test scenario

Use scenarios for repeatable reproductions. Keep account creation in the server
API and orchestration in the wrapper; never edit account/player files.

Fresh state comes from Classic `install_data`; the wrapper creates disposable
asset staging. Scenario state is registered, persistent, and scenario-owned;
it is not generic temporary topology state and must still be selected with
`--state scenario-NAME`. Never add generated assets to scenario state.

1. Select the exact coherent classic-derived profile.
2. Choose a lowercase issue/feature name and use `basic-player` unless a tested
   server-owned preset is genuinely required.
3. If the needed state is expensive to reach, extend the provisioner/preset
   contract with tests rather than writing a fixture save.

```sh
./atrinik scenario create NAME --profile PROFILE --preset basic-player
./atrinik scenario show NAME --json
./atrinik topology show PROFILE --state scenario-NAME --json
./atrinik up --name NAME --profile PROFILE --state scenario-NAME
./atrinik ps NAME --json
./atrinik logs NAME server --follow
./atrinik logs NAME client --follow
./atrinik down NAME
./atrinik scenario reset NAME
```

`show`/`list` hide passwords. Scenario-state `up` validates ownership and
automatically logs in the client. Its disposable secret may appear only in
local argv/client/Codex logs; never reuse it or copy it to GitHub, commits,
public logs, or durable handoffs. State actions/results between `up` and `down`.

Reset only after stopping the topology. It is destructive solely to the
scenario-owned state and refuses running, locked, external, shared, symlinked,
malformed, or unregistered targets. Handoffs include concrete profile,
scenario, topology, state, automatic login, prerequisites, expected result,
cleanup, and repeat-test commands.

When an agent scope deliberately selects `scenario-NAME` as its registered
named state, scope release never removes or resets it. Stop the exact topology,
release the scope with a fresh preview digest, then reset the scenario only as
a separate explicit scenario operation. Concurrent scopes may share neither a
live scenario state nor its operation lease; distinct scenario states progress
independently and credentials remain outside every scope record and journal.

For a split native client, keep scenario credentials local and out of durable logs and handoffs. Persistence acceptance requires clean stop/restart of the same registered state and verification of the saved player/change; handcrafted state and account creation alone do not prove gameplay persistence.

For the fixed Brynknot real-movement benchmark, export an absent route and run it
with a fresh topology name:

```sh
./atrinik scenario route NAME --output /absolute/brynknot.xml
./atrinik scenario benchmark NAME --name RUN --route /absolute/brynknot.xml --json
```

Route export is offline, uses only the fixed `brynknot-v1` producer, records
clean committed source/content provenance in a companion file, and never changes
the registered scenario, account, or state. Benchmark execution requires native
`--live-movement-route`/`--live-movement-report` support, stages evidence below
the fresh topology's `benchmark/` directory, validates real MAP publication and
presented coverage, and generation-fences shutdown without resetting the
scenario. CPU and GPU fields are per-frame host timings; GPU fields do not claim
hardware GPU duration. Failure is nonzero. Bounds are 8 MiB/50,000 checkpoints
for the route, one hour runtime, and 128 MiB for the report.
