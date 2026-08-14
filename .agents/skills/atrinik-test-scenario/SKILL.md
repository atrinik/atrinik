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
./atrinik scenario credentials NAME
./atrinik topology show PROFILE --state scenario-NAME --json
./atrinik up --name NAME --profile PROFILE --state scenario-NAME
./atrinik ps NAME --json
./atrinik logs NAME server --follow
./atrinik logs NAME client --follow
./atrinik down NAME
./atrinik scenario reset NAME
```

`show`/`list` never reveal the password. Retrieve it only immediately before
login and never copy it into commits, review text, issues, logs, arguments, or
final responses. Confirm display forwarding when using the client. State the
exact login/actions/expected result between `up` and `down`.

Reset only after stopping the topology. It is destructive solely to the
scenario-owned state and refuses running, locked, external, shared, symlinked,
malformed, or unregistered targets. Handoffs include concrete profile,
scenario, topology, state, credentials lookup, prerequisites, expected result,
cleanup, and repeat-test commands.
