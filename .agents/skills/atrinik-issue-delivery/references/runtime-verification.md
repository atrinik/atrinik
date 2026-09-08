## Verify with compatible resources

Before provisioning, run applicable supported inventories:

```sh
./atrinik status --json
./atrinik worktree list --json
./atrinik scenario list --json
./atrinik state list --json
./atrinik ps --json
./atrinik profile show PROFILE --json
./atrinik topology show PROFILE --temporary-state --json
```

Reuse rather than recreate exact compatible profiles, stopped topologies,
wrapper-owned state/data,
or stopped/unlocked delivery scenarios. Prefer generation-owned temporary state;
use named/default only for persistence or an existing account. Match immutable
repository, branch, checkout, source, provider, commit, profile, generation,
and owner to final HEAD; incomplete records are inert. Ordinary state must be
test-safe with an appropriate account; never reset it.

Let wrapper metadata choose paths/builds; never reconstruct, copy, edit, delete,
or select internals. Never stop unrelated topologies, reset shared/default/
external state, handcraft saves, or reuse/publish credentials. A disposable
scenario password may appear only in local auto-login argv/client/Codex logs.
Create a unique `basic-player` scenario only for interactive verification when
no exact delivery scenario exists; add a tested server-owned preset only when
ordinary play is impractical.

For Classic, compose the runtime/scenario skills and give exact profile, build,
scenario, automatic-login topology, bounded logs, action/result, repeat,
shutdown, cleanup, and state-policy commands. Initial `down` applies only to
that topology; reset only delivery-owned data; state display/login prerequisites.

For replacement work, create/reuse a delivery profile selecting the final
worktree; inspect capabilities. While adapters are absent, give
owner-native validation plus supported profile/topology inspection, identify
#266/#269/#270, and never substitute Classic. If runtime is irrelevant, give
applicable tests and the capability-aware recipe in the concise PR update and final
handoff.

