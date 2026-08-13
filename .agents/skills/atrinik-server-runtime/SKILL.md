---
name: atrinik-server-runtime
description: Run or diagnose isolated classic servers and supervised topologies through `./atrinik`.
---

# Atrinik server runtime

The wrapper owns builds, collection, state locks, supervision, logs, and
cleanup. Never reconstruct its paths or invoke generated binaries.

Classic preparation owns disposable `assets` staging. Generated `data/*`,
exact-profile `client-maps/*`, and resources use authenticated QUIC by default;
`http_url` only names an optional external HTTP(S) origin. Never stage assets in
state or restore a bundled HTTP listener.

1. Read the workspace and selected server/content/resource guides.
2. Inspect a coherent classic-derived profile and topology.
3. Give concurrent topologies distinct registered states. Never replace source,
   share live state, or edit managed runtime files.

```sh
./atrinik build server --profile PROFILE --test
./atrinik topology show PROFILE --state STATE --json
./atrinik up --name NAME --profile PROFILE --state STATE
./atrinik ps NAME --json
./atrinik logs NAME server --follow
./atrinik down NAME
```

`ps --json` reports generation-bound `live`, `exited`, `stale`, or
`unreachable` liveness and an `observation` naming any topology that retains
the repository-layout lease. Cross-session `down` uses the matching filesystem
control endpoint, never a PID from the caller's namespace. For `unreachable`,
follow the reported safe action: wait for bounded guardian recovery and retry
`ps` and `down`; if the exact lease remains retained, use the starting session.
Never inspect `/proc`, signal the recorded PIDs, unlink control or lease files,
or reuse the name as recovery.
The wrapper uses a bounded descriptor-relative address for deep managed socket
paths and binds the process-tree lease to its generation and file identity.
Missing, replaced, linked, or malformed current lease files are unsafe and
must remain untouched for fail-closed diagnosis.

Let `up` allocate a port unless a distinct fixed port is required. Diagnose
build, state, plugin, network, and gameplay failures separately. Use
`atrinik-test-scenario` for accounts; never handcraft saves. Record actions and
logs, stop the topology, reset only scenario state, and run owner validation.
