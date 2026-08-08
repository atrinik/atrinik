---
name: atrinik-server-runtime
description: Run or diagnose isolated classic server builds, state, resources, scenarios, and supervised topologies through `./atrinik`.
---

# Atrinik server runtime

Use the wrapper for build, collection, state locking, supervision, logs, and
cleanup. Do not reconstruct internal paths or invoke generated binaries when a
wrapper command owns the operation.

1. Read the workspace and selected classic server/content/resource guides.
2. Select a coherent classic-derived profile and inspect it with `profile show`
   and `topology show`.
3. Use a distinct registered state for each concurrent topology. Never replace
   source, share live state, or edit wrapper-managed runtime files.

```sh
./atrinik build server --profile PROFILE --test
./atrinik topology show PROFILE --state STATE --json
./atrinik up --name NAME --profile PROFILE --state STATE
./atrinik ps NAME --json
./atrinik logs NAME server --follow
./atrinik down NAME
```

Let `up` allocate a port unless an exact distinct port is required. Separate
build, collection, state, plugin, network, and gameplay failures before editing
code. Use `atrinik-test-scenario` for a provisioned account/character; never
handcraft saves. Inspect relevant service logs, state exact actions/results,
always stop the topology, and reset only scenario-owned state with `scenario
reset`. Finish with each changed repository's tests and `git diff --check`.
