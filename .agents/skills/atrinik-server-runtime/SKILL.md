---
name: atrinik-server-runtime
description: Prepare, run, diagnose, and validate Atrinik classic server builds, collected resources, isolated state, deterministic scenarios, and supervised topologies. Use for server runtime setup, smoke tests, or failures.
---

# Atrinik Server Runtime

Use this skill for running or diagnosing the classic server. Use the workspace
wrapper for builds, collection, state registration, supervision, logs, and
cleanup; do not reconstruct its internal paths or invoke build artifacts
directly when the wrapper supports the operation.

## Prepare an isolated runtime

1. Read the workspace and server guides plus `atrinik-multi-repo-workspace`.
2. Select exact server, content, resources, protocol, and library sources in a
   named profile. Inspect them with `./atrinik profile show` and
   `./atrinik topology show`.
3. Build and test the server through the wrapper:

   ```sh
   ./atrinik build server --profile PROFILE --test
   ```

4. Use a distinct registered state for each concurrent topology. Never replace
   a dirty worktree, overwrite mutable server data, share state between running
   topologies, or edit wrapper-managed runtime files by hand.

## Run and diagnose

For persistent supervised testing:

```sh
./atrinik topology show PROFILE --state STATE --json
./atrinik up --name NAME --profile PROFILE --state STATE
./atrinik ps NAME --json
./atrinik logs NAME server --follow
./atrinik down NAME
```

Let `up` allocate ports unless a distinct port is required. Use wrapper logs
and status rather than internal PID or log files. Separate build failures,
missing collected resources, invalid state, plugin loading, network setup, and
gameplay behavior before changing code.

Use `atrinik-test-scenario` when the check needs a deterministic account and
character. The server's offline provisioner must remain listener-, plugin-,
console-, and metaserver-free and must persist through normal account APIs;
never handcraft account or player files.

## Validate and clean up

Run the complete server test suite and task-specific resource or map generation
checks. For client-visible behavior, inspect both service logs and state the
exact login actions and expected outcome. Always stop the named topology; reset
only scenario-owned state through `./atrinik scenario reset NAME` when a clean
repeat is needed. Finish with `git diff --check` in each changed repository.
