# Retained runtime handoff

A retained Classic runtime can use its tested producer from an isolated executor
without mounting coordinator-private `build/reviews`. The coordinator publishes
an immutable, bounded envelope under the existing resource workspace's
`build/runtime-handoffs/LEASE_ID/` and serves a short-lived Unix socket there.
The envelope alone grants no runtime authority. The executor must share this
existing build namespace at the same canonical path and Linux UID; no additional
mount or credential is required. Unavailable socket connectivity fails closed.

This contract applies only to an authenticated issue delivery with a completed
retained tested server build. It neither qualifies a runtime nor changes mutable
state ownership or server listeners. Runtime qualification remains a separate
operation after this implementation is accepted.

From the accepted coordinator's issue-delivery skill directory, obtain the exact
current generation, digest and canonical ledger path through `inspect`. Use a
fresh 64-character lowercase SHA-256 lease identity, then keep this command
running in the coordinator:

```sh
python3 scripts/delivery_ledger.py runtime-handoff-publish REVIEW_ROOT LEDGER_NAME \
  --expected-generation GENERATION --expected-digest LEDGER_SHA256 \
  --expected-path CANONICAL_LEDGER_PATH --issue OWNER/REPOSITORY#NUMBER \
  --lease-id LEASE_ID --ttl-seconds 300
```

The helper proves authenticated issue/actor, exact worktrees, producer, source,
artifact and retained resource leases before publication. Original unfinished
client plans remain inert and unchanged. The output supplies the envelope path,
its digest, the lease identity and exact wrapper argument arrays. It never
contains ledger bytes, private report paths, credentials, worker text or server
state. The helper creates an ephemeral Ed25519 signing key in process memory
and emits its public fingerprint as `publisher_fingerprint`. A separate
`endpoint_fingerprint` pins the socket inode and its build, runtime-handoffs and
lease directory incarnations. Select both values from trusted coordinator
stdout and pass them independently as `--handoff-publisher` and
`--handoff-endpoint`; never select them from an untrusted envelope or its command
array. The consumer verifies a fresh signed challenge at every recheck, so an
arbitrary same-UID publisher cannot impersonate the selected coordinator. Every
signed response binds the independently pinned endpoint; a replacement proxy
cannot relay proof from another socket while releasing its operation guard. Both
sides require `/usr/bin/openssl` with Ed25519 support; the private key never
leaves coordinator memory and anonymous descriptors.

Consume on the preserved isolated executor through the emitted commands:

```sh
./atrinik topology show PROFILE --state SCENARIO_STATE --service server \
  --retained-build-plan PLAN_SHA256 --runtime-handoff LEASE_ID \
  --handoff-issue OWNER/REPOSITORY#NUMBER --handoff-attempt ATTEMPT_SHA256 \
  --handoff-publisher TRUSTED_COORDINATOR_FINGERPRINT \
  --handoff-endpoint TRUSTED_COORDINATOR_ENDPOINT --json
./atrinik up --name TOPOLOGY --profile PROFILE --state SCENARIO_STATE --service server \
  --retained-build-plan PLAN_SHA256 --runtime-handoff LEASE_ID \
  --handoff-issue OWNER/REPOSITORY#NUMBER --handoff-attempt ATTEMPT_SHA256 \
  --handoff-publisher TRUSTED_COORDINATOR_FINGERPRINT \
  --handoff-endpoint TRUSTED_COORDINATOR_ENDPOINT --json
```

`topology show` takes positional `PROFILE`. The default server listener stays
loopback. Existing explicit listener choices remain separate operator decisions.
The consumer refuses private review mounts; run ordinary coordinator commands
there instead. Inspection and each startup/reconnect require a fresh connection,
exact issue/attempt/CAS coordinates, a live publisher lease, matching canonical
paths and an unexpired declaration. Sources, public build plan and tested build
artifacts are checked locally. Startup rechecks immediately before the supervisor
is spawned while the normal source/topology/state locks remain held.

For each consumer operation the publisher holds a private ledger guard and
rechecks current inventory, pending transitions, issue state, actor and exact CAS
coordinates. It does not reacquire executor-owned source/topology locks. A
concurrent delivery transition must wait for the consumer operation to finish;
a busy guard refuses new consumers. No private descriptor crosses the socket.
The publisher keeps its guard across idle startup waits, bounded by the remaining
lease rather than a separate 30-second timeout. Every protocol fragment uses
the same absolute monotonic deadline, so slow input cannot extend that lease. The consumer requires a signed
completion acknowledgement sent while that guard is still held. The session
ends when the wrapper operation finishes, the executor disconnects,
the publisher exits, or the lease expires (maximum 900 seconds). This is startup
admission, not continuing authority to manage or terminate a running server.
Expiry or publisher loss rejects subsequent startup checks and completion; it
does not report success or perform later wrapper startup mutations. A supervisor
already spawned before expiry may remain: inspect it with `ps`/`logs` and use
normal owned `down` recovery. Normal state ownership rules continue to apply.

Revoke the owned publisher from the shared namespace:

```sh
./atrinik runtime-handoff revoke LEASE_ID \
  --issue OWNER/REPOSITORY#NUMBER --attempt ATTEMPT_SHA256 \
  --publisher TRUSTED_COORDINATOR_FINGERPRINT --endpoint TRUSTED_COORDINATOR_ENDPOINT
```

Revocation serializes behind an admitted operation, then records a durable
revocation before acknowledgement. Revoked leases cannot restart. Repeated consume and reconnect operations use
the same live publisher, key and unchanged CAS tuple. A stopped, interrupted,
expired or changed publisher needs a fresh lease identity, a new independently
selected public fingerprint, and full proof; the old ephemeral private key is
never recovered from an envelope.
Historical envelopes, lease files and revocation records are preserved. These
commands do not clean up historical files, mutate ledger generations, change
executor mounts, or transfer ownership of resources.

The focused retained-helper test uses disposable fixture repositories and an
isolated Git configuration; production unsafe-origin checks remain unchanged.
Set `ATRINIK_HANDOFF_EXECUTOR_IMAGE` to a locally available, digest-pinned Linux
build image to also exercise signed consume/recheck in a disposable container.
Its transport fixture mounts only the wrapper Python package and its temporary
public build namespace read-only. Its emitted `topology show` command also mounts
disposable fixture sources read-only and their temporary workspace and Git
administration for ordinary wrapper locks. Both assert the private review root
is absent and use no network or additional capabilities. This proves CLI
admission, source/plan checks, transport and isolation; synthetic build payloads
do not qualify a Classic runtime.
