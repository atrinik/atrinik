# Native Windows Classic GPU preflight

This handoff closes issue #539. It covers the boundary where the Classic
client and server are built or packaged in the pinned Docker toolchain, then
the packaged client is executed as a native Windows process on a D3D12 host.
It does not authorize the Linux coordinator to launch a graphical client.

The authoritative machine-readable contract is validated with:

~~~sh
python3 scripts/validate_windows_gpu_evidence.py evidence.json --root .
~~~

The input is one JSON object. The optional root check requires every referenced
file to be a regular file below an `evidence/` directory and caps each
referenced artifact at 8 MiB. The validator never prints input values on
failure.

## Boundaries and authoritative entry points

Keep these stages separate:

| Stage | Authoritative entry point | What it proves |
| --- | --- | --- |
| Cross-build and package | `./atrinik package windows` from the pinned ordinary Linux or `windows-cross` devcontainer | The selected Classic source/profile produced a Windows review ZIP |
| Native package smoke | Classic `tools/ci/smoke_windows_review_bundle.ps1` on Windows | The extracted ZIP has the expected manifest, server lifecycle, launcher, processes, port ownership, and cleanup |
| Production client log | `client.log` created by the packaged `atrinik.exe` | The package client started from its package working directory and recorded build identity or a startup failure |
| D3D12 qualification | Classic `gpu-qualification.yml` commands on a Windows GPU host | The existing production-path integration, benchmark JSONL, lifecycle, and readback contracts ran on qualified hardware |
| Evidence contract | `scripts/validate_windows_gpu_evidence.py` in this repository | The handoff is complete, bounded, secret-free in its metadata, and classified consistently |

Do not substitute a Linux graphical run for the native package smoke. Do not
report a successful cross-build as a successful Windows runtime.

The production package has no command-line switch or supported environment
variable that forces Direct3D12. The production renderer asks SDL_GPU for a
hardware device; the selected backend, device, and driver are authoritative
only when emitted by the existing renderer/qualification evidence. The
`ATRINIK_GPU_CONFORMANCE_DRIVER` environment variable is read only by
builds compiled with `ATRINIK_GPU_CONFORMANCE_TESTS`; it is a test-build
selector, not a production-package setting. The package composer rejects those
test markers in production `atrinik.exe`.

Classic SDL 3.4 does not expose a separate selected D3D12 adapter LUID. The
qualified Windows runner is constrained to an unambiguous hardware adapter.
Record the exact device name reported by the qualification JSONL in both the
`adapter` and `device` fields when no separate adapter label
is available. Do not invent an adapter identifier.

## Evidence schema

The top-level object has exactly these fields:

| Field | Contract |
| --- | --- |
| `schema_version` | Integer `1` |
| `kind` | `native-windows-classic-gpu-preflight` |
| `recorded_at_utc` | Valid RFC 3339 UTC timestamp |
| `status` | `passed` or `failed` |
| `classification` | `passed`, `cross-build`, `package-handoff`, `windows-client-startup-runtime`, `gpu-backend-device`, `benchmark`, `linux-only-coordinator`, or `cleanup` |
| `next_action` | Bounded action text; exactly `none` only for a passed record |
| `source` | Exact `atrinik/classic` source, clean/dirty flag, safe profile name, and package SHA-256 or null when no package exists |
| `host` | Windows name/build and `x86_64` or `arm64` |
| `gpu` | Backend, adapter/device, driver name/version, qualification flag, and `reference`/`minimum` tier or an unavailable marker for failures |
| `commands` | The six required command records in the fixed order below |
| `benchmark` | Benchmark status, JSON/JSONL path, and validated record count |
| `logs` | Relative paths for client, server, and coordinator logs or redacted markers |
| `cleanup` | Cleanup status, bounded actions, and one exit code per action |
| `failure` | Null for a pass; failed command and bounded message for a failure |

The required command order is:

1. `cross-build`
2. `package-handoff`
3. `native-package-smoke`
4. `d3d12-benchmark`
5. `linux-coordinator-diagnostics`
6. `cleanup`

Each command has `name`, `status`, `exit_code`,
`stdout_path`, and `stderr_path`. Status is `passed`,
`failed`, or `not-run`; a passed command has exit code 0, a
failed command has a nonzero exit code, and a not-run command has a null exit
code. Every output path starts with `evidence/`, uses relative
forward-slash components, and is bounded.

A passed record requires:

- a clean source and package digest;
- `direct3d12`, complete GPU identity, qualified reference/minimum hardware;
- all package, smoke, benchmark, and cleanup commands to pass;
- the Linux coordinator diagnostic to be passed or not-run;
- benchmark status `passed` with at least three fresh records; and
- cleanup status `passed`.

A failed record must name the failed command and an actionable next action.
Use the first applicable classification:

| Classification | Failed command | Next action |
| --- | --- | --- |
| `cross-build` | `cross-build` | Inspect the pinned toolchain output, correct the source/build prerequisite, and rebuild the same commit |
| `package-handoff` | `package-handoff` | Recreate the ZIP, verify its manifest revision and SHA-256, then repeat the Windows handoff |
| `windows-client-startup-runtime` | `native-package-smoke` | Inspect the bounded client/package logs and correct missing assets, DLLs, working directory, or Windows startup prerequisites |
| `gpu-backend-device` | `native-package-smoke` or `d3d12-benchmark` | Install or update a supported hardware D3D12 driver, check the qualified adapter, and rerun the existing qualification path |
| `benchmark` | `d3d12-benchmark` | Rerun every workload three times in fresh processes and run the existing complete JSONL verifier |
| `linux-only-coordinator` | `linux-coordinator-diagnostics` | Record the Linux-only failure separately and rerun the native Windows package path; do not relabel it as a Windows client failure |
| `cleanup` | `cleanup` | Contain remaining processes and remove only the private run directory after ownership is certain |

A valid passed-record shape is shown below. Replace synthetic identity and
digest values with the values captured from the same run.

~~~json
{
  "schema_version": 1,
  "kind": "native-windows-classic-gpu-preflight",
  "recorded_at_utc": "2026-09-03T06:00:00Z",
  "status": "passed",
  "classification": "passed",
  "next_action": "none",
  "source": {
    "repository": "atrinik/classic",
    "commit": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "dirty": false,
    "profile": "classic-windows",
    "package_sha256": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  },
  "host": {
    "os": "Windows 11",
    "os_build": "build 26100",
    "architecture": "x86_64"
  },
  "gpu": {
    "backend": "direct3d12",
    "adapter": "Qualified adapter",
    "device": "Qualified adapter",
    "driver_name": "Qualified driver",
    "driver_version": "1.0",
    "qualified_hardware": true,
    "hardware_tier": "reference"
  },
  "commands": [
    {
      "name": "cross-build",
      "status": "passed",
      "exit_code": 0,
      "stdout_path": "evidence/commands/cross-build.stdout",
      "stderr_path": "evidence/commands/cross-build.stderr"
    },
    {
      "name": "package-handoff",
      "status": "passed",
      "exit_code": 0,
      "stdout_path": "evidence/commands/package-handoff.stdout",
      "stderr_path": "evidence/commands/package-handoff.stderr"
    },
    {
      "name": "native-package-smoke",
      "status": "passed",
      "exit_code": 0,
      "stdout_path": "evidence/commands/native-package-smoke.stdout",
      "stderr_path": "evidence/commands/native-package-smoke.stderr"
    },
    {
      "name": "d3d12-benchmark",
      "status": "passed",
      "exit_code": 0,
      "stdout_path": "evidence/commands/d3d12-benchmark.stdout",
      "stderr_path": "evidence/commands/d3d12-benchmark.stderr"
    },
    {
      "name": "linux-coordinator-diagnostics",
      "status": "not-run",
      "exit_code": null,
      "stdout_path": "evidence/commands/linux-coordinator-diagnostics.stdout",
      "stderr_path": "evidence/commands/linux-coordinator-diagnostics.stderr"
    },
    {
      "name": "cleanup",
      "status": "passed",
      "exit_code": 0,
      "stdout_path": "evidence/commands/cleanup.stdout",
      "stderr_path": "evidence/commands/cleanup.stderr"
    }
  ],
  "benchmark": {
    "status": "passed",
    "performance_json_path": "evidence/performance/gpu-qualification.jsonl",
    "records": 15
  },
  "logs": {
    "client": "evidence/logs/client.log",
    "server": "evidence/logs/server.log",
    "coordinator": "evidence/logs/coordinator.log"
  },
  "cleanup": {
    "status": "passed",
    "actions": [
      "stopped packaged client",
      "stopped packaged server",
      "removed temporary extraction directory"
    ],
    "exit_codes": [0, 0, 0]
  },
  "failure": null
}
~~~

## Repeatable execution sequence

Use one selected Classic commit and one profile for all stages. Save the
wrapper JSON and command output under a private evidence directory; copy only
sanitized evidence into the handoff artifact directory.

### 1. Build and package in the pinned toolchain

From the ordinary canonical Linux devcontainer, or directly inside the
pinned `windows-cross` devcontainer:

~~~sh
python3 scripts/atrinik_coordinator_context.py --json
./atrinik profile show classic --json
./atrinik package windows \
  --profile REVIEW_PROFILE \
  --state REVIEW_STATE \
  --output build/packages/review-windows.zip \
  --json
sha256sum build/packages/review-windows.zip
~~~

The wrapper output and SHA-256 are the `cross-build` and
`package-handoff` evidence. The package is sensitive: its server data
may contain credentials, player data, and the private QUIC identity. Never
upload the ZIP or raw server state to a public issue, pull request, CI artifact,
or release.

Do not run the graphical client in the ordinary Linux container. Copy the ZIP
and the exact selected Classic revision to the Windows qualification host.
Retain the matching Classic source checkout only for the existing smoke and
qualification scripts; it is not a second implementation source.

### 2. Extract privately and run the existing Windows package smoke

On native Windows, verify the handoff digest before extraction. Extract to a
new private writable directory and keep the matching Classic source checkout
at the selected commit:

~~~powershell
$Package = (Resolve-Path -LiteralPath .\review-windows.zip).Path
$PackageSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $Package).Hash.ToLowerInvariant()
$ArtifactRoot = Join-Path $env:TEMP ("atrinik-gpu-preflight-" + [Guid]::NewGuid().ToString("N"))
$EvidenceRoot = Join-Path $ArtifactRoot "evidence"
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $EvidenceRoot "commands") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $EvidenceRoot "logs") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $ArtifactRoot "performance") -Force | Out-Null
Expand-Archive -LiteralPath $Package -DestinationPath $ArtifactRoot
$Revision = "REPLACE_WITH_BUNDLE_MANIFEST_REVISION"
$ClassicRoot = "REPLACE_WITH_MATCHING_CLASSIC_SOURCE_ROOT"
$Smoke = Join-Path $ClassicRoot "tools\ci\smoke_windows_review_bundle.ps1"
& $Smoke -Package $Package -Revision $Revision `
  1> (Join-Path $EvidenceRoot "commands\native-package-smoke.stdout") `
  2> (Join-Path $EvidenceRoot "commands\native-package-smoke.stderr")
$SmokeExitCode = $LASTEXITCODE
~~~

The existing script checks the exact bundle revision and payload manifest,
starts the flat server, verifies its isolated UDP endpoint, runs
`run-review.bat`, checks that exactly one packaged server and client
stay alive, and contains both processes during cleanup. It is the authoritative
package smoke; do not replace it with a new launcher.

The production client writes `client.log` in its package working
directory. Copy a redacted bounded copy to `evidence/logs/client.log`.
The smoke script's captured server output may be used as
`evidence/logs/server.log`. If a log is absent or contains sensitive
data, retain a redacted marker file and classify the missing evidence as the
applicable failure instead of publishing the raw log.

### 3. Run the existing D3D12 qualification path

The production ZIP deliberately excludes test switches. Build the existing
Release test target with the pinned shader cohort, then execute it on native
Windows. The CMake options and environment names below are the ones used by
Classic `gpu-qualification.yml`; do not invent a replacement benchmark.

Prepare the shader cohort in the pinned build environment and hand off its
validated directory. From the matching Classic source tree:

~~~powershell
$env:ATRINIK_QUALIFICATION_SHADER_DIRECTORY = "REPLACE_WITH_SHADER_COHORT_DIRECTORY"
cmake -S client -B client/build/gpu-qualification -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON `
  -DATRINIK_GPU_CONFORMANCE_REQUIRED=ON -DPACKAGE_TYPE=none `
  "-DATRINIK_GPU_SHADER_DIRECTORY=$env:ATRINIK_QUALIFICATION_SHADER_DIRECTORY"
cmake --build client/build/gpu-qualification --config Release `
  --target atrinik client-gpu-renderer-integration-tests
~~~

Run the integration target first. It selects the requested backend only in
the conformance test build and performs the existing renderer checks:

~~~powershell
$env:ATRINIK_GPU_CONFORMANCE_DRIVER = "direct3d12"
$env:ATRINIK_GPU_CONFORMANCE_HARDWARE_TIER = "reference"
$env:ATRINIK_GPU_CONFORMANCE_QUALIFIED_HARDWARE = "1"
Set-Location $ClassicRoot\client
& .\build\gpu-qualification\client-gpu-renderer-integration-tests.exe `
  1> (Join-Path $EvidenceRoot "commands\d3d12-benchmark.stdout") `
  2> (Join-Path $EvidenceRoot "commands\d3d12-benchmark.stderr")
$IntegrationExitCode = $LASTEXITCODE
~~~

Run all five existing stress rows three times in fresh processes. Each output
is JSONL and must remain below the private evidence root:

~~~powershell
$Workloads = @(
  @{ Name = "dense-17x17-five-depth-1080p"; Fixture = "gpu-benchmark-dense-17x17-five-depth-1080p" },
  @{ Name = "dense-25x25-seven-depth-1440p"; Fixture = "gpu-qualification-town-25x25" },
  @{ Name = "wire-ceiling-28x28-thirteen-depth-1440p"; Fixture = "gpu-benchmark-wire-ceiling-28x28-thirteen-depth-1440p" },
  @{ Name = "wire-ceiling-28x28-thirteen-depth-4k"; Fixture = "gpu-benchmark-wire-ceiling-28x28-thirteen-depth-4k" },
  @{ Name = "actor-door-roof-animation-25x25"; Fixture = "gpu-benchmark-actor-door-roof-animation-25x25" }
)
$BenchmarkStdout = Join-Path $EvidenceRoot "commands\d3d12-benchmark.stdout"
$BenchmarkStderr = Join-Path $EvidenceRoot "commands\d3d12-benchmark.stderr"
foreach ($Workload in $Workloads) {
  for ($Run = 1; $Run -le 3; $Run++) {
    $Output = Join-Path $ArtifactRoot ("performance\" + $Workload.Name + "-" + $Run + ".jsonl")
    $env:ATRINIK_GPU_CONFORMANCE_OUTPUT = $Output
    & .\build\gpu-qualification\atrinik.exe `
      --gpu-player-view-benchmark `
      ("src/tests/fixtures/player_view/" + $Workload.Fixture + ".xml") `
      $Workload.Name 1>> $BenchmarkStdout 2>> $BenchmarkStderr
    if ($LASTEXITCODE -ne 0) {
      throw "GPU benchmark failed for $($Workload.Name) run $Run"
    }
  }
}
~~~

Run the existing complete verifier from the matching Classic source tree.
Pass the exact JSONL files produced by the loop:

~~~powershell
Set-Location $ClassicRoot
$Records = @(Get-ChildItem -LiteralPath (Join-Path $ArtifactRoot "performance") -Filter "*.jsonl" |
  Select-Object -ExpandProperty FullName)
python3 client/tools/verify_gpu_qualification.py --require-complete $Records
$BenchmarkVerifyExitCode = $LASTEXITCODE
~~~

The qualification JSONL supplies the backend, device, driver name/version,
hardware tier, source revision/dirty identity, workload dimensions, timing
stages, fences, uploads, allocation/retained-resource counters, recovery
events, and final readback checkpoints. Copy the exact sanitized identity
fields into `gpu`; use the number of verified records in
`benchmark.records`. Keep the JSONL as
`benchmark.performance_json_path`.

The existing lifecycle and frozen production-fixture checks can be run after
the stress rows when their visible headful window and golden-review
prerequisites are available:

~~~powershell
$env:ATRINIK_GPU_CONFORMANCE_LIFECYCLE_OUTPUT = Join-Path $ArtifactRoot "performance\lifecycle.jsonl"
$env:ATRINIK_GPU_CONFORMANCE_REVIEW_DIRECTORY = Join-Path $ArtifactRoot "review"
& .\build\gpu-qualification\atrinik.exe --gpu-player-view-lifecycle `
  src/tests/fixtures/player_view/brynknot-movement.xml
$LifecycleExitCode = $LASTEXITCODE
~~~

Run `verify_gpu_qualification.py --collect-lifecycle` and the applicable
human-golden checks as documented by Classic. Lifecycle evidence is additive
to this handoff; a missing human golden is not silently reported as a
successful qualification.

### 4. Write and validate the evidence record

Create the six command rows even when a later stage was not run. Use a null
exit code and `not-run` status for a genuinely skipped stage. Capture the
package SHA-256, selected revision/profile, Windows OS/build, D3D12 identity,
and exact command exit codes from the same run. Use redacted copies for log
paths and keep all paths relative to `evidence/`.

After writing `evidence.json`, validate both its metadata and referenced
files:

~~~powershell
python3 scripts/validate_windows_gpu_evidence.py `
  (Join-Path $ArtifactRoot "evidence.json") `
  --root $ArtifactRoot
if ($LASTEXITCODE -ne 0) {
  throw "Evidence contract rejected the preflight"
}
~~~

A passed record is not valid until the package smoke, D3D12 benchmark
verification, and cleanup all pass. If any stage fails, write a failed record
with the applicable classification and exact next action. A Linux-only
coordinator or SDL/display failure must remain a separate
`linux-only-coordinator` record and must not erase an independently
passed Windows package/runtime result.

## Safe repeat, shutdown, and retention

- Start every run with a new private extraction/evidence directory. Never
  reuse mutable `server-data`, credentials, certificates, or an old
  JSONL output.
- Keep `run-review.bat` and the existing smoke script's process containment
  and cleanup behavior. Stop the client and server before removing the private
  directory; verify process identity and ownership first.
- Do not run `./atrinik cleanup --apply` as part of this handoff. Cleanup
  preview commands are repository maintenance, not runtime containment.
- Retain only sanitized JSON, JSONL, bounded stdout/stderr, and redacted logs.
  Do not publish the review ZIP, server state, private QUIC identity, account
  data, passwords, hostnames, IP addresses, or user-specific paths.
- Delete or move only the exact private directory created for this run after
  all processes have exited. If ownership or process state is uncertain,
  preserve it and report `cleanup` failure.
- A successful Windows runtime and a failing Linux coordinator are separate
  observations. Repeat the Windows smoke and qualification path independently
  after correcting a Linux-only issue.
