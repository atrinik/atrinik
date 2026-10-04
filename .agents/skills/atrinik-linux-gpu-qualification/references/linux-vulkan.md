# Linux Vulkan qualification runbook

Use the candidate Classic worktree itself. Re-read its workflow and verifiers so
the commands, fixtures, schemas, and budgets remain bound to the candidate
revision rather than this reference.

## Prerequisites and reservation

Record `CLASSIC_ROOT`, the exact candidate ref, the resolved 40-character commit,
and clean porcelain status. Verify that the coordinator has assigned an actual
Linux Vulkan `reference` or `minimum` GPU coordinate, visible headful display,
exclusive or otherwise conflict-free time window, writable qualification build
root, and named owner. Do not start the build while any coordinate is missing or
the reservation is stale.

From the wrapper worktree, inspect the selected build environment without
claiming authority from the report:

```sh
python3 -m atrinik_workspace.linux_platform
```

Use the applicable live resource protocol before the GPU/build operation. For a
new source-only project leaf, the coordinator records the GPU and build roots as
shared resources in its milestone plan. An already-bound delivery retains its
existing worktree, context, ledger, CAS, and lease checks. Do not invent a local
lock file or borrow a CPU-worker recipe as a GPU reservation.

Confirm `cmake`, Ninja, Python 3, the candidate's required development libraries,
a visible SDL-compatible display, and a Vulkan loader/device are present. Keep
package installation, runner setup, and privileged host changes separate and
explicitly authorized. The Classic workflow is the canonical package-independent
description of the build and run.

## Bind the source and shader cohort

Run from the clean Classic root. Use private, task-owned cache and artifact paths
that do not overlap another qualification. The example variables are placeholders
to replace with assigned absolute coordinates:

```sh
cd "$CLASSIC_ROOT"
QUALIFICATION_REVISION=$(git rev-parse --verify HEAD)
test "${#QUALIFICATION_REVISION}" -eq 40
test -z "$(git status --porcelain=v1 --untracked-files=normal)"
export ATRINIK_BENCHMARK_REVISION="$QUALIFICATION_REVISION"
export ATRINIK_BENCHMARK_DIRTY=false
python3 client/tools/verify_gpu_fixture_provenance.py
tools/ci/prepare_gpu_shaders.sh \
  "$CLASSIC_ROOT" "$SHADER_DOWNLOAD_CACHE" "$SHADER_OUTPUT"
```

The provenance verifier must report the selected `atrinik/content` `main`
coordinate. Its production artifact contract is `classic-ads-v1`; do not replace
it with historical `content-1x`, locally assembled content, or fixture-only data.

## Configure and build

Use a fresh task-owned build directory and the shader output produced above:

```sh
cmake -S client -B "$GPU_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DATRINIK_GPU_CONFORMANCE_REQUIRED=ON \
  -DPACKAGE_TYPE=none \
  "-DATRINIK_GPU_SHADER_DIRECTORY=$SHADER_OUTPUT"
cmake --build "$GPU_BUILD" --config Release \
  --target atrinik client-gpu-renderer-integration-tests
```

Export the Linux lane and let the executable attest what actually initialized:

```sh
export ATRINIK_GPU_CONFORMANCE_DRIVER=vulkan
export ATRINIK_GPU_CONFORMANCE_HARDWARE_TIER="$HARDWARE_TIER"
export ATRINIK_GPU_CONFORMANCE_QUALIFIED_HARDWARE=1
cd "$CLASSIC_ROOT/client"
"$GPU_BUILD/client-gpu-renderer-integration-tests"
mkdir -p "$ARTIFACT_ROOT/review"
```

`HARDWARE_TIER` is exactly `reference` or `minimum`. Environment values express
the requested lane; records pass only when the initialized backend supplies a
nonempty device, driver name, and driver version and marks qualified hardware.

## Collect the five stress workloads

Run each row in three fresh processes. Keep the workflow's names and fixture
mapping exact:

```sh
for workload in \
  dense-17x17-five-depth-1080p \
  dense-25x25-seven-depth-1440p \
  wire-ceiling-28x28-thirteen-depth-1440p \
  wire-ceiling-28x28-thirteen-depth-4k \
  actor-door-roof-animation-25x25; do
  case "$workload" in
    dense-17x17-five-depth-1080p)
      fixture=gpu-benchmark-dense-17x17-five-depth-1080p ;;
    dense-25x25-seven-depth-1440p)
      fixture=gpu-qualification-town-25x25 ;;
    wire-ceiling-28x28-thirteen-depth-1440p)
      fixture=gpu-benchmark-wire-ceiling-28x28-thirteen-depth-1440p ;;
    wire-ceiling-28x28-thirteen-depth-4k)
      fixture=gpu-benchmark-wire-ceiling-28x28-thirteen-depth-4k ;;
    actor-door-roof-animation-25x25)
      fixture=gpu-benchmark-actor-door-roof-animation-25x25 ;;
  esac
  for run in 1 2 3; do
    ATRINIK_GPU_CONFORMANCE_OUTPUT="$ARTIFACT_ROOT/${workload}-${run}.jsonl" \
      "$GPU_BUILD/atrinik" --gpu-player-view-benchmark \
      "src/tests/fixtures/player_view/${fixture}.xml" "$workload"
  done
done
python3 tools/verify_gpu_qualification.py --require-complete \
  "$ARTIFACT_ROOT"/*-[123].jsonl
```

Do not loop inside one client process. The verifier requires schema 4, one
revision, shader cohort, backend/device/driver/tier identity, all five rows, and
at least three process-cold records per row.

## Collect lifecycle and production fixtures

Lifecycle must run in the assigned headful display session:

```sh
ATRINIK_GPU_CONFORMANCE_LIFECYCLE_OUTPUT="$ARTIFACT_ROOT/lifecycle.jsonl" \
ATRINIK_GPU_CONFORMANCE_REVIEW_DIRECTORY="$ARTIFACT_ROOT/review" \
  "$GPU_BUILD/atrinik" --gpu-player-view-lifecycle \
  src/tests/fixtures/player_view/brynknot-movement.xml
python3 tools/verify_gpu_qualification.py --collect-lifecycle \
  "$ARTIFACT_ROOT/lifecycle.jsonl"
```

Then collect the workflow's exact production fixture cohort:

```sh
: >"$ARTIFACT_ROOT/production-fixtures.jsonl"
export ATRINIK_GPU_CONFORMANCE_REVIEW_DIRECTORY="$ARTIFACT_ROOT/review"
for fixture in smooth radial-light-smooth exit-cues \
  living-outline-translucent living-outline-retained-fow timed-light \
  map-overlay-linked-depth map-overlay-widget-state map-overlay-elevated \
  visibility-fade-centered brynknot-movement \
  gpu-qualification-town-25x25 gpu-ui-closure; do
  "$GPU_BUILD/atrinik" --gpu-player-view \
    "src/tests/fixtures/player_view/${fixture}.xml" \
    >>"$ARTIFACT_ROOT/production-fixtures.jsonl"
done
python3 tools/verify_gpu_qualification.py --collect-production-fixtures \
  "$ARTIFACT_ROOT/production-fixtures.jsonl"
```

Collection validation rehashes review PNGs and checks their dimensions and
semantic pixel hashes. Preserve the full `review/` directory with the JSONL.

Before closure, independently bind all three artifact families to the candidate
and to one hardware identity. The family verifiers check internal consistency,
but do not compare recorded revisions with checkout `HEAD` or identities across
families. Run this check from `client` after collection:

```sh
test "$(git rev-parse --verify HEAD)" = "$QUALIFICATION_REVISION"
test -z "$(git status --porcelain=v1 --untracked-files=normal)"
python3 - "$QUALIFICATION_REVISION" "$ARTIFACT_ROOT" <<'PY'
import json
from pathlib import Path
import sys

revision, root = sys.argv[1], Path(sys.argv[2])
paths = sorted(root.glob("*-?.jsonl")) + [
    root / "lifecycle.jsonl",
    root / "production-fixtures.jsonl",
]
records = []
for path in paths:
    with path.open(encoding="utf-8") as stream:
        records.extend(json.loads(line) for line in stream if line.strip())
if not records or {record.get("revision") for record in records} != {revision}:
    raise SystemExit("qualification artifacts do not match candidate revision")

def identity(record):
    gpu = record.get("gpu", record)
    return (
        gpu.get("backend"), gpu.get("device"), gpu.get("driver_name"),
        gpu.get("driver_version"), gpu.get("adapter_identity", "unavailable"),
        gpu.get("hardware_tier"),
    )

if len({identity(record) for record in records}) != 1:
    raise SystemExit("qualification artifact families mix GPU identities")
cohorts = {record["shader_cohort"] for record in records
           if "shader_cohort" in record}
if len(cohorts) != 1:
    raise SystemExit("stress and lifecycle artifacts mix shader cohorts")
print(json.dumps({"revision": revision, "gpu": identity(records[0]),
                  "shader_cohort": next(iter(cohorts))}, sort_keys=True))
PY
```

## Human review and closure

Inspect the committed contract before claiming acceptance:

```sh
python3 - <<'PY'
import json
from pathlib import Path

path = Path("src/tests/fixtures/player_view/gpu-approved-goldens.json")
contract = json.loads(path.read_text(encoding="utf-8"))
print(json.dumps({
    "schema_version": contract.get("schema_version"),
    "contract": contract.get("contract"),
    "human_approved": contract.get("human_approved"),
    "backends": sorted(contract.get("backends", {})),
}, sort_keys=True))
PY
```

If it is not schema 1, `exact-rgba8`, and explicitly human-approved with the
required Vulkan fixture, lifecycle, UI-state, tier, and display-mode entries,
stop at collected evidence. Human review must compare the preserved PNG set and
authorize a source change to that contract. Never rewrite hashes merely to make
the validators pass.

Once the reviewed contract is present in the exact clean candidate revision,
the revision has changed. Rebuild and recollect stress, lifecycle, production
fixtures, and review PNGs from that new revision, repeat the cross-family check
above, then run both closure gates:

```sh
python3 tools/verify_gpu_qualification.py --lifecycle \
  "$ARTIFACT_ROOT/lifecycle.jsonl"
python3 tools/verify_gpu_qualification.py --production-fixtures \
  "$ARTIFACT_ROOT/production-fixtures.jsonl"
```

Acceptance requires the stress verifier plus collection and closure validation
for lifecycle and production fixtures. Archive or upload artifacts only through
the already authorized delivery or workflow. Release only the exact owned
reservation according to its protocol; do not stop another process, erase build
state, prune caches, or run wrapper cleanup as an implicit final step.
