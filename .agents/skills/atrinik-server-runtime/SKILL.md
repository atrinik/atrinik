---
name: atrinik-server-runtime
description: Run or diagnose isolated classic servers and supervised topologies through `./atrinik`.
---

# Atrinik server runtime

The wrapper owns builds, collection, state locks, supervision, logs, and
cleanup. Never reconstruct its paths or invoke generated binaries.

Classic preparation owns disposable `assets` staging. Immutable
exact-profile `client-maps/*` and resources use authenticated QUIC by default;
server-generated `data/*` transport files live only in the generation-named
runtime-output directory below the exclusively leased state. Do not place
copied asset inputs in state. `http_url` only names an optional external
HTTP(S) origin; never restore a bundled HTTP listener.

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
`unreachable` liveness and exact runtime-generation, process-tree, state, and
port observations. A ready topology retains no repository-layout or mutable
build-root lease. Cross-session `down` uses the matching filesystem
control endpoint, never a PID from the caller's namespace. For `unreachable`,
follow the reported safe action: wait for bounded guardian recovery and retry
`ps` and `down`; preserve an exact retained lease for operator diagnosis.
Never inspect `/proc`, signal the recorded PIDs, unlink control or lease files,
or reuse the name as recovery.
After `down`, retain the record for diagnosis or reclaim it only through the
separate preview-first lifecycle:

```sh
./atrinik cleanup --scope topologies --older-than 7 --dry-run --json
./atrinik cleanup --scope topologies --older-than 7 --apply
```

This scope is excluded from defaults and `all`. It accepts only old `exited` or
legacy `stale` marker-owned records with unreachable controls and released
leases; it never acts as `down` or touches persistent state, scenarios, builds,
profiles, or source.
The wrapper uses a short generation-derived endpoint in the shared workspace
and binds both process-tree and immutable runtime-bundle leases to the exact
generation and file identities. Missing, replaced, linked, or malformed
current generations, manifests, or lease files are unsafe and must remain
untouched for fail-closed diagnosis. Never edit a published generation; rebuild
the profile while it is live only to verify that its recorded manifest digest
and runtime bytes remain unchanged.

Let `up` allocate a port unless a distinct fixed port is required. Diagnose
build, state, plugin, network, and gameplay failures separately. Use
`atrinik-test-scenario` for accounts; never handcraft saves. Record actions and
logs, stop the topology, reset only scenario state, and run owner validation.
