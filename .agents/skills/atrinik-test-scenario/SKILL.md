---
name: atrinik-test-scenario
description: Provision, inspect, reset, and hand off deterministic local Atrinik account-and-character scenarios through the ./atrinik wrapper. Use when an issue, pull request, gameplay change, client/server fix, or manual reproduction needs a ready-to-login test player, isolated mutable server state, repeatable credentials, or exact wrapper-native runtime verification commands.
---

# Atrinik Test Scenario

Use the workspace scenario lifecycle to give a reviewer the smallest safe,
repeatable local player setup for a manual reproduction. Keep account creation
inside the server API and orchestration inside `./atrinik`; never construct or
edit account or player save files directly.

## Select the scenario

1. Read the workspace `AGENTS.md` and the `atrinik-multi-repo-workspace` skill.
2. Identify the exact mixed-component profile for the change. Create or update
   that profile with `./atrinik profile` when the task already authorizes
   workspace setup.
3. Choose a lowercase scenario name tied to the issue or feature, such as
   `issue-42` or `inventory-filter`.
4. Use `basic-player` unless the reproduction truly needs another server-owned
   preset. It provides a normal `human_male` first-login character, including
   the configured starting map, standard skills, and initial items.
5. If the required state cannot be reached cheaply from `basic-player`, extend
   the server provisioner and wrapper preset contract with tests. Do not bypass
   validation by writing a handcrafted player file.

## Provision and inspect

Create the scenario once:

```sh
./atrinik scenario create NAME --profile PROFILE --preset basic-player
```

The command builds the selected server, creates a dedicated registered state
named `scenario-NAME`, writes mode-0600 credentials below the ignored workspace
directory, provisions the account through the server's normal password and
account persistence APIs, and prints the complete manual lifecycle.

Inspect it without revealing the password:

```sh
./atrinik scenario show NAME --json
./atrinik scenario list --json
```

Retrieve credentials only when login is about to be tested:

```sh
./atrinik scenario credentials NAME
```

Do not copy the password into commits, review documents, issue comments, logs,
shell arguments, or final responses. Give reviewers the credentials command.

## Run the reproduction

Use the scenario's recorded profile and state. A client/server check normally
uses:

```sh
./atrinik profile show PROFILE
./atrinik build server --profile PROFILE --test
./atrinik topology show PROFILE --state scenario-NAME --json
./atrinik up --name NAME --profile PROFILE --state scenario-NAME
./atrinik ps NAME --json
./atrinik logs NAME server --follow
./atrinik logs NAME client --follow
./atrinik down NAME
```

Confirm display forwarding before `up` when the client is included. Between
`up` and `down`, state the exact login steps, feature actions, and expected
results for the issue. Use `ps` and wrapper-managed logs instead of reading
internal PID, build, runtime, or log paths.

## Reset safely

After stopping the topology, restore the scenario to its pristine provisioned
state with:

```sh
./atrinik down NAME
./atrinik scenario reset NAME
```

Reset is intentionally destructive only to the scenario-owned state. It keeps
the same account, character, password, profile, preset, and state name. It
refuses a running or otherwise locked server state and never replaces an
external, shared, symlinked, malformed, or unregistered directory.

## Hand off

Every final handoff must include copy-pasteable commands with concrete profile,
scenario, topology, and state names; the credentials lookup; prerequisites;
the exact manual actions and expected outcome; and `down` for cleanup. Mention
`scenario reset` as the repeat-test command when appropriate. Never replace a
supported wrapper command with a reconstructed internal path or direct binary.
