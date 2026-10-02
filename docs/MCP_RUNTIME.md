# Opt-in runtime publication observations

`atrinik_workspace.mcp_runtime.RuntimeService` is a separate, disabled-default
adapter for approved wrapper topology publications. The context server does not
register its two tools automatically. A trusted host supplies `RuntimeApproval`
records and explicitly composes `runtime_tools(service)` into its observe
transport. Tool arguments cannot create approvals, change roots, or enable it.

The separate executable is `python3 -B -m
atrinik_workspace.mcp_observe_server --root TRUSTED_WRAPPER_ROOT --approvals
PRIVATE_APPROVAL_FILE`. The approval file uses schema version 1 and contains
`authorization_identity` and an `approvals` array of the four fields described
below. It is host configuration, not a committed project file. Its bytes are
rechecked before and after every request; removal or changes revoke access until
the server is restarted with reviewed configuration. Observe discovery exposes
only the two runtime tools. Routine context discovery cannot enable them.

Each approval binds topology name, profile, generation and SHA-256 of the exact
registered `spec.json` bytes. Review that spec through the ordinary wrapper
workflow before granting approval. Its digest covers state, scenario, build,
source and service identities without disclosing their private values. Restart,
spec replacement or generation changes require a new explicit approval. The
optional constructor `runtime_root` selects the host-configured wrapper workspace
root; otherwise it is `context.root / "workspace"`. Ambient environment variables
never broaden the root. No deployment, activation or approval is shipped.

`runtime_list` lists only approved registered publications, with deterministic
pagination and bounded per-record failures. It scans at most 1,000 approvals,
returns at most 50 records, and fingerprints the observation and authorization
identity into its cursor. A malformed record does not suppress healthy records.
A changed publication or authorization makes an old cursor unusable.

`runtime_status` selects one exact approved topology and returns its source
coordinate separately from the recorded runtime source coordinates, approved
profile/generation, recorded readiness, per-service states and exit codes,
clean-shutdown facts, opaque state/worktree identities, observation time and
publication fingerprints. This reports the supervisor's **published status**;
it does not assert current process health or probe PIDs, ports, sockets or the
network. Missing process publications remain incomplete. Freshness has zero TTL.
The existing `./atrinik ps NAME --json` remains the authoritative live workflow.

The immutable generation manifest supplies the full source commits, tree hashes,
generation manifest digest and build metadata digest. Its fixed registered path
and ownership marker are verified; private build paths and outputs stay private.

The adapter reads the topology ownership marker, spec, status and immutable
generation marker/manifest. It opens
all ancestors and regular files without following links, caps each file at
256 KiB, compares descriptor and visible identities, rereads the publications,
and fences source identity before returning. Listings recheck every retained
source snapshot and publication at handoff. Cancellation and the five-second
deadline are checked throughout reading; structured listings stay within 32 KiB.
No `Workspace` instance, state initialization, leases, writes, process controls,
arbitrary paths, commands or networks are used. Source-context failures remain
failures and cannot fall back to another provider.

Output is an allowlisted projection. Paths, commands, environment, process IDs,
logs and private state fields are never returned. Error messages contain fixed
text only. Logs are entirely deferred: no log tool or resource is registered,
and status approvals do not authorize log reads.

Validate using synthetic fixtures, including a 77-record catalog:

```sh
python3 -m unittest -v tests.test_mcp_runtime
```

Live comparison requires an independently authorized, unchanged manual topology;
fixture success grants no authority to create, start, stop or adopt one.
