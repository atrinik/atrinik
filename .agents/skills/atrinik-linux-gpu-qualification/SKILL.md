---
name: atrinik-linux-gpu-qualification
description: Qualify Classic Linux Vulkan on assigned real hardware for release acceptance.
---

# Qualify the Classic Linux GPU renderer

Compose the Classic-owned qualification harness; do not create another benchmark
runner or duplicate its acceptance logic in the wrapper. Read the selected
Classic revision's `client/README.md`, `.github/workflows/gpu-qualification.yml`,
`client/tools/verify_gpu_qualification.py`, and
`client/tools/verify_gpu_fixture_provenance.py` before executing or judging
evidence. Treat those files at the qualified revision as authoritative when they
differ from this skill.

Read [the Linux Vulkan runbook](references/linux-vulkan.md) for collection,
validation, review, and handoff commands.

## Admission

Resolve an owned, clean Classic worktree and exact 40-character revision. Before
any build or GPU process starts, obtain the coordinator's exact resource
coordinate for the Linux GPU host or runner, Vulkan backend, hardware tier,
headful display session, writable build root, owner, and bounded reservation.
Follow [`docs/LINUX_EXECUTION.md`](../../../docs/LINUX_EXECUTION.md) for execution
and authority boundaries and [`docs/SOURCE_DELIVERY.md`](../../../docs/SOURCE_DELIVERY.md)
for new source work. Retained or shared resources keep their existing delivery,
lease, and recovery protocol. Do not infer a reservation from device visibility,
an environment flag, a CPU worker, or a proposed change.

The pinned CPU build worker is useful for ordinary compile and toolchain checks,
but it has no display or GPU mounts and supplies no GPU acceptance. A daily check
that reports `gpu-hardware-qualification-required`, an emulated or software
renderer, or a remote GPU without an actual qualified Linux hardware coordinate
is a deferral, not passing Vulkan evidence.

Require a visible, real Vulkan device on either the `reference` or `minimum`
tier. The harness must attest the actual backend, device, driver name, driver
version, and qualified hardware status. Requested environment values never
replace backend-native attestation.

## Acceptance boundary

Qualification uses a clean Release build with `BUILD_TESTING=ON`,
`ATRINIK_GPU_CONFORMANCE_REQUIRED=ON`, and the checksum-pinned shader cohort. Run
the Classic fixture-provenance verifier before collection; it binds the
production fixture set to `atrinik/content@main` and `classic-ads-v1` through the
selected revision's lock and provenance records.

Keep these gates distinct:

- Stress acceptance is schema 4 and contains all five named workloads, each
  collected by three separately launched client processes.
- Recovery lifecycle acceptance is schema 3 and exercises the complete headful
  lifecycle event set.
- Production fixture acceptance is schema 2 and contains the exact fixture
  cohort, including `gpu-ui-closure` and one A-to-B-to-A movement lifecycle.
- Collection validation proves structural, provenance, identity, PNG, and
  semantic completeness. Closure validation additionally requires the committed
  exact-RGBA8 contract with `human_approved: true` and matching Vulkan goldens.

Never convert collection success into release acceptance. Preserve every JSONL
and hash-bound PNG review artifact. A human reviews the images and deliberately
updates the golden contract through separately authorized source work; the agent
does not self-approve new pixels. Re-run closure validation after that reviewed
contract is present at the candidate revision. Because a golden change creates a
new revision, rebuild and recollect every artifact family at that revision before
closure; old pixels may inform review but cannot qualify the new commit.

## Result

Report the exact Classic revision and clean status, shader cohort, Vulkan device
and driver, hardware tier, display mode, resource coordinate and reservation,
build configuration, verifier outputs, artifact paths and hashes, human golden
status, failures, and cleanup or release of only the owned resource. A complete
result needs all stress, lifecycle, production-fixture, and human-golden gates;
otherwise state the exact remaining gate without claiming qualification.
